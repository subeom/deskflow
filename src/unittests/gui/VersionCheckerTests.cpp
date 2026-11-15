/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "VersionCheckerTests.h"

#include "gui/UpdateNetwork.h"
#include "gui/UpdateVerifier.h"
#include "gui/VersionChecker.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

namespace {

class FakeUpdateNetwork final : public deskflow::gui::IUpdateNetwork
{
public:
  quint64
  get(const QUrl &url, const deskflow::gui::UpdateNetworkOptions &options,
      deskflow::gui::UpdateNetworkCallback callback) override
  {
    requestCount++;
    lastUrl = url;
    lastOptions = options;
    requestedUrls.push_back(url);
    m_callback = std::move(callback);
    return 42;
  }

  void cancel(quint64 id) override
  {
    cancelled.push_back(id);
  }

  void cancelAll() override
  {
    cancel(42);
  }

  void complete(const deskflow::gui::UpdateNetworkResult &result)
  {
    if (m_callback) {
      auto callback = std::move(m_callback);
      callback(result);
    }
  }

  int requestCount = 0;
  QUrl lastUrl;
  std::vector<QUrl> requestedUrls;
  deskflow::gui::UpdateNetworkOptions lastOptions;
  std::vector<quint64> cancelled;

private:
  deskflow::gui::UpdateNetworkCallback m_callback;
};

class FakeUpdateVerifier final : public deskflow::gui::IUpdateVerifier
{
public:
  deskflow::gui::UpdateVerifierResult verify(const deskflow::gui::StagedUpdate &staged) override
  {
    verified.push_back(staged);
    return result;
  }

  deskflow::gui::UpdateVerifierResult result{true, {}};
  std::vector<deskflow::gui::StagedUpdate> verified;
};

deskflow::gui::UpdateEnvironment windowsEnvironment()
{
  return {QStringLiteral("windows"), QStringLiteral("x86_64"), false};
}

deskflow::gui::UpdateEnvironment macEnvironment(bool packageManaged = false)
{
  return {QStringLiteral("macos"), QStringLiteral("arm64"), packageManaged};
}

QString sha256(const QByteArray &data)
{
  return QStringLiteral("sha256:%1")
      .arg(QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()));
}

QByteArray macSums(const QByteArray &installer, const QString &name)
{
  return sha256(installer).mid(7).toUtf8() + QByteArrayLiteral("  ") + name.toUtf8() + QByteArrayLiteral("\n");
}

QByteArray macJson(const QByteArray &installer, const QByteArray &sums)
{
  const auto name = QStringLiteral("deskflow-1.28.0-macos-arm64.dmg");
  return QStringLiteral(R"json({
    "tag_name":"v1.28.0",
    "draft":false,
    "prerelease":false,
    "html_url":"https://github.com/deskflow/deskflow/releases/tag/v1.28.0",
    "assets":[{
      "name":"%1",
      "browser_download_url":"https://github.com/deskflow/deskflow/releases/download/v1.28.0/%1",
      "size":%2,
      "digest":"%3"
    },{
      "name":"sums.txt",
      "browser_download_url":"https://github.com/deskflow/deskflow/releases/download/v1.28.0/sums.txt",
      "size":%4,
      "digest":"%5"
    }]
  })json")
      .arg(name)
      .arg(installer.size())
      .arg(sha256(installer))
      .arg(sums.size())
      .arg(sha256(sums))
      .toUtf8();
}

QByteArray stableJson()
{
  return R"json({
    "tag_name":"v1.28.0",
    "draft":false,
    "prerelease":false,
    "html_url":"https://github.com/deskflow/deskflow/releases/tag/v1.28.0",
    "assets":[{
      "name":"deskflow-1.28.0-win-x64.msi",
      "browser_download_url":"https://github.com/deskflow/deskflow/releases/download/v1.28.0/deskflow-1.28.0-win-x64.msi",
      "size":100,
      "digest":"sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    },{
      "name":"sums.txt",
      "browser_download_url":"https://github.com/deskflow/deskflow/releases/download/v1.28.0/sums.txt",
      "size":10,
      "digest":"sha256:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
    }]
  })json";
}

} // namespace

void VersionCheckerTests::checkLatest_deduplicatesAndUsesOfficialEndpoint()
{
  auto network = std::make_unique<FakeUpdateNetwork>();
  auto *fake = network.get();
  VersionChecker checker(std::move(network), windowsEnvironment());
  QSignalSpy updates(&checker, &VersionChecker::updateFound);

  checker.checkLatest();
  checker.checkLatest();
  QCOMPARE(fake->requestCount, 1);
  QCOMPARE(fake->lastUrl, QUrl(QStringLiteral("https://api.github.com/repos/deskflow/deskflow/releases/latest")));

  fake->complete({deskflow::gui::UpdateNetworkStatus::Success, 200, stableJson(), {}, {}, {}, {}});
  QCOMPARE(updates.count(), 1);
  QVERIFY(!checker.isChecking());
}

void VersionCheckerTests::checkLatest_surfacesTimeoutRateLimitAndOversizeErrors()
{
  auto network = std::make_unique<FakeUpdateNetwork>();
  auto *fake = network.get();
  VersionChecker checker(std::move(network), windowsEnvironment());
  QSignalSpy errors(&checker, &VersionChecker::updateError);

  checker.checkLatest();
  fake->complete({deskflow::gui::UpdateNetworkStatus::Timeout, 0, {}, {}, QStringLiteral("timed out"), {}, {}});
  QCOMPARE(errors.count(), 1);

  checker.checkLatest();
  fake->complete(
      {deskflow::gui::UpdateNetworkStatus::RateLimited, 429, {}, {}, QStringLiteral("rate limited"), {}, 120}
  );
  QCOMPARE(errors.count(), 2);

  checker.checkLatest();
  fake->complete(
      {deskflow::gui::UpdateNetworkStatus::TooLarge, 200, {}, {}, QStringLiteral("response too large"), {}, {}}
  );
  QCOMPARE(errors.count(), 3);
}

void VersionCheckerTests::checkLatest_cancelStopsInFlightRequest()
{
  auto network = std::make_unique<FakeUpdateNetwork>();
  auto *fake = network.get();
  VersionChecker checker(std::move(network), windowsEnvironment());

  checker.checkLatest();
  QVERIFY(checker.isChecking());
  checker.cancel();
  QVERIFY(!checker.isChecking());
  QCOMPARE(fake->cancelled, std::vector<quint64>{42});
}

void VersionCheckerTests::checkLatest_stagesAndVerifiesAutomaticMacUpdate()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QByteArray installer("signed installer");
  const QByteArray sums = macSums(installer, QStringLiteral("deskflow-1.28.0-macos-arm64.dmg"));

  auto network = std::make_unique<FakeUpdateNetwork>();
  auto *fakeNetwork = network.get();
  auto verifier = std::make_unique<FakeUpdateVerifier>();
  auto *fakeVerifier = verifier.get();
  VersionChecker checker(std::move(network), macEnvironment(), directory.path(), std::move(verifier));
  QSignalSpy updates(&checker, &VersionChecker::updateStaged);

  checker.checkLatest();
  fakeNetwork->complete({deskflow::gui::UpdateNetworkStatus::Success, 200, macJson(installer, sums), {}, {}, {}, {}});

  QCOMPARE(
      fakeNetwork->lastUrl,
      QUrl(QStringLiteral("https://github.com/deskflow/deskflow/releases/download/v1.28.0/sums.txt"))
  );
  QCOMPARE(fakeNetwork->lastOptions.maximumBytes, sums.size());
  fakeNetwork->complete({deskflow::gui::UpdateNetworkStatus::Success, 200, sums, {}, {}, {}, {}});

  QCOMPARE(
      fakeNetwork->lastUrl,
      QUrl(QStringLiteral(
          "https://github.com/deskflow/deskflow/releases/download/v1.28.0/deskflow-1.28.0-macos-arm64.dmg"
      ))
  );
  QCOMPARE(fakeNetwork->lastOptions.maximumBytes, installer.size());
  fakeNetwork->complete({deskflow::gui::UpdateNetworkStatus::Success, 200, installer, {}, {}, {}, {}});

  QCOMPARE(updates.count(), 1);
  QCOMPARE(fakeVerifier->verified.size(), size_t{1});
  QVERIFY(QFileInfo::exists(fakeVerifier->verified.front().installerPath));
  QVERIFY(!checker.isChecking());
}

void VersionCheckerTests::checkLatest_discardsAndReportsFailedPlatformVerification()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QByteArray installer("untrusted installer");
  const QByteArray sums = macSums(installer, QStringLiteral("deskflow-1.28.0-macos-arm64.dmg"));

  auto network = std::make_unique<FakeUpdateNetwork>();
  auto *fakeNetwork = network.get();
  auto verifier = std::make_unique<FakeUpdateVerifier>();
  verifier->result = {false, QStringLiteral("signature verification failed")};
  auto *fakeVerifier = verifier.get();
  VersionChecker checker(std::move(network), macEnvironment(), directory.path(), std::move(verifier));
  QSignalSpy errors(&checker, &VersionChecker::updateError);
  QSignalSpy staged(&checker, &VersionChecker::updateStaged);

  checker.checkLatest();
  fakeNetwork->complete({deskflow::gui::UpdateNetworkStatus::Success, 200, macJson(installer, sums), {}, {}, {}, {}});
  fakeNetwork->complete({deskflow::gui::UpdateNetworkStatus::Success, 200, sums, {}, {}, {}, {}});
  fakeNetwork->complete({deskflow::gui::UpdateNetworkStatus::Success, 200, installer, {}, {}, {}, {}});

  QCOMPARE(errors.count(), 1);
  QCOMPARE(staged.count(), 0);
  QCOMPARE(fakeVerifier->verified.size(), size_t{1});
  QVERIFY(!QFileInfo::exists(fakeVerifier->verified.front().installerPath));
  QVERIFY(!checker.isChecking());
}

void VersionCheckerTests::checkLatest_windowsUpdateRemainsManual()
{
  auto network = std::make_unique<FakeUpdateNetwork>();
  auto *fake = network.get();
  VersionChecker checker(std::move(network), windowsEnvironment());
  QSignalSpy updates(&checker, &VersionChecker::updateFound);
  QSignalSpy staged(&checker, &VersionChecker::updateStaged);

  checker.checkLatest();
  fake->complete({deskflow::gui::UpdateNetworkStatus::Success, 200, stableJson(), {}, {}, {}, {}});

  QCOMPARE(fake->requestCount, 1);
  QCOMPARE(updates.count(), 1);
  QCOMPARE(staged.count(), 0);
  QCOMPARE(
      updates.first().at(1).toString(), QStringLiteral("https://github.com/deskflow/deskflow/releases/tag/v1.28.0")
  );
}

void VersionCheckerTests::checkLatest_homebrewUpdateRemainsManual()
{
  const QByteArray installer("signed installer");
  const QByteArray sums = macSums(installer, QStringLiteral("deskflow-1.28.0-macos-arm64.dmg"));
  auto network = std::make_unique<FakeUpdateNetwork>();
  auto *fake = network.get();
  VersionChecker checker(std::move(network), macEnvironment(true));
  QSignalSpy updates(&checker, &VersionChecker::updateFound);
  QSignalSpy staged(&checker, &VersionChecker::updateStaged);

  checker.checkLatest();
  fake->complete({deskflow::gui::UpdateNetworkStatus::Success, 200, macJson(installer, sums), {}, {}, {}, {}});

  QCOMPARE(fake->requestCount, 1);
  QCOMPARE(updates.count(), 1);
  QCOMPARE(staged.count(), 0);
}

void VersionCheckerTests::redirectAllowlist_acceptsOnlyGitHubDownloadHosts()
{
  using deskflow::gui::isAllowedUpdateRedirect;
  QVERIFY(isAllowedUpdateRedirect(QUrl(QStringLiteral("https://github.com/deskflow/deskflow/releases/download/x/a"))));
  QVERIFY(isAllowedUpdateRedirect(QUrl(QStringLiteral("https://release-assets.githubusercontent.com/a"))));
  QVERIFY(isAllowedUpdateRedirect(QUrl(QStringLiteral("https://objects.githubusercontent.com/a"))));
  QVERIFY(
      isAllowedUpdateRedirect(QUrl(QStringLiteral("https://api.github.com/repos/deskflow/deskflow/releases/latest")))
  );
  QVERIFY(!isAllowedUpdateRedirect(QUrl(QStringLiteral("http://github.com/a"))));
  QVERIFY(
      !isAllowedUpdateRedirect(QUrl(QStringLiteral("https://user@github.com/deskflow/deskflow/releases/download/x/a")))
  );
  QVERIFY(
      !isAllowedUpdateRedirect(QUrl(QStringLiteral("https://github.com:444/deskflow/deskflow/releases/download/x/a")))
  );
  QVERIFY(!isAllowedUpdateRedirect(
      QUrl(QStringLiteral("https://api.github.com/repos/deskflow/deskflow/releases/latest?unexpected=1"))
  ));
  QVERIFY(!isAllowedUpdateRedirect(QUrl(QStringLiteral("https://github.com/other/repository/releases/download/v1/a"))));
  QVERIFY(!isAllowedUpdateRedirect(QUrl(QStringLiteral("https://api.github.com/repos/other/repo/releases/latest"))));
  QVERIFY(!isAllowedUpdateRedirect(QUrl(QStringLiteral("https://evil.example/a"))));
  QVERIFY(!isAllowedUpdateRedirect(QUrl(QStringLiteral("https://github.com.evil.example/a"))));
}

QTEST_MAIN(VersionCheckerTests)
