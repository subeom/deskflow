/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class ToggleSwitchTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void cleanup();

  void withoutTheStyleLayer_isACheckBoxInEveryRespect();
  void withTheStyleLayer_hasTheSizeOfTheTrack_andOfTheTextBesideIt();
  void click_togglesAndSignalsLikeACheckBox();
  void click_hitsTheWholeSwitch_andSpaceTogglesToo();
  void disabled_isDimmedAndIgnoresClicks();
  void paint_showsTheStateInTheColoursOfTheTokens();
  void paint_followsTheLookWhenItChanges();
  void knob_movesInTheTimeOfAHover_andAtOnceWhenMotionIsReduced();
  void ring_showsForTheKeyboardOnly();
  void text_isDrawnBesideTheTrack();
};
