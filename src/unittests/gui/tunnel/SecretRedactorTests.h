/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QTest>

class SecretRedactorTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void aRegisteredSecret_isMaskedWhereverItAppears();
  void aRegisteredSecret_isMaskedInTheFormsOfAHeaderDump();
  void shortValues_areNotRegistered();
  void headers_ofCloudflareAccessAreMaskedWithoutKnowingTheValue_data();
  void headers_ofCloudflareAccessAreMaskedWithoutKnowingTheValue();
  void aJsonWebToken_isMasked();
  void ordinaryLines_areNotChanged_data();
  void ordinaryLines_areNotChanged();
  void clear_forgetsTheSecrets();
};
