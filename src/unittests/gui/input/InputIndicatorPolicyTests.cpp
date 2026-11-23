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
#endif

#include <QTest>

using namespace deskflow::gui::input;
using Source = deskflow::input::InputIndicatorSource;

#ifdef Q_OS_MACOS
struct MacInputIndicatorControllerTestAccess
{
  static void arm(MacInputIndicatorController &controller)
  {
    controller.m_effectiveEnabled = true;
    controller.m_coreConnected = true;
    controller.m_isOnComputer = true;
  }

  static void trigger(
      MacInputIndicatorController &controller,
      InputIndicatorUiTrigger trigger
  )
  {
    controller.trigger(trigger);
  }

  static std::optional<InputIndicatorUiTrigger> pendingTrigger(
      const MacInputIndicatorController &controller
  )
  {
    return controller.m_pendingTrigger;
  }
};
#endif

namespace {

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

void InputIndicatorPolicyTests::panelPresentationRequestsFrontRegardless()
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

void InputIndicatorPolicyTests::focusTriggerWithoutActivityQueuesProbe()
{
  MacInputIndicatorController controller;
  MacInputIndicatorControllerTestAccess::arm(controller);

  MacInputIndicatorControllerTestAccess::trigger(
      controller, InputIndicatorUiTrigger::FocusedElementChanged
  );

  QCOMPARE(
      MacInputIndicatorControllerTestAccess::pendingTrigger(controller),
      std::optional<InputIndicatorUiTrigger>{
          InputIndicatorUiTrigger::FocusedElementChanged
      }
  );
}

void InputIndicatorPolicyTests::sourceTriggerWithoutActivityQueuesProbe()
{
  MacInputIndicatorController controller;
  MacInputIndicatorControllerTestAccess::arm(controller);

  MacInputIndicatorControllerTestAccess::trigger(
      controller, InputIndicatorUiTrigger::InputSourceChanged
  );

  QCOMPARE(
      MacInputIndicatorControllerTestAccess::pendingTrigger(controller),
      std::optional<InputIndicatorUiTrigger>{
          InputIndicatorUiTrigger::InputSourceChanged
      }
  );
}

void InputIndicatorPolicyTests::coreTriggerWithoutActivityDoesNotQueueProbe()
{
  MacInputIndicatorController controller;
  MacInputIndicatorControllerTestAccess::arm(controller);

  MacInputIndicatorControllerTestAccess::trigger(
      controller, InputIndicatorUiTrigger::CoreActivity
  );

  QVERIFY(!MacInputIndicatorControllerTestAccess::pendingTrigger(
      controller
  ));
}

void InputIndicatorPolicyTests::invalidActivityDoesNotCancelFocusTrigger()
{
  MacInputIndicatorController controller;
  MacInputIndicatorControllerTestAccess::arm(controller);
  MacInputIndicatorControllerTestAccess::trigger(
      controller, InputIndicatorUiTrigger::FocusedElementChanged
  );

  controller.inputIndicatorActivityInvalidated();

  QCOMPARE(
      MacInputIndicatorControllerTestAccess::pendingTrigger(controller),
      std::optional<InputIndicatorUiTrigger>{
          InputIndicatorUiTrigger::FocusedElementChanged
      }
  );
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
