/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/layout/LayoutMemory.h"

#include <QFrame>

#include <optional>

class QLabel;
class QPushButton;

//! One line that says what the remembering of layouts did, with Undo and a way to dismiss it (experimental)
/*!
Hidden while there is nothing to say. The text is one line that is cut to the width that there is, and is all in the tool
tip. It only shows: what Undo does is for whoever owns it to say.
*/
class LayoutNoticeBar : public QFrame
{
  Q_OBJECT

public:
  explicit LayoutNoticeBar(QWidget *parent = nullptr);

  //! Shows the notice, or hides the bar for none
  void setNotice(const std::optional<deskflow::gui::layout::Notice> &notice);
  //! Shows \p text as it is (a notice that is made up)
  void setText(const QString &text, bool canUndo);
  QString fullText() const
  {
    return m_text;
  }

Q_SIGNALS:
  void undoRequested();
  void dismissRequested();

protected:
  void resizeEvent(QResizeEvent *event) override;

private:
  void elide();

  QLabel *m_label = nullptr;
  QPushButton *m_undo = nullptr;
  QPushButton *m_dismiss = nullptr;
  QString m_text;
};
