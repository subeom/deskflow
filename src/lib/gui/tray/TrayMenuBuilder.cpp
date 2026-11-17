/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TrayMenuBuilder.h"

#include "TrayHeaderWidget.h"

#include "gui/style/Icons.h"

#include <QAction>
#include <QMenu>
#include <QWidgetAction>

namespace deskflow::gui::tray {

using Kind = TrayMenuEntry::Kind;

namespace {

// the second row of the header where it is two rows of text
constexpr auto kDetailId = "header:detail";

QFont boldFont(QFont font)
{
  font.setBold(true);
  return font;
}

} // namespace

TrayMenuBuilder::TrayMenuBuilder(QMenu *menu, const TrayPlatform &platform, QObject *parent)
    : QObject(parent),
      m_menu(menu),
      m_platform(platform)
{
}

QMenu *TrayMenuBuilder::menu() const
{
  return m_menu;
}

QAction *TrayMenuBuilder::actionFor(const QString &id) const
{
  return m_actions.value(id);
}

TrayHeaderWidget *TrayMenuBuilder::headerWidget() const
{
  return m_header;
}

void TrayMenuBuilder::setRoundedFrame(bool rounded)
{
  m_menu->setProperty("deskflowRoundedMenu", rounded);
  m_menu->setWindowFlag(Qt::FramelessWindowHint, rounded);
  m_menu->setWindowFlag(Qt::NoDropShadowWindowHint, rounded);
  m_menu->setAttribute(Qt::WA_TranslucentBackground, rounded);
}

QIcon TrayMenuBuilder::iconFor(const QString &name) const
{
  if (name.isEmpty())
    return {};
  if (m_platform.themeIcons)
    return QIcon::fromTheme(TrayMenuModel::themeIconName(name));
  auto icon = style::icon(name, style::IconColor::Text);
  // the menu of macOS tints an icon that is a template (black and clear) to the colour of the text of the row
  if (m_platform.templateIcons)
    icon.setIsMask(true);
  return icon;
}

QAction *TrayMenuBuilder::rowFor(const TrayMenuEntry &entry)
{
  if (auto *found = m_actions.value(entry.id))
    return found;

  QAction *action = nullptr;
  if (entry.kind == Kind::Header && m_platform.headerAsWidget) {
    auto *widgetAction = new QWidgetAction(this);
    m_header = new TrayHeaderWidget;
    widgetAction->setDefaultWidget(m_header);
    action = widgetAction;
  } else {
    action = new QAction(this);
  }
  if (entry.kind == Kind::Separator)
    action->setSeparator(true);
  if (entry.kind == Kind::Command) {
    // the shortcut that the row shows is only shown: the action of the window is the one that has it for the window
    action->setShortcutContext(Qt::WidgetShortcut);
    action->setShortcutVisibleInContextMenu(true);
    // what a row says that it is must not be taken for something of the application menu by the OS
    action->setMenuRole(QAction::NoRole);
    const auto command = entry.command;
    connect(action, &QAction::triggered, this, [this, command] { Q_EMIT commandTriggered(command); });
  } else {
    action->setMenuRole(QAction::NoRole);
  }
  m_actions.insert(entry.id, action);
  return action;
}

void TrayMenuBuilder::update(QAction *action, const TrayMenuEntry &entry)
{
  if (entry.kind == Kind::Separator)
    return;
  action->setEnabled(entry.enabled);
  action->setToolTip(entry.toolTip);
  action->setIcon(iconFor(entry.icon));
  if (entry.kind == Kind::Command) {
    action->setText(entry.text);
    action->setShortcut(entry.shortcut);
    // only the row in bold has a font of its own: the others are drawn as the OS draws a row
    if (entry.bold)
      action->setFont(boldFont(QFont()));
  } else if (entry.kind == Kind::Text) {
    action->setText(entry.text);
  }
}

void TrayMenuBuilder::apply(const TrayMenuSpec &spec)
{
  QList<QAction *> wanted;
  QSet<QString> ids;
  const auto add = [&](const QString &id, QAction *action) {
    ids.insert(id);
    wanted.append(action);
  };

  for (const auto &entry : spec.entries) {
    if (entry.kind == Kind::Header && !m_platform.headerAsWidget) {
      // two rows of text: the state in words, in bold, and the sentence behind it
      auto *head = rowFor(entry);
      head->setText(entry.text);
      head->setEnabled(false);
      head->setFont(boldFont(QFont()));
      head->setToolTip(entry.detail);
      add(entry.id, head);
      if (!entry.detail.isEmpty()) {
        TrayMenuEntry detail;
        detail.kind = Kind::Text;
        detail.id = QLatin1String(kDetailId);
        detail.text = TrayMenuModel::elided(entry.detail);
        detail.toolTip = entry.detail;
        detail.enabled = false;
        auto *row = rowFor(detail);
        update(row, detail);
        add(detail.id, row);
      }
      continue;
    }
    auto *action = rowFor(entry);
    if (entry.kind == Kind::Header) {
      m_header->setContent(spec.state, entry.text, entry.detail, entry.chipText, entry.chipKind);
      // the widget is what is drawn, and the text of the row is what a screen reader reads of the row: all of the header
      action->setText(m_header->accessibleName());
      action->setEnabled(false);
    } else {
      update(action, entry);
    }
    add(entry.id, action);
  }

  // the rows that the menu does not have any more
  for (auto it = m_actions.begin(); it != m_actions.end();) {
    if (ids.contains(it.key())) {
      ++it;
      continue;
    }
    if (it.key() == QLatin1String("header"))
      m_header = nullptr;
    m_menu->removeAction(it.value());
    delete it.value();
    it = m_actions.erase(it);
  }

  if (m_menu->actions() != wanted) {
    m_menu->clear();
    m_menu->addActions(wanted);
  }
}

} // namespace deskflow::gui::tray
