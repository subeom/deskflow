/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "PageTitle.h"

#include "gui/style/AccessibleInterfaces.h"
#include "gui/style/Appearance.h"
#include "gui/style/DesignTokens.h"
#include "gui/style/FontScale.h"
#include "gui/style/LookFollower.h"

#include <QApplication>

using namespace deskflow::gui::style;

PageTitle::PageTitle(QWidget *parent) : QLabel(parent)
{
  init();
}

PageTitle::PageTitle(const QString &text, QWidget *parent) : QLabel(text, parent)
{
  init();
}

void PageTitle::init()
{
  installAccessibleInterfaces();
  setTextFormat(Qt::PlainText);
  new LookFollower(this, [this] {
    auto font = scaleFont(QApplication::font(this), qreal(metrics::fontTitle) / metrics::fontBody);
    font.setWeight(QFont::DemiBold);
    setFont(font);
    auto palette = this->palette();
    palette.setColor(QPalette::WindowText, currentTokens().colors.text);
    setPalette(palette);
  });
}
