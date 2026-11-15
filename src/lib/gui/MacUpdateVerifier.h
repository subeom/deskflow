/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/UpdateVerifier.h"

#include <QStringList>

#include <memory>

namespace deskflow::gui {

struct UpdateCommandResult
{
  bool started = false;
  bool timedOut = false;
  int exitCode = -1;
  QByteArray standardOutput;
  QByteArray standardError;
};

class IUpdateCommandRunner
{
public:
  virtual ~IUpdateCommandRunner() = default;

  virtual UpdateCommandResult run(const QString &program, const QStringList &arguments, int timeoutMs) = 0;
};

class MacUpdateVerifier final : public IUpdateVerifier
{
public:
  explicit MacUpdateVerifier(std::unique_ptr<IUpdateCommandRunner> runner = {});

  UpdateVerifierResult verify(const StagedUpdate &staged) override;
  UpdateVerifierResult verifyApplication(const QString &applicationPath, const QString &expectedVersion = QString());

private:
  UpdateVerifierResult
  verifyMounted(const QString &mountPoint, const QString &temporaryDirectory, const QString &expectedVersion);

  std::unique_ptr<IUpdateCommandRunner> m_runner;
};

std::unique_ptr<IUpdateCommandRunner> createUpdateCommandRunner();
bool updateCommandSucceeded(const UpdateCommandResult &result);
UpdateVerifierResult updateCommandFailure(const QString &description, const UpdateCommandResult &result);

UpdateVerifierResult
parseMacUpdateSignatureEvidence(const QByteArray &codesignEvidence, const QByteArray &gatekeeperEvidence);

} // namespace deskflow::gui
