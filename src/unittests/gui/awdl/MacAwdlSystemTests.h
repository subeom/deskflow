/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTemporaryDir>
#include <QTest>

// The real system of the option, with stand-ins for the programs of the system: a shell script for each, in a temporary
// directory, that says what the program says and writes down the arguments that it was given. Not one of them is
// /sbin/ifconfig, /usr/bin/osascript, /sbin/route or /sbin/ping: the tests never reach the interface, the network or a
// dialog. The programs of the system are named only to say that they are the default.
class MacAwdlSystemTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();

  void theDefaultPrograms_areThoseOfTheSystem_byTheirFullPaths();
  void readState_asksForTheInterfaceAndReadsWhatItSays();
  void readState_aProgramThatIsNotThere_isNotKnown();
  void setLink_givesTheArgumentsOfTheConstantCommand_andReadsTheAnswer();
  void findRoute_readsTheRouteAndTheKindOfTheConnection();
  void measure_sendsToTheGatewayOfTheRouteOnly();
  void measure_aRouteThatMayNotBeMeasured_runsNothing();
  void createSystem_isNoneOffscreen_theFakeOfATest_andTheSeamsSayWhereTheOptionIs();

private:
  QString script(const QString &name, const QString &body) const;
  QString recorded(const QString &name) const;

  QTemporaryDir m_dir;
};
