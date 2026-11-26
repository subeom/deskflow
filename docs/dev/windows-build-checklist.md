# Windows build checklist: monitor reporting (stage 3)

What to run on the Windows PC to build **this commit** and to see that it is the same version as the macOS build of it.
The setup of the PC (Visual Studio 2022 Build Tools, Qt 6.9.3, vcpkg, the clone) is the one of
[multi-pc.md](multi-pc.md); nothing in it changes, and `windows-build.ps1` needed no change for this work. The build
does not install anything, does not touch the installed Deskflow (1.27.0.45) or its service, and writes only to
`build\windows-Release` of the clone.

## 1. Get the commit

In the clone, PowerShell. Stop if `git status` shows changes you want to keep.

```powershell
git status
git fetch https://github.com/subeom/deskflow.git master --tags
git merge --ff-only FETCH_HEAD
```

Until the coordinator has merged the work into `master`, take the branch instead of `master`:

```powershell
git fetch https://github.com/subeom/deskflow.git feat/monitor-reporting --tags
git switch --detach FETCH_HEAD
```

`--tags` matters: the version is counted from the tag `v1.27.0`. Your own tag `v1.27.0.45` may stay as it is; it is
ignored by the version now (`cmake/GitVersion.cmake`).

Check the version that the build is going to have, **before** building:

```powershell
git describe --long --match 'v[0-9]*' --exclude 'v*.*.*.*'     # v1.27.0-<N>-g<sha>
git rev-list --count v1.27.0..HEAD                              # <N>
```

## 2. Build and run the tests

```powershell
powershell.exe -NoProfile -File .\docs\dev\windows-build.ps1
```

It configures (this reads the version), builds everything and runs CTest. The new files are in the CMake lists already.

**On a PC where Deskflow is installed and running, do not run all of `ctest`**: build, and run only the tests that you
name. The options (with none of them the script does what it always did):

```powershell
# build everything, run no test
powershell.exe -NoProfile -File .\docs\dev\windows-build.ps1 -SkipTests
# build everything, go on after a failure (ninja -k 0) to see all the failures at once, run no test
powershell.exe -NoProfile -File .\docs\dev\windows-build.ps1 -SkipTests -KeepGoing
# build and run only the tests whose names match
powershell.exe -NoProfile -File .\docs\dev\windows-build.ps1 -TestFilter 'ServerConfigTests|ArrangementTests|MSWindowsKeyStateTests'
# build only these targets (and the tests, if you do not say -SkipTests, that match -TestFilter)
powershell.exe -NoProfile -File .\docs\dev\windows-build.ps1 -Target deskflow-core,deskflow -SkipTests
```

Tests that are known to touch what a running Deskflow owns: `ClientAppConnectTests` (it is skipped by itself when it cannot
make `Global\DeskflowClose`, see below). No test listens on or connects to port 24800 or to another computer: they use
`127.0.0.1` with a port that the system gives.
Known and not from this work: `ArchNetworkTests` `setNoDelayOnSocket_returnsPreviousValue` failed on the setup PC
before (see multi-pc.md); the script then ends with an error **after** the build, and the executables are there. Any
other failing test is a finding: send me its name and its output.

## 3. The version it must report

```powershell
.\build\windows-Release\bin\deskflow-core.exe --version
.\build\windows-Release\bin\deskflow.exe --version
```

Expected: `deskflow-core v1.27.0.<N> (<sha8>), protocol v1.9` with `<N>` and `<sha8>` of the commit that you checked
out in step 1. The macOS build of the same commit prints the same `1.27.0.<N>`. `<N>` is what `git rev-list --count
v1.27.0..HEAD` prints in step 1, and it grows with every commit: compare with that, not with a number written in a
document. If it prints `1.27.45.x` or `1.27.0.9999`,
the clone has no tag `v1.27.0` (`git fetch --tags`) or the version was read from an old CMake cache: delete
`build\windows-Release\CMakeCache.txt` and run the script again.

## 4. What changed in what the Windows build compiles

The commit of the Windows build that you have installed is `929feee1f` (`v1.27.0.45`). **Source files compiled by the
Windows build, added (A) or modified (M) since then** (`git diff --name-status 929feee1f..HEAD`, without the files of
macOS, X11, libei and Unix). It is more than monitor reporting: the clipboard, arrangement and connection-diagnostics
work of the other branches is in it. Files of **this** work are the ones with `Monitor`, `monitor`, `Reported` or
`NativeMonitors` in the name, `MSWindowsComputer.*`, `ClientProxy1_9.*`, `ServerProxy*.*`, `Server.*`, `Client.*`,
`BaseClientProxy.h`, `IPlatformComputer.h`, `Computer.*`, `EventTypes.h`, `Settings.*`, `ClientProxy1_0.*` and
`ClientProxyUnknown.cpp`.

```
M CMakeLists.txt
A cmake/GitVersion.cmake
A cmake/tests/GitVersionTest.cmake
M src/lib/common/CMakeLists.txt
M src/lib/deskflow/CMakeLists.txt
M src/lib/gui/CMakeLists.txt
M src/lib/net/CMakeLists.txt
M src/lib/platform/CMakeLists.txt
M src/lib/server/CMakeLists.txt
```

Libraries (85 files):

```
M src/lib/arch/IArchNetwork.h
M src/lib/base/EventTypes.h
M src/lib/client/Client.cpp
M src/lib/client/Client.h
M src/lib/client/ServerProxy.cpp
M src/lib/client/ServerProxy.h
M src/lib/client/ServerProxy1_9.cpp
M src/lib/client/ServerProxy1_9.h
A src/lib/common/ClipboardCapabilities.h
A src/lib/common/ConnectionDiagnosis.cpp
A src/lib/common/ConnectionDiagnosis.h
A src/lib/common/MonitorCapabilities.h
M src/lib/common/Settings.cpp
M src/lib/common/Settings.h
M src/lib/deskflow/ClientApp.cpp
M src/lib/deskflow/Clipboard.cpp
M src/lib/deskflow/Clipboard.h
M src/lib/deskflow/ClipboardChunk.cpp
M src/lib/deskflow/ClipboardChunk.h
A src/lib/deskflow/ClipboardIdPolicy.h
A src/lib/deskflow/ClipboardImageSize.cpp
A src/lib/deskflow/ClipboardImageSize.h
A src/lib/deskflow/ClipboardReadCache.cpp
A src/lib/deskflow/ClipboardReadCache.h
A src/lib/deskflow/ClipboardSender.cpp
A src/lib/deskflow/ClipboardSender.h
A src/lib/deskflow/ClipboardSharePolicy.cpp
A src/lib/deskflow/ClipboardSharePolicy.h
A src/lib/deskflow/ClipboardTransferLog.cpp
A src/lib/deskflow/ClipboardTransferLog.h
M src/lib/deskflow/Computer.cpp
M src/lib/deskflow/Computer.h
M src/lib/deskflow/CrossPlatformInputPolicy.cpp
M src/lib/deskflow/CrossPlatformInputPolicy.h
M src/lib/deskflow/IClipboard.cpp
M src/lib/deskflow/IClipboard.h
M src/lib/deskflow/IPlatformComputer.h
A src/lib/deskflow/MonitorReport.cpp
A src/lib/deskflow/MonitorReport.h
A src/lib/deskflow/NativeMonitors.cpp
A src/lib/deskflow/NativeMonitors.h
M src/lib/deskflow/OptionTypes.h
M src/lib/deskflow/ProtocolTypes.cpp
M src/lib/deskflow/ProtocolTypes.h
M src/lib/deskflow/StreamChunker.cpp
M src/lib/deskflow/StreamChunker.h
M src/lib/io/IStream.h
M src/lib/io/StreamFilter.cpp
M src/lib/io/StreamFilter.h
A src/lib/net/ConnectFailureKind.cpp
A src/lib/net/ConnectFailureKind.h
M src/lib/net/IDataSocket.h
M src/lib/net/SecureSocket.cpp
M src/lib/net/SecureSocket.h
M src/lib/net/SocketException.h
M src/lib/net/TCPSocket.cpp
M src/lib/net/TCPSocket.h
M src/lib/platform/MSWindowsComputer.cpp
M src/lib/platform/MSWindowsComputer.h
A src/lib/platform/MSWindowsMonitors.cpp
A src/lib/platform/MSWindowsMonitors.h
A src/lib/server/Arrangement.cpp
A src/lib/server/Arrangement.h
M src/lib/server/BaseClientProxy.h
M src/lib/server/ClientProxy1_0.cpp
M src/lib/server/ClientProxy1_0.h
M src/lib/server/ClientProxy1_1.cpp
M src/lib/server/ClientProxy1_6.cpp
M src/lib/server/ClientProxy1_6.h
M src/lib/server/ClientProxy1_8.cpp
M src/lib/server/ClientProxy1_9.cpp
M src/lib/server/ClientProxy1_9.h
M src/lib/server/ClientProxyUnknown.cpp
M src/lib/server/Config.cpp
M src/lib/server/Config.h
A src/lib/server/MonitorEditor.cpp
A src/lib/server/MonitorEditor.h
A src/lib/server/MonitorGeometry.cpp
A src/lib/server/MonitorGeometry.h
M src/lib/server/PrimaryClient.cpp
M src/lib/server/PrimaryClient.h
A src/lib/server/ReportedMonitors.cpp
A src/lib/server/ReportedMonitors.h
M src/lib/server/Server.cpp
M src/lib/server/Server.h
```

GUI (`Deskflow.exe`, 38 files):

```
M src/lib/gui/MainWindow.cpp
M src/lib/gui/MainWindow.h
M src/lib/gui/MainWindow.ui
M src/lib/gui/config/Computer.cpp
M src/lib/gui/config/Computer.h
A src/lib/gui/config/MonitorEditing.cpp
A src/lib/gui/config/MonitorEditing.h
M src/lib/gui/config/ServerConfig.cpp
M src/lib/gui/config/ServerConfig.h
A src/lib/gui/core/AddressClassifier.cpp
A src/lib/gui/core/AddressClassifier.h
A src/lib/gui/core/ConnectionProbe.cpp
A src/lib/gui/core/ConnectionProbe.h
A src/lib/gui/core/ReportedMonitors.cpp
A src/lib/gui/core/ReportedMonitors.h
A src/lib/gui/core/ReportedScreen.cpp
A src/lib/gui/core/ReportedScreen.h
M src/lib/gui/dialogs/ClientConfigDialog.cpp
M src/lib/gui/dialogs/ClientConfigDialog.ui
M src/lib/gui/dialogs/ComputerSettingsDialog.cpp
M src/lib/gui/dialogs/ComputerSettingsDialog.h
M src/lib/gui/dialogs/ComputerSettingsDialog.ui
A src/lib/gui/dialogs/ConnectionTestDialog.cpp
A src/lib/gui/dialogs/ConnectionTestDialog.h
A src/lib/gui/dialogs/MonitorEditorDialog.cpp
A src/lib/gui/dialogs/MonitorEditorDialog.h
A src/lib/gui/dialogs/ServerAddressesDialog.cpp
A src/lib/gui/dialogs/ServerAddressesDialog.h
M src/lib/gui/dialogs/ServerConfigDialog.cpp
M src/lib/gui/dialogs/ServerConfigDialog.h
M src/lib/gui/dialogs/ServerConfigDialog.ui
A src/lib/gui/widgets/ArrangementCanvas.cpp
A src/lib/gui/widgets/ArrangementCanvas.h
A src/lib/gui/widgets/ArrangementPanel.cpp
A src/lib/gui/widgets/ArrangementPanel.h
A src/lib/gui/widgets/MonitorEditorWidget.cpp
A src/lib/gui/widgets/MonitorEditorWidget.h
M src/lib/gui/widgets/StatusBar.cpp
```

The tests (`src/unittests`, 75 files changed or added) are built and run by the script as well; the five `OSX*` tests are
macOS only.

## 5. The new link edge: `gui` to `server`

`src/lib/gui/CMakeLists.txt` now has `server` in the `PUBLIC` libraries of `gui` (the arrangement canvas and the monitor
editor use `server/Arrangement`, `server/MonitorGeometry`, `server/MonitorEditor`; the GUI parser of the monitor log
line uses `server/MonitorGeometry`). On Windows `server` links `common` only (the `app` library is added on Unix). Those
three files include nothing but `base/DirectionTypes.h` and the standard library, so the link of `Deskflow.exe` needs
nothing else. If it fails on Windows, it fails with **unresolved external symbols** from `deskflow::server::`
(`MonitorLayout::create`, `MonitorLayout::parse`, `arrangement::...`, `editor::...`) in `gui.lib` objects. Then look
at the link line of `deskflow.exe` (`ninja -C build\windows-Release -v`, the line of the last step): `server.lib` must be on it
**after** `gui.lib`. The fix would be a line in `src/lib/gui/CMakeLists.txt` and nothing else.

### The tests that link `server` (found on the first Windows build of `55423b5cd`)

`ComputerTests.exe` did not link on Windows: 9 unresolved symbols (`KeyMap::parseKey/parseModifiers/formatKey`,
`IKeyState::KeyInfo::alloc` x3, `join`, `split`, `ClipboardSharePolicy::sizeOptionKiB`). The test itself uses
`deskflow::server::Config`, which pulls `Config.cpp` and `InputFilter.cpp` out of `server.lib`, and those need the objects
of `app.lib`. **`server` links `app` on Unix only**, so on Windows the test has to name `app` (as the tests of `server`
do: `base arch version app mt net`). The programs are not affected: `deskflow.exe` and `deskflow-core.exe` do not use
`Config` from the GUI side and linked. The gui tests that link `server` now have those libraries on Windows
(`gui_server_libs` in `src/unittests/gui/CMakeLists.txt`, empty on Unix, so the link lines on macOS and Linux are
byte-identical). `ninja` stops at the first failing target: if another test fails to link, the symbols tell the
library (a symbol of `KeyMap`, `IKeyState`, `Clipboard*Policy`, `Event`... is in `app.lib` or `base.lib`; one of
`Arch*` is in `arch.lib`), and the fix is the same one line of `LIBS` of that test. To see every failure at once instead
of the first one, run `windows-build.ps1 -SkipTests -KeepGoing` (ninja goes on with the targets that can still be built);
then `windows-build.ps1 -TestFilter ...` for the tests that you name.

### The tests that failed on the first run of `b361da336` (7 of 53)

The programs built, linked and report `1.27.0.119`. `ctest` passed 46 of 53. The PC is **Windows 11 on ARM64**; the build
is x64 and runs under emulation. **A Deskflow is installed and running on it** (the GUI and the core of the session, the
daemon of session 0, the service): **do not run all of `ctest` on a PC where Deskflow is running** (see the options of
`windows-build.ps1` below: build, and run only the tests that you name). What is known of the seven, and what is not (the Mac
cannot run Windows: "proxy" below is an object-level check with llvm-mingw, not a link with MSVC):

| Test | Status | Cause | What changed |
|:--|:--|:--|:--|
| `MSWindowsKeyStateTests` (did not link, 65 unresolved) | **fixed**, proxy-checked | the test links `platform` and `base arch` only; a `MSWindowsKeyState` is a `KeyState` with a `KeyMap` and a `PlatformComputer`, in `app.lib`, and uses the threads of `mt.lib`. The `server -> app` edge is Unix only | `LIBS base arch io mt net app version` (`src/unittests/platform/CMakeLists.txt`, WIN32 only). Proxy: the objects of the libraries compiled for Windows, resolved the way a static link does: before, **65 symbols of `app` and 13 of `mt`** are missing (65 is the count of the LNK1120); after, none. `MSWindowsClipboardTests` (links today) has none missing in the same check |
| `ServerConfigTests::clipboardSizeOption`, `ArrangementTests::property_derivedLinks_areReadByTheCoreAsTheyAreMeant` (crash, `0xc0000005` at **address 0**) | **fixed**; the cause is certain from the code and the report, the fix is not run on Windows | reading a server configuration resolves its network address (`NetworkAddress::resolve` -> `ArchNetworkWinsock::setAddrPort` -> `htons_winsock`), a function pointer that `Arch::init()` sets, and **`Arch::init()` exists on Windows only**. Both tests have an `Arch m_arch` and never called `m_arch.init()` (the eight other tests that have one do). A call through a null pointer is an access violation at address 0, as in the report. On macOS and Linux there is nothing to initialise | `m_arch.init()` in `initTestCase()` of both |
| `ClientAppConnectTests` (`0xc0000409`, "failed to create event for windows event loop") | **not run when Deskflow is running**; the cause is probable (not shown) | an application object of Windows (`AppUtilWindows`) opens the machine-wide event `Global\DeskflowClose` in a thread, and the process ends if it cannot be made (an exception in a thread). The installed Deskflow (the daemon of session 0) owns it; the probable error is *access denied* | `initTestCase()` makes the event itself and **skips the whole test**, with the reason, when it exists or cannot be made (the CMake file says so too). Stop Deskflow (the GUI, its core and the service) to run it. See the safety note below |
| `ClientConnectDiagnosisTests::connect_toAClosedLocalPort_logsTheRefusalOfTheRealSocket`, `ConnectionProbeTests::system_closedLocalPort_isRefused` | **confirmed on the PC**, tests adapted | the log of the first one shows `connecting to '127.0.0.1:47301'` and exactly 2 s later `cannot reach the server ...: the connection timed out (no answer within 2 s)`: Windows does not hand the refusal over at once (it sends the SYN again after the reset), so a limit of 2 s ends as a timeout. The mapping `WSAECONNREFUSED` -> refused is there (`ArchNetworkWinsock.cpp`) | the limits are 10 s, and the failure messages tell **how long it took** and what the kind is. The product consequence is in `docs/connecting-across-networks.md`; on Windows the *timed out while connecting* message now also says that a port that nothing listens on looks the same (`ConnectionDiagnosis.cpp`) |
| `DialogLayoutTests::serverConfigDialog_computersTab_withTheArrangement_isLaidOutWithoutSqueezing` (crash) | **not understood** | no cause found in the code | see below |

**Safety, `ClientAppConnectTests`.** Could a test stop the Deskflow that runs on the PC? No: the only code that signals
`Global\DeskflowClose` is `MSWindowsProcess::shutdown` (what the GUI and the service use to stop a core); no test calls it,
and the application object only waits on the event and closes its handle. The other way round is possible (the GUI that
stops its core signals the event, and a test that shares it quits), which is why the test is skipped and not run when the
event exists. The same code makes a core that cannot create the event stop with `0xc0000409` instead of a message:
that is in the program too, not only in the test, and it is **not changed here** (not verifiable without Windows).

**`DialogLayoutTests`: not understood, what is known and what to run.** The report (Qt's crash handler, a release build
without symbols): `0xc0000005` inside `QObject::property`, called under `QTableView::doItemsLayout` <-
`QTableView::paintEvent` while the offscreen platform exposes the window of the dialog. `QObject::property` reads the
meta-object of the object, so it was called on an object that is not alive any more or not an object. The test is the first
of the file that paints the *Computers* tab with its table (the `Advanced` tab test, which passed, never paints it).
On the Mac the same test passes in all its rows and **AddressSanitizer finds nothing** in the 182 tests of the file (the
Mac styles only). Nothing in the sources of the dialog is Windows specific (no `Q_OS_WIN`; the only
`QObject::property` of the GUI is `ComputerSettingsDialog`, which is not painted here), so no change is made.

Ranked:

1. *The Windows styles of Qt on the offscreen platform.* The test runs each row with every style that Qt has
   (`Fusion`, `Windows`, and on Windows 11 also `windows11` and `windowsvista`). The native styles read the style object
   of an item with `QObject::property` when they paint (animations), and they were written for a real window. Check: run
   the rows of the test one style at a time:

   ```powershell
   $env:PATH = "$PWD\deps\Qt\6.9.3\msvc2022_64\bin;$PWD\build\windows-Release\vcpkg_installed\x64-windows\bin;$env:PATH"
   $env:QT_QPA_PLATFORM = 'offscreen'
   Set-Location .\build\windows-Release\src\unittests\gui\dialogs
   $test = 'serverConfigDialog_computersTab_withTheArrangement_isLaidOutWithoutSqueezing'
   foreach ($style in 'Fusion','Windows','windows11','windowsvista') {
     foreach ($font in 'x1.0','x1.3') {
       foreach ($size in 'default size','preferred size','minimum size') {
         $row = "$style, font $font, $size"
         .\DialogLayoutTests.exe "${test}:$row" *> $null
         '{0,-44} exit code {1:X}' -f $row, $LASTEXITCODE
       }
     }
   }
   ```

   `0` is a pass; `C0000005` the crash. If only the `windows11` and `windowsvista` rows crash, it is that, and the test
   should leave those two styles out on the offscreen platform (a change of the test that is not made before this is known).
2. *A stack that is too small* (1 MB in the main thread with MSVC, 8 MB on macOS). A stack overflow is `0xc00000fd`, not
   `0xc0000005`, so the exception code of the report already speaks against it; the check is cheap anyway: from a
   Developer PowerShell, `editbin /STACK:16777216 DialogLayoutTests.exe` and run the test again.
3. *An object that is deleted while a paint of the table is waiting* (the table is painted from a posted event). On the Mac
   AddressSanitizer found nothing; on Windows the timing of events differs. It needs the stack: build with debug
   information.

**A stack with names, without installing the packages again** (about as long as a build of the three tests; the packages
of the Release build are reused):

```powershell
powershell.exe -NoProfile -File .\docs\dev\windows-build.ps1 -BuildType RelWithDebInfo -VcpkgInstalledDir .\build\windows-Release\vcpkg_installed -Target DialogLayoutTests,ServerConfigTests,ArrangementTests -SkipTests
$env:PATH = "$PWD\deps\Qt\6.9.3\msvc2022_64\bin;$PWD\build\windows-Release\vcpkg_installed\x64-windows\bin;$env:PATH"
$env:QT_QPA_PLATFORM = 'offscreen'
Set-Location .\build\windows-RelWithDebInfo\src\unittests\gui\dialogs
.\DialogLayoutTests.exe serverConfigDialog_computersTab_withTheArrangement_isLaidOutWithoutSqueezing -v2 *> ..\..\..\..\..\..\dialog-crash.txt
```

(`RelWithDebInfo` writes the PDB files next to the programs, which the crash handler of Qt reads.) If `vcpkg` starts to
install again, stop it (Ctrl+C) and run the same without `-VcpkgInstalledDir`: it takes minutes and is safe. Send the last 40
lines of `dialog-crash.txt`. For the two tests that crashed at address 0 the same commands apply with `ServerConfigTests.exe
clipboardSizeOption` (`...\build\windows-RelWithDebInfo\src\unittests\server`); after the fix of `Arch::init()` they are
expected to pass.

## 6. Cross-compile check (done on the Mac, for what could be checked)

I cannot build or run Windows here. A syntax-only check (`clang -fsyntax-only`, llvm-mingw 20261104, Windows SDK
headers of mingw-w64, **Qt 6.12.0** mingw headers, not the 6.9.3 and not MSVC) of the sources that Windows compiles, at
the head of the branch:

- **Libraries and `deskflow-core`: 132 files** for `x86_64` and for `aarch64`. 126 and 125 compile. The files that do
  not (`ArchMiscWindows.cpp`, `base/String.cpp`, `ServerApp.cpp`, `MSWindowsClipboardBitmapConverter.cpp`,
  `MSWindowsKeyState.cpp`, `MSWindowsClipboardHTMLConverter.cpp` on `aarch64` only; `IOSXKeyResource.cpp` is macOS
  only and was in the list by mistake) are all **unchanged since `929feee1f`**: MSVC-only constructs (`i64`, an
  `ifstream` of a `wstring`) and missing transitive includes that MSVC accepts. They are what mingw does not accept of
  code that MSVC builds today, not findings of this work.
- Of the **40 changed `.cpp` files** of the libraries, 39 compile for `x86_64` (the 40th is `ArchNetworkBSD.cpp`, Unix).
  That includes `MSWindowsMonitors.cpp` and `MSWindowsComputer.cpp` and `NativeMonitors.cpp`.
- **GUI: the 18 changed `.cpp` files** compile for `x86_64` (with the `ui_*.h` that Qt's `uic` made on the Mac).
- **Not checked:** the unit tests (Qt Test is not in the toolchain), `aarch64` for the GUI, linking, any run, MSVC.

A clean syntax check says the code is well formed against the Windows headers. It does not say what Windows does.

## 7. What was not tried on Windows

- `MSWindowsMonitors.cpp`: `EnumDisplayMonitors`, `GetMonitorInfoW`, `GetDpiForMonitor` (loaded from `shcore.dll`).
- `MSWindowsComputer`: listing the monitors only after a server that invited the computer asks for them, and the
  `WM_DISPLAYCHANGE` path that raises `ComputerMonitorsChanged` when the list changed and the box did not.
- **The DPI awareness of `deskflow-core.exe`.** Its manifest does not set one, and the code does not change it. If
  Windows shows the process a virtualised desktop at a scale other than 100 %, the box of the monitors is not the shape
  that the client sends in `DINF`; the client then reports **nothing**, says one INFO line `monitors are not reported to
  the server: ...`, and the server stays on the old path: the behaviour of today, not a wrong one. This is the most
  likely reason for "it does not report".
- The pointer: `MSWindowsDesks::deskMouseMove` is **unchanged**. The research found that it normalises to the primary
  monitor (no `MOUSEEVENTF_VIRTUALDESK`); nothing shows it is wrong on x64, it cannot be tested here, so the fix is
  only proposed (gated by its own capability) in [monitor-reporting.md](monitor-reporting.md).
- Mixed DPI, a monitor on the left of or above the primary (negative coordinates), mirrored monitors, hot-plug.

## 8. Test it after the build (10 minutes)

The server must be the **same commit** (the macOS build), with *Experimental: multi-monitor aware cursor mapping* on
(Server configuration, Advanced). Reporting happens only when both ends are new and the switch is on; with the old
macOS build (1.27.0.89) or with the switch off nothing is sent. Back up the settings first if you want to.

1. Close the installed Deskflow GUI. Start the development GUI (`.\build\windows-Release\bin\deskflow.exe`) or the core
   as a **client** and connect to the Mac server as usual. It must connect and work as before.
2. In the log of the **Windows client** (Deskflow log, level INFO) look for `reported N monitors to the server`. If you
   see `monitors are not reported to the server: <reason>` instead, copy the line: see the DPI note above.
3. In the log of the **Mac server** look for `computer "<Windows name>" reports N monitors: x,y,w,h scale=S primary;...`
   (N is the number of monitors of the PC, `scale` is 100, 125, 150 ... as Windows says).
4. On the Mac open *Server configuration*, the computers tab: the PC must be drawn with its **real monitors**, with
   *reported by the computer* on it. Double-click it: the monitor editor opens on the reported monitors, read-only.
5. **Hot-plug** (or disable, with *Win+P* or the display settings) a monitor on the PC while the dialog is open. The
   client logs `reported N monitors` again; the server logs the new line; the canvas redraws without being reopened.
6. Disconnect the PC: the canvas keeps drawing it from the last report (offline). Connect it with the **old** Windows
   build (1.27.0.45) instead: the server logs `computer "<name>" reports no monitors` and the canvas goes back to the
   manual or guessed shape.
7. Move the pointer from the Mac to the PC and back across the monitors: it must behave as before (this work does not
   change what a pointer move does on Windows).

Anything that differs: copy the log lines (both ends), the Windows version and scale settings of the monitors, and what
you did.
