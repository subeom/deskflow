/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QHostAddress>
#include <QList>
#include <QString>
#include <QStringList>

namespace deskflow::gui {

//! What an address of this computer is for, as far as its address and the name of its network interface say
enum class AddressKind
{
  Lan,       //!< private address (RFC 1918) on a network card: the one a computer on the same network can reach
  Overlay,   //!< VPN or overlay network (CGNAT 100.64.0.0/10, tunnel interfaces such as utun, wt, tailscale, wg)
  Virtual,   //!< made by a virtual machine, container or bridge (docker, vmnet, vEthernet, virbr, bridge)
  LinkLocal, //!< 169.254.0.0/16: no address was given by anybody, e.g. a cable straight between two computers
  Other      //!< anything else, such as a public address
};

//! One IPv4 address of this computer with the interface it is on
struct AddressInfo
{
  QString address;
  QString interfaceName;
  AddressKind kind = AddressKind::Other;

  bool operator==(const AddressInfo &) const = default;
};

//! The part of a network interface that the choice of its addresses depends on (a plain copy, so that it can be made
//! up)
struct InterfaceSnapshot
{
  QString name; //!< the name a person sees: en0, Wi-Fi, vEthernet (WSL), wt0
  bool isUp = true;
  bool isRunning = true;
  bool isLoopBack = false;
  bool isPointToPoint = false;
  bool isVirtualType = false; //!< the system says the interface is virtual
  QStringList addresses;      //!< IPv4 and IPv6 addresses as text
};

/**
 * What an IPv4 address on an interface is for. An address that is link-local is that first, then a name or range that
 * says VPN or overlay, then a name that says virtual, then a private address on a network card; the rest is Other.
 */
AddressKind classifyAddress(
    const QHostAddress &address, const QString &interfaceName, bool isPointToPoint = false, bool isVirtualType = false
);

/**
 * The IPv4 addresses a client could be given to reach a server on this computer: up, running, not loopback. A
 * link-local address is left out when its interface has another address or is virtual or a tunnel, as the list of
 * suggested addresses always did. Ordered by what they are for, LAN first, then by number.
 */
QList<AddressInfo> usableIpv4Addresses(const QList<InterfaceSnapshot> &interfaces);

//! The same for the interfaces of this computer
QList<AddressInfo> usableIpv4Addresses();

//! The short name of a kind, in the language of the application: "LAN", "VPN / overlay", ...
QString addressKindLabel(AddressKind kind);

//! One sentence that says what a kind of address is for, in the language of the application
QString addressKindExplanation(AddressKind kind);

//! The tip that goes with a list of addresses: the client has to reach the one on the network both computers share
QString addressListTip();

//! The addresses one to a line, as `10.0.21.36 (en7, LAN)`, then the tip: for the tooltip of the address of the server
QString addressListToolTip(const QList<AddressInfo> &addresses);

//! The addresses as a table, what each kind is, and the tip, as HTML for a panel
QString addressListHtml(const QList<AddressInfo> &addresses);

} // namespace deskflow::gui
