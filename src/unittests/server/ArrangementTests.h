/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "arch/Arch.h"

#include <QTemporaryDir>
#include <QTest>

class ArrangementTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();

  void derive_layouts_data();
  void derive_layouts();
  void derive_theServerOfTheUserWithThreePcs();
  void derive_tolerance_andTheShortestLink();
  void derive_aNotchIsNoOverlap_andIsLinkedOnBothSides();
  void derive_aRowWithTwoExposedEdges_keepsTheOuterOneAndWarns();
  void derive_linksOfComputersThatAreNotValid_areLeftOut();

  void validation_data();
  void validation();
  void check_aComputerThatNothingTouches_isNotConnected();

  void snap_touchesAndAligns_data();
  void snap_touchesAndAligns();
  void snap_aComputerOnAnotherIsPushedOff();
  void snap_aGroupThatIsNotARectangle_snapsByAnyOfItsMonitors();
  void snap_whatCannotBeSnapped_isRefused();
  void snap_guides_saySnappedEdges();

  void grid_equalSizes_deriveTheLinksOfTheClassicGrid_data();
  void grid_equalSizes_deriveTheLinksOfTheClassicGrid();
  void grid_theRowOfTheUser_isKept();
  void grid_otherSizes_keepTheLinksAndPutThemWherePhysicsPutsThem();
  void grid_whatCannotBeKept_isSaid();
  void grid_computersThatTheGridDoesNotConnect_areBelow();

  void property_derivedLinks_areSymmetricPhysicalAndOnMonitors();
  void property_derivedLinks_areReadByTheCoreAsTheyAreMeant();
  void property_equalGrids_areTheClassicGrid();
  void property_aSnappedComputer_neverOverlaps();

  void windowsDisplayScale_data();
  void windowsDisplayScale();
  void formatInterval_data();
  void formatInterval();

private:
  Arch m_arch;
  QTemporaryDir m_settingsDir;
};
