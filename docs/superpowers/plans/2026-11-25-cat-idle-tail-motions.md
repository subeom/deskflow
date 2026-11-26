# Five Idle Cat Tail Motions Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add exactly five seeded, finite, subtle Cat idle tail motions without changing pointer locomotion, facing, label, glow, scale, or clipping behavior.

**Architecture:** Keep selection, progress, and tail offsets in the pure `InputIndicatorGeometry` layer. Advance that state in `MacInputIndicatorPanel`, pass a compact frame to the existing AppKit path painter, and deform only the near Cat tail region before the existing mirror/scale transforms.

**Tech Stack:** C++20, Objective-C++, AppKit/CoreText, Qt 6.12 Test, CMake/Ninja, CTest.

---

## File Structure

- Modify: `src/lib/gui/input/InputIndicatorGeometry.h`
  - Define the five motion IDs, seeded scheduler state, completed frame, and pure advance/deformation APIs.
- Modify: `src/lib/gui/input/InputIndicatorGeometry.cpp`
  - Implement the deterministic shuffle bag, finite motion envelopes, and tail-point warp.
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.mm`
  - Own scheduler time/state, gate it by Cat movement/Reduced Motion, copy frames to the view, and warp the near tail path.
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
  - Register focused pure-model and native-paint test slots.
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.mm`
  - Add red-first scheduler/reversal/rest tests and expand padded, scaled paint coverage.
- Use: `docs/superpowers/specs/2026-11-25-cat-idle-tail-motions-design.md`
  - Authoritative behavior and invariant source.

## Safe Build Variables

Every configure/build/test command runs with no live product launch. The scratch
environment is local to this worktree session:

```bash
export REPO="$PWD"
export BUILD="/tmp/deskflow-cat-tail-8dd955c0-release"
export SCRATCH="/tmp/deskflow-cat-tail-8dd955c0-scratch"
export QT_ROOT="$(qmake6 -query QT_INSTALL_PREFIX)"
mkdir -p "$SCRATCH/home" "$SCRATCH/cache" "$SCRATCH/config" "$SCRATCH/tmp"
```

`$QT_ROOT` is `/opt/homebrew` and `qmake6 -query QT_VERSION` must return
`6.12.0`.

### Task 1: Add the Pure Motion Tests First

**Files:**
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.mm`

- [ ] **Step 1: Register model test slots**

Add these `private Q_SLOTS` declarations:

```cpp
void catIdleTailMotionsCoverFiveFinitePatterns();
void catIdleTailMotionGatesMovementAndReducedMotion();
void catIdleTailMotionIsSeededAndFacingDeterministic();
```

- [ ] **Step 2: Add first-bag and bounded-pattern assertions**

Add a test that creates state with a fixed seed, advances at exact durations,
collects five IDs, and proves no adjacent repeat:

```cpp
auto state =
    inputIndicatorCatIdleTailMotionInitialState(0x13579bdfU);
std::vector<InputIndicatorCatIdleTailMotion> firstBag;
double now = 0;

for (int index = 0; index < 10; ++index) {
  const auto step =
      inputIndicatorCatIdleTailMotionAdvance(
          state, InputIndicatorMotionState::Idle, now, false
      );
  QVERIFY(step.frame.active);
  QVERIFY(std::isfinite(step.frame.progress));
  QVERIFY(step.frame.progress >= 0);
  QVERIFY(step.frame.progress < 1);
  if (!firstBag.empty()) {
    QVERIFY(
        firstBag.back() != step.frame.motion
    );
  }
  firstBag.push_back(step.frame.motion);
  state = step.state;
  now = state.startedMilliseconds +
      inputIndicatorCatIdleTailMotionDuration(
          step.frame.motion
      );
}

std::set<InputIndicatorCatIdleTailMotion> ids(
    firstBag.begin(), firstBag.begin() + 5
);
QCOMPARE(ids.size(), size_t{5});
```

For each ID, call
`inputIndicatorCatIdleTailOffset(motion, progress)` at 101 samples and assert
both offsets are finite and `abs(x) <= 1.60`, `abs(y) <= 2.60`. Assert that
sampled five-motion offset vectors differ pairwise.
Also require at least 20 changed tail pixels at 1x and 65 at 2x while all
non-tail and glyph pixels remain unchanged.

- [ ] **Step 3: Add idle/movement/reduced-motion assertions**

Select an initial Idle frame, then call the pure advance function with `Walk`
and `Run`. Require inactive frames at progress zero. Re-enter Idle at a later
timestamp and require progress zero plus a different ID from the interrupted
selection.

Start with a copy of fresh state and call advance with `reducedMotion = true`.
Require:

```cpp
QVERIFY(!reduced.frame.active);
QCOMPARE(reduced.state.seed, fresh.seed);
QCOMPARE(reduced.state.remainingMask, fresh.remainingMask);
QCOMPARE(reduced.state.hasLastMotion, false);
```

Apply an inactive frame to a tail point and require the point is unchanged.
Call a finite progress at `quiet_NaN`, infinity, and values outside 0..1;
require no non-finite offset and exact rest at clamped endpoints.

- [ ] **Step 4: Add seed/reversal assertions**

Advance two independently initialized copies from the same seed through
identical timestamps and compare the resulting `seed`, `remainingMask`,
`lastMotion`, `running`, `startedMilliseconds`, frame ID, and progress.
Advance from a second known seed and prove at least one first-five order value
differs.

For a representative active point:

```cpp
const auto right =
    inputIndicatorCatIdleTailPoint(
        {.x = 3.4, .y = 7.0}, step.frame
    );
const auto left =
    inputIndicatorCatFacingPoint(right, false);
QCOMPARE(left.x, 30.0 - right.x);
QCOMPARE(left.y, right.y);
const auto restored =
    inputIndicatorCatFacingPoint(left, true);
QCOMPARE(restored.x, right.x);
QCOMPARE(restored.y, right.y);
```

- [ ] **Step 5: Run the focused target to confirm red**

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

Expected: FAIL because the idle-tail scheduler types/functions do not exist.

### Task 2: Implement the Pure Scheduler and Tail Warp

**Files:**
- Modify: `src/lib/gui/input/InputIndicatorGeometry.h`
- Modify: `src/lib/gui/input/InputIndicatorGeometry.cpp`

- [ ] **Step 1: Add the public pure model**

Declare:

```cpp
enum class InputIndicatorCatIdleTailMotion : unsigned char
{
  RelaxedSway,
  TipFlick,
  CurlUnfurl,
  LazySweep,
  QuestionLift
};

struct InputIndicatorCatIdleTailMotionState
{
  std::uint32_t seed = 0;
  std::uint32_t remainingMask = 0x1f;
  InputIndicatorCatIdleTailMotion lastMotion =
      InputIndicatorCatIdleTailMotion::RelaxedSway;
  bool hasLastMotion = false;
  bool running = false;
  double startedMilliseconds = 0;
};

struct InputIndicatorCatIdleTailFrame
{
  InputIndicatorCatIdleTailMotion motion =
      InputIndicatorCatIdleTailMotion::RelaxedSway;
  double progress = 0;
  bool active = false;
};

struct InputIndicatorCatIdleTailMotionStep
{
  InputIndicatorCatIdleTailMotionState state;
  InputIndicatorCatIdleTailFrame frame;
};
```

Declare initialization, duration, advance, offset, point-warp, and facing
helpers with these signatures:

```cpp
InputIndicatorCatIdleTailMotionState
inputIndicatorCatIdleTailMotionInitialState(std::uint32_t seed);

double inputIndicatorCatIdleTailMotionDuration(
    InputIndicatorCatIdleTailMotion motion);

InputIndicatorCatIdleTailMotionStep
inputIndicatorCatIdleTailMotionAdvance(
    const InputIndicatorCatIdleTailMotionState &state,
    InputIndicatorMotionState motionState,
    double nowMilliseconds,
    bool reducedMotion);

InputIndicatorPoint inputIndicatorCatIdleTailOffset(
    InputIndicatorCatIdleTailMotion motion,
    double progress);

InputIndicatorPoint inputIndicatorCatIdleTailPoint(
    InputIndicatorPoint point,
    const InputIndicatorCatIdleTailFrame &frame);

constexpr InputIndicatorPoint inputIndicatorCatFacingPoint(
    InputIndicatorPoint point, bool facesRight)
{
  return facesRight
      ? point
      : InputIndicatorPoint{.x = 30.0 - point.x, .y = point.y};
}
```

- [ ] **Step 2: Implement the seeded five-item bag**

Normalize seed zero to a nonzero constant. Use one xorshift32 update per
selection. Select from only `remainingMask` bits, excluding `lastMotion` when
another bit exists. Refill all five bits only after the bag is empty.

When the motion state is Walk/Run or Reduced Motion is true, set only
`running = false` and return an inactive frame. Do not mutate the seed, mask, or
last ID. On Idle, select at the provided timestamp, then advance exact
durations with a clamped `elapsed / duration` frame progress.

- [ ] **Step 3: Implement the five finite envelopes**

Use a smooth window and these durations:

```cpp
RelaxedSway 2400
TipFlick    1800
CurlUnfurl  2800
LazySweep   3200
QuestionLift 2400
```

Each formula returns `{0, 0}` for progress zero/one and invalid input. Clamp
the progress first. Keep horizontal offset at 1.60 pt maximum and vertical
offset at 2.60 pt maximum.

- [ ] **Step 4: Implement the tail-region point warp**

Use a smooth influence:

```cpp
constexpr double kRootX = 12.4;
constexpr double kDistalX = 2.8;
raw = std::clamp((kRootX - point.x) / (kRootX - kDistalX), 0, 1);
influence = raw * raw * (3 - 2 * raw);
```

Mix each pattern offset by influence powers so the root is unchanged. Return
the original point for an inactive frame and for all points at/above the
root. Keep the function independent of AppKit and the label/tail artwork data.

- [ ] **Step 5: Run pure tests green**

```bash
cmake --build "$BUILD" --config Release --parallel 8 \
  --target InputIndicatorGeometryTests
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH/home" \
  XDG_CACHE_HOME="$SCRATCH/cache" \
  XDG_CONFIG_HOME="$SCRATCH/config" \
  TMPDIR="$SCRATCH/tmp" \
  ctest --test-dir "$BUILD/src/unittests" \
  -R '^InputIndicatorGeometryTests$' --output-on-failure
```

Expected: all pure scheduler, timing, deformation, and existing geometry slots
pass.

- [ ] **Step 6: Exercise the real panel redraw with a fixed cursor**

Inject an internal pointer-location provider into
`MacInputIndicatorPanel` for tests while production continues to read
`NSEvent.mouseLocation`. Show the real Cat panel, wait 650 ms, move it at a
fixed point, and assert:

```cpp
QCOMPARE(result.frameIntervalMilliseconds, 75);
QVERIFY(afterProgress > beforeProgress);
QVERIFY(tailPixels > 0);
QCOMPARE(nonTailPixels, 0);
```

Expected: the actual `DeskflowInputIndicatorView` caches a changed tail mask
at 2x backing scale, and no body/head/glyph pixels move.

### Task 3: Integrate the Idle Frame into the Cat Painter

**Files:**
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.mm`

- [ ] **Step 1: Add view and implementation state**

Add to `DeskflowInputIndicatorView`:

```objc
@property(nonatomic) NSInteger catIdleTailMotion;
@property(nonatomic) double catIdleTailProgress;
@property(nonatomic) BOOL catIdleTailActive;
```

Add one
`InputIndicatorCatIdleTailMotionState idleTailMotionState;` to
`MacInputIndicatorPanel::Impl`, initialized from a local `steady_clock` seed.
On `show`, active-space reset, and `hide`, set `running = false` without
resetting seed, mask, or last ID.

- [ ] **Step 2: Gate and advance the scheduler**

After existing motion-state selection and before view redraw checks:

```cpp
const auto tailStep =
    inputIndicatorCatIdleTailMotionAdvance(
        m_impl->idleTailMotionState,
        m_impl->companion == InputIndicatorCompanion::Cat
            ? m_impl->motionState
            : InputIndicatorMotionState::Walk,
        std::chrono::duration<double, std::milli>(
            now - m_impl->haloEpoch
        ).count(),
        reducedMotion
    );
m_impl->idleTailMotionState = tailStep.state;
const auto tailFrame = tailStep.frame;
```

Use zero Cat motion phase only while Idle, preserving the Dog/Turtle and
Walk/Run phase values. Add frame ID/progress/active comparisons to the existing
redraw predicate and copy all three fields to the view.

- [ ] **Step 3: Warp only the near tail path**

Extend the existing near-contour `catArtworkPath` overload with a
`InputIndicatorCatIdleTailFrame` argument. Apply
`inputIndicatorCatTorsoPoint` first, then
`inputIndicatorCatIdleTailPoint` to Move/Line/Cubic coordinates. Pass the view
frame through `drawCat` and `drawLanguageGlyph`.

The far path receives no tail frame. The Cat idle call to
`inputIndicatorCatPose` receives phase zero, so it selects `IdleNeutral`.
Walk and Run pass an inactive tail frame and retain all three existing gait
keyframes.

- [ ] **Step 4: Run the focused target**

```bash
cmake --build "$BUILD" --config Release --parallel 8 \
  --target InputIndicatorGeometryTests
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH/home" \
  XDG_CACHE_HOME="$SCRATCH/cache" \
  XDG_CONFIG_HOME="$SCRATCH/config" \
  TMPDIR="$SCRATCH/tmp" \
  ctest --test-dir "$BUILD/src/unittests" \
  -R '^InputIndicatorGeometryTests$' --output-on-failure
```

Expected: geometry tests stay green with the pure Cat frame now consumed by
the AppKit path layer.

- [ ] **Step 5: Commit the production integration**

```bash
git add \
  src/lib/gui/input/InputIndicatorGeometry.h \
  src/lib/gui/input/InputIndicatorGeometry.cpp \
  src/lib/gui/input/MacInputIndicatorPanel.mm \
  src/unittests/gui/input/InputIndicatorGeometryTests.h \
  src/unittests/gui/input/InputIndicatorGeometryTests.mm
git commit \
  -m "feat(input): vary idle cat tail motions" \
  -m "Co-authored-by: Copilot App <223556219+Copilot@users.noreply.github.com>"
```

### Task 4: Expand Native Paint and Clipping Coverage

**Files:**
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.mm`

- [ ] **Step 1: Make the test path use the production warp**

Extend the test `artworkPath(pose, commands)` helper with a Cat idle frame and
apply the same two pure functions in production order:

```cpp
auto point =
    inputIndicatorCatTorsoPoint(
        pose, {.x = x, .y = y}
    );
point = inputIndicatorCatIdleTailPoint(point, tailFrame);
```

Keep far-path construction unchanged.

- [ ] **Step 2: Add all Cat idle-pattern paint rows**

Build the existing nine Cat poses from all three Idle, Walk, and Run artwork
frames. For each Idle frame, add all five motion IDs at progress values
`0.20`, `0.50`, and `0.80`. For Walk/Run, use an inactive frame.

Render every row for:

- Latin and Korean source;
- right and left facing;
- the 1.10 display transform;
- the scaled CoreText label.

- [ ] **Step 3: Add padded panel-clipping accounting**

Create a `pixelBitmap(scale, paddingPoints)` and render into it after
translating by the padding. Count any body pixel outside the centered
33 x 31 pt target as `outsidePixels`, in addition to glyph pixels outside the
filled torso. Log:

```cpp
qInfo() << "outsidePixels" << outsidePixels
        << "cases" << cases;
```

Require `outsidePixels == 0`. Record the actual `cases` count rather than
keeping a stale 36-case constant.

- [ ] **Step 4: Run the paint-focused test**

```bash
cmake --build "$BUILD" --config Release --parallel 8 \
  --target InputIndicatorGeometryTests
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH/home" \
  XDG_CACHE_HOME="$SCRATCH/cache" \
  XDG_CONFIG_HOME="$SCRATCH/config" \
  TMPDIR="$SCRATCH/tmp" \
  "$BUILD/src/unittests/gui/input/InputIndicatorGeometryTests" \
  coreTextCatGlyphPixelsStayInsideSilhouette
```

Expected: every rendered case reports `outsidePixels 0`.

### Task 5: Create a Small Preview from Existing Native Tooling

**Files:**
- Create outside the repository: a temporary AppKit preview source under the
  session artifact directory.
- Save outside the repository:
  `cat-idle-tail-motions.png`.

- [ ] **Step 1: Render a 5-column native montage**

Reuse the compiled geometry and Cat artwork constants with an AppKit bitmap
context. Use `IdleNeutral`, progress chosen at each pattern's visible midpoint,
and draw each Cat at 4x or 6x. Include a second mirrored row if the result fits
in a 600 x 300 pt PNG.

- [ ] **Step 2: Verify the artifact**

Check that the preview is a PNG, has nonzero bytes, and can be decoded by the
existing `sips` tool:

```bash
sips -g pixelWidth -g pixelHeight -g format \
  "/Users/subeomlee/.copilot/session-state/0e04e54a-1ee4-49a1-867f-877e8413aab6/files/cat-idle-tail-motions.png"
```

If existing AppKit tooling cannot render the temporary source without a new
package, record that no preview was produced and continue. The implementation
is never blocked by screenshot tooling.

### Task 6: Run the Safe Focused Gate

**Files:**
- Modify: none unless a failure is caused by this change.

- [ ] **Step 1: Build named focused targets**

```bash
cmake --build "$BUILD" --config Release --parallel 8 --target \
  gui Deskflow \
  InputIndicatorPolicyTests \
  InputIndicatorGeometryTests \
  InputIndicatorIpcTests \
  OSXInputSourceTests \
  OSXInputIndicatorRelayTests \
  CoreProcessInputIndicatorTests \
  InputIndicatorSettingsTests
```

- [ ] **Step 2: Run seven named focused suites**

```bash
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH/home" \
  XDG_CACHE_HOME="$SCRATCH/cache" \
  XDG_CONFIG_HOME="$SCRATCH/config" \
  TMPDIR="$SCRATCH/tmp" \
  ctest --test-dir "$BUILD/src/unittests" \
  -R '^(InputIndicatorPolicyTests|InputIndicatorGeometryTests|InputIndicatorIpcTests|OSXInputSourceTests|OSXInputIndicatorRelayTests|CoreProcessInputIndicatorTests|InputIndicatorSettingsTests)$' \
  --output-on-failure
```

Expected: seven suites pass.

### Task 7: Run the Full Safe Release Gate

**Files:**
- Modify: none unless a change-caused failure exists.

- [ ] **Step 1: Enumerate and build every configured test executable target**

```bash
ctest --test-dir "$BUILD/src/unittests" -N -V > \
  "$BUILD/ctest-list.txt"
TEST_TARGETS="$(sed -n 's/^ *Test #[0-9][0-9]*: //p' \
  "$BUILD/ctest-list.txt" |
  grep -v '^GitVersionTests$' |
  sort -u | tr '\n' ' ')"
test -n "$TEST_TARGETS"
cmake --build "$BUILD" --config Release --parallel 8 \
  --target $TEST_TARGETS
printf '%s\n' "$TEST_TARGETS" | tr ' ' '\n' |
  sed '/^$/d' > "$BUILD/test-targets.txt"
wc -l "$BUILD/test-targets.txt"
```

- [ ] **Step 2: Run CTest with exactly two exclusions**

```bash
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH/home" \
  XDG_CACHE_HOME="$SCRATCH/cache" \
  XDG_CONFIG_HOME="$SCRATCH/config" \
  TMPDIR="$SCRATCH/tmp" \
  ctest --test-dir "$BUILD/src/unittests" \
  -E '^(OSXClipboardTests|OSXKeyStateTests)$' \
  --output-on-failure
```

Expected: every registered test except exactly those two passes.

- [ ] **Step 3: Record the exact exclusions and count**

```bash
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH/home" \
  XDG_CACHE_HOME="$SCRATCH/cache" \
  XDG_CONFIG_HOME="$SCRATCH/config" \
  TMPDIR="$SCRATCH/tmp" \
  ctest --test-dir "$BUILD/src/unittests" \
  -N -E '^(OSXClipboardTests|OSXKeyStateTests)$'
```

Expected: the total and selected test count are visible in CTest output and
only the two named tests are omitted.

### Task 8: Finalize and Handoff

**Files:**
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.mm` if paint
  assertions required a test-only follow-up.

- [ ] **Step 1: Commit paint coverage**

```bash
git add src/unittests/gui/input/InputIndicatorGeometryTests.mm
git commit \
  -m "test(input): cover idle cat tail paint" \
  -m "Co-authored-by: Copilot App <223556219+Copilot@users.noreply.github.com>"
```

- [ ] **Step 2: Verify final repository state**

```bash
git --no-pager status --short --branch
git --no-pager log --format='%H %s%n%b' \
  389bc3924bb0c03657ce5d44876af454f9f26e05..HEAD
git --no-pager diff --check \
  389bc3924bb0c03657ce5d44876af454f9f26e05..HEAD
```

Expected: clean worktree, exact design/implementation/test commits, and no
whitespace errors.

- [ ] **Step 3: Remove only the named scratch directories**

After retaining command/result summaries and the preview:

```bash
rm -rf \
  /tmp/deskflow-cat-tail-8dd955c0-release \
  /tmp/deskflow-cat-tail-8dd955c0-scratch
```

- [ ] **Step 4: Report to the coordinator**

Send a purpose `handoff` report with the assignment ID from the kickoff,
source branch `subeomlee-microsoft-fix-input-source-move-bug`, final branch,
exact commit SHAs, focused/full test counts, `outsidePixels` cases, and the
preview path or explicit unavailable reason. Do not mention or perform release,
push, tag, install, or PR work.
