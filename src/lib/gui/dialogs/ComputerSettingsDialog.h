/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QDialog>

class QWidget;
class QString;
class QSpinBox;

class Computer;
class ComputerList;

namespace Ui {
class ComputerSettingsDialog;
}

class ComputerSettingsDialog : public QDialog
{
  Q_OBJECT

public:
  explicit ComputerSettingsDialog(
      QWidget *parent, Computer *computer = nullptr, const ComputerList *computers = nullptr
  );
  ~ComputerSettingsDialog() override;

  //! Put the monitors that a diagnostic tool printed in \p path as the monitor layout of the computer (experimental)
  /*!
  Returns true if the file has a block of monitors that is a valid layout, which is then in the text field. A block
  that is not valid is put there too, to be put right, and the file is said to not be a fit; a file with no block, or
  that cannot be read, changes nothing.
  */
  bool importMonitorLayout(const QString &path);

public Q_SLOTS:
  void accept() override;

private Q_SLOTS:
  void addAlias();
  void removeAlias() const;
  void checkNewAliasName(const QString &text);
  void aliasSelected();
  void chooseMonitorLayoutFile();
  void updateMonitorLayoutStatus();

private:
  std::unique_ptr<Ui::ComputerSettingsDialog> ui;
  Computer *m_computer;
  // the experiment is on, so the layout can be set
  bool m_monitorLayoutAvailable = false;
  // the size of the screen, when there are no monitors, and the display scale: for the arrangement of the computers
  QSpinBox *m_sbArrangementWidth = nullptr;
  QSpinBox *m_sbArrangementHeight = nullptr;
  QSpinBox *m_sbDisplayScale = nullptr;
  // the scale that the dialog was opened with
  int m_scaleShown = 100;
};
