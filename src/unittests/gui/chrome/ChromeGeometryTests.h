/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class ChromeGeometryTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void buttons_areAtTheRightEdgeOfTheHeader_closeLast();
  void buttons_areNotThereWhereTheSystemDrawsThem();
  void hitTest_says_whatIsAtAPoint_data();
  void hitTest_says_whatIsAtAPoint();
  void hitTest_ofAMaximizedWindow_hasNoEdges();
  void hitTest_ofAWindowThatIsNotResized_hasNoEdges();
  void hitTest_whereTheMenuButtonIs_isTheClient();
  void hitTest_everyPointOfTheWindow_isSomething_andTheEdgesDoNotCoverTheContent();
  void maximizedInsets_areTheThicknessOfTheFrameOnEverySide();
  void firstWindowSize_isTheWantedSize_whereTheScreenHasTheRoom_data();
  void firstWindowSize_isTheWantedSize_whereTheScreenHasTheRoom();
  void edgesOf_saysWhichEdgesARegionResizes();
};
