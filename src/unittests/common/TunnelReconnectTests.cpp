/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TunnelReconnectTests.h"

#include "common/TunnelReconnect.h"

using namespace deskflow::tunnel;

// The retry intervals of the client while the Cloudflare Tunnel is used. A direct link retries every second whatever
// happened (ClientApp::retryTime); these are only for the tunnel.

void TunnelReconnectTests::theFirstRetryIsQuick()
{
  // a session that was working ended, most often because one of the connections of the tunnel was lost: the others are
  // up and a new connection is accepted at once, so the first try does not wait for a whole second
  QCOMPARE(reconnectSeconds(0), 0.25);
  QVERIFY(reconnectSeconds(0) < reconnectSeconds(1));
  QCOMPARE(reconnectSeconds(-3), reconnectSeconds(0));
}

void TunnelReconnectTests::theNextRetriesKeepTheSecondOfADirectLink_data()
{
  QTest::addColumn<int>("retries");
  QTest::newRow("second") << 1;
  QTest::newRow("fifth") << 5;
  QTest::newRow("last of the steady ones") << 15;
}

void TunnelReconnectTests::theNextRetriesKeepTheSecondOfADirectLink()
{
  QFETCH(int, retries);
  QCOMPARE(reconnectSeconds(retries), 1.0);
}

// a tunnel that is down for minutes is asked about once a second for as long as the client runs without this: every
// try is a new connection through Cloudflare (a login of the Service Token, a request that is refused) and a line in
// the log
void TunnelReconnectTests::aLongOutageEasesTheRetriesAndNeverStopsThem()
{
  QCOMPARE(reconnectSeconds(16), 2.0);
  QCOMPARE(reconnectSeconds(45), 2.0);
  QCOMPARE(reconnectSeconds(46), 4.0);
  QCOMPARE(reconnectSeconds(100000), 4.0);

  // never more than this: the client is back within that long after the path is
  QVERIFY(reconnectSeconds(1'000'000) <= 5.0);
}

void TunnelReconnectTests::everyIntervalIsUsable()
{
  double previous = 0.0;
  for (int retries = 0; retries < 200; ++retries) {
    const auto seconds = reconnectSeconds(retries);
    // a timer of 0 would be a loop that never rests, and the interval never gets shorter as the outage goes on
    QVERIFY2(seconds >= 0.1, qPrintable(QString::number(retries)));
    QVERIFY2(seconds >= previous || retries == 1, qPrintable(QString::number(retries)));
    previous = seconds;
  }
}

void TunnelReconnectTests::aPolicyThatIsGivenIsFollowed()
{
  ReconnectPolicy policy;
  policy.firstSeconds = 0.5;
  policy.steadySeconds = 3.0;
  policy.steadyRetries = 2;
  policy.easedSeconds = 6.0;
  policy.easedRetries = 1;
  policy.longSeconds = 9.0;

  QCOMPARE(reconnectSeconds(0, policy), 0.5);
  QCOMPARE(reconnectSeconds(1, policy), 3.0);
  QCOMPARE(reconnectSeconds(2, policy), 3.0);
  QCOMPARE(reconnectSeconds(3, policy), 6.0);
  QCOMPARE(reconnectSeconds(4, policy), 9.0);
}

QTEST_MAIN(TunnelReconnectTests)
