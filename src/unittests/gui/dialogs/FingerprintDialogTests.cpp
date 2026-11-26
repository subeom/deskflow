/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "FingerprintDialogTests.h"

#include "gui/dialogs/FingerprintDialog.h"
#include "gui/style/DeskflowStyle.h"
#include "gui/widgets/FingerprintPreview.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QTest>

#include <net/Fingerprint.h>
#include <net/SecureUtils.h>

using namespace deskflow::gui::style;

namespace {

void installDeskflowStyle()
{
  auto *style = new DeskflowStyle;
  QApplication::setStyle(style);
  Look look;
  look.reduceTransparency = true;
  look.reduceMotion = true;
  style->useLook(look);
}

Fingerprint sample(char seed = 0)
{
  QByteArray data;
  for (int i = 0; i < 32; ++i)
    data.append(static_cast<char>(seed + i * 7));
  return {QCryptographicHash::Sha256, data};
}

QLabel *hashOf(const FingerprintPreview *preview)
{
  return preview->findChild<QLabel *>(QStringLiteral("lblFingerprintHash"));
}

QLabel *artOf(const FingerprintPreview *preview)
{
  return preview->findChild<QLabel *>(QStringLiteral("lblFingerprintArt"));
}

QPushButton *modeButton(const FingerprintDialog &dialog)
{
  return dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Help);
}

} // namespace

void FingerprintDialogTests::cleanup()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  QApplication::setPalette(QPalette());
}

void FingerprintDialogTests::groupedHash_isFourCharactersToAGroupAndFourGroupsToALine()
{
  const auto data = QByteArray::fromHex("00112233445566778899aabbccddeeff0123456789abcdeffedcba9876543210");
  QCOMPARE(
      FingerprintPreview::groupedHash(data), QStringLiteral(
                                                 "0011 2233 4455 6677\n"
                                                 "8899 AABB CCDD EEFF\n"
                                                 "0123 4567 89AB CDEF\n"
                                                 "FEDC BA98 7654 3210"
                                             )
  );
  // a hash that is not a whole number of groups, another size of group and of line, and nothing
  QCOMPARE(FingerprintPreview::groupedHash(QByteArray::fromHex("001122")), QStringLiteral("0011 22"));
  QCOMPARE(FingerprintPreview::groupedHash(QByteArray::fromHex("00112233"), 2, 2), QStringLiteral("00 11\n22 33"));
  QCOMPARE(FingerprintPreview::groupedHash({}), QString());
}

void FingerprintDialogTests::systemLook_showsTheImageFirst_andTheHashAsItWas()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  QVERIFY(!FingerprintPreview::hashFirst());
  const auto fingerprint = sample();
  FingerprintDialog dialog(nullptr, fingerprint, FingerprintDialogMode::Local);
  const auto *preview = dialog.findChild<FingerprintPreview *>();
  QVERIFY(preview);
  QVERIFY(artOf(preview)->isVisibleTo(&dialog));
  QVERIFY(!hashOf(preview)->isVisibleTo(&dialog));
  QCOMPARE(hashOf(preview)->text(), deskflow::formatSSLFingerprintColumns(fingerprint.data));
  QCOMPARE(modeButton(dialog)->text(), QStringLiteral("Show hash"));
  QVERIFY(!modeButton(dialog)->isChecked());

  modeButton(dialog)->click();
  QVERIFY(hashOf(preview)->isVisibleTo(&dialog));
  QVERIFY(!artOf(preview)->isVisibleTo(&dialog));
  QCOMPARE(modeButton(dialog)->text(), QStringLiteral("Show image"));
}

void FingerprintDialogTests::deskflowLook_showsTheGroupedHashFirst_andTheImageOnRequest()
{
  installDeskflowStyle();
  QVERIFY(FingerprintPreview::hashFirst());
  const auto fingerprint = sample();
  FingerprintDialog dialog(nullptr, fingerprint, FingerprintDialogMode::Local);
  const auto *preview = dialog.findChild<FingerprintPreview *>();
  QVERIFY(preview);

  QVERIFY(hashOf(preview)->isVisibleTo(&dialog));
  QVERIFY(!artOf(preview)->isVisibleTo(&dialog));
  QCOMPARE(hashOf(preview)->text(), FingerprintPreview::groupedHash(fingerprint.data));
  // four groups of four characters to each of four lines
  QVERIFY(QRegularExpression(QStringLiteral("^([0-9A-F]{4} ){3}[0-9A-F]{4}(\\n([0-9A-F]{4} ){3}[0-9A-F]{4}){3}$"))
              .match(hashOf(preview)->text())
              .hasMatch());
  QVERIFY(modeButton(dialog)->isChecked());
  QCOMPARE(modeButton(dialog)->text(), QStringLiteral("Show image"));

  // the size is the same in both views: the dialog does not jump when the view is changed
  const auto dialogSize = dialog.size();
  const auto previewSize = preview->size();
  modeButton(dialog)->click();
  QVERIFY(artOf(preview)->isVisibleTo(&dialog));
  QVERIFY(!hashOf(preview)->isVisibleTo(&dialog));
  QCOMPARE(modeButton(dialog)->text(), QStringLiteral("Show hash"));
  QCOMPARE(dialog.size(), dialogSize);
  QCOMPARE(preview->size(), previewSize);
  modeButton(dialog)->click();
  QVERIFY(hashOf(preview)->isVisibleTo(&dialog));
  QCOMPARE(dialog.size(), dialogSize);
}

void FingerprintDialogTests::deskflowLook_comparesTwoGroupedHashes_withoutChangingSize()
{
  installDeskflowStyle();
  for (const auto mode : {FingerprintDialogMode::Server, FingerprintDialogMode::Client}) {
    FingerprintDialog dialog(nullptr, sample(1), mode, sample(2));
    const auto previews = dialog.findChildren<FingerprintPreview *>();
    QCOMPARE(previews.size(), 2);
    for (const auto *preview : previews) {
      QVERIFY(hashOf(preview)->isVisibleTo(&dialog));
      QVERIFY(!artOf(preview)->isVisibleTo(&dialog));
    }
    // two different fingerprints are two different hashes
    QVERIFY(hashOf(previews[0])->text() != hashOf(previews[1])->text());
    const auto size = dialog.size();
    modeButton(dialog)->click();
    QCOMPARE(dialog.size(), size);
    for (const auto *preview : previews)
      QVERIFY(artOf(preview)->isVisibleTo(&dialog));
  }
}

QTEST_MAIN(FingerprintDialogTests)
