# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-License-Identifier: MIT

# A read-only look at a Windows PC: what the machine is (x64 or ARM64) and what the build of docs\dev\windows-build.ps1 has
# and lacks, for x64 and for a native ARM64 build. It installs nothing, changes no setting, does not start, stop or change
# any program or service, does not start the Visual Studio developer shell and writes no file. The only commands that it
# runs are `vswhere`, `git`, `python` and `pip show` (to read their versions).
#   powershell.exe -NoProfile -File .\docs\dev\windows-env-check.ps1 [-Arch arm64|x64|both]
[CmdletBinding()]
param(
    [ValidateSet('x64', 'arm64', 'both')]
    [string]$Arch = 'both',
    [string]$Repo
)

$ErrorActionPreference = 'Continue'
if (-not $Repo) {
    $Repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
}
$pinnedVcpkg = '4702e381dbbd9b54d3f7e1eb565cd8981c1814cd'
$qtVersion = '6.9.3'
$missing = @{ x64 = New-Object System.Collections.Generic.List[string]; arm64 = New-Object System.Collections.Generic.List[string] }

function Write-Item {
    param([string]$Status, [string]$Text)
    Write-Host ('[{0}] {1}' -f $Status.PadRight(7), $Text)
}

function Add-Missing {
    param([string[]]$Archs, [string]$Text)
    foreach ($a in $Archs) {
        $missing[$a].Add($Text)
    }
}

# The machine type that a PE file (an exe or a dll) is built for: x64, ARM64, x86
function Get-PeMachine {
    param([string]$Path)
    try {
        $stream = [System.IO.File]::OpenRead($Path)
    } catch {
        return 'unreadable'
    }
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

function Get-FileVersionText {
    param([string]$Path)
    try {
        return (Get-Item -LiteralPath $Path).VersionInfo.FileVersion
    } catch {
        return 'unknown'
    }
}

function Get-FirstLine {
    param([scriptblock]$Command)
    try {
        $output = & $Command 2>&1 | Select-Object -First 1
        return "$output".Trim()
    } catch {
        return ''
    }
}

Write-Host "Windows build environment check (read-only), $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss zzz')"
Write-Host "Repository: $Repo"
Write-Host ''

# ---------------------------------------------------------------- the machine
Write-Host '== Machine =='
$cpuArch = $null
try {
    $cpu = Get-CimInstance Win32_Processor | Select-Object -First 1
    $cpuArchCode = [int]$cpu.Architecture
    $cpuArch = switch ($cpuArchCode) { 0 { 'x86' } 5 { 'ARM' } 9 { 'x64' } 12 { 'ARM64' } default { "code $cpuArchCode" } }
    Write-Item 'INFO' "CPU (Win32_Processor.Architecture): $cpuArch ($cpuArchCode); $($cpu.Name)"
} catch {
    Write-Item 'WARN' "Win32_Processor could not be read: $_"
}
try {
    $os = Get-CimInstance Win32_OperatingSystem
    Write-Item 'INFO' "OS: $($os.Caption), build $($os.BuildNumber), $($os.OSArchitecture)"
} catch {
    Write-Item 'WARN' "Win32_OperatingSystem could not be read: $_"
}
try {
    $runtimeInfo = [System.Runtime.InteropServices.RuntimeInformation]
    Write-Item 'INFO' "RuntimeInformation: OSArchitecture=$($runtimeInfo::OSArchitecture) ProcessArchitecture=$($runtimeInfo::ProcessArchitecture)"
} catch {
    Write-Item 'INFO' 'RuntimeInformation is not available in this PowerShell.'
}
Write-Item 'INFO' "Environment: PROCESSOR_ARCHITECTURE=$env:PROCESSOR_ARCHITECTURE PROCESSOR_ARCHITEW6432=$env:PROCESSOR_ARCHITEW6432"
Write-Item 'INFO' "PROCESSOR_IDENTIFIER=$env:PROCESSOR_IDENTIFIER"
$thisProcess = (Get-Process -Id $PID).Path
if ($thisProcess) {
    Write-Item 'INFO' "This PowerShell ($thisProcess) is built for: $(Get-PeMachine $thisProcess)"
}
Write-Host '       The environment variables are inherited from the program that started this PowerShell: an x64 program on an ARM64'
Write-Host '       PC passes AMD64 on, even to a native ARM64 PowerShell. Trust the CPU line above them.'
$isArm64Pc = ($cpuArch -eq 'ARM64') -or ($env:PROCESSOR_IDENTIFIER -match 'ARM')
if ($isArm64Pc) {
    Write-Item 'OK' 'This PC has an ARM64 CPU: a native ARM64 build can run here.'
} else {
    Write-Item 'INFO' 'This PC does not look like an ARM64 PC: a native ARM64 build could not run here.'
    Add-Missing @('arm64') 'an ARM64 PC (this one has no ARM64 CPU)'
}
Write-Host ''

# ---------------------------------------------------------------- Visual Studio
Write-Host '== Visual Studio 2022 and the compilers =='
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = $null
if (-not (Test-Path $vswhere)) {
    Write-Item 'MISSING' "vswhere.exe ($vswhere): Visual Studio Installer is not installed"
    Add-Missing @('x64', 'arm64') 'Visual Studio 2022 Build Tools with the C++ workload'
} else {
    Write-Item 'INFO' "vswhere: $(Get-PeMachine $vswhere) program, $(Get-FileVersionText $vswhere)"
    try {
        $instances = & $vswhere -all -products '*' -format json | ConvertFrom-Json
        foreach ($instance in $instances) {
            Write-Item 'INFO' "instance: $($instance.displayName) $($instance.installationVersion) at $($instance.installationPath)"
        }
    } catch {
        Write-Item 'WARN' "vswhere -all failed: $_"
    }
    # the same query as windows-build.ps1 makes
    $vs = & $vswhere -latest -version '[17.0,18.0)' -products '*' `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) {
        Write-Item 'MISSING' 'no Visual Studio 2022 with Microsoft.VisualStudio.Component.VC.Tools.x86.x64 (what windows-build.ps1 looks for)'
        Add-Missing @('x64', 'arm64') 'Visual Studio 2022 with Microsoft.VisualStudio.Component.VC.Tools.x86.x64'
    } else {
        Write-Item 'OK' "windows-build.ps1 would use: $vs"
        $msvcRoot = Join-Path $vs 'VC\Tools\MSVC'
        $msvc = $null
        if (Test-Path $msvcRoot) {
            $msvc = Get-ChildItem $msvcRoot -Directory | Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
        }
        if (-not $msvc) {
            Write-Item 'MISSING' "no MSVC toolset in $msvcRoot"
            Add-Missing @('x64', 'arm64') 'an MSVC toolset (C++ workload)'
        } else {
            Write-Item 'INFO' "newest MSVC toolset: $($msvc.Name) (the program needs the runtime 14.$([version]$msvc.Name | ForEach-Object { $_.Minor }) or newer, see below)"
            $compilers = @{}
            foreach ($hostName in 'x64', 'arm64') {
                foreach ($targetName in 'x64', 'arm64') {
                    $cl = Join-Path $msvc.FullName "bin\Host$hostName\$targetName\cl.exe"
                    $compilers["$hostName->$targetName"] = (Test-Path $cl)
                    if (Test-Path $cl) {
                        Write-Item 'INFO' "compiler Host$hostName\$targetName\cl.exe: present ($(Get-PeMachine $cl) program)"
                    } else {
                        Write-Item 'INFO' "compiler Host$hostName\$targetName\cl.exe: not installed"
                    }
                }
            }
            if ($compilers['x64->x64'] -or $compilers['arm64->x64']) {
                Write-Item 'OK' 'x64 target: a compiler exists'
            } else {
                Write-Item 'MISSING' 'x64 target: no compiler'
                Add-Missing @('x64') 'the x64 MSVC compiler (VC.Tools.x86.x64)'
            }
            if ($compilers['arm64->arm64']) {
                Write-Item 'OK' 'arm64 target: the compiler runs natively (Hostarm64\arm64)'
            } elseif ($compilers['x64->arm64']) {
                Write-Item 'OK' 'arm64 target: a compiler exists (Hostx64\arm64; it runs emulated on an ARM64 PC, slower)'
            } else {
                Write-Item 'MISSING' 'arm64 target: no ARM64 compiler (the component Microsoft.VisualStudio.Component.VC.Tools.ARM64, MSVC v143 ARM64/ARM64EC build tools, is not installed)'
                Add-Missing @('arm64') 'Visual Studio component Microsoft.VisualStudio.Component.VC.Tools.ARM64 (MSVC v143 - VS 2022 C++ ARM64/ARM64EC build tools)'
            }
        }
        foreach ($file in 'VC\Auxiliary\Build\vcvarsarm64.bat', 'VC\Auxiliary\Build\vcvarsamd64_arm64.bat', 'Common7\Tools\VsDevCmd.bat') {
            Write-Item 'INFO' ("{0}: {1}" -f $file, $(if (Test-Path (Join-Path $vs $file)) { 'present' } else { 'absent' }))
        }
        $cmakeTools = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake'
        foreach ($tool in 'CMake\bin\cmake.exe', 'Ninja\ninja.exe') {
            $path = Join-Path $cmakeTools $tool
            if (Test-Path $path) {
                Write-Item 'OK' "$tool ($(Get-PeMachine $path) program)"
            } else {
                Write-Item 'MISSING' "$tool of Visual Studio (component Microsoft.VisualStudio.Component.VC.CMake.Project)"
                Add-Missing @('x64', 'arm64') "Visual Studio component VC.CMake.Project ($tool)"
            }
        }
    }
    $sdkRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Lib'
    if (Test-Path $sdkRoot) {
        $sdk = Get-ChildItem $sdkRoot -Directory | Where-Object { $_.Name -match '^10\.' } | Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
        if ($sdk) {
            foreach ($target in 'x64', 'arm64') {
                $lib = Join-Path $sdk.FullName "um\$target\kernel32.lib"
                if (Test-Path $lib) {
                    Write-Item 'OK' "Windows SDK $($sdk.Name): libraries for $target"
                } else {
                    Write-Item 'MISSING' "Windows SDK $($sdk.Name): no libraries for $target"
                    Add-Missing @($target) "Windows SDK libraries for $target"
                }
            }
        }
    } else {
        Write-Item 'MISSING' "Windows SDK ($sdkRoot)"
        Add-Missing @('x64', 'arm64') 'a Windows 10/11 SDK'
    }
}
Write-Host ''

# ---------------------------------------------------------------- the C++ runtime
Write-Host '== Visual C++ runtime (the program refuses to start with an older one than its compiler) =='
foreach ($name in 'vcruntime140.dll', 'msvcp140.dll') {
    $path = Join-Path $env:SystemRoot "System32\$name"
    if (Test-Path $path) {
        Write-Item 'INFO' "System32\$name : $(Get-FileVersionText $path), $(Get-PeMachine $path)"
    } else {
        Write-Item 'INFO' "System32\$name : absent"
    }
}
if ($isArm64Pc) {
    $arm64Runtime = Join-Path $env:SystemRoot 'System32\vcruntime140.dll'
    if (-not (Test-Path $arm64Runtime)) {
        Write-Item 'MISSING' 'the ARM64 Visual C++ Redistributable (https://aka.ms/vc14/vc_redist.arm64.exe)'
        Add-Missing @('arm64') 'the ARM64 Visual C++ Redistributable (vc_redist.arm64.exe)'
    }
}
Write-Host ''

# ---------------------------------------------------------------- tools
Write-Host '== Tools =='
$git = Get-Command git -ErrorAction SilentlyContinue
if ($git) {
    Write-Item 'OK' "git: $(Get-FirstLine { git --version }) ($(Get-PeMachine $git.Source) program)"
    Write-Item 'INFO' "checkout: $(Get-FirstLine { git -C $Repo rev-parse --short=9 HEAD }) on $(Get-FirstLine { git -C $Repo branch --show-current }); tag v1.27.0: $(if (Get-FirstLine { git -C $Repo tag --list v1.27.0 }) { 'present' } else { 'ABSENT (git fetch --tags)' })"
    Write-Item 'INFO' "commits since v1.27.0 (the N of the version 1.27.0.N): $(Get-FirstLine { git -C $Repo rev-list --count v1.27.0..HEAD })"
} else {
    Write-Item 'MISSING' 'git'
    Add-Missing @('x64', 'arm64') 'git'
}
$python = Join-Path $Repo '.venv\Scripts\python.exe'
if (Test-Path $python) {
    Write-Item 'OK' "venv python: $(Get-FirstLine { & $python --version }) ($(Get-PeMachine $python) program)"
    $aqt = (& $python -m pip show aqtinstall 2>$null | Select-String '^Version:') -replace 'Version:\s*', ''
    if ($aqt) {
        Write-Item 'OK' "aqtinstall $aqt in .venv (3.3.0 is the one that these notes were checked with)"
    } else {
        Write-Item 'MISSING' 'aqtinstall in .venv'
        Add-Missing @('x64', 'arm64') 'aqtinstall (pip) in .venv, to install Qt'
    }
} else {
    $sysPython = Get-Command python -ErrorAction SilentlyContinue
    if ($sysPython) {
        Write-Item 'INFO' "no .venv in the repository; python on PATH: $(Get-FirstLine { python --version }) ($(Get-PeMachine $sysPython.Source) program)"
    } else {
        Write-Item 'MISSING' 'python (and the .venv of the repository)'
    }
    Add-Missing @('x64', 'arm64') 'a .venv with aqtinstall (python -m venv .venv; pip install aqtinstall==3.3.0)'
}
Write-Item 'INFO' "winget: $(if (Get-Command winget -ErrorAction SilentlyContinue) { 'present' } else { 'absent' })"
try {
    $drive = (Get-Item $Repo).PSDrive
    Write-Item 'INFO' ("free space on {0}: {1:N1} GB (a build with its packages needs about 5 GB per architecture)" -f $drive.Name, ($drive.Free / 1GB))
} catch {
}
Write-Host ''

# ---------------------------------------------------------------- Qt
Write-Host "== Qt $qtVersion in the repository (deps\Qt) =="
$kits = @{ x64 = "msvc2022_64"; arm64 = "msvc2022_arm64" }
foreach ($target in 'x64', 'arm64') {
    $root = Join-Path $Repo "deps\Qt\$qtVersion\$($kits[$target])"
    if (-not (Test-Path $root)) {
        Write-Item 'MISSING' "$($kits[$target]): $root"
        Add-Missing @($target) "Qt $qtVersion for $target ($($kits[$target]))"
        continue
    }
    $config = Test-Path (Join-Path $root 'lib\cmake\Qt6\Qt6Config.cmake')
    $moc = Join-Path $root 'bin\moc.exe'
    $deploy = Join-Path $root 'bin\windeployqt.exe'
    $crossKit = Test-Path (Join-Path $root 'bin\target_qt.conf')
    $linguist = Test-Path (Join-Path $root 'lib\cmake\Qt6LinguistTools\Qt6LinguistToolsConfig.cmake')
    $test = Test-Path (Join-Path $root 'lib\cmake\Qt6Test\Qt6TestConfig.cmake')
    $mocMachine = if (Test-Path $moc) { Get-PeMachine $moc } else { 'absent' }
    $coreMachine = Get-PeMachine (Join-Path $root 'bin\Qt6Core.dll')
    Write-Item 'INFO' "$($kits[$target]): Qt6Config=$config Test=$test LinguistTools=$linguist moc.exe=$mocMachine windeployqt.exe=$(Test-Path $deploy) Qt6Core.dll=$coreMachine crossKit=$crossKit"
    $wanted = if ($target -eq 'arm64') { 'ARM64' } else { 'x64' }
    $ok = $config -and $test -and $linguist -and (Test-Path $deploy) -and ($coreMachine -eq $wanted)
    if ($target -eq 'arm64') {
        $ok = $ok -and ($mocMachine -eq 'ARM64')
    }
    if ($ok) {
        Write-Item 'OK' "$($kits[$target]) is complete and built for $wanted"
    } elseif ($crossKit) {
        Write-Item 'MISSING' "$($kits[$target]) is the cross-compiled kit (no host tools): windows-build.ps1 needs the native one: aqt install-qt windows_arm64 desktop $qtVersion win64_msvc2022_arm64"
        Add-Missing @($target) "the native Qt for ARM64 (windows_arm64 / win64_msvc2022_arm64), not the cross-compiled one"
    } else {
        Write-Item 'MISSING' "$($kits[$target]) is incomplete or not built for $wanted"
        Add-Missing @($target) "a complete Qt $qtVersion for $target"
    }
}
Write-Host ''

# ---------------------------------------------------------------- vcpkg and the build directories
Write-Host '== vcpkg (deps\vcpkg) and the build directories =='
$vcpkg = Join-Path $Repo 'deps\vcpkg'
if (Test-Path (Join-Path $vcpkg 'scripts\buildsystems\vcpkg.cmake')) {
    $revision = if ($git) { Get-FirstLine { git -C $vcpkg rev-parse HEAD } } else { '' }
    $exe = Join-Path $vcpkg 'vcpkg.exe'
    Write-Item $(if (Test-Path $exe) { 'OK' } else { 'MISSING' }) "vcpkg.exe: $(if (Test-Path $exe) { Get-PeMachine $exe } else { 'absent (bootstrap-vcpkg.bat -disableMetrics)' })"
    if (-not (Test-Path $exe)) {
        Add-Missing @('x64', 'arm64') 'vcpkg.exe (bootstrap-vcpkg.bat)'
    }
    Write-Item $(if ($revision -eq $pinnedVcpkg) { 'OK' } else { 'WARN' }) "vcpkg revision: $revision (pinned: $pinnedVcpkg)"
    foreach ($triplet in 'x64-windows', 'arm64-windows') {
        Write-Item 'INFO' ("triplet {0}: {1}" -f $triplet, $(if (Test-Path (Join-Path $vcpkg "triplets\$triplet.cmake")) { 'defined' } else { 'NOT defined' }))
    }
} else {
    Write-Item 'MISSING' "deps\vcpkg ($vcpkg)"
    Add-Missing @('x64', 'arm64') 'vcpkg at the pinned revision in deps\vcpkg'
}
foreach ($dir in (Get-ChildItem (Join-Path $Repo 'build') -Directory -Filter 'windows-*' -ErrorAction SilentlyContinue)) {
    $triplets = Get-ChildItem (Join-Path $dir.FullName 'vcpkg_installed') -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -match 'windows' } | ForEach-Object { $_.Name }
    $core = Join-Path $dir.FullName 'bin\deskflow-core.exe'
    $coreText = if (Test-Path $core) { "deskflow-core.exe $(Get-PeMachine $core)" } else { 'no deskflow-core.exe' }
    Write-Item 'INFO' "build\$($dir.Name): $coreText; vcpkg packages: $($triplets -join ', ')"
}
Write-Host ''

# ---------------------------------------------------------------- the installed Deskflow
Write-Host '== The installed Deskflow (only read) =='
$installed = @()
foreach ($key in 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*', 'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*') {
    $installed += Get-ItemProperty $key -ErrorAction SilentlyContinue | Where-Object { $_.DisplayName -like 'Deskflow*' }
}
foreach ($entry in $installed) {
    Write-Item 'INFO' "installed package: $($entry.DisplayName) $($entry.DisplayVersion) at $($entry.InstallLocation)"
}
$locations = @($installed | ForEach-Object { $_.InstallLocation } | Where-Object { $_ })
$locations += (Join-Path $env:ProgramFiles 'Deskflow')
foreach ($location in ($locations | Select-Object -Unique)) {
    foreach ($name in 'deskflow.exe', 'deskflow-core.exe', 'deskflow-daemon.exe') {
        $path = Join-Path $location $name
        if (Test-Path $path) {
            Write-Item 'INFO' "$path : $(Get-PeMachine $path), $(Get-FileVersionText $path)"
        }
    }
    foreach ($dll in (Get-ChildItem $location -Filter 'libssl-3*.dll' -ErrorAction SilentlyContinue)) {
        Write-Item 'INFO' "$($dll.FullName) : $(Get-PeMachine $dll.FullName)"
    }
}
$service = Get-Service -Name 'Deskflow' -ErrorAction SilentlyContinue
if ($service) {
    Write-Item 'INFO' "service Deskflow: $($service.Status), start type $($service.StartType) (not touched)"
}
foreach ($process in (Get-Process -Name 'deskflow*' -ErrorAction SilentlyContinue)) {
    $machine = if ($process.Path) { Get-PeMachine $process.Path } else { 'path not readable' }
    Write-Item 'INFO' "running: $($process.Name) (id $($process.Id)), $machine, $($process.Path)"
}
Write-Host ''

# ---------------------------------------------------------------- summary
Write-Host '== Summary =='
foreach ($target in 'x64', 'arm64') {
    if ($Arch -ne 'both' -and $Arch -ne $target) {
        continue
    }
    if ($missing[$target].Count -eq 0) {
        Write-Item 'OK' "$target build: everything that windows-build.ps1 -Arch $target checks is there."
    } else {
        Write-Item 'MISSING' "$target build: $($missing[$target].Count) thing(s) missing:"
        foreach ($item in $missing[$target]) {
            Write-Host "           - $item"
        }
    }
}
Write-Host 'Nothing was installed or changed. Send this whole output back.'
