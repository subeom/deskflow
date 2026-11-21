/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UpdateStatusModelTests.h"

#include "gui/UpdateStatusModel.h"

#include <QSignalSpy>

using deskflow::gui::UpdateAction;
using deskflow::gui::UpdateStatusModel;
using UpdatePhase = UpdateStatusSnapshot::Phase;

namespace {

UpdateStatusSnapshot snapshot(UpdatePhase phase, bool installable = false)
{
  UpdateStatusSnapshot value;
  value.phase = phase;
  value.version = QStringLiteral("2.0.6");
  value.releaseUrl =
      QUrl(QStringLiteral("https://github.com/subeom/deskflow/releases/tag/v2.0.6"));
  value.installable = installable;
  return value;
}

} // namespace

void UpdateStatusModelTests::states_mapToOneVisibleAction()
{
  UpdateStatusModel model;

  model.setStatus(snapshot(UpdatePhase::ManualAvailable));
  QCOMPARE(model.view().action, UpdateAction::OpenRelease);
  QVERIFY(model.view().visible);

  model.setStatus(snapshot(UpdatePhase::Staged, true));
  QCOMPARE(model.view().action, UpdateAction::ConfirmInstall);
  QVERIFY(model.view().installable);

  model.setStatus(snapshot(UpdatePhase::CheckingRelease));
  QCOMPARE(model.view().action, UpdateAction::None);
  QVERIFY(model.view().visible);
  QVERIFY(!model.view().enabled);

  model.setStatus(snapshot(UpdatePhase::UpToDate));
  QVERIFY(!model.view().visible);
}

void UpdateStatusModelTests::failedState_clearsStagedAndInstallableState()
{
  UpdateStatusModel model;
  model.setStatus(snapshot(UpdatePhase::Staged, true));
  auto failed = snapshot(UpdatePhase::Failed);
  failed.userError = QStringLiteral("signature rejected");
  model.setStatus(failed);
  QCOMPARE(model.view().action, UpdateAction::Retry);
  QVERIFY(!model.view().installable);
  QVERIFY(model.view().toolTip.contains(failed.userError));
}

void UpdateStatusModelTests::translations_useBoundedLocalizedText()
{
  UpdateStatusModel model;
  model.setStatus(snapshot(UpdatePhase::ManualAvailable));
  QCOMPARE(model.view().label, UpdateStatusModel::tr("View update"));

  model.setStatus(snapshot(UpdatePhase::Applying));
  QCOMPARE(model.view().label, UpdateStatusModel::tr("Installing update..."));
}

void UpdateStatusModelTests::unchangedState_doesNotEmitNoise()
{
  UpdateStatusModel model;
  QSignalSpy changed(&model, &UpdateStatusModel::changed);
  const auto value = snapshot(UpdatePhase::ManualAvailable);
  model.setStatus(value);
  model.setStatus(value);
  QCOMPARE(changed.count(), 1);
}

QTEST_MAIN(UpdateStatusModelTests)
