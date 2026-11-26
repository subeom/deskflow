/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/style/PanelWidget.h"
#include "gui/widgets/NavRail.h"

#include <QAbstractScrollArea>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QLayout>
#include <QMainWindow>
#include <QMargins>
#include <QRect>
#include <QSizePolicy>
#include <QStringList>
#include <QWidget>

#include <algorithm>

// The rules that a window of the new look keeps at every size that it can have, as a walk of its tree of widgets. It
// says what is wrong by name, so that a test fails with the list and not with a number:
//  - inside:   a visible widget is inside of its parent (what is outside is cut off, or is drawn over something else);
//              a panel is what is seen of it (the rectangle inside of its shadow). What scrolls (the contents of a
//              scroll area) is as large as it needs, and is not asked to be inside.
//  - overlap:  two visible widgets that have the same parent do not cover each other (the ring of the focus of an item
//              of the sidebar is not part of it).
//  - panel:    a panel (a card, the sidebar) is seen whole in the window, whatever holds it (not for what scrolls)
//  - squeezed: a widget has what its minimum size hint asks for (a text that has a place for its words has the height
//              that its width needs), unless it is the one that scrolls, or it has no wish for the size on that axis.
//  - lines:    a text that wraps uses as few lines as the room that it could have allows: a text that has the room to
//  its
//              right (up to the next widget of its row, or the end of its parent) does not wrap while it would fit.
namespace layoutaudit {

struct Options
{
  //! The object names that are over what is below them on purpose (an overlay, a scrim)
  QStringList overlays;
  //! The object names of the widgets that hold panels and have room for their shadows around them: the sidebar shell
  //! crops the shadow of what is at its edge (its margin is smaller than the shadow), and the panel inside is what must
  //! not be cut (see "panel")
  QStringList shadowHolders = {QStringLiteral("shellContent")};
  bool checkSqueezed = true;
  bool checkLines = true;
};

inline QString nameOf(const QWidget *widget)
{
  QStringList chain;
  for (const QWidget *w = widget; w; w = w->parentWidget()) {
    auto part = QString::fromLatin1(w->metaObject()->className());
    if (!w->objectName().isEmpty())
      part += QLatin1Char('#') + w->objectName();
    chain.prepend(part);
    if (chain.size() >= 4)
      break;
  }
  return chain.join(QStringLiteral(" > "));
}

inline QRect seenRect(const QWidget *widget)
{
  if (const auto *panel = qobject_cast<const deskflow::gui::style::PanelWidget *>(widget))
    return panel->panelRect();
  if (qobject_cast<const NavItem *>(widget))
    return widget->rect().adjusted(NavItem::kRing, NavItem::kRing, -NavItem::kRing, -NavItem::kRing);
  return widget->rect();
}

//! A rectangle in the coordinates of the parent
inline QRect seenInParent(const QWidget *widget)
{
  const auto local = seenRect(widget);
  return local.translated(widget->pos());
}

//! The part of a scroll area that the contents are scrolled in
inline bool isViewport(const QWidget *widget)
{
  const auto *area =
      widget->parentWidget() ? qobject_cast<const QAbstractScrollArea *>(widget->parentWidget()) : nullptr;
  return area && area->viewport() == widget;
}

//! What a scroll area scrolls: it is as large as it needs, and is not asked to be inside of what shows it
inline bool isScrolled(const QWidget *widget)
{
  return widget->parentWidget() && isViewport(widget->parentWidget());
}

//! What a control of Qt is made of (the field of a spin box, of a combo box): its size is the control's business
inline bool isInnerPartOfAControl(const QWidget *widget)
{
  const auto *parent = widget->parentWidget();
  return parent && (qobject_cast<const QAbstractSpinBox *>(parent) || qobject_cast<const QComboBox *>(parent) ||
                    qobject_cast<const QAbstractScrollArea *>(parent));
}

inline QList<QWidget *> shownChildren(const QWidget *parent)
{
  QList<QWidget *> shown;
  for (auto *child : parent->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
    if (child->isWindow() || child->isHidden())
      continue;
    // a widget that has no size yet is not drawn
    if (child->width() <= 0 || child->height() <= 0)
      continue;
    shown << child;
  }
  return shown;
}

inline int heightFor(const QWidget *widget, int width)
{
  if (const auto *label = qobject_cast<const QLabel *>(widget))
    return label->heightForWidth(width);
  return widget->heightForWidth(width);
}

/*!
 * The least height that the widget needs at the width, for the rule "has what its minimum asks for". A text that wraps
 * needs its lines. A widget that holds others needs what its layout needs at least: heightForWidth() is the height that
 * the layout would like, in which a part that does not wrap (a page that scrolls) counts with its size hint and not
 * with its minimum, and that is more than the widget has to be given.
 */
inline int minimumHeightFor(const QWidget *widget, int width)
{
  const int wanted = heightFor(widget, width);
  if (qobject_cast<const QLabel *>(widget))
    return wanted;
  if (const auto *layout = widget->layout(); layout && layout->hasHeightForWidth()) {
    const int liked = layout->heightForWidth(width);
    const int least = layout->minimumHeightForWidth(width);
    if (liked >= 0 && least >= 0 && least < liked)
      return wanted - (liked - least);
  }
  return wanted;
}

inline void walk(const QWidget *parent, const Options &options, QStringList &found)
{
  const auto children = shownChildren(parent);
  const auto bounds = seenRect(parent);

  for (const auto *child : children) {
    if (options.overlays.contains(child->objectName()))
      continue;
    const auto rect = seenInParent(child);
    const auto allowed = options.shadowHolders.contains(child->objectName())
                             ? deskflow::gui::style::PanelWidget::shadowMargins()
                             : QMargins();
    if (!isScrolled(child) && !bounds.marginsAdded(allowed).contains(rect)) {
      found << QStringLiteral("inside: %1 is %2 in a parent of %3")
                   .arg(
                       nameOf(child),
                       QStringLiteral("%1,%2 %3x%4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height()),
                       QStringLiteral("%1x%2").arg(bounds.width()).arg(bounds.height())
                   );
    }
  }

  for (int i = 0; i < children.size(); ++i) {
    if (options.overlays.contains(children[i]->objectName()))
      continue;
    const auto a = seenInParent(children[i]);
    for (int j = i + 1; j < children.size(); ++j) {
      if (options.overlays.contains(children[j]->objectName()))
        continue;
      const auto b = seenInParent(children[j]);
      if (a.intersects(b)) {
        const auto both = a.intersected(b);
        found << QStringLiteral("overlap: %1 and %2 cover %3x%4 of each other")
                     .arg(nameOf(children[i]), nameOf(children[j]))
                     .arg(both.width())
                     .arg(both.height());
      }
    }
  }

  if (options.checkSqueezed) {
    for (const auto *child : children) {
      if (qobject_cast<const QAbstractScrollArea *>(child) || isViewport(child) || isInnerPartOfAControl(child) ||
          options.overlays.contains(child->objectName()))
        continue;
      const auto minimum = child->minimumSizeHint();
      const auto policy = child->sizePolicy();
      QString lack;
      if (minimum.width() > child->width() && policy.horizontalPolicy() != QSizePolicy::Ignored &&
          !(child->minimumWidth() > 0 && child->minimumWidth() <= child->width()))
        lack += QStringLiteral(" width %1 of %2").arg(child->width()).arg(minimum.width());
      const bool wrapped = child->hasHeightForWidth();
      const int needed = wrapped ? minimumHeightFor(child, child->width()) : minimum.height();
      if (needed > child->height() && policy.verticalPolicy() != QSizePolicy::Ignored &&
          !(child->minimumHeight() > 0 && child->minimumHeight() <= child->height()))
        lack += QStringLiteral(" height %1 of %2").arg(child->height()).arg(needed);
      if (!lack.isEmpty())
        found << QStringLiteral("squeezed: %1 has less than it asks for:%2").arg(nameOf(child), lack);
    }
  }

  if (options.checkLines) {
    for (const auto *child : children) {
      const auto *label = qobject_cast<const QLabel *>(child);
      if (!label || !label->wordWrap() || label->text().isEmpty())
        continue;
      // the room that the text could have: to the next widget of its row, or to the end of its parent
      int right = parent->rect().right();
      if (const auto *layout = parent->layout())
        right = parent->rect().right() - layout->contentsMargins().right();
      const auto mine = child->geometry();
      for (const auto *other : children) {
        // a widget at the right, in whatever line: the text is in a column that the layout shares with it
        if (other != child && other->geometry().left() >= mine.right())
          right = std::min(right, other->geometry().left() - 1);
      }
      const int room = right - mine.left() + 1;
      // the text column of a row keeps a gutter between it and the end of the row (the gap to a control): the room that
      // is less than that is not room that the text has
      constexpr int kGutter = 24;
      if (room - mine.width() > kGutter) {
        const int linesNow = heightFor(label, mine.width());
        const int linesWithRoom = heightFor(label, room);
        if (linesNow > linesWithRoom)
          found << QStringLiteral("lines: %1 is %2 wide and has %3 to its right: %4 px high now, %5 px with the room")
                       .arg(nameOf(label))
                       .arg(mine.width())
                       .arg(room)
                       .arg(linesNow)
                       .arg(linesWithRoom);
      }
    }
  }

  for (const auto *child : children)
    walk(child, options, found);
}

/*!
 * The window of a Mac as Qt makes it, offscreen, which has no title bar to take room: its Qt gives the window the safe
 * area (the title bar that is under the content) as contents margins, so that the layout is given the window without
 * the strip, and the minimum size that Qt works out leaves the strip out (QLayout::totalMinimumSize reads only the
 * margins that the program set). Both are done here the way Qt does them, after the window was made and before it is
 * shown. The window then has the size that a Mac gives it at the least.
 */
inline void emulateMacTitleBar(QMainWindow &window, int inset)
{
  window.setContentsMargins(0, inset, 0, 0);
  window.show();
  QCoreApplication::processEvents();
  const auto honest = window.minimumSizeHint();
  window.layout()->setSizeConstraint(QLayout::SetNoConstraint);
  window.setMinimumSize(std::max(window.minimumWidth(), honest.width()), honest.height() - inset);
}

/*!
 * The minimum size that a Mac gives the window now: what the content asks for, without the strip. The layout does it by
 * itself when the content changes (a banner that comes), and the emulation above has switched that off, so a test that
 * changes the content says it again here.
 */
inline void emulateMacMinimumSize(QMainWindow &window, int inset)
{
  QCoreApplication::processEvents();
  const auto honest = window.minimumSizeHint();
  window.setMinimumSize(std::max(window.minimumWidth(), honest.width()), honest.height() - inset);
}

//! A panel that is not scrolled is seen whole in the window
inline void walkPanels(const QWidget &root, QStringList &found)
{
  const QRect window = root.rect();
  for (const auto *panel : root.findChildren<deskflow::gui::style::PanelWidget *>()) {
    if (!panel->isVisibleTo(&root) || panel->width() <= 0)
      continue;
    bool scrolled = false;
    for (const QWidget *w = panel; w && w != &root; w = w->parentWidget())
      scrolled = scrolled || isScrolled(w);
    if (scrolled)
      continue;
    const auto seen = QRect(panel->mapTo(&root, panel->panelRect().topLeft()), panel->panelRect().size());
    if (!window.contains(seen)) {
      found << QStringLiteral("panel: %1 is %2 in a window of %3")
                   .arg(
                       nameOf(panel),
                       QStringLiteral("%1,%2 %3x%4").arg(seen.x()).arg(seen.y()).arg(seen.width()).arg(seen.height()),
                       QStringLiteral("%1x%2").arg(window.width()).arg(window.height())
                   );
    }
  }
}

//! Every violation of the rules in the tree below (and including the contents of) the widget
inline QStringList audit(const QWidget &root, const Options &options = {})
{
  QStringList found;
  walk(&root, options, found);
  walkPanels(root, found);
  return found;
}

} // namespace layoutaudit
