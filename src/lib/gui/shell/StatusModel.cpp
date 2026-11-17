/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "StatusModel.h"

#include <algorithm>

namespace deskflow::gui {

using core::ConnectionState;
using core::ProcessState;
using Kind = StatusChip::Kind;

StatusModel::StatusModel(QObject *parent) : QObject(parent)
{
  rebuild();
}

QString StatusModel::securityKey()
{
  return QStringLiteral("security");
}

const StatusView &StatusModel::view() const
{
  return m_view;
}

void StatusModel::setCore(ConnectionState connection, ProcessState process, bool isServer)
{
  m_connection = connection;
  m_process = process;
  m_isServer = isServer;
  // a state that the core comes to is a new state: what failed before it is told no more
  m_hasError = false;
  rebuild();
}

void StatusModel::setClients(const QStringList &clients)
{
  if (m_clients == clients)
    return;
  m_clients = clients;
  rebuild();
}

void StatusModel::setDetail(const QString &detail)
{
  if (m_detail == detail)
    return;
  m_detail = detail;
  rebuild();
}

void StatusModel::setSecurity(bool shown, bool encrypted, const QString &text)
{
  if (m_securityShown == shown && m_encrypted == encrypted && m_securityText == text)
    return;
  m_securityShown = shown;
  m_encrypted = encrypted;
  m_securityText = text;
  rebuild();
}

void StatusModel::setError(const QString &headline, const QString &detail)
{
  m_hasError = true;
  m_errorHeadline = headline;
  m_errorDetail = detail;
  rebuild();
}

void StatusModel::clearError()
{
  if (!m_hasError)
    return;
  m_hasError = false;
  rebuild();
}

void StatusModel::setExternalChip(const QString &key, const QString &text, Kind kind)
{
  const StatusChipSpec spec{key, text, kind};
  const auto found = std::find_if(m_external.begin(), m_external.end(), [&](const auto &c) { return c.key == key; });
  if (found == m_external.end())
    m_external.append(spec);
  else if (*found == spec)
    return;
  else
    *found = spec;
  rebuild();
}

void StatusModel::clearExternalChip(const QString &key)
{
  const auto removed = m_external.removeIf([&](const auto &c) { return c.key == key; });
  if (removed > 0)
    rebuild();
}

void StatusModel::rebuild()
{
  StatusView view;
  view.detail = m_detail;

  if (m_hasError) {
    view.kind = Kind::Danger;
    view.headline = m_errorHeadline;
    view.detail = m_errorDetail;
    view.icon = QStringLiteral("x-circle");
  } else {
    switch (m_process) {
    case ProcessState::Starting:
      view.kind = Kind::Info;
      view.headline = tr("Starting…");
      view.icon = QStringLiteral("arrow-clockwise");
      break;
    case ProcessState::RetryPending:
      view.kind = Kind::Warning;
      view.headline = tr("Will try again");
      view.icon = QStringLiteral("arrow-clockwise");
      break;
    case ProcessState::Stopping:
      view.kind = Kind::Neutral;
      view.headline = tr("Stopping…");
      view.icon = QStringLiteral("power");
      break;
    case ProcessState::Stopped:
      view.kind = Kind::Neutral;
      view.headline = m_isServer ? tr("Not sharing") : tr("Not connected");
      view.icon = QStringLiteral("power");
      break;
    case ProcessState::Started:
      switch (m_connection) {
      case ConnectionState::Listening:
        view.kind = Kind::Info;
        view.headline = tr("Waiting for clients");
        view.icon = QStringLiteral("wifi-high");
        break;
      case ConnectionState::Connecting:
        view.kind = Kind::Info;
        view.headline = tr("Connecting…");
        view.icon = QStringLiteral("arrow-clockwise");
        break;
      case ConnectionState::Connected: {
        view.kind = Kind::Success;
        view.icon = QStringLiteral("plugs-connected");
        if (m_isServer) {
          const auto count = std::max<qsizetype>(m_clients.size(), 1);
          view.headline = count == 1 ? tr("Sharing with 1 computer") : tr("Sharing with %1 computers").arg(count);
        } else {
          view.headline = tr("Connected");
        }
        break;
      }
      case ConnectionState::Disconnected:
        view.kind = Kind::Warning;
        view.headline = tr("Disconnected");
        view.icon = QStringLiteral("warning");
        break;
      }
      break;
    }
  }

  if (m_securityShown && !m_hasError) {
    view.chips.append({securityKey(), m_securityText, m_encrypted ? Kind::Success : Kind::Warning});
  }
  view.chips.append(m_external);

  if (view == m_view)
    return;
  m_view = view;
  Q_EMIT changed();
}

} // namespace deskflow::gui
