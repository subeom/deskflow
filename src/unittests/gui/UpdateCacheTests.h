/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

class UpdateCacheTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void stage_publishesVerifiedArtifactAndManifest();
  void stage_rejectsInvalidPayloadWithoutPublishing();
  void stage_deduplicatesExistingVerifiedArtifact();
  void stage_requiresVerifiedChecksumAsset();
  void load_reverifiesStagedManifestAndPayload();
  void load_rejectsTamperedManifestOrPayload();
};
