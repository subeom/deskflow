/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AccessibleInterfaces.h"

#include "Appearance.h"
#include "LookFollower.h"

#include <QAbstractButton>
#include <QAccessible>
#include <QAccessibleWidget>

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

// A switch: a button that is on or off
class SwitchAccessible : public QAccessibleWidget
{
public:
  explicit SwitchAccessible(QAbstractButton *button) : QAccessibleWidget(button, QAccessible::Switch)
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

QAccessibleInterface *factory(const QString &className, QObject *object)
{
  auto *widget = qobject_cast<QWidget *>(object);
  if (!widget)
    return nullptr;

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
  if (className == QLatin1String("StatusChip"))
    return new RoleAccessible(widget, QAccessible::StaticText);
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
