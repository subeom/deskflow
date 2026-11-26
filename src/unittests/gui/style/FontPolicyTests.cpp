/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "FontPolicyTests.h"

#include "gui/style/FontPolicy.h"

#include <QScopeGuard>

using namespace deskflow::gui::style;

Q_DECLARE_METATYPE(TextPlatform)
Q_DECLARE_METATYPE(FontChoice::Source)
Q_DECLARE_METATYPE(QList<int>)

namespace {

// The weights that the faces of these families have on the OSes that ship them
QList<int> weightsOf(const QString &family)
{
  if (family == QLatin1String("Malgun Gothic"))
    return {400, 700};
  if (family == QLatin1String("Malgun Gothic Semilight"))
    return {350};
  if (family == QLatin1String("Microsoft YaHei UI") || family == QLatin1String("Microsoft YaHei"))
    return {300, 400, 700};
  if (family == QLatin1String("Apple SD Gothic Neo"))
    return {100, 200, 300, 400, 500, 600, 700, 800};
  if (family == QLatin1String("Segoe UI"))
    return {300, 350, 400, 600, 700, 900};
  return {};
}

FontPolicyInput input(
    TextPlatform platform, const QString &language, const QStringList &installed, const QString &system,
    const QString &variable = QString()
)
{
  FontPolicyInput in;
  in.platform = platform;
  in.language = language;
  in.systemFamily = system;
  in.variable = variable;
  in.hasFamily = [installed](const QString &family) { return installed.contains(family); };
  in.weightsOf = weightsOf;
  return in;
}

const QStringList kWindowsFonts{
    QStringLiteral("Segoe UI"),        QStringLiteral("Malgun Gothic"), QStringLiteral("Malgun Gothic Semilight"),
    QStringLiteral("Gulim"),           QStringLiteral("SimSun"),        QStringLiteral("Microsoft YaHei UI"),
    QStringLiteral("Microsoft YaHei"), QStringLiteral("Yu Gothic UI")
};
const QStringList kMacFonts{
    QStringLiteral("Apple SD Gothic Neo"), QStringLiteral("PingFang SC"), QStringLiteral("Hiragino Sans")
};

} // namespace

void FontPolicyTests::parseFamilyList_isTheFamiliesInTheirOrder_withoutSpacesQuotesEmptyOnesAndRepeats_data()
{
  QTest::addColumn<QString>("text");
  QTest::addColumn<QStringList>("families");

  QTest::newRow("empty") << QString() << QStringList{};
  QTest::newRow("blank") << QStringLiteral("  ") << QStringList{};
  QTest::newRow("commas only") << QStringLiteral(" , ,, ") << QStringList{};
  QTest::newRow("one") << QStringLiteral("Malgun Gothic") << QStringList{QStringLiteral("Malgun Gothic")};
  QTest::newRow("two, in order") << QStringLiteral("Segoe UI,Malgun Gothic")
                                 << QStringList{QStringLiteral("Segoe UI"), QStringLiteral("Malgun Gothic")};
  QTest::newRow("spaces and empty ones") << QStringLiteral("  Segoe UI , ,Malgun Gothic Semilight, ")
                                         << QStringList{
                                                QStringLiteral("Segoe UI"), QStringLiteral("Malgun Gothic Semilight")
                                            };
  QTest::newRow("quotes") << QStringLiteral("\"Segoe UI\", 'Noto Sans KR'")
                          << QStringList{QStringLiteral("Segoe UI"), QStringLiteral("Noto Sans KR")};
  QTest::newRow("a family twice is once") << QStringLiteral("Segoe UI, segoe ui, Malgun Gothic")
                                          << QStringList{QStringLiteral("Segoe UI"), QStringLiteral("Malgun Gothic")};
  QTest::newRow("a name with Hangul") << QStringLiteral("맑은 고딕") << QStringList{QStringLiteral("맑은 고딕")};
}

void FontPolicyTests::parseFamilyList_isTheFamiliesInTheirOrder_withoutSpacesQuotesEmptyOnesAndRepeats()
{
  QFETCH(QString, text);
  QFETCH(QStringList, families);
  QCOMPARE(parseFamilyList(text), families);
}

void FontPolicyTests::chooseFonts_followsThePlatformTheLanguageAndTheVariable_data()
{
  QTest::addColumn<TextPlatform>("platform");
  QTest::addColumn<QString>("language");
  QTest::addColumn<QStringList>("installed");
  QTest::addColumn<QString>("system");
  QTest::addColumn<QString>("variable");
  QTest::addColumn<QStringList>("families");
  QTest::addColumn<FontChoice::Source>("source");
  QTest::addColumn<QList<int>>("weights");

  using Source = FontChoice::Source;
  const auto windowsKorean = QStringList{QStringLiteral("Segoe UI"), QStringLiteral("Malgun Gothic")};
  // Windows, Korean: Malgun Gothic for the Hangul, Segoe UI for the rest, and the weights of Malgun Gothic
  QTest::newRow("Windows ko") << TextPlatform::Windows << QStringLiteral("ko") << kWindowsFonts
                              << QStringLiteral("Segoe UI") << QString() << windowsKorean << Source::Language
                              << QList<int>{400, 700};
  QTest::newRow("Windows ko_KR is Korean too")
      << TextPlatform::Windows << QStringLiteral("ko_KR") << kWindowsFonts << QStringLiteral("Segoe UI") << QString()
      << windowsKorean << Source::Language << QList<int>{400, 700};
  QTest::newRow("Windows ko without Malgun Gothic keeps the system")
      << TextPlatform::Windows << QStringLiteral("ko")
      << QStringList{QStringLiteral("Segoe UI"), QStringLiteral("Gulim")} << QStringLiteral("Segoe UI") << QString()
      << QStringList{} << Source::System << QList<int>{};
  QTest::newRow("Windows ko, system font not given")
      << TextPlatform::Windows << QStringLiteral("ko") << kWindowsFonts << QString() << QString()
      << QStringList{QStringLiteral("Malgun Gothic")} << Source::Language << QList<int>{400, 700};
  // Windows, Simplified Chinese: Microsoft YaHei UI (SimSun is what Qt looks for first), or the one without UI
  QTest::newRow("Windows zh_CN") << TextPlatform::Windows << QStringLiteral("zh_CN") << kWindowsFonts
                                 << QStringLiteral("Segoe UI") << QString()
                                 << QStringList{QStringLiteral("Segoe UI"), QStringLiteral("Microsoft YaHei UI")}
                                 << Source::Language << QList<int>{300, 400, 700};
  QTest::newRow("Windows zh_CN, only YaHei")
      << TextPlatform::Windows << QStringLiteral("zh_CN")
      << QStringList{QStringLiteral("Segoe UI"), QStringLiteral("Microsoft YaHei")} << QStringLiteral("Segoe UI")
      << QString() << QStringList{QStringLiteral("Segoe UI"), QStringLiteral("Microsoft YaHei")} << Source::Language
      << QList<int>{300, 400, 700};
  // Windows, Japanese: Qt looks for Yu Gothic UI first, which is the font of Windows: no choice
  QTest::newRow("Windows ja") << TextPlatform::Windows << QStringLiteral("ja") << kWindowsFonts
                              << QStringLiteral("Segoe UI") << QString() << QStringList{} << Source::System
                              << QList<int>{};
  // macOS, Korean: the system font for the Latin letters, Apple SD Gothic Neo for the Hangul (it has every weight)
  QTest::newRow("macOS ko") << TextPlatform::Mac << QStringLiteral("ko") << kMacFonts
                            << QStringLiteral(".AppleSystemUIFont") << QString()
                            << QStringList{QStringLiteral(".AppleSystemUIFont"), QStringLiteral("Apple SD Gothic Neo")}
                            << Source::Language << QList<int>{100, 200, 300, 400, 500, 600, 700, 800};
  QTest::newRow("macOS zh_CN and ja keep the cascade of the system")
      << TextPlatform::Mac << QStringLiteral("zh_CN") << kMacFonts << QStringLiteral(".AppleSystemUIFont") << QString()
      << QStringList{} << Source::System << QList<int>{};
  // Linux and the others: the fallback of the system (fontconfig) is the right one
  QTest::newRow("Linux ko") << TextPlatform::Linux << QStringLiteral("ko")
                            << QStringList{QStringLiteral("Noto Sans CJK KR")} << QStringLiteral("Noto Sans")
                            << QString() << QStringList{} << Source::System << QList<int>{};
  QTest::newRow("another system ko") << TextPlatform::Other << QStringLiteral("ko") << kWindowsFonts
                                     << QStringLiteral("Segoe UI") << QString() << QStringList{} << Source::System
                                     << QList<int>{};
  // The variable says the families, in every language and on every OS: the first is the Latin letters, the weights are
  // the ones of the last
  QTest::newRow(
      "variable, Windows ko"
  ) << TextPlatform::Windows
    << QStringLiteral("ko") << kWindowsFonts << QStringLiteral("Segoe UI")
    << QStringLiteral("Segoe UI, Malgun Gothic Semilight")
    << QStringList{QStringLiteral("Segoe UI"), QStringLiteral("Malgun Gothic Semilight")} << Source::Variable
    << QList<int>{350};
  QTest::newRow("variable, English") << TextPlatform::Windows << QStringLiteral("en") << kWindowsFonts
                                     << QStringLiteral("Segoe UI") << QStringLiteral("Malgun Gothic")
                                     << QStringList{QStringLiteral("Malgun Gothic")} << Source::Variable
                                     << QList<int>{400, 700};
  QTest::newRow("variable, a family that is not here is still asked for")
      << TextPlatform::Linux << QStringLiteral("ko") << QStringList{} << QStringLiteral("Noto Sans")
      << QStringLiteral("Pretendard") << QStringList{QStringLiteral("Pretendard")} << Source::Variable << QList<int>{};
  QTest::newRow("a variable with nothing in it is no choice")
      << TextPlatform::Windows << QStringLiteral("ko") << kWindowsFonts << QStringLiteral("Segoe UI")
      << QStringLiteral(" , ") << windowsKorean << Source::Language << QList<int>{400, 700};
}

void FontPolicyTests::chooseFonts_followsThePlatformTheLanguageAndTheVariable()
{
  QFETCH(TextPlatform, platform);
  QFETCH(QString, language);
  QFETCH(QStringList, installed);
  QFETCH(QString, system);
  QFETCH(QString, variable);
  QFETCH(QStringList, families);
  QFETCH(FontChoice::Source, source);
  QFETCH(QList<int>, weights);

  const auto choice = chooseFonts(input(platform, language, installed, system, variable));
  QCOMPARE(choice.families, families);
  QCOMPARE(choice.source, source);
  QCOMPARE(choice.weights, weights);
}

void FontPolicyTests::chooseFonts_neverTouchesEnglishOrTheOtherLanguages_whateverThePlatform()
{
  // Nothing is chosen for these: the font of the system and its own fallback stay byte for byte as they are
  const QList<TextPlatform> platforms{
      TextPlatform::Windows, TextPlatform::Mac, TextPlatform::Linux, TextPlatform::Other
  };
  for (const auto platform : platforms) {
    for (const QString &language :
         {QStringLiteral("en"), QString(), QStringLiteral("es"), QStringLiteral("it"), QStringLiteral("ru"),
          QStringLiteral("ja"), QStringLiteral("fr"), QStringLiteral("en_US"), QStringLiteral("zh_TW")}) {
      const auto choice = chooseFonts(input(platform, language, kWindowsFonts + kMacFonts, QStringLiteral("Segoe UI")));
      QVERIFY2(choice.families.isEmpty(), qPrintable(language));
      QVERIFY2(choice.weights.isEmpty(), qPrintable(language));
      QCOMPARE(choice.source, FontChoice::Source::System);
    }
  }
  // not even asked: without the means to ask whether a family is here there is no choice
  FontPolicyInput blind;
  blind.platform = TextPlatform::Windows;
  blind.language = QStringLiteral("ko");
  QVERIFY(chooseFonts(blind).families.isEmpty());
}

void FontPolicyTests::languageFontCandidates_areTheFontsThatTheOsShips_notOnesThatNeedAnInstall()
{
  // the fonts that Windows 11 and macOS ship (no font that a person has to install, and none that the program carries)
  QCOMPARE(
      languageFontCandidates(TextPlatform::Windows, QStringLiteral("ko")), QStringList{QStringLiteral("Malgun Gothic")}
  );
  QCOMPARE(
      languageFontCandidates(TextPlatform::Windows, QStringLiteral("zh_CN")),
      (QStringList{QStringLiteral("Microsoft YaHei UI"), QStringLiteral("Microsoft YaHei")})
  );
  QCOMPARE(
      languageFontCandidates(TextPlatform::Mac, QStringLiteral("ko")),
      QStringList{QStringLiteral("Apple SD Gothic Neo")}
  );
  // Gulim is the one that Qt has first for Korean: it is never chosen
  for (const auto platform : {TextPlatform::Windows, TextPlatform::Mac, TextPlatform::Linux, TextPlatform::Other}) {
    for (const auto &language : {QStringLiteral("ko"), QStringLiteral("zh_CN"), QStringLiteral("ja")})
      QVERIFY(!languageFontCandidates(platform, language).contains(QStringLiteral("Gulim")));
  }
}

void FontPolicyTests::snapWeight_isTheNearestWeightThatTheFontHas_data()
{
  QTest::addColumn<int>("requested");
  QTest::addColumn<QList<int>>("available");
  QTest::addColumn<int>("snapped");

  const QList<int> malgun{400, 700};
  QTest::newRow("Malgun Gothic, Regular") << 400 << malgun << 400;
  QTest::newRow("Malgun Gothic, Medium is Regular (lighter first up to 500)") << 500 << malgun << 400;
  QTest::newRow("Malgun Gothic, DemiBold is Bold") << 600 << malgun << 700;
  QTest::newRow("Malgun Gothic, Bold") << 700 << malgun << 700;
  QTest::newRow("Malgun Gothic, Light is Regular") << 300 << malgun << 400;
  QTest::newRow("Malgun Gothic, Black is Bold") << 900 << malgun << 700;
  const QList<int> neo{100, 200, 300, 400, 500, 600, 700, 800};
  for (const int weight : {400, 500, 600, 700})
    QTest::addRow("Apple SD Gothic Neo has %d", weight) << weight << neo << weight;
  QTest::newRow("a font with Medium and Bold: DemiBold is Bold") << 600 << QList<int>{400, 500, 700} << 700;
  QTest::newRow("a font with Regular and DemiBold: Medium is Regular") << 500 << QList<int>{400, 600} << 400;
  QTest::newRow("a font with only Medium and Bold: Regular is Medium") << 400 << QList<int>{500, 700} << 500;
  QTest::newRow("a font with one weight") << 600 << QList<int>{350} << 350;
  QTest::newRow("a font with Light only: Medium is Light") << 500 << QList<int>{300} << 300;
  QTest::newRow("the list does not have to be sorted") << 600 << QList<int>{700, 400} << 700;
  QTest::newRow("no weights known: as asked") << 600 << QList<int>{} << 600;
}

void FontPolicyTests::snapWeight_isTheNearestWeightThatTheFontHas()
{
  QFETCH(int, requested);
  QFETCH(QList<int>, available);
  QFETCH(int, snapped);
  QCOMPARE(snapWeight(requested, available), snapped);
}

void FontPolicyTests::fontWeight_isWhatIsAskedUntilTheFontsAreChosen()
{
  const auto guard = qScopeGuard([] { setFontWeights({}); });
  QVERIFY(fontWeights().isEmpty());
  QCOMPARE(fontWeight(QFont::Medium), QFont::Medium);
  QCOMPARE(fontWeight(QFont::DemiBold), QFont::DemiBold);
  QCOMPARE(fontWeight(QFont::Bold), QFont::Bold);

  setFontWeights({400, 700});
  QCOMPARE(fontWeight(QFont::Normal), QFont::Normal);
  QCOMPARE(fontWeight(QFont::Medium), QFont::Normal);
  QCOMPARE(fontWeight(QFont::DemiBold), QFont::Bold);
  QCOMPARE(fontWeight(QFont::Bold), QFont::Bold);

  setFontWeights({});
  QCOMPARE(fontWeight(QFont::DemiBold), QFont::DemiBold);
}

QTEST_APPLESS_MAIN(FontPolicyTests)
