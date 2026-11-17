/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TrayPolicyTests.h"

#include "gui/tray/TrayPolicy.h"

using namespace deskflow::gui::tray;

Q_DECLARE_METATYPE(CloseAction)

void TrayPolicyTests::closeAction_hidesToTheTrayOnlyWhereThereIsOne_data()
{
  QTest::addColumn<bool>("closeToTray");
  QTest::addColumn<bool>("trayAvailable");
  QTest::addColumn<CloseAction>("action");

  QTest::newRow("setting off, tray") << false << true << CloseAction::Quit;
  QTest::newRow("setting off, no tray") << false << false << CloseAction::Quit;
  QTest::newRow("setting on, tray") << true << true << CloseAction::HideToTray;
  // GNOME without the extension of the tray: the window would be hidden for good
  QTest::newRow("setting on, no tray") << true << false << CloseAction::Minimize;
}

void TrayPolicyTests::closeAction_hidesToTheTrayOnlyWhereThereIsOne()
{
  QFETCH(bool, closeToTray);
  QFETCH(bool, trayAvailable);
  QFETCH(CloseAction, action);
  QCOMPARE(closeActionFor(closeToTray, trayAvailable), action);
}

void TrayPolicyTests::click_togglesTheWindowExceptOnMacAndGnome_data()
{
  QTest::addColumn<bool>("mac");
  QTest::addColumn<QString>("desktop");
  QTest::addColumn<bool>("toggles");

  QTest::newRow("windows (no desktop variable)") << false << QString() << true;
  QTest::newRow("KDE") << false << "KDE" << true;
  QTest::newRow("XFCE") << false << "XFCE" << true;
  QTest::newRow("GNOME") << false << "GNOME" << false;
  QTest::newRow("Ubuntu GNOME") << false << "ubuntu:GNOME" << false;
  QTest::newRow("lowercase") << false << "gnome" << false;
  QTest::newRow("GNOME in a name that is longer") << false << "GNOME-Flashback:GNOME" << false;
  QTest::newRow("a name that only contains it") << false << "GNOMEISH" << true;
  QTest::newRow("macOS") << true << QString() << false;
  QTest::newRow("macOS with a desktop variable") << true << "KDE" << false;
}

void TrayPolicyTests::click_togglesTheWindowExceptOnMacAndGnome()
{
  QFETCH(bool, mac);
  QFETCH(QString, desktop);
  QFETCH(bool, toggles);
  QCOMPARE(clickTogglesWindow(mac, desktop), toggles);
}

void TrayPolicyTests::trayClick_canCrashOnMacOS27WithQtBefore6113_data()
{
  QTest::addColumn<QString>("qt");
  QTest::addColumn<int>("macOs");
  QTest::addColumn<bool>("crashes");

  QTest::newRow("6.9.3 (the Intel build), macOS 27") << "6.9.3" << 27 << true;
  QTest::newRow("6.9.3, macOS 26") << "6.9.3" << 26 << false;
  QTest::newRow("6.9.3, macOS 15") << "6.9.3" << 15 << false;
  QTest::newRow("6.10.3, macOS 27") << "6.10.3" << 27 << true;
  QTest::newRow("6.11.2, macOS 27") << "6.11.2" << 27 << true;
  QTest::newRow("6.11.3, macOS 27") << "6.11.3" << 27 << false;
  QTest::newRow("6.12.0, macOS 27") << "6.12.0" << 27 << false;
  QTest::newRow("6.12.0, macOS 28") << "6.12.0" << 28 << false;
  QTest::newRow("6.7.3, macOS 28") << "6.7.3" << 28 << true;
  QTest::newRow("6.9.3, not macOS") << "6.9.3" << 0 << false;
}

void TrayPolicyTests::trayClick_canCrashOnMacOS27WithQtBefore6113()
{
  QFETCH(QString, qt);
  QFETCH(int, macOs);
  QFETCH(bool, crashes);
  QCOMPARE(trayClickCanCrash(QVersionNumber::fromString(qt), macOs), crashes);
}

QTEST_APPLESS_MAIN(TrayPolicyTests)
