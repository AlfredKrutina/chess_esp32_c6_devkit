@echo off
REM Start local preview of downloads.html (cwd = gh-pages-ready so landing/assets/... resolve).
cd /d "%~dp0gh-pages-ready"
if exist serve.cmd (call serve.cmd %*) else (python -m http.server 8765)
