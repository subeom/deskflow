/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AwdlCard.h"

#include "SettingsBinder.h"
#include "SettingsPage.h"
#include "common/Settings.h"
#include "gui/awdl/AwdlController.h"
#include "gui/awdl/AwdlTexts.h"
#include "gui/widgets/SettingCard.h"
#include "gui/widgets/SettingRow.h"
#include "gui/widgets/ToggleSwitch.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QMessageBox>
#include <QPushButton>

#include <memory>

using namespace deskflow::awdl;

namespace deskflow::gui::settings {

AwdlCard::AwdlCard(SettingsPage *page, SettingsBinder *binder) : QObject(page), m_page(page)
{
  m_card = page->addCard([] { return QCoreApplication::translate("AwdlTexts", "AirDrop and Handoff"); });
  m_card->setObjectName(QStringLiteral("cardAwdl"));

  // the option itself: a person who has no stutter has no reason to turn it on, and the text says so
  auto *optionRow = page->addRow(
      m_card,
      [] {
        return QCoreApplication::translate("AwdlTexts", "Turn off AirDrop and Handoff's wireless link while sharing");
      },
      [] {
        return QCoreApplication::translate(
            "AwdlTexts",
            "Needed only if the mouse stutters while sharing over Wi-Fi. While the link is off, AirDrop, Handoff, "
            "Universal Control and AirPlay to this Mac do not work. Deskflow asks for the administrator password "
            "once each time sharing starts, or never if you allow it without a password below."
        );
      }
  );
  auto *toggle = new ToggleSwitch;
  toggle->setObjectName(QStringLiteral("cbMacAwdlOff"));
  optionRow->addControl(toggle);
  binder->bindSwitch(Settings::Network::MacAwdlOff, toggle);

  // what the link is now, with what the last measurement found
  m_stateRow = page->addRow(
      m_card, [] { return QCoreApplication::translate("AwdlTexts", "State of the wireless link"); },
      [this] { return stateLine(); }
  );
  m_measure = new QPushButton;
  m_measure->setObjectName(QStringLiteral("btnAwdlMeasure"));
  m_stateRow->addControl(m_measure);

  // the change by hand, whether or not the option is on
  auto *changeRow = page->addRow(
      m_card, [] { return QCoreApplication::translate("AwdlTexts", "Change the wireless link now"); },
      [] {
        return QCoreApplication::translate(
            "AwdlTexts", "Asks for the administrator password, unless it is allowed without a password below. The "
                         "link comes back by itself after a restart of the Mac, and when AirDrop or Handoff use it."
        );
      }
  );
  m_turnOff = new QPushButton;
  m_turnOff->setObjectName(QStringLiteral("btnAwdlTurnOff"));
  m_turnOn = new QPushButton;
  m_turnOn->setObjectName(QStringLiteral("btnAwdlTurnOn"));
  changeRow->addControl(m_turnOff);
  changeRow->addControl(m_turnOn);

  // the permission to do it without a password: one small file for sudo, added and removed with the dialog of the
  // password
  m_permissionRow = page->addRow(
      m_card, [] { return QCoreApplication::translate("AwdlTexts", "Change without a password"); },
      [this] { return permissionLine(); }
  );
  // two wide buttons: under the text when the row is narrow, as a page that does not scroll sideways needs at 150 %
  m_permissionRow->setStackWhenNarrow(true);
  m_allow = new QPushButton;
  m_allow->setObjectName(QStringLiteral("btnAwdlAllow"));
  m_remove = new QPushButton;
  m_remove->setObjectName(QStringLiteral("btnAwdlRemovePermission"));
  m_permissionRow->addControl(m_allow);
  m_permissionRow->addControl(m_remove);

  connect(m_measure, &QPushButton::clicked, this, [this] {
    if (m_controller)
      m_controller->measureNow();
  });
  connect(m_turnOff, &QPushButton::clicked, this, [this] {
    if (m_controller)
      m_controller->turnOffNow();
  });
  connect(m_turnOn, &QPushButton::clicked, this, [this] {
    if (m_controller)
      m_controller->turnBackOn();
  });
  connect(m_allow, &QPushButton::clicked, this, [this] {
    // the person is told what is added before the dialog of the password opens, and may say no
    if (m_controller && confirmAllow())
      m_controller->allowWithoutPassword();
  });
  connect(m_remove, &QPushButton::clicked, this, [this] {
    if (m_controller)
      m_controller->removePermission();
  });
  // what the page lists from the system is that of the time that it is looked at
  connect(page, &SettingsPage::shown, this, [this] {
    if (m_controller)
      m_controller->refresh();
  });
  page->follow([this] { update(); });
}

SettingCard *AwdlCard::card() const
{
  return m_card;
}

void AwdlCard::setController(AwdlController *controller)
{
  if (m_controller == controller)
    return;
  if (m_controller)
    m_controller->disconnect(this);
  m_controller = controller;
  if (controller) {
    connect(controller, &AwdlController::stateChanged, this, [this] { update(); });
    connect(controller, &AwdlController::latencyChanged, this, [this] { update(); });
    connect(controller, &AwdlController::askingChanged, this, [this] { update(); });
    connect(controller, &AwdlController::measuringChanged, this, [this] { update(); });
    connect(controller, &AwdlController::permissionChanged, this, [this] { update(); });
    // a controller that goes away leaves the card as it is made (its pointer is null when this runs)
    connect(controller, &QObject::destroyed, this, [this] { update(); });
  }
  update();
}

QString AwdlCard::stateLine() const
{
  if (!m_controller)
    return stateText(Summary::Unknown);
  const auto summary = m_controller->summary();
  auto text = stateText(summary);
  // the numbers belong to a link that was read: not to one that is not there
  if (summary != Summary::NotAvailable && summary != Summary::Unknown) {
    if (const auto numbers = measurementText(m_controller->latency()); !numbers.isEmpty())
      text += QLatin1Char(' ') + numbers;
  }
  return text;
}

QString AwdlCard::permissionLine() const
{
  if (!m_controller)
    return permissionText(PermissionState::Unknown, false);
  auto text = permissionText(m_controller->permission(), m_controller->permissionFilePresent());
  if (const auto note = permissionNoteText(m_controller->permissionNote()); !note.isEmpty())
    text += QLatin1Char(' ') + note;
  return text;
}

QMessageBox *AwdlCard::createQuestion(QWidget *parent)
{
  auto *box =
      new QMessageBox(QMessageBox::Question, confirmAllowTitle(), confirmAllowText(), QMessageBox::NoButton, parent);
  box->setObjectName(QStringLiteral("awdlQuestion"));
  auto *allow = box->addButton(QCoreApplication::translate("AwdlTexts", "Continue"), QMessageBox::AcceptRole);
  allow->setObjectName(QStringLiteral("btnAwdlQuestionContinue"));
  auto *cancel = box->addButton(QCoreApplication::translate("AwdlTexts", "Cancel"), QMessageBox::RejectRole);
  cancel->setObjectName(QStringLiteral("btnAwdlQuestionCancel"));
  // the safe answer is the default
  box->setDefaultButton(cancel);
  box->setEscapeButton(cancel);
  return box;
}

bool AwdlCard::confirmAllow() const
{
  if (s_confirmForTest)
    return s_confirmForTest(confirmAllowTitle(), confirmAllowText());
  // a program without a screen (a test that forgot to answer) never shows a question, and the answer is no
  if (QGuiApplication::platformName() == QLatin1String("offscreen"))
    return false;
  std::unique_ptr<QMessageBox> box(createQuestion(m_page ? m_page->window() : nullptr));
  box->exec();
  return box->clickedButton() != nullptr &&
         box->clickedButton()->objectName() == QLatin1String("btnAwdlQuestionContinue");
}

void AwdlCard::update()
{
  const bool known = !m_controller.isNull();
  const bool busy = known && m_controller->isAsking();
  const auto state = known ? m_controller->state() : LinkState::Unknown;
  const bool measuring = known && m_controller->isMeasuring();

  m_stateRow->setDescription(stateLine());
  m_measure->setText(
      measuring ? QCoreApplication::translate("AwdlTexts", "Measuring...")
                : QCoreApplication::translate("AwdlTexts", "Measure now")
  );
  m_turnOff->setText(QCoreApplication::translate("AwdlTexts", "Turn off now"));
  m_turnOn->setText(QCoreApplication::translate("AwdlTexts", "Turn back on"));
  m_measure->setEnabled(known && !busy && !measuring && state != LinkState::Unavailable);
  m_turnOff->setEnabled(known && !busy && state == LinkState::Up);
  m_turnOn->setEnabled(known && !busy && state == LinkState::Down);

  m_permissionRow->setDescription(permissionLine());
  m_allow->setText(QCoreApplication::translate("AwdlTexts", "Allow without password..."));
  m_remove->setText(QCoreApplication::translate("AwdlTexts", "Remove permission"));
  // adding is for a computer that has no permission yet (or one that sudo does not use); removing is for the file of
  // the program, which is the only one that this program may remove
  const bool allowed = known && m_controller->permission() == PermissionState::Allowed;
  const bool filePresent = known && m_controller->permissionFilePresent();
  m_allow->setEnabled(known && !busy && !(allowed && filePresent));
  m_remove->setEnabled(known && !busy && filePresent);
}

} // namespace deskflow::gui::settings
