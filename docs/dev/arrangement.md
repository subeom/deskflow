# Arrangement canvas: design note (experimental)

The classic server configuration is a grid of cells (5 × 3): a computer is in a cell and its neighbours are the
computers in the cells next to it. Nothing in it says how big a computer is, so every edge is linked whole, and a
computer that is a different size than its neighbour is linked as if it were the same size. The multi-monitor
experiment ([multi-monitor.md](multi-monitor.md)) maps the pointer inside one computer; this feature, which builds on
it, lets the user **place the computers (and their monitors) in real proportion on one canvas, like the macOS
Displays arrangement**, and **derives the links of the server configuration from where they touch**.

It is **experimental, off by default and has no wire change**: no message, no option id, no capability bit, and not a
byte of the server configuration *format* is new (the links are the `left(a,b) = Name(c,d)` that the core has always
read). The Windows client 1.27.0.45 and the Mac mini keep working without a rebuild.

## Decisions

| | |
|---|---|
| Space | one 2-D **arrangement space**, x to the right, y down, real numbers (`double`) |
| Unit | one **logical pixel**: a macOS *point*, a Windows pixel at 100 % display scale |
| Per computer | its monitors, a **zoom**, and an **origin offset** |
| Links | **derived** from where exposed edges touch; the core is given the result as ordinary links |
| Switch | `server/experimentalMultiMonitor` (master) **and** `server/useArrangement` (this feature); both off by default |

### 1. Space and units

A computer has monitors in *its own coordinate units*: points on macOS, physical pixels on Windows (what it reports in
`DINF`, and what `monitorLayout` is written in). The arrangement space is **logical pixels**, because that is what
both the macOS and the Windows display settings draw their own arrangement in, so two screens look as big next to each
other as the operating systems say they are.

`zoom` is *arrangement units per coordinate unit* of that computer, default **1.0**:

- macOS: 1.0 (points are already logical);
- Windows at *s* % display scale: `zoom = 100 / s`: 100 % → 1, 125 % → 0.8, 150 % → 0.667, 200 % → 0.5. The dialog
  offers "Windows display scale" and stores the zoom; when a diagnostic block with `scale=` is imported, the zoom is
  `1 / scale` of the **primary** monitor (or of the first monitor when none is `primary`). A computer that has
  monitors of *different* scales is approximated by one zoom;
- anything else: any positive number. A user who wants *physical* proportions (a 13" laptop is smaller than a 34"
  ultrawide although its screen has fewer points) sets the zoom by hand; that is the only place where physical size
  enters.

A monitor `m` (computer units) of a computer with zoom `z` and origin offset `(ox, oy)` is the arrangement rectangle
`(ox + z·m.x, oy + z·m.y, z·m.w, z·m.h)`. The **origin offset is where the computer's coordinate origin is** in the
arrangement: on both macOS and Windows that is the top left of the primary monitor, so adding a monitor to a computer
does not move the monitors it had. The bounding box of the monitors of a computer is the shape it reports; all link
fractions are fractions of the sides of this box, as the core has always read them.

Edges touch when they are within `kTouchTolerance = 0.5` units of each other (zoom 0.667 makes fractions that no
decimal says exactly) and the touching segment is longer than `kMinLink = 1` unit. Stretches of one side that overlap
or are apart by less than `kHair = 0.01` unit are the same place (a position that was rounded to three decimals
misses by up to 5e-4): they meet in the middle, so that the intervals that the core reads never overlap.

### 2. Per-computer data

| What | Source of truth | Key |
|---|---|---|
| monitors of a client | the shipped `monitorLayout`; if empty, one monitor `0,0,W,H` | `computer_<name>/monitorLayout` (shipped), `computer_<name>/arrangementWidth` and `arrangementHeight` (new, integers, 0 = unknown) |
| monitors of the **server** | **the operating system**, read when the dialog opens and when the configuration is written, never stored | — |
| position | origin offset in arrangement units | `computer_<name>/arrangementX`, `computer_<name>/arrangementY` (new, real) |
| zoom | arrangement units per unit | `computer_<name>/arrangementZoom` (new, real, default 1) |
| switch | | `server/useArrangement` (new, bool, default false) |

The server's own displays are **one rigid group**: its monitors cannot be moved against each other, only the group
has a position. Its origin offset is stored like any other computer's; since everything is relative, the position of
the server group only means "where the others are around it". The server group is the main display's origin on
macOS (`CGDisplayBounds` in points, global space, the numbers `mmdiag` prints). If the displays change (a monitor is
unplugged) the group changes shape around the same origin, the links are derived again when the configuration is
written, and what no longer touches loses its link (the dialog says so).

A computer that has no keys has no arrangement and is placed by the conversion in section 5. Old configurations have
none of the new keys and read exactly as before.

### 3. Links are derived

The Config model (`src/lib/server/Config.{h,cpp}`) was read, not guessed:

- a link is `side(srcStart,srcEnd) = Name(dstStart,dstEnd)`. `Interval` is a pair of `float`, kept as a *fraction*
  (0 to 1) of the side of the **bounding box**; `ConfigReadContext::parseInterval` reads percentages with `strtod`,
  so **decimals are read** (`left(0,51.6685)`); only the `operator<<` of the core's own `Config` rounds to whole
  percentages, and that is a debug dump, not what the GUI writes;
- **source intervals of one side of one computer must not overlap** (`Cell::add` refuses, the file does not load).
  Destination intervals are not checked, may overlap, and are not an error;
- the intervals are half open `[start, end)`; a position is clamped just below 1;
- `Config::getNeighbor` maps `p` in the source interval to `d0 + (p − s0)/(s1 − s0)·(d1 − d0)`; the server converts a
  pixel to a fraction with `getCursorEdgeFraction` (pixel centre over the side of the box) and back with
  `mapToPixel`; all of it is linear and symmetric;
- several links per side are allowed (`curly` has two on `down` in the example of the Configuration page).

**Derivation.** Every monitor edge is an *edge segment*; the part of it that has another monitor *of the same
computer* right behind it is covered and dropped, the rest is **exposed**. For two different computers A and B and a
pair of sides that face each other (A `right` with B `left`; A `down` with B `up`), every exposed segment of A that
touches (within the tolerance) an exposed segment of B is intersected along the edge: each intersection longer than
`kMinLink` is one link in **each direction**, with

```
srcInterval = (a0 − boxStartA)/boxLengthA … (a1 − boxStartA)/boxLengthA     (a0..a1 = the intersection)
dstInterval = (a0 − boxStartB)/boxLengthB … (a1 − boxStartB)/boxLengthB
```

in arrangement units, so both intervals are the **same physical stretch** of the edge. A pointer that leaves at
physical position `p` along the stretch enters the other computer at the same physical `p`:
`d0 + (p−s0)/(s1−s0)(d1−d0) = (p − boxStartB)/boxLengthB` (proved by the symmetry test and a property test).
Consecutive intersections of the same pair that are continuous on both sides are merged into one link, so
**an equal-size grid derives exactly the link lines of the classic grid** (`right = B`, no interval): that is the
equivalence test. Intervals are written as percentages of up to four decimals, from fractions rounded to 1e-6 so the
boundary that two links share is the same number in both.

A T-shaped or staggered contact is several links of one side (the T of the server: a PC beside the DELL part and one
beside the MacBook part are two links of `left`). A computer touching two computers on one side gives two links.
What is **not** expressible is one row of a side that has *two different exposed edges* (a U shape: the inner edge of
an arm and the outer edge of the other arm, in the same rows) with two neighbours: source intervals would overlap. The
outer edge (the one on the bounding box) wins, the other link is dropped and the dialog warns (`AmbiguousSide`).
Since the core lets the pointer leave through *any* exposed edge of a side, a link also covers the rows of an edge of
that side that touches nothing, if it is in the same rows (the outer edges of the U with a computer in its notch):
that is a warning too (`EdgeInTheSameRows`), not an error. The T of the server does not have it: its edges on a side
are in rows of their own.

### 4. How it composes with the silhouette mapping

The silhouette model keeps its meaning: *entry* lands on the outermost pixel of the row or column of the fraction,
which for a link derived from an exposed edge is exactly the monitor edge that the neighbour touches; *exit* is
through any exposed edge pixel, and the computer on that side is found from the position the pointer would have beyond
the box, as the core has always done. The percentage still runs along the side of the **bounding box**; since the
derived interval is a stretch of the side that has an exposed edge in all of its rows, the pixel it leads to is
always an exposed monitor edge of that stretch.

So that this also works for **the server's own screen**, the experiment gives the server what the clients already
have: the primary uses its monitors, from the operating system without any message (stage B): the pointer leaves
through any exposed edge of the server's desktop (the inner edge of the DELL part of the T, the left and right edges of
the MacBook part), and comes back onto a monitor, away from the jump zone.

The primary's exit is decided from the **position** alone, like it always was: the macOS event tap gives the pointer
position, and the system clamps it to the desktop, so a pointer pushed against an exposed edge sits on its edge
pixel. A position whose pixel `jumpZone` further out (`x − zone`, `x + zone`, `y − zone`, `y + zone`) is no monitor is
*at an exposed edge* and tries to leave on that side (a horizontal side is tried before a vertical one, in a corner).
This is what the box edge test always meant (`x < ax + zone`), for every rectangle it is the same test, and the limits
are the legacy ones: touching the edge is pushing (so *sliding along* an exposed edge pushes too), and the delay and
double-tap options decide, as before, whether a push switches (the double tap is armed when the pointer was as far
from an exposed edge as the option says, for an inner edge as for one on the bounding box). The neighbour is found
from where the pointer would be beyond the **bounding box**, as always, so the fraction along the side is the same
whether the pointer leaves through an inner edge or through the box. A position that the system reports in a hole is
judged from the nearest monitor pixel. The corner locks are the corners of the bounding box. Coming back, the pointer
lands on the outermost monitor pixel of the row (or column) and, if the primary has a neighbour on that side, `jumpZone`
pixels in, so that it does not bounce. If the platform tells no monitors, they are not the screen of the platform, or
they make up a rectangle, nothing of this runs: it is the code that always ran.

### 5. The classic grid becomes an arrangement

The first time the experiment and the arrangement are on and no arrangement is saved (and at **Reset from classic
grid**), each computer gets its monitors and its zoom (its saved ones, the size it was given, or 1920 × 1080), and
the **server group is placed first**. Then, in order of grid distance from the server, every computer is placed
**touching its already placed neighbour in the direction the grid has it**, centred along the shared edge; if that
spot overlaps a computer already placed it slides along the edge until it fits and still touches. Computers that the
grid links are therefore touching, except when the sizes make that impossible (a ring of different sizes): these links
are reported ("not kept") and not silently lost. For computers of the same size the result is a grid and the derived
links are the old ones, line for line. There is no way back to the grid and none is needed: the classic grid stays
stored, untouched, and **Use the classic grid** switches to it.

**Several links of a side and the switch delay.** With a delay before a switch (`switchDelay`), `Server::isSwitchOkay`
remembers the computer it waits to switch to. A side has several neighbours, one for each part of its edge, and the
pointer can slide along the edge from the rows of one into the rows of another: with the arrangement used, that is as
new as another direction (the wait and the double tap start again for the new computer). Without the arrangement,
the code is as it was (the wait stays on the first one).

**The numeric locale.** The GUI writes the intervals with a decimal point whatever the locale (`std::format`). The
`Config` of the core reads them with `strtod`, which follows `LC_NUMERIC`: a process whose numeric locale has a decimal
comma would read `(0,51.6685)` as an invalid interval. The core is a Qt application, and Qt takes the locale of the
system on Unix; the machines that this was made for (Korean and English locales) have the point. The reader was not
changed, because `strtod` also reads forms that another reader would not (so it is not provably the same for every
file); tests pin that the text is the same in every locale and that the core reads it in the C locale.

### 6. Not in scope

- The client **does not report its monitors** or its size to the GUI (a following stage may show the reported size).
- Each monitor is not a screen of its own (no per-monitor links); no rotation; mouse **speed** is not scaled with the
  zoom (the pointer is mapped by position only); mixed per-monitor DPI inside one computer is one zoom.
- Universal Control and similar are not designed for. Nothing depends on them.

### Reported screens (stage D)

A computer tells the server its screen in `DINF` as it always did. While the experiment is on the core logs it
(`computer "X" reports its screen: x,y wxh`, at INFO, once for each change and each connection), the GUI reads that
line from the output of the core (no IPC change, no new message). It is **not** what is drawn or written: it is taken
as the size of a computer that has no monitors and no size when the arrangement is first used and on **Reset from
classic grid**, and the dialog **warns** when the arrangement has another screen for a computer (the core only uses
the monitors of a computer when their bounding box is exactly what it reports).

The GUI takes the line only while the experiment is on, only when it is the whole line as the core writes it (`[time]
INFO: computer "X" reports …`; a name that has a line end in it makes a line that begins with the text, which is not
taken), and only for a computer of the current configuration. A computer that is not known could still send a name
with a whole forged line in it; the worst that does is a wrong size that is adopted once or a warning, and it can be
set again in **Edit computer**.

**The size is assumed** when a computer has no monitors, no size (or only one of the two) and nothing was adopted: the
canvas draws 1920 × 1080 for it and says so on the computer (a dashed border and *size assumed*) and in the status
line, because the line of the log is not there when the level of the log is above INFO or the computer has not
connected.

## The monitor editor (draw the monitors of a computer with the mouse)

Typing `x,y,w,h` in pixels does not work for people: every computer has another display scale, and nobody knows the
pixels of their desktop. The editor draws the monitors of **one computer** in the units of the arrangement, and the
program derives the numbers. All geometry is in `src/lib/server/MonitorEditor.{h,cpp}` (pure, no Qt; Windows compiles it
as it is); the widget and the dialog only draw it and hand it the mouse.

**Drawing.** A `Monitor` is a rectangle in arrangement units (logical pixels at 100 %) and a display *scale*. Its
resolution is what it is drawn at times the scale (`2560 × 1440` at 150 % is `1707 × 960` units), so the proportions
on the canvas are those of Windows' own *Display settings* picture and of macOS' *Displays* arrangement. The first
monitor is the main display. Resolutions come from a preset list (HD to 8K, ultrawides, the MacBooks in points) or are
dragged: dragging an edge or a corner resizes, with the opposite edge fixed; an edge snaps to the edges of the other
monitors (and of the other computers on the canvas), a side snaps to the common ratios (16:9, 16:10, 21:9, 32:9, 4:3,
3:2, 5:4 and their portrait variants) and the pixels snap to a preset, each within a small distance; a side is never
smaller than 160 units (a preset snaps only the sides of the edge that is dragged), and a monitor is never dragged over another (the place is refused and it stays where it was).
Changing the resolution, the scale or the orientation of a monitor keeps the corner that keeps most of the edges that
touch the other monitors. Problems are overlaps, a monitor that touches none (monitors have to share part of an edge),
more than 16, sizes and scales that cannot be.

**What the server needs.** The monitors in the coordinates that the computer **reports** (`monitorLayout`, as before),
and the box of them equal to the screen that the computer reports. The reported screen (the core's INFO line, stage D)
is the ground truth, so the drawing is *fitted* to it: each distinct edge of the drawing (edges within half a unit are
one) is put at its place in the box, `X0 + round((edge − left) · kx)`, with the first edge at the left of the screen
and the last at its right. Rounding **edges**, not sizes, is what keeps the monitors that share an edge sharing it
exactly: no gap and no overlap whatever the scale. `kx`, `ky` are the box of the screen over the box of the drawing;
the shape of the drawing has to be within 3 % of the shape of the screen (`|ky/kx − 1|`), else it is not fitted (the
status says by how much it *differs*, and the size the drawing would be in the pixels of the main display, to say what
to change) unless the person asks to stretch it. The layout is then checked with `MonitorLayout::create`.

**Units of the computer.** The fit does not need to know whether a platform reports pixels or points: it scales to
what the computer reports. What it assumes is that the computer reports with **one scale for the whole desktop** (a
Mac in points; a Windows client that is not aware of per-monitor DPI, whose coordinates are the desktop at the scale of
its main display): then the layout is the drawing times one factor, and the factor is the zoom of the arrangement
(`zoom = 1 / sqrt(kx · ky)`, Windows 150 % is 0.667). A Windows client that reports each monitor in its own physical
pixels, with monitors of different scales, is not drawn right by one factor: the drawing of its monitors at their
real scales is not the shape of what it reports, the status says *differs*, and drawing it at the scale of its pixels
(100 % on every monitor) matches the box, at the price of a canvas size that is then set with the display scale of the
computer. This is not verified against a real Windows machine with mixed scales (see *Verified, and not*).

**Without a reported screen** (not connected yet) the drawing is made at the scale of its main display with the top
left of the main display at 0,0, and the result is *not verified*: it is checked as soon as the screen arrives.

**Storage.** `computer_<name>/monitorLayout` (the monitors, as before), `arrangementZoom`, `arrangementX/Y` (where the
origin of the computer is: made so the drawing stays where it was on the canvas) and one optional key,
`computer_<name>/monitorScales`, a list of the scales of the monitors in the order of the layout (missing: the scale
of the zoom; an entry that is not a scale keeps its place, as "no scale", so the ones that follow stay with their
monitors; editing the display scale of the computer, or its numbers, in **Edit computer...** drops them, so every
monitor follows the new zoom). An old configuration loads unchanged; the text field of the layout stays, as **Advanced: numbers**, and
what is in it is the same layout.

**History.** The editor keeps the states that it went through (200 at most): undo and redo are `Ctrl/Cmd+Z` and
`Shift+Ctrl/Cmd+Z`.

**Resizing a client by its outline on the canvas.** Every computer except the server can be resized by dragging the
outline of its box, without selecting it first. The pointer within 7 screen pixels (a constant, whatever the zoom of the
view; a tiny box keeps its middle: at most a third of its smaller side) of an edge or a corner gets the matching resize
cursor, the outline is highlighted and its handles are shown; a press there starts the resize (and selects the
computer), a press anywhere else in the box moves it, as before. When two outlines are within reach (neighbours that
share an edge) the nearer one wins, then the computer that the pointer is in, then the selected one. The drag has a live
preview, the derived links and the problems are recomputed while it goes, and a bubble says the size. Shift keeps the
shape of the box (a free resize), Alt takes the snapping away, Esc cancels, a change that would put the computer over
another one is not made, and the keys still nudge. What a drag *means* depends on what is known of the computer:

| The computer | A drag of its outline | What is stored |
|---|---|---|
| (a) one rectangle, no reported screen | resizes it, freely: the opposite edge stays; the edges snap to the edges of the other computers, the sides to the common ratios and to the presets (pixels = units / zoom) | its size in its own units (`arrangementWidth/Height`), or its one monitor when it has a layout of one (`monitorLayout`, its scales dropped); never a size that a layout cannot have (16-bit numbers) |
| (b) one rectangle, the screen is known (the computer reported it) | **scales** it: the shape is what it is, so only the size on the canvas changes, uniformly (the opposite corner, or for an edge the opposite edge and the near edge of the other side, stays) | `arrangementZoom` (and the origin); the size and the pixels are never changed |
| (c) several monitors | scales the whole computer in the same way: the monitors keep their relative layout; it is over another computer when one of its *monitors* is, not its box (another computer may be in a notch of the box) | `arrangementZoom` (and the origin); `monitorLayout` and the typed size are untouched; the drawn scales of the monitors follow the zoom (`scale × oldZoom / newZoom`, or are dropped if one would leave 25 to 500 %) |
| the server's own displays | not at all: its outline moves it like its middle does | |

In (b) and (c) the zoom is snapped to a display scale that there is (100, 125, 150 ... %, so `100 / percent`) within 1.5 %
when no edge snapped, and must stay in `kMinZoom .. kMaxZoom`. The pure part is `editor::resize` with
`Constraints::lockAspect`: the side that is not dragged follows (for a corner, the one that changed the most), the
ratios, the presets and the target size are not snapped, and a side is never under `kMinSide`. A single monitor of a
computer is not resized on the canvas: that is for the monitor editor (double-click), which can keep the layout valid and
fitted to the reported screen; the hint under the canvas says so. For a zoom above 1 (a display scale under 100 %) the
size of (a) is a whole number of the units of the computer, so a dragged edge can be off by up to `zoom / 2` units from
the edge that it was snapped to. There is no undo on the canvas: the monitor editor has it, and **Reset from classic
grid** puts everything back.

## Verified, and not

Verified by tests that run here: the library (tables, seeded property tests, the equivalence with the classic grid, the
text read back by the real core `Config`, in the C numeric locale), the golden pins of the pointer decisions that were recorded from the code
before the change, the server's silhouette on the user's real T (leaving through every exposed edge, coming back,
display changes, the log), the canvas widget (drag, snap, keys), the dialogs (state, save, layout in three styles at two
font sizes), the generated configuration (golden) and the settings round trip. Not verified: the feel of the canvas
with a real mouse, a Windows PC or a Mac with several monitors, the pointer at an inner edge on a real macOS (the event
tap was not run), Windows compiling and behaviour, a display that is plugged in while the server runs. For the monitor
editor in particular: the feel of dragging with a real mouse, and what a real Windows client reports for monitors of
different display scales (one scale for the whole desktop, or the physical pixels of each monitor): the model assumes
the first and says *differs* for the second, see *Units of the computer*.

## Where it is

| Where | What |
|---|---|
| `src/lib/server/Arrangement.{h,cpp}` | the pure library: types, validation, snapping, derivation, grid conversion, link text. No Qt, no platform |
| `src/lib/deskflow/IPlatformComputer.h`, `Computer.{h,cpp}`, `src/lib/server/PrimaryClient.{h,cpp}` | `getMonitors()` of the server's own desktop (default: nothing, which is its one rectangle) |
| `src/lib/platform/OSXDisplays.{h,cpp}`, `OSXComputer.{h,mm}` | the only macOS code: the active displays in points, kept up to date with the shape |
| `src/lib/server/Server.{h,cpp}` | silhouette of the primary (exit through any exposed edge, entry on a monitor, double tap), the log lines |
| `src/lib/common/Settings.{h,cpp}` | `server/useArrangement`, `computer_<name>/arrangement{X,Y,Zoom,Width,Height}` |
| `src/lib/gui/widgets/ArrangementCanvas`, `ArrangementPanel` | the canvas and what goes with it |
| `src/lib/gui/config/ServerConfig`, `Computer` | the arrangement of the grid's computers, link generation in `operator<<`, persistence |
| `src/lib/gui/dialogs/ServerConfigDialog`, `ComputerSettingsDialog` | the panel and the offer; screen size and display scale |
| `src/lib/server/MonitorEditor.{h,cpp}` | the pure geometry of the monitor editor: moving, resizing, snapping, presets, rotation, scales, the fit to the reported screen, undo history. No Qt, no platform |
| `src/lib/gui/config/MonitorEditing.{h,cpp}` | a computer as a drawing for the editor, and what the editor made as the computer's monitors, zoom, scales and place |
| `src/lib/gui/widgets/MonitorEditorWidget`, `dialogs/MonitorEditorDialog` | the drawing widget (mouse, handles, guides, the size while dragging) and the dialog (status, controls, "Advanced: numbers") |
| `src/lib/gui/core/ReportedScreen`, `MainWindow` | the line of the core log that says the screen of a computer |

The shared files that a Windows build compiles (everything but `OSXDisplays` and `OSXComputer`): `Arrangement.{h,cpp}`,
`Server.{h,cpp}`, `PrimaryClient.{h,cpp}`, `Computer.{h,cpp}`, `IPlatformComputer.h`, `Settings.{h,cpp}`, and in the GUI
`ArrangementCanvas`, `ArrangementPanel`, `ServerConfig`, `Computer`, `ServerConfigDialog`, `ComputerSettingsDialog`,
`ReportedScreen` and `MainWindow`; and, for the monitor editor, `MonitorEditor.{h,cpp}` (pure), `MonitorEditing`,
`MonitorEditorWidget` and `MonitorEditorDialog`. None of them calls a platform API.
