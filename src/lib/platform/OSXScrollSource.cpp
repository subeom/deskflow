/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/OSXScrollSource.h"

ScrollSource OSXScrollSource::classifyPhases(int64_t scrollPhase, int64_t momentumPhase)
{
  return (scrollPhase != 0 || momentumPhase != 0) ? ScrollSource::Trackpad : ScrollSource::Mouse;
}

OSXScrollFields OSXScrollSource::readFields(CGEventRef event)
{
  OSXScrollFields fields;
  if (event == nullptr) {
    return fields;
  }
  fields.scrollPhase = CGEventGetIntegerValueField(event, kCGScrollWheelEventScrollPhase);
  fields.momentumPhase = CGEventGetIntegerValueField(event, kCGScrollWheelEventMomentumPhase);
  fields.continuous = CGEventGetIntegerValueField(event, kCGScrollWheelEventIsContinuous);
  fields.sourcePid = CGEventGetIntegerValueField(event, kCGEventSourceUnixProcessID);
  return fields;
}

ScrollSource OSXScrollSource::classifyEvent(CGEventRef event)
{
  if (event == nullptr) {
    return ScrollSource::Unknown;
  }
  const auto fields = readFields(event);
  return classifyPhases(fields.scrollPhase, fields.momentumPhase);
}

OSXScrollClassification OSXScrollSource::update(CGEventRef event)
{
  OSXScrollClassification result;
  result.fields = readFields(event);
  result.source =
      event == nullptr ? ScrollSource::Unknown : classifyPhases(result.fields.scrollPhase, result.fields.momentumPhase);
  result.changed = result.source != m_last;
  m_last = result.source;
  return result;
}
