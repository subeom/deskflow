/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "DisclosureBox.h"

#include "gui/style/AccessibleInterfaces.h"
#include "gui/style/Appearance.h"
#include "gui/style/DesignTokens.h"
#include "gui/style/Icons.h"
#include "gui/style/TextSize.h"

#include <QPainter>
#include <QStyle>
#include <QStyleOptionGroupBox>

namespace {

constexpr int kChevron = 16;

} // namespace

DisclosureBox::DisclosureBox(const QString &title, QWidget *parent) : QGroupBox(title, parent)
{
  installAccessibleInterfaces();
  setFlat(true);
  setCheckable(true);
  setChecked(false);
}

void DisclosureBox::focusInEvent(QFocusEvent *event)
{
  QGroupBox::focusInEvent(event);
  update();
}

void DisclosureBox::focusOutEvent(QFocusEvent *event)
{
  QGroupBox::focusOutEvent(event);
  update();
}

void DisclosureBox::paintEvent(QPaintEvent *event)
{
  if (!deskflow::gui::style::isDeskflowStyleInstalled()) {
    QGroupBox::paintEvent(event);
    return;
  }
  using namespace deskflow::gui::style;
  QStyleOptionGroupBox option;
  initStyleOption(&option);
  const auto indicator = style()->subControlRect(QStyle::CC_GroupBox, &option, QStyle::SC_GroupBoxCheckBox, this);
  const auto label = style()->subControlRect(QStyle::CC_GroupBox, &option, QStyle::SC_GroupBoxLabel, this);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const auto mode = isEnabled() ? QIcon::Normal : QIcon::Disabled;
  const auto chevron = icon(isChecked() ? QStringLiteral("caret-down") : QStringLiteral("caret-right"));
  const QRect where(0, 0, px(kChevron), px(kChevron));
  chevron.paint(&painter, QRect(where.translated(indicator.center() - where.center())), Qt::AlignCenter, mode);

  painter.setPen(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::WindowText));
  painter.drawText(label, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextShowMnemonic, title());

  if (hasFocus()) {
    painter.setPen(QPen(palette().color(QPalette::Highlight), metrics::focusRingWidth));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(indicator.united(label)).adjusted(-3, -2, 3, 2), 6, 6);
  }
}
