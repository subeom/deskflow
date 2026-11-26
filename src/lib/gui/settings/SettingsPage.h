/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QList>
#include <QString>
#include <QWidget>

#include <functional>

class PageTitle;
class QScrollArea;
class QVBoxLayout;
class SettingCard;
class SettingRow;

namespace deskflow::gui::settings {

/*!
 * \brief A page of the settings in the main window: its title (a heading for a screen reader) and, under it, cards of
 * settings that scroll when the window is shorter than they are.
 *
 * The page is a child of the stack of the main window. The sidebar shows it (MainShell::addSettingsPage), and the
 * cards are put on the backdrop of the window.
 *
 * The texts are written in functions (follow(), addCard(), addRow()) that run when they are given and again when the
 * language changes, so a page needs nothing else to be translated while it is open.
 */
class SettingsPage : public QWidget
{
  Q_OBJECT

public:
  SettingsPage(const QString &id, std::function<QString()> title, QWidget *parent = nullptr);

  //! The id that the sidebar and the window know the page by
  QString pageId() const;
  PageTitle *titleLabel() const;
  QString title() const;

  //! A card at the end of the page
  SettingCard *addCard(std::function<QString()> title);
  //! A card that is made by the caller, at the end of the page
  void addCard(SettingCard *card);
  //! A row at the end of the card; the texts follow the language
  SettingRow *addRow(SettingCard *card, std::function<QString()> title, std::function<QString()> description = {});

  //! Text that follows the language: \p apply runs now and after each change of the language
  void follow(std::function<void()> apply);

  QList<SettingCard *> cards() const;

  //! Where a later page of the settings puts a card that belongs to it (documented insertion point)
  QVBoxLayout *cardList() const;

protected:
  void changeEvent(QEvent *event) override;
  void showEvent(QShowEvent *event) override;

Q_SIGNALS:
  //! The page is shown: what it lists from the system (the addresses of this computer) is read again
  void shown();

private:
  QString m_id;
  PageTitle *m_title = nullptr;
  QScrollArea *m_scroll = nullptr;
  QVBoxLayout *m_cards = nullptr;
  QList<std::function<void()>> m_followers;
  std::function<QString()> m_titleText;
};

} // namespace deskflow::gui::settings
