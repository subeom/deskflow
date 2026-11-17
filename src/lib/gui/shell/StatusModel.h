/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/Enums.h"
#include "gui/widgets/StatusChip.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

namespace deskflow::gui {

//! A fact that is shown beside the state: "TLS Encryption Enabled", and what other sources add (see StatusModel)
struct StatusChipSpec
{
  QString key;
  QString text;
  StatusChip::Kind kind = StatusChip::Kind::Neutral;

  bool operator==(const StatusChipSpec &other) const = default;
};

//! What the status of the main window says, as the widgets show it
struct StatusView
{
  StatusChip::Kind kind = StatusChip::Kind::Neutral;
  //! The state in a few words ("Sharing with 2 computers")
  QString headline;
  //! The sentence behind it (the one of the status bar: who is connected, how long until the next retry)
  QString detail;
  //! The name of an icon of the Phosphor set
  QString icon;
  QList<StatusChipSpec> chips;

  bool operator==(const StatusView &other) const = default;
};

/*!
 * \brief The state of the program as the main window of the new look says it: one headline with a kind (so a colour
 * and an icon), one sentence, and chips.
 *
 * It does not know the core. The core process says its state to setCore() and setClients(), the main window brings the
 * sentence and the encryption of the status bar, and a failure to start comes through setError(). Any other source can
 * add to what is said without the core: it gives a chip of its own with setExternalChip(), by a key that is its own (a
 * second call with the key changes that chip, clearExternalChip() takes it away), and the widgets that listen to
 * changed() show it. The chips of the core ("security") come first, the chips of the others after them in the order
 * that they were first set.
 */
class StatusModel : public QObject
{
  Q_OBJECT

public:
  explicit StatusModel(QObject *parent = nullptr);

  //! The key of the chip of the encryption
  static QString securityKey();

  void setCore(core::ConnectionState connection, core::ProcessState process, bool isServer);
  void setClients(const QStringList &clients);
  void setDetail(const QString &detail);
  //! \p shown is whether there is an encryption to tell of (the core is connected or is listening)
  void setSecurity(bool shown, bool encrypted, const QString &text);
  //! The core could not start: the state is an error until the next state of the core
  void setError(const QString &headline, const QString &detail);
  void clearError();

  void setExternalChip(const QString &key, const QString &text, StatusChip::Kind kind);
  void clearExternalChip(const QString &key);

  const StatusView &view() const;

Q_SIGNALS:
  void changed();

private:
  void rebuild();

  core::ConnectionState m_connection = core::ConnectionState::Disconnected;
  core::ProcessState m_process = core::ProcessState::Stopped;
  bool m_isServer = true;
  QStringList m_clients;
  QString m_detail;
  bool m_securityShown = false;
  bool m_encrypted = false;
  QString m_securityText;
  bool m_hasError = false;
  QString m_errorHeadline;
  QString m_errorDetail;
  QList<StatusChipSpec> m_external;
  StatusView m_view;
};

} // namespace deskflow::gui
