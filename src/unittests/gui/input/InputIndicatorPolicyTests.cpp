/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "InputIndicatorPolicyTests.h"

#include "gui/input/InputIndicatorPolicy.h"
#ifdef Q_OS_MACOS
#include "gui/input/MacAccessibilityProbe.h"
#include "gui/input/MacInputIndicatorController.h"
#include "gui/input/MacInputIndicatorPanel.h"
#include "platform/OSXInputSource.h"
#include <mach/mach_time.h>
#endif

#include <QTest>

using namespace deskflow::gui::input;
using Source = deskflow::input::InputIndicatorSource;

#ifdef Q_OS_MACOS
struct MacInputIndicatorControllerTestAccess
{
  static std::string sourceIdentifier(Source source)
  {
    switch (source) {
    case Source::LatinABC:
      return "com.apple.keylayout.ABC";
    case Source::Korean2Set:
      return "com.apple.inputmethod.Korean.2SetKorean";
    case Source::Unknown:
      return "com.apple.inputmethod.Unsupported";
    }
  }

  static void arm(
      MacInputIndicatorController &controller,
      Source source = Source::LatinABC
  )
  {
    controller.m_effectiveEnabled = true;
    controller.m_isOnComputer = true;
    controller.m_pointerMonitorComplete = true;
    controller.m_sourceSnapshot = {
        .identifier = sourceIdentifier(source),
        .source = source
    };
    controller.refreshVisibility(true);
  }

  static void inputSourceChanged(
      MacInputIndicatorController &controller
  )
  {
    controller.inputSourceChanged();
  }

  static void pointerTick(MacInputIndicatorController &controller)
  {
    controller.movePointerBadge();
  }

  static bool renderTimerActive(
      const MacInputIndicatorController &controller
  )
  {
    return controller.m_renderTimer.isActive();
  }

  static int renderTimerInterval(
      const MacInputIndicatorController &controller
  )
  {
    return controller.m_renderTimer.interval();
  }

  static bool fallbackTimerActive(
      const MacInputIndicatorController &controller
  )
  {
    return controller.m_fallbackTimer.isActive();
  }

  static int fallbackTimerInterval(
      const MacInputIndicatorController &controller
  )
  {
    return controller.m_fallbackTimer.interval();
  }

  static void renderTick(MacInputIndicatorController &controller)
  {
    controller.movePointerBadge();
  }

  static void setEnabled(
      MacInputIndicatorController &controller, bool enabled
  )
  {
    controller.setEffectiveEnabled(enabled);
  }

  static void setCompanion(
      MacInputIndicatorController &controller,
      InputIndicatorCompanion companion
  )
  {
    controller.m_companion = companion;
    controller.refreshVisibility(true);
  }

  static void enableWithMonitor(
      MacInputIndicatorController &controller
  )
  {
    controller.setEffectiveEnabled(true);
    controller.refreshVisibility(true);
  }

  static bool sourceMonitorActive(
      const MacInputIndicatorController &controller
  )
  {
    return controller.m_sourceMonitor != nullptr;
  }

  static bool panelActive(
      const MacInputIndicatorController &controller
  )
  {
    return controller.m_panel != nullptr;
  }
};
#endif

namespace {

#ifdef Q_OS_MACOS
struct PersistentControllerHarness
{
  Source source = Source::LatinABC;
  std::string identifier = "com.apple.keylayout.ABC";
  std::vector<MacInputIndicatorPanelOptions> presentations;
  int moves = 0;
  int hides = 0;
  bool showResult = true;
  bool moveResult = true;
  bool moveAnimating = false;
  bool moveReducedMotion = false;
  int moveFrameInterval = 0;
  bool pointerMonitorComplete = true;
  int pointerMonitorStarts = 0;
  int pointerMonitorStops = 0;
  std::function<void()> pointerMoved;

  MacInputIndicatorController::Dependencies dependencies()
  {
    MacInputIndicatorController::Dependencies result;
    result.inputSource = [this] {
      return deskflow::platform::OSXInputSourceSnapshot{
          .identifier = identifier, .source = source
      };
    };
    result.panelShow =
        [this](const MacInputIndicatorPanelOptions &options) {
          presentations.push_back(options);
          return showResult;
        };
    result.panelMove = [this](InputIndicatorPosition) {
      ++moves;
      return MacInputIndicatorPanelMoveResult{
          .visible = moveResult,
          .animating = moveAnimating,
          .reducedMotion = moveReducedMotion,
          .frameIntervalMilliseconds = moveFrameInterval,
      };
    };
    result.panelHide = [this] { ++hides; };
    result.pointerMonitorStart =
        [this](std::function<void()> changed) {
          ++pointerMonitorStarts;
          pointerMoved = std::move(changed);
          return MacInputPointerMonitorHandle{
              .complete = pointerMonitorComplete,
              .stop = [this] { ++pointerMonitorStops; },
          };
        };
    return result;
  }
};
#endif

InputIndicatorPolicyInput safeInput()
{
  return {
      .enabled = true,
      .accessibilityTrusted = true,
      .coreConnected = true,
      .isPrimary = true,
      .isOnComputer = true,
      .screenKnown = true,
      .focus = EditableFocusState::SafeEditable,
      .source = Source::LatinABC,
      .probeGeneration = 7,
      .activityGeneration = 7,
      .eventAge = std::chrono::milliseconds{40},
      .maximumEventAge = std::chrono::milliseconds{
          deskflow::input::kInputIndicatorMaximumEventAgeMilliseconds
      },
  };
}

} // namespace

void InputIndicatorPolicyTests::safeStateShowsKnownSource()
{
  const auto decision = InputIndicatorPolicy::decide(safeInput(), std::chrono::milliseconds{1500});

  QVERIFY(decision.show);
  QCOMPARE(decision.source, Source::LatinABC);
  QCOMPARE(decision.dwell, std::chrono::milliseconds{1500});
}

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

void InputIndicatorPolicyTests::sourceTriggerWithoutActivityShows()
{
  auto input = safeInput();
  input.trigger = InputIndicatorTrigger::SourceChanged;
  input.eventAge.reset();

  QVERIFY(InputIndicatorPolicy::decide(
      input, std::chrono::milliseconds{1500}
  ).show);
}

void InputIndicatorPolicyTests::coreTriggerWithoutActivityHidesWithReason()
{
  auto input = safeInput();
  input.trigger = InputIndicatorTrigger::CoreActivity;
  input.eventAge.reset();

  const auto decision = InputIndicatorPolicy::decide(
      input, std::chrono::milliseconds{1500}
  );

  QVERIFY(!decision.show);
  QCOMPARE(
      decision.reason, InputIndicatorVisibilityReason::NoRecentActivity
  );
}

void InputIndicatorPolicyTests::nonCoreTriggerSafetyHides_data()
{
  QTest::addColumn<int>("trigger");
  QTest::addColumn<int>("condition");

  const auto trigger = static_cast<int>(
      InputIndicatorTrigger::SourceChanged
  );
  QTest::newRow("source-disabled") << trigger << 0;
  QTest::newRow("source-no-permission") << trigger << 1;
  QTest::newRow("source-core-disconnected") << trigger << 2;
  QTest::newRow("source-not-primary") << trigger << 3;
  QTest::newRow("source-off-computer") << trigger << 4;
  QTest::newRow("source-screen-unknown") << trigger << 5;
  QTest::newRow("source-secure-focus") << trigger << 6;
  QTest::newRow("source-unknown-source") << trigger << 7;
}

void InputIndicatorPolicyTests::nonCoreTriggerSafetyHides()
{
  QFETCH(int, trigger);
  QFETCH(int, condition);
  auto input = safeInput();
  input.trigger = static_cast<InputIndicatorTrigger>(trigger);
  input.eventAge.reset();

  switch (condition) {
  case 0:
    input.enabled = false;
    break;
  case 1:
    input.accessibilityTrusted = false;
    break;
  case 2:
    input.coreConnected = false;
    break;
  case 3:
    input.isPrimary = false;
    break;
  case 4:
    input.isOnComputer = false;
    break;
  case 5:
    input.screenKnown = false;
    break;
  case 6:
    input.focus = EditableFocusState::SecureOrProtected;
    break;
  case 7:
    input.source = Source::Unknown;
    break;
  default:
    QFAIL("unknown condition");
  }

  QVERIFY(!InputIndicatorPolicy::decide(
      input, std::chrono::milliseconds{1500}
  ).show);
}

void InputIndicatorPolicyTests::accessibilityProbeRejectsOwnApplication()
{
  QVERIFY(!macAccessibilityProbeTargetPid(
      std::optional<int64_t>{4242}, int64_t{4242}
  ));
  QVERIFY(!macAccessibilityProbeTargetPid(
      std::nullopt, int64_t{4242}
  ));
  QCOMPARE(
      QString::fromLatin1(inputIndicatorVisibilityReasonText(
          InputIndicatorVisibilityReason::OwnApplication
      )),
      QStringLiteral("own-application")
  );
}

void InputIndicatorPolicyTests::accessibilityProbeAcceptsExternalApplication()
{
  const auto target = macAccessibilityProbeTargetPid(
      std::optional<int64_t>{31337}, int64_t{4242}
  );

  QCOMPARE(target, std::optional<int64_t>{31337});
}

#ifdef Q_OS_MACOS
void InputIndicatorPolicyTests::accessibilityCandidateUsesFrontmostWhenSystemLookupFails()
{
  const auto candidates =
      macAccessibilityApplicationPidCandidates(std::nullopt, std::optional<int64_t>{4242});

  QCOMPARE(candidates, std::vector<int64_t>{4242});
}

void InputIndicatorPolicyTests::accessibilityCandidatesAvoidDuplicatePids()
{
  const auto candidates = macAccessibilityApplicationPidCandidates(
      std::optional<int64_t>{4242}, std::optional<int64_t>{4242}
  );

  QCOMPARE(candidates, std::vector<int64_t>{4242});
}

void InputIndicatorPolicyTests::accessibilityCandidatesIgnoreStaleSystemPid()
{
  const auto candidates = macAccessibilityApplicationPidCandidates(
      std::optional<int64_t>{31337}, std::optional<int64_t>{4242}
  );

  QCOMPARE(candidates, std::vector<int64_t>{4242});
}

void InputIndicatorPolicyTests::accessibilityCandidatesRequireFrontmostPid()
{
  const auto candidates = macAccessibilityApplicationPidCandidates(
      std::optional<int64_t>{31337}, std::nullopt
  );

  QVERIFY(candidates.empty());
}

void InputIndicatorPolicyTests::accessibilityUnavailableMetadataKeepsTargetIdentity()
{
  const auto snapshot = macUnknownAccessibilitySnapshot(4242);

  QCOMPARE(snapshot.state, EditableFocusState::Unknown);
  QCOMPARE(snapshot.applicationPid, int64_t{4242});

  const auto invalid = macUnknownAccessibilitySnapshot(0);
  QCOMPARE(invalid.state, EditableFocusState::Unknown);
  QCOMPARE(invalid.applicationPid, int64_t{0});
}

void InputIndicatorPolicyTests::accessibilitySnapshotRejectsStalePid()
{
  const MacAccessibilitySnapshot stale{
      .state = EditableFocusState::SafeEditable,
      .applicationPid = 31337
  };

  const auto current = macAccessibilitySnapshotForFrontmostApplication(
      stale, std::optional<int64_t>{4242}
  );

  QCOMPARE(current.state, EditableFocusState::Unknown);
  QCOMPARE(current.applicationPid, int64_t{0});
}

void InputIndicatorPolicyTests::accessibilitySnapshotRequiresFrontmostPid()
{
  const MacAccessibilitySnapshot snapshot{
      .state = EditableFocusState::SafeEditable,
      .applicationPid = 4242
  };

  const auto current =
      macAccessibilitySnapshotForFrontmostApplication(
          snapshot, std::nullopt
      );

  QCOMPARE(current.state, EditableFocusState::Unknown);
  QCOMPARE(current.applicationPid, int64_t{0});
}

void InputIndicatorPolicyTests::accessibilitySnapshotAcceptsMatchingFrontmostPid()
{
  const MacAccessibilitySnapshot snapshot{
      .state = EditableFocusState::SafeEditable,
      .applicationPid = 4242
  };

  const auto current =
      macAccessibilitySnapshotForFrontmostApplication(
          snapshot, std::optional<int64_t>{4242}
      );

  QCOMPARE(current.state, EditableFocusState::SafeEditable);
  QCOMPARE(current.applicationPid, snapshot.applicationPid);
}

void InputIndicatorPolicyTests::panelPresentationNeverRequestsCursorMutation()
{
  std::vector<MacInputIndicatorPanelOrderCommand> commands;
  requestMacInputIndicatorPanelPresentation(
      [&commands](const MacInputIndicatorPanelOrderCommand command) {
        commands.push_back(command);
      }
  );

  QCOMPARE(
      commands,
      std::vector<MacInputIndicatorPanelOrderCommand>{
          MacInputIndicatorPanelOrderCommand::FrontRegardless
      }
  );
}

void InputIndicatorPolicyTests::panelPresentationNeededAfterActiveSpaceChange()
{
  QVERIFY(!macInputIndicatorPresentationNeeded(false, true));
  QVERIFY(macInputIndicatorPresentationNeeded(true, true));
  QVERIFY(macInputIndicatorPresentationNeeded(false, false));
  QVERIFY(macInputIndicatorPresentationNeeded(true, false));
}

void InputIndicatorPolicyTests::persistentPolicyUsesOnlyEnabledLocalAndKnownSource()
{
  const PersistentInputIndicatorPolicyInput visible{
      .enabled = true,
      .isOnComputer = true,
      .source = Source::LatinABC,
  };

  const auto decision =
      InputIndicatorPolicy::decidePersistent(visible);
  QVERIFY(decision.show);
  QCOMPARE(decision.source, Source::LatinABC);

  auto disabled = visible;
  disabled.enabled = false;
  QCOMPARE(
      InputIndicatorPolicy::decidePersistent(disabled).reason,
      InputIndicatorVisibilityReason::Disabled
  );

  auto remote = visible;
  remote.isOnComputer = false;
  QCOMPARE(
      InputIndicatorPolicy::decidePersistent(remote).reason,
      InputIndicatorVisibilityReason::OffComputer
  );

  auto unknown = visible;
  unknown.source = Source::Unknown;
  QCOMPARE(
      InputIndicatorPolicy::decidePersistent(unknown).reason,
      InputIndicatorVisibilityReason::SourceUnknown
  );
}

void InputIndicatorPolicyTests::persistentKnownSourceShowsWithoutConnectivityOrAx()
{
  PersistentControllerHarness harness;
  MacInputIndicatorController controller(
      nullptr, harness.dependencies()
  );
  MacInputIndicatorControllerTestAccess::arm(controller);

  controller.inputIndicatorStateInvalidated();

  QCOMPARE(harness.presentations.size(), size_t{1});
  QCOMPARE(harness.presentations.front().source, Source::LatinABC);
  QCOMPARE(harness.moves, 2);
  QVERIFY(!MacInputIndicatorControllerTestAccess::renderTimerActive(
      controller
  ));
}

void InputIndicatorPolicyTests::persistentOffComputerHides()
{
  PersistentControllerHarness harness;
  MacInputIndicatorController controller(
      nullptr, harness.dependencies()
  );
  MacInputIndicatorControllerTestAccess::arm(controller);

  controller.inputIndicatorLocalChanged(false);

  QVERIFY(harness.hides > 0);
  QVERIFY(!MacInputIndicatorControllerTestAccess::renderTimerActive(
      controller
  ));
  QVERIFY(!MacInputIndicatorControllerTestAccess::fallbackTimerActive(
      controller
  ));

  controller.inputIndicatorLocalChanged(true);
  QVERIFY(!MacInputIndicatorControllerTestAccess::renderTimerActive(
      controller
  ));
  QCOMPARE(harness.presentations.size(), size_t{1});
}

void InputIndicatorPolicyTests::persistentLocalAndDisconnectedTransitionsShow()
{
  PersistentControllerHarness harness;
  MacInputIndicatorController controller(
      nullptr, harness.dependencies()
  );
  MacInputIndicatorControllerTestAccess::arm(controller);

  controller.inputIndicatorLocalChanged(false);
  const auto hides = harness.hides;
  controller.inputIndicatorStateInvalidated();

  QVERIFY(!MacInputIndicatorControllerTestAccess::renderTimerActive(
      controller
  ));
  QCOMPARE(harness.presentations.size(), size_t{1});
  QCOMPARE(harness.hides, hides);
}

void InputIndicatorPolicyTests::persistentUnknownSourceHidesWithoutStaleLabel()
{
  PersistentControllerHarness harness;
  MacInputIndicatorController controller(
      nullptr, harness.dependencies()
  );
  MacInputIndicatorControllerTestAccess::arm(controller);
  harness.identifier = "com.apple.inputmethod.Unsupported";
  harness.source = Source::Unknown;

  MacInputIndicatorControllerTestAccess::inputSourceChanged(controller);

  QVERIFY(harness.hides > 0);
  QVERIFY(!MacInputIndicatorControllerTestAccess::renderTimerActive(
      controller
  ));
  QVERIFY(!MacInputIndicatorControllerTestAccess::fallbackTimerActive(
      controller
  ));
  QCOMPARE(harness.presentations.size(), size_t{1});

  const auto moves = harness.moves;
  controller.inputIndicatorStateInvalidated();
  QCOMPARE(harness.moves, moves);
  QCOMPARE(harness.presentations.size(), size_t{1});
}

void InputIndicatorPolicyTests::persistentPointerTicksMoveWithoutReconfigure()
{
  PersistentControllerHarness harness;
  MacInputIndicatorController controller(
      nullptr, harness.dependencies()
  );
  MacInputIndicatorControllerTestAccess::arm(controller);
  const auto hides = harness.hides;

  QCOMPARE(
      MacInputIndicatorControllerTestAccess::renderTimerInterval(controller),
      8
  );
  MacInputIndicatorControllerTestAccess::pointerTick(controller);

  QCOMPARE(harness.moves, 2);
  QCOMPARE(harness.presentations.size(), size_t{1});
  QCOMPARE(harness.hides, hides);
}

void InputIndicatorPolicyTests::persistentDuplicateSourceDoesNotReconfigure()
{
  PersistentControllerHarness harness;
  MacInputIndicatorController controller(
      nullptr, harness.dependencies()
  );
  MacInputIndicatorControllerTestAccess::arm(controller);

  MacInputIndicatorControllerTestAccess::inputSourceChanged(controller);

  QCOMPARE(harness.presentations.size(), size_t{1});
  QCOMPARE(harness.moves, 1);
}

void InputIndicatorPolicyTests::persistentSourceChangesUpdateLabel()
{
  PersistentControllerHarness harness;
  MacInputIndicatorController controller(
      nullptr, harness.dependencies()
  );
  MacInputIndicatorControllerTestAccess::arm(controller);
  harness.identifier = "com.apple.inputmethod.Korean.2SetKorean";
  harness.source = Source::Korean2Set;

  MacInputIndicatorControllerTestAccess::inputSourceChanged(controller);

  QCOMPARE(harness.presentations.size(), size_t{2});
  QCOMPARE(
      harness.presentations.back().source,
      Source::Korean2Set
  );

  harness.identifier = "com.apple.keylayout.ABC";
  harness.source = Source::LatinABC;
  MacInputIndicatorControllerTestAccess::inputSourceChanged(controller);
  QCOMPARE(harness.presentations.size(), size_t{3});
  QCOMPARE(harness.presentations.back().source, Source::LatinABC);
}

void InputIndicatorPolicyTests::persistentCompanionChoiceUpdatesPanel()
{
  PersistentControllerHarness harness;
  MacInputIndicatorController controller(
      nullptr, harness.dependencies()
  );
  MacInputIndicatorControllerTestAccess::arm(controller);
  QCOMPARE(
      harness.presentations.front().companion,
      InputIndicatorCompanion::Cat
  );

  MacInputIndicatorControllerTestAccess::setCompanion(
      controller, InputIndicatorCompanion::Dog
  );

  QCOMPARE(harness.presentations.size(), size_t{2});
  QCOMPARE(
      harness.presentations.back().companion,
      InputIndicatorCompanion::Dog
  );
}

void InputIndicatorPolicyTests::persistentFeatureOffStopsAndHides()
{
  PersistentControllerHarness harness;
  MacInputIndicatorController controller(
      nullptr, harness.dependencies()
  );
  MacInputIndicatorControllerTestAccess::enableWithMonitor(controller);
  QVERIFY(!MacInputIndicatorControllerTestAccess::renderTimerActive(
      controller
  ));
  QVERIFY(!MacInputIndicatorControllerTestAccess::fallbackTimerActive(
      controller
  ));
  QCOMPARE(harness.pointerMonitorStarts, 1);
  QVERIFY(MacInputIndicatorControllerTestAccess::sourceMonitorActive(
      controller
  ));
  QVERIFY(MacInputIndicatorControllerTestAccess::panelActive(controller));
  const auto presentations = harness.presentations.size();

  MacInputIndicatorControllerTestAccess::setEnabled(controller, false);

  QVERIFY(harness.hides > 0);
  QVERIFY(!MacInputIndicatorControllerTestAccess::renderTimerActive(
      controller
  ));
  QVERIFY(!MacInputIndicatorControllerTestAccess::fallbackTimerActive(
      controller
  ));
  QVERIFY(!MacInputIndicatorControllerTestAccess::sourceMonitorActive(
      controller
  ));
  QVERIFY(!MacInputIndicatorControllerTestAccess::panelActive(controller));
  QCOMPARE(harness.pointerMonitorStops, 1);
  controller.inputIndicatorStateInvalidated();
  QCOMPARE(harness.presentations.size(), presentations);
}

void InputIndicatorPolicyTests::persistentGeometryOrPanelFailureHides()
{
  PersistentControllerHarness geometry;
  MacInputIndicatorController controller(
      nullptr, geometry.dependencies()
  );
  MacInputIndicatorControllerTestAccess::arm(controller);
  geometry.moveResult = false;

  MacInputIndicatorControllerTestAccess::pointerTick(controller);
  QVERIFY(geometry.hides > 0);

  PersistentControllerHarness unavailable;
  unavailable.showResult = false;
  MacInputIndicatorController failedPanel(
      nullptr, unavailable.dependencies()
  );
  MacInputIndicatorControllerTestAccess::arm(failedPanel);

  QCOMPARE(unavailable.presentations.size(), size_t{1});
  QVERIFY(unavailable.hides > 0);
}

void InputIndicatorPolicyTests::persistentActivityDoesNotGateVisibility()
{
  PersistentControllerHarness harness;
  MacInputIndicatorController controller(
      nullptr, harness.dependencies()
  );
  MacInputIndicatorControllerTestAccess::arm(controller);
  const auto presentations = harness.presentations.size();
  const auto moves = harness.moves;
  const auto hides = harness.hides;

  controller.inputIndicatorActivityReceived({});
  controller.inputIndicatorActivityInvalidated();
  QCOMPARE(harness.presentations.size(), presentations);
  QCOMPARE(harness.moves, moves);
  QCOMPARE(harness.hides, hides);
  QVERIFY(!MacInputIndicatorControllerTestAccess::renderTimerActive(
      controller
  ));

  controller.inputIndicatorLocalChanged(false);
  const auto remoteHides = harness.hides;
  controller.inputIndicatorActivityReceived({});
  QCOMPARE(harness.hides, remoteHides);
  QVERIFY(!MacInputIndicatorControllerTestAccess::renderTimerActive(
      controller
  ));
  QVERIFY(!MacInputIndicatorControllerTestAccess::fallbackTimerActive(
      controller
  ));
}

void InputIndicatorPolicyTests::completePointerMonitorUsesEventRendering()
{
  PersistentControllerHarness harness;
  MacInputIndicatorController controller(
      nullptr, harness.dependencies()
  );
  MacInputIndicatorControllerTestAccess::enableWithMonitor(controller);

  QCOMPARE(harness.pointerMonitorStarts, 1);
  QVERIFY(!MacInputIndicatorControllerTestAccess::fallbackTimerActive(
      controller
  ));
  QVERIFY(harness.pointerMoved);

  const auto moves = harness.moves;
  harness.moveAnimating = true;
  harness.moveFrameInterval = 8;
  harness.pointerMoved();
  QCOMPARE(harness.moves, moves + 1);
  QVERIFY(MacInputIndicatorControllerTestAccess::renderTimerActive(
      controller
  ));
}

void InputIndicatorPolicyTests::incompletePointerMonitorKeepsFallbackTimer()
{
  PersistentControllerHarness harness;
  harness.pointerMonitorComplete = false;
  MacInputIndicatorController controller(
      nullptr, harness.dependencies()
  );
  MacInputIndicatorControllerTestAccess::enableWithMonitor(controller);

  QVERIFY(MacInputIndicatorControllerTestAccess::fallbackTimerActive(
      controller
  ));
  QCOMPARE(
      MacInputIndicatorControllerTestAccess::fallbackTimerInterval(
          controller
      ),
      16
  );
}

void InputIndicatorPolicyTests::pointerEventsCoalesceWhileRenderTimerIsActive()
{
  PersistentControllerHarness harness;
  MacInputIndicatorController controller(
      nullptr, harness.dependencies()
  );
  MacInputIndicatorControllerTestAccess::enableWithMonitor(controller);
  harness.moveAnimating = true;
  harness.moveFrameInterval = 8;

  const auto moves = harness.moves;
  harness.pointerMoved();
  QCOMPARE(harness.moves, moves + 1);
  harness.pointerMoved();
  QCOMPARE(harness.moves, moves + 1);

  MacInputIndicatorControllerTestAccess::renderTick(controller);
  QCOMPARE(harness.moves, moves + 2);
  harness.moveAnimating = false;
  MacInputIndicatorControllerTestAccess::renderTick(controller);
  QVERIFY(!MacInputIndicatorControllerTestAccess::renderTimerActive(
      controller
  ));
}

void InputIndicatorPolicyTests::persistentIdleCompanionUsesLowDutyTimer()
{
  PersistentControllerHarness harness;
  harness.moveAnimating = true;
  harness.moveFrameInterval = 75;
  MacInputIndicatorController controller(
      nullptr, harness.dependencies()
  );
  MacInputIndicatorControllerTestAccess::enableWithMonitor(controller);

  QVERIFY(MacInputIndicatorControllerTestAccess::renderTimerActive(
      controller
  ));
  QCOMPARE(
      MacInputIndicatorControllerTestAccess::renderTimerInterval(
          controller
      ),
      75
  );
}

void InputIndicatorPolicyTests::persistentWalkAndRunUseRenderTimer()
{
  PersistentControllerHarness harness;
  harness.moveAnimating = true;
  harness.moveFrameInterval = 8;
  MacInputIndicatorController controller(
      nullptr, harness.dependencies()
  );
  MacInputIndicatorControllerTestAccess::enableWithMonitor(controller);

  QVERIFY(MacInputIndicatorControllerTestAccess::renderTimerActive(
      controller
  ));
  QCOMPARE(
      MacInputIndicatorControllerTestAccess::renderTimerInterval(
          controller
      ),
      8
  );
}

void InputIndicatorPolicyTests::persistentHiddenAndReducedMotionStopTimers()
{
  PersistentControllerHarness harness;
  harness.moveReducedMotion = true;
  harness.pointerMonitorComplete = false;
  MacInputIndicatorController controller(
      nullptr, harness.dependencies()
  );
  MacInputIndicatorControllerTestAccess::enableWithMonitor(controller);

  QVERIFY(!MacInputIndicatorControllerTestAccess::renderTimerActive(
      controller
  ));
  QVERIFY(!MacInputIndicatorControllerTestAccess::fallbackTimerActive(
      controller
  ));

  harness.moveReducedMotion = false;
  controller.inputIndicatorLocalChanged(false);
  QVERIFY(!MacInputIndicatorControllerTestAccess::renderTimerActive(
      controller
  ));
  QVERIFY(!MacInputIndicatorControllerTestAccess::fallbackTimerActive(
      controller
  ));
}
#endif

void InputIndicatorPolicyTests::visibilityDecisionReportsHideReason()
{
  auto input = safeInput();
  input.isOnComputer = false;

  const auto decision =
      InputIndicatorPolicy::decide(input, std::chrono::milliseconds{1500});

  QVERIFY(!decision.show);
  QCOMPARE(
      decision.reason, InputIndicatorVisibilityReason::OffComputer
  );
  QCOMPARE(
      QString::fromLatin1(
          inputIndicatorVisibilityReasonText(decision.reason)
      ),
      QStringLiteral("off-computer")
  );
}

void InputIndicatorPolicyTests::diagnosticThrottleLimitsToOnePerFiveSeconds()
{
  InputIndicatorDiagnosticThrottle throttle;
  const auto start = std::chrono::steady_clock::time_point{
      std::chrono::seconds{10}
  };

  QVERIFY(throttle.allow(start));
  QVERIFY(!throttle.allow(start + std::chrono::milliseconds{4999}));
  QVERIFY(throttle.allow(start + std::chrono::milliseconds{5000}));
}

void InputIndicatorPolicyTests::unsafeStateHides_data()
{
  QTest::addColumn<int>("condition");

  QTest::newRow("disabled") << 0;
  QTest::newRow("trust missing") << 1;
  QTest::newRow("core missing") << 2;
  QTest::newRow("not primary") << 3;
  QTest::newRow("off computer") << 4;
  QTest::newRow("screen unknown") << 5;
  QTest::newRow("secure field") << 6;
  QTest::newRow("source unknown") << 7;
  QTest::newRow("generation changed") << 8;
}

void InputIndicatorPolicyTests::unsafeStateHides()
{
  QFETCH(int, condition);
  auto input = safeInput();

  switch (condition) {
  case 0:
    input.enabled = false;
    break;
  case 1:
    input.accessibilityTrusted = false;
    break;
  case 2:
    input.coreConnected = false;
    break;
  case 3:
    input.isPrimary = false;
    break;
  case 4:
    input.isOnComputer = false;
    break;
  case 5:
    input.screenKnown = false;
    break;
  case 6:
    input.focus = EditableFocusState::SecureOrProtected;
    break;
  case 7:
    input.source = Source::Unknown;
    break;
  case 8:
    ++input.probeGeneration;
    break;
  default:
    QFAIL("unknown test row");
  }

  QVERIFY(!InputIndicatorPolicy::decide(input, std::chrono::milliseconds{1500}).show);
}

void InputIndicatorPolicyTests::staleEventHides()
{
  auto input = safeInput();
  input.eventAge = input.maximumEventAge + std::chrono::milliseconds{1};

  QVERIFY(!InputIndicatorPolicy::decide(input, std::chrono::milliseconds{1500}).show);
}

void InputIndicatorPolicyTests::invalidDwellHides()
{
  QVERIFY(!InputIndicatorPolicy::decide(safeInput(), std::chrono::milliseconds{0}).show);
}

QTEST_MAIN(InputIndicatorPolicyTests)
