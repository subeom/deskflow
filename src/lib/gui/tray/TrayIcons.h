/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "TrayState.h"

#include <QColor>
#include <QIcon>
#include <QImage>
#include <QString>

namespace deskflow::gui::tray {

/*!
 * What the mark of the icon is drawn on:
 *  - Template: macOS tints it (black and clear, the alpha is the shape), so it has no colour of its own
 *  - OnLightBar: a light taskbar or panel, a dark mark (Windows does not recolour, so it has an icon for each)
 *  - OnDarkBar: a dark taskbar or panel, a light mark
 */
enum class TrayTone
{
  Template,
  OnLightBar,
  OnDarkBar
};

/*!
 * One icon of the tray. The mark is the wave of the program (the symbolic mark that it has always had), and the state is
 * a shape that is added to it, so that an icon of one colour tells it: a slash through the mark (stopped), a ring (waiting),
 * a dot (connected), a triangle with an exclamation mark (an error). The mark alone is idle. An update is a dot at the upper
 * right of any of them. The shape is cut out of the mark around it, so that it reads at 16 pixels.
 */
struct TrayIconSpec
{
  TrayState state = TrayState::Idle;
  bool update = false;
  TrayTone tone = TrayTone::Template;
  //! The shapes take the colour of what they say (not on a template, which has none): green, blue, red. The shape is the
  //! same, so the colour is never the only thing that tells the state
  bool coloredBadge = false;

  bool operator==(const TrayIconSpec &other) const = default;
};

//! The picture of an icon as large as \p pixels (square), with a clear ground
QImage trayIconImage(const TrayIconSpec &spec, int pixels);

//! The icon, which draws itself at the size that it is asked for (the tray asks the pictures that it needs, each in its own
//! size and ratio of pixels)
QIcon trayIcon(const TrayIconSpec &spec);

/*!
 * The name of the icon in an icon theme: org.deskflow.deskflow[-stopped|-waiting|-connected|-error][-update]-symbolic.
 * The idle one is the name that the symbolic icon of the program has always had. A theme that has an icon of the name
 * wins over the picture that is drawn (a desktop that tints the symbolic icons by name needs it).
 */
QString trayIconName(const TrayIconSpec &spec);

//! The colour of the mark for a tone
QColor trayMarkColor(TrayTone tone);

//! The colour of the shape of a state when the badge is coloured (the mark colour when it is not), for a tone
QColor trayBadgeColor(const TrayIconSpec &spec);

//! The colour of the dot of an update when the badge is coloured (blue), else the colour of the mark
QColor trayUpdateColor(const TrayIconSpec &spec);

//! The tone that the OS asks for: the template on macOS, on Windows the theme of the taskbar, elsewhere the one of the palette
TrayTone detectTrayTone();

} // namespace deskflow::gui::tray
