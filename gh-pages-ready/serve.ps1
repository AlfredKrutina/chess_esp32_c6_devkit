# Lokální náhled downloads.html přes HTTP (PowerShell).
# Použití: .\serve.ps1   nebo   .\serve.ps1 -Port 9000
param([int]$Port = 8765)
Set-Location $PSScriptRoot
if (Get-Command python -ErrorAction SilentlyContinue) {
  python -m http.server $Port
} elseif (Get-Command py -ErrorAction SilentlyContinue) {
  py -m http.server $Port
} else {
  Write-Error "Není Python. Nainstaluj Python 3 nebo: npx --yes serve -s . -l $Port"
  exit 1
}
