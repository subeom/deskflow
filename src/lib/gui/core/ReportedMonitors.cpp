/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ReportedMonitors.h"

#include <QRegularExpression>
#include <QStringList>

namespace deskflow::gui {

namespace {

constexpr int kMaxMonitors = 16;
constexpr int kShortMin = -32768;
constexpr int kShortMax = 32767;

bool inShort(int value)
{
  return value >= kShortMin && value <= kShortMax;
}

} // namespace

QString ReportedMonitors::toText() const
{
  QStringList items;
  for (const auto &monitor : monitors) {
    auto item = QStringLiteral("%1,%2,%3,%4")
                    .arg(monitor.rect.x())
                    .arg(monitor.rect.y())
                    .arg(monitor.rect.width())
                    .arg(monitor.rect.height());
    if (monitor.scalePercent > 0)
      item += QStringLiteral(" scale=%1").arg(monitor.scalePercent);
    if (monitor.primary)
      item += QStringLiteral(" primary");
    items << item;
  }
  return items.join(QLatin1Char(';'));
}

std::optional<ReportedMonitors> ReportedMonitors::fromText(const QString &text)
{
  if (text.trimmed().isEmpty())
    return std::nullopt;

  // x,y,w,h and, in this order, the scale and the primary monitor; nothing else is part of a monitor
  static const QRegularExpression item(
      QStringLiteral(R"re(^(-?\d{1,6}),(-?\d{1,6}),(\d{1,6}),(\d{1,6})(?: scale=(\d{1,5}))?( primary)?$)re")
  );

  ReportedMonitors reported;
  for (const auto &part : text.split(QLatin1Char(';'))) {
    const auto match = item.match(part.trimmed());
    if (!match.hasMatch() || reported.monitors.size() >= kMaxMonitors)
      return std::nullopt;

    const auto x = match.captured(1).toInt();
    const auto y = match.captured(2).toInt();
    const auto w = match.captured(3).toInt();
    const auto h = match.captured(4).toInt();
    const auto scale = match.captured(5).isEmpty() ? 0 : match.captured(5).toInt();
    if (!inShort(x) || !inShort(y) || w < 1 || h < 1 || w > kShortMax || h > kShortMax || scale > 65535)
      return std::nullopt;
    reported.monitors.push_back({QRect(x, y, w, h), scale, !match.captured(6).isEmpty()});
  }

  // monitors that a computer can have: they do not overlap and make one shape
  if (!deskflow::server::MonitorLayout::create(reported.rects()))
    return std::nullopt;
  return reported;
}

std::vector<deskflow::server::MonitorRect> ReportedMonitors::rects() const
{
  std::vector<deskflow::server::MonitorRect> rects;
  for (const auto &monitor : monitors)
    rects.push_back({monitor.rect.x(), monitor.rect.y(), monitor.rect.width(), monitor.rect.height()});
  return rects;
}

int ReportedMonitors::primaryScalePercent() const
{
  for (const auto &monitor : monitors) {
    if (monitor.primary && monitor.scalePercent > 0)
      return monitor.scalePercent;
  }
  for (const auto &monitor : monitors) {
    if (monitor.scalePercent > 0)
      return monitor.scalePercent;
  }
  return 0;
}

std::optional<ReportedMonitorsLine> parseReportedMonitors(const QString &logLine)
{
  // the whole line, as the core writes it: "[time] INFO: " and the text. A name that has a line end in it makes a line
  // of the log that begins with the text and not with the time and the level, and the core cleans the quotes out of
  // a name, so that a name cannot close itself.
  static const QRegularExpression some(
      QStringLiteral(R"re(^\[[^\]\r\n]+\] INFO: computer "([^"\r\n]+)" reports (\d{1,3}) monitors: ([^\r\n]*?)\s*$)re")
  );
  static const QRegularExpression none(
      QStringLiteral(R"re(^\[[^\]\r\n]+\] INFO: computer "([^"\r\n]+)" reports no monitors\s*$)re")
  );

  if (const auto match = none.match(logLine); match.hasMatch())
    return ReportedMonitorsLine{match.captured(1), {}};

  const auto match = some.match(logLine);
  if (!match.hasMatch())
    return std::nullopt;
  const auto count = match.captured(2).toInt();
  const auto monitors = ReportedMonitors::fromText(match.captured(3));
  if (!monitors || count < 1 || monitors->monitors.size() != count)
    return std::nullopt;
  return ReportedMonitorsLine{match.captured(1), *monitors};
}

} // namespace deskflow::gui
