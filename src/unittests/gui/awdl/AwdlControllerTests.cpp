/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AwdlControllerTests.h"

#include "AwdlSamples.h"
#include "FakeAwdlSystem.h"
#include "gui/awdl/AwdlController.h"

#include <QMap>
#include <QSignalSpy>

using namespace deskflow::awdl;
using namespace awdltest;

Q_DECLARE_METATYPE(LinkState)

namespace {

// A controller with a fake system. The reading of the state again is every 15 ms in a test
struct Rig
{
  explicit Rig(bool optionOn = true, bool supported = true)
  {
    option = optionOn;
    auto *fake = new FakeAwdlSystem;
    system = fake;
    AwdlController::Options options;
    options.optionOn = [this] { return option; };
    options.prompt = [](Direction direction) {
      return direction == Direction::Down ? QStringLiteral("turn it off") : QStringLiteral("turn it on");
    };
    options.pollMs = 15;
    options.burstEveryMs = 80;
    options.supported = supported;
    controller = new AwdlController(fake, options);
  }
  ~Rig()
  {
    delete controller;
  }

  // the core starts and the state that is read is \p state
  void start(LinkState state)
  {
    controller->setCoreRunning(true);
    system->answerRead(state);
  }

  // the controller looked up the route and made a burst: the route is \p route and the samples are \p samples
  void probe(const QList<Sample> &samples, const Route &route = routeOf(LinkKind::WiFi))
  {
    system->answerRoute(route);
    system->answerMeasured(samples);
  }

  bool option = true;
  FakeAwdlSystem *system = nullptr;
  AwdlController *controller = nullptr;
};

} // namespace

void AwdlControllerTests::optionOff_runsNothingAtAll()
{
  Rig rig(false);
  rig.controller->setCoreRunning(true);
  QTest::qWait(60);
  QCOMPARE(rig.system->reads, 0);
  QCOMPARE(rig.system->routeLookups, 0);
  QCOMPARE(rig.system->measures.size(), 0);
  QCOMPARE(rig.system->changes.size(), 0);
  QVERIFY(!rig.controller->hintVisible());
  // the state that someone reads is not acted on
  rig.system->answerRead(LinkState::Up);
  QCOMPARE(rig.system->changes.size(), 0);
  QVERIFY(!rig.controller->hintVisible());
  rig.controller->setCoreRunning(false);
  QCOMPARE(rig.system->reads, 0);
}

void AwdlControllerTests::notAMac_runsNothingAtAll_notEvenTheButtons()
{
  Rig rig(true, false);
  rig.controller->setCoreRunning(true);
  rig.controller->refresh();
  rig.controller->optionChanged();
  rig.controller->turnOffNow();
  rig.controller->turnBackOn();
  rig.controller->measureNow();
  QTest::qWait(60);
  QCOMPARE(rig.system->reads, 0);
  QCOMPARE(rig.system->routeLookups, 0);
  QCOMPARE(rig.system->measures.size(), 0);
  QCOMPARE(rig.system->changes.size(), 0);
  QCOMPARE(rig.controller->state(), LinkState::Unknown);
}

void AwdlControllerTests::theCoreStarts_withTheLinkUp_asksOnce_andReadsAgainToCheck()
{
  Rig rig;
  rig.controller->setCoreRunning(true);
  QCOMPARE(rig.system->reads, 1);
  QCOMPARE(rig.system->changes.size(), 0);

  rig.system->answerRead(LinkState::Up);
  QCOMPARE(rig.controller->state(), LinkState::Up);
  QCOMPARE(rig.system->changes.size(), 1);
  QCOMPARE(rig.system->changes.first().first, Direction::Down);
  QCOMPARE(rig.system->changes.first().second, QStringLiteral("turn it off"));
  QVERIFY(rig.controller->isAsking());
  QVERIFY(!rig.controller->hintVisible());

  // the password was given: the state is read to see it (what the command said is not taken for the state)
  const int before = rig.system->reads;
  rig.system->answerLink(AdminResult::Done);
  QVERIFY(!rig.controller->isAsking());
  QCOMPARE(rig.system->reads, before + 1);
  rig.system->answerRead(LinkState::Down);
  QCOMPARE(rig.controller->state(), LinkState::Down);
  QVERIFY(!rig.controller->hintVisible());
  // and it is not asked again, however long the core runs
  for (int poll = 0; poll < 4; ++poll) {
    QTRY_VERIFY(rig.system->reads > before + 1 + poll);
    rig.system->answerRead(LinkState::Down);
  }
  QCOMPARE(rig.system->changes.size(), 1);
}

void AwdlControllerTests::aNo_isLoggedOnce_leavesTheHint_andIsNeverAskedAgainByItself()
{
  Rig rig;
  // the lines about the password (the bursts have lines of their own, about the latency)
  QStringList log;
  QObject::connect(rig.controller, &AwdlController::logLine, [&log](const QString &line) {
    if (!line.startsWith(QLatin1String("latency")))
      log << line;
  });
  rig.start(LinkState::Up);
  QCOMPARE(rig.system->changes.size(), 1);
  rig.system->answerLink(AdminResult::Cancelled);
  rig.system->answerRead(LinkState::Up);

  QCOMPARE(log.count(), 1);
  QVERIFY2(log.first().contains(QStringLiteral("not given")), qPrintable(log.first()));
  // the link is up and the person said no: the hint is there when the latency has the spikes
  QVERIFY(!rig.controller->hintVisible());
  rig.probe(stutter());
  QVERIFY(rig.controller->hintVisible());
  // the link stays up for a minute: it is read many times, and the dialog is never shown again by itself
  for (int poll = 0; poll < 6; ++poll) {
    const int reads = rig.system->reads;
    QTRY_VERIFY(rig.system->reads > reads);
    rig.system->answerRead(LinkState::Up);
  }
  QCOMPARE(rig.system->changes.size(), 1);
  QVERIFY(rig.controller->hintVisible());
  QCOMPARE(log.count(), 1);
}

void AwdlControllerTests::aFailure_isLoggedAsAFailure_andLeavesTheHint()
{
  Rig rig;
  QStringList log;
  QObject::connect(rig.controller, &AwdlController::logLine, [&log](const QString &line) {
    if (!line.startsWith(QLatin1String("latency")))
      log << line;
  });
  rig.start(LinkState::Up);
  rig.system->answerLink(AdminResult::Failed);
  rig.system->answerRead(LinkState::Up);
  rig.probe(stutter());
  QCOMPARE(log.count(), 1);
  QVERIFY2(log.first().contains(QStringLiteral("could not")), qPrintable(log.first()));
  QVERIFY(rig.controller->hintVisible());
  QCOMPARE(rig.system->changes.size(), 1);
}

void AwdlControllerTests::theLinkComesBackWhileTheCoreRuns_isAHint_andTheButtonAsks()
{
  Rig rig;
  rig.start(LinkState::Up);
  rig.system->answerLink(AdminResult::Done);
  rig.system->answerRead(LinkState::Down);
  QVERIFY(!rig.controller->hintVisible());
  QSignalSpy hint(rig.controller, &AwdlController::hintChanged);

  // AirDrop turned it on again: it is up, and the latency says whether it matters
  const int reads = rig.system->reads;
  QTRY_VERIFY(rig.system->reads > reads);
  rig.system->answerRead(LinkState::Up);
  QVERIFY(!rig.controller->hintVisible());
  rig.probe(stutter());
  QVERIFY(rig.controller->hintVisible());
  QCOMPARE(hint.count(), 1);
  QCOMPARE(hint.first().first().toBool(), true);
  QCOMPARE(rig.system->changes.size(), 1); // no dialog by itself

  // the button of the hint asks for the password, and when the link is down the hint is gone
  rig.controller->turnOffNow();
  QCOMPARE(rig.system->changes.size(), 2);
  QCOMPARE(rig.system->changes.last().first, Direction::Down);
  rig.system->answerLink(AdminResult::Done);
  rig.system->answerRead(LinkState::Down);
  QVERIFY(!rig.controller->hintVisible());
}

void AwdlControllerTests::theCoreStops_restoresNothing_andStopsReading()
{
  Rig rig;
  rig.start(LinkState::Up);
  rig.system->answerLink(AdminResult::Done);
  rig.system->answerRead(LinkState::Down);
  rig.controller->setCoreRunning(false);
  QVERIFY(!rig.controller->hintVisible());
  const int reads = rig.system->reads;
  QTest::qWait(80);
  QCOMPARE(rig.system->reads, reads); // not read while nothing runs
  QCOMPARE(rig.system->ups(), 0);
  QCOMPARE(rig.system->changes.size(), 1);

  // the link comes on in the meantime and the core is stopped: nothing is asked, nothing is shown
  rig.system->answerRead(LinkState::Up);
  QVERIFY(!rig.controller->hintVisible());
  QCOMPARE(rig.system->changes.size(), 1);
}

void AwdlControllerTests::aNewRunOfTheCore_asksAgain_once()
{
  Rig rig;
  rig.start(LinkState::Up);
  rig.system->answerLink(AdminResult::Cancelled);
  rig.system->answerRead(LinkState::Up);
  rig.probe(stutter());
  QVERIFY(rig.controller->hintVisible());
  rig.controller->setCoreRunning(false);
  QVERIFY(!rig.controller->hintVisible());

  // the person starts the sharing again: the run is a new one
  rig.start(LinkState::Up);
  QCOMPARE(rig.system->changes.size(), 2);
  rig.system->answerLink(AdminResult::Cancelled);
  rig.system->answerRead(LinkState::Up);
  rig.probe(stutter());
  QCOMPARE(rig.system->changes.size(), 2);
  QVERIFY(rig.controller->hintVisible());
}

void AwdlControllerTests::theOptionChangedWhileTheCoreRuns_isFollowedAtOnce()
{
  Rig rig(false);
  rig.controller->setCoreRunning(true);
  QCOMPARE(rig.system->reads, 0);

  rig.option = true;
  rig.controller->optionChanged();
  QCOMPARE(rig.system->reads, 1);
  rig.system->answerRead(LinkState::Up);
  QCOMPARE(rig.system->changes.size(), 1);
  rig.system->answerLink(AdminResult::Cancelled);
  rig.system->answerRead(LinkState::Up);
  rig.probe(stutter());
  QVERIFY(rig.controller->hintVisible());

  // turned off: the hint goes, nothing is read or sent, nothing is restored
  rig.option = false;
  rig.controller->optionChanged();
  QVERIFY(!rig.controller->hintVisible());
  const int reads = rig.system->reads;
  const int lookups = rig.system->routeLookups;
  QTest::qWait(120);
  QCOMPARE(rig.system->reads, reads);
  QCOMPARE(rig.system->routeLookups, lookups);
  QCOMPARE(rig.system->ups(), 0);

  // turned on again in the same run: it is asked again (the person chose it again)
  rig.option = true;
  rig.controller->optionChanged();
  rig.system->answerRead(LinkState::Up);
  QCOMPARE(rig.system->changes.size(), 2);
}

void AwdlControllerTests::turnBackOn_isNotTakenBackInThisRun()
{
  Rig rig;
  rig.start(LinkState::Up);
  rig.system->answerLink(AdminResult::Done);
  rig.system->answerRead(LinkState::Down);

  rig.controller->turnBackOn();
  QCOMPARE(rig.system->changes.size(), 2);
  QCOMPARE(rig.system->changes.last().first, Direction::Up);
  QCOMPARE(rig.system->changes.last().second, QStringLiteral("turn it on"));
  rig.system->answerLink(AdminResult::Done);
  rig.system->answerRead(LinkState::Up);
  // it is up and the core runs with the option on: no dialog, no hint, for this run
  QCOMPARE(rig.system->changes.size(), 2);
  QVERIFY(!rig.controller->hintVisible());
  for (int poll = 0; poll < 3; ++poll) {
    const int reads = rig.system->reads;
    QTRY_VERIFY(rig.system->reads > reads);
    rig.system->answerRead(LinkState::Up);
  }
  QCOMPARE(rig.system->changes.size(), 2);
  QVERIFY(!rig.controller->hintVisible());
}

void AwdlControllerTests::turnOffNow_isTheButton_whateverTheOptionAndTheCore()
{
  Rig rig(false);
  QVERIFY(!rig.controller->coreRunning());
  rig.controller->turnOffNow();
  QCOMPARE(rig.system->changes.size(), 1);
  QCOMPARE(rig.system->changes.first().first, Direction::Down);
  QVERIFY(rig.controller->isAsking());
  rig.system->answerLink(AdminResult::Done);
  rig.system->answerRead(LinkState::Down);
  QCOMPARE(rig.controller->state(), LinkState::Down);
  // and nothing else happened: not a read of its own, no second command
  QCOMPARE(rig.system->changes.size(), 1);
  QVERIFY(!rig.controller->hintVisible());
}

void AwdlControllerTests::onlyOneDialogAtATime()
{
  Rig rig;
  rig.start(LinkState::Up);
  QCOMPARE(rig.system->changes.size(), 1);
  rig.controller->turnOffNow();
  rig.controller->turnBackOn();
  rig.controller->turnOffNow();
  QCOMPARE(rig.system->changes.size(), 1);
  rig.system->answerLink(AdminResult::Done);
  QVERIFY(!rig.controller->isAsking());
}

void AwdlControllerTests::aStateThatIsNotKnown_neverAsks()
{
  Rig rig;
  for (const auto state : {LinkState::Unknown, LinkState::Unavailable, LinkState::Down}) {
    rig.controller->setCoreRunning(true);
    rig.system->answerRead(state);
    QCOMPARE(rig.system->changes.size(), 0);
    QVERIFY(!rig.controller->hintVisible());
    rig.controller->setCoreRunning(false);
  }
}

void AwdlControllerTests::theSentenceOfTheDialog_isTheOneThatIsGiven()
{
  Rig rig;
  rig.controller->turnOffNow();
  rig.system->answerLink(AdminResult::Done);
  rig.controller->turnBackOn();
  QCOMPARE(rig.system->changes.at(0).second, QStringLiteral("turn it off"));
  QCOMPARE(rig.system->changes.at(1).second, QStringLiteral("turn it on"));
}

void AwdlControllerTests::signals_sayOnlyWhatChanged()
{
  Rig rig;
  qRegisterMetaType<LinkState>();
  QSignalSpy state(rig.controller, &AwdlController::stateChanged);
  QSignalSpy asking(rig.controller, &AwdlController::askingChanged);
  rig.start(LinkState::Up);
  QCOMPARE(state.count(), 1);
  QCOMPARE(asking.count(), 1);
  rig.system->answerLink(AdminResult::Done);
  QCOMPARE(asking.count(), 2);
  rig.system->answerRead(LinkState::Up);
  // the same state is not said again
  QCOMPARE(state.count(), 1);
  rig.system->answerRead(LinkState::Down);
  QCOMPARE(state.count(), 2);
}

void AwdlControllerTests::theLinkIsNeverTurnedOnByItself_inAnyRun()
{
  // many runs, every answer, the link up and down: the only command that the program gives by itself is "down", and
  // "up" is only the button
  Rig rig;
  for (const auto answer : {AdminResult::Done, AdminResult::Cancelled, AdminResult::Failed}) {
    for (const auto state : {LinkState::Up, LinkState::Down, LinkState::Unavailable, LinkState::Unknown}) {
      rig.controller->setCoreRunning(true);
      rig.system->answerRead(state);
      if (rig.controller->isAsking())
        rig.system->answerLink(answer);
      rig.system->answerRead(state);
      QTest::qWait(20);
      rig.system->answerRead(LinkState::Up);
      rig.controller->setCoreRunning(false);
      rig.system->answerRead(LinkState::Up);
    }
  }
  QCOMPARE(rig.system->ups(), 0);
  QVERIFY(rig.system->downs() > 0);
}

void AwdlControllerTests::theLinkBeingUpIsNoProblem_withoutTheSpikesThereIsNoHint()
{
  // measured on a Mac: the link up and AWDL not in use has the latency of a quiet network (3.8 ms, 6.7 ms at the worst)
  Rig rig;
  rig.start(LinkState::Up);
  rig.system->answerLink(AdminResult::Cancelled);
  rig.system->answerRead(LinkState::Up);
  QCOMPARE(rig.system->routeLookups, 1);
  rig.probe(clean());
  QVERIFY(!rig.controller->hintVisible());
  QCOMPARE(rig.controller->latency().pattern, Pattern::Clean);
  QCOMPARE(rig.controller->summary(), Summary::OnClean);
  QCOMPARE(rig.system->changes.size(), 1);
  // and no hint later either, while the bursts stay clean
  for (int round = 0; round < 3; ++round) {
    const int lookups = rig.system->routeLookups;
    QTRY_VERIFY_WITH_TIMEOUT(
        rig.system->routeLookups > lookups || (rig.system->answerRead(LinkState::Up), false), 2000
    );
    rig.probe(clean());
    QVERIFY(!rig.controller->hintVisible());
  }
}

void AwdlControllerTests::theSpikes_withTheLinkUp_areTheHint_andTheyGoWithTheLink()
{
  Rig rig;
  rig.start(LinkState::Up);
  rig.system->answerLink(AdminResult::Cancelled);
  rig.system->answerRead(LinkState::Up);
  rig.probe(stutter());
  QVERIFY(rig.controller->hintVisible());
  QCOMPARE(rig.controller->summary(), Summary::OnStutter);
  QSignalSpy hint(rig.controller, &AwdlController::hintChanged);

  // the person turned it off (the button of the hint): the link is down, the hint goes, and the spikes that were
  // measured are not the verdict for a link that comes up later
  rig.controller->turnOffNow();
  rig.system->answerLink(AdminResult::Done);
  rig.system->answerRead(LinkState::Down);
  QVERIFY(!rig.controller->hintVisible());
  QCOMPARE(hint.count(), 1);
  QCOMPARE(rig.controller->summary(), Summary::Off);
  rig.system->answerRead(LinkState::Up);
  QVERIFY2(!rig.controller->hintVisible(), "the spikes of the link that was up before are not these");
  QCOMPARE(rig.controller->summary(), Summary::OnNotMeasured);
}

void AwdlControllerTests::aWire_isSaid_andNothingIsSent()
{
  Rig rig;
  rig.start(LinkState::Up);
  rig.system->answerLink(AdminResult::Cancelled);
  rig.system->answerRead(LinkState::Up);
  QCOMPARE(rig.system->routeLookups, 1);
  rig.system->answerRoute(routeOf(LinkKind::Wired));
  QCOMPARE(rig.system->measures.size(), 0);
  QVERIFY(!rig.controller->hintVisible());
  QVERIFY(!rig.controller->isMeasuring());
  QCOMPARE(rig.controller->route().kind, LinkKind::Wired);
  QCOMPARE(rig.controller->summary(), Summary::OnWired);
  // however long it stays on the wire
  QTest::qWait(200);
  rig.system->answerRead(LinkState::Up);
  QCOMPARE(rig.system->measures.size(), 0);
}

void AwdlControllerTests::aGatewayThatIsNotLocal_orAConnectionThatIsNotWiFi_isNeverMeasured()
{
  for (const auto &route :
       {routeOf(LinkKind::WiFi, QStringLiteral("8.8.8.8")), routeOf(LinkKind::WiFi, QString()),
        routeOf(LinkKind::WiFi, QStringLiteral("link#14")),
        routeOf(LinkKind::WiFi, QStringLiteral("192.168.0.1"), QStringLiteral("awdl0")),
        routeOf(LinkKind::Other, QStringLiteral("10.8.0.1"), QStringLiteral("utun4")), routeOf(LinkKind::Unknown),
        Route()}) {
    Rig rig;
    rig.start(LinkState::Up);
    rig.system->answerLink(AdminResult::Cancelled);
    rig.system->answerRead(LinkState::Up);
    rig.system->answerRoute(route);
    QCOMPARE(rig.system->measures.size(), 0);
    QVERIFY(!rig.controller->hintVisible());
    // the button as well
    rig.controller->measureNow();
    rig.system->answerRoute(route);
    QCOMPARE(rig.system->measures.size(), 0);
  }
}

void AwdlControllerTests::aBurst_goesToTheRouteThatWasFound_andToNothingElse()
{
  Rig rig;
  rig.start(LinkState::Up);
  rig.system->answerLink(AdminResult::Cancelled);
  rig.system->answerRead(LinkState::Up);
  const auto route = routeOf(LinkKind::WiFi, QStringLiteral("10.20.30.1"), QStringLiteral("en0"));
  rig.system->answerRoute(route);
  QCOMPARE(rig.system->measures.size(), 1);
  QCOMPARE(rig.system->measures.first(), route);
  QVERIFY(rig.controller->isMeasuring());
  rig.system->answerMeasured(clean());
  QVERIFY(!rig.controller->isMeasuring());
  QCOMPARE(rig.controller->route(), route);

  // the next burst looks the route up again (the Wi-Fi may be another one): it goes where the route goes now
  const int lookups = rig.system->routeLookups;
  QTRY_VERIFY_WITH_TIMEOUT(rig.system->routeLookups > lookups || (rig.system->answerRead(LinkState::Up), false), 2000);
  const auto other = routeOf(LinkKind::WiFi, QStringLiteral("172.20.10.1"), QStringLiteral("en0"));
  rig.system->answerRoute(other);
  QCOMPARE(rig.system->measures.last(), other);
}

void AwdlControllerTests::theBursts_comeOnceAMinute_whileTheCoreRunsAndTheLinkIsUp()
{
  Rig rig; // a minute is 80 ms in a test
  rig.start(LinkState::Up);
  rig.system->answerLink(AdminResult::Cancelled);
  rig.system->answerRead(LinkState::Up);
  QCOMPARE(rig.system->routeLookups, 1);
  rig.probe(clean());

  // right after it: not again, however often the state is read
  for (int reading = 0; reading < 3; ++reading) {
    rig.system->answerRead(LinkState::Up);
  }
  QCOMPARE(rig.system->routeLookups, 1);
  QTest::qWait(120);
  rig.system->answerRead(LinkState::Up);
  QCOMPARE(rig.system->routeLookups, 2);
  // one at a time: while it is under way there is no other
  rig.system->answerRead(LinkState::Up);
  rig.system->answerRead(LinkState::Up);
  QCOMPARE(rig.system->routeLookups, 2);
  rig.probe(clean());
  QCOMPARE(rig.system->measures.size(), 2);
}

void AwdlControllerTests::withTheOptionOff_nothingIsLookedUpOrSent_andTheStoppedCoreSendsNothing()
{
  {
    Rig rig(false);
    rig.controller->setCoreRunning(true);
    rig.system->answerRead(LinkState::Up);
    QTest::qWait(120);
    QCOMPARE(rig.system->routeLookups, 0);
    QCOMPARE(rig.system->measures.size(), 0);
  }
  {
    Rig rig;
    rig.system->answerRead(LinkState::Up); // read by someone else, the core is stopped
    QTest::qWait(120);
    QCOMPARE(rig.system->routeLookups, 0);
    QCOMPARE(rig.system->measures.size(), 0);
    QCOMPARE(rig.system->changes.size(), 0);
  }
  {
    // a link that is down is not measured
    Rig rig;
    rig.start(LinkState::Down);
    QTest::qWait(120);
    rig.system->answerRead(LinkState::Down);
    QCOMPARE(rig.system->routeLookups, 0);
    QCOMPARE(rig.system->measures.size(), 0);
  }
}

void AwdlControllerTests::measureNow_isTheButton_onWiFiOnly_whateverTheOptionAndTheCore()
{
  Rig rig(false);
  QVERIFY(!rig.controller->coreRunning());
  rig.controller->measureNow();
  QCOMPARE(rig.system->routeLookups, 1);
  QVERIFY(rig.controller->isMeasuring());
  // one at a time
  rig.controller->measureNow();
  QCOMPARE(rig.system->routeLookups, 1);
  QSignalSpy changed(rig.controller, &AwdlController::latencyChanged);
  rig.system->answerRoute(routeOf(LinkKind::WiFi));
  QCOMPARE(rig.system->measures.size(), 1);
  rig.system->answerMeasured(stutter());
  QVERIFY(!rig.controller->isMeasuring());
  QVERIFY(changed.count() >= 1);
  QCOMPARE(rig.controller->latency().pattern, Pattern::Stutter);
  QVERIFY(rig.controller->latency().worstMs > 60.0);
  // the numbers are shown, and nothing else happens: no dialog, no hint (the core does not run, the option is off)
  QVERIFY(!rig.controller->hintVisible());
  QCOMPARE(rig.system->changes.size(), 0);

  // on a wire it sends nothing
  rig.controller->measureNow();
  rig.system->answerRoute(routeOf(LinkKind::Wired));
  QCOMPARE(rig.system->measures.size(), 1);
  QVERIFY(!rig.controller->isMeasuring());
  QCOMPARE(rig.controller->summary(), Summary::Unknown); // the link was not read
}

void AwdlControllerTests::refresh_looksAtTheRouteAndSendsNothing()
{
  Rig rig(false);
  rig.controller->refresh();
  QCOMPARE(rig.system->reads, 1);
  QCOMPARE(rig.system->routeLookups, 1);
  rig.system->answerRoute(routeOf(LinkKind::Wired));
  rig.system->answerRead(LinkState::Up);
  QCOMPARE(rig.controller->route().kind, LinkKind::Wired);
  QCOMPARE(rig.controller->summary(), Summary::OnWired);
  rig.controller->refresh();
  rig.system->answerRoute(routeOf(LinkKind::WiFi));
  QCOMPARE(rig.system->measures.size(), 0); // a lookup for the kind of the connection is no burst
  QCOMPARE(rig.controller->summary(), Summary::OnNotMeasured);
}

void AwdlControllerTests::summary_saysWhatTheStatusLineSays()
{
  Rig rig(false);
  QCOMPARE(rig.controller->summary(), Summary::Unknown);
  rig.controller->refresh();
  rig.system->answerRead(LinkState::Unavailable);
  QCOMPARE(rig.controller->summary(), Summary::NotAvailable);
  rig.system->answerRead(LinkState::Down);
  QCOMPARE(rig.controller->summary(), Summary::Off);
  rig.system->answerRead(LinkState::Up);
  QCOMPARE(rig.controller->summary(), Summary::OnNotMeasured);
}

void AwdlControllerTests::aBurstThatComesAfterTheStop_saysNothing()
{
  Rig rig;
  rig.start(LinkState::Up);
  rig.system->answerLink(AdminResult::Cancelled);
  rig.system->answerRead(LinkState::Up);
  rig.system->answerRoute(routeOf(LinkKind::WiFi));
  QCOMPARE(rig.system->measures.size(), 1);
  // the core stops while the burst is under way: when it is over there is no hint
  rig.controller->setCoreRunning(false);
  rig.system->answerMeasured(stutter());
  QVERIFY(!rig.controller->hintVisible());
  QCOMPARE(rig.system->changes.size(), 1);
}

QTEST_MAIN(AwdlControllerTests)
