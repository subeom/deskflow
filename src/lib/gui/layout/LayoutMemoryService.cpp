/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/layout/LayoutMemoryService.h"

#include "common/Settings.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QGuiApplication>
#include <QScreen>

#include <algorithm>
#include <cmath>

namespace deskflow::gui::layout {

namespace {

LayoutMemoryService *&liveInstance()
{
  static LayoutMemoryService *instance = nullptr;
  return instance;
}

QString tr(const char *text)
{
  return QCoreApplication::translate("LayoutMemoryService", text);
}

QString defaultStorePath()
{
  return QFileInfo(Settings::settingsFile()).absolutePath() + QStringLiteral("/layout-profiles.json");
}

} // namespace

LayoutMemoryService::LayoutMemoryService(
    ServerConfig &config, std::unique_ptr<IDisplayProvider> displays, HostHooks hooks, const Options &options,
    QObject *parent
)
    : QObject(parent),
      m_clock(options.clock),
      m_useTimers(options.useTimers),
      m_store(options.storePath.isEmpty() ? defaultStorePath() : options.storePath),
      m_host(
          config, std::move(displays),
          [this](HostHooks hooks) {
            // the notices go through this object, so that the windows have one place to listen to
            auto toNotice = hooks.notify;
            hooks.notify = [this, toNotice](const Notice &notice) {
              onNotice(notice);
              if (toNotice)
                toNotice(notice);
            };
            return hooks;
          }(std::move(hooks))
      ),
      m_memory(m_host, m_store)
{
  m_elapsed.start();
  m_deadline.setSingleShot(true);
  connect(&m_deadline, &QTimer::timeout, this, &LayoutMemoryService::pump);
  connect(&m_poll, &QTimer::timeout, this, [this] {
    if (!m_running)
      return;
    m_memory.poll(now());
    sync();
    reschedule();
  });
  liveInstance() = this;
}

LayoutMemoryService::~LayoutMemoryService()
{
  if (liveInstance() == this)
    liveInstance() = nullptr;
}

LayoutMemoryService *LayoutMemoryService::instance()
{
  return liveInstance();
}

Millis LayoutMemoryService::now() const
{
  return m_clock ? m_clock() : static_cast<Millis>(m_elapsed.elapsed());
}

bool LayoutMemoryService::enabled() const
{
  return m_host.enabled();
}

void LayoutMemoryService::ensureStoreLoaded()
{
  if (m_loaded)
    return;
  m_loaded = true;
  const auto report = m_store.load();
  switch (report.status) {
  case LayoutProfileStore::Status::Damaged:
  case LayoutProfileStore::Status::Unreadable:
    m_host.warn(QStringLiteral("layout memory: %1").arg(report.message));
    break;
  case LayoutProfileStore::Status::Ok:
    if (report.dropped > 0)
      m_host.warn(QStringLiteral("layout memory: %1 (the file as it was is kept as %2)").arg(report.message, report.keptAs));
    break;
  default:
    break;
  }
}

void LayoutMemoryService::settingsChanged()
{
  const bool wanted = m_host.enabled();
  if (wanted && !m_running) {
    ensureStoreLoaded();
    m_running = true;
    m_memory.start(now());
    if (m_memory.state() != LayoutMemory::State::Disabled) {
      connectScreens();
      if (m_useTimers)
        m_poll.start(static_cast<int>(kPollMs));
    }
    Q_EMIT profilesChanged();
  } else if (!wanted && m_running) {
    m_memory.stop();
    disconnectScreens();
    m_poll.stop();
    m_deadline.stop();
    m_running = false;
    if (m_notice && m_notice->kind != Notice::Kind::StoreProblem) {
      m_notice.reset();
      Q_EMIT noticeChanged();
    }
  }
  sync();
  reschedule();
}

void LayoutMemoryService::configSaved()
{
  settingsChanged();
  if (!m_running)
    return;
  m_memory.configSaved(now());
  sync();
  reschedule();
  Q_EMIT profilesChanged();
}

void LayoutMemoryService::displaysMayHaveChanged()
{
  if (!m_running)
    return;
  m_memory.displaysEvent(now());
  reschedule();
}

void LayoutMemoryService::pump()
{
  if (!m_running)
    return;
  m_memory.tick(now());
  sync();
  reschedule();
}

void LayoutMemoryService::reschedule()
{
  if (!m_useTimers)
    return;
  const auto deadline = m_running ? m_memory.nextDeadline() : std::nullopt;
  if (!deadline) {
    m_deadline.stop();
    return;
  }
  m_deadline.start(static_cast<int>(std::clamp<Millis>(*deadline - now(), 0, 60 * 1000)));
}

void LayoutMemoryService::sync()
{
  // a notice that the memory does not keep any more (the user saved, or took the change back) is not shown any more
  if (m_notice && m_notice->kind != Notice::Kind::StoreProblem && !m_memory.lastNotice()) {
    m_notice.reset();
    Q_EMIT noticeChanged();
  }
}

void LayoutMemoryService::onNotice(const Notice &notice)
{
  m_notice = notice;
  Q_EMIT noticeChanged();
  Q_EMIT profilesChanged();
}

void LayoutMemoryService::dismissNotice()
{
  if (!m_notice)
    return;
  m_memory.dropUndo();
  m_notice.reset();
  Q_EMIT noticeChanged();
}

bool LayoutMemoryService::undo()
{
  if (!m_running)
    return false;
  const bool done = m_memory.undo(now());
  sync();
  reschedule();
  Q_EMIT profilesChanged();
  return done;
}

void LayoutMemoryService::undoTakenByDialog()
{
  m_memory.dropUndo();
  if (m_notice) {
    m_notice.reset();
    Q_EMIT noticeChanged();
  }
}

std::optional<Layout> LayoutMemoryService::undoLayout() const
{
  if (const auto *layout = m_memory.undoLayout())
    return *layout;
  return std::nullopt;
}

QString LayoutMemoryService::saveCurrentAs(const QString &name, const Layout &layout)
{
  if (!m_running)
    return {};
  const auto id = m_memory.saveCurrentAs(name, layout, now());
  if (!id.isEmpty())
    Q_EMIT profilesChanged();
  return id;
}

Layout LayoutMemoryService::currentLayout() const
{
  return m_host.currentLayout();
}

QString LayoutMemoryService::currentDisplays() const
{
  const auto signature = DisplaySignature::fromDisplays(const_cast<ServerConfigLayoutHost &>(m_host).displays());
  if (!signature.isValid())
    return tr("the displays cannot be read");
  return QStringLiteral("%1 (%2)").arg(signature.id(), signature.summary());
}

QString LayoutMemoryService::activeProfileId() const
{
  const auto active = m_store.active();
  if (!active)
    return {};
  const auto *profile = m_store.findExact(*active);
  return profile ? profile->id : QString();
}

void LayoutMemoryService::connectScreens()
{
  if (!m_screenConnections.isEmpty() || QGuiApplication::instance() == nullptr)
    return;

  const auto watch = [this](QScreen *screen) {
    if (screen == nullptr)
      return;
    m_screenConnections << connect(screen, &QScreen::geometryChanged, this, [this] { displaysMayHaveChanged(); });
    m_screenConnections << connect(screen, &QScreen::physicalDotsPerInchChanged, this, [this] { displaysMayHaveChanged(); });
    m_screenConnections << connect(screen, &QScreen::logicalDotsPerInchChanged, this, [this] { displaysMayHaveChanged(); });
  };
  for (auto *screen : QGuiApplication::screens())
    watch(screen);
  m_screenConnections << connect(qGuiApp, &QGuiApplication::screenAdded, this, [this, watch](QScreen *screen) {
    watch(screen);
    displaysMayHaveChanged();
  });
  m_screenConnections << connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this] { displaysMayHaveChanged(); });
  m_screenConnections << connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this, [this] { displaysMayHaveChanged(); });
}

void LayoutMemoryService::disconnectScreens()
{
  for (const auto &connection : std::as_const(m_screenConnections))
    QObject::disconnect(connection);
  m_screenConnections.clear();
}

QString LayoutMemoryService::noticeText(const Notice &notice)
{
  QString text;
  switch (notice.kind) {
  case Notice::Kind::Restored:
    if (notice.exact) {
      text = tr("Layout restored: \"%1\" (the same displays as before).").arg(notice.profileName);
    } else {
      text = tr("Layout restored: \"%1\" (similar displays, %2% alike).")
                 .arg(notice.profileName)
                 .arg(static_cast<int>(std::lround(notice.score * 100.0)));
    }
    if (!notice.notInProfile.isEmpty())
      text += QLatin1Char(' ') + tr("Not in that layout, left where they were: %1.").arg(notice.notInProfile.join(QStringLiteral(", ")));
    break;
  case Notice::Kind::Remembered:
    text = tr("No layout was remembered for these displays. The arrangement was kept as \"%1\".").arg(notice.profileName);
    if (!notice.reattached.isEmpty())
      text += QLatin1Char(' ') + tr("Put against the server again: %1.").arg(notice.reattached.join(QStringLiteral(", ")));
    if (!notice.notReattached.isEmpty())
      text += QLatin1Char(' ') + tr("Could not be put against the server: %1.").arg(notice.notReattached.join(QStringLiteral(", ")));
    break;
  case Notice::Kind::Undone:
    text = tr("The change was undone: the arrangement is as it was before.");
    break;
  case Notice::Kind::Failed:
    text = tr("A remembered layout could not be applied: %1. The arrangement was not changed.").arg(notice.detail);
    break;
  case Notice::Kind::StoreProblem:
    return notice.detail;
  }
  if (notice.restarted)
    text += QLatin1Char(' ') + tr("The core was restarted.");
  else if (notice.restartLater)
    text += QLatin1Char(' ') + tr("The core is restarted shortly (not more than once in 10 seconds).");
  return text;
}

} // namespace deskflow::gui::layout
