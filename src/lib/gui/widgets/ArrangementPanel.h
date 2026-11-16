/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QWidget>

class ArrangementCanvas;
class QLabel;
class QPushButton;
class ServerConfig;

//! The arrangement of the computers with what goes with it, for the server configuration dialog (experimental)
/*!
Shows the computers of a \c ServerConfig on an \c ArrangementCanvas, and puts back in it where they were dropped. It
says that it is an experiment and has the way back to the classic grid.
*/
class ArrangementPanel : public QWidget
{
  Q_OBJECT

public:
  explicit ArrangementPanel(ServerConfig &config, QWidget *parent = nullptr);

  //! Show the computers of the configuration as they are now
  void refresh();

  ArrangementCanvas *canvas() const
  {
    return m_canvas;
  }

  //! Whether the arrangement has an error, which a configuration cannot be saved with
  bool hasErrors() const;

  //! Whether a computer can be added: the grid that holds the computers is not full
  void setAddEnabled(bool enabled);

Q_SIGNALS:
  //! A computer was moved, or the arrangement was made again from the classic grid
  void changed();
  void useClassicGridRequested();
  void editComputerRequested(const QString &computer);
  void removeComputerRequested(const QString &computer);
  void addComputerRequested();

private:
  void updateIssues();
  void updateSelection();
  void resetFromClassicGrid();

  ServerConfig &m_config;
  ArrangementCanvas *m_canvas = nullptr;
  QLabel *m_lblTitle = nullptr;
  QLabel *m_lblHint = nullptr;
  QLabel *m_lblIssues = nullptr;
  QPushButton *m_btnClassicGrid = nullptr;
  QPushButton *m_btnFit = nullptr;
  QPushButton *m_btnReset = nullptr;
  QPushButton *m_btnEdit = nullptr;
  QPushButton *m_btnRemove = nullptr;
  QPushButton *m_btnAdd = nullptr;
  bool m_errors = false;
};
