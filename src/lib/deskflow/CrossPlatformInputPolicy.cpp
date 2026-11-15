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
  if (id == kKeyF18 && mapsF18ToHangul()) {
    return {kKeyHangul, mask};
  }
  return {id, mask};
}

bool CrossPlatformInputPolicy::shouldSuppressRepeat(KeyID id) const
{
  return (id == kKeyF18 && mapsF18ToHangul()) || shouldSynthesizeSemanticMedia(id);
}

bool CrossPlatformInputPolicy::mapsF18ToHangul() const
{
  return m_options.f18Action == F18Action::Hangul && isCrossPlatform() && m_source.platform == InputPlatform::MacOS &&
         m_destination.platform == InputPlatform::Windows && (m_destination.capabilities & kInputCapabilityHangul) != 0;
}

ScrollDelta CrossPlatformInputPolicy::toWireScroll(ScrollDelta delta, ScrollSource source) const
{
  if (m_options.scrollInvertScope != ScrollInvertScope::MouseWheelOnly || !canMapScroll() ||
      source != ScrollSource::Mouse) {
    return delta;
  }
  return invertConfiguredAxes(delta);
}

ScrollDelta CrossPlatformInputPolicy::toDestinationScroll(ScrollDelta delta) const
{
  // with MouseWheelOnly the source has already inverted what it recognized as a mouse wheel, and this side
  // cannot tell a mouse from a trackpad, so it must not invert anything
  if (!canMapScroll() || m_options.scrollInvertScope == ScrollInvertScope::MouseWheelOnly) {
    return delta;
  }
  return invertConfiguredAxes(delta);
}

ScrollDelta CrossPlatformInputPolicy::invertConfiguredAxes(ScrollDelta delta) const
{
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
