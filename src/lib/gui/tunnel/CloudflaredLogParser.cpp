/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/tunnel/CloudflaredLogParser.h"

#include <QRegularExpression>

namespace deskflow::tunnel {

namespace {

constexpr int kMaxLineLength = 4096;

QString cleaned(const QString &line)
{
  // the colours of a terminal, then every other control character
  static const QRegularExpression escapes(QStringLiteral("\x1b\\[[0-9;]*[A-Za-z]"));
  auto text = line.left(kMaxLineLength);
  text.remove(escapes);
  for (auto &character : text) {
    if (character.unicode() < 0x20 && character != QLatin1Char('\t')) {
      character = QLatin1Char(' ');
    }
  }
  return text.trimmed();
}

QString levelOf(const QString &text)
{
  // console format: `2026-11-17T04:00:00Z INF Start Websocket listener host=127.0.0.1:24800`
  static const QRegularExpression console(QStringLiteral("\\b(DBG|INF|WRN|ERR|FTL)\\b"));
  if (const auto match = console.match(text); match.hasMatch()) {
    return match.captured(1);
  }

  // JSON format: `{"level":"error",...}`
  static const QRegularExpression json(QStringLiteral("\"level\"\\s*:\\s*\"(debug|info|warn|error|fatal)\""));
  if (const auto match = json.match(text); match.hasMatch()) {
    const auto word = match.captured(1);
    if (word == QLatin1String("debug")) {
      return QStringLiteral("DBG");
    }
    if (word == QLatin1String("info")) {
      return QStringLiteral("INF");
    }
    if (word == QLatin1String("warn")) {
      return QStringLiteral("WRN");
    }
    if (word == QLatin1String("error")) {
      return QStringLiteral("ERR");
    }
    return QStringLiteral("FTL");
  }
  return {};
}

bool has(const QString &text, const char *needle)
{
  return text.contains(QLatin1String(needle), Qt::CaseInsensitive);
}

TunnelFailure whyTheConnectionFailed(const QString &text)
{
  // the handshake first: its message may name the address that was dialled as well
  if (has(text, "bad handshake")) {
    return TunnelFailure::HandshakeRejected;
  }
  if (has(text, "no such host") || has(text, "server misbehaving") || has(text, "name resolution") ||
      has(text, "dns")) {
    return TunnelFailure::HostUnresolvable;
  }
  if (has(text, "i/o timeout") || has(text, "timeout") || has(text, "deadline exceeded") ||
      has(text, "connection refused") || has(text, "network is unreachable") || has(text, "no route to host") ||
      has(text, "connection reset") || has(text, "no connection could be made") || has(text, "dial tcp") ||
      has(text, "tls:")) {
    return TunnelFailure::EdgeUnreachable;
  }
  return TunnelFailure::ConnectFailed;
}

} // namespace

LogEvent parseCloudflaredLine(const QString &line)
{
  LogEvent event;
  event.message = cleaned(line);
  event.level = levelOf(event.message);
  const auto &text = event.message;

  if (has(text, "a browser window should have opened") || has(text, "please open the following url and log in") ||
      has(text, "waiting for login")) {
    event.kind = LogEventKind::LoginRequired;
    event.problem = TunnelFailure::LoginRequired;
  } else if (has(text, "failed to start forwarding server")) {
    event.kind = LogEventKind::BindFailed;
    event.problem = TunnelFailure::PortBusy;
  } else if (has(text, "start websocket listener")) {
    event.kind = LogEventKind::ListenerStarted;
  } else if (has(text, "failed to connect to origin")) {
    event.kind = LogEventKind::OriginFailed;
    event.problem = whyTheConnectionFailed(text);
  }

  return event;
}

} // namespace deskflow::tunnel
