/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QGroupBox>

/*!
 * \brief A flat group box that is ticked to be opened: its contents are shown when it is checked.
 *
 * With the style layer (Appearance=Deskflow) it is painted as a disclosure: a chevron that points to the right when it is
 * closed and down when it is open, and the title, with a ring when the keys are on it; a screen reader is told that it is
 * a button that is expanded or collapsed. Without the style layer it is the group box with a check box that it has always
 * been, painted by the style of the OS. Either way it is a checkable \c QGroupBox: what ticks it opens it.
 */
class DisclosureBox : public QGroupBox
{
  Q_OBJECT

public:
  explicit DisclosureBox(const QString &title, QWidget *parent = nullptr);

protected:
  void paintEvent(QPaintEvent *event) override;
  void focusInEvent(QFocusEvent *event) override;
  void focusOutEvent(QFocusEvent *event) override;
};
