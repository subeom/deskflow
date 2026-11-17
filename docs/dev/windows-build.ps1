# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-License-Identifier: MIT

# Configures, builds and tests Deskflow on Windows x64. With no option it does all of that, as it always did.
#   -Target          build only these targets (cmake --build --target), not everything
#   -SkipTests       do not run the tests. Use it on a PC where Deskflow is running: some tests make objects of the machine
#                    that the running Deskflow owns (see docs\dev\windows-build-checklist.md)
#   -TestFilter      run only the tests whose name matches this regular expression (ctest -R)
#   -KeepGoing       go on with the targets that can be built when one fails (ninja -k 0), to see every failure at once
#   -VcpkgInstalledDir  use the packages that another build directory installed, and do not install them again
# -BuildType RelWithDebInfo builds with the debug information (PDB) that a stack of a crash needs.
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
    [string]$VcpkgInstalledDir
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $QtRoot) {
    $QtRoot = Join-Path $repo 'deps\Qt\6.9.3\msvc2022_64'
}
if (-not $VcpkgRoot) {
    $VcpkgRoot = Join-Path $repo 'deps\vcpkg'
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
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'

$cmakeTools = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake'
$env:PATH = "$(Join-Path $QtRoot 'bin');$(Join-Path $cmakeTools 'CMake\bin');$(Join-Path $cmakeTools 'Ninja');$env:PATH"
$build = Join-Path $repo "build\windows-$BuildType"

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
    '-DVCPKG_TARGET_TRIPLET=x64-windows',
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

if ($SkipTests) {
    Write-Host "Build completed, tests skipped. GUI: $build\bin\deskflow.exe"
    return
}

$opensslBin = Join-Path $vcpkgInstalled 'x64-windows\bin'
if ($BuildType -eq 'Debug') {
    $opensslBin = Join-Path $vcpkgInstalled 'x64-windows\debug\bin'
}
$env:PATH = "$opensslBin;$env:PATH"
$env:QT_QPA_PLATFORM = 'offscreen'
$ctestArgs = @('--test-dir', (Join-Path $build 'src\unittests'), '--output-on-failure')
if ($TestFilter) {
    $ctestArgs += @('-R', $TestFilter)
}
Invoke-Checked ctest $ctestArgs
Write-Host "Build and tests completed. GUI: $build\bin\deskflow.exe"
