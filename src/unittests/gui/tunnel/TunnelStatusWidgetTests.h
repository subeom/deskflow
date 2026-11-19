/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QTest>

class TunnelStatusWidgetTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();

  void thePathLine_namesTheHostAndTheLocalPort();
  void theStatusLine_keepsThePortAndTheServerApart();

  void whileTheTunnelIsOff_theWidgetIsHiddenAndWritesNothing();
  void whileTheTunnelIsOn_theWidgetSaysWhichHostIsDialled();
  void theWidget_followsTheStateOfTheTunnelAndOfTheServerLink();
  void aPortThatIsInUseInsteadOfTheAskedOne_isShown();
  void theLines_doNotWrapAndHaveTheirTextAsToolTip();

private:
  QString m_dir;
};
