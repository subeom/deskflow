/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "OSXKeyGroupResolverTests.h"

#include "platform/OSXKeyGroupResolver.h"

#include <QTest>

namespace {

using Match = OSXKeyGroupResolver::Match;

const std::string kAbc = "com.apple.keylayout.ABC";
const std::string kDvorak = "com.apple.keylayout.Dvorak";
const std::string kKoreanMode = "com.apple.inputmethod.Korean.2SetKorean";
const std::string kKoreanMethod = "com.apple.inputmethod.Korean";
// the layout macOS reports as current while the Korean input mode is active
const std::string kHangulLayout = "com.apple.keylayout.2SetHangul";

std::vector<OSXKeyGroupResolver::Group> enabledAbcAndKorean()
{
  return {{kAbc, true}, {kKoreanMode, false}, {kKoreanMethod, false}};
}

OSXKeyGroupResolver::IdProvider ascii(const std::string &id)
{
  return [id] { return id; };
}

} // namespace

void OSXKeyGroupResolverTests::enabledLayoutResolvesExactlyWithoutAsciiLookup()
{
  OSXKeyGroupResolver resolver;
  resolver.setGroups({{kDvorak, true}, {kAbc, true}});

  bool asciiAsked = false;
  const auto result = resolver.resolve(kAbc, [&] {
    asciiAsked = true;
    return kDvorak;
  });

  QCOMPARE(result.group, 1);
  QVERIFY(result.match == Match::Exact);
  QVERIFY(!result.firstSighting);
  QVERIFY(!asciiAsked);
}

void OSXKeyGroupResolverTests::inputMethodLayoutResolvesThroughAsciiLayout()
{
  // Korean 2-Set is selected: the current layout is not enabled, but the
  // ASCII capable layout is, and it is not the first group
  OSXKeyGroupResolver resolver;
  resolver.setGroups({{kDvorak, true}, {kAbc, true}, {kKoreanMode, false}});

  const auto result = resolver.resolve(kHangulLayout, ascii(kAbc));

  QCOMPARE(result.group, 1);
  QVERIFY(result.match == Match::AsciiLayout);
  QVERIFY(result.firstSighting);
}

void OSXKeyGroupResolverTests::unresolvedFallbackSkipsGroupsWithoutKeyLayout()
{
  OSXKeyGroupResolver resolver;
  resolver.setGroups({{kKoreanMode, false}, {kAbc, true}});

  const auto result = resolver.resolve(kHangulLayout, ascii("com.apple.keylayout.Unknown"));

  QCOMPARE(result.group, 1);
  QVERIFY(result.match == Match::Fallback);
  QVERIFY(result.firstSighting);
}

void OSXKeyGroupResolverTests::unresolvedOutcomeIsReportedOncePerDistinctInput()
{
  OSXKeyGroupResolver resolver;
  resolver.setGroups(enabledAbcAndKorean());

  // every key press polls again: only the first one may be reported
  QVERIFY(resolver.resolve(kHangulLayout, ascii(kAbc)).firstSighting);
  for (int keyPress = 0; keyPress < 50; ++keyPress) {
    QVERIFY(!resolver.resolve(kHangulLayout, ascii(kAbc)).firstSighting);
  }

  // a different unknown layout is a new situation
  QVERIFY(resolver.resolve("com.apple.keylayout.Japanese", ascii(kAbc)).firstSighting);
  QVERIFY(!resolver.resolve("com.apple.keylayout.Japanese", ascii(kAbc)).firstSighting);

  // and so is the same layout once nothing resolves it any more
  QVERIFY(resolver.resolve(kHangulLayout, ascii("com.apple.keylayout.Unknown")).firstSighting);
  QVERIFY(!resolver.resolve(kHangulLayout, ascii("com.apple.keylayout.Unknown")).firstSighting);
}

void OSXKeyGroupResolverTests::setGroupsResetsReportedOutcomes()
{
  OSXKeyGroupResolver resolver;
  resolver.setGroups(enabledAbcAndKorean());
  QVERIFY(resolver.resolve(kHangulLayout, ascii(kAbc)).firstSighting);
  QVERIFY(!resolver.resolve(kHangulLayout, ascii(kAbc)).firstSighting);

  resolver.setGroups(enabledAbcAndKorean());

  QVERIFY(resolver.resolve(kHangulLayout, ascii(kAbc)).firstSighting);
}

void OSXKeyGroupResolverTests::unreadableLayoutKeepsPreviousGroup()
{
  OSXKeyGroupResolver resolver;
  resolver.setGroups({{kDvorak, true}, {kAbc, true}});
  QCOMPARE(resolver.resolve(kAbc, ascii(kAbc)).group, 1);

  // TIS returned nothing (null source): do not jump back to group 0
  const auto first = resolver.resolve("", ascii(""));
  QCOMPARE(first.group, 1);
  QVERIFY(first.match == Match::Previous);
  QVERIFY(first.firstSighting);

  const auto second = resolver.resolve("", ascii(""));
  QCOMPARE(second.group, 1);
  QVERIFY(!second.firstSighting);
}

void OSXKeyGroupResolverTests::unreadableLayoutWithoutHistoryFallsBack()
{
  OSXKeyGroupResolver resolver;
  resolver.setGroups({{kKoreanMode, false}, {kAbc, true}});

  const auto result = resolver.resolve("", ascii(""));

  QCOMPARE(result.group, 1);
  QVERIFY(result.match == Match::Fallback);
  QVERIFY(result.firstSighting);
}

void OSXKeyGroupResolverTests::noGroupsResolvesToFirstGroup()
{
  OSXKeyGroupResolver resolver;

  const auto result = resolver.resolve(kAbc, ascii(kAbc));

  QCOMPARE(result.group, 0);
  QVERIFY(result.match == Match::Fallback);
  QVERIFY(result.firstSighting);
}

QTEST_MAIN(OSXKeyGroupResolverTests)
