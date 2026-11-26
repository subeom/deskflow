# The tray icon and the menus of the new look

This describes the tray icon, the menu of the tray, the menus of the window and the log of the window with the look
Deskflow (the default; the setting `gui/appearance`, or the environment variable `DESKFLOW_APPEARANCE=deskflow`). With the
look System (`gui/appearance=system`, or `DESKFLOW_APPEARANCE=system` for one run) the tray, the menu bar and the log are
what they have always been, except for the three fixes that are listed first: they are bugs, and they apply to both
looks.

The code is in `src/lib/gui/tray/`. Nothing in it needs a window or a screen to be tested, except the builder and the
controller, which need widgets but are tested offscreen (`src/unittests/gui/tray/`).

## Fixes that apply to every look

| Fix | Where | Why |
|---|---|---|
| The window is hidden to the tray only when `QSystemTrayIcon::isSystemTrayAvailable()` says that there is one. Without a tray the window is minimized (the program runs on, the window is in the taskbar or the overview) and the reminder that tells of the tray is not shown | `MainWindow::maybeHideToTray`, `tray::closeActionFor` | GNOME without the extension of the tray hid the window for good: no window and no icon to open it from |
| A click on the tray icon does not show and hide the window on macOS and on GNOME | `MainWindow::connectSlots`, `tray::clickTogglesWindow` | The OS opens the menu on macOS (macOS 27 reports the click as `Unknown`); the extension of GNOME sends a click to the program for a double click only, and a click opens its menu |
| A build with Qt before 6.11.3 says in the log, when the tray starts on macOS 27, that a click on the tray icon can end the program | `MainWindow::setupTrayIcon`, `tray::trayClickCanCrash` | See the next section |

### Qt versions and macOS 27

* Qt before 6.11.3 handles a click on the status item of macOS 27 with an event that is not one of the mouse and ends the
  program (QTBUG-147449). It is fixed in Qt 6.11.3 and 6.12.0 (and not in 6.9.3, 6.10.x or the public 6.8.3). A release
  for macOS 27 needs Qt 6.11.3 or 6.12.0 and later. The continuous integration builds the Apple silicon release with
  Qt 6.12.
* The Intel build (Qt 6.9.3, deployment target 12) does not run on macOS 27 (macOS 27 is for Apple silicon only) and is not
  a target for it. It must also not call an API of macOS 14 or later without asking whether it is there
  (`respondsToSelector`, as `showOSXMenuImages` does).
* macOS 27 hides the picture of a row of a menu unless the row asks for it (`NSMenuItem.preferredImageVisibility`, which
  Qt 6.12 does not set: QTBUG-151046). `showOSXMenuImages(QMenu *)` in `OSXHelpers.mm` sets it, by name, so that an older SDK
  compiles and an older OS is left alone.

## The state of the tray

`TrayState` is `Idle` (no mode is chosen), `Stopped`, `Waiting`, `Connected` or `Error`, made of the state of the core
(`trayStateFor`): the core that runs and waits (starting, a server that listens, a client that connects) is waiting; one
that is connected is connected; one that will try again, that is started and has no connection, or that could not start is an
error.

## The icons

The mark is the wave of the program (the symbolic icon it has always had); the state is a shape that is added to it and
cut out of the waves around it, so that **an icon of one colour tells the state**, and colour is never the only thing that does:

| State | Shape |
|---|---|
| idle | the mark alone |
| stopped | a slash through the mark |
| waiting | a ring, at the lower right |
| connected | a dot, at the lower right |
| error | a triangle with an exclamation mark, at the lower right |
| an update (any state) | a dot at the upper right |

`TrayIcons` draws them (no file per state: the pictures are made at the size and the pixel ratio that the tray asks for) in
three tones:

* `Template` (macOS): black and clear, `QIcon::setIsMask(true)`; macOS tints it. It has no colour, whatever the setting says.
* `OnLightBar` / `OnDarkBar` (Windows, Linux): a dark mark for a light taskbar or panel and a light mark for a dark one. Windows
  does not recolour the icons of the notification area, and the theme of the taskbar is read from the registry as it was
  (`detectTrayTone`).

The setting **Tray icon style** (`gui/symbolicTrayIcon`): *Monocolor* (the default) draws the shapes in the colour of the mark.
*Colorful* draws them in colours (green, blue, red; blue for an update) on Windows and Linux. The shape is the same in both
(a test compares them). On macOS the icon is a template and the setting changes nothing there. With the look System the
setting is what it was: Colorful is the colour logo of the program.

On Linux an icon theme that has an icon of the symbolic name wins over the drawn picture, so that a desktop that tints symbolic
icons by name can: `org.deskflow.deskflow[-stopped|-waiting|-connected|-error][-update]-symbolic` (idle is the name that the
symbolic icon has always had). No such icon is installed by the program yet: the picture is what is sent.

The sizes (`availableSizes`): 16, 20, 22, 24, 32, 48 and 64 pixels; the tests make every state at the sizes of the trays and
check that each two states differ by some pixels (at 16 pixels the two closest, connected and error, differ by 17).

The tool tip says the state in words: `Deskflow: <headline of the status>` and ` (update available)`. It is set only when the
text changes (macOS 27.2 leaves a tool tip on the screen that is set again and again).

## The menu

`TrayMenuModel::build(TrayInput)` makes the content of the menu as a function of the state: a list of rows. The same rows
on every OS, in this order, with a separator between groups and none beside another:

1. **Header**: the state in a few words (the headline of the status of the window) and the sentence behind it; on Windows a
   chip with the state in one word (Running, Waiting, Stopped, Needs attention). Then the external rows (see below).
2. **Commands of the state**: stopped: *Start* (*Connect* for a client); waiting and connected: *Restart* and *Stop*; an error of
   a client: *Test connection* and *Stop*; an error of a server that runs: *Stop*; a core that could not start: *Start*.
3. **Facts**: the version that can be updated to (a row that opens the download), and the computers that are connected
   (`<name>: Connected`), or *No computer has connected yet.* while a server waits.
4. **Window**: *Open Deskflow* (the default row, in bold), *Minimize to tray*, *Preferences*.
5. **Quit**.

The rules of the platforms:

* A row that does not apply is **dimmed** on macOS and on the desktops of Linux, and **left out** on Windows (the convention
  of its notification area). This concerns *Minimize to tray* while the window is not on the screen, and *Start* while no mode
  is chosen. A *Start* that the settings cannot start is dimmed everywhere.
* All the rows of a group have an icon, or none has one.
* The texts of the commands and their shortcuts are those of the actions of the window (translated as they always were); a
  sentence of more than 60 characters is cut and keeps the whole for its tool tip.

### Which part draws what

* `TrayMenuBuilder` makes a `QMenu` of the rows and keeps it: a row that stays is the same `QAction` (a menu that is open changes
  text, not rows). The rows are the builder's own actions, not those of the window (those come and go as they always did); their
  shortcuts are shown (`setShortcutVisibleInContextMenu`) and have the context `Qt::WidgetShortcut`, so they never meet the
  same keys of the window.
* **macOS** draws the menu itself (a native `NSMenu`): the content, the order, the bold row, the icons (templates, so that the
  menu tints them) are what the program decides. The header is two rows of text that cannot be chosen. The painted chip row
  (a `QWidgetAction`) is **not** used: Qt has no keyboard or mouse tracking for such a row, and how VoiceOver reads it is not
  known; the same information is in the text rows (the rule A+ of the plan: only if the review passes).
* **Windows**: Qt draws the menu, so `DeskflowStyle` does (rows of 36 px, a soft accent highlight, icon column, hairline separators,
  the surface of the tokens). The header is a `QWidgetAction` with a painted widget (`TrayHeaderWidget`: the icon of the state,
  the headline, the sentence, a chip) and the text of the row for a screen reader. A menu with round corners on a clear
  ground is only for a person who sets `DESKFLOW_TRAY_MENU_ROUNDED` (a frameless translucent window): how it opens and closes
  on Windows (a click outside, Alt+Tab, the Windows key) is to be seen on a PC, and is **not verified**.
* **Linux**: the desktop draws the menu from what the program sends (labels, icon names, order, separators, enabled state). The
  icons are the names of the icon theme that the rows have always asked for (`TrayMenuModel::themeIconName`).
  A desktop does not show a bold row, a widget or a tool tip.

### The commands

`TrayController::commandRequested(TrayCommand)` goes to `MainWindow::trayCommand`, which calls what the rows of the menu
always called (`startCore`, `resetCore`, `stopCore`, `showConnectionTest`, `showAndActivate`, `hide`, `openSettings`,
`openGetNewVersionUrl`, `close`). *Preferences* opens the pages of the settings in the window of the new look.

### Rows that another feature adds (the hook)

```cpp
m_trayController->setExternalRow(QStringLiteral("tunnel"), text);   // shown under the header
m_trayController->setExternalRow(QStringLiteral("tunnel"), QString()); // taken away
```

A feature with a line of status of its own gives a key that is its own and a text. The row is a text that cannot be chosen,
under the header, in the order in which the keys were first given; it is not there while the text is empty. Nothing else of
the menu changes for it (`TrayMenuModel::setExternalRow`, tested).

**Cloudflare Tunnel** (`feat/cloudflare-tunnel`, `MainWindow::setupTunnel` and `tunnelUsedChanged`) adds a disabled `QAction`
(`m_actionTunnelStatus`) to the tray menu before *Start* (`trayMenu->insertAction(m_actionStartCore, ...)`); its text follows
`TunnelStatusWidget::statusTextChanged` and it is visible while the tunnel is used. When that branch is merged, with the
look Deskflow the action is not put in the menu (the controller owns the rows, and `m_actionStartCore` is not in it) and
both signals go to the hook:

```cpp
// setupTunnel(): instead of insertAction when m_trayController is set
connect(m_tunnelStatus, &TunnelStatusWidget::statusTextChanged, this, [this](const QString &text) {
  if (m_trayController)
    m_trayController->setExternalRow(QStringLiteral("tunnel"), m_tunnelStatus->isTunnelUsed() ? text : QString());
});
// tunnelUsedChanged(bool used):
if (m_trayController)
  m_trayController->setExternalRow(QStringLiteral("tunnel"), used ? m_tunnelStatus->statusText() : QString());
```

With the look System the branch's code is as it is (the action is added before *Start*). No tunnel code or text is in the tray.

## The menus of the window

`DeskflowStyle` paints every menu that Qt draws (the menu bar of a window on Windows and Linux, context menus, the tray menu
on Windows). The menu bar of macOS is the one of macOS.

With the look Deskflow the first menu of the menu bar is a menu of sharing whose rows are all there, dimmed when they do not
apply (`sharingMenuState`): Start, Restart, Stop, Test connection, View local fingerprint, Minimize to tray. On macOS it is
called *Sharing*, and *Preferences* and *Quit* are in it for macOS to take into the menu of the application by their roles
(as *About* is); there is no *Edit* menu. On Windows and Linux it is *File* (with *Quit*) and *Edit* (with *Preferences*).

## The log

With the look Deskflow the log is a card (a dense `PanelWidget`) with its header (title, search, detach, close) and its
lines inside it. The title bar of the dock has no height. The buttons are pictures of the set of the program, with names for a
screen reader.

## Strings that are new

English only (they are for the translation that is made when the strings of the look are frozen):

| Context | Text |
|---|---|
| `deskflow::gui::tray::TrayMenuModel` | `(update available)`, `Running`, `Waiting`, `Stopped`, `Needs attention`, `A new version v%1 is available`, `%1: Connected`, `No computer has connected yet.` |
| `MainWindow` | `&Sharing` (the menu of macOS) |

The texts of the commands, of the status and of the search are the strings that the program already has.

## New settings

None. The environment variable `DESKFLOW_TRAY_MENU_ROUNDED` (Windows, opt-in) is for trying the round menu.

## What is not verified here

The tests run offscreen on macOS. The following need a real tray icon, a real menu or other OS, and are on the lists of
the report of the work: macOS 27 click on the icon (no crash, the menu opens at the first click with the window hidden), the
pictures in the menu (`preferredImageVisibility`), the template icon in a light and a dark menu bar at 1x and 2x, VoiceOver
on the menu, the overflow of the menu bar; on Windows: the open and close of the menu (a click outside, Alt+Tab, the Windows
key), the scales 100 to 200 %, a second monitor, high contrast, the icon in a light and a dark taskbar (and when the theme of
the taskbar changes), Narrator on the header and the rows; on Linux: GNOME with and without the extension, KDE, a desktop
with the XEmbed tray.
