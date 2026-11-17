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
| `CloudflaredLocator` | finds cloudflared: the configured path, then the usual places (Windows `C:\Program Files (x86)\cloudflared\cloudflared.exe` and `C:\Program Files\cloudflared\cloudflared.exe`, macOS `/opt/homebrew/bin` and `/usr/local/bin`), then `PATH` |
| `CloudflaredLaunch` | the command: `access tcp --hostname <host> --url 127.0.0.1:<port> --loglevel info`; the token only in the environment of the child |
| `CloudflaredLogParser` | what a line of cloudflared means (listener started, bind failed, a login is asked for, a connection could not be opened and why); substring based, so a change in the words makes the tunnel less informative and never wrong |
| `SecretRedactor` | masks the token and the shapes of the headers and variables that carry it, in every line that leaves the supervisor |
| `ISecretStore` and the stores | see above |
| `ITunnelProcess`, `QtTunnelProcess` | one run of cloudflared: no console window and a job that ends it with the program on Windows, the parent death signal on Linux, stdin closed |
| `IPortProber`, `QtPortProber` | can a listener be opened, a free port, is a port open (a connection that is closed again at once, no byte sent) |
| `OrphanRecord`, `IProcessInspector` | the record of the running cloudflared and the way to look at a process by its number |
| `TunnelSupervisor` | runs it all: the checks before the start, the process, the state machine, the restarts, the wake from sleep |
| `TunnelController` | the settings on one side, the supervisor on the other; it is the gate that the core asks (`ITunnelGate`) |
| `TunnelStartGuard` | the order: the client core starts when the tunnel listens; the tunnel is let go when the core stops |
| `TunnelTestRunner`, `TunnelTestDialog` | "Test connection" through the tunnel |
| `TunnelStatusWidget` (`widgets/`) | the two lines of the main window |
| `TunnelSettingsGroup` (`widgets/`) | the group "Cloudflare Tunnel" in the dialog that configures the client |

### Life of the tunnel

The GUI **owns** the tunnel, also in Windows service mode. The tunnel lives while the GUI process lives; hiding the window
to the tray keeps it.

1. The user starts the client. `CoreProcess::start()` asks `TunnelStartGuard`. If the tunnel is on and does not listen, the
   start is postponed (the process state is Starting) and the tunnel is asked to start.
2. `TunnelSupervisor::start()` checks, in this order: TLS is on; the hostname is a host name; cloudflared is found; a Service
   Token is stored (this is the only place the Secret is read); a cloudflared that an earlier run left behind is ended (see
   below); the preferred local port can be listened on, otherwise a free port is chosen and written to `tunnel/activePort`.
   A check that fails ends the start with its reason; nothing is started.
3. cloudflared is started with the token in the environment of the child. The supervisor waits for the local port to
   open (it probes at the latest 300 ms after the start, or when the log says that the listener started), and calls the
   tunnel **listening** when the port stayed open for 300 ms with the process still running. If this does not happen in 8 s
   the start fails.
4. When the tunnel listens, the guard starts the core. The core reads the address with `effectiveRemoteHost()`.
5. If cloudflared stops on its own, it is started again after 1, 2, 4, 8, 16, 30, 60 s; five stops in a row within 2 s of
   the start fail the tunnel. A login that is asked for (the Service Token is wrong, expired or not allowed: cloudflared would
   open a browser and wait) ends cloudflared at once and fails the tunnel: it is **not** started again by itself.
6. The client stops: the core is stopped, then the tunnel is let go. The tunnel stays for 3 s so that a restart of the core
   does not restart the tunnel; then cloudflared is asked to go (SIGTERM; on Windows it is ended), and ended hard after 3 s if it
   stays.
7. A switch from client to server stops the tunnel at once.

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
that process only if the number, the start time (within 2 s), and the program path all match and the program is called
`cloudflared*`. A process is never found or ended by its name.

**Quit.** `File > Quit` (and the quit of the tray) stops the core first, then the tunnel, and waits for cloudflared to end.
Desktop mode: the core is a child of the GUI and stops. **Service mode with the tunnel on:** the service core would keep
retrying a tunnel that went with the GUI, so it is stopped as well: `CoreProcess::cleanup()` sends the stop to the service and
`IpcClient::flush()` waits for it to be written. The daemon's stop clears the configuration it keeps for the start at boot,
so it does not start at boot either. With the tunnel off the quit does what it always did (the service core keeps running).

### Security notes

- The Secret reaches cloudflared **only** through the environment of that one child (`TUNNEL_SERVICE_TOKEN_ID`,
  `TUNNEL_SERVICE_TOKEN_SECRET`), never an argument, a file, the log, the settings or a crash message. `cloudflared access tcp`
  has no option for a token file or for stdin. On Windows another process of the same user can read the environment of a
  child; this is the same exposure as typing the variables in a terminal, and is the reason the token is not stored in a place
  that other users can read.
- cloudflared is started with `--loglevel info` and no other level: at `debug` it prints the request headers, and the Secret is
  one of them. The inherited variables that would change what `access tcp` does (`TUNNEL_*`) are removed from the child.
- Every line of cloudflared is redacted before it is read, logged or shown. The summary that "Test connection" copies has no
  token and no Client ID.
- The local listener is open to every user of the computer. What protects the connection is Deskflow's TLS and the fingerprint
  check (the trusted fingerprints are keyed by the certificate, not by the address), so a different program that listens on the
  port cannot take the place of the server unnoticed.
- The probe that checks the local port is a connection that is closed at once. Through the tunnel it makes cloudflared open one
  connection to the server, which the server sees as a connection that ends at once, once per start.
- "Remove all settings" also deletes the Service Token from the secure store (`diagnostic::removeStoredCredentials`).

## Off means off

With `tunnel/enabled` unset or `false`: `TunnelController::isRequired()` is false, so the core process takes the path it always
took; the controller writes nothing (it only removes `tunnel/activePort` when it is there); the main window does not hide or
change any field; the status bar shows the saved address as before; the quit does what it did. The tests that show it:
`TunnelAddressTests` (the address is the saved one for every shape of it), `TunnelControllerTests`
(`withTheTunnelOff_nothingIsRequiredNothingStartsAndNothingIsWritten`: the bytes of the configuration file are the same),
`TunnelStatusWidgetTests` (`whileTheTunnelIsOff_theWidgetIsHiddenAndWritesNothing`).

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

## Tests

All tests are in `src/unittests/gui/tunnel/` (and `core/CoreProcessTunnelTests`, `common/TunnelAddressTests`). None calls the
real cloudflared, the real keychain or the Credential Manager, a real core, the port 24800 or the clipboard.

- `cloudflared-fake` (a test program, `src/unittests/gui/tunnel/fake/`) stands in for cloudflared. The first label of the
  hostname picks what it does (`ok.`, `silent.`, `login.`, `exit.`, `flaky.`, `deny.`, `leak.`, `noport.`). It tells whether
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
