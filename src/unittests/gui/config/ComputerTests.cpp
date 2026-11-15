/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2024 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ComputerTests.h"

#include "common/Settings.h"
#include "gui/config/Computer.h"
#include "gui/dialogs/ComputerSettingsDialog.h"
#include "server/Config.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDataStream>

void ComputerTests::initTestCase()
{
  QDir dir;
  QVERIFY(dir.mkpath(m_settingsPath));

  QFile oldSettings(m_settingsFile);
  if (oldSettings.exists())
    oldSettings.remove();

  Settings::setSettingsFile(m_settingsFile);
  Settings::setStateFile(m_stateFile);
}

void ComputerTests::basicFunctionality()
{
  Computer computer;
  QVERIFY(computer.isNull());

  computer.setName("stub");
  QVERIFY(!computer.isNull());

  computer.saveSettings(Settings::proxy());
  computer.loadSettings(Settings::proxy());
  QCOMPARE("stub", computer.name());
}

void ComputerTests::crossOSSettings_defaultToDisabled()
{
  const auto name = QStringLiteral("crossOS-default");
  Settings::proxy().setValue("name", name);

  Computer computer;
  computer.loadSettings(Settings::proxy());
  QCOMPARE(computer.f18Action(), F18Action::PassThrough);
  QVERIFY(!computer.crossOSInvertX());
  QVERIFY(!computer.crossOSInvertY());
  QCOMPARE(static_cast<int>(computer.crossOSInvertScope()), static_cast<int>(ScrollInvertScope::All));
  QVERIFY(!computer.crossOSSemanticMedia());

  deskflow::server::Config config(nullptr);
  QVERIFY(config.addComputer(name.toStdString()));
  const auto *options = config.getOptions(name.toStdString());
  QVERIFY(options != nullptr);
  QCOMPARE(options->count(kOptionCrossOSF18Key), static_cast<size_t>(0));
  QCOMPARE(options->count(kOptionCrossOSInvertScrollX), static_cast<size_t>(0));
  QCOMPARE(options->count(kOptionCrossOSInvertScrollY), static_cast<size_t>(0));
  QCOMPARE(options->count(kOptionCrossOSInvertScope), static_cast<size_t>(0));
  QCOMPARE(options->count(kOptionCrossOSSemanticMedia), static_cast<size_t>(0));
}

void ComputerTests::crossOSF18Action_loadsMissingInvalidAndLegacyValuesAsPassThrough()
{
  const auto name = QStringLiteral("crossOS-f18-load");
  const auto key = Settings::Computer::F18Action.arg(name);
  Settings::proxy().setValue("name", name);

  const auto loadedAction = [&]() {
    Computer computer;
    computer.loadSettings(Settings::proxy());
    return static_cast<int>(computer.f18Action());
  };
  constexpr auto passThrough = static_cast<int>(F18Action::PassThrough);
  constexpr auto hangul = static_cast<int>(F18Action::Hangul);

  // a config written before the setting existed has no key
  Settings::setValue(key, QVariant());
  QCOMPARE(loadedAction(), passThrough);

  Settings::setValue(key, QStringLiteral("hangul"));
  QCOMPARE(loadedAction(), hangul);
  Settings::setValue(key, QStringLiteral(" Hangul "));
  QCOMPARE(loadedAction(), hangul);

  Settings::setValue(key, QStringLiteral("passthrough"));
  QCOMPARE(loadedAction(), passThrough);
  Settings::setValue(key, QStringLiteral("f19"));
  QCOMPARE(loadedAction(), passThrough);
  Settings::setValue(key, QString());
  QCOMPARE(loadedAction(), passThrough);

  // a config from the build that mapped Right Command still loads, and the old value means nothing now
  Settings::setValue(key, QVariant());
  Settings::setValue(QStringLiteral("computer_%1/rightCommandAction").arg(name), QStringLiteral("hangul"));
  QCOMPARE(loadedAction(), passThrough);
}

void ComputerTests::crossOSInvertScope_loadsMissingAndInvalidValuesAsAll()
{
  const auto name = QStringLiteral("crossOS-scope-load");
  const auto key = Settings::Computer::CrossOSInvertScope.arg(name);
  Settings::proxy().setValue("name", name);

  const auto loadedScope = [&]() {
    Computer computer;
    computer.loadSettings(Settings::proxy());
    return static_cast<int>(computer.crossOSInvertScope());
  };
  constexpr auto all = static_cast<int>(ScrollInvertScope::All);
  constexpr auto wheelOnly = static_cast<int>(ScrollInvertScope::MouseWheelOnly);

  // a config written before the setting existed has no key
  Settings::setValue(key, QVariant());
  QCOMPARE(loadedScope(), all);

  Settings::setValue(key, QStringLiteral("mouseWheelOnly"));
  QCOMPARE(loadedScope(), wheelOnly);
  Settings::setValue(key, QStringLiteral(" MouseWheelONLY "));
  QCOMPARE(loadedScope(), wheelOnly);

  Settings::setValue(key, QStringLiteral("all"));
  QCOMPARE(loadedScope(), all);
  Settings::setValue(key, QStringLiteral("trackpadOnly"));
  QCOMPARE(loadedScope(), all);
  Settings::setValue(key, QString());
  QCOMPARE(loadedScope(), all);
}

void ComputerTests::crossOSDialog_saveSerializeAndProduceOptions()
{
  Computer computer(QStringLiteral("crossOS-ui"));
  ComputerSettingsDialog dialog(nullptr, &computer);

  auto *f18Key = dialog.findChild<QComboBox *>(QStringLiteral("comboF18Key"));
  auto *invertX = dialog.findChild<QCheckBox *>(QStringLiteral("chkCrossOSInvertX"));
  auto *invertY = dialog.findChild<QCheckBox *>(QStringLiteral("chkCrossOSInvertY"));
  auto *invertScope = dialog.findChild<QComboBox *>(QStringLiteral("comboCrossOSInvertScope"));
  auto *semanticMedia = dialog.findChild<QCheckBox *>(QStringLiteral("chkCrossOSSemanticMedia"));
  QVERIFY(f18Key != nullptr);
  QVERIFY(invertX != nullptr);
  QVERIFY(invertY != nullptr);
  QVERIFY(invertScope != nullptr);
  QVERIFY(semanticMedia != nullptr);
  // Command is the Windows key and is not a setting: the old Right Command row is gone
  QVERIFY(dialog.findChild<QComboBox *>(QStringLiteral("comboRightCommand")) == nullptr);
  QCOMPARE(f18Key->count(), 2);
  QCOMPARE(f18Key->itemText(0), QStringLiteral("Pass through (default)"));
  QCOMPARE(f18Key->itemText(1), QStringLiteral("Korean-English toggle (Hangul)"));
  QCOMPARE(f18Key->currentIndex(), 0);
  QVERIFY(!invertX->isChecked());
  QVERIFY(!invertY->isChecked());
  QCOMPARE(invertScope->count(), 2);
  QCOMPARE(invertScope->itemText(0), QStringLiteral("All scrolling"));
  QCOMPARE(invertScope->itemText(1), QStringLiteral("Mouse wheel only (trackpad unchanged)"));
  QCOMPARE(invertScope->currentIndex(), 0);
  QVERIFY(!semanticMedia->isChecked());

  f18Key->setCurrentIndex(1);
  invertX->setChecked(true);
  invertY->setChecked(true);
  invertScope->setCurrentIndex(1);
  semanticMedia->setChecked(true);
  dialog.accept();

  QCOMPARE(computer.f18Action(), F18Action::Hangul);
  QVERIFY(computer.crossOSInvertX());
  QVERIFY(computer.crossOSInvertY());
  QCOMPARE(static_cast<int>(computer.crossOSInvertScope()), static_cast<int>(ScrollInvertScope::MouseWheelOnly));
  QVERIFY(computer.crossOSSemanticMedia());

  QByteArray serialized;
  QDataStream output(&serialized, QIODevice::WriteOnly);
  output << computer;
  Computer copy;
  QDataStream input(&serialized, QIODevice::ReadOnly);
  input >> copy;
  QVERIFY(computer == copy);
  QCOMPARE(static_cast<int>(copy.crossOSInvertScope()), static_cast<int>(ScrollInvertScope::MouseWheelOnly));

  computer.saveSettings(Settings::proxy());
  QCOMPARE(Settings::value(Settings::Computer::F18Action.arg(computer.name())).toString(), QStringLiteral("hangul"));
  QVERIFY(Settings::value(Settings::Computer::CrossOSInvertX.arg(computer.name())).toBool());
  QVERIFY(Settings::value(Settings::Computer::CrossOSInvertY.arg(computer.name())).toBool());
  QCOMPARE(
      Settings::value(Settings::Computer::CrossOSInvertScope.arg(computer.name())).toString(),
      QStringLiteral("mouseWheelOnly")
  );
  QVERIFY(Settings::value(Settings::Computer::CrossOSSemanticMedia.arg(computer.name())).toBool());

  // what the dialog saved is what the server turns into options
  deskflow::server::Config config(nullptr);
  QVERIFY(config.addComputer(computer.name().toStdString()));
  const auto *options = config.getOptions(computer.name().toStdString());
  QVERIFY(options != nullptr);
  QCOMPARE(options->at(kOptionCrossOSF18Key), static_cast<OptionValue>(F18Action::Hangul));
  QCOMPARE(options->at(kOptionCrossOSInvertScrollX), 1);
  QCOMPARE(options->at(kOptionCrossOSInvertScrollY), 1);
  QCOMPARE(options->at(kOptionCrossOSInvertScope), static_cast<OptionValue>(ScrollInvertScope::MouseWheelOnly));
  QCOMPARE(options->at(kOptionCrossOSSemanticMedia), 1);

  // the scope takes part in equality: a reload of what was saved matches, and differs once only the scope changes
  Computer reloaded;
  reloaded.loadSettings(Settings::proxy());
  QVERIFY(reloaded == computer);
  Settings::setValue(Settings::Computer::CrossOSInvertScope.arg(computer.name()), QStringLiteral("all"));
  Computer reloadedAll;
  reloadedAll.loadSettings(Settings::proxy());
  QCOMPARE(static_cast<int>(reloadedAll.crossOSInvertScope()), static_cast<int>(ScrollInvertScope::All));
  QVERIFY(!(reloadedAll == computer));
}

void ComputerTests::shareClipboard_defaultsOnAndAddsNoOption()
{
  // a configuration that predates the setting has no key for it: the computer shares its clipboard, as it always did
  const auto name = QStringLiteral("clipboard-default");
  Settings::proxy().setValue("name", name);

  Computer computer;
  computer.loadSettings(Settings::proxy());
  QVERIFY(computer.shareClipboard());

  deskflow::server::Config config(nullptr);
  QVERIFY(config.addComputer(name.toStdString()));
  const auto *options = config.getOptions(name.toStdString());
  QVERIFY(options != nullptr);
  QCOMPARE(options->count(kOptionClipboardSharing), static_cast<size_t>(0));
}

void ComputerTests::shareClipboard_off_isSavedLoadedAndProducesTheOption()
{
  const auto name = QStringLiteral("clipboard-off");
  Settings::setValue(Settings::Computer::ShareClipboard.arg(name), false);
  Settings::proxy().setValue("name", name);

  Computer computer;
  computer.loadSettings(Settings::proxy());
  QVERIFY(!computer.shareClipboard());

  // saving writes it back, and a reload is the same computer
  computer.saveSettings(Settings::proxy());
  QVERIFY(!Settings::value(Settings::Computer::ShareClipboard.arg(computer.name())).toBool());
  Computer reloaded;
  reloaded.loadSettings(Settings::proxy());
  QVERIFY(!reloaded.shareClipboard());
  QVERIFY(reloaded == computer);

  deskflow::server::Config config(nullptr);
  QVERIFY(config.addComputer(computer.name().toStdString()));
  const auto *options = config.getOptions(computer.name().toStdString());
  QVERIFY(options != nullptr);
  QCOMPARE(options->at(kOptionClipboardSharing), 0);
}

void ComputerTests::shareClipboard_dialogChecksTheBoxAndSerializes()
{
  Computer computer(QStringLiteral("clipboard-ui"));
  ComputerSettingsDialog dialog(nullptr, &computer);
  auto *share = dialog.findChild<QCheckBox *>(QStringLiteral("chkShareClipboard"));
  QVERIFY(share != nullptr);
  QCOMPARE(share->text(), QStringLiteral("Share clipboard with this computer"));
  QVERIFY(share->isChecked());

  share->setChecked(false);
  dialog.accept();
  QVERIFY(!computer.shareClipboard());

  QByteArray serialized;
  QDataStream output(&serialized, QIODevice::WriteOnly);
  output << computer;
  Computer copy;
  QDataStream input(&serialized, QIODevice::ReadOnly);
  input >> copy;
  QVERIFY(!copy.shareClipboard());
  QVERIFY(computer == copy);

  // the dialog shows what the computer has
  ComputerSettingsDialog again(nullptr, &computer);
  QVERIFY(!again.findChild<QCheckBox *>(QStringLiteral("chkShareClipboard"))->isChecked());
}

QTEST_MAIN(ComputerTests)
