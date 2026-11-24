/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "InputIndicatorSettingsTests.h"

#include "common/Settings.h"
#include "gui/settings/SettingsPage.h"
#include "gui/settings/SettingEffects.h"
#include "gui/settings/SettingsPages.h"
#include "gui/widgets/InlineHint.h"
#include "gui/widgets/ToggleSwitch.h"

#include <QComboBox>
#include <QSpinBox>
#include <QTest>

using namespace deskflow::gui::settings;

namespace {

template <class T> T *find(const SettingsPages &pages, const char *name)
{
  for (const auto &entry : pages.entries()) {
    if (auto *found = entry.page->findChild<T *>(QString::fromLatin1(name))) {
      return found;
    }
  }
  return nullptr;
}

template <class T> T *must(const SettingsPages &pages, const char *name)
{
  auto *found = find<T>(pages, name);
  if (found) {
    return found;
  }
  Q_ASSERT_X(false, "must", name);
  return nullptr;
}

void clearIndicatorSettings()
{
  Settings::setValue(Settings::Gui::InputIndicatorEnabled);
  Settings::setValue(Settings::Gui::InputIndicatorPosition);
  Settings::setValue(Settings::Gui::InputIndicatorDwellMs);
  Settings::setValue(Settings::Gui::InputIndicatorOpacityPercent);
  Settings::setValue(Settings::Gui::InputIndicatorTone);
}

} // namespace

void InputIndicatorSettingsTests::initTestCase()
{
  QVERIFY(m_dir.isValid());
  Settings::setSettingsFile(m_dir.filePath(QStringLiteral("Deskflow.conf")));
  Settings::setStateFile(m_dir.filePath(QStringLiteral("Deskflow.state")));
}

void InputIndicatorSettingsTests::cleanup()
{
  clearIndicatorSettings();
}

void InputIndicatorSettingsTests::defaultsAreSafeAndVisible()
{
  SettingsPages pages;
  auto *enabled = must<ToggleSwitch>(pages, "cbInputIndicator");
  auto *position = must<QComboBox>(pages, "comboInputIndicatorPosition");
  auto *opacity = must<QSpinBox>(pages, "sbInputIndicatorOpacity");
  auto *tone = must<QComboBox>(pages, "comboInputIndicatorTone");
  auto *note = must<InlineHint>(pages, "hintInputIndicatorSafety");

  QVERIFY(!enabled->isChecked());
  QCOMPARE(position->currentData().toString(), QStringLiteral("cursorBottomRight"));
  QCOMPARE(opacity->value(), 65);
  QCOMPARE(tone->currentData().toString(), QStringLiteral("automatic"));
  QVERIFY(!find<QSpinBox>(pages, "sbInputIndicatorDwell"));
  QCOMPARE(Settings::value(Settings::Gui::InputIndicatorDwellMs).toInt(), 1500);
  QVERIFY(note->text().contains(QStringLiteral("input-source")));
  QVERIFY(note->text().contains(QStringLiteral("mouse pointer")));
  QVERIFY(note->text().contains(QStringLiteral("Accessibility")));
  QVERIFY(note->text().contains(QStringLiteral("does not read")));
  QVERIFY(!note->text().contains(QStringLiteral("Password")));

  for (const auto &key :
       {Settings::Gui::InputIndicatorEnabled, Settings::Gui::InputIndicatorPosition,
        Settings::Gui::InputIndicatorDwellMs, Settings::Gui::InputIndicatorOpacityPercent,
        Settings::Gui::InputIndicatorTone}) {
    QCOMPARE(effectOf(key), std::optional<Effect>(Effect::Live));
  }
}

void InputIndicatorSettingsTests::dependencyControlsFollowTheToggle()
{
  SettingsPages pages;
  auto *enabled = must<ToggleSwitch>(pages, "cbInputIndicator");
  auto *position = must<QComboBox>(pages, "comboInputIndicatorPosition");
  auto *opacity = must<QSpinBox>(pages, "sbInputIndicatorOpacity");
  auto *tone = must<QComboBox>(pages, "comboInputIndicatorTone");

  QVERIFY(!position->isEnabled());
  QVERIFY(!opacity->isEnabled());
  QVERIFY(!tone->isEnabled());

  enabled->setChecked(true);

  QVERIFY(position->isEnabled());
  QVERIFY(opacity->isEnabled());
  QVERIFY(tone->isEnabled());
}

QTEST_MAIN(InputIndicatorSettingsTests)
