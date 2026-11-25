# Cat Target Pursuit Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the visible Cat traverse every cursor/future-caret transition at bounded Walk/Run speed, rotate diagonally, and keep its glyph upright inside a 49 x 49 transparent canvas without changing the 33 x 31 visible anchor.

**Architecture:** Add a pure pursuit state/step and body-point transform to `InputIndicatorGeometry`. `MacInputIndicatorPanel` preserves the Cat's initialized position across source/Space changes, advances it at 8 ms, and renders the same Cat art in a transparent Cat-only canvas while the 33 x 31 visible-origin geometry remains authoritative.

**Tech Stack:** C++20, Objective-C++, AppKit/CoreText, Qt 6.12 Test, CMake/Ninja, CTest.

---

## File Structure

- Modify: `src/lib/gui/input/InputIndicatorGeometry.h`
  - Define Pointer/Caret target, pursuit state/step, bounded speed constants,
    heading helpers, and Cat body-world point transform.
- Modify: `src/lib/gui/input/InputIndicatorGeometry.cpp`
  - Implement no-teleport pursuit, 180 ms settle, shortest-angle smoothing,
    and mirror/scale/rotation point transform.
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.h`
  - Add Cat canvas metrics, canvas coordinate helpers, target provider seam,
    and heading-aware Cat label bounds.
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.mm`
  - Use pure pursuit for Cat only, preserve initialized position, drive gait
    from actual velocity, render heading-rotated paths, and offset 49 x 49
    canvas without changing the visible anchor.
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
  - Register pure pursuit, heading, canvas, panel path, and reset tests.
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.mm`
  - Add red-first state tests and 49 x 49/actual panel pixel coverage.
- Modify: `src/unittests/gui/input/InputIndicatorPolicyTests.cpp`
  - Extend controller tests only if render timer/source/Space behavior needs
    an explicit contract.
- Use: `docs/superpowers/specs/2026-11-25-cat-target-pursuit-design.md`
  - Authoritative behavior, speed, canvas, reset, and caret limits.

## Safe Variables

```bash
export REPO="$PWD"
export BUILD="/tmp/deskflow-cat-pursuit-0e04e54a-release"
export SCRATCH="/tmp/deskflow-cat-pursuit-0e04e54a-scratch"
export SAFE_TMP="/Users/subeomlee/.copilot/session-state/0e04e54a-1ee4-49a1-867f-877e8413aab6/files/cat-pursuit-tmp"
export QT_ROOT="$(qmake6 -query QT_INSTALL_PREFIX)"
mkdir -p "$SCRATCH/home" "$SCRATCH/cache" "$SCRATCH/config" \
  "$SCRATCH/tmp" "$SAFE_TMP"
```

`qmake6 -query QT_VERSION` must return `6.12.0`.

### Task 1: Add Pure Pursuit and Heading Tests First

**Files:**
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.mm`

- [ ] **Step 1: Register pure pursuit test slots**

Add:

```cpp
void catPursuitBoundsLargeJumpAndNeverTeleports();
void catPursuitWalksNearAndSettlesWithoutOvershoot();
void catPursuitHandlesEightHeadingsDeterministically();
void catPursuitRejectsInvalidAndHonorsReducedMotion();
void catPursuitTreatsPointerAndCaretTargetsIdentically();
void catPursuitPreservesInitializedStateAcrossRepresentationChanges();
```

- [ ] **Step 2: Write the failing large-jump test**

Use the desired API:

```cpp
InputIndicatorCatPursuitState state;
InputIndicatorTarget target{
    .point = {.x = 1000, .y = 0},
    .kind = InputIndicatorTargetKind::Pointer,
};

std::optional<InputIndicatorCatPursuitStep> initial =
    inputIndicatorCatPursuitAdvance(
        state, target, 125000, 8, true, false
    );
QVERIFY(initial);
QVERIFY(initial->state.initialized);
QCOMPARE(initial->state.position.x, 1000.0);
```

First, cover initialization separately using `initialize = true`.
Then create an initialized state at `{0,0}` and call with `initialize=false`;
require Run, `distanceMoved <= 5.7600001`, `position.x > 0`, and
`position.x < 1000`.

Loop with 8 ms steps until reached. For every step:

```cpp
QVERIFY(step.state.position.x >= previous.x);
QVERIFY(step.state.position.x <= 1000.0);
QVERIFY(step.distanceMoved <= 5.7600001);
QCOMPARE(
    step.state.motionState,
    InputIndicatorMotionState::Run
);
```

Require more than 100 unique positions and exact final target.

- [ ] **Step 3: Write near/settle/no-overshoot tests**

Start at `{0,0}` and target `{10,0}`. Require Walk and a first 8 ms move at
most 0.96 pt. Continue to reach exact `{10,0}`, then call with no movement at
179 ms and 180 ms:

```cpp
QCOMPARE(at179.state.motionState, InputIndicatorMotionState::Walk);
QCOMPARE(at180.state.motionState, InputIndicatorMotionState::Idle);
```

Approach from every side and assert each new point is on the segment and
never passes the target.

- [ ] **Step 4: Write heading and transform tests**

Feed eight target unit vectors and assert pursuit heading within tolerance of
the expected angle. Drive sequential 45 degree changes and assert the angle
step is bounded by `10 rad/s * elapsed`.

Call:

```cpp
inputIndicatorCatBodyPoint(
    {.x = 20, .y = 10},
    true, 1.0, heading
);
```

At heading 0 retain x=20/y=10. At right/down/up/diagonal verify expected
rotated coordinates. For left-facing, verify the relative heading uses
`heading - pi` and points in the target direction.

- [ ] **Step 5: Write invalid/reduced/target-kind tests**

Reject NaN/inf state/target, negative elapsed, or target speed. Reduced Motion
must return target exactly and retain target kind. Run the same state/target
with `Pointer` and `Caret` and compare position, velocity, heading, gait, and
distance.

- [ ] **Step 6: Write reset-preservation test**

Start with an initialized pursuit state. Copy it through the same
source/representation path the panel will use and assert position, velocity,
heading, and initialized values remain unchanged. Initial/uninitialized is the
only direct placement path; no helper may replace initialized state with
`initialize=true`.

- [ ] **Step 7: Run red verification**

```bash
cmake -S "$REPO" -B "$BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$QT_ROOT" \
  -DCMAKE_COMPILE_WARNING_AS_ERROR=ON \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14 \
  -DBUILD_TESTS=ON \
  -DSKIP_BUILD_TESTS=ON \
  -DBUILD_INSTALLER=OFF \
  -DBUILD_DEV_DOCS=OFF
cmake --build "$BUILD" --config Release --parallel 8 \
  --target InputIndicatorGeometryTests
```

Expected: FAIL because target/pursuit/body transform APIs do not exist.

### Task 2: Implement Pure Bounded Pursuit and Body Transform

**Files:**
- Modify: `src/lib/gui/input/InputIndicatorGeometry.h`
- Modify: `src/lib/gui/input/InputIndicatorGeometry.cpp`

- [ ] **Step 1: Add target and pursuit types**

Declare:

```cpp
enum class InputIndicatorTargetKind : unsigned char
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
  double stoppedMilliseconds = 0;
  InputIndicatorMotionState motionState =
      InputIndicatorMotionState::Idle;
  bool initialized = false;
};

struct InputIndicatorCatPursuitStep
{
  InputIndicatorCatPursuitState state;
  InputIndicatorPoint movement;
  InputIndicatorTargetKind targetKind =
      InputIndicatorTargetKind::Pointer;
  double distanceMoved = 0;
  double speedPointsPerSecond = 0;
  bool reached = false;
};
```

Declare:

```cpp
std::optional<InputIndicatorCatPursuitStep>
inputIndicatorCatPursuitAdvance(
    const InputIndicatorCatPursuitState &state,
    const InputIndicatorTarget &target,
    double targetSpeedPointsPerSecond,
    double elapsedMilliseconds,
    bool initialize,
    bool reducedMotion);

double inputIndicatorNormalizedAngle(double radians);

InputIndicatorPoint inputIndicatorCatBodyPoint(
    InputIndicatorPoint point,
    bool facesRight,
    double facingHorizontalScale,
    double headingRadians);
```

- [ ] **Step 2: Implement first/Reduced Motion placement**

If `initialize` or `reducedMotion`:

```cpp
state.position = target.point;
state.velocity = {};
state.initialized = true;
state.motionState = Idle;
state.stoppedMilliseconds = 0;
```

Return `reached = true`. This is the only direct movement path.

- [ ] **Step 3: Implement bounded velocity selection**

Constants:

```cpp
kWalkSpeed = 120
kRunSpeed = 720
kRunDistance = 48
kRunTargetSpeed = 260
kReachedDistance = 0.25
kPursuitFrameCap = 8
```

Run when distance >=48, target speed >=260, or the previous state is Run and
distance >12. Otherwise Walk. Cap simulated elapsed at 8 ms and movement at
speed * elapsed. Step only as far as remaining target distance.

- [ ] **Step 4: Implement settle and heading**

If no movement, add full elapsed to `stoppedMilliseconds`. If it reaches 180,
set Idle. If movement occurs, reset stopped time and set Walk/Run.

Rotate `headingRadians` by at most 10 rad/s toward `atan2(deltaY, deltaX)`
using shortest-angle normalization. Actual velocity is movement / simulated
seconds.

- [ ] **Step 5: Implement Cat body point transform**

In source coordinates:

```cpp
if (!facesRight)
  point.x = 30 - point.x;
point.x = 15 + (point.x - 15) * facingHorizontalScale;
relative = normalize(facesRight
    ? headingRadians
    : headingRadians - pi);
rotate around (15,14)
```

- [ ] **Step 6: Run pure tests green**

```bash
cmake --build "$BUILD" --config Release --parallel 8 \
  --target InputIndicatorGeometryTests
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH/home" \
  XDG_CACHE_HOME="$SCRATCH/cache" \
  XDG_CONFIG_HOME="$SCRATCH/config" TMPDIR="$SAFE_TMP" \
  ctest --test-dir "$BUILD/src/unittests" \
  -R '^InputIndicatorGeometryTests$' --output-on-failure
```

Expected: all pure pursuit bounds, heading, settle, invalid, reduced, and
Pointer/Caret equivalence tests pass.

### Task 3: Add Cat Canvas and Panel Pursuit Tests First

**Files:**
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.mm`
- Modify: `src/unittests/gui/input/InputIndicatorPolicyTests.cpp` if a
  controller timer contract gap is exposed.

- [ ] **Step 1: Register panel/canvas tests**

Add:

```cpp
void catPursuitPanelTraversesLargeTargetJump();
void catPursuitPanelPreservesPositionOnSourceAndSpaceChange();
void catPursuitRotatesSilhouetteButKeepsGlyphUpright();
void catPursuitCanvasContainsEightHeadings();
void catPursuitCaretTargetFollowsSamePath();
```

- [ ] **Step 2: Write the failing panel intermediate-position test**

Inject a mutable target provider:

```cpp
InputIndicatorTarget target{
    .point = fixedCursorPoint,
    .kind = InputIndicatorTargetKind::Pointer,
};
MacInputIndicatorPanel panel({}, [&target] {
    return target;
});
```

Show at target, mutate `point.x += 1000`, and call move every 8 ms.
Read visible panel position from the NSPanel origin plus Cat canvas offset.
Require:

- first post-jump movement <=5.76 pt;
- at least 20 unique positions before reaching;
- Run interval 8;
- no position moves away from target;
- final point is exact.

- [ ] **Step 3: Write source/Space no-snap tests**

Start a far Run, capture the Cat's visible anchor, call `show` for a Korean
source change, then call move. Require the first visible origin moves by no
more than the pursuit step, never to target.

Post
`NSWorkspaceActiveSpaceDidChangeNotification` to the panel's workspace
notification center, call move, and require the same bounded continuation.

- [ ] **Step 4: Write diagonal glyph/canvas tests**

For headings at `0,45,90,135,180,225,270,315` degrees, render Cat to a
49 x 49 padded bitmap with the 1.10 scale. Assert:

- `outsidePixels == 0`;
- silhouette angle differs per diagonal;
- glyph mask is axis-upright (same raster orientation) while its center
  follows the rotated torso;
- source startle and tail-extreme combinations remain inside.

- [ ] **Step 5: Write caret seam test**

Run the same large target path with `Pointer`, repeat with `Caret`, and compare
the exact sequence of panel positions, not only pure state. Confirm no source
code reads caret/AX data.

- [ ] **Step 6: Run red panel/canvas tests**

```bash
cmake --build "$BUILD" --config Release --parallel 8 \
  --target InputIndicatorGeometryTests
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH/home" \
  XDG_CACHE_HOME="$SCRATCH/cache" \
  XDG_CONFIG_HOME="$SCRATCH/config" TMPDIR="$SAFE_TMP" \
  "$BUILD/src/unittests/gui/input/InputIndicatorGeometryTests" \
  catPursuitPanelTraversesLargeTargetJump
```

Expected: FAIL because the panel still jumps and has no target provider/49 px
canvas/heading state.

### Task 4: Integrate Bounded Pursuit into the Cat Panel

**Files:**
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.h`
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.mm`

- [ ] **Step 1: Replace the pointer-only panel dependency**

Rename the internal test seam from point-only to target:

```cpp
using TargetLocation =
    std::function<InputIndicatorTarget()>;
```

Production default:

```cpp
const auto cursor = NSEvent.mouseLocation;
return InputIndicatorTarget{
    .point = {.x=cursor.x, .y=cursor.y},
    .kind = InputIndicatorTargetKind::Pointer,
};
```

- [ ] **Step 2: Preserve initialized Cat position**

Remove `followerInitialized = false`, `previousTargetInitialized = false`,
and motion-state reset from ordinary `show` and active-Space notification.
Do not snap `m_impl->follower`.

Use `m_impl->pursuitState` for Cat. Dog/Turtle retain the existing follower
branch.

- [ ] **Step 3: Advance Cat pursuit before gait selection**

After obtaining the cursor-relative visible target, calculate elapsed since
the prior panel move and target speed. For Cat:

```cpp
const auto pursuit =
    inputIndicatorCatPursuitAdvance(
        m_impl->pursuitState,
        {.point=*target, .kind=targetLocation.kind},
        pointerSpeed,
        elapsedMilliseconds,
        !m_impl->pursuitState.initialized,
        reducedMotion
    );
```

Fail closed if the result is null. Set `follower` and `motion` from the actual
movement, update `motionState`/phase from `pursuit.state.motionState`, and keep
the old Dog/Turtle easing branch unchanged.

- [ ] **Step 4: Keep 8/75 ms timer tied to actual pursuit**

Use:

```cpp
animating || followerDistance > 0.25
```

and motion state from pursuit. A far Run remains on 8 ms. Idle tail/startle
continue on their existing 75/8 ms conditions. Reduced Motion returns zero
timer.

- [ ] **Step 5: Run panel path tests green**

```bash
cmake --build "$BUILD" --config Release --parallel 8 \
  --target InputIndicatorGeometryTests InputIndicatorPolicyTests
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH/home" \
  XDG_CACHE_HOME="$SCRATCH/cache" \
  XDG_CONFIG_HOME="$SCRATCH/config" TMPDIR="$SAFE_TMP" \
  ctest --test-dir "$BUILD/src/unittests" \
  -R '^(InputIndicatorGeometryTests|InputIndicatorPolicyTests)$' \
  --output-on-failure
```

Expected: no source/Space snap, bounded intermediate path, exact final target.

### Task 5: Add Heading Rotation and the 49 x 49 Cat Canvas

**Files:**
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.h`
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.mm`

- [ ] **Step 1: Add Cat canvas fields**

Extend `MacInputIndicatorPanelStyle`:

```cpp
double canvasWidth = 0;
double canvasHeight = 0;
double canvasOffsetX = 0;
double canvasOffsetY = 0;
```

Cat returns 49,49,8,9. Dog/Turtle return 33,31,0,0. Keep width/height=33/31
for target positioning and existing metrics.

Add:

```cpp
macInputIndicatorCanvasPoint(point, companion)
macInputIndicatorCanvasRect(rect, companion)
macInputIndicatorCanvasOrigin(style)
```

- [ ] **Step 2: Offset panel origin and draw context**

Set NSPanel content size to canvas dimensions. Keep
`positionInputIndicator` inputs at width/height 33/31. Before
`setFrameOrigin`, subtract `canvasOffsetX/Y` from follower. In drawRect,
translate by `canvasOffset + macInputIndicatorDisplayPoint({})`.

- [ ] **Step 3: Transform Cat path points**

Pass pursuit `headingRadians`, `facesRight`, and `facingScaleX` into
`catArtworkPath`. After torso/tail/startle deformation, apply
`inputIndicatorCatBodyPoint`.

Remove the AppKit mirror/turn concatenation from Cat paths so the same pure
point transform controls tests and painting.

- [ ] **Step 4: Transform only the label center**

Compute the label rectangle center with
`inputIndicatorCatBodyPoint` from the source inner-label center. Keep its 8x7
width/height axis-upright and do not rotate/mirror the glyph itself.

Use the same transformed near contour as the final clipping path.

- [ ] **Step 5: Copy heading into the view**

Add `catBodyHeadingRadians` to `DeskflowInputIndicatorView`, redraw
comparison, and assignments. No rotation property is used by Dog/Turtle.

- [ ] **Step 6: Run rotation/clipping tests green**

```bash
cmake --build "$BUILD" --config Release --parallel 8 \
  --target InputIndicatorGeometryTests
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH/home" \
  XDG_CACHE_HOME="$SCRATCH/cache" \
  XDG_CONFIG_HOME="$SCRATCH/config" TMPDIR="$SAFE_TMP" \
  "$BUILD/src/unittests/gui/input/InputIndicatorGeometryTests" \
  catPursuitCanvasContainsEightHeadings
```

Expected: 49 x 49 all-heading cases report `outsidePixels 0`; the label is
upright and inside.

### Task 6: Run Safe Focused and Full Release Gates

**Files:**
- Modify: none unless a change-caused failure exists.

- [ ] **Step 1: Run focused 7/7 gate**

Use the same named target list from Task 4 and the project test regex for the
seven indicator suites.

- [ ] **Step 2: Run full safe gate**

Build every named CTest target, then run:

```bash
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH/home" \
  XDG_CACHE_HOME="$SCRATCH/cache" \
  XDG_CONFIG_HOME="$SCRATCH/config" TMPDIR="$SAFE_TMP" \
  ctest --test-dir "$BUILD/src/unittests" \
  -E '^(OSXClipboardTests|OSXKeyStateTests)$' \
  --output-on-failure
```

Record the actual isolated count and any extra combined close-fix count
without merging that commit.

### Task 7: Commit and Handoff Both Branch Layers

**Files:**
- Modify: none unless generated translations need restore.

- [ ] **Step 1: Restore generated translations**

```bash
git restore --worktree -- \
  translations/deskflow_es.ts \
  translations/deskflow_it.ts \
  translations/deskflow_ja.ts \
  translations/deskflow_ru.ts \
  translations/deskflow_zh_CN.ts
```

- [ ] **Step 2: Commit pursuit separately**

```bash
git add \
  src/lib/gui/input/InputIndicatorGeometry.h \
  src/lib/gui/input/InputIndicatorGeometry.cpp \
  src/lib/gui/input/MacInputIndicatorPanel.h \
  src/lib/gui/input/MacInputIndicatorPanel.mm \
  src/unittests/gui/input/InputIndicatorGeometryTests.h \
  src/unittests/gui/input/InputIndicatorGeometryTests.mm
git commit \
  -m "feat(input): pursue cat targets without teleporting" \
  -m "Co-authored-by: Copilot App <223556219+Copilot@users.noreply.github.com>"
```

- [ ] **Step 3: Audit and report**

Report startle and pursuit commit SHAs separately, bounded path evidence,
1x/2x rotated canvas pixels, 49/33 geometry, 7/7 and full counts, clean
worktree, and exact limitation: engine accepts `Caret` targets but production
Copilot/Edge fallback remains `Pointer`.
