/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QList>
#include <QWidget>

class InlineHint;
class QHBoxLayout;
class QLabel;
class QVBoxLayout;
class ToggleSwitch;

/*!
 * \brief One setting in a card: a title with a line of guidance under it at the start, and its controls at the end (a
 * switch, a number, a choice). The row is 44 px high at the least.
 *
 * The controls are the widgets that the form has, moved into the row: the code that reads and sets them does not
 * change.
 *  - The last control that is added is the primary one: the title is its buddy (the mnemonic of the title puts the
 * focus on it) and a click on the text of the row toggles it when it is a ToggleSwitch.
 *  - A ToggleSwitch with a text that comes to a row without a title gives its text as the title.
 *  - The row is shown when one of its controls is shown, and its text is dimmed when none of them is enabled, so the
 *    code that hides or disables a control needs no other change.
 *  - The controls get the title as their name for a screen reader, and the first of them the guidance as its
 *    description.
 */
class SettingRow : public QWidget
{
  Q_OBJECT

public:
  explicit SettingRow(const QString &title = {}, const QString &description = {}, QWidget *parent = nullptr);

  //! The title as it is shown, with its mnemonic (&)
  QString rawTitle() const;
  //! The title as it is read: without the mnemonic
  QString title() const;
  void setTitle(const QString &title);

  QString description() const;
  void setDescription(const QString &description);

  //! What is wrong with the value that was typed (it was not applied): shown in place of the guidance in the colour of
  //! an error, until it is set to nothing. A screen reader gets it as the description.
  QString error() const;
  void setError(const QString &error);

  void addControl(QWidget *control);
  QList<QWidget *> controls() const;
  QWidget *primaryControl() const;

  //! The controls take the room that is left and the text only what it needs: for a path or a long text field
  void setExpandingControls(bool expanding);

  //! A hairline on the upper edge; a card sets it for every row but its first one
  bool hasDivider() const;
  void setDivider(bool divider);

  //! The text of the row (title and guidance) as a rectangle in the row
  QRect textRect() const;

  static int minimumRowHeight();

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void applyLook();
  void updateState();
  void updateNames();
  void showHint();
  bool clickOnText(const QPoint &pos) const;

  QString m_rawTitle;
  QLabel *m_title = nullptr;
  InlineHint *m_hint = nullptr;
  QHBoxLayout *m_layout = nullptr;
  QVBoxLayout *m_textLayout = nullptr;
  QHBoxLayout *m_controlLayout = nullptr;
  QList<QWidget *> m_controls;
  bool m_divider = false;
  bool m_pressedOnText = false;
  bool m_hiddenByControls = false;
  QString m_description;
  QString m_error;
};
