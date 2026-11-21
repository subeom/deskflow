/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/UpdateInstaller.h"
#include "gui/UpdateVerifier.h"

#include <QStringList>

#include <memory>

namespace deskflow::gui {

class MacUpdateInstaller final : public IUpdateInstaller
{
public:
  MacUpdateInstaller(
      QString executablePath, QString cacheRoot, bool packageManaged,
      qint64 processId = 0, QString processStartIdentity = {},
      std::unique_ptr<IUpdateProcessLauncher> launcher = {},
      std::unique_ptr<IUpdateApplyOperations> operations = {},
      std::unique_ptr<IUpdateVerifier> verifier = {}
  );

  UpdateInstallResult preflight(const StagedUpdate &staged) override;
  UpdateInstallResult start(
      const StagedUpdate &staged, bool explicitConfirmation
  ) override;
  UpdateInstallResult apply(
      const QString &manifestPath, qint64 parentPid,
      const QString &parentStartIdentity, const QString &receiptPath,
      const QString &helperToken
  );

private:
  QString m_executablePath;
  QString m_cacheRoot;
  bool m_packageManaged = false;
  qint64 m_processId = 0;
  QString m_processStartIdentity;
  std::unique_ptr<IUpdateProcessLauncher> m_launcher;
  std::unique_ptr<IUpdateApplyOperations> m_operations;
  std::unique_ptr<IUpdateVerifier> m_verifier;
};

bool isTrustedUpdateBundleOwner(qint64 effectiveUid, qint64 owner);
bool isTrustedUpdateParentPermissions(
    qint64 effectiveUid, qint64 owner, const QString &groupName,
    bool groupWritable, bool worldWritable
);

} // namespace deskflow::gui
