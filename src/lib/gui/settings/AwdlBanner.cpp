/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AwdlBanner.h"

#include "gui/awdl/AwdlTexts.h"
#include "gui/style/AccessibleInterfaces.h"
#include "gui/style/Appearance.h"
#include "gui/style/Icons.h"
#include "gui/style/LookFollower.h"
#include "gui/style/TextSize.h"

#include <QAccessible>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

using namespace deskflow::gui::style;

namespace deskflow::gui::settings {

AwdlBanner::AwdlBanner(QWidget *parent)
    : PanelWidget(Density::Dense, parent),
      m_icon(new QLabel(this)),
      m_text(new QLabel(this)),
      m_button(new QPushButton(this))
{
  installAccessibleInterfaces();
  setObjectName(QStringLiteral("awdlBanner"));
  setPadding(metrics::spaceLg);

  m_text->setWordWrap(true);
  m_text->setTextFormat(Qt::PlainText);
  m_text->setTextInteractionFlags(Qt::NoTextInteraction);
  m_button->setObjectName(QStringLiteral("btnAwdlBannerTurnOff"));
  m_button->setDefault(false);

  auto *layout = new QHBoxLayout(this);
  layout->setSpacing(metrics::spaceMd);
  layout->addWidget(m_icon, 0, Qt::AlignVCenter);
  layout->addWidget(m_text, 1);
  layout->addWidget(m_button, 0, Qt::AlignVCenter);

  // the icon and the button are as large as the size of text asks
  const auto sizeParts = [this] {
    m_icon->setFixedSize(px(24), px(24));
    m_button->setMinimumHeight(px(36));
  };
  sizeParts();
  onTextSizeChanged(this, sizeParts);

  connect(m_button, &QPushButton::clicked, this, &AwdlBanner::turnOffRequested);
  new LookFollower(this, [this] { applyLook(); });
  retranslate();
  hide();
}

void AwdlBanner::retranslate()
{
  m_text->setText(deskflow::awdl::bannerText(m_stats));
  m_button->setText(tr("Turn off"));
  setAccessibleName(m_text->text());
}

void AwdlBanner::applyLook()
{
  m_icon->setPixmap(icon(QStringLiteral("warning"), IconColor::Warning).pixmap(QSize(20, 20)));
}

void AwdlBanner::setLeastWidth(int width)
{
  if (m_leastWidth == width)
    return;
  m_leastWidth = width;
  updateGeometry();
}

int AwdlBanner::leastWidth() const
{
  return m_leastWidth;
}

QSize AwdlBanner::minimumSizeHint() const
{
  auto hint = PanelWidget::minimumSizeHint();
  if (m_leastWidth > 0 && hasHeightForWidth())
    hint.setHeight(qMax(hint.height(), heightForWidth(m_leastWidth)));
  return hint;
}

QPushButton *AwdlBanner::button() const
{
  return m_button;
}

QString AwdlBanner::text() const
{
  return m_text->text();
}

void AwdlBanner::setStats(const deskflow::awdl::BurstStats &stats)
{
  m_stats = stats;
  m_text->setText(deskflow::awdl::bannerText(m_stats));
  setAccessibleName(m_text->text());
}

void AwdlBanner::setBusy(bool busy)
{
  m_button->setEnabled(!busy);
}

void AwdlBanner::setPending(bool pending)
{
  if (pending == !isHidden())
    return;
  setVisible(pending);
  // a line that comes while the person is somewhere else on the page is read when it comes
  if (pending && isVisible()) {
    QAccessibleEvent event(this, QAccessible::Alert);
    QAccessible::updateAccessibility(&event);
  }
}

bool AwdlBanner::isPending() const
{
  return !isHidden();
}

} // namespace deskflow::gui::settings
