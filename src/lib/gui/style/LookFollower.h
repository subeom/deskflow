/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QMetaObject>
#include <QObject>

#include <functional>

class QWidget;

namespace deskflow::gui::style {

/*!
 * \brief Calls a function when the look of the application changes: the colour scheme, the high contrast and the
 * reduced transparency or motion (onLookChanged), when the palette of the widget or of the application changes, and
 * when the widget gets another style (the signal of the look comes from the style, so it is connected again).
 *
 * The function runs once when this is made. It lives as long as the widget, which owns it.
 */
class LookFollower : public QObject
{
  Q_OBJECT

public:
  LookFollower(QWidget *widget, std::function<void()> apply);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void watch();

  QWidget *m_widget;
  std::function<void()> m_apply;
  QMetaObject::Connection m_connection;
};

//! The text of a label with its mnemonic taken out: "&Misc" is "Misc", "A && B" is "A & B", and "기타(&M)" is "기타"
QString withoutMnemonic(QString text);

} // namespace deskflow::gui::style
