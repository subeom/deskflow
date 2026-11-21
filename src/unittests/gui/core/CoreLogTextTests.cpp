/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CoreLogTextTests.h"

#include "../../gui/KoreanTranslator.h"
#include "gui/core/CoreLogText.h"

#ifdef Q_OS_MACOS
#include "platform/OSXPointerHold.h"
#endif

#include <QCoreApplication>
#include <QTranslator>

using deskflow::gui::localizeCoreLogLine;

namespace {

const auto kStamp = QStringLiteral("[2026-11-21T13:30:00] WARNING: ");

const auto kMoved = QStringLiteral("the pointer of this Mac moved while it is on \"%1\": held it again");
const auto kShown = QStringLiteral("the pointer of this Mac was shown while it is on \"%1\": hid it again");
const auto kTapOff = QStringLiteral("the event tap was off while the pointer is on \"%1\": switched it on again");

// Answers for the sentences of the hold of the pointer only: the English put between T< and >
class MarkingTranslator : public QTranslator
{
public:
  QString translate(const char *context, const char *source, const char *, int) const override
  {
    if (context == nullptr || source == nullptr || QByteArray(context) != "CoreLogText") {
      return {};
    }
    return QStringLiteral("T<") + QString::fromUtf8(source) + QLatin1Char('>');
  }
  bool isEmpty() const override
  {
    return false;
  }
};

} // namespace

void CoreLogTextTests::cleanup()
{
  // every test that installs a translator takes it away
  const auto translators = QCoreApplication::instance()->findChildren<QTranslator *>();
  for (auto *translator : translators) {
    QCoreApplication::removeTranslator(translator);
    delete translator;
  }
}

void CoreLogTextTests::withoutATranslation_everyLineIsKeptAsItIs()
{
  for (const auto &sentence : {kMoved, kShown, kTapOff}) {
    const auto line =
        kStamp + QString(sentence).arg(QStringLiteral("win-pc")) + QStringLiteral(" (trigger: timer, times: 1)");
    QCOMPARE(localizeCoreLogLine(line), line);
  }
}

void CoreLogTextTests::otherLines_areNeverChanged()
{
  const QStringList lines = {
      QString(),
      QStringLiteral("[2026-11-21T13:30:00] INFO: entering computer"),
      QStringLiteral("[2026-11-21T13:30:00] WARNING: cursor may not be visible"),
      QStringLiteral("[2026-11-21T13:30:00] ERROR: quartz event tap was disabled by user input"),
      // the same words in another order, and a sentence that is cut short
      QStringLiteral("the pointer of this Mac moved while it is on"),
      QStringLiteral("the pointer of this Mac moved while it is on \"win-pc"),
      QStringLiteral("the pointer is on \"win-pc\""),
  };
  auto *translator = new MarkingTranslator;
  translator->setParent(QCoreApplication::instance());
  QVERIFY(QCoreApplication::installTranslator(translator));
  for (const auto &line : lines) {
    QCOMPARE(localizeCoreLogLine(line), line);
  }
}

void CoreLogTextTests::theKnownSentences_areTranslatedAndTheDetailsKept_data()
{
  QTest::addColumn<QString>("sentence");
  QTest::addColumn<QString>("details");
  QTest::addRow("moved") << kMoved << QStringLiteral("(moved 214 px, trigger: event, times: 1)");
  QTest::addRow("moved, with an application")
      << kMoved << QStringLiteral("(moved 5 px, trigger: app activated, times: 12, app: com.example.game)");
  QTest::addRow("shown") << kShown << QStringLiteral("(trigger: timer, times: 2)");
  QTest::addRow("tap off") << kTapOff << QStringLiteral("(trigger: tap disabled by user input, times: 3)");
}

void CoreLogTextTests::theKnownSentences_areTranslatedAndTheDetailsKept()
{
  QFETCH(QString, sentence);
  QFETCH(QString, details);

  auto *translator = new MarkingTranslator;
  translator->setParent(QCoreApplication::instance());
  QVERIFY(QCoreApplication::installTranslator(translator));

  const auto name = QStringLiteral("win-pc");
  const auto line = kStamp + QString(sentence).arg(name) + QLatin1Char(' ') + details;
  // what comes before the sentence and after it is kept; the name is put where %1 is; the English that was asked for is
  // the sentence of the core, character for character
  const auto expected = kStamp + QStringLiteral("T<") + QString(sentence).arg(name) + QStringLiteral("> ") + details;
  QCOMPARE(localizeCoreLogLine(line), expected);
}

void CoreLogTextTests::theKorean_hasEveryKnownSentence()
{
  auto *translator = new koreantest::RecordingTranslator;
  translator->setParent(QCoreApplication::instance());
  QVERIFY2(translator->loadShipped(), qPrintable(koreantest::shippedQm()));
  QVERIFY(QCoreApplication::installTranslator(translator));

  const auto details = QStringLiteral(" (moved 214 px, trigger: event, times: 1)");
  for (const auto &sentence : {kMoved, kShown, kTapOff}) {
    const auto line = kStamp + QString(sentence).arg(QStringLiteral("win-pc")) + details;
    const auto korean = localizeCoreLogLine(line);
    QVERIFY2(korean != line, qPrintable(line));
    QVERIFY(korean.startsWith(kStamp));
    QVERIFY(korean.endsWith(details));
    QVERIFY2(korean.contains(QStringLiteral("\"win-pc\"")), qPrintable(korean));
    QVERIFY2(
        !korean.contains(QStringLiteral("the pointer")) && !korean.contains(QStringLiteral("event tap")),
        qPrintable(korean)
    );
  }
  QVERIFY2(translator->missing().isEmpty(), qPrintable(translator->missing().join(QLatin1Char('\n'))));
}

#ifdef Q_OS_MACOS
void CoreLogTextTests::theLinesOfTheCore_areTheSentencesThatAreTranslated()
{
  using deskflow::platform::formatPointerHoldLog;
  using deskflow::platform::PointerHoldDecision;
  using deskflow::platform::PointerHoldTrigger;

  auto *translator = new MarkingTranslator;
  translator->setParent(QCoreApplication::instance());
  QVERIFY(QCoreApplication::installTranslator(translator));

  PointerHoldDecision moved;
  moved.moved = true;
  moved.distance = 214.0;
  moved.trigger = PointerHoldTrigger::AppActivated;
  moved.detections = 1;
  PointerHoldDecision shown;
  shown.wasShown = true;
  shown.detections = 2;
  PointerHoldDecision tapOff;
  tapOff.tapWasOff = true;
  tapOff.reenableTap = true;
  tapOff.detections = 3;

  for (const auto &decision : {moved, shown, tapOff}) {
    const auto line = kStamp + QString::fromStdString(formatPointerHoldLog(decision, "win-pc", "com.example.game"));
    const auto translated = localizeCoreLogLine(line);
    // a line that the GUI does not know stays as it is: T< is there only when the sentence of the core was recognised
    QVERIFY2(translated.startsWith(kStamp + QStringLiteral("T<")), qPrintable(line));
    QVERIFY2(translated.contains(QStringLiteral("app: com.example.game)")), qPrintable(translated));
  }
}
#endif

QTEST_MAIN(CoreLogTextTests)
