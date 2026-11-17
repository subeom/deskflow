/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerConfigDialogTests.h"

#include "common/Settings.h"
#include "dialogs/MonitorEditorDialog.h"
#include "gui/config/Computer.h"
#include "gui/config/ServerConfig.h"
#include "gui/dialogs/ComputerSettingsDialog.h"
#include "gui/dialogs/ServerConfigDialog.h"
#include "gui/widgets/ArrangementCanvas.h"
#include "gui/widgets/ArrangementPanel.h"
#include "gui/widgets/ComputerSetupView.h"
#include "gui/widgets/SettingsDialogButtonBox.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRect>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTemporaryFile>
#include <QTest>
#include <QTimer>

#include <optional>
#include <tuple>
#include <vector>

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

// the computers of the classic grid: three, with the server in the middle, and the displays of the user's MacBook
void writeThreeComputers()
{
  Settings::setValue(Settings::Core::ComputerName, QStringLiteral("srv"));
  auto &settings = Settings::proxy();
  settings.beginGroup(QStringLiteral("internalConfig"));
  settings.remove(QString());
  settings.beginWriteArray(QStringLiteral("screens"));
  for (int i = 0; i < 15; ++i) {
    settings.setArrayIndex(i);
    settings.setValue(
        QStringLiteral("name"), i == 6   ? QStringLiteral("PC")
                                : i == 7 ? QStringLiteral("srv")
                                : i == 8 ? QStringLiteral("MINI")
                                         : QString()
    );
  }
  settings.endArray();
  settings.endGroup();
  ServerConfig::overrideServerMonitors(
      std::vector<deskflow::server::MonitorRect>{{0, 0, 3440, 1440}, {753, 1440, 2064, 1347}}
  );
}

ArrangementPanel *panelOf(const ServerConfigDialog &dialog)
{
  return dialog.findChild<ArrangementPanel *>();
}

QPushButton *buttonNamed(const ServerConfigDialog &dialog, const char *name)
{
  return dialog.findChild<QPushButton *>(QString::fromLatin1(name));
}

ComputerSetupView *gridOf(const ServerConfigDialog &dialog)
{
  return dialog.findChild<ComputerSetupView *>(QStringLiteral("computerSetupView"));
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
  Settings::setValue(Settings::Server::UseArrangement);
  ServerConfig::overrideServerMonitors(std::nullopt);
  for (const auto &name : {"PC", "srv", "MINI"}) {
    for (const auto &key :
         {Settings::Computer::ArrangementX, Settings::Computer::ArrangementY, Settings::Computer::ArrangementZoom,
          Settings::Computer::ArrangementWidth, Settings::Computer::ArrangementHeight})
      Settings::setValue(key.arg(QString::fromLatin1(name)));
  }
  Settings::setValue(Settings::Server::ClipboardTextOnly);
  Settings::setValue(Settings::Server::EnableClipboard);
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor);
  Settings::setValue(Settings::Server::RememberArrangements);
  // the dialog writes these as soon as they are changed
  Settings::setValue(Settings::Server::ExternalConfig);
  Settings::setValue(Settings::Server::ExternalConfigFile);
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

void ServerConfigDialogTests::arrangement_isNotThereWhileTheExperimentIsOff()
{
  writeThreeComputers();
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);
  dialog.show();
  QVERIFY(panelOf(dialog) != nullptr);
  QVERIFY(!panelOf(dialog)->isVisibleTo(&dialog));
  QVERIFY(!buttonNamed(dialog, "btnUseArrangement")->isVisibleTo(&dialog));
  QVERIFY(gridOf(dialog)->isVisibleTo(&dialog));
}

void ServerConfigDialogTests::arrangement_isOfferedWhenTheExperimentIsOn_andTheClassicGridStays()
{
  writeThreeComputers();
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);
  dialog.show();
  experimentalBox(dialog)->setChecked(true);
  QVERIFY(buttonNamed(dialog, "btnUseArrangement")->isVisibleTo(&dialog));
  QVERIFY(buttonNamed(dialog, "btnUseArrangement")->text().contains(QStringLiteral("arrangement")));
  QVERIFY(gridOf(dialog)->isVisibleTo(&dialog));
  QVERIFY(!panelOf(dialog)->isVisibleTo(&dialog));

  // turned off, it is not offered
  experimentalBox(dialog)->setChecked(false);
  QVERIFY(!buttonNamed(dialog, "btnUseArrangement")->isVisibleTo(&dialog));
}

void ServerConfigDialogTests::arrangement_isUsedOnRequest_savedWithThePlaces_andLeftAgain()
{
  writeThreeComputers();
  {
    ServerConfig config;
    ServerConfigDialog dialog(nullptr, config);
    dialog.show();
    experimentalBox(dialog)->setChecked(true);
    buttonNamed(dialog, "btnUseArrangement")->click();

    // the canvas has the computers where the classic grid had them, and takes the place of the grid
    QVERIFY(panelOf(dialog)->isVisibleTo(&dialog));
    QVERIFY(!gridOf(dialog)->isVisibleTo(&dialog));
    QVERIFY(!buttonNamed(dialog, "btnUseArrangement")->isVisibleTo(&dialog));
    QVERIFY(panelOf(dialog)->canvas()->arrangement().computers.size() == 3);
    QVERIFY(saveIsEnabled(dialog));
    // the way back is there, and says what it is
    QVERIFY(buttonNamed(dialog, "btnUseClassicGrid")->isVisibleTo(&dialog));
    QVERIFY(panelOf(dialog)
                ->findChild<QLabel *>(QStringLiteral("lblArrangementTitle"))
                ->text()
                .contains(QStringLiteral("Experimental")));

    Q_EMIT buttons(dialog)->accepted();
  }
  QCOMPARE(Settings::value(Settings::Server::UseArrangement).toBool(), true);
  QCOMPARE(Settings::value(Settings::Server::ExperimentalMultiMonitor).toBool(), true);
  for (const auto *name : {"PC", "srv", "MINI"}) {
    QVERIFY(Settings::value(Settings::Computer::ArrangementX.arg(QString::fromLatin1(name))).isValid());
    QVERIFY(Settings::value(Settings::Computer::ArrangementY.arg(QString::fromLatin1(name))).isValid());
  }
  // the server is where the origin is
  QCOMPARE(Settings::value(Settings::Computer::ArrangementX.arg("srv")).toDouble(), 0.0);
  QCOMPARE(Settings::value(Settings::Computer::ArrangementY.arg("srv")).toDouble(), 0.0);

  // opened again, it is the canvas; the classic grid is a click away, and the places stay when it is used
  {
    ServerConfig config;
    QVERIFY(config.usesArrangement());
    ServerConfigDialog dialog(nullptr, config);
    dialog.show();
    QVERIFY(panelOf(dialog)->isVisibleTo(&dialog));
    QVERIFY(!saveIsEnabled(dialog));
    buttonNamed(dialog, "btnUseClassicGrid")->click();
    QVERIFY(gridOf(dialog)->isVisibleTo(&dialog));
    QVERIFY(!panelOf(dialog)->isVisibleTo(&dialog));
    QVERIFY(saveIsEnabled(dialog));
    Q_EMIT buttons(dialog)->accepted();
  }
  QCOMPARE(Settings::value(Settings::Server::UseArrangement).toBool(), false);
  QVERIFY(Settings::value(Settings::Computer::ArrangementX.arg("PC")).isValid());
  ServerConfig classic;
  QVERIFY(!classic.usesArrangement());
}

void ServerConfigDialogTests::arrangement_withComputersOnTopOfEachOther_cannotBeSaved()
{
  writeThreeComputers();
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Settings::setValue(Settings::Server::UseArrangement, true);
  // the MacBook, a PC on top of it, and a Mac mini beside
  for (const auto &[name, x, y] :
       std::vector<std::tuple<QString, double, double>>{{"srv", 0, 0}, {"PC", 100, 100}, {"MINI", 3440, 0}}) {
    Settings::setValue(Settings::Computer::ArrangementX.arg(name), x);
    Settings::setValue(Settings::Computer::ArrangementY.arg(name), y);
  }
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);
  dialog.show();
  QVERIFY(panelOf(dialog)->hasErrors());
  // something is changed, which would be saved if the computers were not on top of each other
  textOnlyBox(dialog)->setChecked(!textOnlyBox(dialog)->isChecked());
  QVERIFY(!saveIsEnabled(dialog));
  auto *issues = panelOf(dialog)->findChild<QLabel *>(QStringLiteral("lblArrangementIssues"));
  QVERIFY(issues->isVisibleTo(&dialog));
  QVERIFY2(issues->text().contains(QStringLiteral("overlap")), qPrintable(issues->text()));
}

void ServerConfigDialogTests::arrangement_aComputerIsRemovedFromTheCanvas_theServerIsNot()
{
  writeThreeComputers();
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Settings::setValue(Settings::Server::UseArrangement, true);
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);
  dialog.show();
  auto *canvas = panelOf(dialog)->canvas();
  auto *remove = buttonNamed(dialog, "btnArrangementRemove");
  auto *edit = buttonNamed(dialog, "btnArrangementEdit");

  QVERIFY(!remove->isEnabled() && !edit->isEnabled());
  canvas->setSelected(QStringLiteral("srv"));
  QVERIFY(edit->isEnabled());
  QVERIFY2(!remove->isEnabled(), "the screen of this computer is not taken out");

  canvas->setSelected(QStringLiteral("PC"));
  QVERIFY(remove->isEnabled());
  remove->click();
  QCOMPARE(canvas->arrangement().computers.size(), size_t(2));
  QCOMPARE(canvas->arrangement().indexOf("PC"), -1);
  QVERIFY(saveIsEnabled(dialog));
}

void ServerConfigDialogTests::arrangement_aComputerThatReportsAnotherScreen_isWarnedOf()
{
  writeThreeComputers();
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Settings::setValue(Settings::Server::UseArrangement, true);
  Settings::setValue(Settings::Computer::ArrangementWidth.arg("MINI"), 1920);
  Settings::setValue(Settings::Computer::ArrangementHeight.arg("MINI"), 1080);
  for (const auto &[name, x, y] :
       std::vector<std::tuple<QString, double, double>>{{"srv", 0, 0}, {"PC", -1920, 0}, {"MINI", 3440, 0}}) {
    Settings::setValue(Settings::Computer::ArrangementX.arg(name), x);
    Settings::setValue(Settings::Computer::ArrangementY.arg(name), y);
  }
  ServerConfig config;
  // the Mac mini says it is 2560x1440, the arrangement has it as 1920x1080; the PC says what the arrangement has
  config.setReportedScreen("MINI", QRect(0, 0, 2560, 1440));
  config.setReportedScreen("PC", QRect(0, 0, 1920, 1080));
  ServerConfigDialog dialog(nullptr, config);
  dialog.show();
  auto *issues = panelOf(dialog)->findChild<QLabel *>(QStringLiteral("lblArrangementIssues"));
  QVERIFY(issues->isVisibleTo(&dialog));
  QVERIFY2(issues->text().contains(QStringLiteral("MINI reports a screen of 2560 × 1440")), qPrintable(issues->text()));
  QVERIFY(!issues->text().contains(QStringLiteral("PC reports")));
  // a warning does not stop it from being saved
  QVERIFY(!panelOf(dialog)->hasErrors());
}

void ServerConfigDialogTests::arrangement_aSizeThatIsAssumed_isSaidInTheStatusLine()
{
  writeThreeComputers();
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Settings::setValue(Settings::Server::UseArrangement, true);
  const auto issuesOf = [](ServerConfigDialog &dialog) {
    return panelOf(dialog)->findChild<QLabel *>(QStringLiteral("lblArrangementIssues"));
  };
  {
    ServerConfig config;
    ServerConfigDialog dialog(nullptr, config);
    dialog.show();
    QVERIFY(issuesOf(dialog)->isVisibleTo(&dialog));
    const auto text = issuesOf(dialog)->text();
    QVERIFY2(text.contains(QStringLiteral("PC: size assumed 1920 × 1080")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("MINI: size assumed 1920 × 1080")), qPrintable(text));
    // the screen of the server is the operating system's
    QVERIFY(!text.contains(QStringLiteral("srv: size assumed")));
    QCOMPARE(panelOf(dialog)->canvas()->assumedSizes(), QStringList({"PC", "MINI"}));
    // it is a warning: the configuration can be saved
    QVERIFY(!panelOf(dialog)->hasErrors());
  }

  // a size that is said is not assumed
  Settings::setValue(Settings::Computer::ArrangementWidth.arg("MINI"), 1920);
  Settings::setValue(Settings::Computer::ArrangementHeight.arg("MINI"), 1080);
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);
  dialog.show();
  const auto text = issuesOf(dialog)->text();
  QVERIFY2(text.contains(QStringLiteral("PC: size assumed")), qPrintable(text));
  QVERIFY2(!text.contains(QStringLiteral("MINI: size assumed")), qPrintable(text));
}

namespace {

// the row of the user's classic grid: a Windows laptop, this MacBook with its DELL (the server), the Windows PC with
// two monitors and a Mac mini; the sizes are placeholders of what the diagnostics would say
void writeTheRowOfTheUser()
{
  const QStringList names = {"SUBEOM-COPILOT", "Subeoms-MacBook-Pro.local", "9800X3D-5080", "Subeoms-Mac-mini.local"};
  Settings::setValue(Settings::Core::ComputerName, names[1]);
  auto &settings = Settings::proxy();
  settings.beginGroup(QStringLiteral("internalConfig"));
  settings.remove(QString());
  settings.beginWriteArray(QStringLiteral("screens"));
  for (int i = 0; i < 15; ++i) {
    settings.setArrayIndex(i);
    settings.setValue(QStringLiteral("name"), i >= 6 && i <= 9 ? names[i - 6] : QString());
  }
  settings.endArray();
  settings.endGroup();
  ServerConfig::overrideServerMonitors(
      std::vector<deskflow::server::MonitorRect>{{0, 0, 3440, 1440}, {753, 1440, 2064, 1347}}
  );
  Settings::setValue(
      Settings::Computer::MonitorLayout.arg(names[2]), QStringLiteral("-1920,360,1920,1080;0,0,2560,1440")
  );
  Settings::setValue(Settings::Computer::ArrangementZoom.arg(names[0]), 0.8);
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
}

void saveGrab(QWidget &widget, const QString &name)
{
  const auto directory = qEnvironmentVariable("DESKFLOW_SCREENSHOT_DIR");
  if (directory.isEmpty())
    return;
  QDir().mkpath(directory);
  widget.grab().save(QDir(directory).filePath(name));
}

} // namespace

void ServerConfigDialogTests::arrangement_inCanvasMode_theGridOnlyWidgetsGiveWayToButtons_andAComputerIsAdded()
{
  writeThreeComputers();
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Settings::setValue(Settings::Server::UseArrangement, true);
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);
  dialog.show();
  // what is dragged onto the grid, or from it to the trash, has no grid to drag to or from
  for (const auto *name : {"lblNewComputer", "lblRemoveComputer", "label_2"})
    QVERIFY2(!dialog.findChild<QWidget *>(QString::fromLatin1(name))->isVisibleTo(&dialog), name);
  auto *add = buttonNamed(dialog, "btnArrangementAdd");
  QVERIFY(add != nullptr && add->isVisibleTo(&dialog) && add->isEnabled());
  // the button asks for the dialog of a new computer, which is shut here as it opens
  QSignalSpy asked(panelOf(dialog), &ArrangementPanel::addComputerRequested);
  QTimer closer;
  closer.setInterval(10);
  QObject::connect(&closer, &QTimer::timeout, [] {
    if (auto *modal = QApplication::activeModalWidget())
      modal->close();
  });
  closer.start();
  add->click();
  closer.stop();
  QCOMPARE(asked.count(), 1);

  // a computer that is added has a place, apart from the others, and the canvas shows it
  QVERIFY(dialog.addClient(QStringLiteral("NEW")));
  QCOMPARE(panelOf(dialog)->canvas()->arrangement().computers.size(), size_t(4));
  QVERIFY(!panelOf(dialog)->hasErrors());

  // and the classic grid has its widgets again
  buttonNamed(dialog, "btnUseClassicGrid")->click();
  for (const auto *name : {"lblNewComputer", "lblRemoveComputer", "label_2"})
    QVERIFY2(dialog.findChild<QWidget *>(QString::fromLatin1(name))->isVisibleTo(&dialog), name);
}

void ServerConfigDialogTests::arrangement_saveKeepsWhatComputersReportedWhileTheDialogWasOpen()
{
  writeThreeComputers();
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  ServerConfig main;
  ServerConfigDialog dialog(nullptr, main);
  // the computer connected, and told its screen, while the dialog was open
  main.setReportedScreen(QStringLiteral("MINI"), QRect(0, 0, 2560, 1440));
  textOnlyBox(dialog)->setChecked(!textOnlyBox(dialog)->isChecked());
  Q_EMIT buttons(dialog)->accepted();
  QVERIFY(main.reportedScreen(QStringLiteral("MINI")));
  QCOMPARE(*main.reportedScreen(QStringLiteral("MINI")), QRect(0, 0, 2560, 1440));
}

void ServerConfigDialogTests::arrangement_firstUse_adoptsTheSizesThatComputersReported()
{
  writeThreeComputers();
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  {
    ServerConfig config;
    config.setReportedScreen(QStringLiteral("MINI"), QRect(0, 0, 2560, 1440));
    ServerConfigDialog dialog(nullptr, config);
    dialog.show();
    buttonNamed(dialog, "btnUseArrangement")->click();
    const auto &arrangement = panelOf(dialog)->canvas()->arrangement();
    const auto &mini = arrangement.computers[static_cast<size_t>(arrangement.indexOf("MINI"))];
    QCOMPARE(mini.shape().w, 2560);
    QCOMPARE(mini.shape().h, 1440);
    Q_EMIT buttons(dialog)->accepted();
  }
  // it is the size of the computer in the saved arrangement, not what a report says at the time
  QCOMPARE(Settings::value(Settings::Computer::ArrangementWidth.arg("MINI")).toInt(), 2560);
  QCOMPARE(Settings::value(Settings::Computer::ArrangementHeight.arg("MINI")).toInt(), 1440);
  const ServerConfig fresh;
  QVERIFY(fresh.arrangement().computers.size() == 3);
}

void ServerConfigDialogTests::arrangement_withAnExternalConfig_canBeSavedWhateverTheArrangementIs()
{
  writeThreeComputers();
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Settings::setValue(Settings::Server::UseArrangement, true);
  for (const auto &[name, x, y] :
       std::vector<std::tuple<QString, double, double>>{{"srv", 0, 0}, {"PC", 100, 100}, {"MINI", 3440, 0}}) {
    Settings::setValue(Settings::Computer::ArrangementX.arg(name), x);
    Settings::setValue(Settings::Computer::ArrangementY.arg(name), y);
  }
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);
  dialog.show();
  QVERIFY(panelOf(dialog)->hasErrors());

  // the file that is used is the user's own, and its tabs are off: nothing of the arrangement is written
  QTemporaryFile file;
  QVERIFY(file.open());
  auto *external = dialog.findChild<QGroupBox *>(QStringLiteral("groupExternalConfig"));
  auto *line = dialog.findChild<QLineEdit *>(QStringLiteral("lineConfigFile"));
  QVERIFY(external != nullptr && line != nullptr);
  line->setText(file.fileName());
  external->setChecked(true);
  QVERIFY(saveIsEnabled(dialog));
}

void ServerConfigDialogTests::arrangement_monitorsAreDrawnInTheEditor_andKeptOnOk_notOnCancel()
{
  writeThreeComputers();
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Settings::setValue(Settings::Server::UseArrangement, true);
  for (const auto &[name, x, y] :
       std::vector<std::tuple<QString, double, double>>{{"srv", 0, 0}, {"PC", -3840, 0}, {"MINI", 3440, 0}}) {
    Settings::setValue(Settings::Computer::ArrangementX.arg(name), x);
    Settings::setValue(Settings::Computer::ArrangementY.arg(name), y);
  }
  ServerConfig config;
  // the PC tells the server that it is 3840 x 1080
  config.setReportedScreen("PC", QRect(0, 0, 3840, 1080));
  ServerConfigDialog dialog(nullptr, config);
  dialog.show();
  auto *panel = panelOf(dialog);
  const auto monitorsOfPc = [&] {
    const auto &arrangement = panel->canvas()->arrangement();
    return arrangement.computers[static_cast<size_t>(arrangement.indexOf("PC"))].monitors;
  };
  const auto modal = [] { return qobject_cast<MonitorEditorDialog *>(QApplication::activeModalWidget()); };
  using Monitors = std::vector<deskflow::server::MonitorRect>;

  // the button and the double click are for the monitors of the computer that is selected, and not for the server's
  panel->canvas()->setSelected(QStringLiteral("PC"));
  QVERIFY(buttonNamed(dialog, "btnArrangementMonitors")->isEnabled());
  panel->canvas()->setSelected(QStringLiteral("srv"));
  QVERIFY(!buttonNamed(dialog, "btnArrangementMonitors")->isEnabled());
  // asked for the server, nothing opens (this would not return if it did)
  Q_EMIT panel->editMonitorsRequested(QStringLiteral("srv"));

  // Cancel leaves it as it was
  QCOMPARE(monitorsOfPc(), (Monitors{{0, 0, 1920, 1080}}));
  QTimer::singleShot(0, [&] {
    QVERIFY(modal() != nullptr);
    modal()->reject();
  });
  Q_EMIT panel->editMonitorsRequested(QStringLiteral("PC"));
  QCOMPARE(monitorsOfPc(), (Monitors{{0, 0, 1920, 1080}}));

  // OK keeps what was drawn, in the coordinates that it reports: two monitors that make up its screen
  QTimer::singleShot(0, [&] {
    auto *editor = modal();
    QVERIFY(editor != nullptr);
    QVERIFY(
        editor->findChild<QLabel *>(QStringLiteral("lblMonitorStatus"))->text().contains(QStringLiteral("Matches"))
    );
    editor->findChild<QGroupBox *>(QStringLiteral("groupAdvancedNumbers"))->setChecked(true);
    editor->findChild<QPlainTextEdit *>(QStringLiteral("textNumbers"))
        ->setPlainText(QStringLiteral("0,0,1920,1080;1920,0,1920,1080"));
    editor->findChild<QPushButton *>(QStringLiteral("btnApplyNumbers"))->click();
    editor->accept();
  });
  Q_EMIT panel->editMonitorsRequested(QStringLiteral("PC"));
  QCOMPARE(monitorsOfPc(), (Monitors{{0, 0, 1920, 1080}, {1920, 0, 1920, 1080}}));
  // where it was on the canvas, it is
  const auto &arrangement = panel->canvas()->arrangement();
  const auto &pc = arrangement.computers[static_cast<size_t>(arrangement.indexOf("PC"))];
  QCOMPARE(pc.originX, -3840.0);
  QCOMPARE(pc.zoom, 1.0);
  QVERIFY(!panel->hasErrors());
}

void ServerConfigDialogTests::arrangement_doubleClick_drawsTheMonitors_butTheServerHasItsSettings()
{
  writeThreeComputers();
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Settings::setValue(Settings::Server::UseArrangement, true);
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);
  dialog.show();
  auto *panel = panelOf(dialog);

  // a computer: the editor of its monitors
  QString opened;
  QTimer::singleShot(0, [&] {
    if (auto *modal = QApplication::activeModalWidget()) {
      opened = QString::fromLatin1(modal->metaObject()->className());
      modal->close();
    }
  });
  Q_EMIT panel->canvas()->editRequested(QStringLiteral("PC"));
  QCOMPARE(opened, QStringLiteral("MonitorEditorDialog"));

  // the server: its monitors are the operating system's, so its settings
  opened.clear();
  QTimer::singleShot(0, [&] {
    if (auto *modal = QApplication::activeModalWidget()) {
      opened = QString::fromLatin1(modal->metaObject()->className());
      modal->close();
    }
  });
  Q_EMIT panel->canvas()->editRequested(QStringLiteral("srv"));
  QCOMPARE(opened, QStringLiteral("ComputerSettingsDialog"));
}

void ServerConfigDialogTests::arrangement_aComputerScaledOnTheCanvas_hasItsZoomSaved_andTheHintSaysHow()
{
  writeThreeComputers();
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Settings::setValue(Settings::Server::UseArrangement, true);
  for (const auto &[name, x, y] :
       std::vector<std::tuple<QString, double, double>>{{"srv", 0, 0}, {"PC", -1920, 0}, {"MINI", 3440, 0}}) {
    Settings::setValue(Settings::Computer::ArrangementX.arg(name), x);
    Settings::setValue(Settings::Computer::ArrangementY.arg(name), y);
  }
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);
  dialog.show();
  auto *panel = panelOf(dialog);

  // the hint under the canvas says how a computer is dragged and resized
  auto *hint = panel->findChild<QLabel *>(QStringLiteral("lblArrangementHint"));
  QVERIFY(hint->isVisibleTo(&dialog));
  QVERIFY2(hint->text().contains(QStringLiteral("resize it by its outline")), qPrintable(hint->text()));
  QVERIFY(hint->text().contains(QStringLiteral("double-click")));
  QVERIFY(hint->geometry().top() >= panel->canvas()->geometry().bottom());

  // a computer that is scaled on the canvas has the zoom, and is saved with it
  Q_EMIT panel->canvas()->computerScaled(QStringLiteral("PC"), 0.8);
  const auto &arrangement = panel->canvas()->arrangement();
  QCOMPARE(arrangement.computers[static_cast<size_t>(arrangement.indexOf("PC"))].zoom, 0.8);
  Q_EMIT buttons(dialog)->accepted();
  QCOMPARE(Settings::value(Settings::Computer::ArrangementZoom.arg("PC")).toDouble(), 0.8);
  QVERIFY(Settings::value(Settings::Computer::MonitorLayout.arg("PC")).toString().isEmpty());
}

void ServerConfigDialogTests::arrangement_aComputerResizedOnTheCanvas_hasItsSizeSaidAndSaved()
{
  writeThreeComputers();
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Settings::setValue(Settings::Server::UseArrangement, true);
  for (const auto &[name, x, y] :
       std::vector<std::tuple<QString, double, double>>{{"srv", 0, 0}, {"PC", -1920, 0}, {"MINI", 3440, 0}}) {
    Settings::setValue(Settings::Computer::ArrangementX.arg(name), x);
    Settings::setValue(Settings::Computer::ArrangementY.arg(name), y);
  }
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);
  dialog.show();
  auto *panel = panelOf(dialog);
  auto *issues = panel->findChild<QLabel *>(QStringLiteral("lblArrangementIssues"));
  QVERIFY(issues->text().contains(QStringLiteral("PC: size assumed")));

  // the edges of the computer are dragged on the canvas: its size is said, and it is not assumed any more
  Q_EMIT panel->canvas()->computerResized(QStringLiteral("PC"), 0, 0, 2560, 1440);
  QVERIFY2(!issues->text().contains(QStringLiteral("PC: size assumed")), qPrintable(issues->text()));
  QVERIFY(issues->text().contains(QStringLiteral("MINI: size assumed")));
  const auto &arrangement = panel->canvas()->arrangement();
  QCOMPARE(arrangement.computers[static_cast<size_t>(arrangement.indexOf("PC"))].shape().w, 2560);
  // and it is in the configuration that is saved
  Q_EMIT buttons(dialog)->accepted();
  QCOMPARE(Settings::value(Settings::Computer::ArrangementWidth.arg("PC")).toInt(), 2560);
  QCOMPARE(Settings::value(Settings::Computer::ArrangementHeight.arg("PC")).toInt(), 1440);
}

void ServerConfigDialogTests::arrangement_screenshots_areMadeOfTheDialogs()
{
  const auto originalStyle = QApplication::style()->name();
  QApplication::setStyle(QStringLiteral("Fusion"));
  writeTheRowOfTheUser();
  const QStringList names = {"SUBEOM-COPILOT", "Subeoms-MacBook-Pro.local", "9800X3D-5080", "Subeoms-Mac-mini.local"};

  // 1. the experiment is on, and the arrangement is offered beside the classic grid
  {
    ServerConfig config;
    ServerConfigDialog dialog(nullptr, config);
    dialog.resize(760, 640);
    dialog.show();
    QVERIFY(buttonNamed(dialog, "btnUseArrangement")->isVisibleTo(&dialog));
    saveGrab(dialog, QStringLiteral("5-dialog-experiment-on-classic-grid-with-offer.png"));

    // 2. what it makes of the classic grid when it is used for the first time
    buttonNamed(dialog, "btnUseArrangement")->click();
    QVERIFY(panelOf(dialog)->isVisibleTo(&dialog));
    saveGrab(dialog, QStringLiteral("6-dialog-first-use-converted-from-the-classic-grid.png"));
  }

  // 3. the computers where the user wants them: the Windows PC on the left of the DELL, the Mac mini on its right, and
  // the laptop under the DELL beside the MacBook
  Settings::setValue(Settings::Server::UseArrangement, true);
  const auto place = [](const QString &name, double x, double y) {
    Settings::setValue(Settings::Computer::ArrangementX.arg(name), x);
    Settings::setValue(Settings::Computer::ArrangementY.arg(name), y);
  };
  place(names[1], 0, 0);
  place(names[2], -2560, 0);
  place(names[3], 3440, 0);
  place(names[0], -783, 1440);
  Settings::setValue(Settings::Computer::ArrangementWidth.arg(names[3]), 1920);
  Settings::setValue(Settings::Computer::ArrangementHeight.arg(names[3]), 1080);
  {
    ServerConfig config;
    ServerConfigDialog dialog(nullptr, config);
    dialog.resize(760, 640);
    dialog.show();
    panelOf(dialog)->canvas()->setSelected(names[2]);
    QVERIFY(!panelOf(dialog)->hasErrors());
    saveGrab(dialog, QStringLiteral("7-dialog-arranged-t-shape-with-three-pcs.png"));

    // 4. a computer's own settings: its monitors, size and display scale
    Computer computer(names[2]);
    {
      ServerConfig loaded;
      for (const auto &c : std::as_const(loaded).computers()) {
        if (c.name() == names[2])
          computer = c;
      }
    }
    ComputerSettingsDialog settings(&dialog, &computer, nullptr);
    settings.show();
    saveGrab(settings, QStringLiteral("8-computer-settings-monitors-size-and-scale.png"));
  }

  for (const auto &name : names) {
    for (const auto &key :
         {Settings::Computer::ArrangementX, Settings::Computer::ArrangementY, Settings::Computer::ArrangementZoom,
          Settings::Computer::ArrangementWidth, Settings::Computer::ArrangementHeight,
          Settings::Computer::MonitorLayout})
      Settings::setValue(key.arg(name));
  }
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor);
  QApplication::setStyle(originalStyle);
}

// the arrangement is the one that has the switch, so the arrangement is in use
void ServerConfigDialogTests::rememberArrangements_isOffByDefault_andIsLoadedSavedAndRestoredToOff()
{
  writeThreeComputers();
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Settings::setValue(Settings::Server::UseArrangement, true);
  const auto boxOf = [](const ServerConfigDialog &dialog) {
    return panelOf(dialog)->findChild<QCheckBox *>(QStringLiteral("cbRememberArrangements"));
  };
  QVERIFY(!Settings::value(Settings::Server::RememberArrangements).toBool());
  {
    ServerConfig config;
    ServerConfigDialog dialog(nullptr, config);
    auto *box = boxOf(dialog);
    QVERIFY2(box != nullptr, "the arrangement panel has no check box for remembering layouts");
    QVERIFY(!box->isChecked());
    QVERIFY(box->toolTip().contains(QStringLiteral("Experimental")));
    QVERIFY(!saveIsEnabled(dialog));

    box->setChecked(true);
    Q_EMIT buttons(dialog)->accepted();
  }
  QCOMPARE(Settings::value(Settings::Server::RememberArrangements).toBool(), true);
  {
    ServerConfig config;
    ServerConfigDialog dialog(nullptr, config);
    QVERIFY(boxOf(dialog)->isChecked());
    Q_EMIT buttons(dialog)->restoreDefault();
    QVERIFY(!boxOf(dialog)->isChecked());
  }
}

void ServerConfigDialogTests::rememberArrangements_makesTheDialogModified_andCancelDoesNotSaveIt()
{
  writeThreeComputers();
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Settings::setValue(Settings::Server::UseArrangement, true);
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);
  auto *box = panelOf(dialog)->findChild<QCheckBox *>(QStringLiteral("cbRememberArrangements"));
  QVERIFY(box != nullptr);
  QVERIFY(!saveIsEnabled(dialog));

  box->setChecked(true);
  QVERIFY(saveIsEnabled(dialog));
  box->setChecked(false);
  QVERIFY(!saveIsEnabled(dialog));

  box->setChecked(true);
  Q_EMIT buttons(dialog)->rejected();
  QVERIFY(!Settings::value(Settings::Server::RememberArrangements).toBool());
}

QTEST_MAIN(ServerConfigDialogTests)
