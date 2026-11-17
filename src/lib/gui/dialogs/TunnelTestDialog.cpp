/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TunnelTestDialog.h"

#include <QClipboard>
#include <QDialogButtonBox>
#include <QEvent>
#include <QGuiApplication>
#include <QLabel>
#include <QPushButton>
#include <QSysInfo>
#include <QTextBrowser>
#include <QVBoxLayout>

using namespace deskflow::tunnel;

TunnelTestDialog::TunnelTestDialog(
    QWidget *parent, TunnelTestRunner::Dependencies dependencies, ClipboardWriter clipboardWriter
)
    : QDialog(parent),
      m_runner(new TunnelTestRunner(std::move(dependencies), this)),
      m_clipboardWriter(std::move(clipboardWriter)),
      m_resultsView(new QTextBrowser(this)),
      m_status(new QLabel(this)),
      m_copyButton(new QPushButton(this)),
      m_runAgainButton(new QPushButton(this)),
      m_buttons(new QDialogButtonBox(QDialogButtonBox::Close, this))
{
  m_resultsView->setObjectName(QStringLiteral("resultsView"));
  m_resultsView->setOpenLinks(false);
  m_resultsView->setMinimumHeight(fontMetrics().lineSpacing() * 16);
  m_resultsView->setMinimumWidth(fontMetrics().averageCharWidth() * 64);
  // one short line: the long text is in the panel, which scrolls
  m_status->setObjectName(QStringLiteral("lblStatus"));
  m_copyButton->setObjectName(QStringLiteral("btnCopySummary"));
  m_runAgainButton->setObjectName(QStringLiteral("btnRunAgain"));

  m_buttons->addButton(m_copyButton, QDialogButtonBox::ActionRole);
  m_buttons->addButton(m_runAgainButton, QDialogButtonBox::ActionRole);
  connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(m_copyButton, &QPushButton::clicked, this, &TunnelTestDialog::copySummary);
  connect(m_runAgainButton, &QPushButton::clicked, this, &TunnelTestDialog::runTest);
  connect(m_runner, &TunnelTestRunner::changed, this, &TunnelTestDialog::showReport);

  auto *layout = new QVBoxLayout(this);
  layout->addWidget(m_resultsView, 1);
  layout->addWidget(m_status);
  layout->addWidget(m_buttons);

  retranslate();
}

QString TunnelTestDialog::summary() const
{
  return m_runner->report().summaryLine(m_startedAt, QSysInfo::prettyProductName());
}

bool TunnelTestDialog::isTesting() const
{
  return m_runner->isRunning();
}

QString TunnelTestDialog::hintText(TunnelFailure failure)
{
  switch (failure) {
  case TunnelFailure::None:
    return {};
  case TunnelFailure::TlsRequired:
    return tr(
        "The tunnel carries the connection as it is: without TLS it would be readable on the way. Turn on "
        "encryption in Settings."
    );
  case TunnelFailure::InvalidHostname:
    return tr(
        "Enter the hostname of the Access application, for example mbp.example.com, without https:// and "
        "without a port."
    );
  case TunnelFailure::CloudflaredMissing:
    return tr("Install cloudflared, or set its path in Configure client. Deskflow does not download it.");
  case TunnelFailure::CredentialsMissing:
    return tr("Open Configure client and enter the Service Token (Client ID and Secret) of the Access application.");
  case TunnelFailure::PortBusy:
    return tr("Another program listens on the local port. Change the local port in Configure client.");
  case TunnelFailure::LoginRequired:
    return tr(
        "The Service Token is wrong, expired, or not in the Service Auth policy of the Access application. "
        "Create a new token in Cloudflare Zero Trust, add it to the policy, and enter it again."
    );
  case TunnelFailure::HostUnresolvable:
    return tr("Check the spelling of the hostname and that its DNS record exists.");
  case TunnelFailure::EdgeUnreachable:
    return tr(
        "This computer cannot reach Cloudflare. Check the network, a proxy or a firewall: cloudflared needs "
        "outbound HTTPS (port 443)."
    );
  case TunnelFailure::HandshakeRejected:
    return tr(
        "Check the Access policy (its Service Auth rule must include this Service Token) and that the tunnel "
        "at the server runs."
    );
  case TunnelFailure::ConnectFailed:
    return tr(
        "Check that Deskflow runs in server mode on the other computer and listens on the port that the "
        "tunnel forwards to."
    );
  case TunnelFailure::StartTimedOut:
  case TunnelFailure::ProcessExited:
    return tr("Look at the log (View menu) for what cloudflared wrote.");
  }
  return {};
}

void TunnelTestDialog::runTest()
{
  m_startedAt = QDateTime::currentDateTime();
  m_runner->start();
}

void TunnelTestDialog::copySummary()
{
  const auto text = summary();
  if (m_clipboardWriter) {
    m_clipboardWriter(text);
  } else {
    QGuiApplication::clipboard()->setText(text);
  }
}

QSize TunnelTestDialog::sizeHint() const
{
  const auto metrics = fontMetrics();
  return QDialog::sizeHint().expandedTo(QSize(metrics.averageCharWidth() * 84, metrics.lineSpacing() * 24));
}

void TunnelTestDialog::changeEvent(QEvent *event)
{
  QDialog::changeEvent(event);
  if (event->type() == QEvent::FontChange) {
    m_resultsView->setMinimumHeight(fontMetrics().lineSpacing() * 16);
    m_resultsView->setMinimumWidth(fontMetrics().averageCharWidth() * 64);
  }
  if (event->type() == QEvent::LanguageChange) {
    retranslate();
  }
}

void TunnelTestDialog::retranslate()
{
  setWindowTitle(tr("Test connection (Cloudflare Tunnel)"));
  m_copyButton->setText(tr("Copy summary"));
  m_copyButton->setToolTip(tr("Copies one line that says what came of each stage, to paste into a message."));
  m_runAgainButton->setText(tr("Run again"));
  showReport();
}

void TunnelTestDialog::showReport()
{
  const auto &report = m_runner->report();
  const bool testing = m_runner->isRunning();

  QString html = QStringLiteral("<p><i>%1</i></p>")
                     .arg(tr("Tests the way from this computer to the server through the Cloudflare Tunnel, stage by "
                             "stage. A connection is opened and held for a few seconds: no Deskflow data is sent and "
                             "no TLS handshake is made, so the certificate of the server is checked by the real "
                             "connection of the client.")
                              .toHtmlEscaped());

  if (!report.hostname.isEmpty()) {
    html += QStringLiteral("<p>%1</p>").arg(tunnelPathLine(report.hostname, report.localPort).toHtmlEscaped());
  }

  for (const auto &result : report.stages) {
    QString marker;
    QString color;
    switch (result.state) {
    case StageState::Ok:
      marker = QStringLiteral("&#10004;");
      color = QStringLiteral("#2e9d4b");
      break;
    case StageState::Failed:
      marker = QStringLiteral("&#10006;");
      color = QStringLiteral("#d64545");
      break;
    case StageState::Running:
      marker = QStringLiteral("&#8230;");
      color = QStringLiteral("#808080");
      break;
    case StageState::Pending:
    case StageState::Skipped:
      marker = QStringLiteral("&#8211;");
      color = QStringLiteral("#808080");
      break;
    }
    html += QStringLiteral("<p style=\"margin-bottom:0\"><span style=\"color:%1\">%2</span> %3")
                .arg(color, marker, TunnelTestReport::stageName(result.stage).toHtmlEscaped());
    if (!result.detail.isEmpty()) {
      html += QStringLiteral(": %1").arg(result.detail.toHtmlEscaped());
    }
    html += QStringLiteral("</p>");
  }

  if (report.finished && report.failure != TunnelFailure::None) {
    html += QStringLiteral("<p><b>%1</b></p>").arg(hintText(report.failure).toHtmlEscaped());
  } else if (report.ok()) {
    html += QStringLiteral("<p>%1</p>")
                .arg(tr("The tunnel carries a connection to the server. If Deskflow still does not connect, check the "
                        "name of this computer in the layout of the server, the fingerprint and the log.")
                         .toHtmlEscaped());
  }
  m_resultsView->setHtml(html);

  if (testing) {
    m_status->setText(tr("Testing…"));
  } else if (report.finished) {
    m_status->setText(report.ok() ? tr("Done: the tunnel works.") : tr("Done: the tunnel does not work."));
  } else {
    m_status->clear();
  }
  m_copyButton->setEnabled(report.finished);
  m_runAgainButton->setEnabled(!testing);
}
