/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "InputIndicatorIpcTests.h"

#include "deskflow/ipc/InputIndicatorIpc.h"

#include <QTest>

using namespace deskflow::input;

void InputIndicatorIpcTests::activityRoundTrips()
{
  const InputIndicatorActivity expected{
      .sequence = 42,
      .eventTimestampMach = 987654321,
      .kind = InputIndicatorActivityKind::LocalPrimaryKeyDown,
  };

  const auto payload = encodeInputIndicatorActivity(expected);
  const auto decoded = decodeInputIndicatorActivity(payload);

  QVERIFY(decoded.has_value());
  QCOMPARE(decoded->sequence, expected.sequence);
  QCOMPARE(decoded->eventTimestampMach, expected.eventTimestampMach);
  QCOMPARE(decoded->kind, expected.kind);
}

void InputIndicatorIpcTests::malformedActivityIsRejected_data()
{
  QTest::addColumn<QString>("payload");

  QTest::newRow("empty") << QString();
  QTest::newRow("version missing") << QStringLiteral("42:99:0");
  QTest::newRow("wrong version") << QStringLiteral("2:42:99:0");
  QTest::newRow("zero sequence") << QStringLiteral("1:0:99:0");
  QTest::newRow("negative sequence") << QStringLiteral("1:-1:99:0");
  QTest::newRow("missing timestamp") << QStringLiteral("1:42::0");
  QTest::newRow("zero timestamp") << QStringLiteral("1:42:0:0");
  QTest::newRow("negative timestamp") << QStringLiteral("1:42:-99:0");
  QTest::newRow("unknown kind") << QStringLiteral("1:42:99:7");
  QTest::newRow("extra field") << QStringLiteral("1:42:99:0:extra");
  QTest::newRow("equals") << QStringLiteral("1:42:99=0:0");
  QTest::newRow("newline") << QStringLiteral("1:42:99:0\n");
}

void InputIndicatorIpcTests::malformedActivityIsRejected()
{
  QFETCH(QString, payload);
  QVERIFY(!decodeInputIndicatorActivity(payload).has_value());
}

void InputIndicatorIpcTests::staleAndFutureActivityIsRejected()
{
  const InputIndicatorActivity current{
      .sequence = 1,
      .eventTimestampMach = 500,
      .kind = InputIndicatorActivityKind::LocalPrimaryMouseClick,
  };

  QVERIFY(isInputIndicatorActivityFresh(current, 700, 200));
  QVERIFY(isInputIndicatorActivityFresh(current, 500, 0));
  QVERIFY(!isInputIndicatorActivityFresh(current, 701, 200));
  QVERIFY(!isInputIndicatorActivityFresh(current, 499, 200));
}

void InputIndicatorIpcTests::newerSequenceWins()
{
  const InputIndicatorActivity next{
      .sequence = 8,
      .eventTimestampMach = 500,
      .kind = InputIndicatorActivityKind::LocalPrimaryMouseClick,
  };

  QVERIFY(isInputIndicatorActivityNewer(next, 7));
  QVERIFY(!isInputIndicatorActivityNewer(next, 8));
  QVERIFY(!isInputIndicatorActivityNewer(next, 9));
}

void InputIndicatorIpcTests::booleanPayloadsAreStrict()
{
  const auto yes = decodeInputIndicatorBoolean(QStringLiteral("1"));
  const auto no = decodeInputIndicatorBoolean(QStringLiteral("0"));

  QVERIFY(yes.has_value());
  QVERIFY(*yes);
  QVERIFY(no.has_value());
  QVERIFY(!*no);
  QVERIFY(!decodeInputIndicatorBoolean(QStringLiteral("true")).has_value());
  QVERIFY(!decodeInputIndicatorBoolean(QStringLiteral("01")).has_value());
  QVERIFY(!decodeInputIndicatorBoolean(QStringLiteral("1:")).has_value());
}

void InputIndicatorIpcTests::subscriptionStateIsProcessLocalAndClearable()
{
  setInputIndicatorSubscribed(false);
  setInputIndicatorLocal(false);
  QVERIFY(!isInputIndicatorSubscribed());
  QVERIFY(!isInputIndicatorLocal());

  setInputIndicatorSubscribed(true);
  setInputIndicatorLocal(true);
  QVERIFY(isInputIndicatorSubscribed());
  QVERIFY(isInputIndicatorLocal());

  setInputIndicatorSubscribed(false);
  setInputIndicatorLocal(false);
  QVERIFY(!isInputIndicatorSubscribed());
  QVERIFY(!isInputIndicatorLocal());
}

QTEST_MAIN(InputIndicatorIpcTests)
