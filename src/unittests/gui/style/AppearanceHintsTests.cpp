/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AppearanceHintsTests.h"

#include "gui/style/AppearanceHints.h"

#include <QPalette>

using namespace deskflow::gui::style;

namespace {

QPalette paletteWithWindow(const QColor &window, const QColor &text)
{
  QPalette palette;
  palette.setColor(QPalette::Window, window);
  palette.setColor(QPalette::WindowText, text);
  return palette;
}

} // namespace

void AppearanceHintsTests::look_theSchemeThatTheOsSays_decides()
{
  // the OS says dark; the palette is light (a program that set its own palette): the OS is right
  SystemHints hints;
  hints.scheme = Qt::ColorScheme::Dark;
  QCOMPARE(lookFor(hints, paletteWithWindow(Qt::white, Qt::black)).scheme, Scheme::Dark);

  hints.scheme = Qt::ColorScheme::Light;
  QCOMPARE(lookFor(hints, paletteWithWindow(Qt::black, Qt::white)).scheme, Scheme::Light);
}

void AppearanceHintsTests::look_withoutASchemeFromTheOs_thePaletteDecides_data()
{
  QTest::addColumn<bool>("darkPalette");
  QTest::newRow("light palette") << false;
  QTest::newRow("dark palette") << true;
}

void AppearanceHintsTests::look_withoutASchemeFromTheOs_thePaletteDecides()
{
  QFETCH(bool, darkPalette);
  const auto palette = darkPalette ? paletteWithWindow(QColor(0x20, 0x20, 0x20), QColor(0xEE, 0xEE, 0xEE))
                                   : paletteWithWindow(QColor(0xEE, 0xEE, 0xEE), QColor(0x20, 0x20, 0x20));
  QCOMPARE(lookFor(SystemHints{}, palette).scheme, darkPalette ? Scheme::Dark : Scheme::Light);
}

void AppearanceHintsTests::look_carriesTheAccessibilityOptions()
{
  SystemHints hints;
  hints.scheme = Qt::ColorScheme::Light;
  hints.highContrast = true;
  hints.reduceTransparency = true;
  hints.reduceMotion = true;
  const auto look = lookFor(hints, QPalette());
  QVERIFY(look.highContrast);
  QVERIFY(look.reduceTransparency);
  QVERIFY(look.reduceMotion);
  QVERIFY(look.opaque());
  // and none of them by default
  const auto plain = lookFor(SystemHints{Qt::ColorScheme::Light, false, false, false}, QPalette());
  QVERIFY(!plain.highContrast);
  QVERIFY(!plain.reduceTransparency);
  QVERIFY(!plain.reduceMotion);
  QVERIFY(!plain.opaque());
}

void AppearanceHintsTests::readSystemHints_answersWithoutAskingForTheImpossible()
{
  // the offscreen platform has no opinion on anything: this runs on every OS and must not crash or throw
  const auto hints = readSystemHints();
  QVERIFY(
      hints.scheme == Qt::ColorScheme::Unknown || hints.scheme == Qt::ColorScheme::Light ||
      hints.scheme == Qt::ColorScheme::Dark
  );
}

QTEST_MAIN(AppearanceHintsTests)
