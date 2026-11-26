/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "LayoutMatcherTests.h"

#include "LayoutTestData.h"
#include "gui/layout/LayoutMatcher.h"

#include <algorithm>
#include <random>

using namespace layout_test;
using namespace deskflow::gui::layout;

namespace {

Candidate candidate(const DisplaySignature &signature, int64_t used, int64_t created, const QString &id)
{
  return {&signature, used, created, id};
}

} // namespace

void LayoutMatcherTests::equalSignatures_areExact()
{
  const auto a = sig(tShape());
  const auto b = sig({builtIn(964, 1440), ultrawide(0, 0, true)});
  QCOMPARE(similarity(a, b), 1.0);
  const std::vector<Candidate> candidates{candidate(b, 1, 1, "x")};
  const auto result = bestMatch(a, candidates);
  QCOMPARE(result.kind, MatchResult::Kind::Exact);
  QCOMPARE(result.index, 0);
  QCOMPARE(result.score, 1.0);
}

void LayoutMatcherTests::theSameDisplaysInAnotherArrangement_areSimilar()
{
  // the laptop screen is on the right of the ultrawide, not below it
  const auto remembered = sig(tShape());
  const auto current = sig({ultrawide(0, 0, true), builtIn(3440, 0, false)});
  QVERIFY(current.key() != remembered.key());
  QVERIFY(qFuzzyCompare(similarity(current, remembered), 1.0 * kPenaltyOtherArrangement));
  const std::vector<Candidate> candidates{candidate(remembered, 1, 1, "x")};
  QCOMPARE(bestMatch(current, candidates).kind, MatchResult::Kind::Similar);
}

void LayoutMatcherTests::aDifferentMainDisplay_costsAFactor()
{
  // nothing moved, but the laptop screen is the main display now: the arrangement is measured from another origin
  const auto remembered = sig(tShape());
  auto displays = tShape();
  displays[0].primary = false;
  displays[1].primary = true;
  const auto current = sig(displays);
  QVERIFY(qFuzzyCompare(similarity(current, remembered), kPenaltyOtherMain));

  // both at once: the display is somewhere else and it is the main one
  auto moved = displays;
  moved[1].x += 10;
  QVERIFY(qFuzzyCompare(similarity(sig(moved), remembered), kPenaltyOtherMain * kPenaltyOtherArrangement));

  // a situation that says no main display, against one that does
  auto none = tShape();
  none[0].primary = false;
  QVERIFY(qFuzzyCompare(similarity(sig(none), remembered), kPenaltyOtherMain));
}

void LayoutMatcherTests::aStandInOfTheSameKind_isSimilar()
{
  // another monitor of the same resolution and scale stands in for the ultrawide that was there
  const auto remembered = sig(tShape());
  const auto current = sig({otherUltrawide(0, 0, true), builtIn(964, 1440)});
  const double expected = (kCreditStandIn + kCreditSameDisplay) / 2.0;
  QVERIFY(qFuzzyCompare(similarity(current, remembered), expected));
  QVERIFY(expected >= kSimilarThreshold);
  const std::vector<Candidate> candidates{candidate(remembered, 1, 1, "x")};
  const auto result = bestMatch(current, candidates);
  QCOMPARE(result.kind, MatchResult::Kind::Similar);
  QVERIFY(qFuzzyCompare(result.score, expected));
}

void LayoutMatcherTests::aDifferentMonitorOfAnotherKind_isNotSimilar()
{
  const auto remembered = sig(tShape());
  // a 1920 x 1080 monitor in place of the ultrawide: only the laptop screen is alike, half of the situation
  const auto current = sig({fullHd(0, 0, true), builtIn(964, 1440)});
  QVERIFY(qFuzzyCompare(similarity(current, remembered), 0.5 * 1.0));
  const std::vector<Candidate> candidates{candidate(remembered, 1, 1, "x")};
  const auto result = bestMatch(current, candidates);
  QCOMPARE(result.kind, MatchResult::Kind::None);
  QCOMPARE(result.index, -1);
}

void LayoutMatcherTests::theSameMonitorInAnotherMode_isNearlyTheSame()
{
  // the user chose another resolution for the ultrawide: it is the same piece of hardware
  const auto remembered = sig(tShape());
  auto displays = tShape();
  displays[0].w = 2560;
  displays[0].h = 1080;
  const auto current = sig(displays);
  const double expected = (kCreditSameDisplayOtherMode + kCreditSameDisplay) / 2.0;
  QVERIFY(qFuzzyCompare(similarity(current, remembered), expected));
}

void LayoutMatcherTests::aSubsetOrASupersetCostsWhatItLacks_data()
{
  QTest::addColumn<int>("current");
  QTest::addColumn<int>("remembered");
  QTest::addColumn<double>("score");
  QTest::addColumn<bool>("similar");

  // n displays of the same kind, each with a hardware id of its own, on a row
  QTest::newRow("1 of 2 (the laptop alone)") << 1 << 2 << 0.5 << false;
  QTest::newRow("2 of 3") << 2 << 3 << 2.0 / 3.0 << true;
  QTest::newRow("3 of 4") << 3 << 4 << 0.75 << true;
  QTest::newRow("3 of 5, the threshold") << 3 << 5 << 0.6 << true;
  QTest::newRow("2 of 5") << 2 << 5 << 0.4 << false;
  QTest::newRow("3 of 2, a superset") << 3 << 2 << 2.0 / 3.0 << true;
  QTest::newRow("2 of 1") << 2 << 1 << 0.5 << false;
}

namespace {

// n monitors in a row, each of its own hardware; they have the same kind, the main one is the first
std::vector<DisplayInfo> row(int n)
{
  std::vector<DisplayInfo> displays;
  for (int i = 0; i < n; ++i) {
    auto d = fullHd(i * 1920, 0, i == 0, QStringLiteral("hw-%1").arg(i));
    d.serial = 1000 + i;
    displays.push_back(d);
  }
  return displays;
}

} // namespace

void LayoutMatcherTests::aSubsetOrASupersetCostsWhatItLacks()
{
  QFETCH(int, current);
  QFETCH(int, remembered);
  QFETCH(double, score);
  QFETCH(bool, similar);

  const auto rememberedSignature = sig(row(remembered));
  const auto currentSignature = sig(row(current));
  QVERIFY2(qFuzzyCompare(similarity(currentSignature, rememberedSignature), score), "the score");
  const std::vector<Candidate> candidates{candidate(rememberedSignature, 1, 1, "x")};
  QCOMPARE(bestMatch(currentSignature, candidates).kind == MatchResult::Kind::Similar, similar);
}

void LayoutMatcherTests::theThresholdIsInclusive()
{
  // the score of 3 of 5 is 0.6, which is the threshold, and it is used
  const auto a = sig(row(3));
  const auto b = sig(row(5));
  const std::vector<Candidate> candidates{candidate(b, 1, 1, "x")};
  QVERIFY(similarity(a, b) >= kSimilarThreshold - 1e-12);
  QCOMPARE(bestMatch(a, candidates).kind, MatchResult::Kind::Similar);
}

void LayoutMatcherTests::anonymousDisplaysAreComparedByTheirKind()
{
  // two displays that tell nothing about themselves, of the same kind in the same place: the same
  const auto a = sig({anonymous(0, 0, 1920, 1080, true)});
  const auto b = sig({anonymous(0, 0, 1920, 1080, true)});
  QCOMPARE(similarity(a, b), 1.0);

  // one that tells and one that does not: a stand in
  const auto known = sig({fullHd(0, 0, true)});
  QVERIFY(qFuzzyCompare(similarity(a, known), kCreditStandIn));

  // of another resolution: nothing
  const auto other = sig({anonymous(0, 0, 1280, 720, true)});
  QCOMPARE(similarity(a, other), 0.0);
}

void LayoutMatcherTests::nothingIsNeverSimilar()
{
  const DisplaySignature none;
  const auto some = sig(tShape());
  QCOMPARE(similarity(none, some), 0.0);
  QCOMPARE(similarity(some, none), 0.0);
  QCOMPARE(similarity(none, none), 0.0);
  const std::vector<Candidate> candidates{candidate(some, 1, 1, "x")};
  QCOMPARE(bestMatch(none, candidates).kind, MatchResult::Kind::None);
  QCOMPARE(bestMatch(some, {}).kind, MatchResult::Kind::None);
  // a candidate that has no signature is skipped
  const std::vector<Candidate> broken{Candidate{nullptr, 1, 1, "y"}, candidate(some, 1, 1, "x")};
  const auto result = bestMatch(some, broken);
  QCOMPARE(result.index, 1);
}

void LayoutMatcherTests::bestMatch_takesTheExactOneEvenWhenASimilarOneIsNewer()
{
  const auto current = sig(tShape());
  const auto similar = sig({otherUltrawide(0, 0, true), builtIn(964, 1440)});
  const auto exact = sig(tShape());
  const std::vector<Candidate> candidates{candidate(similar, 9000, 9000, "newer"), candidate(exact, 1, 1, "older")};
  const auto result = bestMatch(current, candidates);
  QCOMPARE(result.kind, MatchResult::Kind::Exact);
  QCOMPARE(result.index, 1);
}

void LayoutMatcherTests::bestMatch_tiesGoToTheNewestUse_thenTheNewestMake_thenTheSmallestId()
{
  const auto current = sig({otherUltrawide(0, 0, true), builtIn(964, 1440)});
  // the same score for both: they have the same kind of display in the same places
  const auto a = sig(tShape());
  const auto b = sig(tShape());

  auto result = bestMatch(current, {candidate(a, 100, 5, "a"), candidate(b, 200, 1, "b")});
  QCOMPARE(result.index, 1);
  result = bestMatch(current, {candidate(a, 200, 1, "a"), candidate(b, 200, 5, "b")});
  QCOMPARE(result.index, 1);
  result = bestMatch(current, {candidate(a, 200, 5, "p-2"), candidate(b, 200, 5, "p-1")});
  QCOMPARE(result.index, 1);
  result = bestMatch(current, {candidate(b, 200, 5, "p-1"), candidate(a, 200, 5, "p-2")});
  QCOMPARE(result.index, 0);

  // a better score beats a newer use
  const auto better = sig({otherUltrawide(0, 0, true), builtIn(964, 1440)});
  const auto worse = sig({otherUltrawide(10, 0, true), builtIn(964, 1440)});
  result = bestMatch(current, {candidate(worse, 900, 900, "w"), candidate(better, 1, 1, "b")});
  QCOMPARE(result.kind, MatchResult::Kind::Exact);
  QCOMPARE(result.index, 1);
}

void LayoutMatcherTests::bestMatch_aScoreBelowTheThresholdIsNothing()
{
  const auto current = sig(laptopOnly());
  const auto tooFar = sig(tShape());
  const auto result = bestMatch(current, {candidate(tooFar, 1, 1, "x")});
  QCOMPARE(result.kind, MatchResult::Kind::None);
  QCOMPARE(result.index, -1);
  // the best score that was found is told, to explain why: half of the situation is alike, and it is not in the same
  // place, and it is not the main display there
  QVERIFY(qFuzzyCompare(result.score, 0.5 * kPenaltyOtherArrangement * kPenaltyOtherMain));
}

void LayoutMatcherTests::bestMatch_doesNotDependOnTheOrderOfTheCandidates()
{
  const auto current = sig({otherUltrawide(0, 0, true), builtIn(964, 1440)});
  std::vector<DisplaySignature> signatures;
  signatures.push_back(sig(tShape()));
  signatures.push_back(sig({ultrawide(0, 0, true), builtIn(3440, 0)}));
  signatures.push_back(sig({fullHd(0, 0, true), builtIn(1920, 0)}));
  signatures.push_back(sig(tShape()));
  std::vector<Candidate> candidates;
  for (size_t i = 0; i < signatures.size(); ++i)
    candidates.push_back(candidate(signatures[i], int64_t(100 + i % 2), 7, QStringLiteral("p-%1").arg(i)));

  const auto expected = bestMatch(current, candidates);
  QCOMPARE(expected.kind, MatchResult::Kind::Similar);
  const auto expectedId = candidates[static_cast<size_t>(expected.index)].id;

  std::mt19937 random(11);
  for (int i = 0; i < 30; ++i) {
    std::shuffle(candidates.begin(), candidates.end(), random);
    const auto result = bestMatch(current, candidates);
    QCOMPARE(candidates[static_cast<size_t>(result.index)].id, expectedId);
    QVERIFY(qFuzzyCompare(result.score, expected.score));
  }
}

namespace {

std::vector<DisplayInfo> randomDisplays(std::mt19937 &random)
{
  const std::vector<std::pair<int, int>> sizes{{1512, 982}, {3440, 1440}, {1920, 1080}, {2560, 1440}};
  const int count = 1 + int(random() % 4);
  std::vector<DisplayInfo> displays;
  int x = 0;
  for (int i = 0; i < count; ++i) {
    DisplayInfo d;
    const auto [w, h] = sizes[random() % sizes.size()];
    d.w = w;
    d.h = h;
    d.x = x;
    d.y = int(random() % 3) * 100;
    d.scalePercent = (random() % 2) ? 100 : 200;
    d.builtIn = (random() % 4) == 0;
    // a hardware id of its own most of the time, none sometimes
    if (random() % 4 != 0)
      d.uuid = QStringLiteral("hw-%1").arg(random() % 5);
    d.primary = i == 0;
    x += w;
    displays.push_back(d);
  }
  return displays;
}

} // namespace

void LayoutMatcherTests::property_scoreIsBetweenZeroAndOne_andOneOnlyForEqualKeys()
{
  std::mt19937 random(42);
  for (int i = 0; i < 400; ++i) {
    const auto a = sig(randomDisplays(random));
    const auto b = sig(randomDisplays(random));
    const auto score = similarity(a, b);
    QVERIFY2(score >= 0.0 && score <= 1.0, qPrintable(QStringLiteral("score %1").arg(score)));
    if (a.key() == b.key())
      QCOMPARE(score, 1.0);
    if (score >= 1.0 - 1e-12) {
      QVERIFY2(
          a.key() == b.key(), qPrintable(QStringLiteral("%1 | %2").arg(a.summary(), b.summary()))
      );
    }
  }
}

void LayoutMatcherTests::property_aSituationIsExactlyLikeItself_andTheMatchIsDeterministic()
{
  std::mt19937 random(99);
  for (int i = 0; i < 200; ++i) {
    const auto current = sig(randomDisplays(random));
    QCOMPARE(similarity(current, current), 1.0);

    std::vector<DisplaySignature> signatures;
    for (int j = 0; j < 6; ++j)
      signatures.push_back(sig(randomDisplays(random)));
    signatures.push_back(current);
    std::vector<Candidate> candidates;
    for (size_t j = 0; j < signatures.size(); ++j)
      candidates.push_back(candidate(signatures[j], int64_t(j), int64_t(j), QStringLiteral("p-%1").arg(j)));

    const auto first = bestMatch(current, candidates);
    QCOMPARE(first.kind, MatchResult::Kind::Exact);
    QCOMPARE(candidates[static_cast<size_t>(first.index)].signature->key(), current.key());
    const auto again = bestMatch(current, candidates);
    QCOMPARE(again.index, first.index);
    QCOMPARE(again.score, first.score);
  }
}

QTEST_MAIN(LayoutMatcherTests)
