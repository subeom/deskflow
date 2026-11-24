# Always-On Pointer Input Badge Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the transient activity-driven input indicator with an always-on `A`/`한` badge that follows the native mouse pointer while the pointer is local.

**Architecture:** Keep the existing privacy-safe `NSPanel`, exact TIS snapshot, and core local/off-computer subscription. Remove Accessibility and input-activity dependencies from `MacInputIndicatorController`, separate panel configuration from 60 Hz frame movement, and leave legacy IPC/activity/AX classes buildable but disconnected.

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
