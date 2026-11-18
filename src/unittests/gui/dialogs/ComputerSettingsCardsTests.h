/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>
#include <QTemporaryDir>

// The settings of one computer as cards (the new look): every control of the form is in a row, the dialog saves what
// the form saved and is as small as it was, and the System look is the form as it was
class ComputerSettingsCardsTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void cleanup();

  void systemLook_isTheFormAsItWas();
  void deskflowLook_hasACardForEachGroup_andHidesTheGroups();
  void deskflowLook_everyControlIsInARowOfACard();
  void deskflowLook_savesWhatTheFormSaved();
  void deskflowLook_showsTheErrorOfTheNameAndOfTheAliasInTheirRows();
  void deskflowLook_namesTheControlsByTheirRows();
  void deskflowLook_isAsSmallAsTheFormWas_andTheCardsScrollInIt_data();
  void deskflowLook_isAsSmallAsTheFormWas_andTheCardsScrollInIt();

private:
  QTemporaryDir m_settingsDir;
};
