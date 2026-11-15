/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "StatusBarTests.h"

#include "gui/UpdateCache.h"
#include "gui/widgets/StatusBar.h"

#include <QPushButton>
#include <QSignalSpy>
#include <QTest>

namespace {

QPushButton *textButton(StatusBar &statusBar)
{
  for (auto *button : statusBar.findChildren<QPushButton *>()) {
    if (!button->text().isEmpty() && button->isVisible()) {
      return button;
    }
  }
  return nullptr;
}

} // namespace

void StatusBarTests::manualUpdate_usesExactReleaseUrl()
{
  StatusBar statusBar;
  statusBar.show();
  QSignalSpy requested(&statusBar, &StatusBar::requestUpdateVersion);

  statusBar.updateFound(
      QStringLiteral("1.28.0"), QStringLiteral("https://github.com/deskflow/deskflow/releases/tag/v1.28.0")
  );

  auto *button = textButton(statusBar);
  QVERIFY(button);
  QCOMPARE(button->text(), QStringLiteral("Update available"));
  button->click();
  QCOMPARE(requested.count(), 1);
  QCOMPARE(
      requested.first().at(0).toString(), QStringLiteral("https://github.com/deskflow/deskflow/releases/tag/v1.28.0")
  );
}

void StatusBarTests::stagedUpdate_offersExplicitInstallAction()
{
  StatusBar statusBar;
  statusBar.show();
  QSignalSpy requested(&statusBar, &StatusBar::requestUpdateInstall);
  const deskflow::gui::StagedUpdate staged{
      QStringLiteral("1.28.0"), QStringLiteral("deskflow.dmg"), QStringLiteral("/tmp/deskflow.dmg"),
      QStringLiteral("/tmp/manifest.json")
  };

  statusBar.updateStaged(staged);

  auto *button = textButton(statusBar);
  QVERIFY(button);
  QCOMPARE(button->text(), QStringLiteral("Install update"));
  button->click();
  QCOMPARE(requested.count(), 1);
  QCOMPARE(requested.first().at(0).value<deskflow::gui::StagedUpdate>().version, staged.version);
}

void StatusBarTests::updateError_neverClaimsAnUpdateIsStaged()
{
  StatusBar statusBar;
  statusBar.show();
  statusBar.updateStaged(deskflow::gui::StagedUpdate{});
  statusBar.updateFailed(QStringLiteral("signature verification failed"));

  auto *button = textButton(statusBar);
  QVERIFY(button);
  QCOMPARE(button->text(), QStringLiteral("Update check failed"));
  QVERIFY(button->toolTip().contains(QStringLiteral("signature verification failed")));
}

QTEST_MAIN(StatusBarTests)
