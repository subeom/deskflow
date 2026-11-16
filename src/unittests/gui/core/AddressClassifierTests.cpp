/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AddressClassifierTests.h"

#include "gui/core/AddressClassifier.h"

#include <QTest>

using namespace deskflow::gui;

namespace {

void addColumns()
{
  QTest::addColumn<QString>("name");
  QTest::addColumn<QString>("address");
  QTest::addColumn<bool>("pointToPoint");
  QTest::addColumn<bool>("virtualType");
  QTest::addColumn<int>("kind");
}

void addRow(
    const char *name, const char *address, AddressKind kind, bool pointToPoint = false, bool virtualType = false
)
{
  QTest::addRow("%s %s", name, address) << QString::fromUtf8(name) << QString::fromLatin1(address) << pointToPoint
                                        << virtualType << static_cast<int>(kind);
}

void checkRow()
{
  QFETCH(QString, name);
  QFETCH(QString, address);
  QFETCH(bool, pointToPoint);
  QFETCH(bool, virtualType);
  QFETCH(int, kind);

  QCOMPARE(static_cast<int>(classifyAddress(QHostAddress(address), name, pointToPoint, virtualType)), kind);
}

InterfaceSnapshot
interfaceOf(const QString &name, const QStringList &addresses, bool pointToPoint = false, bool virtualType = false)
{
  InterfaceSnapshot snapshot;
  snapshot.name = name;
  snapshot.addresses = addresses;
  snapshot.isPointToPoint = pointToPoint;
  snapshot.isVirtualType = virtualType;
  return snapshot;
}

AddressInfo infoOf(const char *address, const char *interfaceName, AddressKind kind)
{
  return {QString::fromLatin1(address), QString::fromUtf8(interfaceName), kind};
}

} // namespace

void AddressClassifierTests::classifyAddress_onMacOsInterfaces_data()
{
  addColumns();
  addRow("en0", "10.18.0.77", AddressKind::Lan);
  addRow("en7", "10.0.21.36", AddressKind::Lan);
  // the hotspot of a phone
  addRow("en5", "172.20.10.2", AddressKind::Lan);
  addRow("utun100", "100.76.209.252", AddressKind::Overlay, true);
  addRow("utun5", "10.10.10.10", AddressKind::Overlay, true);
  addRow("utun3", "192.168.1.9", AddressKind::Overlay);
  // virtual machines and Internet Sharing
  addRow("bridge100", "192.168.64.1", AddressKind::Virtual);
  addRow("vmnet8", "192.168.78.1", AddressKind::Virtual);
  addRow("en0", "169.254.12.3", AddressKind::LinkLocal);
  addRow("en1", "203.0.113.7", AddressKind::Other);
}

void AddressClassifierTests::classifyAddress_onMacOsInterfaces()
{
  checkRow();
}

void AddressClassifierTests::classifyAddress_onWindowsInterfaces_data()
{
  addColumns();
  addRow("Ethernet", "192.168.1.20", AddressKind::Lan);
  addRow("Ethernet 2", "10.20.30.40", AddressKind::Lan);
  addRow("Wi-Fi", "10.0.0.5", AddressKind::Lan);
  addRow("vEthernet (Default Switch)", "172.28.80.1", AddressKind::Virtual);
  addRow("vEthernet (WSL (Hyper-V firewall))", "172.22.0.1", AddressKind::Virtual);
  addRow("VMware Network Adapter VMnet8", "192.168.78.1", AddressKind::Virtual);
  addRow("VirtualBox Host-Only Network", "192.168.56.1", AddressKind::Virtual);
  // the virtual adapter of Wi-Fi Direct and the mobile hotspot
  addRow("Local Area Connection* 10", "192.168.137.1", AddressKind::Virtual);
  addRow("Tailscale", "100.101.102.103", AddressKind::Overlay);
  addRow("NetBird", "100.64.12.1", AddressKind::Overlay);
  addRow("WireGuard Tunnel", "10.9.0.2", AddressKind::Overlay);
  addRow("OpenVPN TAP-Windows6", "10.8.0.6", AddressKind::Overlay);
  addRow("Cisco AnyConnect Secure Mobility Client Connection", "10.50.1.2", AddressKind::Overlay);
  addRow("PANGP Virtual Ethernet Adapter Secure", "10.1.1.1", AddressKind::Overlay);
  addRow("Ethernet 3", "169.254.7.7", AddressKind::LinkLocal);
}

void AddressClassifierTests::classifyAddress_onWindowsInterfaces()
{
  checkRow();
}

void AddressClassifierTests::classifyAddress_onLinuxInterfaces_data()
{
  addColumns();
  addRow("eth0", "192.168.0.10", AddressKind::Lan);
  addRow("enp3s0", "10.1.2.3", AddressKind::Lan);
  addRow("wlp2s0", "172.16.5.4", AddressKind::Lan);
  addRow("wlan0", "192.168.178.23", AddressKind::Lan);
  addRow("docker0", "172.17.0.1", AddressKind::Virtual);
  addRow("br-3f2a9c1b7d4e", "172.18.0.1", AddressKind::Virtual);
  addRow("virbr0", "192.168.122.1", AddressKind::Virtual);
  addRow("veth1a2b3c", "10.0.9.9", AddressKind::Virtual);
  addRow("vboxnet0", "192.168.56.1", AddressKind::Virtual);
  addRow("vmnet1", "172.16.100.1", AddressKind::Virtual);
  addRow("lxcbr0", "10.0.3.1", AddressKind::Virtual);
  addRow("cni0", "10.244.0.1", AddressKind::Virtual);
  addRow("tun0", "10.8.0.2", AddressKind::Overlay, true);
  addRow("tailscale0", "100.100.1.1", AddressKind::Overlay);
  addRow("wg0", "10.66.66.2", AddressKind::Overlay);
  addRow("wt0", "100.64.0.5", AddressKind::Overlay);
  addRow("zt5u4abcd", "10.147.17.5", AddressKind::Overlay);
  addRow("ppp0", "10.64.64.64", AddressKind::Overlay, true);
}

void AddressClassifierTests::classifyAddress_onLinuxInterfaces()
{
  checkRow();
}

void AddressClassifierTests::classifyAddress_byRangeAndFlags_data()
{
  addColumns();
  // the shared address space of carriers and overlay networks, whatever the interface is called
  addRow("eth0", "100.64.0.0", AddressKind::Overlay);
  addRow("eth0", "100.64.1.1", AddressKind::Overlay);
  addRow("eth0", "100.127.255.254", AddressKind::Overlay);
  addRow("eth0", "100.63.255.255", AddressKind::Other);
  addRow("eth0", "100.128.0.1", AddressKind::Other);
  // the three private ranges, and the addresses next to them
  addRow("eth0", "10.0.0.1", AddressKind::Lan);
  addRow("eth0", "10.255.255.255", AddressKind::Lan);
  addRow("eth0", "11.0.0.1", AddressKind::Other);
  addRow("eth0", "172.15.255.255", AddressKind::Other);
  addRow("eth0", "172.16.0.1", AddressKind::Lan);
  addRow("eth0", "172.31.255.254", AddressKind::Lan);
  addRow("eth0", "172.32.0.1", AddressKind::Other);
  addRow("eth0", "192.168.0.1", AddressKind::Lan);
  addRow("eth0", "192.169.0.1", AddressKind::Other);
  addRow("eth0", "169.254.0.1", AddressKind::LinkLocal);
  addRow("eth0", "169.253.0.1", AddressKind::Other);
  // the flags of the system count when the name says nothing
  addRow("nic0", "192.168.5.5", AddressKind::Virtual, false, true);
  addRow("foo0", "192.168.5.5", AddressKind::Overlay, true);
  addRow("foo0", "192.168.5.5", AddressKind::Overlay, true, true);
  // a name of a card is not taken for a name of a tunnel: tun, tap, wg and wt only as the start of a name
  addRow("Ethernet", "192.168.5.5", AddressKind::Lan);
  addRow("Intel(R) Ethernet Connection (3) I219-LM", "192.168.5.6", AddressKind::Lan);
  addRow("Realtek PCIe GbE Family Controller", "192.168.5.7", AddressKind::Lan);
  // link-local wins over a virtual name, a loopback is no interface of ours
  addRow("docker0", "169.254.1.1", AddressKind::LinkLocal);
}

void AddressClassifierTests::classifyAddress_byRangeAndFlags()
{
  checkRow();
}

void AddressClassifierTests::usableIpv4Addresses_ofAMacWithTwoNetworkCardsAndTunnels_listsEachWithItsKind()
{
  // what the Mac this was written on looks like: a USB LAN card and Wi-Fi on two networks, two tunnels
  QList<InterfaceSnapshot> interfaces;
  interfaces << interfaceOf(QStringLiteral("lo0"), {QStringLiteral("127.0.0.1"), QStringLiteral("::1")});
  interfaces.last().isLoopBack = true;
  interfaces << interfaceOf(
      QStringLiteral("en0"), {QStringLiteral("10.18.0.77"), QStringLiteral("169.254.5.5"), QStringLiteral("fe80::1")}
  );
  interfaces << interfaceOf(QStringLiteral("en7"), {QStringLiteral("10.0.21.36"), QStringLiteral("169.254.9.9")});
  interfaces << interfaceOf(QStringLiteral("utun100"), {QStringLiteral("100.76.209.252")}, true);
  interfaces << interfaceOf(QStringLiteral("utun5"), {QStringLiteral("10.10.10.10"), QStringLiteral("fd00::1")}, true);

  const auto addresses = usableIpv4Addresses(interfaces);

  const QList<AddressInfo> expected{
      infoOf("10.0.21.36", "en7", AddressKind::Lan),
      infoOf("10.18.0.77", "en0", AddressKind::Lan),
      infoOf("10.10.10.10", "utun5", AddressKind::Overlay),
      infoOf("100.76.209.252", "utun100", AddressKind::Overlay),
  };
  // numbers of an address are compared as numbers: 10.10.10.10 comes after 10.0.21.36 and 10.18.0.77 only by kind
  QCOMPARE(addresses.size(), expected.size());
  for (const auto &info : expected) {
    QVERIFY2(addresses.contains(info), qPrintable(info.address + QStringLiteral(" ") + info.interfaceName));
  }
  QCOMPARE(addresses.at(0).kind, AddressKind::Lan);
  QCOMPARE(addresses.at(1).kind, AddressKind::Lan);
  QCOMPARE(addresses.at(2).kind, AddressKind::Overlay);
  QCOMPARE(addresses.at(3).kind, AddressKind::Overlay);
  // inside a kind by number
  QCOMPARE(addresses.at(0).address, QStringLiteral("10.0.21.36"));
  QCOMPARE(addresses.at(1).address, QStringLiteral("10.18.0.77"));
  QCOMPARE(addresses.at(2).address, QStringLiteral("10.10.10.10"));
}

void AddressClassifierTests::usableIpv4Addresses_ofAWindowsComputerWithVirtualAdapters_putsTheLanFirst()
{
  QList<InterfaceSnapshot> interfaces;
  interfaces << interfaceOf(QStringLiteral("vEthernet (Default Switch)"), {QStringLiteral("172.28.80.1")});
  interfaces << interfaceOf(QStringLiteral("Tailscale"), {QStringLiteral("100.101.102.103")});
  interfaces << interfaceOf(QStringLiteral("Ethernet"), {QStringLiteral("192.168.1.20")});
  interfaces << interfaceOf(QStringLiteral("VMware Network Adapter VMnet8"), {QStringLiteral("192.168.78.1")});
  interfaces << interfaceOf(QStringLiteral("Wi-Fi"), {QStringLiteral("10.0.0.5")});

  const auto addresses = usableIpv4Addresses(interfaces);

  QCOMPARE(addresses.size(), 5);
  QCOMPARE(addresses.at(0), infoOf("10.0.0.5", "Wi-Fi", AddressKind::Lan));
  QCOMPARE(addresses.at(1), infoOf("192.168.1.20", "Ethernet", AddressKind::Lan));
  QCOMPARE(addresses.at(2), infoOf("100.101.102.103", "Tailscale", AddressKind::Overlay));
  // virtual last, by number
  QCOMPARE(addresses.at(3), infoOf("172.28.80.1", "vEthernet (Default Switch)", AddressKind::Virtual));
  QCOMPARE(addresses.at(4), infoOf("192.168.78.1", "VMware Network Adapter VMnet8", AddressKind::Virtual));
}

void AddressClassifierTests::usableIpv4Addresses_ofALinuxServerWithDocker_listsTheBridgeAsVirtual()
{
  QList<InterfaceSnapshot> interfaces;
  interfaces << interfaceOf(QStringLiteral("docker0"), {QStringLiteral("172.17.0.1")});
  interfaces << interfaceOf(QStringLiteral("enp3s0"), {QStringLiteral("192.168.0.10"), QStringLiteral("fe80::2")});
  interfaces << interfaceOf(QStringLiteral("wt0"), {QStringLiteral("100.64.0.5")});

  const auto addresses = usableIpv4Addresses(interfaces);

  QCOMPARE(
      addresses, (QList<AddressInfo>{
                     infoOf("192.168.0.10", "enp3s0", AddressKind::Lan),
                     infoOf("100.64.0.5", "wt0", AddressKind::Overlay),
                     infoOf("172.17.0.1", "docker0", AddressKind::Virtual),
                 })
  );
}

// a cable straight between two computers gives both a link-local address: that is the one to offer then
void AddressClassifierTests::usableIpv4Addresses_onlyLinkLocal_isKeptForACableBetweenTwoComputers()
{
  QList<InterfaceSnapshot> interfaces;
  interfaces << interfaceOf(QStringLiteral("en8"), {QStringLiteral("169.254.88.9")});
  // the one of a virtual interface is not
  interfaces << interfaceOf(QStringLiteral("vmnet1"), {QStringLiteral("169.254.1.1")});

  const auto addresses = usableIpv4Addresses(interfaces);

  QCOMPARE(addresses, (QList<AddressInfo>{infoOf("169.254.88.9", "en8", AddressKind::LinkLocal)}));
}

void AddressClassifierTests::usableIpv4Addresses_leavesOutWhatCannotBeReached()
{
  auto down = interfaceOf(QStringLiteral("en1"), {QStringLiteral("10.0.0.2")});
  down.isUp = false;
  auto notRunning = interfaceOf(QStringLiteral("en2"), {QStringLiteral("10.0.0.3")});
  notRunning.isRunning = false;
  auto loopBack = interfaceOf(QStringLiteral("lo0"), {QStringLiteral("127.0.0.1")});
  loopBack.isLoopBack = true;
  const auto onlyIpv6 = interfaceOf(QStringLiteral("en3"), {QStringLiteral("fe80::1"), QStringLiteral("2001:db8::1")});
  // the same address on two interfaces is listed once, for the first
  const auto first = interfaceOf(QStringLiteral("en4"), {QStringLiteral("10.0.0.9")});
  const auto second = interfaceOf(QStringLiteral("en5"), {QStringLiteral("10.0.0.9")});

  const auto addresses = usableIpv4Addresses({down, notRunning, loopBack, onlyIpv6, first, second});

  QCOMPARE(addresses, (QList<AddressInfo>{infoOf("10.0.0.9", "en4", AddressKind::Lan)}));
}

void AddressClassifierTests::usableIpv4Addresses_withOneAddress_isThatAddress()
{
  const auto addresses = usableIpv4Addresses({interfaceOf(QStringLiteral("eth0"), {QStringLiteral("192.168.0.10")})});
  QCOMPARE(addresses, (QList<AddressInfo>{infoOf("192.168.0.10", "eth0", AddressKind::Lan)}));
  QVERIFY(usableIpv4Addresses(QList<InterfaceSnapshot>{}).isEmpty());
}

void AddressClassifierTests::labels_haveWordsForEveryKind()
{
  for (const auto kind :
       {AddressKind::Lan, AddressKind::Overlay, AddressKind::Virtual, AddressKind::LinkLocal, AddressKind::Other}) {
    QVERIFY(!addressKindLabel(kind).isEmpty());
    QVERIFY(!addressKindExplanation(kind).isEmpty());
  }
  QCOMPARE(addressKindLabel(AddressKind::Lan), QStringLiteral("LAN"));
  QVERIFY(addressKindLabel(AddressKind::Overlay).contains(QStringLiteral("VPN")));
  QVERIFY(addressKindLabel(AddressKind::Virtual).contains(QStringLiteral("virtual"), Qt::CaseInsensitive));
  QVERIFY(addressKindLabel(AddressKind::LinkLocal).contains(QStringLiteral("link-local"), Qt::CaseInsensitive));
  QVERIFY(addressListTip().contains(QStringLiteral("client")));
  QVERIFY(addressListTip().contains(QStringLiteral("shared"), Qt::CaseInsensitive));
}

void AddressClassifierTests::addressListToolTip_putsEachAddressOnALineWithItsInterfaceAndKind()
{
  const QList<AddressInfo> addresses{
      infoOf("10.0.21.36", "en7", AddressKind::Lan),
      infoOf("100.76.209.252", "utun100", AddressKind::Overlay),
      infoOf("172.28.80.1", "vEthernet (Default Switch)", AddressKind::Virtual),
  };

  const auto tip = addressListToolTip(addresses);

  QVERIFY2(tip.contains(QStringLiteral("10.0.21.36 (en7, LAN)")), qPrintable(tip));
  QVERIFY(tip.contains(QStringLiteral("100.76.209.252 (utun100, VPN / overlay)")));
  QVERIFY(tip.contains(QStringLiteral("172.28.80.1 (vEthernet (Default Switch), virtual (VM, container))")));
  QCOMPARE(tip.count(QStringLiteral("<br/>")), 2);
  QVERIFY(tip.contains(addressListTip()));
}

void AddressClassifierTests::addressListHtml_hasATableAnExplanationPerKindAndTheTip()
{
  const QList<AddressInfo> addresses{
      infoOf("10.0.21.36", "en7", AddressKind::Lan),
      infoOf("10.18.0.77", "en0", AddressKind::Lan),
      infoOf("100.76.209.252", "<utun&100>", AddressKind::Overlay),
  };

  const auto html = addressListHtml(addresses);

  QVERIFY(html.contains(QStringLiteral("<b>10.0.21.36</b></td><td>en7</td><td>LAN</td>")));
  QVERIFY(
      html.contains(QStringLiteral("<b>100.76.209.252</b></td><td>&lt;utun&amp;100&gt;</td><td>VPN / overlay</td>"))
  );
  // a kind is explained once, and only when there is an address of it
  QCOMPARE(html.count(addressKindExplanation(AddressKind::Lan).toHtmlEscaped()), 1);
  QCOMPARE(html.count(addressKindExplanation(AddressKind::Overlay).toHtmlEscaped()), 1);
  QCOMPARE(html.count(addressKindExplanation(AddressKind::Virtual).toHtmlEscaped()), 0);
  QVERIFY(html.contains(addressListTip().toHtmlEscaped()));
}

QTEST_MAIN(AddressClassifierTests)
