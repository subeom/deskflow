/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/layout/DisplaySignature.h"

#include <QString>

#include <cstdint>
#include <vector>

namespace deskflow::gui::layout {

//! Credits of a pair of displays (current, remembered)
//! the same piece of hardware, the same resolution and scale
constexpr double kCreditSameDisplay = 1.00;
//! the same piece of hardware, another resolution or scale
constexpr double kCreditSameDisplayOtherMode = 0.90;
//! another display of the same kind (built in or not, resolution, scale)
constexpr double kCreditStandIn = 0.75;
//! The factor of a situation that has the displays in another arrangement, and of one whose main display is not the
//! remembered main display
constexpr double kPenaltyOtherArrangement = 0.95;
constexpr double kPenaltyOtherMain = 0.95;
//! A remembered situation is used when it is this similar
constexpr double kSimilarThreshold = 0.60;

//! What the matcher knows about a remembered situation
struct Candidate
{
  const DisplaySignature *signature = nullptr;
  //! Milliseconds since the epoch, for the tie breaks: the newest wins
  int64_t lastUsedAt = 0;
  int64_t createdAt = 0;
  QString id;
};

struct MatchResult
{
  enum class Kind
  {
    None,
    Similar,
    Exact
  };
  Kind kind = Kind::None;
  //! Index in the candidates, -1 for none
  int index = -1;
  //! 1 for an exact match; the score of a similar one; the best score that was found for none
  double score = 0.0;
};

//! How alike two display situations are, from 0 to 1; 1 only for equal signatures
/*!
The current displays are paired with the remembered ones one to one, best pair first (ties: the order of the lists), and
each pair has a credit (see the constants). The coverage is the sum of the credits over the number of displays of the
bigger situation, so a subset or a superset costs what it lacks; the result is the coverage times
\c kPenaltyOtherArrangement when a paired display is somewhere else, and times \c kPenaltyOtherMain when the main
display is not paired with the remembered main display.
*/
double similarity(const DisplaySignature &current, const DisplaySignature &remembered);

//! The remembered situation to use: the exact one if there is one, else the most similar one that reaches the threshold
/*!
Ties are decided by the score, then by the time it was used last (the newest), then by the time it was made (the
newest), then by the smaller id.
*/
MatchResult bestMatch(const DisplaySignature &current, const std::vector<Candidate> &candidates);

} // namespace deskflow::gui::layout
