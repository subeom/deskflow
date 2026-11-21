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
#include "gui/settings/SettingEffects.h"
#include "gui/settings/SettingsBinder.h"
#include "gui/settings/SettingsPage.h"
#include "gui/settings/SettingsPages.h"
#include "gui/widgets/SettingCard.h"
#include "gui/widgets/SettingRow.h"
#include "gui/widgets/ToggleSwitch.h"

#include <QAccessible>
#include <QApplication>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalSpy>

using namespace deskflow::awdl;
using namespace deskflow::gui::settings;
using namespace awdltest;

Q_DECLARE_METATYPE(Summary)

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
}

void AwdlPageTests::init()
{
  // the pages are those of a Mac unless a test says that they are not
  s_supportedForTest = true;
}

void AwdlPageTests::cleanup()
{
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

  Rig rig;
  SettingsPages pages;
  pages.setAwdlController(rig.controller);
  rig.say(LinkState::Up);
  for (const auto *name : {"btnAwdlMeasure", "btnAwdlTurnOff", "btnAwdlTurnOn"})
    check(QLatin1String(name), must<QPushButton>(pages, name)->text());
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

QTEST_MAIN(AwdlPageTests)
