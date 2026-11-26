/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TunnelTestDialogTests.h"

#include "TunnelTestDoubles.h"

#include "gui/dialogs/TunnelTestDialog.h"
#include "gui/style/DeskflowStyle.h"
#include "gui/widgets/StatusChip.h"
#include "gui/widgets/TestResultRows.h"

#include <QApplication>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QScopeGuard>
#include <QScrollArea>
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

void TunnelTestDialogTests::cleanup()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
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
  // the Client ID is shown by its first characters only
  QVERIFY(shown(dialog).contains(QStringLiteral("Client ID dumm%1").arg(QChar(0x2026))));
  QVERIFY(!shown(dialog).contains(kClientId));
  QVERIFY(!shown(dialog).contains(kClientId.left(8)));
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

void TunnelTestDialogTests::theDialog_asksForARoomThatFitsAScreen_data()
{
  QTest::addColumn<QString>("style");
  QTest::addColumn<int>("pixels");
  // the font of the platform and fonts of 12, 10, 8 and 6 px (the letters are 7, 6, 5 and 4 px wide on the average: the
  // PC of the checks of Windows has narrower letters than a Mac), in the two styles that differ in their margins
  for (const auto &style : {QStringLiteral("Fusion"), QStringLiteral("Windows")}) {
    QTest::newRow(qPrintable(style + QStringLiteral(", the font of the platform"))) << style << 0;
    for (const int pixels : {12, 10, 8, 6})
      QTest::newRow(qPrintable(QStringLiteral("%1, %2 px").arg(style).arg(pixels))) << style << pixels;
  }
}

// The room that the dialog asks for is what its panel asks for: the width of 64 letters of its font, and the margins of
// the layout around it. It is not a number of pixels: the number 400 that was here is what a Mac asks for, and the
// offscreen platform of a PC, with its own font and style, asked for less and failed the test there (qwindows passed
// it). The letters of a font are 4 to 8 px wide and the margins are the style's
void TunnelTestDialogTests::theDialog_asksForARoomThatFitsAScreen()
{
  QFETCH(QString, style);
  QFETCH(int, pixels);
  QApplication::setStyle(style);
  const auto original = QApplication::font();
  auto font = original;
  if (pixels > 0)
    font.setPixelSize(pixels);
  QApplication::setFont(font);
  const auto restoreFont = qScopeGuard([original] { QApplication::setFont(original); });

  Rig rig(m_dir);
  TunnelTestDialog dialog(nullptr, rig.dependencies());
  dialog.runTest();
  const auto size = dialog.sizeHint();
  const auto margins = dialog.layout()->contentsMargins();
  const int panel = dialog.fontMetrics().averageCharWidth() * 64;
  QVERIFY2(
      size.width() >= panel + margins.left() + margins.right(),
      qPrintable(QStringLiteral("%1 wide for a panel of %2 and margins of %3 + %4")
                     .arg(size.width())
                     .arg(panel)
                     .arg(margins.left())
                     .arg(margins.right()))
  );
  // it cannot be made narrower than that (the panel asks for the room, and the layout gives it)
  QVERIFY2(
      dialog.minimumSizeHint().width() >= panel + margins.left() + margins.right(),
      qPrintable(QString::number(dialog.minimumSizeHint().width()))
  );
  // and it asks for no more than a screen has
  QVERIFY(size.width() <= 1100);
  QVERIFY(size.height() <= 800);
}

void TunnelTestDialogTests::newLook_showsTheStagesAsRows_andTheSystemLookKeepsItsText()
{
  using deskflow::gui::TestResultRows;

  // the system look: a text, no rows
  {
    Rig rig(m_dir);
    TunnelTestDialog dialog(nullptr, rig.dependencies());
    dialog.show();
    dialog.runTest();
    QVERIFY(dialog.findChild<TestResultRows *>() == nullptr);
    QVERIFY(dialog.findChild<QTextBrowser *>(QStringLiteral("resultsView"))->isVisibleTo(&dialog));
  }

  // the new look: a row for each stage, in the place of the text
  auto *style = new deskflow::gui::style::DeskflowStyle;
  QApplication::setStyle(style);
  deskflow::gui::style::Look look;
  look.reduceTransparency = true;
  look.reduceMotion = true;
  style->useLook(look);

  Rig rig(m_dir);
  rig.tunnel.storeState->token.reset();
  TunnelTestDialog dialog(nullptr, rig.dependencies());
  dialog.show();
  dialog.runTest();
  QVERIFY(!dialog.isTesting());
  QVERIFY(!dialog.findChild<QTextBrowser *>(QStringLiteral("resultsView"))->isVisibleTo(&dialog));
  QVERIFY(dialog.findChild<QScrollArea *>(QStringLiteral("resultsRows"))->isVisibleTo(&dialog));

  auto *rows = dialog.findChild<TestResultRows *>();
  QVERIFY(rows != nullptr);
  QCOMPARE(rows->rowCount(), 7);
  QVERIFY(rows->intro().contains(QStringLiteral("no Deskflow data is sent")));
  QVERIFY(rows->intro().contains(QStringLiteral("Via Cloudflare Tunnel: mbp.example.com")));
  // no Service Token: that stage failed, and what to do about it is its to open
  const auto all = rows->rows();
  QCOMPARE(all[1].kind, StatusChip::Kind::Danger);
  QCOMPARE(all[1].detail, QStringLiteral("No Service Token is stored."));
  QVERIFY(rows->hintsButton(1) != nullptr);
  QVERIFY(rows->hintsButton(0) == nullptr);
  QVERIFY(all[1].hints.first().contains(QStringLiteral("enter the Service Token")));
  QVERIFY(button(dialog, QStringLiteral("btnCopySummary"))->isEnabled());
  QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("lblStatus"))->text().contains(QStringLiteral("does not work")));
  // the sentence for a message is the same in any look
  QVERIFY(dialog.summary().contains(QStringLiteral("reason: credentials-missing")));
}

QTEST_MAIN(TunnelTestDialogTests)
