/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "LayoutStoreTests.h"

#include "LayoutTestData.h"
#include "gui/layout/Layout.h"
#include "gui/layout/LayoutProfileStore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

#include <random>

using namespace layout_test;
using namespace deskflow::gui::layout;

namespace {

PlacedComputer pc(const QString &name, double x, double y, double zoom = 1.0, bool server = false)
{
  return {name, server, x, y, zoom};
}

// the server in the middle, a PC on each side, and the Mac mini below
Layout tLayout()
{
  return {{pc("srv", 0, 0, 1.0, true), pc("PC1", -1920, 0), pc("PC2", 3440, 0, 0.8), pc("MINI", 964, 1440)}};
}

Layout otherLayout()
{
  return {{pc("srv", 0, 0, 1.0, true), pc("PC1", 0, -1080), pc("PC2", 1512, 0, 0.8), pc("MINI", 0, 982)}};
}

// the n-th signature that nothing else is: one display that is n wide
DisplaySignature signatureNumber(int n)
{
  return sig({anonymous(0, 0, 1000 + n, 800, true)});
}

QByteArray readFile(const QString &path)
{
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

void writeFile(const QString &path, const QByteArray &bytes)
{
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  file.write(bytes);
}

QJsonObject readJson(const QString &path)
{
  return QJsonDocument::fromJson(readFile(path)).object();
}

void writeJson(const QString &path, const QJsonObject &object)
{
  writeFile(path, QJsonDocument(object).toJson(QJsonDocument::Compact));
}

// a store of three profiles, saved
void makeThree(const QString &path)
{
  LayoutProfileStore store(path);
  store.load();
  store.upsert(sig(tShape()), tLayout(), 1000);
  store.upsert(sig(laptopOnly()), otherLayout(), 2000, QStringLiteral("On the move"), true);
  store.upsert(signatureNumber(1), tLayout(), 3000);
  QVERIFY(store.save());
}

} // namespace

void LayoutStoreTests::init()
{
  QVERIFY(m_dir.isValid());
  // whatever a test left in the folder
  QDir dir(m_dir.path());
  for (const auto &name : dir.entryList(QDir::Files | QDir::Hidden))
    dir.remove(name);
}

void LayoutStoreTests::layout_roundTripsThroughJson()
{
  const auto layout = tLayout();
  const auto json = layout.toJson();
  const auto back = Layout::fromJson(json);
  QVERIFY(back.has_value());
  QCOMPARE(back->computers.size(), layout.computers.size());
  QVERIFY(back->sameAs(layout));
  QVERIFY(back->computers[0].isServer);
  QCOMPARE(back->computers[2].zoom, 0.8);

  // through the text, with places that have many decimals
  Layout fine{{pc("A", 1706.6666666666667, -12.5, 2.0 / 3.0)}};
  const auto text = QJsonDocument(fine.toJson()).toJson(QJsonDocument::Compact);
  const auto parsed = Layout::fromJson(QJsonDocument::fromJson(text).object());
  QVERIFY(parsed.has_value());
  QCOMPARE(parsed->computers[0].x, fine.computers[0].x);
  QCOMPARE(parsed->computers[0].zoom, fine.computers[0].zoom);
}

void LayoutStoreTests::layout_find_ignoresTheCase()
{
  const auto layout = tLayout();
  QVERIFY(layout.find(QStringLiteral("pc1")) != nullptr);
  QVERIFY(layout.find(QStringLiteral("PC1")) != nullptr);
  QVERIFY(layout.find(QStringLiteral("nobody")) == nullptr);
  QVERIFY(layout.find(QString()) == nullptr);
}

void LayoutStoreTests::layout_sameAs_ignoresTheOrder_andAMillionth()
{
  auto a = tLayout();
  auto b = tLayout();
  std::reverse(b.computers.begin(), b.computers.end());
  QVERIFY(a.sameAs(b));

  b.computers[0].x += 5e-7;
  QVERIFY(a.sameAs(b));
  b.computers[0].x += 1e-5;
  QVERIFY(!a.sameAs(b));

  b = tLayout();
  b.computers[1].zoom = 0.9;
  QVERIFY(!a.sameAs(b));
  b = tLayout();
  b.computers.pop_back();
  QVERIFY(!a.sameAs(b));
  // a computer by another name, and the case of a name
  b = tLayout();
  b.computers[1].name = QStringLiteral("pc1");
  QVERIFY(a.sameAs(b));
  b.computers[1].name = QStringLiteral("PC9");
  QVERIFY(!a.sameAs(b));
  // the server is a computer that is the server
  b = tLayout();
  b.computers[0].isServer = false;
  QVERIFY(!a.sameAs(b));
}

void LayoutStoreTests::layout_refusesWhatIsNotALayout_data()
{
  QTest::addColumn<QJsonObject>("object");

  const auto entry = [](const QJsonObject &changes) {
    QJsonObject e{{QStringLiteral("name"), QStringLiteral("A")}, {QStringLiteral("server"), false},
                  {QStringLiteral("x"), 0}, {QStringLiteral("y"), 0}, {QStringLiteral("zoom"), 1.0}};
    for (auto it = changes.begin(); it != changes.end(); ++it)
      e.insert(it.key(), it.value());
    return e;
  };
  const auto one = [](const QJsonObject &e) { return QJsonObject{{QStringLiteral("computers"), QJsonArray{e}}}; };

  QTest::newRow("nothing") << QJsonObject();
  QTest::newRow("not a list") << QJsonObject{{QStringLiteral("computers"), 3}};
  QTest::newRow("empty list") << QJsonObject{{QStringLiteral("computers"), QJsonArray()}};
  QTest::newRow("not an object") << QJsonObject{{QStringLiteral("computers"), QJsonArray{1}}};
  QJsonArray many;
  for (int i = 0; i < 33; ++i)
    many.append(entry({{QStringLiteral("name"), QStringLiteral("c%1").arg(i)}}));
  QTest::newRow("33 computers") << QJsonObject{{QStringLiteral("computers"), many}};
  QTest::newRow("no name") << one(entry({{QStringLiteral("name"), QString()}}));
  QTest::newRow("name is a number") << one(entry({{QStringLiteral("name"), 5}}));
  QTest::newRow("name is long") << one(entry({{QStringLiteral("name"), QString(300, QLatin1Char('n'))}}));
  QTest::newRow("far x") << one(entry({{QStringLiteral("x"), 2.0e6}}));
  QTest::newRow("x is text") << one(entry({{QStringLiteral("x"), QStringLiteral("3")}}));
  QTest::newRow("y is missing") << one(QJsonObject{{QStringLiteral("name"), QStringLiteral("A")}, {QStringLiteral("x"), 0}});
  QTest::newRow("zoom is zero") << one(entry({{QStringLiteral("zoom"), 0}}));
  QTest::newRow("zoom is huge") << one(entry({{QStringLiteral("zoom"), 100}}));
  QTest::newRow("zoom is text") << one(entry({{QStringLiteral("zoom"), QStringLiteral("1")}}));
  QTest::newRow("server is a number") << one(entry({{QStringLiteral("server"), 1}}));
  QTest::newRow("the same name twice") << QJsonObject{
      {QStringLiteral("computers"), QJsonArray{entry({}), entry({{QStringLiteral("name"), QStringLiteral("a")}})}}
  };
}

void LayoutStoreTests::layout_refusesWhatIsNotALayout()
{
  QFETCH(QJsonObject, object);
  QVERIFY(!Layout::fromJson(object).has_value());
}

void LayoutStoreTests::store_aMissingFile_isAnEmptyStore()
{
  LayoutProfileStore store(path());
  const auto report = store.load();
  QCOMPARE(report.status, LayoutProfileStore::Status::Missing);
  QVERIFY(store.profiles().empty());
  QVERIFY(!store.isInert());
  QVERIFY(!store.active().has_value());
  QVERIFY(!QFile::exists(path()));
}

void LayoutStoreTests::upsert_makesOneProfileForEachSignature()
{
  LayoutProfileStore store(path());
  const auto first = store.upsert(sig(tShape()), tLayout(), 1000);
  QVERIFY(first.created);
  QVERIFY(!first.id.isEmpty());
  QCOMPARE(store.profiles().size(), size_t(1));

  // the same displays in another order are the same situation
  const auto again = store.upsert(sig({builtIn(964, 1440), ultrawide(0, 0, true)}), otherLayout(), 2000);
  QVERIFY(!again.created);
  QCOMPARE(again.id, first.id);
  QVERIFY(again.changed);
  QCOMPARE(store.profiles().size(), size_t(1));

  const auto other = store.upsert(sig(laptopOnly()), tLayout(), 3000);
  QVERIFY(other.created);
  QVERIFY(other.id != first.id);
  QCOMPARE(store.profiles().size(), size_t(2));

  const auto *found = store.findExact(sig(tShape()));
  QVERIFY(found != nullptr);
  QCOMPARE(found->id, first.id);
  QVERIFY(store.findExact(sig({fullHd(0, 0, true)})) == nullptr);
  QVERIFY(store.findExact(DisplaySignature()) == nullptr);
  QVERIFY(store.find(first.id) == found);
  QVERIFY(store.find(QStringLiteral("nope")) == nullptr);
}

void LayoutStoreTests::upsert_changesTheLayoutOnlyWhenItIsAnother()
{
  LayoutProfileStore store(path());
  const auto id = store.upsert(sig(tShape()), tLayout(), 1000).id;
  const auto &created = *store.find(id);
  QCOMPARE(created.createdAt, int64_t(1000));
  QCOMPARE(created.updatedAt, int64_t(1000));
  QCOMPARE(created.lastUsedAt, int64_t(1000));

  // the same layout: used, not updated
  const auto same = store.upsert(sig(tShape()), tLayout(), 5000);
  QVERIFY(!same.created);
  QVERIFY(!same.changed);
  QCOMPARE(store.find(id)->updatedAt, int64_t(1000));
  QCOMPARE(store.find(id)->lastUsedAt, int64_t(5000));
  QCOMPARE(store.find(id)->createdAt, int64_t(1000));

  const auto changed = store.upsert(sig(tShape()), otherLayout(), 6000);
  QVERIFY(changed.changed);
  QCOMPARE(store.find(id)->updatedAt, int64_t(6000));
  QVERIFY(store.find(id)->layout.sameAs(otherLayout()));
}

void LayoutStoreTests::upsert_refusesWhatIsNotValid()
{
  LayoutProfileStore store(path());
  QVERIFY(store.upsert(DisplaySignature(), tLayout(), 1).id.isEmpty());
  QVERIFY(store.upsert(sig(tShape()), Layout(), 1).id.isEmpty());
  QVERIFY(store.profiles().empty());
}

void LayoutStoreTests::upsert_namesAProfile_orSuggestsTheName()
{
  LayoutProfileStore store(path());
  const auto suggested = store.upsert(sig(tShape()), tLayout(), 1);
  QCOMPARE(store.find(suggested.id)->name, LayoutProfileStore::suggestName(sig(tShape())));
  QVERIFY(!store.find(suggested.id)->userNamed);

  const auto named = store.upsert(sig(laptopOnly()), tLayout(), 2, QStringLiteral("  On the move "), true);
  QCOMPARE(store.find(named.id)->name, QStringLiteral("On the move"));
  QVERIFY(store.find(named.id)->userNamed);

  // an update does not rename
  store.upsert(sig(laptopOnly()), otherLayout(), 3, QStringLiteral("Another"), true);
  QCOMPARE(store.find(named.id)->name, QStringLiteral("On the move"));

  // a name that is empty is the suggestion
  const auto empty = store.upsert(signatureNumber(3), tLayout(), 4, QStringLiteral("   "), true);
  QCOMPARE(store.find(empty.id)->name, LayoutProfileStore::suggestName(signatureNumber(3)));
}

void LayoutStoreTests::suggestName_listsTheDisplays()
{
  QCOMPARE(LayoutProfileStore::suggestName(sig(tShape())), QStringLiteral("external 3440x1440 + built-in 1512x982"));
  QCOMPARE(LayoutProfileStore::suggestName(sig(laptopOnly())), QStringLiteral("built-in 1512x982"));
  QCOMPARE(LayoutProfileStore::suggestName(DisplaySignature()), QStringLiteral("no displays"));
  // many displays do not make a long name
  std::vector<DisplayInfo> many;
  for (int i = 0; i < 10; ++i)
    many.push_back(anonymous(i * 1000, 0, 1000, 800, i == 0));
  QVERIFY(LayoutProfileStore::suggestName(sig(many)).size() <= LayoutProfileStore::kMaxNameLength);
}

void LayoutStoreTests::rename_trimsCapsAndRefuses()
{
  LayoutProfileStore store(path());
  const auto id = store.upsert(sig(tShape()), tLayout(), 1).id;
  QVERIFY(store.rename(id, QStringLiteral("  Desk  "), 50));
  QCOMPARE(store.find(id)->name, QStringLiteral("Desk"));
  QVERIFY(store.find(id)->userNamed);
  QCOMPARE(store.find(id)->updatedAt, int64_t(50));

  QVERIFY(!store.rename(id, QStringLiteral("   "), 60));
  QVERIFY(!store.rename(id, QString(), 60));
  QCOMPARE(store.find(id)->name, QStringLiteral("Desk"));
  QVERIFY(!store.rename(QStringLiteral("nope"), QStringLiteral("x"), 60));

  QVERIFY(store.rename(id, QString(500, QLatin1Char('n')), 70));
  QCOMPARE(store.find(id)->name.size(), int(LayoutProfileStore::kMaxNameLength));
}

void LayoutStoreTests::remove_removesOnlyThatProfile()
{
  LayoutProfileStore store(path());
  const auto a = store.upsert(sig(tShape()), tLayout(), 1).id;
  const auto b = store.upsert(sig(laptopOnly()), tLayout(), 2).id;
  QVERIFY(store.remove(a));
  QVERIFY(!store.remove(a));
  QVERIFY(store.find(a) == nullptr);
  QVERIFY(store.find(b) != nullptr);
  QCOMPARE(store.profiles().size(), size_t(1));
}

void LayoutStoreTests::touch_marksTheUse()
{
  LayoutProfileStore store(path());
  const auto id = store.upsert(sig(tShape()), tLayout(), 1).id;
  store.touch(id, 900);
  QCOMPARE(store.find(id)->lastUsedAt, int64_t(900));
  QCOMPARE(store.find(id)->updatedAt, int64_t(1));
  // an unknown profile is nothing
  store.touch(QStringLiteral("nope"), 1000);
  QCOMPARE(store.profiles().size(), size_t(1));
}

void LayoutStoreTests::candidates_listTheProfilesForTheMatcher()
{
  LayoutProfileStore store(path());
  const auto a = store.upsert(sig(tShape()), tLayout(), 10).id;
  store.touch(a, 20);
  const auto b = store.upsert(sig(laptopOnly()), tLayout(), 30).id;
  const auto candidates = store.candidates();
  QCOMPARE(candidates.size(), size_t(2));
  QCOMPARE(candidates[0].id, a);
  QCOMPARE(candidates[0].lastUsedAt, int64_t(20));
  QCOMPARE(candidates[0].createdAt, int64_t(10));
  QCOMPARE(candidates[1].id, b);
  QVERIFY(candidates[0].signature == &store.profiles()[0].signature);

  const auto match = deskflow::gui::layout::bestMatch(sig(laptopOnly()), candidates);
  QCOMPARE(match.kind, deskflow::gui::layout::MatchResult::Kind::Exact);
  QCOMPARE(candidates[static_cast<size_t>(match.index)].id, b);
}

void LayoutStoreTests::ids_areNeverUsedTwice()
{
  LayoutProfileStore store(path());
  const auto a = store.upsert(signatureNumber(1), tLayout(), 1).id;
  const auto b = store.upsert(signatureNumber(2), tLayout(), 2).id;
  QVERIFY(a != b);
  store.remove(b);
  const auto c = store.upsert(signatureNumber(3), tLayout(), 3).id;
  QVERIFY(c != a);
  QVERIFY(c != b);

  // and not after the file was read again
  QVERIFY(store.save());
  LayoutProfileStore again(path());
  QCOMPARE(again.load().status, LayoutProfileStore::Status::Ok);
  const auto d = again.upsert(signatureNumber(4), tLayout(), 4).id;
  QVERIFY(d != a && d != b && d != c);
}

void LayoutStoreTests::save_and_load_roundTrip()
{
  makeThree(path());

  LayoutProfileStore store(path());
  const auto report = store.load();
  QCOMPARE(report.status, LayoutProfileStore::Status::Ok);
  QCOMPARE(report.dropped, 0);
  QCOMPARE(store.profiles().size(), size_t(3));

  const auto *tProfile = store.findExact(sig(tShape()));
  QVERIFY(tProfile != nullptr);
  QVERIFY(tProfile->layout.sameAs(tLayout()));
  QCOMPARE(tProfile->createdAt, int64_t(1000));
  QCOMPARE(tProfile->lastUsedAt, int64_t(1000));
  QVERIFY(!tProfile->userNamed);
  QCOMPARE(tProfile->signature.displays().size(), size_t(2));
  QCOMPARE(tProfile->signature.displays()[0].label, QStringLiteral("External"));

  const auto *named = store.findExact(sig(laptopOnly()));
  QVERIFY(named != nullptr);
  QCOMPARE(named->name, QStringLiteral("On the move"));
  QVERIFY(named->userNamed);
  QVERIFY(named->layout.sameAs(otherLayout()));

  // the file says what it is
  const auto json = readJson(path());
  QCOMPARE(json.value(QStringLiteral("format")).toString(), QStringLiteral("deskflow-layout-profiles"));
  QCOMPARE(json.value(QStringLiteral("version")).toInt(), 1);
}

void LayoutStoreTests::active_isKeptInTheFile()
{
  {
    LayoutProfileStore store(path());
    store.upsert(sig(tShape()), tLayout(), 1);
    store.setActive(sig(tShape()));
    QVERIFY(store.save());
  }
  LayoutProfileStore store(path());
  store.load();
  QVERIFY(store.active().has_value());
  QCOMPARE(store.active()->key(), sig(tShape()).key());

  store.setActive(std::nullopt);
  QVERIFY(store.save());
  LayoutProfileStore third(path());
  third.load();
  QVERIFY(!third.active().has_value());
}

void LayoutStoreTests::load_aFileThatIsNotAStore_isDamagedAndMovedAside_data()
{
  QTest::addColumn<QByteArray>("content");
  QTest::newRow("text") << QByteArray("this is not json");
  QTest::newRow("empty") << QByteArray();
  QTest::newRow("an array") << QByteArray("[1,2,3]");
  QTest::newRow("another format") << QByteArray(R"({"format":"something","version":1,"profiles":[]})");
  QTest::newRow("no format") << QByteArray(R"({"version":1,"profiles":[]})");
  QTest::newRow("no version") << QByteArray(R"({"format":"deskflow-layout-profiles","profiles":[]})");
  QTest::newRow("version is text") << QByteArray(R"({"format":"deskflow-layout-profiles","version":"1","profiles":[]})");
  QTest::newRow("version 0") << QByteArray(R"({"format":"deskflow-layout-profiles","version":0,"profiles":[]})");
  QTest::newRow("profiles is not a list") << QByteArray(R"({"format":"deskflow-layout-profiles","version":1,"profiles":5})");
  QTest::newRow("cut off") << QByteArray(R"({"format":"deskflow-layout-profiles","version":1,"profiles":[{"id")");
}

void LayoutStoreTests::load_aFileThatIsNotAStore_isDamagedAndMovedAside()
{
  QFETCH(QByteArray, content);
  writeFile(path(), content);

  LayoutProfileStore store(path());
  const auto report = store.load();
  QCOMPARE(report.status, LayoutProfileStore::Status::Damaged);
  QVERIFY(store.profiles().empty());
  QVERIFY(!store.isInert());
  // the file is kept, under another name, and the store can be written
  QVERIFY(!QFile::exists(path()));
  QVERIFY(QFile::exists(path() + QStringLiteral(".damaged")));
  QCOMPARE(report.keptAs, path() + QStringLiteral(".damaged"));
  QCOMPARE(readFile(path() + QStringLiteral(".damaged")), content);
  QVERIFY(!report.message.isEmpty());

  store.upsert(sig(tShape()), tLayout(), 1);
  QVERIFY(store.save());
  LayoutProfileStore again(path());
  QCOMPARE(again.load().status, LayoutProfileStore::Status::Ok);
  QCOMPARE(again.profiles().size(), size_t(1));
}

void LayoutStoreTests::load_aNewerFile_isInertAndLeftAlone()
{
  const QByteArray content = R"({"format":"deskflow-layout-profiles","version":2,"profiles":[{"anything":true}]})";
  writeFile(path(), content);

  LayoutProfileStore store(path());
  const auto report = store.load();
  QCOMPARE(report.status, LayoutProfileStore::Status::NewerVersion);
  QVERIFY(store.isInert());
  QVERIFY(store.profiles().empty());
  QVERIFY(!report.message.isEmpty());

  // the store can be used in memory, but nothing of it goes to the file
  QVERIFY(!store.upsert(sig(tShape()), tLayout(), 1).id.isEmpty());
  QVERIFY(!store.save());
  QVERIFY(!store.lastError().isEmpty());
  QCOMPARE(readFile(path()), content);
  QVERIFY(!QFile::exists(path() + QStringLiteral(".damaged")));
  QVERIFY(!QFile::exists(path() + QStringLiteral(".bak")));
}

void LayoutStoreTests::load_invalidEntries_areDroppedAndTheFileIsKept()
{
  makeThree(path());
  auto json = readJson(path());
  auto profiles = json.value(QStringLiteral("profiles")).toArray();
  auto second = profiles.at(1).toObject();
  second.insert(QStringLiteral("signature"), QJsonObject());
  profiles.replace(1, second);
  json.insert(QStringLiteral("profiles"), profiles);
  writeJson(path(), json);
  const auto before = readFile(path());

  LayoutProfileStore store(path());
  const auto report = store.load();
  QCOMPARE(report.status, LayoutProfileStore::Status::Ok);
  QCOMPARE(report.dropped, 1);
  QCOMPARE(store.profiles().size(), size_t(2));
  QCOMPARE(report.keptAs, path() + QStringLiteral(".bak"));
  QCOMPARE(readFile(path() + QStringLiteral(".bak")), before);
  QVERIFY(!store.isInert());
  QVERIFY(store.findExact(sig(laptopOnly())) == nullptr);

  QVERIFY(store.save());
  QCOMPARE(readFile(path() + QStringLiteral(".bak")), before);
}

void LayoutStoreTests::load_duplicateSignatures_keepTheNewest()
{
  makeThree(path());
  auto json = readJson(path());
  auto profiles = json.value(QStringLiteral("profiles")).toArray();
  auto copy = profiles.at(0).toObject();
  copy.insert(QStringLiteral("id"), QStringLiteral("p99"));
  copy.insert(QStringLiteral("lastUsedAt"), 90000);
  copy.insert(QStringLiteral("layout"), otherLayout().toJson());
  profiles.append(copy);
  json.insert(QStringLiteral("profiles"), profiles);
  writeJson(path(), json);

  LayoutProfileStore store(path());
  const auto report = store.load();
  QCOMPARE(report.status, LayoutProfileStore::Status::Ok);
  QCOMPARE(report.dropped, 1);
  QCOMPARE(store.profiles().size(), size_t(3));
  const auto *kept = store.findExact(sig(tShape()));
  QVERIFY(kept != nullptr);
  QCOMPARE(kept->id, QStringLiteral("p99"));
  QVERIFY(kept->layout.sameAs(otherLayout()));
}

void LayoutStoreTests::load_moreThanTheMaximum_keepsTheNewest()
{
  makeThree(path());
  auto json = readJson(path());
  const auto template0 = json.value(QStringLiteral("profiles")).toArray().at(0).toObject();
  QJsonArray profiles;
  for (int i = 0; i < 40; ++i) {
    auto p = template0;
    p.insert(QStringLiteral("id"), QStringLiteral("p%1").arg(i + 100));
    p.insert(QStringLiteral("signature"), signatureNumber(i).toJson());
    p.insert(QStringLiteral("lastUsedAt"), i);
    p.insert(QStringLiteral("createdAt"), i);
    profiles.append(p);
  }
  json.insert(QStringLiteral("profiles"), profiles);
  writeJson(path(), json);

  LayoutProfileStore store(path());
  const auto report = store.load();
  QCOMPARE(report.status, LayoutProfileStore::Status::Ok);
  QCOMPARE(store.profiles().size(), size_t(LayoutProfileStore::kMaxProfiles));
  QCOMPARE(report.dropped, 8);
  // the eight that were used first are gone
  for (int i = 0; i < 8; ++i)
    QVERIFY(store.findExact(signatureNumber(i)) == nullptr);
  for (int i = 8; i < 40; ++i)
    QVERIFY(store.findExact(signatureNumber(i)) != nullptr);
}

void LayoutStoreTests::load_aFileThatIsTooBig_isDamaged()
{
  QByteArray big(int(LayoutProfileStore::kMaxFileBytes) + 10, ' ');
  big.prepend(R"({"format":"deskflow-layout-profiles","version":1,"profiles":[]})");
  writeFile(path(), big);
  LayoutProfileStore store(path());
  QCOMPARE(store.load().status, LayoutProfileStore::Status::Damaged);
  QVERIFY(store.profiles().empty());
  QVERIFY(QFile::exists(path() + QStringLiteral(".damaged")));
}

void LayoutStoreTests::load_anIdThatIsHuge_doesNotOverflowTheNextId()
{
  makeThree(path());
  auto json = readJson(path());
  auto profiles = json.value(QStringLiteral("profiles")).toArray();
  auto first = profiles.at(0).toObject();
  first.insert(QStringLiteral("id"), QStringLiteral("p9223372036854775806"));
  profiles.replace(0, first);
  json.insert(QStringLiteral("profiles"), profiles);
  json.insert(QStringLiteral("nextId"), 1);
  writeJson(path(), json);

  LayoutProfileStore store(path());
  QCOMPARE(store.load().status, LayoutProfileStore::Status::Ok);
  // the ids that are made after it are ids that the file accepts again, and none is made twice
  QStringList ids;
  for (const auto &profile : store.profiles())
    ids << profile.id;
  for (int i = 0; i < 3; ++i) {
    const auto id = store.upsert(signatureNumber(50 + i), tLayout(), 9000 + i).id;
    QVERIFY2(!id.isEmpty() && !id.contains(QLatin1Char('-')), qPrintable(id));
    QVERIFY(!ids.contains(id));
    ids << id;
  }
  QVERIFY(store.save());
  LayoutProfileStore again(path());
  QCOMPARE(again.load().status, LayoutProfileStore::Status::Ok);
  QCOMPARE(again.profiles().size(), store.profiles().size());
}

void LayoutStoreTests::save_replacesTheFileAtomically()
{
  makeThree(path());
  LayoutProfileStore store(path());
  store.load();
  store.upsert(signatureNumber(5), tLayout(), 9000);
  QVERIFY(store.save());

  // a file and nothing else: no temporary file is left next to it
  const auto files = QDir(m_dir.path()).entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot);
  QCOMPARE(files, QStringList{QStringLiteral("layout-profiles.json")});

  LayoutProfileStore again(path());
  QCOMPARE(again.load().status, LayoutProfileStore::Status::Ok);
  QCOMPARE(again.profiles().size(), size_t(4));
}

void LayoutStoreTests::save_failsWhenTheFolderCannotBeWritten()
{
  const auto folder = m_dir.filePath(QStringLiteral("readonly"));
  QVERIFY(QDir().mkpath(folder));
  const auto file = folder + QStringLiteral("/layout-profiles.json");
  LayoutProfileStore store(file);
  store.upsert(sig(tShape()), tLayout(), 1);
  QVERIFY(store.save());

  QFile::setPermissions(folder, QFileDevice::ReadOwner | QFileDevice::ExeOwner);
  const auto before = readFile(file);
  store.upsert(signatureNumber(1), tLayout(), 2);
  const bool saved = store.save();
  QFile::setPermissions(folder, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
  if (saved)
    QSKIP("the folder is writable even without the permission (a privileged user)");
  QVERIFY(!store.lastError().isEmpty());
  // the file that was there is as it was
  QCOMPARE(readFile(file), before);
}

namespace {

// n profiles that are used at 1, 2, ... n (the first one is the one that was used first)
void fill(LayoutProfileStore &store, int n, bool named = false)
{
  for (int i = 1; i <= n; ++i)
    store.upsert(signatureNumber(i), tLayout(), i * 10, named ? QStringLiteral("n%1").arg(i) : QString(), named);
}

} // namespace

void LayoutStoreTests::evict_removesTheLeastRecentlyUsedOneThatTheUserDidNotName()
{
  LayoutProfileStore store(path());
  fill(store, LayoutProfileStore::kMaxProfiles);
  QCOMPARE(store.profiles().size(), size_t(LayoutProfileStore::kMaxProfiles));
  // the first one is named by the user: it stays, and the second one goes
  store.rename(store.findExact(signatureNumber(1))->id, QStringLiteral("Mine"), 5);

  store.upsert(signatureNumber(100), tLayout(), 5000);
  QCOMPARE(store.profiles().size(), size_t(LayoutProfileStore::kMaxProfiles));
  QVERIFY(store.findExact(signatureNumber(1)) != nullptr);
  QVERIFY(store.findExact(signatureNumber(2)) == nullptr);
  QVERIFY(store.findExact(signatureNumber(3)) != nullptr);
  QVERIFY(store.findExact(signatureNumber(100)) != nullptr);
}

void LayoutStoreTests::evict_removesNamedOnesOnlyWhenThereIsNothingElse()
{
  LayoutProfileStore store(path());
  fill(store, LayoutProfileStore::kMaxProfiles, true);
  store.upsert(signatureNumber(100), tLayout(), 5000);
  QCOMPARE(store.profiles().size(), size_t(LayoutProfileStore::kMaxProfiles));
  QVERIFY(store.findExact(signatureNumber(1)) == nullptr);
  QVERIFY(store.findExact(signatureNumber(2)) != nullptr);
  QVERIFY(store.findExact(signatureNumber(100)) != nullptr);
}

void LayoutStoreTests::evict_neverRemovesTheActiveOrTheNewProfile()
{
  LayoutProfileStore store(path());
  fill(store, LayoutProfileStore::kMaxProfiles);
  // the oldest one is the situation that the arrangement belongs to
  store.setActive(signatureNumber(1));
  // and the new one is the oldest of all by the clock that it is made with
  store.upsert(signatureNumber(100), tLayout(), 1);
  QCOMPARE(store.profiles().size(), size_t(LayoutProfileStore::kMaxProfiles));
  QVERIFY(store.findExact(signatureNumber(1)) != nullptr);
  QVERIFY(store.findExact(signatureNumber(100)) != nullptr);
  QVERIFY(store.findExact(signatureNumber(2)) == nullptr);
}

void LayoutStoreTests::evict_followsTheUse()
{
  LayoutProfileStore store(path());
  fill(store, LayoutProfileStore::kMaxProfiles);
  // the first one is used again: now the second one is the one that was not used for the longest time
  store.touch(store.findExact(signatureNumber(1))->id, 9000);
  store.upsert(signatureNumber(100), tLayout(), 9500);
  QVERIFY(store.findExact(signatureNumber(1)) != nullptr);
  QVERIFY(store.findExact(signatureNumber(2)) == nullptr);
  QVERIFY(store.findExact(signatureNumber(3)) != nullptr);
}

void LayoutStoreTests::fuzz_aDamagedFileNeverBreaksTheStore()
{
  makeThree(path());
  const auto original = readFile(path());
  std::mt19937 random(2026);

  for (int i = 0; i < 400; ++i) {
    auto bytes = original;
    const int operations = 1 + int(random() % 3);
    for (int op = 0; op < operations && !bytes.isEmpty(); ++op) {
      const int at = int(random() % bytes.size());
      switch (random() % 5) {
      case 0:
        bytes.truncate(at);
        break;
      case 1:
        bytes[at] = char(random() % 256);
        break;
      case 2:
        bytes.remove(at, int(random() % 20));
        break;
      case 3:
        bytes.insert(at, bytes.mid(int(random() % bytes.size()), int(random() % 30)));
        break;
      default:
        bytes.insert(at, QByteArray(int(random() % 5), char(random() % 256)));
        break;
      }
    }
    QFile::remove(path() + QStringLiteral(".damaged"));
    QFile::remove(path() + QStringLiteral(".bak"));
    writeFile(path(), bytes);

    LayoutProfileStore store(path());
    const auto report = store.load();
    QVERIFY(report.status == LayoutProfileStore::Status::Ok || report.status == LayoutProfileStore::Status::Damaged ||
            report.status == LayoutProfileStore::Status::NewerVersion);
    QVERIFY(store.profiles().size() <= size_t(LayoutProfileStore::kMaxProfiles));
    QStringList keys;
    QStringList ids;
    for (const auto &profile : store.profiles()) {
      QVERIFY(!profile.id.isEmpty());
      QVERIFY(!profile.name.isEmpty() && profile.name.size() <= LayoutProfileStore::kMaxNameLength);
      QVERIFY(profile.signature.isValid());
      QVERIFY(!profile.layout.isEmpty() && profile.layout.computers.size() <= size_t(Layout::kMaxComputers));
      QVERIFY(!keys.contains(profile.signature.key()));
      QVERIFY(!ids.contains(profile.id));
      keys << profile.signature.key();
      ids << profile.id;
    }
    if (report.status == LayoutProfileStore::Status::Damaged)
      QVERIFY(!QFile::exists(path()));
    // whatever it was, the store can be written and read again
    if (!store.isInert()) {
      store.upsert(signatureNumber(77), tLayout(), 1);
      QVERIFY(store.save());
      LayoutProfileStore again(path());
      QCOMPARE(again.load().status, LayoutProfileStore::Status::Ok);
      QVERIFY(again.findExact(signatureNumber(77)) != nullptr);
    }
  }
}

QTEST_MAIN(LayoutStoreTests)
