# 4K film queue (EEVEE). Masters stay in models/blender/video/*_4k.mp4 (gitignored).
# Web HLS is encoded separately by encode_web.py.

$ErrorActionPreference = "Continue"
$blender = "C:\Users\alfid\AppData\Local\Programs\blender-5.0.1-windows-x64\blender.exe"
$wd = "d:\projects\Programming\Git\chess_esp32_c6_devkit\models\blender"
$logDir = Join-Path $wd "video"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
Set-Location $wd

function Invoke-BlenderJob([string]$name, [string]$script, [string[]]$extra) {
  $log = Join-Path $logDir "_render_$name.log"
  Write-Host "==== $name ===="
  $pass = @("-b", "--factory-startup", "--python", $script, "--") + $extra
  & $blender @pass 2>&1 | Tee-Object -FilePath $log
  if ($LASTEXITCODE -ne 0) {
    Write-Host "FAILED $name exit $LASTEXITCODE"
    return $false
  }
  Write-Host "OK $name"
  return $true
}

Invoke-BlenderJob "stills" "$wd\render_web_stills.py" @() | Out-Null
py -3.12 "$wd\convert_web_stills.py"
py -3.12 "$wd\make_black_piece_webps.py"
Invoke-BlenderJob "predstaveni_4k" "$wd\build_predstaveni.py" @("--res", "3840x2160", "--samples", "24") | Out-Null
Invoke-BlenderJob "rozklad_4k" "$wd\build_rozklad.py" @("--res", "3840x2160", "--samples", "24") | Out-Null
Invoke-BlenderJob "zive_4k" "$wd\build_predstaveni.py" @("--variant", "zive", "--res", "3840x2160", "--samples", "24") | Out-Null
Invoke-BlenderJob "hraci_4k" "$wd\build_hraci.py" @("--final", "--res", "3840x2160", "--samples", "48") | Out-Null

Write-Host "4K queue finished"
py -3.12 "$wd\encode_web.py"
Write-Host "web encode finished"
