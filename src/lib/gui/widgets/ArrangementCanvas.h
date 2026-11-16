/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "server/Arrangement.h"

#include <QHash>
#include <QPointF>
#include <QRectF>
#include <QWidget>

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

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
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
  void moveSelected(double dx, double dy);
  int indexOf(const QString &computer) const;
  //! The arrangement as it is while a computer is dragged: with the computer where it would drop
  deskflow::server::arrangement::Arrangement shownArrangement() const;

  deskflow::server::arrangement::Arrangement m_arrangement;
  deskflow::server::arrangement::DerivedLinks m_derived;
  std::vector<deskflow::server::arrangement::Issue> m_issues;
  QHash<QString, Status> m_status;
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
  };
  Drag m_drag;
};
