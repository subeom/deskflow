/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class LayoutReattachTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void theUsersT_goesToTheLaptop_andTheComputersFollowTheirSides();
  void aServerThatKeepsItsDisplays_changesNothing();
  void aComputerThatNeverTouchedTheServer_staysWhereItIs();
  void computersThatTouchAMovedComputer_moveWithIt();
  void everySide_isPutAgainstTheSameSide_data();
  void everySide_isPutAgainstTheSameSide();
  void theFractionAlongTheSide_isKept();
  void aGroupThatCannotBePlaced_staysAndIsReported();
  void aZoomedComputer_isMovedByItsBox();
  void noServerOrNoDisplays_changesNothing();
  void theOrderOfTheComputers_doesNotMatter();
  void aBiggerScreen_keepsWhatStillTouches();
  void property_whatIsMovedTouchesTheServerAndOverlapsNothing();
};
