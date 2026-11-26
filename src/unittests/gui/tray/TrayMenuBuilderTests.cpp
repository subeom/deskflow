/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TrayMenuBuilderTests.h"

#include "TrayTestData.h"

#include "gui/tray/TrayHeaderWidget.h"
#include "gui/tray/TrayMenuBuilder.h"

#include <QAccessible>
#include <QAction>
#include <QMenu>
#include <QPointer>
#include <QSignalSpy>
#include <QWidgetAction>

using namespace deskflow::gui::tray;
using deskflow::core::ConnectionState;
using deskflow::core::ProcessState;
using tray_test::makeInput;
using Kind = TrayMenuEntry::Kind;

Q_DECLARE_METATYPE(TrayCommand)

namespace {

QAction *actionWithText(const QMenu &menu, const QString &text)
{
  for (auto *action : menu.actions())
    if (action->text() == text)
      return action;
  return nullptr;
}

QStringList texts(const QMenu &menu)
{
  QStringList list;
  for (const auto *action : menu.actions())
    list << (action->isSeparator() ? QStringLiteral("---") : action->text());
  return list;
}

TrayPlatform macPlatform()
{
  return TrayPlatform::mac();
}

} // namespace

void TrayMenuBuilderTests::initTestCase()
{
  qRegisterMetaType<TrayCommand>();
  QAccessible::setActive(true);
}

void TrayMenuBuilderTests::header_isTwoRowsOfTextWhereTheOsDrawsTheMenu()
{
  QMenu menu;
  TrayMenuBuilder builder(&menu, macPlatform());
  auto input = makeInput(ProcessState::Started, ConnectionState::Connected);
  input.headline = QStringLiteral("Sharing with 2 computers");
  input.detail = QStringLiteral("Deskflow is connected, with 2 clients");
  builder.apply(TrayMenuModel(macPlatform()).build(input));

  QVERIFY(!builder.headerWidget());
  const auto actions = menu.actions();
  QCOMPARE(actions.at(0)->text(), QStringLiteral("Sharing with 2 computers"));
  QCOMPARE(actions.at(1)->text(), QStringLiteral("Deskflow is connected, with 2 clients"));
  for (int i = 0; i < 2; ++i) {
    QVERIFY(!qobject_cast<QWidgetAction *>(actions.at(i)));
    QVERIFY(!actions.at(i)->isEnabled());
  }
  QVERIFY(actions.at(0)->font().bold());
  QVERIFY(!actions.at(1)->font().bold());
  QVERIFY(actions.at(2)->isSeparator());

  // a header with no sentence has one row
  input.detail.clear();
  builder.apply(TrayMenuModel(macPlatform()).build(input));
  QVERIFY(menu.actions().at(1)->isSeparator());
}

void TrayMenuBuilderTests::header_isOneWidgetWithTheChipWhereQtDrawsTheMenu()
{
  QMenu menu;
  TrayMenuBuilder builder(&menu, TrayPlatform::windows());
  auto input = makeInput(ProcessState::Started, ConnectionState::Connected);
  input.headline = QStringLiteral("Sharing with 2 computers");
  input.detail = QStringLiteral("Deskflow is connected, with 2 clients");
  builder.apply(TrayMenuModel(TrayPlatform::windows()).build(input));

  auto *header = builder.headerWidget();
  QVERIFY(header);
  auto *action = qobject_cast<QWidgetAction *>(menu.actions().first());
  QVERIFY(action);
  QCOMPARE(action->defaultWidget(), header);
  QVERIFY(!action->isEnabled());
  QCOMPARE(header->headline(), QStringLiteral("Sharing with 2 computers"));
  QCOMPARE(header->detail(), QStringLiteral("Deskflow is connected, with 2 clients"));
  QCOMPARE(header->chip()->text(), QStringLiteral("Running"));
  QCOMPARE(header->chip()->kind(), StatusChip::Kind::Success);
  // the three parts are one text for a screen reader, the same for the widget and for the row of the menu
  const auto said = QStringLiteral("Sharing with 2 computers. Deskflow is connected, with 2 clients. Running");
  QCOMPARE(header->accessibleName(), said);
  QCOMPARE(action->text(), said);

  // the header takes the next state: the same widget says it
  input.headline = QStringLiteral("Not sharing");
  input.process = ProcessState::Stopped;
  builder.apply(TrayMenuModel(TrayPlatform::windows()).build(input));
  QCOMPARE(builder.headerWidget(), header);
  QCOMPARE(header->headline(), QStringLiteral("Not sharing"));
  QCOMPARE(header->chip()->text(), QStringLiteral("Stopped"));
}

void TrayMenuBuilderTests::rows_followTheSpec_withSeparators_andKeepTheRowsThatStay()
{
  QMenu menu;
  TrayMenuBuilder builder(&menu, macPlatform());
  const TrayMenuModel model(macPlatform());

  builder.apply(model.build(makeInput(ProcessState::Stopped, ConnectionState::Disconnected)));
  QCOMPARE(
      texts(menu),
      (QStringList{"Headline", "The sentence behind it", "---", "Start", "---", "&Open Deskflow", "&Minimize to tray",
                   "&Preferences", "---", "&Quit"})
  );
  QPointer<QAction> start = builder.actionFor(QStringLiteral("command:start"));
  QPointer<QAction> open = builder.actionFor(QStringLiteral("command:open"));
  QPointer<QAction> quit = builder.actionFor(QStringLiteral("command:quit"));
  QVERIFY(start && open && quit);

  auto running = makeInput(ProcessState::Started, ConnectionState::Connected);
  running.computers = {QStringLiteral("Desk-PC")};
  running.headline = QStringLiteral("Sharing with 1 computer");
  builder.apply(model.build(running));
  QCOMPARE(
      texts(menu),
      (QStringList{"Sharing with 1 computer", "The sentence behind it", "---", "Rest&art", "Stop", "---",
                   "Desk-PC: Connected", "---", "&Open Deskflow", "&Minimize to tray", "&Preferences", "---", "&Quit"})
  );
  // Start is not a row of this state: it is gone; the rows that stay are the same rows (a menu that is open keeps them)
  QVERIFY(!start);
  QVERIFY(builder.actionFor(QStringLiteral("command:open")) == open.data());
  QVERIFY(builder.actionFor(QStringLiteral("command:quit")) == quit.data());
  QVERIFY(!builder.actionFor(QStringLiteral("command:start")));
  QCOMPARE(menu.actions().size(), 13);

  // the same spec again changes nothing
  const auto before = menu.actions();
  builder.apply(model.build(running));
  QCOMPARE(menu.actions(), before);

  // a row that is not enabled any more is dimmed in the same row
  auto minimizeHidden = running;
  minimizeHidden.windowShown = false;
  builder.apply(model.build(minimizeHidden));
  QVERIFY(!builder.actionFor(QStringLiteral("command:minimize"))->isEnabled());
  QVERIFY(builder.actionFor(QStringLiteral("command:open"))->isEnabled());
}

void TrayMenuBuilderTests::command_isToldWhenItIsChosen_andNotWhenItIsDimmed()
{
  QMenu menu;
  TrayMenuBuilder builder(&menu, macPlatform());
  QSignalSpy spy(&builder, &TrayMenuBuilder::commandTriggered);
  builder.apply(TrayMenuModel(macPlatform()).build(makeInput(ProcessState::Started, ConnectionState::Listening)));

  builder.actionFor(QStringLiteral("command:restart"))->trigger();
  builder.actionFor(QStringLiteral("command:stop"))->trigger();
  builder.actionFor(QStringLiteral("command:open"))->trigger();
  builder.actionFor(QStringLiteral("command:preferences"))->trigger();
  builder.actionFor(QStringLiteral("command:quit"))->trigger();
  QCOMPARE(spy.size(), 5);
  QCOMPARE(spy.at(0).at(0).value<TrayCommand>(), TrayCommand::Restart);
  QCOMPARE(spy.at(1).at(0).value<TrayCommand>(), TrayCommand::Stop);
  QCOMPARE(spy.at(2).at(0).value<TrayCommand>(), TrayCommand::OpenWindow);
  QCOMPARE(spy.at(3).at(0).value<TrayCommand>(), TrayCommand::Preferences);
  QCOMPARE(spy.at(4).at(0).value<TrayCommand>(), TrayCommand::Quit);

  // a row of text is not a command, and a command that is dimmed cannot be chosen
  spy.clear();
  builder.actionFor(QStringLiteral("computers:none"))->trigger();
  auto hidden = makeInput(ProcessState::Started, ConnectionState::Listening, true, false);
  builder.apply(TrayMenuModel(macPlatform()).build(hidden));
  builder.actionFor(QStringLiteral("command:minimize"))->trigger();
  QVERIFY(spy.isEmpty());
  // and the row that is there is told after the rows that were not
  builder.actionFor(QStringLiteral("command:stop"))->trigger();
  QCOMPARE(spy.size(), 1);
  QCOMPARE(spy.last().at(0).value<TrayCommand>(), TrayCommand::Stop);
}

void TrayMenuBuilderTests::shortcuts_areShownButBelongToTheMenuAlone()
{
  QMenu menu;
  TrayMenuBuilder builder(&menu, macPlatform());
  builder.apply(TrayMenuModel(macPlatform()).build(makeInput(ProcessState::Started, ConnectionState::Listening)));
  const auto *stop = builder.actionFor(QStringLiteral("command:stop"));
  QCOMPARE(stop->shortcut(), QKeySequence(QStringLiteral("Ctrl+T")));
  QCOMPARE(stop->shortcutContext(), Qt::WidgetShortcut);
  QVERIFY(stop->isShortcutVisibleInContextMenu());
  // the rows are not roles of the OS: nothing of the menu moves to the menu of the application
  for (const auto *action : menu.actions())
    if (!action->isSeparator())
      QCOMPARE(action->menuRole(), QAction::NoRole);
}

void TrayMenuBuilderTests::icons_areTemplatesOnMacAndNamesOnLinux()
{
  const auto input = makeInput(ProcessState::Started, ConnectionState::Connected);
  {
    QMenu menu;
    TrayMenuBuilder builder(&menu, TrayPlatform::mac());
    builder.apply(TrayMenuModel(TrayPlatform::mac()).build(input));
    for (const auto *id : {"command:restart", "command:stop", "command:open", "command:preferences", "command:quit"}) {
      const auto icon = builder.actionFor(QLatin1String(id))->icon();
      QVERIFY2(!icon.isNull(), id);
      QVERIFY2(icon.isMask(), id);
    }
    // the rows of a group of text only have no icon
    QVERIFY(builder.actionFor(QStringLiteral("header"))->icon().isNull());
  }
  {
    QMenu menu;
    TrayMenuBuilder builder(&menu, TrayPlatform::windows());
    builder.apply(TrayMenuModel(TrayPlatform::windows()).build(input));
    const auto icon = builder.actionFor(QStringLiteral("command:restart"))->icon();
    QVERIFY(!icon.isNull());
    QVERIFY(!icon.isMask());
  }
  {
    // the desktop draws a menu of Linux from names: the icons are the ones of the theme, asked by name
    QMenu menu;
    TrayMenuBuilder builder(&menu, TrayPlatform::linux());
    builder.apply(TrayMenuModel(TrayPlatform::linux()).build(input));
    const auto icon = builder.actionFor(QStringLiteral("command:restart"))->icon();
    QVERIFY(!icon.isMask());
    // there is no theme here: the icon is the one that the theme has not got, and no icon of the new look stands in
    QVERIFY(icon.isNull() || icon.name() == QStringLiteral("view-refresh"));
  }
}

void TrayMenuBuilderTests::openRow_isInBold_andOnlyIt()
{
  QMenu menu;
  TrayMenuBuilder builder(&menu, macPlatform());
  builder.apply(TrayMenuModel(macPlatform()).build(makeInput(ProcessState::Started, ConnectionState::Connected)));
  QVERIFY(builder.actionFor(QStringLiteral("command:open"))->font().bold());
  for (const auto *id : {"command:restart", "command:stop", "command:preferences", "command:quit"})
    QVERIFY2(!builder.actionFor(QLatin1String(id))->font().bold(), id);
}

void TrayMenuBuilderTests::screenReader_isToldTheRowsAndTheirState()
{
  for (const bool windowsOs : {false, true}) {
    QMenu menu;
    const auto platform = windowsOs ? TrayPlatform::windows() : TrayPlatform::mac();
    const TrayMenuModel model(platform);
    TrayMenuBuilder builder(&menu, platform);
    auto input = makeInput(ProcessState::Started, ConnectionState::Connected, true, true);
    input.headline = QStringLiteral("Sharing with 1 computer");
    input.computers = {QStringLiteral("Desk-PC")};
    builder.apply(model.build(input));

    // what a screen reader is told of a row: its role, its name (without the letter of the keyboard) and whether it is dimmed
    const auto read = [&menu]() {
      auto *menuInterface = QAccessible::queryAccessibleInterface(&menu);
      QMap<QString, QAccessibleInterface *> items;
      if (!menuInterface)
        return items;
      for (int i = 0; i < menuInterface->childCount(); ++i) {
        auto *child = menuInterface->child(i);
        if (child && child->role() != QAccessible::Separator) {
          auto name = child->text(QAccessible::Name);
          name.remove(QLatin1Char('&'));
          items.insert(name, child);
        }
      }
      return items;
    };
    auto *menuInterface = QAccessible::queryAccessibleInterface(&menu);
    QVERIFY(menuInterface);
    QCOMPARE(menuInterface->role(), QAccessible::PopupMenu);
    auto items = read();
    for (auto *item : std::as_const(items))
      QCOMPARE(item->role(), QAccessible::MenuItem);

    // the header is the first thing that is read, and it says the state (the widget row says all of the header)
    auto *first = menuInterface->child(0);
    QVERIFY(first->text(QAccessible::Name).startsWith(QStringLiteral("Sharing with 1 computer")));
    QVERIFY(first->state().disabled);
    // the commands can be chosen, and the facts cannot
    QVERIFY(items.value(QStringLiteral("Restart")));
    QVERIFY(!items.value(QStringLiteral("Restart"))->state().disabled);
    QVERIFY(items.value(QStringLiteral("Desk-PC: Connected")));
    QVERIFY(items.value(QStringLiteral("Desk-PC: Connected"))->state().disabled);
    QVERIFY(!items.value(QStringLiteral("Minimize to tray"))->state().disabled);

    // the window is not on the screen: macOS and Linux read the row as dimmed, Windows has no such row
    input.windowShown = false;
    builder.apply(model.build(input));
    items = read();
    if (windowsOs) {
      QVERIFY(!items.contains(QStringLiteral("Minimize to tray")));
    } else {
      QVERIFY(items.value(QStringLiteral("Minimize to tray")));
      QVERIFY(items.value(QStringLiteral("Minimize to tray"))->state().disabled);
    }
  }
}

QTEST_MAIN(TrayMenuBuilderTests)
