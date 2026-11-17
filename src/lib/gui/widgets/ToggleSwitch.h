/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QCheckBox>
#include <QVariantAnimation>

/*!
 * \brief The switch of the new look: a track with a knob that is on (the accent) or off. It is a check box: what asks a
 * QCheckBox for its state, signals or text works as it did, and it is the same size as a check box with a text.
 *
 * With the style layer it paints itself with the tokens: 46 x 28 when it has no text (the switch of a row of settings,
 * SettingRow), 36 x 20 beside a text. The knob moves in the time of a hover (at once when the OS asks for less motion),
 * and a ring of the accent shows that the keyboard is on it. For a screen reader it is a switch that is on or off.
 * Without the style layer it is a QCheckBox in every respect, down to the pixel.
 */
class ToggleSwitch : public QCheckBox
{
  Q_OBJECT

public:
  explicit ToggleSwitch(QWidget *parent = nullptr);
  explicit ToggleSwitch(const QString &text, QWidget *parent = nullptr);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

  //! The size of the track: for a switch without text and for one with it
  static QSize trackSize(bool withText);
  //! 0 (the knob at the off end) to 1 (at the on end)
  qreal knobPosition() const;

protected:
  void paintEvent(QPaintEvent *event) override;
  void checkStateSet() override;
  void focusInEvent(QFocusEvent *event) override;
  void focusOutEvent(QFocusEvent *event) override;
  void changeEvent(QEvent *event) override;
  bool hitButton(const QPoint &pos) const override;

private:
  static qreal targetOf(Qt::CheckState state);
  QSize switchArea() const;
  void moveKnobTo(qreal target);

  QVariantAnimation m_animation;
  qreal m_knob = 0;
  bool m_keyboardFocus = false;
};
