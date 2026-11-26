/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "IAwdlSystem.h"

#include <QPointer>
#include <QString>

#include <functional>

class QProcess;
class QTimer;

namespace deskflow::awdl {

/*!
 * The real system: `/sbin/ifconfig awdl0` for the state, and for the change `/usr/bin/osascript` with the dialog of the
 * system that asks for the password of an administrator (`do shell script ... with administrator privileges`). Deskflow
 * itself never runs as root and the program that runs with the password is `/sbin/ifconfig` with constant arguments
 * (adminArguments()). Both programs are started with a list of arguments, never a string for a shell.
 *
 * The state of the route and the latency are read with `/sbin/route -n get default` and `/sbin/ping`, which a person
 * may run without privilege (ping is not setuid on macOS: it uses a datagram socket, and an interval of 0.2 s is
 * allowed). A burst is only made for the route that canProbe() says yes to, and the only address in its arguments is
 * that gateway.
 *
 * The permission to change the link with no password (AwdlPermission.h) is read with `/usr/bin/sudo -n -ll`, which asks
 * for nothing and changes nothing, and used with `/usr/bin/sudo -n /sbin/ifconfig awdl0 down|up`; it is added and
 * removed with the same dialog of the administrator as the change itself. sudo is started with a list of arguments,
 * with no input, and never through a shell.
 *
 * The programs are named in the constructor so that a test of this class runs stand-ins for them; the program gives the
 * default, which is the system's own (kIfconfigPath, kOsascriptPath, kSudoPath).
 */
class MacAwdlSystem : public IAwdlSystem
{
  Q_OBJECT

public:
  struct Programs
  {
    QString ifconfig = QLatin1String(kIfconfigPath);
    QString osascript = QLatin1String(kOsascriptPath);
    QString route = QLatin1String(kRoutePath);
    QString ping = QLatin1String(kPingPath);
    QString sudo = QLatin1String(kSudoPath);
    //! The file of the permission: only whether it is there is read (it is for root alone to read)
    QString sudoersFile = QLatin1String(kSudoersFile);
    //! What kind of connection a device is (by default what Qt says of the interface of that name)
    std::function<LinkKind(const QString &)> kindOf;
    //! The name of the account that runs the program, for the line of the permission (by default what the OS says of
    //! the user ID of this process: it is never typed by anyone)
    std::function<QString()> accountName;
  };

  explicit MacAwdlSystem(QObject *parent = nullptr);
  MacAwdlSystem(const Programs &programs, QObject *parent = nullptr);
  ~MacAwdlSystem() override;

  void readState() override;
  void setLink(Direction direction, const QString &prompt) override;
  void findRoute() override;
  void measure(const Route &route) override;
  void checkPermission() override;
  void changeLinkSilently(Direction direction) override;
  void installPermission(const QString &prompt) override;
  void removePermission(const QString &prompt) override;

  //! What Qt says of the interface that has the name: Wi-Fi, a wire, or another kind
  static LinkKind kindOfInterface(const QString &name);
  //! The name of the account of this process, from the user ID (empty where there is none)
  static QString currentAccountName();

  //! How long the reading of the state may take before it is ended (the state is then not known)
  static constexpr int kReadTimeoutMs = 5000;

private:
  void runAdmin(const QStringList &arguments, bool permission);

  Programs m_programs;
  QPointer<QProcess> m_read;
  QPointer<QProcess> m_admin;
  QPointer<QProcess> m_listing;
  QPointer<QProcess> m_silent;
  QPointer<QProcess> m_route;
  QPointer<QProcess> m_ping;
};

} // namespace deskflow::awdl
