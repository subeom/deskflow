/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2024 - 2026 Chris Rizzitello <sithord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 - 2024 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "Diagnostic.h"
#include "StyleUtils.h"

#include "dialogs/AboutDialog.h"
#include "dialogs/ClientConfigDialog.h"
#include "dialogs/ConnectionTestDialog.h"
#include "dialogs/FingerprintDialog.h"
#include "dialogs/HelpDialog.h"
#include "dialogs/ServerAddressesDialog.h"
#include "dialogs/ServerConfigDialog.h"
#include "dialogs/SettingsDialog.h"

#include "common/PlatformInfo.h"
#include "common/Settings.h"
#include "common/UrlConstants.h"
#include "common/VersionInfo.h"
#include "gui/Messages.h"
#include "gui/TlsUtility.h"
#include "gui/chrome/MenuMigration.h"
#include "gui/chrome/WindowChrome.h"
#include "gui/chrome/WindowHeader.h"
#include "gui/core/CoreProcess.h"
#include "gui/ipc/DaemonIpcClient.h"
#include "gui/layout/DisplayProvider.h"
#include "gui/layout/LayoutMemoryService.h"
#include "gui/settings/RestartBanner.h"
#include "gui/settings/SettingEffects.h"
#include "gui/settings/SettingsPage.h"
#include "gui/settings/SettingsPages.h"
#include "gui/shell/ClientConnectionSection.h"
#include "gui/shell/MainShell.h"
#include "gui/shell/StatusModel.h"
#include "gui/style/Appearance.h"
#include "gui/style/Icons.h"
#include "gui/style/TextSize.h"
#include "gui/tray/TrayController.h"
#include "gui/tray/TrayMenuModel.h"
#include "gui/tray/TrayPolicy.h"
#include "gui/widgets/LayoutNoticeBar.h"
#include "gui/widgets/LogDock.h"
#include "gui/widgets/NavRail.h"
#include "gui/widgets/SidebarShell.h"
#include "net/FingerprintDatabase.h"
#include "widgets/StatusBar.h"

#include <QCheckBox>
#include <QBoxLayout>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkInterface>
#include <QOperatingSystemVersion>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScreen>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QVersionNumber>

#include <memory>

#if defined(Q_OS_MACOS)
#include <ApplicationServices/ApplicationServices.h>
#endif

using namespace deskflow::gui;

MainWindow::MainWindow()
    : ui{std::make_unique<Ui::MainWindow>()},
      m_coreProcess(m_serverConfig),
      m_trayIcon{new QSystemTrayIcon(this)},
      m_guiDupeChecker{new QLocalServer(this)},
      m_daemonIpcClient{new ipc::DaemonIpcClient(this)},
      m_logDock{new LogDock(this)},
      m_statusBar{new StatusBar(this)},
      m_menuFile{new QMenu(this)},
      m_menuEdit{new QMenu(this)},
      m_menuView{new QMenu(this)},
      m_menuHelp{new QMenu(this)},
      m_actionAbout{new QAction(this)},
      m_actionMinimize{new QAction(this)},
      m_actionQuit{new QAction(this)},
      m_actionTrayQuit{new QAction(this)},
      m_actionRestore{new QAction(this)},
      m_actionSettings{new QAction(this)},
      m_actionStartCore{new QAction(this)},
      m_actionRestartCore{new QAction(this)},
      m_actionStopCore{new QAction(this)},
      m_actionShowHelp{new QAction(this)},
      m_networkMonitor{new NetworkMonitor(this)}
{
  ui->setupUi(this);

  setWindowIcon(QIcon::fromTheme(kRevFqdnName));

  addDockWidget(Qt::BottomDockWidgetArea, m_logDock);

  // Setup Actions
  m_actionAbout->setMenuRole(QAction::AboutRole);
  m_actionAbout->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::HelpAbout));

  m_actionMinimize->setIcon(QIcon::fromTheme(QStringLiteral("window-minimize-pip")));
  m_actionRestore->setIcon(QIcon::fromTheme(QStringLiteral("window-restore-pip")));

  if (!deskflow::platform::isWindows()) {
    m_actionQuit->setShortcut(QKeySequence::Quit);
    m_actionTrayQuit->setShortcut(QKeySequence::Quit);
  }

  m_actionQuit->setIcon(QIcon::fromTheme("application-exit"));
  m_actionQuit->setMenuRole(QAction::QuitRole);

  m_actionTrayQuit->setIcon(QIcon::fromTheme("application-exit"));
  m_actionTrayQuit->setMenuRole(QAction::NoRole);

  m_actionSettings->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
  m_actionSettings->setMenuRole(QAction::PreferencesRole);
  m_actionSettings->setShortcut(QKeySequence::Preferences);

  m_actionStartCore->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));
  m_actionStartCore->setMenuRole(QAction::NoRole);

  m_actionRestartCore->setVisible(false);
  m_actionRestartCore->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
  m_actionRestartCore->setMenuRole(QAction::NoRole);

  m_actionStopCore->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::ProcessStop));
  m_actionStopCore->setMenuRole(QAction::NoRole);

  m_actionShowHelp->setIcon(QIcon::fromTheme(QStringLiteral("question")));
  m_actionShowHelp->setMenuRole(QAction::NoRole);
  m_actionShowHelp->setShortcut(QKeySequence::HelpContents);

  // Setup the Instance Checking
  // In case of a previous crash remove first
  QLocalServer::removeServer(m_guiSocketName);
  m_guiDupeChecker->listen(m_guiSocketName);

  createMenuBar();
  setupControls();
  updateText();
  connectSlots();
  setupTrayIcon();
  updateComputerName();
  setHelpFilePath();

  qDebug().noquote() << "active settings path:" << Settings::settingsPath();

  // Force generation of a certificate on the host
  if (TlsUtility::isEnabled()) {
    if (Settings::value(Settings::Security::KeySize).toInt() < 2048) {
      QMessageBox::information(
          this, kAppName,
          tr("Your current TLS key is smaller than the minimum allowed size, A new key 2048-bit key will be generated.")
      );
      Settings::setValue(Settings::Security::KeySize, 2048);
    }
    if (!TlsUtility::isCertValid()) {
      generateCertificate();
    } else {
      m_fingerprint = {QCryptographicHash::Sha256, TlsUtility::certFingerprint()};
    }
  }

  applyConfig();
  setupLayoutMemory();
  m_statusBar->setSecurityIcon(TlsUtility::isEnabled());
  if (deskflow::gui::style::isDeskflowStyleInstalled()) {
    setupShell();
    applyWindowChrome();
  }
  restoreWindow();

#ifdef Q_OS_MACOS
  // Route native quits (Cmd+Q / Apple menu Quit / Dock "Quit") to the usual close-to-tray decision instead.
  installQuitHandler([this] { return !maybeHideToTray(); });
#endif
}
MainWindow::~MainWindow()
{
  // the remembering of layouts works on the server configuration of this window: it goes first
  if (m_layoutMemory != nullptr) {
    delete m_layoutMemory;
    m_layoutMemory = nullptr;
  }

  // Stop network monitoring
  if (m_networkMonitor) {
    m_networkMonitor->stopMonitoring();
  }

  m_guiDupeChecker->close();
  m_coreProcess.cleanup();
}

const QString &MainWindow::windowGeometryKey() const
{
  // the window of the new look has a position and a size of its own (see Settings::Gui::ShellWindowGeometry)
  return m_shell ? Settings::Gui::ShellWindowGeometry : Settings::Gui::WindowGeometry;
}

void MainWindow::restoreWindow()
{
  auto windowGeometry = Settings::value(windowGeometryKey()).toRect();
  const auto totalGeometry = QGuiApplication::primaryScreen()->availableGeometry();
  if (!windowGeometry.isValid()) {
    if (m_shell) {
      // a window that can be resized has a size to start with: wide enough for the sidebar with its labels
      resize(qMax(sizeHint().width(), 1000), qMax(sizeHint().height(), 620));
    } else {
      adjustSize();
    }
    windowGeometry = geometry();
  } else {
    setGeometry(windowGeometry);
  }
  m_expandedSize = geometry().size();

  if (!totalGeometry.contains(windowGeometry)) {
    QRect screenGeometry = QGuiApplication::primaryScreen()->geometry();
    move(screenGeometry.center() - rect().center());
  }

  // the window of the new look is resized by the user at any time; the old one keeps its size while the log is folded
  if (!m_shell && !Settings::value(Settings::Gui::LogExpanded).toBool())
    setFixedSize(size());
}

void MainWindow::setupControls()
{
  secureSocket(false);

  ui->btnConfigureServer->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
  ui->btnConfigureClient->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
  ui->cbHostname->lineEdit()->setPlaceholderText(tr("LAN address first, then the VPN address, separated by commas"));

  if (Settings::value(Settings::Core::LastVersion).toString() != kVersion) {
    Settings::setValue(Settings::Core::LastVersion, kVersion);
  }

  // the new look has its own setting for the log: it is folded until the user unfolds it (or an error does)
  const bool logExpanded = deskflow::gui::style::isDeskflowStyleInstalled()
                               ? Settings::value(Settings::Gui::ShellLogExpanded).toBool()
                               : Settings::value(Settings::Gui::LogExpanded).toBool();
  if (!logExpanded) {
    m_logDock->hide();
  }

  ui->serverOptions->setVisible(false);
  ui->clientOptions->setVisible(false);

  const auto coreMode = Settings::value(Settings::Core::CoreMode).value<Settings::CoreMode>();
  ui->rbModeClient->setChecked(coreMode == Settings::CoreMode::Client);
  ui->rbModeServer->setChecked(coreMode == Settings::CoreMode::Server);

  ui->lineEditName->setValidator(new QRegularExpressionValidator(m_nameRegEx, this));
  ui->lineEditName->setVisible(false);
  ui->lineEditName->installEventFilter(this);

  if (deskflow::platform::isMac()) {
    ui->rbModeServer->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui->rbModeClient->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui->btnSaveServerConfig->setFixedWidth(ui->btnSaveServerConfig->height());
  } else {
    ui->btnSaveServerConfig->setIconSize(QSize(22, 22));
  }
  setStatusBar(m_statusBar);
}

//////////////////////////////////////////////////////////////////////////////
// Begin slots
//////////////////////////////////////////////////////////////////////////////

// remember: using queued connection allows the render loop to run before
// executing the slot. the default is to instantly call the slot when the
// signal is emitted from the thread that owns the receiver's object.
void MainWindow::connectSlots()
{
  connect(Settings::instance(), &Settings::serverSettingsChanged, this, &MainWindow::serverConfigSaving);
  connect(Settings::instance(), &Settings::settingsChanged, this, &MainWindow::settingsChanged);

  connect(&m_coreProcess, &CoreProcess::error, this, &MainWindow::coreProcessError);
  connect(&m_coreProcess, &CoreProcess::logLine, this, &MainWindow::handleLogLine);
  connect(
      &m_coreProcess, &CoreProcess::processStateChanged, this, &MainWindow::coreProcessStateChanged,
      Qt::QueuedConnection
  );
  connect(&m_coreProcess, &CoreProcess::connectionStateChanged, this, &MainWindow::coreConnectionStateChanged);
  connect(&m_coreProcess, &CoreProcess::secureSocket, this, &MainWindow::secureSocket);
  connect(
      &m_coreProcess, &CoreProcess::daemonIpcClientConnectionFailed, this, &MainWindow::daemonIpcClientConnectionFailed
  );
  connect(&m_coreProcess, &CoreProcess::securityLevelChanged, m_statusBar, &StatusBar::setSecurityLevel);

  connect(m_actionAbout, &QAction::triggered, this, &MainWindow::openAboutDialog);
  connect(m_actionMinimize, &QAction::triggered, this, &MainWindow::hide);

  connect(m_actionQuit, &QAction::triggered, this, &MainWindow::close);
  connect(m_actionTrayQuit, &QAction::triggered, this, &MainWindow::close);
  connect(m_actionRestore, &QAction::triggered, this, &MainWindow::showAndActivate);
  connect(m_actionSettings, &QAction::triggered, this, &MainWindow::openSettings);
  connect(m_actionStartCore, &QAction::triggered, this, &MainWindow::startCore);
  connect(m_actionRestartCore, &QAction::triggered, this, &MainWindow::resetCore);
  connect(m_actionStopCore, &QAction::triggered, this, &MainWindow::stopCore);
  connect(m_actionShowHelp, &QAction::triggered, this, &MainWindow::showHelpViewer);

  // The tray menu is opened by the OS on macOS and by the tray extension on GNOME: a click is not taken there
  if (deskflow::gui::tray::clickTogglesWindow(
          deskflow::platform::isMac(), qEnvironmentVariable("XDG_CURRENT_DESKTOP")
      ))
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::trayIconActivated);

  connect(&m_coreProcess, &CoreProcess::connectedClientsChanged, this, &MainWindow::serverClientsChanged);
  connect(&m_coreProcess, &CoreProcess::unrecognisedClient, this, &MainWindow::handleUnrecognisedClient);
  connect(&m_coreProcess, &CoreProcess::connectionRefused, this, &MainWindow::handleConnectionRefused);
  connect(&m_coreProcess, &CoreProcess::retryIn, this, &MainWindow::updateTimeoutDelay);
  connect(&m_coreProcess, &CoreProcess::peerFingerprint, this, &MainWindow::handlePeerFingerprint);
  connect(&m_coreProcess, &CoreProcess::missingKeyboardLayouts, this, &MainWindow::handleMissingKeyboardLayouts);

  connect(ui->btnToggleCore, &QPushButton::clicked, m_actionStartCore, &QAction::trigger, Qt::UniqueConnection);

  connect(ui->btnRestartCore, &QPushButton::clicked, this, &MainWindow::resetCore);

  connect(ui->cbHostname->lineEdit(), &QLineEdit::returnPressed, ui->btnToggleCore, &QPushButton::click);
  connect(ui->cbHostname, &QComboBox::editTextChanged, this, &MainWindow::remoteHostChanged);

  connect(ui->btnSaveServerConfig, &QPushButton::clicked, this, &MainWindow::saveServerConfig);
  connect(ui->btnConfigureServer, &QPushButton::clicked, this, [this] { showConfigureServer(""); });
  connect(ui->btnConfigureClient, &QPushButton::clicked, this, [this] { showConfigureClient(); });
  connect(ui->btnTestConnection, &QPushButton::clicked, this, &MainWindow::showConnectionTest);
  connect(ui->lblIpAddresses, &QLabel::linkActivated, this, &MainWindow::showServerAddresses);
  connect(ui->lblComputerName, &QLabel::linkActivated, this, &MainWindow::openSettings);

  connect(ui->rbModeServer, &QRadioButton::toggled, this, &MainWindow::coreModeToggled);
  connect(ui->rbModeClient, &QRadioButton::toggled, this, &MainWindow::coreModeToggled);

  connect(m_logDock->toggleViewAction(), &QAction::toggled, this, &MainWindow::toggleLogVisible);

  connect(m_statusBar, &StatusBar::requestShowMyFingerprints, this, &MainWindow::showMyFingerprint);
  connect(m_statusBar, &StatusBar::requestUpdateVersion, this, &MainWindow::openGetNewVersionUrl);
  connect(&m_versionChecker, &VersionChecker::updateFound, m_statusBar, &StatusBar::updateFound);

  connect(m_guiDupeChecker, &QLocalServer::newConnection, this, &MainWindow::showAndActivate);

  connect(ui->btnEditName, &QPushButton::clicked, this, &MainWindow::showHostNameEditor);

  connect(ui->lineEditName, &QLineEdit::editingFinished, this, &MainWindow::setHostName);

  connect(m_networkMonitor, &NetworkMonitor::ipAddressesChanged, this, &MainWindow::updateIpLabel);
}

void MainWindow::toggleLogVisible(bool visible)
{
  // When the main window is hidden e.g. close to tray / minimized), this also triggers the log visibility toggle,
  // but we don't want to hide the log in this case since we would need to un-hide it when the window is shown again.
  if (!isVisible() || isMinimized()) {
    qDebug() << "not toggling log, window not visible";
    return;
  }

  // the window of the new look can be resized: the log comes and goes inside it, and what the user chose is remembered
  // (not what an error did)
  if (m_shell) {
    if (!m_logShownByError)
      Settings::setValue(Settings::Gui::ShellLogExpanded, visible);
    return;
  }

  setFixedSize(16777215, 16777215);
  Settings::setValue(Settings::Gui::LogExpanded, visible);
  if (visible) {
    if (m_logDock->isFloating()) {
      adjustSize();
      setFixedSize(size());
    } else {
      QTimer::singleShot(15, this, [&] { resize(m_expandedSize); });
    }
  } else {
    if (!m_logDock->isFloating()) {
      m_expandedSize = geometry().size();
    }
    m_logDock->hide();
    if (!m_logDock->isFloating()) {
      adjustSize();
    }
    setFixedSize(size());
  }
  Settings::setValue(Settings::Gui::WindowGeometry, geometry());
}

void MainWindow::settingsChanged(const QString &key)
{
  // the remembering of layouts looks again when what it depends on changes (it does nothing while it is off)
  if (m_layoutMemory != nullptr &&
      (key == Settings::Server::RememberArrangements || key == Settings::Server::ExperimentalMultiMonitor ||
       key == Settings::Server::UseArrangement || key == Settings::Server::ExternalConfig ||
       key == Settings::Core::CoreMode))
    m_layoutMemory->settingsChanged();

  if (key == Settings::Log::Level) {
    m_coreProcess.applyLogLevel();
    return;
  }

  if (key == Settings::Core::ComputerName)
    updateComputerName();

  if ((key == Settings::Security::Certificate) || (key == Settings::Security::KeySize) ||
      (key == Settings::Security::TlsEnabled) || (key == Settings::Security::CheckPeers)) {
    if (TlsUtility::isEnabled()) {
      if (!TlsUtility::isCertValid()) {
        qWarning() << tr("invalid certificate, generating a new one");
        TlsUtility::generateCertificate();
      }
      m_fingerprint = {QCryptographicHash::Sha256, TlsUtility::certFingerprint()};
      updateFingerprintButton();
    }
    updateSecurityIcon(m_statusBar->securityIconVisible());
    return;
  }
}

void MainWindow::serverConfigSaving()
{
  m_serverConfig.commit();
  updateShellComputers();
}

void MainWindow::setupLayoutMemory()
{
  using namespace deskflow::gui::layout;
  HostHooks hooks;
  hooks.serverMode = [this] { return m_coreProcess.mode() == CoreMode::Server; };
  hooks.coreRunning = [this] { return m_coreProcess.isStarted(); };
  hooks.restartCore = [this] { m_coreProcess.restart(); };
  m_layoutMemory = new LayoutMemoryService(m_serverConfig, makePlatformDisplayProvider(), hooks, {}, this);

  // one line above the controls, hidden until the remembering of layouts has something to say
  m_layoutNoticeBar = new LayoutNoticeBar(this);
  if (auto *box = qobject_cast<QBoxLayout *>(ui->topLevelWidget->layout()))
    box->insertWidget(0, m_layoutNoticeBar);
  connect(m_layoutMemory, &LayoutMemoryService::noticeChanged, this, &MainWindow::updateLayoutNotice);
  connect(m_layoutNoticeBar, &LayoutNoticeBar::undoRequested, this, [this] { m_layoutMemory->undo(); });
  connect(m_layoutNoticeBar, &LayoutNoticeBar::dismissRequested, m_layoutMemory, &LayoutMemoryService::dismissNotice);

  // nothing runs unless the switch, the experiments and the mode ask for it
  m_layoutMemory->settingsChanged();
}

void MainWindow::updateLayoutNotice()
{
  if (m_layoutMemory == nullptr || m_layoutNoticeBar == nullptr)
    return;
  const auto &notice = m_layoutMemory->notice();
  const bool wasShown = !m_layoutNoticeBar->isHidden();
  // a window of a fixed size keeps the room of its controls: the bar takes its height from outside it
  const bool fixedSize = minimumSize() == maximumSize();
  m_layoutNoticeBar->setNotice(notice);
  const bool shown = !m_layoutNoticeBar->isHidden();
  if (fixedSize && shown != wasShown) {
    const int extra = m_layoutNoticeBar->sizeHint().height() + ui->topLevelWidget->layout()->spacing();
    setFixedSize(width(), height() + (shown ? extra : -extra));
  }

  // the window is often hidden in the tray: the change is told there as well
  if (notice && (!isVisible() || isMinimized()) && m_trayIcon->isVisible() && QSystemTrayIcon::supportsMessages())
    m_trayIcon->showMessage(
        kAppName, deskflow::gui::layout::LayoutMemoryService::noticeText(*notice), QSystemTrayIcon::Information, 10000
    );
}

void MainWindow::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
  if (reason != QSystemTrayIcon::Trigger)
    return;
  isVisible() ? hide() : showAndActivate();
}

void MainWindow::coreProcessError(CoreProcess::Error error)
{
  if (error == CoreProcess::Error::AddressMissing) {
    QMessageBox::warning(
        this, tr("Address missing"), tr("Please enter the hostname or IP address of the other computer.")
    );
  } else if (error == CoreProcess::Error::StartFailed) {
    show();
    auto message = tr("The Core executable could not be started.\n"
                      "Please check if you have sufficient permissions to run %1.")
                       .arg(kCoreBinName);

    if (Settings::value(Settings::Core::CoreMode) == Settings::CoreMode::Server) {
      const auto mode =
          Settings::value(Settings::Server::ExternalConfigFile).toBool() ? tr("read") : tr("read and write");
      message.append(tr("\nAdditionally, check you are able to %1 the server config file: %2")
                         .arg(mode, Settings::serverConfigFile()));
    }
    QMessageBox::warning(this, kAppName, message);
    m_trayStartFailed = true;
    updateTray();
    if (m_shell)
      m_shell->status()->setError(
          tr("Cannot start"),
          tr("The Core executable %1 could not be started. See the message for what to check.").arg(kCoreBinName)
      );
  }
}

void MainWindow::startCore()
{
  // No Mode Set
  if (m_coreProcess.mode() == CoreMode::None) {
    qDebug().noquote() << "skipping core start, no mode set";
    return;
  }

  // Invalid client settings
  if (m_coreProcess.mode() == CoreMode::Client && Settings::textValue(Settings::Client::RemoteHost).isEmpty()) {
    qDebug().noquote() << "skipping core start, undefined remotehost";
    return;
  }

  // Save current IP state when server starts
  if (m_coreProcess.mode() == CoreMode::Server && Settings::value(Settings::Core::Interface).toString().isEmpty()) {
    m_serverStartIPs = NetworkMonitor::validAddresses();
    m_serverStartSuggestedIP = m_serverStartIPs.isEmpty() ? "" : m_serverStartIPs.first();
  }

  m_actionStartCore->setVisible(false);
  m_actionRestartCore->setVisible(true);
  m_coreProcess.start();
}

void MainWindow::stopCore()
{
  qDebug() << "stopping core process";
  m_coreProcess.stop();
  m_actionStartCore->setVisible(true);
  m_actionRestartCore->setVisible(false);
}

void MainWindow::clearSettings()
{
  qDebug() << "clearing settings";

  m_networkMonitor->stopMonitoring();

  disconnect(&m_coreProcess, nullptr, this, nullptr);
  disconnect(&m_versionChecker, nullptr, this, nullptr);
  disconnect(m_guiDupeChecker, nullptr, this, nullptr);
  disconnect(m_trayIcon, nullptr, this, nullptr);
  disconnect(m_logDock->toggleViewAction(), nullptr, this, nullptr);

  m_coreProcess.stop();
  m_coreProcess.clearSettings();

  m_saveOnExit = false;
  diagnostic::clearSettings(true);
}

bool MainWindow::saveServerConfig()
{
  QString fileName = QFileDialog::getSaveFileName(this, tr("Save server configuration as..."));

  if (!fileName.isEmpty() && !m_serverConfig.save(fileName)) {
    QMessageBox::warning(this, tr("Save failed"), tr("Could not save server configuration to file."));
    return true;
  }

  return false;
}

void MainWindow::openAboutDialog()
{
  AboutDialog about(this);
  about.exec();
}

void MainWindow::openGetNewVersionUrl() const
{
  QDesktopServices::openUrl(QUrl(kUrlDownload));
}

void MainWindow::openSettings()
{
  // the settings of the new look are pages of the window: the entry of the menu goes to the first one
  if (m_shell) {
    m_shell->showPage(deskflow::gui::settings::SettingsPages::generalId());
    if (!isVisible() || isMinimized())
      showAndActivate();
    return;
  }

  auto dialog = SettingsDialog(this, m_serverConfig);

  connect(&dialog, &SettingsDialog::requestRemoveAllSettings, this, &MainWindow::clearSettings, Qt::UniqueConnection);
  if (dialog.exec() == QDialog::Accepted) {
    Settings::save();
    disconnect(&dialog, &SettingsDialog::requestRemoveAllSettings, nullptr, nullptr);

    applyConfig();

    if (m_coreProcess.isStarted()) {
      m_coreProcess.restart();
    }
  }
}

void MainWindow::resetCore()
{
  m_coreProcess.restart();
}

void MainWindow::showMyFingerprint()
{
  FingerprintDialog fingerprintDialog(this, m_fingerprint);
  fingerprintDialog.exec();
}

void MainWindow::coreModeToggled(bool checked)
{
  // this method is called when rbClient or rbServer toggles
  // with both being in the same group one must be turned on if the other is turned off
  // only react to toggle on to avoid calling everything twice when the user switches modes
  if (!checked)
    return;

  Settings::CoreMode mode = Settings::CoreMode::None;

  if (ui->rbModeServer->isChecked())
    mode = Settings::CoreMode::Server;
  if (ui->rbModeClient->isChecked())
    mode = Settings::CoreMode::Client;

  qDebug() << QStringLiteral("change mode to: %1").arg(QVariant::fromValue(mode).toString());

  if (m_coreProcess.isStarted() && m_coreProcess.mode() != mode)
    m_coreProcess.stop();
  m_coreProcess.setMode(mode);

  Settings::setValue(Settings::Core::CoreMode, mode);
  Settings::save();

  updateModeControls();
}

void MainWindow::updateModeControls()
{
  const auto mode = m_coreProcess.mode();
  const bool isServer = mode == Settings::CoreMode::Server;
  const bool isClient = mode == Settings::CoreMode::Client;

  ui->serverOptions->setVisible(isServer);
  ui->clientOptions->setVisible(isClient);
  ui->lblNoMode->setVisible(!isServer && !isClient);
  toggleCanRunCore(canRunCore());

  ui->lblIpAddresses->setVisible(
      (isClient && !Settings::value(Settings::Core::Interface).toString().isEmpty()) || isServer
  );

  if (ui->lblIpAddresses->isVisible())
    updateNetworkInfo();

  if (isServer) {
    m_networkMonitor->startMonitoring();
  } else {
    m_networkMonitor->stopMonitoring();
  }

  if (isServer || isClient)
    updateModeControlLabels();

  if (m_shell) {
    m_shell->setMode(
        isServer   ? deskflow::gui::MainShell::Mode::Server
        : isClient ? deskflow::gui::MainShell::Mode::Client
                   : deskflow::gui::MainShell::Mode::None
    );
    updateShellComputers();
  }
}

void MainWindow::updateModeControlLabels()
{
  const bool isServer = m_coreProcess.mode() == CoreMode::Server;
  const bool isStarted = m_coreProcess.isStarted();

  QString startText;
  QString stopText;
  QIcon startIcon;
  QIcon stopIcon;

  if (isServer) {
    startText = tr("Start");
    stopText = tr("Stop");
    startIcon = QIcon::fromTheme(QStringLiteral("system-run"));
    stopIcon = QIcon::fromTheme(QIcon::ThemeIcon::ProcessStop);
  } else {
    startText = tr("Connect");
    stopText = tr("Disconnect");
    startIcon = QIcon::fromTheme(QStringLiteral("network-connect"));
    stopIcon = QIcon::fromTheme(QStringLiteral("network-disconnect"));
  }

  m_actionStartCore->setText(startText);
  m_actionStartCore->setIcon(startIcon);
  m_actionStopCore->setText(stopText);
  m_actionStopCore->setIcon(stopIcon);

  // the button of the state is the primary button of the new look: its icons are the ones of the look, in the colour of
  // what is on the accent
  if (m_shell) {
    using deskflow::gui::style::IconColor;
    startIcon = deskflow::gui::style::icon(isServer ? "play" : "plugs-connected", IconColor::OnAccent);
    stopIcon = deskflow::gui::style::icon(isServer ? "stop" : "plug", IconColor::OnAccent);
  }

  if (isStarted) {
    ui->btnToggleCore->setText(stopText);
    ui->btnToggleCore->setIcon(stopIcon);
  } else {
    ui->btnToggleCore->setText(startText);
    ui->btnToggleCore->setIcon(startIcon);
  }
}

void MainWindow::updateSecurityIcon(bool visible)
{
  m_statusBar->setSecurityIconVisible(visible);
  if (!visible)
    return;
  m_statusBar->setSecurityIcon(TlsUtility::isEnabled());
}

void MainWindow::updateNetworkInfo()
{
  const auto mode = m_coreProcess.mode();
  if (mode == CoreMode::None) {
    return;
  }

  if (mode == CoreMode::Server)
    updateIpLabel(NetworkMonitor::validAddresses());
  else
    updateIpLabel({Settings::value(Settings::Core::Interface).toString()});
}

void MainWindow::serverConnectionConfigureClient(const QString &clientName)
{
  m_serverConfigDialogVisible = true;
  ServerConfigDialog dialog(this, m_serverConfig);
  if (dialog.addClient(clientName) && dialog.exec() == QDialog::Accepted) {
    // the arrangement that was saved is what is remembered for the displays that there are
    if (m_layoutMemory != nullptr)
      m_layoutMemory->configSaved();
    m_coreProcess.restart();
  }
  m_serverConfigDialogVisible = false;
}

//////////////////////////////////////////////////////////////////////////////
// End slots
//////////////////////////////////////////////////////////////////////////////

void MainWindow::open()
{
  if (!Settings::value(Settings::Gui::Autohide).toBool())
    showAndActivate();
  else if (deskflow::platform::isMac())
    // macOS to call hide after this function ends
    QTimer::singleShot(1, this, &MainWindow::hide);
  else
    hide();

  // if a critical error was shown just before the main window (i.e. on app
  // load), it will be hidden behind the main window. therefore we need to raise
  // it up in front of the main window.
  // HACK: because the `onShown` event happens just as the window is shown, the
  // message box has a chance of being raised under the main window. to solve
  // this we delay the error dialog raise by a split second. this seems a bit
  // hacky and fragile, so maybe there's a better approach.
  const auto kCriticalDialogDelay = 100;
  QTimer::singleShot(kCriticalDialogDelay, this, &messages::raiseCriticalDialog);

  if (!Settings::value(Settings::Gui::AutoUpdateCheck).isValid()) {
    Settings::setValue(Settings::Gui::AutoUpdateCheck, messages::showUpdateCheckOption(this));
  }

  if (Settings::value(Settings::Gui::AutoUpdateCheck).toBool()) {
    m_versionChecker.checkLatest();
  } else {
    qDebug() << "skipping check for new version, disabled";
  }

  if (Settings::value(Settings::Gui::AutoStartCore).toBool()) {
    startCore();
  }
}

void MainWindow::createMenuBar()
{
  const bool newLook = deskflow::gui::style::isDeskflowStyleInstalled();
  if (newLook) {
    createSharingMenu();
  } else {
    m_menuFile->addAction(m_actionStartCore);
    m_menuFile->addAction(m_actionRestartCore);
    m_menuFile->addAction(m_actionStopCore);
    m_menuFile->addSeparator();
    m_menuFile->addAction(m_actionQuit);
  }

  m_menuEdit->addAction(m_actionSettings);

  m_menuView->addAction(m_logDock->toggleViewAction());

  m_menuHelp->addAction(m_actionAbout);
  m_menuHelp->addAction(m_actionShowHelp);

  auto menuBar = new QMenuBar(this);
  menuBar->addMenu(m_menuFile);
  // macOS has Preferences in the menu of the application (by its role): the menu of the new look has no menu of its own for it
  if (!(newLook && deskflow::platform::isMac()))
    menuBar->addMenu(m_menuEdit);
  menuBar->addMenu(m_menuView);
  menuBar->addMenu(m_menuHelp);

  setMenuBar(menuBar);
}

void MainWindow::createSharingMenu()
{
  using deskflow::gui::style::icon;
  const bool mac = deskflow::platform::isMac();
  const auto make = [this, mac](const QString &iconName, auto slot) {
    auto *action = new QAction(this);
    action->setMenuRole(QAction::NoRole);
    // the menu bar of macOS has no pictures: the menus of the system have none
    if (!mac)
      action->setIcon(icon(iconName));
    connect(action, &QAction::triggered, this, slot);
    m_menuFile->addAction(action);
    return action;
  };
  m_sharingStart = make(QStringLiteral("play"), &MainWindow::startCore);
  m_sharingRestart = make(QStringLiteral("arrow-clockwise"), &MainWindow::resetCore);
  m_sharingStop = make(QStringLiteral("stop"), &MainWindow::stopCore);
  m_menuFile->addSeparator();
  m_sharingTestConnection = make(QStringLiteral("network"), &MainWindow::showConnectionTest);
  m_sharingFingerprint = make(QStringLiteral("fingerprint"), &MainWindow::showMyFingerprint);
  m_menuFile->addSeparator();
  m_sharingMinimize = make(QStringLiteral("sidebar-simple"), &MainWindow::hide);
  // Preferences and Quit are what the menu of the application of macOS takes by their roles; elsewhere Quit ends the menu
  if (mac) {
    m_menuFile->addAction(m_actionSettings);
    m_menuFile->addAction(m_actionQuit);
  } else {
    m_menuFile->addSeparator();
    m_menuFile->addAction(m_actionQuit);
  }
}

void MainWindow::updateSharingMenu(const deskflow::gui::tray::TrayInput &input)
{
  if (m_sharingStart == nullptr)
    return;
  using deskflow::gui::tray::TrayCommand;
  m_sharingStart->setText(input.labels.value(TrayCommand::Start));
  m_sharingRestart->setText(input.labels.value(TrayCommand::Restart));
  m_sharingStop->setText(input.labels.value(TrayCommand::Stop));
  m_sharingTestConnection->setText(input.labels.value(TrayCommand::TestConnection));
  m_sharingFingerprint->setText(m_statusBar->fingerprintToolTip());
  m_sharingMinimize->setText(input.labels.value(TrayCommand::Minimize));
  m_sharingStart->setShortcut(input.shortcuts.value(TrayCommand::Start));
  m_sharingRestart->setShortcut(input.shortcuts.value(TrayCommand::Restart));
  m_sharingStop->setShortcut(input.shortcuts.value(TrayCommand::Stop));

  const auto state = deskflow::gui::tray::sharingMenuState(input, TlsUtility::isEnabled() && !m_fingerprint.data.isEmpty());
  m_sharingStart->setEnabled(state.start);
  m_sharingRestart->setEnabled(state.restart);
  m_sharingStop->setEnabled(state.stop);
  m_sharingTestConnection->setEnabled(state.testConnection);
  m_sharingFingerprint->setEnabled(state.fingerprint);
  m_sharingMinimize->setEnabled(state.minimize);
}

void MainWindow::setupTrayIcon()
{
  // Qt before 6.11.3 ends the program at a click on the status item of macOS 27 (QTBUG-147449)
  if (deskflow::gui::tray::trayClickCanCrash(
          QVersionNumber::fromString(QString::fromLatin1(qVersion())),
          deskflow::platform::isMac() ? QOperatingSystemVersion::current().majorVersion() : 0
      ))
    qWarning() << "this build uses Qt" << qVersion() << ": a click on the tray icon can end the program on macOS 27";

  auto trayMenu = new QMenu(this);
  if (deskflow::gui::style::isDeskflowStyleInstalled()) {
    setupTrayController(trayMenu);
  } else {
    trayMenu->addActions(
        {m_actionStartCore, m_actionRestartCore, m_actionStopCore, m_actionMinimize, m_actionRestore, m_actionTrayQuit}
    );
    trayMenu->insertSeparator(m_actionMinimize);
    trayMenu->insertSeparator(m_actionTrayQuit);
  }
  m_trayIcon->setContextMenu(trayMenu);

  setTrayIcon();
  updateTray();
  m_trayIcon->show();
}

void MainWindow::setupTrayController(QMenu *menu)
{
  using deskflow::gui::tray::TrayController;
  m_trayController = new TrayController(m_trayIcon, menu, deskflow::gui::tray::TrayPlatform::current(), this);
  connect(m_trayController, &TrayController::commandRequested, this, &MainWindow::trayCommand);
}

void MainWindow::trayCommand(deskflow::gui::tray::TrayCommand command)
{
  using deskflow::gui::tray::TrayCommand;
  switch (command) {
  case TrayCommand::Start:
    startCore();
    break;
  case TrayCommand::Restart:
    resetCore();
    break;
  case TrayCommand::Stop:
    stopCore();
    break;
  case TrayCommand::TestConnection:
    showConnectionTest();
    break;
  case TrayCommand::OpenWindow:
    showAndActivate();
    break;
  case TrayCommand::Minimize:
    hide();
    break;
  case TrayCommand::Preferences:
    openSettings();
    break;
  case TrayCommand::OpenUpdate:
    openGetNewVersionUrl();
    break;
  case TrayCommand::Quit:
    close();
    break;
  }
}

void MainWindow::updateTray()
{
  if (m_trayController == nullptr)
    return;
  using deskflow::gui::tray::TrayCommand;
  const bool isServer = m_coreProcess.mode() == CoreMode::Server;
  const bool isClient = m_coreProcess.mode() == CoreMode::Client;

  deskflow::gui::tray::TrayInput input;
  input.process = m_coreProcess.processState();
  input.connection = m_coreProcess.connectionState();
  input.isServer = isServer;
  input.hasMode = isServer || isClient;
  input.startFailed = m_trayStartFailed;
  input.canStart = m_actionStartCore->isEnabled();
  input.canTestConnection = isClient && ui->btnTestConnection->isEnabled();
  if (m_shell) {
    input.headline = m_shell->status()->view().headline;
    input.detail = m_shell->status()->view().detail;
  }
  if (isServer)
    input.computers = m_connectedClients;
  else if (isClient && input.connection == ConnectionState::Connected && input.process == ProcessState::Started)
    input.computers = {Settings::textValue(Settings::Client::RemoteHost)};
  input.updateVersion = m_statusBar->updateVersion();
  // the window is shown when the menu does not offer to show it (the action of the window that does is hidden then)
  input.windowShown = !m_actionRestore->isVisible();
  input.appName = kAppName;
  input.labels = {
      {TrayCommand::Start, m_actionStartCore->text()},
      {TrayCommand::Restart, m_actionRestartCore->text()},
      {TrayCommand::Stop, m_actionStopCore->text()},
      {TrayCommand::TestConnection, ui->btnTestConnection->text()},
      {TrayCommand::OpenWindow, m_actionRestore->text()},
      {TrayCommand::Minimize, m_actionMinimize->text()},
      {TrayCommand::Preferences, m_actionSettings->text()},
      {TrayCommand::Quit, m_actionTrayQuit->text()},
  };
  input.shortcuts = {
      {TrayCommand::Start, m_actionStartCore->shortcut()},
      {TrayCommand::Restart, m_actionRestartCore->shortcut()},
      {TrayCommand::Stop, m_actionStopCore->shortcut()},
      {TrayCommand::Quit, m_actionTrayQuit->shortcut()},
  };
  m_trayController->setInput(input);
  updateSharingMenu(input);
}

void MainWindow::updateWindowTitle()
{
  if (Settings::value(Settings::Gui::ShowVersionInTitle).toBool()) {
    setWindowTitle(QStringLiteral("%1 - %2").arg(kAppName, kDisplayVersion));
  } else {
    setWindowTitle(kAppName);
  }
}

void MainWindow::applyConfig()
{
  updateWindowTitle();

  const auto host = Settings::textValue(Settings::Client::RemoteHost);
  QSignalBlocker hostnameBlocker(ui->cbHostname);
  ui->cbHostname->clear();
  ui->cbHostname->addItems(Settings::value(Settings::Client::RecentRemoteHosts).toStringList());
  if (!host.isEmpty())
    ui->cbHostname->setCurrentText(host);
  // the signals of the field are blocked here, so the button is not told about the text
  ui->btnTestConnection->setEnabled(!ui->cbHostname->currentText().trimmed().isEmpty());

  updateFingerprintButton();
  setTrayIcon();

  if (const auto ip = Settings::value(Settings::Core::Interface).toString(); !ip.isEmpty()) {
    m_serverStartIPs = {ip};
    m_serverStartSuggestedIP = ip;
  }

  coreModeToggled(true);
}

void MainWindow::saveSettings() const
{
  if (ui->rbModeClient->isChecked()) {
    Settings::setValue(Settings::Core::CoreMode, Settings::CoreMode::Client);
  } else if (ui->rbModeServer->isChecked()) {
    Settings::setValue(Settings::Core::CoreMode, Settings::CoreMode::Server);
  }
  if (!ui->cbHostname->currentText().isEmpty()) {
    Settings::setValue(Settings::Client::RemoteHost, ui->cbHostname->currentText());
  }
  Settings::save();
}

void MainWindow::setTrayIcon()
{
  // the icon of the new look tells the state: it is chosen by the tray, which knows it
  if (m_trayController) {
    m_trayController->setColoredIcon(!Settings::value(Settings::Gui::SymbolicTrayIcon).toBool());
    m_trayController->refreshIcon();
    return;
  }

  static const auto fallbackPath = QStringLiteral(":/icons/%1-%2/apps/64/%3");

  QString themeIcon = kRevFqdnName;
  if (!Settings::value(Settings::Gui::SymbolicTrayIcon).toBool()) {
    if (deskflow::platform::isMac())
      m_trayIcon->setIcon(QIcon::fromTheme(themeIcon));
    else
      m_trayIcon->setIcon(QIcon(fallbackPath.arg(kAppId, QStringLiteral("dark"), themeIcon)));
    return;
  }

  themeIcon.append(QStringLiteral("-symbolic"));

  if (deskflow::platform::isWindows()) {
    QSettings settings(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
        QSettings::NativeFormat
    );
    const QString theme = settings.value(QStringLiteral("SystemUsesLightTheme"), 1).toBool() ? QStringLiteral("light")
                                                                                             : QStringLiteral("dark");
    m_trayIcon->setIcon(QIcon(fallbackPath.arg(kAppId, theme, themeIcon)));
    return;
  }

  auto icon = QIcon::fromTheme(themeIcon, QIcon(fallbackPath.arg(kAppId, iconMode(), themeIcon)));
  icon.setIsMask(true);
  m_trayIcon->setIcon(icon);
}

void MainWindow::handleLogLine(const QString &line)
{
  m_logDock->appendLine(line);
  if (m_shell)
    autoShowLogOnError(line);

  // what a computer that connected says its screen is, for the arrangement of the computers
  m_serverConfig.takeReportedScreenFromLog(line);
  // and the monitors that it says its desktop is made of, which are drawn on the canvas
  m_serverConfig.takeReportedMonitorsFromLog(line);
}

void MainWindow::handleUnrecognisedClient(const QString &clientName)
{
  if (m_ignoredClients.contains(clientName)) {
    qDebug("ignoring %s:", qPrintable(clientName));
    return;
  }

  if (m_newClientPromptShowing || m_serverConfigDialogVisible)
    return;

  if (Settings::value(Settings::Server::ExternalConfig).toBool())
    return;

  if (m_serverConfig.isFull() || m_serverConfig.computerExists(clientName))
    return;

  m_newClientPromptShowing = true;

  showAndActivate();

  if (deskflow::gui::messages::showNewClientPrompt(this, clientName)) {
    serverConnectionConfigureClient(clientName);
  } else {
    m_ignoredClients.insert(clientName);
  }

  m_newClientPromptShowing = false;
}

void MainWindow::handleConnectionRefused(deskflow::core::ConnectionRefusal reason)
{
  if (reason != deskflow::core::ConnectionRefusal::AlreadyConnected)
    return;

  if (!isVisible() || m_clientErrorVisible)
    return;

  m_clientErrorVisible = true;
  showAndActivate();

  const auto address = Settings::textValue(Settings::Client::RemoteHost);
  QMessageBox::warning(
      this, tr("%1 Connection Error").arg(kAppName),
      tr("<p>Failed to connect to the server '%1'.</p>"
         "<p>A Client with your name is already connected to the server.</p>"
         "Please ensure that you're using a unique name and that only a "
         "single instance of the client process is running.</p>")
          .arg(address)
  );

  m_clientErrorVisible = false;
}

void MainWindow::handleMissingKeyboardLayouts(const QString &layouts)
{
  if (Settings::value(Settings::Gui::IgnoreMissingKeyboardLayouts).toBool())
    return;

  QMessageBox msgBox(this);
  msgBox.setIcon(QMessageBox::Warning);
  msgBox.setWindowTitle(tr("Missing Keyboard Layouts"));
  msgBox.setText(tr("<p>Keyboard layout support requires matching layouts on all computers. "
                    "The following layouts from the other computer are not installed on this computer:</p>"
                    "<p><b>%1</b></p>"
                    "<p>Please install them to enable support for these layouts.</p>")
                     .arg(layouts));

  auto *checkBox = new QCheckBox(tr("Don't show this again"), &msgBox);
  msgBox.setCheckBox(checkBox);
  msgBox.exec();

  if (checkBox->isChecked()) {
    Settings::setValue(Settings::Gui::IgnoreMissingKeyboardLayouts, true);
  }
}

void MainWindow::handlePeerFingerprint(const QString &fingerprint)
{
  const auto sha256Text = QString(fingerprint).remove(':');
  const Fingerprint sha256 = {QCryptographicHash::Sha256, QByteArray::fromHex(sha256Text.toLatin1())};

  const bool isClient = m_coreProcess.mode() == CoreMode::Client;
  if ((isClient && m_checkedServers.contains(sha256Text)) || (!isClient && m_checkedClients.contains(sha256Text))) {
    qDebug("ignoring fingerprint, already handled");
    return;
  }

  FingerprintDatabase db;
  db.read(trustedFingerprintDatabase());

  if (db.isTrusted(sha256)) {
    qDebug("fingerprint is trusted");
    return;
  }

  if (isClient) {
    m_checkedServers.append(sha256Text);
    m_coreProcess.stop();
  } else {
    m_checkedClients.append(sha256Text);
  }

  auto mode = isClient ? FingerprintDialogMode::Client : FingerprintDialogMode::Server;
  FingerprintDialog fingerprintDialog(this, m_fingerprint, mode, sha256);

  if (fingerprintDialog.exec() == QDialog::Accepted) {
    db.addTrusted(sha256);
    if (!db.write(trustedFingerprintDatabase())) {
      qCritical().noquote() << "unable to write fingerprint to db:" << trustedFingerprintDatabase();
      m_coreProcess.stop();
      return;
    }
    if (isClient) {
      m_checkedServers.removeAll(sha256Text);
      m_coreProcess.start();
    } else {
      m_checkedClients.removeAll(sha256Text);
    }
  }
}

bool MainWindow::maybeHideToTray()
{
  using deskflow::gui::tray::CloseAction;
  const auto action =
      deskflow::gui::tray::closeActionFor(Settings::value(Settings::Gui::CloseToTray).toBool(), m_trayAvailable());
  if (action == CloseAction::Quit) {
    return false;
  }

  if (action == CloseAction::Minimize) {
    // there is no tray (GNOME without its extension): a window that is hidden could not be found again
    qDebug() << "no system tray, minimizing instead of hiding to tray";
    Settings::setValue(windowGeometryKey(), geometry());
    showMinimized();
    return true;
  }

  if (Settings::value(Settings::Gui::CloseReminder).toBool()) {
    messages::showCloseReminder(this);
    Settings::setValue(Settings::Gui::CloseReminder, false);
  }
  Settings::setValue(windowGeometryKey(), geometry());
  qDebug() << "hiding to tray";
  hide();
  return true;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
  if (event->spontaneous() && maybeHideToTray()) {
    event->ignore();
    return;
  }

  if (m_saveOnExit) {
    Settings::setValue(windowGeometryKey(), geometry());
    Settings::setValue(Settings::Gui::AutoStartCore, m_coreProcess.isStarted());
  }
  qDebug() << "quitting application";

  // any connected dock view acitons will be triggered
  // disconnect them before accepting the event
  disconnect(m_logDock->toggleViewAction(), &QAction::toggled, nullptr, nullptr);

  event->accept();
  QApplication::quit();
}

void MainWindow::showFirstConnectedMessage()
{
  if (Settings::value(Settings::Gui::ShownFirstConnectedMessage).toBool())
    return;
  Settings::setValue(Settings::Gui::ShownFirstConnectedMessage, true);
  messages::showFirstConnectedMessage(this);
}

void MainWindow::updateStatus()
{
  using enum ProcessState;
  // a state that the core comes to is a new state: what failed before it is told no more
  m_trayStartFailed = false;
  const auto connection = m_coreProcess.connectionState();
  const auto process = m_coreProcess.processState();
  const bool isServer = (m_coreProcess.mode() == CoreMode::Server);
  if (process == Stopped || process == Started) {
    updateNetworkInfo();
    ui->btnEditName->setVisible(process == Stopped);
  }
  m_statusBar->setStatus(connection, process, isServer);
  if (m_shell) {
    // a core that is not running has no computers connected, and a core that starts has a new run to show an error in
    if (process == Stopped)
      m_connectedClients.clear();
    if (process == Starting)
      m_logShownByErrorThisRun = false;
    // a core that is stopped or starts reads the settings as they are now: nothing is left to restart
    if (process == Stopped || process == Starting)
      m_shell->restartBanner()->setPending(false);
    m_shell->status()->setClients(m_connectedClients);
    m_shell->status()->setCore(connection, process, isServer);
    updateShellComputers();
  }
  updateTray();
}

void MainWindow::coreProcessStateChanged(ProcessState state)
{
  using enum ProcessState;
  updateStatus();
  if (state == Started) {
    qDebug() << "recording that core has started";
    Settings::setValue(Settings::Gui::AutoStartCore, true);
    if (m_coreProcess.mode() == CoreMode::Server &&
        !Settings::value(Settings::Gui::ShownServerFirstStartMessage).toBool()) {
      qDebug() << "starting core server for first time";
      messages::showFirstServerStartMessage(this);
      Settings::setValue(Settings::Gui::ShownServerFirstStartMessage, true);
    }
  }

  if (state == Started || state == Starting || state == RetryPending) {
    disconnect(ui->btnToggleCore, &QPushButton::clicked, m_actionStartCore, &QAction::trigger);
    connect(ui->btnToggleCore, &QPushButton::clicked, m_actionStopCore, &QAction::trigger, Qt::UniqueConnection);

    ui->btnRestartCore->setEnabled(true);
    m_actionStartCore->setVisible(false);
    m_actionRestartCore->setVisible(true);
    m_actionStopCore->setEnabled(true);

    if (state == Starting) {
      saveSettings();
    }

  } else {
    disconnect(ui->btnToggleCore, &QPushButton::clicked, m_actionStopCore, &QAction::trigger);
    connect(ui->btnToggleCore, &QPushButton::clicked, m_actionStartCore, &QAction::trigger, Qt::UniqueConnection);

    ui->btnRestartCore->setEnabled(false);
    m_actionStartCore->setVisible(true);
    m_actionRestartCore->setVisible(false);
    m_actionStopCore->setEnabled(false);
  }
  updateModeControlLabels();
}

void MainWindow::coreConnectionStateChanged(ConnectionState state)
{
  qDebug() << "core connection state changed:" << static_cast<int>(state);

  updateStatus();

  // always assume connection is not secure when connection changes
  // to anything except connected. the only way the padlock shows is
  // when the correct TLS version string is detected.
  if (state != ConnectionState::Connected) {
    secureSocket(false);
  } else {
    if (m_coreProcess.mode() == CoreMode::Client) {
      if (const auto host = ui->cbHostname->currentText(); !host.isEmpty()) {
        QStringList recentHosts = Settings::value(Settings::Client::RecentRemoteHosts).toStringList();
        if (recentHosts.isEmpty() || recentHosts.first() != host) {
          recentHosts.removeAll(host);
          recentHosts.prepend(host);
          while (recentHosts.size() > 10) {
            recentHosts.removeLast();
          }
          Settings::setValue(Settings::Client::RecentRemoteHosts, recentHosts);
          QSignalBlocker hostnameBlocker(ui->cbHostname);
          ui->cbHostname->clear();
          ui->cbHostname->addItems(recentHosts);
          ui->cbHostname->setCurrentText(host);
        }
      }
    }
    if (isVisible()) {
      showFirstConnectedMessage();
    }
  }
}

void MainWindow::updateFingerprintButton()
{
  m_statusBar->setBtnFingerprintVisible(TlsUtility::isEnabled() && !m_fingerprint.data.isEmpty());
}

void MainWindow::hide()
{
#ifdef Q_OS_MACOS
  macOSNativeHide();
#else
  QMainWindow::hide();
#endif
  m_actionRestore->setVisible(true);
  m_actionMinimize->setVisible(false);
  updateTray();
}

void MainWindow::changeEvent(QEvent *e)
{
  QMainWindow::changeEvent(e);
  if (e->type() == QEvent::PaletteChange) {
    updateIconTheme();
    setWindowIcon(QIcon::fromTheme(kRevFqdnName));
    setTrayIcon();
  } else if (e->type() == QEvent::LanguageChange) {
    ui->retranslateUi(this);
    updateModeControlLabels();
    updateNetworkInfo();
    updateStatus();
    serverClientsChanged({});
    updateText();
    if (m_shell)
      updateShellTexts();
    if (m_header)
      m_header->retranslate();
  }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
  if (obj != ui->lineEditName || event->type() != QEvent::KeyPress)
    return false;
  if (const auto keyEvent = static_cast<QKeyEvent *>(event); keyEvent->key() != Qt::Key_Escape)
    return false;
  ui->lineEditName->hide();
  ui->lblComputerName->show();
  ui->btnEditName->show();
  ui->lineEditName->setText(Settings::value(Settings::Core::ComputerName).toString());
  toggleCanRunCore(canRunCore());
  return true;
}

void MainWindow::updateText()
{
  // the menu of the application of macOS is not a "file" menu: with the new look it is the menu of what is shared
  m_menuFile->setTitle(
      deskflow::gui::style::isDeskflowStyleInstalled() && deskflow::platform::isMac() ? tr("&Sharing") : tr("&File")
  );
  m_menuEdit->setTitle(tr("&Edit"));
  m_menuView->setTitle(tr("&View"));
  m_menuHelp->setTitle(tr("&Help"));

  m_actionMinimize->setText(tr("&Minimize to tray"));
  m_actionQuit->setText(tr("&Quit"));
  m_actionTrayQuit->setText(tr("&Quit"));
  //: %1 will be the replaced with the appname
  m_actionRestore->setText(tr("&Open %1").arg(kAppName));
  m_actionSettings->setText(tr("&Preferences"));
  m_actionStartCore->setText(tr("&Start"));
  m_actionRestartCore->setText(tr("Rest&art"));
  m_actionStopCore->setText(tr("S&top"));
  //: %1 will be the replaced with the appname
  m_actionAbout->setText(tr("About %1...").arg(kAppName));

  m_actionShowHelp->setText(tr("View &Help"));

  //: start / restart core shortcut
  m_actionStartCore->setShortcut(QKeySequence(tr("Ctrl+S")));
  m_actionRestartCore->setShortcut(QKeySequence(tr("Ctrl+S")));

  //: stop core shortcut
  m_actionStopCore->setShortcut(QKeySequence(tr("Ctrl+T")));

  if (deskflow::platform::isWindows()) {
    //: Quit shortcut
    m_actionQuit->setShortcut(QKeySequence(tr("Ctrl+Q")));
    m_actionTrayQuit->setShortcut(QKeySequence(tr("Ctrl+Q")));
  }
  updateTray();
}

void MainWindow::showConfigureServer(const QString &message)
{
  ServerConfigDialog dialog(this, serverConfig());
  dialog.message(message);
  const bool accepted = dialog.exec() == QDialog::Accepted;
  // the arrangement that was saved is what is remembered for the displays that there are
  if (accepted && m_layoutMemory != nullptr)
    m_layoutMemory->configSaved();
  if (accepted && m_coreProcess.isStarted()) {
    m_coreProcess.restart();
  }
}

void MainWindow::showConfigureClient()
{
  ClientConfigDialog dialog(this);
  if (dialog.exec() == QDialog::Accepted) {
    applyConfig();
    if (m_coreProcess.isStarted()) {
      m_coreProcess.restart();
    }
  }
}

void MainWindow::showConnectionTest()
{
  // what is typed in the field, which may not be what the core was started with
  ConnectionTestDialog dialog(this, ui->cbHostname->currentText(), Settings::value(Settings::Core::Port).toInt());
  dialog.runTest();
  dialog.exec();
}

void MainWindow::showServerAddresses()
{
  ServerAddressesDialog dialog(this, m_serverAddresses);
  dialog.exec();
}

void MainWindow::secureSocket(bool secureSocket)
{
  m_secureSocket = secureSocket;
  updateSecurityIcon(m_statusBar->securityIconVisible());
}

void MainWindow::updateComputerName()
{
  const auto computerName = Settings::value(Settings::Core::ComputerName).toString();
  ui->lblComputerName->setText(computerName);
  ui->lineEditName->setText(computerName);
  m_serverConfig.updateServerName();
}

void MainWindow::showAndActivate()
{
  const auto wasVisible = isVisible();
#ifdef Q_OS_MACOS
  forceAppActive();
#endif
  showNormal();
  raise();
  activateWindow();
  m_actionRestore->setVisible(false);
  m_actionMinimize->setVisible(true);
  if (!wasVisible)
    restoreWindow();
  updateTray();
}

void MainWindow::showHostNameEditor()
{
  ui->lineEditName->show();
  ui->lblComputerName->hide();
  ui->btnEditName->hide();
  toggleCanRunCore(false);
  ui->lineEditName->setFocus();
}

void MainWindow::setHostName()
{
  ui->lineEditName->hide();
  ui->lblComputerName->show();
  ui->btnEditName->show();
  toggleCanRunCore(canRunCore());

  QString text = ui->lineEditName->text();
  const auto computerName = Settings::value(Settings::Core::ComputerName).toString();

  if (text == computerName)
    return;

  const bool isServer = ui->rbModeServer->isChecked();
  bool existingComputer = false;
  if (isServer)
    existingComputer = serverConfig().computerExists(text);

  if (!ui->lineEditName->hasAcceptableInput() || text.isEmpty() || existingComputer) {
    blockSignals(true);
    ui->lineEditName->setText(computerName);
    blockSignals(false);

    const auto title = tr("Invalid Computer Name");
    QString body;
    if (existingComputer) {
      body = tr("Computer name already exists");
    } else {
      body =
          tr("The name you have chosen is invalid.\n\n"
             "Valid names:\n"
             "• Use letters and numbers\n"
             "• May also use _ or -\n"
             "• Are between 1 and 255 characters");
    }
    QMessageBox::information(this, title, body);
    return;
  }

  ui->lblComputerName->setText(ui->lineEditName->text());
  Settings::setValue(Settings::Core::ComputerName, ui->lineEditName->text());
  if (isServer)
    serverConfig().updateServerName();
  applyConfig();
}

QString MainWindow::trustedFingerprintDatabase() const
{
  const bool isClient = m_coreProcess.mode() == CoreMode::Client;
  return isClient ? Settings::tlsTrustedServersDb() : Settings::tlsTrustedClientsDb();
}

bool MainWindow::generateCertificate()
{
  const auto certificate = Settings::value(Settings::Security::Certificate).toString();
  if (!QFile::exists(certificate) && !TlsUtility::generateCertificate()) {
    return false;
  }

  m_fingerprint = {QCryptographicHash::Sha256, TlsUtility::certFingerprint()};
  updateFingerprintButton();
  return true;
}

void MainWindow::serverClientsChanged(const QStringList &clients)
{
  if (m_shell) {
    const bool active = m_coreProcess.mode() == CoreMode::Server && m_coreProcess.isStarted();
    m_connectedClients = active ? clients : QStringList();
    m_shell->status()->setClients(m_connectedClients);
    updateShellComputers();
    updateTray();
  }
  if (m_coreProcess.mode() != CoreMode::Server || !m_coreProcess.isStarted())
    return;
  m_statusBar->setServerClients(clients);
}

void MainWindow::daemonIpcClientConnectionFailed()
{
  if (deskflow::gui::messages::showDaemonOffline(this)) {
    m_coreProcess.retryDaemon();
  }
}

void MainWindow::toggleCanRunCore(bool enableButtons)
{
  const bool isStarted = m_coreProcess.isStarted();
  ui->btnToggleCore->setEnabled(enableButtons);
  ui->btnRestartCore->setEnabled(enableButtons && isStarted);
  m_actionStartCore->setEnabled(enableButtons);
  m_actionStopCore->setEnabled(enableButtons && isStarted);
  updateTray();
}

void MainWindow::remoteHostChanged(const QString &newRemoteHost)
{
  m_coreProcess.setAddress(newRemoteHost);
  ui->btnTestConnection->setEnabled(!newRemoteHost.trimmed().isEmpty());
  toggleCanRunCore(!newRemoteHost.isEmpty() && ui->rbModeClient->isChecked());
  if (newRemoteHost.isEmpty()) {
    Settings::setValue(Settings::Client::RemoteHost);
  } else {
    Settings::setValue(Settings::Client::RemoteHost, newRemoteHost);
  }
}

void MainWindow::updateIpLabel(const QStringList &addresses)
{
  const auto mode = m_coreProcess.mode();
  if (mode == CoreMode::None) {
    return;
  }

  static const auto colorText = QStringLiteral(R"(<span style="color:%1;">%2</span>)");
  const bool serverStarted = m_coreProcess.isStarted();
  const bool fixedIP = !Settings::value(Settings::Core::Interface).toString().isEmpty();

  if (!fixedIP && addresses.isEmpty() && !serverStarted || (serverStarted && m_serverStartSuggestedIP.isEmpty())) {
    ui->lblIpAddresses->setText(colorText.arg(palette().linkVisited().color().name(), tr("No IP Detected")));
    ui->lblIpAddresses->setToolTip(tr("Unable to detect an IP address. Check your network connection is active."));
    return;
  }

  QString labelText = fixedIP ? tr("Using IP: ") : tr("Suggested IP: ");
  QString toolTipText = tr("<p>If connecting via the hostname fails, try %1</p>");

  const bool filterIpList = (serverStarted || fixedIP);
  const QRegularExpression ipListFilter(filterIpList ? QStringLiteral("(%1)").arg(m_serverStartIPs.join("|")) : "");
  const QStringList ipList = addresses.filter(ipListFilter);

  bool IPValid = true;
  if (filterIpList && (m_serverStartSuggestedIP != m_currentIpAddress) || !ipList.contains(m_serverStartSuggestedIP)) {
    IPValid = !ipList.isEmpty();
  }

  if (IPValid) {
    m_currentIpAddress = ipList.first();
    labelText.append(m_currentIpAddress);
  } else {
    labelText.append(colorText.arg(palette().linkVisited().color().name(), m_serverStartSuggestedIP));
    toolTipText.append(tr("\nA bound IP is now invalid, you may need to restart the server."));
  }

  if (ipList.count() < 2 || fixedIP) {
    toolTipText = toolTipText.arg(tr("the suggested IP."));
  } else {
    toolTipText = toolTipText.arg(tr("one of the following IPs:<br/>%1").arg(ipList.join("<br/>")));
  }

  // With more than one address to choose from, say which network card each is on and what it is for, so that a
  // client is not given the address of a network it cannot reach. With one address nothing changes.
  m_serverAddresses.clear();
  if (mode == CoreMode::Server && !fixedIP) {
    for (const auto &info : usableIpv4Addresses()) {
      if (ipList.contains(info.address)) {
        m_serverAddresses.append(info);
      }
    }
  }
  if (m_serverAddresses.size() >= 2) {
    labelText.append(
        QStringLiteral(" (<a href=\"addresses\">%1</a>)").arg(tr("all %1 addresses").arg(m_serverAddresses.size()))
    );
    toolTipText = tr("<p>If connecting via the hostname fails, try one of the following IPs:</p>") +
                  addressListToolTip(m_serverAddresses);
    if (!IPValid) {
      toolTipText.append(tr("<p>A bound IP is now invalid, you may need to restart the server.</p>"));
    }
  }

  ui->lblIpAddresses->setText(labelText);
  ui->lblIpAddresses->setToolTip(mode == CoreMode::Server ? toolTipText : QString());
}

void MainWindow::updateTimeoutDelay(int newDelay)
{
  m_statusBar->setConnectionInterval(newDelay);
}

void MainWindow::setHelpFilePath()
{
  const QString appPath = QCoreApplication::applicationDirPath();
  auto buildPath = QString("%1/../docs/HelpMain.md").arg(appPath);
  auto installPath = QString("%1/../share/doc/%2/HelpMain.md").arg(appPath, kAppId);
  if (deskflow::platform::isMac()) {
    installPath = QString("%1/../Resources/docs/HelpMain.md").arg(appPath);
    buildPath = QString("%1/../../../../docs/HelpMain.md").arg(appPath);
  } else if (deskflow::platform::isWindows()) {
    installPath = QString("%1/docs/HelpMain.md").arg(appPath);
  }

  buildPath = QDir::cleanPath(buildPath);
  installPath = QDir::cleanPath(installPath);

  if (QFile::exists(installPath))
    m_helpPath = QUrl::fromLocalFile(installPath);
  else if (QFile::exists(buildPath))
    m_helpPath = QUrl::fromLocalFile(buildPath);
  else
    m_helpPath = kUrlWiki;
}

void MainWindow::showHelpViewer() const
{
  if (m_helpPath.isLocalFile()) {
    HelpDialog dialogHelp(this->centralWidget(), m_helpPath);
    dialogHelp.exec();
  } else {
    QDesktopServices::openUrl(m_helpPath);
  }
}

bool MainWindow::canRunCore() const
{
  const auto mode = m_coreProcess.mode();
  const bool isServer = mode == Settings::CoreMode::Server;
  const bool isClient = mode == Settings::CoreMode::Client;
  return ((isServer || isClient) && (isClient && !ui->cbHostname->currentText().isEmpty()) || isServer);
}

//////////////////////////////////////////////////////////////////////////////
// The main window of the new look
//////////////////////////////////////////////////////////////////////////////

void MainWindow::setupShell()
{
  using deskflow::gui::MainShell;
  using deskflow::gui::MainShellParts;

  MainShellParts parts;
  parts.central = ui->topLevelWidget;
  parts.oldGroup = ui->groupBox;
  parts.nameRow = ui->widget;
  parts.ipLabel = ui->lblIpAddresses;
  parts.modeServer = ui->rbModeServer;
  parts.modeClient = ui->rbModeClient;
  parts.noMode = ui->lblNoMode;
  parts.configureServer = ui->btnConfigureServer;
  parts.exportServer = ui->btnSaveServerConfig;
  parts.configureClient = ui->btnConfigureClient;
  parts.testConnection = ui->btnTestConnection;
  parts.toggleCore = ui->btnToggleCore;
  parts.restartCore = ui->btnRestartCore;
  parts.hostLabel = ui->m_pLabelServerName;
  parts.hostname = ui->cbHostname;
  parts.noticeBar = m_layoutNoticeBar;
  m_shell = new MainShell(parts, this);

  // what the user pressed in the sidebar
  connect(m_shell->rail(), &NavRail::activated, this, &MainWindow::shellEntryActivated);
  // the sidebar that is folded is remembered; the one that a narrow window folded is not
  m_shell->sidebar()->setCollapsed(Settings::value(Settings::Gui::SidebarCollapsed).toBool());
  connect(m_shell->sidebar(), &SidebarShell::collapsedChanged, this, [](bool collapsed) {
    Settings::setValue(Settings::Gui::SidebarCollapsed, collapsed);
  });
  connect(m_logDock->toggleViewAction(), &QAction::toggled, m_shell, &deskflow::gui::MainShell::setLogShown);
  m_shell->setLogShown(!m_logDock->isHidden());

  // the View menu folds and unfolds the sidebar as its button does. Ctrl+B is the shortcut of the sidebar in the
  // editors of code and is free here; the Mac has its own for a sidebar (Control+Command+S)
  m_actionToggleSidebar = new QAction(this);
  m_actionToggleSidebar->setShortcut(
      deskflow::platform::isMac() ? QKeySequence(Qt::CTRL | Qt::META | Qt::Key_S) : QKeySequence(Qt::CTRL | Qt::Key_B)
  );
  m_actionToggleSidebar->setMenuRole(QAction::NoRole);
  connect(m_actionToggleSidebar, &QAction::triggered, this, [this] { m_shell->rail()->toggleButton()->click(); });
  connect(m_shell->rail(), &NavRail::expandedChanged, this, [this] {
    m_actionToggleSidebar->setText(m_shell->rail()->toggleButton()->accessibleName());
  });
  m_menuView->addAction(m_actionToggleSidebar);

  // the size of text: larger, smaller, and back to Automatic. They are keys of the window (the menu bar is not shown
  // with the header), and they write the setting, which the page of the settings and the service follow
  const auto textSizeAction = [this](const QString &name, const QList<QKeySequence> &keys, int direction) {
    auto *action = new QAction(this);
    action->setObjectName(name);
    action->setShortcuts(keys);
    action->setMenuRole(QAction::NoRole);
    connect(action, &QAction::triggered, this, [direction] {
      const auto current = deskflow::gui::style::snapTextPercent(Settings::value(Settings::Gui::TextSize).toInt());
      Settings::setValue(Settings::Gui::TextSize, deskflow::gui::style::stepTextPercent(current, direction));
    });
    addAction(action);
    return action;
  };
  m_actionTextLarger = textSizeAction(
      QStringLiteral("actionTextLarger"),
      {QKeySequence(Qt::CTRL | Qt::Key_Equal), QKeySequence(Qt::CTRL | Qt::Key_Plus)}, 1
  );
  m_actionTextSmaller =
      textSizeAction(QStringLiteral("actionTextSmaller"), {QKeySequence(Qt::CTRL | Qt::Key_Minus)}, -1);
  m_actionTextReset = textSizeAction(QStringLiteral("actionTextReset"), {QKeySequence(Qt::CTRL | Qt::Key_0)}, 0);

  // the settings are pages of the sidebar; a change is written at once and sharing is restarted by the banner
  m_settingsPages = new deskflow::gui::settings::SettingsPages(this);
  for (const auto &entry : m_settingsPages->entries())
    m_shell->addSettingsPage(entry.id, entry.icon, entry.title(), entry.page);
  connect(m_settingsPages, &deskflow::gui::settings::SettingsPages::applied, this, &MainWindow::settingApplied);
  connect(
      m_settingsPages, &deskflow::gui::settings::SettingsPages::fingerprintRequested, this,
      &MainWindow::showMyFingerprint
  );
  connect(
      m_settingsPages, &deskflow::gui::settings::SettingsPages::removeAllSettingsRequested, this,
      &MainWindow::clearSettings
  );
  connect(m_shell->restartBanner(), &deskflow::gui::settings::RestartBanner::restartRequested, this, [this] {
    m_shell->restartBanner()->setPending(false);
    resetCore();
  });

  // the status bar is what says the state (the sentence, the encryption, the buttons): it is kept as the source,
  // hidden, and the shell shows it
  m_statusBar->setHeadless(true);
  connect(m_statusBar, &StatusBar::changed, this, &MainWindow::scheduleShellRefresh);
  // the tray says what the status says: its header is the headline of the window
  connect(m_shell->status(), &deskflow::gui::StatusModel::changed, this, &MainWindow::updateTray);

  // the log says what the size of text is, and why: a person who finds the text small can tell what the program was
  // given
  if (auto *textSize = deskflow::gui::style::TextSize::instance()) {
    qInfo().noquote() << textSize->describe();
    connect(textSize, &deskflow::gui::style::TextSize::planChanged, this, [textSize] {
      qInfo().noquote() << textSize->describe();
    });
  }

  // the window is usable down to a width that follows the size of text, and never asks for more than the screen has
  const auto sizeWindow = [this] {
    int width = deskflow::gui::MainShell::minimumWindowWidth();
    if (const auto *screen = this->screen())
      width = qMin(width, qMax(deskflow::gui::MainShell::kMinimumWindowWidth, screen->availableGeometry().width()));
    setMinimumWidth(width);
  };
  sizeWindow();
  deskflow::gui::style::onTextSizeChanged(this, sizeWindow);
  updateModeControls();
  updateModeControlLabels();
  updateShellTexts();
  updateStatus();
}

void MainWindow::applyWindowChrome()
{
  using namespace deskflow::gui::chrome;
  if (!m_shell)
    return;
  m_frame = decideFrame(ChromeEnvironment::current());
  qInfo().noquote() << "window frame:" << (m_frame.clientSide() ? "client side" : "native") << "("
                    << reasonName(m_frame.reason) << ")";
  if (!m_frame.clientSide())
    return;

  // a Mac has the buttons of the window in its title bar, and the title there; the others have them in the header
  const bool mac = deskflow::platform::isMac();
  WindowHeader::Options options;
  options.captionButtons = !mac;
  options.leftInset = mac ? WindowHeader::kTrafficLightsInset : 0;
  options.title = !mac;
  auto *header = new WindowHeader(options, this);
  auto *chrome = new WindowChrome(this, header, this);
  if (!chrome->apply()) {
    qWarning() << "the frame of the new look could not be made: the window keeps the frame of the system";
    delete chrome;
    delete header;
    m_frame = FrameDecision{FrameKind::Native, FrameReason::PlatformPluginUnsupported};
    return;
  }
  m_header = header;
  m_chrome = chrome;
  m_shell->setHeader(header);
  header->setTitle(windowTitle());
  connect(this, &QWidget::windowTitleChanged, header, &WindowHeader::setTitle);
  connect(header, &WindowHeader::closeRequested, this, &MainWindow::closeFromHeader);
  buildHeaderMenu();
  // the menu bar of a Mac is the one of the system, at the top of the screen
  if (!mac)
    hideMenuBar();
}

QAction *MainWindow::actionFor(deskflow::gui::chrome::MenuAction action) const
{
  using deskflow::gui::chrome::MenuAction;
  switch (action) {
  case MenuAction::Start:
    return m_sharingStart;
  case MenuAction::Restart:
    return m_sharingRestart;
  case MenuAction::Stop:
    return m_sharingStop;
  case MenuAction::TestConnection:
    return m_sharingTestConnection;
  case MenuAction::ViewFingerprint:
    return m_sharingFingerprint;
  case MenuAction::MinimizeToTray:
    return m_sharingMinimize;
  case MenuAction::Quit:
    return m_actionQuit;
  case MenuAction::Preferences:
    return m_actionSettings;
  case MenuAction::ToggleLog:
    return m_logDock->toggleViewAction();
  case MenuAction::ToggleSidebar:
    return m_actionToggleSidebar;
  case MenuAction::About:
    return m_actionAbout;
  case MenuAction::ViewHelp:
    return m_actionShowHelp;
  }
  return nullptr;
}

void MainWindow::buildHeaderMenu()
{
  using namespace deskflow::gui::chrome;
  auto *menu = new QMenu(this);
  menu->setObjectName(QStringLiteral("headerMenu"));
  // the rows are the actions of the window, in the order of the table of what the old menus did: their texts, their
  // state and their keys follow the window, and the table is what the tests check
  int group = -1;
  for (const auto &entry : menuMigrations(deskflow::platform::isMac())) {
    auto *action = actionFor(entry.action);
    if (!action)
      continue;
    if (group >= 0 && entry.group != group)
      menu->addSeparator();
    group = entry.group;
    menu->addAction(action);
  }
  m_header->setMenu(menu);
}

void MainWindow::hideMenuBar()
{
  // a key that belongs to an action of a menu that is not shown would not work: the actions are the window's as well
  QList<QMenu *> menus{m_menuFile, m_menuEdit, m_menuView, m_menuHelp};
  if (m_header && m_header->menu())
    menus << m_header->menu();
  for (auto *menu : std::as_const(menus)) {
    for (auto *action : menu->actions()) {
      if (!action->isSeparator())
        addAction(action);
    }
  }
  menuBar()->hide();
}

void MainWindow::closeFromHeader()
{
  // the close of the system goes to the tray, or ends the program, as the settings say
  if (!maybeHideToTray())
    close();
}

void MainWindow::settingApplied(const QString &key)
{
  // what the window shows follows at once
  if (key == Settings::Gui::ShowVersionInTitle)
    updateWindowTitle();
  else if (key == Settings::Gui::SymbolicTrayIcon)
    setTrayIcon();

  // what the core reads is in use when the core starts again
  using deskflow::gui::settings::Effect;
  const auto effect = deskflow::gui::settings::effectOf(key);
  const auto state = m_coreProcess.processState();
  if (effect == Effect::RestartSharing && (state == ProcessState::Started || state == ProcessState::Starting))
    m_shell->restartBanner()->setPending(true);
}

void MainWindow::updateShellTexts()
{
  deskflow::gui::MainShell::Texts texts;
  texts.preferences = m_actionSettings->text();
  texts.log = m_logDock->toggleViewAction()->text();
  texts.about = m_actionAbout->text();
  texts.fingerprint = m_statusBar->fingerprintToolTip();
  texts.updateAvailable = m_statusBar->updateButtonText();
  m_shell->setTexts(texts);
  m_shell->clientSection()->setAddressHint(tr("LAN address first, then the VPN address, separated by commas"));
  m_actionToggleSidebar->setText(m_shell->rail()->toggleButton()->accessibleName());
  for (const auto &entry : m_settingsPages->entries())
    m_shell->setPageText(entry.id, entry.title());
  m_shell->restartBanner()->retranslate();
  updateShellComputers();
  refreshShell();
}

void MainWindow::scheduleShellRefresh()
{
  if (!m_shell || m_shellRefreshPending)
    return;
  m_shellRefreshPending = true;
  QMetaObject::invokeMethod(this, &MainWindow::refreshShell, Qt::QueuedConnection);
}

void MainWindow::refreshShell()
{
  m_shellRefreshPending = false;
  if (!m_shell)
    return;
  m_shell->status()->setDetail(m_statusBar->statusText());
  m_shell->status()->setSecurity(
      m_statusBar->securityIconVisible(), m_statusBar->isEncrypted(), m_statusBar->securityText()
  );
  m_shell->setFingerprintVisible(m_statusBar->fingerprintVisible());
  m_shell->setUpdateVersion(m_statusBar->updateVersion());
  m_shell->setEncryptionEnabled(TlsUtility::isEnabled());
  // the tray says the sentence of the status and the update as well
  updateTray();
}

void MainWindow::updateShellComputers()
{
  if (!m_shell)
    return;
  const auto &reported = m_serverConfig.reportedMonitorsByComputer();
  const auto thisComputer = Settings::value(Settings::Core::ComputerName).toString();

  QList<deskflow::gui::MainShell::ComputerEntry> entries;
  for (const auto &computer : std::as_const(m_serverConfig).computers()) {
    if (computer.isNull() || computer.isServer() || computer.name() == thisComputer)
      continue;
    deskflow::gui::MainShell::ComputerEntry entry;
    entry.name = computer.name();
    entry.connected = m_connectedClients.contains(computer.name());
    if (const auto found = reported.constFind(computer.name()); found != reported.constEnd() && !found->isEmpty()) {
      const auto count = found->monitors.size();
      entry.detail = (count == 1 ? tr("1 monitor") : tr("%1 monitors").arg(count)) + QStringLiteral(" · ") +
                     tr("Reported by the computer");
    }
    entries.append(entry);
  }
  m_shell->setComputers(entries);
}

void MainWindow::shellEntryActivated(const QString &id)
{
  if (id == QLatin1String("preferences"))
    openSettings();
  else if (id == QLatin1String("log"))
    m_logDock->toggleViewAction()->toggle();
  else if (id == QLatin1String("about"))
    openAboutDialog();
  else if (id == QLatin1String("fingerprint"))
    showMyFingerprint();
  else if (id == QLatin1String("update"))
    openGetNewVersionUrl();
}

void MainWindow::autoShowLogOnError(const QString &line)
{
  static const QRegularExpression errorLine(QStringLiteral(R"(^\[[^\]]*\]\s+(ERROR|CRITICAL|FATAL):)"));
  if (m_logShownByErrorThisRun || !m_logDock->isHidden() || !isVisible() || isMinimized())
    return;
  if (!errorLine.match(line).hasMatch())
    return;
  m_logShownByErrorThisRun = true;
  m_logShownByError = true;
  m_logDock->show();
  m_logShownByError = false;
}
