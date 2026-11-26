/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include <QTest>

class MonitorGeometryTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void layouts_data();
  void layouts();
  void invalidLayouts_data();
  void invalidLayouts();
  void duplicates_areOneMonitor();
  void entry_onAStaggeredDesktop_landsOnTheMonitorThatHasTheRow();
  void entry_onAnLShapedDesktop_landsInsideAMonitor();
  void entry_depth_staysOnTheMonitor();
  void move_againstAnInnerEdge_isBlockedAndSlides();
  void move_hasNoDeadTravel();
  void move_fromAHole_startsOnTheNearestMonitor();
  void move_overAMonitorBoundary_goesOn();
  void parse_data();
  void parse();
  void parse_errors_data();
  void parse_errors();
  void parse_aBlockToPaste_isTheSameAsAList();
  void parse_rejectsWhatIsNotAShapeThatCanBeReported();
  void parse_aHugeListIsRefusedAtOnce();
  void format_isWhatParseReads();
  void extractPasteBlock_data();
  void extractPasteBlock();
  void plainRectangle_isWhatDeskflowHasAlwaysDone_data();
  void plainRectangle_isWhatDeskflowHasAlwaysDone();
  void plainRectangle_ofSeveralMonitors_isWhatDeskflowHasAlwaysDone();
  void property_anEntryIsOnAMonitor();
  void property_anEntryIsOnTheEdge();
  void property_entryIsMonotone();
  void property_fractionAndLineMapToEachOtherExactly();
  void property_aRandomWalkNeverLeavesTheDesktop();
  void property_goingThereAndBackComesBackToTheSameRow();
};
