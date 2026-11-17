/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class BackdropPanelTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void cleanup();

  void backdrop_isAGradientWithBlobs_andFlatWhenTransparencyIsReduced();
  void backdrop_isKeptBetweenPaints_andMadeAgainOnlyForOtherSizeOrLook();
  void backdrop_atTwoTimes_hasTwiceThePixels();

  void shadowPatch_fadesOutFromTheShapeAndIsKept();
  void panel_isTranslucentOverTheBackdrop_andDenseIsMoreOpaque_data();
  void panel_isTranslucentOverTheBackdrop_andDenseIsMoreOpaque();
  void panel_textKeepsItsContrastOnEveryPixelThatIsRendered_data();
  void panel_textKeepsItsContrastOnEveryPixelThatIsRendered();
  void panel_hasRoundedCornersAndAShadowOutsideItsBody();
  void panel_whenTransparencyIsReduced_isOpaqueAndHasNoShadow();
  void panel_followsTheLookWhenItChanges();
  void panel_withoutTheStyleLayer_isOpaqueInTheColoursOfTheOs();
  void panel_contentsMarginsAreTheShadowAndThePadding();
};
