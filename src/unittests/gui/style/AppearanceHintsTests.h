/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class AppearanceHintsTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void look_theSchemeThatTheOsSays_decides();
  void look_withoutASchemeFromTheOs_thePaletteDecides_data();
  void look_withoutASchemeFromTheOs_thePaletteDecides();
  void look_carriesTheAccessibilityOptions();
  void readSystemHints_answersWithoutAskingForTheImpossible();
};
