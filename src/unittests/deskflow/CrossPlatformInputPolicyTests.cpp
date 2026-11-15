/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CrossPlatformInputPolicyTests.h"

#include "deskflow/CrossPlatformInputPolicy.h"

#include <QTest>

namespace {

InputCapabilities capabilities(InputPlatform platform, InputCapabilityMask mask = 0)
{
  return {platform, mask};
}

CrossPlatformInputOptions hangulOptions()
{
  return {.rightCommand = RightCommandAction::Hangul};
}

} // namespace

void CrossPlatformInputPolicyTests::samePlatform_preservesRightCommand()
{
  const CrossPlatformInputPolicy policy(
      capabilities(InputPlatform::MacOS), capabilities(InputPlatform::MacOS, kInputCapabilityHangul), hangulOptions()
  );

  const auto wire = policy.toWireKey(kKeySuper_R, KeyModifierSuper);
  QCOMPARE(wire.id, kKeySuper_L);
  QCOMPARE(wire.mask, KeyModifierSuper);
  QVERIFY(!policy.mapsRightCommandToHangul());
  QVERIFY(!policy.shouldSuppressRepeat(kKeySuper_R));
}

void CrossPlatformInputPolicyTests::unknownPlatform_preservesRightCommand()
{
  const CrossPlatformInputPolicy policy(
      capabilities(InputPlatform::Unknown), capabilities(InputPlatform::Windows, kInputCapabilityHangul),
      hangulOptions()
  );

  const auto wire = policy.toWireKey(kKeySuper_R, KeyModifierSuper);
  QCOMPARE(wire.id, kKeySuper_R);
  QCOMPARE(wire.mask, KeyModifierSuper);
  QVERIFY(!policy.mapsRightCommandToHangul());
}

void CrossPlatformInputPolicyTests::disabledOption_preservesRightCommand()
{
  const CrossPlatformInputPolicy policy(
      capabilities(InputPlatform::MacOS), capabilities(InputPlatform::Windows, kInputCapabilityHangul), {}
  );

  const auto wire = policy.toWireKey(kKeySuper_R, KeyModifierSuper);
  QCOMPARE(wire.id, kKeySuper_L);
  QCOMPARE(wire.mask, KeyModifierSuper);
  const auto destination = policy.toDestinationKey(kKeySuper_R, KeyModifierSuper);
  QCOMPARE(destination.id, kKeySuper_L);
  QCOMPARE(destination.mask, KeyModifierSuper);
  QVERIFY(!policy.mapsRightCommandToHangul());
}

void CrossPlatformInputPolicyTests::missingHangulCapability_preservesRightCommand()
{
  const CrossPlatformInputPolicy policy(
      capabilities(InputPlatform::MacOS), capabilities(InputPlatform::Windows, kInputCapabilityProtocolV19),
      hangulOptions()
  );

  const auto wire = policy.toWireKey(kKeySuper_R, KeyModifierSuper);
  QCOMPARE(wire.id, kKeySuper_L);
  QCOMPARE(wire.mask, KeyModifierSuper);
  QVERIFY(!policy.mapsRightCommandToHangul());
}

void CrossPlatformInputPolicyTests::macToWindows_hangulMapsDestinationKeyAndSuppressesRepeat()
{
  const CrossPlatformInputPolicy policy(
      capabilities(InputPlatform::MacOS), capabilities(InputPlatform::Windows, kInputCapabilityHangul), hangulOptions()
  );

  const auto wire = policy.toWireKey(kKeySuper_R, KeyModifierSuper);
  QCOMPARE(wire.id, kKeySuper_R);
  QCOMPARE(wire.mask, KeyModifierSuper);
  QVERIFY(policy.shouldSuppressRepeat(kKeySuper_R));

  const auto destination = policy.toDestinationKey(kKeySuper_R, KeyModifierSuper);
  QCOMPARE(destination.id, kKeyHangul);
  QCOMPARE(destination.mask, KeyModifierMask{0});
}

void CrossPlatformInputPolicyTests::macSource_legacyRightCommandNormalizesToLeft()
{
  const CrossPlatformInputPolicy policy(
      capabilities(InputPlatform::MacOS), capabilities(InputPlatform::Linux, kInputCapabilityProtocolV19), {}
  );

  const auto wire = policy.toWireKey(kKeySuper_R, KeyModifierSuper);
  QCOMPARE(wire.id, kKeySuper_L);
  QCOMPARE(wire.mask, KeyModifierSuper);
}

void CrossPlatformInputPolicyTests::scrollPolicy_requiresCrossPlatformAndCapabilities()
{
  CrossPlatformInputOptions options{.invertScrollX = true, .invertScrollY = true};

  const CrossPlatformInputPolicy same(
      capabilities(InputPlatform::MacOS), capabilities(InputPlatform::MacOS, kInputCapabilityCrossOSScroll), options
  );
  const auto sameDelta = same.toDestinationScroll({120, -120});
  QCOMPARE(sameDelta.x, 120);
  QCOMPARE(sameDelta.y, -120);

  const CrossPlatformInputPolicy missingCapability(
      capabilities(InputPlatform::MacOS), capabilities(InputPlatform::Windows, kInputCapabilityProtocolV19), options
  );
  const auto missingDelta = missingCapability.toDestinationScroll({120, -120});
  QCOMPARE(missingDelta.x, 120);
  QCOMPARE(missingDelta.y, -120);

  const CrossPlatformInputPolicy cross(
      capabilities(InputPlatform::MacOS), capabilities(InputPlatform::Windows, kInputCapabilityCrossOSScroll), options
  );
  const auto crossDelta = cross.toDestinationScroll({120, -120});
  QCOMPARE(crossDelta.x, -120);
  QCOMPARE(crossDelta.y, 120);
}

QTEST_MAIN(CrossPlatformInputPolicyTests)
