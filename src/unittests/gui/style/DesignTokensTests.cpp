/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "DesignTokensTests.h"

#include "gui/style/Contrast.h"
#include "gui/style/DesignTokens.h"

#include <QList>

#include <algorithm>

using namespace deskflow::gui::style;

namespace {

Look glass(Scheme scheme)
{
  Look look;
  look.scheme = scheme;
  return look;
}

// The palette that the OS hands over in a high contrast theme: black on white, or white on black, and one accent
QPalette highContrastPalette(bool dark)
{
  const QColor background = dark ? Qt::black : Qt::white;
  const QColor foreground = dark ? Qt::white : Qt::black;
  QPalette os;
  os.setColor(QPalette::Window, background);
  os.setColor(QPalette::WindowText, foreground);
  os.setColor(QPalette::Base, background);
  os.setColor(QPalette::Text, foreground);
  os.setColor(QPalette::Highlight, dark ? QColor(0x1A, 0xEB, 0xFF) : QColor(0x00, 0x00, 0x9E));
  os.setColor(QPalette::HighlightedText, dark ? Qt::black : Qt::white);
  return os;
}

void addGlassLooks()
{
  QTest::addColumn<bool>("dark");
  QTest::newRow("light") << false;
  QTest::newRow("dark") << true;
}

// What can be behind a panel: the stops of the gradient and, at their strongest, the two colour blobs over each of them
QList<QColor> backdropExtremes(const Tokens &tokens)
{
  QList<QColor> extremes;
  for (const auto &stop : tokens.backdropStops) {
    extremes.append(stop);
    for (const auto &blob : {tokens.blobUpperLeft, tokens.blobLowerRight})
      extremes.append(over(blob, blob.alphaF(), stop));
  }
  return extremes;
}

// The least contrast that text of a colour has on a panel, wherever the panel is over the backdrop
double worstOnPanel(const Tokens &tokens, const QColor &foreground, qreal panelOpacity)
{
  double worst = 21;
  for (const auto &behind : backdropExtremes(tokens))
    worst = std::min(worst, contrastRatio(foreground, over(tokens.colors.surface, panelOpacity, behind)));
  return worst;
}

} // namespace

void DesignTokensTests::contrast_blackOnWhiteIs21_andARatioNeverDependsOnTheOrder()
{
  QCOMPARE(contrastRatio(Qt::black, Qt::white), 21.0);
  QCOMPARE(contrastRatio(Qt::white, Qt::black), 21.0);
  QCOMPARE(contrastRatio(QColor(0x40, 0x80, 0xC0), QColor(0x40, 0x80, 0xC0)), 1.0);
}

void DesignTokensTests::contrast_aKnownPair_isTheWcagValue()
{
  // #767676 on white is the lightest grey that WCAG AA allows for text: 4.54:1
  QVERIFY(qAbs(contrastRatio(QColor(0x76, 0x76, 0x76), Qt::white) - 4.54) < 0.01);
}

void DesignTokensTests::over_mixesTheLayerWithWhatIsBehindIt()
{
  QCOMPARE(over(Qt::white, 1.0, Qt::black), QColor(Qt::white));
  QCOMPARE(over(Qt::white, 0.0, Qt::black), QColor(Qt::black));
  QCOMPARE(over(Qt::white, 0.5, Qt::black).red(), 128);
}

void DesignTokensTests::ensureContrast_changesAColourOnlyAsFarAsItNeedsTo()
{
  const QColor white(Qt::white);
  // enough already: the same colour
  QCOMPARE(ensureContrast(QColor(0x20, 0x20, 0x20), white, 4.5), QColor(0x20, 0x20, 0x20));
  // too light for text on white: darker, and just enough
  const auto darker = ensureContrast(QColor(0xA0, 0xA0, 0xA0), white, 4.5);
  QVERIFY(contrastRatio(darker, white) >= 4.5);
  QVERIFY(darker.red() > 0x60);
  QVERIFY(darker.red() < 0xA0);
  // on a dark background it goes the other way
  const QColor dark(0x10, 0x10, 0x10);
  const auto lighter = ensureContrast(QColor(0x30, 0x30, 0x30), dark, 3.0);
  QVERIFY(contrastRatio(lighter, dark) >= 3.0);
  QVERIFY(lighter.red() > 0x30);
}

void DesignTokensTests::osColors_areTheOnesOfThePaletteAndEverythingElseIsReadable_data()
{
  QTest::addColumn<bool>("dark");
  QTest::newRow("light") << false;
  QTest::newRow("dark") << true;
}

void DesignTokensTests::osColors_areTheOnesOfThePaletteAndEverythingElseIsReadable()
{
  QFETCH(bool, dark);
  QPalette os;
  os.setColor(QPalette::Window, dark ? QColor(0x2B, 0x2B, 0x2B) : QColor(0xEC, 0xEC, 0xEC));
  os.setColor(QPalette::WindowText, dark ? QColor(0xEE, 0xEE, 0xEE) : QColor(0x1A, 0x1A, 0x1A));
  os.setColor(QPalette::Base, dark ? QColor(0x1E, 0x1E, 0x1E) : Qt::white);
  os.setColor(QPalette::Highlight, QColor(0x0A, 0x60, 0xFF));
  os.setColor(QPalette::HighlightedText, Qt::white);

  Look look;
  look.scheme = dark ? Scheme::Dark : Scheme::Light;
  look.osColors = true;
  const auto tokens = tokensFor(look, os);
  const auto &c = tokens.colors;
  QVERIFY(tokens.flatBackdrop);
  QCOMPARE(tokens.panelOpacity, 1.0);
  QCOMPARE(c.bg, os.color(QPalette::Window));
  QCOMPARE(c.surface, os.color(QPalette::Base));
  QCOMPARE(c.accent, os.color(QPalette::Highlight));
  // the edge of a control and the colour of a state are made readable on what the OS gives
  QVERIFY(contrastRatio(c.controlBorder, c.bg) >= 3.0);
  QVERIFY(contrastRatio(c.controlBorder, c.surface) >= 3.0);
  for (const auto &state : {c.success, c.warning, c.danger})
    QVERIFY(contrastRatio(state, c.surface) >= 4.5);
}

void DesignTokensTests::textPairs_meetTheContrastOfWcagAa_data()
{
  addGlassLooks();
}

void DesignTokensTests::textPairs_meetTheContrastOfWcagAa()
{
  QFETCH(bool, dark);
  const auto tokens = tokensFor(glass(dark ? Scheme::Dark : Scheme::Light));
  const auto &c = tokens.colors;

  // text on a panel that is as translucent as a panel gets (the sidebar, the status card), over any backdrop colour
  QVERIFY(worstOnPanel(tokens, c.text, tokens.panelOpacity) >= 4.5);
  QVERIFY(worstOnPanel(tokens, c.textSecondary, tokens.panelOpacity) >= 4.5);

  // text on the dense panels that carry settings rows, the log and dialogs
  for (const auto &foreground : {c.text, c.textSecondary, c.accent, c.success, c.danger, c.warning}) {
    const auto worst = worstOnPanel(tokens, foreground, tokens.panelDenseOpacity);
    QVERIFY2(worst >= 4.5, qPrintable(QStringLiteral("%1 on a dense panel: %2").arg(foreground.name()).arg(worst)));
  }

  QVERIFY(contrastRatio(c.onAccent, c.accent) >= 4.5);
  QVERIFY(contrastRatio(c.text, c.bg) >= 4.5);
  QVERIFY(contrastRatio(c.textSecondary, c.bg) >= 4.5);
}

void DesignTokensTests::controlBorder_meetsThreeToOneOnADensePanel_data()
{
  addGlassLooks();
}

void DesignTokensTests::controlBorder_meetsThreeToOneOnADensePanel()
{
  QFETCH(bool, dark);
  const auto tokens = tokensFor(glass(dark ? Scheme::Dark : Scheme::Light));
  // the boundary of an input or a button is what tells it from its surroundings: 3:1 (WCAG 1.4.11)
  const auto worst = worstOnPanel(tokens, tokens.colors.controlBorder, tokens.panelDenseOpacity);
  QVERIFY2(worst >= 3.0, qPrintable(QString::number(worst)));
  // and the same on the window and on the surface of a field
  QVERIFY(contrastRatio(tokens.colors.controlBorder, tokens.colors.bg) >= 3.0);
  QVERIFY(contrastRatio(tokens.colors.controlBorder, tokens.colors.surface) >= 3.0);
}

void DesignTokensTests::tints_carryTheirTextAtAaContrast_data()
{
  addGlassLooks();
}

void DesignTokensTests::tints_carryTheirTextAtAaContrast()
{
  QFETCH(bool, dark);
  const auto &c = tokensFor(glass(dark ? Scheme::Dark : Scheme::Light)).colors;
  QVERIFY(contrastRatio(c.success, c.successSoft) >= 4.5);
  QVERIFY(contrastRatio(c.warning, c.warningSoft) >= 4.5);
  QVERIFY(contrastRatio(c.danger, c.dangerSoft) >= 4.5);
  QVERIFY(contrastRatio(c.accent, c.accentSoft) >= 4.5);
}

void DesignTokensTests::opaqueLooks_haveNoGradientNoAlphaAndTheTextStaysReadable_data()
{
  QTest::addColumn<bool>("dark");
  QTest::addColumn<bool>("reduceTransparency");
  QTest::addColumn<bool>("highContrast");
  QTest::newRow("light, reduce transparency") << false << true << false;
  QTest::newRow("dark, reduce transparency") << true << true << false;
  QTest::newRow("light, high contrast") << false << false << true;
  QTest::newRow("dark, high contrast") << true << false << true;
}

void DesignTokensTests::opaqueLooks_haveNoGradientNoAlphaAndTheTextStaysReadable()
{
  QFETCH(bool, dark);
  QFETCH(bool, reduceTransparency);
  QFETCH(bool, highContrast);

  Look look = glass(dark ? Scheme::Dark : Scheme::Light);
  look.reduceTransparency = reduceTransparency;
  look.highContrast = highContrast;
  QVERIFY(look.opaque());

  const auto tokens = tokensFor(look, highContrastPalette(dark));
  QVERIFY(tokens.flatBackdrop);
  QCOMPARE(tokens.panelOpacity, 1.0);
  QCOMPARE(tokens.panelDenseOpacity, 1.0);
  // every panel is the surface colour itself: the contrast of its text does not depend on what is behind it
  QVERIFY(contrastRatio(tokens.colors.text, tokens.colors.surface) >= 4.5);
  QVERIFY(contrastRatio(tokens.colors.textSecondary, tokens.colors.surface) >= 4.5);
  QVERIFY(contrastRatio(tokens.colors.onAccent, tokens.colors.accent) >= 4.5);
}

void DesignTokensTests::highContrast_takesTheColoursOfTheOs()
{
  QPalette os;
  os.setColor(QPalette::Window, Qt::black);
  os.setColor(QPalette::WindowText, Qt::white);
  os.setColor(QPalette::Base, Qt::black);
  os.setColor(QPalette::Text, Qt::white);
  os.setColor(QPalette::Highlight, QColor(0x1A, 0xEB, 0xFF));
  os.setColor(QPalette::HighlightedText, Qt::black);

  Look look = glass(Scheme::Dark);
  look.highContrast = true;
  const auto &c = tokensFor(look, os).colors;
  QCOMPARE(c.bg, QColor(Qt::black));
  QCOMPARE(c.surface, QColor(Qt::black));
  QCOMPARE(c.text, QColor(Qt::white));
  QCOMPARE(c.accent, QColor(0x1A, 0xEB, 0xFF));
  QCOMPARE(c.onAccent, QColor(Qt::black));
  // the boundary of a control is as strong as the text
  QCOMPARE(c.controlBorder, QColor(Qt::white));
}

void DesignTokensTests::motion_isInstantWhenTheOsAsksForLess()
{
  Look look;
  QCOMPARE(motionDuration(look, metrics::motionSidebar), 200);
  look.reduceMotion = true;
  QCOMPARE(motionDuration(look, metrics::motionSidebar), 0);
}

void DesignTokensTests::palette_hasEveryRoleOfTheTokens_data()
{
  addGlassLooks();
}

void DesignTokensTests::palette_hasEveryRoleOfTheTokens()
{
  QFETCH(bool, dark);
  const auto look = glass(dark ? Scheme::Dark : Scheme::Light);
  const auto tokens = tokensFor(look);
  const auto palette = paletteFor(tokens, look);
  const auto &c = tokens.colors;

  QCOMPARE(palette.color(QPalette::Window), c.bg);
  QCOMPARE(palette.color(QPalette::WindowText), c.text);
  QCOMPARE(palette.color(QPalette::Base), c.surface);
  QCOMPARE(palette.color(QPalette::AlternateBase), c.surfaceAlt);
  QCOMPARE(palette.color(QPalette::Text), c.text);
  QCOMPARE(palette.color(QPalette::ButtonText), c.text);
  QCOMPARE(palette.color(QPalette::Highlight), c.accent);
  QCOMPARE(palette.color(QPalette::HighlightedText), c.onAccent);
  QCOMPARE(palette.color(QPalette::Link), c.accent);
  QCOMPARE(palette.color(QPalette::PlaceholderText), c.textSecondary);
  QCOMPARE(palette.color(QPalette::Disabled, QPalette::WindowText), c.textDisabled);
  QCOMPARE(palette.color(QPalette::Disabled, QPalette::Text), c.textDisabled);
}

void DesignTokensTests::palette_textIsReadableInEveryGroup_data()
{
  addGlassLooks();
}

void DesignTokensTests::palette_textIsReadableInEveryGroup()
{
  QFETCH(bool, dark);
  const auto look = glass(dark ? Scheme::Dark : Scheme::Light);
  const auto palette = paletteFor(tokensFor(look), look);

  for (const auto group : {QPalette::Active, QPalette::Inactive}) {
    QVERIFY(contrastRatio(palette.color(group, QPalette::WindowText), palette.color(group, QPalette::Window)) >= 4.5);
    QVERIFY(contrastRatio(palette.color(group, QPalette::Text), palette.color(group, QPalette::Base)) >= 4.5);
    QVERIFY(contrastRatio(palette.color(group, QPalette::ButtonText), palette.color(group, QPalette::Button)) >= 4.5);
    QVERIFY(
        contrastRatio(palette.color(group, QPalette::HighlightedText), palette.color(group, QPalette::Highlight)) >= 4.5
    );
    QVERIFY(contrastRatio(palette.color(group, QPalette::Link), palette.color(group, QPalette::Base)) >= 4.5);
  }
}

QTEST_MAIN(DesignTokensTests)
