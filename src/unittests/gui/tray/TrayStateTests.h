/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class TrayStateTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void state_isToldByTheStateOfTheCore_data();
  void state_isToldByTheStateOfTheCore();
  void startFailed_isAnErrorWhateverTheCoreSays();
  void names_areEachOneOfAKind();
};
