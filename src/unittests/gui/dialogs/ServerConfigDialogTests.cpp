/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerConfigDialogTests.h"

#include "common/Settings.h"
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
#include <QPushButton>
#include <QRect>
#include <QSpinBox>
#include <QTest>

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

QTEST_MAIN(ServerConfigDialogTests)
