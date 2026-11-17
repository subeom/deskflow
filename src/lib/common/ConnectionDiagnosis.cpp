/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "common/ConnectionDiagnosis.h"

#include <QMetaType>

#include <cmath>
#include <format>

namespace deskflow::diagnosis {

namespace {

std::string bracketed(const std::string &address)
{
  if (address.find(':') != std::string::npos && !address.starts_with('[')) {
    return "[" + address + "]";
  }
  return address;
}

std::string seconds(double value)
{
  return std::format("{}", value);
}

bool keepsDetail(FailureKind kind)
{
  switch (kind) {
  case FailureKind::TimedOut:
  case FailureKind::Refused:
  case FailureKind::FingerprintRejected:
  case FailureKind::UnknownClientName:
  case FailureKind::AlreadyConnected:
    return false;
  default:
    return true;
  }
}

constexpr auto kMacLocalNetworkCause =
    " On macOS 15 or later, Deskflow also needs permission under System Settings > Privacy & Security > Local Network.";

// Windows reports a connection to a port that nothing listens on after about two seconds (it sends the first packet
// again after the reset), which the default limit of 2 s ends as a timeout: to the client the two cannot be told apart
constexpr auto kWindowsClosedPortCause =
    " Windows also reports a port that nothing listens on this way, about 2 s after the attempt: the server may simply "
    "not be running.";

} // namespace

std::string formatTarget(const std::string &host, const std::string &resolvedAddress, int port)
{
  auto target = bracketed(host);
  if (!resolvedAddress.empty() && resolvedAddress != host) {
    target += " (" + bracketed(resolvedAddress) + ")";
  }
  if (port > 0) {
    target += ":" + std::to_string(port);
  }
  return target;
}

std::string Diagnosis::line() const
{
  return headline + " " + cause + " See " + kDocsPage;
}

std::string hintText(Hint hint, int port, double timeoutSeconds)
{
  switch (hint) {
  case Hint::ServerFirewall:
    return "On the server computer, allow inbound TCP port " + std::to_string(port) +
           " for Deskflow in its firewall (the client computer needs no inbound port).";
  case Hint::NetworkIsolation:
    return "If both computers are on the same network and it still fails, the network may keep computers from talking "
           "to each other (Wi-Fi client isolation, separate VLANs, access lists): ask IT, or use another network.";
  case Hint::CheckAddress:
    return "Check that the address is the one the server computer has on the network both computers share, not one "
           "of a VPN, Docker or virtual network card.";
  case Hint::StartServer:
    return "Check that Deskflow runs in server mode on that computer and that both computers use the same port.";
  case Hint::SwapRoles:
    return "Try swapping the roles: make the computer that can accept connections the server.";
  case Hint::MacLocalNetwork:
    return "On macOS 15 or later, allow Deskflow in System Settings > Privacy & Security > Local Network.";
  case Hint::VpnLocalNetwork:
    return "A VPN or security agent may block local network traffic: look for its 'allow local network access' option.";
  case Hint::CheckName:
    return "Check the spelling of the server name, or type the server's IP address instead.";
  case Hint::TlsSetting:
    return "Both computers must use the same TLS (encryption) setting.";
  case Hint::InspectionProduct:
    return "A security product that inspects or filters network traffic may interrupt the connection.";
  case Hint::TrustFingerprint:
    return "Compare the fingerprint with the one of the server and approve it when you are asked; remove the old entry "
           "if the server's certificate was replaced on purpose.";
  case Hint::OtherService:
    return "Another program may use that port: check which program listens on it on the server computer.";
  case Hint::UpdateDeskflow:
    return "Update Deskflow on the older of the two computers.";
  case Hint::AddScreen:
    return "Add this computer's exact name (or an alias) as a screen in the server's layout.";
  case Hint::DuplicateName:
    return "Give every computer its own name, or wait about 10 seconds for the old connection to close.";
  case Hint::SlowNetwork:
    return "If the network is slow, raise the connect timeout (setting client/connectTimeout, now " +
           seconds(timeoutSeconds) + " s, allowed 1 to 60).";
  }
  return {};
}

Diagnosis diagnose(const Failure &failure)
{
  const auto target = formatTarget(failure.host, failure.resolvedAddress, failure.port);
  const bool mac = failure.platform == InputPlatform::MacOS;
  const auto limit = seconds(failure.timeoutSeconds);

  Diagnosis result;
  result.kind = failure.kind;
  std::string base;

  switch (failure.kind) {
  case FailureKind::DnsFailure:
    base = "cannot find the server " + target + ": its name could not be resolved";
    result.cause = "Check the spelling, or type the server's IP address instead of its name.";
    result.hints = {Hint::CheckName};
    break;

  case FailureKind::TimedOut:
    if (failure.phase == Phase::Tls) {
      base = "the TCP connection to the server " + target + " was made, but it timed out within " + limit +
             " s while the secure handshake (TLS) was running";
      result.cause = "Both computers must use the same TLS (encryption) setting; a security product that inspects "
                     "traffic or a slow network can also stall the handshake.";
      result.hints = {Hint::TlsSetting, Hint::InspectionProduct, Hint::SlowNetwork};
    } else if (failure.phase == Phase::Hello) {
      base = "the connection to the server " + target + " was made, but it timed out within " + limit +
             " s while waiting for the server's greeting";
      result.cause = "That computer may not run a Deskflow server on this port, or a security product holds the "
                     "connection; on a slow network raise the connect timeout.";
      result.hints = {Hint::OtherService, Hint::InspectionProduct, Hint::SlowNetwork};
    } else {
      base = "cannot reach the server " + target + ": the connection timed out (no answer within " + limit +
             " s) while connecting (TCP)";
      result.cause = "A firewall on the server computer or a network that keeps computers from talking to each other "
                     "drops it silently.";
      result.hints = {Hint::ServerFirewall, Hint::NetworkIsolation, Hint::SwapRoles};
      if (failure.platform == InputPlatform::Windows) {
        result.cause += kWindowsClosedPortCause;
        result.hints.insert(result.hints.begin() + 1, Hint::StartServer);
      }
      if (mac) {
        result.cause += kMacLocalNetworkCause;
        result.hints.push_back(Hint::MacLocalNetwork);
      }
      result.hints.push_back(Hint::VpnLocalNetwork);
      result.hints.push_back(Hint::SlowNetwork);
    }
    break;

  case FailureKind::Refused:
    base = "the server " + target + " refused the connection: nothing is listening on that address and port";
    result.cause = "Deskflow may not run in server mode on that computer, it may use another port or another network "
                   "card address, or a firewall rejects the port.";
    result.hints = {Hint::StartServer, Hint::CheckAddress, Hint::ServerFirewall};
    break;

  case FailureKind::NoRoute:
    base = "no route to the server " + target + ": the network has no path to that address";
    result.cause = "The computers may be on different or isolated networks (separate VLAN, guest or client-isolated "
                   "Wi-Fi), the address may belong to another network card, or a VPN captures local traffic.";
    result.hints = {Hint::NetworkIsolation, Hint::CheckAddress, Hint::VpnLocalNetwork, Hint::SwapRoles};
    if (mac) {
      result.cause += kMacLocalNetworkCause;
      result.hints.push_back(Hint::MacLocalNetwork);
    }
    break;

  case FailureKind::Reset:
    base = "the connection to the server " + target + " was reset while connecting";
    result.cause = "A firewall, a security product that inspects traffic, or another program on that port closed it.";
    result.hints = {Hint::InspectionProduct, Hint::ServerFirewall, Hint::OtherService};
    break;

  case FailureKind::AccessDenied:
    base = "this computer is not allowed to connect to the server " + target;
    result.cause = "A firewall or security product on this computer blocks Deskflow from using the network.";
    result.hints = {Hint::InspectionProduct};
    break;

  case FailureKind::TlsHandshakeFailed:
    base = "the TCP connection to the server " + target + " was made, but the secure handshake (TLS) failed";
    result.cause = "Both computers must use the same TLS (encryption) setting. A security product that inspects or "
                   "rewrites traffic can also break the handshake.";
    result.hints = {Hint::TlsSetting, Hint::InspectionProduct};
    break;

  case FailureKind::FingerprintRejected:
    base = "the server " + target + " presented a certificate fingerprint this computer does not trust";
    result.cause = "This is expected the first time you connect: approve the fingerprint when you are asked. If it "
                   "changed since then, the server's certificate was replaced or something intercepts the connection.";
    result.hints = {Hint::TrustFingerprint};
    break;

  case FailureKind::NotDeskflowServer:
    base = "the server " + target + " answered, but it is not a Deskflow server";
    result.cause = "Another program uses that port, or a proxy sits in between.";
    result.hints = {Hint::OtherService, Hint::InspectionProduct};
    break;

  case FailureKind::VersionMismatch:
    base = "the server " + target + " speaks a protocol version this computer cannot use";
    result.cause = "One of the two computers runs a much older Deskflow, Barrier or Synergy.";
    result.hints = {Hint::UpdateDeskflow};
    break;

  case FailureKind::UnknownClientName:
    base = "the server " + target + " does not know this computer's name";
    result.cause = "The server only accepts computers that are listed as screens: add this computer's exact name or an "
                   "alias there.";
    result.hints = {Hint::AddScreen};
    break;

  case FailureKind::AlreadyConnected:
    base = "a computer with this name is already connected to the server " + target;
    result.cause = "Another computer uses the same name, or the previous connection of this one has not closed yet "
                   "(it can take about 10 seconds).";
    result.hints = {Hint::DuplicateName};
    break;

  case FailureKind::Unknown:
    base = "could not connect to the server " + target;
    result.cause = "Check that the address is correct, that the server is running and that no firewall is in the way.";
    result.hints = {Hint::CheckAddress, Hint::StartServer, Hint::ServerFirewall};
    break;
  }

  if (!failure.detail.empty() && keepsDetail(failure.kind)) {
    base += " (" + failure.detail + ")";
  }
  result.headline = base + ".";
  return result;
}

ConnectTimeout connectTimeoutFromSetting(const QVariant &raw)
{
  const ConnectTimeout fallback{kDefaultConnectTimeoutSeconds, false};

  if (!raw.isValid() || (raw.typeId() == QMetaType::QString && raw.toString().trimmed().isEmpty())) {
    return {kDefaultConnectTimeoutSeconds, true};
  }

  // a value with a comma in it is read as a list of parts by the settings file, never a number
  if (raw.typeId() == QMetaType::QStringList) {
    return fallback;
  }

  bool isNumber = false;
  const auto value = raw.toDouble(&isNumber);
  if (!isNumber || !std::isfinite(value) || value < kMinConnectTimeoutSeconds || value > kMaxConnectTimeoutSeconds) {
    return fallback;
  }
  return {value, true};
}

} // namespace deskflow::diagnosis
