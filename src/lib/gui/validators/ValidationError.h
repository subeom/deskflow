/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2024 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>
#include <QString>

class QLabel;

namespace validators {

class ValidationError : public QObject
{
  Q_OBJECT

public:
  explicit ValidationError(QObject *parent, QLabel *label = nullptr);
  const QString &message() const;
  void setMessage(const QString &message);

Q_SIGNALS:
  //! The message is not the one that it was (a form that shows it in another way follows it)
  void messageChanged(const QString &message);

private:
  void clear();
  QString m_message;
  QLabel *m_label = nullptr;
};

} // namespace validators
