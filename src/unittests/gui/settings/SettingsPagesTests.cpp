/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SettingsPagesTests.h"

#include "../SwitchInput.h"
#include "common/NetworkProtocol.h"
#include "common/PlatformInfo.h"
#include "common/Settings.h"
#include "gui/config/ServerConfig.h"
#include "gui/dialogs/ServerConfigDialog.h"
#include "gui/dialogs/SettingsDialog.h"
#include "gui/settings/RestartBanner.h"
#include "gui/settings/SettingEffects.h"
#include "gui/settings/SettingsBinder.h"
#include "gui/settings/SettingsPage.h"
#include "gui/settings/SettingsPages.h"
#include "gui/style/AccessibleInterfaces.h"
#include "gui/style/Appearance.h"
#include "gui/style/DeskflowStyle.h"
#include "gui/widgets/InlineHint.h"
#include "gui/widgets/PageTitle.h"
#include "gui/widgets/SettingCard.h"
#include "gui/widgets/SettingRow.h"
#include "gui/widgets/SettingsDialogButtonBox.h"
#include "gui/widgets/ToggleSwitch.h"

#include <QAccessible>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMetaEnum>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSignalSpy>
#include <QSpinBox>

using namespace deskflow::gui::settings;

namespace {

template <class T> T *find(const SettingsPages &pages, const char *name)
{
  for (const auto &entry : pages.entries()) {
    if (auto *found = entry.page->findChild<T *>(QString::fromLatin1(name)))
      return found;
  }
  return nullptr;
}

template <class T> T *must(const SettingsPages &pages, const char *name)
{
  auto *found = find<T>(pages, name);
  Q_ASSERT_X(found, "must", name);
  return found;
}

// what a person does with the controls: the signals that tell the binder are the ones that a person causes
void setSwitch(QAbstractButton *button, bool on)
{
  button->setChecked(on);
}

void choose(QComboBox *combo, const QVariant &data)
{
  for (int i = 0; i < combo->count(); ++i) {
    if (SettingsBinder::sameValue(data, combo->itemData(i))) {
      combo->setCurrentIndex(i);
      Q_EMIT combo->activated(i);
      return;
    }
  }
  qFatal("no such choice");
}

// An empty file: a test that cannot make it is a failure of the test, and not a page that is shown a file that is not
// there
bool touch(const QString &path)
{
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly))
    return false;
  file.close();
  return file.error() == QFileDevice::NoError && QFileInfo::exists(path);
}

void type(QLineEdit *edit, const QString &text)
{
  edit->setText(text);
  Q_EMIT edit->editingFinished();
}

QString stored(const QString &key)
{
  QSettings file(Settings::settingsFile(), QSettings::IniFormat);
  return file.value(key).toString();
}

QByteArray settingsFileContent()
{
  QFile file(Settings::settingsFile());
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// The settings that the dialog of the settings writes when it is accepted, and what each of them needs
struct DialogKey
{
  QString key;
  Effect effect;
};

QList<DialogKey> dialogKeys()
{
  using S = Settings;
  return {
      {S::Core::Port, Effect::RestartSharing},
      {S::Core::Interface, Effect::RestartSharing},
      {S::Log::Level, Effect::Live},
      {S::Log::ToFile, Effect::RestartSharing},
      {S::Log::File, Effect::RestartSharing},
      {S::Daemon::Elevate, Effect::RestartSharing},
      {S::Gui::Autohide, Effect::Live},
      {S::Gui::AutoUpdateCheck, Effect::Live},
      {S::Core::PreventSleep, Effect::RestartSharing},
      {S::Security::Certificate, Effect::RestartSharing},
      {S::Security::KeySize, Effect::RestartSharing},
      {S::Security::TlsEnabled, Effect::RestartSharing},
      {S::Gui::CloseToTray, Effect::Live},
      {S::Gui::SymbolicTrayIcon, Effect::Live},
      {S::Security::CheckPeers, Effect::RestartSharing},
      {S::Core::Language, Effect::Live},
      {S::Log::GuiDebug, Effect::Live},
      {S::Gui::ShowVersionInTitle, Effect::Live},
      {S::Core::EnableEnterCommand, Effect::RestartSharing},
      {S::Core::EnableExitCommand, Effect::RestartSharing},
      {S::Core::ComputerEnterCommand, Effect::RestartSharing},
      {S::Core::ComputerExitCommand, Effect::RestartSharing},
      {S::Core::ProcessMode, Effect::RestartSharing},
  };
}

void forgetAll()
{
  for (const auto &entry : dialogKeys())
    Settings::setValue(entry.key);
  for (const auto &key : keysWith(Effect::RestartSharing)) {
    if (key.startsWith(QStringLiteral("server/")))
      Settings::setValue(key);
  }
  Settings::setValue(Settings::Server::XdpRestoreToken);
  Settings::setValue(Settings::Server::XdpClipboardRetried);
  Settings::setValue(Settings::Gui::Appearance);
  Settings::setValue(Settings::Gui::NativeWindowFrame);
  Settings::setValue(Settings::Gui::InputIndicatorEnabled);
  Settings::setValue(Settings::Gui::InputIndicatorPosition);
  Settings::setValue(Settings::Gui::InputIndicatorDwellMs);
  Settings::setValue(Settings::Gui::InputIndicatorOpacityPercent);
  Settings::setValue(Settings::Gui::InputIndicatorTone);
  Settings::setValue(Settings::Core::CoreMode);
}

} // namespace

void SettingsPagesTests::initTestCase()
{
  QVERIFY(m_dir.isValid());
  Settings::setSettingsFile(m_dir.filePath(QStringLiteral("Deskflow.conf")));
  Settings::setStateFile(m_dir.filePath(QStringLiteral("Deskflow.state")));
  QAccessible::setActive(true);
}

void SettingsPagesTests::cleanup()
{
  forgetAll();
  QApplication::setStyle(QStringLiteral("Fusion"));
}

void SettingsPagesTests::effects_everyBoundSettingSaysWhatItNeeds()
{
  SettingsPages pages;
  const auto keys = pages.binder()->keys();
  QVERIFY(keys.size() >= 20);
  for (const auto &key : keys)
    QVERIFY2(effectOf(key).has_value(), qPrintable(key));
  // a key that no page binds has no effect to give
  QVERIFY(!effectOf(QStringLiteral("computer_x/name")).has_value());
}

void SettingsPagesTests::pages_updateCard_keepsStartupChoiceAndOffersOneManualCheck()
{
  SettingsPages pages;
  QVERIFY(must<ToggleSwitch>(pages, "cbAutoUpdate"));
  auto *button = must<QPushButton>(pages, "btnCheckForUpdates");
  QCOMPARE(button->text(), QStringLiteral("Check now"));
  QSignalSpy requested(&pages, &SettingsPages::checkForUpdatesRequested);
  QTest::mouseClick(button, Qt::LeftButton);
  QCOMPARE(requested.count(), 1);
}

void SettingsPagesTests::pages_theWindowFrameIsChosenWithOneSwitch_whichSaysTheFrameOfTheNewLook()
{
  SettingsPages pages;
  auto *toggle = must<ToggleSwitch>(pages, "cbDeskflowFrame");
  // the title of the row says what is on, not what is off, on every system
  const auto row = qobject_cast<SettingRow *>(toggle->parentWidget());
  QVERIFY(row);
  QCOMPARE(row->title(), QStringLiteral("Use the Deskflow window frame"));
  // it is read at the start of the application, and it says so, with the words that the look has for it
  QCOMPARE(row->description(), QStringLiteral("Takes effect the next time the application starts."));
  QCOMPARE(effectOf(Settings::Gui::NativeWindowFrame), std::optional<Effect>(Effect::Live));

  // the setting is the other way round (it says the frame of the system): the switch is on where the new frame is what
  // the platform has by default (not on Linux in the first release), and a choice is written whatever the default is
  Settings::setValue(Settings::Gui::NativeWindowFrame);
  const bool newFrameByDefault = !Settings::defaultValue(Settings::Gui::NativeWindowFrame).toBool();
  QCOMPARE(toggle->isChecked(), newFrameByDefault);
  QVERIFY(!Settings::proxy().contains(Settings::Gui::NativeWindowFrame));

  toggle->setChecked(!newFrameByDefault);
  QCOMPARE(Settings::value(Settings::Gui::NativeWindowFrame).toBool(), newFrameByDefault);
  QVERIFY(Settings::proxy().contains(Settings::Gui::NativeWindowFrame));
  toggle->setChecked(newFrameByDefault);
  QCOMPARE(Settings::value(Settings::Gui::NativeWindowFrame).toBool(), !newFrameByDefault);
  // the choice is written even where it is the default: on Linux the frame of the new look is chosen by writing false,
  // where the default is true
  QVERIFY(Settings::proxy().contains(Settings::Gui::NativeWindowFrame));

  // a change of the setting from elsewhere is taken by the switch
  Settings::setValue(Settings::Gui::NativeWindowFrame, true);
  QVERIFY(!toggle->isChecked());
  Settings::setValue(Settings::Gui::NativeWindowFrame, false);
  QVERIFY(toggle->isChecked());
}

void SettingsPagesTests::pages_theAppearanceCard_isTheWayBackToTheLookOfTheSystem_andTheWayForward()
{
  // the new look is what a person has until a choice is made: the card shows it, and the page does not choose for them
  Settings::setValue(Settings::Gui::Appearance);
  {
    SettingsPages pages;
    auto *combo = must<QComboBox>(pages, "comboAppearance");
    QCOMPARE(combo->currentData().toString(), QStringLiteral("deskflow"));
    QCOMPARE(combo->currentText(), QStringLiteral("Deskflow"));
    QVERIFY(!Settings::proxy().contains(Settings::Gui::Appearance));
    // what the card offers is the two looks that there are, with the words that the person knows
    QCOMPARE(combo->count(), 2);
    QCOMPARE(combo->itemData(0).toString(), QStringLiteral("deskflow"));
    QCOMPARE(combo->itemData(1).toString(), QStringLiteral("system"));
    // and it says when the choice is used
    const auto row = qobject_cast<SettingRow *>(combo->parentWidget());
    QVERIFY(row);
    QCOMPARE(row->description(), QStringLiteral("Takes effect the next time the application starts."));
    QCOMPARE(effectOf(Settings::Gui::Appearance), std::optional<Effect>(Effect::Live));

    // the way back: the choice is written at once, as the word that the application reads at its start
    choose(combo, QStringLiteral("system"));
    QCOMPARE(Settings::value(Settings::Gui::Appearance).toString(), QStringLiteral("system"));
    QVERIFY(Settings::proxy().contains(Settings::Gui::Appearance));
    QCOMPARE(deskflow::gui::style::configuredMode(), deskflow::gui::style::Mode::System);
  }
  // the next start: the card shows the choice, and does not give it back
  {
    SettingsPages pages;
    auto *combo = must<QComboBox>(pages, "comboAppearance");
    QCOMPARE(combo->currentData().toString(), QStringLiteral("system"));
    choose(combo, QStringLiteral("deskflow"));
    QCOMPARE(Settings::value(Settings::Gui::Appearance).toString(), QStringLiteral("deskflow"));
    QCOMPARE(deskflow::gui::style::configuredMode(), deskflow::gui::style::Mode::Deskflow);
  }
  Settings::setValue(Settings::Gui::Appearance);
}

void SettingsPagesTests::effects_theSettingsThatTheCoreReadsNeedARestart()
{
  // the dialogs of the system look restart a running core after every Save: the pages ask for it for these, which are
  // the settings that the core process reads, and for no others
  for (const auto &entry : dialogKeys()) {
    const auto effect = effectOf(entry.key);
    QVERIFY2(effect.has_value(), qPrintable(entry.key));
    QVERIFY2(*effect == entry.effect, qPrintable(entry.key));
  }
  const auto restart = keysWith(Effect::RestartSharing);
  for (const auto &key :
       {Settings::Core::Port, Settings::Core::Interface, Settings::Server::EnableClipboard, Settings::Server::Heartbeat,
        Settings::Server::Protocol, Settings::Server::ExternalConfig}) {
    QVERIFY2(restart.contains(key), qPrintable(key));
  }
  const auto live = keysWith(Effect::Live);
  for (const auto &key : {Settings::Core::Language, Settings::Log::Level, Settings::Gui::ShowVersionInTitle})
    QVERIFY2(live.contains(key), qPrintable(key));
  QVERIFY(restart.size() + live.size() >= dialogKeys().size());
}

void SettingsPagesTests::pages_inputIndicatorDefaultsAndDependencies()
{
#ifndef Q_OS_MACOS
  QSKIP("The input indicator settings card is macOS-only");
#else
  SettingsPages pages;
  auto *enabled = must<ToggleSwitch>(pages, "cbInputIndicator");
  auto *position = must<QComboBox>(pages, "comboInputIndicatorPosition");
  auto *opacity = must<QSpinBox>(pages, "sbInputIndicatorOpacity");
  auto *tone = must<QComboBox>(pages, "comboInputIndicatorTone");
  auto *note = must<InlineHint>(pages, "hintInputIndicatorSafety");

  QVERIFY(!enabled->isChecked());
  QCOMPARE(position->currentData().toString(), QStringLiteral("cursorBottomRight"));
  QCOMPARE(opacity->value(), 85);
  QCOMPARE(tone->currentData().toString(), QStringLiteral("automatic"));
  QVERIFY(!find<QSpinBox>(pages, "sbInputIndicatorDwell"));
  QCOMPARE(Settings::value(Settings::Gui::InputIndicatorDwellMs).toInt(), 1500);
  QVERIFY(!position->isEnabled());
  QVERIFY(!opacity->isEnabled());
  QVERIFY(!tone->isEnabled());
  QVERIFY(note->text().contains(QStringLiteral("input-source")));
  QVERIFY(note->text().contains(QStringLiteral("mouse pointer")));
  QVERIFY(note->text().contains(QStringLiteral("Accessibility")));
  QVERIFY(note->text().contains(QStringLiteral("does not read")));
  QVERIFY(!note->text().contains(QStringLiteral("Password")));

  for (const auto &key :
       {Settings::Gui::InputIndicatorEnabled, Settings::Gui::InputIndicatorPosition,
        Settings::Gui::InputIndicatorDwellMs, Settings::Gui::InputIndicatorOpacityPercent,
        Settings::Gui::InputIndicatorTone}) {
    QCOMPARE(effectOf(key), std::optional<Effect>(Effect::Live));
  }

  enabled->setChecked(true);
  QVERIFY(position->isEnabled());
  QVERIFY(opacity->isEnabled());
  QVERIFY(tone->isEnabled());

  choose(position, QStringLiteral("cursorTopLeft"));
  opacity->setValue(90);
  choose(tone, QStringLiteral("dark"));
  QCOMPARE(Settings::value(Settings::Gui::InputIndicatorPosition).toString(), QStringLiteral("cursorTopLeft"));
  QCOMPARE(Settings::value(Settings::Gui::InputIndicatorOpacityPercent).toInt(), 90);
  QCOMPARE(Settings::value(Settings::Gui::InputIndicatorTone).toString(), QStringLiteral("dark"));
#endif
}

void SettingsPagesTests::pages_areInTheOrderOfTheSidebar_withTitlesAndCards()
{
  SettingsPages pages;
  QStringList ids;
  for (const auto &entry : pages.entries()) {
    ids << entry.id;
    QVERIFY2(!entry.title().isEmpty(), qPrintable(entry.id));
    QVERIFY(!entry.icon.isEmpty());
    QCOMPARE(entry.page->pageId(), entry.id);
    QCOMPARE(entry.page->title(), entry.title());
    QVERIFY2(!entry.page->cards().isEmpty(), qPrintable(entry.id));
    // (the card of the Cloudflare Tunnel holds the group of the dialog of the client, which is not a list of rows)
    for (auto *card : entry.page->cards())
      QVERIFY2(
          !card->rows().isEmpty() || card->objectName() == QStringLiteral("cardClientTunnel"), qPrintable(entry.id)
      );
    // no mnemonic in a title
    QVERIFY2(
        !entry.title().contains(QLatin1Char('&')) || entry.title().contains(QStringLiteral("& ")),
        qPrintable(entry.title())
    );
  }
  QCOMPARE(
      ids,
      (QStringList{
          QStringLiteral("settings.security"), QStringLiteral("settings.display"), QStringLiteral("settings.keyboard"),
          QStringLiteral("settings.network"), QStringLiteral("settings.client"), QStringLiteral("settings.general"),
          QStringLiteral("settings.advanced")
      })
  );
  QCOMPARE(SettingsPages::generalId(), QStringLiteral("settings.general"));
  QCOMPARE(pages.page(QStringLiteral("settings.network"))->title(), QStringLiteral("Network"));
  QCOMPARE(pages.page(QStringLiteral("settings.client"))->title(), QStringLiteral("Client"));
  QCOMPARE(pages.page(QStringLiteral("settings.security"))->title(), QStringLiteral("Security & clipboard"));
  QCOMPARE(pages.page(QStringLiteral("settings.display"))->title(), QStringLiteral("Display & monitors"));
  QCOMPARE(pages.page(QStringLiteral("settings.keyboard"))->title(), QStringLiteral("Keyboard & mouse"));
  QVERIFY(!pages.page(QStringLiteral("settings.nothing")));
}

void SettingsPagesTests::pages_changeIsWrittenAtOnce_andNothingIsWrittenForNoChange()
{
  SettingsPages pages;
  QSignalSpy applied(&pages, &SettingsPages::applied);
  auto *port = must<QSpinBox>(pages, "sbPort");
  QCOMPARE(port->value(), Settings::value(Settings::Core::Port).toInt());

  // a number: the file has it as soon as the spin box has it
  port->setValue(24811);
  QCOMPARE(stored(Settings::Core::Port), QStringLiteral("24811"));
  QCOMPARE(applied.count(), 1);
  QCOMPARE(applied.first().first().toString(), Settings::Core::Port);

  // a switch
  auto *guiDebug = must<ToggleSwitch>(pages, "cbGuiDebug");
  const bool before = Settings::value(Settings::Log::GuiDebug).toBool();
  guiDebug->setChecked(!before);
  QCOMPARE(Settings::value(Settings::Log::GuiDebug).toBool(), !before);
  QCOMPARE(stored(Settings::Log::GuiDebug), before ? QStringLiteral("false") : QStringLiteral("true"));

  // a choice of words that are stored as a boolean
  auto *autoHide = must<QComboBox>(pages, "comboAutoHide");
  choose(autoHide, true);
  QVERIFY(Settings::value(Settings::Gui::Autohide).toBool());
  choose(autoHide, false);
  QVERIFY(!Settings::value(Settings::Gui::Autohide).toBool());

  // a text, written when the field is left
  auto *enter = must<QLineEdit>(pages, "lineCommandEnter");
  enter->setText(QStringLiteral("not left yet"));
  QVERIFY(Settings::value(Settings::Core::ComputerEnterCommand).toString() != QStringLiteral("not left yet"));
  Q_EMIT enter->editingFinished();
  QCOMPARE(Settings::value(Settings::Core::ComputerEnterCommand).toString(), QStringLiteral("not left yet"));

  // the same value again: the file is not touched and nothing is applied
  applied.clear();
  const auto content = settingsFileContent();
  port->setValue(24811);
  Q_EMIT enter->editingFinished();
  guiDebug->setChecked(!before);
  QCOMPARE(applied.count(), 0);
  QCOMPARE(settingsFileContent(), content);
}

void SettingsPagesTests::pages_writeWhatTheDialogWritesForTheSameChanges()
{
  const auto folder = m_dir.path();
  struct Target
  {
    bool autoHide, autoUpdate, preventSleep, closeToTray, mono, requireClient, tls, guiDebug, showVersion, logToFile,
        enterOn, exitOn;
  };
  using S = Settings;
  // every one of them the other way from what it is by default
  Target t{
      !S::defaultValue(S::Gui::Autohide).toBool(),
      !S::defaultValue(S::Gui::AutoUpdateCheck).toBool(),
      !S::defaultValue(S::Core::PreventSleep).toBool(),
      !S::defaultValue(S::Gui::CloseToTray).toBool(),
      !S::defaultValue(S::Gui::SymbolicTrayIcon).toBool(),
      !S::defaultValue(S::Security::CheckPeers).toBool(),
      !S::defaultValue(S::Security::TlsEnabled).toBool(),
      !S::defaultValue(S::Log::GuiDebug).toBool(),
      !S::defaultValue(S::Gui::ShowVersionInTitle).toBool(),
      !S::defaultValue(S::Log::ToFile).toBool(),
      true,
      true,
  };
  const auto certificate = folder + QStringLiteral("/cert.pem");
  const auto logFile = folder + QStringLiteral("/deskflow.log");

  // through the pages
  QStringList keys;
  {
    forgetAll();
    SettingsPages pages;
    keys = pages.binder()->keys();
    must<QSpinBox>(pages, "sbPort")->setValue(24831);
    choose(must<QComboBox>(pages, "comboLogLevel"), QStringLiteral("DEBUG"));
    setSwitch(must<ToggleSwitch>(pages, "cbLogToFile"), t.logToFile);
    type(must<QLineEdit>(pages, "lineLogFilename"), logFile);
    choose(must<QComboBox>(pages, "comboAutoHide"), t.autoHide);
    setSwitch(must<ToggleSwitch>(pages, "cbAutoUpdate"), t.autoUpdate);
    setSwitch(must<ToggleSwitch>(pages, "cbPreventSleep"), t.preventSleep);
    setSwitch(must<ToggleSwitch>(pages, "cbTlsEnabled"), t.tls);
    type(must<QLineEdit>(pages, "lineTlsCertPath"), certificate);
    choose(must<QComboBox>(pages, "comboTlsKeyLength"), 4096);
    choose(must<QComboBox>(pages, "comboCloseToTray"), t.closeToTray);
    choose(must<QComboBox>(pages, "comboTrayIcon"), t.mono);
    setSwitch(must<ToggleSwitch>(pages, "cbRequireClientCert"), t.requireClient);
    setSwitch(must<ToggleSwitch>(pages, "cbGuiDebug"), t.guiDebug);
    setSwitch(must<ToggleSwitch>(pages, "cbShowVersion"), t.showVersion);
    setSwitch(must<ToggleSwitch>(pages, "cbCommandEnter"), t.enterOn);
    type(must<QLineEdit>(pages, "lineCommandEnter"), QStringLiteral("echo in"));
    setSwitch(must<ToggleSwitch>(pages, "cbCommandExit"), t.exitOn);
    type(must<QLineEdit>(pages, "lineCommandExit"), QStringLiteral("echo out"));
  }
  QMap<QString, QString> viaPages;
  for (const auto &key : keys)
    viaPages[key] = Settings::value(key).toString();

  // through the dialog, with the same values
  forgetAll();
  ServerConfig config;
  {
    SettingsDialog dialog(nullptr, config);
    auto check = [&](const char *name) { return dialog.findChild<QCheckBox *>(QString::fromLatin1(name)); };
    auto radio = [&](const char *name) { return dialog.findChild<QRadioButton *>(QString::fromLatin1(name)); };
    auto group = [&](const char *name) { return dialog.findChild<QGroupBox *>(QString::fromLatin1(name)); };
    auto line = [&](const char *name) { return dialog.findChild<QLineEdit *>(QString::fromLatin1(name)); };
    dialog.findChild<QSpinBox *>(QStringLiteral("sbPort"))->setValue(24831);
    auto *level = dialog.findChild<QComboBox *>(QStringLiteral("comboLogLevel"));
    level->setCurrentIndex(level->findData(QStringLiteral("DEBUG")));
    group("groupLogToFile")->setChecked(t.logToFile);
    line("lineLogFilename")->setText(logFile);
    radio(t.autoHide ? "rbAutoHide" : "rbShowOnStart")->setChecked(true);
    check("cbAutoUpdate")->setChecked(t.autoUpdate);
    check("cbPreventSleep")->setChecked(t.preventSleep);
    group("groupSecurity")->setChecked(t.tls);
    line("lineTlsCertPath")->setText(certificate);
    auto *keyLength = dialog.findChild<QComboBox *>(QStringLiteral("comboTlsKeyLength"));
    keyLength->setCurrentIndex(keyLength->findText(QStringLiteral("4096")));
    radio(t.closeToTray ? "rbCloseToTray" : "rbExitOnClose")->setChecked(true);
    radio(t.mono ? "rbIconMono" : "rbIconColorful")->setChecked(true);
    check("cbRequireClientCert")->setChecked(t.requireClient);
    check("cbGuiDebug")->setChecked(t.guiDebug);
    check("cbShowVersion")->setChecked(t.showVersion);
    check("cbRunEnterCommand")->setChecked(t.enterOn);
    line("lineCommandEnter")->setText(QStringLiteral("echo in"));
    check("cbRunExitCommand")->setChecked(t.exitOn);
    line("lineCommandExit")->setText(QStringLiteral("echo out"));
    // accept() of the dialog is private to it: it is the one of QDialog that the signal of the button reaches
    static_cast<QDialog &>(dialog).accept();
  }

  // what the two ways of saving left is the same for every setting that the pages have (the language and the look are
  // not in the dialog: the language is the dialog's own, and the look is new)
  int compared = 0;
  for (const auto &key : keys) {
    if (key == Settings::Core::Language || key == Settings::Gui::Appearance)
      continue;
    QVERIFY2(
        Settings::value(key).toString() == viaPages.value(key),
        qPrintable(
            key + QStringLiteral(": ") + viaPages.value(key) + QStringLiteral(" / ") + Settings::value(key).toString()
        )
    );
    ++compared;
  }
  QVERIFY(compared >= 18);
  QCOMPARE(Settings::value(Settings::Core::Port).toInt(), 24831);
}

void SettingsPagesTests::pages_writeWhatTheServerDialogWritesForTheSameChanges()
{
  using S = Settings;
  const auto configFile = m_dir.filePath(QStringLiteral("server.conf"));
  QVERIFY2(touch(configFile), qPrintable(configFile));
  const bool windows = deskflow::platform::isWindows();
  // every one of them the other way from what it is by default
  const bool clipboard = !S::defaultValue(S::Server::EnableClipboard).toBool();
  const bool textOnly = !S::defaultValue(S::Server::ClipboardTextOnly).toBool();
  const bool heartbeat = !S::defaultValue(S::Server::EnableHeartbeat).toBool();
  const bool delay = !S::defaultValue(S::Server::EnableSwitchDelay).toBool();
  const bool doubleTap = !S::defaultValue(S::Server::EnableSwitchDoubleTap).toBool();
  const bool disableLock = !S::defaultValue(S::Server::DisableLockToComputer).toBool();
  const bool lockAtStart = !S::defaultValue(S::Server::DefaultLockToComputerState).toBool();
  const bool relative = !S::defaultValue(S::Server::RelativeMouseMoves).toBool();
  const bool foreground = !S::defaultValue(S::Server::Win32KeepForeground).toBool();
  const bool multiMonitor = !S::defaultValue(S::Server::ExperimentalMultiMonitor).toBool();
  const auto protocol = networkProtocolToOption(NetworkProtocol::Synergy);

  // the permission of the portal of a desktop that was given for a clipboard, and is to be asked for again
  auto start = [] {
    forgetAll();
    Settings::setValue(Settings::Server::XdpRestoreToken, QStringLiteral("token"));
    Settings::setValue(Settings::Server::XdpClipboardRetried, true);
  };

  // through the pages
  QStringList keys;
  start();
  {
    SettingsPages pages;
    keys = pages.binder()->keys();
    setSwitch(must<ToggleSwitch>(pages, "cbEnableClipboard"), clipboard);
    must<QSpinBox>(pages, "sbClipboardSizeLimit")->setValue(7);
    setSwitch(must<ToggleSwitch>(pages, "cbClipboardTextOnly"), textOnly);
    setSwitch(must<ToggleSwitch>(pages, "cbHeartbeat"), heartbeat);
    must<QSpinBox>(pages, "sbHeartbeat")->setValue(7000);
    choose(must<QComboBox>(pages, "comboProtocol"), protocol);
    setSwitch(must<ToggleSwitch>(pages, "cbSwitchDelay"), delay);
    must<QSpinBox>(pages, "sbSwitchDelay")->setValue(330);
    setSwitch(must<ToggleSwitch>(pages, "cbSwitchDoubleTap"), doubleTap);
    must<QSpinBox>(pages, "sbSwitchDoubleTap")->setValue(440);
    setSwitch(must<ToggleSwitch>(pages, "cbDisableLockToComputer"), disableLock);
    setSwitch(must<ToggleSwitch>(pages, "cbDefaultLockToComputerState"), lockAtStart);
    setSwitch(must<ToggleSwitch>(pages, "cbRelativeMouseMoves"), relative);
    if (windows)
      setSwitch(must<ToggleSwitch>(pages, "cbWin32KeepForeground"), foreground);
    setSwitch(must<ToggleSwitch>(pages, "cbExperimentalMultiMonitor"), multiMonitor);
    type(must<QLineEdit>(pages, "lineConfigFile"), configFile);
    setSwitch(must<ToggleSwitch>(pages, "cbExternalConfig"), true);
  }
  QMap<QString, QString> viaPages;
  for (const auto &key : keys)
    viaPages[key] = Settings::value(key).toString();
  const auto tokenViaPages = Settings::value(Settings::Server::XdpRestoreToken).toString();
  const auto retriedViaPages = Settings::value(Settings::Server::XdpClipboardRetried).toString();

  // through the dialog of the server, with the same values
  start();
  {
    ServerConfig config;
    ServerConfigDialog dialog(nullptr, config);
    auto button = [&](const char *name) { return dialog.findChild<QAbstractButton *>(QString::fromLatin1(name)); };
    auto spin = [&](const char *name) { return dialog.findChild<QSpinBox *>(QString::fromLatin1(name)); };
    button("cbEnableClipboard")->setChecked(clipboard);
    spin("sbClipboardSizeLimit")->setValue(7);
    button("cbClipboardTextOnly")->setChecked(textOnly);
    button("cbHeartbeat")->setChecked(heartbeat);
    spin("sbHeartbeat")->setValue(7000);
    button("rbProtocolSynergy")->setChecked(true);
    button("cbSwitchDelay")->setChecked(delay);
    spin("sbSwitchDelay")->setValue(330);
    button("cbSwitchDoubleTap")->setChecked(doubleTap);
    spin("sbSwitchDoubleTap")->setValue(440);
    button("cbDisableLockToComputer")->setChecked(disableLock);
    button("cbDefaultLockToComputerState")->setChecked(lockAtStart);
    button("cbRelativeMouseMoves")->setChecked(relative);
    if (windows)
      button("cbWin32KeepForeground")->setChecked(foreground);
    button("cbExperimentalMultiMonitor")->setChecked(multiMonitor);
    dialog.findChild<QLineEdit *>(QStringLiteral("lineConfigFile"))->setText(configFile);
    dialog.findChild<QGroupBox *>(QStringLiteral("groupExternalConfig"))->setChecked(true);
    Q_EMIT dialog.findChild<SettingsDialogButtonBox *>()->accepted();
  }

  // what the two ways of saving left is the same for every setting that the pages have, and for the permission
  int compared = 0;
  for (const auto &key : keys) {
    if (!key.startsWith(QStringLiteral("server/")))
      continue;
    QVERIFY2(
        Settings::value(key).toString() == viaPages.value(key),
        qPrintable(
            key + QStringLiteral(": ") + viaPages.value(key) + QStringLiteral(" / ") + Settings::value(key).toString()
        )
    );
    ++compared;
  }
  QVERIFY(compared >= 15);
  QCOMPARE(Settings::value(Settings::Server::XdpRestoreToken).toString(), tokenViaPages);
  QCOMPARE(Settings::value(Settings::Server::XdpClipboardRetried).toString(), retriedViaPages);
  QVERIFY(tokenViaPages.isEmpty());
  QCOMPARE(Settings::value(Settings::Server::ClipboardSize).toInt(), 7);
  QCOMPARE(Settings::value(Settings::Server::Protocol).toString(), protocol);
  QCOMPARE(Settings::value(Settings::Server::ExternalConfigFile).toString(), configFile);
}

namespace {
SettingRow *rowOf(QWidget *control)
{
  for (QWidget *parent = control->parentWidget(); parent; parent = parent->parentWidget()) {
    if (auto *row = qobject_cast<SettingRow *>(parent))
      return row;
  }
  return nullptr;
}
} // namespace

void SettingsPagesTests::pages_theServerSettingsFollowTheirRules()
{
  using S = Settings;
  S::setValue(S::Server::EnableClipboard, false);
  S::setValue(S::Server::ClipboardSize, 0);
  S::setValue(S::Server::XdpRestoreToken, QStringLiteral("token"));
  S::setValue(S::Server::XdpClipboardRetried, true);
  SettingsPages pages;
  auto *clipboard = must<ToggleSwitch>(pages, "cbEnableClipboard");
  auto *textOnly = must<ToggleSwitch>(pages, "cbClipboardTextOnly");
  auto *limit = must<QSpinBox>(pages, "sbClipboardSizeLimit");

  // the clipboard is off: nothing of it can be set
  QVERIFY(!clipboard->isChecked());
  QVERIFY(!textOnly->isEnabled());
  QVERIFY(!limit->isEnabled());

  // a change of something else leaves the permission of the portal alone
  must<ToggleSwitch>(pages, "cbHeartbeat")->setChecked(true);
  QCOMPARE(S::value(S::Server::XdpRestoreToken).toString(), QStringLiteral("token"));

  // the clipboard on: the size that was nothing is the usual one, the permission is asked for again
  clipboard->setChecked(true);
  QVERIFY(textOnly->isEnabled());
  QVERIFY(limit->isEnabled());
  QCOMPARE(S::value(S::Server::ClipboardSize).toInt(), S::defaultValue(S::Server::ClipboardSize).toInt());
  QCOMPARE(limit->value(), S::defaultValue(S::Server::ClipboardSize).toInt());
  QVERIFY(S::value(S::Server::XdpRestoreToken).toString().isEmpty());
  QVERIFY(!S::value(S::Server::XdpClipboardRetried).toBool());

  // text only takes the place of the size limit
  textOnly->setChecked(true);
  QVERIFY(!limit->isEnabled());
  textOnly->setChecked(false);
  QVERIFY(limit->isEnabled());

  // the waits are numbers of their switches
  for (const auto &[toggle, number] :
       {std::pair{"cbSwitchDelay", "sbSwitchDelay"}, std::pair{"cbSwitchDoubleTap", "sbSwitchDoubleTap"},
        std::pair{"cbHeartbeat", "sbHeartbeat"}}) {
    auto *on = must<ToggleSwitch>(pages, toggle);
    on->setChecked(true);
    QVERIFY2(must<QSpinBox>(pages, number)->isEnabled(), number);
    on->setChecked(false);
    QVERIFY2(!must<QSpinBox>(pages, number)->isEnabled(), number);
  }

  // the protocol is read the way the server reads it
  auto *protocol = must<QComboBox>(pages, "comboProtocol");
  choose(protocol, networkProtocolToOption(NetworkProtocol::Synergy));
  QCOMPARE(S::networkProtocol(), NetworkProtocol::Synergy);
  S::setValue(S::Server::Protocol, networkProtocolToOption(NetworkProtocol::Barrier));
  QCOMPARE(protocol->currentText(), QStringLiteral("Barrier"));

  // the file of a configuration has to be there, and its switch makes it a thing to set
  auto *external = must<ToggleSwitch>(pages, "cbExternalConfig");
  auto *path = must<QLineEdit>(pages, "lineConfigFile");
  external->setChecked(false);
  QVERIFY(!path->isEnabled());
  QVERIFY(!must<QPushButton>(pages, "btnBrowseConfigFile")->isEnabled());
  external->setChecked(true);
  QVERIFY(path->isEnabled());
  const auto before = S::value(S::Server::ExternalConfigFile).toString();
  type(path, m_dir.filePath(QStringLiteral("there-is-no-such-file.conf")));
  QCOMPARE(S::value(S::Server::ExternalConfigFile).toString(), before);
  QVERIFY(!rowOf(path)->error().isEmpty());
  const auto existing = m_dir.filePath(QStringLiteral("there.conf"));
  QVERIFY2(touch(existing), qPrintable(existing));
  type(path, existing);
  QCOMPARE(S::value(S::Server::ExternalConfigFile).toString(), existing);
  QVERIFY(rowOf(path)->error().isEmpty());

  // the foreground window is a thing of Windows only
  QCOMPARE(find<QWidget>(pages, "cbWin32KeepForeground") != nullptr, deskflow::platform::isWindows());

  // where this computer is a client, the settings of a server are shown and cannot be changed, and the card says why
  auto cardOf = [](QWidget *control) {
    for (QWidget *parent = control->parentWidget(); parent; parent = parent->parentWidget()) {
      if (auto *card = qobject_cast<SettingCard *>(parent))
        return card;
    }
    return static_cast<SettingCard *>(nullptr);
  };
  for (const char *name :
       {"cbEnableClipboard", "sbHeartbeat", "cbSwitchDelay", "cbExperimentalMultiMonitor", "cbDisableLockToComputer",
        "cbRelativeMouseMoves", "cbExternalConfig"}) {
    auto *card = cardOf(must<QWidget>(pages, name));
    QVERIFY2(card, name);
    auto *note = card->findChild<QLabel *>(QStringLiteral("noteServerOnly"));
    QVERIFY2(note, name);
    // the note is the first thing under the header
    QCOMPARE(card->layout()->itemAt(1)->widget(), note);
    S::setValue(S::Core::CoreMode, S::CoreMode::Server);
    QVERIFY2(card->isEnabled() && note->isHidden(), name);
    S::setValue(S::Core::CoreMode, S::CoreMode::Client);
    QVERIFY2(!card->isEnabled() && !note->isHidden(), name);
    S::setValue(S::Core::CoreMode, S::CoreMode::Server);
  }
  // a card of a setting that a client uses too is not marked
  QVERIFY(!cardOf(must<QWidget>(pages, "sbPort"))->findChild<QLabel *>(QStringLiteral("noteServerOnly")));

  // the names that a person who reads the page with a screen reader hears
  auto nameOf = [&](const char *name) {
    return QAccessible::queryAccessibleInterface(must<QWidget>(pages, name))->text(QAccessible::Name);
  };
  QCOMPARE(nameOf("cbExperimentalMultiMonitor"), QStringLiteral("Multi-monitor aware cursor mapping"));
  QCOMPARE(nameOf("sbHeartbeat"), QStringLiteral("Check clients every"));
  QCOMPARE(nameOf("cbDisableLockToComputer"), QStringLiteral("Disable lock to computer (scroll lock key)"));
  QCOMPARE(nameOf("sbClipboardSizeLimit"), QStringLiteral("Limit to"));
}

void SettingsPagesTests::pages_takeASettingThatChangedElsewhere()
{
  SettingsPages pages;
  auto *port = must<QSpinBox>(pages, "sbPort");
  auto *debug = must<ToggleSwitch>(pages, "cbGuiDebug");
  QSignalSpy applied(&pages, &SettingsPages::applied);

  Settings::setValue(Settings::Core::Port, 24850);
  QCOMPARE(port->value(), 24850);
  Settings::setValue(Settings::Log::GuiDebug, !debug->isChecked());
  QVERIFY(debug->isChecked() == Settings::value(Settings::Log::GuiDebug).toBool());
  // what comes from elsewhere is not a change that a person made: nothing is applied for it
  QCOMPARE(applied.count(), 0);

  // the language: the combo shows the native name of the setting (a test has no translation to list: it may be empty)
  QVERIFY(must<QComboBox>(pages, "comboLanguage"));
}

void SettingsPagesTests::pages_aValueThatIsNotGoodIsNotWritten_andSaysWhy()
{
  SettingsPages pages;
  auto *path = must<QLineEdit>(pages, "lineTlsCertPath");
  QSignalSpy applied(&pages, &SettingsPages::applied);
  const auto before = Settings::value(Settings::Security::Certificate).toString();

  // a file in a folder that is not there
  type(path, m_dir.path() + QStringLiteral("/no/such/folder/cert.pem"));
  QCOMPARE(Settings::value(Settings::Security::Certificate).toString(), before);
  QCOMPARE(applied.count(), 0);
  SettingRow *row = nullptr;
  for (QWidget *parent = path->parentWidget(); parent && !row; parent = parent->parentWidget())
    row = qobject_cast<SettingRow *>(parent);
  QVERIFY(row);
  QVERIFY(!row->error().isEmpty());
  QCOMPARE(row->error(), QStringLiteral("The folder of this file does not exist."));
  // the error is what a screen reader is told about the field
  QCOMPARE(path->accessibleDescription(), row->error());

  // an empty path
  type(path, QString());
  QVERIFY(!row->error().isEmpty());
  QCOMPARE(Settings::value(Settings::Security::Certificate).toString(), before);

  // a good one is written, and the error goes
  const auto good = m_dir.path() + QStringLiteral("/good.pem");
  type(path, good);
  QCOMPARE(Settings::value(Settings::Security::Certificate).toString(), good);
  QVERIFY(row->error().isEmpty());
  QCOMPARE(applied.count(), 1);

  // the port cannot be set to a number that is no port
  auto *port = must<QSpinBox>(pages, "sbPort");
  port->setValue(0);
  QCOMPARE(port->value(), 1);
}

void SettingsPagesTests::pages_whatDependsOnAnotherControlFollowsIt()
{
  SettingsPages pages;
  pages.page(QStringLiteral("settings.security"))->show();
  auto *tls = must<ToggleSwitch>(pages, "cbTlsEnabled");
  auto *fingerprint = must<QPushButton>(pages, "btnFingerprint");

  tls->setChecked(true);
  for (const char *name : {"lineTlsCertPath", "btnTlsCertPath", "comboTlsKeyLength", "btnTlsRegenCert"})
    QVERIFY2(find<QWidget>(pages, name)->isEnabled(), name);
  QVERIFY(!fingerprint->isHidden());

  // the encryption off: what belongs to it is off, and there is no fingerprint to show
  tls->setChecked(false);
  for (const char *name :
       {"lineTlsCertPath", "btnTlsCertPath", "comboTlsKeyLength", "btnTlsRegenCert", "cbRequireClientCert"})
    QVERIFY2(!find<QWidget>(pages, name)->isEnabled(), name);
  QVERIFY(fingerprint->isHidden());

  // the clients are asked for a certificate by a server only
  tls->setChecked(true);
  Settings::setValue(Settings::Core::CoreMode, Settings::CoreMode::Client);
  QVERIFY(!must<ToggleSwitch>(pages, "cbRequireClientCert")->isEnabled());
  Settings::setValue(Settings::Core::CoreMode, Settings::CoreMode::Server);
  QVERIFY(must<ToggleSwitch>(pages, "cbRequireClientCert")->isEnabled());

  // the file of the log and the commands follow their switches
  auto *toFile = must<ToggleSwitch>(pages, "cbLogToFile");
  toFile->setChecked(false);
  QVERIFY(!must<QLineEdit>(pages, "lineLogFilename")->isEnabled());
  toFile->setChecked(true);
  QVERIFY(must<QLineEdit>(pages, "lineLogFilename")->isEnabled());
  auto *enter = must<ToggleSwitch>(pages, "cbCommandEnter");
  enter->setChecked(false);
  QVERIFY(!must<QLineEdit>(pages, "lineCommandEnter")->isEnabled());
  enter->setChecked(true);
  QVERIFY(must<QLineEdit>(pages, "lineCommandEnter")->isEnabled());

  // a level of the log that is above Info says that it costs speed
  auto *level = must<QComboBox>(pages, "comboLogLevel");
  choose(level, QStringLiteral("INFO"));
  SettingRow *levelRow = nullptr;
  for (QWidget *parent = level->parentWidget(); parent && !levelRow; parent = parent->parentWidget())
    levelRow = qobject_cast<SettingRow *>(parent);
  QVERIFY(levelRow);
  QVERIFY(levelRow->description().isEmpty());
  choose(level, QStringLiteral("DEBUG"));
  QVERIFY(!levelRow->description().isEmpty());
}

namespace {
QString roleOf(QObject *object)
{
  auto *interface = QAccessible::queryAccessibleInterface(object);
  return interface ? QString::fromLatin1(QMetaEnum::fromType<QAccessible::Role>().valueToKey(interface->role()))
                   : QString();
}

// One line for each thing that a screen reader walks to: its role and its name
void dumpTree(QAccessibleInterface *interface, int depth, QStringList &lines)
{
  if (interface->state().invisible)
    return;
  lines << QStringLiteral("%1%2 \"%3\"")
               .arg(
                   QString(depth * 2, QLatin1Char(' ')),
                   QString::fromLatin1(QMetaEnum::fromType<QAccessible::Role>().valueToKey(interface->role())),
                   interface->text(QAccessible::Name)
               );
  for (int i = 0; i < interface->childCount(); ++i) {
    if (auto *child = interface->child(i))
      dumpTree(child, depth + 1, lines);
  }
}
} // namespace

void SettingsPagesTests::pages_haveTheRolesAndTheNamesOfTheirParts()
{
  SettingsPages pages;
  for (const auto &entry : pages.entries()) {
    auto *title = entry.page->titleLabel();
    QCOMPARE(roleOf(title), QStringLiteral("Heading"));
    auto *interface = QAccessible::queryAccessibleInterface(title);
    QCOMPARE(interface->text(QAccessible::Name), entry.title());
    for (auto *card : entry.page->cards())
      QCOMPARE(roleOf(card), QStringLiteral("Grouping"));
  }
  // the controls are named by the row that has them
  auto *port = must<QSpinBox>(pages, "sbPort");
  QCOMPARE(QAccessible::queryAccessibleInterface(port)->text(QAccessible::Name), QStringLiteral("Port"));
  auto *tls = must<ToggleSwitch>(pages, "cbTlsEnabled");
  QCOMPARE(
      QAccessible::queryAccessibleInterface(tls)->text(QAccessible::Name), QStringLiteral("Enable TLS Encryption")
  );
  QCOMPARE(
      QAccessible::queryAccessibleInterface(tls)->text(QAccessible::Description),
      QStringLiteral("Both computers must use the same TLS (encryption) setting.")
  );
  QCOMPARE(
      QAccessible::queryAccessibleInterface(must<QPushButton>(pages, "btnClearAllSettings"))->text(QAccessible::Name),
      QStringLiteral("Remove all settings")
  );
}

void SettingsPagesTests::pages_theTreeOfAScreenReaderHasHeadingsGroupsAndNamedControls()
{
  // the switches are switches (not check boxes) in the look of the application
  auto *style = new deskflow::gui::style::DeskflowStyle;
  QApplication::setStyle(style);
  deskflow::gui::style::Look look;
  look.reduceTransparency = true;
  look.reduceMotion = true;
  style->useLook(look);
  SettingsPages pages;
  QStringList all;
  for (const auto &entry : pages.entries()) {
    entry.page->resize(720, 560);
    entry.page->show();
    QCoreApplication::processEvents();
    QStringList lines;
    dumpTree(QAccessible::queryAccessibleInterface(entry.page), 0, lines);
    const auto tree = lines.join(QLatin1Char('\n'));
    all << QStringLiteral("== ") + entry.id << tree;
    // the title of the page is its heading, and each card is a group that is named by its title
    QVERIFY2(tree.contains(QStringLiteral("Heading \"%1\"").arg(entry.title())), qPrintable(entry.id + tree));
    for (const auto *card : entry.page->cards())
      QVERIFY2(tree.contains(QStringLiteral("Grouping \"%1\"").arg(card->title())), qPrintable(entry.id + tree));
    entry.page->hide();
  }
  const auto text = all.join(QLatin1Char('\n'));
  // a switch is a switch with the name of its row, a number and a choice are named by it too (a choice by its value,
  // and its row is its label)
  const auto switchName = QString::fromLatin1(QMetaEnum::fromType<QAccessible::Role>().valueToKey(switchRole()));
  QVERIFY2(text.contains(QStringLiteral("%1 \"Enable TLS Encryption\"").arg(switchName)), qPrintable(text));
  QVERIFY2(text.contains(QStringLiteral("%1 \"Enable clipboard sharing\"").arg(switchName)), qPrintable(text));
  QVERIFY2(text.contains(QRegularExpression(QStringLiteral("SpinBox \"Port\""))), qPrintable(text));
  QVERIFY2(text.contains(QRegularExpression(QStringLiteral("ComboBox \"[^\"]+\""))), qPrintable(text));
  // no control without a name
  QVERIFY2(
      !text.contains(QRegularExpression(QStringLiteral("(%1|SpinBox|Button) \"\"").arg(switchName))), qPrintable(text)
  );

  if (const auto path = qEnvironmentVariable("DESKFLOW_A11Y_DUMP"); !path.isEmpty()) {
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
      file.write(text.toUtf8() + '\n');
  }
}

void SettingsPagesTests::pages_theBackgroundServiceIsOnlyOnWindows()
{
  SettingsPages pages;
  if (deskflow::platform::isWindows() && !Settings::isPortableMode()) {
    QVERIFY(find<QWidget>(pages, "cbService"));
    QVERIFY(find<QWidget>(pages, "cbElevateDaemon"));
  } else {
    QVERIFY(!find<QWidget>(pages, "cbService"));
    QVERIFY(!find<QWidget>(pages, "cbElevateDaemon"));
    QVERIFY(!find<QWidget>(pages, "cardService"));
  }
}

// Every switch of the pages is given a click of the mouse and the key Space, as a person does, in both modes (the cards
// of the server and the cards of the client are disabled in the other mode), and what each paints is looked at. The
// seventh run of the checks of Windows found that the knob of every switch stood still: these tests had set the
// switches by code
void SettingsPagesTests::switches_everySwitchOfThePages_followsAClickAndTheKeySpace()
{
  auto *style = new deskflow::gui::style::DeskflowStyle;
  QApplication::setStyle(style);
  deskflow::gui::style::Look look;
  look.reduceTransparency = true;
  look.reduceMotion = true;
  style->useLook(look);

  QStringList tried;
  for (const auto mode : {Settings::CoreMode::Server, Settings::CoreMode::Client}) {
    Settings::setValue(Settings::Core::CoreMode, mode);
    SettingsPages pages;
    pages.binder()->refresh();
    for (const auto &entry : pages.entries()) {
      entry.page->resize(720, 560);
      entry.page->show();
      QCoreApplication::processEvents();
      const auto sweep = switchinput::sweepSwitches(entry.page);
      QVERIFY2(
          sweep.problems.isEmpty(), qPrintable(entry.id + QStringLiteral(": ") + sweep.problems.join(QLatin1Char('\n')))
      );
      tried << QStringLiteral("%1/%2:%3")
                   .arg(
                       mode == Settings::CoreMode::Server ? QStringLiteral("server") : QStringLiteral("client"),
                       entry.id
                   )
                   .arg(sweep.tried);
      entry.page->hide();
    }
  }
  // the scan is not looking at nothing: the pages have switches, and the ones of each mode are used in it
  int total = 0;
  for (const auto &line : tried)
    total += line.section(QLatin1Char(':'), -1).toInt();
  QVERIFY2(total >= 24, qPrintable(tried.join(QLatin1Char(' '))));
}

void SettingsPagesTests::banner_asksWhenAskedAndTellsItsAlert()
{
  RestartBanner banner;
  QSignalSpy asked(&banner, &RestartBanner::restartRequested);
  QVERIFY(banner.isHidden());
  QVERIFY(!banner.isPending());
  QCOMPARE(roleOf(&banner), QStringLiteral("AlertMessage"));
  QCOMPARE(QAccessible::queryAccessibleInterface(&banner)->text(QAccessible::Name), banner.text());

  banner.setPending(true);
  QVERIFY(banner.isPending());
  QVERIFY(!banner.isHidden());
  QCOMPARE(banner.button()->text(), QStringLiteral("Restart sharing"));
  QTest::mouseClick(banner.button(), Qt::LeftButton);
  QCOMPARE(asked.count(), 1);
  // it asks, it does not hide itself or restart anything: the window does that
  QVERIFY(banner.isPending());
  banner.setPending(false);
  QVERIFY(banner.isHidden());
}

QTEST_MAIN(SettingsPagesTests)
