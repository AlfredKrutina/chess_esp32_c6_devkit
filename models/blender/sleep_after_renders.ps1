# Sleep the PC (standby, not shutdown/hibernate) after all CzechMate Blender
# and HLS jobs finish. Leaves unrelated python (http.server, headphones) alone.

$ErrorActionPreference = "Continue"
$log = "d:\projects\Programming\Git\chess_esp32_c6_devkit\models\blender\video\_sleep_after_renders.log"
$quietNeededSec = 180
$pollSec = 20
$quietSince = $null

function Write-Log([string]$msg) {
  $line = "{0:yyyy-MM-dd HH:mm:ss} {1}" -f (Get-Date), $msg
  Add-Content -Path $log -Value $line -Encoding utf8
  Write-Host $line
}

function Get-BusyJobs {
  $busy = @()
  foreach ($p in Get-CimInstance Win32_Process) {
    $name = $p.Name
    $cmd = [string]$p.CommandLine
    if ($name -match '^blender') {
      $busy += "blender pid=$($p.ProcessId)"
      continue
    }
    if ($name -match '^ffmpeg' -and $cmd -match 'chess_esp32_c6_devkit|encode_web|\\hls\\') {
      $busy += "ffmpeg pid=$($p.ProcessId)"
      continue
    }
    if ($name -match '^python' -and $cmd -match 'encode_web\.py|convert_web_stills\.py|make_black_piece_webps\.py') {
      $busy += "python pid=$($p.ProcessId) encode"
      continue
    }
    if ($name -match '^powershell' -and $cmd -match 'render_4k_queue\.ps1') {
      $busy += "queue pid=$($p.ProcessId)"
      continue
    }
  }
  return $busy
}

function Invoke-SleepOnly {
  Add-Type -TypeDefinition @"
using System.Runtime.InteropServices;
public static class CzSleep {
  [DllImport("powrprof.dll", SetLastError = true)]
  public static extern bool SetSuspendState(bool hibernate, bool forceCritical, bool disableWakeEvent);
}
"@
  $ok = [CzSleep]::SetSuspendState($false, $false, $false)
  $err = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
  Write-Log ("SetSuspendState sleep ok={0} err={1}" -f $ok, $err)
}

New-Item -ItemType Directory -Force -Path (Split-Path $log) | Out-Null
Write-Log "watcher start quiet=${quietNeededSec}s"

while ($true) {
  $busy = @(Get-BusyJobs)
  if ($busy.Count -gt 0) {
    $quietSince = $null
    Write-Log ("busy: " + ($busy -join "; "))
  } else {
    if ($null -eq $quietSince) { $quietSince = Get-Date }
    $idle = ((Get-Date) - $quietSince).TotalSeconds
    Write-Log ("idle {0:n0}s / {1}s" -f $idle, $quietNeededSec)
    if ($idle -ge $quietNeededSec) {
      Write-Log "all renders idle - sleeping PC (not shutdown)"
      Invoke-SleepOnly
      break
    }
  }
  Start-Sleep -Seconds $pollSec
}

Write-Log "watcher exit"
