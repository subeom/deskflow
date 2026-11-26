/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class StatusColorsTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void cleanup();

  void statusColor_withoutTheStyleLayer_isTheStyleSheetThatItAlwaysWas_data();
  void statusColor_withoutTheStyleLayer_isTheStyleSheetThatItAlwaysWas();
  void statusColor_withTheStyleLayer_isThePaletteOfTheLook_data();
  void statusColor_withTheStyleLayer_isThePaletteOfTheLook();
  void statusColor_afterTheStyleLayerAnother_leavesNoOldColour();
  void errorFill_withoutTheStyleLayer_isCrimsonAndWhite();
  void errorFill_withTheStyleLayer_hasTheContrastInBothLooks_data();
  void errorFill_withTheStyleLayer_hasTheContrastInBothLooks();

  void invalidLineEdit_withoutTheStyleLayer_hasTheCrimsonBorderThatItAlwaysHad();
  void invalidLineEdit_withTheStyleLayer_isMarkedForTheStyleAndHasNoStyleSheet();
  void validationError_withTheStyleLayer_isOnTheFillOfAnError();
};
