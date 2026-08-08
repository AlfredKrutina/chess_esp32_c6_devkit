# Local preview of downloads.html over HTTP (PowerShell).
# Usage: .\serve.ps1   or   .\serve.ps1 -Port 9000
param([int]$Port = 8765)
Set-Location $PSScriptRoot
if (Get-Command python -ErrorAction SilentlyContinue) {
  python -m http.server $Port
} elseif (Get-Command py -ErrorAction SilentlyContinue) {
  py -m http.server $Port
} else {
  Write-Error "Python not found. Install Python 3 or: npx --yes serve -s . -l $Port"
  exit 1
}
