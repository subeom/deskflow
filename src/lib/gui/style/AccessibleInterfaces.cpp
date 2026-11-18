/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AccessibleInterfaces.h"

#include "AccessibleItems.h"
#include "Appearance.h"
#include "LookFollower.h"
#include "gui/widgets/NavRail.h"

#include <QAbstractButton>
#include <QAccessible>
#include <QAccessibleWidget>
#include <QGroupBox>
#include <QLabel>

namespace {

using deskflow::gui::style::withoutMnemonic;

// A group, a heading, a line of text: the role, and the name and description that the widget has set
class RoleAccessible : public QAccessibleWidget
{
public:
  RoleAccessible(QWidget *widget, QAccessible::Role role) : QAccessibleWidget(widget, role)
  {
  }
};

// The title of a page: a heading that is named by its text
class TitleAccessible : public QAccessibleWidget
{
public:
  explicit TitleAccessible(QLabel *label) : QAccessibleWidget(label, QAccessible::Heading)
  {
  }

  QString text(QAccessible::Text text) const override
  {
    auto result = QAccessibleWidget::text(text);
    if (text == QAccessible::Name && result.isEmpty())
      result = static_cast<QLabel *>(widget())->text();
    return result;
  }
};

// A switch: a button that is on or off (a check box for a screen reader before Qt 6.11, see switchRole)
class SwitchAccessible : public QAccessibleWidget
{
public:
  explicit SwitchAccessible(QAbstractButton *button) : QAccessibleWidget(button, switchRole())
  {
  }

  QAccessible::State state() const override
  {
    auto state = QAccessibleWidget::state();
    state.checkable = true;
    state.checked = button()->isChecked();
    return state;
  }

  QString text(QAccessible::Text text) const override
  {
    auto result = QAccessibleWidget::text(text);
    // a switch of a row has no text of its own: the row names it. One that has a text is named by it
    if (text == QAccessible::Name && result.isEmpty())
      result = withoutMnemonic(button()->text());
    return result;
  }

  QStringList actionNames() const override
  {
    return {
        QAccessibleActionInterface::toggleAction(), QAccessibleActionInterface::pressAction(),
        QAccessibleActionInterface::setFocusAction()
    };
  }

  void doAction(const QString &name) override
  {
    if (name == QAccessibleActionInterface::toggleAction() || name == QAccessibleActionInterface::pressAction()) {
      if (button()->isEnabled())
        button()->click();
      return;
    }
    QAccessibleWidget::doAction(name);
  }

  QString localizedActionDescription(const QString &name) const override
  {
    return QAccessibleWidget::localizedActionDescription(name);
  }

private:
  QAbstractButton *button() const
  {
    return static_cast<QAbstractButton *>(widget());
  }
};

// An entry of the sidebar: a list item (a page is selectable and the page that is shown is selected), a line of text
// for a status that cannot be pressed
class NavItemAccessible : public QAccessibleWidget
{
public:
  explicit NavItemAccessible(NavItem *item)
      : QAccessibleWidget(item, item->kind() == NavItem::Kind::Status ? QAccessible::StaticText : QAccessible::ListItem)
  {
  }

  QAccessible::State state() const override
  {
    auto state = QAccessibleWidget::state();
    if (item()->kind() == NavItem::Kind::Page) {
      state.selectable = true;
      state.selected = item()->isCurrent();
    } else if (item()->kind() == NavItem::Kind::Action && item()->isCheckable()) {
      state.checkable = true;
      state.checked = item()->isChecked();
    }
    return state;
  }

  QString text(QAccessible::Text text) const override
  {
    auto result = QAccessibleWidget::text(text);
    if (text == QAccessible::Name && result.isEmpty())
      result = withoutMnemonic(item()->text());
    return result;
  }

  QStringList actionNames() const override
  {
    if (item()->kind() == NavItem::Kind::Status)
      return {};
    return {QAccessibleActionInterface::pressAction(), QAccessibleActionInterface::setFocusAction()};
  }

  void doAction(const QString &name) override
  {
    if (name == QAccessibleActionInterface::pressAction()) {
      if (item()->isEnabled() && item()->kind() != NavItem::Kind::Status)
        item()->click();
      return;
    }
    QAccessibleWidget::doAction(name);
  }

private:
  NavItem *item() const
  {
    return static_cast<NavItem *>(widget());
  }
};

// The button that folds the sidebar: it says whether the sidebar is folded or unfolded
class NavToggleAccessible : public QAccessibleWidget
{
public:
  explicit NavToggleAccessible(NavToggleButton *button) : QAccessibleWidget(button, QAccessible::Button)
  {
  }

  QAccessible::State state() const override
  {
    auto state = QAccessibleWidget::state();
    const bool expanded = static_cast<NavToggleButton *>(widget())->isExpandedState();
    state.expandable = true;
    state.expanded = expanded;
    state.collapsed = !expanded;
    return state;
  }

  QStringList actionNames() const override
  {
    return {QAccessibleActionInterface::pressAction(), QAccessibleActionInterface::setFocusAction()};
  }

  void doAction(const QString &name) override
  {
    if (name == QAccessibleActionInterface::pressAction()) {
      static_cast<QAbstractButton *>(widget())->click();
      return;
    }
    QAccessibleWidget::doAction(name);
  }
};

// One choice of a segmented control: a radio button that is chosen or not
class SegmentAccessible : public QAccessibleWidget
{
public:
  explicit SegmentAccessible(QAbstractButton *button) : QAccessibleWidget(button, QAccessible::RadioButton)
  {
  }

  QAccessible::State state() const override
  {
    auto state = QAccessibleWidget::state();
    state.checkable = true;
    state.checked = static_cast<QAbstractButton *>(widget())->isChecked();
    return state;
  }

  QString text(QAccessible::Text text) const override
  {
    auto result = QAccessibleWidget::text(text);
    if (text == QAccessible::Name && result.isEmpty())
      result = withoutMnemonic(static_cast<QAbstractButton *>(widget())->text());
    return result;
  }

  QStringList actionNames() const override
  {
    return {QAccessibleActionInterface::pressAction(), QAccessibleActionInterface::setFocusAction()};
  }

  void doAction(const QString &name) override
  {
    if (name == QAccessibleActionInterface::pressAction()) {
      auto *button = static_cast<QAbstractButton *>(widget());
      if (button->isEnabled())
        button->click();
      return;
    }
    QAccessibleWidget::doAction(name);
  }
};

// A group box that is opened by ticking it: a button that is expanded or collapsed
class DisclosureAccessible : public QAccessibleWidget
{
public:
  explicit DisclosureAccessible(QGroupBox *box) : QAccessibleWidget(box, QAccessible::Button)
  {
  }

  QAccessible::State state() const override
  {
    auto state = QAccessibleWidget::state();
    const bool expanded = box()->isChecked();
    state.expandable = true;
    state.expanded = expanded;
    state.collapsed = !expanded;
    return state;
  }

  QString text(QAccessible::Text text) const override
  {
    auto result = QAccessibleWidget::text(text);
    if (text == QAccessible::Name && result.isEmpty())
      result = withoutMnemonic(box()->title());
    return result;
  }

  QStringList actionNames() const override
  {
    return {
        QAccessibleActionInterface::toggleAction(), QAccessibleActionInterface::pressAction(),
        QAccessibleActionInterface::setFocusAction()
    };
  }

  void doAction(const QString &name) override
  {
    if (name == QAccessibleActionInterface::toggleAction() || name == QAccessibleActionInterface::pressAction()) {
      if (box()->isEnabled())
        box()->setChecked(!box()->isChecked());
      return;
    }
    QAccessibleWidget::doAction(name);
  }

private:
  QGroupBox *box() const
  {
    return static_cast<QGroupBox *>(widget());
  }
};

QAccessibleInterface *factory(const QString &className, QObject *object)
{
  auto *widget = qobject_cast<QWidget *>(object);
  if (!widget)
    return nullptr;

  if (className == QLatin1String("NavRail"))
    return new RoleAccessible(widget, QAccessible::List);
  if (className == QLatin1String("NavItem"))
    return new NavItemAccessible(static_cast<NavItem *>(widget));
  if (className == QLatin1String("NavToggleButton"))
    return new NavToggleAccessible(static_cast<NavToggleButton *>(widget));
  if (className == QLatin1String("SegmentedControl"))
    return new RoleAccessible(widget, QAccessible::Grouping);
  if (className == QLatin1String("SegmentedControlSegment"))
    return new SegmentAccessible(static_cast<QAbstractButton *>(widget));
  if (className == QLatin1String("ToggleSwitch")) {
    // without the style layer it is the check box that it always was, for a screen reader as well
    if (!deskflow::gui::style::isDeskflowStyleInstalled())
      return nullptr;
    return new SwitchAccessible(static_cast<QAbstractButton *>(widget));
  }
  if (className == QLatin1String("SettingRow") || className == QLatin1String("SettingCard") ||
      className == QLatin1String("StatusHero")) {
    return new RoleAccessible(widget, QAccessible::Grouping);
  }
  if (className == QLatin1String("SectionHeader"))
    return new RoleAccessible(widget, QAccessible::Heading);
  if (className == QLatin1String("PageTitle"))
    return new TitleAccessible(static_cast<QLabel *>(widget));
  if (className == QLatin1String("deskflow::gui::settings::RestartBanner"))
    return new RoleAccessible(widget, QAccessible::AlertMessage);
  if (className == QLatin1String("StatusChip"))
    return new RoleAccessible(widget, QAccessible::StaticText);
  if (className == QLatin1String("DisclosureBox")) {
    // the group box with a check box that it always was, for a screen reader as well, when there is no style layer
    if (!deskflow::gui::style::isDeskflowStyleInstalled())
      return nullptr;
    return new DisclosureAccessible(static_cast<QGroupBox *>(widget));
  }
  if (className == QLatin1String("ArrangementCanvas") || className == QLatin1String("MonitorEditorWidget")) {
    // the canvas that was always there, for a screen reader as well, when there is no style layer
    if (!deskflow::gui::style::isDeskflowStyleInstalled())
      return nullptr;
    if (auto *source = dynamic_cast<deskflow::gui::style::AccessibleItemSource *>(widget))
      return deskflow::gui::style::createItemListAccessible(widget, source);
  }
  return nullptr;
}

} // namespace

void installAccessibleInterfaces()
{
  static bool installed = false;
  if (installed)
    return;
  installed = true;
  QAccessible::installFactory(factory);
}
