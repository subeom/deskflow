# Dog and Turtle Companion Parity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give Dog and Turtle Cat-level shared pursuit, 49 pt rotation/facing, five idle patterns, Walk/Run gait art, source reaction/glow, and upright clipped labels without regressing Cat.

**Architecture:** The pure geometry layer owns species-neutral companion identity, idle/reaction aliases, gait pose, and finite Dog/Turtle point warps. A command-art header supplies distinct Bézier silhouettes; one AppKit painter applies the shared halo, body, rotation, glyph, and 49 pt canvas contract to all species.

**Tech Stack:** C++23, Objective-C++, Qt 6.12/QTest, AppKit, Bézier/Quartz, CMake/Ninja.

---

## File map

- `src/lib/gui/input/InputIndicatorGeometry.h/.cpp`
  own companion identity, idle/reaction aliases, gait pose, species point
  deformation, heading, and the shared pursuit model.
- `src/lib/gui/input/MacInputIndicatorCompanionArtwork.h`
  owns the Dog and Turtle Bézier command data, bounds, and inner label
  rectangles. This file has no drawing framework dependency.
- `src/lib/gui/input/MacInputIndicatorCatArtwork.h`
  remains the approved Cat source.
- `src/lib/gui/input/MacInputIndicatorPanel.h`
  defines panel options, render metrics, and render models for all three
  companions.
- `src/lib/gui/input/MacInputIndicatorPanel.mm`
  builds transformed paths, paints one source halo, body, and upright glyph,
  and advances the one shared panel state.
- `src/unittests/gui/input/InputIndicatorGeometryTests.h/.mm`
  contains pure model, path, 1x/2x raster, and native panel coverage.
- `src/unittests/gui/input/InputIndicatorPolicyTests.cpp`
  covers shared settings/controller and all-companion trigger behavior.
- `src/unittests/gui/settings/InputIndicatorSettingsTests.cpp` and
  `src/unittests/gui/settings/SettingsPagesTests.cpp`
  retain the companion-wide speed control.

No new settings or translations are introduced in this revision.

## Shared definitions

The pure geometry header defines:

```cpp
enum class InputIndicatorCompanion
{
  Cat,
  Dog,
  Turtle,
};

enum class InputIndicatorCompanionIdleMotion : unsigned char
{
  RelaxedSway,
  TipFlick,
  CurlUnfurl,
  LazySweep,
  QuestionLift
};
```

The Cat-named types are retained as aliases:

```cpp
using InputIndicatorCatIdleTailMotion =
    InputIndicatorCompanionIdleMotion;
using InputIndicatorCatIdleTailMotionState =
    InputIndicatorCompanionIdleMotionState;
using InputIndicatorCatIdleTailFrame =
    InputIndicatorCompanionIdleFrame;
using InputIndicatorCatSourceStartleState =
    InputIndicatorCompanionSourceStartleState;
using InputIndicatorCatStartleFrame =
    InputIndicatorCompanionStartleFrame;
```

Existing Cat code therefore remains source compatible while Dog and Turtle use
species-neutral names.

---

### Task 1: Add red shared model and species identity tests

**Files:**
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.mm`

- [ ] **Step 1: Declare the new pure tests**

Add:

```cpp
void companionModelUsesFiveSharedIdlePatterns();
void companionMotionPoseDistinguishesDogAndTurtleGaits();
void dogAndTurtleReactionPointsReturnToNeutral();
void companionHeadingTransformsBodyButNotGlyphBounds();
```

- [ ] **Step 2: Test companion identity and idle aliases**

For Cat, Dog, and Turtle:

```cpp
const auto state =
    inputIndicatorCompanionIdleMotionInitialState(0x12345678);
QVERIFY(state.seed != 0);
for (int completion = 0; completion < 100; ++completion) {
  const auto next =
      inputIndicatorCompanionIdleMotionAdvance(
          state,
          InputIndicatorMotionState::Idle,
          completion * 5000.0,
          false
      );
  QVERIFY(next.frame.active);
  QVERIFY(next.frame.progress >= 0);
  QVERIFY(next.frame.progress < 1);
}
```

Track the five enum values 0..4 and run enough deterministic completions to
cover each exactly. Assert two consecutive selected frames at boundary are
different. Assert Walk, Run, and Reduced Motion return `active == false`.

- [ ] **Step 3: Test pure gait poses**

For Dog and Turtle, call:

```cpp
inputIndicatorCompanionMotionPose(
    companion,
    InputIndicatorMotionState::Idle,
    0.25,
    false
);
inputIndicatorCompanionMotionPose(
    companion,
    InputIndicatorMotionState::Walk,
    0.25,
    false
);
inputIndicatorCompanionMotionPose(
    companion,
    InputIndicatorMotionState::Run,
    0.25,
    false
);
```

Assert finite `bodyBob`, `bodyStretch`, `tailPhase`, `limbStride`, and
`bodyLead`. Dog and Turtle Walk poses must differ from Idle, Run must differ
from Walk, and opposite phases must have opposite `limbStride` signs. Assert
Reduced Motion returns all zero fields.

- [ ] **Step 4: Test Dog and Turtle source point transforms**

Create a shared startle state at source A to Korean and get its 160 ms peak:

```cpp
const auto reaction =
    inputIndicatorCompanionSourceStartleAdvance(
        state, 160, false
    );
```

For Dog, transform an ear point `{20.0, 19.0}` and tail point `{3.0, 12.0}`.
For Turtle, transform a head point `{27.0, 13.0}`, shell point `{12.0, 17.0}`,
and foot point `{9.0, 4.0}`. Assert at least one coordinate differs at peak.
Advance at 300 and assert equality to the exact original points.

- [ ] **Step 5: Test heading and glyph center**

Use the same diagonal body point for Cat, Dog, and Turtle:

```cpp
const auto transformed =
    inputIndicatorCompanionBodyPoint(
        {.x = 17, .y = 11},
        true, 1, M_PI / 4
    );
```

Assert all return the same transformed coordinates. Build glyph bounds with
width/height unchanged before and after and assert only center translation
changes:

```cpp
QCOMPARE(transformedBounds.width, original.width);
QCOMPARE(transformedBounds.height, original.height);
```

- [ ] **Step 6: Run the red test build**

Run:

```bash
cmake --build /tmp/deskflow-cat-follow-speed-release \
  --config Release --parallel 8 \
  --target InputIndicatorGeometryTests
```

Expected: FAIL because companion enums, aliases, gait pose, artwork point, and
body-point functions do not exist.

---

### Task 2: Generalize identity, idle, reaction, gait, and point transforms

**Files:**
- Modify: `src/lib/gui/input/InputIndicatorGeometry.h`
- Modify: `src/lib/gui/input/InputIndicatorGeometry.cpp`
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.h`

- [ ] **Step 1: Move companion identity into pure geometry**

Add:

```cpp
enum class InputIndicatorCompanion : unsigned char
{
  Cat,
  Dog,
  Turtle,
};
```

Remove the duplicate enum from `MacInputIndicatorPanel.h`.

- [ ] **Step 2: Rename the idle and reaction types with aliases**

Rename the current Cat definitions in the header to
`InputIndicatorCompanionIdleMotion`,
`InputIndicatorCompanionIdleMotionState`,
`InputIndicatorCompanionIdleFrame`,
`InputIndicatorCompanionIdleMotionStep`,
`InputIndicatorCompanionSourceStartleState`,
`InputIndicatorCompanionStartleFrame`, and
`InputIndicatorCompanionSourceStartleStep`.

Immediately add old Cat aliases as listed in the plan header.

- [ ] **Step 3: Add the companion motion pose**

Define:

```cpp
struct InputIndicatorCompanionMotionPose
{
  double bodyBob = 0;
  double bodyStretch = 0;
  double tailPhase = 0;
  double limbStride = 0;
  double bodyLead = 0;
};
```

Declare:

```cpp
InputIndicatorCompanionMotionPose
inputIndicatorCompanionMotionPose(
    InputIndicatorCompanion companion,
    InputIndicatorMotionState state,
    double phase,
    bool reducedMotion
);
```

For Reduced Motion or invalid phase return `{}`. Use exact:

```cpp
const double wave = std::sin(
    std::clamp(phase, 0.0, 1.0) * 2.0 * M_PI
);
const double stride = std::cos(
    std::clamp(phase, 0.0, 1.0) * 2.0 * M_PI
);
```

Dog:

```cpp
Walk: bodyBob=wave*0.28, bodyStretch=abs(wave)*0.015,
      tailPhase=wave, limbStride=stride*0.9
Run: bodyBob=wave*0.42, bodyStretch=abs(wave)*0.035,
     tailPhase=wave*1.4, limbStride=stride*1.5,
     bodyLead=0.8
```

Turtle:

```cpp
Walk: bodyBob=wave*0.16, bodyStretch=abs(wave)*0.008,
      tailPhase=wave, limbStride=stride*0.55
Run: bodyBob=wave*0.30, bodyStretch=abs(wave)*0.020,
     tailPhase=wave, limbStride=stride*0.95,
     bodyLead=0.55
```

Cat keeps the existing `inputIndicatorMotionPose` values and maps them into
`bodyBob`, `bodyStretch`, and `tailPhase`.

- [ ] **Step 4: Add generic idle/reaction wrappers and artwork point transforms**

Declare:

```cpp
InputIndicatorPoint inputIndicatorCompanionArtworkPoint(
    InputIndicatorCompanion companion,
    InputIndicatorPoint point,
    InputIndicatorMotionState motionState,
    double motionPhase,
    const InputIndicatorCompanionIdleFrame &idleFrame,
    const InputIndicatorCompanionStartleFrame &startleFrame
);
```

Apply in order: gait, idle, reaction, but do not mirror or rotate; the panel
body-point transform does that after.

Implement:

- Dog ID 0: ear point `x >= 18 && y >= 15`, add
  `0.95 * smoothWindow(progress) * sin(pi*progress)` to y;
- Dog ID 1: tail `x <= 7`, add
  `0.8 * sin(2*pi*progress) * smoothWindow(progress)` to y;
- Dog ID 2: near ear region, subtract
  `0.65 * sin(pi*progress)^2` from y;
- Dog ID 3: tail, add
  `0.65 * sin(2*pi*progress)` to x/y;
- Dog ID 4: nose `x >= 24`, subtract
  `0.35 * pulseWindow(progress, 0.18, 0.55)`, and tail curl at
  `x <= 7`;
- Turtle ID 0: head `x >= 22`, add
  `1.10 * sin(pi*progress)` to x;
- Turtle ID 1: shell `6 <= x <= 23`, add
  `-0.25 * sin(pi*progress)` to y;
- Turtle ID 2: feet `y <= 7`, add
  `0.8 * sin(pi*progress)^2` to y;
- Turtle ID 3: head/tail regions, shift y by
  `0.30 * sin(2*pi*progress)`;
- Turtle ID 4: head `x >= 22`, add
  `0.75 * pulseWindow(progress, 0.15, 0.50)` to x and retract
  after `0.72`.

Gait regions:

- Dog feet where `y <= 7`: x offset from `limbStride`, with front feet
  `x >= 16` positive and rear feet negative;
- Turtle feet where `y <= 7`: x/y stride with alternating sign by `x < 14`;
- both bodies add `bodyBob`; run points receive `bodyLead` only in
  front-facing head/torso regions.

Reaction first applies the shared:

```cpp
x = 15 + (x - 15) * startleFrame.horizontalScale +
    startleFrame.recoilOffsetX;
```

Dog adds `dorsalLift * 0.45` to ear y and
`dorsalLift * 0.30` to tail flick. Turtle adds
`dorsalLift * 0.55` to head x and
`dorsalLift * 0.08` to shell y. If `active == false`, no reaction change.

- [ ] **Step 5: Add a generic body-point transform**

Declare:

```cpp
InputIndicatorPoint inputIndicatorCompanionBodyPoint(
    InputIndicatorPoint point,
    bool facesRight,
    double facingHorizontalScale,
    double headingRadians
);
```

Move the current Cat implementation to this name, then make
`inputIndicatorCatBodyPoint` a thin wrapper.

- [ ] **Step 6: Run pure tests**

Run the geometry executable. Expected: the four new pure tests pass and all
existing Cat pure tests retain exact values.

---

### Task 3: Add red Dog/Turtle artwork and raster tests

**Files:**
- Create: `src/lib/gui/input/MacInputIndicatorCompanionArtwork.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.mm`

- [ ] **Step 1: Add artwork declarations to tests**

Add slots:

```cpp
void companionArtworkUsesContinuousSpeciesSilhouettes();
void dogAndTurtleIdleAndGaitPixelsChangeAtOneAndTwoX();
void dogAndTurtleReactionAndHaloPixelsUseOneLayer();
void allCompanionCanvasContainsEightHeadings();
```

- [ ] **Step 2: Assert the Dog and Turtle art contract**

Require:

```cpp
macInputIndicatorDogArtwork().silhouette.size() >= 40;
macInputIndicatorTurtleArtwork().silhouette.size() >= 32;
```

Assert no `MacInputIndicatorCatPathCommand` in Dog/Turtle art is a Move that
starts a primitive rectangle/ellipse API (the data is command only), and
assert bounds are within logical `x 1..29`, `y 3..23`, and label bounds are
inside silhouette bounds.

- [ ] **Step 3: Add a generic bitmap helper**

Add `companionBodyBitmap(...)` beside `catBodyBitmap` with arguments:

```cpp
InputIndicatorCompanion companion,
InputIndicatorMotionState motionState,
double motionPhase,
const InputIndicatorCompanionIdleFrame &idle,
const InputIndicatorCompanionStartleFrame &reaction,
bool facesRight,
double facingScaleX,
double heading,
int scale,
int padding,
bool includeStroke,
bool includeGlow,
InputIndicatorSource glowSource
```

It builds the path from `MacInputIndicatorCompanionArtwork.h`, transforms
each command through `inputIndicatorCompanionArtworkPoint`, then
`inputIndicatorCompanionBodyPoint`, translates/scales exactly as the panel,
draws halo first, body second, and glyph separately.

- [ ] **Step 4: Add 1x/2x idle/gait deltas**

For Dog and Turtle, for each of the five idle frames at representative
progresses, compare baseline to deformed body and assert changed pixels > 0,
1x/2x. Compare Walk phase `0.0` to `0.5`, and Run phase `0.0` to `0.5`, and
assert changes > 0. Assert the glyph difference is zero because labels stay
upright and centered.

- [ ] **Step 5: Add reaction/glow pixel tests**

At 0, 160, and 300:

- `outside-body haloOnly == 0` at 0 and 300;
- `haloOnly > 0` at 160;
- source hue matches Latin slate or Korean rose on halo core;
- body mask excludes stroke from halo mask;
- Reaction changed body pixels return to zero at 300.

For both Dog and Turtle at 1x and 2x.

- [ ] **Step 6: Add 128 heading matrix**

Loop:

```cpp
for companion in {Dog, Turtle}
for facesRight in {true, false}
for source in {LatinABC, Korean2Set}
for heading in eight values
for scale in {1, 2}
```

At 160 ms reaction, paint body+halo and glyph. Count:

```cpp
paintOutsidePixels
glyphOutsidePixels
outsidePixels
haloOnlyPixels
```

Require `paintOutsidePixels == 0`, `glyphOutsidePixels == 0`,
`outsidePixels == 0`, 128 cases, and halo-only pixels > 0.

- [ ] **Step 7: Run the red raster test build**

Expected: FAIL because the artwork header, pure functions, and generic
bitmap helper are incomplete.

---

### Task 4: Add continuous Dog and Turtle Bézier artwork

**Files:**
- Create: `src/lib/gui/input/MacInputIndicatorCompanionArtwork.h`
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.h`

- [ ] **Step 1: Define the shared artwork struct**

```cpp
struct MacInputIndicatorCompanionArtwork
{
  std::span<const MacInputIndicatorCatPathCommand> silhouette;
  InputIndicatorRect silhouetteBounds;
  InputIndicatorRect innerLanguageBounds;
};
```

- [ ] **Step 2: Add the Dog path**

Use only Move, Cubic, Line, and Close commands. Build these contiguous
subpaths:

```cpp
// torso/head/neck/muzzle
Move(7.5, 8.0)
Cubic(10.0, 5.0, 15.0, 4.5, 20.0, 6.0)
Cubic(21.5, 6.8, 22.0, 8.0, 22.2, 10.0)
Cubic(23.5, 10.5, 26.4, 11.8, 27.4, 13.0)
Cubic(28.0, 14.1, 27.1, 15.5, 25.0, 15.2)
Cubic(23.8, 15.0, 22.9, 14.6, 22.1, 14.8)
Cubic(22.0, 17.5, 20.7, 19.5, 18.4, 19.2)
Cubic(17.8, 17.8, 17.0, 16.2, 17.2, 15.0)
Cubic(15.1, 17.0, 11.8, 16.9, 9.1, 14.4)
Cubic(7.9, 13.0, 7.2, 10.6, 7.5, 8.0)
Close
```

Add separate tail:

```cpp
Move(8.0, 9.0)
Cubic(4.6, 7.8, 1.1, 10.0, 2.5, 13.2)
Cubic(3.5, 15.4, 6.5, 14.0, 8.0, 11.4)
Close
```

Add near/far ear and paw subpaths as cubic lobes. Paw regions are y 3.5..7
and x 8..12, 13..15, 18.5..22. Use no rectangle or oval primitives.

Return:

```cpp
.silhouetteBounds = {1.0, 3.0, 27.5, 19.0}
.innerLanguageBounds = {10.0, 7.0, 8.0, 7.0}
```

- [ ] **Step 3: Add the Turtle path**

Build shell:

```cpp
Move(5.0, 8.0)
Cubic(6.0, 4.4, 10.0, 3.4, 15.0, 3.8)
Cubic(20.0, 4.2, 23.5, 7.0, 23.5, 11.0)
Cubic(23.6, 15.8, 20.1, 18.7, 15.5, 18.4)
Cubic(10.3, 18.4, 5.6, 15.8, 5.0, 11.0)
Cubic(4.5, 10.0, 4.7, 9.0, 5.0, 8.0)
Close
```

Add head/neck:

```cpp
Move(22.0, 12.0)
Cubic(24.5, 11.0, 27.5, 10.8, 28.6, 12.3)
Cubic(29.2, 13.5, 28.3, 15.5, 26.3, 16.0)
Cubic(24.8, 16.3, 23.4, 15.1, 22.1, 14.5)
Close
```

Add tapered tail and four curved foot lobes. Return:

```cpp
.silhouetteBounds = {1.0, 3.0, 28.0, 16.0}
.innerLanguageBounds = {10.0, 7.0, 8.0, 7.0}
```

- [ ] **Step 4: Update render models and style**

For Dog/Turtle in `macInputIndicatorRenderModel`, set:

```cpp
.usesSideProfile = true,
.mirrorsWithMotion = true,
.sourceFacesRight = true,
.silhouetteBounds = artwork.silhouetteBounds,
.languageBounds = artwork.innerLanguageBounds
```

In `macInputIndicatorPanelStyle`, make `canvasWidth == 49`,
`canvasHeight == 49`, `canvasOffsetX == 8`, `canvasOffsetY == 9`
for every companion.

- [ ] **Step 5: Run pure/art tests**

Build geometry and run until command count, bounds, and finite point tests
pass. Expected: Cat style tests update to 49 for all while width/height and
offset anchor remain the same.

---

### Task 5: Paint one shared silhouette/glow/glyph pipeline

**Files:**
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.mm`

- [ ] **Step 1: Extract one halo painter**

Add:

```cpp
void drawCompanionHalo(
    NSBezierPath *silhouette,
    const InputIndicatorCompanionStartleFrame &reaction,
    InputIndicatorSource reactionSource,
    InputIndicatorTone tone,
    MacInputIndicatorRenderAlpha alpha
)
```

It:

1. rejects inactive/zero-alpha/reduced motion at its caller;
2. copies the path;
3. scales once around `(15, 14)`;
4. creates one 2.2 pt zero-offset shadow in final glyph color;
5. fills the path once.

Refactor `drawCat` to call this helper with its near+far union. Add Dog/Turtle
calls with their one union.

- [ ] **Step 2: Build a transformed species path**

Add:

```cpp
NSBezierPath *companionArtworkPath(
    InputIndicatorCompanion companion,
    std::span<const MacInputIndicatorCatPathCommand> commands,
    InputIndicatorMotionState state,
    double phase,
    const InputIndicatorCompanionIdleFrame &idle,
    const InputIndicatorCompanionStartleFrame &reaction,
    bool facesRight,
    double facingScaleX,
    double heading
)
```

For each command point:

```cpp
point = inputIndicatorCompanionArtworkPoint(
    companion, point, state, phase, idle, reaction
);
point = inputIndicatorCompanionBodyPoint(
    point, facesRight, facingScaleX, heading
);
```

Use the same Move/Line/Cubic/Close loop as Cat. Cat continues to use its
approved torso/tail functions rather than this generic artwork path.

- [ ] **Step 3: Replace Dog/Turtle primitive drawing**

`drawDog` and `drawTurtle` now take:

```cpp
InputIndicatorMotionState motionState,
double motionPhase,
bool reducedMotion,
bool facesRight,
double facingScaleX,
double bodyHeadingRadians,
const InputIndicatorCompanionIdleFrame &idle,
const InputIndicatorCompanionStartleFrame &reaction,
InputIndicatorSource reactionSource,
InputIndicatorTone tone,
MacInputIndicatorRenderAlpha alpha
```

Build the species path, call `drawCompanionHalo`, then
`fillSilhouette(path, fill, edge)`. Remove all rounded-rectangle, ellipse,
and primitive-stack art.

- [ ] **Step 4: Unify glyph clipping**

In `drawLanguageGlyph`:

- for Cat, retain approved Cat silhouette and label bounds;
- for Dog/Turtle, use `macInputIndicatorRenderModel(companion).languageBounds`
  and their transformed current species path;
- transform only the label center with
  `inputIndicatorCompanionBodyPoint(... heading ...)`;
- use the same CoreText fit loop for all species;
- clip all glyphs to their species body path and label rectangle;
- never rotate, mirror, scale, or startle-deform the glyph.

- [ ] **Step 5: Run raster tests**

Expected: all Dog/Turtle 1x/2x idle/gait/reaction/glow checks pass and Cat
pixel logs remain exact.

---

### Task 6: Generalize panel idle, reaction, facing, and heading state

**Files:**
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.mm`

- [ ] **Step 1: Trigger reaction for every companion**

Replace:

```cpp
if (options.companion == InputIndicatorCompanion::Cat)
```

around `inputIndicatorCatSourceStartleTrigger` with an all-companion call.
Keep `InputIndicatorSource::Unknown` rejection, exact source change, 50 ms
coalescing, and the existing shared state.

- [ ] **Step 2: Run idle for every companion**

In `move`, replace Cat-only `startleStep` and idle advance gates with:

- `inputIndicatorCompanionSourceStartleAdvance` for every companion;
- `startleSuspendsIdle` for every companion;
- `inputIndicatorCompanionIdleMotionAdvance` receives current
  `motionState` for every companion, not a synthetic Walk.

Cat aliases keep existing APIs/test names. Reduced Motion returns inactive.

- [ ] **Step 3: Apply facing/heading to all**

Remove the `if (companion != Cat)` early branch in facing logic. Every
companion uses `macInputIndicatorFacingPose` and stores
`inputIndicatorCompanionPursuitState.headingRadians`.

Set the existing view heading property for every companion; Dog/Turtle painter
draw paths use it.

- [ ] **Step 4: Pass complete frames to drawing**

Add `idleFrame` and `startleFrame` to Dog/Turtle draw calls, plus
reaction source, tone, alpha, facing, and heading. `drawLanguageGlyph` receives
the same values and rebuilds the matching body path for clipping.

- [ ] **Step 5: Update old Dog/Turtle no-startle test**

Rename `otherCompanionsDoNotStartleOnSourceChange` to
`dogAndTurtleStartleOnSourceChange`. Assert:

```cpp
catStartleActive` becomes true after `move`;
progress finite;
latestSource == Korean2Set;
result.frameIntervalMilliseconds == 8;
```

- [ ] **Step 6: Run panel and pure tests**

Run `InputIndicatorPolicyTests` and `InputIndicatorGeometryTests`.
Expected: all shared source, idle, speed, pursuit, and non-snap tests pass.

---

### Task 7: Add fixed panel parity and Reduced Motion coverage

**Files:**
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.mm`

- [ ] **Step 1: Add panel slots**

```cpp
void dogAndTurtleIdlePatternsRepaint();
void dogAndTurtleReactionCoalescesAndEnds();
void allCompanionPanelsHonorHeadingAndReducedMotion();
```

- [ ] **Step 2: Prove idle repaint**

For Dog and Turtle:

- `show` at fixed pointer;
- wait 650 ms;
- `move`;
- capture before/after bitmap;
- assert changed pixels > 0 at WindowServer backing scale;
- idle frame active and interval 75;
- only the species motif region changes.

- [ ] **Step 3: Prove reaction/coalescing**

For Dog and Turtle:

- show Latin, then Korean after 20 ms, then Latin after another 20 ms;
- after 50 ms move, pending advances once;
- capture peak body and halo;
- assert latest source Latin (slate);
- at 300 ms frame inactive and halo pixels zero;
- idle resumes at its prior ID/progress.

Repeat one direct Latin-to-Korean transition for rose evidence.

- [ ] **Step 4: Prove heading and Reduced Motion**

For each species at one diagonal jump:

- read `catBodyHeadingRadians`, assert finite and nonzero;
- glyph bitmap bounds width/height do not change when heading changes;
- glyph is inside body and upright (horizontal ink bounds).

Force Reduced Motion with the existing test hook/environment and assert one
move reaches the target, no idle active, startle active, or halo alpha.

- [ ] **Step 5: Run geometry under WindowServer**

Expected: Cat/Dog/Turtle panel cases all pass, with Cat pixel thresholds
preserved.

---

### Task 8: Run the focused 15-target gate

**Files:**
- Modify: none unless a change-caused failure exists.

- [ ] **Step 1: Build the 15 focused targets**

```bash
cmake --build /tmp/deskflow-cat-follow-speed-release \
  --config Release --parallel 8 --target \
  SettingsTests \
  SettingsPagesTests \
  InputIndicatorSettingsTests \
  InputIndicatorPolicyTests \
  InputIndicatorGeometryTests \
  CoreProcessInputIndicatorTests \
  InputIndicatorIpcTests \
  OSXInputIndicatorRelayTests \
  ServiceQuitTests \
  TranslationFileTests \
  DeskflowStyleTests \
  CanvasThemeTests \
  IconsTests \
  TrayIconTests \
  MainShellTests
```

- [ ] **Step 2: Run exactly 15 CTests**

```bash
env QT_QPA_PLATFORM=offscreen \
  HOME=/tmp/deskflow-cat-follow-speed-scratch/home \
  XDG_CACHE_HOME=/tmp/deskflow-cat-follow-speed-scratch/cache \
  XDG_CONFIG_HOME=/tmp/deskflow-cat-follow-speed-scratch/config \
  TMPDIR=/Users/subeomlee/.copilot/session-state/0e04e54a-1ee4-49a1-867f-877e8413aab6/files/cat-follow-speed-tmp \
  ctest --test-dir \
    /tmp/deskflow-cat-follow-speed-release/src/unittests \
  -R '^(SettingsTests|SettingsPagesTests|InputIndicatorSettingsTests|InputIndicatorPolicyTests|InputIndicatorGeometryTests|CoreProcessInputIndicatorTests|InputIndicatorIpcTests|OSXInputIndicatorRelayTests|ServiceQuitTests|TranslationFileTests|DeskflowStyleTests|CanvasThemeTests|IconsTests|TrayIconTests|MainShellTests)$' \
  --output-on-failure
```

Expected: `15/15`.

---

### Task 9: Run the full safe Release gate

**Files:**
- Modify: none unless generated unrelated translation files changed.

- [ ] **Step 1: Build all registered CTest targets**

```bash
BUILD=/tmp/deskflow-cat-follow-speed-release
ctest --test-dir "$BUILD/src/unittests" -N -V \
  > "$BUILD/ctest-list.txt"
TEST_TARGETS="$(
  sed -nE 's/^ *Test +#[0-9]+: //p' \
    "$BUILD/ctest-list.txt" |
  grep -v '^GitVersionTests$' |
  sort -u | tr '\n' ' '
)"
cmake --build "$BUILD" --config Release --parallel 8 \
  --target $TEST_TARGETS
```

- [ ] **Step 2: Run all safe CTests except Geometry**

```bash
env QT_QPA_PLATFORM=offscreen \
  HOME=/tmp/deskflow-cat-follow-speed-scratch/home \
  XDG_CACHE_HOME=/tmp/deskflow-cat-follow-speed-scratch/cache \
  XDG_CONFIG_HOME=/tmp/deskflow-cat-follow-speed-scratch/config \
  TMPDIR=/Users/subeomlee/.copilot/session-state/0e04e54a-1ee4-49a1-867f-877e8413aab6/files/cat-follow-speed-tmp \
  ctest --test-dir "$BUILD/src/unittests" \
  -E '^(OSXClipboardTests|OSXKeyStateTests|InputIndicatorGeometryTests)$' \
  --output-on-failure
```

- [ ] **Step 3: Run isolated Geometry**

```bash
env QT_QPA_PLATFORM=offscreen \
  HOME=/tmp/deskflow-cat-follow-speed-scratch/home \
  XDG_CACHE_HOME=/tmp/deskflow-cat-follow-speed-scratch/cache \
  XDG_CONFIG_HOME=/tmp/deskflow-cat-follow-speed-scratch/config \
  TMPDIR=/Users/subeomlee/.copilot/session-state/0e04e54a-1ee4-49a1-867f-877e8413aab6/files/cat-follow-speed-tmp \
  ctest --test-dir "$BUILD/src/unittests" \
  -R '^InputIndicatorGeometryTests$' \
  --output-on-failure
```

Expected: every selected test passes; only
`OSXClipboardTests` and `OSXKeyStateTests` are excluded.

---

### Task 10: Review pixels, clean generated files, and commit

**Files:**
- Stage: `src/lib/gui/input/InputIndicatorGeometry.h`
- Stage: `src/lib/gui/input/InputIndicatorGeometry.cpp`
- Stage: `src/lib/gui/input/MacInputIndicatorCompanionArtwork.h`
- Stage: `src/lib/gui/input/MacInputIndicatorPanel.h`
- Stage: `src/lib/gui/input/MacInputIndicatorPanel.mm`
- Stage: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
- Stage: `src/unittests/gui/input/InputIndicatorGeometryTests.mm`
- Stage: `src/unittests/gui/input/InputIndicatorPolicyTests.cpp`
- Restore generated translations if any build changed them.

- [ ] **Step 1: Review exact pixel evidence**

From geometry output record:

```text
Cat outsidePixels, paintOutside, glyphOutside
Dog/Turtle outsidePixels
idle 1x/2x changed pixels
Walk/Run 1x/2x changed pixels
haloOnly/core/color pixels
halo 0/160/300 endpoint counts
128 heading cases
```

Any zero idle/gait delta, any outside pixel, or endpoint halo is a failing
gate.

- [ ] **Step 2: Restore unrelated generated files**

Run:

```bash
git restore --worktree -- \
  translations/deskflow_es.ts \
  translations/deskflow_it.ts \
  translations/deskflow_ja.ts \
  translations/deskflow_ru.ts \
  translations/deskflow_zh_CN.ts
```

Korean is unchanged by this revision unless a translation build changed
line endings; restore it too if its source diff is empty.

- [ ] **Step 3: Inspect and commit**

Run:

```bash
git --no-pager diff --check
git --no-pager status --short
git --no-pager diff --stat
```

Then:

```bash
git add \
  src/lib/gui/input/InputIndicatorGeometry.h \
  src/lib/gui/input/InputIndicatorGeometry.cpp \
  src/lib/gui/input/MacInputIndicatorCompanionArtwork.h \
  src/lib/gui/input/MacInputIndicatorPanel.h \
  src/lib/gui/input/MacInputIndicatorPanel.mm \
  src/unittests/gui/input/InputIndicatorGeometryTests.h \
  src/unittests/gui/input/InputIndicatorGeometryTests.mm \
  src/unittests/gui/input/InputIndicatorPolicyTests.cpp
git commit \
  -m "feat(input): animate dog and turtle companions" \
  -m "Co-authored-by: Copilot App <223556219+Copilot@users.noreply.github.com>"
```

Expected: clean worktree, implementation commit descending from the speed
implementation and parity spec/plan commits. No push, release, install, or PR.
