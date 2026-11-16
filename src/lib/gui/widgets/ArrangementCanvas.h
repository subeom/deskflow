/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "server/Arrangement.h"
#include "server/MonitorEditor.h"

#include <QHash>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QWidget>

#include <optional>
#include <vector>

//! The arrangement of the computers, like the displays arrangement of macOS (experimental)
/*!
Every computer is drawn as its monitors, in real proportion, in one shared space. A computer is dragged as a unit and
snaps to the edges of the others; the links of the server configuration are where edges touch, which are highlighted.
The widget only edits an \c Arrangement; what it means for the settings is done by whoever has it.
*/
class ArrangementCanvas : public QWidget
{
  Q_OBJECT

public:
  //! What is known of whether a computer is connected to the server
  enum class Status
  {
    Unknown,
    Connected,
    Offline
  };

  explicit ArrangementCanvas(QWidget *parent = nullptr);

  void setArrangement(const deskflow::server::arrangement::Arrangement &arrangement);
  const deskflow::server::arrangement::Arrangement &arrangement() const
  {
    return m_arrangement;
  }
  //! Where the exposed edges touch, and what is not as it should be: of the arrangement that is shown
  const deskflow::server::arrangement::DerivedLinks &derived() const
  {
    return m_derived;
  }
  const std::vector<deskflow::server::arrangement::Issue> &issues() const
  {
    return m_issues;
  }

  //! The computers whose size is not known: the canvas says so on them
  void setAssumedSizes(const QStringList &computers);
  QStringList assumedSizes() const
  {
    return m_assumed;
  }
  //! The computers that are drawn from the monitors that they reported (not from what the user said): the canvas says
  //! so
  void setReportedMonitorComputers(const QStringList &computers);
  QStringList reportedMonitorComputers() const
  {
    return m_fromReport;
  }
  //! The size that each computer reports, in its own units: a side of a computer that is resized snaps to it
  void setReportedSizes(const QHash<QString, QSize> &sizes);
  //! The computer that the pointer is at the outline of, or empty
  QString hoveredComputer() const
  {
    return m_hovered;
  }
  void setStatus(const QString &computer, Status status);
  void setSelected(const QString &computer);
  QString selected() const
  {
    return m_selected;
  }

  //! Show everything, as big as it fits
  void zoomToFit();

  //! Where the box of the computer is in the widget; empty if there is no such computer
  QRectF boxOf(const QString &computer) const;
  //! Where a place of the arrangement is in the widget
  QPointF toWidget(double x, double y) const;
  //! The computer that has a monitor at \p point of the widget, or empty
  QString computerAt(const QPointF &point) const;

  //! The text of an issue, for the people who use the canvas
  static QString describe(const deskflow::server::arrangement::Issue &issue);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

Q_SIGNALS:
  //! A computer was dropped, or moved with the keys
  void arrangementChanged();
  void selectedChanged(const QString &computer);
  //! A computer was double clicked
  void editRequested(const QString &computer);
  //! The one rectangle of a computer was resized: it is now this one, in the coordinates of the computer
  void computerResized(const QString &computer, int x, int y, int w, int h);
  //! The whole of a computer was scaled on the canvas: its zoom is now this one, and its monitors are as they were
  void computerScaled(const QString &computer, double zoom);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  bool event(QEvent *event) override;

private:
  void recompute();
  void ensureFit() const;
  void fit() const;
  QRectF arrangementBounds(const deskflow::server::arrangement::Arrangement &arrangement) const;
  QRectF toWidget(const deskflow::server::arrangement::Rect &rect) const;
  QColor colorOf(size_t index, bool server) const;
  void paintComputer(
      QPainter &painter, const deskflow::server::arrangement::Arrangement &shown, size_t index, bool overlapping,
      bool unconnected, bool dragged
  ) const;
  void cancelDrag();
  //! How the outline of a computer is dragged: not at all (the server's own displays), as a rectangle whose size is
  //! free, or as a whole that is scaled (a computer with several monitors, or whose screen is known)
  enum class ResizeKind
  {
    None,
    Free,
    Scale
  };
  struct Hit
  {
    int index = -1;
    deskflow::server::editor::Handle handle = deskflow::server::editor::Handle::Right;
  };
  ResizeKind resizeKindOf(int index) const;
  //! The outline of the computer that is within a few pixels of \p point, wherever it is: no computer has to be
  //! selected
  std::optional<Hit> outlineAt(const QPointF &point) const;
  QPointF toUnits(const QPointF &point) const;
  void moveSelected(double dx, double dy);
  int indexOf(const QString &computer) const;
  //! The arrangement as it is while a computer is dragged: with the computer where it would drop
  deskflow::server::arrangement::Arrangement shownArrangement() const;

  deskflow::server::arrangement::Arrangement m_arrangement;
  deskflow::server::arrangement::DerivedLinks m_derived;
  std::vector<deskflow::server::arrangement::Issue> m_issues;
  QHash<QString, Status> m_status;
  QStringList m_assumed;
  QStringList m_fromReport;
  QString m_hovered;
  QHash<QString, QSize> m_reportedSizes;
  QString m_selected;

  // arrangement to widget: scale and where the arrangement origin is
  mutable double m_scale = 0.1;
  mutable QPointF m_offset;
  // the user has zoomed or panned: the view is not fitted any more
  bool m_manualView = false;
  mutable bool m_needsFit = true;

  struct Drag
  {
    int index = -1;
    QPointF pressed;
    double originX = 0;
    double originY = 0;
    deskflow::server::arrangement::SnapResult snapped;
    bool moved = false;
    bool panning = false;
    Qt::MouseButton button = Qt::NoButton;
    QPointF offsetAtPress;
    // an edge or a corner is dragged
    bool resizing = false;
    deskflow::server::editor::Handle handle = deskflow::server::editor::Handle::Right;
    double grabX = 0;
    double grabY = 0;
    deskflow::server::editor::Layout startLayout;
    //! The screen of the computer in its own units, and its shape, for the scaling of a whole computer
    deskflow::server::MonitorRect shape;
    double ratio = 1.0;
    bool scaleMode = false;
    QString info;
    QPointF pointer;
    deskflow::server::arrangement::Computer preview;
    bool previewValid = false;
    std::vector<deskflow::server::arrangement::Guide> resizeGuides;
  };
  Drag m_drag;
};
