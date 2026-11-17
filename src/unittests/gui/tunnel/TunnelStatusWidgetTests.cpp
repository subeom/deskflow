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

  rig.advance(400);
  rig.prober->answer(true);
  rig.advance(400);
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
