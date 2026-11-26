/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>
#include <QStringList>

namespace deskflow::tunnel {

/**
 * Takes the secrets out of a line of text before it is shown or written anywhere. Everything that cloudflared prints
 * goes through it before the log window sees it. It knows the secrets it was given (in the forms they take in a header
 * dump: as they are, percent encoded and with the escapes of JSON) and the shapes of the credentials of Cloudflare
 * Access in a header, a cookie, an environment variable and a JWT, so a secret that it was not given is also masked
 * when it appears in one of those.
 */
class SecretRedactor
{
public:
  static constexpr auto kMask = "[redacted]";

  //! A value that is masked wherever it appears. Values shorter than 4 characters are ignored: they would damage
  //! ordinary text and no real token is that short.
  void addSecret(const QString &secret);
  void clear();

  QString redact(const QString &text) const;

  //! Only the shapes, without any registered secret
  static QString redactKnownShapes(const QString &text);

private:
  QStringList m_secrets; // longest first
};

} // namespace deskflow::tunnel
