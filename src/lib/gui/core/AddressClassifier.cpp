/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AddressClassifier.h"

#include "NetworkMonitor.h"

#include <QCoreApplication>
#include <QNetworkInterface>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace deskflow::gui {

namespace {

bool matches(const QString &pattern, const QString &name)
{
  const QRegularExpression expression(pattern, QRegularExpression::CaseInsensitiveOption);
  return expression.match(name).hasMatch();
}

// tunnels of VPNs and overlay networks: by the way the systems name them (macOS utun, Linux tun, wg, wt, zt, Windows
// adapters of the products)
bool isOverlayName(const QString &name)
{
  static const auto prefixed = QStringLiteral(R"(^(utun|tun|tap|ppp|wg|wt)\d*$|^zt[0-9a-z]{6,}$)");
  static const auto products = QStringLiteral(
      "vpn|tailscale|netbird|wireguard|wintun|zerotier|nebula|openvpn|tap-windows|anyconnect|globalprotect|pangp|"
      "fortinet|zscaler|netskope|ipsec|l2tp|pptp|sstp|tunnel"
  );
  return matches(prefixed, name) || matches(products, name);
}

// what virtual machines, containers and bridges make; the older list is the one the suggestion of the server uses
bool isVirtualName(const QString &name)
{
  static const auto others = QStringLiteral(
      "vethernet|hyper-v|vmware|virtualbox|vbox|vmnet|bridge\\d*$|^br-|docker|^lxc|^lxd|^podman|^cni\\d*$|^flannel|"
      "^kube|^cali|^weave|virbr|veth|vnic|wsl|local area connection\\*|virtual"
  );
  return NetworkMonitor::isVirtualInterface(name) || matches(others, name);
}

bool isShared100(const QHostAddress &address)
{
  // 100.64.0.0/10: the address space of carriers and overlay networks
  return address.isInSubnet(QHostAddress(QStringLiteral("100.64.0.0")), 10);
}

int kindOrder(AddressKind kind)
{
  switch (kind) {
  case AddressKind::Lan:
    return 0;
  case AddressKind::Other:
    return 1;
  case AddressKind::Overlay:
    return 2;
  case AddressKind::Virtual:
    return 3;
  case AddressKind::LinkLocal:
    return 4;
  }
  return 5;
}

} // namespace

AddressKind
classifyAddress(const QHostAddress &address, const QString &interfaceName, bool isPointToPoint, bool isVirtualType)
{
  if (address.isLinkLocal()) {
    return AddressKind::LinkLocal;
  }
  if (isOverlayName(interfaceName) || isPointToPoint || isShared100(address)) {
    return AddressKind::Overlay;
  }
  if (isVirtualName(interfaceName) || isVirtualType) {
    return AddressKind::Virtual;
  }
  return address.isPrivateUse() ? AddressKind::Lan : AddressKind::Other;
}

QList<AddressInfo> usableIpv4Addresses(const QList<InterfaceSnapshot> &interfaces)
{
  QList<AddressInfo> result;
  QSet<QString> seen;

  for (const auto &interface : interfaces) {
    if (!interface.isUp || !interface.isRunning || interface.isLoopBack) {
      continue;
    }

    QList<QHostAddress> addresses;
    for (const auto &text : interface.addresses) {
      const QHostAddress address(text);
      if (address.protocol() == QHostAddress::IPv4Protocol && !address.isLoopback()) {
        addresses.append(address);
      }
    }
    // a link-local address is only worth offering as the only address of a network card, e.g. a cable between two
    // computers
    const bool hasOtherAddress =
        std::ranges::any_of(addresses, [](const QHostAddress &address) { return !address.isLinkLocal(); });
    const bool isTunnelOrVirtual = interface.isPointToPoint || interface.isVirtualType ||
                                   isOverlayName(interface.name) || isVirtualName(interface.name);
    const bool skipLinkLocal = isTunnelOrVirtual || hasOtherAddress;

    for (const auto &address : std::as_const(addresses)) {
      if ((skipLinkLocal && address.isLinkLocal()) || seen.contains(address.toString())) {
        continue;
      }
      seen.insert(address.toString());
      result.append(
          {address.toString(), interface.name,
           classifyAddress(address, interface.name, interface.isPointToPoint, interface.isVirtualType)}
      );
    }
  }

  std::ranges::stable_sort(result, [](const AddressInfo &a, const AddressInfo &b) {
    if (kindOrder(a.kind) != kindOrder(b.kind)) {
      return kindOrder(a.kind) < kindOrder(b.kind);
    }
    return QHostAddress(a.address).toIPv4Address() < QHostAddress(b.address).toIPv4Address();
  });
  return result;
}

QList<AddressInfo> usableIpv4Addresses()
{
  QList<InterfaceSnapshot> snapshots;
  for (const auto &interface : QNetworkInterface::allInterfaces()) {
    InterfaceSnapshot snapshot;
    snapshot.name = interface.humanReadableName();
    snapshot.isUp = interface.flags().testFlag(QNetworkInterface::IsUp);
    snapshot.isRunning = interface.flags().testFlag(QNetworkInterface::IsRunning);
    snapshot.isLoopBack = interface.flags().testFlag(QNetworkInterface::IsLoopBack);
    snapshot.isPointToPoint = interface.flags().testFlag(QNetworkInterface::IsPointToPoint);
    snapshot.isVirtualType = interface.type() == QNetworkInterface::Virtual;
    for (const auto &entry : interface.addressEntries()) {
      snapshot.addresses.append(entry.ip().toString());
    }
    snapshots.append(snapshot);
  }
  return usableIpv4Addresses(snapshots);
}

QString addressKindLabel(AddressKind kind)
{
  switch (kind) {
  case AddressKind::Lan:
    return QCoreApplication::translate("AddressClassifier", "LAN");
  case AddressKind::Overlay:
    return QCoreApplication::translate("AddressClassifier", "VPN / overlay");
  case AddressKind::Virtual:
    return QCoreApplication::translate("AddressClassifier", "virtual (VM, container)");
  case AddressKind::LinkLocal:
    return QCoreApplication::translate("AddressClassifier", "link-local (no address from the network)");
  case AddressKind::Other:
    break;
  }
  return QCoreApplication::translate("AddressClassifier", "other");
}

QString addressKindExplanation(AddressKind kind)
{
  switch (kind) {
  case AddressKind::Lan:
    return QCoreApplication::translate(
        "AddressClassifier", "An address on a network card: computers on the same network can reach it."
    );
  case AddressKind::Overlay:
    return QCoreApplication::translate(
        "AddressClassifier",
        "An address of a VPN or overlay network (such as NetBird or Tailscale): only computers on that network can "
        "reach it."
    );
  case AddressKind::Virtual:
    return QCoreApplication::translate(
        "AddressClassifier", "An address only the virtual machines or containers on this computer can reach."
    );
  case AddressKind::LinkLocal:
    return QCoreApplication::translate(
        "AddressClassifier",
        "No network gave this card an address. It only works over a cable straight between two computers."
    );
  case AddressKind::Other:
    break;
  }
  return QCoreApplication::translate("AddressClassifier", "Another kind of address, for example a public one.");
}

QString addressListTip()
{
  return QCoreApplication::translate(
      "AddressClassifier",
      "The client has to reach the address you give it. Use the address on the shared network of both computers "
      "(usually LAN), not a virtual or VPN address, unless both computers are on that VPN."
  );
}

QString addressListToolTip(const QList<AddressInfo> &addresses)
{
  QStringList lines;
  for (const auto &info : addresses) {
    lines << QStringLiteral("%1 (%2, %3)")
                 .arg(info.address.toHtmlEscaped(), info.interfaceName.toHtmlEscaped(), addressKindLabel(info.kind));
  }
  return QStringLiteral("<p>%1</p><p>%2</p>")
      .arg(lines.join(QStringLiteral("<br/>")), addressListTip().toHtmlEscaped());
}

QString addressListHtml(const QList<AddressInfo> &addresses)
{
  QString html = QStringLiteral(
                     "<table cellspacing=\"0\" cellpadding=\"4\"><tr><th align=\"left\">%1</th>"
                     "<th align=\"left\">%2</th><th align=\"left\">%3</th></tr>"
  )
                     .arg(
                         QCoreApplication::translate("AddressClassifier", "Address").toHtmlEscaped(),
                         QCoreApplication::translate("AddressClassifier", "Network interface").toHtmlEscaped(),
                         QCoreApplication::translate("AddressClassifier", "What it is").toHtmlEscaped()
                     );
  QList<AddressKind> kinds;
  for (const auto &info : addresses) {
    html += QStringLiteral("<tr><td><b>%1</b></td><td>%2</td><td>%3</td></tr>")
                .arg(info.address.toHtmlEscaped(), info.interfaceName.toHtmlEscaped(), addressKindLabel(info.kind));
    if (!kinds.contains(info.kind)) {
      kinds.append(info.kind);
    }
  }
  html += QStringLiteral("</table>");

  html += QStringLiteral("<ul>");
  for (const auto kind : std::as_const(kinds)) {
    html += QStringLiteral("<li><b>%1</b>: %2</li>")
                .arg(addressKindLabel(kind).toHtmlEscaped(), addressKindExplanation(kind).toHtmlEscaped());
  }
  html += QStringLiteral("</ul><p>%1</p>").arg(addressListTip().toHtmlEscaped());
  return html;
}

} // namespace deskflow::gui
