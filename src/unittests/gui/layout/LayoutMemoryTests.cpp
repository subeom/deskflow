/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "LayoutMemoryTests.h"

#include "FakeLayoutHost.h"
#include "LayoutTestData.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>

#include <algorithm>
#include <random>

using namespace layout_test;
using namespace deskflow::gui::layout;

namespace {

DisplaySignature situationT()
{
  return sig(tShape());
}
DisplaySignature situationLaptop()
{
  return sig(laptopOnly());
}
std::vector<DisplayInfo> twoMonitors()
{
  return {
      fullHd(0, 0, true, QStringLiteral("00000000-0000-4000-8000-0000000000D1")),
      fullHd(1920, 0, false, QStringLiteral("00000000-0000-4000-8000-0000000000D2"))
  };
}
// the same rectangles as the T, but another monitor of the same kind in the place of the ultrawide
std::vector<DisplayInfo> tWithAnotherMonitor()
{
  return {otherUltrawide(0, 0, true), builtIn(964, 1440, false)};
}

bool anyLine(const QStringList &lines, const QString &pattern)
{
  const QRegularExpression expression(pattern);
  return std::ranges::any_of(lines, [&](const QString &line) { return expression.match(line).hasMatch(); });
}

struct Rig
{
  FakeHost host;
  LayoutProfileStore store;
  LayoutMemory memory;
  Millis now = 0;

  explicit Rig(const QString &path) : store(path), memory(host, store)
  {
  }

  void at(Millis time)
  {
    now = time;
    host.clockNow = time;
  }

  // lets the time go to \p to, calling tick at every moment that the memory asked for
  void run(Millis to)
  {
    for (int i = 0; i < 5000; ++i) {
      const auto deadline = memory.nextDeadline();
      if (!deadline || *deadline > to)
        break;
      at(std::max(*deadline, now));
      memory.tick(now);
    }
    at(std::max(to, now));
  }

  // an event at \p time and the time going to \p time
  void event(Millis time)
  {
    run(time);
    memory.displaysEvent(time);
  }

  // the T and the laptop alone are remembered, and the arrangement belongs to the T
  void seed()
  {
    store.upsert(situationT(), layoutForT(), 100);
    store.upsert(situationLaptop(), layoutForLaptop(), 200);
    store.setActive(situationT());
    QVERIFY(store.save());
  }
};

} // namespace

void LayoutMemoryTests::init()
{
  QVERIFY(m_dir.isValid());
  QDir dir(m_dir.path());
  for (const auto &name : dir.entryList(QDir::Files | QDir::Hidden))
    dir.remove(name);
}

void LayoutMemoryTests::off_doesNothingAtAll()
{
  Rig rig(path());
  rig.host.isEnabled = false;
  rig.memory.start(0);
  rig.host.shownDisplays = laptopOnly();
  rig.memory.displaysEvent(10);
  rig.memory.poll(20);
  rig.run(100000);
  rig.memory.configSaved(100000);
  QVERIFY(!rig.memory.undo(100000));
  rig.memory.stop();

  QCOMPARE(rig.memory.state(), LayoutMemory::State::Disabled);
  QVERIFY(!rig.memory.nextDeadline().has_value());
  QCOMPARE(rig.host.displayReads, 0);
  QCOMPARE(rig.host.applyCalls, 0);
  QCOMPARE(rig.host.restartCalls, 0);
  QCOMPARE(rig.host.reattachCalls, 0);
  QVERIFY(rig.host.infoLines.isEmpty());
  QVERIFY(rig.host.warnLines.isEmpty());
  QVERIFY(rig.host.notices.empty());
  QVERIFY(rig.store.profiles().empty());
  QVERIFY(!QFile::exists(path()));
}

void LayoutMemoryTests::start_theFirstTime_remembersTheArrangement()
{
  Rig rig(path());
  rig.memory.start(0);

  QCOMPARE(rig.memory.state(), LayoutMemory::State::Idle);
  QCOMPARE(rig.store.profiles().size(), size_t(1));
  const auto *profile = rig.store.findExact(situationT());
  QVERIFY(profile != nullptr);
  QVERIFY(profile->layout.sameAs(layoutForT()));
  QVERIFY(rig.store.active().has_value());
  QCOMPARE(rig.store.active()->key(), situationT().key());
  QVERIFY(QFile::exists(path()));
  // nothing was changed and nobody is told
  QCOMPARE(rig.host.applyCalls, 0);
  QCOMPARE(rig.host.restartCalls, 0);
  QCOMPARE(rig.host.reattachCalls, 0);
  QVERIFY(rig.host.notices.empty());
}

void LayoutMemoryTests::start_withTheSameDisplays_doesNothing()
{
  Rig rig(path());
  rig.seed();
  const auto before = rig.store.profiles().size();
  rig.memory.start(0);
  QCOMPARE(rig.host.applyCalls, 0);
  QCOMPARE(rig.host.restartCalls, 0);
  QVERIFY(rig.host.notices.empty());
  QCOMPARE(rig.store.profiles().size(), before);
  QCOMPARE(rig.memory.state(), LayoutMemory::State::Idle);
}

void LayoutMemoryTests::start_afterTheDisplaysChangedWhileClosed_putsTheLayoutBack()
{
  Rig rig(path());
  rig.seed();
  rig.host.shownDisplays = laptopOnly();
  // the program has just started: the core is not running yet, and it is not restarted
  rig.host.running = false;
  rig.memory.start(0);

  QVERIFY(rig.host.layout.sameAs(layoutForLaptop()));
  QCOMPARE(rig.host.applyCalls, 1);
  QCOMPARE(rig.host.restartCalls, 0);
  QCOMPARE(rig.host.notices.size(), size_t(1));
  QCOMPARE(rig.host.notices[0].kind, Notice::Kind::Restored);
  QVERIFY(rig.host.notices[0].exact);
}

void LayoutMemoryTests::start_withoutAKnownSituation_adoptsTheArrangement_andNeverRestores()
{
  Rig rig(path());
  // a layout of the T is remembered, but nothing says what the arrangement belongs to (the feature was off for a while):
  // the arrangement that there is, is the one that the user has now, and it is what is remembered
  rig.store.upsert(situationT(), layoutForTwoMonitors(), 100);
  QVERIFY(rig.store.save());
  rig.host.layout = layoutForT();
  rig.memory.start(0);

  QCOMPARE(rig.host.applyCalls, 0);
  QCOMPARE(rig.host.restartCalls, 0);
  QVERIFY(rig.host.layout.sameAs(layoutForT()));
  QVERIFY(rig.host.notices.empty());
  QVERIFY(rig.store.findExact(situationT())->layout.sameAs(layoutForT()));
  QVERIFY(rig.store.active().has_value());
  QCOMPARE(rig.store.active()->key(), situationT().key());
}

void LayoutMemoryTests::configSaved_afterAFailedApply_remembersNothing_untilTheUserChangesTheArrangement()
{
  Rig rig(path());
  rig.seed();
  rig.memory.start(0);
  rig.host.applyFails = true;
  rig.host.shownDisplays = laptopOnly();
  rig.event(0);
  rig.run(kQuietMs);
  QCOMPARE(rig.host.notices.back().kind, Notice::Kind::Failed);
  rig.host.applyFails = false;
  const auto remembered = rig.store.findExact(situationLaptop())->layout;

  // a Save of something else: the arrangement is the one of the T, not what is remembered for the laptop alone
  rig.memory.configSaved(kQuietMs + 10);
  QVERIFY(rig.store.findExact(situationLaptop())->layout.sameAs(remembered));

  // the user changes the arrangement: that is remembered for the displays that there are
  rig.host.layout.computers[1].x -= 50;
  const auto edited = rig.host.layout;
  rig.memory.configSaved(kQuietMs + 20);
  QVERIFY(rig.store.findExact(situationLaptop())->layout.sameAs(edited));
}

void LayoutMemoryTests::configSaved_afterUndo_remembersNothingOfAnUnchangedArrangement()
{
  Rig rig(path());
  rig.seed();
  rig.memory.start(0);
  rig.host.shownDisplays = laptopOnly();
  rig.event(0);
  rig.run(kQuietMs);
  rig.run(60000);
  QVERIFY(rig.memory.undo(60000));
  QVERIFY(rig.host.layout.sameAs(layoutForT()));

  // a Save of something else does not make the arrangement of the T the layout of the laptop alone
  rig.memory.configSaved(61000);
  QVERIFY(rig.store.findExact(situationLaptop())->layout.sameAs(layoutForLaptop()));
}

void LayoutMemoryTests::aBurstOfEvents_isOneLook()
{
  Rig rig(path());
  rig.seed();
  rig.memory.start(0);
  const int reads = rig.host.displayReads;

  // plug, sleep and wake: the displays are reconfigured many times in a row, and end as the laptop alone
  rig.host.shownDisplays = laptopOnly();
  Millis last = 0;
  for (int i = 0; i < 10; ++i) {
    last = 1000 + i * 200;
    rig.event(last);
    QCOMPARE(rig.memory.state(), LayoutMemory::State::Settling);
  }
  rig.run(last + kQuietMs - 1);
  QCOMPARE(rig.host.displayReads, reads);
  QCOMPARE(rig.host.applyCalls, 0);

  rig.run(last + kQuietMs);
  QCOMPARE(rig.host.displayReads, reads + 1);
  QCOMPARE(rig.host.applyCalls, 1);
  QCOMPARE(rig.host.restartCalls, 1);
  QCOMPARE(rig.memory.state(), LayoutMemory::State::Idle);
}

void LayoutMemoryTests::anEndlessBurst_isLookedAtAfterTheMaximumWait()
{
  Rig rig(path());
  rig.seed();
  rig.memory.start(0);
  rig.host.shownDisplays = laptopOnly();

  for (Millis t = 0; t < kMaxWaitMs; t += 500) {
    rig.event(t);
    QCOMPARE(rig.host.applyCalls, 0);
  }
  // the events keep coming: the look is at the maximum wait after the first one
  rig.event(kMaxWaitMs);
  rig.run(kMaxWaitMs);
  QCOMPARE(rig.host.applyCalls, 1);
  QVERIFY(rig.host.layout.sameAs(layoutForLaptop()));
}

void LayoutMemoryTests::anEventThatChangesNothing_isIgnored()
{
  Rig rig(path());
  rig.seed();
  rig.memory.start(0);
  rig.event(1000);
  rig.event(1200);
  rig.run(10000);

  QCOMPARE(rig.host.applyCalls, 0);
  QCOMPARE(rig.host.restartCalls, 0);
  QVERIFY(rig.host.notices.empty());
  QCOMPARE(rig.memory.state(), LayoutMemory::State::Idle);
  QVERIFY(anyLine(rig.host.debugLines, QStringLiteral("signature unchanged")));
  QVERIFY(!anyLine(rig.host.infoLines, QStringLiteral("displays changed")));
}

void LayoutMemoryTests::exactMatch_putsTheLayoutBackAndRestarts()
{
  Rig rig(path());
  rig.seed();
  rig.host.wallClock = 5'000'000;
  rig.memory.start(0);
  rig.host.shownDisplays = laptopOnly();
  rig.event(1000);
  rig.run(1000 + kQuietMs);

  QVERIFY(rig.host.layout.sameAs(layoutForLaptop()));
  QCOMPARE(rig.host.applyCalls, 1);
  QCOMPARE(rig.host.restartCalls, 1);
  QCOMPARE(rig.host.notices.size(), size_t(1));
  const auto &notice = rig.host.notices[0];
  QCOMPARE(notice.kind, Notice::Kind::Restored);
  QVERIFY(notice.exact);
  QVERIFY(notice.restarted);
  QVERIFY(notice.canUndo);
  QCOMPARE(notice.oldId, situationT().id());
  QCOMPARE(notice.newId, situationLaptop().id());
  QCOMPARE(notice.profileName, rig.store.findExact(situationLaptop())->name);
  QVERIFY(rig.memory.canUndo());

  // the profile was used, and the new situation is what the arrangement belongs to, on disk as well
  QCOMPARE(rig.store.findExact(situationLaptop())->lastUsedAt, int64_t(5'000'000));
  LayoutProfileStore again(path());
  again.load();
  QVERIFY(again.active().has_value());
  QCOMPARE(again.active()->key(), situationLaptop().key());
  // and nothing was made
  QCOMPARE(rig.store.profiles().size(), size_t(2));
  QCOMPARE(rig.host.reattachCalls, 0);
}

void LayoutMemoryTests::similarMatch_putsTheLayoutBack_andCopiesItOnTheFirstEdit()
{
  Rig rig(path());
  rig.seed();
  rig.memory.start(0);
  // the layout that the T has is edited by the user in the meantime: it differs from the remembered one
  rig.host.layout = layoutForTwoMonitors();
  rig.host.shownDisplays = tWithAnotherMonitor();
  rig.event(1000);
  rig.run(1000 + kQuietMs);

  QVERIFY(rig.host.layout.sameAs(layoutForT()));
  QCOMPARE(rig.host.notices.size(), size_t(1));
  const auto &notice = rig.host.notices[0];
  QCOMPARE(notice.kind, Notice::Kind::Restored);
  QVERIFY(!notice.exact);
  QVERIFY(qFuzzyCompare(notice.score, (kCreditStandIn + kCreditSameDisplay) / 2.0));
  // no profile was made for a situation that only looks like another one
  QCOMPARE(rig.store.profiles().size(), size_t(2));
  // the rectangles of the displays are the same, and the layout is the one that the core runs with
  QCOMPARE(rig.host.restartCalls, 0);

  // the first edit in this situation makes a profile of its own, and leaves the one that was used as it was
  const auto rememberedT = rig.store.findExact(situationT())->layout;
  rig.host.layout.computers[2].x += 100;
  const auto edited = rig.host.layout;
  rig.memory.configSaved(5000);
  QCOMPARE(rig.store.profiles().size(), size_t(3));
  const auto *own = rig.store.findExact(sig(tWithAnotherMonitor()));
  QVERIFY(own != nullptr);
  QVERIFY(own->layout.sameAs(edited));
  QVERIFY(rig.store.findExact(situationT())->layout.sameAs(rememberedT));
}

void LayoutMemoryTests::noMatch_keepsTheArrangement_putsComputersAgainstTheServer_andRemembers()
{
  Rig rig(path());
  rig.store.upsert(situationT(), layoutForT(), 100);
  rig.store.setActive(situationT());
  rig.host.reattachLayout = layoutForLaptop();
  rig.host.reattachedNames = {"PC2", "MINI"};
  rig.host.notReattachedNames = {};
  rig.memory.start(0);
  rig.host.shownDisplays = laptopOnly();
  rig.event(1000);
  rig.run(1000 + kQuietMs);

  QCOMPARE(rig.host.reattachCalls, 1);
  QVERIFY(rig.host.layout.sameAs(layoutForLaptop()));
  // a profile for the laptop alone was made of what there is now
  const auto *made = rig.store.findExact(situationLaptop());
  QVERIFY(made != nullptr);
  QVERIFY(made->layout.sameAs(layoutForLaptop()));
  QCOMPARE(rig.store.profiles().size(), size_t(2));
  QCOMPARE(rig.host.restartCalls, 1);

  QCOMPARE(rig.host.notices.size(), size_t(1));
  const auto &notice = rig.host.notices[0];
  QCOMPARE(notice.kind, Notice::Kind::Remembered);
  QCOMPARE(notice.reattached, (QStringList{"PC2", "MINI"}));
  QCOMPARE(notice.profileName, made->name);
  QVERIFY(notice.canUndo);
}

void LayoutMemoryTests::noMatch_withoutAnOldSituation_justRemembers()
{
  Rig rig(path());
  rig.host.shownDisplays = laptopOnly();
  rig.host.layout = layoutForLaptop();
  rig.memory.start(0);
  QCOMPARE(rig.host.reattachCalls, 0);
  QVERIFY(rig.store.findExact(situationLaptop()) != nullptr);
  QVERIFY(rig.host.notices.empty());
}

void LayoutMemoryTests::noRestart_whenTheConfigurationDoesNotChange()
{
  Rig rig(path());
  rig.seed();
  rig.memory.start(0);
  // another monitor of the same size: the same rectangles, so the core would be given the same configuration
  rig.host.shownDisplays = tWithAnotherMonitor();
  rig.event(1000);
  rig.run(1000 + kQuietMs);

  QCOMPARE(rig.host.notices.size(), size_t(1));
  QVERIFY(!rig.host.coreConfigDiffers());
  QCOMPARE(rig.host.restartCalls, 0);
  QVERIFY(!rig.host.notices[0].restarted);
  QVERIFY(anyLine(rig.host.infoLines, QStringLiteral("restart=no$")));
}

void LayoutMemoryTests::noRestart_whenTheCoreDoesNotRun()
{
  Rig rig(path());
  rig.seed();
  rig.host.running = false;
  rig.memory.start(0);
  rig.host.shownDisplays = laptopOnly();
  rig.event(1000);
  rig.run(1000 + kQuietMs);

  QCOMPARE(rig.host.applyCalls, 1);
  QCOMPARE(rig.host.restartCalls, 0);
  QVERIFY(anyLine(rig.host.infoLines, QStringLiteral("restart=no$")));
}

void LayoutMemoryTests::restarts_areTheMinimumGapApart_andNeverLoop()
{
  Rig rig(path());
  rig.seed();
  rig.memory.start(0);

  rig.host.shownDisplays = laptopOnly();
  rig.event(0);
  rig.run(kQuietMs);
  QCOMPARE(rig.host.restartCalls, 1);

  // the displays are the T again 3 s later: the layout is put back at once, the restart waits
  rig.host.shownDisplays = tShape();
  rig.event(3000);
  rig.run(3000 + kQuietMs);
  QVERIFY(rig.host.layout.sameAs(layoutForT()));
  QCOMPARE(rig.host.restartCalls, 1);
  QVERIFY(rig.host.notices.back().restartLater);
  QVERIFY(!rig.host.notices.back().restarted);
  QVERIFY(rig.memory.nextDeadline().has_value());
  QVERIFY(*rig.memory.nextDeadline() <= kQuietMs + kMinRestartGapMs);

  rig.run(kQuietMs + kMinRestartGapMs - 1);
  QCOMPARE(rig.host.restartCalls, 1);
  rig.run(kQuietMs + kMinRestartGapMs);
  QCOMPARE(rig.host.restartCalls, 2);
  QVERIFY(rig.host.restartTimes[1] - rig.host.restartTimes[0] >= kMinRestartGapMs);

  // and then nothing happens, however long it is: a restart does not cause another
  const int reads = rig.host.displayReads;
  rig.run(10 * 60 * 1000);
  QCOMPARE(rig.host.restartCalls, 2);
  QCOMPARE(rig.host.displayReads, reads);
  QVERIFY(!rig.memory.nextDeadline().has_value());
}

void LayoutMemoryTests::aRestartThatIsNotNeededAnyMore_isNotDone()
{
  Rig rig(path());
  rig.seed();
  rig.memory.start(0);
  rig.host.shownDisplays = laptopOnly();
  rig.event(0);
  rig.run(kQuietMs);
  QCOMPARE(rig.host.restartCalls, 1);

  rig.host.shownDisplays = tShape();
  rig.event(3000);
  rig.run(3000 + kQuietMs);
  // the restart waits ...
  QCOMPARE(rig.host.restartCalls, 1);
  // ... and the displays are the laptop alone again, which is what the core runs with
  rig.host.shownDisplays = laptopOnly();
  rig.event(6000);
  rig.run(6000 + kQuietMs);
  QVERIFY(!rig.host.coreConfigDiffers());
  rig.run(60000);
  QCOMPARE(rig.host.restartCalls, 1);
}

void LayoutMemoryTests::undo_putsTheArrangementBack_andRestarts()
{
  Rig rig(path());
  rig.seed();
  rig.memory.start(0);
  rig.host.shownDisplays = laptopOnly();
  rig.event(0);
  rig.run(kQuietMs);
  QVERIFY(rig.host.layout.sameAs(layoutForLaptop()));
  QCOMPARE(rig.host.restartCalls, 1);

  rig.run(60000);
  QVERIFY(rig.memory.undo(60000));
  QVERIFY(rig.host.layout.sameAs(layoutForT()));
  QCOMPARE(rig.host.restartCalls, 2);
  QCOMPARE(rig.host.notices.back().kind, Notice::Kind::Undone);
  QVERIFY(rig.host.notices.back().restarted);
  QVERIFY(!rig.memory.canUndo());
  QVERIFY(!rig.memory.undo(60000));
  QVERIFY(anyLine(rig.host.infoLines, QStringLiteral("^layout memory: undo: ")));
  // the situation that the arrangement belongs to is still the laptop alone: nothing looks again
  const int applies = rig.host.applyCalls;
  rig.memory.poll(70000);
  rig.run(80000);
  QCOMPARE(rig.host.applyCalls, applies);
}

void LayoutMemoryTests::undo_ofAMadeProfile_removesIt()
{
  Rig rig(path());
  rig.store.upsert(situationT(), layoutForT(), 100);
  rig.store.setActive(situationT());
  rig.host.reattachLayout = layoutForLaptop();
  rig.host.reattachedNames = {"PC2", "MINI"};
  rig.memory.start(0);
  rig.host.shownDisplays = laptopOnly();
  rig.event(0);
  rig.run(kQuietMs);
  QVERIFY(rig.store.findExact(situationLaptop()) != nullptr);

  rig.run(60000);
  QVERIFY(rig.memory.undo(60000));
  QVERIFY(rig.host.layout.sameAs(layoutForT()));
  QVERIFY(rig.store.findExact(situationLaptop()) == nullptr);
  QVERIFY(rig.store.findExact(situationT()) != nullptr);
}

void LayoutMemoryTests::undo_isGoneWhenTheUserSaves_andWhenThereIsNothingToUndo()
{
  Rig rig(path());
  rig.seed();
  rig.memory.start(0);
  QVERIFY(!rig.memory.undo(0));

  rig.host.shownDisplays = laptopOnly();
  rig.event(0);
  rig.run(kQuietMs);
  QVERIFY(rig.memory.canUndo());
  // what the user did since is not to be thrown away
  rig.memory.configSaved(kQuietMs + 10);
  QVERIFY(!rig.memory.canUndo());
  QVERIFY(!rig.memory.undo(kQuietMs + 20));
  QVERIFY(rig.host.layout.sameAs(layoutForLaptop()));
}

void LayoutMemoryTests::undo_whenNothingChanged_isNotOffered()
{
  Rig rig(path());
  rig.seed();
  rig.memory.start(0);
  rig.host.shownDisplays = tWithAnotherMonitor();
  rig.event(0);
  rig.run(kQuietMs);
  QCOMPARE(rig.host.notices.size(), size_t(1));
  QVERIFY(!rig.host.notices[0].canUndo);
  QVERIFY(!rig.memory.canUndo());
}

void LayoutMemoryTests::aDialogThatIsOpen_defersTheChange_andItIsMadeAfter()
{
  Rig rig(path());
  rig.seed();
  rig.memory.start(0);
  rig.host.canApply = false;
  rig.host.shownDisplays = laptopOnly();
  rig.event(0);
  rig.run(kQuietMs + 5000);

  QCOMPARE(rig.host.applyCalls, 0);
  QCOMPARE(rig.memory.state(), LayoutMemory::State::Deferred);
  QVERIFY(rig.memory.nextDeadline().has_value());
  QVERIFY(rig.host.layout.sameAs(layoutForT()));

  rig.host.canApply = true;
  rig.run(kQuietMs + 5000 + kRetryMs);
  QCOMPARE(rig.host.applyCalls, 1);
  QVERIFY(rig.host.layout.sameAs(layoutForLaptop()));
  QCOMPARE(rig.memory.state(), LayoutMemory::State::Idle);
}

void LayoutMemoryTests::displaysThatCannotBeRead_areTriedAgain_thenGivenUp()
{
  Rig rig(path());
  rig.seed();
  rig.memory.start(0);
  const int reads = rig.host.displayReads;
  rig.host.displaysUnreadable = true;
  rig.event(0);
  rig.run(60000);

  QCOMPARE(rig.host.displayReads - reads, kMaxReadRetries);
  QCOMPARE(rig.host.warnLines.size(), 1);
  QCOMPARE(rig.host.applyCalls, 0);
  QVERIFY(rig.host.layout.sameAs(layoutForT()));
  QCOMPARE(rig.memory.state(), LayoutMemory::State::Idle);

  // when they can be read again, the next event is a look
  rig.host.displaysUnreadable = false;
  rig.host.shownDisplays = laptopOnly();
  rig.event(70000);
  rig.run(70000 + kQuietMs);
  QCOMPARE(rig.host.applyCalls, 1);
}

void LayoutMemoryTests::aLayoutThatCannotBeApplied_leavesTheArrangementAndDoesNotRetry()
{
  Rig rig(path());
  rig.seed();
  rig.memory.start(0);
  rig.host.applyFails = true;
  rig.host.shownDisplays = laptopOnly();
  rig.event(0);
  rig.run(kQuietMs);

  QCOMPARE(rig.host.applyCalls, 1);
  QVERIFY(rig.host.layout.sameAs(layoutForT()));
  QCOMPARE(rig.host.restartCalls, 0);
  QCOMPARE(rig.host.warnLines.size(), 1);
  QCOMPARE(rig.host.notices.size(), size_t(1));
  QCOMPARE(rig.host.notices[0].kind, Notice::Kind::Failed);
  QVERIFY(!rig.memory.canUndo());

  // it is not tried again until the displays change once more
  for (Millis t = 5000; t < 60000; t += 3000)
    rig.memory.poll(t);
  rig.run(60000);
  QCOMPARE(rig.host.applyCalls, 1);
}

void LayoutMemoryTests::poll_seesAChangeThatNoEventToldOf()
{
  Rig rig(path());
  rig.seed();
  rig.memory.start(0);
  rig.memory.poll(1000);
  QCOMPARE(rig.memory.state(), LayoutMemory::State::Idle);
  QCOMPARE(rig.host.applyCalls, 0);

  // another displays list that is empty is nothing
  rig.host.displaysUnreadable = true;
  rig.memory.poll(2000);
  QCOMPARE(rig.memory.state(), LayoutMemory::State::Idle);
  rig.host.displaysUnreadable = false;

  rig.host.shownDisplays = laptopOnly();
  rig.memory.poll(3000);
  QCOMPARE(rig.memory.state(), LayoutMemory::State::Settling);
  rig.run(3000 + kQuietMs);
  QCOMPARE(rig.host.applyCalls, 1);
  QVERIFY(rig.host.layout.sameAs(layoutForLaptop()));
}

void LayoutMemoryTests::configSaved_isRememberedForTheSituationThatWasActive()
{
  Rig rig(path());
  rig.seed();
  rig.memory.start(0);

  // the user opened the dialog with the T, and the monitor was unplugged while it was open
  rig.host.shownDisplays = laptopOnly();
  rig.event(1000);
  auto edited = layoutForT();
  edited.computers[1].x -= 50;
  rig.host.layout = edited;
  rig.memory.configSaved(1100);

  // the edit was made for the T, and that is where it is remembered
  QVERIFY(rig.store.findExact(situationT())->layout.sameAs(edited));
  QVERIFY(rig.store.findExact(situationLaptop())->layout.sameAs(layoutForLaptop()));
  // and the look at the new displays still happens
  rig.run(1000 + kQuietMs + 100);
  QVERIFY(rig.host.layout.sameAs(layoutForLaptop()));
}

void LayoutMemoryTests::configSaved_changesOnlyWhatChanged()
{
  Rig rig(path());
  rig.seed();
  rig.memory.start(0);
  const auto updated = rig.store.findExact(situationT())->updatedAt;
  rig.host.wallClock = 9'000'000;
  rig.memory.configSaved(100);
  QCOMPARE(rig.store.findExact(situationT())->updatedAt, updated);
  QCOMPARE(rig.store.profiles().size(), size_t(2));

  rig.host.layout.computers[1].x += 10;
  rig.memory.configSaved(200);
  QCOMPARE(rig.store.findExact(situationT())->updatedAt, int64_t(9'000'000));
  QCOMPARE(rig.store.profiles().size(), size_t(2));
  QVERIFY(rig.host.notices.empty());
}

void LayoutMemoryTests::saveCurrentAs_namesTheProfileOfTheSituation()
{
  Rig rig(path());
  rig.seed();
  rig.memory.start(0);
  const auto id = rig.memory.saveCurrentAs(QStringLiteral("  At the desk "), layoutForTwoMonitors(), 10);
  QVERIFY(!id.isEmpty());
  const auto *profile = rig.store.find(id);
  QVERIFY(profile != nullptr);
  QCOMPARE(profile->name, QStringLiteral("At the desk"));
  QVERIFY(profile->userNamed);
  QCOMPARE(profile->signature.key(), situationT().key());
  QVERIFY(profile->layout.sameAs(layoutForTwoMonitors()));
  QCOMPARE(rig.store.profiles().size(), size_t(2));

  LayoutProfileStore again(path());
  again.load();
  QCOMPARE(again.find(id)->name, QStringLiteral("At the desk"));
  // an empty name is nothing
  QVERIFY(rig.memory.saveCurrentAs(QStringLiteral("   "), layoutForT(), 20).isEmpty());
}

void LayoutMemoryTests::stop_forgetsEverything()
{
  Rig rig(path());
  rig.seed();
  rig.memory.start(0);
  rig.host.shownDisplays = laptopOnly();
  rig.event(0);
  rig.run(kQuietMs);
  QVERIFY(rig.memory.canUndo());
  rig.host.shownDisplays = tShape();
  rig.event(20000);
  QCOMPARE(rig.memory.state(), LayoutMemory::State::Settling);

  rig.host.isEnabled = false;
  rig.memory.stop();
  QCOMPARE(rig.memory.state(), LayoutMemory::State::Disabled);
  QVERIFY(!rig.memory.canUndo());
  QVERIFY(!rig.memory.nextDeadline().has_value());
  const int applies = rig.host.applyCalls;
  rig.run(100000);
  QCOMPARE(rig.host.applyCalls, applies);
}

void LayoutMemoryTests::aStoreOfANewerVersion_switchesItOffWithAWarning()
{
  {
    QFile file(path());
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"format":"deskflow-layout-profiles","version":9,"profiles":[]})");
  }
  Rig rig(path());
  QCOMPARE(rig.store.load().status, LayoutProfileStore::Status::NewerVersion);
  rig.memory.start(0);
  QCOMPARE(rig.memory.state(), LayoutMemory::State::Disabled);
  QCOMPARE(rig.host.warnLines.size(), 1);
  QVERIFY(rig.host.warnLines[0].contains(QStringLiteral("newer")));
  QCOMPARE(rig.host.notices.size(), size_t(1));
  QCOMPARE(rig.host.notices[0].kind, Notice::Kind::StoreProblem);

  rig.host.shownDisplays = laptopOnly();
  rig.event(10);
  rig.run(100000);
  rig.memory.configSaved(100000);
  QCOMPARE(rig.host.applyCalls, 0);
  QCOMPARE(rig.host.restartCalls, 0);
  QVERIFY(rig.store.profiles().empty());
}

void LayoutMemoryTests::whatIsRemembered_survivesARestartOfTheProgram()
{
  {
    Rig rig(path());
    rig.seed();
    rig.memory.start(0);
    rig.host.shownDisplays = laptopOnly();
    rig.event(0);
    rig.run(kQuietMs);
    QVERIFY(rig.host.layout.sameAs(layoutForLaptop()));
  }
  // the program is started again, with the displays as they were left
  Rig second(path());
  QCOMPARE(second.store.load().status, LayoutProfileStore::Status::Ok);
  second.host.shownDisplays = laptopOnly();
  second.host.layout = layoutForLaptop();
  second.host.runningKey = second.host.currentKey();
  second.memory.start(0);
  QCOMPARE(second.host.applyCalls, 0);
  QVERIFY(second.host.notices.empty());

  // and with the T again: the T is remembered
  second.host.shownDisplays = tShape();
  second.event(1000);
  second.run(1000 + kQuietMs);
  QVERIFY(second.host.layout.sameAs(layoutForT()));
}

void LayoutMemoryTests::theTimesAreTheOnesOfTheHost()
{
  Rig rig(path());
  rig.host.wallClock = 7'000'000;
  rig.memory.start(0);
  const auto *profile = rig.store.findExact(situationT());
  QVERIFY(profile != nullptr);
  QCOMPARE(profile->createdAt, int64_t(7'000'000));
  QCOMPARE(profile->lastUsedAt, int64_t(7'000'000));

  rig.host.wallClock = 8'000'000;
  rig.host.shownDisplays = laptopOnly();
  rig.host.reattachLayout = layoutForLaptop();
  rig.event(0);
  rig.run(kQuietMs);
  QCOMPARE(rig.store.findExact(situationLaptop())->createdAt, int64_t(8'000'000));
}

void LayoutMemoryTests::theLogLinesHaveTheDocumentedFormat()
{
  Rig rig(path());
  rig.seed();
  rig.host.reattachLayout = layoutForTwoMonitors();
  rig.host.reattachedNames = {"PC1", "PC2"};
  rig.memory.start(0);
  // exact, similar, none, undo
  rig.host.shownDisplays = laptopOnly();
  rig.event(0);
  rig.run(kQuietMs);
  rig.host.shownDisplays = tWithAnotherMonitor();
  rig.event(20000);
  rig.run(20000 + kQuietMs);
  rig.host.shownDisplays = twoMonitors();
  rig.event(40000);
  rig.run(40000 + kQuietMs);
  rig.run(80000);
  QVERIFY(rig.memory.undo(80000));

  const QString id = QStringLiteral("[0-9a-f]{8}");
  const QString restart = QStringLiteral("restart=(yes|no|later)$");
  const auto &lines = rig.host.infoLines;
  QVERIFY2(
      anyLine(lines, QStringLiteral("^layout memory: on, \\d+ remembered layouts?, displays=%1 \\(\\d+ displays?: .+\\)$").arg(id)),
      qPrintable(lines.join('\n'))
  );
  QVERIFY2(
      anyLine(
          lines,
          QStringLiteral("^layout memory: displays changed old=(%1|none) \\(\\d+ displays?\\) new=%1 \\(\\d+ displays?: .+\\)$")
              .arg(id)
      ),
      qPrintable(lines.join('\n'))
  );
  QVERIFY2(
      anyLine(lines, QStringLiteral("^layout memory: restored \".+\" \\(exact\\) old=%1 new=%1 %2").arg(id, restart)),
      qPrintable(lines.join('\n'))
  );
  QVERIFY2(
      anyLine(
          lines, QStringLiteral("^layout memory: restored \".+\" \\(similar, score \\d\\.\\d{3}\\) old=%1 new=%1 %2").arg(id, restart)
      ),
      qPrintable(lines.join('\n'))
  );
  QVERIFY2(
      anyLine(
          lines,
          QStringLiteral("^layout memory: no remembered layout for new=%1; kept the arrangement as \".+\" \\(re-attached: "
                         "PC1, PC2; left alone: none\\) %2")
              .arg(id, restart)
      ),
      qPrintable(lines.join('\n'))
  );
  QVERIFY2(
      anyLine(lines, QStringLiteral("^layout memory: undo: back to the arrangement before old=%1 new=%1 %2").arg(id, restart)),
      qPrintable(lines.join('\n'))
  );
  // nothing is logged that is not of the feature, and no line is a serial number or an id of a display
  for (const auto &line : lines)
    QVERIFY2(line.startsWith(QStringLiteral("layout memory: ")), qPrintable(line));
}

void LayoutMemoryTests::property_anySequenceOfEvents_keepsTheRules()
{
  const std::vector<std::vector<DisplayInfo>> situations{tShape(), laptopOnly(), twoMonitors(), tWithAnotherMonitor()};
  for (int seed = 1; seed <= 12; ++seed) {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Rig rig(dir.filePath(QStringLiteral("p.json")));
    rig.seed();
    rig.host.reattachLayout = layoutForTwoMonitors();
    rig.memory.start(0);

    std::mt19937 random(static_cast<unsigned>(seed) * 7919u);
    Millis time = 0;
    for (int step = 0; step < 400; ++step) {
      time += Millis(random() % 4000);
      rig.run(time);
      switch (random() % 10) {
      case 0:
      case 1:
        rig.host.shownDisplays = situations[random() % situations.size()];
        rig.memory.displaysEvent(time);
        break;
      case 2:
        rig.memory.displaysEvent(time);
        break;
      case 3:
        rig.memory.poll(time);
        break;
      case 4:
        rig.host.layout.computers[1 + random() % 3].x += 10;
        rig.memory.configSaved(time);
        // the dialog that saved it restarts the core with it
        rig.host.runningKey = rig.host.currentKey();
        break;
      case 5:
        rig.memory.undo(time);
        break;
      case 6:
        rig.host.canApply = !rig.host.canApply;
        break;
      case 7:
        rig.host.running = !rig.host.running;
        // a core that is started gets the configuration of now
        if (rig.host.running)
          rig.host.runningKey = rig.host.currentKey();
        break;
      case 8:
        rig.host.displaysUnreadable = !rig.host.displaysUnreadable;
        break;
      default:
        break;
      }
      QVERIFY(rig.store.profiles().size() <= size_t(LayoutProfileStore::kMaxProfiles));
      QVERIFY(!rig.host.appliedWhileBlocked);
    }

    // the restarts are the minimum gap apart
    for (size_t i = 1; i < rig.host.restartTimes.size(); ++i)
      QVERIFY2(rig.host.restartTimes[i] - rig.host.restartTimes[i - 1] >= kMinRestartGapMs, "restart gap");

    // when everything is quiet and can be read, the arrangement belongs to the displays that there are, and the core
    // has the configuration that it would get
    rig.host.canApply = true;
    rig.host.displaysUnreadable = false;
    rig.host.running = true;
    rig.memory.displaysEvent(time);
    time += 10 * kMaxWaitMs;
    rig.run(time);
    // the core is in step with what was settled; then the displays change once more, to another situation
    rig.host.runningKey = rig.host.currentKey();
    const auto settled = rig.store.active();
    QVERIFY(settled.has_value());
    for (const auto &situation : situations) {
      if (DisplaySignature::fromDisplays(situation).key() != settled->key()) {
        rig.host.shownDisplays = situation;
        break;
      }
    }
    rig.memory.displaysEvent(time);
    rig.run(time + 10 * kMaxWaitMs);
    QCOMPARE(rig.memory.state(), LayoutMemory::State::Idle);
    QVERIFY(rig.store.active().has_value());
    QVERIFY(rig.store.active()->key() != settled->key());
    QCOMPARE(rig.store.active()->key(), DisplaySignature::fromDisplays(rig.host.shownDisplays).key());
    QVERIFY(!rig.host.coreConfigDiffers());
    QVERIFY(!rig.memory.nextDeadline().has_value());
  }
}

QTEST_MAIN(LayoutMemoryTests)
