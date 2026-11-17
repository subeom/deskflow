/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "StatusColors.h"

#include "Appearance.h"
#include "Contrast.h"

#include <QLabel>
#include <QPalette>

namespace deskflow::gui::style {

namespace {

// What the program has always used, when it has no style layer
QString legacyStyleSheet(Status status)
{
  switch (status) {
  case Status::Success:
    return QStringLiteral("color: #1e8e3e;");
  case Status::Warning:
    return QStringLiteral("color: #b26a00;");
  case Status::Danger:
    break;
  }
  return QStringLiteral("color: #d93025;");
}

QColor tokenColor(const Tokens &tokens, Status status)
{
  switch (status) {
  case Status::Success:
    return tokens.colors.success;
  case Status::Warning:
    return tokens.colors.warning;
  case Status::Danger:
    break;
  }
  return tokens.colors.danger;
}

} // namespace

void applyStatusColor(QLabel *label, Status status)
{
  if (!isDeskflowStyleInstalled()) {
    label->setStyleSheet(legacyStyleSheet(status));
    return;
  }

  label->setStyleSheet({});
  auto palette = label->palette();
  palette.setColor(QPalette::WindowText, tokenColor(currentTokens(), status));
  label->setPalette(palette);
}

Fill errorFill()
{
  if (!isDeskflowStyleInstalled())
    return {QColor(QStringLiteral("crimson")), QColor(Qt::white)};

  const auto background = currentTokens().colors.danger;
  const auto text = contrastRatio(Qt::white, background) >= contrastRatio(Qt::black, background) ? QColor(Qt::white)
                                                                                                 : QColor(Qt::black);
  return {background, text};
}

} // namespace deskflow::gui::style
