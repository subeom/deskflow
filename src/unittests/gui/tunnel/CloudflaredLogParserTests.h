/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QTest>

class CloudflaredLogParserTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void line_isRecognisedInTheConsoleAndTheJsonFormat_data();
  void line_isRecognisedInTheConsoleAndTheJsonFormat();
  void connectionFailure_saysWhyFromTheWordsOfTheError_data();
  void connectionFailure_saysWhyFromTheWordsOfTheError();
  void level_isReadFromBothFormats_data();
  void level_isReadFromBothFormats();
  void unknownLines_areOtherAndNeverAnError_data();
  void unknownLines_areOtherAndNeverAnError();
  void message_hasNoTerminalEscapesOrControlCharactersAndIsCut();
};
