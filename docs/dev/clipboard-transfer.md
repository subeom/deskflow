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
  directly. The event fires on any drain of the buffer, also one that an input packet caused, so up to
  about two chunks (64 KiB) of clipboard data can be in flight at a time, and an input or keepalive packet
  waits behind at most that, not behind the whole clipboard;
- while a paced transfer runs, the kernel send buffer is limited (`TCP_NOTSENT_LOWAT`) so that the
  backlog lives in the user space queue, where input overtakes it, and not in the kernel, where it
  cannot. Without this the pacing alone did not help in the experiment, and the limit alone did not
  either;
- the total transfer time does not change: the same bytes go through the same link.

Measured with the real classes (`ClipboardSender`, `TCPSocket`, `PacketStreamFilter`, the real
reassembly) on loopback through a shaped link (7 ms one way), a 3.1 MB bitmap sent for both ids
(5.9 MiB on the wire), mouse at 125 Hz and a `CALV` every second in the same stream:

| link | legacy: mouse stall / `CALV` gap | paced: mouse stall / `CALV` gap | transfer time, legacy / paced |
|---|---|---|---|
| loopback | 3 ms / 1.0 s | 5 ms / 1.0 s | 12 ms / 17 ms |
| 100 Mbit/s | 531 ms / 1.0 s | 93 ms / 1.0 s | 541 ms / 541 ms |
| 10 Mbit/s | 5.35 s / 5.36 s | 0.76 s / 1.49 s | 5.36 s / 5.36 s |
| 4 Mbit/s | 12.7 s / 12.7 s (disconnect) | 1.8 s / 2.6 s | 12.71 s / 12.72 s |

With the clipboard sent once for a Windows or macOS peer (see below) the transfer takes half as long and has
half the bytes: 280 ms at 100 Mbit/s, 2.69 s at 10 Mbit/s and 6.35 s at 4 Mbit/s, with the same stalls.

The stall is now bounded by what the link and the kernel hold ahead of the next input message, not by
the clipboard size, and the transfer takes as long as before. On a fast link the per chunk hand-over
costs a few milliseconds (12 to 17 ms on loopback).

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
An unknown platform keeps today's behaviour. The grab for id 1 is left out too: Windows empties its one
clipboard when it is told to give it up (whatever the id), so a grab of the selection that no data follows
(a highlight on an X11 computer) would empty it for nothing.

The reverse direction needs the receiver to keep the selection working for X11 computers: a client leaves
the grab and the data of id 1 out only when it is on Windows or macOS itself and the server is too and
advertised the single id capability. Such a server takes the grab and the data of id 0 as those of id 1 too
(`ClientProxy1_6`, in the order grab, grab, data, data, so the grab of an id is seen before its data), so a
Linux computer entered later still gets its selection. `Client::sendClipboard` returns before reading the local
clipboard for a skipped id, which also saves the second pasteboard read on the Mac.

Known limitation (pre-existing, not changed here): the rule above applies to **peers** only
(`exchangesId` in `ClientProxy1_9` and `ServerProxy1_9`). The server's own primary computer
(`PrimaryClient`) still gets `grabClipboard` and `setClipboard` for id 1, whatever its platform. With a
Windows primary, the selection grab (id 1) of an X11 client empties the primary's only clipboard, and with
a Windows or macOS primary, an X11 client that owns different data for id 0 and id 1 leaves the primary
with the id 1 data, because the platform code ignores the id and the last write wins. This only matters
with Linux/X11 clients; Windows and macOS computers among themselves are not affected. A possible
follow-up is to skip id 1 for a primary that is not on X11 in `handleClipboardGrabbed`, `switchComputer`
and `onClipboardChanged`, with the same `exchangesId` rule.

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
current data. A primary that does not share stays the owner of the empty clipboards the server starts
with, and those are not sent to the computers it enters, because a computer that is sent an empty
clipboard empties its own.

## Rewrites with the same content, and loops

Another program can write the clipboard of a computer with what is already on it: Universal Clipboard
and Handoff do it on the Mac with what Deskflow just wrote to the other Mac, a clipboard manager with
what it saw. The computer sees a change and grabs the clipboard again, and every grab used to clear what
the server knew, tell every computer to give up its clipboard, and send the data again at the next
entry. With two Macs this feeds a loop: each transfer is written to a pasteboard, the other Mac rewrites
its own, and so on. Two things now stop it, without any change on the wire:

- **The primary computer grabs again with what is already shared.** The server only looks at this when
  the cursor is on another computer, or within two seconds after Deskflow wrote the clipboard of the
  primary (`setClipboardEchoWindow`). It reads the clipboard the way it does to share it (text only in
  text-only mode) and compares it with the content it passed on last. If it is the same, the grab is
  ignored: nothing is cleared, announced or sent, and the server logs `clipboard 0 skipped for "..."`
  at INFO. Content that is not the same is a copy and is taken as before. Ownership stays where it was,
  because no computer can tell the difference; it moves with the next change. A grab by another
  computer carries no data yet and is always taken: the user may have copied the same text there on
  purpose to take the clipboard, and its data is then passed on the same way, with the grab telling
  the other computers that they no longer own it.
- **A Mac is not sent what it already has.** A Mac does nothing when it is told that another computer
  took the clipboard, so what it was sent last is still on it unless it copied something itself. The
  proxy keeps the size and a hash of what it last sent to a client that reported `macOS`, and does not
  send the same content again (it logs `identical content (it still has it)` at INFO). A grab by that
  computer, or a transfer that was dropped before it ended, forgets it. Windows empties its clipboard
  when it is told to give it up, so it is sent the data again; so is every platform that is not known.

What this does not do: a program on a Windows client that rewrites what Deskflow wrote still makes the
client grab, and the grab reaches the other computers before its data is known (it is sent when the
client leaves or at once if it is not active), so the other computers are told to give up their
clipboards. The Mac computers keep theirs and are not sent it again; the Windows computers are.

## Reading the pasteboard on a Mac

The server reads the clipboard of its own computer every time the cursor leaves it, and a Mac pasteboard
that holds an image offers a bitmap flavour that the system makes from the PNG on request: a 1280x720
screenshot of 21 KB becomes 3.7 MB, one of 2560x1440 becomes 14.7 MB, in several milliseconds on the main
thread, and it is dropped for its size after it was read when it is over the limit. Two things make this
cheaper. Neither changes what is shared or what is on the wire:

- **A cache per change.** What was read of each format is kept until another program changes the
  pasteboard (`OSXClipboard`, `ClipboardReadCache`), so the next switch of computers does not read and
  convert it again. `PasteboardSynchronize` tells what changed since its last call and is cheap, so every
  read asks it first; a change it sees is kept for the 1 s poll that detects grabs (so no grab is lost),
  and Deskflow's own writes empty the cache. What is read is cached up to 8 MiB, and a read that failed is
  not cached.
- **An image that cannot be shared is not converted.** The caller says how large a clipboard may be
  (`IClipboard::sizeLimit()`, set by `Server` and `Client` on the clipboard they read into, which is the
  `CLSZ` limit or 10 KB in text-only mode). Before converting, the Mac reads the PNG or TIFF of the item
  (a few dozen KB), takes its size from the header (`pngImageSize`, `tiffImageSize`), and if a bitmap of
  that many pixels at 4 bytes (a lower bound, so an image is never skipped that would have fit) with the
  12 bytes of the message is over the limit, it returns something as large as the limit instead. The clipboard
  is then dropped for its size exactly as when the image is read, with the same message. Without a
  PNG/TIFF header, or with a limit above 64 MiB, the image is converted as before. Text-only mode never
  reads the image at all (`wants()`).

What is tested: the PNG and TIFF header parsing, the size arithmetic including overflow, the cache, and that
the `Server` passes the limit to the platform. What is not: `OSXClipboard` itself needs a live pasteboard, which
the tests must not touch, so its use of the cache, `PasteboardSynchronize` and the flavours was reviewed but
not run here.

Not done: moving the pasteboard reads to a worker thread. The thread safety of the Carbon pasteboard calls
(and of the `OSXClipboard` cache) cannot be verified here, and a read that blocks (a promised flavour, an
iCloud clipboard, a privacy prompt) would then need a timeout and a way to give up. It is the remaining
cure for a main thread that is stalled for more than the 9 s keepalive by a slow read, and is left as a
follow-up.

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
