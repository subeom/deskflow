# Connect through a Cloudflare Tunnel: design note

A client can reach its server through a Cloudflare Tunnel: `cloudflared access tcp` listens on this computer, and the
client dials that local port instead of the address of the server. Deskflow runs `cloudflared` itself, so nobody has to
keep a terminal open. The feature is **off by default**; with it off nothing of the program changes (see
[Off means off](#off-means-off)).

What is **not** part of it: the server side. The tunnel at the server (a Named Tunnel that exposes the server's port
behind a Cloudflare Access application with a Service Auth policy), the DNS record, the Access application and the Service
Token are created once by hand in Cloudflare. Deskflow does not create, change or look at any of them, and does not
contact the Cloudflare API. `cloudflared` is a separate program (Apache-2.0) that Deskflow starts; it is not bundled, not
downloaded and not updated by Deskflow.

```
Deskflow client -> 127.0.0.1:<port> -> cloudflared access tcp -> mbp.example.com -> tunnel at the server -> Deskflow server
                   (this computer)      (started by Deskflow)      (Cloudflare Access)
```

Deskflow's own TLS and the fingerprint check stay end to end and unchanged: the tunnel carries the TLS connection. That is
why the tunnel is **refused while TLS is off**.

## Settings

| Key | Meaning | Default |
| --- | --- | --- |
| `tunnel/enabled` | the client connects through the tunnel | `false` |
| `tunnel/hostname` | the hostname of the Access application, e.g. `mbp.example.com` | empty |
| `tunnel/localPort` | the local port that cloudflared listens on | `24800` |
| `tunnel/cloudflaredPath` | the path of cloudflared; empty: look for it | empty |
| `tunnel/activePort` | written by the program: the port in use, when it is not `tunnel/localPort` | not set |

The saved server address (`client/remoteHost`) and its list of recent hosts are **never** written by the tunnel: switching
the tunnel on and off, or going between a direct address, a VPN address and the tunnel, keeps the address as it was. The
address that the core dials is a pure function (`deskflow::tunnel::effectiveRemoteHost`, used by `ClientApp::parseArgs`):
with the tunnel on and usable it is `127.0.0.1:<port>` and nothing else (the saved list is not tried, so a tunnel that
fails never falls back to a path that bypasses it); otherwise it is the saved address, unchanged.

The **Service Token** (Client ID and Secret) is not a setting. It is in the secure store of the system:

| System | Store | Name | Notes |
| --- | --- | --- | --- |
| Windows | Credential Manager, generic credential, `CRED_PERSIST_LOCAL_MACHINE` | `Deskflow/CloudflareAccess` | readable by the user who stored it |
| macOS | login keychain, generic password (`SecItem`) | service `org.deskflow.deskflow.cloudflare-access`, account = Client ID | the item is created by the program that is running; after an update that changes the signature the system may ask once |
| Linux | memory of the session only | none | not persisted: the user is told and enters it again after a restart |

`DESKFLOW_SECRET_STORE=memory` makes every system use the store in memory. The tests set it; nothing of the tests calls the
real stores.

## Parts

Everything is in `src/lib/gui/tunnel/` and is plain Qt without a window, except where noted; the window code only calls it.

| Part | Does |
| --- | --- |
| `TunnelStateMachine` | the states (Idle, Starting, Listening, Stopping, Backoff, Failed) and what to do next; no clock, no process, no socket of its own |
| `CloudflaredLocator` | finds cloudflared and gives **one canonical absolute path**: the configured path (it must be absolute), then the usual places (Windows `C:\Program Files (x86)\cloudflared\cloudflared.exe` and `C:\Program Files\cloudflared\cloudflared.exe`, macOS `/opt/homebrew/bin` and `/usr/local/bin`), then `PATH`. That exact path is what is started; the token is never handed to a program that was looked up again by a name |
| `CloudflaredLaunch` | the command: `access tcp --hostname <host> --url 127.0.0.1:<port> --loglevel info`; the token only in the environment of the child |
| `CloudflaredLogParser` | what a line of cloudflared means (listener started, bind failed, a login is asked for, a connection could not be opened and why); substring based, so a change in the words makes the tunnel less informative and never wrong |
| `SecretRedactor` | masks the token and the shapes of the headers and variables that carry it, in every line that leaves the supervisor |
| `ISecretStore` and the stores | see above |
| `OutputLineSplitter` | cuts the output of cloudflared into lines (`\n`, `\r`, `\r\n`); a line over 64 KB is dropped as a whole and replaced by one notice line, so a token can never be given in two halves |
| `ITunnelProcess`, `QtTunnelProcess` | one run of cloudflared: no console window and a job that ends it with the program on Windows, the parent death signal on Linux, stdin closed |
| `IPortProber`, `QtPortProber` | can a listener be opened, a free port, is a port open (a connection that is closed again at once, no byte sent) |
| `OrphanRecord`, `IProcessInspector` | the record of the running cloudflared and the way to look at a process by its number |
| `TunnelSupervisor` | runs it all: the checks before the start, the process, the state machine, the restarts, the wake from sleep |
| `TunnelController` | the settings on one side, the supervisor on the other; it is the gate that the core asks (`ITunnelGate`) |
| `ServiceQuit` | the stop of the service core at quit: connects (bounded), sends, flushes, and says what stays running when it cannot |
| `TunnelStartGuard` | the order: the client core starts when the tunnel listens; the tunnel is let go when the core stops |
| `TunnelTestRunner`, `TunnelTestDialog` | "Test connection" through the tunnel |
| `TunnelStatusWidget` (`widgets/`) | the two lines of the main window |
| `TunnelSettingsGroup` (`widgets/`) | the group "Cloudflare Tunnel" in the dialog that configures the client |

### Life of the tunnel

The GUI **owns** the tunnel, also in Windows service mode. The tunnel lives while the GUI process lives; hiding the window
to the tray keeps it.

1. The user starts the client. `CoreProcess::start()` asks `TunnelStartGuard`. If the tunnel is on and does not listen, the
   start is postponed (the process state is Starting) and the tunnel is asked to start. **When the tunnel listens, the guard
   asks the gate again** (`ITunnelGate::validate()`): the tunnel must still be required and the settings must still be usable
   (TLS on, a valid hostname, the same settings as at the start). If not (TLS was switched off or the hostname cleared during the
   wait of up to 8 s, or the tunnel was turned off), the start does not go on: the tunnel is let go, the start counts as failed
   with the reason (`TlsRequired`, `InvalidHostname`, `SettingsChanged`) and the state of the core is set back to Stopped.
2. `TunnelSupervisor::start()` checks, in this order: TLS is on; the hostname is a host name; cloudflared is found; a Service
   Token is stored (this is the only place the Secret is read); the path of cloudflared is absolute; a cloudflared that an
   earlier run left behind is ended (see below); the preferred local port can be listened on, otherwise a free port is chosen and written to `tunnel/activePort`.
   A check that fails ends the start with its reason; nothing is started.
3. cloudflared is started with the token in the environment of the child. The supervisor waits for the local port to
   open (it probes at the latest 300 ms after the start, or when the log says that the listener started), and calls the
   tunnel **listening** only when **all three** hold: the process runs, the port is open, and cloudflared logged that it starts
   its listener (`Start Websocket listener`), for 300 ms. If this does not happen in 8 s the start fails.
   - An open port alone is not enough: a program of this computer that takes the chosen port after the free-port check would
     answer the probe while cloudflared is still starting. The listener line is **necessary but not sufficient** (cloudflared
     logs it just before it binds); it does not prove that the bind worked.
   - If cloudflared says that the address is in use (the line of its forwarding server, or the words of the system on Unix and
     Windows), the tunnel is not listening, also when the state was Listening already: the process is ended and the start goes on
     **once** with the next free port (the log says "Local port N was taken by another program, using port M instead"). A
     second conflict fails the tunnel with the port busy.
   - If the line of cloudflared is never seen (the words changed in a new version), the tunnel is accepted after the port stayed
     open for 3 s with the process running, and the log says "cloudflared did not log its listener line". A tunnel must not stop
     working because a log line was reworded; a program that took the port cannot satisfy 3 s with cloudflared still running,
     because cloudflared stops or says that the address is in use.
   - Whatever listens on the local port, Deskflow's TLS and the fingerprint check remain the real protection (see the security
     notes); this logic only makes the state honest.
4. When the tunnel listens, the guard starts the core. The core reads the address with `effectiveRemoteHost()`.
   **The core fails closed.** When `tunnel/enabled` is true but the settings are not usable (TLS off, an invalid hostname),
   `effectiveRemoteHost()` is empty and `ClientApp::parseArgs` logs one line ("the Cloudflare Tunnel is on but cannot be used
   (...): not connecting to the saved address") and exits with the configuration error (code 4); it never dials the saved
   direct address without TLS. With `tunnel/enabled` false or absent nothing changes. Under the Windows service the watchdog may
   start the core again after this exit (and log the line again), as it does after any other exit that reports a bad
   configuration; this has not been tried on Windows.
5. If cloudflared stops on its own, it is started again after 1, 2, 4, 8, 16, 30, 60 s; five stops in a row within 2 s of
   the start fail the tunnel. A login that is asked for (the Service Token is wrong, expired or not allowed: cloudflared would
   open a browser and wait) ends cloudflared at once and fails the tunnel: it is **not** started again by itself.
6. The client stops: the core is stopped, then the tunnel is let go. The tunnel stays for 3 s so that a restart of the core
   does not restart the tunnel; then cloudflared is asked to go (SIGTERM; on Windows it is ended), and ended hard after 3 s if it
   stays.
7. A switch from client to server stops the tunnel at once.

**A start that fails inside `start()`.** `QProcess` reports a program that cannot be started (`CreateProcess` fails) inside
`start()` on Windows, before it returns, and later through the event loop on Unix. The supervisor publishes `Starting` before it
launches the process, so both orders give the same result: `Starting`, then `Failed`, `failed()` once for each failed start (a
second failed start is reported again, so a retry never waits for a report that does not come), the process object released and
not touched again, the token not kept. Code that waits for `startFailed` has to accept a signal that is already there
(`count() > 0 || wait()`); a test of this is `QtTunnelProcessTests::aStartThatFailsInsideStart_isReportedBeforeStartReturns`
(an empty program is reported inside `start()` on every system).

A failure of the tunnel while the core runs stops the core (it would retry a port that nothing listens on); while the core
waits for the tunnel it ends the start. The reason stays in the status line.

**Local port and connection are two things.** "The local port is open" only says that cloudflared listens. Whether the
server can be reached is told by the connection of the client: the core's connection state is shown next to the tunnel state
("Tunnel: listening (local port open) | Server: connected"). cloudflared writes why a connection through the tunnel failed
(`failed to connect to origin ... bad handshake`); the supervisor counts these lines (`originFailureCount()`), the status
keeps the reason until the server connects (`notifyServerConnected()`).

**Sleep and network change.** cloudflared opens a new connection to Cloudflare for every local connection, so a network change
needs nothing and the core simply reconnects. A gap in the clock of the supervisor of more than 3 s (the computer slept) is
not counted into the time out of a start, and a tunnel that listens is checked again: if the local port is closed the process
is restarted.

**Orphans.** The supervisor writes `<state dir>/Deskflow-tunnel.json` (process number, start time, the program as the system
reports it, hostname, port; no token) when cloudflared starts and removes it when it ends. After a crash, the next start ends
that process only if the number, the start time, and the program path all match and the program is called `cloudflared*`. A
process is never found or ended by its name. The start time must be within 50 ms on Windows and macOS (the system tells it in
exact steps and the same at every read) and within 2 s on Linux (derived from the boot time, which the kernel can move).
On Windows the process is opened once, with the right to query and to end it, and **the same handle** is checked and ended
(`IProcessInspector::terminateIf`), so a process number that passes to another program in between cannot be ended. On Unix
there is no handle: the check and `kill(2)` are two calls; the process would have to end and its number pass to another
cloudflared with the same path and a start time within the tolerance in the time between them, which is not a thing that
happens in practice.

**Quit.** `File > Quit` (and the quit of the tray) stops the core first, then the tunnel, and waits for cloudflared to end.
Desktop mode: the core is a child of the GUI and stops. **Service mode with the tunnel on:** the service core would keep
retrying a tunnel that went with the GUI, so it is stopped as well: `CoreProcess::cleanup()` first cancels every pending
"send start/stop after the connect" (a late connect cannot start the core during or after the quit), then `deliverStopToService`
connects to the daemon if it is not connected (waits at most 1 s), sends the stop and `IpcClient::flush()` waits for it to be
written (at most 1 s). If the stop cannot be delivered, the quit goes on and the log has a warning that names the consequence:
the service core keeps running and starts at boot. The daemon's stop clears the configuration it keeps for the start at boot,
so it does not start at boot either. With the tunnel off the quit does what it always did (the service core keeps running).

**Mode switch.** A switch from client to server while the start waits for the tunnel cancels the waiting start and sets the
core state back to Stopped (`CoreProcess::setMode()`); it is never left in Starting.

### Known limit: the service started at boot

The Windows service can start the client by itself at boot from the configuration it keeps (the watchdog), before the GUI
exists and so before the tunnel exists. Until the user opens Deskflow (and the GUI starts the tunnel) that client dials a local
port where nothing listens, which gives retry noise in the log and no harm. The quit with the tunnel on stops the service core
and clears that configuration, which keeps it from happening after a normal quit; it can still happen after a crash or a
power loss. **Advice: with the tunnel on, switch "start at boot" of the service off.** A permanent fix would be a tunnel that
the daemon owns, which is a different design (decision 1 of the approved design chose the GUI).

### Interruptions: what the tunnel loses, and what the client does about it

A session through the tunnel has three legs: the client program to its own cloudflared (loopback), cloudflared to the edge of
Cloudflare (a websocket over TCP, one per local connection), and the tunnel of the server to the edge (QUIC over UDP, four
connections, `Retrying connection in up to 2s/4s/...32s` with no end). The interruptions that were measured on a server Mac
(see the session notes of the change) are on the third leg: after a sleep of the Mac, a change of its network (a dock cable,
Wi-Fi) or an IPv6 route that is not there, the four QUIC connections die (cloudflared gives up on a QUIC connection that
was silent for 5 s, `MaxIdleTimeout`) and are made again after a back-off of up to 32 s. The client cannot shorten that; the
tunnel of the server can be configured to lose less (a locally managed tunnel: `edge-ip-version: "4"`, see the session
notes). What the client does, only while the tunnel is used (on, TLS on, a usable hostname):

- **Retry timing** (`common/TunnelReconnect.*`, used by `ClientApp::retryTime()`): the first retry after a session ended waits
  0.25 s (the other three connections of the tunnel are most often up, so a new connection is accepted at once); the next 15
  wait 1 s as before; the next 30 wait 2 s; then 4 s for as long as the path is down, so that a tunnel that is down for
  minutes is not asked about every second (every try is a new connection through Cloudflare). The count starts again when a
  connection is made. A direct link keeps 1 s for every try (tested: `retryTime_directLink_isOneSecondWhateverTheNumberOfTries`);
  a long retry of the dynamic retry setting is kept; a tunnel that is on but cannot be used (TLS off, invalid hostname) gets the
  direct value.
- **"Reconnecting" is not a mistake** (`ServerLinkTracker`, `ServerLink::Reconnecting`, `isTransientPathFailure`): after the
  server was reached through this tunnel, a failure of the path that passes (`bad handshake`, could not connect, edge
  unreachable, host not resolving) is told as "Server: reconnecting, the path through Cloudflare is not available now", the
  chip is "Tunnel: reconnecting" (information) and the line is not the colour of a problem. Before the server was ever reached,
  and for a login that is asked for or any other failure, nothing changed. The memory is dropped when the core stops and when
  the tunnel stops or fails. MainWindow hooks: `updateTunnelServerLink` gives the tracker the connection state of the core
  (and whether the core runs), `updateTunnelShell` resets it and takes the colour from `tunnelStatusIsProblem`.
- **The log does not fill** (`RepeatCollapser`, in `TunnelSupervisor::onOutputLine`): the same `failed to connect to origin`
  line again within 30 s is counted and not shown; "The line above came N more times in the last S s" follows when the window
  is over, when another line comes and when the server connects. The supervisor still reads and counts every line
  (`originFailureCount()`) and redacts it first.

Not done, with the reason (the numbers are in the session notes): the dead-peer time of Deskflow (3 s keep-alive, 9 s) is not
the limit, because the QUIC connection dies after 5 s of silence and cloudflared then closes the stream, which the other side
sees at once; coalescing of mouse moves under back pressure does nothing, because a stalled path of at most 5 s holds about
25 KB in the socket buffers (125 Hz) and the client already compresses a burst of absolute moves; `TCP_NODELAY` is already set
on both ends (`TCPSocket::init`, also for accepted sockets); the 2 s connect time limit (`client/connectTimeout`) is not raised
for the tunnel without a measurement of the connect time through it.

### Security notes

- The Secret reaches cloudflared **only** through the environment of that one child (`TUNNEL_SERVICE_TOKEN_ID`,
  `TUNNEL_SERVICE_TOKEN_SECRET`), never an argument, a file, the log, the settings or a crash message. `cloudflared access tcp`
  has no option for a token file or for stdin. On Windows another process of the same user can read the environment of a
  child; this is the same exposure as typing the variables in a terminal, and is the reason the token is not stored in a place
  that other users can read.
- cloudflared is started with `--loglevel info` and no other level: at `debug` it prints the request headers, and the Secret is
  one of them. **Every** inherited variable whose name starts with `TUNNEL_` is removed from the child (case-insensitive on
  Windows) before the two token variables are set, so a `TUNNEL_TOKEN` or any other variable of the parent cannot change what
  `access tcp` does.
- **One program.** The locator returns one canonical absolute path and exactly that path is started. A configured path that is
  not absolute is refused with the install/path message (it would be looked up in the working directory or in `PATH`, and the
  token would go to whatever was found). A path that the user types or chooses and that is absolute is a trusted choice and is
  accepted as it is.
- **Output.** A line of cloudflared over 64 KB is never given in parts: it is dropped whole and one notice line takes its place,
  so the redactor never sees half of a token. The last line without an end is redacted like every line.
- **The secure store is not asked while the feature is off.** Opening Configure client with the tunnel off makes no call to the
  secure store (on macOS a locked login keychain can raise an unlock prompt for a lookup). The Client ID is looked up once,
  when the tunnel is turned on or when the user touches the credential fields, and kept for the life of the dialog.
- Every line of cloudflared is redacted before it is read, logged or shown. The summary that "Test connection" copies has no
  token and no Client ID.
- The local listener is open to every user of the computer. What protects the connection is Deskflow's TLS and the fingerprint
  check (the trusted fingerprints are keyed by the certificate, not by the address), so a different program that listens on the
  port cannot take the place of the server unnoticed.
- "Test connection" shows only the first characters of the Client ID (never more than half of it) and an ellipsis; the Secret is
  never shown.
- The probe that checks the local port is a connection that is closed at once. Through the tunnel it makes cloudflared open one
  connection to the server, which the server sees as a connection that ends at once, once per start.
- "Remove all settings" also deletes the Service Token from the secure store (`diagnostic::removeStoredCredentials`).

## Off means off

With `tunnel/enabled` unset or `false`: `TunnelController::isRequired()` is false, so the core process takes the path it always
took; the controller writes nothing (it only removes `tunnel/activePort` when it is there); the main window does not hide or
change any field; the status bar shows the saved address as before; the quit does what it did; the secure store is not called; `ClientApp` reads the saved address as it always did. The tests that
show it:
`TunnelAddressTests` (the address is the saved one for every shape of it), `TunnelControllerTests`
(`withTheTunnelOff_nothingIsRequiredNothingStartsAndNothingIsWritten`: the bytes of the configuration file are the same),
`TunnelStatusWidgetTests` (`whileTheTunnelIsOff_theWidgetIsHiddenAndWritesNothing`).
The same holds for the interruption handling above: with the tunnel off, `ClientApp::retryTime()` is the value of a direct link
for any number of tries, the link is never "reconnecting", and the log lines of cloudflared do not exist.

## Hooks in the main window

The tunnel does not depend on the layout of the window. These are all the places where `MainWindow`, `CoreProcess`,
`StatusBar` and the dialogs know about it; every one is a few lines that is marked with a comment that names the Cloudflare
Tunnel, so that another layout of the window can take them over in one commit.

| Where | What | Fed by |
| --- | --- | --- |
| `MainWindow::setupTunnel()` (called after `setupTrayIcon()` in the constructor) | makes `TunnelController::createSystem(this)`, gives it to the core process (`m_coreProcess.setTunnelGate()`), makes the widget, the tray line and the log connection | |
| the widget `TunnelStatusWidget` | two lines: the **path line** `Via Cloudflare Tunnel: mbp.example.com (local 127.0.0.1:24800)` and the **status line** `Tunnel: listening (local port open) \| Server: connected`. Inserted in `ui->verticalLayout` (the group of the mode) right after `ui->widgetModeOptions`. Hidden while the tunnel is not used | `TunnelController::statusChanged`, `setServerLink()` |
| `MainWindow::tunnelUsedChanged(bool)` | while the tunnel is used, hides `ui->m_pLabelServerName` and `ui->cbHostname` (the saved address stays in the field, only not shown), enables the Test connection button, shows the status line in the tray menu | `TunnelStatusWidget::usedChanged` |
| `MainWindow::updateTunnelServerLink(state)` (first lines of `coreConnectionStateChanged`) | tells the widget `Connecting`, `Connected` or `Unknown`, and the controller that the server connected | `CoreProcess::connectionStateChanged` |
| tray menu | a disabled action with the status line, above the start action; hidden while the tunnel is not used | `TunnelStatusWidget::statusTextChanged` |
| `coreModeToggled` | `m_tunnelController->setClientMode(mode == Client)` and `m_tunnelStatus->refresh()` | the mode radio buttons |
| `applyConfig` (last lines) | `m_tunnelStatus->refresh()`: the group in the dialog changed the settings | |
| `startCore`, `canRunCore`, `remoteHostChanged` | a client with the tunnel on may start with an empty saved address (`m_coreProcess.isTunnelRequired()`) | |
| `coreConnectionStateChanged` (Connected branch) | the saved address is not put in the recent hosts when the tunnel was used | |
| `showConnectionTest` (first lines) | with the tunnel on, opens `TunnelTestDialog` with the controller instead of `ConnectionTestDialog` | |
| `clearSettings` and `~MainWindow` | `m_tunnelController->shutdown()` after the core is stopped | |
| `StatusBar` (the "connected as client" text) | with the tunnel used it names the host of the tunnel: `... is connected as client of mbp.example.com through a Cloudflare Tunnel`; with it off the text is the old one | `deskflow::tunnel::displayedTunnelHost()` |
| `CoreProcess` | `setTunnelGate()`, `isTunnelRequired()`; a guard at the start of `start()` (before the lock), `coreStopped()` at the end of `stop()`, the service-mode quit in `cleanup()` | |
| `IpcClient::flush(msecs)` | writes what was sent, before the program ends | |
| `Diagnostic` | `removeStoredCredentials()` in `clearSettings()` | |

For the shell with the sidebar: the **path line** is what goes where the address field is (`ClientConnectionSection::addressStack()`
shows the field or the path line, by `usedChanged`); the **status line** is the one line under it (`addStatusLine()`); the
state of the tunnel for the status model is `TunnelStatus` (`state`, `failure`, `text()`), which can feed an external chip
(`StatusModel::setExternalChip()`); the tray reads `statusTextChanged`. None of this reads the widgets of the old window.

### In the new look (the shell with the sidebar)

With the Deskflow look (`Appearance`) the window is `MainShell`; the system look keeps the layout and the hooks above, byte for
byte. The same controller, the same `tunnel/*` settings and the same effective address feed these places:

| Where | What |
| --- | --- |
| `MainWindow::attachTunnelToShell()` / `updateTunnelShell()` | the `TunnelStatusWidget` stays the source of the texts (it sits in a hidden holder). The **path line** (`tunnelPathLine`) is the second page of `ClientConnectionSection::addressStack()` and replaces the address field while the tunnel is used; the **status line** (`tunnelStatusLine`, an `InlineHint`, red when the tunnel failed or has a failure to tell) is added with `addStatusLine()`; both are taken away when the tunnel is off |
| `StatusModel::setExternalChip("tunnel", ...)` | the chip beside the state of sharing, from `tunnelChip(TunnelStatus, ServerLink)` (`shell/TunnelChip.h`): nine short texts, a chip is as wide as its text. Cleared while the tunnel is not used |
| `TrayController::setExternalRow("tunnel", text)` | the line in the tray menu, empty while the tunnel is not used |
| the page `settings.client` (`SettingsPages::buildClient()`) | "Client": the settings of the dialog of the client (reconnection, connect timeout, language sync, local scroll, recent servers) with the **Cloudflare Tunnel** card, which holds the same `TunnelSettingsGroup` as the dialog. The group is made the first time that the page is shown (the secure store of the system is not asked when the program starts) and is saved with Save, Reset and Restore Defaults under it: the Service Token is not written at each letter. A save does what the dialog does: `validationProblem()`, `save()`, `load()`, then `tunnelSaved()` (the window reads the tunnel again and the Test connection button follows) |
| `settings.client` in the sidebar | it has **no entry of its own** (`SettingsPages::Entry::parentId`, `MainShell::addSubPage()`): the entry of the network is marked, because one more entry makes the window higher than a small screen has the room for (`MainShellTests::deskflowLook_canBeLowEnoughForASmallScreen`). It is reached from the button of the client on the home page (`showConfigureClient()`) and from the row "Client settings" on the page of the network (`SettingsPages::pageRequested`) |
| `TestResultRows` (`widgets/TestResultRows.h`) | the dialogs of the tests (`ConnectionTestDialog`, `TunnelTestDialog`) show a card for each address or stage with a chip and a "What to try" button, in place of the text of the system look. `rowsFor()` of each dialog is the pure part (no widget); the sentence that is copied is the same in any look |

The Test connection button is enabled when the tunnel is used even when the saved address is empty: `applyConfig` reads the state of
the tunnel, because `TunnelController` starts as a client and `usedChanged` is not emitted again for a client that starts with the
tunnel on.

## Tests

All tests are in `src/unittests/gui/tunnel/` (and `core/CoreProcessTunnelTests`, `common/TunnelAddressTests`). None calls the
real cloudflared, the real keychain or the Credential Manager, a real core, the port 24800 or the clipboard.

- `cloudflared-fake` (a test program, `src/unittests/gui/tunnel/fake/`) stands in for cloudflared. The first label of the
  hostname picks what it does (`ok.`, `silent.`, `login.`, `exit.`, `flaky.`, `deny.`, `leak.`, `noport.`, and for the cutting of the output `long.`, `split.`, `cr.`, `noend.`). It tells whether
  it got the token in the environment and not in the arguments (never the value), and in the `leak.` mode it writes the token
  on purpose to prove that the supervisor masks it.
- The supervisor, the controller, the test runner and the dialogs are also run with fakes of the process, the ports, the clock
  and the system (`TunnelTestDoubles.h`); the real process, ports and inspector are used only with the fake program, on ports that
  the system hands out.
- Run with a scratch profile: `HOME=<scratch> XDG_CONFIG_HOME=... QT_QPA_PLATFORM=offscreen DESKFLOW_SECRET_STORE=memory`.
- A test that links dialogs or widgets of the gui library names `LIBS server ${gui_server_libs}` (the libraries of the server
  that the Windows linker needs).

Code that only builds on one system (`WindowsCredentialStore`, `WindowsKillOnCloseJob`, the Windows part of the process
inspector and of `QtTunnelProcess`) is compiled by the build of that system and checked by hand (below).

Before a Windows build it can be compile-checked on another system, which is how a header that did not declare the constructor
of `WindowsCredentialStore` was found to be wrong: compile every file that the branch changes for a Windows target with the
real Win32 headers of mingw-w64 (for example `zig c++ -target x86_64-windows-gnu -std=c++20 -c`, also `aarch64-windows-gnu`)
and the Qt headers of the build system, with `Q_OS_WIN` defined by the target. That checks declarations, signatures, includes and
the Win32 types; it does not link, does not use the MSVC compiler or its STL, and is not a substitute for the Windows build.
Every Win32 function that the Windows-only code calls (`Cred*Read/Write/Delete/Free`, `CreateJobObjectW`,
`SetInformationJobObject`, `AssignProcessToJobObject`, `OpenProcess`, `TerminateProcess`, `GetProcessTimes`,
`GetExitCodeProcess`, `QueryFullProcessImageNameW`, `CloseHandle`) is in `kernel32` or in `advapi32`, which the gui library links
(`target_link_libraries(gui PUBLIC advapi32)`).

## Manual checks

### The secure store of the system (opt-in)

The store of the system is never called by a test, because a call can ask the user for a password. To check it by hand:

```
cmake -S . -B build -DDESKFLOW_BUILD_SECRET_SMOKE=ON
cmake --build build --target secret-store-smoke
build/bin/secret-store-smoke --use-the-real-store
```

It uses an item with a name of its own (`com.deskflow.smoketest.<random>` / `Deskflow/SmokeTest/<random>`) and made-up values,
writes, reads, replaces and removes it, removes it again at the end, never prints the Secret, and never touches the Service
Token of the user. Without `--use-the-real-store` it only prints what it would do. The system may ask for the password: that is
part of what is checked. Expect `ALL PASSED`.

### The whole way

1. Install cloudflared (`winget install --id Cloudflare.cloudflared` on Windows, `brew install cloudflared` on macOS).
2. Configure client: switch **Connect through a Cloudflare Tunnel** on, enter the hostname, Client ID and Secret (the Secret is
   typed into a masked field and never shown again).
3. **Test connection**: every stage should be marked ok. A failure names the stage and what to do.
4. Start the client: the main window shows the path line and `Tunnel: listening ... | Server: connected`.
5. Windows service mode: hide to the tray (the tunnel stays); `File > Quit` stops the tunnel and the service core.

## What is not verified here

The Windows-only code is compiled and checked by the Windows build and by hand. The behaviour of the real keychain and the
Credential Manager, the real output of cloudflared for a wrong token or a denied Access policy, and a real connection through a
tunnel are checked by hand with the user's own tunnel. The continuation of the start of the core after the tunnel listens is
covered by `TunnelStartGuardTests` and `CoreProcessTunnelTests`, not by a real start of the core.
