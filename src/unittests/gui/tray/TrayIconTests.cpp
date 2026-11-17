/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TrayIconTests.h"

#include "gui/tray/TrayIcons.h"

#include <QApplication>
#include <QPixmapCache>

using namespace deskflow::gui::tray;

Q_DECLARE_METATYPE(TrayState)

namespace {

constexpr TrayState kStates[] = {TrayState::Idle, TrayState::Stopped, TrayState::Waiting, TrayState::Connected,
                                 TrayState::Error};

QImage image(TrayState state, int pixels, TrayTone tone = TrayTone::Template, bool update = false, bool colored = false)
{
  return trayIconImage({state, update, tone, colored}, pixels);
}

// How many pixels are in one picture and not in the other, by what is in them (the alpha), more than a step of 80 of 255
int differingPixels(const QImage &a, const QImage &b)
{
  Q_ASSERT(a.size() == b.size());
  int count = 0;
  for (int y = 0; y < a.height(); ++y)
    for (int x = 0; x < a.width(); ++x)
      if (qAbs(qAlpha(a.pixel(x, y)) - qAlpha(b.pixel(x, y))) > 80)
        ++count;
  return count;
}

int inked(const QImage &picture)
{
  int count = 0;
  for (int y = 0; y < picture.height(); ++y)
    for (int x = 0; x < picture.width(); ++x)
      if (qAlpha(picture.pixel(x, y)) > 80)
        ++count;
  return count;
}

qreal luminance(const QColor &color)
{
  const auto linear = [](qreal c) { return c <= 0.03928 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); };
  return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
}

qreal contrast(const QColor &a, const QColor &b)
{
  const auto la = luminance(a);
  const auto lb = luminance(b);
  return (qMax(la, lb) + 0.05) / (qMin(la, lb) + 0.05);
}

} // namespace

void TrayIconTests::markIsThere_atEverySize()
{
  for (const int size : {16, 20, 22, 24, 32, 44, 48, 64}) {
    for (const auto state : kStates) {
      const auto picture = image(state, size);
      QCOMPARE(picture.size(), QSize(size, size));
      // the wave of the program is in every picture: a good part of the picture is ink (a mark that was not found is none)
      QVERIFY2(inked(picture) > size * size / 8, qPrintable(QStringLiteral("%1 at %2").arg(trayStateName(state)).arg(size)));
    }
  }
  // no size, no picture
  QVERIFY(image(TrayState::Idle, 0).isNull() || image(TrayState::Idle, 0).size().isEmpty());
}

void TrayIconTests::template_isBlackAndClear()
{
  // macOS tints a template by what is clear and what is not: nothing in it may have a colour of its own
  for (const auto state : kStates) {
    const auto picture = image(state, 32, TrayTone::Template, true, true);
    for (int y = 0; y < picture.height(); ++y) {
      for (int x = 0; x < picture.width(); ++x) {
        const auto pixel = picture.pixelColor(x, y);
        if (pixel.alpha() > 0) {
          QCOMPARE(pixel.red(), 0);
          QCOMPARE(pixel.green(), 0);
          QCOMPARE(pixel.blue(), 0);
        }
      }
    }
  }
}

void TrayIconTests::everyStateHasAShapeOfItsOwn_atTheSmallSizes_data()
{
  QTest::addColumn<int>("size");
  for (const int size : {16, 18, 22, 24, 32})
    QTest::addRow("%d px", size) << size;
}

void TrayIconTests::everyStateHasAShapeOfItsOwn_atTheSmallSizes()
{
  QFETCH(int, size);
  // each pair of states differs by some pixels that a person sees (the alpha differs by a good step): the shapes are not
  // the same shape whatever the colour is. The least of the differences is the evidence that the states read at this size
  int least = 1 << 20;
  QString leastPair;
  for (const auto a : kStates) {
    for (const auto b : kStates) {
      if (a >= b)
        continue;
      const auto differing = differingPixels(image(a, size), image(b, size));
      if (differing < least) {
        least = differing;
        leastPair = trayStateName(a) + QLatin1String(" and ") + trayStateName(b);
      }
      QVERIFY2(
          differing >= 4,
          qPrintable(QStringLiteral("%1 and %2 at %3 px: %4").arg(trayStateName(a), trayStateName(b)).arg(size).arg(differing))
      );
    }
  }
  qInfo().noquote() << QStringLiteral("%1 px: the two states that are the closest, %2, differ by %3 pixels").arg(size).arg(leastPair).arg(least);
}

void TrayIconTests::colour_neverChangesTheShape()
{
  // the colour is a help: the picture of a state is the same shape (the same alpha) in colour and without it
  for (const auto tone : {TrayTone::OnLightBar, TrayTone::OnDarkBar}) {
    for (const auto state : kStates) {
      const auto mono = image(state, 32, tone, false, false);
      const auto colour = image(state, 32, tone, false, true);
      QCOMPARE(differingPixels(mono, colour), 0);
    }
  }
  // and the colours are the ones of the shapes: not the colour of the mark
  const auto mono = image(TrayState::Connected, 64, TrayTone::OnLightBar, false, false);
  const auto colour = image(TrayState::Connected, 64, TrayTone::OnLightBar, false, true);
  QVERIFY(mono != colour);
}

void TrayIconTests::update_isADotAtTheUpperRightOfAnyState()
{
  for (const auto state : kStates) {
    const auto plain = image(state, 48);
    const auto with = image(state, 48, TrayTone::Template, true);
    int upperRight = 0;
    int elsewhere = 0;
    for (int y = 0; y < 48; ++y) {
      for (int x = 0; x < 48; ++x) {
        if (qAbs(qAlpha(plain.pixel(x, y)) - qAlpha(with.pixel(x, y))) <= 80)
          continue;
        if (x >= 24 && y < 24)
          ++upperRight;
        else
          ++elsewhere;
      }
    }
    QVERIFY2(upperRight >= 6, qPrintable(trayStateName(state)));
    QVERIFY2(elsewhere == 0, qPrintable(trayStateName(state)));
  }
}

void TrayIconTests::tones_haveTheContrastThatTheirBarNeeds()
{
  // a dark mark on a light bar, a light mark on a dark bar: 3:1 is what a graphic needs
  QVERIFY(contrast(trayMarkColor(TrayTone::OnLightBar), QColor(0xf3, 0xf3, 0xf3)) >= 7);
  QVERIFY(contrast(trayMarkColor(TrayTone::OnDarkBar), QColor(0x20, 0x20, 0x20)) >= 7);
  // the colours of the shapes, on the bars of their tone
  for (const auto tone : {TrayTone::OnLightBar, TrayTone::OnDarkBar}) {
    const QColor bar = tone == TrayTone::OnLightBar ? QColor(0xf3, 0xf3, 0xf3) : QColor(0x20, 0x20, 0x20);
    for (const auto state : {TrayState::Waiting, TrayState::Connected, TrayState::Error}) {
      const auto color = trayBadgeColor({state, false, tone, true});
      QVERIFY2(
          contrast(color, bar) >= 3,
          qPrintable(QStringLiteral("%1 on tone %2").arg(trayStateName(state)).arg(static_cast<int>(tone)))
      );
    }
  }
  // the dot of an update is blue on a coloured icon and the colour of the mark when it is not
  QVERIFY(contrast(trayUpdateColor({TrayState::Idle, true, TrayTone::OnLightBar, true}), QColor(0xf3, 0xf3, 0xf3)) >= 3);
  QVERIFY(contrast(trayUpdateColor({TrayState::Idle, true, TrayTone::OnDarkBar, true}), QColor(0x20, 0x20, 0x20)) >= 3);
  QCOMPARE(trayUpdateColor({TrayState::Idle, true, TrayTone::OnDarkBar, false}), trayMarkColor(TrayTone::OnDarkBar));
  // a template has no colour of its own, whatever is asked
  QCOMPARE(trayBadgeColor({TrayState::Error, false, TrayTone::Template, true}), QColor(Qt::black));
}

void TrayIconTests::names_areTheSymbolicNamesOfTheTheme()
{
  // the mark alone is the name that the symbolic icon of the program has always had
  QCOMPARE(trayIconName({TrayState::Idle}), QStringLiteral("org.deskflow.deskflow-symbolic"));
  QCOMPARE(trayIconName({TrayState::Stopped}), QStringLiteral("org.deskflow.deskflow-stopped-symbolic"));
  QCOMPARE(trayIconName({TrayState::Waiting}), QStringLiteral("org.deskflow.deskflow-waiting-symbolic"));
  QCOMPARE(trayIconName({TrayState::Connected}), QStringLiteral("org.deskflow.deskflow-connected-symbolic"));
  QCOMPARE(trayIconName({TrayState::Error}), QStringLiteral("org.deskflow.deskflow-error-symbolic"));
  QCOMPARE(
      trayIconName({TrayState::Connected, true}), QStringLiteral("org.deskflow.deskflow-connected-update-symbolic")
  );
  QCOMPARE(trayIconName({TrayState::Idle, true}), QStringLiteral("org.deskflow.deskflow-update-symbolic"));
}

void TrayIconTests::icon_drawsItselfAtTheSizeAndTheRatioItIsAskedFor()
{
  QPixmapCache::clear();
  const auto icon = trayIcon({TrayState::Connected, false, TrayTone::Template, false});
  QVERIFY(!icon.isNull());
  QVERIFY(icon.isMask());
  // a tray of 22 points at 2 pixels to a point is a picture of 44 pixels with a ratio of 2
  const auto retina = icon.pixmap(QSize(22, 22), 2.0);
  QCOMPARE(retina.size(), QSize(44, 44));
  QCOMPARE(retina.devicePixelRatio(), 2.0);
  const auto small = icon.pixmap(QSize(16, 16));
  QCOMPARE(small.size(), QSize(16, 16));
  QCOMPARE(small.toImage().convertedTo(QImage::Format_ARGB32_Premultiplied), image(TrayState::Connected, 16));
  // the sizes that the trays ask for are all there
  const auto sizes = icon.availableSizes();
  for (const int side : {16, 22, 24, 32, 48, 64})
    QVERIFY(sizes.contains(QSize(side, side)));
  // a coloured icon is not a template (it is the one for Windows and Linux)
  QVERIFY(!trayIcon({TrayState::Connected, false, TrayTone::OnLightBar, true}).isMask());
  // an icon that was copied draws the same
  const auto copy = icon;
  QCOMPARE(copy.pixmap(QSize(24, 24)).toImage(), icon.pixmap(QSize(24, 24)).toImage());
}

QTEST_MAIN(TrayIconTests)
