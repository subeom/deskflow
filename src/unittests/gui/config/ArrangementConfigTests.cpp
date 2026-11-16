/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ArrangementConfigTests.h"

#include "common/Settings.h"
#include "gui/config/ServerConfig.h"

#include <QTextStream>

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

void ArrangementConfigTests::cleanup()
{
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor);
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

QTEST_MAIN(ArrangementConfigTests)
