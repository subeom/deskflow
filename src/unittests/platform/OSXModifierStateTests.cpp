/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "OSXModifierStateTests.h"

#include "platform/OSXModifierState.h"

#include <QTest>

namespace {

constexpr uint32_t kLeftCommand = 54;
constexpr uint32_t kRightCommand = 55;

} // namespace

void OSXModifierStateTests::leftCommandTracksDownAndUp()
{
  OSXModifierState state(kLeftCommand, kRightCommand);

  const auto down = state.update(kLeftCommand, true);
  QVERIFY(down.has_value());
  QCOMPARE(down->id, kKeySuper_L);
  QVERIFY(down->down);

  const auto up = state.update(kLeftCommand, false);
  QVERIFY(up.has_value());
  QCOMPARE(up->id, kKeySuper_L);
  QVERIFY(!up->down);
}

void OSXModifierStateTests::rightCommandTracksDownAndUp()
{
  OSXModifierState state(kLeftCommand, kRightCommand);

  const auto down = state.update(kRightCommand, true);
  QVERIFY(down.has_value());
  QCOMPARE(down->id, kKeySuper_R);
  QVERIFY(down->down);

  const auto up = state.update(kRightCommand, false);
  QVERIFY(up.has_value());
  QCOMPARE(up->id, kKeySuper_R);
  QVERIFY(!up->down);
}

void OSXModifierStateTests::bothCommandsLeftReleasedFirst()
{
  OSXModifierState state(kLeftCommand, kRightCommand);
  QVERIFY(state.update(kLeftCommand, true).has_value());
  QVERIFY(state.update(kRightCommand, true).has_value());

  const auto leftUp = state.update(kLeftCommand, false);
  QVERIFY(leftUp.has_value());
  QCOMPARE(leftUp->id, kKeySuper_L);
  QVERIFY(!leftUp->down);
  QVERIFY(state.leftDown() == false);
  QVERIFY(state.rightDown());

  const auto rightUp = state.update(kRightCommand, false);
  QVERIFY(rightUp.has_value());
  QCOMPARE(rightUp->id, kKeySuper_R);
  QVERIFY(!state.rightDown());
}

void OSXModifierStateTests::bothCommandsRightReleasedFirst()
{
  OSXModifierState state(kLeftCommand, kRightCommand);
  QVERIFY(state.update(kLeftCommand, true).has_value());
  QVERIFY(state.update(kRightCommand, true).has_value());

  const auto rightUp = state.update(kRightCommand, false);
  QVERIFY(rightUp.has_value());
  QCOMPARE(rightUp->id, kKeySuper_R);
  QVERIFY(state.leftDown());
  QVERIFY(!state.rightDown());

  const auto leftUp = state.update(kLeftCommand, false);
  QVERIFY(leftUp.has_value());
  QCOMPARE(leftUp->id, kKeySuper_L);
  QVERIFY(!state.leftDown());
}

void OSXModifierStateTests::duplicateStateDoesNotEmitTransition()
{
  OSXModifierState state(kLeftCommand, kRightCommand);
  QVERIFY(state.update(kLeftCommand, true).has_value());
  QVERIFY(!state.update(kLeftCommand, true).has_value());
}

void OSXModifierStateTests::legacyCommandButtonNormalizesRightSide()
{
  QCOMPARE(
      OSXModifierState::legacyCommandButton(OSXModifierState::rightCommandButton()),
      OSXModifierState::leftCommandButton()
  );
  QCOMPARE(
      OSXModifierState::legacyCommandButton(OSXModifierState::leftCommandButton()),
      OSXModifierState::leftCommandButton()
  );
}

void OSXModifierStateTests::setDownSeedsExistingState()
{
  OSXModifierState state(kLeftCommand, kRightCommand);
  state.setDown(kRightCommand, true);

  const auto release = state.update(kRightCommand, false);
  QVERIFY(release.has_value());
  QCOMPARE(release->id, kKeySuper_R);
  QVERIFY(!release->down);
}

QTEST_MAIN(OSXModifierStateTests)
