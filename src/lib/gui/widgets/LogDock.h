/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QDockWidget>

class LogWidget;
class QLabel;
class QHBoxLayout;
class QPushButton;
class SearchWidget;

class LogDock : public QDockWidget
{
  Q_OBJECT
public:
  explicit LogDock(QWidget *parent = nullptr);
  void appendLine(const QString &msg);
  void setFloating(bool floating);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void changeEvent(QEvent *e) override;

private:
  //! With the style layer the log is a card of the new look: the header (the title, the search and the two buttons) and the
  //! lines are in it, and the title bar of the dock has nothing in it
  void buildCard();
  void buildTitleBar();
  void updateButtonIcons();

  bool m_card = false;
  LogWidget *m_textLog = nullptr;
  QPushButton *m_btnClose = nullptr;
  QPushButton *m_btnFloat = nullptr;
  QLabel *m_lblTitle = nullptr;
  SearchWidget *m_searchWidget = nullptr;
};
