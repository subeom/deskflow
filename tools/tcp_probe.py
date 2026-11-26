#!/usr/bin/env python3
# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-License-Identifier: MIT
"""tcp_probe.py - read-only TCP reachability probe (Python 3.6+, stdlib only).

Client mode : python3 tcp_probe.py HOST [PORT] [--timeout 3] [--count 3] [--prefix 24] [--no-banner]
Listen mode : python3 tcp_probe.py --listen PORT        (reverse test; use a spare port, NOT the Deskflow port)

Examples (10.0.0.5 stands for the IP of the computer that runs the Deskflow server):
  python3 tcp_probe.py 10.0.0.5 24800 --count 3 --prefix 24      (macOS / Linux)
  py tcp_probe.py 10.0.0.5 24800                                  (Windows, Python launcher)

Client mode makes ordinary TCP connects (default port 24800) and classifies the result:
  OPEN      connect() succeeded (then waits ~1.5 s for a banner)
  TIMEOUT   nothing came back: silent drop (firewall DROP, client/AP isolation, ACL, wrong subnet, host off)
  REFUSED   an RST/ICMP came back: host reachable but nothing listening or a REJECT rule
  NO ROUTE  no route / host unreachable (also what macOS 15+ reports when Local Network access is denied)
  RESET     connection was accepted then killed
Exit code: 0 open, 2 timeout, 3 refused, 4 no route, 5 reset, 6 other, 7 name lookup failed.

A Deskflow server logs every probe as an "accepted client connection" (plain) or a failed TLS accept (TLS);
that is harmless. Listen mode on Windows/macOS may raise the OS firewall prompt for python: answer as you
would for Deskflow, or cancel.

Tested on macOS against localhost listeners and closed ports only; the Windows error codes are from the
documented Winsock values and have not been run on a Windows computer.
"""
import argparse
import errno
import ipaddress
import socket
import sys
import time

WIN_REFUSED = {10061}
WIN_NOROUTE = {10051, 10065, 10050}
WIN_TIMEOUT = {10060}
WIN_RESET = {10054, 10053}

REFUSED = {errno.ECONNREFUSED} | WIN_REFUSED
NOROUTE = {errno.EHOSTUNREACH, errno.ENETUNREACH, errno.ENETDOWN} | WIN_NOROUTE
TIMEOUT = {errno.ETIMEDOUT} | WIN_TIMEOUT
RESET = {errno.ECONNRESET, errno.ECONNABORTED} | WIN_RESET

MEANING = {
    "OPEN": "TCP handshake completed - the network path and the port are fine.",
    "TIMEOUT": "no answer: silent drop (host/network firewall DROP, client or AP isolation, VLAN ACL, wrong subnet/IP, host asleep).",
    "REFUSED": "host answered with RST: reachable, but nothing listens on that port (server not running / bound to another IP) or a REJECT rule.",
    "NO ROUTE": "no route/host unreachable: wrong subnet or VLAN, no ARP answer, or (macOS 15+) the app lacks Local Network access.",
    "RESET": "connection was reset right after it opened: a security agent/IPS or a half-working proxy in the path.",
    "OTHER": "unclassified socket error, see the number above.",
}
EXIT = {"OPEN": 0, "TIMEOUT": 2, "REFUSED": 3, "NO ROUTE": 4, "RESET": 5, "OTHER": 6}


def classify(exc):
    if isinstance(exc, socket.timeout):
        return "TIMEOUT"
    code = getattr(exc, "errno", None)
    if code in TIMEOUT:
        return "TIMEOUT"
    if code in REFUSED:
        return "REFUSED"
    if code in NOROUTE:
        return "NO ROUTE"
    if code in RESET:
        return "RESET"
    return "OTHER"


def source_ip_for(family, addr):
    """Local address the OS would use to reach addr (UDP connect sends no packet)."""
    try:
        s = socket.socket(family, socket.SOCK_DGRAM)
    except OSError:
        return None
    try:
        s.connect(addr)
        return s.getsockname()[0]
    except OSError:
        return None
    finally:
        try:
            s.close()
        except OSError:
            pass


def probe_once(family, addr, timeout, read_banner):
    s = socket.socket(family, socket.SOCK_STREAM)
    s.settimeout(timeout)
    t0 = time.time()
    try:
        s.connect(addr)
    except OSError as exc:
        ms = (time.time() - t0) * 1000
        s.close()
        return classify(exc), ms, "{} (errno {})".format(exc.strerror or exc, getattr(exc, "errno", None)), b""
    ms = (time.time() - t0) * 1000
    banner = b""
    if read_banner:
        s.settimeout(1.5)
        try:
            banner = s.recv(64)
        except socket.timeout:
            pass
        except OSError as exc:
            s.close()
            return "RESET", ms, "{} (errno {})".format(exc.strerror or exc, getattr(exc, "errno", None)), b""
    s.close()
    return "OPEN", ms, "", banner


def describe_banner(banner):
    if banner[:7] in (b"Barrier", b"Synergy"):
        return "got a {} hello: Deskflow server with TLS OFF".format(banner[:7].decode())
    if banner:
        return "got {} unexpected bytes: {!r}".format(len(banner), banner[:16])
    return "no banner in 1.5 s: normal for a Deskflow server with TLS ON (it waits for the TLS ClientHello) or a non-Deskflow listener"


def run_client(args):
    try:
        infos = socket.getaddrinfo(args.host, args.port, 0, socket.SOCK_STREAM)
    except socket.gaierror as exc:
        print("NAME LOOKUP FAILED for {!r}: {}".format(args.host, exc))
        print("-> hostname does not resolve from this machine (DNS/mDNS/NetBIOS); try the IP address.")
        return 7
    seen, targets = set(), []
    for fam, _t, _p, _c, sa in infos:
        if sa[:2] not in seen:
            seen.add(sa[:2])
            targets.append((fam, sa))
    print("target {}:{}  resolved to {} address(es)".format(args.host, args.port, len(targets)))
    worst = 0
    any_open = False
    failed_addrs = []
    for fam, sa in targets:
        label = "IPv6" if fam == socket.AF_INET6 else "IPv4"
        print("\n== {} {} port {}".format(label, sa[0], sa[1]))
        src = source_ip_for(fam, sa)
        if src:
            note = ""
            if args.prefix and fam == socket.AF_INET:
                try:
                    net = ipaddress.ip_network("{}/{}".format(src, args.prefix), strict=False)
                    same = ipaddress.ip_address(sa[0]) in net
                    note = "  -> {} the same /{} subnet as the target".format("IN" if same else "NOT in", args.prefix)
                except ValueError:
                    pass
            print("   route: this machine would use source address {}{}".format(src, note))
        results = []
        for i in range(args.count):
            status, ms, detail, banner = probe_once(fam, sa, args.timeout, not args.no_banner)
            results.append((status, ms))
            extra = detail or (describe_banner(banner) if not args.no_banner else "")
            print("   try {}: {:<8} {:7.0f} ms  {}".format(i + 1, status, ms, extra))
            if i + 1 < args.count:
                time.sleep(1)
        statuses = [r[0] for r in results]
        final = max(set(statuses), key=statuses.count) if statuses else "OTHER"
        ok = statuses.count("OPEN")
        print("   summary: {}/{} open. {}".format(ok, len(statuses), MEANING.get(final, "")))
        slow = [m for s, m in results if s == "OPEN" and m > 1000]
        if slow:
            print("   WARNING: connect took >1 s; the Deskflow client gives up after 2 s in total (TCP+TLS+hello).")
        if ok and ok != len(statuses):
            print("   WARNING: flaky path (some tries failed): Wi-Fi roaming, rate limiting or an IPS.")
        if ok:
            any_open = True
        else:
            failed_addrs.append(sa[0])
            worst = max(worst, EXIT[final])
    if any_open and failed_addrs:
        print("\nNOTE: {} did not connect but another address did. Deskflow tries the resolved addresses one after".format(", ".join(failed_addrs)))
        print("      another (a hostname can resolve to IPv6 first while the server listens on IPv4 only); use the IP that works.")
    return 0 if any_open else worst


def run_listen(args):
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        srv.bind(("0.0.0.0", args.listen))
    except OSError as exc:
        print("cannot bind 0.0.0.0:{}: {}".format(args.listen, exc))
        return 6
    srv.listen(5)
    print("listening on 0.0.0.0:{} (IPv4). From the OTHER machine run: tcp_probe.py THIS_IP {}".format(args.listen, args.listen), flush=True)
    print("Ctrl+C to stop.", flush=True)
    try:
        while True:
            conn, peer = srv.accept()
            print("{}  accepted connection from {}:{}".format(time.strftime("%H:%M:%S"), peer[0], peer[1]), flush=True)
            conn.close()
    except KeyboardInterrupt:
        print("\nstopped.")
    finally:
        srv.close()
    return 0


def main():
    p = argparse.ArgumentParser(description="TCP reachability probe (see module docstring).")
    p.add_argument("host", nargs="?", help="IP or hostname of the Deskflow server")
    p.add_argument("port", nargs="?", type=int, default=24800)
    p.add_argument("--timeout", type=float, default=3.0, help="connect timeout in seconds (default 3)")
    p.add_argument("--count", type=int, default=3, help="number of tries per address (default 3)")
    p.add_argument("--prefix", type=int, help="your subnet prefix length (e.g. 24) to print the same-subnet check")
    p.add_argument("--no-banner", action="store_true", help="do not wait for a server banner after connecting")
    p.add_argument("--listen", type=int, metavar="PORT", help="listen mode for the reverse test (use a spare port)")
    args = p.parse_args()
    if args.listen:
        return run_listen(args)
    if not args.host:
        p.error("HOST is required (or use --listen PORT)")
    return run_client(args)


if __name__ == "__main__":
    sys.exit(main())
