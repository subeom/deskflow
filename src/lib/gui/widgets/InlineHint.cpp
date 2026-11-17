/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "InlineHint.h"

#include "gui/style/Appearance.h"
#include "gui/style/FontScale.h"
#include "gui/style/LookFollower.h"

#include <QApplication>

using namespace deskflow::gui::style;

InlineHint::InlineHint(QWidget *parent) : QLabel(parent)
{
  setWordWrap(true);
  setTextFormat(Qt::PlainText);
  // a hint is read, it is not selected and it never takes the keyboard
  setTextInteractionFlags(Qt::NoTextInteraction);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
  new LookFollower(this, [this] { applyLook(); });
}

InlineHint::InlineHint(const QString &text, QWidget *parent) : InlineHint(parent)
{
  setText(text);
}

void InlineHint::applyLook()
{
  // the size is a ratio of the size the system gives text, so that it follows the text size that the user set
  auto font = scaleFont(QApplication::font(this), qreal(metrics::fontCaption) / metrics::fontBody);
  setFont(font);

  auto palette = this->palette();
  const auto &c = currentTokens().colors;
  palette.setColor(QPalette::WindowText, c.textSecondary);
  palette.setColor(QPalette::Disabled, QPalette::WindowText, c.textDisabled);
  setPalette(palette);
}
