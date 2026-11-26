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
  void f18_matrix_data();
  void f18_matrix();
  void f18_defaultsToPassThrough();
  void scrollPolicy_requiresCrossPlatformAndCapabilities();
  void scrollPolicy_preservesNonMacWindowsPairs();
  void scrollPolicy_invertsOnlySelectedAxesAndBoundaries();
  void scrollScope_matrix_data();
  void scrollScope_matrix();
  void scrollScope_defaultsToAll();
  void semanticMediaPolicy_requiresMacWindowsCapabilityAndOption();
  void semanticMediaPolicy_supportsOnlyApprovedIds();
  void crossOSOptionIds_areDistinct();
};
