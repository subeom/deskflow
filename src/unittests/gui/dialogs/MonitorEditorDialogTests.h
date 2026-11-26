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
  void controls_otherSize_staysChosenUntilTheSizeOrTheSelectionChanges();
  void problems_disableOk_andAreSaid();
  void numbers_areInStepWithTheDrawing_andCanBeUsed();
  void keys_rotateDeleteAndNudge();
  void keys_deletingTheLastOne_leavesASelectionThatIsThere();
  void reported_theMonitorsOfTheComputer_areShownAndCannotBeChanged();
  void reported_drawingYourOwn_enablesTheEditing();
  void screenshots_areMadeOfTheEditor();

  // phase 5: the editor in the new look (Appearance=Deskflow); with the system look it is what it was
  void newLook_hasTheBarOfActionsAndTheNumbersClosedUnderADisclosure();
  void newLook_theDisclosureOpensWithTheMouseAndTheKeys();
  void newLook_tabGoesThroughTheMonitors_enterAndSpaceSelect();
  void newLook_monitorsAreNeutral_andTheSelectedOneAndWhatIsWrongHaveColour();
  void newLook_aScreenReaderGetsAListOfTheMonitors();
  void systemLook_isTheEditorItWas();

private:
  QTemporaryDir m_settingsDir;
};
