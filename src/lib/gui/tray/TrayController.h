/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "TrayIcons.h"
#include "TrayMenuModel.h"

#include <QObject>

#include <optional>

class QMenu;
class QSystemTrayIcon;

namespace deskflow::gui::tray {

class TrayMenuBuilder;

/*!
 * \brief The tray icon, its tool tip and its menu of the new look, kept as the state of the program says.
 *
 * The main window gives it what it knows (setInput) each time that it changes; the controller makes the content of the menu
 * (TrayMenuModel), puts it into the menu (TrayMenuBuilder), chooses the icon of the state (TrayIcons) and the tool tip, and
 * says which command a person chose from the menu (commandRequested). Only a window that has the style layer makes one: a
 * window of the system look keeps the menu that it has always had.
 *
 * A feature that has a line of status for the menu (the tunnel of Cloudflare) gives it with setExternalRow(): see
 * TrayMenuModel. Rows that are told in the same turn of the event loop are made once.
 */
class TrayController : public QObject
{
  Q_OBJECT

public:
  TrayController(QSystemTrayIcon *icon, QMenu *menu, const TrayPlatform &platform, QObject *parent = nullptr);

  void setInput(const TrayInput &input);
  //! The text of a row that a feature adds under the header, with a key that is its own; an empty text takes it away
  void setExternalRow(const QString &key, const QString &text);
  //! The icon is drawn with the colour of the shapes (the setting "Tray icon style" is Colorful), or in one colour
  void setColoredIcon(bool colored);
  //! The theme of the taskbar or of the palette is another: the icon is chosen again
  void refreshIcon();
  //! The tone of the icon is the one that the OS asks for, unless another is asked (the pictures of every tone, the tests)
  void setTone(std::optional<TrayTone> tone);

  const TrayMenuSpec &spec() const;
  TrayState state() const;
  const TrayIconSpec &iconSpec() const;
  TrayMenuBuilder *builder() const;
  TrayMenuModel &model();

Q_SIGNALS:
  void commandRequested(deskflow::gui::tray::TrayCommand command);

private:
  void refresh();
  void applyIcon();

  QSystemTrayIcon *m_icon;
  QMenu *m_menu;
  TrayMenuModel m_model;
  TrayMenuBuilder *m_builder;
  TrayInput m_input;
  TrayMenuSpec m_spec;
  TrayIconSpec m_iconSpec;
  bool m_colored = false;
  std::optional<TrayTone> m_tone;
  bool m_hasInput = false;
  QString m_toolTip;
};

} // namespace deskflow::gui::tray
