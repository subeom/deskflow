/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "LookFollower.h"

#include "Appearance.h"

#include <QEvent>
#include <QRegularExpression>
#include <QWidget>

namespace deskflow::gui::style {

LookFollower::LookFollower(QWidget *widget, std::function<void()> apply)
    : QObject(widget),
      m_widget(widget),
      m_apply(std::move(apply))
{
  widget->installEventFilter(this);
  watch();
  m_apply();
}

void LookFollower::watch()
{
  QObject::disconnect(m_connection);
  m_connection = onLookChanged(this, [this] { m_apply(); });
}

bool LookFollower::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == m_widget) {
    switch (event->type()) {
    case QEvent::StyleChange:
      watch();
      m_apply();
      break;
    case QEvent::PaletteChange:
    case QEvent::ApplicationPaletteChange:
      m_apply();
      break;
    default:
      break;
    }
  }
  return false;
}

QString withoutMnemonic(QString text)
{
  // translations into Chinese, Japanese and Korean put the key in parentheses after the text: "기타(&M)"
  static const QRegularExpression parenthesised(QStringLiteral(R"(\s*\(&[A-Za-z0-9]\))"));
  text.remove(parenthesised);
  text.replace(QStringLiteral("&&"), QStringLiteral("\x01"));
  text.remove(QLatin1Char('&'));
  text.replace(QLatin1Char('\x01'), QLatin1Char('&'));
  return text;
}

} // namespace deskflow::gui::style
