/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

class CrossPlatformInputPolicyTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void samePlatform_preservesRightCommand();
  void unknownPlatform_preservesRightCommand();
  void disabledOption_preservesRightCommand();
  void missingHangulCapability_preservesRightCommand();
  void macToWindows_hangulMapsDestinationKeyAndSuppressesRepeat();
  void macSource_legacyRightCommandNormalizesToLeft();
  void scrollPolicy_requiresCrossPlatformAndCapabilities();
  void scrollPolicy_preservesNonMacWindowsPairs();
  void scrollPolicy_invertsOnlySelectedAxesAndBoundaries();
};
