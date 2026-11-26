# Mouse Cursor Input Indicator Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the macOS `한/A` badge follow a fresh local mouse-pointer activity/source transition instead of requiring an editable AX focus or using focus changes as an activation trigger.

**Architecture:** Keep the existing macOS CGEventTap relay, authenticated indicator IPC, TIS source monitor, non-activating `NSPanel`, pointer geometry, settings, and one-second freshness gate. Change the GUI policy/controller so AX `Unknown` and `NotEditable` are neutral for a valid candidate, while `SecureOrProtected`, identity mismatch, connection/local-state failure, unknown source, and stale events remain hard hide conditions.

**Tech Stack:** C++20, Objective-C++, Qt 6.12 Core/Widgets/Test, AppKit, Accessibility, Carbon TIS, CMake/Ninja, CTest, CPack DragNDrop, `hdiutil`, `codesign`, and `shasum`.

**Execution mode:** Inline in the current isolated worktree with the `executing-plans` skill. The implementation is approved for immediate task-by-task execution without another approval turn.

---

## File map

| File | Responsibility after the change |
|---|---|
| `src/lib/gui/input/InputIndicatorPolicy.h` | Define only real activity/source policy triggers and neutral AX states. |
| `src/lib/gui/input/InputIndicatorPolicy.cpp` | Fail-close global provenance; hide only secure/protected AX state, not Unknown/NotEditable. |
| `src/lib/gui/input/MacAccessibilityProbe.h` | Expose AX snapshot/PID identity semantics to pure tests. |
| `src/lib/gui/input/MacAccessibilityProbe.mm` | Preserve target PID when AX metadata is unavailable; distinguish missing role from known secure/not-editable role. |
| `src/lib/gui/input/MacInputIndicatorController.h` | Own trigger invalidation, source baseline, pending target PID, and narrow presentation/source test seams. |
| `src/lib/gui/input/MacInputIndicatorController.mm` | Accept only source/click/key activation; make focus/app changes invalidation-only; verify generation/PID/source before panel show. |
| `src/lib/platform/OSXInputSource.h` | Classify exact IDs and expose a pure source-transition eligibility helper. |
| `src/lib/platform/OSXInputSource.mm` | Keep exact TIS reads and add only the pure transition decision. |
| `src/unittests/gui/input/InputIndicatorPolicyTests.h/.cpp` | Cover policy and controller activation, invalidation, source baseline, AX identity, and panel presentation without a real UI. |
| `src/unittests/gui/input/InputIndicatorGeometryTests.h/.cpp` | Guard pointer-only positioning, edge clamping, and invalid geometry regressions. |
| `src/unittests/platform/OSXInputSourceTests.h/.cpp` | Cover exact IDs and source baseline/duplicate/unknown transition decisions. |
| `src/unittests/gui/settings/InputIndicatorSettingsTests.cpp` | Assert the revised pointer/trigger/privacy hint. |
| `src/lib/gui/settings/SettingsPages.cpp` | Describe pointer activity and AX semantics accurately. |
| `translations/deskflow_ko.ts` | Keep the Korean localized setting text consistent with the new source string. |
| `docs/Configuration.md` | Describe source/click/key triggers and focus invalidation accurately. |
| `docs/superpowers/specs/2026-11-23-mouse-cursor-input-indicator-design.md` | Approved source of truth; do not revise unless implementation proves a contradiction. |

No changes are planned in `OSXComputer.mm`, `OSXInputIndicatorRelay`, `InputIndicatorIpc`, `MacInputIndicatorPanel.mm`, `InputIndicatorGeometry.cpp`, entitlements, CMake source lists, or dependencies. The existing event tap, IPC schema, `NSEvent.mouseLocation`, panel geometry, and privacy boundary stay intact.

## Validation constants

```bash
REPO="$(git rev-parse --show-toplevel)"
BASE=75e651bd3af791723d998d89a0bdcfa8dc018087
PLAN_HEAD=207b4261c41ae8fd48f4b09dd6af67aa59d24cea
QT_ROOT=/opt/homebrew/opt/qt
BUILD="$REPO/build"
SCRATCH="$BUILD/scratch"
PRIOR_DMG=/Users/subeomlee/.copilot/session-state/86886aba-406b-4ad8-b821-40729a6c8539/files/release-build/out-v2.0.9/deskflow-2.0.9-macos-arm64.dmg
PRIOR_SHA256=08e2ec5512c6f55bd752f837b35db160933eb7f04078a96fdddeb462712d210a
PRIOR_APP_CDHASH=7ed63a403dd4d8a9fef06ec9cbfb1e0fafe990f5
```

The `PRIOR_DMG` is an immutable comparison input outside the worktree. It may be mounted and read only. Do not copy it over, modify it, or touch `/Applications`.

## Task 1: Lock the activation policy with failing tests

**Files:**
- Modify: `src/unittests/gui/input/InputIndicatorPolicyTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorPolicyTests.cpp`
- Modify: `src/lib/gui/input/InputIndicatorPolicy.h`
- Modify: `src/lib/gui/input/InputIndicatorPolicy.cpp`

- [ ] **Step 1: Replace focus-trigger and editable-focus expectations with the approved policy matrix**

In `InputIndicatorPolicyTests.h`, remove these slots:

```cpp
void focusTriggerWithoutActivityShows();
void focusTriggerGenerationMismatchHides();
```

Add:

```cpp
void unknownAndNotEditableFocusCanShow_data();
void unknownAndNotEditableFocusCanShow();
void sourceTriggerWithoutActivityShows();
```

In `InputIndicatorPolicyTests.cpp`, define the data test:

```cpp
void InputIndicatorPolicyTests::unknownAndNotEditableFocusCanShow_data()
{
  QTest::addColumn<int>("focus");
  QTest::addColumn<int>("trigger");
  QTest::newRow("unknown-core")
      << static_cast<int>(EditableFocusState::Unknown)
      << static_cast<int>(InputIndicatorTrigger::CoreActivity);
  QTest::newRow("not-editable-core")
      << static_cast<int>(EditableFocusState::NotEditable)
      << static_cast<int>(InputIndicatorTrigger::CoreActivity);
  QTest::newRow("unknown-source")
      << static_cast<int>(EditableFocusState::Unknown)
      << static_cast<int>(InputIndicatorTrigger::SourceChanged);
  QTest::newRow("not-editable-source")
      << static_cast<int>(EditableFocusState::NotEditable)
      << static_cast<int>(InputIndicatorTrigger::SourceChanged);
}

void InputIndicatorPolicyTests::unknownAndNotEditableFocusCanShow()
{
  QFETCH(int, focus);
  QFETCH(int, trigger);
  auto input = safeInput();
  input.focus = static_cast<EditableFocusState>(focus);
  input.trigger = static_cast<InputIndicatorTrigger>(trigger);
  if (input.trigger == InputIndicatorTrigger::SourceChanged) {
    input.eventAge.reset();
  }

  QVERIFY(InputIndicatorPolicy::decide(
      input, std::chrono::milliseconds{1500}
  ).show);
}
```

Change `nonCoreTriggerSafetyHides_data` to build rows for `SourceChanged` only. Remove `not-editable` and `unknown-focus` as hiding conditions. Change `unsafeStateHides_data` and `unsafeStateHides` so `NotEditable` and `Unknown` are no longer rows. Keep `SecureOrProtected` in every trigger matrix.

- [ ] **Step 2: Run the policy target and capture the intended failure**

```bash
cmake --build "$BUILD" --config Release --parallel 8 --target InputIndicatorPolicyTests
QT_QPA_PLATFORM=offscreen ctest --test-dir "$BUILD/src/unittests" \
  -R '^InputIndicatorPolicyTests$' --output-on-failure
```

Expected before implementation: compile or assertion failure because `Unknown`/`NotEditable` still hide.

- [ ] **Step 3: Remove editable-focus gating and the focus trigger**

In `InputIndicatorPolicy.h`, remove:

```cpp
FocusChanged
FocusNotEditable
FocusUnknown
```

Keep:

```cpp
CoreActivity
SourceChanged
```

In `InputIndicatorPolicy.cpp`, delete the `NotEditable` and `Unknown` early returns. Keep the secure/protected branch exactly:

```cpp
if (input.focus == EditableFocusState::SecureOrProtected) {
  return {
      .reason = InputIndicatorVisibilityReason::FocusSecureOrProtected
  };
}
```

Keep `inputIndicatorTriggerRequiresActivity` true only for `CoreActivity`:

```cpp
return trigger == InputIndicatorTrigger::CoreActivity;
```

Remove the now-unreachable reason text entries for `FocusNotEditable` and `FocusUnknown`. Do not weaken the source, connection, permission, local, generation, dwell, or event-age checks.

- [ ] **Step 4: Run the policy target to green**

```bash
cmake --build "$BUILD" --config Release --parallel 8 --target InputIndicatorPolicyTests
QT_QPA_PLATFORM=offscreen ctest --test-dir "$BUILD/src/unittests" \
  -R '^InputIndicatorPolicyTests$' --output-on-failure
```

Expected: `100% tests passed`, `0` failures.

- [ ] **Step 5: Commit the policy change**

```bash
git add src/lib/gui/input/InputIndicatorPolicy.h \
        src/lib/gui/input/InputIndicatorPolicy.cpp \
        src/unittests/gui/input/InputIndicatorPolicyTests.h \
        src/unittests/gui/input/InputIndicatorPolicyTests.cpp
git commit -m "fix(input): allow uncertain cursor indicator focus" \
  -m "Co-authored-by: Copilot App <223556219+Copilot@users.noreply.github.com>"
```

## Task 2: Preserve AX target identity while allowing unavailable metadata

**Files:**
- Modify: `src/lib/gui/input/MacAccessibilityProbe.h`
- Modify: `src/lib/gui/input/MacAccessibilityProbe.mm`
- Modify: `src/unittests/gui/input/InputIndicatorPolicyTests.cpp`
- Modify: `src/unittests/gui/input/InputIndicatorPolicyTests.h`

- [ ] **Step 1: Add the failing matching-Unknown identity regression**

In `InputIndicatorPolicyTests.h`, add:

```cpp
void accessibilitySnapshotKeepsUnknownMetadataForMatchingPid();
```

In `InputIndicatorPolicyTests.cpp`, add:

```cpp
void InputIndicatorPolicyTests::accessibilitySnapshotKeepsUnknownMetadataForMatchingPid()
{
  const MacAccessibilitySnapshot unknown{
      .state = EditableFocusState::Unknown,
      .applicationPid = 4242
  };

  const auto current = macAccessibilitySnapshotForFrontmostApplication(
      unknown, std::optional<int64_t>{4242}
  );

  QCOMPARE(current.state, EditableFocusState::Unknown);
  QCOMPARE(current.applicationPid, int64_t{4242});
}
```

Also keep and strengthen the stale-PID test: a stale snapshot must become `{Unknown, 0}` and never become a show candidate.

- [ ] **Step 2: Run the AX/policy target and confirm the current identity case still passes**

```bash
cmake --build "$BUILD" --config Release --parallel 8 --target InputIndicatorPolicyTests
QT_QPA_PLATFORM=offscreen ctest --test-dir "$BUILD/src/unittests" \
  -R '^InputIndicatorPolicyTests$' --output-on-failure
```

Expected: the new boundary test passes against existing helper semantics. The next step changes the producer so this boundary is reachable when a focused element or role is unavailable.

- [ ] **Step 3: Make unavailable AX metadata produce Unknown with the requested PID**

At every valid external application lookup failure in `MacAccessibilityProbe.mm`, return an Unknown snapshot carrying the requested PID instead of `{}`:

```cpp
return {
    .state = EditableFocusState::Unknown,
    .applicationPid = applicationPid
};
```

Apply that form to:

1. application element creation failure;
2. missing focused UI element;
3. missing/unsupported focused element type;
4. missing role metadata.

Do not apply it to an invalid PID, a not-trusted process, or a stale frontmost target.
If `AXUIElementGetPid` fails, or the focused element PID differs from the requested
application PID, return `{}`. That is an identity mismatch, not missing metadata.

Change role handling to this order:

```cpp
OwnedCF<CFTypeRef> role(copyAttribute(element, kAXRoleAttribute));
if (!role.get()) {
  return {
      .state = EditableFocusState::Unknown,
      .applicationPid = *elementPid
  };
}

OwnedCF<CFTypeRef> subrole(copyAttribute(element, kAXSubroleAttribute));
if (cfStringEquals(subrole.get(), kAXSecureTextFieldSubrole)) {
  return {
      .state = EditableFocusState::SecureOrProtected,
      .applicationPid = *elementPid
  };
}

if (!cfStringEquals(role.get(), kAXTextFieldRole) &&
    !cfStringEquals(role.get(), kAXTextAreaRole)) {
  return {
      .state = EditableFocusState::NotEditable,
      .applicationPid = *elementPid
  };
}
```

Keep enabled/focused metadata classification. Remove the `AXUIElementIsAttributeSettable(..., kAXValueAttribute, ...)` call. The indicator never needs a value settable property and must not read `kAXValueAttribute`.

`readFocusedElementForApplication` is responsible for converting metadata lookup
failures to `{Unknown, applicationPid}`. Do not add a blanket fallback at the end of
`readFocusedElement`: that would incorrectly convert a focused-element PID mismatch
or other stale identity failure into an eligible Unknown snapshot.

- [ ] **Step 4: Run AX/policy regression tests and scan for value reads**

```bash
cmake --build "$BUILD" --config Release --parallel 8 --target InputIndicatorPolicyTests
QT_QPA_PLATFORM=offscreen ctest --test-dir "$BUILD/src/unittests" \
  -R '^InputIndicatorPolicyTests$' --output-on-failure
rg -n 'AXUIElementCopyAttributeValue.*kAXValueAttribute|AXSelected|AXBoundsForRange|AXRange|clipboard|keycode' \
  src/lib/gui/input src/lib/platform/OSXInputIndicatorRelay.* src/lib/deskflow/ipc/InputIndicatorIpc.*
```

Expected: policy tests pass and the search has no matches in the indicator-specific path.

- [ ] **Step 5: Commit AX identity handling**

```bash
git add src/lib/gui/input/MacAccessibilityProbe.h \
        src/lib/gui/input/MacAccessibilityProbe.mm \
        src/unittests/gui/input/InputIndicatorPolicyTests.h \
        src/unittests/gui/input/InputIndicatorPolicyTests.cpp
git commit -m "fix(input): preserve AX target identity" \
  -m "Co-authored-by: Copilot App <223556219+Copilot@users.noreply.github.com>"
```

## Task 3: Make focus invalidation-only and source transitions exact

**Files:**
- Modify: `src/lib/gui/input/MacInputIndicatorController.h`
- Modify: `src/lib/gui/input/MacInputIndicatorController.mm`
- Modify: `src/lib/platform/OSXInputSource.h`
- Modify: `src/lib/platform/OSXInputSource.mm`
- Modify: `src/unittests/gui/input/InputIndicatorPolicyTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorPolicyTests.cpp`
- Modify: `src/unittests/platform/OSXInputSourceTests.h`
- Modify: `src/unittests/platform/OSXInputSourceTests.cpp`

- [ ] **Step 1: Add source-transition helper tests before implementation**

In `OSXInputSourceTests.h`, add:

```cpp
void transitionActivationRequiresAChangedKnownSource_data();
void transitionActivationRequiresAChangedKnownSource();
```

In `OSXInputSourceTests.cpp`, add exact data:

```cpp
using Transition = std::pair<InputIndicatorSource, InputIndicatorSource>;
QTest::addColumn<int>("previous");
QTest::addColumn<int>("current");
QTest::addColumn<bool>("activate");
QTest::newRow("baseline-abc") << LatinABC << LatinABC << false;
QTest::newRow("duplicate-korean") << Korean2Set << Korean2Set << false;
QTest::newRow("abc-to-korean") << LatinABC << Korean2Set << true;
QTest::newRow("korean-to-abc") << Korean2Set << LatinABC << true;
QTest::newRow("unknown-to-abc") << Unknown << LatinABC << true;
QTest::newRow("abc-to-unknown") << LatinABC << Unknown << false;
```

Test the not-yet-defined function:

```cpp
QCOMPARE(
    deskflow::platform::shouldActivateMacInputSourceTransition(previous, current),
    activate
);
```

- [ ] **Step 2: Add controller behavior tests and narrow test seams**

Extend `MacInputIndicatorController::Dependencies` only with callbacks needed by the tests:

```cpp
std::function<deskflow::platform::OSXInputSourceSnapshot()> inputSource;
std::function<bool(const MacInputIndicatorPanelOptions &)> panelShow;
std::function<void()> panelHide;
```

Forward-declare `OSXInputSourceSnapshot` rather than exposing Objective-C++ types.

Add private methods:

```cpp
void focusChanged();
void inputSourceChanged();
```

Add test access:

```cpp
static void focusChanged(MacInputIndicatorController &controller);
static void inputSourceChanged(MacInputIndicatorController &controller);
static void requestAccessibility(MacInputIndicatorController &controller);
static void accessibilityCompleted(
    MacInputIndicatorController &controller,
    uint64_t generation,
    const MacAccessibilitySnapshot &snapshot
);
static void setSource(MacInputIndicatorController &controller, InputIndicatorSource source);
static uint64_t generation(const MacInputIndicatorController &controller);
static std::optional<int64_t> pendingTargetPid(
    const MacInputIndicatorController &controller
);
static std::optional<InputIndicatorUiTrigger> visibleTrigger(...);
```

Add these controller slots:

```cpp
void focusChangeInvalidatesWithoutQueueingProbe();
void duplicateSourceNotificationDoesNotQueueProbe();
void realSourceTransitionQueuesProbe();
void unknownSourceTransitionHidesWithoutProbe();
void unknownFocusCanShowAfterFreshCoreActivity();
void notEditableFocusCanShowAfterFreshCoreActivity();
void secureFocusNeverShows();
void ownApplicationNeverRequestsProbe();
void staleProbeGenerationNeverShows();
void mismatchedSnapshotPidNeverShows();
void invalidActivityDoesNotCancelSourceTrigger();
void focusChangeHidesVisibleCandidate();
```

Each show test injects:

```cpp
Dependencies dependencies;
dependencies.frontmostApplicationPid = [] {
  return std::optional<int64_t>{31337};
};
dependencies.accessibilityTrusted = [] { return true; };
dependencies.inputSource = [] {
  return OSXInputSourceSnapshot{
      .identifier = "com.apple.keylayout.ABC",
      .source = Source::LatinABC
  };
};
dependencies.probeRequest = [&requested](uint64_t generation, std::optional<int64_t> pid) {
  requested = std::pair{generation, pid};
};
dependencies.panelShow = [&shown](const MacInputIndicatorPanelOptions &options) {
  shown.push_back(options);
  return true;
};
dependencies.panelHide = [&hidden] { ++hidden; };
```

For a click/key show, call `inputIndicatorActivityReceived` with a current `mach_absolute_time()` timestamp, invoke `requestAccessibility`, call `accessibilityCompleted` with the requested generation and a matching `MacAccessibilitySnapshot`, and assert `shown.size() == 1`, source is `LatinABC`, position is the configured cursor-relative setting, and `visibleTrigger` is `CoreActivity`.

For focus invalidation, first create a pending/source or visible candidate, then invoke `focusChanged`; assert no pending/visible trigger, no new probe request, the panel was hidden, and the generation advanced.

- [ ] **Step 3: Run the two focused targets and capture the compile failures**

```bash
cmake --build "$BUILD" --config Release --parallel 8 --target \
  InputIndicatorPolicyTests OSXInputSourceTests
```

Expected before implementation:

1. `FocusedElementChanged` still queues a probe;
2. `shouldActivateMacInputSourceTransition` is undefined;
3. test seams are absent.

- [ ] **Step 4: Implement source transition eligibility**

Add to `OSXInputSource.h`:

```cpp
bool shouldActivateMacInputSourceTransition(
    deskflow::input::InputIndicatorSource previous,
    deskflow::input::InputIndicatorSource current
);
```

Implement in `OSXInputSource.mm`:

```cpp
return previous != current &&
       current != deskflow::input::InputIndicatorSource::Unknown;
```

Do not change `classifyMacInputSourceId`; exact `com.apple.keylayout.ABC` and
`com.apple.inputmethod.Korean.2SetKorean` remain the only known values.

- [ ] **Step 5: Implement trigger and invalidation semantics**

In `MacInputIndicatorController.h`, remove `FocusedElementChanged` from `InputIndicatorUiTrigger`; retain only:

```cpp
CoreActivity
InputSourceChanged
```

Add:

```cpp
std::optional<int64_t> m_pendingTargetPid;
std::function<deskflow::platform::OSXInputSourceSnapshot()> m_inputSource;
std::function<bool(const MacInputIndicatorPanelOptions &)> m_panelShow;
std::function<void()> m_panelHide;
```

In `createMonitors`, capture the current source before attaching the source monitor:

```cpp
m_source = m_inputSource().source;
m_focusMonitor = std::make_unique<MacInputFocusMonitor>(
    [this] { focusChanged(); }
);
m_sourceMonitor = std::make_unique<deskflow::platform::OSXInputSourceMonitor>(
    [this] { inputSourceChanged(); }
);
```

In `inputSourceChanged`:

```cpp
const auto previous = m_source;
const auto current = m_inputSource().source;
m_source = current;
if (current == InputIndicatorSource::Unknown) {
  ++m_generation;
  m_pendingTrigger.reset();
  m_pendingTargetPid.reset();
  hide();
  return;
}
if (!shouldActivateMacInputSourceTransition(previous, current)) {
  return;
}
trigger(InputIndicatorUiTrigger::InputSourceChanged);
```

In `focusChanged`:

```cpp
++m_generation;
m_pendingTrigger.reset();
m_pendingTargetPid.reset();
m_probeTimer.stop();
hide();
```

Do not call `trigger` from focus/app changes. No mouse-move monitor is added.

Default `m_panelShow` and `m_panelHide` call `m_panel->show` and `m_panel->hide`. `hide` and `accessibilityCompleted` use those callbacks; production behavior is identical.

In `requestAccessibility`, after selecting the external target:

```cpp
m_pendingTargetPid = targetPid;
m_probeRequest(m_generation, targetPid);
```

At `accessibilityCompleted`, reject and hide unless:

1. callback generation equals `m_generation`;
2. `m_pendingTrigger` exists;
3. `m_pendingTargetPid` exists;
4. snapshot application PID equals `m_pendingTargetPid`;
5. the injected/current frontmost PID still equals the pending PID;
6. the source completion snapshot remains the accepted known source.

Use `m_frontmostApplicationPid()` for both request and completion so production and controller tests exercise the same dependency. Preserve source candidates when only an invalid activity is received:

```cpp
if (pending/visible trigger is CoreActivity) { cancel; }
else { return; }
```

Reset `m_pendingTargetPid` with every path that resets `m_pendingTrigger`. Reset it on local, disconnect, setting, source-unknown, focus, and superseding trigger.

- [ ] **Step 6: Run focused source/controller tests**

```bash
cmake --build "$BUILD" --config Release --parallel 8 --target \
  InputIndicatorPolicyTests OSXInputSourceTests
QT_QPA_PLATFORM=offscreen ctest --test-dir "$BUILD/src/unittests" \
  -R '^(InputIndicatorPolicyTests|OSXInputSourceTests)$' --output-on-failure
```

Expected: all policy/controller/source tests pass, including no focus-only show, source baseline dedupe, real transition show, Unknown/NotEditable show, and secure/PID/generation hard hides.

- [ ] **Step 7: Commit controller/source activation**

```bash
git add src/lib/gui/input/MacInputIndicatorController.h \
        src/lib/gui/input/MacInputIndicatorController.mm \
        src/lib/platform/OSXInputSource.h \
        src/lib/platform/OSXInputSource.mm \
        src/unittests/gui/input/InputIndicatorPolicyTests.h \
        src/unittests/gui/input/InputIndicatorPolicyTests.cpp \
        src/unittests/platform/OSXInputSourceTests.h \
        src/unittests/platform/OSXInputSourceTests.cpp
git commit -m "fix(input): activate badge from real pointer events" \
  -m "Co-authored-by: Copilot App <223556219+Copilot@users.noreply.github.com>"
```

## Task 4: Guard pointer geometry, settings text, and configuration docs

**Files:**
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.cpp`
- Modify: `src/lib/gui/settings/SettingsPages.cpp`
- Modify: `src/unittests/gui/settings/InputIndicatorSettingsTests.cpp`
- Modify: `translations/deskflow_ko.ts`
- Modify: `docs/Configuration.md`

- [ ] **Step 1: Add pointer-only geometry regressions**

Rename `bottomRightOffsetsCursor` to `bottomRightOffsetsPointer`. Add:

```cpp
void changingFocusAnchorDoesNotExist();
void everyCornerRemainsPointerRelative_data();
void everyCornerRemainsPointerRelative();
```

The test does not receive a focus/caret point because the production helper accepts only `InputIndicatorPoint cursor`, badge, and screens. Assert with identical screen/badge:

1. `CursorBottomRight` is `cursor + gap` horizontally and `cursor - badgeHeight - gap` vertically.
2. `CursorTopRight` is `cursor + gap` horizontally and `cursor + gap` vertically.
3. `CursorBottomLeft` is `cursor - badgeWidth - gap` horizontally.
4. `CursorTopLeft` is `cursor - badgeWidth - gap` and `cursor + gap`.
5. edge clamping never introduces a caret/focus-dependent offset.

`changingFocusAnchorDoesNotExist` is a compile-time contract encoded by a comment and the exact function call:

```cpp
const auto result = positionInputIndicator(
    {.x = 400, .y = 500}, badge, oneScreen(),
    InputIndicatorPosition::CursorTopRight, 8, 8
);
```

No function parameter exists for a caret or focus frame.

- [ ] **Step 2: Update user-facing semantics**

Replace the switch description in `SettingsPages.cpp` with:

```cpp
tx(QT_TRANSLATE_NOOP(
    "SettingsPages",
    "한/A briefly beside the mouse pointer after input activity"
))
```

Replace the safety hint with:

```cpp
tx(QT_TRANSLATE_NOOP(
    "SettingsPages",
    "Shown only after a local input-source change, click, or key activity. "
    "Password and protected fields, and unavailable Accessibility permission, "
    "keep it hidden; other unrecognized focus states may still show it."
))
```

Update the two matching Korean `<source>` and `<translation>` entries in
`translations/deskflow_ko.ts`:

```xml
<source>한/A briefly beside the mouse pointer after input activity</source>
<translation>입력 활동 후 마우스 포인터 옆 한/A 짧게 표시</translation>
```

and:

```xml
<translation>로컬 입력 소스 변경, 클릭, 키 입력 뒤에만 표시. Accessibility 권한이 없거나 비밀번호/보호 입력란이면 숨김. 그 외 인식되지 않은 초점에서는 표시 가능.</translation>
```

In `InputIndicatorSettingsTests.cpp`, assert:

1. the hint contains `Password`.
2. the hint contains `input-source`.
3. the hint no longer contains `field recognition unavailable: indicator hidden`.

- [ ] **Step 3: Replace the contradictory configuration paragraph**

Replace the input-indicator paragraph in `docs/Configuration.md` with the following
contract:

1. exact ABC/Korean TIS IDs only;
2. badge at `NSEvent.mouseLocation`, never at caret;
3. only real local source transition, click, or key activity;
4. no setting-enable, mouse move, hover, focus, or app activation trigger;
5. AX Unknown/NotEditable is neutral for a valid activity;
6. secure/protected, own app, off-computer, untrusted, disconnected, unknown source, and stale/mismatched events hide;
7. no text/value/selection/caret/keycode/clipboard reads;
8. short configured dwell.

- [ ] **Step 4: Run focused geometry/settings tests**

```bash
cmake --build "$BUILD" --config Release --parallel 8 --target \
  InputIndicatorGeometryTests InputIndicatorSettingsTests
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH/home" \
  XDG_CACHE_HOME="$SCRATCH/cache" XDG_CONFIG_HOME="$SCRATCH/config" \
  TMPDIR="$SCRATCH/tmp" \
  ctest --test-dir "$BUILD/src/unittests" \
  -R '^(InputIndicatorGeometryTests|InputIndicatorSettingsTests)$' \
  --output-on-failure
```

Expected: geometry and settings tests pass.

- [ ] **Step 5: Check product/user wording drift and commit**

```bash
! rg -n 'field recognition unavailable|real focus change|focused editable field' \
  src/lib/gui/settings/SettingsPages.cpp docs/Configuration.md translations/deskflow_ko.ts
git diff --check
git add src/unittests/gui/input/InputIndicatorGeometryTests.h \
        src/unittests/gui/input/InputIndicatorGeometryTests.cpp \
        src/lib/gui/settings/SettingsPages.cpp \
        src/unittests/gui/settings/InputIndicatorSettingsTests.cpp \
        translations/deskflow_ko.ts \
        docs/Configuration.md
git commit -m "docs(input): align pointer indicator behavior" \
  -m "Co-authored-by: Copilot App <223556219+Copilot@users.noreply.github.com>"
```

## Task 5: Run the focused and complete Release macOS gates

**Files:**
- Modify: none unless a failing gate proves a change-caused defect.

- [ ] **Step 1: Configure a clean Release Qt 6.12 build with tests and warnings as errors**

```bash
rm -rf "$BUILD"
mkdir -p "$SCRATCH/home" "$SCRATCH/cache" "$SCRATCH/config" "$SCRATCH/tmp"
test "$(qmake6 -query QT_VERSION)" = "6.12.0"
test -f "$QT_ROOT/lib/cmake/Qt6/Qt6Config.cmake"
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
grep -E '^(Qt6_DIR|CMAKE_BUILD_TYPE|CMAKE_COMPILE_WARNING_AS_ERROR|CMAKE_OSX_ARCHITECTURES|CMAKE_PROJECT_VERSION)[:=]' \
  "$BUILD/CMakeCache.txt"
```

Expected: Qt 6.12.0, Release, arm64, deployment target 14, warnings as errors ON.

- [ ] **Step 2: Build focused production and indicator targets**

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

Expected: all named targets build with no warnings and no use of `all`/`run_tests`.

- [ ] **Step 3: Run the focused CTest gate**

```bash
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH/home" \
  XDG_CACHE_HOME="$SCRATCH/cache" XDG_CONFIG_HOME="$SCRATCH/config" \
  TMPDIR="$SCRATCH/tmp" \
  ctest --test-dir "$BUILD/src/unittests" \
  -R '^(InputIndicatorPolicyTests|InputIndicatorGeometryTests|InputIndicatorIpcTests|OSXInputSourceTests|OSXInputIndicatorRelayTests|CoreProcessInputIndicatorTests|InputIndicatorSettingsTests)$' \
  --output-on-failure
```

Expected: seven focused executables pass with zero failures. The added Qt test slots run inside the existing executables.

- [ ] **Step 4: Build every registered CTest executable target**

Use CTest's configured names to build an explicit target list, excluding `GitVersionTests`, which is a CMake script rather than an executable:

```bash
ctest --test-dir "$BUILD/src/unittests" -N -V > "$BUILD/ctest-list.txt"
TEST_TARGETS="$(sed -n 's/^ *Test #[0-9][0-9]*: //p' "$BUILD/ctest-list.txt" \
  | grep -v '^GitVersionTests$' | sort -u | tr '\n' ' ')"
test -n "$TEST_TARGETS"
cmake --build "$BUILD" --config Release --parallel 8 --target $TEST_TARGETS
```

Record:

```bash
printf '%s\n' "$TEST_TARGETS" | tr ' ' '\n' | sed '/^$/d' > "$BUILD/test-targets.txt"
wc -l "$BUILD/test-targets.txt"
```

Expected: all configured executable test targets build; count is recorded, not guessed.

- [ ] **Step 5: Run the complete macOS test gate with exactly two exclusions**

```bash
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH/home" \
  XDG_CACHE_HOME="$SCRATCH/cache" XDG_CONFIG_HOME="$SCRATCH/config" \
  TMPDIR="$SCRATCH/tmp" \
  ctest --test-dir "$BUILD/src/unittests" \
  -E '^(OSXClipboardTests|OSXKeyStateTests)$' \
  --output-on-failure
```

Also record:

```bash
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH/home" \
  XDG_CACHE_HOME="$SCRATCH/cache" XDG_CONFIG_HOME="$SCRATCH/config" \
  TMPDIR="$SCRATCH/tmp" \
  ctest --test-dir "$BUILD/src/unittests" \
  -N -E '^(OSXClipboardTests|OSXKeyStateTests)$'
```

Expected: the 158 established registered tests plus new test slots inside them all pass; only
`OSXClipboardTests` and `OSXKeyStateTests` are excluded. If test registration changes, record the actual count and prove the only exclusions are those two names.

- [ ] **Step 6: Re-run a changed test until green if needed**

If a failure is from indicator code, docs/settings wording, stale target handling, or a test seam added here, fix it and rerun the smallest failed target first, then repeat focused and full CTest. If a failure is unrelated, preserve the complete output and do not alter unrelated code.

## Task 6: Produce and verify an uninstalled 2.0.9 package candidate

**Files:**
- Modify: none.
- Output: `build/deskflow-2.0.9-macos-arm64.dmg` and its verification text.

- [ ] **Step 1: Re-fetch the fork and reconfirm the branch before packaging**

```bash
git fetch fork
test "$(git remote get-url fork)" = "https://github.com/subeom/deskflow.git"
test "$(git rev-parse fork/fix/input-indicator-activation)" = \
  "75e651bd3af791723d998d89a0bdcfa8dc018087"
git merge-base --is-ancestor "$BASE" HEAD
git status --short --branch
```

Expected: exact fork URL/ref, implementation commits descendant from the approved base, and no tracked unstaged/staged change. Do not create or remove a tag.

- [ ] **Step 2: Configure and package the candidate**

```bash
cmake -S "$REPO" -B "$BUILD/package" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$QT_ROOT" \
  -DCMAKE_COMPILE_WARNING_AS_ERROR=ON \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14 \
  -DBUILD_TESTS=OFF \
  -DSKIP_BUILD_TESTS=ON \
  -DBUILD_INSTALLER=ON \
  -DBUILD_DEV_DOCS=OFF \
  -DPACKAGE_VERSION_LABEL=2.0.9
cmake --build "$BUILD/package" --config Release --parallel 8 --target package
cp -p "$BUILD/package/deskflow-2.0.9-macos-arm64.dmg" \
  "$BUILD/deskflow-2.0.9-macos-arm64.dmg"
test -f "$BUILD/deskflow-2.0.9-macos-arm64.dmg"
```

`PACKAGE_VERSION_LABEL` controls the exact package name. Git-derived bundle version remains source-controlled by repository tags, and no tag is created. Report the actual `CFBundleShortVersionString`; do not claim it is 2.0.9.0 unless the mounted app proves it.

- [ ] **Step 3: Verify DMG bytes and structure**

```bash
shasum -a 256 "$BUILD/deskflow-2.0.9-macos-arm64.dmg" \
  | tee "$BUILD/deskflow-2.0.9-macos-arm64.sha256"
hdiutil verify "$BUILD/deskflow-2.0.9-macos-arm64.dmg"
mkdir -p "$BUILD/mount-new" "$BUILD/mount-prior"
hdiutil attach -readonly -nobrowse -noautoopen \
  -mountpoint "$BUILD/mount-new" \
  "$BUILD/deskflow-2.0.9-macos-arm64.dmg"
hdiutil attach -readonly -nobrowse -noautoopen \
  -mountpoint "$BUILD/mount-prior" \
  "$PRIOR_DMG"
```

Expected prior hash immediately before mount:

```bash
test "$(shasum -a 256 "$PRIOR_DMG" | awk '{print $1}')" = "$PRIOR_SHA256"
```

- [ ] **Step 4: Verify app/core signatures and designated requirements**

```bash
NEW_APP="$BUILD/mount-new/Deskflow.app"
PRIOR_APP="$BUILD/mount-prior/Deskflow.app"
codesign --verify --deep --strict "$NEW_APP"
codesign --verify --deep --strict "$NEW_APP/Contents/MacOS/deskflow-core"
codesign -d -r- "$NEW_APP" 2>&1 | tee "$BUILD/new-designated-requirement.txt"
codesign -dvvv "$NEW_APP" 2>&1 | tee "$BUILD/new-codesign.txt"
codesign -d -r- "$NEW_APP/Contents/MacOS/deskflow-core" 2>&1 \
  | tee "$BUILD/new-core-designated-requirement.txt"
codesign -dvvv "$NEW_APP/Contents/MacOS/deskflow-core" 2>&1 \
  | tee "$BUILD/new-core-codesign.txt"
```

Extract:

1. app and core CDHash;
2. identifier and certificate leaf or ad-hoc hash requirement;
3. signature Authority and flags;
4. full designated requirement;
5. bundle `CFBundleShortVersionString`;
6. bundle and core architecture.

Run the same read-only commands for `$PRIOR_APP`, save `prior-codesign.txt`, and compare. Expected prior app CDHash is exactly `$PRIOR_APP_CDHASH`. A content change must change the new CDHash. If the normal signing identity is unavailable, an ad-hoc `cdhash H"..."` designated requirement is valid for the uninstalled candidate and the changed requirement must be reported, not hidden.

- [ ] **Step 5: Verify payload and mounted version**

```bash
plutil -p "$NEW_APP/Contents/Info.plist" \
  | tee "$BUILD/new-info-plist.txt"
file "$NEW_APP/Contents/MacOS/Deskflow" \
     "$NEW_APP/Contents/MacOS/deskflow-core"
lipo -archs "$NEW_APP/Contents/MacOS/Deskflow"
lipo -archs "$NEW_APP/Contents/MacOS/deskflow-core"
find "$NEW_APP/Contents" -maxdepth 4 -type f -perm -111 -print \
  | sort | tee "$BUILD/new-executables.txt"
```

Check:

1. no unexpected executable or nested application;
2. arm64 for the app/core;
3. no input-indicator IPC field beyond existing kind/sequence/timestamp;
4. no unexpected framework/rpath addition caused by this change.

- [ ] **Step 6: Detach read-only mounts**

```bash
hdiutil detach "$BUILD/mount-new"
hdiutil detach "$BUILD/mount-prior"
test ! -e "$BUILD/mount-new/Deskflow.app"
test ! -e "$BUILD/mount-prior/Deskflow.app"
```

Never install or copy the mounted app to `/Applications`.

## Task 7: Final repository and handoff verification

- [ ] **Step 1: Confirm commit series and trailers**

```bash
git --no-pager log --format='%h %s%n%b' "$PLAN_HEAD"..HEAD
git --no-pager diff --stat "$BASE"..HEAD
git --no-pager diff --name-only "$BASE"..HEAD
git diff --check "$BASE"..HEAD
git status --short --branch
```

Expected:

1. implementation changes are committed;
2. every commit has the required Co-authored-by trailer;
3. only planned source/test/docs/translation files changed;
4. worktree is clean except ignored `build` output.

- [ ] **Step 2: Confirm privacy and trigger invariants by source scan**

```bash
rg -n 'FocusedElementChanged|FocusChanged' \
  src/lib/gui/input src/unittests/gui/input || true
rg -n 'kAXValueAttribute|AXSelected|AXBoundsForRange|AXRangeFor|AXPosition|AXSize' \
  src/lib/gui/input || true
rg -n 'clipboard|pasteboard|keycode|selected text|field value' \
  src/lib/gui/input src/lib/deskflow/ipc/InputIndicatorIpc.* || true
rg -n 'NSEvent\\.mouseLocation|positionInputIndicator' \
  src/lib/gui/input/MacInputIndicatorPanel.mm \
  src/lib/gui/input/InputIndicatorGeometry.cpp
```

Expected first three scans: no activation or privacy violation; last scan shows the only position source is `NSEvent.mouseLocation` plus existing geometry.

- [ ] **Step 3: Report exact evidence**

Report:

1. branch and all implementation commit hashes;
2. exact parent/base and clean worktree;
3. focused executable count and full CTest pass count with the two exclusions;
4. DMG full path, byte count, SHA-256;
5. mounted app/core architectures, version fields, CDHash, and designated requirement;
6. prior candidate hash/CDHash/requirement comparison;
7. proof that no `/Applications` modification, push, merge, tag, release, PR, or install occurred;
8. residual live AX secure-field risk and the manual matrix still required.

---

## Plan self-review criteria

- [ ] Every success criterion in the approved spec maps to a task and a named test.
- [ ] Unknown/NotEditable can show only after a valid source/click/key candidate.
- [ ] Focus/app change never creates an activation candidate.
- [ ] Source initialization and duplicate notifications never show.
- [ ] Secure/protected and every stale/identity/local/connection gate remains hard.
- [ ] No key/text/value/range/selection/caret/clipboard data path or log is added.
- [ ] Panel coordinates remain pointer-derived and bounded by existing geometry.
- [ ] Settings, translation, and `docs/Configuration.md` agree with implementation.
- [ ] The build/test command records actual counts and excludes only the same two tests.
- [ ] Package verification reads and compares; it never installs or changes `/Applications`.
- [ ] No dependency, entitlement, permission, event tap, IPC schema, push, tag, release, or PR action is introduced.
