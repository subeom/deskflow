/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerConfigDialogTests.h"

#include "common/Settings.h"
#include "gui/config/ServerConfig.h"
#include "gui/dialogs/ServerConfigDialog.h"
#include "gui/widgets/SettingsDialogButtonBox.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTest>

namespace {

QCheckBox *textOnlyBox(const ServerConfigDialog &dialog)
{
  return dialog.findChild<QCheckBox *>(QStringLiteral("cbClipboardTextOnly"));
}

QCheckBox *clipboardBox(const ServerConfigDialog &dialog)
{
  return dialog.findChild<QCheckBox *>(QStringLiteral("cbEnableClipboard"));
}

QCheckBox *experimentalBox(const ServerConfigDialog &dialog)
{
  return dialog.findChild<QCheckBox *>(QStringLiteral("cbExperimentalMultiMonitor"));
}

SettingsDialogButtonBox *buttons(const ServerConfigDialog &dialog)
{
  return dialog.findChild<SettingsDialogButtonBox *>();
}

bool saveIsEnabled(const ServerConfigDialog &dialog)
{
  return buttons(dialog)->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->isEnabled();
}

} // namespace

void ServerConfigDialogTests::initTestCase()
{
  QVERIFY(m_settingsDir.isValid());
  Settings::setSettingsFile(m_settingsDir.filePath(QStringLiteral("Deskflow.conf")));
  Settings::setStateFile(m_settingsDir.filePath(QStringLiteral("Deskflow.state")));
}

void ServerConfigDialogTests::cleanup()
{
  Settings::setValue(Settings::Server::ClipboardTextOnly);
  Settings::setValue(Settings::Server::EnableClipboard);
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor);
}

void ServerConfigDialogTests::experimentalMultiMonitor_isOffByDefaultAndSaysItIsExperimental()
{
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);

  const auto *box = experimentalBox(dialog);
  QVERIFY2(box != nullptr, "the dialog has no check box for the experiment");
  QVERIFY(!box->isChecked());
  QCOMPARE(box->text(), QStringLiteral("Experimental: multi-monitor aware cursor mapping"));
  QVERIFY2(
      box->toolTip().contains(QStringLiteral("off = Deskflow behaves exactly as before")), qPrintable(box->toolTip())
  );

  // the tag is seen without reading the check box: it is the title of the group that it is in
  const auto *group = dialog.findChild<QGroupBox *>(QStringLiteral("groupExperimental"));
  QVERIFY(group != nullptr);
  QVERIFY(group->title().contains(QStringLiteral("Experimental")));
  QVERIFY(box->parentWidget() == group);
}

void ServerConfigDialogTests::experimentalMultiMonitor_isLoadedSavedAndRestoredToOff()
{
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  {
    ServerConfig config;
    ServerConfigDialog dialog(nullptr, config);
    auto *box = experimentalBox(dialog);
    QVERIFY(box != nullptr);
    QVERIFY(box->isChecked());

    box->setChecked(false);
    Q_EMIT buttons(dialog)->accepted();
    QCOMPARE(Settings::value(Settings::Server::ExperimentalMultiMonitor).toBool(), false);
  }
  {
    ServerConfig config;
    ServerConfigDialog dialog(nullptr, config);
    auto *box = experimentalBox(dialog);
    QVERIFY(!box->isChecked());
    box->setChecked(true);
    Q_EMIT buttons(dialog)->accepted();
    QCOMPARE(Settings::value(Settings::Server::ExperimentalMultiMonitor).toBool(), true);
  }
  {
    ServerConfig config;
    ServerConfigDialog dialog(nullptr, config);
    QVERIFY(experimentalBox(dialog)->isChecked());
    Q_EMIT buttons(dialog)->restoreDefault();
    QVERIFY(!experimentalBox(dialog)->isChecked());
  }
}

void ServerConfigDialogTests::experimentalMultiMonitor_makesTheDialogModified()
{
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);
  auto *box = experimentalBox(dialog);
  QVERIFY(box != nullptr);
  QVERIFY(!saveIsEnabled(dialog));

  box->setChecked(true);
  QVERIFY(saveIsEnabled(dialog));

  box->setChecked(false);
  QVERIFY(!saveIsEnabled(dialog));
}

void ServerConfigDialogTests::experimentalMultiMonitor_isToldToTheComputerDialogsThatItOpens()
{
  // what the check box says before it is saved is what a computer's layout can be set for
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);
  QVERIFY(!dialog.property("experimentalMultiMonitor").toBool());
  experimentalBox(dialog)->setChecked(true);
  QVERIFY(dialog.property("experimentalMultiMonitor").toBool());
  experimentalBox(dialog)->setChecked(false);
  QVERIFY(!dialog.property("experimentalMultiMonitor").toBool());
}

void ServerConfigDialogTests::clipboardTextOnly_isOffByDefault()
{
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);

  const auto *box = textOnlyBox(dialog);
  QVERIFY2(box != nullptr, "the dialog has no \"share text only\" check box");
  QVERIFY(!box->isChecked());
}

void ServerConfigDialogTests::clipboardTextOnly_isOnlyAvailableWhileClipboardsAreShared()
{
  Settings::setValue(Settings::Server::EnableClipboard, true);
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);
  auto *box = textOnlyBox(dialog);
  auto *sharing = clipboardBox(dialog);
  QVERIFY(box != nullptr && sharing != nullptr);

  QVERIFY(sharing->isChecked());
  QVERIFY(box->isEnabled());

  sharing->setChecked(false);
  QVERIFY2(!box->isEnabled(), "text only is a way of sharing clipboards: nothing to choose when they are not shared");

  sharing->setChecked(true);
  QVERIFY(box->isEnabled());

  // and a dialog that starts with clipboards not shared starts without the choice
  Settings::setValue(Settings::Server::EnableClipboard, false);
  ServerConfigDialog notSharing(nullptr, config);
  QVERIFY(textOnlyBox(notSharing) != nullptr);
  QVERIFY(!textOnlyBox(notSharing)->isEnabled());
}

void ServerConfigDialogTests::clipboardTextOnly_replacesTheSizeLimit()
{
  Settings::setValue(Settings::Server::EnableClipboard, true);
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);
  auto *box = textOnlyBox(dialog);
  auto *limit = dialog.findChild<QSpinBox *>(QStringLiteral("sbClipboardSizeLimit"));
  QVERIFY(box != nullptr && limit != nullptr);

  QVERIFY(limit->isEnabled());

  // text only is 10 KB whatever the limit says, so the limit is not offered
  box->setChecked(true);
  QVERIFY(!limit->isEnabled());

  box->setChecked(false);
  QVERIFY(limit->isEnabled());

  // and the limit is not offered while clipboards are not shared at all
  dialog.findChild<QCheckBox *>(QStringLiteral("cbEnableClipboard"))->setChecked(false);
  QVERIFY(!limit->isEnabled());
}

void ServerConfigDialogTests::clipboardTextOnly_isLoadedFromTheSettings()
{
  Settings::setValue(Settings::Server::EnableClipboard, true);
  Settings::setValue(Settings::Server::ClipboardTextOnly, true);
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);

  const auto *box = textOnlyBox(dialog);
  QVERIFY(box != nullptr);
  QVERIFY(box->isChecked());
}

void ServerConfigDialogTests::clipboardTextOnly_isSavedToTheSettings()
{
  Settings::setValue(Settings::Server::EnableClipboard, true);
  {
    ServerConfig config;
    ServerConfigDialog dialog(nullptr, config);
    auto *box = textOnlyBox(dialog);
    QVERIFY(box != nullptr);
    box->setChecked(true);

    Q_EMIT buttons(dialog)->accepted();
    QCOMPARE(Settings::value(Settings::Server::ClipboardTextOnly).toBool(), true);
  }
  {
    ServerConfig config;
    ServerConfigDialog dialog(nullptr, config);
    auto *box = textOnlyBox(dialog);
    QVERIFY(box != nullptr);
    QVERIFY(box->isChecked());
    box->setChecked(false);

    Q_EMIT buttons(dialog)->accepted();
    QCOMPARE(Settings::value(Settings::Server::ClipboardTextOnly).toBool(), false);
  }
}

void ServerConfigDialogTests::clipboardTextOnly_isTurnedOffByRestoringDefaults()
{
  Settings::setValue(Settings::Server::EnableClipboard, true);
  Settings::setValue(Settings::Server::ClipboardTextOnly, true);
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);
  auto *box = textOnlyBox(dialog);
  QVERIFY(box != nullptr);
  QVERIFY(box->isChecked());

  Q_EMIT buttons(dialog)->restoreDefault();
  QVERIFY(!box->isChecked());
}

void ServerConfigDialogTests::clipboardTextOnly_makesTheDialogModified()
{
  Settings::setValue(Settings::Server::EnableClipboard, true);
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);
  auto *box = textOnlyBox(dialog);
  QVERIFY(box != nullptr);

  // nothing changed: nothing to save
  QVERIFY(!saveIsEnabled(dialog));

  box->setChecked(true);
  QVERIFY(saveIsEnabled(dialog));

  box->setChecked(false);
  QVERIFY(!saveIsEnabled(dialog));
}

// What the user gives up has to be said where the choice is made
void ServerConfigDialogTests::clipboardTextOnly_explainsWhatIsNotShared()
{
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);
  const auto *box = textOnlyBox(dialog);
  QVERIFY(box != nullptr);

  QVERIFY(box->text().contains(QStringLiteral("10 KB")));
  const auto tip = box->toolTip();
  QVERIFY2(tip.contains(QStringLiteral("10 KB")), qPrintable(tip));
  QVERIFY2(tip.contains(QStringLiteral("image"), Qt::CaseInsensitive), qPrintable(tip));
  QVERIFY2(tip.contains(QStringLiteral("HTML")), qPrintable(tip));
}

QTEST_MAIN(ServerConfigDialogTests)
