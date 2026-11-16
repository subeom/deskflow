/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "server/MonitorEditor.h"

#include <QDialog>
#include <QRect>

#include <optional>

class MonitorEditorWidget;
class QComboBox;
class QDialogButtonBox;
class QGroupBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

/*!
 * \class MonitorEditorDialog
 * \brief Draw the monitors of a computer with the mouse; the program derives the numbers.
 *
 * The screen that the computer reports is the ground truth: the drawing is fitted to it when its shape is the same, and
 * the status says that it matches, that it does not (and by how much), or that nothing is known yet. The numbers of the
 * layout are in "Advanced: numbers", in step with the drawing.
 */
class MonitorEditorDialog : public QDialog
{
  Q_OBJECT

public:
  MonitorEditorDialog(
      QWidget *parent, const QString &computerName, const deskflow::server::editor::Layout &drawing,
      const std::optional<QRect> &reported
  );

  const deskflow::server::editor::Layout &drawing() const;
  //! What the drawing is in the coordinates of the computer, the way that it is saved
  deskflow::server::editor::Realised realised() const;
  //! The layout can be saved: it has no problem that is an error
  bool canSave() const;

  //! The drawing is what the computer reports: it is shown and cannot be changed, until the user chooses to draw his
  //! own
  void showReportedMonitors();
  bool isShowingReportedMonitors() const
  {
    return m_showingReported;
  }

  MonitorEditorWidget *editorWidget() const
  {
    return m_widget;
  }

private:
  void changeTo(const deskflow::server::editor::Layout &layout, int selected = -2);
  void refresh();
  void syncControls();
  void applyNumbers();
  void importNumbers();
  void chooseResolution();
  void chooseScale();
  void undo();
  void redo();
  void updateReportedMode();
  QString describeProblem(const deskflow::server::editor::Problem &problem) const;

  QString m_name;
  std::optional<QRect> m_reported;
  deskflow::server::editor::History m_history;
  bool m_stretch = false;
  // "Other size..." was chosen for the monitor that is selected: the sizes are typed until it or the size changes
  bool m_otherSize = false;
  // the scale of the zoom of what was drawn, which numbers that are typed are in
  double m_numbersZoom = 1.0;
  // the monitors are the ones that the computer reported: nothing is edited until the user draws his own
  bool m_showingReported = false;

  MonitorEditorWidget *m_widget = nullptr;
  QLabel *m_status = nullptr;
  QComboBox *m_resolution = nullptr;
  QSpinBox *m_pixelsW = nullptr;
  QSpinBox *m_pixelsH = nullptr;
  QLabel *m_times = nullptr;
  QComboBox *m_scale = nullptr;
  QPushButton *m_rotate = nullptr;
  QPushButton *m_makeMain = nullptr;
  QPushButton *m_add = nullptr;
  QPushButton *m_remove = nullptr;
  QPushButton *m_undo = nullptr;
  QPushButton *m_redo = nullptr;
  QPushButton *m_stretchButton = nullptr;
  QPushButton *m_drawOwn = nullptr;
  QGroupBox *m_advanced = nullptr;
  QWidget *m_advancedContent = nullptr;
  QPlainTextEdit *m_numbers = nullptr;
  QLabel *m_numbersStatus = nullptr;
  QPushButton *m_import = nullptr;
  QPushButton *m_apply = nullptr;
  QDialogButtonBox *m_buttons = nullptr;
};
