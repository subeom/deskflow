/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QWidget>

class QHBoxLayout;
class QLabel;

/*!
 * \brief The title of a group of settings: a line of text one size above the body text, and at its end a widget when
 * there is one (a StatusChip, a button). A screen reader finds it as a heading. The mnemonic of a title taken from a
 * group box ("&Misc") is left out: a heading has no key.
 */
class SectionHeader : public QWidget
{
  Q_OBJECT

public:
  explicit SectionHeader(const QString &title, QWidget *parent = nullptr);

  QString title() const;
  void setTitle(const QString &title);

  //! The widget at the end of the line; the header owns it. One that was there is deleted.
  void setTrailingWidget(QWidget *widget);
  QWidget *trailingWidget() const;

private:
  void applyLook();

  QLabel *m_title = nullptr;
  QHBoxLayout *m_layout = nullptr;
  QWidget *m_trailing = nullptr;
};
