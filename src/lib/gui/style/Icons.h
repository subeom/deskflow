/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "DesignTokens.h"

#include <QByteArray>
#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QSize>
#include <QString>
#include <QStringList>

namespace deskflow::gui::style {

/*!
 * \brief The icons of the new look are the Phosphor icons (MIT, see icons/phosphor/LICENSE): a subset that is in the
 * resources of the library, in three weights. Navigation and states are duotone, the actions in a line of text regular.
 *
 * An icon is an SVG with the colour "currentColor": the loader puts the colour in, renders it at the size and the
 * device pixel ratio that is asked for (so that it is as sharp on a screen of 200 % as on one of 100 %) and keeps the
 * picture.
 */
enum class IconWeight
{
  Regular,
  Duotone,
  Fill
};

// The colours that an icon can have: roles of the tokens, so that they follow the look (dark, high contrast)
enum class IconColor
{
  Text,
  TextSecondary,
  Accent,
  OnAccent,
  Success,
  Warning,
  Danger
};

QStringList availableIcons(IconWeight weight = IconWeight::Regular);
bool hasIcon(const QString &name, IconWeight weight = IconWeight::Regular);

// The SVG of the icon with the colour put in; empty when there is no such icon
QByteArray coloredSvg(const QString &name, IconWeight weight, const QColor &color);

// The key under which the picture of an icon is kept
QString iconCacheKey(const QString &name, IconWeight weight, const QColor &color, const QSize &size, qreal dpr);

// The picture of the icon (square, as large as the size allows), kept for the next time; null when there is no such
// icon
QPixmap iconPixmap(const QString &name, IconWeight weight, const QColor &color, const QSize &size, qreal dpr = 1.0);

// The colour of a role for a state of the icon (disabled, selected)
QColor iconColor(IconColor role, QIcon::Mode mode, const Tokens &tokens);

/*!
 * An icon that is drawn in the colour of its role in the look that the application has when it is painted: dark mode
 * and the look of the OS need no new icon.
 */
QIcon icon(const QString &name, IconColor color = IconColor::Text, IconWeight weight = IconWeight::Regular);

} // namespace deskflow::gui::style
