/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>
#include <QString>
#include <QTemporaryDir>

class DialogLayoutTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void cleanup();
  void computerSettingsDialog_isLaidOutWithoutSqueezing_data();
  void computerSettingsDialog_isLaidOutWithoutSqueezing();
  void clientConfigDialog_isLaidOutWithoutSqueezing_data();
  void clientConfigDialog_isLaidOutWithoutSqueezing();
  void computerSettingsDialog_monitorLayout_isLaidOutWithoutSqueezing_data();
  void computerSettingsDialog_monitorLayout_isLaidOutWithoutSqueezing();
  void serverConfigDialog_advancedTab_isLaidOutWithoutSqueezing_data();
  void serverConfigDialog_advancedTab_isLaidOutWithoutSqueezing();
  void monitorEditorDialog_isLaidOutWithoutSqueezing_data();
  void monitorEditorDialog_isLaidOutWithoutSqueezing();
  void serverConfigDialog_computersTab_withTheArrangement_isLaidOutWithoutSqueezing_data();
  void serverConfigDialog_computersTab_withTheArrangement_isLaidOutWithoutSqueezing();
  void connectionTestDialog_isLaidOutWithoutSqueezing_data();
  void connectionTestDialog_isLaidOutWithoutSqueezing();

private:
  QTemporaryDir m_settingsDir;
  QString m_originalStyle;
};
