/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "Contrast.h"

#include <algorithm>
#include <cmath>

namespace deskflow::gui::style {

namespace {

double linear(double channel)
{
  return channel <= 0.03928 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
}

} // namespace

double relativeLuminance(const QColor &color)
{
  return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
}

double contrastRatio(const QColor &first, const QColor &second)
{
  const auto a = relativeLuminance(first);
  const auto b = relativeLuminance(second);
  return (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
}

QColor over(const QColor &layer, qreal opacity, const QColor &under)
{
  const auto mix = [opacity](qreal top, qreal bottom) { return top * opacity + bottom * (1 - opacity); };
  return QColor::fromRgbF(
      mix(layer.redF(), under.redF()), mix(layer.greenF(), under.greenF()), mix(layer.blueF(), under.blueF())
  );
}

QColor ensureContrast(const QColor &foreground, const QColor &background, double minimum)
{
  if (contrastRatio(foreground, background) >= minimum)
    return foreground;

  // away from the background: darker on a light one, lighter on a dark one
  const auto towards = relativeLuminance(background) > 0.5 ? QColor(Qt::black) : QColor(Qt::white);
  QColor candidate = foreground;
  for (int step = 1; step <= 100; ++step) {
    candidate = over(towards, step / 100.0, foreground);
    if (contrastRatio(candidate, background) >= minimum)
      return candidate;
  }
  return towards;
}

} // namespace deskflow::gui::style
