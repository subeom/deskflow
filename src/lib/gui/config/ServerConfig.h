/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/Constants.h"
#include "common/Hotkey.h"
#include "gui/config/ComputerConfig.h"
#include "gui/config/ComputerList.h"
#include "server/Arrangement.h"

#include <QHash>
#include <QList>
#include <QRect>

#include <optional>
#include <vector>

class QTextStream;
class QSettings;
class QString;
class QFile;
class ServerConfigDialog;

class ServerConfig : public ComputerConfig
{
  friend class ServerConfigDialog;
  friend QTextStream &operator<<(QTextStream &outStream, const ServerConfig &config);

public:
  explicit ServerConfig(int columns = kServerGridWidth, int rows = kServerGridHeight);
  ~ServerConfig() = default;

  bool operator==(const ServerConfig &sc) const;

  const ComputerList &computers() const
  {
    return m_computers;
  }

  //
  // New methods
  //
  const HotkeyList &hotkeys() const
  {
    return m_Hotkeys;
  }

  bool save(const QString &fileName) const;
  bool computerExists(const QString &computerName) const;
  void save(QFile &file) const;
  bool isFull() const;
  void commit();
  int numComputers() const;
  QString getServerName() const;
  void updateServerName();
  QString configFile() const;
  bool useExternalConfig() const;
  void addClient(const QString &clientName);

  //! @name Experimental: the arrangement of the computers
  //@{

  //! Whether the links are made from the arrangement: the experiment is on, and so is the use of the arrangement
  bool usesArrangement() const;

  //! Whether the computers have been put in the arrangement
  bool hasArrangement() const;

  //! The computers of the grid, with their monitors, zoom and place in the arrangement
  /*!
  A server has the displays of this computer, a computer that has a monitor layout has those monitors, a computer that
  has neither is one monitor of the size that was said (1920 x 1080 if none was). A computer that has no place is put
  beside the others, apart from them; if none has one, this is the classic grid as an arrangement.
  */
  deskflow::server::arrangement::Arrangement arrangement() const;

  //! The classic grid as an arrangement, which is where the arrangement begins
  deskflow::server::arrangement::GridConversion classicGridAsArrangement() const;

  //! Take the places of the computers from \p arrangement
  void setArrangement(const deskflow::server::arrangement::Arrangement &arrangement);

  //! What a computer that is connected said its screen is: its position and size in its own coordinates
  /*!
  Only known while the server runs and the computer is connected, and it is not saved. It is what a computer that has no
  monitors and no size that was said is drawn as.
  */
  void setReportedScreen(const QString &computer, const QRect &shape);
  //! The one rectangle of a computer, drawn on the canvas, is \p width x \p height in the units of the computer: its
  //! monitor if it has a layout of one, else its size
  void setComputerSize(const QString &computer, int width, int height);
  //! The computers that the arrangement has to draw with a size that it assumed: they have no monitors and no size
  QStringList assumedSizes() const;
  //! Takes what a line of the log of the core says about the screen of a computer, if it does
  bool takeReportedScreenFromLog(const QString &line);
  std::optional<QRect> reportedScreen(const QString &computer) const;
  const QHash<QString, QRect> &reportedScreens() const
  {
    return m_reportedScreens;
  }
  void setReportedScreens(const QHash<QString, QRect> &screens)
  {
    m_reportedScreens = screens;
  }

  //! Make the screen that a computer reported its size, when it has no monitors and no size that was said
  /*!
  A size is only taken from a report where the screen begins at 0,0: another one needs its monitors said.
  */
  void adoptReportedScreens();

  //! The displays of this computer, as the operating system says: nothing if it does not
  static std::vector<deskflow::server::MonitorRect> serverMonitors();

  //! Say what the displays of this computer are, or that the operating system is asked again; for the tests
  static void overrideServerMonitors(const std::optional<std::vector<deskflow::server::MonitorRect>> &monitors);

  //@}

private:
  void recall();
  void setupComputers();
  QSettingsProxy &settings();
  ComputerList &computers()
  {
    return m_computers;
  }
  void setComputers(const ComputerList &computers)
  {
    m_computers = computers;
  }
  void addComputer(const Computer &computer)
  {
    m_computers.append(computer);
  }
  void setConfigFile(const QString &configFile) const;
  void setUseExternalConfig(bool useExternalConfig) const;
  HotkeyList &hotkeys()
  {
    return m_Hotkeys;
  }
  int adjacentComputerIndex(int idx, int deltaColumn, int deltaRow) const;
  std::vector<deskflow::server::arrangement::Computer> arrangementComputers() const;
  bool findComputerName(const QString &name, int &index);
  bool fixNoServer(const QString &name, int &index);

private:
  HotkeyList m_Hotkeys;

  ComputerList m_computers;
  // by the name of the computer in lower case
  QHash<QString, QRect> m_reportedScreens;
  int m_columns;
  int m_rows;
};

QTextStream &operator<<(QTextStream &outStream, const ServerConfig &config);
