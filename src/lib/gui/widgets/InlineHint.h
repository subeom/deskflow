/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QLabel>

/*!
 * \brief A line of guidance under a title or a control: a wrapped text in the secondary colour, a little smaller than
 * the text around it. It says what a setting does in words, so that the jargon of its name needs no tooltip.
 */
class InlineHint : public QLabel
{
  Q_OBJECT

public:
  explicit InlineHint(QWidget *parent = nullptr);
  explicit InlineHint(const QString &text, QWidget *parent = nullptr);

  //! The line says what is wrong (a value that was not applied): it has the colour of an error
  void setDanger(bool danger);
  bool isDanger() const;

private:
  void applyLook();

  bool m_danger = false;
};
