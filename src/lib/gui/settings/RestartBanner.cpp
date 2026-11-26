/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "RestartBanner.h"

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

RestartBanner::RestartBanner(QWidget *parent)
    : PanelWidget(Density::Dense, parent),
      m_icon(new QLabel(this)),
      m_text(new QLabel(this)),
      m_button(new QPushButton(this))
{
  installAccessibleInterfaces();
  setObjectName(QStringLiteral("restartBanner"));
  setPadding(metrics::spaceLg);

  m_text->setWordWrap(true);
  m_text->setTextFormat(Qt::PlainText);
  m_text->setTextInteractionFlags(Qt::NoTextInteraction);
  m_button->setObjectName(QStringLiteral("btnRestartSharing"));
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

  connect(m_button, &QPushButton::clicked, this, &RestartBanner::restartRequested);
  new LookFollower(this, [this] { applyLook(); });
  retranslate();
  hide();
}

void RestartBanner::retranslate()
{
  m_text->setText(tr("Restart sharing to use the changes that you made."));
  m_button->setText(tr("Restart sharing"));
  setAccessibleName(m_text->text());
}

void RestartBanner::applyLook()
{
  m_icon->setPixmap(icon(QStringLiteral("arrow-clockwise"), IconColor::Warning).pixmap(QSize(20, 20)));
}

QPushButton *RestartBanner::button() const
{
  return m_button;
}

QString RestartBanner::text() const
{
  return m_text->text();
}

void RestartBanner::setPending(bool pending)
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

bool RestartBanner::isPending() const
{
  return !isHidden();
}

} // namespace deskflow::gui::settings
