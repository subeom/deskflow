/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "server/MonitorGeometry.h"

#include <QRect>
#include <QString>
#include <QVector>

#include <optional>
#include <vector>

namespace deskflow::gui {

//! A monitor that a computer reported, in the coordinates of the screen that it reports
struct ReportedMonitor
{
  QRect rect;
  //! The display scale in percent (100, 150, 200 ...); 0 when the computer did not say
  int scalePercent = 0;
  bool primary = false;
  //! The rectangle is in logical units (the points of a Mac): the scale is how sharp the monitor is, not how big
  bool logicalUnits = false;

  bool operator==(const ReportedMonitor &) const = default;
};

//! The monitors that a computer reported: valid ones, or none
struct ReportedMonitors
{
  QVector<ReportedMonitor> monitors;

  bool isEmpty() const
  {
    return monitors.isEmpty();
  }

  //! The monitors the way the log of the core and the settings write them: x,y,w,h[ scale=S][ primary][ logical];...
  QString toText() const;

  //! The monitors of \p text, if it is a list that a computer can report: 1 to 16 monitors that are valid together
  static std::optional<ReportedMonitors> fromText(const QString &text);

  std::vector<deskflow::server::MonitorRect> rects() const;

  //! The display scale of the primary monitor, or of the first monitor that has one; 0 when none says
  int primaryScalePercent() const;

  //! The coordinates are in logical units (a Mac), not in physical pixels (a Windows PC)
  bool logicalUnits() const;

  bool operator==(const ReportedMonitors &) const = default;
};

//! What a line of the log of the core says about the monitors of a computer
struct ReportedMonitorsLine
{
  QString computer;
  //! Empty when the line says that the computer reports no monitors (any more)
  ReportedMonitors monitors;
};

//! The monitors that a line of the log of the core says a computer reports, if it says (experimental)
/*!
The core logs \c computer "X" reports N monitors: x,y,w,h scale=S primary;x,y,w,h when a computer tells the monitors
that its desktop is made of, and \c computer "X" reports no monitors when it does not any more, while the experiment is
on. Only the whole line, as the core writes it, is taken, and only when its list is one that a computer can report.
*/
std::optional<ReportedMonitorsLine> parseReportedMonitors(const QString &logLine);

} // namespace deskflow::gui
