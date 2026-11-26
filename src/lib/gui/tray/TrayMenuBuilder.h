/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "TrayMenuModel.h"

#include <QHash>
#include <QObject>

class QAction;
class QMenu;
class QWidgetAction;

namespace deskflow::gui::tray {

class TrayHeaderWidget;

/*!
 * \brief Makes the QMenu of the tray out of what TrayMenuModel says, and keeps it so.
 *
 * Each row of the model has an action of its own that lives as long as the row is in the menu (the menu that is open
 * changes the text of a row, not the row), and the actions are the ones of the builder, not the shared ones of the window:
 * the window offers and hides its own actions as it always did, and the shortcuts that the menu shows are only shown (their
 * context is the widget, so that they never meet the shortcut of the window with the same keys).
 *
 * The header is a QWidgetAction with a painted widget where Qt draws the menu (Windows), and two rows of text that cannot be
 * chosen elsewhere (the OS draws the menu: macOS, Linux). A command that is chosen is told by commandTriggered().
 */
class TrayMenuBuilder : public QObject
{
  Q_OBJECT

public:
  TrayMenuBuilder(QMenu *menu, const TrayPlatform &platform, QObject *parent = nullptr);

  QMenu *menu() const;

  //! Makes the menu what the spec says: the rows that are there are kept, the others are added or taken away
  void apply(const TrayMenuSpec &spec);

  //! The action of a row (by its id), null when the menu has no such row
  QAction *actionFor(const QString &id) const;
  //! The widget of the header (null where the header is rows of text)
  TrayHeaderWidget *headerWidget() const;

  /*!
   * The menu with a frame of its own: no window frame, a clear ground and a shadow that the style draws, so that the corners
   * are round. Only for Windows, and only as a choice (the environment variable DESKFLOW_TRAY_MENU_ROUNDED), until it is
   * seen to open and close as a menu does on the PC of a person.
   */
  void setRoundedFrame(bool rounded);

Q_SIGNALS:
  void commandTriggered(deskflow::gui::tray::TrayCommand command);

private:
  QAction *rowFor(const TrayMenuEntry &entry);
  void update(QAction *action, const TrayMenuEntry &entry);
  QIcon iconFor(const QString &name) const;

  QMenu *m_menu;
  TrayPlatform m_platform;
  QHash<QString, QAction *> m_actions;
  TrayHeaderWidget *m_header = nullptr;
};

} // namespace deskflow::gui::tray
