# Cat Source-Change Startle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the Cat source-change glow with one 300 ms recoil/compression/dorsal-fur startle while preserving the `A`/`한` transition, Cat gait, label, dimensions, Reduced Motion, and five idle-tail motions.

**Architecture:** A pure `InputIndicatorGeometry` state machine validates exact source changes, throttles/coalesces to one 50 ms-bounded start, and produces a finite contour frame. `MacInputIndicatorPanel` reuses its existing `show` source-change path, removes Cat halo state/drawing, pauses/resumes the idle-tail scheduler, and maps the frame into the continuous Cat paths.

**Tech Stack:** C++20, Objective-C++, AppKit/CoreText, Qt 6.12 Test, CMake/Ninja, CTest.

---

## File Structure

- Modify: `src/lib/gui/input/InputIndicatorGeometry.h`
  - Add exact-source startle state, frame, step, point-warper APIs.
- Modify: `src/lib/gui/input/InputIndicatorGeometry.cpp`
  - Implement 300 ms startle trigger/coalescing and continuous-point pose.
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.h`
  - Remove Cat halo pure helpers and retain internal panel dependencies.
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.mm`
  - Replace halo fields/properties/drawing with one startle frame, pause idle
    tail, preserve source crossfade, and keep 8/75 ms timer policy.
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
  - Register pure startle, source panel, and native pixel slots.
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.mm`
  - Add red-first state tests, fixed-cursor actual panel repaint tests,
    startle paint/diff tests, and no-halo clipping assertions.
- Modify: `src/unittests/gui/input/InputIndicatorPolicyTests.cpp`
  - Keep exact snapshot source-change behavior covered at controller level if
    panel-specific pure tests do not expose duplicate suppression.
- Use: `docs/superpowers/specs/2026-11-25-cat-source-change-startle-design.md`
  - Authoritative source behavior and geometry.

## Safe Variables

```bash
export REPO="$PWD"
export BUILD="/tmp/deskflow-cat-startle-0e04e54a-release"
export SCRATCH="/tmp/deskflow-cat-startle-0e04e54a-scratch"
export SAFE_TMP="/Users/subeomlee/.copilot/session-state/0e04e54a-1ee4-49a1-867f-877e8413aab6/files/cat-startle-tmp"
export QT_ROOT="$(qmake6 -query QT_INSTALL_PREFIX)"
mkdir -p "$SCRATCH/home" "$SCRATCH/cache" "$SCRATCH/config" \
  "$SCRATCH/tmp" "$SAFE_TMP"
```

`qmake6 -query QT_VERSION` must return `6.12.0`. `SAFE_TMP` is used as
`TMPDIR` because it is not a `/private` path and is not reached through the
`/tmp` symlink.

### Task 1: Add Pure Startle Tests First

**Files:**
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.mm`

- [ ] **Step 1: Register pure test slots**

Add:

```cpp
void catSourceStartleTimelineUsesFiniteNeutralEndpoints();
void catSourceStartleCoalescesLatestSourceAtFiftyMilliseconds();
void catSourceStartleRejectsNonEventsAndReducedMotion();
void catSourceStartleWarpsOnlyApprovedDorsalRegion();
```

- [ ] **Step 2: Write the failing trigger/timeline test**

Add a test using the desired types:

```cpp
using Source = deskflow::input::InputIndicatorSource;
InputIndicatorCatSourceStartleState state;
state = inputIndicatorCatSourceStartleTrigger(
    state, Source::LatinABC, Source::Korean2Set, 100, false
);

const auto start =
    inputIndicatorCatSourceStartleAdvance(state, 100, false);
const auto recoil =
    inputIndicatorCatSourceStartleAdvance(state, 170, false);
const auto fur =
    inputIndicatorCatSourceStartleAdvance(state, 260, false);
const auto ended =
    inputIndicatorCatSourceStartleAdvance(state, 400, false);

QVERIFY(start.frame.active);
QCOMPARE(start.frame.recoilOffsetX, 0.0);
QCOMPARE(start.frame.horizontalScale, 1.0);
QCOMPARE(start.frame.dorsalLift, 0.0);
QVERIFY(recoil.frame.recoilOffsetX < 0);
QVERIFY(recoil.frame.horizontalScale >= 0.92);
QCOMPARE(recoil.frame.dorsalLift, 0.0);
QCOMPARE(fur.frame.recoilOffsetX, 0.0);
QCOMPARE(fur.frame.horizontalScale, 1.0);
QVERIFY(std::abs(fur.frame.dorsalLift - 1.8) < 1e-9);
QVERIFY(!ended.frame.active);
QCOMPARE(ended.frame.recoilOffsetX, 0.0);
QCOMPARE(ended.frame.horizontalScale, 1.0);
QCOMPARE(ended.frame.dorsalLift, 0.0);
```

Sample 0-300 ms in 1 ms increments and require all fields finite,
`recoilOffsetX >= -0.45`, `horizontalScale >= 0.92`,
`horizontalScale <= 1`, and `0 <= dorsalLift <= 1.8`.

- [ ] **Step 3: Write failing coalescing and latest-source tests**

Trigger Korean at time 0, Latin at 20, Korean at 35, and a duplicate Korean at
49. Advance at 49 and require the original reaction still has no second start;
advance at 50 and require one replacement with `latestSource == Korean2Set`.
Record `lastStartedMilliseconds == 50`, no queue capacity, and no more than one
active frame.

Then trigger every 10 ms for one second and assert no two consecutive
`startedMilliseconds` values differ by less than 50 ms.

- [ ] **Step 4: Write failing rest/Reduced Motion/event tests**

For `Unknown`, identical source, and first-known source from `Unknown`, require
the state/active frame unchanged.

Start a reaction, advance with Reduced Motion, and require:

```cpp
QVERIFY(!reduced.frame.active);
QVERIFY(!reduced.state.active);
QVERIFY(!reduced.state.pending);
QCOMPARE(reduced.frame.recoilOffsetX, 0.0);
QCOMPARE(reduced.frame.horizontalScale, 1.0);
QCOMPARE(reduced.frame.dorsalLift, 0.0);
```

- [ ] **Step 5: Write failing point-neutrality and dorsal-region tests**

Use the fur peak frame. Assert:

```cpp
const auto tail = inputIndicatorCatStartlePoint(
    {.x = 5, .y = 7}, frame, true
);
const auto dorsal = inputIndicatorCatStartlePoint(
    {.x = 15.5, .y = 12.5}, frame, true
);
const auto head = inputIndicatorCatStartlePoint(
    {.x = 22, .y = 16}, frame, true
);
```

`dorsal.y` must differ from 12.5, but `tail.y` and `head.y` must remain
unchanged at the pure fur peak. At age 300 every sampled point must compare
exactly to its input.

- [ ] **Step 6: Run red verification**

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

Expected: FAIL because the startle state/frame/point APIs do not exist.

### Task 2: Implement the Pure Startle State and Pose

**Files:**
- Modify: `src/lib/gui/input/InputIndicatorGeometry.h`
- Modify: `src/lib/gui/input/InputIndicatorGeometry.cpp`

- [ ] **Step 1: Add the state/frame API**

Declare these public types:

```cpp
struct InputIndicatorCatSourceStartleState
{
  deskflow::input::InputIndicatorSource latestSource =
      deskflow::input::InputIndicatorSource::Unknown;
  bool active = false;
  bool pending = false;
  bool hasStarted = false;
  double startedMilliseconds = 0;
  double lastStartedMilliseconds = 0;
};

struct InputIndicatorCatStartleFrame
{
  bool active = false;
  double progress = 0;
  double recoilOffsetX = 0;
  double horizontalScale = 1;
  double dorsalLift = 0;
};

struct InputIndicatorCatSourceStartleStep
{
  InputIndicatorCatSourceStartleState state;
  InputIndicatorCatStartleFrame frame;
};
```

Declare:

```cpp
InputIndicatorCatSourceStartleState
inputIndicatorCatSourceStartleTrigger(
    const InputIndicatorCatSourceStartleState &state,
    deskflow::input::InputIndicatorSource previousSource,
    deskflow::input::InputIndicatorSource source,
    double nowMilliseconds,
    bool reducedMotion);

InputIndicatorCatSourceStartleStep
inputIndicatorCatSourceStartleAdvance(
    const InputIndicatorCatSourceStartleState &state,
    double nowMilliseconds,
    bool reducedMotion);

InputIndicatorPoint inputIndicatorCatStartlePoint(
    InputIndicatorPoint point,
    const InputIndicatorCatStartleFrame &frame,
    bool dorsalContour);
```

- [ ] **Step 2: Implement exact trigger and coalescing**

Accept only `LatinABC` and `Korean2Set` when the values differ. Normalize
non-finite/negative time to zero. Start immediately when no active start exists.
For another event, store only the latest source in `latestSource` and one
`pending = true` flag. `advance` starts the pending reaction when 50 ms has
elapsed from `lastStartedMilliseconds`.

Do not add a container to the state. `active`, `pending`, `started`, `lastStarted`,
and `latestSource` are the complete queue state.

- [ ] **Step 3: Implement the 300 ms frame**

Use smoothstep envelopes:

```text
0..70: recoil 0 -> -0.45; horizontalScale 1 -> 0.92; dorsal 0
70..160: recoil -> 0; scale -> 1; dorsal 0 -> 1.8
160..300: recoil 0; scale 1; dorsal 1.8 -> 0
```

At 300 ms or later return inactive/rest and clear pending if no throttled event
remains. Reduced Motion always clears active/pending and returns exact rest.

- [ ] **Step 4: Implement the contour point function**

Return the original point for an inactive frame. For active frames:

```cpp
point.x =
    15.0 + (point.x - 15.0) *
    frame.horizontalScale +
    frame.recoilOffsetX;
```

When `dorsalContour` is true, multiply `frame.dorsalLift` by a smooth x/y
weight covering the upper back and add it to `point.y`. Fade completely to
zero before the tail root and before the ears/head.

- [ ] **Step 5: Run pure tests green**

```bash
cmake --build "$BUILD" --config Release --parallel 8 \
  --target InputIndicatorGeometryTests
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH/home" \
  XDG_CACHE_HOME="$SCRATCH/cache" \
  XDG_CONFIG_HOME="$SCRATCH/config" TMPDIR="$SAFE_TMP" \
  ctest --test-dir "$BUILD/src/unittests" \
  -R '^InputIndicatorGeometryTests$' --output-on-failure
```

Expected: the four startle tests pass without changing Cat paint yet.

### Task 3: Add Failing Panel and Native Pixel Tests

**Files:**
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.mm`

- [ ] **Step 1: Register panel/native slots**

Add:

```cpp
void catPanelSourceChangesTriggerOnlyOneStartle();
void catStartleRenderKeepsLabelAndCanvasStable();
void catStartleReplacesHaloAndResumesIdleTail();
void otherCompanionsDoNotStartleOnSourceChange();
```

- [ ] **Step 2: Add exact source panel test**

Use `MacInputIndicatorPanel` with a fixed pointer provider. Show Cat LatinABC;
require the initial view reaction inactive. Show the same source and require
still inactive. Show Unknown and require no reaction. Show Korean and inspect:

```objc
[[view valueForKey:@"catStartleActive"] boolValue]
[[view valueForKey:@"catStartleLatestSource"] integerValue]
[[view valueForKey:@"catStartlePending"] boolValue]
```

Require one active reaction and Korean as latest.

- [ ] **Step 3: Add rapid flip and Dog/Turtle tests**

Show Latin, trigger Korean, show Latin 20 ms later, and Korean 10 ms after
that. Query the one view frame. Require:

- one pending latest source;
- no vector/pulse-count property;
- at most one 50 ms replacement;
- only the latest source remains after 50 ms.

Run the same source pair for Dog and Turtle and require:

- existing `previousSource`/`source` crossfade;
- reaction inactive at every move;
- no Cat deformation.

- [ ] **Step 4: Add startle native paint rows**

Extend the test `artworkPath` helper to apply:

```cpp
point = inputIndicatorCatTorsoPoint(pose, point);
point = inputIndicatorCatIdleTailPoint(point, tailFrame);
point = inputIndicatorCatStartlePoint(
    point, startleFrame, true
);
```

The far path applies startle with `dorsalContour = false`.

For ages `{70, 110, 160, 230, 299, 300}` render each Cat facing and source at
1x and 2x into padded bitmaps. Assert:

- no pixel outside 33 x 31;
- no pixels created by a halo/pulse path;
- glyph mask is unchanged and inside the body;
- dorsal-region change is visible at 70-230 ms;
- age 300 has exact pixel equality with the baseline;
- prior tail-perceptibility minimums remain green.

- [ ] **Step 5: Run red integration verification**

```bash
cmake --build "$BUILD" --config Release --parallel 8 \
  --target InputIndicatorGeometryTests
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH/home" \
  XDG_CACHE_HOME="$SCRATCH/cache" \
  XDG_CONFIG_HOME="$SCRATCH/config" TMPDIR="$SAFE_TMP" \
  "$BUILD/src/unittests/gui/input/InputIndicatorGeometryTests" \
  catPanelSourceChangesTriggerOnlyOneStartle
```

Expected: FAIL because no startle properties or AppKit integration exist.

### Task 4: Integrate Startle and Remove Cat Halo

**Files:**
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.h`
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.mm`

- [ ] **Step 1: Remove Cat halo model and view state**

Delete:

- `MacInputIndicatorHaloPulse`;
- `MacInputIndicatorHaloFrame`;
- `kMacInputIndicatorHaloDurationMilliseconds`;
- `kMacInputIndicatorHaloMinimumStartInterval`;
- `kMacInputIndicatorMaximumHaloWaves`;
- `macInputIndicatorAppendHaloPulse`;
- `macInputIndicatorHaloFrame`;
- `macInputIndicatorActiveHaloFrames`.

Remove `haloPulseCount/source/scale/alpha` view properties and Impl fields.
Remove ambient `drawHalo` and source pulse drawing from `drawCat`.

- [ ] **Step 2: Trigger one startle from the existing source-change branch**

In `show`, keep the existing local `previousSource` behavior. Compute
monotonic milliseconds from a renamed `animationEpoch`. Before assigning the
new current source, call:

```cpp
m_impl->startleState =
    inputIndicatorCatSourceStartleTrigger(
        m_impl->startleState,
        previousSource,
        options.source,
        animationMilliseconds,
        isOSXReduceMotion()
    );
```

Call it only for Cat; for a Dog/Turtle companion clear any prior Cat state
without creating a reaction.

- [ ] **Step 3: Advance the startle on every panel move**

Call `inputIndicatorCatSourceStartleAdvance` after motion-state selection.
Copy active/progress/recoil/scale/dorsal/latest/pending values to the view.
Add reaction-active-or-pending to the 8 ms animation predicate.

The existing 200 ms source transition continues unchanged. At rest after both
transition and reaction end, use 75 ms.

- [ ] **Step 4: Pause and resume idle-tail state**

Add `idleTailPaused` and `idleTailPauseStartedMilliseconds` to the Impl.
During Cat active/pending startle:

```cpp
idleTailFrame = {};
```

Do not call `inputIndicatorCatIdleTailMotionAdvance`, so the selected ID and
progress are preserved. On leaving the reaction in Idle, shift
`idleTailMotionState.startedMilliseconds` forward by the pause duration, then
advance normally. If state is Walk/Run or Reduced Motion, use the existing gate
and do not resume.

- [ ] **Step 5: Warp existing Cat paths**

Extend the `catArtworkPath` near-contour overload with a startle frame and
apply:

```cpp
point = inputIndicatorCatTorsoPoint(pose, point);
point = inputIndicatorCatIdleTailPoint(point, tailFrame);
point = inputIndicatorCatStartlePoint(point, startleFrame, true);
```

Apply startle with `dorsalContour = false` to the far path. Pass the same
transformed near path into `drawLanguageGlyph` clipping; draw the glyph itself
without startle coordinates.

- [ ] **Step 6: Run panel/native tests green**

```bash
cmake --build "$BUILD" --config Release --parallel 8 \
  --target InputIndicatorGeometryTests
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH/home" \
  XDG_CACHE_HOME="$SCRATCH/cache" \
  XDG_CONFIG_HOME="$SCRATCH/config" TMPDIR="$SAFE_TMP" \
  ctest --test-dir "$BUILD/src/unittests" \
  -R '^InputIndicatorGeometryTests$' --output-on-failure
```

Expected: startle state, source panel, paint/clipping, tail resume, and prior
264 cases all pass.

### Task 5: Preserve Controller and Non-Cat Contracts

**Files:**
- Modify: `src/unittests/gui/input/InputIndicatorPolicyTests.cpp` only if a
  controller-level exact/duplicate transition test gap exists.
- Modify: none in production unless a focused test proves a timer bug.

- [ ] **Step 1: Verify exact duplicate suppression**

Reuse `persistentSourceChangesPresentImmediately`, extend only if needed to
assert one panel show for duplicate snapshots and one for each exact
Latin/Korean transition.

- [ ] **Step 2: Run focused named indicator targets**

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

env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH/home" \
  XDG_CACHE_HOME="$SCRATCH/cache" \
  XDG_CONFIG_HOME="$SCRATCH/config" TMPDIR="$SAFE_TMP" \
  ctest --test-dir "$BUILD/src/unittests" \
  -R '^(InputIndicatorPolicyTests|InputIndicatorGeometryTests|InputIndicatorIpcTests|OSXInputSourceTests|OSXInputIndicatorRelayTests|CoreProcessInputIndicatorTests|InputIndicatorSettingsTests)$' \
  --output-on-failure
```

Expected: 7/7 focused tests pass.

### Task 6: Run the Full Safe Release Gate

**Files:**
- Modify: none unless a change-caused failure exists.

- [ ] **Step 1: Build every named CTest executable target**

```bash
ctest --test-dir "$BUILD/src/unittests" -N -V > \
  "$BUILD/ctest-list.txt"
TEST_TARGETS="$(sed -nE 's/^ *Test +#[0-9]+: //p' \
  "$BUILD/ctest-list.txt" |
  grep -v '^GitVersionTests$' |
  sort -u | tr '\n' ' ')"
test -n "$TEST_TARGETS"
printf '%s\n' "$TEST_TARGETS" | tr ' ' '\n' |
  sed '/^$/d' > "$BUILD/test-targets.txt"
env HOME="$SCRATCH/home" XDG_CACHE_HOME="$SCRATCH/cache" \
  XDG_CONFIG_HOME="$SCRATCH/config" TMPDIR="$SAFE_TMP" \
  cmake --build "$BUILD" --config Release --parallel 8 \
  --target $TEST_TARGETS
```

Record `ctest -N` totals. This isolated branch currently registers 159 tests,
so the safe run is 157. A coordinated close-fix baseline may register one more
and must report 158/158 separately without merging into this worktree.

- [ ] **Step 2: Run the safe CTest set**

```bash
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH/home" \
  XDG_CACHE_HOME="$SCRATCH/cache" \
  XDG_CONFIG_HOME="$SCRATCH/config" TMPDIR="$SAFE_TMP" \
  ctest --test-dir "$BUILD/src/unittests" \
  -E '^(OSXClipboardTests|OSXKeyStateTests)$' \
  --output-on-failure
```

Expected: every selected test passes with exactly those two exclusions.

### Task 7: Commit, Audit, and Handoff

**Files:**
- Modify: none unless the final audit finds formatting generated by Qt tools.

- [ ] **Step 1: Restore build-generated translations**

If `lupdate` changes translation files, verify the diff is generated and run:

```bash
git restore --worktree -- \
  translations/deskflow_es.ts \
  translations/deskflow_it.ts \
  translations/deskflow_ja.ts \
  translations/deskflow_ru.ts \
  translations/deskflow_zh_CN.ts
```

- [ ] **Step 2: Commit implementation and tests**

```bash
git add \
  src/lib/gui/input/InputIndicatorGeometry.h \
  src/lib/gui/input/InputIndicatorGeometry.cpp \
  src/lib/gui/input/MacInputIndicatorPanel.h \
  src/lib/gui/input/MacInputIndicatorPanel.mm \
  src/unittests/gui/input/InputIndicatorGeometryTests.h \
  src/unittests/gui/input/InputIndicatorGeometryTests.mm
git commit \
  -m "feat(input): startle cat on source change" \
  -m "Co-authored-by: Copilot App <223556219+Copilot@users.noreply.github.com>"
```

- [ ] **Step 3: Audit final state**

```bash
git --no-pager diff --check
git --no-pager status --short --branch
git --no-pager log -4 --format='%H %s%n%b'
```

Expected: clean worktree, spec/plan/implementation commits, required trailers,
and no push/merge/release/install operation.

- [ ] **Step 4: Remove only named scratch directories**

```bash
rm -rf \
  /tmp/deskflow-cat-startle-0e04e54a-release \
  /tmp/deskflow-cat-startle-0e04e54a-scratch \
  /Users/subeomlee/.copilot/session-state/0e04e54a-1ee4-49a1-867f-877e8413aab6/files/cat-startle-tmp
```

- [ ] **Step 5: Report to the coordinator**

Send purpose `handoff` with:

- root behavior and latest commit;
- exact source-change/coalescing rules;
- red test that failed;
- 1x/2x fur and label pixel evidence;
- focused and full safe counts;
- clipping and tail-pixel regression counts;
- clean branch and whether close-fix integration remains external.
