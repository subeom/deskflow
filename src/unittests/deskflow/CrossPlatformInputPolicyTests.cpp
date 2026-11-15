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

void CrossPlatformInputPolicyTests::scrollScope_matrix_data()
{
  QTest::addColumn<int>("sourcePlatform");
  QTest::addColumn<int>("destinationPlatform");
  QTest::addColumn<uint32_t>("destinationCapabilities");
  QTest::addColumn<int>("scope");
  QTest::addColumn<bool>("invertX");
  QTest::addColumn<bool>("invertY");
  QTest::addColumn<int>("source");
  QTest::addColumn<int>("wireX");
  QTest::addColumn<int>("wireY");
  QTest::addColumn<int>("destinationX");
  QTest::addColumn<int>("destinationY");

  constexpr auto mac = static_cast<int>(InputPlatform::MacOS);
  constexpr auto windows = static_cast<int>(InputPlatform::Windows);
  constexpr auto linuxPlatform = static_cast<int>(InputPlatform::Linux);
  constexpr auto unknown = static_cast<int>(InputPlatform::Unknown);
  constexpr uint32_t eligible = kInputCapabilityProtocolV19 | kInputCapabilityCrossOSScroll;
  constexpr uint32_t noScroll = kInputCapabilityProtocolV19;
  constexpr auto all = static_cast<int>(ScrollInvertScope::All);
  constexpr auto wheelOnly = static_cast<int>(ScrollInvertScope::MouseWheelOnly);
  constexpr auto mouse = static_cast<int>(ScrollSource::Mouse);
  constexpr auto trackpad = static_cast<int>(ScrollSource::Trackpad);
  constexpr auto unknownSource = static_cast<int>(ScrollSource::Unknown);

  // the input delta is always {120, -120}; "wire" is what the source writes, "destination" is what the
  // destination makes of a packet that carries the unchanged input
  const auto row = [&](const char *name, int src, int dst, uint32_t caps, int scope, bool x, bool y, int device, int wx,
                       int wy, int dx, int dy) {
    QTest::newRow(name) << src << dst << caps << scope << x << y << device << wx << wy << dx << dy;
  };

  // scope All is the legacy behavior: the destination inverts every event whatever the device was
  row("all, mouse", mac, windows, eligible, all, true, true, mouse, 120, -120, -120, 120);
  row("all, trackpad", mac, windows, eligible, all, true, true, trackpad, 120, -120, -120, 120);
  row("all, unknown device", mac, windows, eligible, all, true, true, unknownSource, 120, -120, -120, 120);
  row("all, x only", mac, windows, eligible, all, true, false, mouse, 120, -120, -120, -120);
  row("all, no axes", mac, windows, eligible, all, false, false, mouse, 120, -120, 120, -120);

  // scope MouseWheelOnly: the source inverts mouse events only, the destination never inverts
  row("wheelOnly, mouse", mac, windows, eligible, wheelOnly, true, true, mouse, -120, 120, 120, -120);
  row("wheelOnly, trackpad", mac, windows, eligible, wheelOnly, true, true, trackpad, 120, -120, 120, -120);
  row("wheelOnly, unknown device", mac, windows, eligible, wheelOnly, true, true, unknownSource, 120, -120, 120, -120);
  row("wheelOnly, mouse, x only", mac, windows, eligible, wheelOnly, true, false, mouse, -120, -120, 120, -120);
  row("wheelOnly, mouse, y only", mac, windows, eligible, wheelOnly, false, true, mouse, 120, 120, 120, -120);
  row("wheelOnly, mouse, no axes", mac, windows, eligible, wheelOnly, false, false, mouse, 120, -120, 120, -120);

  // links the scroll mapping never applies to are left alone in every scope
  for (const auto scope : {all, wheelOnly}) {
    const auto tag = scope == all ? "all" : "wheelOnly";
    const auto name = [&](const char *what) { return QStringLiteral("%1, %2").arg(tag, what).toLatin1(); };
    row(name("same OS").constData(), mac, mac, eligible, scope, true, true, mouse, 120, -120, 120, -120);
    row(name("unknown source OS").constData(), unknown, windows, eligible, scope, true, true, mouse, 120, -120, 120,
        -120);
    row(name("unknown destination OS").constData(), mac, unknown, eligible, scope, true, true, mouse, 120, -120, 120,
        -120);
    row(name("missing capability").constData(), mac, windows, noScroll, scope, true, true, mouse, 120, -120, 120, -120);
    row(name("mac to linux").constData(), mac, linuxPlatform, eligible, scope, true, true, mouse, 120, -120, 120, -120);
    row(name("windows to mac").constData(), windows, mac, eligible, scope, true, true, mouse, 120, -120, 120, -120);
    row(name("linux to windows").constData(), linuxPlatform, windows, eligible, scope, true, true, mouse, 120, -120,
        120, -120);
  }
}

void CrossPlatformInputPolicyTests::scrollScope_matrix()
{
  QFETCH(int, sourcePlatform);
  QFETCH(int, destinationPlatform);
  QFETCH(uint32_t, destinationCapabilities);
  QFETCH(int, scope);
  QFETCH(bool, invertX);
  QFETCH(bool, invertY);
  QFETCH(int, source);
  QFETCH(int, wireX);
  QFETCH(int, wireY);
  QFETCH(int, destinationX);
  QFETCH(int, destinationY);

  const CrossPlatformInputPolicy policy(
      capabilities(static_cast<InputPlatform>(sourcePlatform)),
      capabilities(static_cast<InputPlatform>(destinationPlatform), destinationCapabilities),
      {.invertScrollX = invertX, .invertScrollY = invertY, .scrollInvertScope = static_cast<ScrollInvertScope>(scope)}
  );

  const auto wire = policy.toWireScroll({120, -120}, static_cast<ScrollSource>(source));
  QCOMPARE(wire.x, wireX);
  QCOMPARE(wire.y, wireY);

  const auto destination = policy.toDestinationScroll({120, -120});
  QCOMPARE(destination.x, destinationX);
  QCOMPARE(destination.y, destinationY);
}

void CrossPlatformInputPolicyTests::scrollScope_defaultsToAll()
{
  const CrossPlatformInputOptions defaults;
  QCOMPARE(static_cast<int>(defaults.scrollInvertScope), static_cast<int>(ScrollInvertScope::All));

  // a policy built without a scope keeps today's destination-side inversion and never touches the wire
  const CrossPlatformInputPolicy policy(
      capabilities(InputPlatform::MacOS), capabilities(InputPlatform::Windows, kInputCapabilityCrossOSScroll),
      {.invertScrollX = true, .invertScrollY = true}
  );
  const auto wire = policy.toWireScroll({120, -120}, ScrollSource::Mouse);
  QCOMPARE(wire.x, 120);
  QCOMPARE(wire.y, -120);
  const auto destination = policy.toDestinationScroll({120, -120});
  QCOMPARE(destination.x, -120);
  QCOMPARE(destination.y, 120);
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
