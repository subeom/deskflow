/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QTest>

class OutputLineSplitterTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void aLine_isGivenWhenItsEndArrives();
  void aLineInPieces_isGivenOnceAndWhole();
  void aTokenSplitAcrossTwoReadsInsideTheLimit_isWholeWhenTheLineIsGiven();
  void carriageReturnAlone_orWithLineFeed_endsALineAndMakesNoEmptyLine();
  void aLineThatPassesTheLimit_isNeverGivenNotEvenInParts();
  void aTokenAcrossTheLimit_isNeverGivenInPieces();
  void aLongLineInOneRead_isSkippedAsWell();
  void afterASkippedLine_theNextLineIsGiven();
  void theNoticeIsGivenOncePerSkippedLine();
  void aLineThatIsExactlyTheLimit_isGiven();
  void finish_givesTheLastLineThatHadNoEnd_andNotASkippedOne();
  void aMultiByteCharacterSplitAcrossReads_isWholeInTheLine();
};
