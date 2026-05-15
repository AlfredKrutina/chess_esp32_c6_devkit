@echo off
REM Spustí lokální náhled downloads.html (cwd = gh-pages-ready, aby fungovaly landing/assets/...).
cd /d "%~dp0gh-pages-ready"
if exist serve.cmd (call serve.cmd %*) else (python -m http.server 8765)
