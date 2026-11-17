/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/widgets/TunnelStatusWidget.h"

#include "gui/tunnel/TunnelController.h"

#include <QLabel>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace deskflow::gui {

using namespace deskflow::tunnel;

namespace {

QLabel *makeLine(const QString &name, QWidget *parent)
{
  auto *label = new QLabel(parent);
  label->setObjectName(name);
  // a line that does not wrap: a window does not grow for wrapped text, so what does not fit is cut and is in the tool
  // tip
  label->setWordWrap(false);
  label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  return label;
}

bool isRunning(TunnelState state)
{
  return state == TunnelState::Starting || state == TunnelState::Listening || state == TunnelState::Backoff ||
         state == TunnelState::Stopping;
}

} // namespace

TunnelStatusWidget::TunnelStatusWidget(TunnelController *controller, QWidget *parent)
    : QWidget(parent),
      m_controller(controller),
      m_pathLabel(makeLine(QStringLiteral("tunnelPathLabel"), this)),
      m_statusLabel(makeLine(QStringLiteral("tunnelStatusLabel"), this))
{
  setObjectName(QStringLiteral("tunnelStatusWidget"));

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(2);
  layout->addWidget(m_pathLabel);
  layout->addWidget(m_statusLabel);

  connect(controller, &TunnelController::statusChanged, this, &TunnelStatusWidget::refresh);
  refresh();
}

void TunnelStatusWidget::setServerLink(ServerLink link)
{
  if (m_link == link) {
    return;
  }
  m_link = link;
  refresh();
}

QString TunnelStatusWidget::statusText() const
{
  return m_statusLabel->text();
}

void TunnelStatusWidget::refresh()
{
  const bool used = m_controller->isRequired();
  const auto config = m_controller->config();
  const auto status = m_controller->status();

  const auto port = isRunning(status.state) && status.localPort > 0 ? status.localPort : config.port();
  const auto path = tunnelPathLine(config.hostname, port);
  const auto line = tunnelStatusLine(status, m_link);

  m_pathLabel->setText(path);
  m_pathLabel->setToolTip(path);
  const bool textChanged = m_statusLabel->text() != line;
  m_statusLabel->setText(line);
  m_statusLabel->setToolTip(line);

  setVisible(used);
  if (used != m_used) {
    m_used = used;
    Q_EMIT usedChanged(used);
  }
  if (textChanged) {
    Q_EMIT statusTextChanged(line);
  }
}

} // namespace deskflow::gui
