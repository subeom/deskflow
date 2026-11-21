/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class UpdateVerifierTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void signatureEvidence_acceptsPinnedForkIdentity();
  void signatureEvidence_rejectsWrongBundleWrongLeafAdHocAndDeveloperId();
  void signatureEvidence_rejectsNotarizationOnlyAndMissingCore();
  void verifierApplication_checksVersionBundleAndNestedCore();
  void verifierApplication_boundsCommandFailures();
};
