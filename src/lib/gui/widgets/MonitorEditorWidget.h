/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/style/AccessibleItems.h"
#include "gui/style/CanvasTheme.h"
#include "server/MonitorEditor.h"

#include <QPixmap>
#include <QWidget>

#include <optional>
#include <utility>

/*!
 * \class MonitorEditorWidget
 * \brief Draws the monitors of one computer in real proportion, and changes them with the mouse.
 *
 * All the geometry is in \c deskflow::server::editor; this draws it, hands it the mouse, and says when a change is
 * done. A monitor is dragged to move it, its edges and corners are dragged to resize it (with guides and snapping), and
 * the keys nudge, rotate and delete the selected one.
 *
 * With the style layer (Appearance=Deskflow) it is painted from the tokens of the look like the arrangement canvas
 * (neutral monitors, the accent for the selected one, the colour of an error for what is wrong), Tab goes through the
 * monitors, Enter and Space select, a ring shows where the keys are, and a screen reader gets a list of monitors.
 * Without the style layer it is what it always was.
 */
class MonitorEditorWidget : public QWidget, public deskflow::gui::style::AccessibleItemSource
{
  Q_OBJECT

public:
  explicit MonitorEditorWidget(QWidget *parent = nullptr);

  void setDrawing(const deskflow::server::editor::Layout &layout);
  const deskflow::server::editor::Layout &drawing() const
  {
    return m_layout;
  }

  //! The drawing is only looked at: the mouse and the keys change nothing (the monitors that a computer reports)
  void setReadOnly(bool readOnly);
  bool isReadOnly() const
  {
    return m_readOnly;
  }

  void setSelected(int index);
  int selected() const
  {
    return m_selected;
  }

  //! A size in units that a side of the only monitor snaps to: the screen that the computer reports
  void setTargetSize(const std::optional<std::pair<double, double>> &size)
  {
    m_targetSize = size;
  }

  //! Where the monitor \p index is in the widget, and a place of the drawing
  QRectF rectOf(int index) const;
  QPointF toWidget(double x, double y) const;
  //! The text of a monitor: its pixels and its scale
  static QString describe(const deskflow::server::editor::Monitor &monitor);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

  int accessibleItemCount() const override;
  QString accessibleItemName(int index) const override;
  QString accessibleItemDescription(int index) const override;
  QRect accessibleItemRect(int index) const override;
  bool accessibleItemSelected(int index) const override;
  void selectAccessibleItem(int index) override;

Q_SIGNALS:
  //! A change is done: the mouse was released, or a key was pressed
  void drawingChanged(const deskflow::server::editor::Layout &layout);
  void selectionChanged(int index);

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void focusInEvent(QFocusEvent *event) override;
  void focusOutEvent(QFocusEvent *event) override;
  bool focusNextPrevChild(bool next) override;

private:
  bool m_readOnly = false;

  struct Drag
  {
    bool active = false;
    bool moved = false;
    bool resizing = false;
    deskflow::server::editor::Handle handle = deskflow::server::editor::Handle::Right;
    int index = -1;
    QPointF pressed;
    // where the pointer is from the corner or the edge that it holds, in units
    double grabX = 0;
    double grabY = 0;
    deskflow::server::editor::Layout start;
    deskflow::server::editor::Layout preview;
    std::vector<deskflow::server::editor::Guide> guides;
    QString info;
    QPointF pointer;
  };

  void fit() const;
  QPointF toUnits(const QPointF &point) const;
  std::optional<deskflow::server::editor::Handle> handleAt(const QPointF &point) const;
  int monitorAt(const QPointF &point) const;
  void dragTo(const QPointF &point);
  void commit(const deskflow::server::editor::Layout &layout);
  const deskflow::server::editor::Layout &shown() const
  {
    return m_drag.active ? m_drag.preview : m_layout;
  }
  QColor colorOf(size_t index) const;
  void followLook();
  const QPixmap &backdrop(qreal dpr) const;
  bool selectAtTheKeys();

  deskflow::server::editor::Layout m_layout;
  std::optional<std::pair<double, double>> m_targetSize;
  int m_selected = -1;
  // the view: made when it is first needed, and again when what is drawn or the size of the widget changes
  mutable double m_scale = 0.1;
  mutable QPointF m_origin;
  mutable bool m_needsFit = true;
  Drag m_drag;

  // painted from the tokens of the look: there is a style layer
  bool m_themed = false;
  deskflow::gui::style::CanvasTheme m_theme;
  mutable QPixmap m_backdrop;
  // where the keys are: the monitor that was selected last, which a key selects again after a click on the background
  int m_cursor = -1;
};
