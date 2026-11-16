# A relay for Deskflow: why not now, and what it would take

A short note, not a plan. Status: **not built, not planned.**

## The problem

Deskflow connects one way: the client opens a TCP connection to `server:24800`. When the two computers cannot do that
(a firewall in front of the server, a network that keeps computers apart, two different networks), nothing in Deskflow can
help: there is no relay and no NAT traversal. People then put an overlay network (WireGuard based, such as NetBird or
Tailscale) under Deskflow. A relay in Deskflow would make that unnecessary for some of them.

## Why not now

- Most "cannot connect" cases on one company network are a closed port, an isolated network or a missing permission. A relay
  does not solve them: the relay needs an outbound connection to an Internet address that the same company network may
  not allow, and it moves the input of a person through a third computer.
- It is a **service**, not a feature: it has to be run, paid for, updated, defended and trusted by someone, for ever.
  The project has no such infrastructure and no owner for it.
- The in-application diagnostics (the connection test, the classified log lines, the list of the server addresses) answer the
  question people have first: *what is in the way*. A relay is a project of its own.

## What it would take

1. **Rendezvous.** The server and the client register under a name at a server of the project (or of the user), outbound
   over TCP 443 so that it passes proxies, and learn how to reach each other. Direct first (a TCP connection over a LAN or
   with simultaneous open or UDP hole punching), then the relay.
2. **A relay that never terminates TLS.** The relay copies bytes between two outbound connections. The TLS session of
   Deskflow stays **end to end** between the two computers, so the relay sees neither keys nor clipboard, only sizes and
   times. This holds only if the client keeps checking the certificate of the server by fingerprint exactly as today and the
   relay can never present its own certificate.
3. **Authentication and pinning.** The existing trusted-fingerprint databases are the identity of both sides. The rendezvous
   has to tie a registered name to a fingerprint, so that a stranger cannot register as your server, and a relay that is
   hostile or hacked can at worst refuse to carry traffic. Pairing is a new flow in the user interface.
4. **Protocol work.** The existing wire protocol is not to change. The relay is under it: a new transport next to
   `TCPSocket`, new settings (a relay address, a pairing code), a new section of the interface and of the documents.
5. **Latency.** Every input event goes to the relay and back, through a data centre: often 20 to 100 ms or more on top of
   the network. A direct path, such as a VPN that managed a direct connection, is faster. The 9 second keep-alive and the
   2 second connect limit of the client need a new look.
6. **Hosting.** A relay has to be reachable from everywhere and carry the traffic of every pair: bandwidth (clipboard and
   files are not small), abuse protection, rate limits, logs that must not hold secrets, and a way to run one's own.
7. **Policy risk on company networks.** Input and clipboard of a company computer that run through a relay on the Internet
   are what a security team forbids first, and a rendezvous over 443 can look like a tunnel that avoids the controls. A
   relay that works in a company is one that the company runs itself. It does not fix what is stopped on purpose.
8. **Cost of being wrong.** The project would carry the security of a network service: reviews, a report path, updates.

## What exists instead

- An overlay network (NetBird, Tailscale, a company VPN) under Deskflow, which is what people do now.
- An SSH tunnel to the server, when SSH is allowed (see `docs/connecting-across-networks.md`).
- A list of addresses in `client/remoteHost` that falls back from the LAN address to the overlay address.
- The roles swapped, so that the computer that can be reached is the server.

## If it is ever started

Start from a written threat model, keep the end-to-end TLS and the fingerprint check as the only trust, make the relay
optional and self-hostable, and build the rendezvous with direct connection first. Do not put a relay in the default path.
