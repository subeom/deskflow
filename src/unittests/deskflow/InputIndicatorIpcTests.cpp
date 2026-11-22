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

void InputIndicatorIpcTests::subscriptionCapabilityIsExactAndRedacted()
{
  const QString capability(64, QLatin1Char('a'));
  const auto payload = encodeInputIndicatorSubscription(true, capability);
  const auto decoded = decodeInputIndicatorSubscription(payload, capability);

  QVERIFY(decoded.has_value());
  QVERIFY(*decoded);
  QVERIFY(!decodeInputIndicatorSubscription(payload, QString(64, QLatin1Char('b'))).has_value());
  QVERIFY(!decodeInputIndicatorSubscription(QStringLiteral("1:short"), capability).has_value());

  const auto raw =
      QStringLiteral("%1=%2")
          .arg(QLatin1String(kInputIndicatorSubscriptionCommand), payload)
          .toLatin1();
  const auto redacted = redactInputIndicatorMessage(raw);
  QVERIFY(redacted.contains(QStringLiteral("<redacted>")));
  QVERIFY(!redacted.contains(capability));
}

void InputIndicatorIpcTests::rawIndicatorMessagesAreStrict()
{
  const QByteArray valid("inputIndicatorLocal=1");
  const auto parsed = parseInputIndicatorRawMessage(valid);
  QCOMPARE(parsed.state, InputIndicatorRawMessageState::Valid);
  QCOMPARE(parsed.command, QStringLiteral("inputIndicatorLocal"));
  QCOMPARE(parsed.payload, QStringLiteral("1"));

  const QList<QByteArray> invalid{
      QByteArray("inputIndicatorLocal=1=extra"),
      QByteArray("inputIndicatorLocal="),
      QByteArray("inputIndicatorLocal =1"),
      QByteArray("inputIndicatorLocal=1\r"),
      QByteArray("inputIndicatorLocal=1\0extra", 24),
      QByteArray("prefix-inputIndicatorLocal=1"),
      QByteArray("inputIndicatorLocal=") + QByteArray(300, 'x'),
  };
  for (const auto &message : invalid) {
    QCOMPARE(parseInputIndicatorRawMessage(message).state, InputIndicatorRawMessageState::Invalid);
  }
  QCOMPARE(
      parseInputIndicatorRawMessage(QByteArray("connectionState=Connected")).state,
      InputIndicatorRawMessageState::NotIndicator
  );
}

void InputIndicatorIpcTests::subscriptionStateIsProcessLocalAndClearable()
{
  setInputIndicatorSubscribed(false);
  setInputIndicatorLocal(false);
  const auto before = inputIndicatorSubscriptionEpoch();
  QVERIFY(!isInputIndicatorSubscribed());
  QVERIFY(!isInputIndicatorLocal());

  setInputIndicatorSubscribed(true);
  setInputIndicatorLocal(true);
  QVERIFY(isInputIndicatorSubscribed());
  QVERIFY(isInputIndicatorLocal());
  QVERIFY(inputIndicatorSubscriptionEpoch() > before);

  const auto subscribedEpoch = inputIndicatorSubscriptionEpoch();
  setInputIndicatorSubscribed(true);
  QCOMPARE(inputIndicatorSubscriptionEpoch(), subscribedEpoch);
  setInputIndicatorSubscribed(false);
  setInputIndicatorLocal(false);
  QVERIFY(!isInputIndicatorSubscribed());
  QVERIFY(!isInputIndicatorLocal());
}

QTEST_MAIN(InputIndicatorIpcTests)
