/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TunnelSettingsGroupTests.h"

#include "common/Settings.h"
#include "gui/tunnel/CloudflaredLocator.h"
#include "gui/tunnel/InMemorySecretStore.h"
#include "gui/widgets/TunnelSettingsGroup.h"

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFocusEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QSpinBox>

using namespace deskflow::gui;
using namespace deskflow::tunnel;

namespace {

// made-up values that look nothing like a real token
const auto kId = QStringLiteral("TESTID0001a1b2c3.access");
const auto kSecret = QStringLiteral("TESTSECRET-do-not-leak-0002");
const auto kBase = QByteArray("[core]\ncomputerName=test-pc\n\n[log]\nlevel=Info\n\n[security]\ntlsEnabled=true\n");

QStringList g_messages;

void collectMessages(QtMsgType, const QMessageLogContext &, const QString &message)
{
  g_messages.append(message);
}

QByteArray readFile(const QString &path)
{
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray("<unreadable>");
}

// what the group asks its surroundings, set by the test
struct World
{
  bool tls = true;
  bool confirmAnswer = true;
  int confirmAsked = 0;
  QString lastPathAsked;
  CloudflaredLocation location{CloudflaredLocation::Source::Path, QStringLiteral("/usr/local/bin/cloudflared"), false};
};

// a store in memory that says it keeps the token for good, as the stores of Windows and macOS do
class PersistentTestStore : public InMemorySecretStore
{
public:
  using InMemorySecretStore::InMemorySecretStore;
  QString name() const override
  {
    return QStringLiteral("Test keychain");
  }
  bool isPersistent() const override
  {
    return true;
  }
};

TunnelSettingsDependencies dependencies(const std::shared_ptr<World> &world, std::shared_ptr<ISecretStore> store)
{
  TunnelSettingsDependencies deps;
  deps.secretStore = std::move(store);
  deps.locate = [world](const QString &path) {
    world->lastPathAsked = path;
    return world->location;
  };
  deps.confirmRemoval = [world] {
    ++world->confirmAsked;
    return world->confirmAnswer;
  };
  deps.tlsEnabled = [world] { return world->tls; };
  return deps;
}

struct Widgets
{
  explicit Widgets(TunnelSettingsGroup &group)
      : enabled(group.findChild<QCheckBox *>(QStringLiteral("cbTunnelEnabled"))),
        note(group.findChild<QLabel *>(QStringLiteral("lblTunnelNote"))),
        startNote(group.findChild<QLabel *>(QStringLiteral("lblTunnelStartNote"))),
        tlsNote(group.findChild<QLabel *>(QStringLiteral("lblTunnelTlsNote"))),
        hostname(group.findChild<QLineEdit *>(QStringLiteral("leTunnelHostname"))),
        port(group.findChild<QSpinBox *>(QStringLiteral("sbTunnelLocalPort"))),
        path(group.findChild<QLineEdit *>(QStringLiteral("leCloudflaredPath"))),
        found(group.findChild<QLabel *>(QStringLiteral("lblCloudflaredFound"))),
        install(group.findChild<QLineEdit *>(QStringLiteral("leCloudflaredInstall"))),
        clientId(group.findChild<QLineEdit *>(QStringLiteral("leTokenClientId"))),
        secret(group.findChild<QLineEdit *>(QStringLiteral("leTokenSecret"))),
        status(group.findChild<QLabel *>(QStringLiteral("lblCredentialStatus"))),
        remove(group.findChild<QPushButton *>(QStringLiteral("btnRemoveCredentials"))),
        error(group.findChild<QLabel *>(QStringLiteral("lblTunnelError")))
  {
  }

  // the user clicks into the Client ID field: the secure store is asked, once
  void reveal() const
  {
    QFocusEvent in(QEvent::FocusIn);
    QApplication::sendEvent(clientId, &in);
  }

  QCheckBox *enabled;
  QLabel *note;
  QLabel *startNote;
  QLabel *tlsNote;
  QLineEdit *hostname;
  QSpinBox *port;
  QLineEdit *path;
  QLabel *found;
  QLineEdit *install;
  QLineEdit *clientId;
  QLineEdit *secret;
  QLabel *status;
  QPushButton *remove;
  QLabel *error;
};

QTemporaryDir &scratch()
{
  static QTemporaryDir dir;
  return dir;
}

// a configuration file of its own for the test that calls it, and the settings are loaded from it
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

bool confHas(const QString &file, const QString &key)
{
  QSettings conf(file, QSettings::IniFormat);
  return conf.contains(key);
}

} // namespace

void TunnelSettingsGroupTests::init()
{
  QVERIFY(scratch().isValid());
  g_messages.clear();
  qInstallMessageHandler(collectMessages);
}

void TunnelSettingsGroupTests::cleanup()
{
  qInstallMessageHandler(nullptr);
}

void TunnelSettingsGroupTests::defaults_areOffAndEmptyAndTheSecretIsNeverRead()
{
  useConfig();
  const auto world = std::make_shared<World>();
  const auto store = std::make_shared<InMemorySecretStore>();
  TunnelSettingsGroup group(dependencies(world, store));
  const Widgets w(group);

  QVERIFY(!w.enabled->isChecked());
  QVERIFY(w.hostname->text().isEmpty());
  QCOMPARE(w.port->value(), 24800);
  QCOMPARE(w.port->minimum(), 1024);
  QCOMPARE(w.port->maximum(), 65535);
  QVERIFY(w.path->text().isEmpty());
  QVERIFY(w.clientId->text().isEmpty());
  QVERIFY(w.secret->text().isEmpty());
  QCOMPARE(w.secret->echoMode(), QLineEdit::Password);
  // the secure store is not asked until it is needed: the line says so
  QVERIFY(w.status->text().startsWith(QStringLiteral("Credentials are checked")));
  QVERIFY(!w.remove->isEnabled());
  QVERIFY(w.error->isHidden());
  QVERIFY(group.isDefault());
  QVERIFY(!group.isModified());
  QVERIFY(group.validationProblem().isEmpty());

  // what the group says about the saved address of the server, about this version, and not about TLS
  QVERIFY(w.note->text().contains(QStringLiteral("Your saved server address is kept")));
  // Deskflow starts cloudflared itself: the group says so and does not tell the user to start it
  QVERIFY(kTunnelSupervisorAvailable);
  QVERIFY(!w.startNote->isHidden());
  QVERIFY(w.startNote->text().contains(QStringLiteral("Deskflow starts cloudflared")));
  QVERIFY(!w.startNote->text().contains(QStringLiteral("start it yourself")));
  QVERIFY(!w.startNote->toolTip().contains(QStringLiteral("start it yourself with")));
  QVERIFY(w.tlsNote->isHidden());

  QCOMPARE(store->state()->loadCount, 0);
  QCOMPARE(store->state()->idReadCount, 0);
}

void TunnelSettingsGroupTests::tlsOff_blocksEnablingAndSaysWhy()
{
  useConfig();
  const auto world = std::make_shared<World>();
  world->tls = false;
  TunnelSettingsGroup group(dependencies(world, std::make_shared<InMemorySecretStore>()));
  const Widgets w(group);

  QVERIFY(!w.enabled->isEnabled());
  QVERIFY(!w.tlsNote->isHidden());
  QVERIFY(w.tlsNote->text().contains(QStringLiteral("requires TLS")));

  // even when it is switched on some other way, it cannot be saved
  w.hostname->setText(QStringLiteral("mbp.example.com"));
  w.enabled->setChecked(true);
  QVERIFY(group.validationProblem().contains(QStringLiteral("requires TLS")));
}

void TunnelSettingsGroupTests::tlsOff_stillLetsAnEnabledTunnelBeTurnedOff()
{
  useConfig("[core]\ncomputerName=test-pc\n\n[log]\nlevel=Info\n\n[tunnel]\nenabled=true\nhostname=mbp.example.com\n");
  const auto world = std::make_shared<World>();
  world->tls = false;
  TunnelSettingsGroup group(dependencies(world, std::make_shared<InMemorySecretStore>()));
  const Widgets w(group);

  QVERIFY(w.enabled->isChecked());
  QVERIFY(w.enabled->isEnabled());
  QVERIFY(!group.validationProblem().isEmpty());
  w.enabled->setChecked(false);
  QVERIFY(group.validationProblem().isEmpty());
  QVERIFY(group.save());
  QVERIFY(!Settings::value(Settings::Tunnel::Enabled).toBool());
}

void TunnelSettingsGroupTests::save_withNothingTouched_writesNothingToTheConfiguration()
{
  const auto file = useConfig();
  const auto before = readFile(file);
  const auto world = std::make_shared<World>();
  TunnelSettingsGroup group(dependencies(world, std::make_shared<InMemorySecretStore>()));

  QVERIFY(!group.isModified());
  QVERIFY(group.validationProblem().isEmpty());
  QVERIFY(group.save());

  QCOMPARE(readFile(file), before);
  QVERIFY(!QSettings(file, QSettings::IniFormat).childGroups().contains(QStringLiteral("tunnel")));
}

void TunnelSettingsGroupTests::save_writesOnlyWhatChangedAndNormalisesTheHostname()
{
  const auto file = useConfig();
  const auto world = std::make_shared<World>();
  TunnelSettingsGroup group(dependencies(world, std::make_shared<InMemorySecretStore>()));
  const Widgets w(group);

  w.hostname->setText(QStringLiteral("  MBP.Example.COM "));
  w.enabled->setChecked(true);
  QVERIFY(group.isModified());
  QVERIFY(group.validationProblem().isEmpty());
  QVERIFY(group.save());

  QSettings conf(file, QSettings::IniFormat);
  QCOMPARE(conf.value(QStringLiteral("tunnel/enabled")).toBool(), true);
  QCOMPARE(conf.value(QStringLiteral("tunnel/hostname")).toString(), QStringLiteral("mbp.example.com"));
  QVERIFY(!conf.contains(QStringLiteral("tunnel/localPort")));
  QVERIFY(!conf.contains(QStringLiteral("tunnel/cloudflaredPath")));
  QVERIFY(!conf.contains(QStringLiteral("tunnel/activePort")));
  QVERIFY(!group.isModified());

  // the port and the path when they are changed
  w.port->setValue(24801);
  w.path->setText(QStringLiteral("/opt/tools/cloudflared"));
  QVERIFY(group.save());
  QCOMPARE(Settings::value(Settings::Tunnel::LocalPort).toInt(), 24801);
  QCOMPARE(Settings::value(Settings::Tunnel::CloudflaredPath).toString(), QStringLiteral("/opt/tools/cloudflared"));

  // and an empty path is no path
  w.path->clear();
  QVERIFY(group.save());
  QVERIFY(!confHas(file, QStringLiteral("tunnel/cloudflaredPath")));

  // the saved address of the server is not something that it ever writes
  QVERIFY(!confHas(file, QStringLiteral("client/remoteHost")));
  QVERIFY(!confHas(file, QStringLiteral("client/recentRemoteHosts")));
}

void TunnelSettingsGroupTests::save_thatChangesTheConfigurationForgetsTheActivePort()
{
  const auto file = useConfig(
      "[core]\ncomputerName=test-pc\n\n[log]\nlevel=Info\n\n[tunnel]\nenabled=true\n"
      "hostname=mbp.example.com\nactivePort=51823\n"
  );
  const auto world = std::make_shared<World>();
  TunnelSettingsGroup group(dependencies(world, std::make_shared<InMemorySecretStore>()));
  const Widgets w(group);

  // nothing changed: the port that is in use is not forgotten
  QVERIFY(group.save());
  QCOMPARE(Settings::value(Settings::Tunnel::ActivePort).toInt(), 51823);

  w.port->setValue(25000);
  QVERIFY(group.save());
  QVERIFY(!confHas(file, QStringLiteral("tunnel/activePort")));
  QCOMPARE(Settings::value(Settings::Tunnel::LocalPort).toInt(), 25000);
}

void TunnelSettingsGroupTests::save_storesTheTokenAndNeverShowsTheSecretAgain()
{
  const auto file = useConfig();
  const auto world = std::make_shared<World>();
  const auto store = std::make_shared<PersistentTestStore>();
  TunnelSettingsGroup group(dependencies(world, store));
  const Widgets w(group);
  QSignalSpy changed(&group, &TunnelSettingsGroup::changed);

  w.hostname->setText(QStringLiteral("mbp.example.com"));
  w.enabled->setChecked(true);
  w.clientId->setText(kId);
  w.secret->setText(kSecret);
  QVERIFY(group.isModified());
  QVERIFY(group.validationProblem().isEmpty());
  QVERIFY(changed.count() > 0);

  QVERIFY(group.save());

  // in the store
  QVERIFY(store->state()->token.has_value());
  QCOMPARE(store->state()->token->clientId, kId);
  QCOMPARE(store->state()->token->secret, kSecret);
  QCOMPARE(store->state()->loadCount, 0);

  // and shown as a token that is there, without the Secret
  QVERIFY(w.secret->text().isEmpty());
  QCOMPARE(w.clientId->text(), kId);
  QVERIFY(w.status->text().contains(QStringLiteral("Stored in Test keychain (Client ID ...a1b2c3)")));
  QVERIFY(w.secret->placeholderText().contains(QStringLiteral("Stored")));
  QVERIFY(w.remove->isEnabled());
  QVERIFY(!group.isModified());

  // nowhere else: not in the configuration file, not in what any widget shows, not in what was logged
  const auto conf = readFile(file);
  QVERIFY(!conf.contains(kSecret.toUtf8()));
  QVERIFY(!conf.contains(kId.toUtf8()));
  for (const auto *edit : group.findChildren<QLineEdit *>()) {
    QVERIFY2(!edit->text().contains(kSecret), qPrintable(edit->objectName()));
    QVERIFY2(!edit->placeholderText().contains(kSecret), qPrintable(edit->objectName()));
  }
  for (const auto *label : group.findChildren<QLabel *>()) {
    QVERIFY2(!label->text().contains(kSecret), qPrintable(label->objectName()));
  }
  for (const auto &message : std::as_const(g_messages)) {
    QVERIFY2(!message.contains(kSecret), qPrintable(message));
  }

  // a dialog that is opened again shows the same, and still does not read the Secret
  TunnelSettingsGroup again(dependencies(world, store));
  const Widgets v(again);
  v.reveal();
  QCOMPARE(v.clientId->text(), kId);
  QVERIFY(v.secret->text().isEmpty());
  QVERIFY(v.status->text().contains(QStringLiteral("...a1b2c3")));
  QCOMPARE(store->state()->loadCount, 0);
}

void TunnelSettingsGroupTests::validation_saysWhatIsMissing_data()
{
  QTest::addColumn<bool>("enabled");
  QTest::addColumn<QString>("hostname");
  QTest::addColumn<QString>("clientId");
  QTest::addColumn<QString>("secret");
  QTest::addColumn<bool>("alreadyStored");
  QTest::addColumn<QString>("problem");

  const auto host = QStringLiteral("mbp.example.com");
  QTest::newRow("all-there") << true << host << kId << kSecret << false << QString();
  QTest::newRow("nothing-and-off") << false << QString() << QString() << QString() << false << QString();
  QTest::newRow("bad-hostname-while-off-is-not-asked")
      << false << QStringLiteral("10.0.0.5") << QString() << QString() << false << QString();
  QTest::newRow("no-hostname") << true << QString() << QString() << QString() << false
                               << QStringLiteral("valid tunnel hostname");
  QTest::newRow("an-address-is-not-a-hostname") << true << QStringLiteral("10.0.0.5") << QString() << QString() << false
                                                << QStringLiteral("valid tunnel hostname");
  QTest::newRow("a-url-is-not-a-hostname") << true << QStringLiteral("https://mbp.example.com") << QString()
                                           << QString() << false << QStringLiteral("valid tunnel hostname");
  QTest::newRow("secret-without-id") << true << host << QString() << kSecret << false
                                     << QStringLiteral("Client ID that goes with the Secret");
  QTest::newRow("id-without-secret") << true << host << kId << QString() << false
                                     << QStringLiteral("Secret that goes with the Client ID");
  QTest::newRow("same-id-as-stored-without-secret-is-fine") << true << host << kId << QString() << true << QString();
  QTest::newRow("another-id-than-stored-needs-its-secret")
      << true << host << QStringLiteral("OTHER.access") << QString() << true
      << QStringLiteral("Secret that goes with the Client ID");
}

void TunnelSettingsGroupTests::validation_saysWhatIsMissing()
{
  QFETCH(bool, enabled);
  QFETCH(QString, hostname);
  QFETCH(QString, clientId);
  QFETCH(QString, secret);
  QFETCH(bool, alreadyStored);
  QFETCH(QString, problem);

  useConfig();
  const auto world = std::make_shared<World>();
  const auto store = std::make_shared<InMemorySecretStore>();
  if (alreadyStored) {
    QVERIFY(store->save(ServiceToken(kId, kSecret)).ok);
  }
  TunnelSettingsGroup group(dependencies(world, store));
  const Widgets w(group);

  w.hostname->setText(hostname);
  w.enabled->setChecked(enabled);
  w.clientId->setText(clientId);
  w.secret->setText(secret);

  const auto found = group.validationProblem();
  if (problem.isEmpty()) {
    QVERIFY2(found.isEmpty(), qPrintable(found));
  } else {
    QVERIFY2(found.contains(problem), qPrintable(found));
  }
  // asking writes nothing
  QVERIFY(!store->state()->token.has_value() || alreadyStored);
}

void TunnelSettingsGroupTests::removeCredentials_asksAndThenRemoves()
{
  useConfig();
  const auto world = std::make_shared<World>();
  const auto store = std::make_shared<InMemorySecretStore>();
  QVERIFY(store->save(ServiceToken(kId, kSecret)).ok);
  TunnelSettingsGroup group(dependencies(world, store));
  const Widgets w(group);
  QSignalSpy changed(&group, &TunnelSettingsGroup::changed);

  w.reveal();
  QVERIFY(w.remove->isEnabled());
  world->confirmAnswer = false;
  w.remove->click();
  QCOMPARE(world->confirmAsked, 1);
  QVERIFY(store->state()->token.has_value());
  QCOMPARE(store->state()->removeCount, 0);
  QCOMPARE(w.clientId->text(), kId);

  world->confirmAnswer = true;
  w.remove->click();
  QCOMPARE(world->confirmAsked, 2);
  QVERIFY(!store->state()->token.has_value());
  QCOMPARE(store->state()->removeCount, 1);
  QVERIFY(w.clientId->text().isEmpty());
  QVERIFY(w.secret->text().isEmpty());
  QVERIFY(w.status->text().startsWith(QStringLiteral("No credentials stored")));
  QVERIFY(!w.remove->isEnabled());
  QVERIFY(changed.count() > 0);
}

void TunnelSettingsGroupTests::aStoreThatRefuses_savesNothingAndKeepsWhatWasTyped()
{
  const auto file = useConfig();
  const auto world = std::make_shared<World>();
  const auto store = std::make_shared<InMemorySecretStore>();
  TunnelSettingsGroup group(dependencies(world, store));
  const Widgets w(group);

  w.hostname->setText(QStringLiteral("mbp.example.com"));
  w.enabled->setChecked(true);
  w.clientId->setText(kId);
  w.secret->setText(kSecret);
  store->state()->failWith = QStringLiteral("the keychain is locked");

  QVERIFY(!group.save());
  QVERIFY(!w.error->isHidden());
  QVERIFY(w.error->text().contains(QStringLiteral("the keychain is locked")));
  QVERIFY(!w.error->text().contains(kSecret));
  // no setting was written, and what was typed is still there to try again
  QVERIFY(!confHas(file, QStringLiteral("tunnel/enabled")));
  QVERIFY(!confHas(file, QStringLiteral("tunnel/hostname")));
  QCOMPARE(w.secret->text(), kSecret);

  // typing again clears the message
  w.secret->setText(kSecret + QLatin1Char('x'));
  QVERIFY(w.error->isHidden());
}

void TunnelSettingsGroupTests::aStoreThatCannotBeRead_isSaidAndNothingBreaks()
{
  useConfig();
  const auto world = std::make_shared<World>();
  const auto store = std::make_shared<InMemorySecretStore>();
  store->state()->failWith = QStringLiteral("access to the keychain was denied");
  TunnelSettingsGroup group(dependencies(world, store));
  const Widgets w(group);

  w.reveal();
  QVERIFY(w.status->text().contains(QStringLiteral("unreadable")));
  QVERIFY(w.status->text().contains(QStringLiteral("access to the keychain was denied")));
  QVERIFY(!w.remove->isEnabled());
  QVERIFY(group.validationProblem().isEmpty());
}

void TunnelSettingsGroupTests::cloudflaredLine_followsTheLocatorAndTheTypedPath()
{
  useConfig();
  const auto world = std::make_shared<World>();
  TunnelSettingsGroup group(dependencies(world, std::make_shared<InMemorySecretStore>()));
  const Widgets w(group);

  QVERIFY(w.found->text().startsWith(QStringLiteral("Found: ")));
  QVERIFY(w.found->text().contains(QDir::toNativeSeparators(QStringLiteral("/usr/local/bin/cloudflared"))));
  QVERIFY(w.install->isHidden());

  // not found: where to get it, as a line that can be copied
  world->location = CloudflaredLocation();
  w.path->setText(QStringLiteral("/elsewhere/cloudflared"));
  QCOMPARE(world->lastPathAsked, QStringLiteral("/elsewhere/cloudflared"));
  QCOMPARE(w.found->text(), QStringLiteral("cloudflared was not found."));
  QVERIFY(!w.install->isHidden());
  QCOMPARE(w.install->text(), cloudflaredInstallHint(currentPlatform()));
  QVERIFY(w.install->isReadOnly());
  QVERIFY(w.found->toolTip().contains(QStringLiteral("brew install cloudflared")));
  QVERIFY(w.found->toolTip().contains(QStringLiteral("winget install --id Cloudflare.cloudflared")));

  // a typed path that cannot be run, and another one that is found instead
  world->location = CloudflaredLocation{
      CloudflaredLocation::Source::KnownLocation, QStringLiteral("/opt/homebrew/bin/cloudflared"), true
  };
  w.path->setText(QStringLiteral("/gone/cloudflared"));
  QVERIFY(w.found->text().contains(QStringLiteral("cannot be run")));
  QVERIFY(w.found->text().contains(QDir::toNativeSeparators(QStringLiteral("/opt/homebrew/bin/cloudflared"))));
  QVERIFY(w.install->isHidden());
}

void TunnelSettingsGroupTests::resetToDefault_dropsTheEntriesAndKeepsTheToken()
{
  useConfig();
  const auto world = std::make_shared<World>();
  const auto store = std::make_shared<InMemorySecretStore>();
  QVERIFY(store->save(ServiceToken(kId, kSecret)).ok);
  TunnelSettingsGroup group(dependencies(world, store));
  const Widgets w(group);

  w.hostname->setText(QStringLiteral("mbp.example.com"));
  w.enabled->setChecked(true);
  w.port->setValue(25000);
  w.path->setText(QStringLiteral("/x/cloudflared"));
  w.secret->setText(QStringLiteral("typed-but-not-saved"));
  QVERIFY(!group.isDefault());

  group.resetToDefault();
  QVERIFY(group.isDefault());
  QVERIFY(!w.enabled->isChecked());
  QVERIFY(w.hostname->text().isEmpty());
  QCOMPARE(w.port->value(), 24800);
  QVERIFY(w.path->text().isEmpty());
  QVERIFY(w.secret->text().isEmpty());
  QCOMPARE(w.clientId->text(), kId);
  QCOMPARE(store->state()->token->secret, kSecret);
  QCOMPARE(store->state()->removeCount, 0);
}

void TunnelSettingsGroupTests::isModifiedAndIsDefault_followTheEntries()
{
  useConfig();
  const auto world = std::make_shared<World>();
  TunnelSettingsGroup group(dependencies(world, std::make_shared<InMemorySecretStore>()));
  const Widgets w(group);

  QVERIFY(!group.isModified());
  w.enabled->setChecked(true);
  QVERIFY(group.isModified());
  QVERIFY(!group.isDefault());
  w.enabled->setChecked(false);
  QVERIFY(!group.isModified());

  w.port->setValue(24801);
  QVERIFY(group.isModified());
  QVERIFY(!group.isDefault());
  w.port->setValue(24800);

  // a typed Secret is a change, though it is not a setting
  w.secret->setText(QStringLiteral("abc"));
  QVERIFY(group.isModified());
  QVERIFY(group.isDefault());
  w.secret->clear();
  QVERIFY(!group.isModified());

  // blanks around the hostname are not a change
  w.hostname->setText(QStringLiteral("   "));
  QVERIFY(!group.isModified());
}

void TunnelSettingsGroupTests::credentialLine_saysWhereAPersistentTokenIsAndThatAMemoryOneIsForgotten()
{
  useConfig();
  const auto world = std::make_shared<World>();

  const auto memory = std::make_shared<InMemorySecretStore>();
  QVERIFY(memory->save(ServiceToken(kId, kSecret)).ok);
  TunnelSettingsGroup inMemory(dependencies(world, memory));
  Widgets(inMemory).reveal();
  QVERIFY(Widgets(inMemory).status->text().contains(QStringLiteral("memory only")));
  QVERIFY(Widgets(inMemory).status->text().contains(QStringLiteral("forgotten")));

  const auto persistent = std::make_shared<PersistentTestStore>();
  QVERIFY(persistent->save(ServiceToken(kId, kSecret)).ok);
  TunnelSettingsGroup stored(dependencies(world, persistent));
  Widgets(stored).reveal();
  QVERIFY(!Widgets(stored).status->text().contains(QStringLiteral("forgotten")));
  QVERIFY(Widgets(stored).status->text().contains(QStringLiteral("Test keychain")));

  // and a system without a secure store says so before anything is stored
  TunnelSettingsGroup empty(dependencies(world, std::make_shared<InMemorySecretStore>()));
  Widgets(empty).reveal();
  QVERIFY(Widgets(empty).status->text().contains(QStringLiteral("no secure storage")));
}

void TunnelSettingsGroupTests::notWritable_disablesEveryEntry()
{
  useConfig();
  const auto world = std::make_shared<World>();
  const auto store = std::make_shared<InMemorySecretStore>();
  QVERIFY(store->save(ServiceToken(kId, kSecret)).ok);
  TunnelSettingsGroup group(dependencies(world, store));
  const Widgets w(group);
  w.reveal();

  group.setWritable(false);
  QVERIFY(!w.enabled->isEnabled());
  QVERIFY(!w.hostname->isEnabled());
  QVERIFY(!w.port->isEnabled());
  QVERIFY(!w.path->isEnabled());
  QVERIFY(!w.clientId->isEnabled());
  QVERIFY(!w.secret->isEnabled());
  QVERIFY(!w.remove->isEnabled());

  group.setWritable(true);
  QVERIFY(w.enabled->isEnabled());
  QVERIFY(w.hostname->isEnabled());
  QVERIFY(w.remove->isEnabled());
}

void TunnelSettingsGroupTests::saveInTheOtherDirection_aSavedTunnelIsShownAgain()
{
  useConfig();
  const auto world = std::make_shared<World>();
  const auto store = std::make_shared<InMemorySecretStore>();
  {
    TunnelSettingsGroup group(dependencies(world, store));
    const Widgets w(group);
    w.hostname->setText(QStringLiteral("mbp.example.com"));
    w.port->setValue(24801);
    w.path->setText(QStringLiteral("/opt/tools/cloudflared"));
    w.enabled->setChecked(true);
    QVERIFY(group.save());
  }

  TunnelSettingsGroup again(dependencies(world, store));
  const Widgets v(again);
  QVERIFY(v.enabled->isChecked());
  QCOMPARE(v.hostname->text(), QStringLiteral("mbp.example.com"));
  QCOMPARE(v.port->value(), 24801);
  QCOMPARE(v.path->text(), QStringLiteral("/opt/tools/cloudflared"));
  QVERIFY(!again.isModified());
}

// Asking the secure store can make the system ask the user for a password (a locked keychain), so it is not asked until
// the user has a reason: turns the tunnel on, or goes into the fields of the token
void TunnelSettingsGroupTests::openingWithTheTunnelOff_makesNoCallToTheSecureStore()
{
  useConfig();
  const auto world = std::make_shared<World>();
  const auto store = std::make_shared<InMemorySecretStore>();
  QVERIFY(store->save(ServiceToken(kId, kSecret)).ok);
  const auto saves = store->state()->saveCount;

  TunnelSettingsGroup group(dependencies(world, store));
  const Widgets w(group);
  // the dialog loads the group again when it is made and when it is reset
  group.load();
  group.load();
  QVERIFY(!group.isModified());
  QVERIFY(group.isDefault());
  QVERIFY(group.validationProblem().isEmpty());
  group.resetToDefault();
  group.setWritable(false);
  group.setWritable(true);

  QCOMPARE(store->state()->idReadCount, 0);
  QCOMPARE(store->state()->loadCount, 0);
  QCOMPARE(store->state()->removeCount, 0);
  QCOMPARE(store->state()->saveCount, saves);
  QVERIFY(w.clientId->text().isEmpty());
  QVERIFY(w.status->text().startsWith(QStringLiteral("Credentials are checked")));
}

void TunnelSettingsGroupTests::openingWithTheTunnelOn_looksUpOnce()
{
  useConfig(
      "[core]\ncomputerName=test-pc\n\n[security]\ntlsEnabled=true\n\n[tunnel]\nenabled=true\n"
      "hostname=mbp.example.com\n"
  );
  const auto world = std::make_shared<World>();
  const auto store = std::make_shared<InMemorySecretStore>();
  QVERIFY(store->save(ServiceToken(kId, kSecret)).ok);

  TunnelSettingsGroup group(dependencies(world, store));
  const Widgets w(group);
  group.load();

  QCOMPARE(store->state()->idReadCount, 1);
  QCOMPARE(store->state()->loadCount, 0);
  QCOMPARE(w.clientId->text(), kId);
  QVERIFY(w.remove->isEnabled());
}

void TunnelSettingsGroupTests::turningTheTunnelOn_looksUpOnceAndTheAnswerIsKept()
{
  useConfig();
  const auto world = std::make_shared<World>();
  const auto store = std::make_shared<InMemorySecretStore>();
  QVERIFY(store->save(ServiceToken(kId, kSecret)).ok);

  TunnelSettingsGroup group(dependencies(world, store));
  const Widgets w(group);
  QCOMPARE(store->state()->idReadCount, 0);

  w.enabled->setChecked(true);
  QCOMPARE(store->state()->idReadCount, 1);
  QCOMPARE(w.clientId->text(), kId);
  QVERIFY(w.remove->isEnabled());

  // off, on, the fields, the dialog loading again: the answer that was got is kept for the life of the dialog
  w.enabled->setChecked(false);
  w.enabled->setChecked(true);
  w.reveal();
  group.load();
  QCOMPARE(store->state()->idReadCount, 1);
  QCOMPARE(store->state()->loadCount, 0);
}

void TunnelSettingsGroupTests::goingIntoTheFieldsOfTheToken_looksUpOnce()
{
  useConfig();
  const auto world = std::make_shared<World>();
  const auto store = std::make_shared<InMemorySecretStore>();
  QVERIFY(store->save(ServiceToken(kId, kSecret)).ok);
  TunnelSettingsGroup group(dependencies(world, store));
  const Widgets w(group);

  // the Secret field as well as the Client ID field
  QFocusEvent in(QEvent::FocusIn);
  QApplication::sendEvent(w.secret, &in);
  QCOMPARE(store->state()->idReadCount, 1);
  QCOMPARE(w.clientId->text(), kId);
  QVERIFY(w.status->text().contains(QStringLiteral("Client ID ...a1b2c3")));

  w.reveal();
  QCOMPARE(store->state()->idReadCount, 1);
  QCOMPARE(store->state()->loadCount, 0);
}

void TunnelSettingsGroupTests::typingInAField_looksUpOnceAndDoesNotOverwriteWhatWasTyped()
{
  useConfig();
  const auto world = std::make_shared<World>();
  const auto store = std::make_shared<InMemorySecretStore>();
  QVERIFY(store->save(ServiceToken(kId, kSecret)).ok);
  TunnelSettingsGroup group(dependencies(world, store));
  const Widgets w(group);

  w.clientId->setText(QStringLiteral("typed-id.access"));
  QCOMPARE(store->state()->idReadCount, 1);
  QCOMPARE(w.clientId->text(), QStringLiteral("typed-id.access"));
  w.clientId->setText(QStringLiteral("typed-id-2.access"));
  QCOMPARE(store->state()->idReadCount, 1);
}

void TunnelSettingsGroupTests::savingWithoutTouchingTheToken_makesNoCallToTheSecureStore()
{
  const auto file = useConfig();
  const auto world = std::make_shared<World>();
  const auto store = std::make_shared<InMemorySecretStore>();
  QVERIFY(store->save(ServiceToken(kId, kSecret)).ok);
  const auto saves = store->state()->saveCount;
  TunnelSettingsGroup group(dependencies(world, store));
  const Widgets w(group);

  // another setting changed with the tunnel off, and the dialog was accepted
  w.hostname->setText(QStringLiteral("mbp.example.com"));
  QVERIFY(group.validationProblem().isEmpty());
  QVERIFY(group.save());

  QCOMPARE(store->state()->idReadCount, 0);
  QCOMPARE(store->state()->loadCount, 0);
  QCOMPARE(store->state()->saveCount, saves);
  QCOMPARE(Settings::value(Settings::Tunnel::Hostname).toString(), QStringLiteral("mbp.example.com"));
  QVERIFY(!confHas(file, QStringLiteral("tunnel/enabled")));
}

void TunnelSettingsGroupTests::savingATypedToken_storesItAndShowsWhatIsStored()
{
  useConfig();
  const auto world = std::make_shared<World>();
  const auto store = std::make_shared<InMemorySecretStore>();
  TunnelSettingsGroup group(dependencies(world, store));
  const Widgets w(group);

  w.clientId->setText(kId);
  w.secret->setText(kSecret);
  QVERIFY(group.validationProblem().isEmpty());
  QVERIFY(group.save());

  QCOMPARE(store->state()->saveCount, 1);
  QCOMPARE(store->state()->loadCount, 0);
  // the typing looked the store up once (before the token was saved), and the save shows what is stored now
  QVERIFY(store->state()->idReadCount <= 2);
  QVERIFY(w.secret->text().isEmpty());
  QCOMPARE(w.clientId->text(), kId);
  QVERIFY(w.status->text().contains(QStringLiteral("...a1b2c3")));
  QVERIFY(w.remove->isEnabled());
}

QTEST_MAIN(TunnelSettingsGroupTests)
