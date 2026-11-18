/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/style/Icons.h"
#include "gui/style/PanelWidget.h"

#include <QAbstractButton>
#include <QList>
#include <QVariantAnimation>

class NavRail;

/*!
 * \brief One entry of the sidebar: an icon and a label. The label is what a screen reader reads and what the tooltip of
 * the collapsed rail says, whether it is drawn or not.
 *  - Page: the page of the window that is shown (one of them is the current one: a capsule of the accent)
 *  - Action: opens something, or switches something on and off (checkable: "Log")
 *  - Status: a line of information that cannot be pressed (it is text for a screen reader, and has no focus)
 */
class NavItem : public QAbstractButton
{
  Q_OBJECT

public:
  enum class Kind
  {
    Page,
    Action,
    Status
  };

  NavItem(Kind kind, const QString &id, const QString &iconName, const QString &text, QWidget *parent);

  Kind kind() const;
  QString id() const;
  QString iconName() const;
  void setIconName(const QString &name);
  //! The colour of the icon of a status line (a state: success, warning); the others have the colour of their text
  void setIconColor(deskflow::gui::style::IconColor color);
  bool isCurrent() const;
  void setCurrent(bool current);
  //! In the collapsed rail the label is not drawn, so it is the tooltip
  void setRailCollapsed(bool collapsed);

  //! The room for the ring of the keyboard focus around the capsule
  static constexpr int kRing = 3;
  //! The height of an item at the size of text that the layout was drawn for; itemHeight() is it at the size of text
  //! now
  static constexpr int kHeight = 44;
  static int itemHeight();
  QSize sizeHint() const override;

Q_SIGNALS:
  //! An arrow key, Home or End was pressed: the rail moves the focus
  void navigate(int key);

protected:
  void paintEvent(QPaintEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void focusInEvent(QFocusEvent *event) override;
  void focusOutEvent(QFocusEvent *event) override;
  void changeEvent(QEvent *event) override;

private:
  Kind m_kind;
  QString m_id;
  QString m_iconName;
  deskflow::gui::style::IconColor m_iconColor = deskflow::gui::style::IconColor::Text;
  bool m_current = false;
  bool m_collapsed = false;
  bool m_keyboardFocus = false;
};

/*!
 * \brief The button that folds the sidebar to its rail and unfolds it. For a screen reader it says whether the sidebar
 * is unfolded or folded.
 */
class NavToggleButton : public QAbstractButton
{
  Q_OBJECT

public:
  explicit NavToggleButton(QWidget *parent);

  void setExpandedState(bool expanded);
  bool isExpandedState() const;
  QSize sizeHint() const override;

protected:
  void paintEvent(QPaintEvent *event) override;
  void focusInEvent(QFocusEvent *event) override;
  void focusOutEvent(QFocusEvent *event) override;

private:
  bool m_expanded = true;
  bool m_keyboardFocus = false;
};

/*!
 * \brief The sidebar of the main window: a floating panel with a button that folds it, the pages and actions of the
 * window, and at its foot a few lines of status.
 *
 * It is 236 px wide with the labels, and 76 px as a rail of icons. The width moves in 200 ms (at once when motion is
 * reduced, or when the rail is not shown). The icons stay where they are while it moves, so that only the labels come
 * and go.
 *
 * For the keyboard the toggle button is a stop of its own and all the entries together are one stop: the arrow keys,
 * Home and End move in them, and Enter and Space press the one that has the focus (roving tabindex). A screen reader
 * finds a list named "Sidebar" with the entries in it, the page that is shown as selected.
 */
class NavRail : public deskflow::gui::style::PanelWidget
{
  Q_OBJECT

public:
  enum class Region
  {
    Top,
    Bottom
  };

  explicit NavRail(QWidget *parent = nullptr);

  NavItem *addPage(const QString &id, const QString &iconName, const QString &text, Region region = Region::Top);
  //! A page that comes before the entry \p beforeId (the pages of the settings come before what ends the group)
  NavItem *addPageBefore(const QString &beforeId, const QString &id, const QString &iconName, const QString &text);
  NavItem *addAction(const QString &id, const QString &iconName, const QString &text, Region region = Region::Top);
  NavItem *addStatus(const QString &id, const QString &iconName, const QString &text, Region region = Region::Bottom);
  //! A title for the group of entries that follows. The collapsed rail draws a line for it
  void addSection(const QString &text);
  void addSeparator();

  NavItem *item(const QString &id) const;
  QList<NavItem *> items() const;
  void setItemText(const QString &id, const QString &text);
  void setItemVisible(const QString &id, bool visible);
  void setSectionText(int index, const QString &text);

  QString currentPage() const;
  void setCurrentPage(const QString &id);

  NavToggleButton *toggleButton() const;
  void setToggleTexts(const QString &collapseText, const QString &expandText);

  //! The labels are drawn (true) or only the icons (false)
  bool isExpanded() const;
  void setExpanded(bool expanded, bool animate = true);
  //! The width of the panel now, which is between the two widths while it moves
  int panelWidth() const;
  static int collapsedPanelWidth();
  static int expandedPanelWidth();
  //! The width of the widget for a width of the panel: the room of the shadow comes to it
  static int widgetWidthFor(int panelWidth);

  //! The rail is painted over an opaque surface (the panel is translucent): what is behind it is not read through it
  void setSolid(bool solid);
  bool isSolid() const;

  QSize minimumSizeHint() const override;
  QSize sizeHint() const override;

Q_SIGNALS:
  //! The user pressed an entry (a page, an action)
  void activated(const QString &id);
  //! The user chose another page
  void pageSelected(const QString &id);
  void toggleRequested();
  void panelWidthChanged(int width);
  void expandedChanged(bool expanded);

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  struct Entry
  {
    NavItem *item = nullptr;
    QString section;
    bool separator = false;
    Region region = Region::Top;
    // where it is in the panel, for what is drawn by the rail itself
    QRect slot;
  };

  NavItem *add(NavItem::Kind kind, const QString &id, const QString &iconName, const QString &text, Region region);
  void relayout();
  void applyPanelWidth(qreal width);
  void moveFocus(int key);
  void setFocusStop(NavItem *item);
  QList<NavItem *> focusable() const;

  NavToggleButton *m_toggle = nullptr;
  QList<Entry> m_entries;
  QVariantAnimation m_animation;
  qreal m_panelWidth = 236;
  bool m_expanded = true;
  QString m_current;
  QString m_collapseText = QStringLiteral("Collapse sidebar");
  QString m_expandText = QStringLiteral("Expand sidebar");
  NavItem *m_focusStop = nullptr;
  bool m_solid = false;
};
