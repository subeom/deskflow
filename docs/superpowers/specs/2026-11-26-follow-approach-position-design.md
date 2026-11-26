# Follow-Approach Cursor Position

**Date:** 2026-11-26

**Status:** Approved for immediate implementation

**Base:** `74150e1e16cb40eaa539331de344af34cac6bb0a`

## Goal

Add an optional `Follow approach` placement mode so a companion stops naturally
on the side from which it approached the cursor. The mode applies to Cat, Dog,
and Turtle, uses their existing pursuit heading and speed cap, and does not
replace or change the four fixed placement modes or their default.

## Settings behavior

Add a fifth entry to the existing **Settings > Keyboard & mouse > Input source
indicator > Position from cursor** combo:

- `Follow approach` -> stored value `cursorFollowApproach`;
- Korean label `접근 방향 따라가기`.

Keep the existing values and behavior:

- `Bottom right` -> `cursorBottomRight` (default);
- `Top right` -> `cursorTopRight`;
- `Bottom left` -> `cursorBottomLeft`;
- `Top left` -> `cursorTopLeft`.

The setting remains live. Changing placement while the indicator is visible
updates the controller position mode and triggers a normal bounded panel move;
it does not hide, re-show, replace the panel, or reset pursuit state.
Companion follow speed, source change, Space change, and opacity/tone changes
remain independent.

## Pure geometry

The existing fixed-mode behavior is preserved exactly. The new helper input is
an optional valid approach heading:

```cpp
std::optional<double> approachHeadingRadians
```

For `CursorFollowApproach`, an absent or non-finite heading uses the exact
existing Bottom right calculation.

Given cursor point `p`, visible badge width `W = 33`, height `H = 31`, cursor
gap `g = 7`, and unit approach direction:

```text
d = (cos(theta), sin(theta))
support = abs(d.x) * W/2 + abs(d.y) * H/2
center = p - d * (support + g)
origin = (center.x - W/2, center.y - H/2)
```

The rectangle's point closest to the cursor is therefore exactly 7 logical
points from the cursor in unconstrained screen space. A companion approaching
from the left stops to the left, and a companion approaching diagonally stops
behind that diagonal direction.

After computing the desired origin, clamp it to the same screen that contains
the cursor:

```text
x = clamp(origin.x, screen.x + margin,
          screen.x + screen.width - margin - W)
y = clamp(origin.y, screen.y + margin,
          screen.y + screen.height - margin - H)
```

The helper never chooses another screen, never follows an invalid screen, and
returns no value only for invalid geometry or a cursor outside all valid
screens. At an edge/corner the clamped rectangle stays on-screen; an inward
7 pt cursor gap remains exact whenever the requested support point fits.

## Two-stage pursuit integration

The target helper runs before the pursuit update because pursuit needs a target
origin. Follow approach therefore uses:

1. the previous valid approach heading to compute this frame's target;
2. the bounded pursuit call to move toward that target;
3. the new pursuit state heading to become the next frame's approach heading
   only when the step moved a meaningful distance.

The panel keeps:

```cpp
bool approachHeadingValid = false;
double approachHeadingRadians = 0;
```

The panel keeps the previous raw cursor point as well as the previous
pursuit position. On a successful non-initial, non-reached pursuit step:

- if `distanceMoved > 0.25 pt` and the new state heading is finite, retain that
  heading and mark it valid;
- a meaningful raw cursor delta supplies the approach heading exactly; when no
  valid heading exists and the cursor itself did not move, seed it from the
  cursor minus the previous pursuit **center** (origin plus half badge size);
- if the normalized raw-heading delta is at most `0.01 rad`, keep the prior
  heading; this sub-degree deadband prevents whole-point AppKit rounding from
  moving the dynamic support target after the companion has effectively
  settled;
- while Idle, within settled distance, or during no/zero movement, keep the
  previous heading;
- never derive or overwrite heading on a reached/no-movement tick;
- Reduced Motion still places directly, ignores speed as before, and retains
  the prior heading when present.

The first `show`, a reset/uninitialized state, invalid heading, and a
zero-length approach use Bottom right fallback. First-show direct placement is
the existing initialization path, not a new snap.

This staged update prevents the dynamic target from circularly recomputing from
its own future value. Large heading changes produce a bounded pursuit toward a
new side of the cursor, so the panel passes through intermediate positions
instead of jumping between corners.

## Live mode and state preservation

`positionFromSetting` maps only the exact string `cursorFollowApproach` to the
new enum. Unknown values continue to fall back to Bottom right.

Controller `settingsChanged` handles `InputIndicatorPosition` as a direct live
mode update:

- update `m_position`;
- preserve `m_panelConfigured`;
- do not call `hide` or `show`;
- call the normal move path immediately when the panel is configured.

This differs from opacity/companion/tone reconfiguration, but preserves the
same fixed-mode UI and defaults. Source, Space, speed, and mode changes do not
reset the approach heading or initialized pursuit position.

## Architecture decision

Three approaches were considered:

1. **Choose among four fixed corners from heading quadrants.** Rejected because
   it jumps at quadrant boundaries and never uses diagonal support distance.
2. **Orbit the final panel origin immediately from heading.** Rejected because
   it can teleport and violates the configured follow-speed cap.
3. **Previous-heading support target + bounded pursuit + retained updated
   heading.** Selected because it is mathematically exact, continuous, and
   shares the existing no-overshoot engine.

## Test contract

### Pure geometry

Tests cover eight approach angles: 0°, 45°, 90°, 135°, 180°, 225°, 270°, and
315°. For each:

- the desired rectangle center lies opposite the approach direction;
- before clamp, the cursor-to-rectangle support gap is exactly 7 pt;
- the resulting origin lies within the cursor screen;
- the equivalent fixed-mode result remains byte-for-byte unchanged.

No/NaN/infinite optional heading falls back exactly to Bottom right. Invalid
cursor, zero badge, invalid screen, or screen too small returns no target.

### Edge and multi-screen

Tests cover all four edges and four corners, plus adjacent screens with gaps.
The target clamps to the cursor's own screen and screen margin. Follow mode
does not cross or choose a different screen.

### State and controller

Tests prove:

- the fifth combo item maps/stores `cursorFollowApproach`;
- default remains `cursorBottomRight`;
- an exact Follow approach setting reaches `panelMove`;
- position setting change does not call `panelShow` or `panelHide`;
- live speed/source/Space changes preserve panel/pursuit state;
- first show falls back without teleport beyond initialization;
- last valid heading is retained while Idle;
- a large heading reversal causes multiple bounded intermediate positions.

### Native/raster

For Cat, Dog, and Turtle at 1x/2x and 50/100/200:

- all intermediate positions stay on-screen and in the 49 x 49 canvas;
- per-step movement does not exceed the configured cap plus AppKit whole-point
  quantization;
- final visible anchor lies on the expected approach side within 1 pt/axis;
- paint, glyph, and outside-pixel counts remain zero;
- old fixed-mode pixel behavior is unchanged.

### Regression

The 15 focused Release targets remain green. The full safe Release split runs
every registered test except only `OSXClipboardTests` and
`OSXKeyStateTests`, with Geometry isolated for WindowServer.

## Scope

In scope: one enum/UI value, exact Korean label, pure support geometry, direct
live controller placement updates, retained approach heading in panel state,
native/pure tests, docs, and commits.

Out of scope: changing fixed offsets, cursor/source APIs, speed model,
rotation/facing model, glyph behavior, panel size, new companion types,
translations beyond Korean, release, install, push, and pull request.
