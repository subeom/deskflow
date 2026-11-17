/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "IconsTests.h"

#include "gui/style/DeskflowStyle.h"
#include "gui/style/Icons.h"

#include <QApplication>
#include <QImage>
#include <QPixmapCache>

using namespace deskflow::gui::style;

namespace {

bool near(const QColor &a, const QColor &b, int tolerance = 12)
{
  return qAbs(a.red() - b.red()) <= tolerance && qAbs(a.green() - b.green()) <= tolerance &&
         qAbs(a.blue() - b.blue()) <= tolerance;
}

// The colour of the strongest pixels of a picture: what the icon was drawn in
QColor strongestColor(const QPixmap &pixmap)
{
  const auto image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
  QColor best;
  int bestAlpha = -1;
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      const auto pixel = image.pixelColor(x, y);
      if (pixel.alpha() > bestAlpha) {
        bestAlpha = pixel.alpha();
        best = pixel;
      }
    }
  }
  return best;
}

Look lookOf(bool dark)
{
  Look look;
  look.scheme = dark ? Scheme::Dark : Scheme::Light;
  return look;
}

} // namespace

void IconsTests::cleanup()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  QApplication::setPalette(QPalette());
  QPixmapCache::clear();
}

void IconsTests::availableIcons_areTheBundledSubsetInThreeWeights()
{
  const auto regular = availableIcons(IconWeight::Regular);
  const auto duotone = availableIcons(IconWeight::Duotone);
  const auto fill = availableIcons(IconWeight::Fill);

  // what the designs use: the same names in regular and duotone, a few that carry a state in fill
  QVERIFY(regular.size() >= 50);
  QCOMPARE(regular, duotone);
  QVERIFY(regular.contains(QStringLiteral("gear")));
  QVERIFY(regular.contains(QStringLiteral("wifi-high")));
  QVERIFY(regular.contains(QStringLiteral("sidebar-simple")));
  QVERIFY(fill.contains(QStringLiteral("check-circle")));
  QVERIFY(fill.contains(QStringLiteral("warning")));
  QVERIFY(!fill.contains(QStringLiteral("gear")));
  QVERIFY(hasIcon(QStringLiteral("gear"), IconWeight::Duotone));
  QVERIFY(!hasIcon(QStringLiteral("gear"), IconWeight::Fill));
  QVERIFY(!hasIcon(QStringLiteral("no-such-icon")));
}

void IconsTests::coloredSvg_putsTheColourIn_andOnlyForAnIconThatExists()
{
  const auto svg = coloredSvg(QStringLiteral("gear"), IconWeight::Regular, QColor(0x30, 0x50, 0xE0));
  QVERIFY(svg.contains("<svg"));
  QVERIFY(svg.contains("#3050e0"));
  QVERIFY(!svg.contains("currentColor"));
  QVERIFY(coloredSvg(QStringLiteral("no-such-icon"), IconWeight::Regular, Qt::black).isEmpty());
}

void IconsTests::iconPixmap_isRenderedAtTheDevicePixelRatio_inTheColourAsked()
{
  const QColor color(0x10, 0x73, 0x3A);
  const auto one = iconPixmap(QStringLiteral("check-circle"), IconWeight::Regular, color, {24, 24}, 1.0);
  QCOMPARE(one.size(), QSize(24, 24));
  QCOMPARE(one.devicePixelRatio(), 1.0);

  const auto two = iconPixmap(QStringLiteral("check-circle"), IconWeight::Regular, color, {24, 24}, 2.0);
  QCOMPARE(two.size(), QSize(48, 48));
  QCOMPARE(two.devicePixelRatio(), 2.0);

  // the corner of the picture is empty, and what is drawn is in the colour that was asked for
  QCOMPARE(two.toImage().pixelColor(0, 0).alpha(), 0);
  const auto strongest = strongestColor(two);
  QVERIFY2(strongest.alpha() > 200, qPrintable(QString::number(strongest.alpha())));
  QVERIFY2(near(strongest, color), qPrintable(strongest.name()));
}

void IconsTests::iconPixmap_duotoneHasALayerOfLessOpacity()
{
  const auto pixmap = iconPixmap(QStringLiteral("shield-check"), IconWeight::Duotone, Qt::black, {64, 64}, 1.0);
  const auto image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
  int fullyDrawn = 0;
  int tinted = 0;
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      const auto alpha = image.pixelColor(x, y).alpha();
      fullyDrawn += alpha > 240 ? 1 : 0;
      // the second layer of a duotone icon is 20 % of the colour
      tinted += (alpha > 40 && alpha < 70) ? 1 : 0;
    }
  }
  QVERIFY(fullyDrawn > 50);
  QVERIFY2(tinted > 100, qPrintable(QString::number(tinted)));
}

void IconsTests::iconPixmap_isKeptForTheNextTime()
{
  const QColor color(Qt::red);
  const QSize size(20, 20);
  const auto key = iconCacheKey(QStringLiteral("gear"), IconWeight::Regular, color, size, 2.0);
  QVERIFY(!key.isEmpty());
  // other colour, size, weight or device pixel ratio: another key
  QVERIFY(key != iconCacheKey(QStringLiteral("gear"), IconWeight::Regular, Qt::blue, size, 2.0));
  QVERIFY(key != iconCacheKey(QStringLiteral("gear"), IconWeight::Regular, color, {22, 22}, 2.0));
  QVERIFY(key != iconCacheKey(QStringLiteral("gear"), IconWeight::Duotone, color, size, 2.0));
  QVERIFY(key != iconCacheKey(QStringLiteral("gear"), IconWeight::Regular, color, size, 1.0));

  QPixmap found;
  QVERIFY(!QPixmapCache::find(key, &found));
  const auto first = iconPixmap(QStringLiteral("gear"), IconWeight::Regular, color, size, 2.0);
  QVERIFY(QPixmapCache::find(key, &found));
  QCOMPARE(iconPixmap(QStringLiteral("gear"), IconWeight::Regular, color, size, 2.0).cacheKey(), first.cacheKey());
}

void IconsTests::iconPixmap_ofAnUnknownIcon_isNull()
{
  QVERIFY(iconPixmap(QStringLiteral("no-such-icon"), IconWeight::Regular, Qt::black, {24, 24}).isNull());
}

void IconsTests::iconColor_followsTheRoleAndTheStateOfTheIcon()
{
  const auto tokens = tokensFor(lookOf(false));
  const auto &c = tokens.colors;
  QCOMPARE(iconColor(IconColor::Text, QIcon::Normal, tokens), c.text);
  QCOMPARE(iconColor(IconColor::TextSecondary, QIcon::Normal, tokens), c.textSecondary);
  QCOMPARE(iconColor(IconColor::Accent, QIcon::Normal, tokens), c.accent);
  QCOMPARE(iconColor(IconColor::OnAccent, QIcon::Normal, tokens), c.onAccent);
  QCOMPARE(iconColor(IconColor::Success, QIcon::Normal, tokens), c.success);
  QCOMPARE(iconColor(IconColor::Warning, QIcon::Normal, tokens), c.warning);
  QCOMPARE(iconColor(IconColor::Danger, QIcon::Normal, tokens), c.danger);
  // whatever the role, an icon that is off is the colour of what is off
  for (const auto role : {IconColor::Text, IconColor::Accent, IconColor::Danger})
    QCOMPARE(iconColor(role, QIcon::Disabled, tokens), c.textDisabled);
  // and on a selected row the colour that has the contrast with the accent
  QCOMPARE(iconColor(IconColor::Text, QIcon::Selected, tokens), c.onAccent);
}

void IconsTests::icon_isPaintedInTheColoursOfTheLookThatTheApplicationHasThen()
{
  auto *style = new DeskflowStyle;
  QApplication::setStyle(style);
  const auto gear = icon(QStringLiteral("gear"));
  QVERIFY(!gear.isNull());

  style->useLook(lookOf(false));
  const auto onLight = strongestColor(gear.pixmap(QSize(24, 24)));
  QVERIFY2(near(onLight, tokensFor(lookOf(false)).colors.text), qPrintable(onLight.name()));

  // the same icon after the OS went dark: no new icon is made
  style->useLook(lookOf(true));
  QPixmapCache::clear();
  const auto onDark = strongestColor(gear.pixmap(QSize(24, 24)));
  QVERIFY2(near(onDark, tokensFor(lookOf(true)).colors.text), qPrintable(onDark.name()));
}

void IconsTests::icon_inTheModeDisabled_isInTheDisabledColour()
{
  auto *style = new DeskflowStyle;
  QApplication::setStyle(style);
  style->useLook(lookOf(false));
  const auto gear = icon(QStringLiteral("gear"), IconColor::Accent);
  const auto disabled = strongestColor(gear.pixmap(QSize(24, 24), QIcon::Disabled));
  QVERIFY2(near(disabled, tokensFor(lookOf(false)).colors.textDisabled), qPrintable(disabled.name()));
  const auto normal = strongestColor(gear.pixmap(QSize(24, 24), QIcon::Normal));
  QVERIFY2(near(normal, tokensFor(lookOf(false)).colors.accent), qPrintable(normal.name()));
}

void IconsTests::icon_atTwoTimes_isTwiceTheSizeInPixels()
{
  const auto gear = icon(QStringLiteral("gear"));
  const auto pixmap = gear.pixmap(QSize(24, 24), 2.0);
  QCOMPARE(pixmap.size(), QSize(48, 48));
  QCOMPARE(pixmap.devicePixelRatio(), 2.0);
}

QTEST_MAIN(IconsTests)
