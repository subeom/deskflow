/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTemporaryDir>
#include <QTest>

class ArrangementConfigTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void cleanup();

  // The server configuration that the classic grid makes. Recorded from the code before the arrangement was added;
  // with the experiment off, or no arrangement used, it has to stay byte for byte what it is.
  void classicGrid_generatesTheConfigItAlwaysDid_data();
  void classicGrid_generatesTheConfigItAlwaysDid();

private:
  QTemporaryDir m_settingsDir;
};
