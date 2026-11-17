/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QAbstractButton>
#include <QList>
#include <QWidget>

class SegmentedControl;

//! One choice of a SegmentedControl
class SegmentedControlSegment : public QAbstractButton
{
  Q_OBJECT

public:
  explicit SegmentedControlSegment(QWidget *parent);
  QSize sizeHint() const override;

Q_SIGNALS:
  void navigate(int key);

protected:
  void paintEvent(QPaintEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void focusInEvent(QFocusEvent *event) override;
  void focusOutEvent(QFocusEvent *event) override;

private:
  bool m_keyboardFocus = false;
};

/*!
 * \brief A choice of one out of a few, side by side in a capsule: the one that is chosen is a capsule of the accent.
 *
 * It is the control for what the window is in ("Share this computer" or "Use another computer"). For the keyboard it is
 * one stop: the arrow keys choose the previous and the next one, as they do for radio buttons. A screen reader finds a
 * group of radio buttons, one of them checked, and each choice can have a description that it reads after its name.
 */
class SegmentedControl : public QWidget
{
  Q_OBJECT

public:
  explicit SegmentedControl(QWidget *parent = nullptr);

  int addSegment(const QString &text, const QString &description = {});
  int count() const;
  QString segmentText(int index) const;
  void setSegmentText(int index, const QString &text);
  void setSegmentDescription(int index, const QString &description);
  SegmentedControlSegment *segment(int index) const;

  //! -1 when no choice is made
  int current() const;
  void setCurrent(int index);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

  static constexpr int kHeight = 44;
  static constexpr int kPadding = 4;

Q_SIGNALS:
  void currentChanged(int index);

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  void relayout();
  void navigate(int key);
  void updateFocusStop();

  QList<SegmentedControlSegment *> m_segments;
  int m_current = -1;
};
