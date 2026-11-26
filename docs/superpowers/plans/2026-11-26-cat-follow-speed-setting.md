# Companion Follow Speed Setting Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a live 50..200% companion follow-speed setting and route Cat, Dog, and Turtle through one validated, velocity-bounded pursuit path without resetting the initialized position.

**Architecture:** The settings page writes one live integer key and the controller clamps it before passing it through `MacInputIndicatorPanelOptions` or a state-only panel setter. A species-neutral pure pursuit type/function owns the existing Cat timing thresholds and speed caps; the panel uses it for every companion, while species-specific paint remains for the follow-on parity revision.

**Tech Stack:** C++23, Objective-C++, Qt 6.12/QTest, AppKit, CMake/Ninja, macOS QSettings and `QSpinBox`.

---

## File map

- `src/lib/common/Settings.h` and `src/lib/common/Settings.cpp`
  declare, allow, and default `gui/inputIndicatorSpeedPercent`.
- `src/lib/gui/settings/SettingEffects.cpp` marks the key live.
- `src/lib/gui/settings/SettingsPages.cpp` builds and enables
  `sbInputIndicatorSpeed`.
- `translations/deskflow_ko.ts` translates the single new user-visible label.
- `src/lib/gui/input/InputIndicatorGeometry.h` and
  `src/lib/gui/input/InputIndicatorGeometry.cpp` define the shared pursuit
  state/step and multiplier-aware velocity cap.
- `src/lib/gui/input/MacInputIndicatorPanel.h` and
  `src/lib/gui/input/MacInputIndicatorPanel.mm` carry the option, expose the
  non-resetting setter, and use shared pursuit for Cat, Dog, and Turtle.
- `src/lib/gui/input/MacInputIndicatorController.h` and
  `src/lib/gui/input/MacInputIndicatorController.mm` clamp the stored value and
  forward live updates without re-showing the panel.
- `src/unittests/gui/settings/InputIndicatorSettingsTests.*`,
  `src/unittests/gui/settings/SettingsPagesTests.cpp`,
  `src/unittests/gui/input/InputIndicatorPolicyTests.*`, and
  `src/unittests/gui/input/InputIndicatorGeometryTests.*` provide the red/green
  setting, controller, pure, and native contracts.

No CMake target changes are needed. Existing targets already build all touched
files.

## Shared constants

Use these literals consistently:

```cpp
constexpr int kInputIndicatorSpeedMinimumPercent = 50;
constexpr int kInputIndicatorSpeedMaximumPercent = 200;
constexpr int kInputIndicatorSpeedDefaultPercent = 100;
```

The panel may keep the constants private if the settings page and controller
each use explicit 50/100/200 values with tests; do not introduce a settings/UI
dependency in the pure geometry layer.

## Environment

Create the isolated Release build and scratch directories once:

```bash
BUILD=/tmp/deskflow-cat-follow-speed-release
SCRATCH=/tmp/deskflow-cat-follow-speed-scratch
SAFE_TMP=/Users/subeomlee/.copilot/session-state/0e04e54a-1ee4-49a1-867f-877e8413aab6/files/cat-follow-speed-tmp
cmake -S . -B "$BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release
mkdir -p "$SCRATCH/home" "$SCRATCH/cache" \
  "$SCRATCH/config" "$SAFE_TMP"
```

Use the canonical session-owned `SAFE_TMP`, not `/tmp`, because the macOS
update tests reject `/tmp`'s symlinked path while the AWDL script test rejects
`/private/tmp`.

---

### Task 1: Add red settings and UI tests

**Files:**
- Modify: `src/unittests/gui/settings/InputIndicatorSettingsTests.cpp`
- Modify: `src/unittests/gui/settings/SettingsPagesTests.cpp`

- [ ] **Step 1: Add the speed key to cleanup and assert its default contract**

In both test cleanup paths, reset:

```cpp
Settings::setValue(Settings::Gui::InputIndicatorSpeedPercent);
```

In `defaultsAreSafeAndVisible()` and
`pages_inputIndicatorDefaultsAndDependencies()`, add:

```cpp
auto *speed = must<QSpinBox>(
    pages, "sbInputIndicatorSpeed"
);
QVERIFY(speed);
QCOMPARE(speed->minimum(), 50);
QCOMPARE(speed->maximum(), 200);
QCOMPARE(speed->singleStep(), 1);
QCOMPARE(speed->value(), 100);
QCOMPARE(speed->suffix(), QStringLiteral("%"));
QCOMPARE(
    Settings::defaultValue(
        Settings::Gui::InputIndicatorSpeedPercent
    ).toInt(),
    100
);
QVERIFY(Settings::validKeys().contains(
    Settings::Gui::InputIndicatorSpeedPercent
));
```

Add the key to the existing `effectOf(...) == Effect::Live` loop.

- [ ] **Step 2: Assert dependency behavior for all companions**

In each dependency test, before enabling:

```cpp
QVERIFY(!speed->isEnabled());
```

After enabling:

```cpp
QVERIFY(speed->isEnabled());
```

Select Dog, then Turtle, then Cat with the existing `choose` helper and assert
`speed->isEnabled()` remains true for all three. Confirm the stored value
round-trips:

```cpp
speed->setValue(175);
QCOMPARE(
    Settings::value(
        Settings::Gui::InputIndicatorSpeedPercent
    ).toInt(),
    175
);
speed->setValue(1);
QCOMPARE(speed->value(), 50);
speed->setValue(999);
QCOMPARE(speed->value(), 200);
```

- [ ] **Step 3: Assert a malformed stored number stays safe**

Set `Settings::Gui::InputIndicatorSpeedPercent` to
`QStringLiteral("not-a-number")`, construct `SettingsPages`, and assert:

```cpp
auto *speed = must<QSpinBox>(
    pages, "sbInputIndicatorSpeed"
);
QCOMPARE(speed->value(), 50);
QCOMPARE(
    Settings::value(
        Settings::Gui::InputIndicatorCompanion
    ).toString(),
    QStringLiteral("cat")
);
```

The UI's conversion-to-zero followed by the spin range is the nearest safe
bound for this malformed value. The controller fallback test in Task 5 proves
the independent nonnumeric runtime fallback is 100.

- [ ] **Step 4: Run the settings tests and verify the red state**

Run:

```bash
BUILD=/tmp/deskflow-cat-follow-speed-release
cmake --build "$BUILD" --config Release --parallel 8 \
  --target InputIndicatorSettingsTests SettingsPagesTests
```

Expected: FAIL to compile because
`Settings::Gui::InputIndicatorSpeedPercent` and
`sbInputIndicatorSpeed` do not exist.

---

### Task 2: Implement the setting, UI, effect, and Korean label

**Files:**
- Modify: `src/lib/common/Settings.h`
- Modify: `src/lib/common/Settings.cpp`
- Modify: `src/lib/gui/settings/SettingEffects.cpp`
- Modify: `src/lib/gui/settings/SettingsPages.cpp`
- Modify: `translations/deskflow_ko.ts`

- [ ] **Step 1: Declare, allow, and default the setting**

In `Settings::Gui` add:

```cpp
inline static const auto InputIndicatorSpeedPercent =
    QStringLiteral("gui/inputIndicatorSpeedPercent");
```

Add it next to the opacity key in `m_validKeys`.

In `Settings::defaultValue` add:

```cpp
if (key == Gui::InputIndicatorSpeedPercent)
  return 100;
```

- [ ] **Step 2: Mark it live**

In `SettingEffects.cpp`, next to the other input-indicator keys:

```cpp
live(S::Gui::InputIndicatorSpeedPercent);
```

- [ ] **Step 3: Add the `QSpinBox`**

Inside the existing macOS Input source indicator card, after companion
selection and before position:

```cpp
auto *indicatorSpeed = addNumber(
    page,
    m_binder,
    inputIndicator,
    Settings::Gui::InputIndicatorSpeedPercent,
    tx(QT_TRANSLATE_NOOP(
        "SettingsPages",
        "Companion follow speed"
    )),
    50,
    200
);
indicatorSpeed->setSuffix(QStringLiteral("%"));
indicatorSpeed->setObjectName(
    QStringLiteral("sbInputIndicatorSpeed")
);
```

Extend the existing refresh capture list and callback:

```cpp
m_binder->onRefresh(
    [indicatorEnabled, indicatorSpeed,
     indicatorCompanion, indicatorPosition,
     indicatorOpacity, indicatorTone] {
      const bool enabled =
          Settings::isWritable() &&
          indicatorEnabled->isChecked();
      indicatorCompanion->setEnabled(enabled);
      indicatorSpeed->setEnabled(enabled);
      indicatorPosition->setEnabled(enabled);
      indicatorOpacity->setEnabled(enabled);
      indicatorTone->setEnabled(enabled);
    }
);
```

The speed control intentionally ignores companion selection because it is
enabled for Cat, Dog, and Turtle.

- [ ] **Step 4: Add the Korean source and translation**

Add this message under the `SettingsPages` context in
`translations/deskflow_ko.ts` near `Badge opacity`:

```xml
<message>
    <source>Companion follow speed</source>
    <translation>동반자 따라가기 속도</translation>
</message>
```

Do not edit any other translation file. The final build may regenerate other
catalogues; restore those generated changes in Task 8.

- [ ] **Step 5: Run the settings tests green**

Run:

```bash
cmake --build "$BUILD" --config Release --parallel 8 \
  --target InputIndicatorSettingsTests SettingsPagesTests
HOME=/tmp/deskflow-cat-follow-speed-scratch/home \
XDG_CACHE_HOME=/tmp/deskflow-cat-follow-speed-scratch/cache \
XDG_CONFIG_HOME=/tmp/deskflow-cat-follow-speed-scratch/config \
TMPDIR=/Users/subeomlee/.copilot/session-state/0e04e54a-1ee4-49a1-867f-877e8413aab6/files/cat-follow-speed-tmp \
ctest --test-dir "$BUILD/src/unittests" \
  -R '^(InputIndicatorSettingsTests|SettingsPagesTests)$' \
  --output-on-failure
```

Expected: `2/2` pass.

---

### Task 3: Add red pure pursuit speed tests

**Files:**
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.mm`

- [ ] **Step 1: Declare the speed-cap test**

Add:

```cpp
void companionPursuitScalesCapsWithoutChangingGates();
```

- [ ] **Step 2: Add exact 8 ms cap cases**

Use one table for 50/100/200:

```cpp
struct SpeedCase
{
  double multiplier;
  double walkDistance;
  double runDistance;
};
const SpeedCase cases[] = {
    {.multiplier = 0.5, .walkDistance = 0.48,
     .runDistance = 2.88},
    {.multiplier = 1.0, .walkDistance = 0.96,
     .runDistance = 5.76},
    {.multiplier = 2.0, .walkDistance = 1.92,
     .runDistance = 11.52},
};
```

For Walk, use target `{.x = 20, .y = 0}`, target speed 0, and a state at the
origin. For Run, use target `{.x = 100, .y = 0}`, target speed 0. Both call:

```cpp
inputIndicatorCompanionPursuitAdvance(
    state,
    target,
    0,
    8,
    false,
    false,
    speedCase.multiplier
)
```

Assert:

```cpp
QVERIFY(step);
QVERIFY(std::abs(step->distanceMoved -
    speedCase.walkDistance) < 1e-9);
QCOMPARE(step->state.motionState,
    InputIndicatorMotionState::Walk);
```

Use the corresponding Run assertion for `{100, 0}`. For every case, assert the
new position is finite, advances monotonically, stays on the segment, and is
not past the target.

- [ ] **Step 3: Prove thresholds and Reduced Motion are unchanged**

Add these exact cases:

```cpp
// 47 pt + 259.999 target speed remains Walk.
// 47 pt + 260.0 target speed becomes Run.
// 48 pt + target speed 0 becomes Run.
// A current Run state remains Run at 13 pt and becomes Walk at 12 pt.
```

Call with multiplier 2.0 to prove thresholds do not scale.

Call Reduced Motion with multiplier 0.5 and a target far away; assert the
state moves directly to the target and reports `Idle` and `reached`.

- [ ] **Step 4: Prove invalid multipliers fail closed**

For each of:

```cpp
std::numeric_limits<double>::quiet_NaN(),
std::numeric_limits<double>::infinity(),
0.0,
0.499,
2.001
```

assert the companion pursuit function returns no step. Include a valid
`initialize = false`, target, state, elapsed 8, and Reduced Motion false.

- [ ] **Step 5: Run the pure test red**

Run:

```bash
cmake --build "$BUILD" --config Release --parallel 8 \
  --target InputIndicatorGeometryTests
```

Expected: FAIL to compile because
`inputIndicatorCompanionPursuitAdvance` does not exist.

---

### Task 4: Implement one multiplier-aware shared pursuit path

**Files:**
- Modify: `src/lib/gui/input/InputIndicatorGeometry.h`
- Modify: `src/lib/gui/input/InputIndicatorGeometry.cpp`

- [ ] **Step 1: Generalize the pursuit state and step names**

Rename the existing definitions:

```cpp
struct InputIndicatorCompanionPursuitState
{
  InputIndicatorPoint position;
  InputIndicatorPoint velocity;
  double headingRadians = 0;
  double stoppedMilliseconds = 0;
  InputIndicatorMotionState motionState =
      InputIndicatorMotionState::Idle;
  bool initialized = false;
};

struct InputIndicatorCompanionPursuitStep
{
  InputIndicatorCompanionPursuitState state;
  InputIndicatorPoint movement;
  InputIndicatorTargetKind targetKind =
      InputIndicatorTargetKind::Pointer;
  double distanceMoved = 0;
  double speedPointsPerSecond = 0;
  bool reached = false;
};
```

Keep source compatibility for the existing Cat tests:

```cpp
using InputIndicatorCatPursuitState =
    InputIndicatorCompanionPursuitState;
using InputIndicatorCatPursuitStep =
    InputIndicatorCompanionPursuitStep;
```

- [ ] **Step 2: Add the shared function and Cat wrapper**

Declare:

```cpp
std::optional<InputIndicatorCompanionPursuitStep>
inputIndicatorCompanionPursuitAdvance(
    const InputIndicatorCompanionPursuitState &state,
    const InputIndicatorTarget &target,
    double targetSpeedPointsPerSecond,
    double elapsedMilliseconds,
    bool initialize,
    bool reducedMotion,
    double speedMultiplier
);
```

Keep the current Cat declaration with its old six arguments and implement it
in the `.cpp` as a call to the shared function with `1.0`.

- [ ] **Step 3: Validate the multiplier and scale only speed**

Rename the current implementation, return the generalized step type, and add
this to its initial invalid-input condition:

```cpp
!std::isfinite(speedMultiplier) ||
speedMultiplier < 0.5 ||
speedMultiplier > 2.0
```

Keep direct initialize/Reduced Motion before cap integration.

Change only:

```cpp
const double speed =
    (run
        ? kCatRunSpeedPointsPerSecond
        : kCatWalkSpeedPointsPerSecond) *
    speedMultiplier;
```

Do not touch the Run predicate, heading turn, 8 ms cap, stop logic, or
overshoot branches.

- [ ] **Step 4: Run the pure and full geometry suite green**

Run:

```bash
cmake --build "$BUILD" --config Release --parallel 8 \
  --target InputIndicatorGeometryTests
env QT_QPA_PLATFORM=offscreen \
  HOME=/tmp/deskflow-cat-follow-speed-scratch/home \
  XDG_CACHE_HOME=/tmp/deskflow-cat-follow-speed-scratch/cache \
  XDG_CONFIG_HOME=/tmp/deskflow-cat-follow-speed-scratch/config \
  TMPDIR=/Users/subeomlee/.copilot/session-state/0e04e54a-1ee4-49a1-867f-877e8413aab6/files/cat-follow-speed-tmp \
  "$BUILD/src/unittests/gui/input/InputIndicatorGeometryTests"
```

Expected: `73+` QTest cases pass, including exact 0.48/0.96/1.92 and
2.88/5.76/11.52 results.

---

### Task 5: Add red controller live-update tests

**Files:**
- Modify: `src/unittests/gui/input/InputIndicatorPolicyTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorPolicyTests.cpp`

- [ ] **Step 1: Extend the harness without touching the real panel**

Add:

```cpp
std::vector<int> speedUpdates;
```

In `dependencies()`:

```cpp
result.panelSetFollowSpeed =
    [this](int percent) {
      speedUpdates.push_back(percent);
    };
```

- [ ] **Step 2: Add the live speed test**

Declare `persistentFollowSpeedUpdatesWithoutReconfigure`.

Test initial propagation:

```cpp
Settings::setValue(
    Settings::Gui::InputIndicatorSpeedPercent, 150
);
PersistentControllerHarness harness;
MacInputIndicatorController controller(
    nullptr, harness.dependencies()
);
MacInputIndicatorControllerTestAccess::arm(controller);
QCOMPARE(
    harness.presentations.back().followSpeedPercent,
    150
);
```

Record presentation/hide counts, write 200, then assert:

```cpp
QCOMPARE(harness.speedUpdates.back(), 200);
QCOMPARE(harness.presentations.size(), presentations);
QCOMPARE(harness.hides, hides);
```

Repeat with Dog and Turtle presentation selections through the existing
`setCompanion` access helper, proving every companion receives the same
value and no selection-specific re-enable branch exists.

- [ ] **Step 3: Add invalid fallback coverage**

For a fresh controller with the stored value
`QStringLiteral("not-a-number")`, arm it and assert the first option has
`followSpeedPercent == 100`.

For stored `25`, assert 50. For stored `250`, assert 200. Restore the setting
to its prior value with `QScopeGuard`.

- [ ] **Step 4: Run the controller test red**

Run:

```bash
cmake --build "$BUILD" --config Release --parallel 8 \
  --target InputIndicatorPolicyTests
```

Expected: FAIL to compile because panel options and dependencies do not yet
contain `followSpeedPercent` or the setter callback.

---

### Task 6: Wire controller, panel option, live setter, and all-companion pursuit

**Files:**
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.h`
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.mm`
- Modify: `src/lib/gui/input/MacInputIndicatorController.h`
- Modify: `src/lib/gui/input/MacInputIndicatorController.mm`

- [ ] **Step 1: Add the generic option and setter**

In `MacInputIndicatorPanelOptions`:

```cpp
int followSpeedPercent = 100;
```

Add public:

```cpp
void setFollowSpeedPercent(int percent);
```

In `Impl`, keep one multiplier:

```cpp
double followSpeedMultiplier = 1.0;
InputIndicatorCompanionPursuitState pursuitState;
```

Normalize through:

```cpp
static int clampedFollowSpeedPercent(int percent)
{
  return std::clamp(percent, 50, 200);
}
```

`show` sets:

```cpp
m_impl->followSpeedMultiplier =
    clampedFollowSpeedPercent(
        options.followSpeedPercent
    ) / 100.0;
```

`setFollowSpeedPercent` replaces only that multiplier. It must not touch
`pursuitState`, `follower`, `previousTarget`, source transition state, or panel
origin.

- [ ] **Step 2: Use shared pursuit for every companion**

Replace the current `if (m_impl->companion == Cat) ... else ...`
integration with one call:

```cpp
const auto pursuit =
    inputIndicatorCompanionPursuitAdvance(
        m_impl->pursuitState,
        {.point = *target, .kind = targetLocation.kind},
        pointerSpeed,
        elapsedMilliseconds,
        !m_impl->pursuitState.initialized,
        reducedMotion,
        m_impl->followSpeedMultiplier
    );
if (!pursuit) {
  hide();
  return {};
}
m_impl->pursuitState = pursuit->state;
m_impl->follower = pursuit->state.position;
m_impl->followerInitialized = true;
m_impl->motion =
    reducedMotion ? InputIndicatorPoint{} : pursuit->movement;
nextMotionState = pursuit->state.motionState;
```

Remove the legacy non-Cat `followInputIndicator`/`inputIndicatorMotionOffset`
movement branches. Do not remove Cat startle, idle-tail, rendering, or facing
logic.

- [ ] **Step 3: Preserve all initialized companion state**

In `show`, initialize or transfer `pursuitState` from `follower` only when
`pursuitState` is not initialized. Do not reset `followerInitialized` or
`previousTargetInitialized` merely because `companion != Cat`.

In the active-space notification, stop clearing `followerInitialized`,
`previousTargetInitialized`, `motion`, or `motionState` for Dog/Turtle. Keep
only `activeSpaceChanged`, scheduler timestamps, and the presentation
invalidated callback.

Source `show` for the same companion must preserve visible position exactly;
species switches continue to preserve the current follower coordinate while
updating paint identity.

- [ ] **Step 4: Clamp and route controller settings**

Add a controller dependency:

```cpp
std::function<void(int)> panelSetFollowSpeed;
```

Initialize it to a lambda that calls `m_panel->setFollowSpeedPercent`.

Add:

```cpp
int m_followSpeedPercent = 100;
```

Add a local helper:

```cpp
int inputIndicatorSpeedPercentFromSetting(
    const QVariant &value
)
{
  bool valid = false;
  const int parsed = value.toInt(&valid);
  return valid
      ? std::clamp(parsed, 50, 200)
      : 100;
}
```

Use it in `applySettings`.

Handle the speed key before the reconfigure key group:

```cpp
if (key ==
    Settings::Gui::InputIndicatorSpeedPercent) {
  m_followSpeedPercent =
      inputIndicatorSpeedPercentFromSetting(
          Settings::value(key)
      );
  if (m_panelSetFollowSpeed) {
    m_panelSetFollowSpeed(m_followSpeedPercent);
  }
  return;
}
```

Carry the value in the initial show:

```cpp
.followSpeedPercent = m_followSpeedPercent,
```

- [ ] **Step 5: Run controller and focused geometry tests green**

Run:

```bash
cmake --build "$BUILD" --config Release --parallel 8 \
  --target InputIndicatorPolicyTests InputIndicatorGeometryTests
env QT_QPA_PLATFORM=offscreen \
  HOME=/tmp/deskflow-cat-follow-speed-scratch/home \
  XDG_CACHE_HOME=/tmp/deskflow-cat-follow-speed-scratch/cache \
  XDG_CONFIG_HOME=/tmp/deskflow-cat-follow-speed-scratch/config \
  TMPDIR=/Users/subeomlee/.copilot/session-state/0e04e54a-1ee4-49a1-867f-877e8413aab6/files/cat-follow-speed-tmp \
  ctest --test-dir "$BUILD/src/unittests" \
  -R '^(InputIndicatorPolicyTests|InputIndicatorGeometryTests)$' \
  --output-on-failure
```

Expected: `2/2` pass.

---

### Task 7: Add native 50/100/200 panel coverage for all companions

**Files:**
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.mm`

- [ ] **Step 1: Add two native tests**

Add declarations:

```cpp
void companionPursuitPanelHonorsFollowSpeed();
void companionPursuitPanelSpeedChangePreservesPosition();
```

- [ ] **Step 2: Exercise three speeds for Cat, Dog, and Turtle**

Use fixed `fixedPanelTestPointer()` for every case. For each companion and
percent/cap table:

```cpp
const struct
{
  InputIndicatorCompanion companion;
  int percent;
  double movementCap;
} cases[] = {
  {InputIndicatorCompanion::Cat, 50, 2.88},
  ...
};
```

Include all nine combinations. Construct the panel with
`.followSpeedPercent = percent`, show it at the fixed point, move the target
700 pt away while remaining inside its screen, then call `move` every 8 ms.
Compute the visible anchor as:

```cpp
window.frame.origin + style.canvasOffset
```

Assert:

- every step is visible and uses frame interval 8 until settled;
- Euclidean movement is no greater than
  `movementCap + 0.5 * sqrt(2.0)`;
- current distance is monotonic within the quantization tolerance;
- at least 10 intermediate origins exist for 200%, and larger counts for
  100% and 50%;
- final position is within 0.5 pt per axis or the documented 1 pt AppKit
  fractional-target allowance.

At Cat, additionally retain the current 49 x 49 / outside-pixel assertions.

- [ ] **Step 3: Prove live setter position preservation**

For each companion:

1. start with 50%;
2. create a 700 pt jump;
3. advance once and record the visible origin;
4. call `panel.setFollowSpeedPercent(200)`;
5. record the origin again and assert exact equality;
6. advance at 8 ms and assert the next movement is bounded by
   `11.52 + 0.5 * sqrt(2.0)`;
7. assert the panel has not been hidden/reordered and the glyph/source remain
   unchanged.

- [ ] **Step 4: Run geometry and inspect pixel logs**

Run:

```bash
cmake --build "$BUILD" --config Release --parallel 8 \
  --target InputIndicatorGeometryTests
HOME=/tmp/deskflow-cat-follow-speed-scratch/home \
XDG_CACHE_HOME=/tmp/deskflow-cat-follow-speed-scratch/cache \
XDG_CONFIG_HOME=/tmp/deskflow-cat-follow-speed-scratch/config \
TMPDIR=/Users/subeomlee/.copilot/session-state/0e04e54a-1ee4-49a1-867f-877e8413aab6/files/cat-follow-speed-tmp \
"$BUILD/src/unittests/gui/input/InputIndicatorGeometryTests"
```

Expected: the existing Cat
`outsidePixels=0`, `paintOutside=0`, and `glyphOutside=0` logs remain, and
the new nine native cases all complete through intermediate positions.

---

### Task 8: Run focused and complete Release gates

**Files:**
- Modify: none unless generated translation files changed.

- [ ] **Step 1: Build the focused targets**

```bash
cmake --build "$BUILD" --config Release --parallel 8 --target \
  SettingsPagesTests InputIndicatorSettingsTests \
  InputIndicatorPolicyTests InputIndicatorGeometryTests \
  TranslationFileTests
```

- [ ] **Step 2: Run focused CTest**

```bash
env QT_QPA_PLATFORM=offscreen \
  HOME=/tmp/deskflow-cat-follow-speed-scratch/home \
  XDG_CACHE_HOME=/tmp/deskflow-cat-follow-speed-scratch/cache \
  XDG_CONFIG_HOME=/tmp/deskflow-cat-follow-speed-scratch/config \
  TMPDIR=/Users/subeomlee/.copilot/session-state/0e04e54a-1ee4-49a1-867f-877e8413aab6/files/cat-follow-speed-tmp \
  ctest --test-dir "$BUILD/src/unittests" \
  -R '^(SettingsPagesTests|InputIndicatorSettingsTests|InputIndicatorPolicyTests|InputIndicatorGeometryTests|TranslationFileTests)$' \
  --output-on-failure
```

Expected: `5/5`.

- [ ] **Step 3: Build all CTest targets**

```bash
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

- [ ] **Step 4: Run the safe 156-test gate**

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

Expected: `156/156`.

- [ ] **Step 5: Run isolated WindowServer geometry**

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

Expected: `1/1`, combined `157/157`; only
`OSXClipboardTests` and `OSXKeyStateTests` excluded.

---

### Task 9: Clean generated files and commit the implementation

**Files:**
- Restore generated: `translations/deskflow_es.ts`,
  `translations/deskflow_it.ts`, `translations/deskflow_ja.ts`,
  `translations/deskflow_ru.ts`, `translations/deskflow_zh_CN.ts`
- Stage only: all speed-setting source/test files plus
  `translations/deskflow_ko.ts`

- [ ] **Step 1: Restore unrelated generated catalogues**

```bash
git restore --worktree -- \
  translations/deskflow_es.ts \
  translations/deskflow_it.ts \
  translations/deskflow_ja.ts \
  translations/deskflow_ru.ts \
  translations/deskflow_zh_CN.ts
```

- [ ] **Step 2: Inspect and commit**

Run:

```bash
git --no-pager diff --check
git --no-pager status --short
git --no-pager diff --stat
```

Verify no files outside the mapped list changed. Then:

```bash
git add \
  src/lib/common/Settings.h \
  src/lib/common/Settings.cpp \
  src/lib/gui/settings/SettingEffects.cpp \
  src/lib/gui/settings/SettingsPages.cpp \
  src/lib/gui/input/InputIndicatorGeometry.h \
  src/lib/gui/input/InputIndicatorGeometry.cpp \
  src/lib/gui/input/MacInputIndicatorPanel.h \
  src/lib/gui/input/MacInputIndicatorPanel.mm \
  src/lib/gui/input/MacInputIndicatorController.h \
  src/lib/gui/input/MacInputIndicatorController.mm \
  src/unittests/gui/settings/InputIndicatorSettingsTests.h \
  src/unittests/gui/settings/InputIndicatorSettingsTests.cpp \
  src/unittests/gui/settings/SettingsPagesTests.cpp \
  src/unittests/gui/input/InputIndicatorPolicyTests.h \
  src/unittests/gui/input/InputIndicatorPolicyTests.cpp \
  src/unittests/gui/input/InputIndicatorGeometryTests.h \
  src/unittests/gui/input/InputIndicatorGeometryTests.mm \
  translations/deskflow_ko.ts
git commit \
  -m "feat(input): add companion follow speed" \
  -m "Co-authored-by: Copilot App <223556219+Copilot@users.noreply.github.com>"
```

Verify:

```bash
git --no-pager status --short
git --no-pager log --oneline --max-count=5
```

Expected: clean worktree and a new implementation commit descending from the
speed spec/plan commits.
