# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-License-Identifier: MIT
<#
.SYNOPSIS
  tcp_probe.ps1 - read-only TCP reachability probe for Windows PowerShell 5.1 / PowerShell 7.

.DESCRIPTION
  UNTESTED ON WINDOWS. It was written against the standard .NET TcpClient API but has not been run on a Windows
  computer yet (the author had no Windows or pwsh available). If it misbehaves, the Python version tcp_probe.py
  in the same folder does the same and was tested.

  It makes ONE ordinary TCP connect to Target:Port and says whether it opened, timed out (silent drop) or failed
  with a socket error. It sends no data and closes the connection at once. A Deskflow server logs it as a
  connection that went away; that is harmless.

.EXAMPLE
  .\tcp_probe.ps1 -Target 10.0.0.5
  (10.0.0.5 stands for the IP of the computer that runs the Deskflow server; the port defaults to 24800)

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File .\tcp_probe.ps1 -Target 10.0.0.5 -Port 24800 -TimeoutMs 3000
  (when scripts are blocked by the execution policy)
#>
param([Parameter(Mandatory)][string]$Target, [int]$Port = 24800, [int]$TimeoutMs = 3000)
$sw = [Diagnostics.Stopwatch]::StartNew(); $c = New-Object Net.Sockets.TcpClient
try {
  $r = $c.BeginConnect($Target, $Port, $null, $null)
  if (-not $r.AsyncWaitHandle.WaitOne($TimeoutMs)) { "TIMEOUT after $TimeoutMs ms: silent drop (firewall DROP / client isolation / ACL / wrong subnet / host off)" }
  else {
    try { $c.EndConnect($r); "OPEN in $($sw.ElapsedMilliseconds) ms: network path and port are fine" }
    catch { $e = $_.Exception.InnerException; if (-not $e) { $e = $_.Exception }
            "FAILED in $($sw.ElapsedMilliseconds) ms: $($e.SocketErrorCode) - $($e.Message)  (ConnectionRefused = reachable but nothing listening/REJECT; HostUnreachable/NetworkUnreachable = no route)" }
  }
} finally { $c.Close() }
