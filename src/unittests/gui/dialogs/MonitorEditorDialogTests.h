/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>
#include <QTemporaryDir>

class MonitorEditorDialogTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();

  void status_saysWhetherTheDrawingMatchesWhatTheComputerReports();
  void status_aShapeThatDiffers_canBeStretched();
  void mouse_dragAMonitor_movesItWithSnapping_andUndoGoesBack();
  void mouse_dragAnEdge_resizesTheMonitor();
  void mouse_aRightClickInTheMiddleOfADrag_doesNotEndIt();
  void controls_resolutionScaleRotateAddRemoveAndMain();
  void controls_followTheSelectedMonitor();
  void problems_disableOk_andAreSaid();
  void numbers_areInStepWithTheDrawing_andCanBeUsed();
  void keys_rotateDeleteAndNudge();
  void screenshots_areMadeOfTheEditor();

private:
  QTemporaryDir m_settingsDir;
};
