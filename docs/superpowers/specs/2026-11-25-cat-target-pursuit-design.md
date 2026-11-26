# Cat Target Pursuit

**Date:** 2026-11-25
**Status:** Approved for immediate implementation
**Revision base:** `5966a4d95d038332c013ea52c8c93242b525aaa9`

## Goal

The visible Cat must never teleport after a cursor or future exact-caret target
change. It walks when the target is near and slow, runs when the target is far
or fast, traverses every intermediate 8 ms position, and settles exactly
without overshoot.

The production target remains the native pointer. The movement engine accepts a
typed `Pointer` or `Caret` target seam for future native caret sources. It does
not read application text, use private cursor/Accessibility/IMK APIs, or
invent caret coordinates.

## Teleport Root Cause

The current follower computes a normal 0.5 eased point, then discards it when
the remaining distance exceeds 9 pt:

```text
next = target - 9 pt toward previous
```

A 100 pt target jump therefore moves the Cat 91 pt in one frame. `show` and
active-Space invalidation also reset `followerInitialized`, turning an already
visible Cat into a first-frame snap.

This design removes that branch for Cat pursuit. The legacy easing remains
available to Dog/Turtle.

## Pure Pursuit Model

Add a pure model in `InputIndicatorGeometry`:

```cpp
enum class InputIndicatorTargetKind
{
  Pointer,
  Caret
};

struct InputIndicatorTarget
{
  InputIndicatorPoint point;
  InputIndicatorTargetKind kind =
      InputIndicatorTargetKind::Pointer;
};

struct InputIndicatorCatPursuitState
{
  InputIndicatorPoint position;
  InputIndicatorPoint velocity;
  double headingRadians = 0;
  bool initialized = false;
};
```

Each 8 ms step returns:

- the bounded next visible position;
- actual movement velocity;
- `InputIndicatorMotionState` (`Idle`, `Walk`, or `Run`);
- smoothed heading;
- target kind;
- whether the exact target was reached.

Rules:

| Condition | Result |
| --- | --- |
| Invalid point, elapsed, or target | reject with no state |
| First visible Cat | appear at target, as there is no prior visible position |
| Reduced Motion | move directly to target |
| remaining distance <= 0.25 pt | settle exactly, no overshoot |
| remaining >= 48 pt or target speed >= 260 pt/s | Run |
| otherwise moving | Walk |
| just settled for <180 ms | retain Walk/Run gait |
| settled for >=180 ms | Idle |

Maximum chase speeds are 120 pt/s for Walk and 720 pt/s for Run. The requested
8 ms frame therefore moves at most 0.96 pt walking or 5.76 pt running. A stalled
input interval is capped to 8 ms of simulated pursuit per rendered frame, so
the Cat cannot consume a large elapsed gap as an unshown teleport.

The final segment snaps only the sub-step distance that is already inside the
current speed capsule. Monotonic position is guaranteed: every step lies on
the line segment from the previous position to the target and never crosses
the target.

`Pointer` and `Caret` use exactly the same pursuit math. The production
provider returns `Pointer`; tests inject `Caret` without enabling any caret or
Accessibility API.

## Gait and Timer

Pursuit supplies actual Cat speed to the gait selector, not raw pointer speed:

- Walk drives the existing Reach/Pass/Push artwork.
- Run drives the existing Coil/Air/Land artwork.
- Phase time continues from `motionStateStarted`.
- A target jump switches to Run on the current 8 ms timer and remains Run while
  traveling.
- Idle remains on the 75 ms low-duty timer after 180 ms settle.
- The existing startle reaction keeps its 8 ms requirement and suspends idle
  tail; it does not suspend bounded pursuit.

## Diagonal Heading

Heading is the smoothed direction of the actual follower displacement. It uses
the shortest angular path at 10 rad/s and does not infer direction from
`NSEvent` direction or the horizontal Cat mirror alone.

The Cat artwork remains right-facing in source coordinates. A pure world-point
transform applies:

1. persistent left/right mirror;
2. existing 0.80-1.00 facing-turn horizontal compression;
3. rotation around the Cat logical center.

The rotation angle is:

```text
facesRight: headingRadians
facesLeft: headingRadians - 180 degrees, normalized
```

This aligns up/down/diagonal art with screen-space pursuit while preserving
the existing facing turn.

The `A`/`한` glyph center follows the transformed torso center, but the glyph
rectangle is drawn axis-upright and never rotated or mirrored. Its width,
height, font preference, clipping, and startle behavior stay unchanged.

## Transparent Rotating Canvas

Keep the approved visual anchor:

- visible geometry/hitbox contract: 33 x 31 pt;
- pointer gap and screen margin: 7 pt;
- source logical art: 30 x 28 pt;
- display scale: 1.10.

Cat only receives a 49 x 49 pt noninteractive transparent render canvas. The
original 33 x 31 visible anchor is centered in it with an 8 pt X offset and a
9 pt Y offset. Dog and Turtle retain a 33 x 31 canvas.

`positionInputIndicator` continues to size/clamp the 33 x 31 visible anchor.
`MacInputIndicatorPanel` moves the NSPanel origin by `visible origin - canvas
offset`, so the Cat-to-cursor relationship does not change. The extra 16 pt
horizontal and 18 pt vertical are transparent rotation room only.

Native render measurement covers all 8 Cat headings at 1x and 2x, including
startle/idle-tail extremes. The rotated pixels stay inside 49 x 49 with no
change to the 33 x 31 screen-edge anchor.

## Reset and Visibility Rules

- first `show` with no previously visible Cat position may initialize at the
  target;
- ordinary source/style `show` preserves the initialized pursuit state;
- active Space changes invalidate presentation only and preserve position;
- source transition startle and idle tail continue over the moving Cat;
- an invalid/offscreen target hides without modifying pursuit state or
  warping offscreen;
- a later valid target resumes from the last visible position;
- Reduced Motion is the only direct-to-target movement exception.

## Alternatives Considered

1. **AppKit spring or linear animation.** Faster to prototype but hides
   position state from gait tests and is interrupt/reset dependent. Rejected.
2. **Bound follower without rotation.** Prevents teleporting but looks sideways
   on diagonal movement and leaves the glyph/canvas issue unresolved.
   Rejected.
3. **Pure velocity pursuit + transformed Cat canvas.** Makes every position,
   heading, and clip bound testable without AppKit timing. Selected.

## Test Contract

Pure tests prove:

- a 1000 pt jump moves <=5.76 pt per 8 ms and selects Run;
- every long-jump step is monotonic and never overshoots;
- a near target selects Walk and reaches exact target;
- all 8 cardinal/diagonal body angles are finite and expected;
- heading smoothing uses the shortest arc;
- Idle occurs after the 180 ms settled delay;
- invalid values return no step;
- Reduced Motion moves directly;
- source/startle target changes and Space/representation changes preserve the
  initialized pursuit state;
- Pointer and Caret target kinds produce identical positions/velocities.

Panel/native tests prove:

- a large target mutation produces multiple unique intermediate visible
  positions;
- final position is exact and frame interval is 8 ms until settled;
- diagonal Cat body pixels rotate while the glyph remains axis-upright;
- 49 x 49 canvas has `outsidePixels == 0` for all 8 headings, both scales,
  facings, startle extremes, and tail extremes;
- source and active-Space change do not snap;
- first visible show is the only direct initialization;
- existing startle 48 cases, 264 Cat paint cases, 33 x 31 anchor metrics,
  tail pixel gains, and 7 pt geometry all remain green.

## Current Caret Limitation

The branch exposes only the pursuit target seam. Current Copilot/Edge/
Chromium builds do not provide a reliable exact caret screen rectangle through
an allowed cross-application API, so production remains pointer-driven. No
fake caret position, Accessibility text read, or IMK/private API is introduced.
