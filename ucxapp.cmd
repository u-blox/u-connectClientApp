@echo off
REM Thin Windows caller for ucxapp.py (build, run, --linux, --stm32, --detect, --test).
setlocal
set "PY="
where py >nul 2>nul && set "PY=py -3"
if not defined PY where python >nul 2>nul && set "PY=python"
if not defined PY (
    echo ERROR: Python 3 not found. Install it from https://www.python.org/downloads/
    echo        or start app\bin\ucxapp.exe directly.
    exit /b 1
)
%PY% "%~dp0ucxapp.py" %*
exit /b %errorlevel%
