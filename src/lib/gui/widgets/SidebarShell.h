/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QWidget>

class NavRail;

/*!
 * \brief The sidebar and the content of a window, and what the sidebar does as the window changes its width.
 *
 * There are three states of the sidebar:
 *  - unfolded: 236 px with the labels. It is what there is when the window is wide enough and the user did not fold it
 *  - folded: 76 px, a rail of icons. The user's choice (isCollapsed(), saved by whoever listens to collapsedChanged()),
 *    or what the window makes of it when it is too narrow (below autoCollapseWidth(): isNarrow()). A narrow window does
 *    not change the choice of the user: the sidebar is unfolded again when the window is wide again
 *  - overlay: in a narrow window the button of the sidebar unfolds it over the content (the content stays where it was,
 *    and is dimmed). Escape, a click on the dimmed content, and a press on an entry fold it again, and the focus
 *    goes back to the button
 *
 * The content is placed by this widget (not by a layout) so that it follows the width of the sidebar while it moves.
 */
class SidebarShell : public QWidget
{
  Q_OBJECT

public:
  explicit SidebarShell(QWidget *parent = nullptr);

  NavRail *rail() const;
  void setContent(QWidget *content);
  QWidget *content() const;

  //! What the user chose
  bool isCollapsed() const;
  void setCollapsed(bool collapsed);

  //! The window is too narrow for the sidebar with its labels
  bool isNarrow() const;
  int autoCollapseWidth() const;
  void setAutoCollapseWidth(int width);

  bool isOverlayOpen() const;
  void openOverlay();
  void closeOverlay();

  //! The sidebar shows its labels now (unfolded, or in the overlay)
  bool isRailExpanded() const;

  //! The room around what is drawn, and between the sidebar and the content
  static constexpr int kMargin = 12;
  static constexpr int kGap = 12;
  static constexpr int kDefaultAutoCollapseWidth = 900;

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

Q_SIGNALS:
  //! The user folded or unfolded the sidebar (and the window is wide enough to show the difference)
  void collapsedChanged(bool collapsed);
  void narrowChanged(bool narrow);
  void overlayChanged(bool open);

protected:
  bool event(QEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private:
  void relayout();
  void applyState(bool animate);
  void toggleRequested();
  bool wantsExpanded() const;

  NavRail *m_rail = nullptr;
  QWidget *m_content = nullptr;
  QWidget *m_scrim = nullptr;
  bool m_collapsed = false;
  bool m_narrow = false;
  bool m_overlay = false;
  int m_autoCollapseWidth = kDefaultAutoCollapseWidth;
};
