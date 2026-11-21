/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TunnelStatusWidgetTests.h"

#include "TunnelTestDoubles.h"

#include "gui/widgets/TunnelStatusWidget.h"

#include <QLabel>
#include <QSignalSpy>
#include <QTemporaryDir>

using namespace deskflow::tunnel;
using namespace deskflow::gui;
using namespace tunneltest;

namespace {

QTemporaryDir &scratchDir()
{
  static QTemporaryDir dir;
  return dir;
}

QByteArray readAll(const QString &path)
{
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray("<unreadable>");
}

QLabel *pathLabel(const TunnelStatusWidget &widget)
{
  return widget.findChild<QLabel *>(QStringLiteral("tunnelPathLabel"));
}

QLabel *statusLabel(const TunnelStatusWidget &widget)
{
  return widget.findChild<QLabel *>(QStringLiteral("tunnelStatusLabel"));
}

} // namespace

void TunnelStatusWidgetTests::initTestCase()
{
  QVERIFY(scratchDir().isValid());
  m_dir = scratchDir().path();
}

void TunnelStatusWidgetTests::thePathLine_namesTheHostAndTheLocalPort()
{
  QCOMPARE(
      tunnelPathLine(QStringLiteral("mbp.example.com"), 24800),
      QStringLiteral("Via Cloudflare Tunnel: mbp.example.com (local 127.0.0.1:24800)")
  );
  QCOMPARE(
      tunnelPathLine(QStringLiteral("mbp.example.com"), 51234),
      QStringLiteral("Via Cloudflare Tunnel: mbp.example.com (local 127.0.0.1:51234)")
  );
  QCOMPARE(tunnelPathLine(QString(), 24800), QStringLiteral("Via Cloudflare Tunnel: no hostname is set"));
  QCOMPARE(tunnelPathLine(QStringLiteral("  "), 24800), QStringLiteral("Via Cloudflare Tunnel: no hostname is set"));
}

void TunnelStatusWidgetTests::theStatusLine_keepsThePortAndTheServerApart()
{
  TunnelStatus listening;
  listening.state = TunnelState::Listening;

  // the local port is open: that is all that the tunnel knows
  QCOMPARE(tunnelStatusLine(listening, ServerLink::Unknown), QStringLiteral("Tunnel: listening (local port open)"));
  // and the connection to the server over it is another thing, told by the core
  QCOMPARE(
      tunnelStatusLine(listening, ServerLink::Connecting),
      QStringLiteral("Tunnel: listening (local port open) | Server: connecting")
  );
  QCOMPARE(
      tunnelStatusLine(listening, ServerLink::Connected),
      QStringLiteral("Tunnel: listening (local port open) | Server: connected")
  );

  // a tunnel that does not listen has no connection through it to tell about
  for (const auto state :
       {TunnelState::Idle, TunnelState::Starting, TunnelState::Stopping, TunnelState::Backoff, TunnelState::Failed}) {
    TunnelStatus status;
    status.state = state;
    status.failure = state == TunnelState::Failed ? TunnelFailure::LoginRequired : TunnelFailure::None;
    QCOMPARE(tunnelStatusLine(status, ServerLink::Connected), status.text());
    QVERIFY(!tunnelStatusLine(status, ServerLink::Connected).contains(QStringLiteral("Server:")));
  }

  // a problem of the connection stays in the text of the tunnel
  listening.failure = TunnelFailure::HandshakeRejected;
  QVERIFY(tunnelStatusLine(listening, ServerLink::Connecting).contains(failureText(TunnelFailure::HandshakeRejected)));
}

// The server was reached through this tunnel and is not now: that is most often the tunnel of the server making its
// connections to Cloudflare again (seconds), and not a mistake in the settings. The line says that the client is
// reconnecting, and does not tell a person to look at the Access policy.
void TunnelStatusWidgetTests::theStatusLine_saysReconnectingAfterAServerThatWasReached()
{
  TunnelStatus listening;
  listening.state = TunnelState::Listening;
  QCOMPARE(
      tunnelStatusLine(listening, ServerLink::Reconnecting),
      QStringLiteral("Tunnel: listening (local port open) | Server: reconnecting")
  );

  for (const auto failure :
       {TunnelFailure::HandshakeRejected, TunnelFailure::ConnectFailed, TunnelFailure::EdgeUnreachable,
        TunnelFailure::HostUnresolvable}) {
    listening.failure = failure;
    const auto line = tunnelStatusLine(listening, ServerLink::Reconnecting);
    QCOMPARE(
        line, QStringLiteral(
                  "Tunnel: listening (local port open) | Server: reconnecting, the path through Cloudflare is not "
                  "available now"
              )
    );
    QVERIFY(!line.contains(failureText(failure)));
    // the same failure while the server was never reached is still told as it is
    QVERIFY(tunnelStatusLine(listening, ServerLink::Connecting).contains(failureText(failure)));
  }

  // a failure that is not about the path stays as it is
  listening.failure = TunnelFailure::LoginRequired;
  QVERIFY(tunnelStatusLine(listening, ServerLink::Reconnecting).contains(failureText(TunnelFailure::LoginRequired)));

  // a tunnel that does not listen says what it says
  for (const auto state :
       {TunnelState::Idle, TunnelState::Starting, TunnelState::Stopping, TunnelState::Backoff, TunnelState::Failed}) {
    TunnelStatus status;
    status.state = state;
    QCOMPARE(tunnelStatusLine(status, ServerLink::Reconnecting), status.text());
  }
}

void TunnelStatusWidgetTests::aPathProblem_isTransient_aMistakeIsNot_data()
{
  QTest::addColumn<int>("failure");
  QTest::addColumn<bool>("transient");
  QTest::newRow("none") << int(TunnelFailure::None) << false;
  QTest::newRow("handshake refused") << int(TunnelFailure::HandshakeRejected) << true;
  QTest::newRow("could not connect") << int(TunnelFailure::ConnectFailed) << true;
  QTest::newRow("edge unreachable") << int(TunnelFailure::EdgeUnreachable) << true;
  QTest::newRow("host does not resolve") << int(TunnelFailure::HostUnresolvable) << true;
  QTest::newRow("login asked for") << int(TunnelFailure::LoginRequired) << false;
  QTest::newRow("no credentials") << int(TunnelFailure::CredentialsMissing) << false;
  QTest::newRow("tls required") << int(TunnelFailure::TlsRequired) << false;
  QTest::newRow("invalid hostname") << int(TunnelFailure::InvalidHostname) << false;
  QTest::newRow("cloudflared missing") << int(TunnelFailure::CloudflaredMissing) << false;
  QTest::newRow("port busy") << int(TunnelFailure::PortBusy) << false;
  QTest::newRow("process exited") << int(TunnelFailure::ProcessExited) << false;
  QTest::newRow("start timed out") << int(TunnelFailure::StartTimedOut) << false;
  QTest::newRow("settings changed") << int(TunnelFailure::SettingsChanged) << false;
}

void TunnelStatusWidgetTests::aPathProblem_isTransient_aMistakeIsNot()
{
  QFETCH(int, failure);
  QFETCH(bool, transient);
  QCOMPARE(isTransientPathFailure(TunnelFailure(failure)), transient);
}

// the colour of the line and of the chip: a problem, unless it is one that passes and the server was reached before
void TunnelStatusWidgetTests::theColourOfAProblem_followsWhetherTheServerWasReached()
{
  TunnelStatus status;
  status.state = TunnelState::Listening;
  for (const auto link :
       {ServerLink::Unknown, ServerLink::Connecting, ServerLink::Connected, ServerLink::Reconnecting}) {
    status.failure = TunnelFailure::None;
    QVERIFY(!tunnelStatusIsProblem(status, link));
  }

  status.failure = TunnelFailure::HandshakeRejected;
  QVERIFY(tunnelStatusIsProblem(status, ServerLink::Unknown));
  QVERIFY(tunnelStatusIsProblem(status, ServerLink::Connecting));
  QVERIFY(tunnelStatusIsProblem(status, ServerLink::Connected));
  QVERIFY(!tunnelStatusIsProblem(status, ServerLink::Reconnecting));

  status.failure = TunnelFailure::LoginRequired;
  QVERIFY(tunnelStatusIsProblem(status, ServerLink::Reconnecting));

  // a tunnel that failed is a problem, with or without a link
  status.state = TunnelState::Failed;
  status.failure = TunnelFailure::HandshakeRejected;
  QVERIFY(tunnelStatusIsProblem(status, ServerLink::Reconnecting));
  status.failure = TunnelFailure::None;
  QVERIFY(tunnelStatusIsProblem(status, ServerLink::Unknown));

  // one that waits to be started again is not a problem of the server link
  status.state = TunnelState::Backoff;
  QVERIFY(!tunnelStatusIsProblem(status, ServerLink::Reconnecting));
}

// The window gives the tracker what the core says; the tracker says how the link is.
void TunnelStatusWidgetTests::theServerLinkTracker_remembersThatTheServerWasReached()
{
  ServerLinkTracker tracker;
  using Observation = ServerObservation;

  // before the server was reached: connecting is connecting, and nothing is "again"
  QCOMPARE(tracker.update(Observation::Disconnected, true), ServerLink::Unknown);
  QCOMPARE(tracker.update(Observation::Connecting, true), ServerLink::Connecting);
  QCOMPARE(tracker.update(Observation::Disconnected, true), ServerLink::Unknown);
  QCOMPARE(tracker.update(Observation::Connected, true), ServerLink::Connected);

  // after it: every state that is not connected is a reconnect
  QCOMPARE(tracker.update(Observation::Disconnected, true), ServerLink::Reconnecting);
  QCOMPARE(tracker.update(Observation::Connecting, true), ServerLink::Reconnecting);
  QCOMPARE(tracker.update(Observation::Disconnected, true), ServerLink::Reconnecting);
  QCOMPARE(tracker.update(Observation::Connected, true), ServerLink::Connected);

  // the core that is stopped is not reconnecting: the memory is gone with it
  QCOMPARE(tracker.update(Observation::Disconnected, false), ServerLink::Unknown);
  QCOMPARE(tracker.update(Observation::Connecting, true), ServerLink::Connecting);

  // and a tunnel that stopped forgets as well
  QCOMPARE(tracker.update(Observation::Connected, true), ServerLink::Connected);
  tracker.reset();
  QCOMPARE(tracker.update(Observation::Disconnected, true), ServerLink::Unknown);
}

void TunnelStatusWidgetTests::whileTheTunnelIsOff_theWidgetIsHiddenAndWritesNothing()
{
  ControllerRig rig(m_dir);
  const auto before = readAll(rig.file);

  TunnelStatusWidget widget(rig.controller.get());
  QSignalSpy used(&widget, &TunnelStatusWidget::usedChanged);
  widget.show();
  widget.setServerLink(ServerLink::Connected);
  widget.refresh();

  QVERIFY(!widget.isTunnelUsed());
  QVERIFY(!widget.isVisible());
  QCOMPARE(used.count(), 0);
  QCOMPARE(readAll(rig.file), before);
}

void TunnelStatusWidgetTests::whileTheTunnelIsOn_theWidgetSaysWhichHostIsDialled()
{
  ControllerRig rig(m_dir);
  TunnelStatusWidget widget(rig.controller.get());
  QSignalSpy used(&widget, &TunnelStatusWidget::usedChanged);
  QVERIFY(!widget.isTunnelUsed());

  rig.turnOn();
  widget.refresh();
  QVERIFY(widget.isTunnelUsed());
  QCOMPARE(used.count(), 1);
  QCOMPARE(used.takeFirst().at(0).toBool(), true);
  QCOMPARE(pathLabel(widget)->text(), QStringLiteral("Via Cloudflare Tunnel: mbp.example.com (local 127.0.0.1:24800)"));
  QVERIFY(!pathLabel(widget)->text().contains(QStringLiteral("10.0.0.5")));

  // the saved address is not what is shown, and is not changed by showing the tunnel
  Settings::setValue(Settings::Client::RemoteHost, QStringLiteral("10.0.0.5"));
  widget.refresh();
  QVERIFY(!pathLabel(widget)->text().contains(QStringLiteral("10.0.0.5")));
  QCOMPARE(Settings::textValue(Settings::Client::RemoteHost), QStringLiteral("10.0.0.5"));

  // the switch to the server hides it
  rig.controller->setClientMode(false);
  widget.refresh();
  QVERIFY(!widget.isTunnelUsed());
  QCOMPARE(used.count(), 1);
  QCOMPARE(used.takeFirst().at(0).toBool(), false);
}

void TunnelStatusWidgetTests::theWidget_followsTheStateOfTheTunnelAndOfTheServerLink()
{
  ControllerRig rig(m_dir);
  rig.turnOn();
  TunnelStatusWidget widget(rig.controller.get());
  QSignalSpy texts(&widget, &TunnelStatusWidget::statusTextChanged);
  QCOMPARE(statusLabel(widget)->text(), QStringLiteral("Tunnel: stopped"));

  rig.controller->acquire();
  QCOMPARE(statusLabel(widget)->text(), QStringLiteral("Tunnel: starting"));

  rig.finishStart();
  QCOMPARE(statusLabel(widget)->text(), QStringLiteral("Tunnel: listening (local port open)"));
  QCOMPARE(widget.statusText(), statusLabel(widget)->text());

  widget.setServerLink(ServerLink::Connecting);
  QCOMPARE(statusLabel(widget)->text(), QStringLiteral("Tunnel: listening (local port open) | Server: connecting"));
  widget.setServerLink(ServerLink::Connected);
  QCOMPARE(statusLabel(widget)->text(), QStringLiteral("Tunnel: listening (local port open) | Server: connected"));
  QVERIFY(texts.count() >= 4);
  QCOMPARE(texts.last().at(0).toString(), statusLabel(widget)->text());

  rig.process()->write(QStringLiteral("INF A browser window should have opened at the following URL:"));
  QVERIFY(statusLabel(widget)->text().startsWith(QStringLiteral("Tunnel: failed (")));
  QVERIFY(!statusLabel(widget)->text().contains(QStringLiteral("Server:")));
}

void TunnelStatusWidgetTests::aPortThatIsInUseInsteadOfTheAskedOne_isShown()
{
  ControllerRig rig(m_dir);
  rig.turnOn();
  rig.prober->preferredFree = false;
  TunnelStatusWidget widget(rig.controller.get());

  rig.controller->acquire();
  QCOMPARE(
      pathLabel(widget)->text(),
      QStringLiteral("Via Cloudflare Tunnel: mbp.example.com (local 127.0.0.1:%1)").arg(kFree)
  );

  rig.controller->setClientMode(false);
  rig.controller->setClientMode(true);
  widget.refresh();
  // stopped: the port that is asked for again
  QCOMPARE(pathLabel(widget)->text(), QStringLiteral("Via Cloudflare Tunnel: mbp.example.com (local 127.0.0.1:24800)"));
}

void TunnelStatusWidgetTests::theLines_doNotWrapAndHaveTheirTextAsToolTip()
{
  ControllerRig rig(m_dir);
  rig.turnOn(QStringLiteral("a-very-long-name-for-a-host-of-a-tunnel.subdomain.example.com"));
  TunnelStatusWidget widget(rig.controller.get());
  widget.refresh();

  for (auto *label : {pathLabel(widget), statusLabel(widget)}) {
    QVERIFY(label != nullptr);
    QVERIFY(!label->wordWrap());
    QCOMPARE(label->sizePolicy().horizontalPolicy(), QSizePolicy::Ignored);
    QCOMPARE(label->toolTip(), label->text());
  }
}

QTEST_MAIN(TunnelStatusWidgetTests)
