/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/InputCapabilities.h"

#include <QVariant>

#include <string>
#include <vector>

/**
 * @file
 * Turns what happened when a connection to the server failed into one line that says what is wrong and what to try.
 * Pure functions: no sockets, no log, no settings access, so the core (client) and the GUI (test connection) share one
 * set of words and one set of tests.
 */
namespace deskflow::diagnosis {

//! What went wrong. Values describe the symptom, not the operating system error that was seen.
enum class FailureKind
{
  Unknown,
  DnsFailure,          //!< the server name could not be resolved
  TimedOut,            //!< nothing answered within the time limit
  Refused,             //!< the address answered with a reset: nothing listens on that port
  NoRoute,             //!< no route to the host or network unreachable
  Reset,               //!< the connection was reset while connecting
  AccessDenied,        //!< the operating system or a firewall on this computer denied the connection
  TlsHandshakeFailed,  //!< TCP connected, the secure handshake (TLS) failed
  FingerprintRejected, //!< TLS worked, the server certificate fingerprint is not trusted
  NotDeskflowServer,   //!< something answered, but not with a Deskflow hello
  VersionMismatch,     //!< the server speaks a protocol version this client does not
  UnknownClientName,   //!< the server has no screen with this computer's name
  AlreadyConnected     //!< the server already has a computer with this name connected
};

//! How far a connection attempt got when it failed. The client gives TCP, TLS and the hello one shared time limit.
enum class Phase
{
  Resolving, //!< looking up the server name
  Tcp,       //!< waiting for the TCP connection
  Tls,       //!< TCP is connected, the secure handshake is running
  Hello      //!< the connection is up, waiting for the server's greeting
};

//! One piece of advice. The GUI translates these, the core logs the English text of hintText().
enum class Hint
{
  ServerFirewall,
  NetworkIsolation,
  CheckAddress,
  StartServer,
  SwapRoles,
  MacLocalNetwork,
  VpnLocalNetwork,
  CheckName,
  TlsSetting,
  InspectionProduct,
  TrustFingerprint,
  OtherService,
  UpdateDeskflow,
  AddScreen,
  DuplicateName,
  SlowNetwork
};

inline constexpr double kDefaultConnectTimeoutSeconds = 2.0;
inline constexpr double kMinConnectTimeoutSeconds = 1.0;
inline constexpr double kMaxConnectTimeoutSeconds = 60.0;

//! Where the page that explains these messages is, relative to the source tree.
inline constexpr auto kDocsPage = "docs/connecting-across-networks.md";

struct Failure
{
  FailureKind kind = FailureKind::Unknown;
  Phase phase = Phase::Tcp;
  std::string host;            //!< as typed by the user
  std::string resolvedAddress; //!< the numeric address that was tried, empty when it is the same as host
  int port = 0;
  double timeoutSeconds = kDefaultConnectTimeoutSeconds; //!< only used to explain a timeout
  std::string detail;                                    //!< text from the operating system or the peer, may be empty
  InputPlatform platform = InputPlatform::Other;         //!< the computer this client runs on
};

struct Diagnosis
{
  FailureKind kind = FailureKind::Unknown;
  std::string headline; //!< what happened, names the server
  std::string cause;    //!< the most likely reasons
  std::vector<Hint> hints;

  //! headline, cause and the pointer to the docs, as one line for the log
  std::string line() const;
};

//! `host:port`, `host (resolved):port` when the typed name differs from the address used, IPv6 in brackets
std::string formatTarget(const std::string &host, const std::string &resolvedAddress, int port);

Diagnosis diagnose(const Failure &failure);

//! The English sentence of a hint
std::string hintText(Hint hint, int port, double timeoutSeconds = kDefaultConnectTimeoutSeconds);

struct ConnectTimeout
{
  double seconds = kDefaultConnectTimeoutSeconds;
  bool valid = true; //!< false: the value was out of range or not a number and the default is used instead
};

/**
 * The time limit for TCP + TLS + hello from the value of `client/connectTimeout`.
 * An unset or empty value is the default (2 s) and valid. A number from 1 to 60 is used as it is. Anything else
 * (text, NaN, below 1, above 60) falls back to the default and is reported as not valid so the caller can say so once.
 */
ConnectTimeout connectTimeoutFromSetting(const QVariant &raw);

} // namespace deskflow::diagnosis
