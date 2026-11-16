# Monitor reporting: design note (experimental)

[multi-monitor.md](multi-monitor.md) maps the pointer around the holes of a computer whose monitors do not make up a
rectangle, but the monitors have to be **typed in** by the user. This feature, stage 3 of that work, lets a **client
tell the server its monitors** when it connects and again whenever they change, so that the server maps with them, and
the arrangement canvas draws them, without anyone typing anything.

It is **experimental, negotiated and off by default**. A link on which it has not been negotiated carries not one byte
that it did not carry before and takes not one decision that it did not take.

## Capability

`kLayoutCapabilityMonitors = 1u << 12` (`src/lib/common/MonitorCapabilities.h`). Bits 0 to 3 are input
(`InputCapabilities.h`) and 8 and 9 clipboard (`ClipboardCapabilities.h`); 10 to 11 stay free for the clipboard and 12
was never used. It travels in the capability mask of `DIPC` and `CIPC`, like the clipboard bits: a peer that predates a
bit ignores it, because it only tests the bits it knows. No protocol minor changes (still 1.9).

| Who | Sets the bit | When |
|---|---|---|
| server, in `DIPC` | "I use a monitor report" | only when `server/experimentalMultiMonitor` is on when the computer connects. With it off, or for a legacy server, the mask is byte for byte what it is today, so a computer is never invited |
| client, in `CIPC` | "I report my monitors" | only when it **received** the bit in `DIPC` **and** its platform can list its monitors. It is the acknowledgement; there is no other reply |

The server accepts a `DMON` only from a computer that has the bit in its `CIPC` while it advertised it itself. The
client sends a `DMON` only to a server that advertised it. Nothing waits for it: the handshake is **not** gated, the
server maps with the one rectangle of `DINF` until a valid report arrives, which is a fraction of a second after the
connection and long before anyone pushes against an edge.

Why a capability is mandatory: a receiver that gets a message code it does not know **discards the whole buffered
stream** (`ClientProxy1_0::handleData`, `ServerProxy::handleData`), so a stray `DMON` would eat the next key and mouse
events of a peer that never heard of it.

## The message

`DMON`, client to server, only after negotiation. It is **not** a list with a length of its own: its size is fixed by
its header, so a receiver always knows how much of the stream is the message, whatever it holds.

```
"DMON" version(1) generation(2) count(1)  then  count x monitor
monitor = x(2) y(2) w(2) h(2) scale(2) flags(1)          (11 bytes, big endian)
```

- `version` is 1. A later layout of the records has to use another message code, because a receiver reads the size
  that this version says; any other value of `version` is a bad report (dropped with an INFO line).
- `generation` counts the reports of this connection, 1 on, wrapping at 65 535. The server keeps it and logs it at
  DEBUG; a report that has the same monitors as the last one is not sent again, so a changed generation means a
  changed list.
- `count` is 0 to 255 on the wire and 1 to 16 when valid. **0 means "I have no monitors to report": forget the last
  report** (sent when the monitors cannot be made to tile the shape, see below). A receiver always reads the
  `count x 11` bytes, so even 255 leaves the stream where the sender left it.
- `x y w h` are `int16` in **the coordinate space of the `DINF` shape** of the same client: points on macOS, physical
  virtual-desktop pixels on Windows. They are not scaled for display.
- `scale` is `uint16`, the display scale in percent (100, 125, 150, 200 ...), 0 if unknown. It is informational for
  the canvas and not used for the pointer.
- `flags`: bit 0 is the primary monitor. The other bits are 0 now and ignored by a reader.

A valid report is at most 4 + 1 + 2 + 1 + 16 x 11 = 184 bytes.

### When the client sends it

1. Right after its `CIPC`, which is after its `DINF` (the server asks for it first), so the server has the shape to
   check the report against.
2. After **every** `DINF` it sends (a shape change, or the server asking for it again).
3. When the list of monitors changes **and the bounding box does not** (two monitors that swap places, a scale change):
   the platform raises `ComputerMonitorsChanged`. A change of the box already sends `DINF`, so it also sends a report.
4. The client compares with the report it sent last, and sends nothing if the monitors are the same.
5. A report is only sent when the shape of the computer is the one of the last `DINF` that was sent: if the shape changed
   and its `DINF` is still to be sent (two display events in a row), the `DINF` brings the report, so a server never gets a
   report for a shape that it has not been told.

A client that **cannot** make its monitors tile exactly the shape of its `DINF` (the rectangles come from another
system call than the shape, or a monitor is out of the range of 16 bits) **does not report** and logs one INFO line
with the reason; if it had reported before it sends `count = 0`.

### Where the client gets its monitors

`IPlatformComputer::getMonitorInfo()` (defaulted to the rectangles of `getMonitors()`; empty means "cannot report", so
the capability is not acknowledged). Platform code only produces numbers; everything else (the 16-bit range, the
mirrored displays, the overlap check, the box that must equal the shape) is `deskflow/MonitorReport.cpp`, the same code
on every platform and unit tested with fake enumerations.

| Platform | Source | Coordinates | Notes |
|---|---|---|---|
| macOS | `CGGetActiveDisplayList` + `CGDisplayBounds` (`OSXDisplays.cpp`, already read for the server's own screen) | points, Quartz global space: exactly the space of `DINF` | scale = pixels of the display mode per point (200 on a Retina display); main display is primary; a mirror set is one monitor. `displayReconfigurationCallback` already raises `ComputerShapeChanged` for every change, so a report follows every `DINF` |
| Windows | `EnumDisplayMonitors` + `GetMonitorInfoW` (`rcMonitor`, `MONITORINFOF_PRIMARY`) + `GetDpiForMonitor(MDT_EFFECTIVE_DPI)` loaded from `shcore.dll` (`MSWindowsMonitors.cpp`; 100 % when it is missing) | the virtual-screen pixels of the calling thread: the same space as `GetSystemMetrics(SM_*VIRTUALSCREEN)` that makes the shape | The DPI awareness of the process is **not** changed. When the box of the monitors is not the shape (a process that is not per-monitor aware sees a virtualised space) nothing is reported and one INFO line says so. The list is read the first time a server that invited the computer asks for it (a link that did not negotiate monitors never calls Windows for them), and with every `WM_DISPLAYCHANGE` after that; when the box did not change but the list did, `ComputerMonitorsChanged` is raised |
| Linux (X11, libei) | none | | `getMonitors()` is empty: no acknowledgement, the legacy rectangle |

## What the server does with it

`ClientProxy1_9` reads the message, checks it, and keeps **one** report for the connection; it is gone when the
connection is. Every failure below drops the report, writes **one INFO line**, and leaves the stream and the connection
alone: a bad report is never a reason to disconnect a computer.

| Check | |
|---|---|
| negotiated | no bit in the `CIPC`, or the server did not advertise it: the message is read and ignored (no INFO at all) |
| version 1, `count` 1 to 16 | else dropped. `count` 0 is a valid "forget" |
| each monitor `w > 0`, `h > 0`, no `int16` overflow | the wire has 16 bits; a `w` or `h` with the sign bit is a negative size |
| no two monitors overlap, one connected shape | the rules of `MonitorLayout::create`, the same as for a typed layout |
| the union box equals the latest `DINF` shape | else dropped |

The report is checked again **when it is used**: it is used only while its box still equals the shape that the computer
reports now. So a `DINF` that moves the box (and has no new report yet) puts the computer back on the legacy path at
once, and the next valid report brings it back.

### Which monitors the server uses

`Server::monitorLayoutFor` stays the one predicate. In this order, the first that holds:

1. `server/experimentalMultiMonitor` is off, or the computer is the server's own screen: **legacy**.
2. The user's `computer_<name>/monitorLayout` (typed, imported or **drawn** in the monitor editor) parses and its box
   equals the shape: **the user's layout wins** (`manual`).
3. The computer's report is valid and its box equals the shape: **the report** (`reported`). This also holds for a
   user's layout that no longer fits the shape (a monitor was plugged in): the real monitors are better than the legacy
   rectangle.
4. Otherwise: **legacy**, exactly as today.

A computer whose monitors make up a rectangle is mapped as it always was, whatever the source.

A computer that has a report while the experiment is off is not mapped by it and is not logged about: the master switch
is read when the computer connects (for the invitation) and at every decision (for the use).

### The log line the GUI reads

```
INFO: computer "<name>" reports <N> monitors: <x>,<y>,<w>,<h>[ scale=<S>][ primary];<x>,...
```

One line per **change** of a computer's report, and one when it connects, only while the experiment is on, with the name
cleaned (a quote, a line end or another control character becomes `?`). `N` is the number of monitors; `monitors` is
always plural. A report that is dropped says `computer "<name>" monitor report ignored: <reason>`. The GUI reads the
first line only when it is the whole line as the core writes it, as it does for `reports its screen:`.

## The GUI

The GUI reads the log line above in the output of the core, as it reads `reports its screen:` (no IPC change): only while
the experiment is on, only the whole line as the core writes it, only for a computer of the configuration, and only a
list that a computer can report (1 to 16 monitors, valid together, the count of the line equal to the list). A name
that has a line end in it makes a line that does not begin with the time and the level, and the core has cleaned the
quotes out of a name, so a name cannot close itself.

- **Kept** in **one new optional key**, `computer_<name>/reportedMonitors` (text, the notation of the log, empty by
  default). Only the GUI writes it, as soon as a line is taken, and the core never reads it (it uses the live report):
  a computer that is offline is still drawn from what it reported last. `reports no monitors` takes it back.
- **Drawn.** The arrangement canvas draws a computer that has no layout of the user's from its reported monitors, with the
  line *reported by the computer* on it, at the scale of its primary monitor (a Windows PC at 150 % is `100 / 150` of
  an arrangement unit; a zoom that is exactly 1, the one that nobody set, is replaced by that; a computer that the
  user scaled keeps its zoom). A computer that reported its monitors is not "size assumed". The typed layout
  (typed, imported or drawn in the monitor editor) wins, as for the server.
- **Live.** `ServerConfig::listenToReports` tells the open server configuration dialog when a line says something new
  about a computer; the dialog copies what the main window knows into the copy that it works on and redraws the
  canvas, so a computer that reports another desktop is followed without restarting the dialog.
- **Editor.** *Edit monitors* of a computer that reports and has no layout of its own opens the monitor editor on the
  reported monitors, in their scales, **read-only** (the mouse, the keys, undo and the controls change nothing, and
  closing it changes nothing). *Draw my own instead* turns the editing on; what is then saved is the user's layout,
  which wins from then on. The editor shows the report as it was when it was opened.

Computers that never report keep the manual path (`monitorLayout`, the size, the guess).

## Compatibility matrix

| Server | Client | Result |
|---|---|---|
| new, experiment on | new (this) | `DIPC` has bit 12, `CIPC` has bit 12, `DMON` after the handshake and on changes |
| new, experiment on | legacy (Windows 1.27.0.45, the Mac mini 1.27.0.89) | `DIPC` has the bit, `CIPC` does not: the server never expects a `DMON`. A `DMON` that comes anyway is read and ignored, the stream is kept |
| new, experiment **off** | any | `DIPC` is byte identical to today; no bit, no `DMON`; a `DMON` that comes anyway is read and ignored |
| legacy (1.27.0.89 and older) | new | no bit in `DIPC`: the client sends no `DMON` and does not set bit 12 in its `CIPC` |
| legacy | legacy | as today |
| protocol 1.8 and older | new | no `DIPC`, so no negotiation: nothing changes |

The tests run these with the real proxy classes, on the bytes: golden bytes for every link where the gate is not met.

## Not done / not verified

- Linux (X11, libei) does not report: `getMonitors()` is empty, the capability bit is not set, they stay on the legacy
  path.
- Nothing here was run on Windows. The Windows source is compile-checked with a cross compiler only
  ([windows-build-checklist.md](windows-build-checklist.md) says what and how far).
- A monitor on a different scale than the others is reported with its own scale, but the pointer does not use it.
- Rotation, refresh rate and the physical size of a monitor are not reported.
