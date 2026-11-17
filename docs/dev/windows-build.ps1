# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-License-Identifier: MIT

# Configures, builds and tests Deskflow on Windows x64 (or, with -Arch arm64, native Windows ARM64). With no option it does
# all of that for x64, as it always did.
#   -Target          build only these targets (cmake --build --target), not everything
#   -SkipTests       do not run the tests. Use it on a PC where Deskflow is running: some tests make objects of the machine
#                    that the running Deskflow owns (see docs\dev\windows-build-checklist.md)
#   -TestFilter      run only the tests whose name matches this regular expression (ctest -R)
#   -KeepGoing       go on with the targets that can be built when one fails (ninja -k 0), to see every failure at once
#   -VcpkgInstalledDir  use the packages that another build directory installed, and do not install them again
# -BuildType RelWithDebInfo builds with the debug information (PDB) that a stack of a crash needs.
#   -Arch            x64 (the default, exactly as before) or arm64: a native Windows ARM64 build, for a Windows on ARM PC, in its own
#                    build directory build\windows-arm64-<BuildType> (see docs\dev\windows-arm64.md and windows-env-check.ps1)
[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug', 'RelWithDebInfo')]
    [string]$BuildType = 'Release',
    [string]$QtRoot,
    [string]$VcpkgRoot,
    [string[]]$Target,
    [switch]$SkipTests,
    [string]$TestFilter,
    [switch]$KeepGoing,
    [string]$VcpkgInstalledDir,
    [ValidateSet('x64', 'arm64')]
    [string]$Arch = 'x64'
)

$ErrorActionPreference = 'Stop'
if ($Target) {
    # with powershell.exe -File a list of targets can arrive as one string, "a,b"
    $Target = @($Target | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
}
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$triplet = 'x64-windows'
$qtKit = 'msvc2022_64'
$buildName = "windows-$BuildType"
if ($Arch -eq 'arm64') {
    $triplet = 'arm64-windows'
    $qtKit = 'msvc2022_arm64'
    $buildName = "windows-arm64-$BuildType"
}
if (-not $QtRoot) {
    $QtRoot = Join-Path $repo "deps\Qt\6.9.3\$qtKit"
}
if (-not $VcpkgRoot) {
    $VcpkgRoot = Join-Path $repo 'deps\vcpkg'
}
if ($Arch -eq 'arm64' -and -not (Test-Path $QtRoot)) {
    throw "Missing prerequisite: $QtRoot (Qt for Windows ARM64). See docs\dev\windows-arm64.md; windows-env-check.ps1 -Arch arm64 shows what is missing."
}
$QtRoot = (Resolve-Path $QtRoot).Path
$VcpkgRoot = (Resolve-Path $VcpkgRoot).Path

foreach ($required in @(
    (Join-Path $QtRoot 'lib\cmake\Qt6\Qt6Config.cmake'),
    (Join-Path $QtRoot 'bin\windeployqt.exe'),
    (Join-Path $VcpkgRoot 'vcpkg.exe'),
    (Join-Path $VcpkgRoot 'scripts\buildsystems\vcpkg.cmake')
)) {
    if (-not (Test-Path $required)) {
        throw "Missing prerequisite: $required. See docs\dev\multi-pc.md."
    }
}
if ($Arch -eq 'arm64' -and -not (Test-Path (Join-Path $QtRoot 'bin\moc.exe'))) {
    throw "$QtRoot has no host tools (moc.exe): it is the cross-compiled Qt for ARM64. Install the native one: aqt install-qt windows_arm64 desktop 6.9.3 win64_msvc2022_arm64 (docs\dev\windows-arm64.md)."
}

# The machine type that a PE file (an exe or a dll) is built for: x64, ARM64, x86
function Get-PeMachine {
    param([string]$Path)
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $reader = New-Object System.IO.BinaryReader($stream)
        if ($stream.Length -lt 0x40 -or $reader.ReadUInt16() -ne 0x5A4D) {
            return 'not a PE file'
        }
        $stream.Position = 0x3C
        $peOffset = $reader.ReadInt32()
        if ($peOffset -lt 0 -or $peOffset + 6 -gt $stream.Length) {
            return 'not a PE file'
        }
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) {
            return 'not a PE file'
        }
        $machine = $reader.ReadUInt16()
        switch ($machine) {
            0x8664 { return 'x64' }
            0xAA64 { return 'ARM64' }
            0x014C { return 'x86' }
            default { return ('0x{0:X4}' -f $machine) }
        }
    } finally {
        $stream.Dispose()
    }
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) {
    throw 'Install Visual Studio 2022 Build Tools with the C++ workload. See docs\dev\multi-pc.md.'
}
$vs = & $vswhere -latest -version '[17.0,18.0)' -products '*' `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $vs) {
    throw 'Visual Studio 2022 with the x64 C++ tools was not found.'
}
Import-Module (Join-Path $vs 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
if ($Arch -eq 'arm64') {
    # The compiler that builds for ARM64 runs natively on an ARM64 PC (Hostarm64) or, if only the x64 one is installed,
    # emulated (Hostx64). The newest MSVC toolset is the one that the developer shell takes.
    $msvc = Get-ChildItem (Join-Path $vs 'VC\Tools\MSVC') -Directory | Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
    $hostArches = @()
    foreach ($hostArch in 'arm64', 'x64') {
        if ($msvc -and (Test-Path (Join-Path $msvc.FullName "bin\Host$hostArch\arm64\cl.exe"))) {
            $hostArches += $hostArch
        }
    }
    if (-not $hostArches) {
        throw "No compiler for ARM64 in $($msvc.FullName)\bin (Hostarm64\arm64 or Hostx64\arm64). Add the component Microsoft.VisualStudio.Component.VC.Tools.ARM64 (MSVC v143 - VS 2022 C++ ARM64/ARM64EC build tools) in the Visual Studio Installer (docs\dev\windows-arm64.md)."
    }
    $entered = $false
    foreach ($hostArch in $hostArches) {
        try {
            Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments "-arch=arm64 -host_arch=$hostArch"
        } catch {
            Write-Warning "The developer shell for -arch=arm64 -host_arch=$hostArch failed: $_"
        }
        if ($env:VSCMD_ARG_TGT_ARCH -eq 'arm64' -and (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
            Write-Host "ARM64 compiler: $($msvc.Name), host $hostArch ($((Get-Command cl.exe).Source))"
            $entered = $true
            break
        }
    }
    if (-not $entered) {
        throw 'The Visual Studio developer shell for ARM64 did not set up the ARM64 compiler.'
    }
} else {
    Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
}

$cmakeTools = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake'
$env:PATH = "$(Join-Path $QtRoot 'bin');$(Join-Path $cmakeTools 'CMake\bin');$(Join-Path $cmakeTools 'Ninja');$env:PATH"
$build = Join-Path $repo "build\$buildName"

function Invoke-Checked {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Program failed with exit code $LASTEXITCODE."
    }
}

$configureArgs = @(
    '-S', $repo, '-B', $build, '-G', 'Ninja',
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DCMAKE_PREFIX_PATH=$QtRoot",
    "-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot\scripts\buildsystems\vcpkg.cmake",
    "-DVCPKG_TARGET_TRIPLET=$triplet",
    '-DVCPKG_QT=OFF',
    '-DBUILD_INSTALLER=OFF',
    '-DBUILD_TESTS=ON',
    '-DSKIP_BUILD_TESTS=ON'
)
$vcpkgInstalled = Join-Path $build 'vcpkg_installed'
if ($VcpkgInstalledDir) {
    $vcpkgInstalled = (Resolve-Path $VcpkgInstalledDir).Path
    $configureArgs += "-DVCPKG_INSTALLED_DIR=$vcpkgInstalled"
}
Invoke-Checked cmake $configureArgs

$buildArgs = @('--build', $build, '--parallel', '4')
if ($Target) {
    $buildArgs += '--target'
    $buildArgs += $Target
}
if ($KeepGoing) {
    $buildArgs += @('--', '-k', '0')
}
Invoke-Checked cmake $buildArgs

if ($Arch -eq 'arm64') {
    # What was built must be native: the machine type of each program that exists in the build directory
    foreach ($name in 'deskflow-core.exe', 'deskflow.exe', 'deskflow-daemon.exe') {
        $exe = Join-Path $build "bin\$name"
        if (Test-Path $exe) {
            $machine = Get-PeMachine $exe
            Write-Host "$name machine type: $machine"
            if ($machine -ne 'ARM64') {
                throw "$exe is $machine, not ARM64."
            }
        }
    }
}

if ($SkipTests) {
    Write-Host "Build completed, tests skipped. GUI: $build\bin\deskflow.exe"
    return
}

$opensslBin = Join-Path $vcpkgInstalled "$triplet\bin"
if ($BuildType -eq 'Debug') {
    $opensslBin = Join-Path $vcpkgInstalled "$triplet\debug\bin"
}
$env:PATH = "$opensslBin;$env:PATH"
$env:QT_QPA_PLATFORM = 'offscreen'
$ctestArgs = @('--test-dir', (Join-Path $build 'src\unittests'), '--output-on-failure')
if ($TestFilter) {
    $ctestArgs += @('-R', $TestFilter)
}
Invoke-Checked ctest $ctestArgs
Write-Host "Build and tests completed. GUI: $build\bin\deskflow.exe"
