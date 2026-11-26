/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class ArrangementCanvasTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void drag_aComputerSnapsToTheEdgeOfAnother();
  void drag_belowTheThreshold_isAClick();
  void drag_ontoAnotherComputer_isPushedOff();
  void drag_escapeCancelsIt();
  void drag_aRightClickInTheMiddleOfIt_doesNotEndIt();
  void assumedSize_isMarkedOnTheComputerThatHasIt();
  void resize_theEdgesOfTheOneRectangleOfAComputer_changeItsSize();
  void resize_anEdgeSnapsToTheNeighbour_theRatiosAndThePresets();
  void outline_aClient_hasResizeCursorsWithAToleranceInScreenPixels();
  void outline_pressSelectsTheComputer_andResizesIt();
  void outline_isHighlightedUnderThePointer();
  void outline_shiftKeepsTheAspect_altTakesTheSnappingAway();
  void outline_escapeCancelsADrag();
  void scale_aClientWithAReportedSize_isScaledAndNotResized();
  void scale_aComputerOfSeveralMonitors_isScaledAsAWhole();
  void scale_aComputerInTheNotchOfAnother_doesNotStopIt();
  void scale_neverOverAnotherComputer_andTheZoomStaysInItsLimits();
  void resize_aComputerThatIsZoomed_isSizedInItsOwnUnits();
  void resize_theServer_isPutAndNotResized();
  void resize_aPresetSnapOnOneEdge_doesNotMoveTheOtherOne();
  void resize_beyondWhatALayoutCanHave_isNeverStored();
  void resize_overAnotherComputer_isNotDone();
  void drag_aBackgroundDrag_movesTheViewNotTheComputers();
  void keys_moveTheSelectedComputer();
  void doubleClick_asksToEditTheComputer();
  void selection_followsTheClicks();
  void issues_areFoundAndTold();
  void render_showsTheComputers_andCanBeSaved();
  void screenshots_areMadeOfTheStatesTheUserSees();

  // phase 5: the keys, the colours and the tree of a screen reader in the new look (Appearance=Deskflow); with the system
  // look nothing of it is there
  void tab_inTheSystemLook_leavesTheCanvasAsItAlwaysDid();
  void tab_goesThroughTheComputersInTheOrderOfTheScreen_andLeavesAtTheEnds();
  void tab_comingIn_selectsTheFirstOrTheLast();
  void enterAndSpace_selectWhereTheKeysAre_andLeaveWhatIsSelectedToTheDialog();
  void arrows_moveTheSelectedComputerByAStep_inTheNewLookToo();
  void focus_isSeenOnTheSelectedComputer_andOnTheCanvasWhenNothingIsSelected();
  void paint_inTheSystemLook_isTheOldColours();
  void paint_colourMeansThisComputerConnectedSelectedAndError();
  void paint_followsTheDarkAndTheHighContrastLook_data();
  void paint_followsTheDarkAndTheHighContrastLook();
  void accessibility_isAListOfTheComputers_withNamesRolesAndStates();
  void accessibility_pressSelectsTheComputer();
  void accessibility_inTheSystemLook_isWhatItWas();
};
