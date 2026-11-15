# GUI Config

 Deskflow will automatically figure out where to save settings and other files.

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
| dynamicConnectionInterval | `true` or `false`  | Use dynamic connection retry times based on number of previously failed attempts [default: false] |
| languageSync              | `true` or `false`  | Sync to server language [default: true] |
| remoteHost                | `IP` or `hostname` | The remote host(s) to connect to. Use a comma separated list when you want to try several hosts |
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
| externalConfig     | `true` or `false` | When true use the external config path |
| externalConfigFile | Filepath          | Path the server config file if it does not exist the GUI will it generated based on the `internalConfig` section.|
| gridHeight         | int               | Height of the server's intenal grid used for the computer layout (default: 3)|
| gridWidth          | int               | Width of the server's intenal grid used for the computer layout (default: 5) |
| heartbeat          | int               | The server will expect each client to send a message no less than every `N` milliseconds. If no message arrives from a client within `3N` seconds the server forces that client to disconnect. If deskflow fails to detect clients disconnecting while the server is sleeping or vice versa, try using this option.|
| protocol           | `barrier` or `synergy` | The protocol to use when saying hello to clients. Can be set to barrier or synergy. If not set barrier is used as the default |
|relativeMouseMoves  | `true` or `false` | If set to ''true'' then secondary computers move the mouse using relative rather than absolute mouse moves when and only when the cursor is locked to the computer (by ''Scroll Lock'' or a configured hot key). This is intended to make Deskflow work better with certain games. If set to ''false'' or not set then all mouse moves are absolute.|
| switchDelay        | int               | Deskflow won't switch computers when the mouse reaches edge of a computer unless it stays on the edge for `N` milliseconds. This helps prevent unintentional switching when working near an edge. (default: 250)|
| switchDoubleTap    | int               | Deskflow won't switch computers when the mouse reaches the edge of a computer unless it's moved away from the edge and then back to the edge within `N` milliseconds. With the option you have to quickly tap the edge twice to switch. This helps prevent unintentional switching when working near the edge.|
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
