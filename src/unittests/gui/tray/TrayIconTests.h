/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

// The icons of the tray: the state is a shape, so that an icon of one colour tells it, and it reads at the small sizes
class TrayIconTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void markIsThere_atEverySize();
  void template_isBlackAndClear();
  void everyStateHasAShapeOfItsOwn_atTheSmallSizes_data();
  void everyStateHasAShapeOfItsOwn_atTheSmallSizes();
  void shape_isCutOutOfTheMarkAroundIt();
  void colour_neverChangesTheShape();
  void update_isADotAtTheUpperRightOfAnyState();
  void tones_haveTheContrastThatTheirBarNeeds();
  void names_areTheSymbolicNamesOfTheTheme();
  void icon_drawsItselfAtTheSizeAndTheRatioItIsAskedFor();
};
