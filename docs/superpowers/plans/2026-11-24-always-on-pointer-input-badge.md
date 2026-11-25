# Always-On Pointer Input Badge Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the transient activity-driven input indicator with an always-on `A`/`한` badge that follows the native mouse pointer while the pointer is local.

**Architecture:** Keep the privacy-safe `NSPanel`, exact TIS snapshot, and core local/off-computer subscription. Add Cat/Dog/Turtle vector drawing, event-driven local/global pointer targeting with a short-lived 8 ms render timer and 16 ms fallback, and leave legacy IPC/activity/AX classes buildable but disconnected.

**Tech Stack:** C++20, Objective-C++, Qt 6.12 Core/Widgets/Test, AppKit `NSPanel`/`NSEvent`, Carbon TIS, CMake/Ninja, CTest, CPack DragNDrop.

---

## File Map

**Controller and policy**

- Modify `src/lib/gui/input/MacInputIndicatorController.h`
- Modify `src/lib/gui/input/MacInputIndicatorController.mm`
- Modify `src/lib/gui/input/InputIndicatorPolicy.h`
- Modify `src/lib/gui/input/InputIndicatorPolicy.cpp`

These files decide whether a persistent badge is logically visible and separate
TIS/source configuration from pointer-frame updates.

**Panel**

- Modify `src/lib/gui/input/MacInputIndicatorPanel.h`
- Modify `src/lib/gui/input/MacInputIndicatorPanel.mm`

The panel owns AppKit presentation and exposes independent `show(options)` and
`move(position)` operations.

**Core and platform integration**

- Modify `src/lib/gui/MainWindow.cpp`
- Modify `src/lib/platform/OSXComputer.h`
- Modify `src/lib/platform/OSXComputer.mm`

Only local/off-computer state stays connected from core to the controller.
Click/key activity relay wiring is removed without changing the IPC payloads.

**Settings**

- Modify `src/lib/gui/settings/SettingsPages.cpp`
- Modify `src/unittests/gui/settings/InputIndicatorSettingsTests.cpp`
- Modify `src/unittests/gui/settings/SettingsPagesTests.cpp`

The display-time control is removed; its setting key remains registered.

**Tests**

- Modify `src/unittests/gui/input/InputIndicatorPolicyTests.h`
- Modify `src/unittests/gui/input/InputIndicatorPolicyTests.cpp`
- Reuse `src/unittests/gui/input/InputIndicatorGeometryTests.cpp`
- Reuse `src/unittests/platform/OSXInputSourceTests.cpp`
- Reuse `src/unittests/platform/OSXInputIndicatorRelayTests.cpp`
- Reuse `src/unittests/gui/core/CoreProcessInputIndicatorTests.cpp`

No new CMake target is needed because the existing tests already cover these
boundaries.

---

### Task 1: Record the Persistent Controller RED

**Files:**
- Modify: `src/unittests/gui/input/InputIndicatorPolicyTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorPolicyTests.cpp`

- [ ] **Step 1: Replace transient macOS controller declarations with persistent
  contract declarations**

Remove the old `focusChanged`, probe, secure-focus, and stale-probe slots from
the macOS controller portion of the header. Add these slots:

```cpp
#ifdef Q_OS_MACOS
  void persistentKnownSourceShowsWithoutConnectivityOrAx();
  void persistentOffComputerHides();
  void persistentLocalAndDisconnectedTransitionsShow();
  void persistentUnknownSourceHidesWithoutStaleLabel();
  void persistentPointerTicksMoveWithoutReconfigure();
  void persistentDuplicateSourceDoesNotReconfigure();
  void persistentSourceChangesUpdateLabel();
  void persistentFeatureOffStopsAndHides();
  void persistentGeometryOrPanelFailureHides();
  void persistentActivityDoesNotGateVisibility();
  void panelPresentationNeverRequestsCursorMutation();
#endif
```

- [ ] **Step 2: Add a RED test using the existing dependency seam**

In the test access struct, keep direct access to the existing private
`inputIndicatorLocalChanged`, `inputSourceChanged`, `setEffectiveEnabled`, and
timer methods. Add this test before changing production:

```cpp
void InputIndicatorPolicyTests::persistentKnownSourceShowsWithoutConnectivityOrAx()
{
  int shown = 0;
  MacInputIndicatorController::Dependencies dependencies;
  dependencies.accessibilityTrusted = [] { return false; };
  dependencies.inputSource = [] {
    return OSXInputSourceSnapshot{
        .identifier = "com.apple.keylayout.ABC",
        .source = Source::LatinABC
    };
  };
  dependencies.panelShow =
      [&shown](const MacInputIndicatorPanelOptions &) {
        ++shown;
        return true;
      };
  dependencies.panelHide = [] {};
  MacInputIndicatorController controller(nullptr, std::move(dependencies));

  MacInputIndicatorControllerTestAccess::arm(
      controller, Source::LatinABC
  );
  controller.inputIndicatorStateInvalidated();

  QCOMPARE(shown, 1);
}
```

- [ ] **Step 3: Build and run the focused test to capture RED**

Run:

```bash
cmake --build build --config Release --parallel 8 \
  --target InputIndicatorPolicyTests
env QT_QPA_PLATFORM=offscreen \
  HOME="$PWD/build/scratch/home" \
  XDG_CACHE_HOME="$PWD/build/scratch/cache" \
  XDG_CONFIG_HOME="$PWD/build/scratch/config" \
  TMPDIR="$PWD/build/scratch/tmp" \
  ctest --test-dir build/src/unittests \
    -R '^InputIndicatorPolicyTests$' --output-on-failure
```

Expected: FAIL because the current controller treats disconnected state and
missing Accessibility trust as hide reasons, so `shown == 0`.

- [ ] **Step 4: Add the remaining desired-controller tests**

Add a panel test harness that captures configured options, move calls, and hide
calls. Its `panelMove` lambda initially references the not-yet-existing
`Dependencies::panelMove` seam:

```cpp
struct PersistentControllerHarness
{
  Source source = Source::LatinABC;
  std::string identifier = "com.apple.keylayout.ABC";
  std::vector<MacInputIndicatorPanelOptions> presentations;
  int moves = 0;
  int hides = 0;
  bool moveResult = true;
  bool showResult = true;

  MacInputIndicatorController::Dependencies dependencies()
  {
    MacInputIndicatorController::Dependencies result;
    result.inputSource = [this] {
      return OSXInputSourceSnapshot{
          .identifier = identifier, .source = source
      };
    };
    result.panelShow = [this](const MacInputIndicatorPanelOptions &options) {
      presentations.push_back(options);
      return showResult;
    };
    result.panelMove = [this](InputIndicatorPosition) {
      ++moves;
      return moveResult;
    };
    result.panelHide = [this] { ++hides; };
    return result;
  }
};
```

The remaining tests assert:

```cpp
controller.inputIndicatorLocalChanged(false); // one hide, timer stopped
controller.inputIndicatorLocalChanged(true);  // one presentation
controller.inputIndicatorStateInvalidated();  // local assumed, visible
MacInputIndicatorControllerTestAccess::pointerTick(controller);
QCOMPARE(harness.moves, 1);
QCOMPARE(harness.presentations.size(), size_t{1});
```

For source changes, set `identifier` and `source` before invoking
`inputSourceChanged()`. Assert duplicate exact snapshots leave presentation
count unchanged and ABC/Korean transitions capture the new `options.source`.

- [ ] **Step 5: Re-run the RED test and preserve the expected failure**

Run the command from Step 3. Expected: compile failure at
`Dependencies::panelMove` or behavioral failures for the still-transient
controller. Do not edit production in this task.

---

### Task 2: Add the Persistent Visibility Policy

**Files:**
- Modify: `src/lib/gui/input/InputIndicatorPolicy.h`
- Modify: `src/lib/gui/input/InputIndicatorPolicy.cpp`
- Modify: `src/unittests/gui/input/InputIndicatorPolicyTests.cpp`

- [ ] **Step 1: Add a focused policy test**

Add a data-driven test around the new public type:

```cpp
struct PersistentInputIndicatorPolicyInput
{
  bool enabled = false;
  bool isOnComputer = true;
  InputIndicatorSource source = InputIndicatorSource::Unknown;
};
```

Test these rows:

```text
disabled, known, local -> hidden/Disabled
enabled, unknown, local -> hidden/SourceUnknown
enabled, known, off-computer -> hidden/OffComputer
enabled, known, local -> shown with the exact source
```

Disconnected state is intentionally absent: the persistent policy has no
connectivity or Accessibility field.

- [ ] **Step 2: Run the test and capture RED**

Run the `InputIndicatorPolicyTests` command from Task 1. Expected: compile
failure because `PersistentInputIndicatorPolicyInput` and
`decidePersistent()` do not exist.

- [ ] **Step 3: Implement the minimal policy**

Add:

```cpp
class InputIndicatorPolicy
{
public:
  static InputIndicatorVisibilityDecision
  decidePersistent(const PersistentInputIndicatorPolicyInput &input);
  // Existing transient decision remains for disconnected legacy coverage.
  static InputIndicatorVisibilityDecision
  decide(const InputIndicatorPolicyInput &input, std::chrono::milliseconds dwell);
};
```

Implement in `.cpp`:

```cpp
if (!input.enabled)
  return {.reason = InputIndicatorVisibilityReason::Disabled};
if (!input.isOnComputer)
  return {.reason = InputIndicatorVisibilityReason::OffComputer};
if (input.source == InputIndicatorSource::Unknown)
  return {.reason = InputIndicatorVisibilityReason::SourceUnknown};
return {
    .show = true,
    .source = input.source,
    .reason = InputIndicatorVisibilityReason::Shown,
};
```

- [ ] **Step 4: Rebuild and run the policy test**

Run the Task 1 command. Expected: new policy test passes; controller tests still
fail until Task 3.

---

### Task 3: Rework the Controller State Machine

**Files:**
- Modify: `src/lib/gui/input/MacInputIndicatorController.h`
- Modify: `src/lib/gui/input/MacInputIndicatorController.mm`
- Modify: `src/unittests/gui/input/InputIndicatorPolicyTests.cpp`

- [ ] **Step 1: Replace controller dependencies and state**

Remove controller ownership and construction of `MacAccessibilityProbe` and
`MacInputFocusMonitor`. Remove frontmost PID, accessibility trust, probe,
generation, activity, dwell, pending trigger/source/PID, and visible trigger
members.

Keep this dependency boundary:

```cpp
struct Dependencies
{
  std::function<OSXInputSourceSnapshot()> inputSource;
  std::function<bool(const MacInputIndicatorPanelOptions &)> panelShow;
  std::function<bool(InputIndicatorPosition)> panelMove;
  std::function<void()> panelHide;
  std::function<void(InputIndicatorVisibilityReason)> diagnosticEmitter;
};
```

Keep `inputIndicatorActivityReceived` and invalidation slots as compatibility
slots, but they contain no visibility logic. Keep `inputIndicatorLocalChanged`
and `inputIndicatorStateInvalidated` as the routing boundary.

- [ ] **Step 2: Add the pointer timer and exact snapshot**

Use:

```cpp
static constexpr int kPointerFrameIntervalMs = 16;
OSXInputSourceSnapshot m_sourceSnapshot;
QTimer m_pointerTimer;
bool m_isOnComputer = true;
bool m_panelConfigured = false;
```

Set `Qt::PreciseTimer` and start only while persistent policy says show.

- [ ] **Step 3: Implement direct reconciliation**

Use the persistent policy in a helper:

```cpp
void MacInputIndicatorController::refreshVisibility(
    bool reconfigure
)
{
  const auto decision = InputIndicatorPolicy::decidePersistent({
      .enabled = m_effectiveEnabled,
      .isOnComputer = m_isOnComputer,
      .source = m_sourceSnapshot.source,
  });
  if (!decision.show) {
    m_pointerTimer.stop();
    hide();
    reportHiddenReason(decision.reason);
    return;
  }
  if (!m_pointerTimer.isActive())
    m_pointerTimer.start(kPointerFrameIntervalMs);
  if (reconfigure || !m_panelConfigured) {
    const bool presented = m_panelShow({...options...});
    m_panelConfigured = true;
    if (!presented) {
      hide();
      reportHiddenReason(InputIndicatorVisibilityReason::PanelUnavailable);
      return;
    }
    return;
  }
  movePointerBadge();
}
```

The label/style have been configured even when geometry or native presentation
temporarily fails, so the next pointer timer tick uses `move` rather than
reconfiguring the source. The retry never changes native cursor state.

- [ ] **Step 4: Implement exact source transitions**

Compare the complete `identifier` and `source`:

```cpp
const auto current = m_inputSource();
if (current.identifier == m_sourceSnapshot.identifier &&
    current.source == m_sourceSnapshot.source) {
  return;
}
m_sourceSnapshot = std::move(current);
refreshVisibility(true);
```

This guarantees duplicate TIS notifications do no panel presentation.

- [ ] **Step 5: Implement local and invalidated routing**

```cpp
void inputIndicatorLocalChanged(bool local)
{
  m_isOnComputer = local;
  refreshVisibility(false);
}

void inputIndicatorStateInvalidated()
{
  m_isOnComputer = true;
  refreshVisibility(false);
}
```

`inputIndicatorActivityReceived(...)` and
`inputIndicatorActivityInvalidated()` intentionally do no work.

- [ ] **Step 6: Update test access and run GREEN**

Expose only the private operations used by the existing test-friend pattern:
`setEffectiveEnabled`, `inputSourceChanged`, `pointerTick`, and timer-active
query. Remove the now-obsolete `dependencies.accessibilityTrusted` assignment
from the first RED test because the production dependency boundary no longer
contains Accessibility. Run the focused test command. Expected: all new
persistent controller tests pass.

---

### Task 4: Split Panel Presentation from Pointer Movement

**Files:**
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.h`
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.mm`
- Modify: `src/unittests/gui/input/InputIndicatorPolicyTests.cpp`

- [ ] **Step 1: Add the panel move contract**

Add to `MacInputIndicatorPanel`:

```cpp
bool show(const MacInputIndicatorPanelOptions &options);
bool move(InputIndicatorPosition position);
void hide();
```

- [ ] **Step 2: Move geometry and presentation into `move`**

In `show`, validate and assign the label, tone, and opacity, mark the view for
display, and call `move(options.position)`. In `move`, preserve the existing
geometry:

```cpp
const auto cursor = NSEvent.mouseLocation;
const auto location = positionInputIndicator(
    {.x = cursor.x, .y = cursor.y},
    {.width = kPanelWidth, .height = kPanelHeight},
    screenRects(),
    position,
    kCursorGap,
    kScreenMargin
);
if (!location) {
  hide();
  return false;
}
[m_impl->panel setFrameOrigin:NSMakePoint(location->x, location->y)];
if (![m_impl->panel isVisible]) {
  requestMacInputIndicatorPanelPresentation(...);
}
return static_cast<bool>([m_impl->panel isVisible]);
```

Do not call `setNeedsDisplay` from `move`, and do not call any native cursor
API.

- [ ] **Step 3: Add a pure panel command assertion**

Keep `requestMacInputIndicatorPanelPresentation()` as the AppKit command seam.
Assert its dispatch vector is exactly:

```cpp
{MacInputIndicatorPanelOrderCommand::FrontRegardless}
```

Name the test `panelPresentationNeverRequestsCursorMutation`; the command enum
contains no cursor action and the panel code contains no `NSCursor`,
`CGDisplayHideCursor`, `CGDisplayShowCursor`, or cursor warp call.

- [ ] **Step 4: Run panel and controller tests**

Run:

```bash
cmake --build build --config Release --parallel 8 \
  --target InputIndicatorPolicyTests InputIndicatorGeometryTests
env QT_QPA_PLATFORM=offscreen \
  HOME="$PWD/build/scratch/home" \
  XDG_CACHE_HOME="$PWD/build/scratch/cache" \
  XDG_CONFIG_HOME="$PWD/build/scratch/config" \
  TMPDIR="$PWD/build/scratch/tmp" \
  ctest --test-dir build/src/unittests \
    -R '^(InputIndicatorPolicyTests|InputIndicatorGeometryTests)$' \
    --output-on-failure
```

Expected: 2/2 tests pass. `pointerTicksMoveWithoutReconfigure` proves one timer
tick increments only `moves`, not `presentations`.

---

### Task 5: Disconnect Click, Key, and Accessibility Work

**Files:**
- Modify: `src/lib/gui/MainWindow.cpp`
- Modify: `src/lib/platform/OSXComputer.h`
- Modify: `src/lib/platform/OSXComputer.mm`
- Leave buildable: `src/lib/platform/OSXInputIndicatorRelay.*`
- Leave buildable: `src/lib/gui/input/MacAccessibilityProbe.*`
- Leave buildable: `src/lib/gui/input/MacInputFocusMonitor.*`

- [ ] **Step 1: Remove GUI activity connections**

In `MainWindow.cpp`, delete the connections from
`CoreProcess::inputIndicatorActivityReceived` and
`inputIndicatorActivityInvalidated` to the controller. Keep enabled, local, and
state-invalidated connections.

- [ ] **Step 2: Remove relay creation and event-tap notifications**

In `OSXComputer.mm`, remove the `m_inputIndicatorRelay` construction in the
constructor, its `invalidate()` calls, and the click/key branch at the start of
`handleCGInputEvent`. Remove the member from `OSXComputer.h` and its now-unused
include/forward declaration.

Keep the `inputIndicatorLocalState` send/update code:

```cpp
deskflow::input::setInputIndicatorLocal(local);
if (deskflow::input::isInputIndicatorSubscribed()) {
  ipcSendInputIndicatorToClient(
      QLatin1String(deskflow::input::kInputIndicatorLocalCommand),
      deskflow::input::encodeInputIndicatorBoolean(local)
  );
}
```

- [ ] **Step 3: Run relay and core compatibility tests**

Run:

```bash
cmake --build build --config Release --parallel 8 \
  --target OSXInputIndicatorRelayTests CoreProcessInputIndicatorTests
env QT_QPA_PLATFORM=offscreen \
  HOME="$PWD/build/scratch/home" \
  XDG_CACHE_HOME="$PWD/build/scratch/cache" \
  XDG_CONFIG_HOME="$PWD/build/scratch/config" \
  TMPDIR="$PWD/build/scratch/tmp" \
  ctest --test-dir build/src/unittests \
    -R '^(OSXInputIndicatorRelayTests|CoreProcessInputIndicatorTests)$' \
    --output-on-failure
```

Expected: 2/2 pass. The IPC classes remain valid; they are simply no longer
used by the indicator controller/event path.

---

### Task 6: Remove Display Time from Settings

**Files:**
- Modify: `src/lib/gui/settings/SettingsPages.cpp`
- Modify: `src/unittests/gui/settings/InputIndicatorSettingsTests.cpp`
- Modify: `src/unittests/gui/settings/SettingsPagesTests.cpp`

- [ ] **Step 1: Write failing settings assertions**

Update both settings tests to assert:

```cpp
QVERIFY(!pages.entries().front()->page->findChild<QSpinBox *>(
    QStringLiteral("sbInputIndicatorDwell")
));
QCOMPARE(
    Settings::value(Settings::Gui::InputIndicatorDwellMs).toInt(), 1500
);
QCOMPARE(
    effectOf(Settings::Gui::InputIndicatorDwellMs),
    std::optional<Effect>(Effect::Live)
);
```

Keep enabled, position, opacity, and tone dependency assertions. Update the
privacy note assertions to require `mouse pointer`, `input-source`, and
`Accessibility`-free wording such as `does not read focus or text`, and reject
the old `Password` and `after ... activity` wording.

- [ ] **Step 2: Run settings tests to verify RED**

Run:

```bash
cmake --build build --config Release --parallel 8 \
  --target InputIndicatorSettingsTests SettingsPagesTests
env QT_QPA_PLATFORM=offscreen \
  HOME="$PWD/build/scratch/home" \
  XDG_CACHE_HOME="$PWD/build/scratch/cache" \
  XDG_CONFIG_HOME="$PWD/build/scratch/config" \
  TMPDIR="$PWD/build/scratch/tmp" \
  ctest --test-dir build/src/unittests \
    -R '^(InputIndicatorSettingsTests|SettingsPagesTests)$' \
    --output-on-failure
```

Expected: FAIL because `sbInputIndicatorDwell` still exists.

- [ ] **Step 3: Remove the control and update copy**

Delete the `addNumber(...InputIndicatorDwellMs...)` row and remove it from
the refresh lambda. Preserve `Settings::Gui::InputIndicatorDwellMs` in
`Settings.h`, `Settings.cpp`, `SettingEffects.cpp`, and settings key lists.

Set:

```cpp
"한/A always beside the mouse pointer while the pointer is local"
```

for the switch detail and a privacy note:

```cpp
"Shows the current ABC or 2-Set Korean input source beside the local mouse "
"pointer. It reads only the input-source identifier and pointer position; "
"it does not read Accessibility focus or text, keys, caret contents, "
"clipboard contents, titles, URLs, selections, values, or screenshots."
```

- [ ] **Step 4: Run settings tests to verify GREEN**

Run the Step 2 command. Expected: 2/2 pass.

---

### Task 7: Run the Focused Integration Gate

**Files:**
- Modify: none unless a gate finds a change-caused failure.

- [ ] **Step 1: Build the focused targets**

```bash
cmake --build build --config Release --parallel 8 --target \
  InputIndicatorPolicyTests \
  InputIndicatorGeometryTests \
  InputIndicatorSettingsTests \
  SettingsPagesTests \
  OSXInputSourceTests \
  OSXInputIndicatorRelayTests \
  CoreProcessInputIndicatorTests
```

- [ ] **Step 2: Run focused CTest**

```bash
env QT_QPA_PLATFORM=offscreen \
  HOME="$PWD/build/scratch/home" \
  XDG_CACHE_HOME="$PWD/build/scratch/cache" \
  XDG_CONFIG_HOME="$PWD/build/scratch/config" \
  TMPDIR="$PWD/build/scratch/tmp" \
  ctest --test-dir build/src/unittests \
    -R '^(InputIndicatorPolicyTests|InputIndicatorGeometryTests|InputIndicatorSettingsTests|SettingsPagesTests|OSXInputSourceTests|OSXInputIndicatorRelayTests|CoreProcessInputIndicatorTests)$' \
    --output-on-failure
```

Expected: 7/7 pass.

- [ ] **Step 3: Commit the implementation**

```bash
git add \
  src/lib/gui/MainWindow.cpp \
  src/lib/gui/input/InputIndicatorPolicy.cpp \
  src/lib/gui/input/InputIndicatorPolicy.h \
  src/lib/gui/input/MacInputIndicatorController.h \
  src/lib/gui/input/MacInputIndicatorController.mm \
  src/lib/gui/input/MacInputIndicatorPanel.h \
  src/lib/gui/input/MacInputIndicatorPanel.mm \
  src/lib/gui/settings/SettingsPages.cpp \
  src/lib/platform/OSXComputer.h \
  src/lib/platform/OSXComputer.mm \
  src/unittests/gui/input/InputIndicatorPolicyTests.cpp \
  src/unittests/gui/input/InputIndicatorPolicyTests.h \
  src/unittests/gui/settings/InputIndicatorSettingsTests.cpp \
  src/unittests/gui/settings/SettingsPagesTests.cpp
git commit -m "feat(input): keep language badge on pointer" \
  -m "Co-authored-by: Copilot App <223556219+Copilot@users.noreply.github.com>"
```

Do not include generated translation updates from the build.

---

### Task 8: Run the Full Release Gate

**Files:**
- Modify: none unless a failure is caused by this implementation.

- [ ] **Step 1: Enumerate all registered tests**

```bash
ctest --test-dir build/src/unittests -N -V > build/pointer-badge-ctest-list.txt
sed -nE 's/^ +Test +#[0-9]+: //p' \
  build/pointer-badge-ctest-list.txt |
  grep -v '^GitVersionTests$' |
  sort -u > build/pointer-badge-test-targets.txt
```

The target count must be recorded from `wc -l`.

- [ ] **Step 2: Build every executable test target**

```bash
TEST_TARGETS="$(tr '\n' ' ' < build/pointer-badge-test-targets.txt)"
cmake --build build --config Release --parallel 8 \
  --target $TEST_TARGETS
```

- [ ] **Step 3: Run the full CTest gate**

```bash
env QT_QPA_PLATFORM=offscreen \
  HOME="$PWD/build/scratch/home" \
  XDG_CACHE_HOME="$PWD/build/scratch/cache" \
  XDG_CONFIG_HOME="$PWD/build/scratch/config" \
  TMPDIR="$PWD/build/scratch/tmp" \
  ctest --test-dir build/src/unittests \
    -E '^(OSXClipboardTests|OSXKeyStateTests)$' \
    --output-on-failure
```

Expected: every eligible registered test passes and the output proves only the
two named tests were excluded.

- [ ] **Step 4: Restore build-generated translations and verify the commit**

```bash
git restore --worktree -- \
  translations/deskflow_es.ts \
  translations/deskflow_it.ts \
  translations/deskflow_ja.ts \
  translations/deskflow_ru.ts \
  translations/deskflow_zh_CN.ts
git diff --check
git status --short --branch
```

Expected: clean branch at the implementation commit.

---

### Task 9: Package and Verify the Final 2.0.9 Candidate

**Files:**
- Create ignored build metadata under `build/pointer-badge-package`
- Output: `build/pointer-badge/deskflow-2.0.9.0-macos-arm64.dmg`

- [ ] **Step 1: Reconfirm package metadata without a tag**

Use the ignored `build/package-version-2.0.9.0.cmake` project-include already
used to override generated package metadata, not a git tag. Configure:

```bash
cmake -S . -B build/pointer-badge-package -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/opt/homebrew \
  -DCMAKE_COMPILE_WARNING_AS_ERROR=ON \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14 \
  -DBUILD_TESTS=OFF \
  -DSKIP_BUILD_TESTS=ON \
  -DBUILD_INSTALLER=ON \
  -DBUILD_DEV_DOCS=OFF \
  -DPACKAGE_VERSION_LABEL=2.0.9.0 \
  -DCMAKE_PROJECT_INCLUDE="$PWD/build/package-version-2.0.9.0.cmake"
```

Expected log line: `Building deskflow-2.0.9.0` and package basename
`deskflow-2.0.9.0-macos-arm64`.

- [ ] **Step 2: Build the package**

```bash
cmake --build build/pointer-badge-package \
  --config Release --parallel 8 --target package
```

Do not pass `APPLE_CODESIGN_DEV` unless `security find-identity` already shows
an existing valid identity. Without one, the package remains ad-hoc signed.

- [ ] **Step 3: Verify and record the candidate**

```bash
mkdir -p build/pointer-badge
cp -p \
  build/pointer-badge-package/deskflow-2.0.9.0-macos-arm64.dmg \
  build/pointer-badge/deskflow-2.0.9.0-macos-arm64.dmg
shasum -a 256 \
  build/pointer-badge/deskflow-2.0.9.0-macos-arm64.dmg
hdiutil verify \
  build/pointer-badge/deskflow-2.0.9.0-macos-arm64.dmg
```

Mount read-only under `build/pointer-badge/mount`, assert both
`CFBundleShortVersionString` and `CFBundleVersion` equal `2.0.9.0`, run
`codesign --verify --deep --strict`, record `codesign -dv --verbose=4`, detach,
and report hash, architecture, signature type, TeamIdentifier, CDHash, and
any `macdeployqt` dependency-resolution warnings.

- [ ] **Step 4: Verify final repository state**

```bash
git status --short --branch
git log -3 --format='%H %P %s'
git tag --points-at HEAD
```

Expected: clean branch, ancestry
`8bd772e8 -> spec commit -> plan commit -> implementation commit`, no HEAD tag,
and no push, merge, release, PR, install, or `/Applications` change.

---

## Revision 2: Compact Trailing Badge and Active-Space Presentation

### Task 10: Record Compact Geometry and Motion RED

**Files:**
- Modify: `src/lib/gui/input/InputIndicatorGeometry.h`
- Modify: `src/lib/gui/input/InputIndicatorGeometry.cpp`
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.cpp`
- Modify: `src/unittests/gui/settings/InputIndicatorSettingsTests.cpp`
- Modify: `src/unittests/gui/settings/SettingsPagesTests.cpp`

- [ ] **Step 1: Add compact style expectations**

Add these test slots:

```cpp
void compactPanelStyleUsesApprovedMetrics();
void followerCatchesUpWithoutOvershoot();
void followerCapsTrailingLag();
void followerRejectsInvalidInput();
```

Assert the production style helper:

```cpp
const auto style = macInputIndicatorPanelStyle();
QCOMPARE(style.width, 24.0);
QCOMPARE(style.height, 20.0);
QCOMPARE(style.cursorGap, 6.0);
QCOMPARE(style.screenMargin, 6.0);
QCOMPARE(style.labelSize, 11.0);
QCOMPARE(style.cornerRadius, 7.0);
QCOMPARE(style.opacityPercent, 65);
```

Change settings expectations from 85 to 65 while preserving the 60-100
control range.

- [ ] **Step 2: Add follower expectations**

```cpp
const auto first = followInputIndicator(
    {.x = 0, .y = 0}, {.x = 10, .y = 0}, 0.5, 18
);
QVERIFY(first);
QCOMPARE(first->x, 5.0);
QCOMPARE(first->y, 0.0);

auto current = *first;
current = *followInputIndicator(current, {10, 0}, 0.5, 18);
QCOMPARE(current.x, 7.5);
current = *followInputIndicator(current, {10, 0}, 0.5, 18);
QCOMPARE(current.x, 8.75);
QVERIFY(current.x <= 10.0);
```

For a far target, assert:

```cpp
const auto bounded =
    followInputIndicator({0, 0}, {100, 0}, 0.5, 18);
QCOMPARE(bounded->x, 82.0);
```

Add invalid rows for NaN coordinates, follow factor outside 0-1, negative
maximum lag, and zero catch-up factor with a target farther than the cap.

- [ ] **Step 4: Capture RED**

```bash
cmake --build build --config Release --parallel 8 \
  --target InputIndicatorGeometryTests InputIndicatorSettingsTests SettingsPagesTests
env QT_QPA_PLATFORM=offscreen \
  HOME="$PWD/build/scratch/home" \
  XDG_CACHE_HOME="$PWD/build/scratch/cache" \
  XDG_CONFIG_HOME="$PWD/build/scratch/config" \
  TMPDIR="$PWD/build/scratch/tmp" \
  ctest --test-dir build/src/unittests \
    -R '^(InputIndicatorGeometryTests|InputIndicatorSettingsTests|SettingsPagesTests)$' \
    --output-on-failure
```

Expected: compile failure for the missing style/follower helpers and value
failures for the old 85% default.

### Task 11: Record Active-Space Presentation RED

**Files:**
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.h`
- Modify: `src/unittests/gui/input/InputIndicatorPolicyTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorPolicyTests.cpp`

- [ ] **Step 1: Add the presentation-state test**

Add:

```cpp
void panelPresentationNeededAfterActiveSpaceChange();
```

Assert a pure helper:

```cpp
QVERIFY(!macInputIndicatorPresentationNeeded(false, true));
QVERIFY(macInputIndicatorPresentationNeeded(true, true));
QVERIFY(macInputIndicatorPresentationNeeded(false, false));
QVERIFY(macInputIndicatorPresentationNeeded(true, false));
```

The helper's first argument is an active-Space invalidation and its second is
actual `NSPanel.isVisible`. The result is true when the panel should call
`orderFrontRegardless`.

- [ ] **Step 2: Extend pointer movement assertions**

In the controller harness, verify a Space-invalidated panel move still uses
`move` only and never source reconfiguration or native cursor commands.

- [ ] **Step 4: Capture RED**

Run `InputIndicatorPolicyTests` with the focused command. Expected: compile
failure because `macInputIndicatorPresentationNeeded` does not exist.

### Task 12: Implement Compact Follower and Space Recovery

**Files:**
- Modify: `src/lib/gui/input/InputIndicatorGeometry.h`
- Modify: `src/lib/gui/input/InputIndicatorGeometry.cpp`
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.h`
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.mm`
- Modify: `src/lib/gui/input/MacInputIndicatorController.h`
- Modify: `src/lib/gui/input/MacInputIndicatorController.mm`
- Modify: `src/lib/common/Settings.cpp`

- [ ] **Step 1: Add pure panel metrics and follower**

Declare:

```cpp
struct MacInputIndicatorPanelStyle
{
  double width;
  double height;
  double cursorGap;
  double screenMargin;
  double labelSize;
  double cornerRadius;
  int opacityPercent;
};

constexpr MacInputIndicatorPanelStyle
macInputIndicatorPanelStyle()
{
  return {
      .width = 24,
      .height = 20,
      .cursorGap = 6,
      .screenMargin = 6,
      .labelSize = 11,
      .cornerRadius = 7,
      .opacityPercent = 65,
  };
}
```

Declare in `InputIndicatorGeometry.h`:

```cpp
std::optional<InputIndicatorPoint> followInputIndicator(
    InputIndicatorPoint previous, InputIndicatorPoint target,
    double catchUpFactor, double maximumLag
);
```

Implementation computes the Euclidean distance, returns null for invalid
values, advances by `catchUpFactor`, and if the remaining distance exceeds the
cap returns `target - unitVector * maximumLag`. It never overshoots.

- [ ] **Step 2: Add the panel presentation helper**

Declare:

```cpp
constexpr bool macInputIndicatorPresentationNeeded(
    bool activeSpaceChanged, bool visible
)
{
  return activeSpaceChanged || !visible;
}
```

Use only `orderFrontRegardless` as the presentation command. Keep the existing
window level and collection behavior.

- [ ] **Step 3: Implement the Space observer**

In `MacInputIndicatorPanel::Impl`, add:

```cpp
bool activeSpaceChanged = false;
bool followerInitialized = false;
InputIndicatorPoint follower;
id activeSpaceToken = nil;
```

Subscribe to `NSWorkspaceActiveSpaceDidChangeNotification` through the
workspace notification center on the main queue. The callback marks
`activeSpaceChanged = true` and `followerInitialized = false`; it reads no
window title, PID, content, or accessibility data.

Remove the observer before the panel closes. The observer uses the existing
shared-lifetime guard pattern from the disconnected focus monitor so a queued
block cannot dereference a destroyed `Impl`.

- [ ] **Step 4: Configure and follow with the compact style**

Update the panel view to use the style helper for panel dimensions, font size,
and radius. For automatic tone, use the current `controlAccentColor` only as a
subtle tint and draw text with the system label color; preserve explicit light
and dark treatments.

`show(options)` resets `followerInitialized` and marks presentation needed.
`move(position)`:

1. computes the exact target with 6 pt gap/margin;
2. returns false and hides for invalid geometry;
3. snaps target when `followerInitialized` is false, otherwise calls
   `followInputIndicator(..., 0.5, 18)`;
4. updates only the panel frame;
5. calls `orderFrontRegardless` when
   `macInputIndicatorPresentationNeeded(activeSpaceChanged, isVisible)` is
   true;
6. clears `activeSpaceChanged` only after the panel reports visible.

- [ ] **Step 5: Change the default opacity**

Change `Settings::Gui::InputIndicatorOpacityPercent` default from 85 to 65 and
the controller/panel option defaults to 65. Do not remove or change the 60-100
range.

- [ ] **Step 6: Verify focused GREEN**

Build and run:

```bash
InputIndicatorGeometryTests
InputIndicatorPolicyTests
InputIndicatorSettingsTests
SettingsPagesTests
TranslationFileTests
OSXInputSourceTests
OSXInputIndicatorRelayTests
CoreProcessInputIndicatorTests
ServerConfigDialogTests
```

Expected: 9/9 focused CTest executables pass.

### Task 13: Commit, Run the Full Gate, and Package

- [ ] **Step 1: Commit the implementation**

Commit only the geometry/panel/controller/settings/test files and updated
Korean/default metadata required by the focused tests. Preserve the mandatory
Co-authored-by trailer. Do not amend the earlier persistent-badge commits.

- [ ] **Step 2: Build and run the full Release gate**

Repeat the exact target enumeration from Task 8 and run CTest with only:

```text
-E '^(OSXClipboardTests|OSXKeyStateTests)$'
```

Expected: 158 executable targets build and all 157 eligible tests pass.

- [ ] **Step 3: Package to a clean path**

Configure `build/pointer-badge-trail-package`, package
`deskflow-2.0.9.0-macos-arm64.dmg`, and copy it to
`build/pointer-badge-trail/`. Recheck 2.0.9.0 metadata, arm64, SHA-256,
`hdiutil verify`, deep/strict codesign, TeamIdentifier, and CDHash. Stable sign
only if a valid identity exists; otherwise record ad-hoc state and the
Homebrew packaging diagnostics.

- [ ] **Step 4: Verify final ancestry and state**

Expected: clean named branch; direct ancestry from
`58069ddaac60648528df19c40de92b93e425d398` through the revision docs and
implementation commit; no HEAD tag, push, merge, release, PR, install, or
system UI change.

---

## Revision 3: Original Pet Companion and Event-Driven Tracking

### Task 14: Record Companion, Render, and Tracking RED

**Files:**
- Create: `src/lib/gui/input/MacInputPointerMonitor.h`
- Create: `src/lib/gui/input/MacInputPointerMonitor.mm`
- Modify: `src/lib/gui/CMakeLists.txt`
- Modify: `src/lib/common/Settings.h`
- Modify: `src/lib/common/Settings.cpp`
- Modify: `src/lib/gui/settings/SettingEffects.cpp`
- Modify: `src/lib/gui/settings/SettingsPages.cpp`
- Modify: `src/lib/gui/input/InputIndicatorGeometry.h`
- Modify: `src/lib/gui/input/InputIndicatorGeometry.cpp`
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.cpp`
- Modify: `src/unittests/gui/input/InputIndicatorPolicyTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorPolicyTests.cpp`
- Modify: `src/unittests/gui/settings/InputIndicatorSettingsTests.cpp`
- Modify: `src/unittests/gui/settings/SettingsPagesTests.cpp`

- [ ] **Step 1: Add companion setting assertions**

Update both settings tests to require:

```cpp
auto *companion = must<QComboBox>(
    pages, "comboInputIndicatorCompanion"
);
QCOMPARE(
    companion->currentData().toString(),
    QStringLiteral("cat")
);
QCOMPARE(
    Settings::value(Settings::Gui::InputIndicatorCompanion).toString(),
    QStringLiteral("cat")
);
QVERIFY(companion->findData(QStringLiteral("dog")).toInt() >= 0);
QVERIFY(companion->findData(QStringLiteral("turtle")).toInt() >= 0);
```

Assert the companion control is disabled with the feature off, enabled with
the feature, and participates in `SettingEffects::Live`.

- [ ] **Step 2: Add companion and render-state geometry assertions**

Use the pure types to test:

```cpp
QCOMPARE(macInputIndicatorPanelStyle(InputIndicatorCompanion::Cat).width, 28.0);
QCOMPARE(macInputIndicatorPanelStyle(InputIndicatorCompanion::Cat).height, 24.0);
QCOMPARE(macInputIndicatorPanelStyle(InputIndicatorCompanion::Dog).width, 28.0);
QCOMPARE(macInputIndicatorPanelStyle(InputIndicatorCompanion::Dog).height, 24.0);
QCOMPARE(macInputIndicatorPanelStyle(InputIndicatorCompanion::Turtle).width, 30.0);
QCOMPARE(macInputIndicatorPanelStyle(InputIndicatorCompanion::Turtle).height, 22.0);
```

Assert `inputIndicatorCompanionFromSetting("cat"|"dog"|"turtle")` and Cat for
unknown values.

- [ ] **Step 3: Add motion and Reduced Motion assertions**

Add tests for:

```cpp
inputIndicatorMotionOffset(previousTarget, target, 8, false)
inputIndicatorTransitionProgress(elapsedMs, 200, false)
inputIndicatorTransitionProgress(elapsedMs, 200, true)
```

Rows cover a 100 pt movement clamped to length 8, a 4 pt movement preserved,
zero motion, eased 0/100/200 ms progress (`0`, `0.5`, `1`), and Reduced Motion
returning zero deformation plus transition progress 1.

- [ ] **Step 4: Add event-monitor controller assertions**

Extend `PersistentControllerHarness` with:

```cpp
std::function<void()> pointerMoved;
bool pointerMonitorComplete = true;
int pointerMonitorStarts = 0;
int pointerMonitorStops = 0;
```

Set `Dependencies::pointerMonitorStart` to capture the callback and return
`MacInputPointerMonitorHandle{.complete = pointerMonitorComplete,
.stop = [&] { ++pointerMonitorStops; }}`.

Tests assert:

```cpp
pointerMonitorComplete == true -> no 16 ms fallback timer;
pointerMonitorComplete == false -> 16 ms fallback timer;
pointerMoved() -> one immediate move, 8 ms render timer;
pointerMoved() twice while render active -> only next frame sees latest target;
move result animating=false -> render timer stops;
feature off -> monitor stop called and render/fallback timers stop.
```

- [ ] **Step 5: Capture RED**

Build and run:

```bash
InputIndicatorGeometryTests
InputIndicatorPolicyTests
InputIndicatorSettingsTests
SettingsPagesTests
```

Expected: compile failures for missing companion/pointer-monitor/render APIs
and value failures for Cat/default setting.

### Task 15: Implement Settings and Pure Render Helpers

**Files:**
- Modify: files from Task 14
- Modify: `src/lib/gui/settings/SettingEffects.cpp`
- Modify: `src/unittests/gui/settings/InputIndicatorSettingsTests.cpp`
- Modify: `src/unittests/gui/settings/SettingsPagesTests.cpp`

- [ ] **Step 1: Add the companion setting**

In `Settings::Gui`:

```cpp
inline static const auto InputIndicatorCompanion =
    QStringLiteral("gui/inputIndicatorCompanion");
```

Add it to the all-keys list, the  default-value logic:

```cpp
if (key == Gui::InputIndicatorCompanion)
  return QStringLiteral("cat");
```

and `SettingEffects::live`.

- [ ] **Step 2: Add the Settings UI**

In the macOS Input source indicator card, add a `QComboBox` named
`comboInputIndicatorCompanion` with:

```cpp
Cat    -> "cat"
Dog    -> "dog"
Turtle -> "turtle"
```

Bind it to the new key and enable it together with position, opacity, and
tone.

- [ ] **Step 3: Add pure companion and style helpers**

Add:

```cpp
enum class InputIndicatorCompanion { Cat, Dog, Turtle };
InputIndicatorCompanion inputIndicatorCompanionFromSetting(
    const QString &value
);
MacInputIndicatorPanelStyle macInputIndicatorPanelStyle(
    InputIndicatorCompanion companion
);
```

Use `Cat` for invalid values. Cat/Dog style is 28 x 24; Turtle is 30 x 22. Tag
font is 8 pt semibold and normal body opacity is 65.

- [ ] **Step 4: Add pure motion helpers**

Use two helpers:

```cpp
std::optional<InputIndicatorPoint> inputIndicatorMotionOffset(
    InputIndicatorPoint previousTarget,
    InputIndicatorPoint target,
    double maximumLength,
    bool reducedMotion
);
double inputIndicatorTransitionProgress(
    double elapsedMilliseconds,
    double durationMilliseconds,
    bool reducedMotion
);
```

The motion helper returns zero for Reduced Motion, otherwise the target delta
clamped to `maximumLength` by Euclidean magnitude. The transition helper clamps
elapsed, applies smoothstep, and returns 1 immediately for Reduced Motion.

- [ ] **Step 5: Verify settings/pure-helper GREEN**

Run the Task 14 command. Expected: all four focused executables pass.

### Task 16: Implement Event-Driven Pointer Monitoring

**Files:**
- Create: `src/lib/gui/input/MacInputPointerMonitor.h`
- Create: `src/lib/gui/input/MacInputPointerMonitor.mm`
- Modify: `src/lib/gui/CMakeLists.txt`
- Modify: `src/lib/gui/input/MacInputIndicatorController.h`
- Modify: `src/lib/gui/input/MacInputIndicatorController.mm`
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.h`

- [ ] **Step 1: Add the AppKit monitor handle**

Declare:

```cpp
struct MacInputPointerMonitorHandle
{
  bool complete = false;
  std::function<void()> stop;
};

MacInputPointerMonitorHandle startMacInputPointerMonitor(
    std::function<void()> changed
);
```

The `.mm` implementation registers local and global monitors for moved and
dragged mouse events. Blocks call `changed` only; they neither store nor log
event data. The local handler returns its original event. `complete` is true
only when both monitor tokens exist.

- [ ] **Step 2: Add controller dependencies and timers**

Add:

```cpp
std::function<MacInputPointerMonitorHandle(std::function<void()>)>
    pointerMonitorStart;
std::function<MacInputIndicatorPanelMoveResult(InputIndicatorPosition)>
    panelMove;
```

Keep the source/panel injection seam real. Replace the always-active 16 ms
timer with:

```cpp
QTimer m_renderTimer;   // 8 ms, one-shot active only
QTimer m_fallbackTimer; // 16 ms when monitor setup is incomplete
```

- [ ] **Step 3: Coalesce events and stop on settlement**

`pointerTargetChanged()` calls an immediate panel move if the render timer is
inactive. Additional events while it is active leave only a dirty target
implicit in the next read of `NSEvent.mouseLocation`.

`panelMove` returns:

```cpp
struct MacInputIndicatorPanelMoveResult
{
  bool visible = false;
  bool animating = false;
};
```

When `animating` is true keep/restart the 8 ms render timer. When false stop
it. The 16 ms fallback timer remains independent only if `complete` is false.

- [ ] **Step 4: Preserve Space invalidation**

Pass a `presentationInvalidated` callback into
`MacInputIndicatorPanel`. Its Active Space observer marks `activeSpaceChanged`
and calls the callback, which starts one render frame even while no pointer
event has arrived.

- [ ] **Step 5: Verify tracking GREEN**

Run `InputIndicatorPolicyTests` and `InputIndicatorGeometryTests`. Expected:
complete monitor, incomplete fallback, coalesced event, timer-stop, and
Space-invalidation tests pass.

### Task 17: Draw the Original Companions

**Files:**
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.h`
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.mm`
- Modify: `src/lib/gui/input/MacInputIndicatorController.h`
- Modify: `src/lib/gui/input/MacInputIndicatorController.mm`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.cpp`
- Modify: `src/unittests/gui/input/InputIndicatorPolicyTests.cpp`

- [ ] **Step 1: Store transition and motion state**

Extend the AppKit view properties with companion, source, previous source,
transition progress, motion offset, and Reduced Motion. Extend the panel
`Impl` with previous target, follower, motion pose, and source transition
start.

- [ ] **Step 2: Draw three original vector silhouettes**

Use `NSBezierPath` only:

- Cat: rounded head/body, two triangular ears, curled side tail.
- Dog: rounded head/body, two floppy ears, curved tail.
- Turtle: oval shell, tiny head, feet, and tail.

Add a 1 pt system highlight stroke and translucent fill. Draw the language tag
in a small internal rounded capsule. Use cool English and warm Korean body
tints. No raster image, external asset, native cursor call, shadow, or private
API.

- [ ] **Step 3: Animate only allowed state**

In `move`, compute:

```cpp
sourceProgress =
  inputIndicatorTransitionProgress(elapsedMs, 200, reducedMotion);
motionTarget =
  inputIndicatorMotionOffset(previousTarget, target, 8, reducedMotion);
```

Ease the motion pose, then apply it only to body/ear/tail curves and <=10%
body-width morph. Cap the panel follower at 8 pt. Crossfade old/new label and
source tint. Mark `animating=true` until transition, follower, and motion are
settled; otherwise return false.

For Reduced Motion, set follower directly to target, motion offset to zero,
source progress to 1, and no animation timer.

- [ ] **Step 4: Update Korean UI copy and translation contract**

Add/translate:

- Companion
- Cat
- Dog
- Turtle

Update the Korean indicator description to describe a persistent companion
that carries the current input-source tag. Keep every source string translated
in `translations/deskflow_ko.ts`; restore generated changes in other locale
files.

- [ ] **Step 5: Verify focused GREEN**

Build and run the existing nine focused suites plus `TranslationFileTests`
for the new labels. Expected: 9/9 executable tests pass; the full geometry and
policy suites contain the pet enum, style, motion, Reduced Motion, event,
fallback, and Space coverage.

### Task 18: Commit, Run Full Gate, and Package

- [ ] **Step 1: Commit docs and implementation**

Keep the docs revision and implementation as separate commits. Preserve the
required Co-authored-by trailer and do not amend prior commits.

- [ ] **Step 2: Build 158 targets and run 157 eligible CTests**

Use Release Qt 6.12, warnings as errors, and exclude only
`OSXClipboardTests` and `OSXKeyStateTests`.

- [ ] **Step 3: Package to a new clean path**

Configure `build/pointer-badge-companion-package`, copy the exact
2.0.9.0 DMG to `build/pointer-badge-companion/`, and record SHA-256, metadata,
architecture, codesign state, TeamIdentifier, CDHash, and macdeploy diagnostics.
Do not install or stable-sign without an existing identity.

- [ ] **Step 4: Verify final state**

Expected: clean branch with direct ancestry from
`a0db752a36ad254824f478fd96e97d7463509877`, no tag/push/merge/release/PR, and
the final report identifies visual feel, live full-screen transition, and
peripheral latency as residual visual-only checks not automated by CTest.

---

## Revision 4: Idle, Walk, and Run Pet Motion

### Task 19: Record Deterministic Motion-State RED

**Files:**
- Modify: `src/lib/gui/input/InputIndicatorGeometry.h`
- Modify: `src/lib/gui/input/InputIndicatorGeometry.cpp`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.cpp`

- [ ] **Step 1: Add state, phase, and timing tests**

Add these slots:

```cpp
void companionMotionStateClassifiesIdleWalkRun();
void companionMotionStateKeepsMovingForSettleDelay();
void companionMotionPhaseWrapsPerState();
void companionMotionFrameIntervalsStayLowDuty();
```

Define the desired pure API:

```cpp
enum class InputIndicatorMotionState
{
  Idle,
  Walk,
  Run
};

InputIndicatorMotionState inputIndicatorMotionState(
    InputIndicatorMotionState previous,
    double pointerSpeedPointsPerSecond,
    double stoppedMilliseconds,
    bool reducedMotion
);

double inputIndicatorMotionPhase(
    InputIndicatorMotionState state,
    double elapsedMilliseconds
);

int inputIndicatorMotionFrameInterval(
    InputIndicatorMotionState state,
    bool reducedMotion
);
```

- [ ] **Step 2: Fix the threshold rows**

Assert:

```text
speed 0, stopped >= 180 -> Idle
speed 30, previous Idle -> Walk
speed 650, previous Walk -> Run
speed 0, stopped 179, previous Run -> Run
speed 0, stopped 180, previous Run -> Idle
reduced motion -> Idle with frame interval 0
```

Choose exact thresholds in one constant:

```cpp
walk = 30 pt/s
run = 650 pt/s
settle = 180 ms
idle interval = 75 ms
walk/run interval = 8 ms
```

- [ ] **Step 3: Assert phase wrap**

Idle phase is 0.0 at 0 ms, 0.5 at 600 ms, and 0.0 at 1200 ms. Walk uses
250 ms and Run uses approximately 143 ms for 4 Hz and 7 Hz, respectively.
Invalid/non-finite input returns phase 0.

- [ ] **Step 4: Capture RED**

Build and run `InputIndicatorGeometryTests`. Expected: compile failure for the
missing motion-state API.

### Task 20: Record Controller and Panel Duty-Cycle RED

**Files:**
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.h`
- Modify: `src/unittests/gui/input/InputIndicatorPolicyTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorPolicyTests.cpp`

- [ ] **Step 1: Extend panel move results**

The test-facing result contains:

```cpp
struct MacInputIndicatorPanelMoveResult
{
  bool visible = false;
  bool animating = false;
  bool reducedMotion = false;
  int frameIntervalMilliseconds = 0;
};
```

- [ ] **Step 2: Add controller timer tests**

Add:

```cpp
void persistentIdleCompanionUsesLowDutyTimer();
void persistentWalkCompanionUsesRenderTimer();
void persistentRunCompanionUsesRenderTimer();
void persistentHiddenAndReducedMotionStopTimers();
```

Harness rows set `frameIntervalMilliseconds` to 75 or 8 and
`reducedMotion` true/false. Assert:

```text
Idle visible result -> render timer 75 ms
Walk/Run visible result -> render timer 8 ms
reduced result -> render and fallback timers stopped
hidden result -> render stopped
```

Add a panel/state test that source changes and companion changes do not change
the current `InputIndicatorMotionState` contract.

- [ ] **Step 3: Capture RED**

Run `InputIndicatorPolicyTests`. Expected: the old move result has no
interval/reduced fields and the controller never changes its 8 ms interval to
75 ms.

### Task 21: Implement Idle, Walk, and Run

**Files:**
- Modify: `src/lib/gui/input/InputIndicatorGeometry.h`
- Modify: `src/lib/gui/input/InputIndicatorGeometry.cpp`
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.h`
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.mm`
- Modify: `src/lib/gui/input/MacInputIndicatorController.h`
- Modify: `src/lib/gui/input/MacInputIndicatorController.mm`

- [ ] **Step 1: Add pure state and phase helpers**

Implement the API from Task 19 with finite-value guards. Run is selected at
650 pt/s, Walk at 30 pt/s, and Idle starts only after 180 ms below the Walk
threshold. Reduced Motion always returns Idle and interval 0.

- [ ] **Step 2: Track pointer velocity inside the panel**

Extend `MacInputIndicatorPanel::Impl` with:

```cpp
InputIndicatorMotionState motionState = InputIndicatorMotionState::Idle;
std::chrono::steady_clock::time_point motionStateStarted;
std::chrono::steady_clock::time_point lastPointerMovement;
std::chrono::steady_clock::time_point previousTargetTime;
```

On each `move`, calculate speed from the target frame delta and elapsed
monotonic time. Mark `lastPointerMovement` only for a real target delta above
the settled tolerance. Transition state using the pure helper.

- [ ] **Step 3: Render Cat motion**

Add `motionState` and `motionPhase` properties to the AppKit view.

- Idle: body fixed, tail sway with a 1200 ms sine cycle; no blink required.
- Walk: use a 250 ms phase, alternating paw/leg offsets and <=0.45 pt body
  bob.
- Run: use a 143 ms phase, <=0.9 pt body bob, 6-8% body/ear response, and
  stronger tail pose.

Tail/leg motion stays inside the panel bounds and never changes the target
frame. Keep the tag stationary and source crossfade independent.

- [ ] **Step 4: Give Dog and Turtle analogous bounded motion**

Use the same state/phase machinery with Dog tail/ear and paw motion and
Turtle head/feet/shell bob. Keep the Turtle shell/body stretch within the same
limits. Do not create new controls or assets.

- [ ] **Step 5: Apply timer duty**

Return the desired frame interval from `MacInputIndicatorPanel::move`:

```text
Idle -> 75 ms
Walk/Run -> 8 ms
source morph active -> 8 ms
Reduced Motion -> 0 and animating=false
```

In `MacInputIndicatorController`, set `m_renderTimer` to the returned interval
whenever it changes. Stop it for hidden/panel failure/reduced states. The
16 ms fallback still operates only when event-monitor setup is incomplete and
motion is not Reduced.

- [ ] **Step 6: Verify focused GREEN**

Run:

```text
InputIndicatorGeometryTests
InputIndicatorPolicyTests
InputIndicatorSettingsTests
SettingsPagesTests
TranslationFileTests
OSXInputSourceTests
OSXInputIndicatorRelayTests
CoreProcessInputIndicatorTests
ServerConfigDialogTests
```

Expected: 9/9 pass, including phase/state and timer-duty assertions.

### Task 22: Commit, Full Gate, and Package

- [ ] **Step 1: Commit docs and implementation separately**

Preserve the mandatory Co-authored-by trailer and do not amend earlier
commits.

- [ ] **Step 2: Build 158 targets and run 157 eligible tests**

Use Release Qt 6.12 with warnings as errors and exclude only
`OSXClipboardTests` and `OSXKeyStateTests`.

- [ ] **Step 3: Package**

Build a clean `build/pointer-badge-motion/deskflow-2.0.9.0-macos-arm64.dmg`.
Record hash, exact metadata, architecture, signature/CDHash and tooling state.
Do not install or stable-sign.

- [ ] **Step 4: Verify final ancestry and state**

Expected: clean named branch with direct ancestry from
`4f508481f3200dce827939197040cd0642e19349`, no tag/push/merge/release/PR,
and visual feel/full-screen Space/physical-device smoothness reported as
residual live checks.

---

## Revision 5: Unified Companion Transparency

### Task 23: Record Render-Alpha RED

**Files:**
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.cpp`

- [ ] **Step 1: Add the render-model test**

Add:

```cpp
void companionRenderAlphaFollowsSettingAndBoundsDetail();
```

Assert:

```cpp
macInputIndicatorRenderAlpha(59).body == 0.60
macInputIndicatorRenderAlpha(65).body == 0.65
macInputIndicatorRenderAlpha(101).body == 1.00
detailAlpha >= bodyAlpha
detailAlpha - bodyAlpha <= 0.10
```

- [ ] **Step 2: Capture RED**

Run `InputIndicatorGeometryTests`. Expected: compile failure because the
render-alpha model does not exist.

### Task 24: Apply One Alpha Model

**Files:**
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.h`
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.mm`

- [ ] **Step 1: Add the pure render model**

```cpp
struct MacInputIndicatorRenderAlpha
{
  double body = 0;
  double detail = 0;
};

constexpr MacInputIndicatorRenderAlpha
macInputIndicatorRenderAlpha(int opacityPercent);
```

Clamp body to 60-100% and set detail to the minimum of body + 10 percentage
points and 100%.

- [ ] **Step 2: Replace fixed alpha**

Pass both values through `indicatorTint`, `fillAndHighlight`, tail stroke,
eyes, tag background, tag border, and `A`/`한` rendering. Cat/Dog/Turtle body
parts use body alpha exactly; glyph/outline use detail alpha crossfaded by
the source transition.

- [ ] **Step 3: Verify focused GREEN**

Run the existing nine focused targets. Expected: 9/9 pass and the render-alpha
test proves the bound.

### Task 25: Commit, Full Gate, and Package

- [ ] **Step 1: Commit docs and implementation separately**

Do not amend existing visual commits.

- [ ] **Step 2: Run 158 build targets and 157 eligible tests**

Exclude only `OSXClipboardTests` and `OSXKeyStateTests`.

- [ ] **Step 3: Package**

Build a clean `build/pointer-badge-alpha/deskflow-2.0.9.0-macos-arm64.dmg`,
verify metadata, SHA-256, architecture, signature, and package tooling state.
Do not install or stable-sign.

- [ ] **Step 4: Verify final state**

Expected: clean ancestry from
`28cd0c13e6db15bc652e627e295250e750d31f2f`, no tag/push/merge/release/PR,
and no source change outside the render model.

---

## Revision 6: Eye-Free In-Body Cat Silhouette

### Task 26: Record Silhouette Render-Model RED

**Files:**
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.cpp`

- [ ] **Step 1: Add a pure render plan**

Define:

```cpp
enum class InputIndicatorRenderPrimitive
{
  Tile,
  SilhouetteUnion,
  LanguageGlyph
};

struct MacInputIndicatorRenderModel
{
  InputIndicatorCompanion companion;
  InputIndicatorRenderPrimitive silhouette =
      InputIndicatorRenderPrimitive::SilhouetteUnion;
  bool usesEyePrimitives = false;
  double tileWidth = 24;
  double tileHeight = 24;
  double totalWidth = 30;
  double totalHeight = 28;
  InputIndicatorRect languageBounds;
};

constexpr MacInputIndicatorRenderModel
macInputIndicatorRenderModel(InputIndicatorCompanion companion);
```

- [ ] **Step 2: Assert the silhouette contract**

For Cat, Dog, and Turtle assert:

```text
usesEyePrimitives == false
silhouette == SilhouetteUnion
tileWidth/height == 24
totalWidth <= 30, totalHeight <= 28
languageBounds stays wholly inside the companion body/shell bounds
```

Cat's bounds are the torso under the head; Dog's bounds are centered in the
body; Turtle's are centered in the shell. The external-corner badge concept is
not valid.

- [ ] **Step 3: Add two-segment Cat-tail pose assertions**

Define:

```cpp
struct InputIndicatorTailPose
{
  InputIndicatorPoint root;
  InputIndicatorPoint proximalControl;
  InputIndicatorPoint middleIncomingControl;
  InputIndicatorPoint middle;
  InputIndicatorPoint middleOutgoingControl;
  InputIndicatorPoint tipControl;
  InputIndicatorPoint tip;
};

InputIndicatorTailPose inputIndicatorCatTailPose(
    InputIndicatorPoint root, double length, double phase,
    double velocityX, bool reducedMotion
);
```

Assert the root equals the supplied anchor, incoming/outgoing middle
controls share one tangent, the complete bend stays bounded, the tip has
phase lag relative to the middle, and Reduced Motion returns the fixed neutral
curl.

- [ ] **Step 4: Capture RED**

Run `InputIndicatorGeometryTests`. Expected: compile failure because
`MacInputIndicatorRenderModel` and
`macInputIndicatorRenderModel` do not exist.

### Task 27: Replace Faces with Unified Silhouettes

**Files:**
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.h`
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.mm`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.cpp`

- [ ] **Step 1: Implement the render model**

Return the one render plan per companion. Cat has `SilhouetteUnion`; Dog and
Turtle use the same primitive with their own bounds. The plan never contains
an eye primitive.

- [ ] **Step 2: Replace Cat drawing**

Build one filled Cat path from a compact sitting/walking profile:

- rounded torso;
- two pointed ears integrated into the upper silhouette;
- curved tail joined to the body side;
- lower paw notches that follow walk/run phase.

Do not append or draw eye, pupil, mouth, or whisker paths. Use a dark/cool
source-tinted silhouette alpha at detail opacity on a subtle translucent tile.

- [ ] **Step 3: Replace Dog and Turtle drawing**

Dog uses one floppy-ear/body/tail silhouette. Turtle uses one shell/head/tail
silhouette. Both receive the same alpha and motion model but remain visually
distinct at <=30 x 28.

- [ ] **Step 4: Move the language glyph inside**

Remove the separate lower/corner tag background. Draw `A`/`한` at the
`languageBounds` returned by the render model, inside the torso/shell. Keep a
small high-contrast glyph/optional translucent underlay without facial
elements.

- [ ] **Step 5: Keep natural motion**

Idle uses only tail sway and no face change. Walk uses alternating paw/foot
contours, <=1-2 pt body displacement, and <=5 degree lean. Run remains
velocity-gated, with <=0.9 pt bob and <=8%/8 degree deformation. Reduced Motion
draws the stable silhouette directly.

The Cat tail uses the pure two-segment pose: one root-anchored cubic into the
middle and one tangent-continuous cubic to the tip. A tapered root/thinner tip
and opposite-direction velocity influence make the tip follow with inertia
without becoming a rigid pole or detached segment.

- [ ] **Step 6: Verify focused GREEN**

Run the existing nine focused targets. Expected: 9/9 pass; geometry proves no
eye primitive, one silhouette primitive, in-body glyph bounds, tile/total
size, alpha bounds, root/tangent continuity, tail phase lag, and reduced
neutral pose.

### Task 28: Commit, Full Gate, and Package

- [ ] **Step 1: Commit docs and implementation separately**

Preserve the mandatory trailer; do not amend the prior transparency or
locomotion commits.

- [ ] **Step 2: Build and run the full Release gate**

Build 158 targets and run 157 eligible CTests with only the existing two
exclusions.

- [ ] **Step 3: Package**

Build a clean `build/pointer-badge-silhouette/deskflow-2.0.9.0-macos-arm64.dmg`,
record metadata, SHA-256, signature/CDHash/tool state, and do not install or
stable-sign.

- [ ] **Step 4: Verify state**

Expected: direct ancestry from
`396258834879ffc0f0419f47411489cbd72edef5`, clean branch, no tag/push/merge/
release/PR, and actual silhouette cuteness remains a visual-only live check.

---

## Revision 7: Tile-Free Side-Profile Cat Companion

Revision 6's rounded-tile concept is superseded. The companion is only the
free-standing animal silhouette plus its internal language glyph. No rounded
rectangle, icon container, border, separate tag, or other framing path is
allowed in the render model or drawing path.

### Task 29: Record Tile-Free Cat Render RED

**Files:**
- Modify: `src/lib/gui/input/InputIndicatorGeometry.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.cpp`

- [ ] **Step 1: Remove the tile expectation**

The render model exposes exactly the silhouette union and language-glyph
primitives. It has no `Tile` enum value, tile dimensions, tile-use flag, tile
radius, or generic badge background.

- [ ] **Step 2: Add the Cat anatomy and facing contract**

Define Cat head, torso, paw, and tail bounds in the 30 x 28 pt model. Assert:

```text
Cat usesSideProfile == true
Cat mirrorsWithMotion == true
head lies in front of the torso
tail extends behind the torso
paws remain below the torso
languageBounds stays wholly inside torsoBounds
all anatomy bounds stay inside silhouetteBounds
```

Add `inputIndicatorCompanionFacesRight(previous, motionX)`. Positive horizontal
motion selects right-facing, negative motion selects left-facing, motion in the
dead zone preserves the prior facing, and non-finite motion preserves it too.

- [ ] **Step 3: Capture RED**

Build `InputIndicatorGeometryTests`. Expected failure is limited to the
missing two-primitive model, Cat anatomy fields, and facing helper.

### Task 30: Draw a Free-Standing Cat Profile

**Files:**
- Modify: `src/lib/gui/input/InputIndicatorGeometry.cpp`
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.h`
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.mm`

- [ ] **Step 1: Implement the pure model**

Remove every tile field and style metric. Keep one silhouette primitive and
one language primitive. Give Cat a left-facing base profile centered in the
30 pt panel; preserve Dog and Turtle as distinct unified silhouettes.

- [ ] **Step 2: Replace the Cat path**

Build one side-profile Cat from the modeled regions:

- a rounded elongated torso;
- a connected neck and head with a subtle muzzle;
- two pointed ears integrated into the upper outline;
- alternating lower paws with bounded walk/run displacement;
- the existing anchored two-segment, tapered tail behind the torso.

Do not append eye, pupil, mouth, nose-mark, whisker, highlight, or face-detail
paths. No tile or framing path may be called before or after the animal path.

- [ ] **Step 3: Mirror only the silhouette**

Track one `facesRight` flag in the panel implementation. Feed the eased
horizontal motion through the pure facing helper, mirror the AppKit path around
the panel center, and keep the language glyph outside the transform so `A` and
`한` remain upright. Preserve facing while stopped.

- [ ] **Step 4: Apply the no-tile alpha**

The free-standing animal and all of its body parts use body opacity directly,
defaulting to 65%. Only the glyph uses the bounded detail alpha. Source
transition, locomotion, Space presentation, Reduced Motion, and privacy
behavior remain unchanged.

### Task 31: Verify the Cat Correction

- [ ] **Step 1: Run focused GREEN**

Build and run the existing nine focused targets. Expected: 9/9 pass, including
no-tile render primitives, region containment, facing persistence, tail
continuity, alpha bounds, motion state, and panel/controller behavior.

- [ ] **Step 2: Run the Release gate**

Build all 158 registered targets and run 157 eligible CTests under Release
Qt 6.12 warnings as errors, excluding only `OSXClipboardTests` and
`OSXKeyStateTests`.

- [ ] **Step 3: Self-review**

Check the final diff for any remaining Tile/tile-use model field, drawing
function, corner-radius metric, external tag, face primitive, or accidental
native-cursor change. Inspect the side-profile path and transform for clipping
inside 30 x 28 pt.

### Task 32: Commit, Package, and Audit

- [ ] **Step 1: Commit docs and implementation separately**

Keep the mandatory trailer. Do not amend the Revision 6 commits.

- [ ] **Step 2: Package the final candidate**

Build a clean
`build/pointer-badge-cat/deskflow-2.0.9.0-macos-arm64.dmg`, record metadata,
SHA-256, signature/CDHash, architecture, and package tooling state. Do not
install, stable-sign, push, merge, tag, release, or open a PR.

- [ ] **Step 3: Audit final state**

Expected: direct ancestry from
`2724a5794f711ebd6503191f4ad0537278c77cef`, a clean worktree, and only the
live visual feel remaining as a non-automated check.

---

## Revision 8: Seated, Walk, Run Cat and Glyph Palette

### Task 33: Record Pose and Palette RED

**Files:**
- Modify: `src/lib/gui/input/InputIndicatorGeometry.h`
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.cpp`

- [ ] **Step 1: Add pose-selection tests**

Extend the pure motion-state test input with raw pointer movement and
pre-catch-up follower lag. Assert:

```text
Idle + 30 pt/s or 3 pt movement -> Walk
Walk + 650 pt/s -> Run
Walk + 48 pt movement -> Run
Walk + 24 pt follower lag -> Run
Run preserves direction for the 180 ms settle delay
Reduced Motion always returns Idle
```

- [ ] **Step 2: Add pure Cat geometry tests**

Define a `InputIndicatorCatPose` containing torso, head, front paw, rear paw,
body angle, tail length, and tail straightness. Assert:

```text
Idle torso is seated/upright
Walk torso is lower/wider and level
Run torso is widest/lowest and leans <=8 degrees
Run front paw moves forward and rear paw moves backward
all posed regions stay inside the 30 x 28 silhouette model
language bounds stay in every torso pose
Reduced Motion reproduces the neutral seated pose
```

- [ ] **Step 3: Add tail-straightness assertions**

Reuse the root/tangent/phase-lag tail tests while asserting Run straightness
has less vertical deviation and controlled inertia than Idle. The common
two-segment continuity and clamp contracts remain unchanged.

- [ ] **Step 4: Add palette and mirror tests**

Define a pure render color model with body, edge, `A`, and `한` RGB values.
Assert:

```text
body remains #F2F3F5-class off-white and source tint is <=8%
A is muted steel/slate blue and 한 is dusty mauve/rose
Automatic/Light/Dark glyph/body contrast >= 4.5
body alpha == configured alpha
glyph alpha == body + <=10 points
languageMirrorsWithSilhouette == false
```

- [ ] **Step 5: Add halo-wave tests**

Define a fixed-cap pulse helper and deterministic frame model. Assert:

```text
starts within 50 ms are rejected/coalesced
at most 3 newest waves remain
alternating A/한 sources remain ordered
280 ms frames scale <=1.25 and alpha is bounded
alpha blooms then decays with no abrupt end
Reduced Motion produces no frame
```

- [ ] **Step 6: Capture RED**

Build `InputIndicatorGeometryTests`. Expected failure is limited to the new
distance inputs, Cat pose API, straightness input, language-mirror flag, and
color model.

### Task 34: Draw Three Cat Gaits and Neutral Palette

**Files:**
- Modify: `src/lib/gui/input/InputIndicatorGeometry.cpp`
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.h`
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.mm`

- [ ] **Step 1: Feed escape distance into the state selector**

Capture raw pointer distance from the prior exact target and companion/target
lag before follower catch-up. Preserve speed as a signal; Run when any
high-speed, large-step, or escape-lag threshold is met. The follower remains
bounded/no-overshoot with an 8 pt maximum lag.

- [ ] **Step 2: Implement pure Cat pose geometry**

Return state-specific torso/head/paw/angle/tail values. Idle is seated, Walk is
a horizontal standing gait, and Run is an elongated leap pose. Reduced Motion
returns the fixed Idle pose and no paw displacement.

- [ ] **Step 3: Replace the legs-only Cat drawing**

Draw the Cat from the selected pose rather than perturbing one fixed body.
Keep head/ear/muzzle in profile, preserve the anchored tapered tail, and apply
pose angle only inside the silhouetted path. Only the silhouette mirrors with
motion; the language glyph remains upright at stable bounds.

- [ ] **Step 4: Apply off-white body and source glyph colors**

Remove whole-body blue/coral source tinting. Fill Cat, Dog, and Turtle with
only an <=8% source blend on the neutral body, stroke each silhouette with the
subtle edge, and choose source-specific `A`/`한` glyph color for the 200 ms
crossfade. Keep body opacity at the configured value and glyph alpha at the
existing detail bound.

- [ ] **Step 5: Add ambient and pulsed Cat halo**

Draw a low-alpha source-colored ambient halo behind the Cat body. On exact
source change, accept at most one wave per 50 ms, preserve only the three newest
waves, and render each behind the body for 280 ms with eased expansion, bloom,
and decay. Rapid latest requests remain pending rather than accumulating
timers. Reduced Motion and hidden states clear/bypass the halo and never extend
the timer.

### Task 35: Verify and Self-Review

- [ ] **Step 1: Run focused GREEN**

Build and run the existing nine focused suites. Expected: 9/9 pass with pose,
distance, palette contrast, alpha, mirroring, tail, controller, and settings
coverage.

- [ ] **Step 2: Run the full Release gate**

Build all targets and run 157 eligible CTests, excluding only
`OSXClipboardTests` and `OSXKeyStateTests`.

- [ ] **Step 3: Inspect visual invariants**

Confirm no native cursor API, new monitor, coordinate IPC/logging, tile, face
primitive, glyph mirror, or body source tint above 8% exists. Confirm Run does
not change the follower contract and every pose remains inside the panel.

### Task 36: Commit, Package, and Audit

- [ ] **Step 1: Commit docs and implementation separately**

Use the standard trailer and do not amend the Revision 7 commits.

- [ ] **Step 2: Package**

Build a clean
`build/pointer-badge-gait/deskflow-2.0.9.0-macos-arm64.dmg`, verify metadata,
SHA-256, arm64, CDHash, signature, and package diagnostics. Do not install or
stable-sign.

- [ ] **Step 3: Verify final state**

Expected: direct ancestry from
`732b06ac4cab581b59a052cda47c55104d1407fa`, clean worktree, no tag, push,
merge, release, PR, or install.

---

## Revision 9: Approved Cat Artwork Bézier Integration

### Task 37: Record Approved Artwork RED

**Files:**
- Modify: `src/lib/gui/input/InputIndicatorGeometry.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.cpp`

- [ ] **Step 1: Add the art-frame selector contract**

Expose `InputIndicatorCatArtworkFrame` values for `IdleRise`, `IdleNeutral`,
`IdleFall`, `WalkReach`, `WalkPass`, `WalkPush`, `RunCoil`, `RunAir`, and
`RunLand`. Extend `InputIndicatorCatPose` with:

```cpp
InputIndicatorCatArtworkFrame artworkFrame;
InputIndicatorRect silhouetteBounds;
InputIndicatorRect languageBounds;
```

Assert:

```text
Idle phase 0 -> IdleNeutral
Idle positive tail phase -> IdleRise
Idle negative tail phase -> IdleFall
Walk thirds -> Reach / Pass / Push
Run thirds -> Coil / Air / Land
Reduced Motion -> IdleNeutral
```

- [ ] **Step 2: Add SVG-derived proportion assertions**

Use the approved sheet's union transform: source x 53.910000..654.845600,
source y 67.875200..323.323648, output x 1..29, bottom 4, one uniform source
scale on X, and a 10% optical Y lift for 8 pt glyph containment. Assert
normalized width/height for representative frames:

```text
IdleNeutral width ~= 20.6824, height ~= 12.9470
WalkReach width ~= 21.8200, height ~= 10.7184
RunAir width ~= 28.0000, height ~= 9.7445
```

Tolerance is 0.02 pt. This proves the wide run pose is not replaced by the
previous primitive rounded-body drawing.

- [ ] **Step 3: Assert pose-specific glyph centers**

Assert language bounds center around the source sheet's normalized points:

```text
IdleNeutral center ~= (17.4985, 9.0907)
WalkReach center ~= (16.3336, 10.5258)
RunAir center ~= (16.6132, 11.1921)
```

Mirror the center position for a left-facing Cat while keeping
`languageMirrorsWithSilhouette == false`. Assert the complete 9 x 8 pt bounds
stay inside each normalized silhouette.

- [ ] **Step 4: Capture RED**

Build `InputIndicatorGeometryTests`. Expected failure is limited to missing art
frames, source-derived bounds, and pose-specific language bounds.

### Task 38: Generate Compiled Native Cat Path Constants

**Files:**
- Create: `src/lib/gui/input/MacInputIndicatorCatArtwork.h`
- Modify: `src/lib/gui/input/InputIndicatorGeometry.h`

- [ ] **Step 1: Convert the approved sheet at authoring time**

Use only the verified art geometry. Convert each SVG `M`, `L`, `C`, and `Q`
command into a compiled command array; convert `Q` to cubic control points.
Normalize every coordinate with the transform in Task 37 and invert SVG Y into
AppKit Y. Include the far and near path for all nine keyframes.

- [ ] **Step 2: Expose no runtime asset mechanism**

The generated header exposes `constexpr` command arrays and a pure
`macInputIndicatorCatArtwork(frame)` lookup. It does not expose a file name,
URL, absolute session path, string path parser, SVG parser, file read, XML
dependency, or network access.

- [ ] **Step 3: Define art metrics next to path constants**

Give each frame the exact normalized silhouette bounds and glyph center used by
the geometry tests. Keep command count and bounds in one structure so tests,
selection, and drawing cannot drift.

### Task 39: Render the Approved Cat Art Natively

**Files:**
- Modify: `src/lib/gui/input/InputIndicatorGeometry.cpp`
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.h`
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.mm`

- [ ] **Step 1: Replace primitive Cat construction**

Delete the rounded-rect/oval Cat body/head/ear/paw composition and iterate the
compiled command arrays into `NSBezierPath`. Draw the far path first in the
softer off-white shadow, then the near path in the source-art off-white. Keep
the subtle edge, Cat halo, and source tint behavior around the exact art union.

- [ ] **Step 2: Mirror path and label placement separately**

Source coordinates face right. Mirror the path about panel width only when
`facesRight == false`. Derive the 9 x 8 pt label bounds from the selected
frame's glyph center, mirror only that position when facing left, and draw the
glyph without a transform.

- [ ] **Step 3: Preserve behavior and Reduced Motion**

Retain 120 Hz Walk/Run selection, Idle 75 ms timing, exact pointer lag, the
source halo pulse history, and Space/orderFront behavior. Reduced Motion uses
the `IdleNeutral` frame and bypasses tail/gait/halo animation.

### Task 40: Verify, Commit, Package, and Audit

- [ ] **Step 1: Verify focused and full gates**

Run the nine focused CTest executables, then all targets and 157 eligible
Release CTests with only `OSXClipboardTests` and `OSXKeyStateTests` excluded.

- [ ] **Step 2: Commit and package**

Commit the spec/plan and implementation separately with the standard trailer.
Build
`build/pointer-badge-artwork/deskflow-2.0.9.0-macos-arm64.dmg`, verify metadata,
SHA-256, arm64, signature, and CDHash. Do not install or stable-sign.

- [ ] **Step 3: Audit final state**

Expected: direct ancestry from
`12081d1f97cd0100fb4dbcb27053f9367e4f3a91`, clean worktree, no generated SVG
asset/path parser/runtime file read, no tile/eye/cursor-mutation change, and no
push/merge/tag/release/PR/install.

---

## Revision 10: Source- and Gait-Aware Label Fit

### Task 41: Record Label-Fit RED

**Files:**
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.cpp`

- [ ] **Step 1: Add pure source/state preference assertions**

Define a pure Cat label layout API that takes the source, motion state, label
container, measured ink origin, and measured ink size. Assert exact preferred
sizes:

```text
A Idle 8.0, Walk 8.0, Run 7.6
한 Idle 6.8, Walk 6.4, Run 6.1
```

- [ ] **Step 2: Prove every label rectangle stays in its pose**

For every Cat art frame and both sources, pass conservative real AppKit
measurement ratios into the pure fitter. Assert:

```text
layout font size <= preferred size
ink rectangle is finite and positive
ink rectangle is wholly inside the frame's languageBounds
Korean layout applies a negative AppKit-Y ink correction
```

- [ ] **Step 3: Add fail-closed fitting**

Pass an intentionally oversized measurement to Idle, Walk, and Run. Assert
the returned font size decreases and the fitted ink rectangle remains inside
the 9 x 8 pt container. Pass invalid/non-finite geometry and assert
`std::nullopt`.

- [ ] **Step 4: Capture RED**

Build `InputIndicatorGeometryTests`. Expected failure is limited to the
missing preference/measurement-fit layout API.

### Task 42: Measure and Draw Fitted Cat Glyphs

**Files:**
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.h`
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.mm`

- [ ] **Step 1: Implement pure fitting**

Return one layout with preferred/fitted font size, drawing origin, and ink
rectangle. Use uniform scaling only, center the ink, clamp the Korean baseline
correction into the container, and reject non-finite/non-positive input.

- [ ] **Step 2: Measure real AppKit glyph bounds**

For each source in the label crossfade, create its source/state font, then use
`NSString boundingRectWithSize:options:attributes:context:` with
`NSStringDrawingUsesDeviceMetrics` and line/font-leading options. Re-run the
pure fitter after any size reduction so the actual native measurement and the
pure bound agree.

- [ ] **Step 3: Draw by the fitted rectangle**

Replace the shared 8 pt font and `drawInRect` call with a per-label measured
rect and `drawWithRect:options:attributes:context:`. Keep paragraph
alignment/alpha transition unchanged. Preserve the exact Dog/Turtle 8 pt
render path.

### Task 43: Verify, Package, and Audit

- [ ] **Step 1: Run focused gates**

Run InputIndicatorGeometryTests, InputIndicatorPolicyTests, the panel/render
build, SettingsPagesTests, InputIndicatorSettingsTests, and TranslationFileTests.

- [ ] **Step 2: Run the full Release gate**

Build all targets and run 157 eligible CTests, excluding only
`OSXClipboardTests` and `OSXKeyStateTests`.

- [ ] **Step 3: Commit and package**

Commit docs and implementation separately with the standard trailer. Build
`build/pointer-badge-label-fit/deskflow-2.0.9.0-macos-arm64.dmg` and verify
metadata, SHA-256, arm64, signature and CDHash. Do not install or stable-sign.

- [ ] **Step 4: Audit final state**

Expected: direct ancestry from
`7b7d2f8fc609e72a42782d1b4f4137aeb9bb850c`, clean worktree, no label
behavior changed outside source/state fitting, and no push/merge/tag/release/
PR/install.

---

## Revision 11: CoreText Glyph-Ink Containment

### Task 44: Record Actual-Ink RED

**Files:**
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.cpp`
- Add: `src/unittests/gui/input/InputIndicatorCoreTextRenderTests.mm`
- Modify: `src/unittests/gui/input/CMakeLists.txt`

- [ ] **Step 1: Replace offset assumptions in the pure fitter test**

Pass a negative Korean glyph-path origin (for example `y=-0.374`) and positive
A origin (`y=0`) into the fitter. Assert the baseline-derived drawing origin
centers the actual ink without any `inkYCorrection` field.

- [ ] **Step 2: Add CoreText native measurement tests**

Create a `CTLine` for exact `A`/`한` font candidates. Assert
`kCTLineBoundsUseGlyphPathBounds` returns the negative Korean origin seen in
the native evidence and that glyph size differs from typographic bounds.

- [ ] **Step 3: Add offscreen pixel-containment regression**

Render every Cat artwork frame for both sources and both directions (36 native
cases total) into separate body-mask and glyph bitmaps. Use the production
CoreText helper and Cat compiled path constants. Fail when any nonzero glyph
alpha pixel is present where body-mask alpha is zero.

- [ ] **Step 4: Capture RED**

Run the pure and native render targets. Expected: compile failure for removed
offset-free/CoreText APIs or nonzero spill output from the old renderer path.

### Task 45: Draw from CoreText Glyph Bounds

**Files:**
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.h`
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.mm`

- [ ] **Step 1: Remove arbitrary correction**

Keep only source/state maximum font sizes. Replace
`MacInputIndicatorLabelMeasurement.origin/size` with actual glyph-path ink
bounds relative to a CoreText baseline; retain typographic bounds only for
diagnostic/native tests.

- [ ] **Step 2: Add shared CoreText helpers**

Create attributed `CTLine`, query `kCTLineBoundsUseGlyphPathBounds`, fit the
font until actual ink fits the inner label target, and draw at
`targetInkOrigin - measuredInkOrigin`. Expose the native helper for the render
test.

- [ ] **Step 3: Clip to the actual Cat torso silhouette**

For Cat only, intersect the existing label-box clip with the compiled near/far
silhouette path. Dog/Turtle continue their existing 8 pt path.

### Task 46: Verify, Package, and Audit

- [ ] **Step 1: Run focused and native gates**

Run InputIndicatorCoreTextRenderTests, InputIndicatorGeometryTests, policy,
settings, translation, and panel build targets. Required pre-run output is the
native spill count, with `outsidePixels == 0` for all 36 cases.

- [ ] **Step 2: Run full Release**

Build all targets and run 157 eligible CTests with only
`OSXClipboardTests` and `OSXKeyStateTests` excluded.

- [ ] **Step 3: Commit and package**

Commit docs and implementation separately with the standard trailer. Build
`build/pointer-badge-coretext/deskflow-2.0.9.0-macos-arm64.dmg`, record hash and
signature, and do not install.

- [ ] **Step 4: Audit final state**

Expected: clean ancestry from
`9c34e2b625877cf2a52cf415e8a8a8255c49a35b`, zero-spill native output, no
arbitrary baseline offset, no push/tag/release/merge/PR/install.
