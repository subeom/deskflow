/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "server/MonitorGeometry.h"

#include <QJsonObject>
#include <QString>

#include <cstdint>
#include <optional>
#include <vector>

namespace deskflow::gui::layout {

//! One display of the server computer, as the platform layer describes it (experimental: layout profiles)
/*!
Pure data: no Qt widget, no platform call. The rectangle is in the units that the arrangement of the server uses (the
points of a Mac), with the origin at the top left of the main display, which is also what \c ServerConfig::serverMonitors
returns, so that the same numbers rebuild the arrangement of the server group.
*/
struct DisplayInfo
{
  //! A stable id from the operating system (the display UUID on macOS); empty when there is none
  QString uuid;
  //! The EDID based numbers, 0 when unknown
  uint32_t vendor = 0;
  uint32_t model = 0;
  uint32_t serial = 0;
  bool builtIn = false;
  bool primary = false;
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
  //! Pixels for each unit in percent (200 on a Retina display); 0 when the system does not say
  int scalePercent = 0;
  //! For people only (a list of remembered situations); never part of the identity or the key
  QString label;

  //! What says that two displays are the same piece of hardware, empty when nothing does (\c uuid, else the numbers)
  QString identity() const;
  //! Whether the kind of display is the same: built in or not, resolution and scale
  bool sameClassAs(const DisplayInfo &other) const;
  bool operator==(const DisplayInfo &) const = default;
};

//! The display situation of the server: which displays, how they are, and how they are arranged
/*!
The list is normalised when it is made, so that two lists of the same situation are the same signature whatever order
the system lists the displays in: displays that are not valid (no size) are left out, a mirrored display (the same
rectangle as another) is counted once (the main one, then the built in one, wins), the rest is sorted by position, size,
identity. At most \c kMaxDisplays are kept; the rest is dropped and \c truncated() says so.

\c key() is the canonical text that is compared (exact match); \c id() is a short non-reversible hash of it, which is
what the log shows, so that a serial number is never written to a log.
*/
class DisplaySignature
{
public:
  static constexpr int kMaxDisplays = 16;

  DisplaySignature() = default;
  static DisplaySignature fromDisplays(const std::vector<DisplayInfo> &displays);

  //! At least one display is known
  bool isValid() const
  {
    return !m_displays.empty();
  }
  const std::vector<DisplayInfo> &displays() const
  {
    return m_displays;
  }
  bool truncated() const
  {
    return m_truncated;
  }

  //! The canonical text: one \c identity|class|position|primary part for each display, joined by semicolons
  QString key() const;
  //! 8 hexadecimal characters of the SHA-256 of the key
  QString id() const;
  //! For people and the log, without any serial number or id: \c "built-in 1512x982@200% main + external 3440x1440@100%"
  QString summary() const;
  //! The displays as rectangles, the way \c ServerConfig::serverMonitors lists them
  std::vector<deskflow::server::MonitorRect> monitors() const;

  QJsonObject toJson() const;
  //! Only a signature that is valid and bounded; anything else is nothing
  static std::optional<DisplaySignature> fromJson(const QJsonObject &object);

  bool operator==(const DisplaySignature &other) const
  {
    return key() == other.key();
  }

private:
  std::vector<DisplayInfo> m_displays;
  bool m_truncated = false;
};

} // namespace deskflow::gui::layout
