# Multi-monitor aware cursor mapping: design note (experimental)

Deskflow treats every computer as **one rectangle**: the bounding box of all of its monitors. That is
exact for a single monitor and for monitors that make up a rectangle, and wrong when they do not
(monitors of different heights side by side, an L shape, a portrait monitor next to a landscape one).
The part of the box that no monitor covers is a *hole*. The server maps a crossing over the whole box
and lets its virtual pointer travel through the holes, so for such a computer

- a pointer that enters on an edge can be put **in a hole**, and the operating system then moves it to
  some other pixel (25 to 70 % of the edge positions in the layouts that were studied);
- a pointer that is pushed against a monitor edge that is not an edge of the box keeps travelling in
  the hole, and the server only switches computers after up to 1 920 px of *dead travel*.

This feature, **off by default**, fixes both for a computer whose monitors the server is told about.
User documentation is in [Configuration.md](../Configuration.md#experimental-multi-monitor-aware-cursor-mapping);
this note is about how it works and what it does not do.

## Nothing changes unless it is asked for

The feature is server side, manual and has **no wire change**: no message, no option id, no capability
bit. A computer is never told about it, so the Windows client 1.27.0.45 and every other client work
with a server that has it on, without a rebuild. The monitors are *not* an option either: options are
`int32` values sent to the computer, the layout is a text that only the server core reads.

The one decision (`Server::monitorLayoutFor`) is true only when **all** of these hold:

1. `server/experimentalMultiMonitor` is on;
2. the computer is not the server's own screen (see [Not done](#not-done));
3. the computer has `computer_<name>/monitorLayout`, and it parses and validates;
4. the bounding box of the layout is **equal to the shape the computer reports now** (`DINF`);
5. the monitors are not a rectangle (a rectangle is mapped as it always was, to the bit).

If any is false, the code that runs is the code that always ran. The server logs one INFO line each time
the decision for a computer changes (`monitor geometry on|off for "X": reason`), not on every mouse move,
and one INFO line at every switch of computers while the experiment is on (source and destination
shape, the edge and its fraction, and the pixel the pointer enters at).

A change of the reported shape (a resolution change, a monitor that is plugged in), and a computer that
connects again with another shape, are checked against the layout again. A layout that no longer fits
is dropped, and used again when the shape matches again.

## The silhouette model

`deskflow::server::MonitorLayout` (`src/lib/server/MonitorGeometry.{h,cpp}`) is pure geometry: integers,
no Qt, no platform call. The layout is validated when it is made: 1 to 16 monitors, `w` and `h` greater
than 0, no two overlapping, all coordinates and the bounding box inside what a `DINF` shape can say
(16 bits), and the monitors form **one connected shape** (they share part of an edge; touching at a
corner is not enough). Monitors that are exactly the same rectangle (a mirrored display) are counted once.

Because the shape is connected, **every row and every column of the bounding box has a monitor on it**.
So there is no need for another scale along an edge than the one Deskflow always used:

- **Entering.** The fraction of the side of the bounding box is turned into a row (or column) with the
  arithmetic of `Server::mapToPixel`, and the pointer enters on the *outermost pixel of that row* (or
  column): always on a monitor, on the box edge where a monitor reaches it, on an inner edge where the
  box edge is a hole. How far the pointer was taken into the computer is kept.
- **Leaving.** The server's virtual pointer moves like the pointer of the operating system: over the
  monitors, and *sliding* along an edge that it is pushed against (`MonitorLayout::move`). What is left
  of a move that could not go on is what pushes the pointer out, and the pointer leaves through **any
  exposed edge pixel**, including the inner edges of an L. A horizontal push is tried before a vertical
  one, as it is for a rectangle. The computer on that side is found as it always was, from where the
  pointer would be past the edge of the box.
- **Round trip.** The row of a fraction and the fraction of a row map to each other exactly, so a
  pointer that goes to another computer and straight back is on the row it left from.
- **Rectangles.** For a layout that is a rectangle the result is, bit for bit, what
  `Server::mapToFraction` and `Server::mapToPixel` give (tested for every edge pixel of 12 shapes).

### Link intervals

A link interval (`left(0,75) = PC(0,75)`) is still a percentage of the side of the **bounding box**, for
a computer that has a layout as for any other. What is different is the pixel it leads to: the
outermost pixel of the row (or column) that the percentage is at, which for a row below a smaller
monitor is the inner edge of the monitor that has it, not the edge of the box. Intervals written for a
computer whose monitors make up a rectangle do not shift when a layout is set. (The research prototype
counted the *covered* rows instead; the connected shape makes that the same list as all of them, and
leaving the percentages alone is what keeps existing links working.)

## Where it is

| Where | What |
|---|---|
| `src/lib/common/Settings.{h,cpp}` | `server/experimentalMultiMonitor` (bool, default false) and `computer_<name>/monitorLayout` (text, default empty) |
| `src/lib/server/Config.{h,cpp}` | `Config::addComputer` reads the layout from the settings, as it reads the other per computer values; `setMonitorLayout` / `getMonitorLayout`; the layout is part of `operator==` |
| `src/lib/server/Server.{h,cpp}` | the predicate, the entry on a monitor in `mapToNeighbor`, the moves in `onMouseMoveSecondary`, the log lines |
| `src/lib/gui/...` | the check box in the server configuration dialog (Advanced), the text field, validation and import in the computer settings dialog |
| [arrangement.md](arrangement.md) | the arrangement of the computers, which makes the links from where the screens touch and gives the server's own screen its edges |

The GUI and the core are different processes that share one settings file. Saving the server
configuration dialog restarts a running core, which reads the file again; `ServerApp::reloadConfig`
(the reload signal) reads it again as well and calls `Server::setConfig`, which switches the experiment
on or off. A file that is edited by hand takes effect when the core is restarted. The layout is read
for the computers of the settings in external config mode too, because `Config::read` adds them first.

The text is read with `Settings::textValue`, not `value().toString()`: QSettings returns a value of a
settings file that has a comma in it and no quotes as a `QStringList`, and `toString()` of a list of
more than one part is empty, which silently dropped a hand-written layout. A `;` that is not in quotes
starts a comment in a settings file and cannot be recovered by any reader, so a layout of more than one
monitor has to be in double quotes there (the dialog writes it so); the tests pin both.

## Compatibility pins (tests)

The golden values of the pointer pins were recorded from the **unmodified** server, in a real `Server`
with a `PointerRig` (a primary computer and remote computers that have shapes of their own), before the
server was changed, and are not to be re-recorded.

| Pin | Test |
|---|---|
| P1 experiment off: every pointer decision is the same | `ServerTests::pointerPin_*` with the experiment off, and off with layouts set |
| P3 on with no layout, a layout that is wrong, or one of another size: legacy | the same tests in those modes |
| P4 on with a rectangle: legacy; and the library maps a rectangle bit for bit | the same tests in that mode; `MonitorGeometryTests::plainRectangle_*` |
| P2, P5 no byte on the wire, no new message | `ClientProxyTests::handshake_monitorExperiment_changesNoByteOnTheWire` |
| P6 upgrade: an old configuration has it all off | `SettingsTests`, `ServerConfigTests::monitorLayout_*`, `ComputerTests::monitorLayout_*` |

The behaviour tests (`ServerTests::monitorGeometry_*`) cover an entry into what is a hole today, leaving
through an inner edge, no dead travel, link intervals, a shape that changes, a computer that connects
again, the log lines, the corners that lock the cursor, and that nothing is added to the options a
computer is told. `MonitorGeometryTests` has a table of layouts (a single monitor, side by side, one
above the other, staggered, an L, negative origins, portrait and landscape, ultrawide, 2 by 2, a hole in
the middle of an edge, a Mac with an external display, mixed scales) and seeded property tests (an entry
is on a monitor, on the edge, monotone, the round trip is exact, a random walk never leaves the
desktop). The dialogs have layout tests in three styles at two font sizes.

## Not done

- **The client does not report its monitors.** The layout is typed (or imported) by the user. A
  negotiated capability and a message for it (stage 3 of the research) would need a new Windows and Mac
  client, and is not part of this.
- **The server's own screen** is not mapped by a layout: a layout for the primary computer is ignored (and the log
  says so). Its displays are read from the operating system and used with the arrangement of the computers (the
  `server/useArrangement` setting, see [arrangement.md](arrangement.md)); without it the edge detection of the
  server stays that of one rectangle.
- **Each monitor is not a screen of its own**, and there are no links per monitor.
- **Mouse speed** is not scaled; `DESKFLOW_MOUSE_ADJUSTMENT` still applies to every computer.
- **Relative mouse moves** (locked cursor, games) are relative as before and do not use the layout.
- The **physical** alignment of monitors of different sizes is not tried: an edge is mapped fraction to
  fraction, as before.
- A layout is checked against the *box* the computer reports, not against where each monitor is. An
  arrangement change that keeps the box (two monitors that swap places) is not seen by anyone; the
  layout then describes the old arrangement.

## What could not be tested here

The tests drive the real `Server` with computers that record what they are told. Not verified on real
hardware: a Windows PC or a Mac with several monitors; what Windows does with a point that is on no
monitor (which the server no longer sends); the conversion of the absolute move that the Windows client
makes (`MSWindowsDesks::deskMouseMove`, which normalises to the primary monitor only, with a
`(DWORD)(float)` that is 0 for a negative value on ARM64); mixed-DPI desktops; and how the dialogs look
on Windows. Windows compiles the same shared files as every platform; there is no platform code in
this feature.
