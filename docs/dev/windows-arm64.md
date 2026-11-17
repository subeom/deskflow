# A native Windows ARM64 build

Is a native Windows ARM64 build of this fork worth having, and how is it built? This page gives the answer, what it rests
on, what was added, and the prompts to run on the ARM64 PC. Nothing here was run on Windows by the author: every claim
carries a label.

| Label | Meaning |
| --- | --- |
| **verified** | read in this repository, or run on the Mac in this work (the command or the file is named) |
| **documented** | stated by Microsoft, Qt, aqtinstall or the release notes of this fork (the source is named) |
| **inferred** | follows from the above, not observed |
| **unverified** | needs the Windows PC; the command that would verify it is named |

## 1. The answer

**Go, with one gate: the PC must be an ARM64 PC.** An ARM64 binary does not run on an x64 PC. Prompt A (section 8) shows
the CPU. If it says x64, stop: the native build is not for that PC.

Why go, if it is an ARM64 PC:

- The build files already support it. **verified:** the root `CMakeLists.txt` derives `BUILD_ARCHITECTURE` arm64 from the
  compiler target (`VSCMD_ARG_TGT_ARCH`) or `CMAKE_SYSTEM_PROCESSOR`; `deploy/windows/deploy.cmake` passes it to the
  installer; upstream builds this in CI (`windows-2022-arm64`: runner `windows-11-arm`, vcpkg triplet `arm64-windows`,
  Qt 6.12, Ninja). What this fork lacked was the local helper, the documentation, and a check that the result is native.
- An ARM64 build of this fork exists already and was published. **documented:** the release `v1.27.0.45` has
  `deskflow-1.27.0.45-win-arm64.msi` and `-portable.zip`, cross-built from commit `929feee1f` on an x64 host with the
  Visual Studio ARM64 compiler, ARM64 Qt 6.9.3 and ARM64 OpenSSL 3.4.1, and its notes say the ARM64 binaries "could be
  structurally verified but not executed". Nothing in the repository reproduces that build, and no newer commit has one.
- The installed Deskflow of the PC is probably that package. **inferred:** the Application Error events name
  `libssl-3-arm64.dll` (OpenSSL puts the architecture in its Windows DLL names: `libssl-3-x64.dll` for x64, `libssl-3-arm64.dll` for ARM64) for a
  `deskflow-core.exe` 1.27.0.29, which means an ARM64 `deskflow-core.exe` ran natively on that PC. Why it crashed (an access
  violation in OpenSSL) is **unverified**; nothing in the history of the fork branches `fix/tls-no-client-cert` and
  `fix/tls-cert-overwrite` (both from 2026-10-09, before) explains it. Do not read a cause into it.
- Today the PC builds x64 and runs it emulated. **documented** by the setup notes. Input hooks, the TLS path and a real client
  session under emulation were never verified either. A native build removes the emulation from the question.
- It found a real native-only defect (section 5), which the shipped ARM64 package, if it is what is installed, has too.

Why it is cheap: a second build tree (`build\windows-arm64-<type>`), a second Qt kit, a second vcpkg triplet. The x64
build, its directory and its command lines do not change (section 4).

Why it is not free: about 2.3 GB of Qt on disk, a first OpenSSL build for ARM64, the ARM64 C++ tools and the ARM64 C++ runtime on the
PC, and every behaviour of it on Windows is **unverified** until the prompts of section 8 have run.

## 2. Native on the ARM64 PC, cross-build, emulation, CI

| Way | Needs | Verdict |
| --- | --- | --- |
| **Native on the ARM64 PC** (this page) | VS ARM64 C++ tools, the native Qt kit (`windows_arm64`), vcpkg `arm64-windows`, the ARM64 VC++ runtime | **Recommended.** One PC, one Qt kit, the Qt tools (`moc`, `rcc`, `uic`, `lrelease`, `windeployqt`) are ARM64 and run natively. |
| Cross-build from x64 (what the `v1.27.0.45` release did) | the VS ARM64 tools, the **cross-compiled** Qt kit and the x64 Qt kit beside it as host tools | Works (**documented** by that release) but needs both Qt kits in one tree and a host path; the script refuses the cross kit on purpose. Only worth it if a machine other than the ARM64 PC has to produce the binaries. |
| Keep x64 under emulation (today) | nothing new | Builds and `--version` work; real hooking, TLS and a client session are unverified. Keeps working beside the native build. |
| CI (`windows-11-arm` runner) | a workflow in the fork | Option only. **Not added or enabled by this work.** Upstream has the job; the fork's Actions are its own decision. |

The two Qt kits (**verified** on the Mac: `aqt` 3.3.0 listed, downloaded `qtbase` and `qttools`, read the PE headers):

| | native | cross-compiled |
| --- | --- | --- |
| aqt command | `aqt install-qt windows_arm64 desktop 6.9.3 win64_msvc2022_arm64` | `aqt install-qt windows desktop 6.9.3 win64_msvc2022_arm64_cross_compiled` |
| installs to | `6.9.3\msvc2022_arm64` | `6.9.3\msvc2022_arm64` (same name) |
| `moc.exe`, `rcc.exe`, `uic.exe`, `windeployqt.exe`, `lrelease.exe`, `Qt6Core.dll` | all ARM64 | `Qt6Core.dll` ARM64; **no tools**; `bin\target_qt.conf` says `HostPrefix=../../msvc2022_64` |
| runs on | an ARM64 PC | needs the x64 kit beside it |
| size on disk (6.9.3) | `qtbase` 331 MB, `qttools` 114 MB, the other three archives (mostly `qtdeclarative`) 1.8 GB: 2.3 GB for the default install | `qtbase` 318 MB |

Both have the same folder name, so a cross kit that was unpacked before is **not** the native one. Check `bin\moc.exe`: if it
is missing, it is the cross kit. `aqt` 3.3.0 lists `win64_msvc2022_arm64` for Qt 6.9.3, 6.10.3 and 6.12.0 on `windows_arm64`.
The project needs Qt 6.7 or newer (`REQUIRED_QT_VERSION`); 6.9.3 is what the x64 build of this fork uses, so the ARM64 build
uses it too.

## 3. What the PC must have (and how to see it)

Run the read-only check first: `docs\dev\windows-env-check.ps1` (prompt A). It installs nothing and changes nothing.

| Need | Component or command | Label |
| --- | --- | --- |
| Visual Studio 2022 17.14 with the C++ workload | already there (setup notes) | documented |
| An ARM64 compiler | component `Microsoft.VisualStudio.Component.VC.Tools.ARM64`, "MSVC v143 - VS 2022 C++ ARM64/ARM64EC build tools (Latest)" | documented (Microsoft Learn, VS 2022 Build Tools component directory) |
| ...which runs natively or emulated | `VC\Tools\MSVC\<ver>\bin\HostARM64\ARM64` (native) or `Hostx64\arm64` (x64 compiler, emulated). Visual Studio 2022 17.4 and newer on an ARM64 PC has native ARM64-hosted tools | documented (Microsoft Learn, "Visual Studio on Arm-powered devices"). Which component brings which folder on this PC is **unverified**: the check prints which folders exist |
| Windows SDK with ARM64 libraries | the SDK of the workload | the check looks for `um\arm64\kernel32.lib` |
| Native Qt 6.9.3 | `aqt install-qt windows_arm64 desktop 6.9.3 win64_msvc2022_arm64 --outputdir deps\Qt` (in the clone, no administrator, 2.3 GB on disk) | verified (command and files) on the Mac; the install itself on Windows is unverified |
| vcpkg | `deps\vcpkg` at the pinned revision (already there). The triplet `arm64-windows` is a standard one; the OpenSSL for it is built by the first configure | documented (the `v1.27.0.45` release built OpenSSL 3.4.1 for ARM64 with these pins) |
| The ARM64 Visual C++ runtime | `https://aka.ms/vc14/vc_redist.arm64.exe` (the repository README links it). The program refuses to start with a runtime older than its compiler (`ArchMiscWindows::guardRuntimeVersion`, 14.\<toolset minor\>) | verified in code |

Adding the ARM64 compiler (administrator, ask first; from a folder other than the installer's own):

```powershell
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\setup.exe" modify --installPath "<path that the check printed>" --add Microsoft.VisualStudio.Component.VC.Tools.ARM64 --passive --norestart
```

The same in the UI: Visual Studio Installer, Modify, Individual components, "MSVC v143 - VS 2022 C++ ARM64/ARM64EC build
tools (Latest)". (`setup.exe modify` is **documented** for `--installPath`/`--config`; `--add` of one component is the usual
form of the same command, **unverified** on this PC.)

## 4. What was added

- `docs/dev/windows-build.ps1`: a parameter `-Arch x64|arm64`, default `x64`. With `x64` nothing changes. **verified:** the
  script of `fork/master` and this one were run on the Mac under PowerShell 7 with the external programs mocked
  (`cmake`, `ctest`, `vswhere`, the developer shell) for three argument sets (none, `-BuildType Debug`,
  `-SkipTests -KeepGoing -Target ...`): the printed `vswhere`, developer shell, `cmake` configure, `cmake --build` and
  `ctest` command lines are identical. With `arm64` it uses `deps\Qt\6.9.3\msvc2022_arm64`, the triplet `arm64-windows`, the
  build directory `build\windows-arm64-<BuildType>`, the developer shell `-arch=arm64 -host_arch=arm64` (or
  `-host_arch=x64` when only the x64-hosted ARM64 compiler is installed), and the OpenSSL path `arm64-windows\bin`. It
  composes with `-Target`, `-SkipTests`, `-TestFilter`, `-KeepGoing`, `-VcpkgInstalledDir` and `RelWithDebInfo`. It stops with a
  message that names the missing piece (no ARM64 compiler, a Qt kit without host tools). After the build it reads the PE
  machine type of `deskflow-core.exe`, `deskflow.exe` and `deskflow-daemon.exe` and **fails if one is not ARM64**.
  A list of targets that arrives as one string (`-Target a,b` through `powershell.exe -File`) is split.
- `docs/dev/windows-env-check.ps1`: the read-only check (what the machine is, Visual Studio and its compilers, the C++
  runtime, Qt kits, vcpkg, build directories, the installed Deskflow with the machine type of its files). Logic exercised on the
  Mac with a fake tree; the real values are **unverified** until prompt A has run.
- `src/lib/deskflow/AbsolutePointer.h` with `src/unittests/deskflow/AbsolutePointerTests.*`, and its use in
  `MSWindowsDesks::deskMouseMove` (section 5).
- This page, and a pointer in `multi-pc.md`.

No GitHub Actions workflow was added or changed. The auto-updater branch was not touched.

## 5. The source audit

By reading every `MSWindows*.cpp`, `arch/win32` and the daemon, and by a syntax-only cross compile for `aarch64`
(llvm-mingw 20261104, Windows SDK headers of mingw-w64, Qt 6.12.0 mingw headers: not MSVC, not Qt 6.9.3, nothing linked).

| Question | Finding | Label |
| --- | --- | --- |
| `_M_X64`, `_M_AMD64`, `_WIN64`, `__x86_64`, intrinsics, `__cpuid`, inline assembly | none in `src` (the only compiler guards are `_MSC_VER`) | verified (`grep`) |
| DLL or hook chosen by architecture | none. The keyboard and mouse hooks are low-level hooks (`WH_KEYBOARD_LL`, `WH_MOUSE_LL`) in the process itself, so no hook DLL of the same architecture has to be injected. The only DLLs loaded by name are system ones (`sas.dll`, `shcore.dll`, `user32.dll`, `kernel32.dll`) | verified in code; that low-level hooks run in the installing thread is **documented** (`SetWindowsHookEx`) |
| Pointer, struct and ABI sizes | `INPUT`, `HANDLE` and `WPARAM` follow `_WIN64`, which is true for ARM64 as for x64 | documented |
| **Float to unsigned casts** | `MSWindowsDesks::deskMouseMove` converted `(DWORD)((65535.0f * x) / (w - 1) + 0.5f)`. Negative values (a monitor left of or above the primary one) are undefined for an unsigned type. x64 converts through a signed 64-bit integer and wraps; a native ARM64 compiler saturates to 0. **Found, fixed below.** | the saturation is **verified** with clang on arm64 (Mac): 114 688 of 344 078 grid values differ, all of them negative; for MSVC ARM64 it is **inferred**, not measured |
| The other float to `DWORD` casts (`MSWindowsEventQueueBuffer`, `ArchMultithreadWindows` x2, `ArchNetworkWinsock`: `(DWORD)(1000.0 * timeout)`) | a negative timeout is `INFINITE` before the cast, so only a timeout of 4 294 967.296 s (about 49.7 days) or more would differ (x64 wraps, ARM64 saturates). Not reachable. **Not changed.** | verified in code |
| Memory ordering (ARM64 is weakly ordered, x64 is not) | the hook callbacks and the globals they use (`g_fakeServerInput`, `g_hookThread`, `g_isPrimary`) run on the thread that installed the hooks; `g_keyStateValid` is under `g_keyStateMutex`. No unsynchronised flag shared between threads was found. This is reading, not proof | inferred |
| The MSVC runtime guard | the same code path for both; needs the ARM64 runtime (section 3) | verified in code |
| The cross syntax check (`aarch64`) | 132 library and `deskflow-core` files: **125 compile**, 7 do not. `x86_64`: 126 compile, 6 do not. Failing files are the same as in the earlier check of the monitor-reporting work: `ArchMiscWindows.cpp`, `base/String.cpp`, `ServerApp.cpp`, `MSWindowsClipboardBitmapConverter.cpp`, `MSWindowsKeyState.cpp` (both targets), `MSWindowsClipboardHTMLConverter.cpp` (`aarch64` only), `IOSXKeyResource.cpp` (macOS, listed by mistake). They are MSVC-only constructs (`i64`, an `ifstream` of a `wstring`, missing transitive includes that MSVC accepts) in files that this work did not touch. `MSWindowsDesks.cpp`, which this work changed, compiles for both | verified (Mac) |

### The fix, and why x64 stays as it is

`deskflow::pointer::absoluteAxisValue(position, extent)` (header only, `src/lib/deskflow/AbsolutePointer.h`) computes the
same float expression as before and converts it as x64 does: truncate toward zero through `int64_t`, keep the low 32 bits;
a value that no integer holds is 0. It is defined C++ on every architecture. `deskMouseMove` passes the result to
`SendInput` as before.

- **RED to GREEN** (Mac, arm64, clang, scratch HOME): with the old expression moved into the function, 7 of 18 cases of
  `AbsolutePointerTests` failed, every case with a negative position gave 0 (for example `-960` on a 1920 px axis: expected
  `0xFFFF7FF0`, got `0`) and the one-pixel axis gave an undefined value. With the fix, 18 of 18 pass.
- **x64 results are identical** (**verified**): the old expression and the function agree on all 344 078 values of a grid
  (14 axis lengths from 640 to 7680 px, positions -8192 to 16384) when compiled for **x86_64** with clang (run under
  Rosetta): 0 differences. Compiled for **arm64**, the old expression differs on 114 688 values, all negative, and
  the function is the same as on x86_64. That x64 **MSVC** gives the values of the test is **inferred** from how it converts
  (a signed 64-bit conversion), not measured.
- What it does not claim: that the x64 formula places a pointer correctly on a monitor left of or above the primary one. That
  is the open question of [monitor-reporting.md](monitor-reporting.md) ("Windows pointer accuracy"), untouched here. The
  function only makes ARM64 do what x64 does.

## 6. Build and verify

```powershell
# once: the ARM64 Qt, in the clone (no administrator, 2.3 GB on disk)
.\.venv\Scripts\python.exe -m aqt install-qt windows_arm64 desktop 6.9.3 win64_msvc2022_arm64 --outputdir deps\Qt

# the read-only check, then the build (the first one also builds OpenSSL for ARM64)
powershell.exe -NoProfile -File .\docs\dev\windows-env-check.ps1 -Arch arm64
powershell.exe -NoProfile -File .\docs\dev\windows-build.ps1 -Arch arm64 -SkipTests -Target deskflow-core,deskflow,deskflow-daemon
```

Use `-SkipTests` on a PC where Deskflow is installed: never run the full CTest there (see
[windows-build-checklist.md](windows-build-checklist.md)). The x64 build keeps `build\windows-Release`; the ARM64 one is in
`build\windows-arm64-Release`. They share nothing, unless you pass `-VcpkgInstalledDir` (then the packages of both
triplets sit side by side in that folder).

**An x64 build on an ARM64 PC.** The vcpkg program there is native ARM64, so its host triplet defaults to `arm64-windows` and
vcpkg stops with "Unable to find a valid toolchain for requested target architecture arm64" under Visual Studio 2022. The script
passes `-DVCPKG_HOST_TRIPLET=x64-windows` to every x64 configure (the same as setting `VCPKG_DEFAULT_HOST_TRIPLET=x64-windows`
by hand). On an x64 PC that is the host triplet vcpkg chooses anyway; the ARM64 build is not affected.

Verify that it is native and what version it is:

```powershell
function Get-PeMachine($p){$s=[IO.File]::OpenRead($p);$r=New-Object IO.BinaryReader($s);$s.Position=0x3C;$s.Position=$r.ReadInt32()+4;$m=$r.ReadUInt16();$s.Dispose();switch($m){0xAA64{'ARM64'}0x8664{'x64'}default{'0x{0:X4}' -f $m}}}
$bin = '.\build\windows-arm64-Release\bin'
Get-ChildItem "$bin\*.exe","$bin\libssl*.dll","$bin\libcrypto*.dll" | ForEach-Object { '{0}  {1}' -f $_.Name, (Get-PeMachine $_.FullName) }
& "$bin\deskflow-core.exe" --version
& "$bin\deskflow.exe" --version
git rev-list --count v1.27.0..HEAD
```

Expected: every line `ARM64` (the OpenSSL DLLs are `libssl-3-arm64.dll` and `libcrypto-3-arm64.dll`), and
`deskflow-core v1.27.0.<N> (<sha8>), protocol v1.9` with `<N>` equal to the last command. The macOS build of the same commit
prints the same `1.27.0.<N>` (`cmake/GitVersion.cmake` counts commits since the tag `v1.27.0`; the clone needs the tags).
`dumpbin /headers deskflow-core.exe` in the developer shell prints `AA64 machine (ARM64)` as well. In Task Manager the
column "Architecture" says `ARM64` for a native process and `x64` for an emulated one.

## 7. Not verified (all of it needs the PC)

- That the PC is an ARM64 PC (prompt A prints the CPU).
- That `windows-build.ps1 -Arch arm64` runs to the end: the developer shell for `-host_arch=arm64`, the first OpenSSL
  build for `arm64-windows`, the post-build `windeployqt`, the MSVC compile of every file for ARM64.
- Which Visual Studio component gives the native `HostARM64` compiler on this PC.
- That the unit tests pass natively (not part of the prompts: do not run the full CTest on that PC).
- Real input on a native ARM64 client: hooks, `SendInput`, the TLS path (the earlier crash in `libssl-3-arm64.dll` is
  unexplained), the pointer on a monitor left of or above the primary one, clipboard.
- That MSVC ARM64 saturates the negative float cast (the fix does not depend on it).
- That the package of release `v1.27.0.45` is what is installed on the PC (prompt A prints the machine type of the installed
  files).

## 8. Prompts for the Windows Copilot (Korean, paste as they are)

Order: A, then B only for what A shows missing, then C, D, E. F says what to bring back. Every prompt starts with the same
rules.

### A. Read-only check

````text
규칙: 읽기 전용 명령 외에는 내가 확인한 뒤에만 실행해. 설치된 Deskflow(GUI, deskflow-core, deskflow-daemon, "Deskflow" 서비스)는 건드리지 마. 프로세스 종료, 서비스 변경, 재시작은 하지 마. 전체 ctest는 돌리지 마. 설치는 명령을 먼저 보여주고 내 승인을 받은 뒤에만 해. 결과는 요약하지 말고 원문 그대로 보여줘.

목표: 이 PC가 ARM64인지, ARM64 네이티브 빌드에 무엇이 있고 없는지 읽기 전용으로 점검한다.

1. 이전에 windows-build.ps1을 돌린 deskflow 클론 폴더로 이동해. 작업 트리는 바꾸지 말고 점검 스크립트만 가져와:
   git fetch https://github.com/subeom/deskflow.git feat/windows-arm64 --tags
   git show FETCH_HEAD:docs/dev/windows-env-check.ps1 | Set-Content -Encoding UTF8 $env:TEMP\windows-env-check.ps1
2. 실행:
   powershell.exe -NoProfile -File $env:TEMP\windows-env-check.ps1 -Repo (Get-Location).Path
3. 아래 두 값도 출력해:
   $env:PROCESSOR_ARCHITECTURE
   (Get-CimInstance Win32_Processor).Architecture      # 12는 ARM64, 9는 x64
4. 점검 스크립트 출력 전체와 3번 값을 원문 그대로 보여줘.
````

### B. One-time setup (only what A showed missing)

````text
규칙: 읽기 전용 명령 외에는 내가 확인한 뒤에만 실행해. 설치된 Deskflow(GUI, deskflow-core, deskflow-daemon, "Deskflow" 서비스)는 건드리지 마. 프로세스 종료, 서비스 변경, 재시작은 하지 마. 전체 ctest는 돌리지 마. 설치는 명령을 먼저 보여주고 내 승인을 받은 뒤에만 해. 결과는 요약하지 말고 원문 그대로 보여줘.

목표: 점검(A) 결과에서 "MISSING"으로 나온 항목만 하나씩 준비한다. 항목마다 명령을 보여주고, 내가 "진행"이라고 답한 뒤에만 실행해. 이미 있는 항목은 건너뛰어.

1. ARM64 C++ 빌드 도구(관리자 권한 필요, 하나씩 승인 받기). 점검이 "windows-build.ps1 would use"로 출력한 경로를 <VS경로>로 써서, 클론 폴더에서:
   & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\setup.exe" modify --installPath "<VS경로>" --add Microsoft.VisualStudio.Component.VC.Tools.ARM64 --passive --norestart
   (Visual Studio Installer의 수정 > 개별 구성 요소 > "MSVC v143 - VS 2022 C++ ARM64/ARM64EC 빌드 도구(최신)"로 해도 같다.)
2. ARM64 Visual C++ 재배포 패키지(관리자 권한 필요): https://aka.ms/vc14/vc_redist.arm64.exe 를 내려받아 설치.
3. ARM64용 Qt(관리자 권한 불필요, 디스크 약 2.3GB, 클론 안 deps\Qt에 설치):
   .\.venv\Scripts\python.exe -m aqt install-qt windows_arm64 desktop 6.9.3 win64_msvc2022_arm64 --outputdir deps\Qt
   deps\Qt\6.9.3\msvc2022_arm64 에 이미 폴더가 있는데 bin\moc.exe가 없으면 크로스 컴파일용 Qt다. 지우지 말고 나에게 알려줘.
4. 다 끝나면 A의 점검을 다시 돌려서 "arm64 build: everything ... is there"가 나오는지 보여줘.
````

### C. Build

````text
규칙: 읽기 전용 명령 외에는 내가 확인한 뒤에만 실행해. 설치된 Deskflow(GUI, deskflow-core, deskflow-daemon, "Deskflow" 서비스)는 건드리지 마. 프로세스 종료, 서비스 변경, 재시작은 하지 마. 전체 ctest는 돌리지 마. 설치는 명령을 먼저 보여주고 내 승인을 받은 뒤에만 해. 결과는 요약하지 말고 원문 그대로 보여줘.

목표: ARM64 네이티브로 빌드한다. 설치된 Deskflow와 x64 빌드(build\windows-Release)는 그대로 둔다.

1. 클론 폴더에서 git status. 보존할 변경이 있으면 여기서 멈추고 알려줘.
2. 코드 받기:
   git fetch https://github.com/subeom/deskflow.git feat/windows-arm64 --tags
   git switch --detach FETCH_HEAD
   git rev-parse HEAD
   git rev-list --count v1.27.0..HEAD
3. 빌드(테스트 제외, 첫 실행은 ARM64 OpenSSL을 만드느라 오래 걸린다. 끝날 때까지 기다려):
   powershell.exe -NoProfile -File .\docs\dev\windows-build.ps1 -Arch arm64 -SkipTests -Target deskflow-core,deskflow,deskflow-daemon
4. 실패하면 오류 원문 전체를 보여주고, 고치려고 소스를 바꾸지 말고 멈춰. 마지막에 "deskflow-core.exe machine type: ARM64" 줄이 나와야 한다.
````

### D. Native machine type and version

````text
규칙: 읽기 전용 명령 외에는 내가 확인한 뒤에만 실행해. 설치된 Deskflow(GUI, deskflow-core, deskflow-daemon, "Deskflow" 서비스)는 건드리지 마. 프로세스 종료, 서비스 변경, 재시작은 하지 마. 전체 ctest는 돌리지 마. 결과는 요약하지 말고 원문 그대로 보여줘.

목표: 방금 만든 빌드가 네이티브 ARM64이고 버전이 맞는지 확인한다(--version은 연결하지 않고 바로 끝난다).

클론 폴더에서 아래를 그대로 실행해:
function Get-PeMachine($p){$s=[IO.File]::OpenRead($p);$r=New-Object IO.BinaryReader($s);$s.Position=0x3C;$s.Position=$r.ReadInt32()+4;$m=$r.ReadUInt16();$s.Dispose();switch($m){0xAA64{'ARM64'}0x8664{'x64'}default{'0x{0:X4}' -f $m}}}
$bin = '.\build\windows-arm64-Release\bin'
Get-ChildItem "$bin\*.exe","$bin\libssl*.dll","$bin\libcrypto*.dll" | ForEach-Object { '{0}  {1}' -f $_.Name, (Get-PeMachine $_.FullName) }
& "$bin\deskflow-core.exe" --version
& "$bin\deskflow.exe" --version
git rev-parse --short=8 HEAD
git rev-list --count v1.27.0..HEAD

기대: 모든 줄이 ARM64, 버전은 1.27.0.<N>(N은 마지막 줄 숫자)과 같고 sha가 일치. 다르면 그대로 알려줘.
````

### E. Try the native client against the macOS server

````text
규칙: 읽기 전용 명령 외에는 내가 확인한 뒤에만 실행해. 설치된 Deskflow(GUI, deskflow-core, deskflow-daemon, "Deskflow" 서비스)는 건드리지 마. 프로세스 종료, 서비스 변경, 재시작은 하지 마. 전체 ctest는 돌리지 마. 결과는 요약하지 말고 원문 그대로 보여줘.

목표: 방금 만든 ARM64 클라이언트를 macOS 서버에 잠깐 연결해 본다. 설치된 앱의 설정 파일은 바꾸지 않는다.

1. 실행 중인지 확인(읽기 전용): Get-Process deskflow-core -ErrorAction SilentlyContinue | Format-Table Name,Id,Path
   deskflow GUI 창과 deskflow-daemon 서비스 프로세스는 그대로 둔다. deskflow-core가 하나라도 있으면 멈추고 나에게 "설치된 Deskflow GUI에서 Stop을 눌러 달라"고 말해. 내가 직접 Stop을 누른다. 너는 프로세스를 끝내거나 서비스를 건드리지 마. 내가 "Stop 했다"고 한 뒤 다시 확인해서 비어 있어야 진행해.
2. 설정 백업(복사만):
   $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
   Copy-Item "$env:APPDATA\Deskflow" "$env:USERPROFILE\Deskflow-backup-$stamp\appdata" -Recurse -ErrorAction SilentlyContinue
   Copy-Item "$env:ProgramData\Deskflow" "$env:USERPROFILE\Deskflow-backup-$stamp\programdata" -Recurse -ErrorAction SilentlyContinue
   백업 폴더 경로를 알려줘(TLS 개인키가 들어 있으니 어디에도 올리지 마).
3. 서버 주소 확인(읽기 전용), 포트 24800: Wi-Fi 10.18.0.77, USB LAN 10.0.21.36, Tailscale 100.76.209.252
   각각 Test-NetConnection <주소> -Port 24800 | Select ComputerName,TcpTestSucceeded
   결과를 보여주고 어느 주소로 할지 나에게 물어봐.
4. 시험용 설정 파일 만들기(설치된 설정은 그대로, 복사본만 고친다). 먼저 $server = '<내가 고른 주소>' 로 정해:
   $src = if (Test-Path "$env:APPDATA\Deskflow\Deskflow.conf") { "$env:APPDATA\Deskflow\Deskflow.conf" } else { "$env:ProgramData\Deskflow\Deskflow.conf" }
   $conf = Join-Path $env:TEMP 'deskflow-arm64-test.conf'
   Copy-Item $src $conf -Force
   $text = [IO.File]::ReadAllText($conf)
   if ($text -match '(?m)^remoteHost=') { $text = [regex]::Replace($text, '(?m)^remoteHost=.*$', "remoteHost=$server") }
   elseif ($text -match '(?m)^\[client\]') { $text = [regex]::Replace($text, '(?m)^\[client\]\s*$', "[client]`r`nremoteHost=$server", 1) }
   else { $text += "`r`n[client]`r`nremoteHost=$server`r`n" }
   [IO.File]::WriteAllText($conf, $text, (New-Object Text.UTF8Encoding $false))
   Get-Content $conf
   결과를 보여주고 내 확인을 받아.
5. 내 확인 후 클라이언트를 앞(foreground)에서 실행하고 로그를 파일에도 남겨(--new-instance는 쓰지 마. 이미 실행 중이라는 메시지로 끝나면 멈추고 알려줘):
   .\build\windows-arm64-Release\bin\deskflow-core.exe client -s $conf 2>&1 | Tee-Object $env:TEMP\deskflow-arm64-run.log
   연결 로그가 나오면 나에게 알려줘. 나는 Mac에서 포인터를 Windows 모니터들로 옮겨 본다(특히 기본 모니터의 왼쪽이나 위쪽 모니터, 키보드, 클립보드). 내가 끝났다고 하면 Ctrl+C로 중지하는 것을 안내해.
6. 중지 후(읽기 전용): 최근 1시간 Application Error 이벤트 중 Deskflow 관련:
   Get-WinEvent -FilterHashtable @{LogName='Application'; Id=1000; StartTime=(Get-Date).AddHours(-1)} -ErrorAction SilentlyContinue | Where-Object Message -like '*deskflow*' | Format-List TimeCreated,Message
   $env:TEMP\deskflow-arm64-run.log 전체와 위 이벤트를 보여줘. 시험용 설정 파일과 로그는 지우지 말고 경로만 알려줘.
7. 설치된 Deskflow는 내가 직접 다시 Start 한다. 너는 시작하지 마.
````

### F. What to bring back

- The whole output of A (and B, if it ran), with the CPU line.
- C: the last 40 lines of the build, or the whole error; `git rev-parse HEAD` and the count.
- D: the whole output.
- E: the log file, the Application Error events (or "none"), and in words: which address worked; whether the client
  connected; for each PC monitor, whether the pointer arrived where it should, with the position of the monitor relative to
  the primary one (especially a monitor left of or above it); typing and clipboard; anything that was slow or odd.
- For all: say which step you did not run.

## 9. CI, as an option only

Upstream builds ARM64 on every push with `windows-2022-arm64` (runner `windows-11-arm`, triplet `arm64-windows`, Qt 6.12).
If the fork ever wants ARM64 packages without using the PC, that job is the model. Nothing of it was added here.
