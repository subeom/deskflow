/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2024 - 2026 Chris Rizzitello <sithord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 - 2024 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QMainWindow>
#include <QProcess>
#include <QRegularExpression>
#include <QSystemTrayIcon>
#include <QUrl>

#include <functional>

#include "VersionChecker.h"
#include "config/ServerConfig.h"
#include "gui/chrome/ChromePolicy.h"
#include "gui/core/AddressClassifier.h"
#include "gui/core/CoreProcess.h"
#include "gui/core/NetworkMonitor.h"
#include "gui/tunnel/TunnelTypes.h"
#include "net/Fingerprint.h"

#ifdef Q_OS_MACOS
#include "gui/OSXHelpers.h"
#endif

class InlineHint;
class QAction;
class QLabel;
class QMenu;
class QLocalServer;

class DeskflowApplication;
class LayoutNoticeBar;
class LogDock;
class StatusBar;

namespace deskflow::gui {
class MainShell;
class IUpdateInstaller;
class UpdateShutdown;
class UpdateStatusModel;
enum class UpdateAction;
}

namespace deskflow::gui::settings {
class SettingsPages;
}

namespace deskflow::gui::input {
class MacInputIndicatorController;
}

namespace deskflow::gui::layout {
class LayoutMemoryService;
}

namespace deskflow::gui::chrome {
class WindowChrome;
class WindowHeader;
enum class MenuAction;
} // namespace deskflow::gui::chrome

namespace deskflow::gui::tray {
class TrayController;
enum class TrayCommand;
struct TrayInput;
}

namespace Ui {
class MainWindow;
}

namespace deskflow::gui::ipc {
class DaemonIpcClient;
}

struct MainWindowTestAccess;

// The option that turns off the wireless link of AirDrop while sharing (macOS, see docs/Configuration.md)
namespace deskflow::awdl {
class AwdlController;
}

// Cloudflare Tunnel (see docs/dev/cloudflare-tunnel.md)
namespace deskflow::tunnel {
class TunnelController;
}
namespace deskflow::gui {
class TunnelStatusWidget;
}

class MainWindow : public QMainWindow
{
  //! The tests of the main window reach what the window keeps (the core is never started by them)
  friend struct ::MainWindowTestAccess;

  using ConnectionState = deskflow::core::ConnectionState;
  using CoreMode = Settings::CoreMode;
  using CoreProcess = deskflow::gui::CoreProcess;
  using NetworkMonitor = deskflow::gui::NetworkMonitor;
  using ProcessState = deskflow::core::ProcessState;

  Q_OBJECT

public:
  enum class LogLevel
  {
    Error,
    Info
  };

public:
  explicit MainWindow();
  ~MainWindow() override;

  [[nodiscard]] CoreMode coreMode() const
  {
    return m_coreProcess.mode();
  }
  void open();
  ServerConfig &serverConfig()
  {
    return m_serverConfig;
  }

  void hide();

protected:
  void changeEvent(QEvent *e) override;
  bool eventFilter(QObject *obj, QEvent *event) override;

private:
  /**
   * @brief updateText Update all text not in the UI
   */
  void updateText();
  void toggleLogVisible(bool visible);

  void settingsChanged(const QString &key = QString());
  void serverConfigSaving();
  //! Experimental: remembers the arrangement for each set of displays (does nothing unless it is switched on)
  void setupLayoutMemory();
  void updateLayoutNotice();
  void coreProcessError(CoreProcess::Error error);
  void coreConnectionStateChanged(ConnectionState state);
  void coreProcessStateChanged(ProcessState state);

  void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
  void serverConnectionConfigureClient(const QString &clientName);

  void clearSettings();
  void openAboutDialog();
  void openGetNewVersionUrl() const;
  void checkForUpdates();
  void handleUpdateAction(deskflow::gui::UpdateAction action);
  void confirmAndInstallUpdate();
  void updateShutdownCompleted();
  void updateShutdownFailed(const QString &message);
  void refreshUpdateStatus();
  void publishUpdateHandshake();
  void openSettings();
  void startCore();
  void stopCore();
  bool saveServerConfig();
  void resetCore();

  void showMyFingerprint();
  void updateSecurityIcon(bool visible);
  void updateNetworkInfo();

  void coreModeToggled(bool checked);
  void updateModeControls();
  void updateModeControlLabels();
  std::unique_ptr<Ui::MainWindow> ui;

  void createMenuBar();
  void setupTrayIcon();
  //! The tray of the new look: its menu says the state (see TrayController); the window gives it what it knows
  void setupTrayController(QMenu *menu);
  void updateTray();
  //! The menu bar of the new look: a menu of sharing whose rows are all there, and dimmed when they do not apply
  void createSharingMenu();
  void updateSharingMenu(const deskflow::gui::tray::TrayInput &input);
  void trayCommand(deskflow::gui::tray::TrayCommand command);
  // Cloudflare Tunnel
  void setupTunnel();
  void tunnelUsedChanged(bool used);
  void updateTunnelServerLink(ConnectionState state);
  //! The window of the new look: the lines of the tunnel are in its shell (see attachTunnelToShell in MainWindow.cpp)
  void attachTunnelToShell();
  void updateTunnelShell();
  // The option that turns off the wireless link of AirDrop while sharing (macOS only: nowhere else is anything made)
  void setupAwdl();
  void attachAwdlToShell();
  void applyConfig();
  void setTrayIcon();
  void handleUnrecognisedClient(const QString &clientName);
  void handleConnectionRefused(deskflow::core::ConnectionRefusal reason);
  void handlePeerFingerprint(const QString &fingerprint);
  void handleMissingKeyboardLayouts(const QString &layouts);
  void closeEvent(QCloseEvent *event) override;
  bool maybeHideToTray();
  bool canQuitApplication();
  bool shouldQuitForNativeRequest();
  void secureSocket(bool secureSocket);
  void connectSlots();
  void handleLogLine(const QString &line);
  void updateFingerprintButton();
  void updateComputerName();
  void saveSettings() const;
  void showConfigureServer(const QString &message);
  void showConfigureClient();
  void showConnectionTest();
  void showServerAddresses();
  void restoreWindow();
  //! What a window of the new look opens with: the size that is wanted, inside of the work area of the screen
  QSize firstShellSize(const QSize &wanted, const QRect &workArea) const;
  //! Where the window keeps its position and size: each look has its own
  const QString &windowGeometryKey() const;
  void setupControls();
  void showFirstConnectedMessage();
  void updateStatus();
  void showAndActivate();
  void showHostNameEditor();
  void setHostName();
  void daemonIpcClientConnectionFailed();
  void toggleCanRunCore(bool enableButtons);
  void remoteHostChanged(const QString &newRemoteHost);
  void updateIpLabel(const QStringList &addresses);
  void updateTimeoutDelay(int newDelay);
  void setHelpFilePath();
  void showHelpViewer() const;

  //! The main window of the new look (Appearance: deskflow): the sidebar and the page. It takes over the controls of
  //! this window and is told what to show; the old window is what there is without it.
  void setupShell();
  //! The frame of the new look (a header in the place of the title bar and the menu bar of the system, see
  //! WindowChrome), when the platform and the person allow it; otherwise the window keeps the frame of the system
  void applyWindowChrome();
  void buildHeaderMenu();
  //! The action of the window that does what an action of the old menus did
  QAction *actionFor(deskflow::gui::chrome::MenuAction action) const;
  //! The menu bar is not shown; what had a key in its menus keeps the key
  void hideMenuBar();
  //! What the close button of the header does: what the close of the system does
  void closeFromHeader();
  void updateShellTexts();
  //! The shell shows what the status bar says: it is asked once for a run of changes
  void scheduleShellRefresh();
  void refreshShell();
  void updateShellComputers();
  void shellEntryActivated(const QString &id);
  void autoShowLogOnError(const QString &line);
  //! A page of the settings changed a setting: what the window shows follows, and sharing is to be restarted when the
  //! core reads the setting
  void settingApplied(const QString &key);
  void updateWindowTitle();

  bool canRunCore() const;

  /**
   * @brief trustedFingerprintDatabase get the FingerprintDatabase for the trusted clients or trusted servers.
   * @return The path to the trusted fingerprint file
   */
  QString trustedFingerprintDatabase() const;

  /**
   * @brief generateCertificate Generate a new certificate
   * @return true when successful
   */
  bool generateCertificate();

  Fingerprint m_fingerprint;

  void serverClientsChanged(const QStringList &clients);

  inline static const auto m_guiSocketName = QStringLiteral("deskflow-gui");
  inline static const auto m_nameRegEx = QRegularExpression(QStringLiteral("^[\\w\\-_\\.]{0,255}$"));

  VersionChecker m_versionChecker;
  deskflow::gui::UpdateStatusModel *m_updateStatus = nullptr;
  std::unique_ptr<deskflow::gui::IUpdateInstaller> m_updateInstaller;
  deskflow::gui::UpdateShutdown *m_updateShutdown = nullptr;
  QString m_updateReceiptPath;
  bool m_updateApplying = false;
  bool m_updateExitAuthorized = false;
  bool m_secureSocket = false;
  bool m_saveOnExit = true;
  bool m_clientErrorVisible = false;
  ServerConfig m_serverConfig;
  deskflow::gui::CoreProcess m_coreProcess;
  QSet<QString> m_ignoredClients;
  bool m_newClientPromptShowing = false;
  bool m_serverConfigDialogVisible = false;
  deskflow::gui::layout::LayoutMemoryService *m_layoutMemory = nullptr;
  LayoutNoticeBar *m_layoutNoticeBar = nullptr;
  QSize m_expandedSize = QSize();
  QStringList m_checkedClients;
  QStringList m_checkedServers;
  QSystemTrayIcon *m_trayIcon = nullptr;
  //! Whether the OS has a tray to hide the window in (the tests of the tray say what they need)
  std::function<bool()> m_trayAvailable = [] { return QSystemTrayIcon::isSystemTrayAvailable(); };
  //! The work area of the screen that the window opens on (the tests of the first size say what they need)
  inline static std::function<QRect()> s_workArea;
  // The tray of the new look: null while the application has the look of the system
  deskflow::gui::tray::TrayController *m_trayController = nullptr;
  bool m_trayStartFailed = false;
  QLocalServer *m_guiDupeChecker = nullptr;
  deskflow::gui::ipc::DaemonIpcClient *m_daemonIpcClient = nullptr;

  LogDock *m_logDock;
  StatusBar *m_statusBar = nullptr;

  // The new look: null while the application has the look of the system
  deskflow::gui::MainShell *m_shell = nullptr;
  // the frame of the new look: the header and what makes the window have no title bar (null with the frame of the
  // system)
  deskflow::gui::chrome::FrameDecision m_frame;
  deskflow::gui::chrome::WindowHeader *m_header = nullptr;
  deskflow::gui::chrome::WindowChrome *m_chrome = nullptr;
  deskflow::gui::settings::SettingsPages *m_settingsPages = nullptr;
  deskflow::gui::input::MacInputIndicatorController *m_inputIndicatorController = nullptr;
  bool m_shellRefreshPending = false;
  QStringList m_connectedClients;
  // the log was shown by an error (and is not what the user chose): it is not remembered, and it is done once for a run
  bool m_logShownByError = false;
  bool m_logShownByErrorThisRun = false;

  // Window Menu
  QMenu *m_menuFile = nullptr;
  QMenu *m_menuEdit = nullptr;
  QMenu *m_menuView = nullptr;
  QMenu *m_menuHelp = nullptr;

  // Window Actions
  QAction *m_actionAbout = nullptr;
  QAction *m_actionMinimize = nullptr;
  QAction *m_actionQuit = nullptr;
  QAction *m_actionTrayQuit = nullptr;
  QAction *m_actionRestore = nullptr;
  QAction *m_actionSettings = nullptr;
  QAction *m_actionStartCore = nullptr;
  QAction *m_actionRestartCore = nullptr;
  QAction *m_actionStopCore = nullptr;
  QAction *m_actionShowHelp = nullptr;
  //! Only in the window of the new look (View menu): folds and unfolds the sidebar, as its button does
  QAction *m_actionToggleSidebar = nullptr;
  // the keys of the size of text (the new look): larger, smaller, and back to Automatic
  QAction *m_actionTextLarger = nullptr;
  QAction *m_actionTextSmaller = nullptr;
  QAction *m_actionTextReset = nullptr;
  // Only in the window of the new look: the rows of the menu of sharing (the actions above are the ones of the system look)
  QAction *m_sharingStart = nullptr;
  QAction *m_sharingRestart = nullptr;
  QAction *m_sharingStop = nullptr;
  QAction *m_sharingTestConnection = nullptr;
  QAction *m_sharingFingerprint = nullptr;
  QAction *m_sharingMinimize = nullptr;

  // The wireless link of AirDrop (null: not a Mac, or not a Mac window)
  deskflow::awdl::AwdlController *m_awdl = nullptr;
  // Cloudflare Tunnel
  deskflow::tunnel::TunnelController *m_tunnelController = nullptr;
  deskflow::gui::TunnelStatusWidget *m_tunnelStatus = nullptr;
  QAction *m_actionTunnelStatus = nullptr;
  // The window of the new look: the widget above only knows what the two lines say (it is in a holder that is never
  // shown), the lines are these, in the card of the connection
  QWidget *m_tunnelHolder = nullptr;
  QLabel *m_tunnelPathLine = nullptr;
  InlineHint *m_tunnelStatusLine = nullptr;
  deskflow::tunnel::ServerLink m_tunnelServerLink = deskflow::tunnel::ServerLink::Unknown;
  deskflow::tunnel::ServerLinkTracker m_tunnelLinkTracker;

  // Network monitoring
  NetworkMonitor *m_networkMonitor = nullptr;
  QString m_currentIpAddress;
  // every IPv4 address a client could use, when there is more than one to choose from (empty otherwise)
  QList<deskflow::gui::AddressInfo> m_serverAddresses;

  // Server IP strategy optimization
  QStringList m_serverStartIPs;
  QString m_serverStartSuggestedIP;
  QUrl m_helpPath;
};
