/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "StatusBarTests.h"

#include "gui/widgets/StatusBar.h"

#include <QPushButton>
#include <QSignalSpy>

using deskflow::gui::UpdateAction;
using deskflow::gui::UpdateStatusView;

namespace {

QPushButton *button(StatusBar &bar)
{
  return bar.findChild<QPushButton *>(QStringLiteral("updateButton"));
}

} // namespace

void StatusBarTests::manualAndStaged_statesUseOneAction()
{
  StatusBar bar;
  bar.show();
  QCoreApplication::processEvents();
  auto *update = button(bar);
  QVERIFY(update);
  QSignalSpy requested(&bar, &StatusBar::requestUpdateAction);

  UpdateStatusView manual;
  manual.version = QStringLiteral("2.0.6");
  manual.label = QStringLiteral("View update");
  manual.toolTip = QStringLiteral("Release page");
  manual.visible = true;
  manual.enabled = true;
  manual.action = UpdateAction::OpenRelease;
  bar.setUpdateStatus(manual);
  QVERIFY(update->isVisible());
  QCOMPARE(update->text(), manual.label);
  QTest::mouseClick(update, Qt::LeftButton);
  QCOMPARE(requested.count(), 1);
  QCOMPARE(requested.first().at(0).value<UpdateAction>(), UpdateAction::OpenRelease);

  manual.action = UpdateAction::ConfirmInstall;
  manual.installable = true;
  manual.label = QStringLiteral("Install update");
  bar.setUpdateStatus(manual);
  QCOMPARE(update->text(), manual.label);
  QTest::mouseClick(update, Qt::LeftButton);
  QCOMPARE(requested.count(), 2);
  QCOMPARE(requested.last().at(0).value<UpdateAction>(), UpdateAction::ConfirmInstall);
}

void StatusBarTests::failedAndChecking_statesNeverLookInstallable()
{
  StatusBar bar;
  bar.show();
  QCoreApplication::processEvents();
  auto *update = button(bar);
  QVERIFY(update);

  UpdateStatusView checking;
  checking.label = QStringLiteral("Checking for updates...");
  checking.visible = true;
  checking.enabled = false;
  checking.action = UpdateAction::None;
  bar.setUpdateStatus(checking);
  QVERIFY(update->isVisible());
  QVERIFY(!update->isEnabled());
  QVERIFY(bar.updateVersion().isEmpty());

  checking.label = QStringLiteral("Update failed");
  checking.enabled = true;
  checking.toolTip = QStringLiteral("bounded error");
  checking.action = UpdateAction::Retry;
  bar.setUpdateStatus(checking);
  QVERIFY(update->isEnabled());
  QCOMPARE(update->toolTip(), checking.toolTip);
  QVERIFY(bar.updateVersion().isEmpty());
}

QTEST_MAIN(StatusBarTests)
