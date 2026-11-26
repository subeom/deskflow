/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

// The log of the window: as it has always been with the look of the system, a card of the new look with DeskflowStyle
class LogDockTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void cleanup();

  void systemLook_isTheDockItHasAlwaysBeen();
  void deskflowLook_isACardWithItsHeaderAndItsLines();
  void deskflowLook_theButtonsHaveTheIconsAndTheNamesOfTheLook();
  void deskflowLook_detachingChangesTheButtonAndTheHeader();
  void deskflowLook_theCardIsPaintedAsASurface();
  void lines_areAppended_inBothLooks();
};
