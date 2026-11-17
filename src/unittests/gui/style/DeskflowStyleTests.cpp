/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "DeskflowStyleTests.h"

#include "gui/style/DesignTokens.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QImage>
#include <QLineEdit>
#include <QListView>
#include <QPainter>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStyleFactory>
#include <QStyleOptionButton>
#include <QStyleOptionFrame>

#include <memory>

using namespace deskflow::gui::style;

namespace {

Look lookOf(bool dark)
{
  Look look;
  look.scheme = dark ? Scheme::Dark : Scheme::Light;
  return look;
}

bool near(const QColor &a, const QColor &b, int tolerance = 8)
{
  return qAbs(a.red() - b.red()) <= tolerance && qAbs(a.green() - b.green()) <= tolerance &&
         qAbs(a.blue() - b.blue()) <= tolerance;
}

QString describe(const QColor &color)
{
  return color.name();
}

QImage pictureOf(QWidget &widget, const QSize &size)
{
  widget.resize(size);
  return widget.grab().toImage();
}

// What the style paints for an option, on the background of the window
QImage paint(const QSize &size, const std::function<void(QPainter &)> &draw)
{
  QImage image(size, QImage::Format_ARGB32_Premultiplied);
  image.fill(QApplication::palette().color(QPalette::Window));
  QPainter painter(&image);
  draw(painter);
  return image;
}

void addLooks()
{
  QTest::addColumn<bool>("dark");
  QTest::newRow("light") << false;
  QTest::newRow("dark") << true;
}

} // namespace

void DeskflowStyleTests::init()
{
  m_style = new DeskflowStyle;
  QApplication::setStyle(m_style);
  m_style->useLook(lookOf(false));
}

void DeskflowStyleTests::cleanup()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  QApplication::setPalette(QPalette());
  m_style = nullptr;
}

void DeskflowStyleTests::polish_givesTheApplicationThePaletteOfTheLook_data()
{
  addLooks();
}

void DeskflowStyleTests::polish_givesTheApplicationThePaletteOfTheLook()
{
  QFETCH(bool, dark);
  m_style->useLook(lookOf(dark));
  const auto &c = tokensFor(lookOf(dark)).colors;
  QCOMPARE(QApplication::palette().color(QPalette::Window), c.bg);
  QCOMPARE(QApplication::palette().color(QPalette::Base), c.surface);
  QCOMPARE(QApplication::palette().color(QPalette::Highlight), c.accent);
  QCOMPARE(m_style->tokens().colors.accent, c.accent);
  QCOMPARE(m_style->look().scheme, dark ? Scheme::Dark : Scheme::Light);
}

void DeskflowStyleTests::lookChanged_isEmittedOnlyWhenTheLookIsOther()
{
  QSignalSpy spy(m_style, &DeskflowStyle::lookChanged);
  m_style->useLook(lookOf(false));
  QCOMPARE(spy.count(), 0);
  m_style->useLook(lookOf(true));
  QCOMPARE(spy.count(), 1);
  m_style->useLook(lookOf(true));
  QCOMPARE(spy.count(), 1);
}

void DeskflowStyleTests::pushButton_default_isACapsuleInTheAccentWithTheTextOfTheAccent_data()
{
  addLooks();
}

void DeskflowStyleTests::pushButton_default_isACapsuleInTheAccentWithTheTextOfTheAccent()
{
  QFETCH(bool, dark);
  m_style->useLook(lookOf(dark));
  const auto &c = m_style->tokens().colors;

  QPushButton button(QStringLiteral("Apply"));
  button.setDefault(true);
  const auto image = pictureOf(button, {140, 32});

  // a capsule: the corners are the window, the ends and the middle are the accent
  QVERIFY2(near(image.pixelColor(2, 2), c.bg), qPrintable(describe(image.pixelColor(2, 2))));
  QVERIFY2(near(image.pixelColor(137, 29), c.bg), qPrintable(describe(image.pixelColor(137, 29))));
  QVERIFY2(near(image.pixelColor(10, 16), c.accent), qPrintable(describe(image.pixelColor(10, 16))));
  QVERIFY2(near(image.pixelColor(130, 16), c.accent), qPrintable(describe(image.pixelColor(130, 16))));

  // the text is in the colour for text on the accent, not in the colour of the text of a button
  int onAccent = 0;
  int buttonText = 0;
  for (int y = 8; y < 24; ++y) {
    for (int x = 40; x < 100; ++x) {
      onAccent += near(image.pixelColor(x, y), c.onAccent, 24) ? 1 : 0;
      buttonText += near(image.pixelColor(x, y), c.text, 24) ? 1 : 0;
    }
  }
  QVERIFY2(onAccent > 10, qPrintable(QString::number(onAccent)));
  QCOMPARE(buttonText, 0);
}

void DeskflowStyleTests::pushButton_secondary_hasTheRadiusOfInputsAndTheBorderOfControls_data()
{
  addLooks();
}

void DeskflowStyleTests::pushButton_secondary_hasTheRadiusOfInputsAndTheBorderOfControls()
{
  QFETCH(bool, dark);
  m_style->useLook(lookOf(dark));
  const auto &c = m_style->tokens().colors;

  QPushButton button(QStringLiteral("Cancel"));
  const auto image = pictureOf(button, {140, 32});

  // 12 px radius: the corner is the window, the border is 1 px of the control border colour
  QVERIFY2(near(image.pixelColor(1, 1), c.bg), qPrintable(describe(image.pixelColor(1, 1))));
  QVERIFY2(near(image.pixelColor(70, 0), c.controlBorder), qPrintable(describe(image.pixelColor(70, 0))));
  QVERIFY2(near(image.pixelColor(0, 16), c.controlBorder), qPrintable(describe(image.pixelColor(0, 16))));
  QVERIFY2(near(image.pixelColor(70, 31), c.controlBorder), qPrintable(describe(image.pixelColor(70, 31))));
  // and inside it a surface, not the window and not the accent
  QVERIFY2(near(image.pixelColor(10, 16), c.surface), qPrintable(describe(image.pixelColor(10, 16))));
}

void DeskflowStyleTests::pushButton_disabled_hasNoAccent()
{
  const auto &c = m_style->tokens().colors;
  QPushButton button(QStringLiteral("Apply"));
  button.setDefault(true);
  button.setEnabled(false);
  const auto image = pictureOf(button, {140, 32});
  QVERIFY2(!near(image.pixelColor(10, 16), c.accent, 20), qPrintable(describe(image.pixelColor(10, 16))));
}

void DeskflowStyleTests::pushButton_withKeyboardFocus_hasTheRingInsideItsEdge()
{
  const auto &c = m_style->tokens().colors;
  const auto draw = [&](bool focus) {
    return paint({140, 32}, [&](QPainter &painter) {
      QStyleOptionButton option;
      option.rect = QRect(0, 0, 140, 32);
      option.palette = QApplication::palette();
      option.state = QStyle::State_Enabled |
                     (focus ? QStyle::State_HasFocus | QStyle::State_KeyboardFocusChange : QStyle::State_None);
      m_style->drawControl(QStyle::CE_PushButton, &option, &painter, nullptr);
    });
  };

  const auto focused = draw(true);
  QVERIFY2(near(focused.pixelColor(70, 0), c.accent), qPrintable(describe(focused.pixelColor(70, 0))));
  QVERIFY2(near(focused.pixelColor(70, 1), c.accent), qPrintable(describe(focused.pixelColor(70, 1))));
  // the same button without the focus: the border is 1 px, the second row is the inside of the button
  const auto plain = draw(false);
  QVERIFY2(!near(plain.pixelColor(70, 1), c.accent), qPrintable(describe(plain.pixelColor(70, 1))));
}

void DeskflowStyleTests::pushButton_atAFractionalScale_hasABorderOfWholeDevicePixels()
{
  const auto &c = m_style->tokens().colors;
  // 125 %: a logical pixel is 1.25 device pixels; the border must still be one row of device pixels, not a blend
  QImage image(QSize(150, 40), QImage::Format_ARGB32_Premultiplied);
  image.setDevicePixelRatio(1.25);
  image.fill(QApplication::palette().color(QPalette::Window));
  {
    QPainter painter(&image);
    QStyleOptionButton option;
    option.rect = QRect(0, 0, 120, 32);
    option.palette = QApplication::palette();
    option.state = QStyle::State_Enabled;
    m_style->drawPrimitive(QStyle::PE_PanelButtonCommand, &option, &painter, nullptr);
  }
  const auto column = 75;
  QVERIFY2(near(image.pixelColor(column, 0), c.controlBorder), qPrintable(describe(image.pixelColor(column, 0))));
  QVERIFY2(near(image.pixelColor(column, 1), c.surface), qPrintable(describe(image.pixelColor(column, 1))));
  // and the same at the bottom edge: 32 logical px are 40 device px
  QVERIFY2(near(image.pixelColor(column, 39), c.controlBorder), qPrintable(describe(image.pixelColor(column, 39))));
  QVERIFY2(near(image.pixelColor(column, 38), c.surface), qPrintable(describe(image.pixelColor(column, 38))));
}

void DeskflowStyleTests::lineEdit_isRounded_andInvalidIsDangerColoured()
{
  const auto &c = m_style->tokens().colors;
  QLineEdit edit(QStringLiteral("0.0.0.0"));
  auto image = pictureOf(edit, {160, 30});
  QVERIFY2(near(image.pixelColor(1, 1), c.bg), qPrintable(describe(image.pixelColor(1, 1))));
  QVERIFY2(near(image.pixelColor(100, 0), c.controlBorder), qPrintable(describe(image.pixelColor(100, 0))));
  QVERIFY2(near(image.pixelColor(100, 15), c.surface), qPrintable(describe(image.pixelColor(100, 15))));

  edit.setProperty(DeskflowStyle::kInvalidProperty, true);
  image = pictureOf(edit, {160, 30});
  QVERIFY2(near(image.pixelColor(100, 0), c.danger), qPrintable(describe(image.pixelColor(100, 0))));
}

void DeskflowStyleTests::lineEdit_withFocus_hasTheAccentBorder()
{
  const auto &c = m_style->tokens().colors;
  const auto image = paint({160, 30}, [&](QPainter &painter) {
    QStyleOptionFrame option;
    option.rect = QRect(0, 0, 160, 30);
    option.palette = QApplication::palette();
    option.lineWidth = 1;
    option.state = QStyle::State_Enabled | QStyle::State_HasFocus;
    m_style->drawPrimitive(QStyle::PE_PanelLineEdit, &option, &painter, nullptr);
  });
  QVERIFY2(near(image.pixelColor(100, 0), c.accent), qPrintable(describe(image.pixelColor(100, 0))));
  QVERIFY2(near(image.pixelColor(100, 1), c.accent), qPrintable(describe(image.pixelColor(100, 1))));
}

void DeskflowStyleTests::comboBoxAndSpinBox_haveTheBorderOfInputs()
{
  const auto &c = m_style->tokens().colors;
  QComboBox combo;
  combo.addItem(QStringLiteral("Barrier"));
  auto image = pictureOf(combo, {160, 30});
  QVERIFY2(near(image.pixelColor(1, 1), c.bg), qPrintable(describe(image.pixelColor(1, 1))));
  QVERIFY2(near(image.pixelColor(80, 0), c.controlBorder), qPrintable(describe(image.pixelColor(80, 0))));

  QSpinBox spin;
  image = pictureOf(spin, {100, 30});
  QVERIFY2(near(image.pixelColor(1, 1), c.bg), qPrintable(describe(image.pixelColor(1, 1))));
  QVERIFY2(near(image.pixelColor(50, 0), c.controlBorder), qPrintable(describe(image.pixelColor(50, 0))));
}

void DeskflowStyleTests::checkBox_isAToggle_inTheAccentWhenChecked_data()
{
  addLooks();
}

void DeskflowStyleTests::checkBox_isAToggle_inTheAccentWhenChecked()
{
  QFETCH(bool, dark);
  m_style->useLook(lookOf(dark));
  const auto &c = m_style->tokens().colors;

  QCheckBox box;
  QStyleOptionButton option;
  option.initFrom(&box);
  QCOMPARE(m_style->pixelMetric(QStyle::PM_IndicatorWidth, &option, &box), metrics::toggleCompactWidth);
  QCOMPARE(m_style->pixelMetric(QStyle::PM_IndicatorHeight, &option, &box), metrics::toggleCompactHeight);

  const auto toggle = [&](bool on) {
    return paint({36, 20}, [&](QPainter &painter) {
      QStyleOptionButton indicator;
      indicator.rect = QRect(0, 0, 36, 20);
      indicator.palette = QApplication::palette();
      indicator.state = QStyle::State_Enabled | (on ? QStyle::State_On : QStyle::State_Off);
      m_style->drawPrimitive(QStyle::PE_IndicatorCheckBox, &indicator, &painter, &box);
    });
  };

  // on: an accent track with the knob at the right in the colour for what is on the accent
  const auto on = toggle(true);
  QVERIFY2(near(on.pixelColor(6, 10), c.accent), qPrintable(describe(on.pixelColor(6, 10))));
  QVERIFY2(near(on.pixelColor(26, 10), c.onAccent), qPrintable(describe(on.pixelColor(26, 10))));
  // off: a surface track with a border and the knob at the left in the colour of the border
  const auto off = toggle(false);
  QVERIFY2(near(off.pixelColor(18, 0), c.controlBorder), qPrintable(describe(off.pixelColor(18, 0))));
  QVERIFY2(near(off.pixelColor(26, 10), c.surface), qPrintable(describe(off.pixelColor(26, 10))));
  QVERIFY2(near(off.pixelColor(10, 10), c.controlBorder), qPrintable(describe(off.pixelColor(10, 10))));
}

void DeskflowStyleTests::checkBox_inAnItemView_staysACheckBox()
{
  std::unique_ptr<QStyle> fusion(QStyleFactory::create(QStringLiteral("Fusion")));
  QListView view;
  QStyleOptionButton option;
  QCOMPARE(
      m_style->pixelMetric(QStyle::PM_IndicatorWidth, &option, &view),
      fusion->pixelMetric(QStyle::PM_IndicatorWidth, &option, &view)
  );
  QCOMPARE(
      m_style->pixelMetric(QStyle::PM_IndicatorHeight, &option, &view),
      fusion->pixelMetric(QStyle::PM_IndicatorHeight, &option, &view)
  );
}

void DeskflowStyleTests::sizes_neverShrinkBelowFusion_andButtonsAreAtLeastThirtyHigh()
{
  std::unique_ptr<QStyle> fusion(QStyleFactory::create(QStringLiteral("Fusion")));

  const auto check = [&](QWidget &widget, const char *what) {
    const auto ours = widget.sizeHint();
    widget.setStyle(fusion.get());
    const auto theirs = widget.sizeHint();
    widget.setStyle(m_style);
    QVERIFY2(ours.width() >= theirs.width(), what);
    QVERIFY2(ours.height() >= theirs.height(), what);
  };

  QPushButton button(QStringLiteral("Restart"));
  check(button, "button");
  QVERIFY(button.sizeHint().height() >= 30);
  QLineEdit edit(QStringLiteral("name"));
  check(edit, "line edit");
  QVERIFY(edit.sizeHint().height() >= 30);
  QComboBox combo;
  combo.addItem(QStringLiteral("Barrier"));
  check(combo, "combo box");
  QVERIFY(combo.sizeHint().height() >= 30);
  QSpinBox spin;
  check(spin, "spin box");
  QVERIFY(spin.sizeHint().height() >= 30);
  QCheckBox box(QStringLiteral("Enable"));
  check(box, "check box");
}

QTEST_MAIN(DeskflowStyleTests)
