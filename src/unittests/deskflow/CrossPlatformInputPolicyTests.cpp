/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CrossPlatformInputPolicyTests.h"

#include "deskflow/CrossPlatformInputPolicy.h"

#include <QTest>

#include <array>
#include <limits>

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

void CrossPlatformInputPolicyTests::scrollPolicy_preservesNonMacWindowsPairs()
{
  CrossPlatformInputOptions options{.invertScrollX = true, .invertScrollY = true};
  const ScrollDelta input{120, -120};

  const CrossPlatformInputPolicy macToLinux(
      capabilities(InputPlatform::MacOS), capabilities(InputPlatform::Linux, kInputCapabilityCrossOSScroll), options
  );
  const auto linuxDelta = macToLinux.toDestinationScroll(input);
  QCOMPARE(linuxDelta.x, input.x);
  QCOMPARE(linuxDelta.y, input.y);

  const CrossPlatformInputPolicy windowsToMac(
      capabilities(InputPlatform::Windows), capabilities(InputPlatform::MacOS, kInputCapabilityCrossOSScroll), options
  );
  const auto macDelta = windowsToMac.toDestinationScroll(input);
  QCOMPARE(macDelta.x, input.x);
  QCOMPARE(macDelta.y, input.y);
}

void CrossPlatformInputPolicyTests::scrollPolicy_invertsOnlySelectedAxesAndBoundaries()
{
  const auto source = capabilities(InputPlatform::MacOS);
  const auto destination = capabilities(InputPlatform::Windows, kInputCapabilityCrossOSScroll);

  const CrossPlatformInputPolicy xOnly(source, destination, {.invertScrollX = true});
  const auto xDelta = xOnly.toDestinationScroll({120, -120});
  QCOMPARE(xDelta.x, -120);
  QCOMPARE(xDelta.y, -120);

  const CrossPlatformInputPolicy yOnly(source, destination, {.invertScrollY = true});
  const auto yDelta = yOnly.toDestinationScroll({120, -120});
  QCOMPARE(yDelta.x, 120);
  QCOMPARE(yDelta.y, 120);

  const CrossPlatformInputPolicy both(source, destination, {.invertScrollX = true, .invertScrollY = true});
  const auto zeroDelta = both.toDestinationScroll({0, 0});
  QCOMPARE(zeroDelta.x, 0);
  QCOMPARE(zeroDelta.y, 0);

  const auto edgeDelta =
      both.toDestinationScroll({std::numeric_limits<int16_t>::min(), std::numeric_limits<int16_t>::max()});
  QCOMPARE(edgeDelta.x, 32768);
  QCOMPARE(edgeDelta.y, -32767);
}

void CrossPlatformInputPolicyTests::semanticMediaPolicy_requiresMacWindowsCapabilityAndOption()
{
  const CrossPlatformInputOptions enabled{.semanticMedia = true};
  const InputCapabilities mac{InputPlatform::MacOS, kInputCapabilityProtocolV19};
  const InputCapabilities windows{InputPlatform::Windows, kInputCapabilitySemanticMedia};

  const CrossPlatformInputPolicy eligible(mac, windows, enabled);
  QVERIFY(eligible.shouldSynthesizeSemanticMedia(kKeyAudioPlay));
  QVERIFY(eligible.shouldSuppressRepeat(kKeyAudioPlay));

  const CrossPlatformInputPolicy same(mac, capabilities(InputPlatform::MacOS, kInputCapabilitySemanticMedia), enabled);
  QVERIFY(!same.shouldSynthesizeSemanticMedia(kKeyAudioPlay));

  const CrossPlatformInputPolicy disabled(mac, windows, {});
  QVERIFY(!disabled.shouldSynthesizeSemanticMedia(kKeyAudioPlay));

  const CrossPlatformInputPolicy missingCapability(
      mac, capabilities(InputPlatform::Windows, kInputCapabilityProtocolV19), enabled
  );
  QVERIFY(!missingCapability.shouldSynthesizeSemanticMedia(kKeyAudioPlay));

  const CrossPlatformInputPolicy unknown(capabilities(InputPlatform::Unknown), windows, enabled);
  QVERIFY(!unknown.shouldSynthesizeSemanticMedia(kKeyAudioPlay));
}

void CrossPlatformInputPolicyTests::semanticMediaPolicy_supportsOnlyApprovedIds()
{
  const CrossPlatformInputPolicy policy(
      capabilities(InputPlatform::MacOS), capabilities(InputPlatform::Windows, kInputCapabilitySemanticMedia),
      {.semanticMedia = true}
  );
  constexpr std::array supported = {kKeyAudioUp,   kKeyAudioDown, kKeyAudioMute, kKeyAudioPrev,
                                    kKeyAudioNext, kKeyAudioPlay, kKeyAudioStop};
  for (const auto key : supported) {
    QVERIFY(policy.shouldSynthesizeSemanticMedia(key));
  }

  constexpr std::array unsupported = {kKeyBrightnessUp, kKeyBrightnessDown, kKeyMissionControl,
                                      kKeyLaunchpad,    kKeyEject,          kKeyF1,
                                      kKeyF20};
  for (const auto key : unsupported) {
    QVERIFY(!policy.shouldSynthesizeSemanticMedia(key));
    QVERIFY(!policy.shouldSuppressRepeat(key));
  }
}

QTEST_MAIN(CrossPlatformInputPolicyTests)
