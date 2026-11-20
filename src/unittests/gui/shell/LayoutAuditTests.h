/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTemporaryDir>
#include <QTest>

// The main window of the new look at every size that it can have: nothing in it covers a sibling, nothing is cut off by
// its parent, nothing has less room than it asks for, and a text that wraps uses as few lines as its room allows. It is
// walked page by page, at the smallest size that the window gives itself and at larger ones, with the sizes of text of
// the setting (90 to 150 %), the font floor of a Mac (13 px) and of Windows (14 px), in English and in Korean, as a
// server and as a client. On a Mac the window has the title bar of the system under its content (emulated: see
// layoutaudit::emulateMacTitleBar), which is what gave the minimum size less room than the content needs.
class LayoutAuditTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void init();
  void cleanup();

  void windowFits_atItsLeastSizeAndLarger_data();
  void windowFits_atItsLeastSizeAndLarger();
  void addressLabel_usesAsFewLinesAsItsRoomAllows_data();
  void addressLabel_usesAsFewLinesAsItsRoomAllows();
  void theAuditSeesWhatItIsMadeToSee();

private:
  QTemporaryDir m_dir;
};
