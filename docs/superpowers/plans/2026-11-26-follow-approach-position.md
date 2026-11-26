# Follow-Approach Position Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a live fifth `Follow approach` cursor placement mode that stops every companion behind the cursor on its retained approach direction while preserving all four fixed modes.

**Architecture:** The pure placement helper takes an optional last-valid heading and computes exact support-distance geometry before screen clamp. The panel advances pursuit with that target, then stores the new finite heading only after a meaningful bounded movement; the controller changes placement mode without re-showing the panel.

**Tech Stack:** C++23, Objective-C++, Qt 6.12/QTest, AppKit, CMake/Ninja.

---

## File map

- `src/lib/gui/input/InputIndicatorGeometry.h/.cpp`: enum, pure support geometry,
  and preserved fixed-mode helper behavior.
- `src/lib/gui/input/MacInputIndicatorController.mm`: exact setting mapping and
  direct live placement update.
- `src/lib/gui/input/MacInputIndicatorPanel.mm`: previous-heading target
  calculation, meaningful-step retention, and bounded direction transitions.
- `src/lib/gui/settings/SettingsPages.cpp`: fifth combo item.
- `translations/deskflow_ko.ts`: Korean label only.
- `src/unittests/gui/input/InputIndicatorGeometryTests.h/.mm`: pure angles,
  fallback, edge/multi-screen, panel movement, and raster checks.
- `src/unittests/gui/input/InputIndicatorPolicyTests.h/.cpp`: controller
  live-setting/non-reconfiguration tests.
- `src/unittests/gui/settings/InputIndicatorSettingsTests.*` and
  `SettingsPagesTests.cpp`: combo values/default behavior.

No new setting key, panel size, or companion motion constant is added.

## Test environment

Use the existing Release build and canonical scratch path:

```bash
BUILD=/tmp/deskflow-cat-follow-speed-release
SAFE_TMP=/Users/subeomlee/.copilot/session-state/0e04e54a-1ee4-49a1-867f-877e8413aab6/files/cat-follow-speed-tmp
HOME_DIR=/tmp/deskflow-cat-follow-speed-scratch/home
CACHE_DIR=/tmp/deskflow-cat-follow-speed-scratch/cache
CONFIG_DIR=/tmp/deskflow-cat-follow-speed-scratch/config
mkdir -p "$HOME_DIR" "$CACHE_DIR" "$CONFIG_DIR" "$SAFE_TMP"
```

---

### Task 1: Add red pure follow-approach geometry tests

**Files:**
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.mm`

- [ ] **Step 1: Declare exact geometry tests**

Add:

```cpp
void followApproachUsesExactRectangleSupportAtEightHeadings();
void followApproachFallbackAndFixedModesStayCompatible();
void followApproachClampsEveryScreenEdgeAndCorner();
```

- [ ] **Step 2: Test eight angles**

Use:

```cpp
const InputIndicatorSize badge{.width = 33, .height = 31};
const InputIndicatorPoint cursor{.x = 500, .y = 500};
const double headings[] = {
  0, M_PI/4, M_PI/2, 3*M_PI/4,
  M_PI, 5*M_PI/4, 3*M_PI/2, 7*M_PI/4
};
```

Call:

```cpp
positionInputIndicator(
    cursor, badge, oneScreen(),
    InputIndicatorPosition::CursorFollowApproach,
    7, 7, heading
)
```

Reconstruct:

```cpp
const InputIndicatorPoint direction{
    .x = std::cos(heading),
    .y = std::sin(heading),
};
const InputIndicatorPoint center{
    .x = result->x + badge.width / 2.0,
    .y = result->y + badge.height / 2.0,
};
const double support =
    std::abs(direction.x) * badge.width / 2.0 +
    std::abs(direction.y) * badge.height / 2.0;
const InputIndicatorPoint supportPoint{
    .x = center.x + direction.x * support,
    .y = center.y + direction.y * support,
};
```

Assert `cursor-supportPoint` is 7.0 within `1e-9`, and:

```cpp
QVERIFY((center.x-cursor.x) * direction.x +
        (center.y-cursor.y) * direction.y < 0);
```

- [ ] **Step 3: Test exact fixed compatibility and fallback**

For Bottom right, Top right, Bottom left, and Top left:

1. call the helper without optional heading;
2. call again with `0.37` as optional heading;
3. `QCOMPARE` both origins.

For Follow approach with:

```cpp
std::nullopt
std::numeric_limits<double>::quiet_NaN()
std::numeric_limits<double>::infinity()
```

assert exact equality to Bottom right.

- [ ] **Step 4: Test four edges, four corners, and multiple screens**

Use a 1920x1080 screen with margin 7. Select cardinal/diagonal headings whose
unclamped desired origin goes beyond each boundary. Assert x/y equal the exact
screen margin/max values.

Use adjacent screens:

```text
[-1280, 0, 1280, 800]
[0, 0, 1920, 1080]
```

Put cursor at `{-5, 400}`, force heading 0 and pi, and assert the result
remains on the negative-x screen. Assert `{-1281, 400}` returns no value.

- [ ] **Step 5: Run the red geometry build**

Run:

```bash
cmake --build "$BUILD" --config Release --parallel 8 \
  --target InputIndicatorGeometryTests
```

Expected: FAIL because `CursorFollowApproach` and the optional-heading
argument do not exist.

---

### Task 2: Implement pure Follow approach support geometry

**Files:**
- Modify: `src/lib/gui/input/InputIndicatorGeometry.h`
- Modify: `src/lib/gui/input/InputIndicatorGeometry.cpp`

- [ ] **Step 1: Add the enum value and optional argument**

Append, do not reorder:

```cpp
enum class InputIndicatorPosition
{
  CursorBottomRight,
  CursorTopRight,
  CursorBottomLeft,
  CursorTopLeft,
  CursorFollowApproach
};
```

Change only the declaration to:

```cpp
std::optional<InputIndicatorPoint> positionInputIndicator(
    InputIndicatorPoint cursor,
    InputIndicatorSize badge,
    const std::vector<InputIndicatorRect> &screens,
    InputIndicatorPosition position,
    double gap,
    double margin,
    std::optional<double> approachHeadingRadians =
        std::nullopt
);
```

- [ ] **Step 2: Keep fixed branches byte-for-byte equivalent**

After screen and bounds validation, select:

```cpp
InputIndicatorPoint result;
if (position == CursorFollowApproach &&
    approachHeadingRadians &&
    std::isfinite(*approachHeadingRadians)) {
  ...
} else {
  // Existing fixed branch, copied without changed offsets.
  const bool onLeft = ...
  const bool above = ...
  result.x = ...
  result.y = ...
}
```

For dynamic:

```cpp
const InputIndicatorPoint direction{
    .x = std::cos(*approachHeadingRadians),
    .y = std::sin(*approachHeadingRadians),
};
const double support =
    std::abs(direction.x) * badge.width / 2.0 +
    std::abs(direction.y) * badge.height / 2.0;
const InputIndicatorPoint center{
    .x = cursor.x -
        direction.x * (support + gap),
    .y = cursor.y -
        direction.y * (support + gap),
};
result = {
    .x = center.x - badge.width / 2.0,
    .y = center.y - badge.height / 2.0,
};
```

Clamp exactly once at the existing `minimumX/maximumX/minimumY/maximumY`.

- [ ] **Step 3: Run pure geometry green**

Run the full `InputIndicatorGeometryTests` executable and verify old
`bottomRightOffsetsCursor`, `positionsFlipInsideScreen`, and
`everyPositionIsPointerRelative` still pass without changed expected values.

---

### Task 3: Add red settings and controller live-mode tests

**Files:**
- Modify: `src/unittests/gui/settings/InputIndicatorSettingsTests.cpp`
- Modify: `src/unittests/gui/settings/SettingsPagesTests.cpp`
- Modify: `src/unittests/gui/input/InputIndicatorPolicyTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorPolicyTests.cpp`

- [ ] **Step 1: Assert the fifth combo item**

In both settings page tests:

```cpp
const auto follow =
    position->findData(
        QStringLiteral("cursorFollowApproach")
    );
QVERIFY(follow >= 0);
QCOMPARE(
    position->itemText(follow),
    QCoreApplication::translate(
        "SettingsPages", "Follow approach"
    )
);
```

Assert initial `position->currentData()` remains
`cursorBottomRight`; select Follow approach and assert Settings stores
`cursorFollowApproach`.

- [ ] **Step 2: Extend controller harness movement capture**

In `PersistentControllerHarness`, add:

```cpp
std::vector<InputIndicatorPosition> movePositions;
```

In `panelMove`, append the received position.

- [ ] **Step 3: Add the live controller test**

Declare:

```cpp
void persistentPositionModeUpdatesWithoutPanelReconfigure();
```

Set `InputIndicatorPosition` to `cursorFollowApproach` before construction,
arm, and assert:

```cpp
harness.presentations.back().position ==
    InputIndicatorPosition::CursorFollowApproach;
```

Record presentation/hide/move counts. Set position to
`cursorTopLeft`. Assert:

```cpp
presentations unchanged
hides unchanged
moves increases
movePositions.back() == CursorTopLeft
```

Set back to `cursorFollowApproach` and assert the same no-reconfigure rule.

- [ ] **Step 4: Verify red**

Build the three targets:

```bash
cmake --build "$BUILD" --config Release --parallel 8 \
  --target InputIndicatorSettingsTests \
           SettingsPagesTests \
           InputIndicatorPolicyTests
```

Expected: FAIL because the combo item and controller mapping do not exist.

---

### Task 4: Implement UI, translation, and live controller mode

**Files:**
- Modify: `src/lib/gui/settings/SettingsPages.cpp`
- Modify: `translations/deskflow_ko.ts`
- Modify: `src/lib/gui/input/MacInputIndicatorController.mm`

- [ ] **Step 1: Add the fifth combo choice**

Append after Top left:

```cpp
{
    tx(QT_TRANSLATE_NOOP(
        "SettingsPages", "Follow approach"
    )),
    QStringLiteral("cursorFollowApproach")
}
```

- [ ] **Step 2: Add Korean text**

In `deskflow_ko.ts` near the existing position labels:

```xml
<message>
    <source>Follow approach</source>
    <translation>접근 방향 따라가기</translation>
</message>
```

Do not edit other `.ts` files.

- [ ] **Step 3: Map the exact stored value**

In `positionFromSetting`:

```cpp
if (value == QStringLiteral("cursorFollowApproach")) {
  return InputIndicatorPosition::CursorFollowApproach;
}
```

Keep Bottom right as final fallback.

- [ ] **Step 4: Handle position settings without reconfiguration**

In `settingsChanged`, before the panel reconfiguration key group:

```cpp
if (key ==
    Settings::Gui::InputIndicatorPosition) {
  m_position = positionFromSetting(
      Settings::value(key).toString()
  );
  if (m_panelConfigured) {
    movePointerBadge();
  }
  return;
}
```

Remove `InputIndicatorPosition` from the reconfiguration condition.

- [ ] **Step 5: Run settings/controller green**

Expected:

```text
3/3 CTests
```

for `InputIndicatorSettingsTests`, `SettingsPagesTests`, and
`InputIndicatorPolicyTests`.

---

### Task 5: Add and implement retained approach-heading state in the panel

**Files:**
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.mm`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.mm`

- [ ] **Step 1: Add panel-state tests**

Add:

```cpp
void followApproachUsesLastHeadingWhileIdle();
void followApproachHeadingChangeTraversesIntermediatePositions();
void followApproachFirstShowAndInvalidStateUseBottomRight();
```

The first test initializes at fixed pointer with
`CursorFollowApproach`, moves the target diagonally in a 100 pt jump, advances
until the motion heading is retained, then advances with no cursor change.
Assert the follower origin remains on the retained side and does not jitter.

- [ ] **Step 2: Add state fields**

In `Impl`:

```cpp
bool approachHeadingValid = false;
double approachHeadingRadians = 0;
```

Do not reset either on source, Space, speed, or placement settings. Reset only
if/when the underlying panel/pursuit is destroyed or explicitly reinitialized
from an uninitialized state.

- [ ] **Step 3: Compute the dynamic target from prior heading**

At the start of `move`:

```cpp
std::optional<double> approachHeading;
if (position ==
        InputIndicatorPosition::CursorFollowApproach &&
    m_impl->approachHeadingValid &&
    std::isfinite(
        m_impl->approachHeadingRadians
    )) {
  approachHeading =
      m_impl->approachHeadingRadians;
}
const auto target =
    positionInputIndicator(
        targetLocation.point,
        {.width = style.width,
         .height = style.height},
        screenRects(),
        position,
        style.cursorGap,
        style.screenMargin,
        approachHeading
    );
```

- [ ] **Step 4: Retain only meaningful new movement**

Before pursuit:

```cpp
const bool initializePursuit =
    !m_impl->pursuitState.initialized;
```

After a valid step:

```cpp
if (!initializePursuit &&
    !reducedMotion &&
    pursuit->distanceMoved >
        kSettledDistance &&
    hadPursuitPosition) {
  const double heading = std::atan2(
      targetLocation.point.y -
          previousPursuitPosition.y,
      targetLocation.point.x -
          previousPursuitPosition.x
  );
  if (!m_impl->approachHeadingValid ||
      std::abs(inputIndicatorNormalizedAngle(
          heading -
          m_impl->approachHeadingRadians
      )) > 0.01) {
    m_impl->approachHeadingRadians = heading;
    m_impl->approachHeadingValid = true;
  }
  nextPursuitState.headingRadians =
      m_impl->approachHeadingRadians;
}
```

Compute the retained angle from the real cursor target and previous pursuit
position, not from the dynamic offset target. Do not write heading for
initialize, Reduced Motion, zero motion, invalid values, or a sub-0.01 radian
change.

- [ ] **Step 5: Prove smooth direction reversal**

At 50%, place an 800 pt target first to the right, advance several frames,
then jump the cursor target to the left. Record origins for 20+ ticks. Assert:

```cpp
positions.size() > 20
each movement <= 2.88 + sqrt(2.0)
no target-to-target teleport
direction eventually changes to the other support side
```

Repeat minimum representative cases for Cat, Dog, and Turtle; 100/200 and
AppKit clipping are covered in Task 6.

---

### Task 6: Add native 1x/2x companion/speed/approach raster tests

**Files:**
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.mm`

- [ ] **Step 1: Add the matrix test**

Declare:

```cpp
void followApproachCompanionSpeedRasterMatrix();
```

Loop:

```cpp
for companion in {Cat, Dog, Turtle}
for scale in {1, 2}
for speed in {{50, 2.88}, {100, 5.76}, {200, 11.52}}
for approachHeading in eight values
```

The direct helper produces a paint/body/glyph bitmap in the selected 49x49
canvas. For each panel frame, record the expected dynamic target and:

- `paintOutsidePixels == 0`;
- `glyphOutsidePixels == 0`;
- `outsidePixels == 0`;
- movement <= pure cap plus whole-point quantization;
- final visible anchor center support gap is 7 pt within 1 pt/axis;
- at least 10/40/100 intermediate frames for 200/100/50.

- [ ] **Step 2: Verify fixed mode pixels are unchanged**

Run the same body/glyph checks for Bottom right, Top right, Bottom left, and
Top left and compare origins to the existing exact expected values from
`everyPositionIsPointerRelative`.

- [ ] **Step 3: Verify invalid state and first show**

Capture Follow approach first visible origin and compare it to Bottom right
within AppKit point quantization. Assert the panel is visible, initialized at
that point, and no second-show/source/speed operation returns to the cursor.

- [ ] **Step 4: Run Geometry**

Run the direct executable, then isolated CTest. Expected: 128 Dog/Turtle
artwork and all previous Cat pixel logs remain zero/outside and the new
Follow approach raster cases are green.

---

### Task 7: Run focused 15-target and full safe Release gates

**Files:**
- Modify: none unless generated translations changed.

- [ ] **Step 1: Build the exact 15 focused targets**

```bash
cmake --build "$BUILD" --config Release --parallel 8 --target \
  SettingsTests SettingsPagesTests InputIndicatorSettingsTests \
  InputIndicatorPolicyTests InputIndicatorGeometryTests \
  CoreProcessInputIndicatorTests InputIndicatorIpcTests \
  OSXInputIndicatorRelayTests ServiceQuitTests TranslationFileTests \
  DeskflowStyleTests CanvasThemeTests IconsTests TrayIconTests \
  MainShellTests
```

- [ ] **Step 2: Run 15/15**

```bash
env QT_QPA_PLATFORM=offscreen \
  HOME="$HOME_DIR" XDG_CACHE_HOME="$CACHE_DIR" \
  XDG_CONFIG_HOME="$CONFIG_DIR" TMPDIR="$SAFE_TMP" \
  ctest --test-dir "$BUILD/src/unittests" \
  -R '^(SettingsTests|SettingsPagesTests|InputIndicatorSettingsTests|InputIndicatorPolicyTests|InputIndicatorGeometryTests|CoreProcessInputIndicatorTests|InputIndicatorIpcTests|OSXInputIndicatorRelayTests|ServiceQuitTests|TranslationFileTests|DeskflowStyleTests|CanvasThemeTests|IconsTests|TrayIconTests|MainShellTests)$' \
  --output-on-failure
```

- [ ] **Step 3: Build all and run split full gate**

Build all named CTest targets from `ctest -N -V`, then run:

```bash
ctest --test-dir "$BUILD/src/unittests" \
  -E '^(OSXClipboardTests|OSXKeyStateTests|InputIndicatorGeometryTests)$' \
  --output-on-failure
ctest --test-dir "$BUILD/src/unittests" \
  -R '^InputIndicatorGeometryTests$' \
  --output-on-failure
```

Use the same scratch variables for both. Exclude only the two established
OSX tests.

---

### Task 8: Restore generated files, review, and commit

**Files:**
- Stage: all files in the file map.
- Restore: non-Korean generated catalogues.

- [ ] **Step 1: Restore unrelated `.ts` files**

```bash
git restore --worktree -- \
  translations/deskflow_es.ts \
  translations/deskflow_it.ts \
  translations/deskflow_ja.ts \
  translations/deskflow_ru.ts \
  translations/deskflow_zh_CN.ts
```

- [ ] **Step 2: Review exact geometry evidence and diff**

Run:

```bash
git --no-pager diff --check
git --no-pager status --short
git --no-pager diff --stat
```

Verify Geometry output contains exact 7 pt support gaps, no fixed-mode delta,
zero outside/glyph/paint pixels, and the expected cap counts.

- [ ] **Step 3: Commit**

```bash
git add \
  src/lib/gui/input/InputIndicatorGeometry.h \
  src/lib/gui/input/InputIndicatorGeometry.cpp \
  src/lib/gui/input/MacInputIndicatorController.mm \
  src/lib/gui/input/MacInputIndicatorPanel.mm \
  src/lib/gui/settings/SettingsPages.cpp \
  src/unittests/gui/input/InputIndicatorGeometryTests.h \
  src/unittests/gui/input/InputIndicatorGeometryTests.mm \
  src/unittests/gui/input/InputIndicatorPolicyTests.h \
  src/unittests/gui/input/InputIndicatorPolicyTests.cpp \
  src/unittests/gui/settings/InputIndicatorSettingsTests.cpp \
  src/unittests/gui/settings/SettingsPagesTests.cpp \
  translations/deskflow_ko.ts
git commit \
  -m "feat(input): follow the cursor approach" \
  -m "Co-authored-by: Copilot App <223556219+Copilot@users.noreply.github.com>"
```

Expected: clean worktree and one implementation commit on the
`74150e1e16cb40eaa539331de344af34cac6bb0a` lineage. No push, release,
install, or PR.
