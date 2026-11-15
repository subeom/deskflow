/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/CrossPlatformInputPolicy.h"

CrossPlatformInputPolicy::CrossPlatformInputPolicy(
    InputCapabilities source, InputCapabilities destination, CrossPlatformInputOptions options
)
    : m_source(source),
      m_destination(destination),
      m_options(options)
{
}

InputKeyEvent CrossPlatformInputPolicy::toWireKey(KeyID id, KeyModifierMask mask) const
{
  if (id == kKeySuper_R && m_source.platform == InputPlatform::MacOS && !mapsRightCommandToHangul()) {
    id = kKeySuper_L;
  }
  return {id, mask};
}

InputKeyEvent CrossPlatformInputPolicy::toDestinationKey(KeyID id, KeyModifierMask mask) const
{
  if (id == kKeySuper_R && m_source.platform == InputPlatform::MacOS) {
    if (mapsRightCommandToHangul()) {
      return {kKeyHangul, mask & ~KeyModifierSuper};
    }
    return {kKeySuper_L, mask};
  }
  return {id, mask};
}

bool CrossPlatformInputPolicy::shouldSuppressRepeat(KeyID id) const
{
  return (id == kKeySuper_R && mapsRightCommandToHangul()) || shouldSynthesizeSemanticMedia(id);
}

bool CrossPlatformInputPolicy::mapsRightCommandToHangul() const
{
  return canMapHangul();
}

ScrollDelta CrossPlatformInputPolicy::toDestinationScroll(ScrollDelta delta) const
{
  if (!canMapScroll()) {
    return delta;
  }
  if (m_options.invertScrollX) {
    delta.x = -delta.x;
  }
  if (m_options.invertScrollY) {
    delta.y = -delta.y;
  }
  return delta;
}

bool CrossPlatformInputPolicy::shouldSynthesizeSemanticMedia(KeyID id) const
{
  if (!canSynthesizeSemanticMedia()) {
    return false;
  }

  switch (id) {
  case kKeyAudioUp:
  case kKeyAudioDown:
  case kKeyAudioMute:
  case kKeyAudioPrev:
  case kKeyAudioNext:
  case kKeyAudioPlay:
  case kKeyAudioStop:
    return true;
  default:
    return false;
  }
}

bool CrossPlatformInputPolicy::isCrossPlatform() const
{
  return isKnownInputPlatform(m_source.platform) && isKnownInputPlatform(m_destination.platform) &&
         m_source.platform != m_destination.platform;
}

bool CrossPlatformInputPolicy::canMapHangul() const
{
  return m_options.rightCommand == RightCommandAction::Hangul && isCrossPlatform() &&
         m_source.platform == InputPlatform::MacOS && m_destination.platform == InputPlatform::Windows &&
         (m_destination.capabilities & kInputCapabilityHangul) != 0;
}

bool CrossPlatformInputPolicy::canMapScroll() const
{
  return isCrossPlatform() && m_source.platform == InputPlatform::MacOS &&
         m_destination.platform == InputPlatform::Windows &&
         (m_destination.capabilities & kInputCapabilityCrossOSScroll) != 0;
}

bool CrossPlatformInputPolicy::canSynthesizeSemanticMedia() const
{
  return m_options.semanticMedia && isCrossPlatform() && m_source.platform == InputPlatform::MacOS &&
         m_destination.platform == InputPlatform::Windows &&
         (m_destination.capabilities & kInputCapabilitySemanticMedia) != 0;
}
