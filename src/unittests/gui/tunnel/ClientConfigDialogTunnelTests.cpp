/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ClientConfigDialogTunnelTests.h"

#include "common/Settings.h"
#include "gui/dialogs/ClientConfigDialog.h"
#include "gui/tunnel/InMemorySecretStore.h"
#include "gui/tunnel/SecretStoreFactory.h"
#include "gui/widgets/SettingsDialogButtonBox.h"
#include "gui/widgets/TunnelSettingsGroup.h"

#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>

using namespace deskflow::gui;
using namespace deskflow::tunnel;

namespace {

const auto kId = QStringLiteral("TESTID0001a1b2c3.access");
const auto kSecret = QStringLiteral("TESTSECRET-do-not-leak-0002");
const auto kBase = QByteArray(
    "[client]\nremoteHost=10.0.0.5, 100.64.0.7\nrecentRemoteHosts=10.0.0.5, 100.64.0.7\n\n"
    "[core]\ncomputerName=test-pc\n\n[log]\nlevel=Info\n\n[security]\ntlsEnabled=true\n"
);

QTemporaryDir &scratch()
{
  static QTemporaryDir dir;
  return dir;
}

QByteArray readFile(const QString &path)
{
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray("<unreadable>");
}

QString useConfig(const QByteArray &content = kBase)
{
  static int counter = 0;
  const auto dir = QStringLiteral("%1/c%2").arg(scratch().path()).arg(++counter);
  QDir().mkpath(dir);
  const auto file = QStringLiteral("%1/Deskflow.conf").arg(dir);
  QFile out(file);
  if (out.open(QIODevice::WriteOnly)) {
    out.write(content);
  }
  out.close();
  Settings::setSettingsFile(file);
  Settings::setStateFile(QStringLiteral("%1/Deskflow.state").arg(dir));
  return file;
}

TunnelSettingsDependencies dependencies(const std::shared_ptr<ISecretStore> &store)
{
  TunnelSettingsDependencies deps;
  deps.secretStore = store;
  deps.locate = [](const QString &) {
    return CloudflaredLocation{CloudflaredLocation::Source::Path, QStringLiteral("/usr/local/bin/cloudflared"), false};
  };
  deps.confirmRemoval = [] { return true; };
  deps.tlsEnabled = [] { return true; };
  return deps;
}

// the "Save" of the dialog
void pressSave(ClientConfigDialog &dialog)
{
  auto *box = dialog.findChild<SettingsDialogButtonBox *>();
  QVERIFY(box != nullptr);
  Q_EMIT box->accepted();
}

// what the dialog did before the group of the tunnel was in it, as the writes that it made
void writeWhatTheDialogAlwaysWrote()
{
  Settings::setValue(Settings::Client::DynamicConnectionRetry, false);
  Settings::setValue(Settings::Client::LanguageSync, false);
  Settings::setValue(Settings::Client::ConnectTimeout, 2.0);
  Settings::setValue(Settings::Client::InvertYScroll, false);
  Settings::setValue(Settings::Client::YScrollScale, 1.0);
  Settings::setValue(Settings::Client::InvertXScroll, false);
  Settings::setValue(Settings::Client::XScrollScale, 1.0);
}

} // namespace

void ClientConfigDialogTunnelTests::initTestCase()
{
  QVERIFY(scratch().isValid());
  // whatever a dialog is made without a store of its own: the one in memory, never a keychain
  qputenv(kSecretStoreVariable, "memory");
}

void ClientConfigDialogTunnelTests::save_withTheTunnelUntouched_writesExactlyWhatTheDialogWroteBefore()
{
  const auto store = std::make_shared<InMemorySecretStore>();

  // with the dialog as it is now
  const auto withGroup = useConfig();
  {
    ClientConfigDialog dialog(nullptr, dependencies(store));
    pressSave(dialog);
    QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
  }
  const auto fromDialog = readFile(withGroup);

  // and the writes that the dialog made before the group was in it, on the same configuration
  const auto before = useConfig();
  writeWhatTheDialogAlwaysWrote();
  const auto fromOldDialog = readFile(before);

  QVERIFY2(fromOldDialog != kBase, "the old writes changed nothing, so the comparison says nothing");
  QCOMPARE(fromDialog, fromOldDialog);
  QVERIFY(!fromDialog.contains("tunnel"));
  QVERIFY(!QSettings(withGroup, QSettings::IniFormat).childGroups().contains(QStringLiteral("tunnel")));
}

void ClientConfigDialogTunnelTests::save_withTheTunnelUntouched_leavesTheSavedAddressesAndTheTunnelKeysAlone()
{
  const auto file = useConfig();
  ClientConfigDialog dialog(nullptr, dependencies(std::make_shared<InMemorySecretStore>()));
  pressSave(dialog);

  const QSettings conf(file, QSettings::IniFormat);
  QCOMPARE(Settings::textValue(Settings::Client::RemoteHost), QStringLiteral("10.0.0.5,100.64.0.7"));
  QVERIFY(conf.contains(QStringLiteral("client/remoteHost")));
  QVERIFY(conf.contains(QStringLiteral("client/recentRemoteHosts")));
  for (const auto &key : conf.allKeys()) {
    QVERIFY2(!key.startsWith(QStringLiteral("tunnel/")), qPrintable(key));
  }
}

void ClientConfigDialogTunnelTests::save_withATunnelThatCannotBeSaved_staysOpenAndWritesNothing()
{
  const auto file = useConfig();
  const auto before = readFile(file);
  ClientConfigDialog dialog(nullptr, dependencies(std::make_shared<InMemorySecretStore>()));
  auto *group = dialog.findChild<TunnelSettingsGroup *>();
  QVERIFY(group != nullptr);

  // on, with a hostname that is not one
  group->findChild<QCheckBox *>(QStringLiteral("cbTunnelEnabled"))->setChecked(true);
  group->findChild<QLineEdit *>(QStringLiteral("leTunnelHostname"))->setText(QStringLiteral("10.0.0.5"));
  pressSave(dialog);

  QVERIFY(dialog.result() != static_cast<int>(QDialog::Accepted));
  auto *error = group->findChild<QLabel *>(QStringLiteral("lblTunnelError"));
  QVERIFY(error != nullptr && !error->isHidden());
  QVERIFY(error->text().contains(QStringLiteral("valid tunnel hostname")));
  // not even the settings of the client that the dialog writes otherwise
  QCOMPARE(readFile(file), before);
}

void ClientConfigDialogTunnelTests::save_withATunnel_writesTheGroupAndAcceptsTheDialog()
{
  const auto file = useConfig();
  const auto store = std::make_shared<InMemorySecretStore>();
  ClientConfigDialog dialog(nullptr, dependencies(store));
  auto *group = dialog.findChild<TunnelSettingsGroup *>();
  QVERIFY(group != nullptr);

  group->findChild<QLineEdit *>(QStringLiteral("leTunnelHostname"))->setText(QStringLiteral("mbp.example.com"));
  group->findChild<QLineEdit *>(QStringLiteral("leTokenClientId"))->setText(kId);
  group->findChild<QLineEdit *>(QStringLiteral("leTokenSecret"))->setText(kSecret);
  group->findChild<QCheckBox *>(QStringLiteral("cbTunnelEnabled"))->setChecked(true);
  pressSave(dialog);

  QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
  const QSettings conf(file, QSettings::IniFormat);
  QCOMPARE(conf.value(QStringLiteral("tunnel/enabled")).toBool(), true);
  QCOMPARE(conf.value(QStringLiteral("tunnel/hostname")).toString(), QStringLiteral("mbp.example.com"));
  QCOMPARE(store->state()->token->secret, kSecret);
  // the saved addresses of the server are as they were, and the secret is nowhere in the file
  QCOMPARE(conf.value(QStringLiteral("client/remoteHost")).toStringList(), (QStringList{"10.0.0.5", "100.64.0.7"}));
  const auto text = readFile(file);
  QVERIFY(!text.contains(kSecret.toUtf8()));
  QVERIFY(!text.contains(kId.toUtf8()));
}

void ClientConfigDialogTunnelTests::save_thatTheStoreRefuses_staysOpen()
{
  const auto file = useConfig();
  const auto store = std::make_shared<InMemorySecretStore>();
  ClientConfigDialog dialog(nullptr, dependencies(store));
  auto *group = dialog.findChild<TunnelSettingsGroup *>();
  QVERIFY(group != nullptr);

  group->findChild<QLineEdit *>(QStringLiteral("leTokenClientId"))->setText(kId);
  group->findChild<QLineEdit *>(QStringLiteral("leTokenSecret"))->setText(kSecret);
  store->state()->failWith = QStringLiteral("the keychain is locked");
  pressSave(dialog);

  QVERIFY(dialog.result() != static_cast<int>(QDialog::Accepted));
  QVERIFY(!QSettings(file, QSettings::IniFormat).contains(QStringLiteral("tunnel/enabled")));
  QVERIFY(!readFile(file).contains(kSecret.toUtf8()));
}

void ClientConfigDialogTunnelTests::restoreDefaults_resetsTheGroupToo()
{
  useConfig(
      "[core]\ncomputerName=test-pc\n\n[log]\nlevel=Info\n\n[tunnel]\nenabled=true\nhostname=mbp.example.com\n"
      "localPort=24801\n"
  );
  ClientConfigDialog dialog(nullptr, dependencies(std::make_shared<InMemorySecretStore>()));
  auto *group = dialog.findChild<TunnelSettingsGroup *>();
  QVERIFY(group != nullptr);
  QVERIFY(!group->isDefault());

  auto *box = dialog.findChild<SettingsDialogButtonBox *>();
  QVERIFY(box != nullptr);
  Q_EMIT box->restoreDefault();

  QVERIFY(group->isDefault());
  QVERIFY(!group->findChild<QCheckBox *>(QStringLiteral("cbTunnelEnabled"))->isChecked());
  QCOMPARE(group->findChild<QSpinBox *>(QStringLiteral("sbTunnelLocalPort"))->value(), 24800);
}

void ClientConfigDialogTunnelTests::theGroup_isModifiedMakesTheDialogSaveable()
{
  useConfig();
  ClientConfigDialog dialog(nullptr, dependencies(std::make_shared<InMemorySecretStore>()));
  auto *group = dialog.findChild<TunnelSettingsGroup *>();
  QVERIFY(group != nullptr);

  // the Save button is enabled when something is changed: a change in the group is one
  auto *box = dialog.findChild<SettingsDialogButtonBox *>();
  QVERIFY(box != nullptr);
  QVERIFY(!group->isModified());
  group->findChild<QSpinBox *>(QStringLiteral("sbTunnelLocalPort"))->setValue(24801);
  QVERIFY(group->isModified());
}

QTEST_MAIN(ClientConfigDialogTunnelTests)
