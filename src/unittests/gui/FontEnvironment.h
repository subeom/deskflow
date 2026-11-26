/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QFont>
#include <QFontInfo>
#include <QGuiApplication>
#include <QScreen>

// What the tests of the layout need to know of the fonts of the platform that they run on. The offscreen platform of Qt
// on Windows has no folder of fonts (Qt no longer ships them): its font database is empty, every font is the box engine
// of Qt, QFontInfo says -1 for every size, and every letter is as wide as it is high. The tests are run there with
// QT_QPA_FONTDIR set to a folder of fonts (Qt reads *.ttf, *.otf, *.pfa and *.pfb from it), or with the platform of the
// system.
namespace testfonts {

//! The size in pixels that a font has: what the font database matched (QFontInfo), and where the database cannot say
//! (-1) the size that the font was asked for
inline int fontPixels(const QFont &font)
{
  if (const int matched = QFontInfo(font).pixelSize(); matched > 0)
    return matched;
  if (font.pixelSize() > 0)
    return font.pixelSize();
  const auto *screen = QGuiApplication::primaryScreen();
  const qreal dpi = screen ? screen->logicalDotsPerInchY() : 96.0;
  return qRound(font.pointSizeF() * dpi / 72.0);
}

//! Whether the font database of the platform has fonts that can be measured (for the font that is asked about, the font
//! of the application by default): false where the font is the box engine, as every font is where the database is empty
inline bool fontDatabaseAnswers(const QFont &font = QGuiApplication::font())
{
  return QFontInfo(font).pixelSize() > 0;
}

} // namespace testfonts

//! The tests that measure the width of text are meaningless without fonts: they are not run (and say why), they are not
//! failed. A run of the tests on Windows is to have none of them skipped for this reason (see the runbook)
#define SKIP_WITHOUT_FONTS()                                                                                           \
  do {                                                                                                                 \
    if (!testfonts::fontDatabaseAnswers())                                                                             \
      QSKIP(                                                                                                           \
          "the font database is empty: every font is a box (offscreen on Windows has no folder of fonts). Set "        \
          "QT_QPA_FONTDIR to a folder of fonts or use the platform of the system"                                      \
      );                                                                                                               \
  } while (false)
