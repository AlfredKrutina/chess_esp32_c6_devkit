@echo off
REM Local preview of downloads.html (HTTP). Usage: serve.cmd   or   serve.cmd 9000
cd /d "%~dp0"
if "%~1"=="" (set "PORT=8765") else set "PORT=%~1"
where python >nul 2>&1 && goto :run_python
where py >nul 2>&1 && goto :run_py
echo [serve.cmd] Python not found. Add python.exe to PATH or use: py -m http.server %PORT%
exit /b 1

:run_python
python -m http.server %PORT%
goto :eof

:run_py
py -m http.server %PORT%
