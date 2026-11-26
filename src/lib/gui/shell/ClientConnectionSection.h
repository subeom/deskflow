/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QStackedLayout;
class QVBoxLayout;
class SettingCard;
class SettingRow;

namespace deskflow::gui {

/*!
 * \brief The connection of a client in the main window: the card with the address of the server, the button that tests
 * it, and the button of the settings of the client. The widgets are the ones of the main window (they are moved into
 * the card), so what reads and sets them does not change.
 *
 * It is a section of its own so that what is added to the connection later does not touch the rest of the window. There
 * are two places that are made for that, and nothing is in them now:
 *  - addressStack(): the address is in a stack of widgets. Index 0 is the field of the address. A widget that is added
 *    to the stack and made the current one replaces the field (a line that says the address and cannot be typed in);
 *    the field goes back by making index 0 the current one again.
 *  - addStatusLine(): a line of text under the address row, one for each source that has something to say about the
 *    connection. The place is not shown while it has no line.
 */
class ClientConnectionSection : public QWidget
{
  Q_OBJECT

public:
  ClientConnectionSection(
      QLabel *hostLabel, QComboBox *hostname, QPushButton *testButton, QPushButton *configureButton,
      QWidget *parent = nullptr
  );

  SettingCard *card() const;
  SettingRow *addressRow() const;

  //! See the description of the class: the address, and what can stand in its place
  QStackedLayout *addressStack() const;
  //! See the description of the class: the section owns the line
  void addStatusLine(QWidget *line);
  void removeStatusLine(QWidget *line);
  int statusLineCount() const;

  //! The texts that the main window translates (they are taken again when the language changes)
  void retranslate();
  //! What to write in the field of the address: it is said once, under the title of the row, and the field itself is
  //! empty (the same long sentence in the row and in the field was read twice and looked twice)
  void setAddressHint(const QString &hint);

private:
  QLabel *m_hostLabel = nullptr;
  QComboBox *m_hostname = nullptr;
  SettingCard *m_card = nullptr;
  SettingRow *m_addressRow = nullptr;
  QWidget *m_addressBox = nullptr;
  QStackedLayout *m_addressStack = nullptr;
  QString m_addressHint;
  QWidget *m_statusLines = nullptr;
  QVBoxLayout *m_statusLayout = nullptr;
};

} // namespace deskflow::gui
