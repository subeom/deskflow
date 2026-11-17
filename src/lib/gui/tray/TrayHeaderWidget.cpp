/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TrayHeaderWidget.h"

#include "gui/style/Appearance.h"
#include "gui/style/FontScale.h"
#include "gui/style/Icons.h"
#include "gui/style/LookFollower.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace deskflow::gui::tray {

using namespace deskflow::gui::style;

namespace {

constexpr int kIconSize = 24;
// the sentence is as wide as this, and goes on a second line when it is longer
constexpr int kDetailWidth = 260;

} // namespace

TrayHeaderWidget::TrayHeaderWidget(QWidget *parent)
    : QWidget(parent),
      m_icon(new QLabel(this)),
      m_headline(new QLabel(this)),
      m_detail(new QLabel(this)),
      m_chip(new StatusChip(this))
{
  // the menu moves over the header with the keys and the pointer: it does not take either
  setFocusPolicy(Qt::NoFocus);
  setAttribute(Qt::WA_TransparentForMouseEvents);

  m_icon->setFixedSize(kIconSize, kIconSize);
  m_icon->setAlignment(Qt::AlignCenter);

  auto headFont = scaleFont(QApplication::font(this), qreal(metrics::fontSubtitle) / metrics::fontBody);
  headFont.setWeight(QFont::DemiBold);
  m_headline->setFont(headFont);
  m_headline->setTextInteractionFlags(Qt::NoTextInteraction);

  m_detail->setFont(scaleFont(QApplication::font(this), qreal(metrics::fontCaption) / metrics::fontBody));
  m_detail->setWordWrap(true);
  m_detail->setFixedWidth(kDetailWidth);
  m_detail->setTextInteractionFlags(Qt::NoTextInteraction);

  auto *text = new QVBoxLayout;
  text->setContentsMargins(0, 0, 0, 0);
  text->setSpacing(2);
  text->addWidget(m_headline);
  text->addWidget(m_detail);

  auto *row = new QHBoxLayout(this);
  row->setContentsMargins(metrics::spaceMd, metrics::spaceMd, metrics::spaceMd, metrics::spaceMd);
  row->setSpacing(metrics::spaceMd);
  row->addWidget(m_icon, 0, Qt::AlignTop);
  row->addLayout(text, 1);
  row->addWidget(m_chip, 0, Qt::AlignTop);

  new LookFollower(this, [this] {
    updateIcon();
    updateColors();
  });
  updateColors();
}

void TrayHeaderWidget::setContent(
    TrayState state, const QString &headline, const QString &detail, const QString &chipText, StatusChip::Kind chipKind
)
{
  m_state = state;
  m_headline->setText(headline);
  m_detail->setText(detail);
  m_detail->setVisible(!detail.isEmpty());
  m_chip->setText(chipText);
  m_chip->setKind(chipKind);
  m_chip->setVisible(!chipText.isEmpty());
  updateIcon();
  // the three parts are one text for a screen reader: what the state is, and what is behind it
  QStringList parts{headline};
  if (!detail.isEmpty())
    parts << detail;
  if (!chipText.isEmpty())
    parts << chipText;
  setAccessibleName(parts.join(QStringLiteral(". ")));
  updateGeometry();
}

QString TrayHeaderWidget::headline() const
{
  return m_headline->text();
}

QString TrayHeaderWidget::detail() const
{
  return m_detail->text();
}

const StatusChip *TrayHeaderWidget::chip() const
{
  return m_chip;
}

QString TrayHeaderWidget::iconName(TrayState state)
{
  switch (state) {
  case TrayState::Connected:
    return QStringLiteral("plugs-connected");
  case TrayState::Waiting:
    return QStringLiteral("wifi-high");
  case TrayState::Error:
    return QStringLiteral("warning");
  case TrayState::Stopped:
  case TrayState::Idle:
    break;
  }
  return QStringLiteral("power");
}

void TrayHeaderWidget::updateIcon()
{
  IconColor color = IconColor::TextSecondary;
  switch (m_state) {
  case TrayState::Connected:
    color = IconColor::Success;
    break;
  case TrayState::Waiting:
    color = IconColor::Accent;
    break;
  case TrayState::Error:
    color = IconColor::Danger;
    break;
  case TrayState::Stopped:
  case TrayState::Idle:
    break;
  }
  m_icon->setPixmap(
      iconPixmap(iconName(m_state), IconWeight::Duotone, iconColor(color, QIcon::Normal, currentTokens()),
                 QSize(kIconSize, kIconSize), devicePixelRatioF())
  );
}

void TrayHeaderWidget::updateColors()
{
  auto palette = m_detail->palette();
  palette.setColor(QPalette::WindowText, currentTokens().colors.textSecondary);
  m_detail->setPalette(palette);
}

} // namespace deskflow::gui::tray
