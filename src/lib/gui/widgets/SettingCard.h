/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/style/PanelWidget.h"

#include <QList>

class QVBoxLayout;
class SectionHeader;
class SettingRow;

/*!
 * \brief A panel of settings: an optional header (a SectionHeader, with a chip or a button at its end) over a column of
 * SettingRow with a hairline between each two of them. It is the panel of the new look that carries text (Dense), and
 * it is what a group box of a form is replaced by.
 *
 * The hairline is for the rows that are shown: when a row is hidden (a setting that this OS has not) the next one does
 * not start with a line, and a card that has no row shown is hidden.
 */
class SettingCard : public deskflow::gui::style::PanelWidget
{
  Q_OBJECT

public:
  explicit SettingCard(const QString &title = {}, QWidget *parent = nullptr);

  SectionHeader *header() const;
  QString title() const;

  //! The card owns the row; rows are in the order that they were added
  void addRow(SettingRow *row);
  QList<SettingRow *> rows() const;
  //! Takes every row away and deletes it (a list that is made again, the computers)
  void clearRows();

  //! A row is the first when it is the first one that is shown
  void refreshDividers();

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  QVBoxLayout *m_layout = nullptr;
  SectionHeader *m_header = nullptr;
  QList<SettingRow *> m_rows;
  bool m_hiddenByRows = false;
};
