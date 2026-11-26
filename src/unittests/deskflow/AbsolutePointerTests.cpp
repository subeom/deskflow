/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AbsolutePointerTests.h"

#include "deskflow/AbsolutePointer.h"

#include <QTest>

using deskflow::pointer::absoluteAxisValue;

// The expected values are what the x64 build of `(DWORD)((65535.0f * x) / (w - 1) + 0.5f)` gives: the float is
// truncated toward zero through a signed 64-bit integer, and a negative result keeps its 32-bit two's complement
// pattern. The conversion of a negative float to an unsigned type is what differs on ARM64 (it saturates to 0), so
// these values are the ones a native ARM64 build has to give too.
void AbsolutePointerTests::normalised_data()
{
  QTest::addColumn<int>("position");
  QTest::addColumn<int>("extent");
  QTest::addColumn<quint32>("expected");

  QTest::newRow("primary left edge") << 0 << 1920 << quint32(0);
  QTest::newRow("primary middle") << 960 << 1920 << quint32(32785);
  QTest::newRow("primary right edge") << 1919 << 1920 << quint32(65535);
  QTest::newRow("primary 1080 high, bottom edge") << 1079 << 1080 << quint32(65535);
  QTest::newRow("monitor right of the primary, first pixel") << 1920 << 1920 << quint32(65569);
  QTest::newRow("monitor right of the primary, last pixel") << 3839 << 1920 << quint32(131104);
  QTest::newRow("1440 high, monitor below, last pixel") << 3999 << 1440 << quint32(182123);
  QTest::newRow("monitor left of the primary, 1 pixel") << -1 << 1920 << quint32(0xFFFFFFDFu);
  QTest::newRow("monitor left of the primary, middle") << -960 << 1920 << quint32(0xFFFF7FF0u);
  QTest::newRow("monitor left of the primary, far edge") << -1920 << 1920 << quint32(0xFFFEFFE0u);
  QTest::newRow("monitor above the primary, far edge") << -1080 << 1080 << quint32(0xFFFEFFC5u);
  QTest::newRow("1440 high, monitor above, far edge") << -2560 << 1440 << quint32(0xFFFE3895u);
  QTest::newRow("largest value of an axis of 2 pixels") << 32767 << 2 << quint32(2147385344u);
  QTest::newRow("smallest value of an axis of 2 pixels") << -32768 << 2 << quint32(2147516416u);
}

void AbsolutePointerTests::normalised()
{
  QFETCH(int, position);
  QFETCH(int, extent);
  QFETCH(quint32, expected);

  QCOMPARE(absoluteAxisValue(position, extent), expected);
}

void AbsolutePointerTests::normalised_negativeIsTruncatedTowardZero()
{
  // -65535 / 100000 + 0.5 is -0.155: the value is 0, not the 32 bits of -1
  QCOMPARE(absoluteAxisValue(-1, 100001), quint32(0));
}

void AbsolutePointerTests::normalised_degenerateExtent_isZero()
{
  // an axis of one pixel divides by zero; x64 gives 0 for the infinite and for the undefined value
  QCOMPARE(absoluteAxisValue(0, 1), quint32(0));
  QCOMPARE(absoluteAxisValue(5, 1), quint32(0));
}

QTEST_MAIN(AbsolutePointerTests)
