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
#include <QDoubleSpinBox>
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
  Settings::setValue(Settings::Gui::InputIndicatorCompanion);
  Settings::setValue(Settings::Gui::InputIndicatorPosition);
  Settings::setValue(Settings::Gui::InputIndicatorDwellMs);
  Settings::setValue(Settings::Gui::InputIndicatorOpacityPercent);
  Settings::setValue(Settings::Gui::InputIndicatorSpeedPercent);
  Settings::setValue(Settings::Gui::InputIndicatorIdleGrowthMaxScale);
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
  auto *companion = find<QComboBox>(
      pages, "comboInputIndicatorCompanion"
  );
  auto *position = must<QComboBox>(pages, "comboInputIndicatorPosition");
  auto *opacity = must<QSpinBox>(pages, "sbInputIndicatorOpacity");
  auto *speed = must<QSpinBox>(pages, "sbInputIndicatorSpeed");
  auto *idleGrowth = must<QDoubleSpinBox>(pages, "sbInputIndicatorIdleGrowthMaxScale");
  auto *tone = must<QComboBox>(pages, "comboInputIndicatorTone");
  auto *note = must<InlineHint>(pages, "hintInputIndicatorSafety");

  QVERIFY(companion);
  QVERIFY(!enabled->isChecked());
  QCOMPARE(companion->currentData().toString(), QStringLiteral("cat"));
  QVERIFY(companion->findData(QStringLiteral("dog")) >= 0);
  QVERIFY(companion->findData(QStringLiteral("turtle")) >= 0);
  QCOMPARE(
      Settings::value(Settings::Gui::InputIndicatorCompanion).toString(),
      QStringLiteral("cat")
  );
  QCOMPARE(position->currentData().toString(), QStringLiteral("cursorBottomRight"));
  const auto followApproach =
      position->findData(
          QStringLiteral("cursorFollowApproach")
      );
  QVERIFY(followApproach >= 0);
  QCOMPARE(
      position->itemText(followApproach),
      QCoreApplication::translate(
          "SettingsPages", "Follow approach"
      )
  );
  QCOMPARE(opacity->value(), 65);
  QCOMPARE(speed->minimum(), 50);
  QCOMPARE(speed->maximum(), 200);
  QCOMPARE(speed->singleStep(), 1);
  QCOMPARE(speed->value(), 100);
  QCOMPARE(speed->suffix(), QStringLiteral("%"));
  QCOMPARE(idleGrowth->minimum(), 1.0);
  QCOMPARE(idleGrowth->maximum(), 5.0);
  QCOMPARE(idleGrowth->singleStep(), 0.1);
  QCOMPARE(idleGrowth->decimals(), 1);
  QCOMPARE(idleGrowth->value(), 5.0);
  QCOMPARE(idleGrowth->suffix(), QStringLiteral("x"));
  QCOMPARE(tone->currentData().toString(), QStringLiteral("automatic"));
  QVERIFY(!find<QSpinBox>(pages, "sbInputIndicatorDwell"));
  QCOMPARE(Settings::value(Settings::Gui::InputIndicatorDwellMs).toInt(), 1500);
  QCOMPARE(
      Settings::defaultValue(
          Settings::Gui::InputIndicatorSpeedPercent
      ).toInt(),
      100
  );
  QVERIFY(Settings::validKeys().contains(
      Settings::Gui::InputIndicatorSpeedPercent
  ));
  QCOMPARE(Settings::defaultValue(Settings::Gui::InputIndicatorIdleGrowthMaxScale).toDouble(), 5.0);
  QVERIFY(Settings::validKeys().contains(Settings::Gui::InputIndicatorIdleGrowthMaxScale));
  QVERIFY(note->text().contains(QStringLiteral("input-source")));
  QVERIFY(note->text().contains(QStringLiteral("mouse pointer")));
  QVERIFY(note->text().contains(QStringLiteral("Accessibility")));
  QVERIFY(note->text().contains(QStringLiteral("does not read")));
  QVERIFY(!note->text().contains(QStringLiteral("Password")));

  for (const auto &key :
       {Settings::Gui::InputIndicatorEnabled, Settings::Gui::InputIndicatorPosition,
        Settings::Gui::InputIndicatorCompanion, Settings::Gui::InputIndicatorDwellMs,
        Settings::Gui::InputIndicatorOpacityPercent, Settings::Gui::InputIndicatorSpeedPercent,
        Settings::Gui::InputIndicatorIdleGrowthMaxScale, Settings::Gui::InputIndicatorTone}) {
    QCOMPARE(effectOf(key), std::optional<Effect>(Effect::Live));
  }
}

void InputIndicatorSettingsTests::dependencyControlsFollowTheToggle()
{
  SettingsPages pages;
  auto *enabled = must<ToggleSwitch>(pages, "cbInputIndicator");
  auto *companion = find<QComboBox>(
      pages, "comboInputIndicatorCompanion"
  );
  auto *position = must<QComboBox>(pages, "comboInputIndicatorPosition");
  auto *opacity = must<QSpinBox>(pages, "sbInputIndicatorOpacity");
  auto *speed = must<QSpinBox>(pages, "sbInputIndicatorSpeed");
  auto *idleGrowth = must<QDoubleSpinBox>(pages, "sbInputIndicatorIdleGrowthMaxScale");
  auto *tone = must<QComboBox>(pages, "comboInputIndicatorTone");

  QVERIFY(companion);
  QVERIFY(!companion->isEnabled());
  QVERIFY(!position->isEnabled());
  QVERIFY(!opacity->isEnabled());
  QVERIFY(!speed->isEnabled());
  QVERIFY(!idleGrowth->isEnabled());
  QVERIFY(!tone->isEnabled());

  enabled->setChecked(true);

  QVERIFY(companion->isEnabled());
  QVERIFY(position->isEnabled());
  QVERIFY(opacity->isEnabled());
  QVERIFY(speed->isEnabled());
  QVERIFY(idleGrowth->isEnabled());
  QVERIFY(tone->isEnabled());

  const auto followApproach =
      position->findData(
          QStringLiteral("cursorFollowApproach")
      );
  QVERIFY(followApproach >= 0);
  position->setCurrentIndex(followApproach);
  Q_EMIT position->activated(followApproach);
  QCOMPARE(
      Settings::value(
          Settings::Gui::InputIndicatorPosition
      ).toString(),
      QStringLiteral("cursorFollowApproach")
  );

  const auto dog = companion->findData(QStringLiteral("dog"));
  QVERIFY(dog >= 0);
  companion->setCurrentIndex(dog);
  Q_EMIT companion->activated(dog);
  QCOMPARE(
      Settings::value(Settings::Gui::InputIndicatorCompanion).toString(),
      QStringLiteral("dog")
  );
  QVERIFY(speed->isEnabled());
  const auto turtle = companion->findData(QStringLiteral("turtle"));
  QVERIFY(turtle >= 0);
  companion->setCurrentIndex(turtle);
  Q_EMIT companion->activated(turtle);
  QVERIFY(speed->isEnabled());
  const auto cat = companion->findData(QStringLiteral("cat"));
  QVERIFY(cat >= 0);
  companion->setCurrentIndex(cat);
  Q_EMIT companion->activated(cat);
  QVERIFY(speed->isEnabled());
  speed->setValue(175);
  QCOMPARE(
      Settings::value(
          Settings::Gui::InputIndicatorSpeedPercent
      ).toInt(),
      175
  );
  speed->setValue(1);
  QCOMPARE(speed->value(), 50);
  speed->setValue(999);
  QCOMPARE(speed->value(), 200);
  idleGrowth->setValue(1.5);
  QCOMPARE(Settings::value(Settings::Gui::InputIndicatorIdleGrowthMaxScale).toDouble(), 1.5);
  idleGrowth->setValue(0.1);
  QCOMPARE(idleGrowth->value(), 1.0);
  idleGrowth->setValue(9.9);
  QCOMPARE(idleGrowth->value(), 5.0);
}

void InputIndicatorSettingsTests::invalidStoredSpeedUsesSafeBounds()
{
  Settings::setValue(
      Settings::Gui::InputIndicatorSpeedPercent,
      QStringLiteral("not-a-number")
  );
  SettingsPages pages;
  auto *speed = must<QSpinBox>(
      pages, "sbInputIndicatorSpeed"
  );
  QCOMPARE(speed->value(), 50);
  QCOMPARE(
      Settings::value(
          Settings::Gui::InputIndicatorCompanion
      ).toString(),
      QStringLiteral("cat")
  );
}

void InputIndicatorSettingsTests::invalidStoredIdleGrowthMaximumUsesDefault()
{
  Settings::setValue(Settings::Gui::InputIndicatorIdleGrowthMaxScale, QStringLiteral("not-a-number"));
  SettingsPages pages;
  auto *idleGrowth = must<QDoubleSpinBox>(pages, "sbInputIndicatorIdleGrowthMaxScale");
  QCOMPARE(idleGrowth->value(), 5.0);
}

QTEST_MAIN(InputIndicatorSettingsTests)
