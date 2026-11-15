/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/Coordinate.h"
#include "common/InputCapabilities.h"
#include "deskflow/KeyTypes.h"
#include "deskflow/MouseTypes.h"
#include "deskflow/OptionTypes.h"

#include <cstdint>

struct CrossPlatformInputOptions
{
  RightCommandAction rightCommand = RightCommandAction::Preserve;
  bool invertScrollX = false;
  bool invertScrollY = false;
  ScrollInvertScope scrollInvertScope = ScrollInvertScope::All;
  bool semanticMedia = false;
};

struct InputKeyEvent
{
  KeyID id = kKeyNone;
  KeyModifierMask mask = 0;
};

class CrossPlatformInputPolicy
{
public:
  CrossPlatformInputPolicy(InputCapabilities source, InputCapabilities destination, CrossPlatformInputOptions options);

  InputKeyEvent toWireKey(KeyID id, KeyModifierMask mask) const;
  InputKeyEvent toDestinationKey(KeyID id, KeyModifierMask mask) const;
  bool shouldSuppressRepeat(KeyID id) const;
  bool mapsRightCommandToHangul() const;

  //! Scroll inversion applied by the source computer before it writes the wheel packet
  /*!
  Exactly one side applies the configured inversion.  With \c ScrollInvertScope::All the
  destination does (see toDestinationScroll()) and this returns \p delta unchanged.  With
  \c ScrollInvertScope::MouseWheelOnly the source does, and only for events it classified as
  \c ScrollSource::Mouse: trackpad and unknown events are never inverted.
  */
  ScrollDelta toWireScroll(ScrollDelta delta, ScrollSource source) const;

  //! Scroll inversion applied by the destination computer to a received wheel packet
  /*!
  The destination never learns which device produced a packet, so under
  \c ScrollInvertScope::MouseWheelOnly it fails closed and leaves every packet alone.
  */
  ScrollDelta toDestinationScroll(ScrollDelta delta) const;
  bool shouldSynthesizeSemanticMedia(KeyID id) const;

private:
  bool isCrossPlatform() const;
  bool canMapHangul() const;
  bool canMapScroll() const;
  bool canSynthesizeSemanticMedia() const;
  ScrollDelta invertConfiguredAxes(ScrollDelta delta) const;

  InputCapabilities m_source;
  InputCapabilities m_destination;
  CrossPlatformInputOptions m_options;
};
