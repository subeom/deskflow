/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AccessibleItems.h"

#include <QAccessible>
#include <QAccessibleActionInterface>
#include <QAccessibleEvent>
#include <QAccessibleWidget>
#include <QPointer>
#include <QWidget>

#include <algorithm>

namespace deskflow::gui::style {

namespace {

// One item of the widget: a list item that can be selected, named and described by what the widget says of it. It is read
// from the widget each time that it is asked, so that it is never out of date: only its place in the order is its
// identity
class ItemAccessible : public QAccessibleInterface, public QAccessibleActionInterface
{
public:
  ItemAccessible(QWidget *widget, AccessibleItemSource *source, int index)
      : m_widget(widget),
        m_source(source),
        m_index(index)
  {
  }

  int index() const
  {
    return m_index;
  }

  bool isValid() const override
  {
    return !m_widget.isNull() && m_index < m_source->accessibleItemCount();
  }

  QObject *object() const override
  {
    return nullptr;
  }

  QWindow *window() const override
  {
    return m_widget ? m_widget->windowHandle() : nullptr;
  }

  QAccessibleInterface *parent() const override
  {
    return m_widget ? QAccessible::queryAccessibleInterface(m_widget) : nullptr;
  }

  QAccessibleInterface *child(int) const override
  {
    return nullptr;
  }

  int childCount() const override
  {
    return 0;
  }

  int indexOfChild(const QAccessibleInterface *) const override
  {
    return -1;
  }

  QAccessibleInterface *childAt(int, int) const override
  {
    return nullptr;
  }

  QString text(QAccessible::Text text) const override
  {
    if (!isValid())
      return {};
    switch (text) {
    case QAccessible::Name:
      return m_source->accessibleItemName(m_index);
    case QAccessible::Description:
      return m_source->accessibleItemDescription(m_index);
    default:
      return {};
    }
  }

  void setText(QAccessible::Text, const QString &) override
  {
  }

  QRect rect() const override
  {
    if (!isValid())
      return {};
    const auto inside = m_source->accessibleItemRect(m_index);
    return QRect(m_widget->mapToGlobal(inside.topLeft()), inside.size());
  }

  QAccessible::Role role() const override
  {
    return QAccessible::ListItem;
  }

  QAccessible::State state() const override
  {
    QAccessible::State state;
    if (!isValid())
      return state;
    const bool selected = m_source->accessibleItemSelected(m_index);
    state.selectable = true;
    state.selected = selected;
    state.focusable = true;
    state.focused = selected && m_widget->hasFocus();
    state.disabled = !m_widget->isEnabled();
    state.invisible = !m_widget->isVisible();
    return state;
  }

  void *interface_cast(QAccessible::InterfaceType type) override
  {
    if (type == QAccessible::ActionInterface)
      return static_cast<QAccessibleActionInterface *>(this);
    return nullptr;
  }

  QStringList actionNames() const override
  {
    return {QAccessibleActionInterface::pressAction(), QAccessibleActionInterface::setFocusAction()};
  }

  void doAction(const QString &name) override
  {
    if (!isValid() || !m_widget->isEnabled())
      return;
    // the keys are where the selection is: to put the focus on an item is to select it, as Tab does
    if (name == QAccessibleActionInterface::pressAction() || name == QAccessibleActionInterface::setFocusAction())
      m_source->selectAccessibleItem(m_index);
  }

  QStringList keyBindingsForAction(const QString &) const override
  {
    return {};
  }

private:
  QPointer<QWidget> m_widget;
  AccessibleItemSource *m_source;
  int m_index;
};

// The widget as a list, whose items are what it paints
class ItemListAccessible : public QAccessibleWidget
{
public:
  ItemListAccessible(QWidget *widget, AccessibleItemSource *source)
      : QAccessibleWidget(widget, QAccessible::List),
        m_source(source)
  {
  }

  ~ItemListAccessible() override
  {
    for (const auto id : std::as_const(m_ids))
      QAccessible::deleteAccessibleInterface(id);
  }

  int childCount() const override
  {
    return m_source->accessibleItemCount();
  }

  QAccessibleInterface *child(int index) const override
  {
    if (index < 0 || index >= childCount())
      return nullptr;
    sync();
    return QAccessible::accessibleInterface(m_ids.at(static_cast<size_t>(index)));
  }

  int indexOfChild(const QAccessibleInterface *child) const override
  {
    if (const auto *item = dynamic_cast<const ItemAccessible *>(child); item && item->index() < childCount())
      return item->index();
    return -1;
  }

  QAccessibleInterface *childAt(int x, int y) const override
  {
    const auto at = widget()->mapFromGlobal(QPoint(x, y));
    // the one on top is the last that is read
    for (int i = childCount() - 1; i >= 0; --i) {
      if (m_source->accessibleItemRect(i).contains(at))
        return child(i);
    }
    return nullptr;
  }

  QAccessibleInterface *focusChild() const override
  {
    if (!widget()->hasFocus())
      return nullptr;
    for (int i = 0; i < childCount(); ++i) {
      if (m_source->accessibleItemSelected(i))
        return child(i);
    }
    return nullptr;
  }

private:
  // as many interfaces as there are items: an item is told by its place, so only how many there are matters
  void sync() const
  {
    const auto count = static_cast<size_t>(childCount());
    while (m_ids.size() < count) {
      auto *item = new ItemAccessible(widget(), m_source, static_cast<int>(m_ids.size()));
      m_ids.push_back(QAccessible::registerAccessibleInterface(item));
    }
    while (m_ids.size() > count) {
      QAccessible::deleteAccessibleInterface(m_ids.back());
      m_ids.pop_back();
    }
  }

  AccessibleItemSource *m_source;
  mutable std::vector<QAccessible::Id> m_ids;
};

} // namespace

QAccessibleInterface *createItemListAccessible(QWidget *widget, AccessibleItemSource *source)
{
  return new ItemListAccessible(widget, source);
}

void announceSelectedItem(QWidget *widget, int index)
{
  if (!QAccessible::isActive() || index < 0)
    return;
  for (const auto type : {QAccessible::Focus, QAccessible::SelectionAdd}) {
    QAccessibleEvent event(widget, type);
    event.setChild(index);
    QAccessible::updateAccessibility(&event);
  }
}

} // namespace deskflow::gui::style
