/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "LogWidget.h"

#include <gui/Logger.h>
#include <gui/StyleUtils.h>
#include <gui/style/TextSize.h>

#include <QAbstractTextDocumentLayout>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QVBoxLayout>

LogWidget::LogWidget(QWidget *parent) : QWidget{parent}, m_textLog{new QPlainTextEdit(this)}
{
  m_textLog->setReadOnly(true);
  m_textLog->setMaximumBlockCount(10000);
  m_textLog->setLineWrapMode(QPlainTextEdit::NoWrap);
  m_textLog->setFont(fixedFont());

  auto layout = new QVBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(m_textLog);

  setLayout(layout);

  connect(deskflow::gui::Logger::instance(), &deskflow::gui::Logger::newLine, this, &LogWidget::appendLine);
  // the font of the log has a size of its own on a Mac: it follows the size of text of the new look
  deskflow::gui::style::onTextSizeChanged(this, [this] { m_textLog->setFont(fixedFont()); });
}

void LogWidget::useCardLook(const QString &accessibleName)
{
  m_textLog->setFrameShape(QFrame::NoFrame);
  m_textLog->document()->setDocumentMargin(4);
  // the card is the ground: the text is painted on it
  auto palette = m_textLog->palette();
  for (const auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled})
    palette.setColor(group, QPalette::Base, Qt::transparent);
  m_textLog->setPalette(palette);
  m_textLog->viewport()->setAutoFillBackground(false);
  m_textLog->setAccessibleName(accessibleName);
}

void LogWidget::setLogName(const QString &name)
{
  m_textLog->setAccessibleName(name);
}

void LogWidget::appendLine(const QString &msg)
{
  m_textLog->appendPlainText(msg);
}

void LogWidget::findNext(const QString &text)
{
  if (text.isEmpty())
    return;

  if (!m_textLog->find(text)) {
    m_textLog->moveCursor(QTextCursor::Start);
    m_textLog->find(text);
  }
}

void LogWidget::findPrevious(const QString &text)
{
  if (text.isEmpty())
    return;

  if (!m_textLog->find(text, QTextDocument::FindBackward)) {
    m_textLog->moveCursor(QTextCursor::End);
    m_textLog->find(text, QTextDocument::FindBackward);
  }
}
