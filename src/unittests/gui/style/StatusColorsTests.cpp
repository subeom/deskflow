/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "StatusColorsTests.h"

#include "gui/style/Appearance.h"
#include "gui/style/Contrast.h"
#include "gui/style/DeskflowStyle.h"
#include "gui/style/StatusColors.h"
#include "gui/validators/EmptyStringValidator.h"
#include "gui/validators/LineEditValidator.h"
#include "gui/validators/ValidationError.h"

#include <QApplication>
#include <QLabel>
#include <QLineEdit>

using namespace deskflow::gui::style;

namespace {

Look lookOf(bool dark)
{
  Look look;
  look.scheme = dark ? Scheme::Dark : Scheme::Light;
  return look;
}

DeskflowStyle *install(bool dark)
{
  auto *style = new DeskflowStyle;
  QApplication::setStyle(style);
  style->useLook(lookOf(dark));
  return style;
}

void addStatuses()
{
  QTest::addColumn<int>("status");
  QTest::newRow("success") << static_cast<int>(Status::Success);
  QTest::newRow("warning") << static_cast<int>(Status::Warning);
  QTest::newRow("danger") << static_cast<int>(Status::Danger);
}

QColor tokenColorOf(const Tokens &tokens, Status status)
{
  switch (status) {
  case Status::Success:
    return tokens.colors.success;
  case Status::Warning:
    return tokens.colors.warning;
  case Status::Danger:
    return tokens.colors.danger;
  }
  return {};
}

} // namespace

void StatusColorsTests::cleanup()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  QApplication::setPalette(QPalette());
}

void StatusColorsTests::statusColor_withoutTheStyleLayer_isTheStyleSheetThatItAlwaysWas_data()
{
  addStatuses();
}

void StatusColorsTests::statusColor_withoutTheStyleLayer_isTheStyleSheetThatItAlwaysWas()
{
  QFETCH(int, status);
  QVERIFY(!isDeskflowStyleInstalled());
  const QHash<int, QString> always{
      {static_cast<int>(Status::Success), QStringLiteral("color: #1e8e3e;")},
      {static_cast<int>(Status::Warning), QStringLiteral("color: #b26a00;")},
      {static_cast<int>(Status::Danger), QStringLiteral("color: #d93025;")},
  };
  QLabel label;
  applyStatusColor(&label, static_cast<Status>(status));
  QCOMPARE(label.styleSheet(), always.value(status));
}

void StatusColorsTests::statusColor_withTheStyleLayer_isThePaletteOfTheLook_data()
{
  QTest::addColumn<bool>("dark");
  QTest::addColumn<int>("status");
  for (const auto dark : {false, true}) {
    for (const auto status : {Status::Success, Status::Warning, Status::Danger}) {
      QTest::addRow("%s, %d", dark ? "dark" : "light", static_cast<int>(status)) << dark << static_cast<int>(status);
    }
  }
}

void StatusColorsTests::statusColor_withTheStyleLayer_isThePaletteOfTheLook()
{
  QFETCH(bool, dark);
  QFETCH(int, status);
  install(dark);
  const auto &tokens = tokensFor(lookOf(dark));

  QLabel label;
  applyStatusColor(&label, static_cast<Status>(status));
  // no style sheet: the label keeps the style of the application
  QVERIFY(label.styleSheet().isEmpty());
  const auto color = label.palette().color(QPalette::WindowText);
  QCOMPARE(color, tokenColorOf(tokens, static_cast<Status>(status)));
  // and the colour reads on the surface that the label is on, in the light and in the dark
  QVERIFY(contrastRatio(color, tokens.colors.surface) >= 4.5);
}

void StatusColorsTests::statusColor_afterTheStyleLayerAnother_leavesNoOldColour()
{
  install(false);
  QLabel label;
  applyStatusColor(&label, Status::Danger);
  applyStatusColor(&label, Status::Success);
  QCOMPARE(label.palette().color(QPalette::WindowText), tokensFor(lookOf(false)).colors.success);
  QVERIFY(label.styleSheet().isEmpty());
}

void StatusColorsTests::errorFill_withoutTheStyleLayer_isCrimsonAndWhite()
{
  const auto fill = errorFill();
  QCOMPARE(fill.background, QColor(QStringLiteral("crimson")));
  QCOMPARE(fill.text, QColor(Qt::white));
}

void StatusColorsTests::errorFill_withTheStyleLayer_hasTheContrastInBothLooks_data()
{
  QTest::addColumn<bool>("dark");
  QTest::newRow("light") << false;
  QTest::newRow("dark") << true;
}

void StatusColorsTests::errorFill_withTheStyleLayer_hasTheContrastInBothLooks()
{
  QFETCH(bool, dark);
  install(dark);
  const auto fill = errorFill();
  QCOMPARE(fill.background, tokensFor(lookOf(dark)).colors.danger);
  QVERIFY2(
      contrastRatio(fill.text, fill.background) >= 4.5,
      qPrintable(QString::number(contrastRatio(fill.text, fill.background)))
  );
}

void StatusColorsTests::invalidLineEdit_withoutTheStyleLayer_hasTheCrimsonBorderThatItAlwaysHad()
{
  QVERIFY(!isDeskflowStyleInstalled());
  QLineEdit edit;
  validators::LineEditValidator validator(&edit);
  validator.addValidator(std::make_unique<validators::EmptyStringValidator>(QStringLiteral("empty")));
  QString input;
  int position = 0;
  QCOMPARE(validator.validate(input, position), QValidator::Intermediate);
  QVERIFY(edit.styleSheet().contains(QStringLiteral("solid crimson")));

  input = QStringLiteral("name");
  QCOMPARE(validator.validate(input, position), QValidator::Acceptable);
  QVERIFY(edit.styleSheet().isEmpty());
}

void StatusColorsTests::invalidLineEdit_withTheStyleLayer_isMarkedForTheStyleAndHasNoStyleSheet()
{
  install(false);
  QLineEdit edit;
  validators::LineEditValidator validator(&edit);
  validator.addValidator(std::make_unique<validators::EmptyStringValidator>(QStringLiteral("empty")));
  QString input;
  int position = 0;
  QCOMPARE(validator.validate(input, position), QValidator::Intermediate);
  QVERIFY(edit.styleSheet().isEmpty());
  QVERIFY(edit.property(DeskflowStyle::kInvalidProperty).toBool());

  input = QStringLiteral("name");
  QCOMPARE(validator.validate(input, position), QValidator::Acceptable);
  QVERIFY(!edit.property(DeskflowStyle::kInvalidProperty).toBool());
}

void StatusColorsTests::validationError_withTheStyleLayer_isOnTheFillOfAnError()
{
  install(true);
  QLabel label;
  validators::ValidationError error(nullptr, &label);
  error.setMessage(QStringLiteral("not valid"));
  const auto fill = errorFill();
  QVERIFY(label.autoFillBackground());
  QCOMPARE(label.palette().color(QPalette::Window), fill.background);
  QCOMPARE(label.palette().color(QPalette::WindowText), fill.text);
}

QTEST_MAIN(StatusColorsTests)
