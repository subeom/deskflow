# Remember the arrangement for each set of displays: design note (experimental)

A laptop that is the server has one arrangement of the other computers with an external monitor and another without it.
When the displays change (the monitor is unplugged or plugged in, the lid is closed, the Mac sleeps and wakes with another
set), the links that the arrangement derives are wrong for the new displays: the computers that were next to the DELL
touch nothing of the MacBook, so the pointer cannot get to them. This feature **remembers the arrangement of the other
computers for each situation of the server's own displays and puts the matching or most similar one back**.

It is **experimental, off by default, and has no wire change**: no message, no option id, no capability bit; the core is not
changed at all. User documentation is in [Configuration.md](../Configuration.md#experimental-remember-the-arrangement-for-each-set-of-displays).
It builds on [arrangement.md](arrangement.md) (the canvas and the derived links) and [multi-monitor.md](multi-monitor.md).

## Where it lives, and why not in the core

The core already follows the shape of the server's own screen: `OSXComputer::displayReconfigurationCallback` →
`updateComputerShape` → the server reads the monitors again. But the core **never writes the server configuration**: the GUI
derives the links (`ServerConfig`'s `operator<<` → `deriveLinks`) from the arrangement settings
(`computer_<name>/arrangementX/Y/Zoom`) and from the server's displays (read from the operating system when the file is
written), and the core reads that file only when it is started. So the detection, the choice and the restart belong to the
**GUI process**, which owns the settings, the generation of the file and the core process. Nothing in the core, the protocol
or the generated syntax changes, so the feature cannot change the behaviour of the core or of a client.

## Switch and gating

`server/rememberArrangements` (bool, default **false**), next to `server/useArrangement`. It has an effect only when **all** of:

1. it is on;
2. `server/experimentalMultiMonitor` is on and `server/useArrangement` is on: the thing that is remembered is the
   arrangement; the classic grid has no arrangement, and its links do not depend on the displays;
3. `server/externalConfig` is off: with the user's own file the GUI does not generate the links;
4. the program is in server mode.

If any is false, `LayoutMemoryService` does nothing: no timer, no screen connected, no file read or written, no setting
written, no restart (tested: the generated configuration, the settings and the files are untouched, byte for byte). The
setting is in the arrangement panel of the server configuration dialog and is saved with **Save**, like the other
switches of the dialog.

## The display signature

`DisplayInfo` (pure data) per display: `uuid` (macOS: the display UUID), `vendor`/`model`/`serial` (EDID based, 0 =
unknown), `builtIn`, `primary`, the rectangle `x,y,w,h` in points with the origin at the **main display** (the numbers
`ServerConfig::serverMonitors()` returns, so the same numbers rebuild the arrangement of the server group), the `scale` in
percent, and a `label` for people only.

`DisplaySignature` normalises a list: invalid displays (no size) are left out; displays that are the same rectangle (a
mirror) count once (the main one, then the built in one, wins); the rest is sorted by position, size, identity; at most 16
(`MonitorLayout`'s limit; the rest is dropped, `truncated()` says so). So the order in which the system lists the displays
never matters.

- **identity** = the UUID if there is one, else `v:<vendor>:<model>:<serial>` if any number is known, else empty (anonymous);
- **class** = built in or not, width, height, scale;
- **key** = one `identity|class|x,y|main` part per display, joined by `;`. Equal keys are the **exact** match. The position is
  the raw one because the arrangement is measured from the main display: another position, or another main display, is
  another arrangement;
- **id** = the first 8 hex characters of the SHA-256 of the key. Only the id (and the summary below) is logged; a serial
  number or a UUID never is;
- **summary** = `external 3440x1440@100% at 0,0 (main) [10ac:a0c4] + built-in 1512x982@200% at 964,1440`: the maker and the
  model, not the serial.

**The monitors that the other computers report are not in the key.** The user's case is the server's displays, and a client
whose monitors change changes nothing here: it only changes what the canvas draws, and the layout holds nothing of a
client's monitors.

## Matching

Exact: the keys are equal (score 1). Otherwise the current displays are paired one to one with the remembered ones, the best
pair first (a pair in the same place before one that is not, then the order of the lists: deterministic), with these credits:

| pair | credit |
|---|---|
| the same identity (both anonymous counts), the same class | 1.00 |
| the same identity, another resolution or scale (the monitor is the same, its mode is not) | 0.90 |
| no common identity, the same class (a stand-in of the same kind) | 0.75 |
| anything else | 0 |

`coverage = sum of credits / max(number of current displays, number of remembered ones)` (a subset or a superset costs what
it lacks), times **0.95** if a paired display is somewhere else (the same displays in another arrangement), times **0.95** if
the main display is not paired with the remembered main display (or only one side has one).
**Threshold: score ≥ 0.60 is "similar"** and is used. Consequences (all tested): the same displays arranged otherwise
0.95; a different 34" monitor of the same resolution and scale for the old one (1 + 0.75) / 2 = 0.875; 2 of 3 displays 0.667;
**1 of 2 displays 0.5, which is not similar**: the arrangement of the T is not the right one for the laptop alone, and the
fallback below is better.

Ties: the higher score, then the one used last, then the one made last, then the smaller id. The exact one always wins.

## What is remembered

A **layout** is, for each computer (the server group too, whose origin is part of the relative arrangement): its **origin
and zoom** (where it is in the arrangement and how big it is drawn). That is exactly what a move or a scale on the canvas
changes. Its monitors, its size and its display scales are **what the computer is**, not where it is: they are set by what it
reports or by the monitor editor, and a change of the server's displays must not turn them back. A zoom is restored through
`ServerConfig::setComputerZoom`, so the drawn display scales follow it as they do on the canvas.

A layout is made or updated **when the user saves the arrangement** while a situation is active (the situation is the one the
arrangement was last known to belong to, so an edit made in a dialog that was open while a monitor was unplugged is
remembered for the situation that it was made in). The first edit in a situation that was only *similar* makes a profile of
its own and leaves the similar one as it was. **A Save that leaves the arrangement as it is teaches nothing**: the program
keeps the arrangement as it last knew it (after its own change, after Undo, after the user's save), and a Save of something
else (the heartbeat, a hotkey) does not make the arrangement of the old displays the layout of the new ones, as it would
after a restore that failed or was undone.

**When nothing says what the arrangement belongs to** (the first time, or the feature was off for a while) the arrangement
that there is, is taken as the user's for the displays that there are: it is remembered for them, and a layout that is
remembered for them is not put over it. Switching the feature off forgets the situation for that reason (nothing is tracked
while it is off).

## The store

A separate file, `layout-profiles.json`, next to the settings file (`Settings::settingsPath()`). **Not settings keys**:
`Deskflow.conf` is rewritten as a whole by two programs and is read by the core (a damaged value could keep Deskflow from
starting), its text values already needed workarounds for commas and semicolons, and up to 32 profiles of 16 computers would
bloat it. The file is read and written by the GUI only, replaced atomically (`QSaveFile`), and local: it has the names of the
user's computers, so it is never committed or sent anywhere.

Format: `{ "format": "deskflow-layout-profiles", "version": 1, "nextId", "active": <signature>, "profiles": [...] }`; a
profile has `id`, `name`, `userNamed`, `signature`, `layout`, `createdAt`, `updatedAt`, `lastUsedAt` (ms since the epoch).
`active` is the situation that the arrangement in the settings belongs to, so that the program started after a change of
displays knows the old situation too.

| Situation | What happens |
|---|---|
| no file | an empty store |
| not JSON, another format, no version, a list that is not a list, bigger than 2 MiB | the file is moved to `layout-profiles.json.damaged`, the store starts empty, one warning |
| a newer `version` | **inert**: the file is not read as a store, not written, not moved; the feature is off for the session with a warning and a notice |
| an entry that is not valid (bad names, positions, zoom, sizes, times, signature, a name twice) | the entry is left out (counted); the file as it was is kept as `.bak`; a profile twice for one signature keeps the one used last |
| more than 32 | the least recently used goes, one that the user did not name before one that the user did; the situation that is active and the profile just made or used stay |
| the file cannot be written | one warning, the feature goes on in memory |

**The user's current arrangement is never in danger from the store**: it lives in the settings, which this file does not
touch, and a layout is applied only as a whole (below).

## The state machine

`LayoutMemory` has no timer and no clock: the caller says what time it is (`now`) and asks `nextDeadline()`. It talks to the
outside only through `ILayoutHost` (displays, the arrangement, the core, the clock, the log, the window), so the tests run it
with fakes and a clock that they move.

```
Disabled --enabled--> Idle --event, or a poll that sees another situation--> Settling
Settling --quiet for 1.5 s, or 8 s after the first event--> look --> Idle
                     (the host cannot change the arrangement: Deferred, tried again every 1 s)
```

- **Events.** The service connects `QScreen::geometryChanged`, the DPI signals and `screenAdded/Removed/primaryScreenChanged`
  while the feature is on. A burst (plug, sleep, wake, lid) extends one waiting time; a burst that does not stop is looked at
  8 s after its first event. A poll every 3 s looks at the displays without an event (the system does not always say) and
  treats another situation as an event. **An event that leaves the signature unchanged is ignored** (a debug line only).
- **Reading.** Displays that cannot be read (an empty list while the Mac wakes) are tried again, 1 s apart, 5 times, then
  given up with one warning and the arrangement kept.
- **Deferral.** While a modal dialog is open (the server configuration dialog works on a **copy** of the arrangement and
  would overwrite a change) nothing is applied; it is tried again every second and made when the dialog is closed.
- **Look.** The exact layout is put back, else the most similar one, else the fallback below. A failure (the layout would
  overlap, say) changes **nothing** (all or nothing: the new arrangement is worked out and checked in memory, then set, then
  written once), is told with a warning and a notice, and is not tried again until the displays change once more.
- **Restart.** After a change the core is restarted **only if it runs and the configuration it would be given now is not the
  one it was started with** (the text of `deskflow-server.conf` against what `ServerConfig` generates now), and not more than
  once in 10 s: a second restart that is due sooner waits, and is dropped if it is not needed any more when the wait ends. A
  restart does not change the displays, so it cannot start another look: there is no loop (tested with random sequences).
- **Undo.** One step, in memory: the arrangement as it was before the last change. In the main window it applies to the
  program's own arrangement; in the arrangement panel (a dialog is open) it applies to the copy that the dialog works on, and
  **Save** then remembers it. A profile that the change made is removed by Undo. Saving the arrangement, or dismissing the
  notice, ends the possibility.

## The fallback: put the computers against the server again

When nothing is remembered for the new displays, the arrangement is kept, but the computers that were next to the server and
are out of reach now would stay lost. `reattachToServer` (pure, on `arrangement::Arrangement`): the computers that touch each
other are one group that moves as it is; a group that touched the server before (the old displays are the stored `active`
situation) and touches it no more is moved so that its computer with the longest contact is against the **same side of the
box of the new displays at the same fraction of that side**, sliding along it in steps of 20 units to avoid an overlap; the
result is checked with the derived links (the group touches the server, nothing overlaps) or the group stays where it was and is
reported. A group that still touches is never moved, nor one that never touched the server. It is used only when the
configuration takes the displays of the server from the platform as the signature has them (the rectangles are the same): a
platform that tells one rectangle for the whole screen is not the displays of the signature, and the computers are not put
against a screen that is not there. What it made is then remembered for the new displays.

## The user interface (minimal)

In the arrangement panel of the server configuration dialog: the check box (**Remember the arrangement for each set of
displays**), **Remembered layouts...** (a read only list: name, when it was used, which displays; **Rename**, **Forget**,
**Save current arrangement as...**; a line that says what the displays are now, to be copied for a report), and a one line
notice with **Undo** and **Dismiss** that is shown only when there is something to say. The same notice bar is at the top of
the main window (hidden otherwise) and the change is told in the tray when the window is hidden. Names, forgetting and saving
under a name are done at once, not when the dialog that opened the list is accepted. Strings are English and translatable;
`translations/*.ts` are not part of this change.

## The log (INFO, in the log of the GUI)

```
layout memory: on, 2 remembered layouts, displays=ba4a8180 (2 displays: external 3440x1440@100% at 0,0 (main) [10ac:a0c4] + ...)
layout memory: displays changed old=ba4a8180 (2 displays) new=c3d4e5f6 (1 display: built-in 1512x982@200% at 0,0 (main) [0610:a050])
layout memory: restored "<name>" (exact) old=ba4a8180 new=c3d4e5f6 restart=yes
layout memory: restored "<name>" (similar, score 0.875) old=... new=... restart=no
layout memory: no remembered layout for new=c3d4e5f6; kept the arrangement as "<name>" (re-attached: PC2, MINI; left alone: none) restart=yes
layout memory: undo: back to the arrangement before old=ba4a8180 new=c3d4e5f6 restart=no
```

`restart=` is `yes`, `no` (not needed, or the core does not run) or `later` (it waits for the 10 s gap). The display ids are
hashes. "Signature unchanged" is a DEBUG line (shown with the debug log of the GUI).

## Platforms

- **macOS (what is wired and run here):** the displays from Quartz (`osxOnlineDisplayDetails()`: the UUID, the maker, the
  model, the serial number and the built in flag, only asked). The **online** displays are listed, not only the active ones: a
  display that is asleep is not active, and the situation must not change when the screens go to sleep. A mirror is online
  with the rectangle of the display it mirrors and counts once.
- **Windows and Linux (compiled, not run):** the same logic with `QtDisplayProvider` (`QScreen` geometry, the scale, and the
  maker, model and serial number that Qt knows; the built in display is guessed from the name `eDP`, `LVDS`, `DSI`). On these
  platforms the server's own displays in `ServerConfig::serverMonitors()` are one rectangle (the size of the screen), so the
  signature is finer than what the arrangement uses and **the fallback re-attach is not used** (the rectangles differ); a
  remembered layout is still put back. Windows monitor identification through `MSWindowsMonitors` (the device path, the EDID
  based id) is **not** wired. Nothing is claimed about them beyond that it compiles in the shared files.

## Where it is

| Where | What |
|---|---|
| `src/lib/gui/layout/DisplaySignature.{h,cpp}`, `LayoutMatcher.{h,cpp}` | the signature, the exact key, the similarity and the tie breaks |
| `Layout.{h,cpp}`, `LayoutProfileStore.{h,cpp}` | what is remembered; the file, the format, the limits, the eviction |
| `Reattach.{h,cpp}` | the fallback |
| `LayoutMemory.{h,cpp}` | the state machine, `ILayoutHost` |
| `ServerConfigLayout.{h,cpp}`, `ServerConfigLayoutHost.{h,cpp}` | capture and apply on a `ServerConfig`, the gate, the comparison with the running configuration |
| `LayoutMemoryService.{h,cpp}`, `DisplayProvider.{h,cpp}` | the Qt side: timers, screens, the notice; the display readers |
| `src/lib/platform/OSXDisplays.{h,cpp}` | `osxOnlineDisplayDetails()` (the only platform code) |
| `src/lib/gui/widgets/ArrangementPanel`, `LayoutNoticeBar`, `dialogs/LayoutProfilesDialog`, `ServerConfigDialog`, `MainWindow` | the interface and the wiring |
| `src/lib/common/Settings.h` | `server/rememberArrangements` |
| `src/lib/gui/config/ServerConfig` | `arrangementWith(displays)` (the arrangement under other displays); nothing else changed |

## Tests

| Test | What it pins |
|---|---|
| `DisplaySignatureTests` | the order, mirrors, invalid displays, every part of the key, the id, the summary, JSON, the locale |
| `LayoutMatcherTests` | exact, similar, stand-ins, subsets, the threshold, the tie breaks, seeded properties |
| `LayoutStoreTests` | the round trip, damaged and newer files, invalid entries, the size cap, eviction, atomic save, a fuzz of damaged files |
| `LayoutReattachTests` | the T to the laptop, every side, the fraction, a group that cannot be placed, zoom, seeded properties |
| `LayoutMemoryTests` | the state machine with a clock that is moved (no sleeps): the burst, the maximum wait, an unchanged signature, exact, similar, none, undo, the restart rules, deferral, unreadable displays, a failure, the log lines, random sequences |
| `LayoutHostTests` | the real `ServerConfig` and settings: capture and apply, only arrangement keys written, the gate, the whole round trip (the unplugged laptop, and back to the very same configuration text), the feature off |
| `LayoutProfilesDialogTests`, `ServerConfigDialogTests`, `DialogLayoutTests` | the window, the notice and Undo in the copy of a dialog, the switch saved and restored, the layouts in three styles at two font sizes |
| `OSXDisplaysTests` | the displays are only asked, and are the ones of the info |

## Not done, and not verified

- A real change of displays on a real Mac (an unplugged monitor, the lid, sleep and wake): the tests drive the state machine
  with a clock and the real configuration with displays that they change, not a window server.
- Windows and Linux servers (above).
- The feel of the notice bar in the main window with a log expanded or not (the window keeps its size by changing it by the
  height of the bar, which was tested only in the dialogs).
- Two situations that have the same displays but a different main display are told apart only by the key; the similarity
  says 0.95 for them.
- The clients' monitors are not in the key (a decision, above).
