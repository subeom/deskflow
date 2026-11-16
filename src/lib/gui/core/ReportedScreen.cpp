/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ReportedScreen.h"

#include <QRegularExpression>

namespace deskflow::gui {

std::optional<ReportedScreen> parseReportedScreen(const QString &logLine)
{
  static const QRegularExpression pattern(
      QStringLiteral(R"re(computer "(.+)" reports its screen: (-?\d+),(-?\d+) (\d+)x(\d+)\s*$)re")
  );
  const auto match = pattern.match(logLine);
  if (!match.hasMatch())
    return std::nullopt;

  bool ok[4] = {false, false, false, false};
  const auto x = match.captured(2).toInt(&ok[0]);
  const auto y = match.captured(3).toInt(&ok[1]);
  const auto w = match.captured(4).toInt(&ok[2]);
  const auto h = match.captured(5).toInt(&ok[3]);
  if (!(ok[0] && ok[1] && ok[2] && ok[3]) || w <= 0 || h <= 0)
    return std::nullopt;
  return ReportedScreen{match.captured(1), QRect(x, y, w, h)};
}

} // namespace deskflow::gui
