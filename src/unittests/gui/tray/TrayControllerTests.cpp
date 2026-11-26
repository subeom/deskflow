/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TrayControllerTests.h"

#include "TrayTestData.h"

#include "gui/tray/TrayController.h"
#include "gui/tray/TrayMenuBuilder.h"

#include <QAction>
#include <QMenu>
#include <QSignalSpy>
#include <QSystemTrayIcon>

using namespace deskflow::gui::tray;
using deskflow::core::ConnectionState;
using deskflow::core::ProcessState;
using tray_test::makeInput;

Q_DECLARE_METATYPE(TrayCommand)

void TrayControllerTests::initTestCase()
{
  qRegisterMetaType<TrayCommand>();
}

void TrayControllerTests::input_makesTheMenuTheIconAndTheToolTip()
{
  QSystemTrayIcon icon;
  QMenu menu;
  TrayController controller(&icon, &menu, TrayPlatform::mac());
  auto input = makeInput(ProcessState::Started, ConnectionState::Connected);
  input.headline = QStringLiteral("Sharing with 2 computers");
  controller.setInput(input);

  QCOMPARE(controller.state(), TrayState::Connected);
  QCOMPARE(icon.toolTip(), QStringLiteral("Deskflow: Sharing with 2 computers"));
  QVERIFY(!icon.icon().isNull());
  QCOMPARE(controller.iconSpec().state, TrayState::Connected);
  QVERIFY(menu.actions().size() > 5);
  QCOMPARE(menu.actions().first()->text(), QStringLiteral("Sharing with 2 computers"));

  input.process = ProcessState::Stopped;
  input.headline = QStringLiteral("Not sharing");
  controller.setInput(input);
  QCOMPARE(controller.state(), TrayState::Stopped);
  QCOMPARE(icon.toolTip(), QStringLiteral("Deskflow: Not sharing"));
  QCOMPARE(controller.iconSpec().state, TrayState::Stopped);
}

void TrayControllerTests::theSameInput_changesNothing_andDoesNotSetTheToolTipAgain()
{
  QSystemTrayIcon icon;
  QMenu menu;
  TrayController controller(&icon, &menu, TrayPlatform::mac());
  const auto input = makeInput(ProcessState::Started, ConnectionState::Listening);
  controller.setInput(input);
  const auto rows = menu.actions();

  // macOS 27.2 leaves a tool tip on the screen when it is set again and again: a text that has not changed is not set
  icon.setToolTip(QStringLiteral("set by someone else"));
  controller.setInput(input);
  QCOMPARE(icon.toolTip(), QStringLiteral("set by someone else"));
  QCOMPARE(menu.actions(), rows);

  auto other = input;
  other.headline = QStringLiteral("Another headline");
  controller.setInput(other);
  QCOMPARE(icon.toolTip(), QStringLiteral("Deskflow: Another headline"));
}

void TrayControllerTests::externalRow_isShownUnderTheHeader_andTakenAway()
{
  QSystemTrayIcon icon;
  QMenu menu;
  TrayController controller(&icon, &menu, TrayPlatform::mac());
  // before the first input there is nothing to show: the row waits
  controller.setExternalRow(QStringLiteral("tunnel"), QStringLiteral("Tunnel: starting"));
  QVERIFY(menu.actions().isEmpty());

  controller.setInput(makeInput(ProcessState::Stopped, ConnectionState::Disconnected));
  auto *row = controller.builder()->actionFor(QStringLiteral("external:tunnel"));
  QVERIFY(row);
  QCOMPARE(row->text(), QStringLiteral("Tunnel: starting"));
  QVERIFY(!row->isEnabled());
  // the header is two rows (the state and its sentence), the row of the feature is the third
  QCOMPARE(menu.actions().indexOf(row), 2);

  // the text is said again when the feature says it again, and the row goes when the text does
  controller.setExternalRow(QStringLiteral("tunnel"), QStringLiteral("Tunnel: up"));
  QCOMPARE(controller.builder()->actionFor(QStringLiteral("external:tunnel"))->text(), QStringLiteral("Tunnel: up"));
  controller.setExternalRow(QStringLiteral("tunnel"), QString());
  QVERIFY(!controller.builder()->actionFor(QStringLiteral("external:tunnel")));
}

void TrayControllerTests::commandsOfTheMenu_areToldToTheOwner()
{
  QSystemTrayIcon icon;
  QMenu menu;
  TrayController controller(&icon, &menu, TrayPlatform::mac());
  QSignalSpy spy(&controller, &TrayController::commandRequested);
  controller.setInput(makeInput(ProcessState::Started, ConnectionState::Connected));
  controller.builder()->actionFor(QStringLiteral("command:restart"))->trigger();
  QCOMPARE(spy.size(), 1);
  QCOMPARE(spy.first().first().value<TrayCommand>(), TrayCommand::Restart);
}

void TrayControllerTests::colourAndTone_choosTheIcon()
{
  QSystemTrayIcon icon;
  QMenu menu;
  TrayController controller(&icon, &menu, TrayPlatform::windows());
  controller.setTone(TrayTone::OnDarkBar);
  controller.setInput(makeInput(ProcessState::Started, ConnectionState::Connected));
  QCOMPARE(controller.iconSpec().tone, TrayTone::OnDarkBar);
  QVERIFY(!controller.iconSpec().coloredBadge);
  QVERIFY(!icon.icon().isMask());

  controller.setColoredIcon(true);
  QVERIFY(controller.iconSpec().coloredBadge);
  QCOMPARE(controller.iconSpec().state, TrayState::Connected);

  // a template (macOS) has no colour, whatever the setting is, and it is a mask for the OS
  controller.setTone(TrayTone::Template);
  QVERIFY(!controller.iconSpec().coloredBadge);
  QVERIFY(icon.icon().isMask());
  QCOMPARE(
      icon.icon().pixmap(QSize(22, 22)).toImage().convertedTo(QImage::Format_ARGB32_Premultiplied),
      trayIconImage({TrayState::Connected, false, TrayTone::Template, false}, 22)
  );

  // an update is the dot on the icon
  auto input = makeInput(ProcessState::Started, ConnectionState::Connected);
  input.updateVersion = QStringLiteral("2.0");
  controller.setInput(input);
  QVERIFY(controller.iconSpec().update);
}

void TrayControllerTests::iconOfALinuxDesktop_isAskedOfTheTheme()
{
  QSystemTrayIcon icon;
  QMenu menu;
  TrayController controller(&icon, &menu, TrayPlatform::linux());
  controller.setTone(TrayTone::OnDarkBar);
  controller.setInput(makeInput(ProcessState::Stopped, ConnectionState::Disconnected));
  // no theme here has the icon of the name: the picture that is drawn is what the desktop gets
  QVERIFY(!icon.icon().isNull());
  QCOMPARE(controller.iconSpec().state, TrayState::Stopped);
  QCOMPARE(trayIconName(controller.iconSpec()), QStringLiteral("org.deskflow.deskflow-stopped-symbolic"));
}

QTEST_MAIN(TrayControllerTests)
