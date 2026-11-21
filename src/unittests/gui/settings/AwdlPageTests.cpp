/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AwdlPageTests.h"

#include "../../gui/KoreanTranslator.h"
#include "../awdl/AwdlSamples.h"
#include "../awdl/FakeAwdlSystem.h"
#include "common/Settings.h"
#include "gui/awdl/AwdlController.h"
#include "gui/awdl/AwdlPlatform.h"
#include "gui/awdl/AwdlTexts.h"
#include "gui/settings/AwdlBanner.h"
#include "gui/settings/AwdlCard.h"
#include "gui/settings/SettingEffects.h"
#include "gui/settings/SettingsBinder.h"
#include "gui/settings/SettingsPage.h"
#include "gui/settings/SettingsPages.h"
#include "gui/widgets/SettingCard.h"
#include "gui/widgets/SettingRow.h"
#include "gui/widgets/ToggleSwitch.h"

#include <QAccessible>
#include <QApplication>
#include <QGuiApplication>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalSpy>

using namespace deskflow::awdl;
using namespace deskflow::gui::settings;
using namespace awdltest;

Q_DECLARE_METATYPE(Summary)
Q_DECLARE_METATYPE(Listing)

namespace {

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

// The row that has the control, and so the texts that a person reads next to it
SettingRow *rowOf(QWidget *control)
{
  return qobject_cast<SettingRow *>(control->parentWidget());
}

// A controller on a fake system, with the sentences of the dialog of the program
struct Rig
{
  Rig()
  {
    fake = new FakeAwdlSystem;
    AwdlController::Options options;
    options.optionOn = [] { return Settings::value(Settings::Network::MacAwdlOff).toBool(); };
    options.prompt = [](Direction direction) { return prompt(direction); };
    options.permissionPrompt = [](PermissionAction action) { return permissionPrompt(action); };
    controller = new AwdlController(fake, options);
  }
  ~Rig()
  {
    delete controller;
  }

  // the link is read as \p state, and the route is the one of a Wi-Fi
  void say(LinkState state)
  {
    fake->answerRead(state);
  }
  void measureWith(const QList<Sample> &samples)
  {
    controller->measureNow();
    fake->answerRoute(routeOf(LinkKind::WiFi));
    fake->answerMeasured(samples);
  }

  FakeAwdlSystem *fake = nullptr;
  AwdlController *controller = nullptr;
};

} // namespace

void AwdlPageTests::initTestCase()
{
  QVERIFY(m_dir.isValid());
  qputenv("DESKFLOW_SECRET_STORE", "memory");
  Settings::setSettingsFile(m_dir.filePath(QStringLiteral("Deskflow.conf")));
  Settings::setStateFile(m_dir.filePath(QStringLiteral("Deskflow.state")));
  QAccessible::setActive(true);
  qRegisterMetaType<Summary>();
  qRegisterMetaType<Listing>();
}

void AwdlPageTests::init()
{
  // the pages are those of a Mac unless a test says that they are not
  s_supportedForTest = true;
}

void AwdlPageTests::cleanup()
{
  AwdlCard::s_confirmForTest = nullptr;
  s_supportedForTest.reset();
  Settings::setValue(Settings::Network::MacAwdlOff);
}

void AwdlPageTests::card_isOnTheNetworkPageOfAMac_andIsNotMadeWhereThereIsNoAwdl()
{
  {
    SettingsPages pages;
    auto *network = pages.page(QStringLiteral("settings.network"));
    QVERIFY(network);
    auto *card = network->findChild<SettingCard *>(QStringLiteral("cardAwdl"));
    QVERIFY(card);
    QVERIFY(network->cards().contains(card));
    QVERIFY(find<ToggleSwitch>(pages, "cbMacAwdlOff"));
    QVERIFY(pages.binder()->keys().contains(Settings::Network::MacAwdlOff));
  }

  // Windows and Linux: nothing is made, nothing is bound, and the setting is not looked at
  s_supportedForTest = false;
  SettingsPages pages;
  auto *network = pages.page(QStringLiteral("settings.network"));
  QVERIFY(network);
  QVERIFY(!network->findChild<SettingCard *>(QStringLiteral("cardAwdl")));
  QVERIFY(!find<ToggleSwitch>(pages, "cbMacAwdlOff"));
  QVERIFY(!find<QPushButton>(pages, "btnAwdlTurnOff"));
  QVERIFY(!pages.binder()->keys().contains(Settings::Network::MacAwdlOff));
}

void AwdlPageTests::switch_isOffByDefault_writesTheSetting_andIsLive()
{
  SettingsPages pages;
  auto *toggle = must<ToggleSwitch>(pages, "cbMacAwdlOff");
  QVERIFY(!toggle->isChecked());
  QVERIFY(!Settings::value(Settings::Network::MacAwdlOff).toBool());

  QSignalSpy applied(&pages, &SettingsPages::applied);
  toggle->setChecked(true);
  QVERIFY(Settings::value(Settings::Network::MacAwdlOff).toBool());
  QCOMPARE(applied.count(), 1);
  QCOMPARE(applied.first().first().toString(), Settings::Network::MacAwdlOff);
  // the window follows it at once: it asks, or stops asking, for the password while the core runs
  QCOMPARE(effectOf(Settings::Network::MacAwdlOff), std::optional<Effect>(Effect::Live));

  toggle->setChecked(false);
  QVERIFY(!Settings::value(Settings::Network::MacAwdlOff).toBool());
}

void AwdlPageTests::switch_saysThatItIsNeededOnlyWhenTheMouseStutters()
{
  SettingsPages pages;
  auto *row = rowOf(must<ToggleSwitch>(pages, "cbMacAwdlOff"));
  QVERIFY(row);
  QCOMPARE(row->title(), QStringLiteral("Turn off AirDrop and Handoff's wireless link while sharing"));
  const auto text = row->description();
  QVERIFY2(text.contains(QStringLiteral("only if the mouse stutters")), qPrintable(text));
  // what stops working, and that the password of an administrator is asked for
  for (const auto &word : {"AirDrop", "Handoff", "Universal Control", "AirPlay", "administrator password"})
    QVERIFY2(text.contains(QLatin1String(word)), word);
}

void AwdlPageTests::stateText_saysEachStateOfTheLink_data()
{
  QTest::addColumn<Summary>("summary");
  QTest::addColumn<QString>("contains");

  QTest::newRow("not available") << Summary::NotAvailable << QStringLiteral("Not available");
  QTest::newRow("unknown") << Summary::Unknown << QStringLiteral("Not read yet");
  QTest::newRow("off") << Summary::Off << QStringLiteral("Off");
  QTest::newRow("on, on a wire") << Summary::OnWired << QStringLiteral("wired");
  QTest::newRow("on, not Wi-Fi") << Summary::OnNotWiFi << QStringLiteral("not Wi-Fi");
  QTest::newRow("on, not measured") << Summary::OnNotMeasured << QStringLiteral("not been measured");
  QTest::newRow("on, no data") << Summary::OnNoData << QStringLiteral("did not answer");
  QTest::newRow("on, clean") << Summary::OnClean << QStringLiteral("On, no latency problem seen");
  QTest::newRow("on, stutter") << Summary::OnStutter << QStringLiteral("spikes");
}

void AwdlPageTests::stateText_saysEachStateOfTheLink()
{
  QFETCH(Summary, summary);
  QFETCH(QString, contains);
  const auto text = stateText(summary);
  QVERIFY2(text.contains(contains), qPrintable(text));
  // a sentence, with nothing left of a placeholder, and no em dash
  QVERIFY2(!text.contains(QRegularExpression(QStringLiteral("%\\d"))), qPrintable(text));
  QVERIFY2(!text.contains(QChar(0x2014)), qPrintable(text));
  QVERIFY(text.endsWith(QLatin1Char('.')));
}

void AwdlPageTests::stateText_saysTheNumbersOfTheLastBurst()
{
  BurstStats stats;
  stats.pattern = Pattern::Stutter;
  stats.medianMs = 3.84;
  stats.worstMs = 139.5;
  stats.received = 40;
  const auto text = measurementText(stats);
  // under 10 ms one decimal, above it none: the numbers of the user's own measurements read the same way
  QCOMPARE(text, QStringLiteral("Last measurement: median 3.8 ms, worst 140 ms."));
  QCOMPARE(formatMs(0.5), QStringLiteral("0.5"));
  QCOMPARE(formatMs(9.94), QStringLiteral("9.9"));
  QCOMPARE(formatMs(10.0), QStringLiteral("10"));
  QCOMPARE(formatMs(53.4), QStringLiteral("53"));

  // nothing measured (or nothing answered): no numbers to say
  QCOMPARE(measurementText(BurstStats()), QString());
  stats.received = 0;
  QCOMPARE(measurementText(stats), QString());

  stats.received = 40;
  const auto banner = bannerText(stats);
  QVERIFY2(banner.contains(QStringLiteral("3.8")) && banner.contains(QStringLiteral("140")), qPrintable(banner));
  QVERIFY2(banner.contains(QStringLiteral("stutter")), qPrintable(banner));
}

void AwdlPageTests::card_followsTheController()
{
  Rig rig;
  SettingsPages pages;
  pages.setAwdlController(rig.controller);
  auto *state = rowOf(must<QPushButton>(pages, "btnAwdlMeasure"));
  QVERIFY(state);
  auto *off = must<QPushButton>(pages, "btnAwdlTurnOff");
  auto *on = must<QPushButton>(pages, "btnAwdlTurnOn");
  auto *measure = must<QPushButton>(pages, "btnAwdlMeasure");

  // not read yet: nothing to change
  QCOMPARE(state->description(), stateText(Summary::Unknown));
  QVERIFY(!off->isEnabled());
  QVERIFY(!on->isEnabled());

  rig.say(LinkState::Up);
  QVERIFY(state->description().startsWith(QStringLiteral("On")));
  QVERIFY(off->isEnabled());
  QVERIFY(!on->isEnabled());
  QVERIFY(measure->isEnabled());

  // the numbers of a burst are in the line, and the line says what they mean
  rig.measureWith(stutter());
  QVERIFY(state->description().contains(QStringLiteral("spikes")));
  QVERIFY(state->description().contains(QStringLiteral("Last measurement: median")));
  QVERIFY(measure->isEnabled());

  rig.say(LinkState::Down);
  QVERIFY(state->description().startsWith(QStringLiteral("Off")));
  QVERIFY(!off->isEnabled());
  QVERIFY(on->isEnabled());

  rig.say(LinkState::Unavailable);
  QCOMPARE(state->description(), stateText(Summary::NotAvailable));
  QVERIFY(!off->isEnabled());
  QVERIFY(!on->isEnabled());
  QVERIFY(!measure->isEnabled());
}

void AwdlPageTests::card_buttonsAskTheController()
{
  Rig rig;
  SettingsPages pages;
  pages.setAwdlController(rig.controller);
  auto *off = must<QPushButton>(pages, "btnAwdlTurnOff");
  auto *on = must<QPushButton>(pages, "btnAwdlTurnOn");
  auto *measure = must<QPushButton>(pages, "btnAwdlMeasure");

  rig.say(LinkState::Up);
  off->click();
  QCOMPARE(rig.fake->changes.size(), 1);
  QCOMPARE(rig.fake->changes.first().first, Direction::Down);
  // the dialog says what it is for, in the language of the program
  QCOMPARE(rig.fake->changes.first().second, prompt(Direction::Down));
  QVERIFY(!rig.fake->changes.first().second.isEmpty());
  // while the dialog is open no button asks again, and none can be pressed
  QVERIFY(!off->isEnabled());
  QVERIFY(!on->isEnabled());
  QVERIFY(!measure->isEnabled());
  rig.fake->answerLink(AdminResult::Done);
  rig.say(LinkState::Down);
  QVERIFY(on->isEnabled());

  on->click();
  QCOMPARE(rig.fake->changes.size(), 2);
  QCOMPARE(rig.fake->changes.last().first, Direction::Up);
  QCOMPARE(rig.fake->changes.last().second, prompt(Direction::Up));
  rig.fake->answerLink(AdminResult::Cancelled);
  rig.say(LinkState::Down);

  // the button of the measuring sends nothing by itself: it looks for the route first, and a burst follows the route
  QCOMPARE(rig.fake->routeLookups, 0);
  measure->click();
  QCOMPARE(rig.fake->routeLookups, 1);
  QVERIFY(!measure->isEnabled());
  QCOMPARE(measure->text(), QStringLiteral("Measuring..."));
  rig.fake->answerRoute(routeOf(LinkKind::WiFi));
  QCOMPARE(rig.fake->measures.size(), 1);
  rig.fake->answerMeasured(clean());
  QVERIFY(measure->isEnabled());
  QCOMPARE(measure->text(), QStringLiteral("Measure now"));
}

void AwdlPageTests::card_afterTheLinkWasTurnedOff_saysTheNumbersOfTheMeasurementThatFollows()
{
  // the way to see the effect: measure with the link up (spikes), turn it off, measure again (clean)
  Settings::setValue(Settings::Network::MacAwdlOff, true);
  Rig rig;
  SettingsPages pages;
  pages.setAwdlController(rig.controller);
  auto *state = rowOf(must<QPushButton>(pages, "btnAwdlMeasure"));
  rig.controller->setCoreRunning(true);
  rig.say(LinkState::Up);
  rig.fake->answerLink(AdminResult::Cancelled);
  rig.measureWith(stutter());
  QVERIFY(state->description().contains(QStringLiteral("worst 64 ms")));

  must<QPushButton>(pages, "btnAwdlTurnOff")->click();
  rig.fake->answerLink(AdminResult::Done);
  rig.say(LinkState::Down);
  QVERIFY(state->description().startsWith(QStringLiteral("Off")));
  rig.measureWith(clean());
  QVERIFY2(state->description().contains(QStringLiteral("median 2.")), qPrintable(state->description()));
  QVERIFY2(!state->description().contains(QStringLiteral("64")), qPrintable(state->description()));
  QVERIFY(state->description().startsWith(QStringLiteral("Off")));
}

void AwdlPageTests::card_withoutAController_hasItsButtonsOff()
{
  SettingsPages pages;
  for (const auto *name : {"btnAwdlMeasure", "btnAwdlTurnOff", "btnAwdlTurnOn"})
    QVERIFY2(!must<QPushButton>(pages, name)->isEnabled(), name);
  QCOMPARE(rowOf(must<QPushButton>(pages, "btnAwdlMeasure"))->description(), stateText(Summary::Unknown));

  // a controller that goes away leaves the card as it was made
  {
    Rig rig;
    pages.setAwdlController(rig.controller);
    rig.say(LinkState::Up);
    QVERIFY(must<QPushButton>(pages, "btnAwdlTurnOff")->isEnabled());
  }
  QVERIFY(!must<QPushButton>(pages, "btnAwdlTurnOff")->isEnabled());
  QCOMPARE(rowOf(must<QPushButton>(pages, "btnAwdlMeasure"))->description(), stateText(Summary::Unknown));
}

void AwdlPageTests::card_readsTheStateWhenThePageIsShown()
{
  Rig rig;
  SettingsPages pages;
  pages.setAwdlController(rig.controller);
  QCOMPARE(rig.fake->reads, 0);
  auto *network = pages.page(QStringLiteral("settings.network"));
  network->show();
  QVERIFY(QTest::qWaitForWindowExposed(network));
  // what the page lists from the system is that of the time it is looked at: the state of the link, and the route
  QVERIFY(rig.fake->reads >= 1);
  QVERIFY(rig.fake->routeLookups >= 1);
  // looking at it changes nothing (a read is no change)
  QVERIFY(rig.fake->changes.isEmpty());
  QVERIFY(rig.fake->measures.isEmpty());
}

void AwdlPageTests::banner_isHiddenUntilTheHint_saysTheNumbers_andItsButtonAsks()
{
  QWidget window;
  auto *banner = new AwdlBanner(&window);
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  QVERIFY(!banner->isPending());
  QVERIFY(!banner->isVisible());

  BurstStats stats;
  stats.pattern = Pattern::Stutter;
  stats.medianMs = 4.2;
  stats.worstMs = 139.0;
  stats.received = 40;
  banner->setStats(stats);
  banner->setPending(true);
  QVERIFY(banner->isPending());
  QVERIFY(banner->isVisible());
  QCOMPARE(banner->text(), bannerText(stats));
  QVERIFY(banner->text().contains(QStringLiteral("139")));
  QCOMPARE(banner->button()->text(), QStringLiteral("Turn off"));
  // a screen reader finds the sentence
  QCOMPARE(banner->accessibleName(), banner->text());

  QSignalSpy asked(banner, &AwdlBanner::turnOffRequested);
  banner->button()->click();
  QCOMPARE(asked.count(), 1);

  banner->setPending(false);
  QVERIFY(!banner->isVisible());
}

void AwdlPageTests::banner_whileTheDialogIsOpen_hasItsButtonOff()
{
  QWidget window;
  auto *banner = new AwdlBanner(&window);
  banner->setPending(true);
  window.show();
  QVERIFY(banner->button()->isEnabled());
  banner->setBusy(true);
  QVERIFY(!banner->button()->isEnabled());
  banner->setBusy(false);
  QVERIFY(banner->button()->isEnabled());
}

void AwdlPageTests::korean_everyTextOfTheOptionIsTranslated()
{
  auto *translator = new koreantest::RecordingTranslator;
  QVERIFY2(translator->loadShipped(), qPrintable(koreantest::shippedQm()));
  QVERIFY(QCoreApplication::installTranslator(translator));
  const auto remove = qScopeGuard([translator] {
    QCoreApplication::removeTranslator(translator);
    delete translator;
  });

  const QRegularExpression hangul(QStringLiteral("[가-힣]"));
  const QChar emDash(0x2014);
  QStringList wrong;
  const auto check = [&](const QString &name, const QString &text) {
    if (!text.contains(hangul))
      wrong << QStringLiteral("%1 is not Korean: \"%2\"").arg(name, text);
    if (text.contains(emDash))
      wrong << QStringLiteral("%1 has an em dash").arg(name);
    if (text.contains(QRegularExpression(QStringLiteral("%\\d"))))
      wrong << QStringLiteral("%1 has a placeholder left: \"%2\"").arg(name, text);
  };

  BurstStats stats;
  stats.pattern = Pattern::Stutter;
  stats.medianMs = 4.2;
  stats.worstMs = 139.0;
  stats.received = 40;
  for (const auto summary :
       {Summary::NotAvailable, Summary::Unknown, Summary::Off, Summary::OnWired, Summary::OnNotWiFi,
        Summary::OnNotMeasured, Summary::OnNoData, Summary::OnClean, Summary::OnStutter})
    check(QStringLiteral("state %1").arg(static_cast<int>(summary)), stateText(summary));
  check(QStringLiteral("measurement"), measurementText(stats));
  check(QStringLiteral("banner"), bannerText(stats));
  check(QStringLiteral("prompt down"), prompt(Direction::Down));
  check(QStringLiteral("prompt up"), prompt(Direction::Up));
  // the permission to change the link with no password
  for (const auto state : {PermissionState::Unknown, PermissionState::NotAllowed, PermissionState::Allowed}) {
    for (const bool present : {false, true})
      check(
          QStringLiteral("permission %1 %2").arg(permissionStateId(state)).arg(present), permissionText(state, present)
      );
  }
  for (const auto note :
       {PermissionNote::Added, PermissionNote::Removed, PermissionNote::Cancelled, PermissionNote::Failed,
        PermissionNote::Refused})
    check(QStringLiteral("note %1").arg(static_cast<int>(note)), permissionNoteText(note));
  check(QStringLiteral("question title"), confirmAllowTitle());
  check(QStringLiteral("question text"), confirmAllowText());
  check(QStringLiteral("prompt allow"), permissionPrompt(PermissionAction::Allow));
  check(QStringLiteral("prompt remove"), permissionPrompt(PermissionAction::Remove));
  // the file and the commands are named as they are, in any language
  for (const auto &keep :
       {QStringLiteral("/etc/sudoers.d/deskflow-awdl"), QStringLiteral("ifconfig awdl0 down"),
        QStringLiteral("ifconfig awdl0 up")}) {
    if (!confirmAllowText().contains(keep))
      wrong << QStringLiteral("the question lost %1").arg(keep);
  }
  if (!permissionPrompt(PermissionAction::Allow).contains(QStringLiteral("/etc/sudoers.d/deskflow-awdl")))
    wrong << QStringLiteral("the prompt lost the file");

  Rig rig;
  SettingsPages pages;
  pages.setAwdlController(rig.controller);
  rig.say(LinkState::Up);
  for (const auto *name :
       {"btnAwdlMeasure", "btnAwdlTurnOff", "btnAwdlTurnOn", "btnAwdlAllow", "btnAwdlRemovePermission"})
    check(QLatin1String(name), must<QPushButton>(pages, name)->text());
  check(QStringLiteral("permission title"), rowOf(must<QPushButton>(pages, "btnAwdlAllow"))->title());
  check(QStringLiteral("permission description"), rowOf(must<QPushButton>(pages, "btnAwdlAllow"))->description());
  auto *row = rowOf(must<ToggleSwitch>(pages, "cbMacAwdlOff"));
  check(QStringLiteral("switch title"), row->title());
  check(QStringLiteral("switch description"), row->description());
  check(QStringLiteral("change title"), rowOf(must<QPushButton>(pages, "btnAwdlTurnOff"))->title());
  check(QStringLiteral("change description"), rowOf(must<QPushButton>(pages, "btnAwdlTurnOff"))->description());
  check(QStringLiteral("state title"), rowOf(must<QPushButton>(pages, "btnAwdlMeasure"))->title());
  QWidget window;
  auto *banner = new AwdlBanner(&window);
  banner->setStats(stats);
  check(QStringLiteral("banner button"), banner->button()->text());
  check(QStringLiteral("banner text"), banner->text());

  QVERIFY2(wrong.isEmpty(), qPrintable(wrong.join(QLatin1Char('\n'))));
  // what was asked for and is not in the file: none of it is the option's
  QStringList mine;
  for (const auto &name : translator->missing()) {
    if (name.contains(QStringLiteral("Awdl")) || name.contains(QStringLiteral("wireless link")))
      mine << name;
  }
  QVERIFY2(mine.isEmpty(), qPrintable(mine.join(QLatin1Char('\n'))));
}

void AwdlPageTests::permissionRow_hasItsTextsAndButtons_andTheTextsSaySheDoesNotAskTwice()
{
  SettingsPages pages;
  auto *allow = must<QPushButton>(pages, "btnAwdlAllow");
  auto *remove = must<QPushButton>(pages, "btnAwdlRemovePermission");
  auto *row = rowOf(allow);
  QVERIFY(row);
  QCOMPARE(rowOf(remove), row);
  QCOMPARE(row->title(), QStringLiteral("Change without a password"));
  QCOMPARE(allow->text(), QStringLiteral("Allow without password..."));
  QCOMPARE(remove->text(), QStringLiteral("Remove permission"));
  // with no controller there is nothing to ask: both buttons are off, and the line says that nothing is known
  QVERIFY(!allow->isEnabled());
  QVERIFY(!remove->isEnabled());
  QCOMPARE(row->description(), permissionText(PermissionState::Unknown, false));

  // the other texts of the card say that the password is asked once for the permission, and then never
  const auto option = rowOf(must<ToggleSwitch>(pages, "cbMacAwdlOff"))->description();
  QVERIFY2(option.contains(QStringLiteral("administrator password")), qPrintable(option));
  QVERIFY2(option.contains(QStringLiteral("never if you allow it without a password")), qPrintable(option));
  const auto change = rowOf(must<QPushButton>(pages, "btnAwdlTurnOff"))->description();
  QVERIFY2(change.contains(QStringLiteral("unless it is allowed without a password")), qPrintable(change));
}

void AwdlPageTests::permissionRow_followsTheController_data()
{
  QTest::addColumn<Listing>("listing");
  QTest::addColumn<bool>("filePresent");
  QTest::addColumn<QString>("startsWith");
  QTest::addColumn<bool>("allowEnabled");
  QTest::addColumn<bool>("removeEnabled");

  QTest::newRow("allowed by the file of the program")
      << Listing::NoPassword << true << QStringLiteral("Allowed.") << false << true;
  // a rule of someone else's allows it: nothing to add, nothing of ours to remove
  QTest::newRow("allowed by another rule")
      << Listing::NoPassword << false << QStringLiteral("Allowed.") << true << false;
  QTest::newRow("not allowed, no file") << Listing::PasswordNeeded << false << QStringLiteral("Not allowed.") << true
                                        << false;
  QTest::newRow("not allowed, the file is there")
      << Listing::PasswordNeeded << true << QStringLiteral("Not allowed. The file") << true << true;
  QTest::newRow("unknown, no file") << Listing::Unknown << false << QStringLiteral("Unknown.") << true << false;
  QTest::newRow("unknown, the file is there") << Listing::Unknown << true << QStringLiteral("Unknown.") << true << true;
}

void AwdlPageTests::permissionRow_followsTheController()
{
  QFETCH(Listing, listing);
  QFETCH(bool, filePresent);
  QFETCH(QString, startsWith);
  QFETCH(bool, allowEnabled);
  QFETCH(bool, removeEnabled);
  Rig rig;
  rig.fake->listing = listing;
  rig.fake->filePresent = filePresent;
  SettingsPages pages;
  pages.setAwdlController(rig.controller);
  auto *allow = must<QPushButton>(pages, "btnAwdlAllow");
  auto *remove = must<QPushButton>(pages, "btnAwdlRemovePermission");
  auto *row = rowOf(allow);
  // the page that is shown looks at the permission
  rig.controller->refresh();
  QVERIFY2(row->description().startsWith(startsWith), qPrintable(row->description()));
  QCOMPARE(allow->isEnabled(), allowEnabled);
  QCOMPARE(remove->isEnabled(), removeEnabled);

  // while a dialog is open nothing else can be asked for
  rig.controller->turnOffNow();
  QVERIFY(rig.controller->isAsking());
  QVERIFY(!allow->isEnabled());
  QVERIFY(!remove->isEnabled());
}

void AwdlPageTests::allowButton_asksTheQuestionFirst_andTheDialogOpensOnlyOnYes()
{
  Rig rig;
  SettingsPages pages;
  pages.setAwdlController(rig.controller);
  rig.controller->refresh();
  auto *allow = must<QPushButton>(pages, "btnAwdlAllow");

  // a no to the question: the dialog of the password never opens, and nothing is asked of the system
  QStringList questions;
  bool answer = false;
  AwdlCard::s_confirmForTest = [&](const QString &title, const QString &text) {
    questions << title + QLatin1Char('|') + text;
    return answer;
  };
  allow->click();
  QCOMPARE(questions.size(), 1);
  QVERIFY(rig.fake->installs.isEmpty());
  QVERIFY(!rig.controller->isAsking());

  // a yes: the dialog, once, with the sentence of the program for it, and the question was asked before it
  answer = true;
  allow->click();
  QCOMPARE(questions.size(), 2);
  QCOMPARE(rig.fake->installs, (QStringList{permissionPrompt(PermissionAction::Allow)}));
  QVERIFY(rig.controller->isAsking());
  // while it is open the button is off and the question is not asked again
  QVERIFY(!allow->isEnabled());
  QCOMPARE(questions.size(), 2);
  QVERIFY(rig.fake->removals.isEmpty());
  QVERIFY(rig.fake->changes.isEmpty());
}

void AwdlPageTests::theQuestion_saysWhatIsAdded_whatItAllows_whoElseCanDoIt_andHowToRemoveIt()
{
  QCOMPARE(confirmAllowTitle(), QStringLiteral("Allow without a password?"));
  const auto text = confirmAllowText();
  // plain words, in the order of what a person wants to know
  for (const auto &part :
       {QStringLiteral("asks for the administrator password once"), QStringLiteral("one small file"),
        QStringLiteral("/etc/sudoers.d/deskflow-awdl"), QStringLiteral("your account"),
        QStringLiteral("only \"ifconfig awdl0 down\""), QStringLiteral("\"ifconfig awdl0 up\""),
        QStringLiteral("as root without a password"),
        QStringLiteral("Any program that runs as you can then do the same"), QStringLiteral("AirDrop and Handoff"),
        QStringLiteral("\"Remove permission\""), QStringLiteral("deleting the file")})
    QVERIFY2(text.contains(part), qPrintable(part));
  QVERIFY(
      text.indexOf(QStringLiteral("one small file")) < text.indexOf(QStringLiteral("Any program that runs as you"))
  );
  QVERIFY(
      text.indexOf(QStringLiteral("Any program that runs as you")) < text.indexOf(QStringLiteral("Remove permission"))
  );
  // the sentences of the dialogs of the system name the file as well
  QVERIFY(permissionPrompt(PermissionAction::Allow).contains(QStringLiteral("/etc/sudoers.d/deskflow-awdl")));
  QVERIFY(permissionPrompt(PermissionAction::Remove).contains(QStringLiteral("/etc/sudoers.d/deskflow-awdl")));
  for (const auto &sentence :
       {text, permissionPrompt(PermissionAction::Allow), permissionPrompt(PermissionAction::Remove),
        confirmAllowTitle()})
    QVERIFY2(!sentence.contains(QChar(0x2014)), qPrintable(sentence));
}

void AwdlPageTests::allowButton_withNoAnswerOffscreen_isNoAndOpensNothing()
{
  // a test that forgot to answer the question never shows it (the platform is offscreen), and the answer is no
  QCOMPARE(QGuiApplication::platformName(), QStringLiteral("offscreen"));
  AwdlCard::s_confirmForTest = nullptr;
  Rig rig;
  SettingsPages pages;
  pages.setAwdlController(rig.controller);
  rig.controller->refresh();
  must<QPushButton>(pages, "btnAwdlAllow")->click();
  QVERIFY(rig.fake->installs.isEmpty());
  QVERIFY(!rig.controller->isAsking());
}

void AwdlPageTests::removeButton_opensTheDialogAtOnce()
{
  Rig rig;
  rig.fake->listing = Listing::NoPassword;
  rig.fake->filePresent = true;
  SettingsPages pages;
  pages.setAwdlController(rig.controller);
  rig.controller->refresh();
  auto *remove = must<QPushButton>(pages, "btnAwdlRemovePermission");
  QVERIFY(remove->isEnabled());
  // taking a permission away is the safe direction: no question, only the dialog of the password
  bool asked = false;
  AwdlCard::s_confirmForTest = [&](const QString &, const QString &) {
    asked = true;
    return true;
  };
  remove->click();
  QVERIFY(!asked);
  QCOMPARE(rig.fake->removals, (QStringList{permissionPrompt(PermissionAction::Remove)}));
  QVERIFY(rig.fake->installs.isEmpty());
}

void AwdlPageTests::permissionRow_saysWhatCameOfTheLastThing()
{
  Rig rig;
  SettingsPages pages;
  pages.setAwdlController(rig.controller);
  rig.controller->refresh();
  auto *row = rowOf(must<QPushButton>(pages, "btnAwdlAllow"));
  AwdlCard::s_confirmForTest = [](const QString &, const QString &) { return true; };

  must<QPushButton>(pages, "btnAwdlAllow")->click();
  rig.fake->answerPermissionSet(AdminResult::Cancelled);
  QVERIFY2(
      row->description().contains(QStringLiteral("The password was not given. Nothing was changed.")),
      qPrintable(row->description())
  );
  QVERIFY(row->description().startsWith(QStringLiteral("Not allowed.")));

  must<QPushButton>(pages, "btnAwdlAllow")->click();
  // the note of the last time is gone while the dialog is open
  QVERIFY2(!row->description().contains(QStringLiteral("Nothing was changed")), qPrintable(row->description()));
  rig.fake->answerPermissionSet(AdminResult::Refused);
  QVERIFY2(row->description().contains(QStringLiteral("cannot be used in the file")), qPrintable(row->description()));

  must<QPushButton>(pages, "btnAwdlAllow")->click();
  rig.fake->listing = Listing::NoPassword;
  rig.fake->filePresent = true;
  rig.fake->answerPermissionSet(AdminResult::Done);
  QVERIFY2(row->description().startsWith(QStringLiteral("Allowed.")), qPrintable(row->description()));
  QVERIFY2(row->description().contains(QStringLiteral("The file was added.")), qPrintable(row->description()));
}

void AwdlPageTests::permissionRow_isAbsentWhereThereIsNoAwdl()
{
  s_supportedForTest = false;
  SettingsPages pages;
  QVERIFY(!find<QPushButton>(pages, "btnAwdlAllow"));
  QVERIFY(!find<QPushButton>(pages, "btnAwdlRemovePermission"));
  // and the controller of nothing is no problem
  pages.setAwdlController(nullptr);
}

QTEST_MAIN(AwdlPageTests)
