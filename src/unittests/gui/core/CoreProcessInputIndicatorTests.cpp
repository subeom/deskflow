/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CoreProcessInputIndicatorTests.h"

#include "FakeDaemonIpcClient.h"
#include "deskflow/ipc/InputIndicatorIpc.h"
#include "gui/core/CoreProcess.h"
#include "gui/ipc/CoreIpcClient.h"
#include "gui/config/ServerConfig.h"

#include <QTest>

#include <mach/mach_time.h>

struct CoreProcessInputIndicatorTestAccess
{
  static void receive(
      deskflow::gui::CoreProcess &process, const QString &command, const QString &args
  )
  {
    process.onCoreIpcMessageReceived(command, args);
  }

  static void reset(deskflow::gui::CoreProcess &process)
  {
    process.resetInputIndicatorState();
  }
};

namespace {

class RecordingCoreIpcClient : public deskflow::gui::ipc::CoreIpcClient
{
public:
  QStringList sent;

protected:
  void sendMessage(const QString &message) override
  {
    sent << message;
  }
};

std::unique_ptr<deskflow::gui::CoreProcess> makeCore()
{
  static ServerConfig config;
  return std::make_unique<deskflow::gui::CoreProcess>(
      config, [](QObject *parent) { return new FakeDaemonIpcClient(parent); }
  );
}

deskflow::input::InputIndicatorActivity validActivity(uint64_t sequence)
{
  return {
      .sequence = sequence,
      .eventTimestampMach = mach_absolute_time(),
      .kind = deskflow::input::InputIndicatorActivityKind::LocalPrimaryKeyDown,
  };
}

} // namespace

void CoreProcessInputIndicatorTests::coreClientEncodesSubscription()
{
  RecordingCoreIpcClient client;
  client.sendInputIndicatorSubscription(true);
  client.sendInputIndicatorSubscription(false);

  QCOMPARE(client.sent.count(), 2);
  QCOMPARE(client.sent.at(0), QStringLiteral("inputIndicatorSubscription=1"));
  QCOMPARE(client.sent.at(1), QStringLiteral("inputIndicatorSubscription=0"));
}

void CoreProcessInputIndicatorTests::validLocalAndActivityAreForwarded()
{
  auto process = makeCore();
  process->setInputIndicatorEnabled(true);
  std::optional<bool> local;
  std::optional<deskflow::input::InputIndicatorActivity> activity;
  connect(process.get(), &deskflow::gui::CoreProcess::inputIndicatorLocalChanged, process.get(),
          [&local](bool value) { local = value; });
  connect(
      process.get(), &deskflow::gui::CoreProcess::inputIndicatorActivityReceived, process.get(),
      [&activity](const deskflow::input::InputIndicatorActivity &value) { activity = value; }
  );

  CoreProcessInputIndicatorTestAccess::receive(
      *process, QLatin1String(deskflow::input::kInputIndicatorLocalCommand), QStringLiteral("1")
  );
  const auto expected = validActivity(7);
  CoreProcessInputIndicatorTestAccess::receive(
      *process, QLatin1String(deskflow::input::kInputIndicatorActivityCommand),
      deskflow::input::encodeInputIndicatorActivity(expected)
  );

  QVERIFY(local.has_value());
  QVERIFY(*local);
  QVERIFY(activity.has_value());
  QCOMPARE(activity->sequence, expected.sequence);
}

void CoreProcessInputIndicatorTests::malformedStaleAndDuplicateActivityInvalidate()
{
  auto process = makeCore();
  process->setInputIndicatorEnabled(true);
  int invalidated = 0;
  int forwarded = 0;
  connect(
      process.get(), &deskflow::gui::CoreProcess::inputIndicatorActivityInvalidated, process.get(),
      [&invalidated] { ++invalidated; }
  );
  connect(
      process.get(), &deskflow::gui::CoreProcess::inputIndicatorActivityReceived, process.get(),
      [&forwarded](const deskflow::input::InputIndicatorActivity &) { ++forwarded; }
  );

  CoreProcessInputIndicatorTestAccess::receive(
      *process, QLatin1String(deskflow::input::kInputIndicatorLocalCommand), QStringLiteral("1")
  );
  CoreProcessInputIndicatorTestAccess::receive(
      *process, QLatin1String(deskflow::input::kInputIndicatorActivityCommand), QStringLiteral("bad")
  );
  auto future = validActivity(2);
  future.eventTimestampMach += 1000000000;
  CoreProcessInputIndicatorTestAccess::receive(
      *process, QLatin1String(deskflow::input::kInputIndicatorActivityCommand),
      deskflow::input::encodeInputIndicatorActivity(future)
  );
  const auto accepted = validActivity(3);
  CoreProcessInputIndicatorTestAccess::receive(
      *process, QLatin1String(deskflow::input::kInputIndicatorActivityCommand),
      deskflow::input::encodeInputIndicatorActivity(accepted)
  );
  CoreProcessInputIndicatorTestAccess::receive(
      *process, QLatin1String(deskflow::input::kInputIndicatorActivityCommand),
      deskflow::input::encodeInputIndicatorActivity(accepted)
  );
  const auto acceptedAfterRejections = validActivity(4);
  CoreProcessInputIndicatorTestAccess::receive(
      *process, QLatin1String(deskflow::input::kInputIndicatorActivityCommand),
      deskflow::input::encodeInputIndicatorActivity(acceptedAfterRejections)
  );

  QCOMPARE(invalidated, 3);
  QCOMPARE(forwarded, 2);
}

void CoreProcessInputIndicatorTests::resetDropsLocalState()
{
  auto process = makeCore();
  process->setInputIndicatorEnabled(true);
  bool local = false;
  int changes = 0;
  connect(
      process.get(), &deskflow::gui::CoreProcess::inputIndicatorLocalChanged, process.get(),
      [&local, &changes](bool value) {
        local = value;
        ++changes;
      }
  );

  CoreProcessInputIndicatorTestAccess::receive(
      *process, QLatin1String(deskflow::input::kInputIndicatorLocalCommand), QStringLiteral("1")
  );
  CoreProcessInputIndicatorTestAccess::reset(*process);

  QVERIFY(!local);
  QCOMPARE(changes, 2);
}

QTEST_MAIN(CoreProcessInputIndicatorTests)
