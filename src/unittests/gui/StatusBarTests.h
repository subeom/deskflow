/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

class StatusBarTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void manualUpdate_usesExactReleaseUrl();
  void stagedUpdate_offersExplicitInstallAction();
  void updateError_neverClaimsAnUpdateIsStaged();
};
