/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2024 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ValidationError.h"

#include "gui/style/StatusColors.h"

#include <QLabel>
#include <QPalette>

namespace validators {

ValidationError::ValidationError(QObject *parent, QLabel *label) : QObject(parent), m_label(label)
{
  if (!m_label)
    return;

  m_label->clear();
  m_label->setContentsMargins(5, 3, 5, 3);

  // crimson with white text; in the look of the application the colour of danger with the text that reads on it
  const auto fill = deskflow::gui::style::errorFill();
  auto palette = m_label->palette();
  palette.setColor(QPalette::WindowText, fill.text);
  palette.setColor(QPalette::Window, fill.background);
  m_label->setPalette(palette);
}

const QString &ValidationError::message() const
{
  return m_message;
}

void ValidationError::setMessage(const QString &message)
{
  if (m_message == message)
    return;

  m_message = message;

  if (m_label) {
    m_label->setAutoFillBackground(!message.isEmpty());
    m_label->setText(message);
  }
  Q_EMIT messageChanged(m_message);
}

} // namespace validators
