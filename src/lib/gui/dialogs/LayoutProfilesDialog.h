/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/layout/Layout.h"

#include <QDialog>

#include <functional>

class QLabel;
class QPushButton;
class QTableWidget;

namespace deskflow::gui::layout {
class LayoutMemoryService;
}

//! The layouts that are remembered, one for each set of displays: look at them, name them, forget them (experimental)
/*!
The list is read only: what is in it is made by the program as the arrangement is saved and the displays change. A name
is the one thing that can be changed, and a layout can be forgotten. \e Save current arrangement as... remembers what the
canvas shows for the displays that there are now, under a name. Everything here is done at once, not when the dialog that
opened this one is accepted. \e Current displays says what the displays are now, to be copied for a report.
*/
class LayoutProfilesDialog : public QDialog
{
  Q_OBJECT

public:
  //! \p arrangement says where the computers are in the window that opened this (its copy of the arrangement)
  LayoutProfilesDialog(
      deskflow::gui::layout::LayoutMemoryService &service, std::function<deskflow::gui::layout::Layout()> arrangement,
      QWidget *parent = nullptr
  );

  //! The profile of the selected row, or empty
  QString selectedId() const;
  //! What the three buttons do once they have the name or the answer
  bool renameSelected(const QString &name);
  bool deleteSelected();
  bool saveCurrentAs(const QString &name);
  //! Reads the list again
  void refresh();

private:
  void askForRename();
  void askForDelete();
  void askForSaveAs();
  void updateButtons();

  deskflow::gui::layout::LayoutMemoryService &m_service;
  std::function<deskflow::gui::layout::Layout()> m_arrangement;
  QLabel *m_lblInfo = nullptr;
  QTableWidget *m_table = nullptr;
  QPushButton *m_btnRename = nullptr;
  QPushButton *m_btnDelete = nullptr;
  QPushButton *m_btnSaveAs = nullptr;
  QLabel *m_lblCurrent = nullptr;
};
