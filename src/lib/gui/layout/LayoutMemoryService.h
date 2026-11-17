/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/layout/LayoutMemory.h"
#include "gui/layout/ServerConfigLayoutHost.h"

#include <QElapsedTimer>
#include <QMetaObject>
#include <QObject>
#include <QTimer>

#include <functional>
#include <memory>
#include <optional>

namespace deskflow::gui::layout {

//! Remembers the arrangement of the computers for each set of displays of this computer, and puts it back (experimental)
/*!
The Qt side of \c LayoutMemory: it owns the store, the host and the memory, turns the screens that Qt says changed and a
slow poll into events, and keeps one single shot timer at the deadline that the memory asks for. While the feature is off
(the setting, the experiments, the mode) it has no timer running, no screen connected, no file open: it does nothing at
all. \c settingsChanged() says when to look again.
*/
class LayoutMemoryService : public QObject
{
  Q_OBJECT

public:
  struct Options
  {
    //! The file of the remembered layouts; empty: \c layout-profiles.json next to the settings
    QString storePath;
    //! false: no timer is made, and the caller calls \c pump() and the events (tests)
    bool useTimers = true;
    //! Milliseconds of a clock that only goes forward; empty: the time since this object was made
    std::function<Millis()> clock;
  };

  LayoutMemoryService(
      ServerConfig &config, std::unique_ptr<IDisplayProvider> displays, HostHooks hooks, const Options &options,
      QObject *parent = nullptr
  );
  ~LayoutMemoryService() override;

  //! The service of the program, if there is one; the arrangement panel asks it for the notice and the list
  static LayoutMemoryService *instance();

  //! The settings and the mode ask for the feature
  bool enabled() const;
  //! The switch, an experiment, the mode or the use of an external file changed: starts or stops
  void settingsChanged();
  //! The user saved the configuration (a dialog that was accepted)
  void configSaved();
  //! Qt says that a screen came, went or changed
  void displaysMayHaveChanged();
  //! Time passed: looks if it is time (what the timers call)
  void pump();

  //! The notice that is shown, if there is one
  const std::optional<Notice> &notice() const
  {
    return m_notice;
  }
  //! The words for a notice, in the language of the program
  static QString noticeText(const Notice &notice);
  //! Hides the notice
  void dismissNotice();
  //! Puts the arrangement back as it was before the last change; false if there is nothing to put back now
  bool undo();
  //! A dialog that has a copy of the arrangement took the undo into its copy: it is not done twice
  void undoTakenByDialog();
  //! Where the computers were before the last change
  std::optional<Layout> undoLayout() const;

  //! \p layout (what a dialog shows) is remembered as the layout of the situation that is, under \p name
  QString saveCurrentAs(const QString &name, const Layout &layout);
  //! Gives a remembered layout a name of the user's; false for an empty name or a profile that is not there
  bool renameProfile(const QString &id, const QString &name);
  //! Forgets a remembered layout
  bool removeProfile(const QString &id);
  //! The feature runs (it is asked for by the settings, and the file of the layouts can be used)
  bool isRunning() const
  {
    return m_running;
  }
  //! Where the computers are now
  Layout currentLayout() const;
  //! The displays now as the memory sees them, for the diagnostics: \c "ba4a8180 (2 displays: ...)"
  QString currentDisplays() const;

  LayoutProfileStore &store()
  {
    return m_store;
  }
  const LayoutProfileStore &store() const
  {
    return m_store;
  }
  LayoutMemory &memory()
  {
    return m_memory;
  }
  //! The id of the profile that the arrangement belongs to now, or empty
  QString activeProfileId() const;

Q_SIGNALS:
  //! The notice that is shown changed (a new one, or it is gone)
  void noticeChanged();
  //! The remembered layouts changed (the list in a dialog is to be read again)
  void profilesChanged();

private:
  void ensureStoreLoaded();
  void connectScreens();
  void disconnectScreens();
  void sync();
  void reschedule();
  Millis now() const;
  void onNotice(const Notice &notice);

  std::function<Millis()> m_clock;
  QElapsedTimer m_elapsed;
  bool m_useTimers = true;
  LayoutProfileStore m_store;
  ServerConfigLayoutHost m_host;
  LayoutMemory m_memory;
  bool m_loaded = false;
  bool m_running = false;
  std::optional<Notice> m_notice;
  QTimer m_deadline;
  QTimer m_poll;
  QList<QMetaObject::Connection> m_screenConnections;
};

} // namespace deskflow::gui::layout
