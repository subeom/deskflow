/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/widgets/StatusChip.h"

#include <QList>
#include <QStringList>
#include <QWidget>

class InlineHint;
class QPushButton;
class QVBoxLayout;
class SettingCard;

namespace deskflow::gui {

/*!
 * \brief What the dialogs of the connection tests show in the new look: a card for each address or stage, with a chip
 * that says how it went, the sentence that says what it means, and what to try (hidden until it is asked for).
 *
 * The dialogs of the system look keep a text with colours that they have always had. The rows follow the colours of the
 * look, in a dark one too, and a screen reader finds a group for each address or stage, with the state in its name.
 */
class TestResultRows : public QWidget
{
  Q_OBJECT

public:
  struct Row
  {
    StatusChip::Kind kind = StatusChip::Kind::Neutral;
    //! The word of the chip ("Open", "Failed")
    QString state;
    //! The address or the stage
    QString title;
    //! What it means, in a sentence
    QString detail;
    //! What to do about it, one sentence for each; they are shown when the person asks for them
    QStringList hints;

    bool operator==(const Row &other) const = default;
  };

  explicit TestResultRows(QWidget *parent = nullptr);

  //! The text above the rows (what the test does and does not do); empty takes it away
  void setIntro(const QString &text);
  //! The rows, in the order that they are given; a row that is given again keeps being open when it has the same hints
  void setRows(const QList<Row> &rows);
  //! The sentence under the rows; \p problem gives it the colour of an error
  void setConclusion(const QString &text, bool problem = false);

  QList<Row> rows() const;
  int rowCount() const;
  //! The card of a row, the button that opens its hints and whether they are shown (for the tests and the screen
  //! reader)
  SettingCard *card(int index) const;
  QPushButton *hintsButton(int index) const;
  bool hintsShown(int index) const;
  QString intro() const;
  QString conclusion() const;

private:
  void rebuild();

  QList<Row> m_rows;
  QList<bool> m_open;
  InlineHint *m_intro = nullptr;
  InlineHint *m_conclusion = nullptr;
  QVBoxLayout *m_list = nullptr;
  QList<SettingCard *> m_cards;
  QList<QPushButton *> m_buttons;
  QList<QWidget *> m_blocks;
};

} // namespace deskflow::gui
