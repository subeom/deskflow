/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/tunnel/SecretRedactor.h"

#include <QRegularExpression>
#include <QUrl>

#include <algorithm>

namespace deskflow::tunnel {

namespace {

constexpr int kMinSecretLength = 4;

QString jsonEscaped(const QString &value)
{
  QString escaped;
  for (const auto character : value) {
    if (character == QLatin1Char('"') || character == QLatin1Char('\\')) {
      escaped.append(QLatin1Char('\\'));
    }
    escaped.append(character);
  }
  return escaped;
}

} // namespace

void SecretRedactor::addSecret(const QString &secret)
{
  if (secret.size() < kMinSecretLength) {
    return;
  }

  for (const auto &form : {secret, QString::fromLatin1(QUrl::toPercentEncoding(secret)), jsonEscaped(secret)}) {
    if (form.size() >= kMinSecretLength && !m_secrets.contains(form)) {
      m_secrets.append(form);
    }
  }
  std::ranges::sort(m_secrets, [](const QString &a, const QString &b) { return a.size() > b.size(); });
}

void SecretRedactor::clear()
{
  for (auto &secret : m_secrets) {
    std::ranges::fill(secret, QChar(u'\0'));
  }
  m_secrets.clear();
}

QString SecretRedactor::redact(const QString &text) const
{
  auto result = redactKnownShapes(text);
  for (const auto &secret : m_secrets) {
    result.replace(secret, QLatin1String(kMask));
  }
  return result;
}

QString SecretRedactor::redactKnownShapes(const QString &text)
{
  auto result = text;

  // a header, a JSON key or an environment variable: the value up to the end of the quote, the line or the item
  static const QRegularExpression named(
      QStringLiteral(
          "((?:cf-access-client-secret|cf-access-client-id|cf-access-token|cf_authorization|"
          "tunnel_service_token_secret|tunnel_service_token_id)[\"']?\\s*[:=]\\s*(?:\\[?\\s*[\"']?))"
          "([^\"'\\s,;\\]}]+)"
      ),
      QRegularExpression::CaseInsensitiveOption
  );
  result.replace(named, QStringLiteral("\\1%1").arg(QLatin1String(kMask)));

  // a JSON Web Token, which is what the Access cookie and the token header hold
  static const QRegularExpression jwt(QStringLiteral("eyJ[A-Za-z0-9_-]{5,}\\.[A-Za-z0-9_-]{5,}\\.[A-Za-z0-9_-]*"));
  result.replace(jwt, QLatin1String(kMask));

  return result;
}

} // namespace deskflow::tunnel
