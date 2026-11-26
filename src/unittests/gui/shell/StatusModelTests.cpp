/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "StatusModelTests.h"

#include "gui/shell/StatusModel.h"

#include <QSignalSpy>

using namespace deskflow::gui;
using deskflow::core::ConnectionState;
using deskflow::core::ProcessState;
using Kind = StatusChip::Kind;

Q_DECLARE_METATYPE(ConnectionState)
Q_DECLARE_METATYPE(ProcessState)
Q_DECLARE_METATYPE(StatusChip::Kind)

void StatusModelTests::headline_saysTheStateOfTheCore_data()
{
  QTest::addColumn<ProcessState>("process");
  QTest::addColumn<ConnectionState>("connection");
  QTest::addColumn<bool>("server");
  QTest::addColumn<Kind>("kind");
  QTest::addColumn<QString>("headline");
  QTest::addColumn<QString>("icon");

  using enum ProcessState;
  using enum ConnectionState;
  QTest::newRow("server stopped") << Stopped << Disconnected << true << Kind::Neutral << "Not sharing" << "power";
  QTest::newRow("client stopped") << Stopped << Disconnected << false << Kind::Neutral << "Not connected" << "power";
  QTest::newRow("starting") << Starting << Disconnected << true << Kind::Info << "Starting…" << "arrow-clockwise";
  QTest::newRow("retry") << RetryPending << Disconnected << false << Kind::Warning << "Will try again"
                         << "arrow-clockwise";
  QTest::newRow("stopping") << Stopping << Disconnected << true << Kind::Neutral << "Stopping…" << "power";
  QTest::newRow("server listening") << Started << Listening << true << Kind::Info << "Waiting for clients"
                                    << "wifi-high";
  QTest::newRow("client connecting") << Started << Connecting << false << Kind::Info << "Connecting…"
                                     << "arrow-clockwise";
  QTest::newRow("client connected") << Started << Connected << false << Kind::Success << "Connected"
                                    << "plugs-connected";
  QTest::newRow("disconnected") << Started << Disconnected << false << Kind::Warning << "Disconnected" << "warning";
}

void StatusModelTests::headline_saysTheStateOfTheCore()
{
  QFETCH(ProcessState, process);
  QFETCH(ConnectionState, connection);
  QFETCH(bool, server);
  QFETCH(Kind, kind);
  QFETCH(QString, headline);
  QFETCH(QString, icon);

  StatusModel model;
  model.setCore(connection, process, server);
  QCOMPARE(model.view().headline, headline);
  QCOMPARE(model.view().kind, kind);
  QCOMPARE(model.view().icon, icon);
}

void StatusModelTests::sharingWith_countsTheComputersThatAreConnected()
{
  StatusModel model;
  model.setCore(ConnectionState::Connected, ProcessState::Started, true);
  // the core says it is connected before the list is known: one computer at least
  QCOMPARE(model.view().headline, QStringLiteral("Sharing with 1 computer"));
  model.setClients({QStringLiteral("Desk-PC")});
  QCOMPARE(model.view().headline, QStringLiteral("Sharing with 1 computer"));
  model.setClients({QStringLiteral("Desk-PC"), QStringLiteral("Laptop")});
  QCOMPARE(model.view().headline, QStringLiteral("Sharing with 2 computers"));
  QCOMPARE(model.view().kind, Kind::Success);
  QCOMPARE(model.view().icon, QStringLiteral("plugs-connected"));
}

void StatusModelTests::detail_isTheSentenceThatWasGiven()
{
  StatusModel model;
  model.setDetail(QStringLiteral("Deskflow is not running"));
  QCOMPARE(model.view().detail, QStringLiteral("Deskflow is not running"));
  model.setCore(ConnectionState::Listening, ProcessState::Started, true);
  QCOMPARE(model.view().detail, QStringLiteral("Deskflow is not running"));
}

void StatusModelTests::securityChip_isShownWhileThereIsAnEncryptionToTellOf()
{
  StatusModel model;
  QVERIFY(model.view().chips.isEmpty());
  model.setSecurity(true, true, QStringLiteral("TLS Encryption Enabled"));
  QCOMPARE(model.view().chips.size(), 1);
  QCOMPARE(model.view().chips[0].key, StatusModel::securityKey());
  QCOMPARE(model.view().chips[0].text, QStringLiteral("TLS Encryption Enabled"));
  QCOMPARE(model.view().chips[0].kind, Kind::Success);

  // without an encryption it is a warning that the connection is open
  model.setSecurity(true, false, QStringLiteral("Encryption Disabled"));
  QCOMPARE(model.view().chips[0].kind, Kind::Warning);
  model.setSecurity(false, false, QString());
  QVERIFY(model.view().chips.isEmpty());
}

void StatusModelTests::externalChips_comeAfterTheChipsOfTheCore_byTheirOwnKey()
{
  StatusModel model;
  QSignalSpy changed(&model, &StatusModel::changed);
  model.setExternalChip(QStringLiteral("tunnel"), QStringLiteral("Tunnel on"), Kind::Info);
  QCOMPARE(changed.size(), 1);
  model.setSecurity(true, true, QStringLiteral("TLS"));
  model.setExternalChip(QStringLiteral("other"), QStringLiteral("Other"), Kind::Neutral);
  QCOMPARE(model.view().chips.size(), 3);
  // the chip of the core is first whatever came first
  QCOMPARE(model.view().chips[0].key, StatusModel::securityKey());
  QCOMPARE(model.view().chips[1].key, QStringLiteral("tunnel"));
  QCOMPARE(model.view().chips[2].key, QStringLiteral("other"));

  // the same key changes the chip that it has, and does not add one
  model.setExternalChip(QStringLiteral("tunnel"), QStringLiteral("Tunnel failed"), Kind::Danger);
  QCOMPARE(model.view().chips.size(), 3);
  QCOMPARE(model.view().chips[1].text, QStringLiteral("Tunnel failed"));
  QCOMPARE(model.view().chips[1].kind, Kind::Danger);

  model.clearExternalChip(QStringLiteral("tunnel"));
  QCOMPARE(model.view().chips.size(), 2);
  QCOMPARE(model.view().chips[1].key, QStringLiteral("other"));
  // a key that is not there is nothing
  changed.clear();
  model.clearExternalChip(QStringLiteral("nothing"));
  QCOMPARE(changed.size(), 0);

  // the core going to another state leaves what others said
  model.setCore(ConnectionState::Listening, ProcessState::Started, true);
  QCOMPARE(model.view().chips.size(), 2);
}

void StatusModelTests::error_winsUntilTheCoreComesToAnotherState()
{
  StatusModel model;
  model.setSecurity(true, true, QStringLiteral("TLS"));
  model.setCore(ConnectionState::Disconnected, ProcessState::Stopped, true);
  model.setError(QStringLiteral("Cannot start"), QStringLiteral("The core could not be started"));
  QCOMPARE(model.view().kind, Kind::Danger);
  QCOMPARE(model.view().headline, QStringLiteral("Cannot start"));
  QCOMPARE(model.view().detail, QStringLiteral("The core could not be started"));
  QCOMPARE(model.view().icon, QStringLiteral("x-circle"));
  // the encryption of a core that is not running is not told while it is an error
  QVERIFY(model.view().chips.isEmpty());

  model.setCore(ConnectionState::Disconnected, ProcessState::Starting, true);
  QCOMPARE(model.view().headline, QStringLiteral("Starting…"));

  model.setError(QStringLiteral("Cannot start"), QString());
  model.clearError();
  QCOMPARE(model.view().headline, QStringLiteral("Starting…"));
}

void StatusModelTests::changed_isOnlySaidWhenWhatIsShownChanges()
{
  StatusModel model;
  QSignalSpy changed(&model, &StatusModel::changed);
  model.setCore(ConnectionState::Disconnected, ProcessState::Stopped, true);
  // the same state as it was: nothing to say
  QCOMPARE(changed.size(), 0);
  model.setCore(ConnectionState::Listening, ProcessState::Started, true);
  QCOMPARE(changed.size(), 1);
  model.setCore(ConnectionState::Listening, ProcessState::Started, true);
  QCOMPARE(changed.size(), 1);
  model.setClients({});
  model.setDetail(QString());
  model.setSecurity(false, false, QString());
  model.clearError();
  QCOMPARE(changed.size(), 1);
  model.setDetail(QStringLiteral("x"));
  QCOMPARE(changed.size(), 2);
}

QTEST_MAIN(StatusModelTests)
