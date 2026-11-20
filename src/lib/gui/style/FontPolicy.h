/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "TextSizeModel.h"

#include <QFont>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

namespace deskflow::gui::style {

//! The environment variable that chooses the fonts of the new look (a trial: it is not in the settings). A list of
//! families, separated by commas, in the order that text is looked up in: the first has the Latin letters, a later one
//! has the letters that the ones before it do not have (Hangul). Empty or not set: the choice of the program.
inline constexpr auto kFontFamiliesVariable = "DESKFLOW_FONT_FAMILIES";

/*!
 * What the choice of the fonts depends on, as plain values: it is made without a screen, so that the rules can be
 * tested. TextSizeEnvironment gives the real ones.
 */
struct FontPolicyInput
{
  TextPlatform platform = TextPlatform::Other;
  //! The language of the program (the 639 name that the translations have: ko, ja, zh_CN, en ...)
  QString language;
  //! The family of the font that the system gives text: it keeps the Latin letters
  QString systemFamily;
  //! The value of kFontFamiliesVariable
  QString variable;
  //! Whether the computer has a family of fonts
  std::function<bool(const QString &)> hasFamily;
  //! The weights that the faces of a family have, as QFont::Weight values (empty where the computer cannot say)
  std::function<QList<int>(const QString &)> weightsOf;
};

struct FontChoice
{
  enum class Source
  {
    //! No choice: the font of the system as it is (its own fallback for the letters that it does not have)
    System,
    //! The language of the program asks for a font that the OS ships for it
    Language,
    //! The person chose the families (kFontFamiliesVariable)
    Variable
  };

  Source source = Source::System;
  //! The families in the order that text is looked up in; empty for System
  QStringList families;
  //! The weights that the last family has: a role that asks for another weight gets the nearest of these
  //! (fontWeight()), so that no face is made up by the font engine and the letters of the two scripts of a line have
  //! the same weight. Empty: every weight as it was asked for.
  QList<int> weights;
};

//! "A, B , 'C D'" is A, B, C D: trimmed, without quotes, without empty ones and without a family twice
QStringList parseFamilyList(const QString &text);

/*!
 * The fonts that the OS ships for the letters that the font of the system does not have, for a language of the program;
 * the first of them that the computer has is used. Only where the fallback of Qt on that OS is not the font that the OS
 * itself uses for the language:
 *  - Korean on Windows: Qt looks for letters that Segoe UI does not have in a list of its own that begins with Gulim
 *    (qwindowsfontdatabasebase.cpp, kr_tryFonts), an old font with one weight and thin strokes, whose bold is made up
 *    by the engine. The font of Windows for Hangul is Malgun Gothic (Regular and Bold).
 *  - Simplified Chinese on Windows: the same list begins with SimSun; the font of Windows is Microsoft YaHei UI.
 *  - Korean on macOS: the cascade of the system font reaches Apple SD Gothic Neo with the weight Regular for a role
 *    that asks for Medium or Semibold (Hangul lighter than the Latin letters beside it); the family of its own, with
 * the system font for the Latin letters, has every weight. Japanese is the font of Windows already (Qt's list begins
 * with Yu Gothic UI), and Linux and the other languages keep the fallback of their system (fontconfig, CoreText), so
 * they have no entry.
 */
QStringList languageFontCandidates(TextPlatform platform, const QString &language);

//! The fonts for an environment
FontChoice chooseFonts(const FontPolicyInput &input);

//! The weight of \p available that a role that asks for \p requested gets (the rule of CSS: lighter before heavier for
//! 500 and under, heavier before lighter above it); what was asked for when \p available is empty or has it
int snapWeight(int requested, const QList<int> &available);

//! The weights that the roles of the look get (set by the service of the size of text, empty with the look of the
//! system)
void setFontWeights(const QList<int> &weights);
QList<int> fontWeights();

//! The weight that a role gets: what it asks for, or the nearest weight that the font of the application has
QFont::Weight fontWeight(QFont::Weight requested);

} // namespace deskflow::gui::style
