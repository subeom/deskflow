/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/Coordinate.h"
#include "common/InputCapabilities.h"
#include "deskflow/KeyTypes.h"
#include "deskflow/OptionTypes.h"

#include <cstdint>

struct CrossPlatformInputOptions
{
  RightCommandAction rightCommand = RightCommandAction::Preserve;
  bool invertScrollX = false;
  bool invertScrollY = false;
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
  ScrollDelta toDestinationScroll(ScrollDelta delta) const;

private:
  bool isCrossPlatform() const;
  bool canMapHangul() const;

  InputCapabilities m_source;
  InputCapabilities m_destination;
  CrossPlatformInputOptions m_options;
};
