/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "AwdlModel.h"
#include "AwdlPermission.h"

#include <QObject>

namespace deskflow::awdl {

/*!
 * Everything that touches the system for the option that turns off the wireless link: reading the state of awdl0 and
 * the default route, the burst of probes to the gateway (all without privilege), and the one command that needs the
 * password of an administrator. The program gives the controller the real one (MacAwdlSystem); every test gives it a
 * fake, and none runs a program of the system, sends a packet or shows a dialog.
 *
 * Every call answers once, by a signal, from the event loop. A call while the same kind of call is under way is the
 * business of the caller (the controller never makes it).
 */
class IAwdlSystem : public QObject
{
  Q_OBJECT

public:
  using QObject::QObject;

  //! Reads the state of the interface; answers with stateRead()
  virtual void readState() = 0;
  //! Turns the interface down or up with the password of an administrator, asking for it with \p prompt in the dialog
  //! of the system; answers with linkSet()
  virtual void setLink(Direction direction, const QString &prompt) = 0;

  //! Looks up the default route (the interface that carries the traffic, its gateway, and what kind of connection it
  //! is); answers with routeFound()
  virtual void findRoute() = 0;
  //! A burst of probes to the gateway of \p route and to nothing else: it is only made for a route that canProbe() says
  //! yes to (anything else answers with no samples at once, and sends nothing); answers with measured()
  virtual void measure(const Route &route) = 0;

  //! What sudo says of the permission to change the link with no password (`sudo -n -ll`, which asks for nothing and
  //! changes nothing), and whether the file of the program is there; answers with permissionChecked()
  virtual void checkPermission() = 0;
  //! Turns the interface down or up with `sudo -n`: no dialog, no password; answers with silentRan(). Only for a
  //! permission that checkPermission() said is there (the controller never makes it otherwise)
  virtual void changeLinkSilently(Direction direction) = 0;
  //! Adds the file of the permission, or removes it, with the password of an administrator, asking for it with
  //! \p prompt in the dialog of the system; answer with permissionSet()
  virtual void installPermission(const QString &prompt) = 0;
  virtual void removePermission(const QString &prompt) = 0;

Q_SIGNALS:
  void stateRead(deskflow::awdl::LinkState state);
  void linkSet(deskflow::awdl::AdminResult result);
  void routeFound(deskflow::awdl::Route route);
  void measured(QList<deskflow::awdl::Sample> samples);
  void permissionChecked(deskflow::awdl::Listing listing, bool filePresent);
  void silentRan(deskflow::awdl::SudoResult result);
  void permissionSet(deskflow::awdl::AdminResult result);
};

} // namespace deskflow::awdl
