/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace deskflow::awdl {

/*!
 * \file
 * The wireless link of AirDrop, Handoff, Universal Control and AirPlay (AWDL, the interface awdl0 of macOS) can make
 * the mouse stutter while Deskflow shares it through Wi-Fi: it hops channels every few seconds and the traffic of
 * Deskflow waits for it. Measured on a Mac: with awdl0 up and AWDL not in use the latency to the router is clean (3.8
 * ms on average, 6.7 ms at the worst), and while it is in use it spikes every 3.6 s (9.2 ms on average, 139 ms at the
 * worst). So the link being up is no problem by itself: the program only tells of a problem when the latency to the
 * gateway shows the pattern. This is what the option "turn off the wireless link while sharing" knows, as plain values,
 * so that every rule can be tested without a Mac: reading the state of the interface, the one privileged command, the
 * measuring of the latency (what is probed, and what the samples say), and when the program asks and tells. Nothing
 * here touches the system (see IAwdlSystem for what does).
 */

//! The state of awdl0
enum class LinkState
{
  //! Not read yet, or the output could not be understood
  Unknown,
  //! There is no such interface (the Mac has no AWDL, or the system took it away)
  Unavailable,
  Up,
  Down
};

//! The one command that is run with the password of an administrator: the interface goes down or up
enum class Direction
{
  Down,
  Up
};

//! What happened to the command
enum class AdminResult
{
  Done,
  //! The person closed the dialog of the password
  Cancelled,
  //! The password was wrong, the system said no, or the command could not be started
  Failed,
  //! Nothing was started: the program would not run the command (the name of the account cannot be used in the file of
  //! the permission, see AwdlPermission.h)
  Refused
};

inline constexpr auto kInterface = "awdl0";
inline constexpr auto kIfconfigPath = "/sbin/ifconfig";
inline constexpr auto kOsascriptPath = "/usr/bin/osascript";
inline constexpr auto kRoutePath = "/sbin/route";
inline constexpr auto kPingPath = "/sbin/ping";

//! The state from what `ifconfig awdl0` said: its exit code and what it wrote. UP in its flags is Up, a flag list
//! without it is Down, "interface awdl0 does not exist" is Unavailable; the word "status:" is the second source when
//! there are no flags
LinkState parseIfconfig(int exitCode, const QString &standardOutput, const QString &standardError = {});

//! A stable word for the state ("up", "down", "unavailable", "unknown"), for the log and for the tests
QString linkStateId(LinkState state);

//! The result of `osascript` from its exit code and what it wrote on the error output; \p started is false when the
//! program could not be started at all
AdminResult parseAdminResult(bool started, int exitCode, const QString &standardError);

//! \p text as a string literal of AppleScript: in quotes, with the backslash and the quote escaped, and no line breaks
//! or other control characters (a text of a translation cannot end the string and add a command)
QString appleScriptString(const QString &text);

//! The arguments of osascript (never a shell string): the constant command and the text of the dialog, nothing that a
//! person typed. The text of the dialog is the program's own sentence in the language of the program.
QStringList adminArguments(Direction direction, const QString &prompt);

//! The arguments of `ifconfig` that read the state (no privilege)
QStringList readArguments();

//! What the connection of this computer to the network is, for the interface of the default route
enum class LinkKind
{
  //! Not looked at yet, or there is no default route
  Unknown,
  WiFi,
  //! A wire (Ethernet, a dock): there is no AWDL on it, and nothing is measured
  Wired,
  //! Anything else (a VPN, a bridge)
  Other
};

//! The default route: the interface that carries the traffic and the gateway that it goes through
struct Route
{
  QString interfaceName;
  QString gateway;
  LinkKind kind = LinkKind::Unknown;

  bool operator==(const Route &other) const = default;
};

//! The gateway and the interface from what `route -n get default` wrote (the kind is not in it: it stays Unknown)
Route parseRoute(const QString &routeOutput);

//! Whether \p address is an IPv4 address of the local network: private (10/8, 172.16/12, 192.168/16), shared
//! (100.64/10) or link local (169.254/16). The program measures the latency to the gateway of the local network and to
//! nothing else
bool isLocalGateway(const QString &address);

//! Whether the route may be measured: Wi-Fi, a name of an interface that is a plain device name and not awdl0 (or
//! another interface of the wireless link itself), and a gateway of the local network
bool canProbe(const Route &route);

//! A burst of probes: how many, how far apart (a burst of 8 s holds at least two of the spikes that come every 3.6 s)
inline constexpr int kBurstCount = 40;
inline constexpr int kBurstIntervalMs = 200;
//! How often a burst is made while the core runs and the link is up
inline constexpr int kBurstEveryMs = 60000;

//! The arguments of `/sbin/ping` for a burst (no privilege: the interval is above the 2 ms that a person may use), or
//! none when the route may not be measured. The only address in them is the gateway of the route
QStringList pingArguments(const Route &route, int count = kBurstCount, int intervalMs = kBurstIntervalMs);

//! One probe of a burst: its number, the time of the answer, and whether there was none
struct Sample
{
  int sequence = 0;
  double rttMs = 0;
  bool lost = false;

  bool operator==(const Sample &other) const = default;
};

//! The probes from what `ping` wrote: a line with "icmp_seq=N ... time=X ms" is an answer and "Request timeout for
//! icmp_seq N" is a loss. The probes that were not written about (the last ones at the end) are not in the list
QList<Sample> parsePing(const QString &pingOutput);

//! What a burst says
enum class Pattern
{
  NotMeasured,
  //! Too few answers to say (the gateway does not answer, the network went away)
  NoData,
  Clean,
  //! The spikes of AWDL
  Stutter
};

struct BurstStats
{
  Pattern pattern = Pattern::NotMeasured;
  double medianMs = 0;
  double worstMs = 0;
  int received = 0;
  int lost = 0;
  //! The answers that are above the threshold
  int spikes = 0;
  //! The threshold that was used: an answer above it is a spike
  double thresholdMs = 0;
};

/*!
 * The pattern in the samples of one burst of \p sent probes. The threshold is the larger of 20 ms and five times the
 * median (a network that is slow all the time has no spikes). It is Stutter with at least three answers above it, or
 * two spikes that are 2.8 to 4.4 s apart (the 3.6 s of AWDL); with fewer than half of the answers (and fewer than
 * eight) it is NoData.
 */
BurstStats analyzeBurst(const QList<Sample> &samples, int sent = kBurstCount, int intervalMs = kBurstIntervalMs);

//! What the policy remembers of one run of the core (it ends when the core stops, or the option is turned off)
struct SessionMemory
{
  //! The password was asked for in this run, whatever the answer
  bool asked = false;
  //! The dialog of the password is open (the state that is read meanwhile says nothing)
  bool askRunning = false;
  //! The person turned the link on again with the button, which the program must not take back
  bool restoredByUser = false;

  bool operator==(const SessionMemory &other) const = default;
};

struct PolicyInput
{
  //! The program is on a Mac (nothing is done anywhere else)
  bool supported = false;
  //! The option
  bool optionOn = false;
  //! The core runs (server or client): it is starting, started or waiting to try again
  bool coreRunning = false;
  LinkState link = LinkState::Unknown;
  //! What the last burst of probes said, taken while the link was up (anything else: not measured)
  Pattern latency = Pattern::NotMeasured;
  SessionMemory memory;
};

struct PolicyDecision
{
  //! Show the dialog of the password now (once for a run)
  bool ask = false;
  //! Say in the status that the mouse may stutter, with the button that asks for the password: the link is up, the
  //! person was asked (or turned it off), and the latency to the gateway shows the spikes
  bool hint = false;
  SessionMemory memory;
};

/*!
 * What to do for a state that was read. Option off, not a Mac, or the core not running: nothing at all, and the run is
 * forgotten. The link is up and the password was not asked for in this run: ask, once (the person chose the option).
 * The link is up and it was asked (the answer was no, or the link came on again later): only the hint, and only when
 * the latency shows the spikes, never a second dialog by itself. The link is not up: nothing. There is no way back in
 * it: the program never turns the link on by itself (that asks for a password again), the person does it with the
 * button.
 */
PolicyDecision decide(const PolicyInput &input);

//! What decides whether a burst is to be made now
struct ProbeInput
{
  bool supported = false;
  bool optionOn = false;
  bool coreRunning = false;
  LinkState link = LinkState::Unknown;
  //! The dialog of the password is open
  bool askRunning = false;
  //! A lookup of the route or a burst is under way
  bool busy = false;
  //! How long ago the last lookup began (-1: never)
  qint64 sinceLastMs = -1;
  qint64 everyMs = kBurstEveryMs;
};

//! A burst is made only on a Mac, with the option on, while the core runs, with the link up, when none is under way and
//! the last one was long enough ago. (Whether the connection is Wi-Fi is learned by the lookup that comes first:
//! canProbe().)
bool shouldProbe(const ProbeInput &input);

//! What the status line of the settings says
enum class Summary
{
  //! There is no awdl0 (or this is not a Mac)
  NotAvailable,
  Unknown,
  //! The link is down
  Off,
  //! Up, and this computer is on a wire: it does not matter
  OnWired,
  //! Up, and the connection is not Wi-Fi (a VPN): nothing is measured
  OnNotWiFi,
  OnNotMeasured,
  OnNoData,
  OnClean,
  OnStutter
};

Summary summarize(LinkState link, LinkKind kind, Pattern latency);

} // namespace deskflow::awdl
