# Developing this fork on multiple PCs

Source changes are shared through `https://github.com/subeom/deskflow.git`.
Each PC needs its own clone, SDKs, and build directory. Do not put the checkout
in a live two-way file-sync folder or copy build caches between PCs.

## Windows x64 setup

Keep an existing Deskflow installation and its settings. The development build
below does not install an MSI, replace the Windows service, or write to
`Program Files`. On a PC without Deskflow, install the released app separately
with `winget install --id Deskflow.Deskflow --exact`.

Run these commands in PowerShell, checking each command succeeds before continuing.
Use a normal writable local folder for the clone.

```powershell
git clone https://github.com/subeom/deskflow.git
Set-Location deskflow
```

Install prerequisites only if missing. Visual Studio **Code** is an editor, not
the C++ compiler. Visual Studio 2022 (including Community) with the Desktop
development with C++ workload can be reused. The Build Tools command below
requires approval/elevation and includes MSVC, a Windows SDK, CMake, and Ninja:

```powershell
winget install --id Git.Git --exact --source winget
winget install --id Microsoft.VisualStudio.2022.BuildTools --exact --source winget --override "--wait --passive --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
winget install --id Python.Python.3.12 --exact --source winget
```

Restart PowerShell after installing Git/Python. Prepare dependencies inside this
clone using the same external Qt approach as CI. Qt 6.9.3 satisfies the project's
Qt 6.7+ requirement. The vcpkg tooling revision is pinned separately from the
package baseline in `cmake\vcpkg.json.in`: newer download tooling is needed
because the old tooling references MSYS2 archives that are no longer available.
The manifest still selects the repository's original dependency versions.
Downloads and OpenSSL compilation require internet access and free disk space.

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install aqtinstall==3.3.0
.\.venv\Scripts\python.exe -m aqt install-qt windows desktop 6.9.3 win64_msvc2022_64 --outputdir deps\Qt
git clone https://github.com/microsoft/vcpkg.git deps\vcpkg
git -C deps\vcpkg checkout 4702e381dbbd9b54d3f7e1eb565cd8981c1814cd
.\deps\vcpkg\bootstrap-vcpkg.bat -disableMetrics
```

These are one-time commands, not commands to repeat on every build. If you
already have an MSVC x64 Qt SDK (including LinguistTools, Test, and
`windeployqt`) or vcpkg, reuse it with `-QtRoot` / `-VcpkgRoot` below.
Do not mix an external Qt SDK with `VCPKG_QT=ON`.

## Edit, build, and run

From the repository root:

```powershell
powershell.exe -NoProfile -File .\docs\dev\windows-build.ps1
# Optional separate Debug build:
powershell.exe -NoProfile -File .\docs\dev\windows-build.ps1 -BuildType Debug
# Example using an existing Qt/vcpkg installation:
powershell.exe -NoProfile -File .\docs\dev\windows-build.ps1 -QtRoot C:\Qt\6.9.3\msvc2022_64 -VcpkgRoot C:\dev\vcpkg
```

The script finds Visual Studio 2022, initializes its x64 compiler environment in
that process, configures Ninja, builds all applications/tests, and runs CTest
from `build\windows-Release\src\unittests` (or `windows-Debug`).
It stops on any command failure. Installer generation is disabled, so WiX is
not needed. It does not persist PATH changes, install the build, or start a service.
If your execution policy blocks a local script, follow your organization's policy;
do not disable security policy globally.

Edit C++/Qt files under `src` using VS Code, Visual Studio, or Qt Creator and
rerun the script. Build outputs and dependencies are already ignored by Git.
With Visual Studio/Qt Creator, use the same CMake options as the script and a
separate build directory; put machine-specific paths in an ignored
`CMakeUserPresets.json`, not in committed files.

Before interactively opening a development GUI, exit the installed GUI to avoid
single-instance conflicts. The development GUI may use the same per-user
settings as the installed app: back them up first if changing settings.
Do not change the installed service or activate a keyboard/mouse connection
just to check a build. For a read-only core smoke check and an optional GUI launch:

```powershell
.\build\windows-Release\bin\deskflow-core.exe --version
# GUI startup/plugin check without opening the main window:
.\build\windows-Release\bin\deskflow.exe --version
# Only when you want to open the development GUI:
.\build\windows-Release\bin\deskflow.exe
```

### The version a build reports

The version comes from `git describe` at configure time (`cmake/GitVersion.cmake`): the last release tag of three
numbers, `v1.27.0`, and the number of commits since it, so a commit builds `1.27.0.N` with the same `N` on every PC.
A tag of four numbers, such as `v1.27.0.45` that marks an installed build, is **not** a release tag and is ignored (it
used to be read as `1.27.45.N`). The clone needs the tag `v1.27.0` (`git fetch --tags` brings it; a shallow clone has no
tags and reports `1.27.0.9999`). The number is read when CMake configures, which `windows-build.ps1` does on every run.

The exact steps for the commit that adds the monitor reporting, with the expected version, the files that changed since
the installed build and a short test list, are in [windows-build-checklist.md](windows-build-checklist.md).

### Validation on the setup PC

On 2026-11-15, Windows x64 Release at source revision `c4eb0426` built successfully
with MSVC 19.44, Qt 6.9.3, and OpenSSL 3.4.1. Both applications' `--version`
commands exited successfully. The existing installed app remained 1.27.0.0.

CTest passed 28 of 29 test executables. The existing `ArchNetworkTests` failed
`setNoDelayOnSocket_returnsPreviousValue` at `ArchNetworkTests.cpp:473`.
No application source or tests were changed to hide that failure, and the helper
returns an error when tests fail. A fully green suite remains unresolved.
To capture that test's Qt output on Windows:

```powershell
$env:PATH = "${PWD}\deps\Qt\6.9.3\msvc2022_64\bin;${PWD}\build\windows-Release\vcpkg_installed\x64-windows\bin;$env:PATH"
.\build\windows-Release\src\unittests\arch\ArchNetworkTests.exe -o .\deps\network-test.txt,txt
Get-Content .\deps\network-test.txt
```

## Safely share edits through GitHub

Authenticate Git on each PC using your own account (Git Credential Manager or
`gh auth login` followed by `gh auth setup-git`). Never store tokens in the
repository. Configure your chosen commit name/email on each PC if needed.

Before starting work, inspect the checkout and update only when it is clean:

```powershell
git status
git branch --show-current
git pull --ff-only origin master
```

Work on one PC at a time on the same branch. After reviewing and building your
changes, stage only the intended source/document files:

```powershell
git diff
git add <specific-files>
git diff --cached
git commit -m "Describe the change"
git push origin master
```

On the next PC, use `git status` then `git pull --ff-only origin master` and
rebuild locally. Local edits do **not** reach another PC until committed and
pushed. New setup files also must be committed/pushed before a fresh clone can
use them; alternatively transfer just those reviewed files explicitly.

If pull reports divergence or push is rejected, stop and inspect both histories.
Commit any intended local work first, then deliberately merge the remote branch
and resolve conflicts before building and pushing. Do not force-push, blindly
reset, or overwrite another PC's work. If using a different branch, replace
`master` consistently with that branch in the commands.

## Other operating systems

This helper is for native Windows x64, not Windows ARM64, Linux, or macOS.
For those PCs, clone the same fork and follow [Building Deskflow](build.md) and
the OS dependency lists in
`.github\actions\install-dependencies\action.yml`.
Use their native compiler/Qt/dependencies; never copy Windows binaries or SDKs.
The Git workflow is the same on all PCs. No other PC is configured remotely by
this guide.
