/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "OSXInputSourceTests.h"

#include "platform/OSXInputSource.h"

#include <QTest>

using deskflow::input::InputIndicatorSource;

void OSXInputSourceTests::exactIdsAreClassified()
{
  QCOMPARE(
      deskflow::platform::classifyMacInputSourceId("com.apple.keylayout.ABC"),
      InputIndicatorSource::LatinABC
  );
  QCOMPARE(
      deskflow::platform::classifyMacInputSourceId("com.apple.inputmethod.Korean.2SetKorean"),
      InputIndicatorSource::Korean2Set
  );
}

void OSXInputSourceTests::supportedSnapshotsPreserveIdentifierAndClassification()
{
  struct TestCase
  {
    const char *identifier;
    InputIndicatorSource source;
  };
  const TestCase testCases[] = {
      {"com.apple.keylayout.ABC", InputIndicatorSource::LatinABC},
      {"com.apple.inputmethod.Korean.2SetKorean", InputIndicatorSource::Korean2Set},
  };

  for (const auto &testCase : testCases) {
    const auto snapshot =
        deskflow::platform::makeMacInputSourceSnapshot(testCase.identifier);

    QCOMPARE(snapshot.identifier, std::string(testCase.identifier));
    QCOMPARE(snapshot.source, testCase.source);
  }
}

void OSXInputSourceTests::otherValuesAreUnknown()
{
  const char *unsupported[] = {
      "",
      "ABC",
      "com.apple.keylayout.US",
      "com.apple.inputmethod.Korean",
      "com.apple.inputmethod.Korean.3SetKorean",
      "ko",
      "Korean - 2-Set Korean",
      "com.apple.inputmethod.Korean.2SetKorean.extra",
  };

  for (const auto *sourceId : unsupported) {
    QCOMPARE(deskflow::platform::classifyMacInputSourceId(sourceId), InputIndicatorSource::Unknown);
  }
}

void OSXInputSourceTests::transitionActivationRequiresAChangedKnownSource_data()
{
  QTest::addColumn<int>("previous");
  QTest::addColumn<int>("current");
  QTest::addColumn<bool>("activate");

  using Source = InputIndicatorSource;
  QTest::newRow("baseline-abc")
      << static_cast<int>(Source::LatinABC)
      << static_cast<int>(Source::LatinABC) << false;
  QTest::newRow("duplicate-korean")
      << static_cast<int>(Source::Korean2Set)
      << static_cast<int>(Source::Korean2Set) << false;
  QTest::newRow("abc-to-korean")
      << static_cast<int>(Source::LatinABC)
      << static_cast<int>(Source::Korean2Set) << true;
  QTest::newRow("korean-to-abc")
      << static_cast<int>(Source::Korean2Set)
      << static_cast<int>(Source::LatinABC) << true;
  QTest::newRow("unknown-to-abc")
      << static_cast<int>(Source::Unknown)
      << static_cast<int>(Source::LatinABC) << true;
  QTest::newRow("abc-to-unknown")
      << static_cast<int>(Source::LatinABC)
      << static_cast<int>(Source::Unknown) << false;
}

void OSXInputSourceTests::transitionActivationRequiresAChangedKnownSource()
{
  QFETCH(int, previous);
  QFETCH(int, current);
  QFETCH(bool, activate);

  QCOMPARE(
      deskflow::platform::shouldActivateMacInputSourceTransition(
          static_cast<InputIndicatorSource>(previous),
          static_cast<InputIndicatorSource>(current)
      ),
      activate
  );
}

QTEST_MAIN(OSXInputSourceTests)
