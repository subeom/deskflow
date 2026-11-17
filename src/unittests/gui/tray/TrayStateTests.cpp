/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TrayStateTests.h"

#include "gui/tray/TrayState.h"

#include <QSet>

using namespace deskflow::gui::tray;
using deskflow::core::ConnectionState;
using deskflow::core::ProcessState;

Q_DECLARE_METATYPE(ProcessState)
Q_DECLARE_METATYPE(ConnectionState)
Q_DECLARE_METATYPE(TrayState)

void TrayStateTests::state_isToldByTheStateOfTheCore_data()
{
  QTest::addColumn<ProcessState>("process");
  QTest::addColumn<ConnectionState>("connection");
  QTest::addColumn<bool>("hasMode");
  QTest::addColumn<TrayState>("state");

  using enum ProcessState;
  using enum ConnectionState;
  QTest::newRow("stopped") << Stopped << Disconnected << true << TrayState::Stopped;
  QTest::newRow("stopping") << Stopping << Disconnected << true << TrayState::Stopped;
  QTest::newRow("no mode: nothing to tell") << Stopped << Disconnected << false << TrayState::Idle;
  QTest::newRow("starting") << Starting << Disconnected << true << TrayState::Waiting;
  QTest::newRow("a server that waits for clients") << Started << Listening << true << TrayState::Waiting;
  QTest::newRow("a client that connects") << Started << Connecting << true << TrayState::Waiting;
  QTest::newRow("connected") << Started << Connected << true << TrayState::Connected;
  QTest::newRow("started but disconnected") << Started << Disconnected << true << TrayState::Error;
  QTest::newRow("will try again") << RetryPending << Disconnected << true << TrayState::Error;
  // the connection of a core that has been stopped is whatever it was: it says nothing
  QTest::newRow("stopped, connection that was left") << Stopped << Connected << true << TrayState::Stopped;
}

void TrayStateTests::state_isToldByTheStateOfTheCore()
{
  QFETCH(ProcessState, process);
  QFETCH(ConnectionState, connection);
  QFETCH(bool, hasMode);
  QFETCH(TrayState, state);
  QCOMPARE(trayStateFor(process, connection, hasMode, false), state);
}

void TrayStateTests::startFailed_isAnErrorWhateverTheCoreSays()
{
  for (const auto process : {ProcessState::Starting, ProcessState::Started, ProcessState::Stopping,
                             ProcessState::Stopped, ProcessState::RetryPending}) {
    for (const auto connection : {ConnectionState::Disconnected, ConnectionState::Connecting,
                                  ConnectionState::Connected, ConnectionState::Listening}) {
      QCOMPARE(trayStateFor(process, connection, true, true), TrayState::Error);
      QCOMPARE(trayStateFor(process, connection, false, true), TrayState::Error);
    }
  }
}

void TrayStateTests::names_areEachOneOfAKind()
{
  QSet<QString> names;
  for (const auto state :
       {TrayState::Idle, TrayState::Stopped, TrayState::Waiting, TrayState::Connected, TrayState::Error})
    names.insert(trayStateName(state));
  QCOMPARE(names.size(), 5);
  QVERIFY(names.contains(QStringLiteral("stopped")));
  QVERIFY(names.contains(QStringLiteral("error")));
}

QTEST_APPLESS_MAIN(TrayStateTests)
