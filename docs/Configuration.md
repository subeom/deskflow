# GUI Config

 Deskflow will automatically figure out where to save settings and other files.
 If two computers cannot reach each other, see [Connecting across networks](connecting-across-networks.md).

## Search paths

Deskflow will look for settings in several places depending on your operating system.
The search order for a setting file depends on your operating system

### Linux

  1. `<XDG_CONFIG_HOME>/Deskflow/Deskflow.conf`
  2. `~/.config/Deskflow/Deskflow.conf`
  3. `/etc/Deskflow/Deskflow.conf`
 
A new settings file will be created in the user path if no settings file is found.
The path of the settings file will be used as the base for all other config files.

### macOS
 
  1. `~/Library/Deskflow/Deskflow.conf`
  2. `/Library/Deskflow/Deskflow.conf`
 
A new settings file will be created in the user path if no settings file is found.
The path of the settings file will be used as the base for all other config files.

### Windows

  1. `<install-path>/settings/Deskflow.conf`
  2. Windows Registry `HKCU\Software\Deskflow\Deskflow`

Windows will save to the install dir if settings are loaded from there. If not, it saves any other config files in: `C:\ProgramData\Deskflow\`

When using settings from the install dir, the service mode will not be available.

## Valid GUI Keys

The GUI config file contains several sections.
Each section is formatted the same.
Option-value pairs are only written if the value is not the default value.

```
[section]
option=value
```

### Client

This section contains options used when in client mode. 
It will begin with `[client]`

| Option                    |    Valid Values    | Description |
|:--------------------------|:------------------:|:-----------|
| connectTimeout            | Number 1 - 60      | Seconds the client waits for one connection attempt to finish: the TCP connection, the secure (TLS) handshake and the server's greeting together. Raise it on a slow or far away network. A value outside 1 - 60 or that is not a number is ignored with a warning and the default is used [default: 2] |
| dynamicConnectionInterval | `true` or `false`  | Use dynamic connection retry times based on number of previously failed attempts [default: false] |
| languageSync              | `true` or `false`  | Sync to server language [default: true] |
| remoteHost                | `IP` or `hostname` | The remote host(s) to connect to. Use a comma separated list when you want to try several hosts, for example `192.168.1.5,100.64.1.7` (LAN address first, then the VPN address). They are tried in this order, one at a time: a host that does not answer is given `connectTimeout` seconds plus a 1 second pause before the next one is tried, and when the last one failed the list starts again with the first. Each entry can have its own port, `host:port` or `[ipv6]:port` |
| yScrollScale              | Double 0.1 - 10.0  | Vertical mouse scrolling will be scaled by this amount on the client [default: 1.0] |
| xScrollScale              | Double 0.1 - 10.0  | Horizontal mouse scrolling will be scaled by this amount on the client [default: 1.0] |
| invertYScroll             | `true` or `false`  | Invert vertical scroll on this client [default: false] |
| invertXScroll             | `true` or `false`  | Invert horizontal scroll on this client [default: false] |

### Core

This section contains general options it will begin with `[core]`

|Option         | Valid Values|Description|
|:--------------|:-----------:|:-----------|
| coreMode      | `0` or `1` or `2` | The mode to start in 0: None, 1: Client, 2: Server [default: 0]|
| display       |  int              | The XWindow display to use [default: autodetected] |
| interface     | IP Address        | Preferred IP to use for network communication. By default the server board casts on any available address |
| lastVersion   | M.m.p.t           | The version last run used for checking for updates |
| port          | port #            | Port to use when connecting [default: 24800 |
| preventSleep  | `true` or `false` | Prevent sleep when Deskflow is active [default: false] |
| processMode   | `1` or `0`        | The mode we use to start the process Service or Desktop |
| computerName  | string            | Name used to identify the computer [default: machine's hostname] |
| useHooks      | `true` or `false` | If Windows uses hooks or not [default: true] |
| language      | 639 language      | The language to display the GUI in [default: en] |
| enableEnterCommand | `true` or `false` | Should the enter command be triggered when the computer is entered [defaut: false] |
| enterCommand  | command | A command to run when the computer is entered. |
| enableExitCommand | `true` or `false` | Should the exit command be triggered when the computer is exited [defaut: false] |
| exitCommand  | command | A command to run when the computer is exited. |

### Daemon

This section contains options used by the daemon on windows it will begin with `[daemon]`

|Option | Valid Values|Description|
|:----------|:-----------:|:-----------|
| elevate   | `true` or `false` | Elevate the daemon app [default: true unless portable mode ] |
| logFile   | Filepath          | Filepath of the daemon log |
| logLevel  | valid log Level,  | Log Level  |

### GUI

This section contains options used by the GUI it will begin with `[gui]`

|Option                          | Valid Values      |Description|
|:-------------------------------|:-----------------:|:-----------|
| autoHide                       | `true` or `false` | When true the app will hide itself on start up [default: false] |
| enableUpdateCheck              | `true` or `false` | When true check the update URL to see if a new version was released on start up [default: false] |
| closeReminder                  | `true` or `false` | Used to track if we have shown the reminder that when you close the app it remain running in the background  [default: true]|
| closeToTray                    | `true` or `false` | When `true` the gui will run in the systemTray when its closed [default: true] |
| logExpanded                    | `true` or `false` | Should the log section of the GUI be opened [default: false] |
| symbolicTrayIcon               | `true` or `false` | When true use the monocolor (symbolic) icon false uses a colorful icon for the tray [default: true] |
| showGenericClientFailureDialog | `true` or `false` | When `true` client connection errors will not show popup error messages [default: true] |
| shownFirstConnectedMessage     | `true` or `false` | When `true` GUI has shown the user the message for connecting the first time [default: false] |
| shownServerFirstStartMessage   | `true` or `false` | When `true` GUI has shown the user the Deskflow server is now running message [default: false] |
| shownVerionInTitle             | `true` or `false` | When `true` GUI will include the version in the window title [default: false] |
| startCoreWithGui               | `true` or `false` | When true the Core will be started with the GUI. It is set to the Core's state on exit. |
| updateCheckUrl                 | URL               | The URL to use when checking for a new version number, it should return a version [default: https://api.deskflow.org/version]|

### Log

This section contains options used by the application logging it will begin with `[log]`

|Option    |    Valid Values   |Description|
|:---------|:-----------------:|:-----------|
| file     | Filepath          | The file to write the log into |
| level    | Valid log level   | Log level to use |
| toFile   | `true` or `false` | When true the log will be written to the value of the `file` option |
| guiDebug | `true` or `false` | When true the log will show the Gui's internal debug messages |

### Security

This section contains options used by the application security it will begin with `[security]`

|Option                 |   Valid Values    |Description|
|:----------------------|:-----------------:|:-----------|
| checkPeerFingerprints | `true` or `false` | When true peers will have their fingerprints confirmed by the user and stored [default: true] |
| certificate           | Filepath          | Path to the certificate to use to encrypt messages.|
| keySize               | `2048` OR `4096`  | Size of the TLS key to use [default: 2048]| 
| tlsEnabled            | `true` or `false` | Are we using TLS encryption when communicating [default: true].|

### Server

This section contains options used when in server mode it will begin with `[server]`

|Option              |    Valid Values   |Description|
|:-------------------|:-----------------:|:-----------|
| clipboardSize      | int > 0           | Deskflow will send a maximum of `N` megabytes of clipboard data to another computer when the mouse transitions to that computer. Clients use the server's value for both sending and receiving.|
| clipboardTextOnly  | `true` or `false` | When `true` only plain text of up to 10 KB is shared, see [Share text only](#share-text-only-up-to-10-kb). `clipboardSize` is not used then. Needs `enableClipboard`. (default: false)|
| defaultLockToComputerState| `true` or `false` | When this is true the cursor is locked to the new computer when switching (default: false)|
| disableLockToComputer| `true` or `false` | If false pressing scroll lock will toggle your cursor to be locked to current computer. (default: false) |
| enableClipboard    | `true` or `false` | When `true` the clipboard will be shared with all clients If set to ''true'' then clipboard shared and the ''clipboardSharingSize'' setting will be used. If set to false, then clipboard sharing will be disabled and the the ''clipboardSharingSize'' setting will be ignored.|
| enableHeartbeat    | `true` or `false` | Send a heartbeat to connected clients; this has been replaced by internal keep alive (default: false)|
| enableSwitchDelay  | `true` or `false` | Switching will be delayed by the set value (default: false)|
| enableSwitchDoubleTap  | `true` or `false` | Enables the doubletap to switch method (default: false)|
| experimentalMultiMonitor | `true` or `false` | Experimental: map the pointer around the holes of the desktop of the computers that have a `monitorLayout`, see [Experimental: multi-monitor aware cursor mapping](#experimental-multi-monitor-aware-cursor-mapping). `false` behaves exactly as before. (default: false)|
| externalConfig     | `true` or `false` | When true use the external config path |
| externalConfigFile | Filepath          | Path the server config file if it does not exist the GUI will it generated based on the `internalConfig` section.|
| gridHeight         | int               | Height of the server's intenal grid used for the computer layout (default: 3)|
| gridWidth          | int               | Width of the server's intenal grid used for the computer layout (default: 5) |
| heartbeat          | int               | The server will expect each client to send a message no less than every `N` milliseconds. If no message arrives from a client within `3N` seconds the server forces that client to disconnect. If deskflow fails to detect clients disconnecting while the server is sleeping or vice versa, try using this option.|
| protocol           | `barrier` or `synergy` | The protocol to use when saying hello to clients. Can be set to barrier or synergy. If not set barrier is used as the default |
|relativeMouseMoves  | `true` or `false` | If set to ''true'' then secondary computers move the mouse using relative rather than absolute mouse moves when and only when the cursor is locked to the computer (by ''Scroll Lock'' or a configured hot key). This is intended to make Deskflow work better with certain games. If set to ''false'' or not set then all mouse moves are absolute.|
| switchDelay        | int               | Deskflow won't switch computers when the mouse reaches edge of a computer unless it stays on the edge for `N` milliseconds. This helps prevent unintentional switching when working near an edge. (default: 250)|
| switchDoubleTap    | int               | Deskflow won't switch computers when the mouse reaches the edge of a computer unless it's moved away from the edge and then back to the edge within `N` milliseconds. With the option you have to quickly tap the edge twice to switch. This helps prevent unintentional switching when working near the edge.|
| useArrangement     | `true` or `false` | Experimental: the links of the server configuration are made from the arrangement of the computers (where their screens touch), and the server maps the pointer around the holes of its own screen, see [Experimental: arrangement of the computers](#experimental-arrangement-of-the-computers). Only has an effect while `experimentalMultiMonitor` is on. (default: false)|
|win32KeepForeground | `true` or `false` | If set to ''true'' (the default), Deskflow will grab the foreground focus on a Windows server (thereby putting all other windows in the background) upon switching to a client. If set to ''false'', it will leave the currently foreground window in the foreground. Deskflow grabs the focus to avoid issues with other apps interfering with Deskflow's ability to read the hardware inputs. |

 - You can use both the ''switchDelay'' and ''switchDoubleTap'' options at the same time. Deskflow will switch when either requirement is satisfied.

#### Share text only (up to 10 KB)

A clipboard is sent whole, with every format it holds (text, HTML, images), over the connection that also carries the mouse and the keyboard, so a large clipboard can make switching slow or even drop the connection. Setting `clipboardTextOnly` to `true` (**Share text only (max 10 KB)** in **Server Configuration > Advanced > Misc**, under **Enable clipboard sharing**) shares plain text of up to 10 KB and nothing else:

- Only the plain text is shared. Images, formatted (HTML) text and every other format are not, and neither is a clipboard that holds no text.
- The limit is 10 KB = 10 × 1024 = 10240 bytes of the clipboard as it is sent: the UTF-8 text plus 12 bytes that every clipboard message carries. The longest text that is shared is therefore **10228 bytes**; a text of exactly 10240 bytes is not shared. These are bytes, not characters: a Hangul syllable takes 3 bytes and an emoji 4. The computers that send their own clipboard to the server compare the same number with `>=`, so from them the longest text is 10227 bytes.
- Text that is too long is never cut short, because a cut-off text is a damaged text: it is not shared at all. The server reads a clipboard again on every switch to another computer, so it logs at INFO level that a clipboard was not shared only when it differs from the one it reported last (it is not shared for another reason, or its text has another size), and only for the clipboard, not for the selection, which Windows and macOS read from the same pasteboard. The same one again is logged at DEBUG level. An empty clipboard cannot be told from one that holds only an image, because the server does not look at the formats it does not share, so it is reported once like the others.
- `clipboardSize` is not used while this is on: the box is greyed out and replaced by the fixed limit. The limit the server gives to its clients (the `CLSZ` option, in kilobytes) is 10 KB whatever `clipboardSize` is, so a `clipboardSize` of 0 does not keep text sharing off. With the setting off, nothing changes: the clipboard is shared and limited exactly as before, and a `clipboardSize` of 0 still turns clipboard sharing off.

The server applies the rule itself in both directions and does not rely on the computers it serves. Its own clipboard is reduced to the text before it is sent to a client, and it does not read the HTML and image formats of its own clipboard at all. The clipboard a client sends is reduced to the text, or dropped, before the server keeps it or passes it on to the other clients.

The other computers need **no new build and no setting**: the existing `CLSZ` option already makes a client refuse to send a clipboard larger than the limit. The server sends it when a client connects and whenever it applies its configuration, and saving the **Server Configuration** dialog restarts the server, which is when the setting takes effect.

What this does not do:

- A client that has no such setting still reads and sends every format of its own clipboard when the whole of it is under the limit, and the server keeps the text. A clipboard whose formats together are over 10 KB (formatted text that is long, for example) is not sent by the client at all, even if its plain text alone would fit.
- Copying something that is not shared still empties the clipboards of the other computers, as any copy does, so there is nothing to paste there until something that is shared is copied.
- Sending each clipboard once rather than twice and pacing the transfer are not part of this setting; they are done for every clipboard, see `docs/dev/clipboard-transfer.md`.

#### Leave one computer out of clipboard sharing

Clipboard sharing can be turned off for a single computer, for example one whose clipboard is rewritten by another program all the time (**Share clipboard with this computer** in **Computer settings**, `shareClipboard` in its `[computer_...]` section). The server then does not tell that computer about a copy, does not send it a clipboard, ignores what the computer copies, and tells it to turn its own clipboard sharing off. The other computers keep sharing among themselves. Nothing is sent for a computer that shares (the default), so an existing configuration behaves as before.

#### Experimental: multi-monitor aware cursor mapping

Deskflow treats a computer as one rectangle, the bounding box of its monitors. When the monitors do not make up a rectangle (monitors of different heights side by side, an L shape, a portrait monitor next to a landscape one) the box has *holes*: a pointer that comes in across the edge of a hole is put on nothing, and the operating system moves it somewhere else, and a pointer that is pushed against a monitor edge that is not an edge of the box has to be pushed through the whole hole before the cursor goes on to the next computer (up to a screen width of "dead" travel). If all of your computers are rectangles, or no link leads to the side of a hole, this does nothing for you.

With this on, the server is given the monitors of a computer by you, and then puts the pointer on a monitor when it comes in, lets it leave through any exposed edge, including the inner edges of an L, and moves it over the monitors, not into the holes. It is **experimental and off by default**; off, and for every computer that has no layout, Deskflow behaves exactly as before, and **nothing about it is sent to a computer**: the Windows and macOS clients you have need no new build.

**Turn it on** with **Experimental: multi-monitor aware cursor mapping** in **Server Configuration > Advanced** (`experimentalMultiMonitor` in `[server]`), then give a computer its monitors in **Computer settings > Monitor layout** (`monitorLayout` in `[computer_<name>]`). The text field is available only while the check box is on.

**The text** is the monitors of the computer as `x,y,w,h`, separated by `;` or by lines: the position of the top left corner and the size of each monitor, **in the coordinates that the computer reports its screen in**: physical pixels of the virtual desktop on Windows (the origin is the top left of the primary monitor, a monitor on its left or above it has a negative position), points on macOS. Spaces, empty items, a `;` at the end and Windows line ends are all right. It is checked as you type: 1 to 16 monitors, a width and a height greater than 0, none overlapping, all in one connected shape (they have to share part of an edge: monitors that only touch at a corner, or have a gap between them, are refused), and numbers that a screen size can have (-32768 to 32767). Monitors that are exactly the same (a mirrored display) count once. A layout that is not valid is not saved, and the line under the field says what is wrong with it.

```
0,0,2560,1440
-1920,360,1920,1080
```

**The box has to fit.** The smallest rectangle that has all of the monitors in it has to be exactly the screen size the computer reports to the server (the server log says which it is when it does not fit: `monitor geometry off for "PC": the monitor layout does not match the screen size the computer reported (layout ..., reported ...)`). If it is not, the layout is not used, and the computer is mapped as it always was; it is used again when the sizes are the same again, so a resolution or monitor change cannot leave a stale layout in use. A layout whose monitors make up a rectangle changes nothing, and is not used.

**Capturing a layout.** The layout can be written by hand from the display settings of the computer (the position and the size of each monitor in pixels, with the primary monitor at 0,0). The diagnostic helpers that were written for this feature print it, with the holes of each edge, as a block to paste, and **Import from diagnostic file...** (or pasting the block, with the `computer:` and `space:` lines and `scale=` and `primary` after the numbers) reads it; a block that has a monitor that is wrong is put in the field to be put right. They are not part of this repository, so these are the commands:

- **macOS** (needs the Xcode command line tools): this prints the monitors in points, which is what Deskflow uses on a Mac:

  ```
  cat > /tmp/monitors.swift <<'EOF'
  import CoreGraphics
  var ids = [CGDirectDisplayID](repeating: 0, count: 16)
  var count: UInt32 = 0
  CGGetActiveDisplayList(16, &ids, &count)
  for id in ids.prefix(Int(count)) {
    let r = CGDisplayBounds(id)
    print("monitor \(Int(r.origin.x)),\(Int(r.origin.y)),\(Int(r.size.width)),\(Int(r.size.height))")
  }
  EOF
  swiftc -O /tmp/monitors.swift -o /tmp/monitors && /tmp/monitors
  ```

  It only reads the display list. A display that mirrors another has the same rectangle and counts once.
- **Windows**: the helper script `mmdiag-windows.ps1` reads the virtual desktop in physical pixels and prints the block (`powershell -NoProfile -ExecutionPolicy Bypass -File .\mmdiag-windows.ps1 -OutFile "$env:USERPROFILE\Desktop\mmdiag.txt"`). It has not been run on Windows yet. Without it, work the numbers out from **Settings > System > Display**: the size of each monitor in pixels, with the primary monitor at 0,0 and the others placed by how they are lined up (a monitor on the left of the primary one has a negative x, one above it a negative y). Windows does not show positions as numbers, and at a scaling above 100 % the arrangement is drawn smaller than the pixels, so check the box of your numbers against the size that the server log gives for the computer (the log line above) and correct them until it fits.

**Example.** A Windows PC with a 2560×1440 primary monitor and a 1920×1080 monitor on its left, with the bottoms of both on one line, so that the left one starts 360 pixels lower. The PC reports its screen as `-1920,0 4480x1440`, and the layout is

```
-1920,360,1920,1080
0,0,2560,1440
```

Say the PC is on the right of a MacBook with a 1728×1117 screen, so that the pointer goes from the right edge of the Mac to the left edge of the PC. Above the left monitor (the first 360 rows of the PC) the left edge of the PC's box is a hole. Without the layout the pointer enters at `-1920,129` when it leaves the Mac 100 pixels from the top, which is on nothing, and Windows moves it. With the layout it enters at `0,129`, on the left edge of the primary monitor. From 360 rows down, at 558 pixels it enters at `-1920,720` on the left monitor, with and without the layout. And a pointer that is pushed to the left on the primary monitor above the left monitor goes to the Mac as soon as it reaches the left edge of the primary monitor, not 1920 pixels later at the edge of the box.

**Link intervals** (`left(0,75) = PC(0,75)`, in an external config file) are still percentages of the side of the bounding box, for a computer that has a layout as for any other, so they do not shift when a layout is set. What differs is the pixel that a percentage leads to: the outermost monitor pixel of the row or column at that percentage, which, below a smaller monitor, is the inner edge of the monitor that is there and not the edge of the box.

**When it takes effect.** The graphical interface and the core share the settings file. Saving the server configuration dialog restarts a running core, which reads the check box and every layout again. After a settings file that was edited by hand, restart the core (reloading the configuration reads them too). In the settings file the layout is `monitorLayout="0,0,2560,1440;-1920,360,1920,1080"`, **in double quotes** when there is more than one monitor, as the dialog writes it: a settings file takes a `;` that is not in quotes as the start of a comment, and what comes after it is gone, so only the first monitor would be read (and the log would say that the layout does not match the screen size). A line break in a quoted value is `\n`. The core logs one INFO line each time what is decided for a computer changes, `monitor geometry on for "PC": 2 monitors, -1920,0 4480x1440` or `monitor geometry off for "PC": <why>`, and, while the experiment is on, one at every switch of computers with the shape of both, the edge and its fraction, and the pixel the pointer enters at.

**What it does not do.** The client does not report its monitors; you give them to the server. The server's own screen is not mapped by a layout (a layout set for it is ignored, and the log says so): its displays come from the operating system, with [the arrangement of the computers](#experimental-arrangement-of-the-computers). A layout is checked against the size of the box, so an arrangement that keeps the box but moves monitors in it is not noticed. Monitors are not screens of their own and cannot be linked one by one. Mouse speed is not changed. The design, the limits and the tests are in [docs/dev/multi-monitor.md](dev/multi-monitor.md).

#### Experimental: arrangement of the computers

The server configuration dialog has a grid of cells: a computer is in a cell, its neighbours are the computers in the cells next to it, and a computer is linked to the whole side of the next one whatever the size of its screen. With this you place the computers the way the displays are arranged in the macOS settings, in **real proportion** and **on one canvas**, and the links are **made from where the screens touch**. It builds on [Experimental: multi-monitor aware cursor mapping](#experimental-multi-monitor-aware-cursor-mapping) and is **experimental and off by default**; off, or with no arrangement saved, the classic grid, the server configuration file and the pointer behave exactly as before, and **nothing is sent to any computer**: the Windows and macOS clients you have need no new build.

**Use it.**

1. Turn on **Experimental: multi-monitor aware cursor mapping** in **Server Configuration > Advanced** (it is the master switch), and **Save**.
2. In the **Computers** tab, under the classic grid, press **Use the arrangement**. The computers are put where the classic grid has them, with the size of their screens, and the canvas takes the place of the grid. **Use the classic grid** brings the grid back (the arrangement stays saved and is used again when you choose it).
3. **Drag a computer to where its screen is.** It moves as a unit and snaps to the edges of the others: edges that touch, edges that are in line, middles that are in line (pink guides, like the displays arrangement of macOS); it cannot be put on another computer. The arrow keys move the selected computer by 1 unit (**Shift**: 10), the wheel zooms, a drag on the background pans, **Zoom to fit** shows everything.
4. The **green lines** are the edges that touch: that is where the pointer goes from one computer to the other. Where a computer is bigger than its neighbour, the link covers only the part of the edge that touches. A computer that nothing touches is marked, because the pointer cannot get there; computers on top of each other are an error, and the configuration is not saved until they are not.
5. **Double click** a computer (or select it and press **Edit monitors...**) to **draw its monitors with the mouse** (see below); **Edit computer...** has its other settings, and its monitors, screen size and display scale as numbers. A client can also be **resized on the canvas itself by dragging the outline of its box**, with no need to select it first (see below). **Remove computer** takes a computer out and **Add computer...** adds one (the grid's computer icon and trash are only there while the grid is shown). The screen of the server itself is the operating system's: only its display scale can be set. A computer that is added has no place yet, and is put beside the others where nothing touches it, until you drag it.
6. **Save.** The server configuration that is written has the **links that the arrangement derives**, and the core is restarted as usual. **Reset from classic grid** puts every computer back where the classic grid has them.

**Resize a computer by its outline.** Move the pointer to the outline of a client's box (a few pixels from its edge or corner): the cursor becomes a resize cursor and the outline is highlighted. Drag to resize; the size is shown while you drag, the green links follow, and the edges snap to the neighbours, to the common ratios (16:9, 16:10, 21:9, 32:9, 4:3, 3:2, 5:4 and their portrait forms) and to standard resolutions. **Shift** keeps the shape, **Alt/Option** takes the snapping away, **Esc** cancels. What it changes depends on what is known: a computer **whose screen is not known** is resized freely (its size is saved as its screen size); a computer **whose screen the server knows** (it reported it), or that has **several monitors**, is **scaled as a whole** in its own shape: its pixels and its monitors do not change, only how big it is drawn, which is its display scale (it snaps to 100, 125, 150 ... %). A computer is never put over another one. The server's own displays cannot be resized. To change one monitor of a computer, double-click it.

**Draw the monitors of a computer.** You do not have to know pixels. **Edit monitors...** shows the monitors of the computer as rectangles in real proportion, the way the displays arrangement of macOS or *Display settings* of Windows does:

1. Drag a monitor to move it (it snaps to the edges and middles of the others, with guides); drag the edges and corners of the selected one to resize it (snapping to the edges of the other monitors, to 16:9, 16:10, 21:9, 32:9, 4:3, 3:2, 5:4 and their portrait variants, and to standard resolutions; the size and the ratio are shown while you drag). **Add monitor**, **Remove monitor**, **Rotate** (portrait or landscape) and **Make main** are the buttons; **R**, **Delete** and the arrow keys work on the selected monitor. **Ctrl/Cmd+Z** undoes.
2. Pick the **resolution** of the selected monitor from the list (HD to 8K, ultrawides, the MacBooks in points) and its **display scale** (100, 125, 150 ... or any) as the display settings of that computer say. A monitor of 2560 × 1440 at 150 % is drawn as 1707 × 960, as in Windows' own picture.
3. The line on top says what the program knows about the computer. When the computer has told the server its screen, the drawing is **fitted** to it: the numbers are made from your drawing by scaling it to the screen that the computer reports, so you never type a pixel, and monitors that share an edge keep sharing it exactly. **✔ Matches what the computer reports** is green. **⚠ Does not match**, in amber, says by how much the *shape* of your drawing differs from the screen the computer reports (and what size your drawing is), so you can change the monitors, or press **Stretch to the reported size** to force it. **Not verified yet** means the computer has not connected: your layout is saved and checked when it does.
4. **Advanced: numbers** shows the same layout as `x,y,w,h` for each monitor, in the coordinates that the computer reports, with **Import from diagnostic file...**; **Use these numbers** draws them. The drawing and the numbers are always the same layout.

The editor assumes that a computer reports its whole desktop at one scale (a Mac in points, a Windows client that is not aware of per-monitor display scales). A Windows client that reports the physical pixels of monitors that have different scales shows **Does not match**: draw it with the scale of every monitor at 100 % (its pixels), and give the computer its **display scale** in **Edit computer...** for its size on the canvas. Mixed display scales on one computer are not verified on a real Windows machine.

**What the canvas shows.** The screen of this computer, the server, is the group in blue: its own displays as the operating system says they are (a MacBook below a DELL ultrawide is a T), not rearranged, with the whole group having the origin of the arrangement. Each other computer is its monitors in real proportion. A computer that is connected shows what it reported to the server; if the arrangement has another screen for it, the dialog warns (it is the screen of the computer that the server compares the monitors of a computer with).

**Units.** The arrangement is in *logical pixels*: a point of a Mac, a pixel of a Windows PC at 100 % display scale. A Windows PC at 150 % is drawn 1.5 times smaller than its pixels: give its monitors the display scale of 150 % in the monitor editor, or set the **display scale** of the computer to 150 % (the zoom is `100 / 150`; **Import from diagnostic file...** reads the scale of the primary monitor from a Windows block). That is how a Mac and a PC show as big next to each other as the operating systems say they are. To show them in their *physical* size set the scale by hand.

**What is derived.** Every edge of every monitor is looked at, and the part that another monitor of the same computer is right behind is dropped; what is left is an *exposed* edge. Where an exposed edge of a computer touches (within half a unit) an exposed edge of another computer, for at least a unit, there is a link in each direction, from that part of the edge of one to the same part of the edge of the other, so the pointer comes in where it went out. The part is a percentage of the side of the **bounding box** of each computer, as the link intervals of the server configuration always were, with up to four decimals (`left(0,51.6685) = PC`). A T-shaped or staggered contact is several links on a side. Two exposed edges that touch two different computers in the *same* rows of one side (the arms of a U) cannot be told apart by the server: the one on the bounding box wins and the dialog warns.

**Example.** The MacBook Pro (a 3440 x 1440 DELL above a 2064 x 1347 MacBook, 3440 x 2787 in all) is the server. A Windows PC with a 2560 x 1440 monitor and a 1920 x 1080 one on its left is on the left of the DELL, a Mac mini of 1920 x 1080 on the right of the DELL, and a laptop (1920 x 1080 at 125 %, which is 1536 x 864 units) is under the DELL part and beside the MacBook part. The core reads:

```
section: links
	9800X3D-5080:
		right = Subeoms-MacBook-Pro.local(0,51.6685)
		down(82.5223,100) = SUBEOM-COPILOT(0,50.9766)
	Subeoms-MacBook-Pro.local:
		right(0,38.7513) = Subeoms-Mac-mini.local
		left(0,51.6685) = 9800X3D-5080
		left(51.6685,82.6695) = SUBEOM-COPILOT
		down(0,21.8895) = SUBEOM-COPILOT(50.9766,100)
	Subeoms-Mac-mini.local:
		left = Subeoms-MacBook-Pro.local(0,38.7513)
	SUBEOM-COPILOT:
		right = Subeoms-MacBook-Pro.local(51.6685,82.6695)
		up(0,50.9766) = 9800X3D-5080(82.5223,100)
		up(50.9766,100) = Subeoms-MacBook-Pro.local(0,21.8895)
end
```

The pointer leaves the DELL through its right edge, over its top 1080 rows (38.7513 % of the 2787 rows of the bounding box), and comes in at the same height on the Mac mini; it leaves through the left edge of the DELL (the top 1440 rows, 51.6685 %) to the Windows PC, and through the left edge of the MacBook (the next rows) and the bottom of the DELL, left of the MacBook, to the laptop. The screen of the server is not a rectangle, so the server needs to know where its edges are: see the next paragraph.

**The server's own screen.** With the arrangement used, the pointer leaves the server through **any exposed edge of its screen**, including the inner edges of the T (the left and right edge of the MacBook, the bottom of the DELL beside it), and when it comes back it lands on a monitor, away from the edge. The server reads its displays from the operating system (macOS: the active displays in points, the numbers `mmdiag` prints; the other systems do not tell, which is the one rectangle of their screen as before). The decision is made from the position of the pointer, like it always was: a pointer on an edge pixel that has no monitor one jump zone further out is at an edge and leaves on that side (the system keeps the pointer on a monitor, so a pointer pushed against an edge is on its pixel). The limits are those of the edge detection of the box: touching the edge is pushing, so sliding along an exposed edge leaves too, and the delay and double tap options decide whether a push switches; the corner locks are the corners of the bounding box. If the displays change (one is unplugged), the arrangement is drawn again and the links are made again when the configuration is written, so what no longer touches loses its link. The core logs one line when it decides for the screen of the server (`monitor geometry on for "<server>": 2 monitors, 0,0 3440x2787`) and says it once for each change.

**What it does not do.** The client does not report its monitors: you give them, or the size that it reported to the server is taken as its screen size when the arrangement is first used and when you press **Reset from classic grid** (the dialog learns it from the core log line `computer "X" reports its screen: x,y wxh`, written while the experiment is on; a size that you typed is never replaced, and what a computer reports later changes nothing that is drawn or written, it only makes the dialog warn). A computer that has no monitors and no size is drawn as 1920 × 1080 and marked **size assumed** (on the canvas and in the line under it): set its size in **Edit computer**. The links are written with a decimal point (`left(0,51.6685)`), which the core reads when the numeric locale of its process has a point, as the Korean and English ones do. Each monitor is not a screen of its own, there are no rotations, the mouse speed is not scaled with the zoom, and the arrangement does not turn back into a grid: use the grid, which is kept as it was. The design, the limits and the tests are in [docs/dev/arrangement.md](dev/arrangement.md).

### Computer Settings

Each computer will have a section where its configuration will be stored, if the computer was named "foo" the section will be named `[computer_foo]`

|Option                  |    Valid Values   |Description|
|:-----------------------|:-----------------:|:-----------|
| aliases                | Comma separated list of hostnames | Names here will be used as alternatives for the computer. Names must be valid hostnames. |
| name                   | Valid hostname    | The name of the client. |
| halfDuplexCapsLock     | `true` or `false` | This computer has a ''Caps Lock'' key that doesn't report a press and a release event when the user presses it but instead reports a press event when it's turned on and a release event when it's turned off. If ''Caps Lock'' acts strangely on all computers then you may need to set this option to true on the server. If it acts strangely on one computer then that computer may need the option set to true.|
| halfDuplexNumLock      | `true` or `false` | This computer has a ''Num Lock'' key that doesn't report a press and a release event when the user presses it but instead reports a press event when it's turned on and a release event when it's turned off. If ''Num Lock'' acts strangely on all computers then you may need to set this option to true on the server. If it acts strangely on one computer then that computer may need the option set to true.|
| halfDuplexScrollLock   | `true` or `false` | This computer has a ''Scroll Lock'' key that doesn't report a press and a release event when the user presses it but instead reports a press event when it's turned on and a release event when it's turned off. If ''Scroll Lock'' acts strangely on all computers then you may need to set this option to true on the server. If it acts strangely on one computer then that computer may need the option set to true.|
| modifierAlt             | `shift` `ctrl` `alt` `altgr` `meta` `super` or  `none` | Map the server's alt modifer to different key on this computer|
| modifierAltGr           | `shift` `ctrl` `alt` `altgr` `meta` `super` or  `none` | Map the server's altgr modifer to different key on this computer|
| modifierCtrl            | `shift` `ctrl` `alt` `altgr` `meta` `super` or  `none` | Map the server's ctrl modifer to different key on this computer|
| modifierMeta            | `shift` `ctrl` `alt` `altgr` `meta` `super` or  `none` | Map the server's meta modifer to different key on this computer|
| modifierShift           | `shift` `ctrl` `alt` `altgr` `meta` `super` or  `none` | Map the server's shift modifer to different key on this computer|
| modifierSuper           | `shift` `ctrl` `alt` `altgr` `meta` `super` or  `none` | Map the server's super modifer to different key on thist computer|
| monitorLayout           | Text `x,y,w,h[;x,y,w,h...]` | Experimental: the monitors of this computer, see [Experimental: multi-monitor aware cursor mapping](#experimental-multi-monitor-aware-cursor-mapping). Empty (the default, and what a configuration from before has) means the computer is mapped as it always was. |
| arrangementX, arrangementY | real numbers | Experimental: where the origin of the coordinates of this computer (the top left of its primary monitor) is in the arrangement, in units that are points on a Mac and pixels of a Windows PC at 100 % scale. No key: not placed yet. See [Experimental: arrangement of the computers](#experimental-arrangement-of-the-computers). |
| arrangementZoom         | real number > 0 | Experimental: arrangement units per unit of this computer: 1 (the default) for a Mac, `100 / display scale in percent` for Windows (0.667 for 150 %). |
| monitorScales           | Comma separated percents, `150,100` | Experimental: the display scale of each monitor of `monitorLayout`, in its order, as the monitor editor drew them (25 to 500). Optional: with none, every monitor has the scale of `arrangementZoom`. Only the editor reads it. |
| arrangementWidth, arrangementHeight | integers | Experimental: the size of the screen of this computer in its own pixels, for a computer that has no `monitorLayout`; none (0): the size that the computer reported to the server, else 1920 x 1080. |
| shareClipboard          | `true` or `false` | `false` keeps this computer out of clipboard sharing: its clipboard is not sent to the other computers, theirs is not sent to it, and the server tells it to switch its own clipboard sharing off (`CLPS`), so it does not read its clipboard either. It is **Share clipboard with this computer** in the **Computer settings** dialog. A missing value means `true`, so an existing configuration shares with every computer as before [default: true] |
| f18Action               | `passthrough` or `hangul` | What the F18 key does when this computer is a Windows destination of a macOS server over a protocol 1.9 link: `passthrough` sends F18 unchanged, `hangul` sends the Korean-English (Hangul) key instead. A missing or unrecognised value means `passthrough` [default: passthrough] |
| crossOSInvertX         | `true` or `false` | Invert horizontal wheel deltas only on a negotiated macOS-to-Windows protocol 1.9 link [default: false] |
| crossOSInvertY         | `true` or `false` | Invert vertical wheel deltas only on a negotiated macOS-to-Windows protocol 1.9 link [default: false] |
| crossOSInvertScope     | `all` or `mouseWheelOnly` | Which scroll events `crossOSInvertX` and `crossOSInvertY` apply to: every wheel event (`all`), or only events the macOS server classifies as a mouse wheel, leaving trackpad scrolling unchanged (`mouseWheelOnly`). A missing or unrecognised value means `all` [default: all] |
| crossOSSemanticMedia  | `true` or `false` | Synthesize volume up/down/mute, previous/next track, play/pause, and stop only on a negotiated macOS-to-Windows protocol 1.9 link [default: false] |
| switchCornerSize       | integer (N)       | Sets the size of all corners in pixels. The cursor must be within `N` pixels of the corner to be considered to be in the corner.|
| switchCornerTopLeft    | `true` or `false` | Deskflow won't switch computers when the mouse reaches the edge of the computer if it includes this corner. The size of the corner is given by the `switchCornerSize` option. |
| switchCornerTopRight   | `true` or `false` | Deskflow won't switch computers when the mouse reaches the edge of the computer if it includes this corner. The size of the corner is given by the `switchCornerSize` option. |
| switchCornerBottomLeft | `true` or `false` | Deskflow won't switch computers when the mouse reaches the edge of the computer if it includes this corner. The size of the corner is given by the `switchCornerSize` option. |
| switchCornerBottomRight| `true` or `false` | Deskflow won't switch computers when the mouse reaches the edge of the computer if it includes this corner. The size of the corner is given by the `switchCornerSize` option. |
| xtestIsXineramaUnaware | `true` or `false` | This option works around a bug in the XTest extension when used in combination with Xinerama. It affects X11 clients only. Not all versions of the XTest extension are aware of the Xinerama extension. As a result, they do not move the mouse correctly when using multiple Xinerama screens. This option is currently ''true'' by default. If you know your XTest extension is Xinerama aware then set this option to ''false''.|
| weakX11Focus           | `true` or `false` | This X11 Client will not gain focus until an input event has been processed its any of its screens |

The `f18Action`, `crossOSInvertX`, `crossOSInvertY`, `crossOSInvertScope`, and `crossOSSemanticMedia` values are available in the destination computer's **Cross-OS Input** settings. These options apply only to negotiated macOS-to-Windows protocol 1.9 links. By default (`crossOSInvertScope=all`) scroll inversion applies to every wheel event and does not distinguish mouse from trackpad devices. The destination-local **Client Configuration** inversion and scale remain separate and are applied afterward, so enabling both layers can compound.

#### F18 as the Korean-English key

Command is always the Windows key. The Cross-OS Input settings never change it: Left and Right Command are sent exactly as they were before these settings existed, on every link and whatever the settings are. A `rightCommandAction` value left in the configuration by an earlier test build is ignored.

Setting `f18Action` to `hangul` (**F18 key: Korean-English toggle (Hangul)** in the **Cross-OS Input** settings) makes the macOS server send the Korean-English (Hangul) key to the Windows computer whenever F18 is pressed on the Mac. It takes effect only when all of these hold; in every other case F18, like every other key, is sent unchanged:

- the server is macOS and the destination is Windows
- the link negotiated protocol 1.9
- the Windows computer reported that it can type the Hangul key
- the setting is `hangul` (the default is `passthrough`, for which no option is sent at all)

The Windows computer reports the Hangul key only when its current keyboard layout has one, so **the Windows input language must be Korean** for the toggle to work. The report is made when the connection is made, so connect (or reconnect) after switching the Windows computer to the Korean layout.

The server does the mapping, so the Windows computer needs no setting. The key's press and release stay paired, the modifiers that were held are kept, and holding F18 toggles the input mode once: the repeats of the mapped key are not sent.

Deskflow only sees F18. It does not remap keys itself, and these docs do not cover how to make a key of your keyboard send F18 on macOS.

#### Mouse wheel only scroll inversion

Setting `crossOSInvertScope` to `mouseWheelOnly` (**Apply to: Mouse wheel only (trackpad unchanged)** in the **Cross-OS Input** settings) applies `crossOSInvertX` and `crossOSInvertY` only to scroll events that the macOS server classifies as coming from a mouse wheel. It is meant for setups where the mouse wheel and the trackpad need to scroll in opposite directions on the Windows computer, for example when a utility such as Scroll Reverser is used on the Mac. Restart the server after changing it.

The server classifies each scroll event when it captures it, using one rule:

- the event carries a scroll phase (`kCGScrollWheelEventScrollPhase`) or a momentum phase (`kCGScrollWheelEventMomentumPhase`) which is not zero: **trackpad**
- otherwise: **mouse**

The inversion is applied on the macOS server before the wheel event is sent, so the Windows computer needs no change and the wheel packet format is unchanged. While `mouseWheelOnly` is active the server does not send the `crossOSInvertX`/`crossOSInvertY` values to the destination, so the destination does not invert again. Any scroll event that cannot be classified (an unknown source, or a destination that is not macOS-to-Windows protocol 1.9) is never inverted by this scope.

The classification is a heuristic with known limits:

- A Magic Mouse reports scroll phases and is therefore treated as a trackpad.
- Utilities that smooth or emulate scrolling are classified by the phases of the events they post. One that fabricates phases is treated as a trackpad, and continuous (pixel based) events without phases are treated as a mouse. `kCGScrollWheelEventIsContinuous` and the sending process are deliberately not used.
- The rule is covered by automated tests using synthetic events only. It has not been verified against every device and driver.

To verify how your devices are classified, set the log level to **Debug** (**Preferences** > **Logs** > **Level**, or `level=DEBUG` in the `[log]` section), then stop and start the server so the new level is read. Scroll with the mouse wheel and then with the trackpad on the Mac and look in the log pane (or in the log file when **Log to file** is enabled) for lines such as:

```
scroll source changed to mouse (scrollPhase=0 momentumPhase=0 continuous=0 sourcePid=...)
scroll source changed to trackpad (scrollPhase=2 momentumPhase=0 continuous=1 sourcePid=...)
```

The raw fields are logged only when the classification changes, never for every event. The `continuous` and `sourcePid` values are shown for diagnosis only and are not part of the rule. If a device is classified differently than expected, the phases shown tell why.

To confirm that the classification also reaches the server's relay, set the log level to **Verbose**: every relayed wheel event is then logged as `onMouseWheel +120,-120 source=1`, where `source` is `0` for unknown, `1` for mouse and `2` for trackpad. Verbose logging writes one line per scroll event, so switch back to **Info** afterwards.

### InternalConfig

This section contains options used when in server mode it will begin with `[internalConfig]`
block of a server config file as seen below. This section is used by the GUI to generate a server configuration

```
[internalConfig]
hotkeys\1\actions\1\activeOnRelease=false
hotkeys\1\actions\1\hasScreens=true
hotkeys\1\actions\1\keys\1\key=83
hotkeys\1\actions\1\keys\size=1
hotkeys\1\actions\1\lockCursorToScreen=0
hotkeys\1\actions\1\restartServer=false
hotkeys\1\actions\1\switchInDirection=0
hotkeys\1\actions\1\switchScreenName=void
hotkeys\1\actions\1\type=0
hotkeys\1\actions\1\typeScreenNames\size=0
hotkeys\1\actions\size=1
hotkeys\1\keys\1\key=83
hotkeys\1\keys\size=1
hotkeys\size=1
screens\1\name=
screens\10\name=null
screens\11\name=
screens\12\name=
screens\13\name=
screens\14\name=
screens\15\name=
screens\2\name=
screens\3\name=
screens\4\name=
screens\5\name=
screens\6\name=
screens\7\name=void
screens\8\name=chris-Precision-5570
screens\9\name=abyss.lan
screens\size=15
```


# Server Config

The `deskflow-server` command accepts the `-c` or `--config` option, which takes one argument,
the path to a server configuration file. When using the GUI the `internalConfig` section of the GUI settings will be exported as the server configuration.
The configuration file is plain text and case-sensitive. The file is broken into sections, and each section has the form:
```
section: ''name''
	''arg'' = ''value''
end
```

Comments are introduced by ''#'' and continue to the end of the line. ''name'' must be one of the following:

* ''links''
* ''options''

The file is parsed top to bottom and names cannot be used before they've been defined in the general config or as an alias in the general config.

### links secion

''args'' is a list of computer names just like in the ''screens'' section except each computer is followed by a list of links, one per line. Each link has the form:
```
 {left|right|up|down}[<range>] = name[<range>]
```

A link indicates which computer is adjacent in the given direction.

Each side of a link can specify a range which defines a portion of an edge. A range on the direction is the portion of edge you can leave from while a range on the computer is the portion of edge you'll enter into. Ranges are optional and default to the entire edge. All ranges on a particular direction of a particular computer must not overlap.

A ''range'' is written as <code>(start,end)</code>. Both ''start'' and ''end'' are percentages in the range 0 to 100, inclusive. The start must be less than the end. 0 is the left or top of an edge and 100 is the right or bottom. 

```
section: links
	moe:
		right        = larry
		up(50,100)   = curly(0,50)
	larry:
		left         = moe
		up(0,50)     = curly(50,100)
	curly:
		down(0,50)   = moe
		down(50,100) = larry(0,50)
end
```

This indicates that computer ''larry'' is to the right of computer ''moe'' (so moving the cursor off the right edge of ''moe'' would make it appear at the left edge of ''larry''), the left half of curly is above the right half of ''moe'', ''moe'' is to the left of ''larry'' (edges are not necessarily symmetric so you have to provide both directions), the right half of curly is above the left half of ''larry'', all of ''moe'' is below the left half of ''curly'', and the left half of ''larry'' is below the right half of ''curly''.

Note that links do not have to be symmetrical; for instance, here the edge between ''moe'' and ''curly'' maps to different ranges depending on if you're going up or down. In fact links don't have to be bidirectional. You can configure the right of ''moe'' to go to ''larry'' without a link from the left of ''larry'' to ''moe''. It's possible to configure a computer with no outgoing links; the cursor will get stuck on that computer unless you have a hot key configured to switch off of that computer.

### options section

''args'' is a list of lines of the form <code>name = value</code>. These set the global options.

```
section: options
        name = value
end
```

#### List of options allowed in options section

| Options | Value Values| Description|
|:--------|:-----------:|:-----------|
|screenSaverSync| `true` or `false`| ''Note: Removed in v1.14.1'' If set to ''false'' then Deskflow won't synchronize screen savers. Client screen savers will start according to their individual configurations. The server screen saver won't start if there is input, even if that input is directed toward a client computer.|
|keystroke(key) | actions | Binds the ''key'' combination key to the given ''actions''. ''key'' is an optional list of modifiers (''shift'', ''control'', ''alt'', ''meta'' or ''super'') optionally followed by a character or a key name, all separated by + (plus signs). You must have either modifiers or a character/key name or both. See below for `valid key names` and `actions`. Keyboard hot keys are handled while the cursor any computer. Separate actions can be assigned to press and release.|
|mousebutton(button) | actions| Binds the modifier and mouse button combination ''button'' to the given ''actions''. ''button'' is an optional list of modifiers (''shift'', ''control'', ''alt'', ''meta'' or ''super'') followed by a button number. The primary button (the left button for right handed users) is button 1, the middle button is 2, etc. Actions can be found below. Mouse button actions are not handled while the cursor is on the server. You cannot use these to perform an action while on the server. Separate actions can be assigned to press and release.|

##### Actions

Actions are two lists of individual actions separated by commas. The two lists are separated by a '';'' (semicolon). Either list can be empty and if the second list is empty then the semicolon is optional. The first list lists actions to take when the condition becomes true (e.g. the hot key or mouse button is pressed) and the second lists actions to take when the condition becomes false (e.g. the hot key or button is released). The condition becoming true is called activation and becoming false is called deactivation. Allowed individual actions are:

* `keystroke(key[,computers])`

* `keyDown(key[,computers])`

* `keyUp(key[,computers])`


: Synthesizes the modifiers and key given in ''key'' which has the same form as described in the ''keystroke'' option. If given, ''computers'' lists the computer or computers to direct the event to, regardless of the active computer. If not given then the event is directed to the active computer only.
: ''keyDown'' synthesizes a key press and ''keyUp'' synthesizes a key release. ''keystroke'' synthesizes a key press on activation and a release on deactivation and is equivalent to a ''keyDown'' on activation and ''keyUp'' on deactivation.
: ''computers'' is either ''*'' (asterisk) to indicate all computers or a '':'' (colon) separated list of computer names. (Note that the computer name must have already been encountered in the configuration file so you'll probably want to put ''actions'' at the bottom of the file.)

* `mousebutton(button)`
* `mouseDown(button)`
* `mouseUp(button)`
: Synthesizes the modifiers and mouse button given in ''button'' which has the same form as described in the ''mousebutton'' option.
: ''mouseDown'' synthesizes a mouse press and ''mouseUp'' synthesizes a mouse release. ''mousebutton'' synthesizes a mouse press on activation and a release on deactivation and is equivalent to a ''mouseDown'' on activation and ''mouseUp'' on deactivation.

* `lockCursorToScreen(mode)`
: Locks the cursor to or unlocks the cursor from the active computer. ''mode'' can be ''off'' to unlock the cursor, ''on'' to lock the cursor, or ''toggle'' to toggle the current state. The default is ''toggle''. If the configuration has no ''lockCursorToScreen'' action and ''Scroll Lock'' is not used as a hot key then ''Scroll Lock'' toggles cursor locking.

* `switchToScreen(computerName)`
: Jump to computer with name or alias ''computerName''.

* `switchInDirection(dir)`
: Switch to the computer in the direction ''dir'', which may be one of ''left'', ''right'', ''up'' or ''down''.

* `switchToNextScreen()`
: Cycle to the next computer in the configuration order. If at the last computer, cycles back to the first computer.

##### Keynames
Valid key names are:

* AppMail
* AppMedia
* AppUser1
* AppUser2
* AudioDown
* AudioMute
* AudioNext
* AudioPlay
* AudioPrev
* AudioStop
* AudioUp
* BackSpace
* Begin
* Break
* Cancel
* CapsLock
* Clear
* Delete
* Down
* Eject
* End
* Escape
* Execute
* F1
* F2
* F3
* F4
* F5
* F6
* F7
* F8
* F9
* F10
* F11
* F12
* F13
* F14
* F15
* F16
* F17
* F18
* F19
* F20
* F21
* F22
* F23
* F24
* F25
* F26
* F27
* F28
* F29
* F30
* F31
* F32
* F33
* F34
* F35
* Find
* Help
* Home
* Insert
* KP_0
* KP_1
* KP_2
* KP_3
* KP_4
* KP_5
* KP_6
* KP_7
* KP_8
* KP_9
* KP_Add
* KP_Begin
* KP_Decimal
* KP_Delete
* KP_Divide
* KP_Down
* KP_End
* KP_Enter
* KP_Equal
* KP_F1
* KP_F2
* KP_F3
* KP_F4
* KP_Home
* KP_Insert
* KP_Left
* KP_Multiply
* KP_PageDown
* KP_PageUp
* KP_Right
* KP_Separator
* KP_Space
* KP_Subtract
* KP_Tab
* KP_Up
* Left
* LeftTab
* Linefeed
* Menu
* NumLock
* PageDown
* PageUp
* Pause
* Print
* Redo
* Return
* Right
* ScrollLock
* Select
* Sleep
* Space
* SysReq
* Tab
* Undo
* Up
* WWWBack
* WWWFavorites
* WWWForward
* WWWHome
* WWWRefresh
* WWWSearch
* WWWStop
* Space
* Exclaim
* DoubleQuote
* Number
* Dollar
* Percent
* Ampersand
* Apostrophe
* ParenthesisL
* ParenthesisR
* Asterisk
* Plus
* Comma
* Minus
* Period
* Slash
* Colon
* Semicolon
* Less
* Equal
* Greater
* Question
* At
* BracketL
* Backslash
* BracketR
* Circumflex
* Underscore
* Grave
* BraceL
* Bar
* BraceR
* Tilde

Additionally, a name of the form `\uXXXX` where ''XXXX'' is a hexadecimal number is interpreted as a unicode character code. Key and modifier names are case-insensitive. Keys that don't exist on the keyboard or in the default keyboard layout will not work.

### Example textual configuration file

The alias section is no longer in the server config

This example comes from doc/deskflow-basic.conf

```
# sample deskflow configuration file
#
# comments begin with the # character and continue to the end of
# line.  comments may appear anywhere the syntax permits.
# +-------+  +--------+ +---------+
# |Laptop |  |Desktop1| |iMac     |
# |       |  |        | |         |
# +-------+  +--------+ +---------+

section: links
	# iMac is to the right of Desktop1
	# Laptop is to the left of Desktop1
	Desktop1:
	        right(0,100) = iMac # the numbers in parentheses indicate the percentage of the computer's edge to be considered active for switching)
		left  = Laptop
		shift = shift (shift, alt, super, meta can be mapped to any of the others)
	# Desktop1 is to the right of Laptop
	Laptop:
		right = Desktop1
	# Desktop1 is to the left of iMac
	iMac:
		left  = Desktop1
end
section: aliases
	# The "real" name of iMac is John-Smiths-iMac-3.local. 
	# If we wanted we could remove this alias and instead use John-Smiths-iMac-3.local everywhere iMac is above. 
	# Hopefully it should be easy to see why using an alias is nicer
	iMac:
		John-Smiths-iMac-3.local
end
```

#### Cursor Wrapping

The text config allows computers to be wrapped around. For example, with two machines (a server and a client), the mouse can go off the right of the server onto the left side of the client, then off the right side of the client back onto the left side of server. This config also uses ''Ctrl''+''Super''+(''left arrow''/''right arrow'') to switch between machines on keypress.

```
# Physical monitor arrangement, with machine names as used by Deskflow.
#  +----------+----------+
#  | syn-serv | syn-cli  |
#  |          |          |
#  +----------+----------+ 
 
section: links
	syn-serv:
		left = syn-cli     # "wrapping" arrangement
		right = syn-cli    # "normal" arrangement
	syn-cli:
		left = syn-serv    # "normal"
		right = syn-serv   # "wrapping"
end
section: options
                keystroke(control+super+right) = switchInDirection(right) # Switch computers on keypress 
<!--		keystroke(control+super+left) = switchInDirection(left) -->
end
```


See also: the man page for ''deskflow-core''.

### Stacked Example

Stack one computer on top of another's.

```
#           +-------+
#           | curly |
#           |       |
#           +-------+
# +-------+ +-------+
# | moe   | | larry |
# |       | |       |
# +-------+ +-------+

section: links
	# larry is to the right of moe and curly is above moe.
	moe:
		right = larry
		up    = curly

	# moe is to the left of larry and curly is above larry.
	larry:
		left  = moe
		up    = curly

	# larry is below curly.
	curly:
		down  = larry
end

```

### Horizontal Example

Align all computers horizontally.

```
# +-------+ +-------+ +-------+
# | moe   | | larry | | curly |
# |       | |       | |       |
# +-------+ +-------+ +-------+

section: links
	# curly is to the right of larry and moe is to the left of larry.
	larry:
		right = curly
		left  = moe

	# larry is to the right of moe.
	moe:
		right = larry

	# larry is to the left of curly.
	curly:
		left  = larry
end

```

### Span Example

Span one computer across the two other computers.

```
# +-------+ +-------+
# | curly | | curly |
# |       | |       |
# +-------+ +-------+
# +-------+ +-------+
# | moe   | | larry |
# |       | |       |
# +-------+ +-------+

section: links
	# larry is to the right of moe and curly is above moe.
	moe:
		right = larry
		up    = curly

	# moe is to the left of larry and curly is above larry.
	larry:
		left  = moe
		up    = curly

	# larry is below curly.
	curly:
		down  = larry
end
```
