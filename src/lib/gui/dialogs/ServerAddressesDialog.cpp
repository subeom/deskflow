/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerAddressesDialog.h"

#include <QDialogButtonBox>
#include <QEvent>
#include <QTextBrowser>
#include <QVBoxLayout>

using namespace deskflow::gui;

ServerAddressesDialog::ServerAddressesDialog(QWidget *parent, const QList<AddressInfo> &addresses)
    : QDialog(parent),
      m_addresses(addresses),
      m_view(new QTextBrowser(this)),
      m_buttons(new QDialogButtonBox(QDialogButtonBox::Close, this))
{
  m_view->setObjectName(QStringLiteral("addressesView"));
  m_view->setOpenLinks(false);
  // a few lines of the font in use, so that the panel is never a sliver; the text that is long is in the panel,
  // which scrolls, so no wrapped label asks for a height that depends on the width
  m_view->setMinimumHeight(fontMetrics().lineSpacing() * 10);
  m_view->setMinimumWidth(fontMetrics().averageCharWidth() * 56);
  connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  auto *layout = new QVBoxLayout(this);
  layout->addWidget(m_view, 1);
  layout->addWidget(m_buttons);

  retranslate();
}

QSize ServerAddressesDialog::sizeHint() const
{
  const auto metrics = fontMetrics();
  return QDialog::sizeHint().expandedTo(QSize(metrics.averageCharWidth() * 72, metrics.lineSpacing() * 26));
}

void ServerAddressesDialog::changeEvent(QEvent *event)
{
  QDialog::changeEvent(event);
  if (event->type() == QEvent::FontChange) {
    // the panel is as many lines and letters as the font in use, also when the font is changed after the dialog was
    // made
    m_view->setMinimumHeight(fontMetrics().lineSpacing() * 10);
    m_view->setMinimumWidth(fontMetrics().averageCharWidth() * 56);
  }
  if (event->type() == QEvent::LanguageChange) {
    retranslate();
  }
}

void ServerAddressesDialog::retranslate()
{
  setWindowTitle(tr("Server addresses"));
  const auto intro = tr("A client reaches this server by one of these addresses.").toHtmlEscaped();
  m_view->setHtml(QStringLiteral("<p>%1</p>%2").arg(intro, addressListHtml(m_addresses)));
}
