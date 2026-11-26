/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "OSXInputIndicatorRelayTests.h"

#include "deskflow/ipc/InputIndicatorIpc.h"
#include "platform/OSXInputIndicatorRelay.h"

#include <QTest>

#include <functional>
#include <vector>

using namespace deskflow::input;
using deskflow::platform::OSXInputIndicatorRelay;

void OSXInputIndicatorRelayTests::init()
{
  setInputIndicatorSubscribed(true);
}

void OSXInputIndicatorRelayTests::coalescesToLatestActivity()
{
  std::vector<std::function<void()>> scheduled;
  std::vector<InputIndicatorActivity> sent;
  OSXInputIndicatorRelay relay(
      [&scheduled](std::function<void()> work) { scheduled.push_back(std::move(work)); },
      [&sent](const InputIndicatorActivity &activity) { sent.push_back(activity); }
  );

  relay.notify(
      InputIndicatorActivityKind::LocalPrimaryMouseClick, 100, true, true
  );
  relay.notify(InputIndicatorActivityKind::LocalPrimaryKeyDown, 120, true, true);

  QCOMPARE(scheduled.size(), size_t{1});
  QVERIFY(sent.empty());

  scheduled.front()();

  QCOMPARE(sent.size(), size_t{1});
  QCOMPARE(sent.front().kind, InputIndicatorActivityKind::LocalPrimaryKeyDown);
  QCOMPARE(sent.front().eventTimestampMach, uint64_t{120});
  QVERIFY(sent.front().sequence > 0);

  relay.notify(InputIndicatorActivityKind::LocalPrimaryMouseClick, 140, true, true);
  QCOMPARE(scheduled.size(), size_t{2});
  scheduled.back()();
  QCOMPARE(sent.size(), size_t{2});
  QVERIFY(sent.back().sequence > sent.front().sequence);
}

void OSXInputIndicatorRelayTests::rejectsRemoteAndSecondaryActivity()
{
  std::vector<std::function<void()>> scheduled;
  std::vector<InputIndicatorActivity> sent;
  OSXInputIndicatorRelay relay(
      [&scheduled](std::function<void()> work) { scheduled.push_back(std::move(work)); },
      [&sent](const InputIndicatorActivity &activity) { sent.push_back(activity); }
  );

  relay.notify(InputIndicatorActivityKind::LocalPrimaryKeyDown, 100, false, true);
  relay.notify(InputIndicatorActivityKind::LocalPrimaryKeyDown, 101, true, false);
  relay.notify(InputIndicatorActivityKind::LocalPrimaryMouseClick, 102, false, false);

  QVERIFY(scheduled.empty());
  QVERIFY(sent.empty());
}

void OSXInputIndicatorRelayTests::unsubscribeAndInvalidationDropQueuedWork()
{
  std::vector<std::function<void()>> scheduled;
  std::vector<InputIndicatorActivity> sent;
  OSXInputIndicatorRelay relay(
      [&scheduled](std::function<void()> work) { scheduled.push_back(std::move(work)); },
      [&sent](const InputIndicatorActivity &activity) { sent.push_back(activity); }
  );

  relay.notify(InputIndicatorActivityKind::LocalPrimaryKeyDown, 100, true, true);
  relay.invalidate();
  QVERIFY(!scheduled.empty());
  scheduled.front()();
  QVERIFY(sent.empty());

  setInputIndicatorSubscribed(false);
  relay.notify(InputIndicatorActivityKind::LocalPrimaryKeyDown, 110, true, true);
  QVERIFY(sent.empty());
  QVERIFY(scheduled.size() == 1);

  setInputIndicatorSubscribed(true);
  relay.notify(InputIndicatorActivityKind::LocalPrimaryKeyDown, 120, true, true);
  QCOMPARE(scheduled.size(), size_t{2});
  setInputIndicatorSubscribed(false);
  setInputIndicatorSubscribed(true);
  scheduled.back()();
  QVERIFY(sent.empty());
}

QTEST_MAIN(OSXInputIndicatorRelayTests)
