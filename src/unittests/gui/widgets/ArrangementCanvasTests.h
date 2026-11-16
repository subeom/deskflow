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
  void resize_anEdgeSnapsToTheNeighbour_theRatiosAndTheReportedSize();
  void resize_aComputerThatIsZoomed_isSizedInItsOwnUnits();
  void resize_theServerAndAComputerOfSeveralMonitors_arePutAndNotResized();
  void resize_overAnotherComputer_isNotDone();
  void drag_aBackgroundDrag_movesTheViewNotTheComputers();
  void keys_moveTheSelectedComputer();
  void doubleClick_asksToEditTheComputer();
  void selection_followsTheClicks();
  void issues_areFoundAndTold();
  void render_showsTheComputers_andCanBeSaved();
  void screenshots_areMadeOfTheStatesTheUserSees();
};
