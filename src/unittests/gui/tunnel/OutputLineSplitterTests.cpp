/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "OutputLineSplitterTests.h"

#include "gui/tunnel/OutputLineSplitter.h"

using namespace deskflow::tunnel;

namespace {

// made up: nothing like a real token
const QByteArray kSecret = "dummy-secret-value-0123456789";

QString notice()
{
  return QString::fromLatin1(OutputLineSplitter::kTooLongNotice);
}

// Whether any part of the secret of at least \p minimum letters is in a line: a half of a token that was cut in two
// must not be given either
bool holdsAPartOfTheSecret(const QStringList &lines, int minimum = 8)
{
  const auto secret = QString::fromLatin1(kSecret);
  for (const auto &line : lines) {
    for (int start = 0; start + minimum <= secret.size(); ++start) {
      for (int length = minimum; start + length <= secret.size(); ++length) {
        if (line.contains(secret.mid(start, length))) {
          return true;
        }
      }
    }
  }
  return false;
}

} // namespace

void OutputLineSplitterTests::aLine_isGivenWhenItsEndArrives()
{
  OutputLineSplitter splitter;
  QCOMPARE(splitter.feed("first\nsecond\n"), (QStringList{"first", "second"}));
  QCOMPARE(splitter.feed("partial"), QStringList());
  QCOMPARE(splitter.feed("\n"), QStringList{"partial"});
}

void OutputLineSplitterTests::aLineInPieces_isGivenOnceAndWhole()
{
  OutputLineSplitter splitter;
  QStringList got;
  for (const auto &piece : {"INF Start ", "Websocket ", "listener", " host=127.0.0.1:24800", "\n"}) {
    got << splitter.feed(piece);
  }
  QCOMPARE(got, QStringList{"INF Start Websocket listener host=127.0.0.1:24800"});
}

// the limit does not cut a line that is in pieces: the pieces are put together, and the line that is given holds the
// token
void OutputLineSplitterTests::aTokenSplitAcrossTwoReadsInsideTheLimit_isWholeWhenTheLineIsGiven()
{
  OutputLineSplitter splitter;
  const auto half = kSecret.size() / 2;
  QStringList got;
  got << splitter.feed("ERR bad token: " + kSecret.left(half));
  QVERIFY(got.isEmpty());
  got << splitter.feed(kSecret.mid(half) + " end\n");

  QCOMPARE(got.size(), 1);
  QVERIFY(got.first().contains(QString::fromLatin1(kSecret)));
}

void OutputLineSplitterTests::carriageReturnAlone_orWithLineFeed_endsALineAndMakesNoEmptyLine()
{
  OutputLineSplitter splitter;
  QCOMPARE(splitter.feed("one\rtwo\r\nthree\n\n\r\nfour\r"), (QStringList{"one", "two", "three", "four"}));
  // a \r\n that is cut between the two characters
  QCOMPARE(splitter.feed("five\r"), QStringList{"five"});
  QCOMPARE(splitter.feed("\nsix\n"), QStringList{"six"});
}

void OutputLineSplitterTests::aLineThatPassesTheLimit_isNeverGivenNotEvenInParts()
{
  OutputLineSplitter splitter(100);
  QStringList got;
  // 250 letters in reads of 30, then the end of the line
  for (int sent = 0; sent < 250; sent += 30) {
    got << splitter.feed(QByteArray(30, 'x'));
  }
  got << splitter.feed("\n");

  // only the notice: no part of the line is given, now or later
  QCOMPARE(got, QStringList{notice()});
  QCOMPARE(splitter.finish(), QStringList());
}

// The case of the review: the token is where the buffer is full. Whatever way the reads cut the line, no piece of the
// token is ever given
void OutputLineSplitterTests::aTokenAcrossTheLimit_isNeverGivenInPieces()
{
  constexpr int limit = 1000;
  for (int cut = 1; cut < kSecret.size(); ++cut) {
    // the token starts so that the limit falls inside it, after \p cut of its letters
    const auto before = limit - cut;
    const auto line = QByteArray(before, 'a') + kSecret + QByteArray(300, 'b') + "\n";

    for (const int readSize : {1, 7, 64, 333, 100000}) {
      OutputLineSplitter splitter(limit);
      QStringList got;
      for (qsizetype at = 0; at < line.size(); at += readSize) {
        got << splitter.feed(line.mid(at, readSize));
      }
      got << splitter.finish();

      QVERIFY2(!holdsAPartOfTheSecret(got), qPrintable(QStringLiteral("cut %1, read size %2").arg(cut).arg(readSize)));
      QCOMPARE(got, QStringList{notice()});
    }
  }
}

void OutputLineSplitterTests::aLongLineInOneRead_isSkippedAsWell()
{
  OutputLineSplitter splitter(100);
  const auto got = splitter.feed(QByteArray(60, 'a') + kSecret + QByteArray(60, 'b') + "\nnext\n");
  QCOMPARE(got, (QStringList{notice(), "next"}));
  QVERIFY(!holdsAPartOfTheSecret(got));
}

void OutputLineSplitterTests::afterASkippedLine_theNextLineIsGiven()
{
  OutputLineSplitter splitter(50);
  QStringList got;
  got << splitter.feed(QByteArray(200, 'x'));
  got << splitter.feed(QByteArray(200, 'y') + "\r");
  got << splitter.feed("after\n");
  QCOMPARE(got, (QStringList{notice(), "after"}));
}

void OutputLineSplitterTests::theNoticeIsGivenOncePerSkippedLine()
{
  OutputLineSplitter splitter(20);
  QStringList got;
  for (int line = 0; line < 3; ++line) {
    for (int piece = 0; piece < 10; ++piece) {
      got << splitter.feed(QByteArray(15, 'z'));
    }
    got << splitter.feed("\nok\n");
  }
  QCOMPARE(got.count(notice()), 3);
  QCOMPARE(got.count(QStringLiteral("ok")), 3);
  QCOMPARE(got.size(), 6);
}

void OutputLineSplitterTests::aLineThatIsExactlyTheLimit_isGiven()
{
  OutputLineSplitter splitter(40);
  QCOMPARE(splitter.feed(QByteArray(40, 'k') + "\n"), QStringList{QString(40, QLatin1Char('k'))});
  QCOMPARE(splitter.feed(QByteArray(41, 'k') + "\n"), QStringList{notice()});
}

void OutputLineSplitterTests::finish_givesTheLastLineThatHadNoEnd_andNotASkippedOne()
{
  OutputLineSplitter splitter(30);
  QCOMPARE(splitter.feed("ERR the last words"), QStringList());
  QCOMPARE(splitter.finish(), QStringList{"ERR the last words"});
  QCOMPARE(splitter.finish(), QStringList());

  // a line that was skipped does not come back at the end
  QStringList got = splitter.feed(QByteArray(100, 'q'));
  QCOMPARE(got, QStringList{notice()});
  QCOMPARE(splitter.finish(), QStringList());
  // and the next line after the end is a new line
  QCOMPARE(splitter.feed("fresh\n"), QStringList{"fresh"});
}

void OutputLineSplitterTests::aMultiByteCharacterSplitAcrossReads_isWholeInTheLine()
{
  OutputLineSplitter splitter;
  const auto bytes = QStringLiteral("ERR 한글 ✓ done\n").toUtf8();
  QStringList got;
  for (qsizetype at = 0; at < bytes.size(); ++at) {
    got << splitter.feed(bytes.mid(at, 1));
  }
  QCOMPARE(got, QStringList{QStringLiteral("ERR 한글 ✓ done")});
}

QTEST_MAIN(OutputLineSplitterTests)
