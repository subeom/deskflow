/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ConnectionTestDialog.h"

#include "common/PlatformInfo.h"
#include "common/Settings.h"

#include <QClipboard>
#include <QDialogButtonBox>
#include <QEvent>
#include <QGuiApplication>
#include <QLabel>
#include <QPushButton>
#include <QSysInfo>
#include <QTextBrowser>
#include <QVBoxLayout>

#include <algorithm>

using namespace deskflow::gui;
namespace diag = deskflow::diagnosis;

namespace {

// a result is slow when the connect alone takes half of what the client allows for connecting, TLS and greeting
bool isSlowForTheClient(const ProbeResult &result, double clientTimeoutSeconds)
{
  return result.elapsedMs * 2 > static_cast<qint64>(clientTimeoutSeconds * 1000.0);
}

QString secondsText(double seconds)
{
  return QLocale().toString(seconds, 'g', 3);
}

} // namespace

ConnectionTestDialog::ConnectionTestDialog(
    QWidget *parent, const QString &servers, int defaultPort, ProbeFactory probeFactory, ClipboardWriter clipboardWriter
)
    : QDialog(parent),
      m_servers(servers),
      m_defaultPort(defaultPort),
      m_probeFactory(std::move(probeFactory)),
      m_clipboardWriter(std::move(clipboardWriter)),
      m_resultsView(new QTextBrowser(this)),
      m_status(new QLabel(this)),
      m_copyButton(new QPushButton(this)),
      m_runAgainButton(new QPushButton(this)),
      m_buttons(new QDialogButtonBox(QDialogButtonBox::Close, this))
{
  m_resultsView->setObjectName(QStringLiteral("resultsView"));
  m_resultsView->setOpenLinks(false);
  // a few lines of the font in use, so that the panel is never a sliver
  m_resultsView->setMinimumHeight(m_resultsView->fontMetrics().lineSpacing() * 16);
  m_resultsView->setMinimumWidth(m_resultsView->fontMetrics().averageCharWidth() * 64);
  // one short line: the text that is long lives in the panel, which scrolls, so no wrapped label asks for a height
  // that depends on the width
  m_status->setObjectName(QStringLiteral("lblStatus"));
  m_copyButton->setObjectName(QStringLiteral("btnCopySummary"));
  m_runAgainButton->setObjectName(QStringLiteral("btnRunAgain"));

  m_buttons->addButton(m_copyButton, QDialogButtonBox::ActionRole);
  m_buttons->addButton(m_runAgainButton, QDialogButtonBox::ActionRole);
  connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(m_copyButton, &QPushButton::clicked, this, &ConnectionTestDialog::copySummary);
  connect(m_runAgainButton, &QPushButton::clicked, this, &ConnectionTestDialog::runTest);

  auto *layout = new QVBoxLayout(this);
  layout->addWidget(m_resultsView, 1);
  layout->addWidget(m_status);
  layout->addWidget(m_buttons);

  retranslate();
}

QString ConnectionTestDialog::summary() const
{
  const auto timeoutMs = m_probe != nullptr ? m_probe->timeoutMs() : ConnectionProbe::kDefaultTimeoutMs;
  return summaryLine(m_results, timeoutMs, m_startedAt, QSysInfo::prettyProductName());
}

bool ConnectionTestDialog::isTesting() const
{
  return m_probe != nullptr && m_probe->isRunning();
}

QString ConnectionTestDialog::hintText(diag::Hint hint, int port, double timeoutSeconds)
{
  using diag::Hint;
  switch (hint) {
  case Hint::ServerFirewall:
    return tr("On the server computer, allow inbound TCP port %1 for Deskflow in its firewall. The client computer "
              "needs no inbound port.")
        .arg(port);
  case Hint::NetworkIsolation:
    return tr(
        "If both computers are on the same network and it still fails, the network may keep computers from "
        "talking to each other (Wi-Fi client isolation, separate VLANs, access lists). Only the people who run "
        "the network (IT) can change that."
    );
  case Hint::CheckAddress:
    return tr(
        "Use the address the server computer has on the network both computers share, not the one of a VPN, "
        "Docker or virtual network card. The window of the server lists its addresses."
    );
  case Hint::StartServer:
    return tr("Check that Deskflow runs in server mode on that computer and that both computers use the same port.");
  case Hint::SwapRoles:
    return tr(
        "The roles can be swapped: let the computer that can accept connections be the server and this one the "
        "client."
    );
  case Hint::MacLocalNetwork:
    return tr(
        "On macOS 15 or later, Deskflow needs permission to reach the local network: open System Settings > "
        "Privacy & Security > Local Network and switch Deskflow on."
    );
  case Hint::VpnLocalNetwork:
    return tr(
        "A VPN or security agent may block local network traffic. Look for its option to allow local network "
        "access."
    );
  case Hint::CheckName:
    return tr("Check the spelling of the name, or type the server's IP address instead.");
  case Hint::TlsSetting:
    return tr("Both computers must use the same TLS (encryption) setting.");
  case Hint::InspectionProduct:
    return tr("A security product that inspects or filters network traffic may interrupt the connection.");
  case Hint::TrustFingerprint:
    return tr("Compare the fingerprint with the one of the server and approve it when you are asked.");
  case Hint::OtherService:
    return tr("Another program may use that port: check which program listens on it on the server computer.");
  case Hint::UpdateDeskflow:
    return tr("Update Deskflow on the older of the two computers.");
  case Hint::AddScreen:
    return tr("Add the exact name of this computer as a screen in the layout of the server.");
  case Hint::DuplicateName:
    return tr("Give every computer its own name, or wait about 10 seconds for the old connection to close.");
  case Hint::SlowNetwork:
    return tr("If the network is slow, raise the connect timeout (client/connectTimeout in the settings file, now %1 "
              "s).")
        .arg(secondsText(timeoutSeconds));
  }
  return {};
}

QString ConnectionTestDialog::statusText(const ProbeResult &result, double clientTimeoutSeconds)
{
  if (!result.problem.isEmpty()) {
    return tr("Not tested: %1").arg(result.problem);
  }

  if (result.open) {
    auto text = tr("Open: the connection was made in %1 ms. The network and the port are fine. If Deskflow still does "
                   "not connect, the network is not the reason: check that the name of this computer is in the "
                   "layout of the server, the fingerprint and the log.")
                    .arg(result.elapsedMs);
    if (isSlowForTheClient(result, clientTimeoutSeconds)) {
      text += QLatin1Char(' ') + tr("This is slow: the client gives up after %1 s for the connection, the secure "
                                    "handshake and the greeting together. If it fails, raise client/connectTimeout in "
                                    "the settings file.")
                                     .arg(secondsText(clientTimeoutSeconds));
    }
    return text;
  }

  switch (result.kind) {
  case diag::FailureKind::TimedOut:
    return tr("Timed out: no answer after %1 ms. A firewall or the network drops the connection without a word.")
        .arg(result.elapsedMs);
  case diag::FailureKind::Refused:
    return tr("Refused: the computer answered, but nothing listens on that port.");
  case diag::FailureKind::NoRoute:
    return tr("No route: the network has no path to that address.");
  case diag::FailureKind::DnsFailure:
    return tr("Name not found: the name could not be turned into an address.");
  case diag::FailureKind::Reset:
    return tr("Reset: the connection was closed as soon as it was made.");
  case diag::FailureKind::AccessDenied:
    return tr("Blocked on this computer: a firewall or security product here denies the connection.");
  default:
    return tr("Failed: %1").arg(result.detail.isEmpty() ? tr("unknown error") : result.detail);
  }
}

void ConnectionTestDialog::runTest()
{
  if (m_probe != nullptr) {
    m_probe->cancel();
  }
  m_results.clear();
  m_finished = false;
  m_startedAt = QDateTime::currentDateTime();
  m_clientTimeoutSeconds = diag::connectTimeoutFromSetting(Settings::value(Settings::Client::ConnectTimeout)).seconds;

  const auto entries = parseServerEntries(m_servers, m_defaultPort);
  if (entries.isEmpty()) {
    showResults();
    return;
  }

  if (m_probe == nullptr) {
    m_probe = m_probeFactory ? m_probeFactory(this) : ConnectionProbe::createSystem(this);
    connect(m_probe, &ConnectionProbe::resultReady, this, [this](const ProbeResult &result) {
      m_results.append(result);
      showResults();
    });
    connect(m_probe, &ConnectionProbe::finished, this, [this](const QList<ProbeResult> &results) {
      m_results = results;
      m_finished = true;
      showResults();
    });
  }

  m_probe->start(entries);
  // the test is under way now, unless everything was settled without the network
  if (m_probe->isRunning()) {
    showResults();
  }
}

void ConnectionTestDialog::copySummary()
{
  const auto text = summary();
  if (m_clipboardWriter) {
    m_clipboardWriter(text);
  } else {
    QGuiApplication::clipboard()->setText(text);
  }
}

QSize ConnectionTestDialog::sizeHint() const
{
  const auto metrics = fontMetrics();
  return QDialog::sizeHint().expandedTo(QSize(metrics.averageCharWidth() * 84, metrics.lineSpacing() * 28));
}

void ConnectionTestDialog::changeEvent(QEvent *event)
{
  QDialog::changeEvent(event);
  if (event->type() == QEvent::LanguageChange) {
    retranslate();
  }
}

void ConnectionTestDialog::retranslate()
{
  setWindowTitle(tr("Test connection"));
  m_copyButton->setText(tr("Copy summary"));
  m_copyButton->setToolTip(tr("Copies one line that says what came of each address, to paste into a message."));
  m_runAgainButton->setText(tr("Run again"));
  showResults();
}

void ConnectionTestDialog::showResults()
{
  const auto entries = parseServerEntries(m_servers, m_defaultPort);
  const bool testing = isTesting();

  QString html = QStringLiteral("<p><i>%1</i></p>")
                     .arg(tr("Tries to open a connection to each server address from this computer. Only the "
                             "connection is made and closed again: no Deskflow data is sent, the server just sees a "
                             "connection that ends at once.")
                              .toHtmlEscaped());

  if (entries.isEmpty()) {
    html += QStringLiteral("<p>%1</p>")
                .arg(tr("Type the address of the server computer in the \"Connect to\" field first.").toHtmlEscaped());
    m_resultsView->setHtml(html);
    m_status->clear();
    m_copyButton->setEnabled(false);
    m_runAgainButton->setEnabled(true);
    return;
  }

  for (const auto &result : std::as_const(m_results)) {
    html += resultHtml(result);
  }
  const auto openCount =
      static_cast<int>(std::ranges::count_if(m_results, [](const ProbeResult &result) { return result.open; }));
  const bool anyFailed = openCount < m_results.size();
  if (m_finished && openCount > 0 && anyFailed) {
    html +=
        QStringLiteral("<p>%1</p>")
            .arg(tr("Put the address that works first in the list, and remove the ones that do not.").toHtmlEscaped());
  }
  m_resultsView->setHtml(html);

  if (testing) {
    m_status->setText(tr("Testing…"));
  } else if (m_finished) {
    m_status->setText(tr("Done: %1 of %2 open.").arg(openCount).arg(m_results.size()));
  } else {
    m_status->setText(QString());
  }
  m_copyButton->setEnabled(m_finished && !m_results.isEmpty());
  m_runAgainButton->setEnabled(!testing);
}

QString ConnectionTestDialog::resultHtml(const ProbeResult &result) const
{
  const auto timeoutSeconds = (m_probe != nullptr ? m_probe->timeoutMs() : ConnectionProbe::kDefaultTimeoutMs) / 1000.0;
  const auto target = result.problem.isEmpty()
                          ? QString::fromStdString(
                                diag::formatTarget(result.host.toStdString(), result.address.toStdString(), result.port)
                            )
                          : result.entry;
  const bool good = result.open;
  const auto marker = good ? QStringLiteral("&#10004;") : QStringLiteral("&#10006;");
  const auto color = good ? QStringLiteral("#2e9d4b") : QStringLiteral("#d64545");

  auto html =
      QStringLiteral(
          "<h4 style=\"margin-bottom:0\"><span style=\"color:%1\">%2</span> %3</h4>"
          "<p style=\"margin-top:2px\">%4</p>"
      )
          .arg(color, marker, target.toHtmlEscaped(), statusText(result, m_clientTimeoutSeconds).toHtmlEscaped());

  if (!good && result.problem.isEmpty()) {
    diag::Failure failure;
    failure.kind = result.kind;
    failure.phase = result.kind == diag::FailureKind::DnsFailure ? diag::Phase::Resolving : diag::Phase::Tcp;
    failure.host = result.host.toStdString();
    failure.resolvedAddress = result.address.toStdString();
    failure.port = result.port;
    failure.timeoutSeconds = timeoutSeconds;
    failure.platform = deskflow::platform::currentInputPlatform();

    QString items;
    for (const auto hint : diag::diagnose(failure).hints) {
      // what the probe waited is not what the client waits: the client's limit is raised in the settings
      if (hint == diag::Hint::SlowNetwork) {
        continue;
      }
      items += QStringLiteral("<li>%1</li>").arg(hintText(hint, result.port, timeoutSeconds).toHtmlEscaped());
    }
    if (!items.isEmpty()) {
      html += QStringLiteral("<ul style=\"margin-top:0\">%1</ul>").arg(items);
    }
  }
  return html;
}
