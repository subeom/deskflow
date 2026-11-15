/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "VersionCheckerTests.h"

#include "gui/UpdateNetwork.h"
#include "gui/VersionChecker.h"

#include <QSignalSpy>
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
  deskflow::gui::UpdateNetworkOptions lastOptions;
  std::vector<quint64> cancelled;

private:
  deskflow::gui::UpdateNetworkCallback m_callback;
};

deskflow::gui::UpdateEnvironment windowsEnvironment()
{
  return {QStringLiteral("windows"), QStringLiteral("x86_64"), false};
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
      "browser_download_url":"https://github.com/deskflow/deskflow/releases/download/v1.28.0/win.msi",
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
  QVERIFY(!isAllowedUpdateRedirect(QUrl(QStringLiteral("https://github.com/other/repository/releases/download/v1/a"))));
  QVERIFY(!isAllowedUpdateRedirect(QUrl(QStringLiteral("https://api.github.com/repos/other/repo/releases/latest"))));
  QVERIFY(!isAllowedUpdateRedirect(QUrl(QStringLiteral("https://evil.example/a"))));
  QVERIFY(!isAllowedUpdateRedirect(QUrl(QStringLiteral("https://github.com.evil.example/a"))));
}

QTEST_MAIN(VersionCheckerTests)
