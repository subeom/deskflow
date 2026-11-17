/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>
#include <QWidget>

class QPlainTextEdit;
class QTemporaryFile;

class LogWidget : public QWidget
{
  Q_OBJECT
public:
  explicit LogWidget(QWidget *parent = nullptr);
  void appendLine(const QString &msg);
  void findNext(const QString &text);
  void findPrevious(const QString &text);
  //! The log as the text of a card of the new look: no frame of its own and no ground of its own (the card is the ground),
  //! with the room of a card around the lines
  void useCardLook(const QString &accessibleName);
  //! The name that a screen reader says for the lines (the card look)
  void setLogName(const QString &name);

private:
  QPlainTextEdit *m_textLog = nullptr;
};
