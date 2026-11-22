/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "InputIndicatorPolicyTests.h"

#include "gui/input/InputIndicatorPolicy.h"

#include <QTest>

using namespace deskflow::gui::input;
using Source = deskflow::input::InputIndicatorSource;

namespace {

InputIndicatorPolicyInput safeInput()
{
  return {
      .enabled = true,
      .accessibilityTrusted = true,
      .coreConnected = true,
      .isPrimary = true,
      .isOnComputer = true,
      .screenKnown = true,
      .focus = EditableFocusState::SafeEditable,
      .source = Source::LatinABC,
      .probeGeneration = 7,
      .activityGeneration = 7,
      .eventAge = std::chrono::milliseconds{40},
      .maximumEventAge = std::chrono::milliseconds{300},
  };
}

} // namespace

void InputIndicatorPolicyTests::safeStateShowsKnownSource()
{
  const auto decision = InputIndicatorPolicy::decide(safeInput(), std::chrono::milliseconds{1500});

  QVERIFY(decision.show);
  QCOMPARE(decision.source, Source::LatinABC);
  QCOMPARE(decision.dwell, std::chrono::milliseconds{1500});
}

void InputIndicatorPolicyTests::unsafeStateHides_data()
{
  QTest::addColumn<int>("condition");

  QTest::newRow("disabled") << 0;
  QTest::newRow("trust missing") << 1;
  QTest::newRow("core missing") << 2;
  QTest::newRow("not primary") << 3;
  QTest::newRow("off computer") << 4;
  QTest::newRow("screen unknown") << 5;
  QTest::newRow("secure field") << 6;
  QTest::newRow("not editable") << 7;
  QTest::newRow("focus unknown") << 8;
  QTest::newRow("source unknown") << 9;
  QTest::newRow("generation changed") << 10;
}

void InputIndicatorPolicyTests::unsafeStateHides()
{
  QFETCH(int, condition);
  auto input = safeInput();

  switch (condition) {
  case 0:
    input.enabled = false;
    break;
  case 1:
    input.accessibilityTrusted = false;
    break;
  case 2:
    input.coreConnected = false;
    break;
  case 3:
    input.isPrimary = false;
    break;
  case 4:
    input.isOnComputer = false;
    break;
  case 5:
    input.screenKnown = false;
    break;
  case 6:
    input.focus = EditableFocusState::SecureOrProtected;
    break;
  case 7:
    input.focus = EditableFocusState::NotEditable;
    break;
  case 8:
    input.focus = EditableFocusState::Unknown;
    break;
  case 9:
    input.source = Source::Unknown;
    break;
  case 10:
    ++input.probeGeneration;
    break;
  default:
    QFAIL("unknown test row");
  }

  QVERIFY(!InputIndicatorPolicy::decide(input, std::chrono::milliseconds{1500}).show);
}

void InputIndicatorPolicyTests::staleEventHides()
{
  auto input = safeInput();
  input.eventAge = input.maximumEventAge + std::chrono::milliseconds{1};

  QVERIFY(!InputIndicatorPolicy::decide(input, std::chrono::milliseconds{1500}).show);
}

void InputIndicatorPolicyTests::invalidDwellHides()
{
  QVERIFY(!InputIndicatorPolicy::decide(safeInput(), std::chrono::milliseconds{0}).show);
}

QTEST_MAIN(InputIndicatorPolicyTests)
