/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerConfig.h"

#include "common/Hotkey.h"
#include "common/Settings.h"
#include "gui/core/ReportedScreen.h"

#include <QAbstractButton>
#include <QGuiApplication>
#include <QPushButton>
#include <QScreen>

#ifdef Q_OS_MACOS
#include "platform/OSXDisplays.h"
#endif

#include <algorithm>

using enum ComputerConfig::SwitchCorner;
using enum ComputerConfig::Fix;

static const struct
{
  int x;
  int y;
  const char *name;
} neighbourDirs[] = {
    {1, 0, "right"},
    {-1, 0, "left"},
    {0, -1, "up"},
    {0, 1, "down"},

};

const int serverDefaultIndex = 7;

namespace {

using namespace deskflow::server;

// the size of a computer that has no monitors and no size that was said
constexpr int kDefaultWidth = 1920;
constexpr int kDefaultHeight = 1080;
// what is between a computer that is put aside and the others
constexpr double kGapOfNewComputers = 200.0;

std::optional<std::vector<MonitorRect>> &monitorsOverride()
{
  static std::optional<std::vector<MonitorRect>> override;
  return override;
}

std::vector<MonitorRect> monitorsOf(const ::Computer &computer, const std::vector<MonitorRect> &display)
{
  // the screen of this computer is what the operating system says it is
  if (computer.isServer() && !display.empty() && MonitorLayout::create(display))
    return display;
  if (!computer.monitorLayout().isEmpty()) {
    if (const auto parsed = MonitorLayout::parse(computer.monitorLayout().toStdString()); parsed)
      return parsed.layout->monitors();
  }
  return {
      {0, 0, computer.arrangementWidth() > 0 ? computer.arrangementWidth() : kDefaultWidth,
       computer.arrangementHeight() > 0 ? computer.arrangementHeight() : kDefaultHeight}
  };
}

} // namespace

ServerConfig::ServerConfig(int columns, int rows) : m_computers(columns), m_columns(columns), m_rows(rows)
{
  recall();
}

bool ServerConfig::save(const QString &fileName) const
{
  QFile file(fileName);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    return false;

  save(file);
  file.close();

  return true;
}

bool ServerConfig::operator==(const ServerConfig &sc) const
{
  return m_computers == sc.m_computers && //
         m_Hotkeys == sc.m_Hotkeys;       //
}

void ServerConfig::save(QFile &file) const
{
  QTextStream outStream(&file);
  outStream << *this;
}

void ServerConfig::setupComputers()
{
  computers().clear();
  hotkeys().clear();

  // There must always be computer objects for each cell in the computers QList.
  // Unused computers are identified by having an empty name.
  for (int i = 0; i < m_columns * m_rows; i++)
    addComputer(Computer());
}

void ServerConfig::commit()
{
  qDebug("committing server config");

  settings().beginGroup("internalConfig");
  settings().remove("");

  settings().beginWriteArray("screens");
  for (int i = 0; i < computers().size(); i++) {
    settings().setArrayIndex(i);
    const auto &computer = computers()[i];
    computer.saveSettings(settings());
    auto computerName = Settings::value(Settings::Core::ComputerName).toString();
    if (computer.isServer() && computerName != computer.name()) {
      Settings::setValue(Settings::Core::ComputerName, computer.name());
    }
  }
  settings().endArray();

  settings().beginWriteArray("hotkeys");
  for (int i = 0; i < hotkeys().size(); i++) {
    settings().setArrayIndex(i);
    hotkeys()[i].saveSettings(settings().get());
  }
  settings().endArray();

  settings().endGroup();
}

void ServerConfig::recall()
{
  qDebug("recalling server config");

  settings().beginGroup("internalConfig");

  m_columns = Settings::value(Settings::Server::GridWidth).toInt();
  m_rows = Settings::value(Settings::Server::GridHeight).toInt();

  // we need to know the number of columns and rows before we can set up
  // ourselves
  setupComputers();

  int numComputers = settings().beginReadArray("screens");
  Q_ASSERT(numComputers <= computers().size());
  for (int i = 0; i < numComputers; i++) {
    settings().setArrayIndex(i);
    computers()[i].loadSettings(settings());
    if (getServerName() == computers()[i].name()) {
      computers()[i].markAsServer();
    }
  }
  settings().endArray();

  int numHotkeys = settings().beginReadArray("hotkeys");
  for (int i = 0; i < numHotkeys; i++) {
    settings().setArrayIndex(i);
    Hotkey h;
    h.loadSettings(settings().get());
    hotkeys().append(h);
  }
  settings().endArray();

  settings().endGroup();
}

int ServerConfig::adjacentComputerIndex(int idx, int deltaColumn, int deltaRow) const
{
  if (computers()[idx].isNull())
    return -1;

  // if we're at the left or right end of the table, don't find results going
  // further left or right
  if ((deltaColumn > 0 && (idx + 1) % m_columns == 0) || (deltaColumn < 0 && idx % m_columns == 0))
    return -1;

  int arrayPos = idx + deltaColumn + deltaRow * m_columns;

  if (arrayPos >= computers().size() || arrayPos < 0)
    return -1;

  return arrayPos;
}

QTextStream &operator<<(QTextStream &outStream, const ServerConfig &config)
{
  // Experimental: the links are where the exposed edges of the computers in the arrangement touch
  if (config.usesArrangement()) {
    const auto arrangement = config.arrangement();
    outStream << QString::fromStdString(
        deskflow::server::arrangement::formatLinks(arrangement, deskflow::server::arrangement::deriveLinks(arrangement))
    );
    outStream << "section: options" << Qt::endl;
    for (const Hotkey &hotkey : config.hotkeys())
      outStream << hotkey;
    outStream << "end" << Qt::endl << Qt::endl;
    return outStream;
  }

  outStream << "section: links" << Qt::endl;

  for (int i = 0; const auto &computer : config.computers()) {
    if (!computer.isNull()) {
      outStream << "\t" << computer.name() << ":\n";
      for (const auto &neighbour : std::as_const(neighbourDirs)) {
        int idx = config.adjacentComputerIndex(i, neighbour.x, neighbour.y);
        if (idx != -1 && !config.computers()[idx].isNull())
          outStream << "\t\t" << neighbour.name << " = " << config.computers()[idx].name() << Qt::endl;
      }
    }
    i++;
  }

  outStream << "end" << Qt::endl << Qt::endl;

  outStream << "section: options" << Qt::endl;

  for (const Hotkey &hotkey : config.hotkeys())
    outStream << hotkey;

  outStream << "end" << Qt::endl << Qt::endl;

  return outStream;
}

int ServerConfig::numComputers() const
{
  int rval = 0;

  for (const Computer &s : computers()) {
    if (!s.isNull())
      rval++;
  }

  return rval;
}

QString ServerConfig::getServerName() const
{
  return Settings::value(Settings::Core::ComputerName).toString();
}

void ServerConfig::updateServerName()
{
  for (auto &computer : computers()) {
    if (computer.isServer()) {
      computer.setName(Settings::value(Settings::Core::ComputerName).toString());
      break;
    }
  }
}

QString ServerConfig::configFile() const
{
  return Settings::value(Settings::Server::ExternalConfigFile).toString();
}

bool ServerConfig::useExternalConfig() const
{
  return Settings::value(Settings::Server::ExternalConfig).toBool();
}

bool ServerConfig::isFull() const
{
  bool isFull = true;

  for (const auto &computer : computers()) {
    if (computer.isNull()) {
      isFull = false;
      break;
    }
  }

  return isFull;
}

bool ServerConfig::computerExists(const QString &computerName) const
{
  bool isExists = false;

  for (const auto &computer : computers()) {
    if (!computer.isNull() && computer.name() == computerName) {
      isExists = true;
      break;
    }
  }

  return isExists;
}

void ServerConfig::addClient(const QString &clientName)
{
  int serverIndex = -1;
  const auto computerName = Settings::value(Settings::Core::ComputerName).toString();

  if (findComputerName(computerName, serverIndex)) {
    m_computers[serverIndex].markAsServer();
  } else {
    fixNoServer(computerName, serverIndex);
  }

  m_computers.addComputerByPriority(Computer(clientName));
}

void ServerConfig::setConfigFile(const QString &configFile) const
{
  Settings::setValue(Settings::Server::ExternalConfigFile, configFile);
}

void ServerConfig::setUseExternalConfig(bool useExternalConfig) const
{
  Settings::setValue(Settings::Server::ExternalConfig, useExternalConfig);
}

bool ServerConfig::findComputerName(const QString &name, int &index)
{
  bool found = false;
  for (int i = 0; i < computers().size(); i++) {
    if (!computers()[i].isNull() && computers()[i].name().compare(name) == 0) {
      index = i;
      found = true;
      break;
    }
  }
  return found;
}

bool ServerConfig::fixNoServer(const QString &name, int &index)
{
  bool fixed = false;
  if (computers()[serverDefaultIndex].isNull()) {
    m_computers[serverDefaultIndex].setName(name);
    m_computers[serverDefaultIndex].markAsServer();
    index = serverDefaultIndex;
    fixed = true;
  }

  return fixed;
}

QSettingsProxy &ServerConfig::settings()
{
  return Settings::proxy();
}

//
// The arrangement (experimental)
//

bool ServerConfig::usesArrangement() const
{
  return Settings::value(Settings::Server::ExperimentalMultiMonitor).toBool() &&
         Settings::value(Settings::Server::UseArrangement).toBool();
}

bool ServerConfig::hasArrangement() const
{
  return std::ranges::any_of(computers(), [](const Computer &c) { return !c.isNull() && c.arrangementPlaced(); });
}

std::vector<MonitorRect> ServerConfig::serverMonitors()
{
  if (monitorsOverride())
    return *monitorsOverride();
#ifdef Q_OS_MACOS
  return deskflow::platform::osxActiveDisplays();
#else
  // The operating system is asked for the size of the screen only: the monitors of this computer are not known here
  if (const auto *screen = QGuiApplication::primaryScreen()) {
    const auto size = screen->virtualSize() * screen->devicePixelRatio();
    if (size.width() >= 1 && size.height() >= 1)
      return {{0, 0, size.width(), size.height()}};
  }
  return {};
#endif
}

void ServerConfig::setReportedScreen(const QString &computer, const QRect &shape)
{
  m_reportedScreens.insert(computer.toLower(), shape);
}

QStringList ServerConfig::assumedSizes() const
{
  QStringList names;
  for (const auto &computer : computers()) {
    if (computer.isNull() || computer.isServer())
      continue;
    const bool hasMonitors =
        !computer.monitorLayout().isEmpty() && MonitorLayout::parse(computer.monitorLayout().toStdString());
    if (!hasMonitors && (computer.arrangementWidth() <= 0 || computer.arrangementHeight() <= 0))
      names << computer.name();
  }
  return names;
}

bool ServerConfig::takeReportedScreenFromLog(const QString &line)
{
  // the core writes the line only while the experiment is on
  if (!Settings::value(Settings::Server::ExperimentalMultiMonitor).toBool())
    return false;
  const auto screen = deskflow::gui::parseReportedScreen(line);
  if (!screen)
    return false;
  // and for a computer that has connected, which is one of the configuration
  const auto &known = std::as_const(*this).computers();
  const auto computer = std::ranges::find_if(known, [&](const ::Computer &c) {
    return !c.isNull() && c.name().compare(screen->computer, Qt::CaseInsensitive) == 0;
  });
  if (computer == known.cend())
    return false;
  setReportedScreen(screen->computer, screen->shape);
  return true;
}

std::optional<QRect> ServerConfig::reportedScreen(const QString &computer) const
{
  const auto found = m_reportedScreens.constFind(computer.toLower());
  if (found == m_reportedScreens.cend())
    return std::nullopt;
  return *found;
}

void ServerConfig::adoptReportedScreens()
{
  for (auto &computer : computers()) {
    if (computer.isNull() || computer.isServer() || !computer.monitorLayout().isEmpty() ||
        computer.arrangementWidth() > 0 || computer.arrangementHeight() > 0)
      continue;
    const auto reported = reportedScreen(computer.name());
    if (reported && reported->x() == 0 && reported->y() == 0)
      computer.setArrangementSize(reported->width(), reported->height());
  }
}

void ServerConfig::overrideServerMonitors(const std::optional<std::vector<MonitorRect>> &monitors)
{
  monitorsOverride() = monitors;
}

std::vector<arrangement::Computer> ServerConfig::arrangementComputers() const
{
  const auto display = serverMonitors();
  std::vector<arrangement::Computer> result;
  for (const auto &computer : computers()) {
    if (computer.isNull())
      continue;
    arrangement::Computer entry;
    entry.name = computer.name().toStdString();
    entry.isServer = computer.isServer();
    entry.zoom = computer.arrangementZoom();
    entry.monitors = monitorsOf(computer, display);
    entry.originX = computer.arrangementX();
    entry.originY = computer.arrangementY();
    result.push_back(entry);
  }
  return result;
}

arrangement::Arrangement ServerConfig::arrangement() const
{
  if (!hasArrangement())
    return classicGridAsArrangement().arrangement;

  arrangement::Arrangement result;
  result.computers = arrangementComputers();
  std::vector<size_t> unplaced;
  size_t index = 0;
  arrangement::Rect bounds;
  bool anyPlaced = false;
  for (const auto &computer : computers()) {
    if (computer.isNull())
      continue;
    if (!computer.arrangementPlaced()) {
      unplaced.push_back(index);
    } else {
      const auto box = result.computers[index].box();
      bounds = anyPlaced ? arrangement::Rect{std::min(bounds.x, box.x), std::min(bounds.y, box.y),
                                std::max(bounds.right(), box.right()) - std::min(bounds.x, box.x),
                                std::max(bounds.bottom(), box.bottom()) - std::min(bounds.y, box.y)}
                         : box;
      anyPlaced = true;
    }
    ++index;
  }

  // a computer that has not been put anywhere is beside the others, so that it touches none of them
  for (const auto at : unplaced) {
    auto &computer = result.computers[at];
    const auto shape = computer.shape();
    computer.originX = bounds.right() + kGapOfNewComputers - computer.zoom * shape.x;
    computer.originY = bounds.y - computer.zoom * shape.y;
    const auto box = computer.box();
    bounds.w = box.right() - bounds.x;
    bounds.h = std::max(bounds.bottom(), box.bottom()) - bounds.y;
  }
  return result;
}

arrangement::GridConversion ServerConfig::classicGridAsArrangement() const
{
  const auto entries = arrangementComputers();
  std::vector<arrangement::GridCell> cells;
  size_t entry = 0;
  std::string server;
  for (int i = 0; i < computers().size(); ++i) {
    if (computers()[i].isNull())
      continue;
    arrangement::GridCell cell;
    cell.column = i % m_columns;
    cell.row = i / m_columns;
    cell.computer = entries[entry++];
    if (cell.computer.isServer)
      server = cell.computer.name;
    cells.push_back(cell);
  }
  return arrangement::arrangementFromGrid(cells, server);
}

void ServerConfig::setArrangement(const arrangement::Arrangement &arrangement)
{
  for (const auto &placed : arrangement.computers) {
    for (auto &computer : computers()) {
      if (!computer.isNull() && computer.name().compare(QString::fromStdString(placed.name), Qt::CaseInsensitive) == 0)
        computer.setArrangementOrigin(placed.originX, placed.originY);
    }
  }
}
