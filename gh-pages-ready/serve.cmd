@echo off
REM Lokální náhled downloads.html (HTTP). Použití: serve.cmd   nebo   serve.cmd 9000
cd /d "%~dp0"
if "%~1"=="" (set "PORT=8765") else set "PORT=%~1"
where python >nul 2>&1 && goto :run_python
where py >nul 2>&1 && goto :run_py
echo [serve.cmd] Nenalezen Python. Do PATH dej python.exe nebo pouzij: py -m http.server %PORT%
exit /b 1

:run_python
python -m http.server %PORT%
goto :eof

:run_py
py -m http.server %PORT%
