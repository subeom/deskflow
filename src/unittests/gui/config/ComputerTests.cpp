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
#include <QFile>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>

namespace {

// What the diagnostic tools print, with the block to paste in the middle of it
const auto kMacOutput = QStringLiteral(
    "== Holes in the bounding box ==\n"
    "left   covered 1347/1347 lines (100.0%)   hole ranges: none\n"
    "\n"
    "== Paste-ready block (proposed experimental format; copy this whole block into the research thread) ==\n"
    "computer: subeoms-macbook-pro.local\n"
    "space: points x,y,w,h scale\n"
    "  monitor 0,0,2064,1347 scale=2.0 primary\n"
    "  monitor 2064,-300,2560,1440 scale=1.0\n"
    "\n"
    "Cross-check (also read-only):  system_profiler SPDisplaysDataType\n"
);

const auto kWindowsOutput = QStringLiteral(
    "Paste-ready block (proposed experimental format; copy it into the research thread):\r\n"
    "computer: SUBEOM-COPILOT\r\n"
    "space: virtual-desktop physical pixels x,y,w,h scale%\r\n"
    "  monitor -1920,360,1920,1080 scale=100%\r\n"
    "  monitor 0,0,2560,1440 scale=150% primary\r\n"
    "\r\n"
    "Nothing was changed, no input was sent, the cursor was not moved.\r\n"
);

const auto kStaggered = QStringLiteral("0,0,2560,1440;2560,0,1920,1080");

QPlainTextEdit *layoutEdit(const ComputerSettingsDialog &dialog)
{
  return dialog.findChild<QPlainTextEdit *>(QStringLiteral("textMonitorLayout"));
}

QLabel *layoutStatus(const ComputerSettingsDialog &dialog)
{
  return dialog.findChild<QLabel *>(QStringLiteral("lblMonitorLayoutStatus"));
}

QPushButton *importButton(const ComputerSettingsDialog &dialog)
{
  return dialog.findChild<QPushButton *>(QStringLiteral("btnImportMonitorLayout"));
}

// a file with \p bytes in it, in \p directory
QString writeFile(const QTemporaryDir &directory, const QString &name, const QByteArray &bytes)
{
  const auto path = directory.filePath(name);
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size())
    qFatal("cannot write %s", qPrintable(path));
  return path;
}

} // namespace

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

void ComputerTests::monitorLayout_isEmptyAndAddsNothingByDefault()
{
  // a configuration that predates the setting has no layout for a computer
  const auto name = QStringLiteral("layout-default");
  Settings::proxy().setValue("name", name);

  Computer computer;
  computer.loadSettings(Settings::proxy());
  QVERIFY(computer.monitorLayout().isEmpty());

  deskflow::server::Config config(nullptr);
  QVERIFY(config.addComputer(name.toStdString()));
  QVERIFY(config.getMonitorLayout(name.toStdString()) == nullptr);
  QCOMPARE(config.getOptions(name.toStdString())->count(kOptionClipboardSharing), size_t{0});
}

void ComputerTests::monitorLayout_isSavedLoadedAndReadByTheServer_data()
{
  QTest::addColumn<QString>("text");
  QTest::addColumn<QString>("kept");

  QTest::newRow("none") << QString() << QString();
  QTest::newRow("a layout") << kStaggered << kStaggered;
  QTest::newRow("a layout with white space around it")
      << "  0,0,2560,1440;\n2560,0,1920,1080\n " << "0,0,2560,1440;\n2560,0,1920,1080";
  QTest::newRow("a layout that is wrong") << "abc\n1,2" << "abc\n1,2";
}

void ComputerTests::monitorLayout_isSavedLoadedAndReadByTheServer()
{
  QFETCH(QString, text);
  QFETCH(QString, kept);
  const auto name = QStringLiteral("layout-round-trip");
  const auto key = Settings::Computer::MonitorLayout.arg(name);
  Settings::proxy().setValue("name", name);
  Settings::setValue(key, text.isEmpty() ? QVariant() : QVariant(text));

  Computer computer;
  computer.loadSettings(Settings::proxy());
  QCOMPARE(computer.monitorLayout(), kept);

  // saving writes it back, and a layout of nothing is no key at all
  Settings::setValue(key, QStringLiteral("something that saving has to replace"));
  computer.saveSettings(Settings::proxy());
  QCOMPARE(Settings::value(key).toString(), kept);
  if (kept.isEmpty())
    QVERIFY(!Settings::value(key).isValid() || Settings::value(key).toString().isEmpty());

  Computer reloaded;
  reloaded.loadSettings(Settings::proxy());
  QCOMPARE(reloaded.monitorLayout(), kept);
  QVERIFY(reloaded == computer);

  // it goes where the computer goes
  QByteArray serialized;
  QDataStream output(&serialized, QIODevice::WriteOnly);
  output << computer;
  Computer copy;
  QDataStream input(&serialized, QIODevice::ReadOnly);
  input >> copy;
  QCOMPARE(copy.monitorLayout(), kept);
  QVERIFY(copy == computer);

  // and the server reads what was saved, as the text that it parses
  deskflow::server::Config config(nullptr);
  QVERIFY(config.addComputer(name.toStdString()));
  const auto *layout = config.getMonitorLayout(name.toStdString());
  if (kept.isEmpty()) {
    QVERIFY(layout == nullptr);
  } else {
    QVERIFY(layout != nullptr);
    QCOMPARE(QString::fromStdString(layout->text), kept);
  }
  Settings::setValue(key);
}

void ComputerTests::monitorLayout_takesPartInEquality()
{
  const auto name = QStringLiteral("layout-equality");
  const auto key = Settings::Computer::MonitorLayout.arg(name);
  Settings::proxy().setValue("name", name);

  Settings::setValue(key);
  Computer without;
  without.loadSettings(Settings::proxy());
  Settings::setValue(key, kStaggered);
  Computer with;
  with.loadSettings(Settings::proxy());
  QVERIFY(!(with == without));
  Settings::setValue(key);
}

void ComputerTests::monitorLayoutDialog_isOffUntilTheExperimentIsOn()
{
  const auto name = QStringLiteral("layout-dialog-off");
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, false);
  Settings::setValue(Settings::Computer::MonitorLayout.arg(name), kStaggered);
  Settings::proxy().setValue("name", name);
  Computer computer;
  computer.loadSettings(Settings::proxy());

  ComputerSettingsDialog dialog(nullptr, &computer);
  QVERIFY(layoutEdit(dialog) != nullptr && layoutStatus(dialog) != nullptr && importButton(dialog) != nullptr);
  QVERIFY(!layoutEdit(dialog)->isEnabled());
  QVERIFY(!importButton(dialog)->isEnabled());
  // what it holds is shown, and the way to use it is said
  QCOMPARE(layoutEdit(dialog)->toPlainText(), kStaggered);
  QVERIFY2(
      layoutStatus(dialog)->text().contains(QStringLiteral("Experimental: multi-monitor aware cursor mapping")),
      qPrintable(layoutStatus(dialog)->text())
  );

  // saving the other things in the dialog leaves the layout as it is
  dialog.accept();
  QCOMPARE(computer.monitorLayout(), kStaggered);

  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  ComputerSettingsDialog on(nullptr, &computer);
  QVERIFY(layoutEdit(on)->isEnabled());
  QVERIFY(importButton(on)->isEnabled());
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor);
  Settings::setValue(Settings::Computer::MonitorLayout.arg(name));
}

void ComputerTests::monitorLayoutDialog_followsTheServerDialog()
{
  // the check box of the server dialog that opened this one counts before it is saved
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, false);
  Computer computer(QStringLiteral("layout-dialog-parent"));
  QWidget serverDialog;
  serverDialog.setProperty("experimentalMultiMonitor", true);
  ComputerSettingsDialog on(&serverDialog, &computer);
  QVERIFY(layoutEdit(on)->isEnabled());

  serverDialog.setProperty("experimentalMultiMonitor", false);
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  ComputerSettingsDialog off(&serverDialog, &computer);
  QVERIFY(!layoutEdit(off)->isEnabled());
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor);
}

void ComputerTests::monitorLayoutDialog_saysWhetherTheLayoutIsValid()
{
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Computer computer(QStringLiteral("layout-dialog-status"));
  ComputerSettingsDialog dialog(nullptr, &computer);
  auto *edit = layoutEdit(dialog);
  auto *status = layoutStatus(dialog);
  QVERIFY(edit != nullptr && status != nullptr);
  QVERIFY(edit->isEnabled());

  // nothing is not wrong: the computer is mapped as it always was
  QVERIFY2(status->text().contains(QStringLiteral("No layout")), qPrintable(status->text()));

  edit->setPlainText(kStaggered);
  QVERIFY2(status->text().contains(QStringLiteral("Valid")), qPrintable(status->text()));
  QVERIFY2(status->text().contains(QStringLiteral("2 monitors")), qPrintable(status->text()));
  QVERIFY2(status->text().contains(QStringLiteral("0,0 4480x1440")), qPrintable(status->text()));

  // a rectangle is valid and changes nothing, which is said
  edit->setPlainText(QStringLiteral("0,0,1920,1080;1920,0,1920,1080"));
  QVERIFY2(status->text().contains(QStringLiteral("Valid")), qPrintable(status->text()));
  QVERIFY2(status->text().contains(QStringLiteral("rectangle")), qPrintable(status->text()));

  // what is wrong is said where
  edit->setPlainText(QStringLiteral("0,0,1920,1080\n1920,0,x,1080"));
  QVERIFY2(status->text().contains(QStringLiteral("Not valid")), qPrintable(status->text()));
  QVERIFY2(status->text().contains(QStringLiteral("line 2")), qPrintable(status->text()));

  edit->setPlainText(QStringLiteral("0,0,100,100;500,500,100,100"));
  QVERIFY2(status->text().contains(QStringLiteral("not connected")), qPrintable(status->text()));

  edit->clear();
  QVERIFY2(status->text().contains(QStringLiteral("No layout")), qPrintable(status->text()));
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor);
}

void ComputerTests::monitorLayoutDialog_savesTheLayoutAsTheServerReadsIt()
{
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Computer computer(QStringLiteral("layout-dialog-save"));
  ComputerSettingsDialog dialog(nullptr, &computer);
  // the block that a tool prints, as a person pastes it: from its first line to the empty line after it
  const auto block = kWindowsOutput.mid(kWindowsOutput.indexOf(QStringLiteral("computer:")));
  layoutEdit(dialog)->setPlainText(block.left(block.indexOf(QStringLiteral("\r\n\r\n"))));
  dialog.accept();
  QCOMPARE(computer.monitorLayout(), QStringLiteral("-1920,360,1920,1080;0,0,2560,1440"));

  computer.saveSettings(Settings::proxy());
  deskflow::server::Config config(nullptr);
  QVERIFY(config.addComputer(computer.name().toStdString()));
  const auto *layout = config.getMonitorLayout(computer.name().toStdString());
  QVERIFY(layout != nullptr);
  QVERIFY(layout->parsed);
  QCOMPARE(layout->parsed.layout->monitors().size(), size_t{2});

  // a layout that is taken away is taken away
  ComputerSettingsDialog again(nullptr, &computer);
  QCOMPARE(layoutEdit(again)->toPlainText(), QStringLiteral("-1920,360,1920,1080;0,0,2560,1440"));
  layoutEdit(again)->clear();
  again.accept();
  QVERIFY(computer.monitorLayout().isEmpty());
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor);
  Settings::setValue(Settings::Computer::MonitorLayout.arg(computer.name()));
}

void ComputerTests::monitorLayoutDialog_doesNotSaveALayoutThatIsWrong()
{
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Computer computer(QStringLiteral("layout-dialog-wrong"));
  ComputerSettingsDialog dialog(nullptr, &computer);
  layoutEdit(dialog)->setPlainText(QStringLiteral("0,0,1920"));
  dialog.accept();
  QCOMPARE(dialog.result(), static_cast<int>(QDialog::Rejected));
  QVERIFY(computer.monitorLayout().isEmpty());
  QVERIFY2(
      layoutStatus(dialog)->text().contains(QStringLiteral("Not valid")), qPrintable(layoutStatus(dialog)->text())
  );

  // and once it is right it is saved
  layoutEdit(dialog)->setPlainText(QStringLiteral("0,0,1920,1080"));
  dialog.accept();
  QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
  QCOMPARE(computer.monitorLayout(), QStringLiteral("0,0,1920,1080"));
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor);
}

void ComputerTests::monitorLayoutDialog_importsWhatTheDiagnosticToolsPrint_data()
{
  QTest::addColumn<QByteArray>("file");
  QTest::addColumn<QString>("expected");

  const auto utf16 = [](const QString &text) {
    // what a PowerShell that redirects its output writes: UTF-16 with a byte order mark
    QByteArray bytes("\xFF\xFE", 2);
    bytes.append(reinterpret_cast<const char *>(text.utf16()), static_cast<qsizetype>(text.size() * 2));
    return bytes;
  };

  QTest::newRow("macOS") << kMacOutput.toUtf8() << "0,0,2064,1347\n2064,-300,2560,1440";
  QTest::newRow("Windows") << kWindowsOutput.toUtf8() << "-1920,360,1920,1080\n0,0,2560,1440";
  QTest::newRow("Windows with a byte order mark")
      << (QByteArray("\xEF\xBB\xBF") + kWindowsOutput.toUtf8()) << "-1920,360,1920,1080\n0,0,2560,1440";
  QTest::newRow("Windows PowerShell redirect (UTF-16)")
      << utf16(kWindowsOutput) << "-1920,360,1920,1080\n0,0,2560,1440";
}

void ComputerTests::monitorLayoutDialog_importsWhatTheDiagnosticToolsPrint()
{
  QFETCH(QByteArray, file);
  QFETCH(QString, expected);
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto path = writeFile(directory, QStringLiteral("mmdiag.txt"), file);

  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Computer computer(QStringLiteral("layout-dialog-import"));
  ComputerSettingsDialog dialog(nullptr, &computer);
  QVERIFY(dialog.importMonitorLayout(path));
  // one monitor to a line, to read it
  QCOMPARE(layoutEdit(dialog)->toPlainText(), expected);
  QVERIFY2(layoutStatus(dialog)->text().contains(QStringLiteral("Valid")), qPrintable(layoutStatus(dialog)->text()));
  dialog.accept();
  QCOMPARE(computer.monitorLayout().count(QLatin1Char(';')), 1);
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor);
}

void ComputerTests::monitorLayoutDialog_importOfAFileThatIsNotAFit_saysSoAndChangesNothing()
{
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  Computer computer(QStringLiteral("layout-dialog-import-bad"));
  ComputerSettingsDialog dialog(nullptr, &computer);
  layoutEdit(dialog)->setPlainText(kStaggered);

  // no block in it
  const auto none = writeFile(directory, QStringLiteral("none.txt"), "computer: X\nnothing to see here\n");
  QVERIFY(!dialog.importMonitorLayout(none));
  QCOMPARE(layoutEdit(dialog)->toPlainText(), kStaggered);
  QVERIFY2(layoutStatus(dialog)->text().contains(QStringLiteral("no block")), qPrintable(layoutStatus(dialog)->text()));

  // a block that is not a layout is shown, to be put right, and is said not to be valid
  const auto bad = writeFile(
      directory, QStringLiteral("bad.txt"),
      "Paste-ready block (x):\ncomputer: X\n  monitor 0,0,100,100 scale=1.0\n  monitor 900,900,100,100 scale=1.0\n\n"
  );
  QVERIFY(!dialog.importMonitorLayout(bad));
  QVERIFY2(
      layoutStatus(dialog)->text().contains(QStringLiteral("Not valid")), qPrintable(layoutStatus(dialog)->text())
  );
  QVERIFY(layoutEdit(dialog)->toPlainText().contains(QStringLiteral("900,900,100,100")));

  // a file that is not there
  QVERIFY(!dialog.importMonitorLayout(directory.filePath(QStringLiteral("missing.txt"))));
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor);
}

QTEST_MAIN(ComputerTests)
