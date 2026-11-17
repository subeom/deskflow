/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QList>
#include <QObject>
#include <QString>

class NavRail;
class QComboBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QStackedWidget;
class QWidget;
class SegmentedControl;
class SettingCard;
class SidebarShell;
class StatusHero;

namespace deskflow::gui {

class ClientConnectionSection;
class StatusModel;

namespace style {
class BackdropWidget;
}

//! The widgets of the main window that the new shell takes over (it moves them, it does not make them again)
struct MainShellParts
{
  //! The central widget of the window: the shell is put in it
  QWidget *central = nullptr;
  //! What was in the central widget and is not used by the shell (it is hidden)
  QWidget *oldGroup = nullptr;
  //! The row with the name of this computer
  QWidget *nameRow = nullptr;
  QLabel *ipLabel = nullptr;
  QRadioButton *modeServer = nullptr;
  QRadioButton *modeClient = nullptr;
  QLabel *noMode = nullptr;
  QPushButton *configureServer = nullptr;
  QPushButton *exportServer = nullptr;
  QPushButton *configureClient = nullptr;
  QPushButton *testConnection = nullptr;
  QPushButton *toggleCore = nullptr;
  QPushButton *restartCore = nullptr;
  QLabel *hostLabel = nullptr;
  QComboBox *hostname = nullptr;
  //! The line of the layout memory, above everything
  QWidget *noticeBar = nullptr;
};

/*!
 * \brief The main window of the new look: a sidebar and, beside it, a page with the state first (the status hero), the
 * mode, and what belongs to the mode (the computers of a server, the connection of a client).
 *
 * The window keeps all its logic: the controls are the ones that it has, moved here, and it tells the shell what to
 * show (the mode, the computers, the state of the core through status()). The shell tells it only what the user pressed
 * in the sidebar (rail()->activated()).
 *
 * Where later work puts things:
 *  - another page of the window: addPage(), which makes an entry of the sidebar and a page of the stack
 *  - what the state says: status() takes chips from any source (StatusModel::setExternalChip), the window does not have
 *    to know the source
 *  - the connection of a client: clientSection() has the places for a line that replaces the address and for lines
 *    below it (ClientConnectionSection)
 */
class MainShell : public QObject
{
  Q_OBJECT

public:
  enum class Mode
  {
    None,
    Server,
    Client
  };

  //! What the shell writes that the main window already has in the language of the program
  struct Texts
  {
    QString preferences;
    QString log;
    QString about;
    QString fingerprint;
    QString updateAvailable;
  };

  struct ComputerEntry
  {
    QString name;
    QString detail;
    bool connected = false;
  };

  static constexpr int kMinimumWindowWidth = 720;

  MainShell(const MainShellParts &parts, QObject *parent = nullptr);

  QWidget *widget() const;
  SidebarShell *sidebar() const;
  NavRail *rail() const;
  StatusModel *status() const;
  StatusHero *hero() const;
  SegmentedControl *modeControl() const;
  ClientConnectionSection *clientSection() const;
  SettingCard *computersCard() const;
  QStackedWidget *pages() const;
  QLabel *pageTitle() const;

  //! The texts of the main window (taken from its actions and buttons); the texts of the shell itself come with them
  void setTexts(const Texts &texts);
  void retranslate();

  void setMode(Mode mode);
  Mode mode() const;
  void setComputers(const QList<ComputerEntry> &computers);

  //! Whether the encryption is switched on in the settings, for the line at the foot of the sidebar
  void setEncryptionEnabled(bool enabled);
  void setFingerprintVisible(bool visible);
  //! The version that can be updated to; none when it is empty
  void setUpdateVersion(const QString &version);
  void setLogShown(bool shown);

  //! The page of the stack that an entry of the sidebar shows (a page that is not the home page: later work)
  void addPage(const QString &id, const QString &iconName, const QString &text, QWidget *page);

private:
  void updateModeTexts();
  void applyStatus();
  void updateComputersPlaceholder();

  MainShellParts m_parts;
  Texts m_texts;
  Mode m_mode = Mode::None;
  QList<ComputerEntry> m_computers;
  bool m_encryption = true;
  QString m_updateVersion;

  deskflow::gui::style::BackdropWidget *m_backdrop = nullptr;
  SidebarShell *m_sidebar = nullptr;
  StatusModel *m_status = nullptr;
  StatusHero *m_hero = nullptr;
  SegmentedControl *m_modeControl = nullptr;
  QLabel *m_title = nullptr;
  QStackedWidget *m_pages = nullptr;
  QStackedWidget *m_modePages = nullptr;
  SettingCard *m_computersCard = nullptr;
  SettingCard *m_noModeCard = nullptr;
  QWidget *m_noModePage = nullptr;
  QWidget *m_serverPage = nullptr;
  ClientConnectionSection *m_clientSection = nullptr;
};

} // namespace deskflow::gui
