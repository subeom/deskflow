/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QRect>
#include <QString>

class QAccessibleInterface;
class QWidget;

namespace deskflow::gui::style {

/*!
 * \brief What a widget that paints items of its own (the computers of the arrangement canvas, the monitors of the
 * monitor editor) tells a screen reader: a list of items, each with a name, a description, a place and whether it is
 * the selected one. The items are the children of the widget in the tree of accessibility: a list, whose items can be
 * selected.
 *
 * The index of an item is its place in the order that the keys go through them (Tab), which is the order that a screen
 * reader reads them in.
 */
class AccessibleItemSource
{
public:
  virtual ~AccessibleItemSource() = default;

  virtual int accessibleItemCount() const = 0;
  virtual QString accessibleItemName(int index) const = 0;
  virtual QString accessibleItemDescription(int index) const = 0;
  //! Where the item is in the widget, in the coordinates of the widget
  virtual QRect accessibleItemRect(int index) const = 0;
  virtual bool accessibleItemSelected(int index) const = 0;
  //! The screen reader asks to select the item
  virtual void selectAccessibleItem(int index) = 0;
};

//! The accessible interface of a widget that is an \c AccessibleItemSource: a list with the items as its children
QAccessibleInterface *createItemListAccessible(QWidget *widget, AccessibleItemSource *source);

//! Tells the screen readers that the item is now the selected one and where the keys are (when one listens)
void announceSelectedItem(QWidget *widget, int index);

} // namespace deskflow::gui::style
