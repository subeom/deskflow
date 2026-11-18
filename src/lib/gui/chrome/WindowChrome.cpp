/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "WindowChrome.h"

#include "ChromeGeometry.h"
#include "WindowHeader.h"

#ifdef Q_OS_WIN
#include "WindowChromeWin.h"
#endif

#include <QApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QMainWindow>
#include <QMouseEvent>
#include <QWindow>

namespace deskflow::gui::chrome {

namespace {

#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
Qt::CursorShape cursorFor(Qt::Edges edges)
{
  if (edges == Qt::Edges(Qt::LeftEdge) || edges == Qt::Edges(Qt::RightEdge))
    return Qt::SizeHorCursor;
  if (edges == Qt::Edges(Qt::TopEdge) || edges == Qt::Edges(Qt::BottomEdge))
    return Qt::SizeVerCursor;
  if (edges == (Qt::TopEdge | Qt::LeftEdge) || edges == (Qt::BottomEdge | Qt::RightEdge))
    return Qt::SizeFDiagCursor;
  return Qt::SizeBDiagCursor;
}
#endif

} // namespace

WindowChrome::WindowChrome(QMainWindow *window, WindowHeader *header, QObject *parent)
    : QObject(parent),
      m_window(window),
      m_header(header)
{
}

WindowChrome::~WindowChrome()
{
  if (m_edgeCursor)
    QGuiApplication::restoreOverrideCursor();
}

bool WindowChrome::isApplied() const
{
  return m_applied;
}

bool WindowChrome::apply()
{
  if (m_applied)
    return true;

#if defined(Q_OS_WIN)
  // a platform plugin that is not the one of Windows (the tests of the program run offscreen) has no window of the
  // system to change: the header is there and the system has nothing to do with it
  if (QGuiApplication::platformName() == QLatin1String("windows")) {
    if (!installWindowsFrame(m_window, m_header, this))
      return false;
  }
#elif defined(Q_OS_MACOS)
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
  // the content goes under the title bar, which is drawn without a background: the buttons of the system are over the
  // left end of the header
  m_window->setWindowFlag(Qt::ExpandedClientAreaHint, true);
  m_window->setWindowFlag(Qt::NoTitleBarBackgroundHint, true);
#else
  return false;
#endif
#else
  m_window->setWindowFlag(Qt::FramelessWindowHint, true);
#endif

  connect(m_header, &WindowHeader::minimizeRequested, m_window, &QWidget::showMinimized);
  connect(m_header, &WindowHeader::maximizeToggleRequested, this, &WindowChrome::toggleMaximized);
#ifndef Q_OS_WIN
  // Windows has the caption for the header: it drags the window and a double click maximizes it
  connect(m_header, &WindowHeader::dragRequested, this, &WindowChrome::startMove);
  connect(m_header, &WindowHeader::captionDoubleClicked, this, &WindowChrome::toggleMaximized);
  qApp->installEventFilter(this);
#endif
  m_window->installEventFilter(this);
  m_header->setMaximized(m_window->isMaximized());
  m_applied = true;
  return true;
}

void WindowChrome::toggleMaximized()
{
  if (m_window->isMaximized() || m_window->isFullScreen())
    m_window->showNormal();
  else
    m_window->showMaximized();
}

void WindowChrome::startMove()
{
#ifndef Q_OS_WIN
  if (auto *handle = m_window->windowHandle(); handle && handle->startSystemMove())
    return;
  // the compositor did not do it: the window follows the mouse until the button is let go
  m_manualMove = true;
  m_pressedAt = QCursor::pos();
  m_windowAt = m_window->pos();
#endif
}

bool WindowChrome::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == m_window && event->type() == QEvent::WindowStateChange)
    m_header->setMaximized(m_window->isMaximized());

#ifndef Q_OS_WIN
  const auto type = event->type();
  if (type != QEvent::MouseMove && type != QEvent::MouseButtonPress && type != QEvent::MouseButtonRelease &&
      type != QEvent::Leave)
    return false;
  const auto *widget = qobject_cast<QWidget *>(watched);
  if (!widget || widget->window() != m_window)
    return false;

  if (m_manualMove) {
    if (type == QEvent::MouseMove) {
      m_window->move(m_windowAt + (QCursor::pos() - m_pressedAt));
      return true;
    }
    if (type == QEvent::MouseButtonRelease) {
      m_manualMove = false;
      return true;
    }
  }

#if !defined(Q_OS_MACOS)
  // a window without a frame has no edges for the system to resize it by: they are the strip along its sides
  if (type == QEvent::Leave) {
    if (m_edgeCursor) {
      QGuiApplication::restoreOverrideCursor();
      m_edgeCursor = false;
    }
    return false;
  }
  if (type == QEvent::MouseMove || type == QEvent::MouseButtonPress) {
    const auto *mouse = static_cast<QMouseEvent *>(event);
    const auto local = m_window->mapFromGlobal(mouse->globalPosition().toPoint());
    ChromeMetrics metrics;
    metrics.windowSize = m_window->size();
    metrics.resizeBorder = (m_window->isMaximized() || m_window->isFullScreen()) ? 0 : kResizeBorder;
    metrics.headerHeight = WindowHeader::kHeight;
    metrics.buttonWidth = m_header->options().captionButtons ? WindowHeader::kCaptionButtonWidth : 0;
    metrics.maximized = m_window->isMaximized();
    const auto edges = edgesOf(hitTest(metrics, local));
    if (type == QEvent::MouseMove) {
      if (edges) {
        if (m_edgeCursor)
          QGuiApplication::changeOverrideCursor(cursorFor(edges));
        else
          QGuiApplication::setOverrideCursor(cursorFor(edges));
        m_edgeCursor = true;
      } else if (m_edgeCursor) {
        QGuiApplication::restoreOverrideCursor();
        m_edgeCursor = false;
      }
    } else if (edges && mouse->button() == Qt::LeftButton) {
      if (auto *handle = m_window->windowHandle(); handle && handle->startSystemResize(edges))
        return true;
    }
  }
#endif
#endif
  return false;
}

} // namespace deskflow::gui::chrome
