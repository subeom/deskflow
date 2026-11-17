/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QLabel>

/*!
 * \brief The title of a page of the main window: one line of text in the title size, in the colour of the text of the
 * look. A screen reader finds it as a heading, so that it can be walked to from one page to the next.
 */
class PageTitle : public QLabel
{
  Q_OBJECT

public:
  explicit PageTitle(QWidget *parent = nullptr);
  explicit PageTitle(const QString &text, QWidget *parent = nullptr);

private:
  void init();
};
