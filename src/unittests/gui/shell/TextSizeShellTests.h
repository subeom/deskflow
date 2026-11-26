/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTemporaryDir>
#include <QTest>

// The main window of the new look at every size of text: the keys, the choice in the settings, the sizes of the
// controls, and the layout at the least width (nothing cut, squeezed or beyond the window) with every step and with a
// wide font. The system is made up (a Windows with a font of 12 px, the size of Segoe UI 9 pt), and the core is never
// started.
class TextSizeShellTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void cleanup();

  void systemLook_hasNoKeysForTheSizeOfText();
  void keys_makeTheTextLargerAndSmaller_andGoBackToAutomatic();
  void theChoiceInTheSettings_followsTheKeys_andTheKeysFollowIt();

  void theControlsFollowTheText_andStayAsDrawnAtTheSizeOfTheFloor();
  void anOpenWindow_isLaidOutAsANewOneIs_whenTheTextSizeChanges();
  void theWindowAndTheSidebarFollowTheSizeOfText();

  void layout_keepsItsPromise_atEveryTextSize_data();
  void layout_keepsItsPromise_atEveryTextSize();
  void pages_areNotCut_atTheLeastWidth_atEveryTextSize_data();
  void pages_areNotCut_atTheLeastWidth_atEveryTextSize();

private:
  QTemporaryDir m_dir;
};
