/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

// The line of the log of the core about the hold of the pointer of a Mac server, in the language of the program
class CoreLogTextTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void cleanup();
  void withoutATranslation_everyLineIsKeptAsItIs();
  void otherLines_areNeverChanged();
  void theKnownSentences_areTranslatedAndTheDetailsKept_data();
  void theKnownSentences_areTranslatedAndTheDetailsKept();
  void theKorean_hasEveryKnownSentence();
#ifdef Q_OS_MACOS
  void theLinesOfTheCore_areTheSentencesThatAreTranslated();
#endif
};
