/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TunnelTestDialogTests.h"

#include "TunnelTestDoubles.h"

#include "gui/dialogs/TunnelTestDialog.h"

#include <QPushButton>
#include <QTemporaryDir>
#include <QTextBrowser>

using namespace deskflow::tunnel;
using namespace tunneltest;

namespace {

QTemporaryDir &scratchDir()
{
  static QTemporaryDir dir;
  return dir;
}

struct Rig
{
  ControllerRig tunnel;
  std::vector<std::function<void(HoldOutcome)>> holds;
  QStringList copied;

  explicit Rig(const QString &base) : tunnel(base, kConfig, 80)
  {
    tunnel.turnOn();
  }

  TunnelTestRunner::Dependencies dependencies()
  {
    TunnelTestRunner::Dependencies deps;
    deps.controller = tunnel.controller.get();
    deps.store = std::make_shared<InMemorySecretStore>(tunnel.storeState);
    deps.locate = [](const QString &path) {
      CloudflaredLocation location;
      location.source = CloudflaredLocation::Source::Configured;
      location.path = path.isEmpty() ? kProgram : path;
      return location;
    };
    deps.holdConnection = [this](int, int, std::function<void(HoldOutcome)> done) { holds.push_back(std::move(done)); };
    deps.graceMs = 10;
    deps.startTimeoutMs = 60000;
    return deps;
  }
};

QString shown(const TunnelTestDialog &dialog)
{
  return dialog.findChild<QTextBrowser *>(QStringLiteral("resultsView"))->toPlainText();
}

QPushButton *button(const TunnelTestDialog &dialog, const QString &name)
{
  return dialog.findChild<QPushButton *>(name);
}

} // namespace

void TunnelTestDialogTests::initTestCase()
{
  QVERIFY(scratchDir().isValid());
  m_dir = scratchDir().path();
}

void TunnelTestDialogTests::everyStageIsShownAsItHappens()
{
  Rig rig(m_dir);
  TunnelTestDialog dialog(nullptr, rig.dependencies(), [&](const QString &text) { rig.copied << text; });
  dialog.runTest();
  QVERIFY(dialog.isTesting());

  const auto text = shown(dialog);
  for (const auto &name :
       {QStringLiteral("cloudflared is installed"), QStringLiteral("Service Token is stored"),
        QStringLiteral("TLS is on"), QStringLiteral("Tunnel hostname is valid"),
        QStringLiteral("cloudflared is running"), QStringLiteral("Local port is open"),
        QStringLiteral("Connection through the tunnel")}) {
    QVERIFY2(text.contains(name), qPrintable(name));
  }
  QVERIFY(text.contains(QStringLiteral("Via Cloudflare Tunnel: mbp.example.com (local 127.0.0.1:24800)")));
  // no data is sent: the dialog says what is and is not tested
  QVERIFY(text.contains(QStringLiteral("no Deskflow data is sent")));
  QVERIFY(!button(dialog, QStringLiteral("btnCopySummary"))->isEnabled());
  QVERIFY(!button(dialog, QStringLiteral("btnRunAgain"))->isEnabled());
}

void TunnelTestDialogTests::aFailure_isMarkedWithWhatToDoAboutIt()
{
  Rig rig(m_dir);
  rig.tunnel.storeState->token.reset();
  TunnelTestDialog dialog(nullptr, rig.dependencies());
  dialog.runTest();

  QVERIFY(!dialog.isTesting());
  const auto text = shown(dialog);
  QVERIFY(text.contains(QStringLiteral("No Service Token is stored.")));
  QVERIFY(text.contains(QStringLiteral("enter the Service Token")));
  QVERIFY(button(dialog, QStringLiteral("btnCopySummary"))->isEnabled());
  QVERIFY(button(dialog, QStringLiteral("btnRunAgain"))->isEnabled());
  QVERIFY(dialog.summary().contains(QStringLiteral("reason: credentials-missing")));
  QVERIFY(dialog.summary().contains(QStringLiteral("does not work")));
}

void TunnelTestDialogTests::aTunnelThatWorks_saysSoAndAllowsTheSummaryToBeCopied()
{
  Rig rig(m_dir);
  TunnelTestDialog dialog(nullptr, rig.dependencies());
  dialog.runTest();
  rig.tunnel.finishStart();
  QCOMPARE(rig.holds.size(), size_t(1));

  rig.holds.front()(HoldOutcome::Held);
  QTRY_VERIFY_WITH_TIMEOUT(!dialog.isTesting(), 3000);
  QVERIFY(shown(dialog).contains(QStringLiteral("The tunnel carries a connection to the server")));
  QVERIFY(dialog.summary().contains(QStringLiteral("works via mbp.example.com")));
  QVERIFY(button(dialog, QStringLiteral("btnCopySummary"))->isEnabled());
}

void TunnelTestDialogTests::copy_writesTheSummaryToTheWriterOnly()
{
  Rig rig(m_dir);
  rig.tunnel.storeState->token.reset();
  TunnelTestDialog dialog(nullptr, rig.dependencies(), [&](const QString &text) { rig.copied << text; });
  dialog.runTest();

  button(dialog, QStringLiteral("btnCopySummary"))->click();
  QCOMPARE(rig.copied.size(), 1);
  QCOMPARE(rig.copied.front(), dialog.summary());
  QVERIFY(!rig.copied.front().contains(kSecret));
  QVERIFY(!rig.copied.front().contains(kClientId));
}

void TunnelTestDialogTests::runAgain_startsANewTest()
{
  Rig rig(m_dir);
  rig.tunnel.storeState->token.reset();
  TunnelTestDialog dialog(nullptr, rig.dependencies());
  dialog.runTest();
  QVERIFY(shown(dialog).contains(QStringLiteral("No Service Token is stored.")));

  // the token is stored now, and the test is run again
  rig.tunnel.storeState->token = ServiceToken(kClientId, kSecret);
  button(dialog, QStringLiteral("btnRunAgain"))->click();
  QVERIFY(dialog.isTesting());
  QVERIFY(!shown(dialog).contains(QStringLiteral("No Service Token is stored.")));
  QVERIFY(shown(dialog).contains(kClientId));
}

void TunnelTestDialogTests::everyFailureHasAHint()
{
  QVERIFY(TunnelTestDialog::hintText(TunnelFailure::None).isEmpty());
  for (int value = static_cast<int>(TunnelFailure::TlsRequired);
       value <= static_cast<int>(TunnelFailure::SettingsChanged); ++value) {
    const auto hint = TunnelTestDialog::hintText(static_cast<TunnelFailure>(value));
    QVERIFY2(!hint.isEmpty(), qPrintable(failureKey(static_cast<TunnelFailure>(value))));
    // no hint tells the user to type the secret into a place other than the field of the dialog
    QVERIFY(!hint.contains(kSecret));
  }
}

void TunnelTestDialogTests::theDialog_asksForARoomThatFitsAScreen()
{
  Rig rig(m_dir);
  TunnelTestDialog dialog(nullptr, rig.dependencies());
  dialog.runTest();
  const auto size = dialog.sizeHint();
  QVERIFY(size.width() >= 400);
  QVERIFY(size.width() <= 1100);
  QVERIFY(size.height() <= 800);
}

QTEST_MAIN(TunnelTestDialogTests)
