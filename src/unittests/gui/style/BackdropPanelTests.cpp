/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "BackdropPanelTests.h"

#include "gui/style/Appearance.h"
#include "gui/style/BackdropWidget.h"
#include "gui/style/Contrast.h"
#include "gui/style/DeskflowStyle.h"
#include "gui/style/PanelWidget.h"

#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QPixmapCache>

#include <algorithm>

using namespace deskflow::gui::style;

namespace {

Look lookOf(bool dark, bool reduceTransparency = false)
{
  Look look;
  look.scheme = dark ? Scheme::Dark : Scheme::Light;
  look.reduceTransparency = reduceTransparency;
  return look;
}

DeskflowStyle *install(const Look &look)
{
  auto *style = new DeskflowStyle;
  QApplication::setStyle(style);
  style->useLook(look);
  return style;
}

bool near(const QColor &a, const QColor &b, int tolerance = 4)
{
  return qAbs(a.red() - b.red()) <= tolerance && qAbs(a.green() - b.green()) <= tolerance &&
         qAbs(a.blue() - b.blue()) <= tolerance;
}

QString describe(const QColor &color)
{
  return color.name();
}

// A window of backdrop with one panel in it: the panel's body is at (50, 46) to (330, 224) in window coordinates
struct Scene
{
  explicit Scene(PanelWidget::Density density = PanelWidget::Density::Dense) : panel(density, &window)
  {
    window.resize(640, 420);
    panel.setGeometry(40, 40, 300, 200);
  }

  QImage backdropOnly()
  {
    panel.hide();
    const auto image = window.grab().toImage();
    panel.show();
    return image;
  }

  QImage withPanel()
  {
    return window.grab().toImage();
  }

  BackdropWidget window;
  PanelWidget panel;
};

// The least contrast that text has on a panel that is over any of the colours that can be behind it
double worstAnalytic(const Tokens &tokens, qreal opacity)
{
  double worst = 21;
  for (const auto &stop : tokens.backdropStops) {
    for (const auto &behind :
         {stop, over(tokens.blobUpperLeft, tokens.blobUpperLeft.alphaF(), stop),
          over(tokens.blobLowerRight, tokens.blobLowerRight.alphaF(), stop)}) {
      worst = std::min(worst, contrastRatio(tokens.colors.text, over(tokens.colors.surface, opacity, behind)));
    }
  }
  return worst;
}

} // namespace

void BackdropPanelTests::cleanup()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  QApplication::setPalette(QPalette());
  QPixmapCache::clear();
}

void BackdropPanelTests::backdrop_isAGradientWithBlobs_andFlatWhenTransparencyIsReduced()
{
  install(lookOf(false));
  const auto &tokens = currentTokens();
  {
    BackdropWidget window;
    window.resize(640, 420);
    const auto image = window.grab().toImage();
    // not one colour: the upper left is not the lower right, and neither is the window colour
    QVERIFY(!near(image.pixelColor(5, 5), image.pixelColor(634, 414), 12));
    QVERIFY(!near(image.pixelColor(320, 210), tokens.colors.bg, 2));
    // the blob in the upper left is white over the first stop: lighter than the stop
    QVERIFY(image.pixelColor(77, 34).lightness() > tokens.backdropStops[0].lightness());
  }

  install(lookOf(false, true));
  BackdropWidget flat;
  flat.resize(640, 420);
  const auto image = flat.grab().toImage();
  for (const auto &point : {QPoint(5, 5), QPoint(634, 414), QPoint(320, 210), QPoint(77, 34)})
    QVERIFY2(
        near(image.pixelColor(point), currentTokens().colors.bg, 1), qPrintable(describe(image.pixelColor(point)))
    );
}

void BackdropPanelTests::backdrop_isKeptBetweenPaints_andMadeAgainOnlyForOtherSizeOrLook()
{
  auto *style = install(lookOf(false));
  BackdropWidget window;
  window.resize(640, 420);
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  window.grab();
  QCOMPARE(window.renderCount(), 1);
  window.update();
  window.grab();
  window.grab();
  QCOMPARE(window.renderCount(), 1);

  window.resize(700, 420);
  window.grab();
  QCOMPARE(window.renderCount(), 1);
  QTest::qWait(100);
  window.grab();
  QCOMPARE(window.renderCount(), 2);

  style->useLook(lookOf(true));
  window.grab();
  QCOMPARE(window.renderCount(), 3);
}

void BackdropPanelTests::backdrop_doesNotRenderEveryFrameOfARapidResize_andFinishesWhenItSettles()
{
  install(lookOf(false));
  BackdropWidget window;
  window.resize(640, 420);
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  window.grab();
  QCOMPARE(window.renderCount(), 1);

  for (int frame = 1; frame <= 12; ++frame) {
    window.resize(640 + frame * 8, 420 + frame * 3);
    window.grab();
    QTest::qWait(5);
  }
  QCOMPARE(window.renderCount(), 1);

  QTest::qWait(100);
  window.grab();
  QCOMPARE(window.renderCount(), 2);
  window.update();
  window.grab();
  QCOMPARE(window.renderCount(), 2);
}

void BackdropPanelTests::backdrop_atTwoTimes_hasTwiceThePixels()
{
  const auto tokens = tokensFor(lookOf(false));
  const auto picture = backdropPixmap({100, 80}, 2.0, tokens);
  QCOMPARE(picture.size(), QSize(200, 160));
  QCOMPARE(picture.devicePixelRatio(), 2.0);
  QVERIFY(backdropPixmap({0, 80}, 1.0, tokens).isNull());
}

void BackdropPanelTests::shadowPatch_fadesOutFromTheShapeAndIsKept()
{
  const QColor color(24, 32, 90, 90);
  const auto patch = shadowPatch(18, 20, color, 1.0);
  // two corners of 18 + 2 * 20 and two pixels between them
  QCOMPARE(patch.size(), QSize(118, 118));
  const auto image = patch.toImage().convertToFormat(QImage::Format_ARGB32);
  // nothing in the outermost corner, the whole colour in the middle, and a ramp between them
  QCOMPARE(image.pixelColor(0, 0).alpha(), 0);
  QVERIFY(qAbs(image.pixelColor(59, 59).alpha() - color.alpha()) <= 2);
  int last = -1;
  for (int x = 0; x <= 59; ++x) {
    const auto alpha = image.pixelColor(x, 59).alpha();
    QVERIFY2(alpha >= last, qPrintable(QString::number(x)));
    last = alpha;
  }
  QVERIFY(image.pixelColor(25, 59).alpha() > 0);
  QVERIFY(image.pixelColor(25, 59).alpha() < image.pixelColor(40, 59).alpha());

  QCOMPARE(shadowPatch(18, 20, color, 1.0).cacheKey(), patch.cacheKey());
  QCOMPARE(shadowPatch(18, 20, color, 2.0).size(), QSize(236, 236));
}

void BackdropPanelTests::panel_isTranslucentOverTheBackdrop_andDenseIsMoreOpaque_data()
{
  QTest::addColumn<bool>("dark");
  QTest::addColumn<bool>("dense");
  QTest::newRow("light, normal") << false << false;
  QTest::newRow("light, dense") << false << true;
  QTest::newRow("dark, normal") << true << false;
  QTest::newRow("dark, dense") << true << true;
}

void BackdropPanelTests::panel_isTranslucentOverTheBackdrop_andDenseIsMoreOpaque()
{
  QFETCH(bool, dark);
  QFETCH(bool, dense);
  install(lookOf(dark));
  const auto &tokens = currentTokens();

  Scene scene(dense ? PanelWidget::Density::Dense : PanelWidget::Density::Normal);
  const auto behind = scene.backdropOnly();
  const auto image = scene.withPanel();
  const auto opacity = dense ? tokens.panelDenseOpacity : tokens.panelOpacity;
  for (const auto &point : {QPoint(190, 135), QPoint(80, 90), QPoint(300, 200), QPoint(120, 190)}) {
    const auto expected = over(tokens.colors.surface, opacity, behind.pixelColor(point));
    QVERIFY2(
        near(image.pixelColor(point), expected, 3),
        qPrintable(QStringLiteral("%1: %2 expected %3")
                       .arg(point.x())
                       .arg(describe(image.pixelColor(point)), describe(expected)))
    );
  }
}

void BackdropPanelTests::panel_textKeepsItsContrastOnEveryPixelThatIsRendered_data()
{
  QTest::addColumn<bool>("dark");
  QTest::addColumn<bool>("dense");
  QTest::newRow("light, normal") << false << false;
  QTest::newRow("light, dense") << false << true;
  QTest::newRow("dark, normal") << true << false;
  QTest::newRow("dark, dense") << true << true;
}

// The check that the design makes on paper (the worst case over the colours behind the panel) against what is painted
void BackdropPanelTests::panel_textKeepsItsContrastOnEveryPixelThatIsRendered()
{
  QFETCH(bool, dark);
  QFETCH(bool, dense);
  install(lookOf(dark));
  const auto &tokens = currentTokens();

  Scene scene(dense ? PanelWidget::Density::Dense : PanelWidget::Density::Normal);
  // the panel over a backdrop of the whole window, so that it is over the blobs as well
  scene.window.resize(1280, 800);
  scene.panel.setGeometry(0, 0, 1280, 800);
  const auto image = scene.withPanel();

  double worst = 21;
  for (int y = 40; y < 780; y += 7) {
    for (int x = 40; x < 1240; x += 7)
      worst = std::min(worst, contrastRatio(tokens.colors.text, image.pixelColor(x, y)));
  }
  const auto analytic = worstAnalytic(tokens, dense ? tokens.panelDenseOpacity : tokens.panelOpacity);
  QVERIFY2(worst >= 4.5, qPrintable(QString::number(worst)));
  // the worst case of the design is a bound that nothing that is painted goes under
  QVERIFY2(worst >= analytic - 0.02, qPrintable(QStringLiteral("rendered %1, on paper %2").arg(worst).arg(analytic)));
}

void BackdropPanelTests::panel_hasRoundedCornersAndAShadowOutsideItsBody()
{
  install(lookOf(false));
  Scene scene;
  const auto behind = scene.backdropOnly();
  const auto image = scene.withPanel();

  const auto body = scene.panel.panelRect().translated(scene.panel.pos());
  QCOMPARE(body, QRect(50, 46, 280, 178));
  // the corner of the body is cut by the radius: what is there is the backdrop (and a little of its shadow)
  QVERIFY(
      near(image.pixelColor(body.left() + 1, body.top() + 1), behind.pixelColor(body.left() + 1, body.top() + 1), 12)
  );
  // below the panel the shadow darkens the backdrop; far from it nothing changes
  QVERIFY(image.pixelColor(190, body.bottom() + 5).lightness() < behind.pixelColor(190, body.bottom() + 5).lightness());
  QVERIFY(near(image.pixelColor(190, 330), behind.pixelColor(190, 330), 2));
  // and the shadow is not under the panel: the inside is the colour of the panel over the backdrop, not darker than
  // that
  const auto expected =
      over(currentTokens().colors.surface, currentTokens().panelDenseOpacity, behind.pixelColor(190, 100));
  QVERIFY(near(image.pixelColor(190, 100), expected, 3));
}

void BackdropPanelTests::panel_whenTransparencyIsReduced_isOpaqueAndHasNoShadow()
{
  install(lookOf(false, true));
  Scene scene;
  const auto behind = scene.backdropOnly();
  const auto image = scene.withPanel();
  const auto body = scene.panel.panelRect().translated(scene.panel.pos());

  QVERIFY2(
      near(image.pixelColor(190, 135), currentTokens().colors.surface, 1),
      qPrintable(describe(image.pixelColor(190, 135)))
  );
  QVERIFY(near(image.pixelColor(190, body.bottom() + 5), behind.pixelColor(190, body.bottom() + 5), 1));
}

void BackdropPanelTests::panel_followsTheLookWhenItChanges()
{
  auto *style = install(lookOf(false));
  Scene scene;
  const auto light = scene.withPanel();
  QVERIFY(light.pixelColor(190, 135).lightness() > 200);

  style->useLook(lookOf(true));
  const auto dark = scene.withPanel();
  // the panel was made before the look changed, and it is painted in the colours of the new look
  QVERIFY(dark.pixelColor(190, 135).lightness() < light.pixelColor(190, 135).lightness());
  QVERIFY(dark.pixelColor(190, 135).lightness() < 100);
}

void BackdropPanelTests::panel_withoutTheStyleLayer_isOpaqueInTheColoursOfTheOs()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  QVERIFY(!isDeskflowStyleInstalled());
  Scene scene;
  const auto image = scene.withPanel();
  // the panel is the colour of a surface of the OS, the backdrop is the colour of its windows, and nothing is blended
  QVERIFY(near(image.pixelColor(190, 135), QApplication::palette().color(QPalette::Base), 1));
  QVERIFY(near(image.pixelColor(5, 5), QApplication::palette().color(QPalette::Window), 1));
}

void BackdropPanelTests::panel_atAFractionalScale_hasAnEdgeOfWholeDevicePixels()
{
  install(lookOf(false));
  const auto &tokens = currentTokens();
  // 125 %: the panel is painted into a picture with 1.25 device pixels in a logical pixel, over the backdrop
  QImage image(QSize(800, 525), QImage::Format_ARGB32_Premultiplied);
  image.setDevicePixelRatio(1.25);
  {
    QPainter painter(&image);
    painter.drawPixmap(0, 0, backdropPixmap({640, 420}, 1.25, tokens));
    painter.translate(40, 40);
    paintPanelBody(&painter, QRect(0, 0, 300, 200), QRect(10, 6, 280, 178), tokens, true, true, 1.25);
  }
  // the top edge of the body is at 46 * 1.25 = 57.5 device pixels: it is put on the row 58, and the row above is the
  // backdrop, not a blend of it and the panel
  const int x = qRound(190 * 1.25);
  const auto above = image.pixelColor(x, 57);
  const auto edge = image.pixelColor(x, 58);
  QVERIFY2(above.lightness() < 246, qPrintable(describe(above)));
  QVERIFY2(edge.lightness() > 252, qPrintable(describe(edge)));
}

void BackdropPanelTests::panel_contentsMarginsAreTheShadowAndThePadding()
{
  PanelWidget panel;
  const auto shadow = PanelWidget::shadowMargins();
  QCOMPARE(panel.contentsMargins(), shadow + QMargins(20, 20, 20, 20));

  panel.setPadding(12);
  QCOMPARE(panel.contentsMargins(), shadow + QMargins(12, 12, 12, 12));

  panel.setElevated(false);
  QCOMPARE(panel.contentsMargins(), QMargins(12, 12, 12, 12));
  panel.resize(200, 100);
  QCOMPARE(panel.panelRect(), QRect(0, 0, 200, 100));
}

QTEST_MAIN(BackdropPanelTests)
