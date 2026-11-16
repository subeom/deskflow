/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerConfig.h"

#include "gui/config/MonitorEditing.h"

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

std::vector<MonitorRect> monitorsOf(
    const ::Computer &computer, const std::vector<MonitorRect> &display,
    const std::optional<deskflow::gui::ReportedMonitors> &reported
)
{
  // the screen of this computer is what the operating system says it is
  if (computer.isServer() && !display.empty() && MonitorLayout::create(display))
    return display;
  if (!computer.monitorLayout().isEmpty()) {
    if (const auto parsed = MonitorLayout::parse(computer.monitorLayout().toStdString()); parsed)
      return parsed.layout->monitors();
  }
  // what the user did not say, the computer did: it reported its monitors (experimental)
  if (reported && !computer.isServer())
    return reported->rects();
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

  // what the computers reported when they were last connected
  m_reportedMonitors.clear();
  for (const auto &computer : computers()) {
    if (computer.isNull() || computer.name().isEmpty())
      continue;
    if (const auto reported = deskflow::gui::ReportedMonitors::fromText(
            Settings::textValue(Settings::Computer::ReportedMonitors.arg(computer.name()))
        ))
      m_reportedMonitors.insert(computer.name().toLower(), *reported);
  }
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

void ServerConfig::setComputerSize(const QString &name, int width, int height)
{
  // a size that a layout cannot have is not stored, as a size or as a monitor
  if (width <= 0 || height <= 0 || !MonitorLayout::create({{0, 0, width, height}}))
    return;
  for (auto &computer : computers()) {
    if (computer.isNull() || computer.isServer() || computer.name().compare(name, Qt::CaseInsensitive) != 0)
      continue;
    if (!computer.monitorLayout().isEmpty()) {
      // a layout of one monitor is kept as one, where it was
      int x = 0;
      int y = 0;
      if (const auto parsed = MonitorLayout::parse(computer.monitorLayout().toStdString()); parsed) {
        x = parsed.layout->monitors().front().x;
        y = parsed.layout->monitors().front().y;
      }
      computer.setMonitorLayout(QStringLiteral("%1,%2,%3,%4").arg(x).arg(y).arg(width).arg(height));
      computer.setMonitorScales({});
    } else {
      computer.setArrangementSize(width, height);
    }
  }
}

void ServerConfig::setComputerZoom(const QString &name, double zoom)
{
  using deskflow::server::arrangement::kMaxZoom;
  using deskflow::server::arrangement::kMinZoom;
  if (!(zoom >= kMinZoom && zoom <= kMaxZoom))
    return;
  for (auto &computer : computers()) {
    if (computer.isNull() || computer.isServer() || computer.name().compare(name, Qt::CaseInsensitive) != 0)
      continue;
    // a monitor that is drawn at a scale is as big, in the drawing, as its pixels at that scale: with another zoom it
    // is another scale. Scales that would not be scales are dropped, and every monitor follows the zoom.
    const double ratio = computer.arrangementZoom() / zoom;
    QList<double> scales;
    bool fine = true;
    for (const auto scale : computer.monitorScales()) {
      const double scaled = scale * ratio;
      if (std::isfinite(scale) && (scaled < 25.0 || scaled > 500.0))
        fine = false;
      scales.append(std::isfinite(scale) ? scaled : scale);
    }
    computer.setArrangementZoom(zoom);
    computer.setMonitorScales(fine ? scales : QList<double>());
  }
}

QStringList ServerConfig::assumedSizes() const
{
  QStringList names;
  for (const auto &computer : computers()) {
    if (computer.isNull() || computer.isServer())
      continue;
    const bool hasMonitors = deskflow::gui::hasTypedMonitors(computer) || reportedMonitors(computer.name());
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
  if (const auto listener = m_reportListener.lock())
    (*listener)(computer->name());
  return true;
}

std::optional<QRect> ServerConfig::reportedScreen(const QString &computer) const
{
  const auto found = m_reportedScreens.constFind(computer.toLower());
  if (found == m_reportedScreens.cend())
    return std::nullopt;
  return *found;
}

bool ServerConfig::takeReportedMonitorsFromLog(const QString &line)
{
  // the core writes the line only while the experiment is on
  if (!Settings::value(Settings::Server::ExperimentalMultiMonitor).toBool())
    return false;
  const auto report = deskflow::gui::parseReportedMonitors(line);
  if (!report)
    return false;
  // and for a computer of the configuration
  auto &known = computers();
  const auto computer = std::ranges::find_if(known, [&](const ::Computer &c) {
    return !c.isNull() && c.name().compare(report->computer, Qt::CaseInsensitive) == 0;
  });
  if (computer == known.end())
    return false;

  // remembered, so that the computer is drawn from it when it is offline; one that has none takes it back
  const auto name = computer->name();
  const auto key = Settings::Computer::ReportedMonitors.arg(name);
  bool changed = true;
  if (report->monitors.isEmpty()) {
    // the core says it for every computer that does not report, each time that it connects: most have nothing to take
    // back
    changed = m_reportedMonitors.remove(name.toLower()) > 0;
    Settings::setValue(key);
  } else {
    // the first time that a computer reports, its zoom is set the way importing its monitors would: its physical pixels
    // are drawn at the scale of its display. After that the zoom is the user's, whatever it is.
    if (!m_reportedMonitors.contains(name.toLower()))
      adoptZoomOfFirstReport(*computer, report->monitors);
    m_reportedMonitors.insert(name.toLower(), report->monitors);
    Settings::setValue(key, report->monitors.toText());
  }
  if (changed) {
    if (const auto listener = m_reportListener.lock())
      (*listener)(name);
  }
  return true;
}

void ServerConfig::adoptZoomOfFirstReport(::Computer &computer, const deskflow::gui::ReportedMonitors &reported)
{
  // a layout of the user's, a zoom that was set, the server, and monitors in points (a Mac) are drawn as they are
  if (computer.isServer() || deskflow::gui::hasTypedMonitors(computer) || computer.arrangementZoom() != 1.0 ||
      reported.logicalUnits())
    return;
  const auto percent = reported.primaryScalePercent();
  if (percent <= 0 || percent == 100)
    return;
  const auto zoom =
      std::clamp(100.0 / percent, deskflow::server::arrangement::kMinZoom, deskflow::server::arrangement::kMaxZoom);
  computer.setArrangementZoom(zoom);
  Settings::setValue(Settings::Computer::ArrangementZoom.arg(computer.name()), zoom);
}

std::optional<deskflow::gui::ReportedMonitors> ServerConfig::reportedMonitors(const QString &computer) const
{
  const auto found = m_reportedMonitors.constFind(computer.toLower());
  if (found == m_reportedMonitors.cend())
    return std::nullopt;
  return *found;
}

QStringList ServerConfig::monitorsFromReport() const
{
  QStringList names;
  for (const auto &computer : computers()) {
    if (computer.isNull() || computer.isServer() || deskflow::gui::hasTypedMonitors(computer))
      continue;
    if (reportedMonitors(computer.name()))
      names << computer.name();
  }
  return names;
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
    const auto reported = reportedMonitors(computer.name());
    entry.zoom = computer.arrangementZoom();
    entry.monitors = monitorsOf(computer, display, reported);
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
