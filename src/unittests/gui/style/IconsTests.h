/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class IconsTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void cleanup();

  void availableIcons_areTheBundledSubsetInThreeWeights();
  void coloredSvg_putsTheColourIn_andOnlyForAnIconThatExists();
  void iconPixmap_isRenderedAtTheDevicePixelRatio_inTheColourAsked();
  void iconPixmap_duotoneHasALayerOfLessOpacity();
  void iconPixmap_isKeptForTheNextTime();
  void iconPixmap_ofAnUnknownIcon_isNull();
  void iconColor_followsTheRoleAndTheStateOfTheIcon();
  void icon_isPaintedInTheColoursOfTheLookThatTheApplicationHasThen();
  void icon_inTheModeDisabled_isInTheDisabledColour();
  void icon_atTwoTimes_isTwiceTheSizeInPixels();
};
