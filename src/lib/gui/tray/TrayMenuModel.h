/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "TrayState.h"

#include "gui/widgets/StatusChip.h"

#include <QCoreApplication>
#include <QKeySequence>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

namespace deskflow::gui::tray {

//! What a row of the menu that can be chosen does
enum class TrayCommand
{
  Start,
  Restart,
  Stop,
  TestConnection,
  OpenWindow,
  Minimize,
  Preferences,
  OpenUpdate,
  Quit
};

/*!
 * What differs between the OS in the content of the menu. The menu is the same on all of them (the same rows, in the
 * same order); the OS decides how a row that does not apply is shown, and how the header is drawn.
 */
struct TrayPlatform
{
  //! A row that does not apply is left out (the convention of Windows) instead of dimmed (macOS, and the desktops of Linux)
  bool hideInapplicable = false;
  //! The header is one row with a painted widget (Qt draws the menu of Windows); elsewhere it is two rows of text
  bool headerAsWidget = false;
  //! The rows have icons (a desktop that shows none gets none)
  bool icons = true;
  //! The icons are asked of the icon theme by name (the desktop draws the menu): the icons that the program has always
  //! used for these rows. Elsewhere they are the icons of the new look
  bool themeIcons = false;
  //! The icons are templates (black and clear) that the OS tints to the colour of the text of the row (macOS)
  bool templateIcons = false;

  static TrayPlatform windows();
  static TrayPlatform mac();
  static TrayPlatform linux();
  //! The one of the OS that the program runs on
  static TrayPlatform current();
};

//! A row of text that another part of the program adds to the menu (see TrayMenuModel::setExternalRow)
struct TrayExternalRow
{
  QString key;
  QString text;

  bool operator==(const TrayExternalRow &other) const = default;
};

/*!
 * What the menu is made of, as the main window knows it. Nothing in it is a widget: the texts are the ones that the
 * window has in the language of the person (the rows that were in the menu before use their texts), and the model turns
 * them into rows.
 */
struct TrayInput
{
  core::ProcessState process = core::ProcessState::Stopped;
  core::ConnectionState connection = core::ConnectionState::Disconnected;
  bool isServer = true;
  //! A mode is chosen (server or client)
  bool hasMode = true;
  //! The core could not start, until the next state of the core
  bool startFailed = false;
  //! The core can be started with the settings that there are (a client needs the address of the server)
  bool canStart = true;
  //! A client with the address of a server can be tested
  bool canTestConnection = false;
  //! What the status of the window says: the state in a few words, and the sentence behind it
  QString headline;
  QString detail;
  //! The computers that are connected (the server: its clients; the client: the server)
  QStringList computers;
  //! The version that can be updated to, empty if there is none
  QString updateVersion;
  //! The window is on the screen: the menu offers to hide it, and does not offer to show it
  bool windowShown = false;
  QString appName;
  //! The text and the shortcut of every row that is a command, as the actions of the window have them
  QMap<TrayCommand, QString> labels;
  QMap<TrayCommand, QKeySequence> shortcuts;
};

/*!
 * Which rows of the menu of sharing of the menu bar of the window can be chosen (the rows are all there, and those that do not
 * apply are dimmed: the menu bar is not a place where a row comes and goes). It is the same state as the tray has, with
 * whether there is a fingerprint to show.
 */
struct SharingMenuState
{
  bool start = false;
  bool restart = false;
  bool stop = false;
  bool testConnection = false;
  bool fingerprint = false;
  bool minimize = false;

  bool operator==(const SharingMenuState &other) const = default;
};

SharingMenuState sharingMenuState(const TrayInput &input, bool fingerprintAvailable);

//! One row of the menu
struct TrayMenuEntry
{
  enum class Kind
  {
    //! The state in words: the headline and the sentence behind it (and a chip), not to be chosen
    Header,
    //! A row of text that is not to be chosen
    Text,
    //! A row that is chosen
    Command,
    Separator
  };

  Kind kind = Kind::Text;
  //! Stable name of the row ("header", "computer:Desk-PC", "command:stop", ...)
  QString id;
  QString text;
  //! What the row says in a longer form (the whole of a sentence that was cut short)
  QString toolTip;
  TrayCommand command = TrayCommand::Start;
  bool enabled = true;
  //! The row that is done when the icon is double-clicked on the OS that has a default row: in bold
  bool bold = false;
  //! The name of an icon of the set of the program, empty for a row without one
  QString icon;
  QKeySequence shortcut;
  //! The header only: the second line, and the chip that tells the state again
  QString detail;
  QString chipText;
  StatusChip::Kind chipKind = StatusChip::Kind::Neutral;

  bool operator==(const TrayMenuEntry &other) const = default;
};

struct TrayMenuSpec
{
  TrayState state = TrayState::Idle;
  //! What the icon says when the pointer rests on it
  QString toolTip;
  QList<TrayMenuEntry> entries;

  bool operator==(const TrayMenuSpec &other) const = default;
};

/*!
 * \brief The content of the menu of the tray icon, as a function of the state of the program.
 *
 * It is the whole decision (which rows, in which order, which of them dimmed or left out, which texts and icons) and
 * has no widget: TrayMenuBuilder makes a QMenu of what it says. The menu is, from the top:
 *
 *  - the header: the state in a few words and the sentence behind it, and a chip with the state in one word
 *  - the rows that other parts of the program add (setExternalRow), under the header
 *  - the commands of the state: Start (stopped), Restart and Stop (running), Test connection and Stop (the client
 *    could not connect), Start (the core could not start)
 *  - the facts: the version that can be updated to (a row that opens the download), and the computers that are connected,
 *    or that none has connected yet
 *  - Open (the default row, in bold), Minimize to tray, Preferences
 *  - Quit
 *
 * A group with nothing in it leaves out its separator. All the rows of a group have an icon, or none has one (the rows of
 * text of a group of facts are with icons when the group has more than one row).
 *
 * External rows: a feature that has a line of status of its own (the tunnel of Cloudflare, with the text that its widget
 * emits) calls setExternalRow(key, text) with a key that is its own. The row is shown under the header while the text is
 * not empty, and is taken away when it is empty; it cannot be chosen. Nothing else of the menu changes for it.
 */
class TrayMenuModel
{
  Q_DECLARE_TR_FUNCTIONS(deskflow::gui::tray::TrayMenuModel)

public:
  explicit TrayMenuModel(TrayPlatform platform = TrayPlatform::current());

  //! An empty text takes the row away
  void setExternalRow(const QString &key, const QString &text);
  const QList<TrayExternalRow> &externalRows() const;

  const TrayPlatform &platform() const;

  TrayMenuSpec build(const TrayInput &input) const;

  //! The text cut to the most that a row of a menu should have, with the whole of it kept for the tool tip
  static QString elided(const QString &text, int maxChars = 60);

  //! The name in the icon theme of the desktop for an icon of a row (Linux: the desktop draws the menu from names)
  static QString themeIconName(const QString &icon);

private:
  TrayPlatform m_platform;
  QList<TrayExternalRow> m_external;
};

} // namespace deskflow::gui::tray
