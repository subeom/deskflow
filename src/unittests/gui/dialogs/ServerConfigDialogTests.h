/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>
#include <QTemporaryDir>

class ServerConfigDialogTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void cleanup();
  void clipboardTextOnly_isOffByDefault();
  void clipboardTextOnly_isOnlyAvailableWhileClipboardsAreShared();
  void clipboardTextOnly_replacesTheSizeLimit();
  void clipboardTextOnly_isLoadedFromTheSettings();
  void clipboardTextOnly_isSavedToTheSettings();
  void clipboardTextOnly_isTurnedOffByRestoringDefaults();
  void clipboardTextOnly_makesTheDialogModified();
  void clipboardTextOnly_explainsWhatIsNotShared();

private:
  QTemporaryDir m_settingsDir;
};
