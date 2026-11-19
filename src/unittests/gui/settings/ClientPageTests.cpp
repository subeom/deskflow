/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ClientPageTests.h"

#include "common/Settings.h"
#include "gui/dialogs/ClientConfigDialog.h"
#include "gui/settings/SettingEffects.h"
#include "gui/settings/SettingsBinder.h"
#include "gui/settings/SettingsPage.h"
#include "gui/settings/SettingsPages.h"
#include "gui/tunnel/CloudflaredLocator.h"
#include "gui/tunnel/InMemorySecretStore.h"
#include "gui/widgets/InlineHint.h"
#include "gui/widgets/SettingCard.h"
#include "gui/widgets/SettingRow.h"
#include "gui/widgets/SettingsDialogButtonBox.h"
#include "gui/widgets/ToggleSwitch.h"
#include "gui/widgets/TunnelSettingsGroup.h"

#include <QAccessible>
#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QSpinBox>

using namespace deskflow::gui;
using namespace deskflow::gui::settings;
using namespace deskflow::tunnel;

namespace {

// made-up values that look nothing like a real token
const auto kId = QStringLiteral("TESTID0001a1b2c3.access");
const auto kSecret = QStringLiteral("TESTSECRET-do-not-leak-0002");

template <class T> T *find(const SettingsPages &pages, const char *name)
{
  for (const auto &entry : pages.entries()) {
    if (auto *found = entry.page->findChild<T *>(QString::fromLatin1(name)))
      return found;
  }
  return nullptr;
}

template <class T> T *must(const SettingsPages &pages, const char *name)
{
  auto *found = find<T>(pages, name);
  Q_ASSERT_X(found, "must", name);
  return found;
}

QVariant stored(const QString &key)
{
  QSettings file(Settings::settingsFile(), QSettings::IniFormat);
  return file.value(key);
}

QByteArray settingsFileContent()
{
  QFile file(Settings::settingsFile());
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// every key that these tests write, back to what it never was
void forgetAll()
{
  for (const auto &key :
       {Settings::Client::DynamicConnectionRetry, Settings::Client::LanguageSync, Settings::Client::ConnectTimeout,
        Settings::Client::InvertYScroll, Settings::Client::YScrollScale, Settings::Client::InvertXScroll,
        Settings::Client::XScrollScale, Settings::Client::RecentRemoteHosts, Settings::Tunnel::Enabled,
        Settings::Tunnel::Hostname, Settings::Tunnel::LocalPort, Settings::Tunnel::CloudflaredPath,
        Settings::Core::CoreMode})
    Settings::setValue(key);
}

// what the group asks its surroundings: nothing of the system
struct World
{
  bool tls = true;
};

TunnelSettingsDependencies dependencies(const std::shared_ptr<World> &world, std::shared_ptr<ISecretStore> store)
{
  TunnelSettingsDependencies deps;
  deps.secretStore = std::move(store);
  deps.locate = [](const QString &) {
    return CloudflaredLocation(CloudflaredLocation::Source::Path, QStringLiteral("/usr/local/bin/cloudflared"), false);
  };
  deps.confirmRemoval = [] { return true; };
  deps.tlsEnabled = [world] { return world->tls; };
  return deps;
}

// the page is shown, which makes the group
TunnelSettingsGroup *show(SettingsPages &pages)
{
  auto *page = pages.page(SettingsPages::clientId());
  page->resize(900, 900);
  page->show();
  // the group is made while the page is being shown: what is put into a window that is shown appears at the next turn
  QCoreApplication::processEvents();
  return pages.tunnelGroup();
}

QPushButton *button(SettingsPages &pages, QDialogButtonBox::StandardButton which)
{
  return must<QDialogButtonBox>(pages, "tunnelButtons")->button(which);
}

} // namespace

void ClientPageTests::initTestCase()
{
  QVERIFY(m_dir.isValid());
  Settings::setSettingsFile(m_dir.filePath(QStringLiteral("Deskflow.conf")));
  Settings::setStateFile(m_dir.filePath(QStringLiteral("Deskflow.state")));
  QAccessible::setActive(true);
}

void ClientPageTests::cleanup()
{
  forgetAll();
  QApplication::setStyle(QStringLiteral("Fusion"));
}

void ClientPageTests::page_hasNoEntryOfItsOwn_andTheNetworkPageAsksForIt()
{
  SettingsPages pages;
  QSignalSpy requested(&pages, &SettingsPages::pageRequested);
  for (const auto &entry : pages.entries()) {
    // only the page of the client is one that the sidebar has no entry for; the entry of the network is marked for it
    if (entry.id == SettingsPages::clientId())
      QCOMPARE(entry.parentId, QStringLiteral("settings.network"));
    else
      QVERIFY2(entry.parentId.isEmpty(), qPrintable(entry.id));
  }
  auto *open =
      pages.page(QStringLiteral("settings.network"))->findChild<QPushButton *>(QStringLiteral("btnOpenClientSettings"));
  QVERIFY(open != nullptr);
  QVERIFY(requested.isEmpty());
  open->click();
  QCOMPARE(requested.count(), 1);
  QCOMPARE(requested.first().first().toString(), SettingsPages::clientId());
}

void ClientPageTests::page_isAfterTheNetworkPage_andBindsTheKeysOfTheDialog()
{
  SettingsPages pages;
  QStringList ids;
  for (const auto &entry : pages.entries())
    ids << entry.id;
  const auto network = ids.indexOf(QStringLiteral("settings.network"));
  QVERIFY(network >= 0);
  QCOMPARE(ids.value(network + 1), SettingsPages::clientId());
  QCOMPARE(SettingsPages::clientId(), QStringLiteral("settings.client"));
  QCOMPARE(pages.page(SettingsPages::clientId())->title(), QStringLiteral("Client"));

  // the settings of the dialog "Client Configuration" (without the tunnel, whose group saves them with its buttons)
  const auto keys = pages.binder()->keys();
  for (const auto &key :
       {Settings::Client::DynamicConnectionRetry, Settings::Client::LanguageSync, Settings::Client::ConnectTimeout,
        Settings::Client::InvertYScroll, Settings::Client::YScrollScale, Settings::Client::InvertXScroll,
        Settings::Client::XScrollScale}) {
    QVERIFY2(keys.contains(key), qPrintable(key));
    // the client reads them when it starts
    QCOMPARE(effectOf(key), std::optional<Effect>(Effect::RestartSharing));
  }
  // what is saved by the buttons of the group is not bound: it is not written at each letter
  for (const auto &key : {Settings::Tunnel::Enabled, Settings::Tunnel::Hostname, Settings::Tunnel::LocalPort})
    QVERIFY2(!keys.contains(key), qPrintable(key));
  for (const auto &key :
       {Settings::Tunnel::Enabled, Settings::Tunnel::Hostname, Settings::Tunnel::LocalPort,
        Settings::Tunnel::CloudflaredPath})
    QCOMPARE(effectOf(key), std::optional<Effect>(Effect::RestartSharing));
}

void ClientPageTests::page_writesWhatTheDialogWritesForTheSameChanges()
{
  using S = Settings;
  const QStringList keys = {
      S::Client::DynamicConnectionRetry, S::Client::LanguageSync,  S::Client::ConnectTimeout, S::Client::InvertYScroll,
      S::Client::YScrollScale,           S::Client::InvertXScroll, S::Client::XScrollScale
  };
  // every one of them the other way from what it is by default
  const bool dynamic = !S::defaultValue(S::Client::DynamicConnectionRetry).toBool();
  const bool language = !S::defaultValue(S::Client::LanguageSync).toBool();
  const bool invertY = !S::defaultValue(S::Client::InvertYScroll).toBool();
  const bool invertX = !S::defaultValue(S::Client::InvertXScroll).toBool();

  // through the page
  QMap<QString, QVariant> throughThePage;
  forgetAll();
  {
    SettingsPages pages;
    must<ToggleSwitch>(pages, "cbDynamicConnectTime")->setChecked(dynamic);
    must<ToggleSwitch>(pages, "cbLanguageSync")->setChecked(language);
    must<QDoubleSpinBox>(pages, "sbConnectTimeout")->setValue(7.5);
    must<ToggleSwitch>(pages, "cbYScrollInvert")->setChecked(invertY);
    must<QDoubleSpinBox>(pages, "sbYScrollScale")->setValue(2.5);
    must<ToggleSwitch>(pages, "cbXScrollInvert")->setChecked(invertX);
    must<QDoubleSpinBox>(pages, "sbXScrollScale")->setValue(0.5);
    for (const auto &key : keys)
      throughThePage.insert(key, stored(key));
  }

  // through the dialog, accepted
  QMap<QString, QVariant> throughTheDialog;
  forgetAll();
  {
    auto world = std::make_shared<World>();
    ClientConfigDialog dialog(nullptr, dependencies(world, std::make_shared<InMemorySecretStore>()));
    dialog.findChild<QCheckBox *>(QStringLiteral("cbDynamicConnectTime"))->setChecked(dynamic);
    dialog.findChild<QCheckBox *>(QStringLiteral("cbLanguageSync"))->setChecked(language);
    dialog.findChild<QDoubleSpinBox *>(QStringLiteral("sbConnectTimeout"))->setValue(7.5);
    dialog.findChild<QCheckBox *>(QStringLiteral("cbYScrollInvert"))->setChecked(invertY);
    dialog.findChild<QDoubleSpinBox *>(QStringLiteral("sbYScrollScale"))->setValue(2.5);
    dialog.findChild<QCheckBox *>(QStringLiteral("cbXScrollInvert"))->setChecked(invertX);
    dialog.findChild<QDoubleSpinBox *>(QStringLiteral("sbXScrollScale"))->setValue(0.5);
    auto *box = dialog.findChild<SettingsDialogButtonBox *>();
    QVERIFY(box != nullptr);
    Q_EMIT box->accepted();
    for (const auto &key : keys)
      throughTheDialog.insert(key, stored(key));
  }

  for (const auto &key : keys) {
    QVERIFY2(throughThePage.value(key).isValid(), qPrintable(key));
    QCOMPARE(throughThePage.value(key), throughTheDialog.value(key));
  }
  // a time that is not a time limit is the default for the client (the page shows what the client uses)
  forgetAll();
  Settings::setValue(Settings::Client::ConnectTimeout, QStringLiteral("not a number"));
  SettingsPages pages;
  QCOMPARE(
      must<QDoubleSpinBox>(pages, "sbConnectTimeout")->value(), S::defaultValue(S::Client::ConnectTimeout).toDouble()
  );
}

void ClientPageTests::page_isForAClient_aServerSeesItDimmedWithANote()
{
  Settings::setValue(Settings::Core::CoreMode, Settings::CoreMode::Server);
  SettingsPages pages;
  auto *page = pages.page(SettingsPages::clientId());
  QVERIFY(!page->cards().isEmpty());
  for (auto *card : page->cards())
    QVERIFY2(!card->isEnabled(), qPrintable(card->objectName()));
  for (auto *note : page->findChildren<InlineHint *>(QStringLiteral("noteClientOnly")))
    QVERIFY(!note->isHidden());
  QVERIFY(!page->findChildren<InlineHint *>(QStringLiteral("noteClientOnly")).isEmpty());

  Settings::setValue(Settings::Core::CoreMode, Settings::CoreMode::Client);
  pages.binder()->refresh();
  for (auto *card : page->cards())
    QVERIFY2(card->isEnabled(), qPrintable(card->objectName()));
  for (auto *note : page->findChildren<InlineHint *>(QStringLiteral("noteClientOnly")))
    QVERIFY(note->isHidden());
}

void ClientPageTests::page_clearsTheListOfServers()
{
  Settings::setValue(Settings::Core::CoreMode, Settings::CoreMode::Client);
  Settings::setValue(Settings::Client::RecentRemoteHosts, QStringList{QStringLiteral("a"), QStringLiteral("b")});
  SettingsPages pages;
  auto *clear = must<QPushButton>(pages, "btnClearServerHistory");
  QVERIFY(clear->isEnabled());
  QSignalSpy cleared(&pages, &SettingsPages::serverHistoryCleared);
  clear->click();
  QCOMPARE(cleared.size(), 1);
  QVERIFY(Settings::value(Settings::Client::RecentRemoteHosts).toStringList().isEmpty());
  QVERIFY(!clear->isEnabled());
  // nothing to forget: the button has nothing to do
  pages.binder()->refresh();
  QVERIFY(!clear->isEnabled());
}

void ClientPageTests::tunnel_theGroupIsMadeWhenThePageIsShown_andTheStoreIsNotAskedForIt()
{
  Settings::setValue(Settings::Core::CoreMode, Settings::CoreMode::Client);
  auto store = std::make_shared<InMemorySecretStore>();
  auto world = std::make_shared<World>();
  SettingsPages pages(nullptr, dependencies(world, store));

  // the program starts: the page is not looked at, and there is no group (nothing can ask the secure store of the
  // system)
  QVERIFY(pages.tunnelGroup() == nullptr);
  const auto reads = [&] { return store->state()->idReadCount + store->state()->loadCount; };
  const auto before = reads();

  auto *group = show(pages);
  QVERIFY(group != nullptr);
  QVERIFY(group->isVisibleTo(pages.page(SettingsPages::clientId())));
  QCOMPARE(group->objectName(), QStringLiteral("groupCloudflareTunnel"));
  // the tunnel is off: the store is not asked, however many times the page is shown
  QCOMPARE(reads(), before);
  pages.page(SettingsPages::clientId())->hide();
  pages.page(SettingsPages::clientId())->show();
  QCOMPARE(pages.tunnelGroup(), group);
  QCOMPARE(reads(), before);
}

void ClientPageTests::tunnel_isSavedWithTheButton_asTheDialogSavesIt()
{
  Settings::setValue(Settings::Core::CoreMode, Settings::CoreMode::Client);

  const auto type = [](QLineEdit *edit, const QString &text) {
    edit->setText(text);
    Q_EMIT edit->editingFinished();
  };

  // through the page
  auto pageStore = std::make_shared<InMemorySecretStore>();
  QMap<QString, QVariant> throughThePage;
  {
    auto world = std::make_shared<World>();
    SettingsPages pages(nullptr, dependencies(world, pageStore));
    auto *group = show(pages);
    QVERIFY(group != nullptr);
    QSignalSpy saved(&pages, &SettingsPages::tunnelSaved);
    QSignalSpy applied(&pages, &SettingsPages::applied);
    group->findChild<QCheckBox *>(QStringLiteral("cbTunnelEnabled"))->setChecked(true);
    type(group->findChild<QLineEdit *>(QStringLiteral("leTunnelHostname")), QStringLiteral("mbp.example.com"));
    group->findChild<QSpinBox *>(QStringLiteral("sbTunnelLocalPort"))->setValue(24811);
    type(group->findChild<QLineEdit *>(QStringLiteral("leTokenClientId")), kId);
    type(group->findChild<QLineEdit *>(QStringLiteral("leTokenSecret")), kSecret);

    // nothing is written at each letter that is typed
    QVERIFY(stored(Settings::Tunnel::Enabled).isNull());
    QVERIFY(button(pages, QDialogButtonBox::Save)->isEnabled());
    button(pages, QDialogButtonBox::Save)->click();

    QCOMPARE(saved.size(), 1);
    QVERIFY(!applied.isEmpty());
    QCOMPARE(applied.last().first().toString(), Settings::Tunnel::Enabled);
    for (const auto &key : {Settings::Tunnel::Enabled, Settings::Tunnel::Hostname, Settings::Tunnel::LocalPort})
      throughThePage.insert(key, stored(key));
    // the token is in the store and not in the settings file, and after the save the group shows the Client ID only
    QVERIFY(!settingsFileContent().contains(kSecret.toUtf8()));
    QVERIFY(!settingsFileContent().contains(kId.toUtf8()));
    QVERIFY(group->findChild<QLineEdit *>(QStringLiteral("leTokenSecret"))->text().isEmpty());
    QVERIFY(!button(pages, QDialogButtonBox::Save)->isEnabled());
  }
  const auto pageToken = pageStore->load();
  QVERIFY(pageToken.has_value());
  QCOMPARE(pageToken->clientId, kId);

  // through the dialog, the same typed values, accepted
  forgetAll();
  Settings::setValue(Settings::Core::CoreMode, Settings::CoreMode::Client);
  auto dialogStore = std::make_shared<InMemorySecretStore>();
  QMap<QString, QVariant> throughTheDialog;
  {
    auto world = std::make_shared<World>();
    ClientConfigDialog dialog(nullptr, dependencies(world, dialogStore));
    auto *group = dialog.findChild<TunnelSettingsGroup *>();
    QVERIFY(group != nullptr);
    group->findChild<QCheckBox *>(QStringLiteral("cbTunnelEnabled"))->setChecked(true);
    type(group->findChild<QLineEdit *>(QStringLiteral("leTunnelHostname")), QStringLiteral("mbp.example.com"));
    group->findChild<QSpinBox *>(QStringLiteral("sbTunnelLocalPort"))->setValue(24811);
    type(group->findChild<QLineEdit *>(QStringLiteral("leTokenClientId")), kId);
    type(group->findChild<QLineEdit *>(QStringLiteral("leTokenSecret")), kSecret);
    Q_EMIT dialog.findChild<SettingsDialogButtonBox *>()->accepted();
    for (const auto &key : {Settings::Tunnel::Enabled, Settings::Tunnel::Hostname, Settings::Tunnel::LocalPort})
      throughTheDialog.insert(key, stored(key));
  }
  const auto dialogToken = dialogStore->load();
  QVERIFY(dialogToken.has_value());
  QCOMPARE(dialogToken->clientId, pageToken->clientId);
  QCOMPARE(dialogToken->secret, pageToken->secret);
  for (auto it = throughThePage.cbegin(); it != throughThePage.cend(); ++it) {
    QVERIFY2(it.value().isValid(), qPrintable(it.key()));
    QCOMPARE(it.value(), throughTheDialog.value(it.key()));
  }
}

void ClientPageTests::tunnel_whatCannotBeSavedWritesNothingAndSaysWhy()
{
  Settings::setValue(Settings::Core::CoreMode, Settings::CoreMode::Client);
  auto store = std::make_shared<InMemorySecretStore>();
  auto world = std::make_shared<World>();
  SettingsPages pages(nullptr, dependencies(world, store));
  auto *group = show(pages);
  QVERIFY(group != nullptr);
  QSignalSpy saved(&pages, &SettingsPages::tunnelSaved);

  // the tunnel on with a hostname that is not one: nothing is written, the reason is under the group
  const auto before = settingsFileContent();
  group->findChild<QCheckBox *>(QStringLiteral("cbTunnelEnabled"))->setChecked(true);
  auto *hostname = group->findChild<QLineEdit *>(QStringLiteral("leTunnelHostname"));
  hostname->setText(QStringLiteral("not a host name!"));
  Q_EMIT hostname->editingFinished();
  QVERIFY(!group->validationProblem().isEmpty());
  button(pages, QDialogButtonBox::Save)->click();
  QCOMPARE(saved.size(), 0);
  QCOMPARE(settingsFileContent(), before);
  QVERIFY(!store->load().has_value());
  auto *error = group->findChild<QLabel *>(QStringLiteral("lblTunnelError"));
  QVERIFY(error != nullptr);
  QCOMPARE(error->text(), group->validationProblem());
  QVERIFY(!error->isHidden());
}

void ClientPageTests::tunnel_resetTakesTheStoredValues_andRestoreDefaultsTheDefaults()
{
  Settings::setValue(Settings::Core::CoreMode, Settings::CoreMode::Client);
  Settings::setValue(Settings::Tunnel::Enabled, true);
  Settings::setValue(Settings::Tunnel::Hostname, QStringLiteral("stored.example.com"));
  auto world = std::make_shared<World>();
  SettingsPages pages(nullptr, dependencies(world, std::make_shared<InMemorySecretStore>()));
  auto *group = show(pages);
  auto *hostname = group->findChild<QLineEdit *>(QStringLiteral("leTunnelHostname"));
  QCOMPARE(hostname->text(), QStringLiteral("stored.example.com"));

  // typed, not saved: Reset gives the stored value back
  hostname->setText(QStringLiteral("typed.example.com"));
  Q_EMIT hostname->editingFinished();
  QVERIFY(button(pages, QDialogButtonBox::Reset)->isEnabled());
  button(pages, QDialogButtonBox::Reset)->click();
  QCOMPARE(hostname->text(), QStringLiteral("stored.example.com"));
  QVERIFY(!button(pages, QDialogButtonBox::Reset)->isEnabled());
  QCOMPARE(stored(Settings::Tunnel::Hostname).toString(), QStringLiteral("stored.example.com"));

  // Restore Defaults empties what is shown (and saves nothing until Save)
  QVERIFY(button(pages, QDialogButtonBox::RestoreDefaults)->isEnabled());
  button(pages, QDialogButtonBox::RestoreDefaults)->click();
  QVERIFY(hostname->text().isEmpty());
  QVERIFY(!group->findChild<QCheckBox *>(QStringLiteral("cbTunnelEnabled"))->isChecked());
  QCOMPARE(stored(Settings::Tunnel::Hostname).toString(), QStringLiteral("stored.example.com"));
  QVERIFY(button(pages, QDialogButtonBox::Save)->isEnabled());
  QVERIFY(!button(pages, QDialogButtonBox::RestoreDefaults)->isEnabled());
}

void ClientPageTests::tunnel_theButtonsFollowWhatIsEntered()
{
  Settings::setValue(Settings::Core::CoreMode, Settings::CoreMode::Client);
  auto world = std::make_shared<World>();
  SettingsPages pages(nullptr, dependencies(world, std::make_shared<InMemorySecretStore>()));
  auto *group = show(pages);
  // nothing entered, nothing stored: nothing to save or to reset, and nothing to restore (these are the defaults)
  QVERIFY(!button(pages, QDialogButtonBox::Save)->isEnabled());
  QVERIFY(!button(pages, QDialogButtonBox::Reset)->isEnabled());
  QVERIFY(!button(pages, QDialogButtonBox::RestoreDefaults)->isEnabled());
  group->findChild<QSpinBox *>(QStringLiteral("sbTunnelLocalPort"))->setValue(24900);
  QVERIFY(button(pages, QDialogButtonBox::Save)->isEnabled());
  QVERIFY(button(pages, QDialogButtonBox::Reset)->isEnabled());
  QVERIFY(button(pages, QDialogButtonBox::RestoreDefaults)->isEnabled());
}

QTEST_MAIN(ClientPageTests)
