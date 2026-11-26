/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class CanvasThemeTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void text_isReadableOnEveryFill_data();
  void text_isReadableOnEveryFill();
  void marks_areSeenAgainstWhatIsBehindThem_data();
  void marks_areSeenAgainstWhatIsBehindThem();
  void colour_meansFourThingsOnly_data();
  void colour_meansFourThingsOnly();
  void highContrast_hasNoDots_data();
  void highContrast_hasNoDots();
  void current_isTheThemeOfTheLookThatIsPaintedWith();
};
