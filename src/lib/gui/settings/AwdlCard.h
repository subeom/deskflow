/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>
#include <QPointer>

#include <functional>

class QMessageBox;
class QPushButton;
class SettingCard;
class SettingRow;

namespace deskflow::awdl {
class AwdlController;
}

namespace deskflow::gui::settings {

class SettingsBinder;
class SettingsPage;

/*!
 * \brief The card of the Network page of a Mac that has the option "turn off the wireless link of AirDrop and Handoff
 * while sharing": the switch (the setting), the line that says the state of the link with the numbers of the last
 * measurement, the buttons that measure now, turn the link off now and turn it back on, and the row of the permission
 * to do it without a password (its state, and the buttons that add and remove the file for sudo).
 *
 * The button that adds the file asks a question first, in plain words, before the dialog of the password opens (what is
 * added, what it lets the account do, who else can do it, how to remove it). The question is a message box; a test
 * gives its own answer (s_confirmForTest), and offscreen it is never shown and the answer is no.
 *
 * It knows the controller and nothing of the system: the buttons ask the controller, and the line says what the
 * controller says. Without a controller (nothing to ask) the buttons are off and the line says that nothing was read.
 * It is made only where there is a wireless link of this kind (SettingsPages checks awdl::platformSupported()).
 */
class AwdlCard : public QObject
{
  Q_OBJECT

public:
  AwdlCard(SettingsPage *page, SettingsBinder *binder);

  SettingCard *card() const;
  //! The controller that the card shows and asks (not owned; null: none)
  void setController(deskflow::awdl::AwdlController *controller);

  //! The question that is asked before the dialog of the password opens, as a box that is not shown yet: its title and
  //! text are what the person reads, its buttons are Continue (the accepting one) and Cancel (the default). The card
  //! shows it; the pictures of the screens make it the same way.
  static QMessageBox *createQuestion(QWidget *parent);

  //! Only a test sets it: the answer to the question before the dialog of the password opens (title and text are what
  //! the person would read)
  inline static std::function<bool(const QString &title, const QString &text)> s_confirmForTest;

private:
  void update();
  QString stateLine() const;
  QString permissionLine() const;
  bool confirmAllow() const;

  QPointer<SettingsPage> m_page;
  SettingCard *m_card = nullptr;
  SettingRow *m_stateRow = nullptr;
  QPushButton *m_measure = nullptr;
  QPushButton *m_turnOff = nullptr;
  QPushButton *m_turnOn = nullptr;
  SettingRow *m_permissionRow = nullptr;
  QPushButton *m_allow = nullptr;
  QPushButton *m_remove = nullptr;
  QPointer<deskflow::awdl::AwdlController> m_controller;
};

} // namespace deskflow::gui::settings
