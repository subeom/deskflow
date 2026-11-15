# Clipboard transfer: design note

Clipboard sharing used to make a Deskflow link slow and, on a weak link, drop the connection. This
note records what the optimisations in this fork do, why, and what was deliberately left out. The
wire details are in [protocol_reference.md](protocol_reference.md#paced-clipboard-stream).

## The problem

When the cursor enters a computer the server pushed the whole marshalled clipboard (every format, up
to the size limit, 3 MiB by default) for clipboard id 0 **and** id 1 in 512 KiB chunks into the same
TCP stream as mouse, key and keepalive messages, with no flow control:

- mouse and keys written afterwards waited for the whole transfer (head of line blocking): the stall
  equals the transfer time;
- only `CALV` counted as proof of life for the client (9 s). On a slow link the `CALV` waited behind
  the clipboard and the client declared the server dead;
- every grab cleared the cached content, so a clipboard rewritten with the same content (Handoff,
  Universal Clipboard, a clipboard manager) was sent again;
- macOS read and converted every pasteboard format, including ones over the limit, on every change.

A loopback experiment with a shaped link measured the head of line blocking directly (a 3 MiB
clipboard in both ids is 5.9 MiB on the wire): the mouse stalled for 551 ms at 100 Mbit/s, 5.2 s at
10 Mbit/s and 12.5 s at 4 Mbit/s, and the 12.5 s gap in `CALV` is what disconnects.

## The "separate channel" is a logical channel

A second TCP connection (its own listener, TLS, authentication, firewall rule and reconnect logic)
was judged too large for what it buys. What the user needs is that clipboard data never delays input
and keepalives, and a **logical low priority channel inside the existing stream** gives that:

- the sender cuts a clipboard into 32 KiB chunks and writes **one chunk each time the stream reports
  that its output has drained** (`StreamOutputFlushed`). Input and keepalive messages are written
  directly, so they are never queued behind more than a chunk of clipboard data;
- while a paced transfer runs, the kernel send buffer is limited (`TCP_NOTSENT_LOWAT`) so that the
  backlog lives in the user space queue, where input overtakes it, and not in the kernel, where it
  cannot. Without this the pacing alone did not help in the experiment, and the limit alone did not
  either;
- the total transfer time does not change: the same bytes go through the same link.

Measured with the real classes (`ClipboardSender`, `TCPSocket`, `PacketStreamFilter`, the real
reassembly) on loopback through a shaped link (7 ms one way), a 3.1 MB bitmap sent for both ids
(5.9 MiB on the wire), mouse at 125 Hz and a `CALV` every second in the same stream:

| link | legacy: mouse stall / `CALV` gap | paced: mouse stall / `CALV` gap | transfer time |
|---|---|---|---|
| loopback | 3 ms / 1.0 s | 0.1 ms / 1.0 s | 13 ms, 16 ms |
| 100 Mbit/s | 533 ms / 1.0 s | 92 ms / 1.0 s | 543 ms, 541 ms |
| 10 Mbit/s | 5.4 s / 5.4 s | 0.76 s / 1.6 s | 5.42 s, 5.42 s |
| 4 Mbit/s | 12.7 s / 12.7 s (disconnect) | 1.8 s / 2.6 s | 12.70 s, 12.77 s |

The stall is now bounded by what the link and the kernel hold ahead of the next input message, not by
the clipboard size, and the transfer takes as long as before. On a fast link the per chunk hand-over
costs a few milliseconds (13 to 16 ms on loopback).

Because the receiver only ever sees the same `start`/`data`/`end` messages with different chunk
sizes, a receiver needs no change to take a paced stream. That is what lets a Windows build that
predates this work (it speaks protocol 1.9) benefit when the server sends to it. The receiving side
gets one more change, below.

### Receiver proof of life

A client whose server advertised the capability counts **any received data** as proof of life
(`ServerProxy::handleData`), not only `CALV`, so a long inbound clipboard cannot trigger "server is
dead". The server side already did this for data from clients. A peer that did not advertise the
capability is handled exactly as before.

### Superseding, aborting and tearing down

- a new transfer for a clipboard id replaces a running or waiting transfer of that id; the old one
  is abandoned **without an end chunk**. The receiver (`ClipboardChunk::assemble`) restarts at every
  start chunk and drops its partial data, so no error is raised and the next transfer is accepted;
- a clipboard grab for an id also drops the running transfer of that id on the sender: the grab says
  its data is stale, and the stale data must not arrive after the grab;
- when the connection goes away the sender is destroyed with its proxy; queued data is freed and the
  handlers are removed, so a late `StreamOutputFlushed` has nothing to do;
- leaving or entering a screen does not touch a running transfer: it carries on in the background and
  the receiver applies it when it ends.

## Gating

Every change on the wire is gated so that links that do not negotiate it stay byte identical:

| Change | Gate | Unchanged for |
|---|---|---|
| paced chunks (32 KiB, one per drain) | the peer's `DIPC`/`CIPC` was received (protocol 1.9 handshake) | protocol 1.8 and older, and 1.9 before the peer's capabilities are known: the 512 KiB legacy stream |
| any data is proof of life (client) | the server's `DIPC` carries `kClipboardCapabilityPaced` (bit 8) | servers without the bit |
| no data for id 1, server to client | the client reported `Windows` or `macOS` in `CIPC` | other platforms, unknown platform, and links without the 1.9 handshake |
| no data for id 1, client to server | the server reported `Windows` or `macOS` in `DIPC` and advertised `kClipboardCapabilitySingleId` (bit 9) | servers without the bit, other platforms |

Regression tests pin the legacy bytes against the original `StreamChunker` path and feed the paced
stream to the real receivers (`ClipboardChunk::assemble` through `ClientProxy1_6` and `ServerProxy`).

## One clipboard id for Windows and macOS

The selection (clipboard id 1) is an X11 concept. The platform code of Windows and macOS ignores the id
(`OSXComputer::setClipboard`, `MSWindowsComputer::setClipboard` take an unnamed id), and both emit a grab
for each id for the same clipboard. So the server sent every clipboard twice, and the receiver wrote it
twice (on macOS each write is a `PasteboardClear` plus the flavours, which also bumps the pasteboard
generation that Universal Clipboard advertises).

Data is now left out for id 1 when the peer reported `Windows` or `macOS` in the 1.9 handshake. This
halves the bytes, the time and the pasteboard writes in the direction where the sender knows the peer.
An unknown platform keeps today's behaviour. The grab message for id 1 is still sent (a few bytes), so a
peer that predates this keeps the same ownership bookkeeping; only the data is saved.

The reverse direction needs the receiver to keep the selection working for X11 computers: a client leaves
the data of id 1 out only toward a server that advertised the single id capability, and such a server
stores the data of id 0 from a Windows or macOS client as the selection too (`ClientProxy1_6`), so a Linux
computer entered later still gets its selection. `Client::sendClipboard` returns before reading the local
clipboard for a skipped id, which also saves the second pasteboard read on the Mac.

## Leaving one computer out

A computer can be set to not share its clipboard (`computer_<name>/shareClipboard`, **Share clipboard with
this computer** in the computer settings dialog; default on, and a configuration that has no key for it is
on). The setting is a per computer `CLPS=0` screen option in the server's `Config`. The server leaves such a
computer out of every clipboard path: a grab by it is ignored and not announced to the others, a clipboard
it says it changed is not read, it is not told about the grabs of the others, and it is not sent a
clipboard when the cursor enters it. It is also sent `CLPS=0` (after the global `CLPS`, because a client
takes the last value), so it stops reading and sending its own clipboard. Nothing changes for a computer
that shares: its options are exactly the ones it got before. When the primary leaves for a computer that
does not share, the primary's clipboard is still read, so that the next computer that does share gets
current data.

## What an unchanged Windows build (v1.27.0.45) gets

It speaks protocol 1.9 and sends `CIPC` with `Windows` as its platform, so the server paces what it sends to
it and sends the data of id 0 only (server to Windows needs no Windows rebuild). What Windows sends to the server stays the legacy stream and Windows does
not count data as proof of life: that direction needs a Windows rebuild.

## Not done, and why

- **A second TCP connection**: too large (listener, TLS, auth, firewall, reconnect) and it buys
  nothing over the logical channel, which already removes the head of line blocking.
- **Lazy pull (paste time fetch)**: needs a new message and delayed rendering on each platform
  (`NSPasteboardItemDataProvider`, `WM_RENDERFORMAT`), and a paste would wait for the network inside
  the platform's synchronous call. The risk of a stalled paste is high and cannot be verified here.
- **PNG as a clipboard format** and **compression**: they cut the bytes, not the stall, and PNG needs a
  new format id and converters on every platform. They can follow independently.
- **Moving pasteboard reads to a worker thread**: the thread safety of the pasteboard APIs cannot be
  verified here. Documented as a follow-up.
