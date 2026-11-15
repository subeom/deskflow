# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-License-Identifier: MIT

[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')]
    [string]$BuildType = 'Release',
    [string]$QtRoot,
    [string]$VcpkgRoot
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

Invoke-Checked cmake @(
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
Invoke-Checked cmake @('--build', $build, '--parallel', '4')

$opensslBin = Join-Path $build 'vcpkg_installed\x64-windows\bin'
if ($BuildType -eq 'Debug') {
    $opensslBin = Join-Path $build 'vcpkg_installed\x64-windows\debug\bin'
}
$env:PATH = "$opensslBin;$env:PATH"
$env:QT_QPA_PLATFORM = 'offscreen'
Invoke-Checked ctest @('--test-dir', (Join-Path $build 'src\unittests'), '--output-on-failure')
Write-Host "Build and tests completed. GUI: $build\bin\deskflow.exe"
