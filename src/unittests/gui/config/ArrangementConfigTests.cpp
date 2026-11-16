/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ArrangementConfigTests.h"

#include "common/Settings.h"
#include "gui/config/ServerConfig.h"

#include <QTextStream>

#include <optional>
#include <vector>

namespace {

// The cells of the classic grid that have a computer: its index (row * 5 + column) and name
using Cells = QList<QPair<int, QString>>;

constexpr int kCells = 15;

// Puts \p cells in the settings the way the server configuration saves them, and \p serverName as the name of this
// computer
void writeGrid(const Cells &cells, const QString &serverName)
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

QString generated(const ServerConfig &config)
{
  QString text;
  QTextStream stream(&text);
  stream << config;
  stream.flush();
  return text;
}

const auto kEmpty = QStringLiteral("section: links\nend\n\nsection: options\nend\n\n");

} // namespace

void ArrangementConfigTests::initTestCase()
{
  QVERIFY(m_settingsDir.isValid());
  Settings::setSettingsFile(m_settingsDir.filePath(QStringLiteral("Deskflow.conf")));
  Settings::setStateFile(m_settingsDir.filePath(QStringLiteral("Deskflow.state")));
}

namespace {
void forget(const QStringList &names);
}

void ArrangementConfigTests::cleanup()
{
  // whatever a test said about the computers it used
  forget({"A", "B", "C", "srv", "PC1", "PC2", "MINI", "SUBEOM-COPILOT", "9800X3D-5080"});
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor);
  Settings::setValue(Settings::Server::UseArrangement);
  ServerConfig::overrideServerMonitors(std::nullopt);
}

void ArrangementConfigTests::classicGrid_generatesTheConfigItAlwaysDid_data()
{
  QTest::addColumn<Cells>("cells");
  QTest::addColumn<QString>("server");
  QTest::addColumn<bool>("experiment");
  QTest::addColumn<QString>("expected");

  // what the user has: a row of four, the Mac that is the server second from the left. This is the text that the
  // shipped build wrote to deskflow-server.conf
  const Cells row = {
      {6, "SUBEOM-COPILOT"}, {7, "Subeoms-MacBook-Pro.local"}, {8, "9800X3D-5080"}, {9, "Subeoms-Mac-mini.local"}
  };
  const auto rowText = QStringLiteral(
      "section: links\n"
      "\tSUBEOM-COPILOT:\n"
      "\t\tright = Subeoms-MacBook-Pro.local\n"
      "\tSubeoms-MacBook-Pro.local:\n"
      "\t\tright = 9800X3D-5080\n"
      "\t\tleft = SUBEOM-COPILOT\n"
      "\t9800X3D-5080:\n"
      "\t\tright = Subeoms-Mac-mini.local\n"
      "\t\tleft = Subeoms-MacBook-Pro.local\n"
      "\tSubeoms-Mac-mini.local:\n"
      "\t\tleft = 9800X3D-5080\n"
      "end\n"
      "\n"
      "section: options\n"
      "end\n"
      "\n"
  );
  QTest::newRow("the row of four, experiment off") << row << "Subeoms-MacBook-Pro.local" << false << rowText;
  QTest::newRow("the row of four, experiment on") << row << "Subeoms-MacBook-Pro.local" << true << rowText;

  // a cross with the server in the middle
  const Cells cross = {{2, "up"}, {6, "left"}, {7, "srv"}, {8, "right"}, {12, "down"}};
  const auto crossText = QStringLiteral(
      "section: links\n"
      "\tup:\n"
      "\t\tdown = srv\n"
      "\tleft:\n"
      "\t\tright = srv\n"
      "\tsrv:\n"
      "\t\tright = right\n"
      "\t\tleft = left\n"
      "\t\tup = up\n"
      "\t\tdown = down\n"
      "\tright:\n"
      "\t\tleft = srv\n"
      "\tdown:\n"
      "\t\tup = srv\n"
      "end\n"
      "\n"
      "section: options\n"
      "end\n"
      "\n"
  );
  QTest::newRow("a cross") << cross << "srv" << false << crossText;
  QTest::newRow("a cross, experiment on") << cross << "srv" << true << crossText;

  // a 2 by 2 block: every computer has two neighbours
  const Cells block = {{6, "a"}, {7, "b"}, {11, "c"}, {12, "d"}};
  const auto blockText = QStringLiteral(
      "section: links\n"
      "\ta:\n"
      "\t\tright = b\n"
      "\t\tdown = c\n"
      "\tb:\n"
      "\t\tleft = a\n"
      "\t\tdown = d\n"
      "\tc:\n"
      "\t\tright = d\n"
      "\t\tup = a\n"
      "\td:\n"
      "\t\tleft = c\n"
      "\t\tup = b\n"
      "end\n"
      "\n"
      "section: options\n"
      "end\n"
      "\n"
  );
  QTest::newRow("a 2 by 2 block") << block << "a" << false << blockText;

  // an empty cell between two computers is a gap: no link; the end of a row is not the start of the next one (cells 4
  // and 5), while the cell below one is its neighbour (4 and 9)
  const Cells gaps = {{4, "end-of-row-0"}, {5, "start-of-row-1"}, {7, "x"}, {9, "y"}};
  const auto gapsText = QStringLiteral(
      "section: links\n"
      "\tend-of-row-0:\n"
      "\t\tdown = y\n"
      "\tstart-of-row-1:\n"
      "\tx:\n"
      "\ty:\n"
      "\t\tup = end-of-row-0\n"
      "end\n"
      "\n"
      "section: options\n"
      "end\n"
      "\n"
  );
  QTest::newRow("gaps and the ends of the rows") << gaps << "x" << false << gapsText;

  QTest::newRow("no computer") << Cells{} << "x" << false << kEmpty;
}

void ArrangementConfigTests::classicGrid_generatesTheConfigItAlwaysDid()
{
  QFETCH(Cells, cells);
  QFETCH(QString, server);
  QFETCH(bool, experiment);
  QFETCH(QString, expected);

  writeGrid(cells, server);
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, experiment);
  const ServerConfig config;
  QCOMPARE(generated(config), expected);
}

namespace {

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

// forget what a test said about the computers
void forget(const QStringList &names)
{
  for (const auto &name : names) {
    for (const auto &key :
         {Settings::Computer::ArrangementX, Settings::Computer::ArrangementY, Settings::Computer::ArrangementZoom,
          Settings::Computer::ArrangementWidth, Settings::Computer::ArrangementHeight,
          Settings::Computer::MonitorLayout})
      Settings::setValue(key.arg(name));
  }
}

// the screen of the MacBook Pro of the user below a DELL, as the operating system says
const std::vector<deskflow::server::MonitorRect> kServerDisplays = {{0, 0, 3440, 1440}, {753, 1440, 2064, 1347}};

} // namespace

void ArrangementConfigTests::arrangement_isUsedOnlyWhenTheExperimentAndTheUseAreOn_data()
{
  QTest::addColumn<bool>("experiment");
  QTest::addColumn<bool>("use");
  QTest::addColumn<bool>("arrangement");
  QTest::newRow("both off") << false << false << false;
  QTest::newRow("the experiment on, the use off") << true << false << false;
  QTest::newRow("the use on, the experiment off") << false << true << false;
  QTest::newRow("both on") << true << true << true;
}

void ArrangementConfigTests::arrangement_isUsedOnlyWhenTheExperimentAndTheUseAreOn()
{
  QFETCH(bool, experiment);
  QFETCH(bool, use);
  QFETCH(bool, arrangement);

  // A, 1920x1080, with B, 2560x1440, on its right: the classic grid links them whole, the arrangement by what touches
  writeGrid({{6, "A"}, {7, "B"}}, "A");
  ServerConfig::overrideServerMonitors(std::vector<deskflow::server::MonitorRect>{});
  place("A", 0, 0, 1.0, 1920, 1080);
  place("B", 1920, 0, 1.0, 2560, 1440);
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, experiment);
  Settings::setValue(Settings::Server::UseArrangement, use);

  const ServerConfig config;
  QCOMPARE(config.usesArrangement(), arrangement);
  const auto classic =
      QStringLiteral("section: links\n\tA:\n\t\tright = B\n\tB:\n\t\tleft = A\nend\n\nsection: options\nend\n\n");
  const auto derived = QStringLiteral(
      "section: links\n\tA:\n\t\tright = B(0,75)\n\tB:\n\t\tleft(0,75) = A\nend\n\nsection: options\nend\n\n"
  );
  QCOMPARE(generated(config), arrangement ? derived : classic);
  forget({"A", "B"});
}

void ArrangementConfigTests::arrangement_ofEqualComputersInTheClassicGrid_isTheClassicText_data()
{
  QTest::addColumn<Cells>("cells");
  QTest::addColumn<QString>("server");
  QTest::addColumn<QString>("expected");
  // the same grids as the pins of the classic grid above: with no place and no size they are the classic grid
  QTest::newRow(
      "a row of four"
  ) << Cells{{6, "SUBEOM-COPILOT"}, {7, "Subeoms-MacBook-Pro.local"}, {8, "9800X3D-5080"}, {9, "Subeoms-Mac-mini.local"}}
    << "Subeoms-MacBook-Pro.local"
    << QStringLiteral(
           "section: links\n"
           "\tSUBEOM-COPILOT:\n"
           "\t\tright = Subeoms-MacBook-Pro.local\n"
           "\tSubeoms-MacBook-Pro.local:\n"
           "\t\tright = 9800X3D-5080\n"
           "\t\tleft = SUBEOM-COPILOT\n"
           "\t9800X3D-5080:\n"
           "\t\tright = Subeoms-Mac-mini.local\n"
           "\t\tleft = Subeoms-MacBook-Pro.local\n"
           "\tSubeoms-Mac-mini.local:\n"
           "\t\tleft = 9800X3D-5080\n"
           "end\n\nsection: options\nend\n\n"
       );
  QTest::newRow("a cross") << Cells{{2, "up"}, {6, "left"}, {7, "srv"}, {8, "right"}, {12, "down"}} << "srv"
                           << QStringLiteral(
                                  "section: links\n"
                                  "\tup:\n\t\tdown = srv\n"
                                  "\tleft:\n\t\tright = srv\n"
                                  "\tsrv:\n\t\tright = right\n\t\tleft = left\n\t\tup = up\n\t\tdown = down\n"
                                  "\tright:\n\t\tleft = srv\n"
                                  "\tdown:\n\t\tup = srv\n"
                                  "end\n\nsection: options\nend\n\n"
                              );
  QTest::newRow("a 2 by 2 block") << Cells{{6, "a"}, {7, "b"}, {11, "c"}, {12, "d"}} << "a"
                                  << QStringLiteral(
                                         "section: links\n"
                                         "\ta:\n\t\tright = b\n\t\tdown = c\n"
                                         "\tb:\n\t\tleft = a\n\t\tdown = d\n"
                                         "\tc:\n\t\tright = d\n\t\tup = a\n"
                                         "\td:\n\t\tleft = c\n\t\tup = b\n"
                                         "end\n\nsection: options\nend\n\n"
                                     );
  QTest::newRow("gaps and the ends of the rows")
      << Cells{{4, "end-of-row-0"}, {5, "start-of-row-1"}, {7, "x"}, {9, "y"}} << "x"
      << QStringLiteral(
             "section: links\n"
             "\tend-of-row-0:\n\t\tdown = y\n"
             "\tstart-of-row-1:\n"
             "\tx:\n"
             "\ty:\n\t\tup = end-of-row-0\n"
             "end\n\nsection: options\nend\n\n"
         );
}

void ArrangementConfigTests::arrangement_ofEqualComputersInTheClassicGrid_isTheClassicText()
{
  QFETCH(Cells, cells);
  QFETCH(QString, server);
  QFETCH(QString, expected);
  writeGrid(cells, server);
  // the screen of this computer is not asked: it is a 1920x1080 one, like the others
  ServerConfig::overrideServerMonitors(std::vector<deskflow::server::MonitorRect>{});
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Settings::setValue(Settings::Server::UseArrangement, true);
  const ServerConfig config;
  QVERIFY(!config.hasArrangement());
  QCOMPARE(generated(config), expected);
}

void ArrangementConfigTests::arrangement_theServerOfTheUser_makesPartialLinks()
{
  writeGrid({{6, "PC1"}, {7, "srv"}, {8, "MINI"}, {12, "PC2"}}, "srv");
  ServerConfig::overrideServerMonitors(kServerDisplays);
  Settings::setValue(Settings::Computer::MonitorLayout.arg("PC1"), QStringLiteral("-1920,360,1920,1080;0,0,2560,1440"));
  place("srv", 0, 0);
  place("PC1", -2560, 0);
  place("MINI", 3440, 0, 1.0, 1920, 1080);
  place("PC2", -847, 1440, 1.0, 1600, 900);
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Settings::setValue(Settings::Server::UseArrangement, true);

  const ServerConfig config;
  QVERIFY(config.hasArrangement());
  QCOMPARE(
      generated(config), QStringLiteral(
                             "section: links\n"
                             "\tPC1:\n"
                             "\t\tright = srv(0,51.6685)\n"
                             "\t\tdown(81.0938,100) = PC2(0,52.9375)\n"
                             "\tsrv:\n"
                             "\t\tright(0,38.7513) = MINI\n"
                             "\t\tleft(0,51.6685) = PC1\n"
                             "\t\tleft(51.6685,83.9612) = PC2\n"
                             "\t\tdown(0,21.8895) = PC2(52.9375,100)\n"
                             "\tMINI:\n"
                             "\t\tleft = srv(0,38.7513)\n"
                             "\tPC2:\n"
                             "\t\tright = srv(51.6685,83.9612)\n"
                             "\t\tup(0,52.9375) = PC1(81.0938,100)\n"
                             "\t\tup(52.9375,100) = srv(0,21.8895)\n"
                             "end\n\nsection: options\nend\n\n"
                         )
  );

}

void ArrangementConfigTests::arrangement_aComputerThatIsNotPlaced_isBesideTheOthers()
{
  // C was added after the others were arranged: it has no place, and links nothing until it is put somewhere
  writeGrid({{6, "A"}, {7, "B"}, {8, "C"}}, "A");
  ServerConfig::overrideServerMonitors(std::vector<deskflow::server::MonitorRect>{});
  place("A", 0, 0);
  place("B", 1920, 0);
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Settings::setValue(Settings::Server::UseArrangement, true);
  const ServerConfig config;
  QCOMPARE(
      generated(config),
      QStringLiteral("section: links\n\tA:\n\t\tright = B\n\tB:\n\t\tleft = A\n\tC:\nend\n\nsection: options\nend\n\n")
  );
  const auto arrangement = config.arrangement();
  const auto &c = arrangement.computers[static_cast<size_t>(arrangement.indexOf("C"))];
  QVERIFY(c.box().x >= 3840 + 100);
  forget({"A", "B", "C"});
}

void ArrangementConfigTests::arrangement_isSavedAndLoaded()
{
  writeGrid({{6, "A"}, {7, "B"}, {8, "C"}}, "A");
  place("A", 0, 0);
  place("B", 1706.667, -12.5, 2.0 / 3.0, 2560, 1440);
  {
    ServerConfig config;
    QVERIFY(config.hasArrangement());
    config.commit();
  }
  {
    const ServerConfig config;
    const auto &computers = config.computers();
    const auto at = [&](const QString &name) -> const Computer & {
      return *std::ranges::find_if(computers, [&](const Computer &c) { return c.name() == name; });
    };
    QVERIFY(at("A").arrangementPlaced());
    QCOMPARE(at("A").arrangementX(), 0.0);
    QVERIFY(at("B").arrangementPlaced());
    QCOMPARE(at("B").arrangementX(), 1706.667);
    QCOMPARE(at("B").arrangementY(), -12.5);
    QVERIFY(std::abs(at("B").arrangementZoom() - 2.0 / 3.0) < 1.0e-9);
    QCOMPARE(at("B").arrangementWidth(), 2560);
    QCOMPARE(at("B").arrangementHeight(), 1440);
    QVERIFY(!at("C").arrangementPlaced());
    // nothing is saved for a computer that has no place, and defaults are not saved
    QVERIFY(!Settings::value(Settings::Computer::ArrangementX.arg("C")).isValid());
    QVERIFY(
        !Settings::value(Settings::Computer::ArrangementZoom.arg("A")).isValid() ||
        Settings::value(Settings::Computer::ArrangementZoom.arg("A")).toDouble() == 1.0
    );
  }
  forget({"A", "B", "C"});
}

void ArrangementConfigTests::arrangement_oldConfigurationsLoadAsTheyWere()
{
  // a configuration of a build that did not know the arrangement: no key
  writeGrid({{6, "A"}, {7, "B"}}, "A");
  forget({"A", "B"});
  ServerConfig config;
  QVERIFY(!config.hasArrangement());
  for (const auto &computer : std::as_const(config).computers()) {
    if (computer.isNull())
      continue;
    QVERIFY(!computer.arrangementPlaced());
    QCOMPARE(computer.arrangementZoom(), 1.0);
    QCOMPARE(computer.arrangementWidth(), 0);
    QCOMPARE(computer.arrangementHeight(), 0);
  }
  // and saving it adds no key
  config.commit();
  QVERIFY(!Settings::value(Settings::Computer::ArrangementX.arg("A")).isValid());
  QVERIFY(!Settings::value(Settings::Computer::ArrangementWidth.arg("B")).toBool());
  QCOMPARE(
      generated(config),
      QStringLiteral("section: links\n\tA:\n\t\tright = B\n\tB:\n\t\tleft = A\nend\n\nsection: options\nend\n\n")
  );
}

void ArrangementConfigTests::arrangement_classicGridConversion_keepsTheLinks()
{
  // the row of the user with sizes: the server is the T, the PCs are what the diagnostics said
  writeGrid({{6, "SUBEOM-COPILOT"}, {7, "srv"}, {8, "9800X3D-5080"}, {9, "MINI"}}, "srv");
  ServerConfig::overrideServerMonitors(kServerDisplays);
  Settings::setValue(
      Settings::Computer::MonitorLayout.arg("9800X3D-5080"), QStringLiteral("-1920,360,1920,1080;0,0,2560,1440")
  );
  Settings::setValue(Settings::Computer::ArrangementZoom.arg("SUBEOM-COPILOT"), 0.8);
  const ServerConfig config;
  const auto conversion = config.classicGridAsArrangement();
  QVERIFY(conversion.notKept.empty());
  QCOMPARE(conversion.arrangement.computers.size(), size_t(4));
  // the server is where the origin is
  QCOMPARE(conversion.arrangement.computers[1].originX, 0.0);
  QCOMPARE(conversion.arrangement.computers[1].originY, 0.0);
  forget({"SUBEOM-COPILOT", "9800X3D-5080"});
}

QTEST_MAIN(ArrangementConfigTests)
