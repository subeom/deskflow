/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ChromeGeometry.h"

#include <algorithm>

namespace deskflow::gui::chrome {

QRect closeRect(const ChromeMetrics &metrics)
{
  if (metrics.buttonWidth <= 0)
    return {};
  return QRect(metrics.windowSize.width() - metrics.buttonWidth, 0, metrics.buttonWidth, metrics.headerHeight);
}

QRect maximizeRect(const ChromeMetrics &metrics)
{
  if (metrics.buttonWidth <= 0)
    return {};
  return QRect(metrics.windowSize.width() - 2 * metrics.buttonWidth, 0, metrics.buttonWidth, metrics.headerHeight);
}

QRect minimizeRect(const ChromeMetrics &metrics)
{
  if (metrics.buttonWidth <= 0)
    return {};
  return QRect(metrics.windowSize.width() - 3 * metrics.buttonWidth, 0, metrics.buttonWidth, metrics.headerHeight);
}

Region hitTest(const ChromeMetrics &metrics, const QPoint &point, const QList<QRect> &interactive)
{
  const QRect window(QPoint(0, 0), metrics.windowSize);
  if (!window.contains(point))
    return Region::Client;

  const auto close = closeRect(metrics);
  const auto maximize = maximizeRect(metrics);
  const auto minimize = minimizeRect(metrics);
  const bool overAButton = close.contains(point) || maximize.contains(point) || minimize.contains(point);

  // the edges come first: a window is resized by them
  const int border = metrics.maximized ? 0 : qMax(0, metrics.resizeBorder);
  if (border > 0) {
    const bool left = point.x() < border;
    const bool right = point.x() >= window.width() - border;
    const bool top = point.y() < border;
    const bool bottom = point.y() >= window.height() - border;
    // the strip of the top edge does not take the top rows of a button, so that its whole height is the button
    const bool topOfAButton = top && !left && !right && overAButton;
    if (!topOfAButton) {
      if (top && left)
        return Region::TopLeft;
      if (top && right)
        return Region::TopRight;
      if (bottom && left)
        return Region::BottomLeft;
      if (bottom && right)
        return Region::BottomRight;
      if (left)
        return Region::Left;
      if (right)
        return Region::Right;
      if (top)
        return Region::Top;
      if (bottom)
        return Region::Bottom;
    }
  }

  if (point.y() < metrics.headerHeight) {
    if (close.contains(point))
      return Region::Close;
    if (maximize.contains(point))
      return Region::Maximize;
    if (minimize.contains(point))
      return Region::Minimize;
    const bool onAControl =
        std::any_of(interactive.cbegin(), interactive.cend(), [&](const QRect &rect) { return rect.contains(point); });
    return onAControl ? Region::Client : Region::Caption;
  }
  return Region::Client;
}

Qt::Edges edgesOf(Region region)
{
  switch (region) {
  case Region::Left:
    return Qt::LeftEdge;
  case Region::Right:
    return Qt::RightEdge;
  case Region::Top:
    return Qt::TopEdge;
  case Region::Bottom:
    return Qt::BottomEdge;
  case Region::TopLeft:
    return Qt::TopEdge | Qt::LeftEdge;
  case Region::TopRight:
    return Qt::TopEdge | Qt::RightEdge;
  case Region::BottomLeft:
    return Qt::BottomEdge | Qt::LeftEdge;
  case Region::BottomRight:
    return Qt::BottomEdge | Qt::RightEdge;
  default:
    break;
  }
  return {};
}

QMargins maximizedInsets(int frameThickness)
{
  if (frameThickness <= 0)
    return {};
  return QMargins(frameThickness, frameThickness, frameThickness, frameThickness);
}

QSize firstWindowSize(const QSize &wanted, const QSize &minimum, const QSize &available, const QSize &frame)
{
  // the room that is left at the edges of the screen, so that the window is not flush with the taskbar
  constexpr int kMargin = 16;
  // (a size that is null has -1 for its sides: it is no frame)
  const int width = qMin(wanted.width(), available.width() - qMax(0, frame.width()) - kMargin);
  const int height = qMin(wanted.height(), available.height() - qMax(0, frame.height()) - kMargin);
  return QSize(qMax(width, minimum.width()), qMax(height, minimum.height()));
}

TrackSizes trackSizes(const QSize &minimum, const QSize &maximum, qreal scale, const QSize &frame)
{
  // the side of a size that is not limited (what a widget has for a window that has no most size)
  constexpr int kNotLimited = 16777215;

  const auto scaled = [scale](int side) { return side > 0 && side < kNotLimited ? qRound(side * scale) : side; };
  const auto sizeOf = [&](int least, int most, int frameSide, int &leastOut, int &mostOut) {
    leastOut = scaled(least);
    mostOut = scaled(most);
    // a most size that is under the least size is the least size
    const int bound = qMax(mostOut, leastOut);
    if (leastOut > 0)
      leastOut += frameSide;
    mostOut = bound < kNotLimited ? bound + frameSide : 0;
  };

  const int frameWidth = qMax(0, frame.width());
  const int frameHeight = qMax(0, frame.height());
  TrackSizes sizes;
  int leastWidth = 0;
  int mostWidth = 0;
  int leastHeight = 0;
  int mostHeight = 0;
  sizeOf(minimum.width(), maximum.width(), frameWidth, leastWidth, mostWidth);
  sizeOf(minimum.height(), maximum.height(), frameHeight, leastHeight, mostHeight);
  sizes.minimum = QSize(qMax(0, leastWidth), qMax(0, leastHeight));
  sizes.maximum = QSize(mostWidth, mostHeight);
  return sizes;
}

} // namespace deskflow::gui::chrome
