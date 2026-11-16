# Connecting two computers across networks

Two computers that are on the same network, and that Deskflow still cannot join, are a common case on company networks.
This page says how Deskflow connects, how to find out what is in the way in a minute, what you can do about it without a
VPN or overlay network (such as NetBird or Tailscale), and what you have to ask IT for when it is not yours to change.

> Everything here that is about a company network, a firewall product, Windows or macOS settings is written from the
> documentation of the systems and from tests on one macOS computer with 127.0.0.1. It has not been run on a real
> company network. The probe script for PowerShell has not been run on Windows. Where something is a guess it says so.

## How Deskflow connects

- The **server** listens on **TCP port 24800** (setting `core/port`) on every IPv4 address of the computer, unless
  `core/interface` names one. It does not listen on IPv6.
- The **client** opens the connection **to** the server, from any port. Only the server needs an inbound port; the client
  needs no inbound port at all.
- Everything is one TCP connection, so a computer that is allowed to connect out and a server that is allowed to be
  reached are all there is to it. By default the connection is encrypted (TLS 1.2 or later) and the client checks the
  fingerprint of the certificate of the server.
- The client gives **one attempt 2 seconds** for the TCP connection, the TLS handshake and the greeting of the server
  together (setting `client/connectTimeout`, 1 to 60 seconds), and tries again a second later, for as long as it runs.

### What Deskflow does not do

- **No relay and no NAT traversal.** Nothing goes through a third computer, no address is found out for you, there is no
  hole punching, no UPnP, no STUN or TURN. If the client cannot open a TCP connection to the address it was given, that
  is the end of it. The note `docs/dev/relay-design-note.md` in the source tree says why, and what it would take.
- **No discovery.** Names are looked up by the system only (DNS, hosts file, mDNS of the system), so `desk.local` works
  only where the system can resolve it. An IP address always works the same.

## Find out what is in the way, in a minute

### With Deskflow

In client mode, type the address of the server in the **Connect to** field and press **Test connection**. Deskflow tries
to open a connection to every address of the field, all at once, three seconds each, and closes it again at once. It
sends no byte of its own protocol. Per address it says one of:

| Result | What it means |
|:--|:--|
| **Open** | A TCP connection can be made. The network and the port are fine. If Deskflow still does not connect: check that the name of this computer is in the layout of the server, the fingerprint and the log. |
| **Timed out** | Nothing answered. A firewall on the server computer, or a network that keeps computers apart, drops the packets. |
| **Refused** | The computer answered, but nothing listens on that port. Deskflow is not running as a server there, it uses another port, or it is the address of another network card. |
| **No route** | The network has no path to that address: another or an isolated network (VLAN, guest Wi-Fi, client isolation), a VPN that takes local traffic, or on macOS 15 or later the permission for the local network is missing. |
| **Name not found** | The name does not resolve. Type the IP address. |
| **Blocked on this computer** / **Reset** | A firewall or security product, here or on the way, refused or cut the connection. |

The button also has a line, **Copy summary**, that you can paste into a message to IT, for example
`Deskflow connection test 2026-11-16 22:40 (macOS 15): 10.0.0.5:24800 timed out (no answer in 3000 ms); 100.64.1.7:24800 open in 12 ms`.

In server mode, when the computer has more than one IPv4 address (a LAN, a VPN, a Docker or virtual machine network),
the address beside **Suggested IP** links to a list that says which network interface each address is on and what it is
for: **LAN**, **VPN / overlay**, **virtual**, **link-local**. The client has to be given the address on the network both
computers share, usually the LAN one.

### With the log

Raise the log level to Debug if you want to see the address that was tried (`Preferences > Logs`). A client that cannot
connect logs, next to the lines it always logged, one line that says what is wrong and what is likely the cause. These are
the lines, with `10.0.0.5:24800` standing for the server:

| The client logs | What it means and what to do |
|:--|:--|
| `cannot reach the server 10.0.0.5:24800: the connection timed out (no answer within 2 s) while connecting (TCP). A firewall on the server computer or a network that keeps computers from talking to each other drops it silently.` On macOS 15 or later it adds `Deskflow also needs permission under System Settings > Privacy & Security > Local Network`. | Nothing came back for the first packet. Server firewall (allow the port there), isolation (IT), wrong subnet, server computer asleep. |
| `the TCP connection to the server 10.0.0.5:24800 was made, but it timed out within 2 s while the secure handshake (TLS) was running.` | TCP works, the handshake does not finish: a different TLS setting on the two computers, a security product that looks into the traffic, or a slow network (raise `client/connectTimeout`). |
| `the connection to the server 10.0.0.5:24800 was made, but it timed out within 2 s while waiting for the server's greeting.` | Something took the connection that does not greet like Deskflow, or the network is very slow. |
| `the TCP connection to the server 10.0.0.5:24800 was made, but the secure handshake (TLS) failed.` | The two computers disagree about TLS, or a product on the way breaks it. The server log has the reason. |
| `the server 10.0.0.5:24800 refused the connection: nothing is listening on that address and port.` | The server is not running, uses another port, or the address is the one of another network card. |
| `no route to the server 10.0.0.5:24800: the network has no path to that address (No route to host).` | See *No route* above. On macOS see [the local network permission](#macos-the-local-network-permission). |
| `the connection to the server 10.0.0.5:24800 was reset while connecting (…)` | A firewall or security product, or another program on that port, closed it. |
| `this computer is not allowed to connect to the server 10.0.0.5:24800 (…)` | A firewall or security product on this computer. |
| `cannot find the server desk:24800: its name could not be resolved (…)` | Type the IP address. |
| `the server 10.0.0.5:24800 presented a certificate fingerprint this computer does not trust.` | Approve the fingerprint when asked. If it changed, the certificate of the server was replaced or something sits in between. |
| `the server 10.0.0.5:24800 answered, but it is not a Deskflow server (it greeted with '…').` | Another program uses the port, or a proxy sits in between. |
| `the server 10.0.0.5:24800 speaks a protocol version this computer cannot use (…)` | Update the older Deskflow. |
| `the server 10.0.0.5:24800 does not know this computer's name.` | Add the exact name of this computer, or an alias, as a screen on the server. |
| `a computer with this name is already connected to the server 10.0.0.5:24800.` | Two computers with one name, or the old connection is still closing (about 10 seconds). |

The server has its own lines, and the ones it always had: when a client tries and the server log shows **no**
`accepted client connection` at all, the packets do not reach the server program: firewall, isolation or a wrong address.

### With the command line

`SERVER_IP` is the IP address of the computer that runs the server. Run these **on the client** while the server runs.

| System | Command |
|:--|:--|
| macOS | `nc -vz -G 3 SERVER_IP 24800` |
| Linux | `nc -vz -w 3 SERVER_IP 24800` |
| Windows (PowerShell) | `Test-NetConnection SERVER_IP -Port 24800 -InformationLevel Detailed` |

Which computer listens, on the server: macOS `lsof -nP -iTCP:24800 -sTCP:LISTEN`, Linux `ss -ltnp 'sport = :24800'`,
Windows `Get-NetTCPConnection -LocalPort 24800 -State Listen`. An address where it listens that is not the one the client
uses is a reason for *refused*.

Reading what you get:

| Answer | Meaning |
|:--|:--|
| open, succeeded | The network path and the port are fine. The problem is in Deskflow's settings: see the log. |
| timeout | A silent drop: server firewall or network isolation. |
| refused | The server computer answered: wrong address or port, or the server is not running. |
| no route to host | No path: other network, isolation, or the local network permission of macOS. |

### The probe scripts

Two small scripts, `tools/tcp_probe.py` and `tools/tcp_probe.ps1` in the source tree of Deskflow, do the same and a little more. They make an ordinary TCP connection, send nothing
and close it. A Deskflow server logs them as a connection that went away, which is harmless.

```sh
python3 tools/tcp_probe.py SERVER_IP 24800 --count 3 --prefix 24     # macOS, Linux (Python 3.6 or later, no packages)
py tools\tcp_probe.py SERVER_IP 24800                                # Windows, Python launcher
powershell -ExecutionPolicy Bypass -File tools\tcp_probe.ps1 -Target SERVER_IP      # Windows without Python
```

`tcp_probe.py` was run on macOS against a listener (open) and a closed port (refused), both on 127.0.0.1.
`tcp_probe.ps1` has **not been run on Windows**; if it misbehaves use the Python one or `Test-NetConnection`.
`--prefix 24` says whether the target is on the same /24 network as the address this computer would use.

Both directions: a firewall can be open one way only. On the computer that is **not** the server run
`python3 tools/tcp_probe.py --listen 24899` (a spare port, not 24800), and on the other one
`python3 tools/tcp_probe.py THAT_IP 24899`. The listener may make the system ask about the firewall for `python`.

## What you can do without a VPN or an overlay network

The most likely causes are (in the order a typical office network makes them likely, a guess and not a measurement):
the firewall of the server computer, a network that keeps computers apart, the permission for the local network on macOS,
and an address of the wrong network card. Go down this list.

### 1. Let the port in on the server computer

Only the **server** needs it, only from the local network.

- **Windows**, in a PowerShell that was started as administrator (change the path to where Deskflow is installed):
  ```powershell
  New-NetFirewallRule -DisplayName "Deskflow server (LAN)" -Direction Inbound -Action Allow -Protocol TCP -LocalPort 24800 `
      -Program "C:\Program Files\Deskflow\deskflow-core.exe" -Profile Private,Domain -RemoteAddress LocalSubnet
  ```
  When the network is called *Public*, the rule above does not apply to it. Changing the kind of a network on a company
  computer may be against the rules: ask IT. A company can also switch off local firewall rules by policy, then this does not
  work either.
- **macOS** (server): System Settings > Network > Firewall > Options, and allow incoming connections for Deskflow.
- **Linux**: `sudo ufw allow from 192.168.1.0/24 to any port 24800 proto tcp`, or with firewalld
  `sudo firewall-cmd --permanent --add-rich-rule='rule family=ipv4 source address=192.168.1.0/24 port port=24800 protocol=tcp accept' && sudo firewall-cmd --reload`
  (change the network to yours).

### 2. Use the right address

A computer with Wi-Fi and a cable, a VPN, Docker or virtual machines has several addresses. The server's window says which
(see above). A client that is given the address of the wrong card gets *timed out*, *refused* or *no route*. Then give the
client the address of the network both computers share.

### 3. Swap the roles

Only the server needs an inbound port, and only the client needs the permission of macOS for the local network. If one
computer cannot be reached by the other but can reach it, make **that** one the **server**. Set up the other computer as the
client with the address of the first. The screen name must be the same on both sides, and the fingerprints are trusted again
on the new pair. This does not help when the network keeps the two computers apart in both directions.

### 4. Write more than one address

`client/remoteHost` (the **Connect to** field) takes a list, `192.168.1.5,100.64.1.7`: the LAN address first and, if you
have a VPN you are allowed to use, its address after. In the file (`Deskflow.conf`) the line is
`remoteHost=192.168.1.5,100.64.1.7`; quotes are not needed. Exactly how the list is used (the order, the fall back after a
refusal and after a silent address, and the new round are tested with real sockets on 127.0.0.1; the rest is read from the
code of the client):

- The addresses are tried **in the order of the list, one at a time**. A name that has several addresses has each tried
  before the next entry of the list.
- A failed address is followed by the next after a **pause of 1 second** (5 s to 5 min later, when you switched on
  `client/dynamicConnectionInterval`, after many rounds of failure).
- An address that answers *refused* costs only that second. One that does **not answer** costs the connect time limit
  as well: with the default that is 2 s and 1 s pause, **3 s per address**; with `client/connectTimeout` = 1, 2 s.
- The same holds for a failure that is not a silent address: a TLS or greeting failure ends as the time limit does and then
  the next address is tried. A server that answers *but sends the client away* (unknown name, name in use) is not
  a failure of the address: the client stays with that address and tries it again.
- After the last address failed, the round starts again with the first. When the client has connected, a connection that is
  lost later is retried starting again with the first address of the list.
- Each entry can have its own port: `host:port`, `[ipv6]:port`.

### 5. Another port

Both sides set the same `core/port`. It only helps when the network filters that port number. The rule of the firewall of
the server has to name the new port. Ports like 443 that look like web traffic can be handled by a device that looks into
it; prefer one that IT gave you.

### 6. A tunnel with SSH

When port 22 reaches the server computer and the sshd of it allows forwarding (macOS: *Remote Login*, Windows: OpenSSH
Server, Linux: `sudo systemctl enable --now ssh`), the connection of Deskflow can go inside it. This is something IT has
to be fine with; it is not a way past the rules. Forwarding that is switched off by the server (`AllowTcpForwarding no`)
does not work.

On the **client**, the tunnel (leave the terminal open):

```sh
ssh -N -o ExitOnForwardFailure=yes -o ServerAliveInterval=5 -o ServerAliveCountMax=3 -L 24800:127.0.0.1:24800 user@SERVER_IP
```

Then give the Deskflow client the address `127.0.0.1`. The TLS fingerprint check is on the certificate, so it does not matter
that the address now looks like this computer. When only the other direction is allowed, on the **server**:

```sh
ssh -N -o ExitOnForwardFailure=yes -o ServerAliveInterval=5 -o ServerAliveCountMax=3 -R 24800:127.0.0.1:24800 user@CLIENT_IP
```

and give the client `127.0.0.1` the same way. It is a TCP connection inside a TCP connection: after a lost packet the
mouse can stutter, and when the tunnel dies Deskflow notices after about 9 seconds and tries again.

### 7. A cable between the two computers

A private link of its own does not touch the company network at all.

- Mac to Mac: a Thunderbolt cable; the *Thunderbolt Bridge* service gets a 169.254.x.x address on both ends by itself.
- Other pairs: a USB Ethernet adapter on each side and one cable, and a fixed address on each side, for example
  `192.168.77.1` and `192.168.77.2` with the mask `255.255.255.252` and **no gateway**. Give the client the address of the server.
- Windows treats a new adapter as a *Public* network and blocks the inbound port: the rule in section 1 has to cover that
  profile. Do **not** switch on *Internet Sharing* or *Internet Connection Sharing*, they can disturb the company network.
- A USB network adapter can be forbidden on a company computer: ask IT.

### What is not a good idea

- A phone hotspot, Wi-Fi Direct or Bluetooth network for a company computer: it takes the computer off the company network,
  which many companies do not allow. Ask IT.
- Another VPN or overlay network on a company computer without asking: it is the same kind of software as the one you wanted
  to avoid.
- Any service that is made to pass connections through a server on the Internet: your keys and clipboard would go through
  a third party.

## macOS: the local network permission

Since macOS 15 an application needs the permission **Local Network** to connect to a computer on the local network (an
address on a network card, not on a VPN or overlay network). When it is missing, the connect fails with **No route to host**
although the server is fine. Only the connecting side needs it: a **server** that is connected to does not.

- Check: System Settings > Privacy & Security > Local Network, and switch Deskflow on. The permission is not shown for an
  application that has not asked for it yet.
- A sign of this cause: `nc -vz SERVER_IP 24800` in the Terminal works while the Deskflow client says *No route to host*.
  Programs started from the Terminal are allowed by the permission of the Terminal.
- The prompt can be missing for a build of Deskflow that is not signed with a developer certificate. These are known
  problems of the project: [#9451](https://github.com/deskflow/deskflow/issues/9451),
  [#9954](https://github.com/deskflow/deskflow/issues/9954). Ways around it: start the client with `deskflow-core client`
  from the Terminal, or make the Mac the **server** (section 3).
- A VPN or overlay network is not the local network, so it needs no permission. That is one reason it works when the LAN
  address does not.
- IT can allow whole networks for every program of the managed Macs; see Apple's technical note TN3179 (not tested here).

## Asking IT

Only IT can change a network that keeps computers apart, a rule of the firewall that comes from a policy, or the settings of a
VPN agent. A request that is short and exact is answered sooner. Fill in the parts in `< >`.

### English

> **Subject:** Allow same-segment TCP between two managed PCs for Deskflow (keyboard/mouse sharing)
>
> We want to share one keyboard and mouse between two company PCs with Deskflow. Both are on the corporate network but
> cannot reach each other directly.
> - Server PC: `<host name / MAC / IP>`; client PC: `<host name / MAC / IP>`
> - Direction: client to server. Only the server PC needs **inbound TCP 24800**; the client needs no inbound port.
> - Scope: peer to peer inside the same VLAN/subnet between these two PCs only. No Internet access is needed.
> - Requests: (1) the two PCs can talk in the same VLAN/subnet (no Wi-Fi client isolation or per-device VLAN), or an exception
>   for the two MAC addresses; (2) a host firewall rule (GPO/Intune) that allows inbound TCP 24800 for `deskflow-core` on the
>   server, for the local subnet, Domain/Private profiles; (3) a DHCP reservation for the server; (4) a local-subnet
>   exception in the VPN/ZTNA agent, if there is one.
> - Notes: the traffic is TLS 1.2 or later with certificate fingerprint pinning. No cloud service or relay is used. Today it
>   only works through an overlay VPN; we would like to connect directly on the corporate LAN instead.
> - Test result of the client (paste the line from **Test connection > Copy summary**): `<line>`

### 한국어

> **제목:** 키보드·마우스 공유(Deskflow)를 위한 업무용 PC 2대 간 동일 구간 TCP 허용 요청
>
> 업무용 PC 2대 사이에서 키보드/마우스 공유 프로그램(Deskflow)을 쓰려고 합니다. 두 PC는 같은 사내망에 연결되어 있지만
> 서로 직접 접속이 되지 않아 요청드립니다.
> - 서버 PC: `<호스트명 / MAC / IP>`, 클라이언트 PC: `<호스트명 / MAC / IP>`
> - 연결 방향: 클라이언트 → 서버. 서버 PC에서만 **TCP 24800 인바운드**가 필요하고, 클라이언트 PC는 인바운드가 필요 없습니다.
> - 범위: 위 두 PC 사이의 동일 VLAN/서브넷 내 피어 간 통신만. 인터넷 접속은 필요 없습니다.
> - 요청: (1) 두 PC가 같은 VLAN/서브넷에서 서로 통신할 수 있게(Wi-Fi 클라이언트 격리, 장치별 VLAN 제외) 또는 두 MAC 주소에
>   한한 예외, (2) 서버 PC의 호스트 방화벽(GPO/Intune)에 `deskflow-core`의 인바운드 TCP 24800 허용(로컬 서브넷 한정,
>   도메인/프라이빗 프로필), (3) 서버 PC IP 고정(DHCP 예약), (4) VPN/ZTNA 에이전트가 있다면 로컬 서브넷 예외.
> - 참고: 트래픽은 TLS 1.2 이상으로 암호화되고 인증서 지문 고정을 사용하며, 클라우드 서비스나 릴레이를 쓰지 않습니다.
>   현재는 오버레이 VPN을 거쳐야만 연결되는데, 사내망에서 직접 연결하고 싶습니다.
> - 클라이언트의 테스트 결과(**연결 테스트 > 요약 복사**의 한 줄을 붙여 넣으세요): `<한 줄>`

## See also

- [Configuration](Configuration.md): `client/connectTimeout`, `client/remoteHost`, `core/port`, `core/interface`
- `docs/dev/relay-design-note.md` in the source tree: why Deskflow has no relay, and what one would take
