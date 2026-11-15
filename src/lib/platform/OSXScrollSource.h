/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/MouseTypes.h"

#include <CoreGraphics/CoreGraphics.h>

#include <cstdint>

//! Raw CGEvent scroll fields behind a classification
struct OSXScrollFields
{
  int64_t scrollPhase = 0;
  int64_t momentumPhase = 0;
  // diagnostics only: neither of these takes part in the classification
  int64_t continuous = 0;
  int64_t sourcePid = 0;
};

//! Result of classifying one scroll event
struct OSXScrollClassification
{
  ScrollSource source = ScrollSource::Unknown;
  OSXScrollFields fields;
  //! true when \c source differs from the previous event's classification
  bool changed = false;
};

//! Tells a mouse wheel from a trackpad using the phases macOS attaches to scroll events
/*!
Rule: an event is \c ScrollSource::Trackpad iff it carries a scroll phase or a momentum
phase, otherwise it is \c ScrollSource::Mouse.

This is a heuristic.  It deliberately ignores \c kCGScrollWheelEventIsContinuous (mouse
driver software emits continuous pixel events that are not a trackpad) and the source
process id (an unverified assumption).  Known limits:
- A Magic Mouse reports phases and is therefore treated as a trackpad.
- Utilities that smooth or emulate scrolling are classified by the phases they emit, so
  one that fabricates phases is treated as a trackpad.
*/
class OSXScrollSource
{
public:
  //! Classify from the two phase fields; never returns \c ScrollSource::Unknown
  static ScrollSource classifyPhases(int64_t scrollPhase, int64_t momentumPhase);

  //! Read the raw scroll fields of \p event (all zero for a null event)
  static OSXScrollFields readFields(CGEventRef event);

  //! Classify \p event; a null event is \c ScrollSource::Unknown
  static ScrollSource classifyEvent(CGEventRef event);

  //! Classify \p event and report whether the classification changed since the last call
  /*!
  Meant to be called once per scroll event so callers can log the raw fields when the
  device changes without logging every event.  Not thread safe.
  */
  OSXScrollClassification update(CGEventRef event);

private:
  ScrollSource m_last = ScrollSource::Unknown;
};
