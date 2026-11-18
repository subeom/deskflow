/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "ChromeGeometry.h"

#include <QAbstractButton>
#include <QList>
#include <QPointer>
#include <QRect>
#include <QWidget>

class QLabel;
class QMenu;

namespace deskflow::gui::chrome {

//! A button of the header: one of the three of the window (minimize, maximize or restore, close), or the one that opens
//! the menu. It is drawn the way the buttons of the window of the system are drawn (a line, a square, two squares, a
//! cross), with the colours of the look.
class HeaderButton : public QAbstractButton
{
  Q_OBJECT

public:
  enum class Glyph
  {
    Minimize,
    Maximize,
    Restore,
    Close,
    More
  };

  explicit HeaderButton(Glyph glyph, QWidget *parent = nullptr);

  Glyph glyph() const;
  void setGlyph(Glyph glyph);

  /*!
   * The system tells where the mouse is, for a button that it takes for itself (the button of maximizing on Windows 11,
   * which has the flyout of the snap layouts): the button is drawn as it is when the mouse is over it, or pressed.
   */
  void setNativeState(bool hover, bool pressed);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

protected:
  void paintEvent(QPaintEvent *event) override;
  void focusInEvent(QFocusEvent *event) override;
  void focusOutEvent(QFocusEvent *event) override;

private:
  Glyph m_glyph;
  bool m_nativeHover = false;
  bool m_nativePressed = false;
  bool m_keyboardFocus = false;
};

/*!
 * \brief The top of the main window of the new look, in the place of the title bar and the menu bar of the system: the
 * name of the program, the button of the menu with the three dots (all that the menu bar had), and the buttons of the
 * window. The rest of it is where a press drags the window.
 *
 * It does not move or close the window itself: it says what the person asked for (minimizeRequested, ...) and the
 * controller of the window (WindowChrome) does it the way that the system does it. On a Mac the buttons of the window
 * are the ones of the system, which are drawn over the left end of the header (leftInset); there are none of its own.
 */
class WindowHeader : public QWidget
{
  Q_OBJECT

public:
  //! The height, and the width of a button of the window: the size of the ones of Windows 11, with a little more height
  static constexpr int kHeight = 40;
  static constexpr int kCaptionButtonWidth = 46;
  //! The room for the buttons that the system of a Mac draws at the left
  static constexpr int kTrafficLightsInset = 80;

  struct Options
  {
    //! The three buttons of the window (not on a Mac, whose system has them)
    bool captionButtons = true;
    //! The room at the left that something else draws over (the buttons of a Mac)
    int leftInset = 0;
    //! The name of the program in the header (a Mac shows the title in its title bar)
    bool title = true;
  };

  explicit WindowHeader(const Options &options, QWidget *parent = nullptr);

  Options options() const;

  void setTitle(const QString &title);
  QString title() const;

  //! The menu of the button with the three dots; it opens under the button, at its right edge
  void setMenu(QMenu *menu);
  QMenu *menu() const;

  void setMaximized(bool maximized);
  bool isMaximized() const;

  HeaderButton *moreButton() const;
  //! A button of the window; none when the header has no buttons of its own or the region is not a button
  HeaderButton *captionButton(Region region) const;

  //! The rectangles (in the coordinates of this widget) of what takes the mouse for itself: it is not the caption
  QList<QRect> interactiveRects() const;

  //! The texts that the header says in the language of the program
  void retranslate();

  //! Opens the menu as a press on its button does
  void openMenu();

Q_SIGNALS:
  void minimizeRequested();
  void maximizeToggleRequested();
  void closeRequested();
  //! A press on the empty part: where the system has no caption to drag (Linux), the window is moved
  void dragRequested();
  void captionDoubleClicked();
  void systemMenuRequested(const QPoint &globalPos);

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;

private:
  Options m_options;
  QLabel *m_title = nullptr;
  HeaderButton *m_more = nullptr;
  HeaderButton *m_minimize = nullptr;
  HeaderButton *m_maximize = nullptr;
  HeaderButton *m_close = nullptr;
  QPointer<QMenu> m_menu;
  bool m_maximized = false;
};

} // namespace deskflow::gui::chrome
