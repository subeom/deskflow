/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CanvasThemeTests.h"

#include "gui/style/Appearance.h"
#include "gui/style/CanvasTheme.h"
#include "gui/style/Contrast.h"
#include "gui/style/DeskflowStyle.h"

#include <QApplication>

#include <algorithm>

using namespace deskflow::gui::style;

namespace {

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

CanvasTheme themeOf(const QString &which)
{
  Look look;
  if (which == QLatin1String("dark")) {
    look.scheme = Scheme::Dark;
  } else if (which.startsWith(QLatin1String("high contrast"))) {
    look.highContrast = true;
    look.scheme = which.endsWith(QLatin1String("dark")) ? Scheme::Dark : Scheme::Light;
    return canvasThemeFor(tokensFor(look, highContrastPalette(look.scheme == Scheme::Dark)), look);
  } else if (which == QLatin1String("less transparency")) {
    look.reduceTransparency = true;
  }
  return canvasThemeFor(tokensFor(look), look);
}

void addLooks()
{
  QTest::addColumn<QString>("which");
  for (const auto *name : {"light", "dark", "less transparency", "high contrast light", "high contrast dark"})
    QTest::newRow(name) << QString::fromLatin1(name);
}

} // namespace

void CanvasThemeTests::text_isReadableOnEveryFill_data()
{
  addLooks();
}

void CanvasThemeTests::text_isReadableOnEveryFill()
{
  QFETCH(QString, which);
  const auto theme = themeOf(which);
  for (const auto &fill : {theme.fill, theme.fillThis}) {
    for (const auto &text : {theme.text, theme.textSecondary}) {
      const auto ratio = contrastRatio(text, fill);
      QVERIFY2(ratio >= 4.5, qPrintable(QStringLiteral("%1 on %2 is %3:1").arg(text.name(), fill.name()).arg(ratio)));
    }
  }
  const auto bubble = contrastRatio(theme.bubbleText, theme.bubble);
  QVERIFY2(bubble >= 4.5, qPrintable(QString::number(bubble)));
}

void CanvasThemeTests::marks_areSeenAgainstWhatIsBehindThem_data()
{
  addLooks();
}

// An edge, a ring, a mark: 3:1 against the canvas and against the fill that it is on or next to
void CanvasThemeTests::marks_areSeenAgainstWhatIsBehindThem()
{
  QFETCH(QString, which);
  const auto theme = themeOf(which);
  struct Mark
  {
    const char *name;
    QColor color;
    QList<QColor> against;
  };
  const QList<Mark> marks = {
      {"frame", theme.frame, {theme.background}},
      {"border", theme.border, {theme.background, theme.fill}},
      {"border of this computer", theme.borderThis, {theme.background, theme.fillThis}},
      {"selected", theme.selected, {theme.background, theme.fill, theme.fillThis}},
      {"focus", theme.focus, {theme.background, theme.fill, theme.fillThis}},
      {"link", theme.link, {theme.background, theme.fill, theme.fillThis}},
      {"error", theme.error, {theme.background, theme.fill, theme.fillThis}},
      {"connected", theme.connected, {theme.fill, theme.fillThis}},
      {"offline", theme.offline, {theme.fill, theme.fillThis}},
      {"guide", theme.guide, {theme.background}},
  };
  for (const auto &mark : marks) {
    for (const auto &behind : mark.against) {
      const auto ratio = contrastRatio(mark.color, behind);
      QVERIFY2(
          ratio >= 3.0, qPrintable(QStringLiteral("%1 (%2) on %3 is %4:1").arg(mark.name, mark.color.name(), behind.name()).arg(ratio))
      );
    }
  }
}

void CanvasThemeTests::colour_meansFourThingsOnly_data()
{
  QTest::addColumn<bool>("dark");
  QTest::newRow("light") << false;
  QTest::newRow("dark") << true;
}

// Neutral fills and edges for the computers (so that colour that there is can be found); the colour of the rest is the
// colour of what it tells: this computer and the selection are the accent, a connection is the success colour, what is
// wrong is the danger colour
void CanvasThemeTests::colour_meansFourThingsOnly()
{
  QFETCH(bool, dark);
  Look look;
  look.scheme = dark ? Scheme::Dark : Scheme::Light;
  const auto tokens = tokensFor(look);
  const auto &c = tokens.colors;
  const auto theme = canvasThemeFor(tokens, look);

  QCOMPARE(theme.fill, c.surface);
  QCOMPARE(theme.border, c.controlBorder);
  QCOMPARE(theme.fillThis, c.accentSoft);
  QCOMPARE(theme.text, c.text);
  // a computer is not a colour of its own: no fill, border or text that has a hue that is another one's (a tint, not a
  // colour: the spread of its channels is small)
  for (const auto &neutral : {theme.fill, theme.border, theme.text, theme.textSecondary, theme.offline}) {
    const auto spread = std::max({neutral.red(), neutral.green(), neutral.blue()}) -
                        std::min({neutral.red(), neutral.green(), neutral.blue()});
    QVERIFY2(spread <= 80, qPrintable(neutral.name()));
  }
  // the colour that tells something is the colour of the look for it (kept at the contrast that is asked)
  QVERIFY(theme.selected.hueF() >= 0);
  QVERIFY2(qAbs(theme.selected.hueF() - c.accent.hueF()) < 0.02, qPrintable(theme.selected.name()));
  QVERIFY2(qAbs(theme.connected.hueF() - c.success.hueF()) < 0.03, qPrintable(theme.connected.name()));
  QVERIFY2(qAbs(theme.error.hueF() - c.danger.hueF()) < 0.03, qPrintable(theme.error.name()));
  // the pink of the guides of the old canvas is gone: it said nothing
  QVERIFY(qAbs(theme.guide.hueF() - QColor(226, 42, 142).hueF()) > 0.1);
}

void CanvasThemeTests::highContrast_hasNoDots_data()
{
  QTest::addColumn<bool>("highContrast");
  QTest::newRow("glass") << false;
  QTest::newRow("high contrast") << true;
}

void CanvasThemeTests::highContrast_hasNoDots()
{
  QFETCH(bool, highContrast);
  Look look;
  look.highContrast = highContrast;
  const auto theme = canvasThemeFor(tokensFor(look, highContrastPalette(false)), look);
  QCOMPARE(theme.showDots, !highContrast);
}

void CanvasThemeTests::current_isTheThemeOfTheLookThatIsPaintedWith()
{
  auto *style = new DeskflowStyle;
  QApplication::setStyle(style);
  Look look;
  look.scheme = Scheme::Dark;
  look.reduceTransparency = true;
  style->useLook(look);
  QCOMPARE(currentCanvasTheme().fill, canvasThemeFor(tokensFor(look), look).fill);
  QCOMPARE(currentCanvasTheme().fill, style->tokens().colors.surface);
  QApplication::setStyle(QStringLiteral("Fusion"));
  // no style layer: the colours of the OS palette, flat
  QCOMPARE(currentCanvasTheme().fill, currentTokens().colors.surface);
}

QTEST_MAIN(CanvasThemeTests)
