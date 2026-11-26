/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "FrameMessagesTests.h"

#include "gui/chrome/FrameMessages.h"

#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>

using namespace deskflow::gui::chrome;

namespace {

constexpr qintptr kUntouched = 0x5eed;

// What answers for a window in these tests: it notes what it was asked and says what it is told to
struct FakeHost
{
  QStringList calls;
  bool takesTheClick = true;
  qintptr hit = 12; // HTCLIENT is 1, HTMAXBUTTON 9: the value does not matter here, the router passes it on

  void calculateClientArea(std::uintptr_t wParam, std::intptr_t lParam)
  {
    calls << QStringLiteral("calculateClientArea(%1,%2)").arg(wParam).arg(lParam);
  }
  void limitSizes(std::intptr_t lParam)
  {
    calls << QStringLiteral("limitSizes(%1)").arg(lParam);
  }
  void paintBackground(std::uintptr_t wParam)
  {
    calls << QStringLiteral("paintBackground(%1)").arg(wParam);
  }
  qintptr hitCodeAt(std::intptr_t lParam)
  {
    calls << QStringLiteral("hitCodeAt(%1)").arg(lParam);
    return hit;
  }
  void noteMouseOverNonClient(std::uintptr_t wParam)
  {
    calls << QStringLiteral("noteMouseOverNonClient(%1)").arg(wParam);
  }
  void noteMouseLeftNonClient()
  {
    calls << QStringLiteral("noteMouseLeftNonClient()");
  }
  bool pressesMaximizeButton(std::uintptr_t wParam)
  {
    calls << QStringLiteral("pressesMaximizeButton(%1)").arg(wParam);
    return takesTheClick;
  }
  bool releasesMaximizeButton(std::uintptr_t wParam)
  {
    calls << QStringLiteral("releasesMaximizeButton(%1)").arg(wParam);
    return takesTheClick;
  }
};

constexpr FrameMessage kAll[] = {FrameMessage::EraseBackground, FrameMessage::GetMinMaxInfo,
                                 FrameMessage::NcCalcSize,      FrameMessage::NcHitTest,
                                 FrameMessage::NcMouseMove,     FrameMessage::NcLeftButtonDown,
                                 FrameMessage::NcLeftButtonUp,  FrameMessage::NcMouseLeave};

std::uint32_t number(FrameMessage message)
{
  return static_cast<std::uint32_t>(message);
}

// The rule of Qt 6.9.3 for the messages that windowsProc does not filter again because the loop of messages did
// (QWindowsContext::isInputMessage, qwindowscontext.cpp): written here from the source of Qt, as the table is checked
// against it
bool qtTakesItFromTheQueue(std::uint32_t m)
{
  switch (m) {
  case 0x010D: // WM_IME_STARTCOMPOSITION
  case 0x010E: // WM_IME_ENDCOMPOSITION
  case 0x010F: // WM_IME_COMPOSITION
  case 0x00FF: // WM_INPUT
  case 0x0240: // WM_TOUCH
  case 0x02A1: // WM_MOUSEHOVER
  case 0x02A3: // WM_MOUSELEAVE
  case 0x02A0: // WM_NCMOUSEHOVER
  case 0x02A2: // WM_NCMOUSELEAVE
  case 0x0214: // WM_SIZING
  case 0x0216: // WM_MOVING
  case 0x0112: // WM_SYSCOMMAND
  case 0x0111: // WM_COMMAND
  case 0x031F: // WM_DWMNCRENDERINGCHANGED
  case 0x000F: // WM_PAINT
    return true;
  default:
    break;
  }
  return (m >= 0x0200 && m <= 0x020E)     // WM_MOUSEFIRST to WM_MOUSELAST
         || (m >= 0x00A0 && m <= 0x00AD)  // WM_NCMOUSEMOVE to WM_NCXBUTTONDBLCLK
         || (m >= 0x0100 && m <= 0x0109); // WM_KEYFIRST to WM_KEYLAST
}

QString sourceOf(const QString &path)
{
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

QStringList guiSources()
{
  QStringList paths;
  QDirIterator it(
      QStringLiteral(DESKFLOW_GUI_SOURCE_DIR), {QStringLiteral("*.cpp"), QStringLiteral("*.h"), QStringLiteral("*.mm")},
      QDir::Files, QDirIterator::Subdirectories
  );
  while (it.hasNext())
    paths << it.next();
  return paths;
}

} // namespace

void FrameMessagesTests::delivery_isWhatQtDoes_withTheMessagesOfTheQueueAndTheOnesThatAreSent()
{
  // a message that Qt takes from the queue has no place for the answer; the others (sent by Windows) have one
  for (const auto message : kAll) {
    const bool queued = qtTakesItFromTheQueue(number(message));
    QCOMPARE(deliveryOf(message) == Delivery::Queued, queued);
  }
  // the four that need an answer are sent, the four of the mouse in the non client area are queued
  QCOMPARE(deliveryOf(FrameMessage::NcCalcSize), Delivery::Sent);
  QCOMPARE(deliveryOf(FrameMessage::GetMinMaxInfo), Delivery::Sent);
  QCOMPARE(deliveryOf(FrameMessage::EraseBackground), Delivery::Sent);
  QCOMPARE(deliveryOf(FrameMessage::NcHitTest), Delivery::Sent);
  QCOMPARE(deliveryOf(FrameMessage::NcLeftButtonDown), Delivery::Queued);
  QCOMPARE(deliveryOf(FrameMessage::NcLeftButtonUp), Delivery::Queued);
  QCOMPARE(deliveryOf(FrameMessage::NcMouseMove), Delivery::Queued);
  QCOMPARE(deliveryOf(FrameMessage::NcMouseLeave), Delivery::Queued);
  QCOMPARE(kHitMaximizeButton, std::uintptr_t(9));
}

void FrameMessagesTests::sent_areAnsweredInTheirPlace_andTheHostDoesItsPart()
{
  struct Row
  {
    FrameMessage message;
    qintptr answer;
    QString call;
  };
  const Row rows[] = {
      {FrameMessage::NcCalcSize, 0, QStringLiteral("calculateClientArea(1,77)")},
      {FrameMessage::GetMinMaxInfo, 0, QStringLiteral("limitSizes(77)")},
      {FrameMessage::EraseBackground, 1, QStringLiteral("paintBackground(1)")},
      {FrameMessage::NcHitTest, 12, QStringLiteral("hitCodeAt(77)")},
  };
  for (const auto &row : rows) {
    FakeHost host;
    qintptr place = kUntouched;
    QVERIFY(routeFrameMessage(host, number(row.message), 1, 77, &place));
    QCOMPARE(place, row.answer);
    QCOMPARE(host.calls, QStringList{row.call});
  }
}

void FrameMessagesTests::sent_withNoPlaceForTheAnswer_areLeftToWindows_andTheHostIsNotAsked()
{
  // a message that needs an answer and has no place for it is not taken: nothing is done, and it comes again through
  // the window procedure of Qt, which has the place
  for (const auto message :
       {FrameMessage::NcCalcSize, FrameMessage::GetMinMaxInfo, FrameMessage::EraseBackground,
        FrameMessage::NcHitTest}) {
    FakeHost host;
    QVERIFY2(!routeFrameMessage(host, number(message), 1, 77, nullptr), qPrintable(QString::number(number(message))));
    QVERIFY(host.calls.isEmpty());
  }
}

void FrameMessagesTests::clickOnTheMaximizeButton_fromTheQueue_isTaken_andWritesNothing()
{
  // the crash of the seventh run: the loop of messages of Qt gives the filter a null pointer for the click
  for (const auto message : {FrameMessage::NcLeftButtonDown, FrameMessage::NcLeftButtonUp}) {
    FakeHost host;
    QVERIFY(routeFrameMessage(host, number(message), kHitMaximizeButton, 0, nullptr));
    // the host was asked and took the click; a click that is taken goes no further (the message is not given to the
    // system)
    QCOMPARE(host.calls.size(), 1);
    QVERIFY(host.calls.first().endsWith(QStringLiteral("MaximizeButton(9)")));
  }
}

void FrameMessagesTests::clickOnTheMaximizeButton_withAPlace_isAnsweredWithZero()
{
  for (const auto message : {FrameMessage::NcLeftButtonDown, FrameMessage::NcLeftButtonUp}) {
    FakeHost host;
    qintptr place = kUntouched;
    QVERIFY(routeFrameMessage(host, number(message), kHitMaximizeButton, 0, &place));
    QCOMPARE(place, qintptr(0));
  }
}

void FrameMessagesTests::clickThatIsNotOnTheMaximizeButton_isLeftToWindows()
{
  // the host does not take it (another part of the window, or a system that has no snap layouts): not handled, and what
  // the place held is not touched
  for (const auto message : {FrameMessage::NcLeftButtonDown, FrameMessage::NcLeftButtonUp}) {
    for (qintptr *place : {static_cast<qintptr *>(nullptr)}) {
      FakeHost host;
      host.takesTheClick = false;
      QVERIFY(!routeFrameMessage(host, number(message), 2, 0, place));
      QCOMPARE(host.calls.size(), 1);
    }
    FakeHost host;
    host.takesTheClick = false;
    qintptr place = kUntouched;
    QVERIFY(!routeFrameMessage(host, number(message), 2, 0, &place));
    QCOMPARE(place, kUntouched);
  }
}

void FrameMessagesTests::theMouseOverTheNonClientArea_isNoted_andNeverHandled()
{
  FakeHost host;
  QVERIFY(!routeFrameMessage(host, number(FrameMessage::NcMouseMove), kHitMaximizeButton, 0, nullptr));
  QVERIFY(!routeFrameMessage(host, number(FrameMessage::NcMouseLeave), 0, 0, nullptr));
  QCOMPARE(
      host.calls, (QStringList{QStringLiteral("noteMouseOverNonClient(9)"), QStringLiteral("noteMouseLeftNonClient()")})
  );

  qintptr place = kUntouched;
  QVERIFY(!routeFrameMessage(host, number(FrameMessage::NcMouseMove), 2, 0, &place));
  QVERIFY(!routeFrameMessage(host, number(FrameMessage::NcMouseLeave), 0, 0, &place));
  QCOMPARE(place, kUntouched);
}

void FrameMessagesTests::otherMessages_areNotHandled_withAPlaceOrNot()
{
  // WM_PAINT, WM_NCLBUTTONDBLCLK, WM_NULL, WM_MOUSEMOVE, WM_KEYDOWN and a number that is no message
  for (const std::uint32_t message : {0x000Fu, 0x00A3u, 0x0000u, 0x0200u, 0x0100u, 0xFFFFu}) {
    FakeHost host;
    QVERIFY(!routeFrameMessage(host, message, 9, 5, nullptr));
    qintptr place = kUntouched;
    QVERIFY(!routeFrameMessage(host, message, 9, 5, &place));
    QCOMPARE(place, kUntouched);
    QVERIFY(host.calls.isEmpty());
  }
}

void FrameMessagesTests::everyMessageWithEveryPlace_neverWritesWhereThereIsNoPlace_andNeverCrashes()
{
  for (const auto message : kAll) {
    for (const bool takes : {true, false}) {
      for (const std::uintptr_t wParam :
           {std::uintptr_t(0), std::uintptr_t(1), kHitMaximizeButton, std::uintptr_t(2)}) {
        // no place: the call must come back (a write through a null pointer is a crash that no verdict can show)
        FakeHost none;
        none.takesTheClick = takes;
        const bool handledWithNone = routeFrameMessage(none, number(message), wParam, 3, nullptr);

        // a place: the answer is written when the message is handled, and only then
        FakeHost with;
        with.takesTheClick = takes;
        qintptr place = kUntouched;
        const bool handledWithPlace = routeFrameMessage(with, number(message), wParam, 3, &place);
        QCOMPARE(place != kUntouched, handledWithPlace);

        // what the program does does not depend on whether Qt gave the place, except for a message that needs it
        if (deliveryOf(message) == Delivery::Queued) {
          QCOMPARE(handledWithNone, handledWithPlace);
          QCOMPARE(none.calls, with.calls);
        } else {
          QVERIFY(!handledWithNone);
          QVERIFY(none.calls.isEmpty());
          QVERIFY(handledWithPlace);
        }
      }
    }
  }
}

// A new filter that writes the answer itself brings the crash back; this is the rule that finds it where it is written
void FrameMessagesTests::source_aNativeEventFilter_goesThroughTheRules_andWritesNoAnswerItself()
{
  const QRegularExpression definition(QStringLiteral(R"(\bnativeEventFilter\s*\([^;{]*\)\s*(const\s*)?override)"));
  const QRegularExpression raw(QStringLiteral(R"(\*\s*result\s*=[^=])"));
  int filters = 0;
  for (const auto &path : guiSources()) {
    const auto text = sourceOf(path);
    if (text.isEmpty() || path.endsWith(QStringLiteral("chrome/FrameMessages.h")))
      continue;
    if (!definition.match(text).hasMatch())
      continue;
    ++filters;
    QVERIFY2(
        text.contains(QStringLiteral("routeFrameMessage(")),
        qPrintable(path + ": a native event filter has to go through routeFrameMessage")
    );
    QVERIFY2(
        !raw.match(text).hasMatch(),
        qPrintable(
            path +
            ": the answer is written through the pointer that Qt gave (it can be null for a message of the queue)"
        )
    );
  }
  // the frame of Windows has the one (if there is none the scan looks at nothing)
  QVERIFY2(filters >= 1, "no native event filter was found: the scan is not looking at the sources");
}

void FrameMessagesTests::source_theNumbersOfTheMessages_areCheckedAgainstTheHeadersOfWindows()
{
  QString windowsFile;
  for (const auto &path : guiSources()) {
    if (path.endsWith(QStringLiteral("chrome/WindowChromeWin.cpp")))
      windowsFile = path;
  }
  QVERIFY(!windowsFile.isEmpty());
  const auto text = sourceOf(windowsFile);
  QVERIFY(!text.isEmpty());
  const struct
  {
    const char *name;
    const char *macro;
  } pairs[] = {
      {"EraseBackground", "WM_ERASEBKGND"}, {"GetMinMaxInfo", "WM_GETMINMAXINFO"},
      {"NcCalcSize", "WM_NCCALCSIZE"},      {"NcHitTest", "WM_NCHITTEST"},
      {"NcMouseMove", "WM_NCMOUSEMOVE"},    {"NcLeftButtonDown", "WM_NCLBUTTONDOWN"},
      {"NcLeftButtonUp", "WM_NCLBUTTONUP"}, {"NcMouseLeave", "WM_NCMOUSELEAVE"},
  };
  for (const auto &pair : pairs) {
    const auto check =
        QStringLiteral("FrameMessage::%1) == %2").arg(QLatin1String(pair.name), QLatin1String(pair.macro));
    QVERIFY2(text.contains(check), qPrintable(QStringLiteral("WindowChromeWin.cpp does not check ") + check));
  }
  QVERIFY(text.contains(QStringLiteral("kHitMaximizeButton == HTMAXBUTTON")));
}

QTEST_MAIN(FrameMessagesTests)
