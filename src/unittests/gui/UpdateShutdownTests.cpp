/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UpdateShutdownTests.h"

#include "gui/UpdateShutdown.h"

#include <QSignalSpy>

namespace {

struct FakeTargets
{
  bool coreRunning = true;
  bool tunnelRunning = true;
  bool coreStopped = false;
  bool tunnelStopped = false;
  int stopCoreCalls = 0;
  int stopTunnelCalls = 0;
  int restoreCoreCalls = 0;
  int restoreTunnelCalls = 0;
  int receiptCalls = 0;

  deskflow::gui::UpdateShutdownTargets values()
  {
    return {
        [this] { ++stopCoreCalls; },
        [this] { return coreStopped; },
        [this] { ++restoreCoreCalls; coreStopped = false; },
        [this] { ++stopTunnelCalls; },
        [this] { return tunnelStopped; },
        [this] { ++restoreTunnelCalls; tunnelStopped = false; },
        [this](const QString &, const QString &) {
          ++receiptCalls;
          return deskflow::gui::UpdateInstallResult{true, {}, QString()};
        },
    };
  }
};

} // namespace

void UpdateShutdownTests::shutdown_waitsForCoreAndTunnelBeforeHelper()
{
  FakeTargets fake;
  deskflow::gui::UpdateShutdown shutdown(100, 100);
  QSignalSpy completed(&shutdown, &deskflow::gui::UpdateShutdown::completed);
  shutdown.begin(fake.values(), QStringLiteral("/tmp/receipt.json"));
  QCOMPARE(fake.stopCoreCalls, 1);
  QVERIFY(!completed.count());

  fake.coreStopped = true;
  shutdown.coreStateChanged();
  QCOMPARE(fake.stopTunnelCalls, 1);
  QVERIFY(!completed.count());

  fake.tunnelStopped = true;
  shutdown.tunnelStateChanged();
  QCOMPARE(completed.count(), 1);
  QCOMPARE(fake.receiptCalls, 1);
}

void UpdateShutdownTests::shutdown_timeoutCancelsApplyAndRestoresRunningState()
{
  FakeTargets fake;
  deskflow::gui::UpdateShutdown shutdown(10, 10);
  QSignalSpy failed(&shutdown, &deskflow::gui::UpdateShutdown::failed);
  shutdown.begin(fake.values(), QStringLiteral("/tmp/receipt.json"));
  QTRY_COMPARE(failed.count(), 1);
  QCOMPARE(fake.restoreCoreCalls, 1);
  QCOMPARE(fake.restoreTunnelCalls, 1);
}

void UpdateShutdownTests::shutdown_ignoresLateCoreAndTunnelSignalsAfterCancellation()
{
  FakeTargets fake;
  deskflow::gui::UpdateShutdown shutdown(20, 20);
  QSignalSpy completed(&shutdown, &deskflow::gui::UpdateShutdown::completed);
  shutdown.begin(fake.values(), QStringLiteral("/tmp/receipt.json"));
  shutdown.cancel();
  fake.coreStopped = true;
  fake.tunnelStopped = true;
  shutdown.coreStateChanged();
  shutdown.tunnelStateChanged();
  QVERIFY(completed.isEmpty());
  QCOMPARE(fake.restoreCoreCalls, 1);
  QCOMPARE(fake.restoreTunnelCalls, 1);
}

void UpdateShutdownTests::shutdown_writesCompleteReceiptOnlyOnce()
{
  FakeTargets fake;
  fake.coreStopped = true;
  fake.tunnelStopped = true;
  deskflow::gui::UpdateShutdown shutdown(100, 100);
  QSignalSpy completed(&shutdown, &deskflow::gui::UpdateShutdown::completed);
  shutdown.begin(fake.values(), QStringLiteral("/tmp/receipt.json"));
  shutdown.coreStateChanged();
  shutdown.tunnelStateChanged();
  shutdown.coreStateChanged();
  QCOMPARE(completed.count(), 1);
  QCOMPARE(fake.receiptCalls, 1);
}

QTEST_MAIN(UpdateShutdownTests)
