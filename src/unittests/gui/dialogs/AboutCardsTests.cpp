/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AboutCardsTests.h"

#include "gui/dialogs/AboutDialog.h"
#include "gui/style/Appearance.h"
#include "gui/style/BackdropWidget.h"
#include "gui/style/DeskflowStyle.h"
#include "gui/widgets/SettingCard.h"

#include <QApplication>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalSpy>
#include <QTest>

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

template <class T> T *child(const QObject &parent, const char *name)
{
  auto *found = parent.findChild<T *>(QString::fromLatin1(name));
  Q_ASSERT_X(found, "child", name);
  return found;
}

} // namespace

void AboutCardsTests::cleanup()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  QApplication::setPalette(QPalette());
}

void AboutCardsTests::systemLook_isTheFormAsItWas()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  AboutDialog dialog;
  QVERIFY(dialog.findChildren<SettingCard *>().isEmpty());
  QVERIFY(dialog.findChildren<BackdropWidget *>().isEmpty());
  QVERIFY(!child<QScrollArea>(dialog, "scrollArea")->isHidden());
  QVERIFY(!child<QGroupBox>(dialog, "groupBox")->isHidden());
  QCOMPARE(child<QLabel>(dialog, "lblImportantDevs")->parentWidget(), child<QGroupBox>(dialog, "groupBox"));
}

void AboutCardsTests::deskflowLook_hasTheCreditsInCards_withTheHeaderAndTheButtonKept()
{
  installDeskflowStyle();
  AboutDialog dialog;
  dialog.show();
  QVERIFY(QTest::qWaitForWindowExposed(&dialog));

  QStringList titles;
  for (const auto *card : dialog.findChildren<SettingCard *>())
    titles << card->title();
  QCOMPARE(titles, (QStringList{QString(), QStringLiteral("Important Developers")}));
  QCOMPARE(dialog.findChildren<BackdropWidget *>().size(), 1);
  QVERIFY(child<QScrollArea>(dialog, "scrollArea")->isHidden());
  QVERIFY(child<QGroupBox>(dialog, "groupBox")->isHidden());

  // what the dialog shows is where the cards are: the text, the credits, the links
  for (const auto &name : {"lblDescription", "lblCopyright", "label_2", "lblImportantDevs", "lblName", "lblVersion"}) {
    auto *label = child<QLabel>(dialog, name);
    QVERIFY2(label->isVisibleTo(&dialog), name);
    QVERIFY2(!label->text().isEmpty(), name);
  }
  QVERIFY(child<QLabel>(dialog, "lblImportantDevs")->text().contains(QStringLiteral("Chris")));
  QVERIFY(child<QLabel>(dialog, "lblImportantDevs")->wordWrap());
  QVERIFY(child<QWidget>(dialog, "frameLogo")->isVisibleTo(&dialog));
  QVERIFY(child<QWidget>(dialog, "btnCopyVersion")->isVisibleTo(&dialog));

  // the button closes the dialog as before
  auto *close = child<QPushButton>(dialog, "btnOk");
  QVERIFY(close->isVisibleTo(&dialog));
  QVERIFY(close->isDefault());
  close->click();
  QVERIFY(!dialog.isVisible());
}

void AboutCardsTests::iconsOfTheNewLook_areCreditedWithTheirLicence_onlyWhereTheyAreUsed()
{
  // the icons of the new look are the Phosphor icons (MIT): the copyright notice is in the repository, and the person
  // who has the program can read where the icons are from. The look of the system draws none of them
  QApplication::setStyle(QStringLiteral("Fusion"));
  {
    AboutDialog dialog;
    for (const auto *label : dialog.findChildren<QLabel *>())
      QVERIFY2(!label->text().contains(QStringLiteral("Phosphor")), qPrintable(label->objectName()));
  }

  installDeskflowStyle();
  AboutDialog dialog;
  dialog.show();
  QVERIFY(QTest::qWaitForWindowExposed(&dialog));
  auto *credit = child<QLabel>(dialog, "lblPhosphorCredit");
  QVERIFY(credit->isVisibleTo(&dialog));
  QVERIFY(credit->text().contains(QStringLiteral("Phosphor Icons")));
  QVERIFY(credit->text().contains(QStringLiteral("MIT")));
  QVERIFY(credit->text().contains(QStringLiteral("https://phosphoricons.com")));
  QVERIFY(credit->openExternalLinks());
  QVERIFY(credit->wordWrap());
  // it is in the card with the other credit of icons, under it, and is not cut at the side
  auto *breeze = child<QLabel>(dialog, "label_2");
  QCOMPARE(credit->parentWidget(), breeze->parentWidget());
  QVERIFY(credit->mapTo(&dialog, QPoint(0, 0)).y() >= breeze->mapTo(&dialog, QPoint(0, breeze->height())).y() - 1);
  QVERIFY(credit->mapTo(&dialog, QPoint(credit->width(), 0)).x() <= dialog.width());
  QVERIFY(credit->height() >= credit->heightForWidth(credit->width()));
}

void AboutCardsTests::deskflowLook_isAsSmallAsTheFormWas_andTheCardsAreNotCutAtTheSide()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  QSize form;
  {
    AboutDialog dialog;
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    form = dialog.minimumSizeHint();
  }

  installDeskflowStyle();
  AboutDialog dialog;
  dialog.show();
  QVERIFY(QTest::qWaitForWindowExposed(&dialog));
  const auto cards = dialog.minimumSizeHint();
  qInfo() << "minimum size: form" << form << "cards" << cards;
  const auto sizes =
      QStringLiteral("%1 x %2 / %3 x %4").arg(cards.width()).arg(cards.height()).arg(form.width()).arg(form.height());
  QVERIFY2(cards.height() <= form.height() * 12 / 10 && cards.height() >= form.height() * 9 / 10, qPrintable(sizes));
  QVERIFY2(cards.width() <= form.width() * 12 / 10 && cards.width() >= form.width() * 9 / 10, qPrintable(sizes));

  dialog.resize(cards);
  QCoreApplication::processEvents();
  for (auto *scroll : dialog.findChildren<QScrollArea *>()) {
    if (scroll->isHidden() || !scroll->widget())
      continue;
    QVERIFY(scroll->widget()->width() <= scroll->viewport()->width());
  }
}

QTEST_MAIN(AboutCardsTests)
