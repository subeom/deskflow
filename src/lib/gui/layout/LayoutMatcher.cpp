/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/layout/LayoutMatcher.h"

#include <algorithm>
#include <cmath>

namespace deskflow::gui::layout {

namespace {

// two scores that differ by less than this are the same score
constexpr double kScoreEpsilon = 1e-9;

// what a remembered display is worth to a current one
double creditOf(const DisplayInfo &current, const DisplayInfo &remembered)
{
  const auto identity = current.identity();
  const bool sameClass = current.sameClassAs(remembered);
  if (identity == remembered.identity()) {
    // displays that say nothing about themselves are as alike as their kind: there is no other mode of them
    if (sameClass)
      return kCreditSameDisplay;
    return identity.isEmpty() ? 0.0 : kCreditSameDisplayOtherMode;
  }
  return sameClass ? kCreditStandIn : 0.0;
}

int mainDisplayOf(const std::vector<DisplayInfo> &displays)
{
  for (size_t i = 0; i < displays.size(); ++i) {
    if (displays[i].primary)
      return static_cast<int>(i);
  }
  return -1;
}

struct Pairing
{
  double credit = 0.0;
  bool samePlace = false;
  size_t current = 0;
  size_t remembered = 0;
};

} // namespace

double similarity(const DisplaySignature &current, const DisplaySignature &remembered)
{
  const auto &a = current.displays();
  const auto &b = remembered.displays();
  if (a.empty() || b.empty())
    return 0.0;
  if (current.key() == remembered.key())
    return 1.0;

  std::vector<Pairing> pairings;
  for (size_t i = 0; i < a.size(); ++i) {
    for (size_t j = 0; j < b.size(); ++j) {
      const double credit = creditOf(a[i], b[j]);
      if (credit > 0.0)
        pairings.push_back({credit, a[i].x == b[j].x && a[i].y == b[j].y, i, j});
    }
  }
  // the best pair first, a pair that is in the same place before one that is not, then the order of the lists
  std::ranges::sort(pairings, [](const Pairing &l, const Pairing &r) {
    if (l.credit != r.credit)
      return l.credit > r.credit;
    if (l.samePlace != r.samePlace)
      return l.samePlace;
    if (l.current != r.current)
      return l.current < r.current;
    return l.remembered < r.remembered;
  });

  std::vector<int> pairedWith(a.size(), -1);
  std::vector<bool> taken(b.size(), false);
  double sum = 0.0;
  bool elsewhere = false;
  for (const auto &pairing : pairings) {
    if (pairedWith[pairing.current] >= 0 || taken[pairing.remembered])
      continue;
    pairedWith[pairing.current] = static_cast<int>(pairing.remembered);
    taken[pairing.remembered] = true;
    sum += pairing.credit;
    elsewhere = elsewhere || !pairing.samePlace;
  }

  double score = sum / static_cast<double>(std::max(a.size(), b.size()));
  if (elsewhere)
    score *= kPenaltyOtherArrangement;

  // the main display is where the arrangement of the computers is measured from
  const int mainCurrent = mainDisplayOf(a);
  const int mainRemembered = mainDisplayOf(b);
  if ((mainCurrent < 0) != (mainRemembered < 0))
    score *= kPenaltyOtherMain;
  else if (mainCurrent >= 0 && pairedWith[static_cast<size_t>(mainCurrent)] >= 0 &&
           pairedWith[static_cast<size_t>(mainCurrent)] != mainRemembered)
    score *= kPenaltyOtherMain;

  return std::clamp(score, 0.0, 1.0);
}

MatchResult bestMatch(const DisplaySignature &current, const std::vector<Candidate> &candidates)
{
  MatchResult result;
  if (!current.isValid())
    return result;

  const Candidate *best = nullptr;
  double bestScore = 0.0;
  int bestIndex = -1;
  const auto better = [&](double score, const Candidate &candidate) {
    if (best == nullptr)
      return true;
    if (std::abs(score - bestScore) > kScoreEpsilon)
      return score > bestScore;
    if (candidate.lastUsedAt != best->lastUsedAt)
      return candidate.lastUsedAt > best->lastUsedAt;
    if (candidate.createdAt != best->createdAt)
      return candidate.createdAt > best->createdAt;
    return candidate.id < best->id;
  };

  for (size_t i = 0; i < candidates.size(); ++i) {
    const auto &candidate = candidates[i];
    if (candidate.signature == nullptr)
      continue;
    const double score = similarity(current, *candidate.signature);
    if (score > 0.0 && better(score, candidate)) {
      best = &candidate;
      bestScore = score;
      bestIndex = static_cast<int>(i);
    }
  }

  result.score = bestScore;
  if (best == nullptr)
    return result;
  if (best->signature->key() == current.key()) {
    result.kind = MatchResult::Kind::Exact;
    result.index = bestIndex;
    result.score = 1.0;
  } else if (bestScore + kScoreEpsilon >= kSimilarThreshold) {
    result.kind = MatchResult::Kind::Similar;
    result.index = bestIndex;
  }
  return result;
}

} // namespace deskflow::gui::layout
