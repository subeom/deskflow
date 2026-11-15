/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

class UpdateVerifierTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void signatureEvidence_acceptsPinnedNotarizedDeveloperId();
  void signatureEvidence_rejectsWrongIdentityTeamOrAuthority();
  void signatureEvidence_rejectsUnnotarizedAssessment();
  void verifier_mountsCopiesChecksAndDetaches();
  void verifier_detachesAndRemovesCopyWhenEvidenceFails();
  void verifier_rejectsAnUnexpectedApplicationVersion();
  void platformVerifier_isAvailableOnlyOnMac();
};
