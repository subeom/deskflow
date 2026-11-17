/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ToggleSwitchTests.h"

#include "gui/style/Appearance.h"
#include "gui/style/DeskflowStyle.h"
#include "gui/widgets/ToggleSwitch.h"

#include <QApplication>
#include <QCheckBox>
#include <QFocusEvent>
#include <QImage>
#include <QPixmapCache>
#include <QSignalSpy>

using namespace deskflow::gui::style;

namespace {

Look lookOf(bool dark = false, bool reduceMotion = false)
{
  Look look;
  look.scheme = dark ? Scheme::Dark : Scheme::Light;
  look.reduceMotion = reduceMotion;
  return look;
}

DeskflowStyle *install(const Look &look)
{
  auto *style = new DeskflowStyle;
  QApplication::setStyle(style);
  style->useLook(look);
  return style;
}

bool near(const QColor &a, const QColor &b, int tolerance = 3)
{
  return qAbs(a.red() - b.red()) <= tolerance && qAbs(a.green() - b.green()) <= tolerance &&
         qAbs(a.blue() - b.blue()) <= tolerance;
}

// A switch without a text is 52 x 34: the track of 46 x 28 and 3 px around it for the ring. The knob is at x = 17 when
// off and x = 35 when on, and the middle of the track is at y = 17.
constexpr int kMiddleY = 17;
constexpr int kOffKnobX = 17;
constexpr int kOnKnobX = 35;

void settle(ToggleSwitch &toggle)
{
  toggle.resize(toggle.sizeHint());
  toggle.show();
}

void focusIn(QWidget &widget, Qt::FocusReason reason)
{
  QFocusEvent event(QEvent::FocusIn, reason);
  QApplication::sendEvent(&widget, &event);
}

} // namespace

void ToggleSwitchTests::cleanup()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  QApplication::setPalette(QPalette());
  QPixmapCache::clear();
}

void ToggleSwitchTests::withoutTheStyleLayer_isACheckBoxInEveryRespect()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  QVERIFY(!isDeskflowStyleInstalled());

  ToggleSwitch toggle(QStringLiteral("Share the clipboard"));
  QCheckBox reference(QStringLiteral("Share the clipboard"));
  for (auto *check : {static_cast<QCheckBox *>(&toggle), &reference})
    check->setChecked(true);

  QCOMPARE(toggle.sizeHint(), reference.sizeHint());
  QCOMPARE(toggle.minimumSizeHint(), reference.minimumSizeHint());
  toggle.resize(reference.sizeHint());
  reference.resize(reference.sizeHint());
  // pixel for pixel: the program without the style layer looks as it did
  QCOMPARE(toggle.grab().toImage(), reference.grab().toImage());
}

void ToggleSwitchTests::withTheStyleLayer_hasTheSizeOfTheTrack_andOfTheTextBesideIt()
{
  install(lookOf());

  ToggleSwitch bare;
  QCOMPARE(bare.sizeHint(), QSize(52, 34));
  QCOMPARE(ToggleSwitch::trackSize(false), QSize(46, 28));

  ToggleSwitch labelled(QStringLiteral("Share the clipboard"));
  const auto textWidth = labelled.fontMetrics().size(Qt::TextShowMnemonic, labelled.text()).width();
  QCOMPARE(ToggleSwitch::trackSize(true), QSize(36, 20));
  // the track, the room for the ring at both sides, the gap and the text
  QCOMPARE(labelled.sizeHint().width(), 36 + 6 + 8 + textWidth);
  QVERIFY(labelled.sizeHint().height() >= 26);
  QCOMPARE(labelled.minimumSizeHint(), labelled.sizeHint());
}

void ToggleSwitchTests::click_togglesAndSignalsLikeACheckBox()
{
  install(lookOf(false, true));
  ToggleSwitch toggle;
  settle(toggle);
  QSignalSpy toggled(&toggle, &QAbstractButton::toggled);
  QSignalSpy clicked(&toggle, &QAbstractButton::clicked);

  QTest::mouseClick(&toggle, Qt::LeftButton);
  QVERIFY(toggle.isChecked());
  QCOMPARE(toggled.size(), 1);
  QCOMPARE(toggled.last().first().toBool(), true);
  QCOMPARE(clicked.size(), 1);

  QTest::mouseClick(&toggle, Qt::LeftButton);
  QVERIFY(!toggle.isChecked());
  QCOMPARE(toggled.size(), 2);

  // set by the code: the signal goes out, the click does not
  toggle.setChecked(true);
  QCOMPARE(toggled.size(), 3);
  QCOMPARE(clicked.size(), 2);
}

void ToggleSwitchTests::click_hitsTheWholeSwitch_andSpaceTogglesToo()
{
  install(lookOf(false, true));
  ToggleSwitch toggle;
  settle(toggle);

  // the ring around the track is part of the target
  QTest::mouseClick(&toggle, Qt::LeftButton, Qt::NoModifier, QPoint(1, 1));
  QVERIFY(toggle.isChecked());
  QTest::mouseClick(&toggle, Qt::LeftButton, Qt::NoModifier, QPoint(toggle.width() - 2, toggle.height() - 2));
  QVERIFY(!toggle.isChecked());

  QTest::keyClick(&toggle, Qt::Key_Space);
  QVERIFY(toggle.isChecked());
  QTest::keyClick(&toggle, Qt::Key_Space);
  QVERIFY(!toggle.isChecked());
}

void ToggleSwitchTests::disabled_isDimmedAndIgnoresClicks()
{
  install(lookOf(false, true));
  ToggleSwitch toggle;
  toggle.setChecked(true);
  settle(toggle);
  const auto enabled = toggle.grab().toImage();

  toggle.setEnabled(false);
  QTest::mouseClick(&toggle, Qt::LeftButton);
  QVERIFY(toggle.isChecked());
  const auto disabled = toggle.grab().toImage();
  // the accent of the track fades: lighter on a light window
  QVERIFY(disabled.pixelColor(12, kMiddleY).lightness() > enabled.pixelColor(12, kMiddleY).lightness() + 20);
}

void ToggleSwitchTests::paint_showsTheStateInTheColoursOfTheTokens()
{
  install(lookOf(false, true));
  const auto &c = currentTokens().colors;
  ToggleSwitch toggle;
  settle(toggle);

  const auto off = toggle.grab().toImage();
  // off: the knob is the colour of the border at the start of the track, the rest of the track is the surface
  QVERIFY2(
      near(off.pixelColor(kOffKnobX, kMiddleY), c.controlBorder), qPrintable(off.pixelColor(kOffKnobX, kMiddleY).name())
  );
  QVERIFY2(near(off.pixelColor(kOnKnobX, kMiddleY), c.surface), qPrintable(off.pixelColor(kOnKnobX, kMiddleY).name()));

  toggle.setChecked(true);
  const auto on = toggle.grab().toImage();
  // on: the track is the accent and the knob, at the end, is what is on the accent
  QVERIFY2(near(on.pixelColor(kOffKnobX, kMiddleY), c.accent), qPrintable(on.pixelColor(kOffKnobX, kMiddleY).name()));
  QVERIFY2(near(on.pixelColor(kOnKnobX, kMiddleY), c.onAccent), qPrintable(on.pixelColor(kOnKnobX, kMiddleY).name()));
}

void ToggleSwitchTests::paint_followsTheLookWhenItChanges()
{
  auto *style = install(lookOf(false, true));
  ToggleSwitch toggle;
  settle(toggle);
  const auto light = toggle.grab().toImage();

  style->useLook(lookOf(true, true));
  const auto dark = toggle.grab().toImage();
  QVERIFY(dark.pixelColor(kOnKnobX, kMiddleY).lightness() < light.pixelColor(kOnKnobX, kMiddleY).lightness() - 40);
}

void ToggleSwitchTests::knob_movesInTheTimeOfAHover_andAtOnceWhenMotionIsReduced()
{
  auto *style = install(lookOf(false, false));
  ToggleSwitch toggle;
  settle(toggle);
  QCOMPARE(toggle.knobPosition(), 0.0);

  toggle.setChecked(true);
  // it starts at the off end and is on its way
  QVERIFY(toggle.knobPosition() < 1.0);
  QTRY_COMPARE_WITH_TIMEOUT(toggle.knobPosition(), 1.0, 2000);

  style->useLook(lookOf(false, true));
  toggle.setChecked(false);
  QCOMPARE(toggle.knobPosition(), 0.0);

  // a switch that is not shown has nothing to move: it is at its end
  ToggleSwitch hidden;
  hidden.setChecked(true);
  QCOMPARE(hidden.knobPosition(), 1.0);
}

void ToggleSwitchTests::ring_showsForTheKeyboardOnly()
{
  install(lookOf(false, true));
  const auto &c = currentTokens().colors;
  ToggleSwitch toggle;
  settle(toggle);
  const auto plain = toggle.grab().toImage();

  // the mouse puts the focus on it without a ring
  focusIn(toggle, Qt::MouseFocusReason);
  QCOMPARE(toggle.grab().toImage(), plain);
  QFocusEvent mouseOut(QEvent::FocusOut, Qt::MouseFocusReason);
  QApplication::sendEvent(&toggle, &mouseOut);

  focusIn(toggle, Qt::TabFocusReason);
  const auto ring = toggle.grab().toImage();
  QVERIFY(ring != plain);
  QVERIFY2(near(ring.pixelColor(2, kMiddleY), c.accent), qPrintable(ring.pixelColor(2, kMiddleY).name()));

  QFocusEvent out(QEvent::FocusOut, Qt::TabFocusReason);
  QApplication::sendEvent(&toggle, &out);
  QCOMPARE(toggle.grab().toImage(), plain);
}

void ToggleSwitchTests::text_isDrawnBesideTheTrack()
{
  install(lookOf(false, true));
  ToggleSwitch toggle(QStringLiteral("Heartbeat"));
  settle(toggle);
  const auto image = toggle.grab().toImage();

  // the text lies right of the track (36 + 6 + 8), and something there is not the background
  const auto background = image.pixelColor(image.width() - 1, 0);
  bool found = false;
  for (int x = 50; x < image.width() && !found; ++x) {
    for (int y = 0; y < image.height() && !found; ++y)
      found = !near(image.pixelColor(x, y), background, 40);
  }
  QVERIFY(found);
}

QTEST_MAIN(ToggleSwitchTests)
