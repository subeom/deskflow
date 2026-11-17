/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/style/DeskflowStyle.h"

#include <QTest>

class DeskflowStyleTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void init();
  void cleanup();

  void polish_givesTheApplicationThePaletteOfTheLook_data();
  void polish_givesTheApplicationThePaletteOfTheLook();
  void lookChanged_isEmittedOnlyWhenTheLookIsOther();

  void pushButton_default_isACapsuleInTheAccentWithTheTextOfTheAccent_data();
  void pushButton_default_isACapsuleInTheAccentWithTheTextOfTheAccent();
  void pushButton_secondary_hasTheRadiusOfInputsAndTheBorderOfControls_data();
  void pushButton_secondary_hasTheRadiusOfInputsAndTheBorderOfControls();
  void pushButton_disabled_hasNoAccent();
  void pushButton_withKeyboardFocus_hasTheRingInsideItsEdge();

  void lineEdit_isRounded_andInvalidIsDangerColoured();
  void lineEdit_withFocus_hasTheAccentBorder();
  void comboBoxAndSpinBox_haveTheBorderOfInputs();

  void checkBox_isAToggle_inTheAccentWhenChecked_data();
  void checkBox_isAToggle_inTheAccentWhenChecked();
  void checkBox_inAnItemView_staysACheckBox();

  void sizes_neverShrinkBelowFusion_andButtonsAreAtLeastThirtyHigh();

private:
  deskflow::gui::style::DeskflowStyle *m_style = nullptr;
};
