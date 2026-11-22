/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>
#include <QTemporaryDir>

class InputIndicatorSettingsTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void cleanup();

  void defaultsAreSafeAndVisible();
  void dependencyControlsFollowTheToggle();

private:
  QTemporaryDir m_dir;
};
