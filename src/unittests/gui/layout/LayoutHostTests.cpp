/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "LayoutHostTests.h"

#include "LayoutTestData.h"
#include "common/Settings.h"
#include "gui/config/ServerConfig.h"
#include "gui/layout/DisplayProvider.h"
#include "gui/layout/LayoutMemoryService.h"
#include "gui/layout/ServerConfigLayout.h"
#include "gui/layout/ServerConfigLayoutHost.h"

#include <QDialog>
#include <QFile>
#include <QGuiApplication>
#include <QMap>
#include <QScreen>
#include <QSettings>
#include <QTextStream>
#include <QTimer>

#include <algorithm>

using namespace layout_test;
using namespace deskflow::gui::layout;

namespace {

constexpr int kCells = 15;

// the settings the way the server configuration saves them: the computers on cells of the grid
void writeGrid(const QList<QPair<int, QString>> &cells, const QString &serverName)
{
  Settings::setValue(Settings::Core::ComputerName, serverName);
  auto &settings = Settings::proxy();
  settings.beginGroup(QStringLiteral("internalConfig"));
  settings.remove(QString());
  settings.beginWriteArray(QStringLiteral("screens"));
  for (int i = 0; i < kCells; ++i) {
    settings.setArrayIndex(i);
    QString name;
    for (const auto &cell : cells) {
      if (cell.first == i)
        name = cell.second;
    }
    settings.setValue(QStringLiteral("name"), name);
  }
  settings.endArray();
  settings.endGroup();
}

void place(const QString &name, double x, double y, double zoom = 1.0, int width = 0, int height = 0)
{
  Settings::setValue(Settings::Computer::ArrangementX.arg(name), x);
  Settings::setValue(Settings::Computer::ArrangementY.arg(name), y);
  if (zoom != 1.0)
    Settings::setValue(Settings::Computer::ArrangementZoom.arg(name), zoom);
  if (width > 0)
    Settings::setValue(Settings::Computer::ArrangementWidth.arg(name), width);
  if (height > 0)
    Settings::setValue(Settings::Computer::ArrangementHeight.arg(name), height);
}

const QStringList kNames{"srv", "PC1", "PC2", "MINI"};

void forget()
{
  for (const auto &name : kNames) {
    for (const auto &key :
         {Settings::Computer::ArrangementX, Settings::Computer::ArrangementY, Settings::Computer::ArrangementZoom,
          Settings::Computer::ArrangementWidth, Settings::Computer::ArrangementHeight,
          Settings::Computer::MonitorLayout, Settings::Computer::MonitorScales, Settings::Computer::ReportedMonitors})
      Settings::setValue(key.arg(name));
  }
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor);
  Settings::setValue(Settings::Server::UseArrangement);
  Settings::setValue(Settings::Server::RememberArrangements);
  Settings::setValue(Settings::Server::ExternalConfig);
  ServerConfig::overrideServerMonitors(std::nullopt);
}

// the user's T with the computers around it, and the experiments on
void arrange(const std::vector<DisplayInfo> &displays = tShape())
{
  writeGrid({{6, "PC1"}, {7, "srv"}, {8, "PC2"}, {9, "MINI"}}, "srv");
  ServerConfig::overrideServerMonitors(sig(displays).monitors());
  place("srv", 0, 0);
  place("PC1", -1920, 0, 1.0, 1920, 1080);
  place("PC2", 3440, 0, 1.0, 1920, 1080);
  place("MINI", 964, 2422, 1.0, 1920, 1080);
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Settings::setValue(Settings::Server::UseArrangement, true);
}

QMap<QString, QVariant> snapshotOfSettings()
{
  auto &settings = Settings::proxy().get();
  settings.sync();
  QMap<QString, QVariant> all;
  for (const auto &key : settings.allKeys())
    all.insert(key, settings.value(key));
  return all;
}

// what differs between two snapshots: the keys that were added, removed or have another value
QStringList differences(const QMap<QString, QVariant> &a, const QMap<QString, QVariant> &b)
{
  QStringList keys;
  for (auto it = a.begin(); it != a.end(); ++it) {
    if (!b.contains(it.key()) || b.value(it.key()) != it.value())
      keys << it.key();
  }
  for (auto it = b.begin(); it != b.end(); ++it) {
    if (!a.contains(it.key()))
      keys << it.key();
  }
  keys.removeDuplicates();
  keys.sort();
  return keys;
}

bool isArrangementKey(const QString &key)
{
  return key.contains(QStringLiteral("/arrangement")) || key.contains(QStringLiteral("/monitorScales"));
}

QString text(const ServerConfig &config)
{
  return generatedConfigText(config);
}

class FixedProvider : public IDisplayProvider
{
public:
  std::vector<DisplayInfo> shown = tShape();
  std::vector<DisplayInfo> displays() const override
  {
    return shown;
  }
};

// the world of the service: the operating system and the core
struct World
{
  FixedProvider *provider = nullptr;
  Millis clock = 0;
  bool coreRunning = true;
  bool dialogOpen = false;
  bool serverMode = true;
  int restarts = 0;
  QString runningText;
  QList<Notice> notices;
  ServerConfig *config = nullptr;
};

HostHooks hooksFor(World &world)
{
  HostHooks hooks;
  hooks.serverMode = [&world] { return world.serverMode; };
  hooks.coreRunning = [&world] { return world.coreRunning; };
  hooks.restartCore = [&world] {
    ++world.restarts;
    world.runningText = text(*world.config);
  };
  hooks.dialogOpen = [&world] { return world.dialogOpen; };
  hooks.runningConfigText = [&world] { return world.runningText; };
  hooks.notify = [&world](const Notice &notice) { world.notices.append(notice); };
  hooks.wallClockMs = [&world] { return int64_t(1'700'000'000'000) + world.clock; };
  hooks.log = [](QtMsgType, const QString &) {};
  return hooks;
}

// everything the service needs, in a scratch folder
struct Fixture
{
  World world;
  std::unique_ptr<ServerConfig> config = std::make_unique<ServerConfig>();
  std::unique_ptr<LayoutMemoryService> service;
  QString storePath;

  explicit Fixture(const QString &store)
      : storePath(store)
  {
    world.config = config.get();
    world.runningText = text(*config);
    auto provider = std::make_unique<FixedProvider>();
    world.provider = provider.get();
    LayoutMemoryService::Options options;
    options.storePath = storePath;
    options.useTimers = false;
    options.clock = [this] { return world.clock; };
    service = std::make_unique<LayoutMemoryService>(*config, std::move(provider), hooksFor(world), options);
  }

  // the operating system has these displays now
  void setDisplays(const std::vector<DisplayInfo> &displays)
  {
    world.provider->shown = displays;
    ServerConfig::overrideServerMonitors(sig(displays).monitors());
  }

  // the displays changed at \p time, and the time goes on until the memory looks
  void changeAt(Millis time)
  {
    world.clock = time;
    service->displaysMayHaveChanged();
    world.clock = time + kQuietMs;
    service->pump();
  }
};

} // namespace

void LayoutHostTests::initTestCase()
{
  QVERIFY(m_dir.isValid());
  Settings::setSettingsFile(m_dir.filePath(QStringLiteral("Deskflow.conf")));
  Settings::setStateFile(m_dir.filePath(QStringLiteral("Deskflow.state")));
}

void LayoutHostTests::init()
{
  QFile::remove(m_dir.filePath(QStringLiteral("layout-profiles.json")));
  QFile::remove(m_dir.filePath(QStringLiteral("layout-profiles.json.damaged")));
  QFile::remove(m_dir.filePath(QStringLiteral("layout-profiles.json.bak")));
}

void LayoutHostTests::cleanup()
{
  forget();
}

void LayoutHostTests::capture_isWhereTheComputersAre()
{
  arrange();
  const ServerConfig config;
  const auto layout = captureLayout(config);
  QCOMPARE(layout.computers.size(), size_t(4));
  const auto *server = layout.find("srv");
  QVERIFY(server != nullptr);
  QVERIFY(server->isServer);
  QCOMPARE(layout.find("PC1")->x, -1920.0);
  QCOMPARE(layout.find("PC2")->x, 3440.0);
  QCOMPARE(layout.find("MINI")->y, 2422.0);
  QVERIFY(!layout.find("PC2")->isServer);
  QCOMPARE(layout.find("PC2")->zoom, 1.0);
}

void LayoutHostTests::capture_isNothingWithoutAnArrangement()
{
  // the classic grid only: nothing is placed
  writeGrid({{6, "PC1"}, {7, "srv"}}, "srv");
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Settings::setValue(Settings::Server::UseArrangement, true);
  const ServerConfig config;
  QVERIFY(!config.hasArrangement());
  QVERIFY(captureLayout(config).isEmpty());
}

void LayoutHostTests::apply_movesTheComputers_andWritesOnlyTheirPlaces()
{
  arrange();
  ServerConfig config;
  config.commit();
  const auto before = snapshotOfSettings();
  const auto textBefore = text(config);

  auto wanted = captureLayout(config);
  for (auto &c : wanted.computers) {
    if (c.name == "PC2") {
      c.x = 3440;
      c.y = 300;
    }
  }
  const auto outcome = applyLayout(config, wanted);
  QVERIFY(outcome.applied);
  QVERIFY(outcome.changed);
  QVERIFY(outcome.error.isEmpty());
  QVERIFY(captureLayout(config).sameAs(wanted));
  QVERIFY(text(config) != textBefore);

  config.commit();
  for (const auto &key : differences(before, snapshotOfSettings()))
    QVERIFY2(isArrangementKey(key), qPrintable(key));
  // a reload sees them
  const ServerConfig again;
  QVERIFY(captureLayout(again).sameAs(wanted));
}

void LayoutHostTests::apply_ofTheSameLayout_changesNothing()
{
  arrange();
  ServerConfig config;
  const auto textBefore = text(config);
  const auto outcome = applyLayout(config, captureLayout(config));
  QVERIFY(outcome.applied);
  QVERIFY(!outcome.changed);
  QCOMPARE(text(config), textBefore);
}

void LayoutHostTests::apply_isAllOrNothing_whenItWouldOverlap()
{
  arrange();
  ServerConfig config;
  const auto before = captureLayout(config);
  const auto textBefore = text(config);

  // PC1 and PC2 on the same place
  auto wanted = before;
  for (auto &c : wanted.computers) {
    if (c.name == "PC1") {
      c.x = 3440;
      c.y = 0;
    }
  }
  const auto outcome = applyLayout(config, wanted);
  QVERIFY(!outcome.applied);
  QVERIFY(!outcome.changed);
  QVERIFY(!outcome.error.isEmpty());
  QVERIFY(captureLayout(config).sameAs(before));
  QCOMPARE(text(config), textBefore);
}

void LayoutHostTests::apply_keepsTheComputersThatTheLayoutDoesNotKnow()
{
  arrange();
  ServerConfig config;
  auto wanted = captureLayout(config);
  wanted.computers.erase(
      std::remove_if(wanted.computers.begin(), wanted.computers.end(), [](const PlacedComputer &c) { return c.name == "MINI"; }),
      wanted.computers.end()
  );
  for (auto &c : wanted.computers) {
    if (c.name == "PC2")
      c.y = 100;
  }
  const auto outcome = applyLayout(config, wanted);
  QVERIFY(outcome.applied);
  QCOMPARE(outcome.notInLayout, QStringList{"MINI"});
  const auto layout = captureLayout(config);
  QCOMPARE(layout.find("MINI")->x, 964.0);
  QCOMPARE(layout.find("MINI")->y, 2422.0);
  QCOMPARE(layout.find("PC2")->y, 100.0);
}

void LayoutHostTests::apply_putsAComputerThatWouldOverlapBesideTheOthers()
{
  arrange();
  ServerConfig config;
  // PC2 goes where MINI is, and the layout does not know MINI: MINI is put beside the others instead of overlapping
  auto wanted = captureLayout(config);
  wanted.computers.erase(
      std::remove_if(wanted.computers.begin(), wanted.computers.end(), [](const PlacedComputer &c) { return c.name == "MINI"; }),
      wanted.computers.end()
  );
  for (auto &c : wanted.computers) {
    if (c.name == "PC2") {
      c.x = 964;
      c.y = 2422;
    }
  }
  const auto outcome = applyLayout(config, wanted);
  QVERIFY(outcome.applied);
  QCOMPARE(outcome.notInLayout, QStringList{"MINI"});
  const auto arrangement = config.arrangement();
  for (const auto &issue : deskflow::server::arrangement::validate(arrangement))
    QVERIFY2(issue.severity() != deskflow::server::arrangement::Issue::Severity::Error, "an error is left");
  QCOMPARE(captureLayout(config).find("PC2")->x, 964.0);
}

void LayoutHostTests::apply_ignoresTheCaseOfNames_andComputersThatAreGone()
{
  arrange();
  ServerConfig config;
  auto wanted = captureLayout(config);
  for (auto &c : wanted.computers) {
    if (c.name == "PC2") {
      c.name = "pc2";
      c.y = 50;
    }
  }
  wanted.computers.push_back({"GONE", false, 99999, 99999, 1.0});
  const auto outcome = applyLayout(config, wanted);
  QVERIFY(outcome.applied);
  QVERIFY(outcome.notInLayout.isEmpty());
  QCOMPARE(captureLayout(config).find("PC2")->y, 50.0);
  QVERIFY(captureLayout(config).find("GONE") == nullptr);
}

void LayoutHostTests::apply_theZoomGoesThroughTheCanvasRule()
{
  arrange();
  Settings::setValue(Settings::Computer::MonitorLayout.arg("PC2"), QStringLiteral("0,0,1920,1080"));
  Settings::setValue(Settings::Computer::MonitorScales.arg("PC2"), QStringLiteral("100"));
  ServerConfig config;
  auto wanted = captureLayout(config);
  for (auto &c : wanted.computers) {
    if (c.name == "PC2")
      c.zoom = 0.5;
  }
  const auto outcome = applyLayout(config, wanted);
  QVERIFY(outcome.applied);
  QVERIFY(outcome.changed);
  const auto &computers = std::as_const(config).computers();
  const auto pc2 = std::ranges::find_if(computers, [](const Computer &c) { return c.name() == QStringLiteral("PC2"); });
  QVERIFY(pc2 != computers.end());
  QCOMPARE(pc2->arrangementZoom(), 0.5);
  // the drawn display scale followed the zoom, as it does when the computer is scaled on the canvas
  QCOMPARE(pc2->monitorScales().size(), qsizetype(1));
  QCOMPARE(pc2->monitorScales().first(), 200.0);
  // and the server's own zoom is not something a layout changes
  QCOMPARE(captureLayout(config).find("srv")->zoom, 1.0);
}

void LayoutHostTests::reattach_putsTheComputersAgainstTheServerOfTheNewDisplays()
{
  arrange();
  const ServerConfig config;
  const auto from = sig(tShape());
  const auto to = sig(laptopOnly());
  const auto outcome = reattachLayout(config, from, to);
  QVERIFY(outcome.layout.has_value());
  QCOMPARE(outcome.reattached.size(), 2);
  QVERIFY(outcome.reattached.contains("PC2"));
  QVERIFY(outcome.reattached.contains("MINI"));
  QVERIFY(outcome.notReattached.isEmpty());
  // against the laptop screen: the right edge of 1512 points, and below 982
  QCOMPARE(outcome.layout->find("PC2")->x, 1512.0);
  QCOMPARE(outcome.layout->find("MINI")->y, 982.0);
  // the config itself was not changed
  QCOMPARE(captureLayout(config).find("PC2")->x, 3440.0);

  // the same displays: nothing to do
  QVERIFY(!reattachLayout(config, from, from).layout.has_value());
}

void LayoutHostTests::generatedConfigText_isWhatTheCoreIsGiven()
{
  arrange();
  const ServerConfig config;
  QString expected;
  QTextStream stream(&expected);
  stream << config;
  stream.flush();
  QCOMPARE(generatedConfigText(config), expected);
  QVERIFY(generatedConfigText(config).startsWith(QStringLiteral("section: links")));
}

void LayoutHostTests::host_isEnabledOnlyWhenEverythingAsksForIt_data()
{
  QTest::addColumn<bool>("remember");
  QTest::addColumn<bool>("experiment");
  QTest::addColumn<bool>("use");
  QTest::addColumn<bool>("external");
  QTest::addColumn<bool>("serverMode");
  QTest::addColumn<bool>("enabled");

  QTest::newRow("everything") << true << true << true << false << true << true;
  QTest::newRow("the switch is off (the default)") << false << true << true << false << true << false;
  QTest::newRow("no multi-monitor experiment") << true << false << true << false << true << false;
  QTest::newRow("the classic grid is used") << true << true << false << false << true << false;
  QTest::newRow("an external configuration file") << true << true << true << true << true << false;
  QTest::newRow("client mode") << true << true << true << false << false << false;
  QTest::newRow("nothing") << false << false << false << false << false << false;
}

void LayoutHostTests::host_isEnabledOnlyWhenEverythingAsksForIt()
{
  QFETCH(bool, remember);
  QFETCH(bool, experiment);
  QFETCH(bool, use);
  QFETCH(bool, external);
  QFETCH(bool, serverMode);
  QFETCH(bool, enabled);

  arrange();
  Settings::setValue(Settings::Server::RememberArrangements, remember);
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, experiment);
  Settings::setValue(Settings::Server::UseArrangement, use);
  Settings::setValue(Settings::Server::ExternalConfig, external);

  ServerConfig config;
  World world;
  world.serverMode = serverMode;
  world.config = &config;
  ServerConfigLayoutHost host(config, std::make_unique<FixedProvider>(), hooksFor(world));
  QCOMPARE(host.enabled(), enabled);
}

void LayoutHostTests::host_coreConfigDiffers_comparesWithTheTextTheCoreWasStartedWith()
{
  arrange();
  ServerConfig config;
  World world;
  world.config = &config;
  ServerConfigLayoutHost host(config, std::make_unique<FixedProvider>(), hooksFor(world));

  world.runningText = text(config);
  QVERIFY(!host.coreConfigDiffers());
  world.runningText = text(config) + QStringLiteral("\n");
  QVERIFY(host.coreConfigDiffers());
  // a file that cannot be read is another configuration
  world.runningText.clear();
  QVERIFY(host.coreConfigDiffers());
}

void LayoutHostTests::host_applyWritesTheSettings_andAReloadSeesThem()
{
  arrange();
  ServerConfig config;
  World world;
  world.config = &config;
  ServerConfigLayoutHost host(config, std::make_unique<FixedProvider>(), hooksFor(world));
  auto wanted = host.currentLayout();
  wanted.computers[2].y += 40;
  const auto outcome = host.applyLayout(wanted);
  QVERIFY(outcome.applied && outcome.changed);

  const ServerConfig again;
  QVERIFY(captureLayout(again).sameAs(wanted));
  // a layout that changes nothing writes nothing
  const auto before = snapshotOfSettings();
  QVERIFY(!host.applyLayout(wanted).changed);
  QVERIFY(differences(before, snapshotOfSettings()).isEmpty());
}

void LayoutHostTests::host_dialogOpen_isAModalWidgetByDefault()
{
  arrange();
  ServerConfig config;
  HostHooks hooks;
  ServerConfigLayoutHost host(config, std::make_unique<FixedProvider>(), hooks);
  QVERIFY(host.canApplyNow());

  QDialog dialog;
  bool during = true;
  QTimer::singleShot(50, &dialog, [&] {
    during = host.canApplyNow();
    dialog.accept();
  });
  dialog.exec();
  QVERIFY(!during);
  QVERIFY(host.canApplyNow());
}

void LayoutHostTests::service_off_changesNothingAtAll()
{
  arrange();
  const auto settingsBefore = snapshotOfSettings();
  Fixture fixture(m_dir.filePath(QStringLiteral("layout-profiles.json")));
  const auto textBefore = text(*fixture.config);
  const auto layoutBefore = captureLayout(*fixture.config);

  // the default: off
  QVERIFY(!fixture.service->enabled());
  fixture.service->settingsChanged();
  fixture.setDisplays(laptopOnly());
  fixture.changeAt(1000);
  fixture.service->configSaved();
  fixture.service->displaysMayHaveChanged();
  fixture.world.clock = 100000;
  fixture.service->pump();

  // what the core would be given is what it was, byte for byte; the arrangement and the settings are not touched
  fixture.setDisplays(tShape());
  QCOMPARE(text(*fixture.config), textBefore);
  QVERIFY(captureLayout(*fixture.config).sameAs(layoutBefore));
  QVERIFY(differences(settingsBefore, snapshotOfSettings()).isEmpty());
  QCOMPARE(fixture.world.restarts, 0);
  QVERIFY(fixture.world.notices.isEmpty());
  QVERIFY(!fixture.service->notice().has_value());
  QVERIFY(!QFile::exists(fixture.storePath));
  QVERIFY(!QFile::exists(fixture.storePath + QStringLiteral(".damaged")));

  // switched on without the experiments or in client mode: still nothing
  Settings::setValue(Settings::Server::RememberArrangements, true);
  Settings::setValue(Settings::Server::UseArrangement, false);
  fixture.service->settingsChanged();
  fixture.changeAt(200000);
  QVERIFY(!QFile::exists(fixture.storePath));
  QCOMPARE(fixture.world.restarts, 0);
}

void LayoutHostTests::service_followsTheDisplays_andComesBackToTheSameConfig()
{
  arrange();
  Fixture fixture(m_dir.filePath(QStringLiteral("layout-profiles.json")));
  const auto textT = text(*fixture.config);
  const auto layoutT = captureLayout(*fixture.config);

  // the program has written its configuration before, as it does at every save
  fixture.config->commit();
  Settings::setValue(Settings::Server::RememberArrangements, true);
  fixture.service->settingsChanged();
  QVERIFY(fixture.service->enabled());
  // the arrangement of the T is remembered, and nothing else happens
  QVERIFY(QFile::exists(fixture.storePath));
  QCOMPARE(fixture.service->store().profiles().size(), size_t(1));
  QCOMPARE(fixture.world.restarts, 0);
  QVERIFY(fixture.world.notices.isEmpty());
  const auto afterStart = snapshotOfSettings();

  // the external monitor is unplugged
  fixture.setDisplays(laptopOnly());
  fixture.changeAt(1000);
  QVERIFY(text(*fixture.config) != textT);
  QCOMPARE(fixture.world.restarts, 1);
  QCOMPARE(fixture.world.notices.size(), 1);
  QCOMPARE(fixture.world.notices[0].kind, Notice::Kind::Remembered);
  QVERIFY(fixture.world.notices[0].reattached.contains("PC2"));
  QCOMPARE(captureLayout(*fixture.config).find("PC2")->x, 1512.0);
  QCOMPARE(fixture.service->store().profiles().size(), size_t(2));
  const auto afterUnplug = snapshotOfSettings();
  for (const auto &key : differences(afterStart, afterUnplug))
    QVERIFY2(isArrangementKey(key), qPrintable(key));

  // it is plugged in again: the arrangement of the T comes back, and the config is the very one that it was
  fixture.setDisplays(tShape());
  fixture.changeAt(30000);
  QCOMPARE(text(*fixture.config), textT);
  QVERIFY(captureLayout(*fixture.config).sameAs(layoutT));
  QCOMPARE(fixture.world.restarts, 2);
  QCOMPARE(fixture.world.notices.size(), 2);
  QCOMPARE(fixture.world.notices[1].kind, Notice::Kind::Restored);
  QVERIFY(fixture.world.notices[1].exact);
  for (const auto &key : differences(afterStart, snapshotOfSettings()))
    QVERIFY2(isArrangementKey(key), qPrintable(key));
  // and a third time the laptop alone: the remembered layout, not the fallback
  fixture.setDisplays(laptopOnly());
  fixture.changeAt(60000);
  QCOMPARE(fixture.world.notices.size(), 3);
  QCOMPARE(fixture.world.notices[2].kind, Notice::Kind::Restored);
  QCOMPARE(fixture.world.restarts, 3);
  QCOMPARE(fixture.service->store().profiles().size(), size_t(2));
}

void LayoutHostTests::service_undo_putsTheArrangementBack()
{
  arrange();
  Fixture fixture(m_dir.filePath(QStringLiteral("layout-profiles.json")));
  const auto layoutT = captureLayout(*fixture.config);
  Settings::setValue(Settings::Server::RememberArrangements, true);
  fixture.service->settingsChanged();
  fixture.setDisplays(laptopOnly());
  fixture.changeAt(1000);
  QVERIFY(fixture.service->notice().has_value());
  QVERIFY(fixture.service->notice()->canUndo);
  QVERIFY(fixture.service->undoLayout().has_value());
  QVERIFY(fixture.service->undoLayout()->sameAs(layoutT));

  fixture.world.clock = 60000;
  QVERIFY(fixture.service->undo());
  QVERIFY(captureLayout(*fixture.config).sameAs(layoutT));
  QCOMPARE(fixture.service->notice()->kind, Notice::Kind::Undone);
  QVERIFY(!fixture.service->undo());
  // what was made for the laptop alone is gone, and the arrangement stays what the user has
  QCOMPARE(fixture.service->store().profiles().size(), size_t(1));
}

void LayoutHostTests::service_waitsForADialog()
{
  arrange();
  Fixture fixture(m_dir.filePath(QStringLiteral("layout-profiles.json")));
  Settings::setValue(Settings::Server::RememberArrangements, true);
  fixture.service->settingsChanged();
  const auto layoutT = captureLayout(*fixture.config);

  fixture.world.dialogOpen = true;
  fixture.setDisplays(laptopOnly());
  fixture.changeAt(1000);
  QVERIFY(captureLayout(*fixture.config).sameAs(layoutT));
  QCOMPARE(fixture.world.restarts, 0);

  fixture.world.dialogOpen = false;
  fixture.world.clock += kRetryMs;
  fixture.service->pump();
  QVERIFY(!captureLayout(*fixture.config).sameAs(layoutT));
  QCOMPARE(fixture.world.restarts, 1);
}

void LayoutHostTests::service_configSaved_updatesTheProfile()
{
  arrange();
  Fixture fixture(m_dir.filePath(QStringLiteral("layout-profiles.json")));
  Settings::setValue(Settings::Server::RememberArrangements, true);
  fixture.service->settingsChanged();

  // the dialog was accepted with PC2 moved
  auto edited = captureLayout(*fixture.config);
  for (auto &c : edited.computers) {
    if (c.name == "PC2")
      c.y = 70;
  }
  QVERIFY(applyLayout(*fixture.config, edited).applied);
  fixture.service->configSaved();
  const auto *profile = fixture.service->store().findExact(sig(tShape()));
  QVERIFY(profile != nullptr);
  QVERIFY(profile->layout.sameAs(edited));

  // it is on disk
  LayoutProfileStore again(fixture.storePath);
  again.load();
  QVERIFY(again.findExact(sig(tShape()))->layout.sameAs(edited));
}

void LayoutHostTests::service_switchedOff_stops()
{
  arrange();
  Fixture fixture(m_dir.filePath(QStringLiteral("layout-profiles.json")));
  Settings::setValue(Settings::Server::RememberArrangements, true);
  fixture.service->settingsChanged();
  QVERIFY(fixture.service->enabled());

  Settings::setValue(Settings::Server::RememberArrangements, false);
  fixture.service->settingsChanged();
  QVERIFY(!fixture.service->enabled());
  const auto layout = captureLayout(*fixture.config);
  fixture.setDisplays(laptopOnly());
  fixture.changeAt(1000);
  QVERIFY(captureLayout(*fixture.config).sameAs(layout));
  QCOMPARE(fixture.world.restarts, 0);
  // and the remembered layouts are still in their file
  LayoutProfileStore again(fixture.storePath);
  again.load();
  QCOMPARE(again.profiles().size(), size_t(1));
}

void LayoutHostTests::service_noticeText_saysWhatHappened()
{
  Notice exact;
  exact.kind = Notice::Kind::Restored;
  exact.exact = true;
  exact.profileName = "At the desk";
  exact.restarted = true;
  exact.canUndo = true;
  const auto exactText = LayoutMemoryService::noticeText(exact);
  QVERIFY(exactText.contains("At the desk"));
  QVERIFY(exactText.contains("restored", Qt::CaseInsensitive));
  QVERIFY(exactText.contains("restarted", Qt::CaseInsensitive));
  QVERIFY(!exactText.contains("similar", Qt::CaseInsensitive));

  auto similar = exact;
  similar.exact = false;
  similar.score = 0.875;
  similar.restarted = false;
  similar.restartLater = true;
  const auto similarText = LayoutMemoryService::noticeText(similar);
  QVERIFY(similarText.contains("similar", Qt::CaseInsensitive));
  QVERIFY(similarText.contains("0.88") || similarText.contains("88"));
  QVERIFY(similarText.contains("restart", Qt::CaseInsensitive));

  Notice remembered;
  remembered.kind = Notice::Kind::Remembered;
  remembered.profileName = "external 3440x1440";
  remembered.reattached = {"PC2", "MINI"};
  remembered.notReattached = {"PC9"};
  const auto rememberedText = LayoutMemoryService::noticeText(remembered);
  QVERIFY(rememberedText.contains("PC2"));
  QVERIFY(rememberedText.contains("MINI"));
  QVERIFY(rememberedText.contains("PC9"));

  Notice failed;
  failed.kind = Notice::Kind::Failed;
  failed.profileName = "x";
  failed.detail = "it does not fit";
  QVERIFY(LayoutMemoryService::noticeText(failed).contains("it does not fit"));
  Notice undone;
  undone.kind = Notice::Kind::Undone;
  QVERIFY(LayoutMemoryService::noticeText(undone).contains("undone", Qt::CaseInsensitive));
  Notice store;
  store.kind = Notice::Kind::StoreProblem;
  store.detail = "newer version";
  QVERIFY(LayoutMemoryService::noticeText(store).contains("newer version"));
}

void LayoutHostTests::service_instance_isTheLiveOne()
{
  arrange();
  QVERIFY(LayoutMemoryService::instance() == nullptr);
  {
    Fixture fixture(m_dir.filePath(QStringLiteral("layout-profiles.json")));
    QVERIFY(LayoutMemoryService::instance() == fixture.service.get());
  }
  QVERIFY(LayoutMemoryService::instance() == nullptr);
}

void LayoutHostTests::service_theStoreIsNextToTheSettings()
{
  arrange();
  ServerConfig config;
  World world;
  world.config = &config;
  LayoutMemoryService::Options options;
  options.useTimers = false;
  LayoutMemoryService service(config, std::make_unique<FixedProvider>(), hooksFor(world), options);
  QCOMPARE(
      QFileInfo(service.store().path()).absoluteFilePath(),
      QFileInfo(m_dir.filePath(QStringLiteral("layout-profiles.json"))).absoluteFilePath()
  );
  // nothing was read or written for a feature that is off
  QVERIFY(!QFile::exists(service.store().path()));
}

void LayoutHostTests::service_aNewerStore_isNotUsed()
{
  arrange();
  const auto path = m_dir.filePath(QStringLiteral("layout-profiles.json"));
  const QByteArray content = R"({"format":"deskflow-layout-profiles","version":7,"profiles":[]})";
  {
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
  }
  Fixture fixture(path);
  Settings::setValue(Settings::Server::RememberArrangements, true);
  fixture.service->settingsChanged();
  QVERIFY(fixture.service->notice().has_value());
  QCOMPARE(fixture.service->notice()->kind, Notice::Kind::StoreProblem);

  const auto layout = captureLayout(*fixture.config);
  fixture.setDisplays(laptopOnly());
  fixture.changeAt(1000);
  QVERIFY(captureLayout(*fixture.config).sameAs(layout));
  QCOMPARE(fixture.world.restarts, 0);
  QFile file(path);
  QVERIFY(file.open(QIODevice::ReadOnly));
  QCOMPARE(file.readAll(), content);
}

void LayoutHostTests::displaysFromScreens_areOfTheScreens()
{
  const auto screens = QGuiApplication::screens();
  if (screens.isEmpty())
    QSKIP("no screen");
  const auto displays = displaysFromScreens(screens, QGuiApplication::primaryScreen());
  QCOMPARE(displays.size(), size_t(screens.size()));
  int mains = 0;
  for (const auto &d : displays) {
    QVERIFY(d.w > 0 && d.h > 0);
    QVERIFY(d.scalePercent > 0);
    mains += d.primary ? 1 : 0;
  }
  QCOMPARE(mains, 1);
  QCOMPARE(sig(displays).key(), sig(QtDisplayProvider().displays()).key());
  QVERIFY(makePlatformDisplayProvider() != nullptr);
}

QTEST_MAIN(LayoutHostTests)
