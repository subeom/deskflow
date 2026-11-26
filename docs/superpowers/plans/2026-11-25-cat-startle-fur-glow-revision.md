# Cat Startle Fur and Glow Revision Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Strengthen the Cat's 300 ms startle dorsal fur to 2.4 pt and add one soft source-colored full-silhouette glow without restoring ambient/stacked halo behavior.

**Architecture:** Extend the existing pure `InputIndicatorCatStartleFrame` with glow scale/alpha and change the dorsal peak. The existing `MacInputIndicatorPanel` paint path will render one transformed full-Cat halo before the continuous body and glyph; state/source coalescing and Reduced Motion stay in the current startle model.

**Tech Stack:** C++20, Objective-C++, AppKit/CoreText/CoreGraphics, Qt 6.12 Test, CMake/Ninja, CTest.

---

## File Structure

- Modify: `src/lib/gui/input/InputIndicatorGeometry.h`
  - Extend the existing startle frame with `haloScale`/`haloAlpha`; keep one
    state/step boundary.
- Modify: `src/lib/gui/input/InputIndicatorGeometry.cpp`
  - Raise dorsal peak to 2.4 and evaluate one finite halo envelope.
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.mm`
  - Paint one source-colored full-Cat halo/shadow behind the existing body;
    do not create a pulse list or ambient halo.
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
  - Register fur/glow pure and native pixel slots.
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.mm`
  - Add red-first 2.4, halo timing/color/layer, 1x/2x, heading, source, and
    Reduced Motion coverage.
- Use: `docs/superpowers/specs/2026-11-25-cat-startle-fur-glow-revision.md`
  - Authoritative visual and interaction behavior.

## Safe Variables

```bash
export REPO="$PWD"
export BUILD="/tmp/deskflow-cat-fur-glow-650d-release"
export SCRATCH="/tmp/deskflow-cat-fur-glow-650d-scratch"
export SAFE_TMP="/Users/subeomlee/.copilot/session-state/0e04e54a-1ee4-49a1-867f-877e8413aab6/files/cat-fur-glow-tmp"
export QT_ROOT="$(qmake6 -query QT_INSTALL_PREFIX)"
mkdir -p "$SCRATCH/home" "$SCRATCH/cache" "$SCRATCH/config" \
  "$SCRATCH/tmp" "$SAFE_TMP"
```

`qmake6 -query QT_VERSION` must return `6.12.0`.

### Task 1: Add Pure Fur/Glow Tests First

**Files:**
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.mm`

- [ ] **Step 1: Register pure slots**

Add:

```cpp
void catStartleFurReachesTwoPointFourWithoutChangingTiming();
void catStartleGlowUsesOneBoundedEnvelope();
void catStartleLatestSourceSelectsGlowColor();
void catStartleFurAndGlowRespectReducedMotion();
```

- [ ] **Step 2: Write the failing 2.4 pt fur test**

Trigger Latin to Korean at 0 and advance at 160:

```cpp
const auto peak =
    inputIndicatorCatSourceStartleAdvance(
        state, 160, false
    );
QVERIFY(std::abs(
    peak.frame.dorsalLift - 2.4
) < 1e-9);
```

Assert the 70/160/300 boundaries stay finite, the reaction still ends at 300,
and `inputIndicatorCatStartlePoint` reaches a 2.4 pt dorsal peak only in the
approved dorsal region.

- [ ] **Step 3: Write the failing halo envelope test**

Require:

```cpp
QCOMPARE(at0.frame.haloScale, 1.0);
QCOMPARE(at0.frame.haloAlpha, 0.0);
QVERIFY(atPeak.frame.haloScale <= 1.12);
QVERIFY(atPeak.frame.haloScale > 1.0);
QVERIFY(atPeak.frame.haloAlpha <= 0.16);
QVERIFY(atPeak.frame.haloAlpha > 0.0);
QCOMPARE(at300.frame.haloScale, 1.0);
QCOMPARE(at300.frame.haloAlpha, 0.0);
```

Assert one frame exists in each step, with no vector, list, or wave count.

- [ ] **Step 4: Write source color/coalescing tests**

Use `macInputIndicatorGlyphColor(Source::LatinABC, tone)` and Korean. Compare
the exact selected glow source from startle state after rapid flips. Require:

- initial Latin -> Korean selects Korean;
- Korean -> Latin within 20 ms changes `latestSource` to Latin;
- one advance at 50 ms starts only the latest source;
- no old source remains as a second frame.

- [ ] **Step 5: Write Reduced Motion/rest tests**

Advance an active reaction with `reducedMotion=true`; require zero dorsal
lift, scale 1, alpha 0, inactive startle state, and no pending frame.

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

Expected: FAIL because `haloScale`/`haloAlpha` do not exist and dorsal peak is
still 1.8.

### Task 2: Implement the Pure Fur/Glow Frame

**Files:**
- Modify: `src/lib/gui/input/InputIndicatorGeometry.h`
- Modify: `src/lib/gui/input/InputIndicatorGeometry.cpp`

- [ ] **Step 1: Extend the startle frame**

```cpp
struct InputIndicatorCatStartleFrame
{
  bool active = false;
  double progress = 0;
  double recoilOffsetX = 0;
  double horizontalScale = 1;
  double dorsalLift = 0;
  double haloScale = 1;
  double haloAlpha = 0;
};
```

- [ ] **Step 2: Change the dorsal peak**

Replace `kCatStartleDorsalLift = 1.8` with `2.4`. Keep the existing
70/160/300 ms branch and the same contour weight.

- [ ] **Step 3: Add one finite halo envelope**

Use a smooth hump based on age:

```cpp
const double haloEnvelope =
    std::sin(age / 300.0 * kPi);
frame.haloScale = 1.0 + 0.12 * haloEnvelope;
frame.haloAlpha = 0.16 * haloEnvelope;
```

Return defaults for age 0 and age >=300. Reduced Motion leaves defaults.

- [ ] **Step 4: Run pure tests green**

```bash
cmake --build "$BUILD" --config Release --parallel 8 \
  --target InputIndicatorGeometryTests
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH/home" \
  XDG_CACHE_HOME="$SCRATCH/cache" \
  XDG_CONFIG_HOME="$SCRATCH/config" TMPDIR="$SAFE_TMP" \
  ctest --test-dir "$BUILD/src/unittests" \
  -R '^InputIndicatorGeometryTests$' --output-on-failure
```

Expected: pure fur/glow bounds and existing startle/tail/pursuit state tests
pass.

### Task 3: Add Native Fur/Glow Paint Tests

**Files:**
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.mm`

- [ ] **Step 1: Register native paint slots**

```cpp
void catStartleFurIsVisiblyStrongerThanOldPeak();
void catStartleGlowIsSingleColoredAndBehindBody();
void catStartleFurAndGlowStayInsideRotatingCanvas();
```

- [ ] **Step 2: Add a reference-fur pixel test**

Render `IdleNeutral` at age 160 with:

- actual frame dorsal 2.4;
- a copy of the frame with only `dorsalLift = 1.8`.

Compare 1x and 2x body masks. Require actual-vs-baseline changed pixels and
actual-vs-old-fur changed pixels both increase. Keep label masks identical and
`outsidePixels == 0`.

- [ ] **Step 3: Add source-colored halo layer tests**

Create a test paint helper that uses the production pure frame and the same
two Cat paths. Draw:

1. halo/shadow mask;
2. body mask;
3. glyph mask.

For `LatinABC` and `Korean2Set`, sample halo-only pixels and compare their
premultiplied RGB hue against `macInputIndicatorGlyphColor` within a small
tolerance. Require:

- halo-only pixels > 0 at peak;
- halo mask outside or at body edge, never over the filled body;
- body mask remains opaque at the original body;
- glyph pixels remain unchanged;
- no halo-only pixels at age 0 or 300.

- [ ] **Step 4: Add 1x/2x heading/canvas coverage**

Extend startle rows across both sources, both facings, and headings at 0,
45, 90, 135, 180, 225, 270, and 315 degrees. Render halo+body into 49 x 49
plus padding and include a QuestionLift tail extreme. Require
`outsidePixels == 0`, glyph remains inside, and halo never covers glyph pixels.

- [ ] **Step 5: Run red native verification**

```bash
cmake --build "$BUILD" --config Release --parallel 8 \
  --target InputIndicatorGeometryTests
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH/home" \
  XDG_CACHE_HOME="$SCRATCH/cache" \
  XDG_CONFIG_HOME="$SCRATCH/config" TMPDIR="$SAFE_TMP" \
  "$BUILD/src/unittests/gui/input/InputIndicatorGeometryTests" \
  catStartleGlowIsSingleColoredAndBehindBody
```

Expected: FAIL because production/test paint has no halo layer and the pure
peak was still 1.8 before Task 2.

### Task 4: Paint One Full-Cat Glow

**Files:**
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.mm`

- [ ] **Step 1: Copy view source/alpha into drawCat**

Extend `drawCat` with:

```cpp
deskflow::input::InputIndicatorSource reactionSource,
InputIndicatorTone tone,
MacInputIndicatorRenderAlpha alpha
```

Use `self.catStartleLatestSource`, not the intermediate crossfade source, for
the halo color.

- [ ] **Step 2: Build one full-Cat halo**

After building the transformed `far` and `near` paths, append both into a
single `NSBezierPath *halo`. Scale it about `(15,14)` by
`startleFrame.haloScale`.

Draw only when:

```cpp
startleFrame.active &&
startleFrame.haloAlpha > 0 &&
!reducedMotion
```

- [ ] **Step 3: Paint one soft source-colored layer behind the body**

Use `indicatorGlyphColor(reactionSource, tone, alpha.body * haloAlpha)`.
Save graphics state, set a 2.2 pt zero-offset shadow with the same source
color, fill the one halo path, restore, then draw far/near body and glyph in
their existing order.

Do not loop, append a pulse, or maintain halo storage.

- [ ] **Step 4: Preserve coalescing and Reduced Motion**

Use the existing `InputIndicatorCatSourceStartleState.latestSource` and
single frame. Do not add panel state. The source-startle trigger remains the
only halo trigger; Reduced Motion clears active/pending and yields zero alpha.

- [ ] **Step 5: Run native tests green**

```bash
cmake --build "$BUILD" --config Release --parallel 8 \
  --target InputIndicatorGeometryTests
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH/home" \
  XDG_CACHE_HOME="$SCRATCH/cache" \
  XDG_CONFIG_HOME="$SCRATCH/config" TMPDIR="$SAFE_TMP" \
  ctest --test-dir "$BUILD/src/unittests" \
  -R '^InputIndicatorGeometryTests$' --output-on-failure
```

Expected: fur delta, halo layer/color/endpoints, 48 startle cases, 64 heading
cases, 264 base Cat cases, and prior tail tests pass.

### Task 5: Run Focused and WindowServer-Split Full Gates

**Files:**
- Modify: none unless a change-caused failure exists.

- [ ] **Step 1: Run focused named targets**

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

Expected: 7/7.

- [ ] **Step 2: Build complete named CTest inventory**

Use the proven robust parser:

```bash
ctest --test-dir "$BUILD/src/unittests" -N -V > "$BUILD/ctest-list.txt"
TEST_TARGETS="$(sed -nE 's/^ *Test +#[0-9]+: //p' \
  "$BUILD/ctest-list.txt" |
  grep -v '^GitVersionTests$' |
  sort -u | tr '\n' ' ')"
env HOME="$SCRATCH/home" XDG_CACHE_HOME="$SCRATCH/cache" \
  XDG_CONFIG_HOME="$SCRATCH/config" TMPDIR="$SAFE_TMP" \
  cmake --build "$BUILD" --config Release --parallel 8 \
  --target $TEST_TARGETS
```

- [ ] **Step 3: Run split safe gate**

Run every safe CTest except `InputIndicatorGeometryTests`:

```bash
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH/home" \
  XDG_CACHE_HOME="$SCRATCH/cache" \
  XDG_CONFIG_HOME="$SCRATCH/config" TMPDIR="$SAFE_TMP" \
  ctest --test-dir "$BUILD/src/unittests" \
  -E '^(OSXClipboardTests|OSXKeyStateTests|InputIndicatorGeometryTests)$' \
  --output-on-failure
```

Then run the WindowServer-only geometry executable directly:

```bash
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH/home" \
  XDG_CACHE_HOME="$SCRATCH/cache" \
  XDG_CONFIG_HOME="$SCRATCH/config" TMPDIR="$SAFE_TMP" \
  ctest --test-dir "$BUILD/src/unittests" \
  -R '^InputIndicatorGeometryTests$' \
  --output-on-failure
```

Record combined selected totals; only the two established OSX tests are
excluded. The 650d isolated baseline is expected to have 159 registered and
157 selected.

### Task 6: Commit and Handoff

**Files:**
- Modify: none unless generated translations changed.

- [ ] **Step 1: Restore generated translations**

```bash
git restore --worktree -- \
  translations/deskflow_es.ts \
  translations/deskflow_it.ts \
  translations/deskflow_ja.ts \
  translations/deskflow_ru.ts \
  translations/deskflow_zh_CN.ts
```

- [ ] **Step 2: Commit implementation**

```bash
git add \
  src/lib/gui/input/InputIndicatorGeometry.h \
  src/lib/gui/input/InputIndicatorGeometry.cpp \
  src/lib/gui/input/MacInputIndicatorPanel.mm \
  src/unittests/gui/input/InputIndicatorGeometryTests.h \
  src/unittests/gui/input/InputIndicatorGeometryTests.mm
git commit \
  -m "feat(input): add source-colored cat startle glow" \
  -m "Co-authored-by: Copilot App <223556219+Copilot@users.noreply.github.com>"
```

- [ ] **Step 3: Audit and report**

```bash
git --no-pager diff --check
git --no-pager status --short --branch
git --no-pager log -3 --format='%H %s%n%b'
```

Report exact commits, fur changed-pixel delta, halo color/layer evidence, 1x/2x
clipping, focused/full counts, clean worktree, and no live/release/install
operation.
