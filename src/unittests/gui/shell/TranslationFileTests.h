/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

// The file of the Korean translation (translations/deskflow_ko.ts), read as a file. The build runs lupdate over the
// sources before these tests, and it adds every text that the sources have and the file has not as an unfinished
// message: so a text that was added to the program without its Korean is found here, by name. What each translation
// keeps of its English is what the program needs of it: the places of the values (%1, %n), the key of a menu (&), the
// breaks of a line, the marks of the HTML and the spaces at the ends.
class TranslationFileTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void file_hasAKoreanTextForEveryTextOfTheProgram();
  void file_keepsThePlacesOfTheValues();
  void file_keepsTheKeysOfTheMenus();
  void file_keepsTheBreaksOfTheLinesAndTheSpacesAtTheEnds();
  void file_keepsTheMarksOfTheHtml();
  void file_hasNoEmDash();
  void file_hasTheTextsThatTheRedesignAdded();
};
