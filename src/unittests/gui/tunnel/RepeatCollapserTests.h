/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QTest>

class RepeatCollapserTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void aFirstLine_isShown();
  void theSameLineAgainInsideTheWindow_isCountedAndNotShown();
  void theSameLineAfterTheWindow_isShownWithTheCountOfWhatWasLeftOut();
  void aOtherLine_isShownAndFlushesTheCount();
  void flush_givesTheCountOnceAndForgetsIt();
  void theTimeOfALineInTheKey_doesNotMakeTwoLinesOne();
  void reset_forgetsEverything();
};
