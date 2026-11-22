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

QTEST_MAIN(OSXInputSourceTests)
