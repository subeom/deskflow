/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/awdl/IAwdlSystem.h"

#include <QList>
#include <QPair>
#include <QStringList>

// The system of a test: it records what it is asked and answers when the test says so. No program is run, no dialog is
// shown, and awdl0 is not touched, whatever a test does with it.
class FakeAwdlSystem : public deskflow::awdl::IAwdlSystem
{
public:
  using IAwdlSystem::IAwdlSystem;

  void readState() override
  {
    ++reads;
  }

  void setLink(deskflow::awdl::Direction direction, const QString &prompt) override
  {
    changes.append({direction, prompt});
  }

  void findRoute() override
  {
    ++routeLookups;
  }

  void measure(const deskflow::awdl::Route &route) override
  {
    measures.append(route);
  }

  void checkPermission() override
  {
    ++permissionChecks;
    // by default sudo answers at once, and the answer is that a password is needed (nothing is allowed): the tests that
    // are about the dialog need no more. A test that is about the permission sets what is said, or holds the answer
    // back
    if (answerPermissionAtOnce)
      Q_EMIT permissionChecked(listing, filePresent);
  }

  void changeLinkSilently(deskflow::awdl::Direction direction) override
  {
    silentChanges.append(direction);
  }

  void installPermission(const QString &prompt) override
  {
    installs.append(prompt);
  }

  void removePermission(const QString &prompt) override
  {
    removals.append(prompt);
  }

  //! What a test answers
  void answerRead(deskflow::awdl::LinkState state)
  {
    Q_EMIT stateRead(state);
  }
  void answerLink(deskflow::awdl::AdminResult result)
  {
    Q_EMIT linkSet(result);
  }

  void answerRoute(const deskflow::awdl::Route &route)
  {
    Q_EMIT routeFound(route);
  }
  void answerMeasured(const QList<deskflow::awdl::Sample> &samples)
  {
    Q_EMIT measured(samples);
  }
  void answerPermission(deskflow::awdl::Listing said, bool present)
  {
    Q_EMIT permissionChecked(said, present);
  }
  void answerSilent(deskflow::awdl::SudoResult result)
  {
    Q_EMIT silentRan(result);
  }
  void answerPermissionSet(deskflow::awdl::AdminResult result)
  {
    Q_EMIT permissionSet(result);
  }

  int reads = 0;
  int routeLookups = 0;
  int permissionChecks = 0;
  //! What sudo says when it is asked (and whether the file of the program is there), and whether it says it at once
  deskflow::awdl::Listing listing = deskflow::awdl::Listing::PasswordNeeded;
  bool filePresent = false;
  bool answerPermissionAtOnce = true;
  //! The changes that were made with sudo -n (no dialog), and the sentences of the dialogs that add and remove the file
  QList<deskflow::awdl::Direction> silentChanges;
  QStringList installs;
  QStringList removals;
  //! The routes that were measured: a burst is made for the route that was found, and for nothing else
  QList<deskflow::awdl::Route> measures;
  QList<QPair<deskflow::awdl::Direction, QString>> changes;

  int downs() const
  {
    int count = 0;
    for (const auto &change : changes)
      count += change.first == deskflow::awdl::Direction::Down ? 1 : 0;
    return count;
  }
  int ups() const
  {
    return changes.size() - downs();
  }
};
