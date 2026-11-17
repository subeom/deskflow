/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "DisplaySignatureTests.h"

#include "LayoutTestData.h"
#include "gui/layout/DisplaySignature.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QRegularExpression>

#include <algorithm>
#include <random>

using namespace layout_test;
using deskflow::gui::layout::DisplayInfo;
using deskflow::gui::layout::DisplaySignature;

Q_DECLARE_METATYPE(std::vector<DisplayInfo>)

void DisplaySignatureTests::identity_isTheUuid_thenTheNumbers_thenNothing()
{
  auto d = ultrawide();
  QCOMPARE(d.identity(), QStringLiteral("u:00000000-0000-4000-8000-0000000000b2"));

  d.uuid.clear();
  QCOMPARE(d.identity(), QStringLiteral("v:10ac:a0c4:aae61"));

  d.vendor = d.model = d.serial = 0;
  QVERIFY(d.identity().isEmpty());

  // a model that is known without a serial still says something
  d.vendor = 0x10AC;
  QVERIFY(!d.identity().isEmpty());
}

void DisplaySignatureTests::sameClass_isBuiltInResolutionAndScale()
{
  const auto a = ultrawide();
  QVERIFY(a.sameClassAs(otherUltrawide()));
  auto b = a;
  b.w = 2560;
  QVERIFY(!a.sameClassAs(b));
  b = a;
  b.h = 1080;
  QVERIFY(!a.sameClassAs(b));
  b = a;
  b.scalePercent = 200;
  QVERIFY(!a.sameClassAs(b));
  b = a;
  b.builtIn = true;
  QVERIFY(!a.sameClassAs(b));
  // where it is and which one it is do not matter for the class
  b = a;
  b.x = 99;
  b.uuid = QStringLiteral("other");
  b.primary = !a.primary;
  QVERIFY(a.sameClassAs(b));
}

void DisplaySignatureTests::sameDisplaysInAnyOrder_areTheSameSignature_data()
{
  QTest::addColumn<std::vector<DisplayInfo>>("displays");

  QTest::newRow("t") << tShape();
  QTest::newRow("t reversed") << std::vector<DisplayInfo>{builtIn(964, 1440), ultrawide(0, 0, true)};
  QTest::newRow("three") << std::vector<DisplayInfo>{
      fullHd(3440, 0), ultrawide(0, 0, true), builtIn(964, 1440)
  };
}

void DisplaySignatureTests::sameDisplaysInAnyOrder_areTheSameSignature()
{
  QFETCH(std::vector<DisplayInfo>, displays);
  const auto expected = DisplaySignature::fromDisplays(displays);
  QVERIFY(expected.isValid());

  auto shuffled = displays;
  std::mt19937 random(7);
  for (int i = 0; i < 20; ++i) {
    std::shuffle(shuffled.begin(), shuffled.end(), random);
    const auto actual = DisplaySignature::fromDisplays(shuffled);
    QCOMPARE(actual.key(), expected.key());
    QCOMPARE(actual.id(), expected.id());
    QVERIFY(actual == expected);
  }
}

void DisplaySignatureTests::aMirroredDisplay_isCountedOnce()
{
  // the same rectangle: the main one stays
  auto mirror = builtIn(0, 0, false);
  mirror.w = 3440;
  mirror.h = 1440;
  const auto signature = sig({mirror, ultrawide(0, 0, true)});
  QCOMPARE(signature.displays().size(), size_t(1));
  QVERIFY(signature.displays().front().primary);
  QCOMPARE(signature.displays().front().identity(), ultrawide().identity());

  // neither is the main one: the built in one stays
  auto other = ultrawide(0, 0, false);
  const auto second = sig({other, mirror});
  QCOMPARE(second.displays().size(), size_t(1));
  QVERIFY(second.displays().front().builtIn);

  // and whatever the order of the list, the same one stays
  QCOMPARE(sig({mirror, other}).key(), sig({other, mirror}).key());
}

void DisplaySignatureTests::displaysThatAreNotValid_areLeftOut()
{
  auto empty = ultrawide(0, 0, false);
  empty.w = 0;
  auto negative = fullHd(0, 0);
  negative.h = -5;
  const auto signature = sig({empty, negative, builtIn(0, 0, true)});
  QCOMPARE(signature.displays().size(), size_t(1));
  QVERIFY(signature.isValid());

  QVERIFY(!sig({empty, negative}).isValid());
  QVERIFY(!sig({}).isValid());
  QVERIFY(sig({}).key().isEmpty());
}

void DisplaySignatureTests::everyPartOfTheSituation_changesTheKey_data()
{
  QTest::addColumn<int>("what");
  QTest::newRow("identity") << 0;
  QTest::newRow("width") << 1;
  QTest::newRow("height") << 2;
  QTest::newRow("scale") << 3;
  QTest::newRow("built in") << 4;
  QTest::newRow("x") << 5;
  QTest::newRow("y") << 6;
  QTest::newRow("main display") << 7;
  QTest::newRow("another display") << 8;
}

void DisplaySignatureTests::everyPartOfTheSituation_changesTheKey()
{
  QFETCH(int, what);
  const auto base = sig(tShape());
  auto displays = tShape();
  auto &second = displays[1];
  switch (what) {
  case 0:
    second.uuid = QStringLiteral("something-else");
    break;
  case 1:
    second.w += 1;
    break;
  case 2:
    second.h += 1;
    break;
  case 3:
    second.scalePercent = 100;
    break;
  case 4:
    second.builtIn = false;
    break;
  case 5:
    second.x += 1;
    break;
  case 6:
    second.y += 1;
    break;
  case 7:
    displays[0].primary = false;
    second.primary = true;
    break;
  default:
    displays.push_back(fullHd(3440, 0));
    break;
  }
  const auto changed = sig(displays);
  QVERIFY(changed.key() != base.key());
  QVERIFY(changed.id() != base.id());
  QVERIFY(!(changed == base));
}

void DisplaySignatureTests::theLabel_isNotPartOfTheKey()
{
  auto displays = tShape();
  const auto before = sig(displays);
  displays[0].label = QStringLiteral("a name the system gave");
  displays[1].label.clear();
  QCOMPARE(sig(displays).key(), before.key());
}

void DisplaySignatureTests::theId_isShortStableAndHidesTheHardware()
{
  const auto signature = sig(tShape());
  QVERIFY(QRegularExpression(QStringLiteral("^[0-9a-f]{8}$")).match(signature.id()).hasMatch());
  // a value that was computed once by hand: the id of this key does not change from one build to the next
  QCOMPARE(signature.key(), QStringLiteral("u:00000000-0000-4000-8000-0000000000b2|e3440x1440@100|0,0|p;"
                                          "u:00000000-0000-4000-8000-0000000000a1|b1512x982@200|964,1440|-"));
  QCOMPARE(signature.id(), QStringLiteral("ba4a8180"));
  QVERIFY(!signature.id().contains(QStringLiteral("700001")));
  QVERIFY(sig({}).id().isEmpty());
}

void DisplaySignatureTests::moreThanTheMaximum_isCutTheSameWayWhateverTheOrder()
{
  std::vector<DisplayInfo> displays;
  for (int i = 0; i < 20; ++i)
    displays.push_back(anonymous(i * 100, 0, 100, 100, i == 0));
  const auto signature = sig(displays);
  QCOMPARE(signature.displays().size(), size_t(DisplaySignature::kMaxDisplays));
  QVERIFY(signature.truncated());
  QVERIFY(!sig(tShape()).truncated());

  std::reverse(displays.begin(), displays.end());
  QCOMPARE(sig(displays).key(), signature.key());
}

void DisplaySignatureTests::monitors_areTheRectanglesOfTheDisplays()
{
  const auto monitors = sig({builtIn(964, 1440), ultrawide(0, 0, true)}).monitors();
  QCOMPARE(monitors.size(), size_t(2));
  QVERIFY((monitors[0] == deskflow::server::MonitorRect{0, 0, 3440, 1440}));
  QVERIFY((monitors[1] == deskflow::server::MonitorRect{964, 1440, 1512, 982}));
}

void DisplaySignatureTests::summary_describesTheSituationWithoutTheHardwareIds()
{
  const auto summary = sig(tShape()).summary();
  QCOMPARE(
      summary, QStringLiteral("external 3440x1440@100% at 0,0 (main) [10ac:a0c4] + built-in 1512x982@200% at 964,1440 "
                              "[0610:a050]")
  );
  QVERIFY(!summary.contains(QStringLiteral("700001")));
  QVERIFY(!summary.contains(QStringLiteral("0000-4000")));
  // a display that says nothing about itself, and one whose scale is not known
  auto d = anonymous(0, 0, 1280, 720, true);
  d.scalePercent = 0;
  QCOMPARE(sig({d}).summary(), QStringLiteral("external 1280x720@? at 0,0 (main)"));
}

void DisplaySignatureTests::json_roundTrips()
{
  auto displays = tShape();
  displays.push_back(anonymous(3440, 0, 800, 600));
  displays.back().label = QStringLiteral("x");
  const auto signature = sig(displays);
  const auto json = signature.toJson();

  const auto back = DisplaySignature::fromJson(json);
  QVERIFY(back.has_value());
  QCOMPARE(back->key(), signature.key());
  QCOMPARE(back->displays().size(), signature.displays().size());
  // what is only for people is kept
  QCOMPARE(back->displays()[0].label, signature.displays()[0].label);

  // through the text as well
  const auto text = QJsonDocument(json).toJson(QJsonDocument::Compact);
  const auto parsed = DisplaySignature::fromJson(QJsonDocument::fromJson(text).object());
  QVERIFY(parsed.has_value());
  QCOMPARE(parsed->key(), signature.key());
}

void DisplaySignatureTests::json_refusesWhatIsNotASignature_data()
{
  QTest::addColumn<QJsonObject>("object");

  const auto good = sig(tShape()).toJson();
  const auto displayAt = [&](int i) { return good.value(QStringLiteral("displays")).toArray().at(i).toObject(); };
  const auto withDisplay = [&](const QJsonObject &d) {
    return QJsonObject{{QStringLiteral("displays"), QJsonArray{d}}};
  };
  const auto changed = [&](const QString &key, const QJsonValue &value) {
    auto d = displayAt(0);
    d.insert(key, value);
    return withDisplay(d);
  };

  QTest::newRow("nothing") << QJsonObject();
  QTest::newRow("no list") << QJsonObject{{QStringLiteral("displays"), 5}};
  QTest::newRow("empty list") << QJsonObject{{QStringLiteral("displays"), QJsonArray()}};
  QTest::newRow("not an object") << QJsonObject{{QStringLiteral("displays"), QJsonArray{1}}};
  QJsonArray many;
  for (int i = 0; i < 17; ++i)
    many.append(displayAt(0));
  QTest::newRow("17 displays") << QJsonObject{{QStringLiteral("displays"), many}};
  QTest::newRow("no width") << changed(QStringLiteral("w"), 0);
  QTest::newRow("huge width") << changed(QStringLiteral("w"), 1.0e9);
  QTest::newRow("width is text") << changed(QStringLiteral("w"), QStringLiteral("5"));
  QTest::newRow("far position") << changed(QStringLiteral("x"), 1.0e12);
  QTest::newRow("fractional position") << changed(QStringLiteral("y"), 0.5);
  QTest::newRow("uuid is a number") << changed(QStringLiteral("uuid"), 5);
  QTest::newRow("uuid is long") << changed(QStringLiteral("uuid"), QString(500, QLatin1Char('a')));
  QTest::newRow("negative vendor") << changed(QStringLiteral("vendor"), -1);
  QTest::newRow("vendor too big") << changed(QStringLiteral("vendor"), 5000000000.0);
  QTest::newRow("scale too big") << changed(QStringLiteral("scale"), 5000);
  QTest::newRow("built in is a number") << changed(QStringLiteral("builtIn"), 1);
}

void DisplaySignatureTests::json_refusesWhatIsNotASignature()
{
  QFETCH(QJsonObject, object);
  QVERIFY(!DisplaySignature::fromJson(object).has_value());
}

void DisplaySignatureTests::key_isTheSameWhateverTheLocale()
{
  const auto expected = sig(tShape()).key();
  const auto previous = QLocale();
  QLocale::setDefault(QLocale(QLocale::Arabic, QLocale::Egypt));
  QCOMPARE(sig(tShape()).key(), expected);
  QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
  QCOMPARE(sig(tShape()).key(), expected);
  QLocale::setDefault(previous);
}

QTEST_MAIN(DisplaySignatureTests)
