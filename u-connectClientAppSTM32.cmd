@echo off
setlocal EnableDelayedExpansion

REM Build, flash and monitor u-connectClientApp on an STM32 Nucleo board.
REM Order of execution: clean, build, erase, flash, log.

set "ROOT=%~dp0"
set "SELF=%~f0"
set "SRC=%ROOT%u-connectClientApp\stm32"
set "VARIANT=h753"
set "DO_CLEAN=" & set "DO_BUILD=" & set "DO_ERASE=" & set "DO_FLASH=" & set "DO_LOG=" & set "DO_RECOVER="
set "LOG_PORT="

if "%~1"=="" goto show_help

:parse_loop
if "%~1"=="" goto parsed
if /i "%~1"=="--help"    goto show_help
if /i "%~1"=="--clean"   set "DO_CLEAN=1" & shift & goto parse_loop
if /i "%~1"=="--build"   set "DO_BUILD=1" & shift & goto parse_loop
if /i "%~1"=="--rebuild" set "DO_CLEAN=1" & set "DO_BUILD=1" & shift & goto parse_loop
if /i "%~1"=="--erase"   set "DO_ERASE=1" & shift & goto parse_loop
if /i "%~1"=="--recover" set "DO_RECOVER=1" & shift & goto parse_loop
if /i "%~1"=="--flash"   set "DO_FLASH=1" & shift & goto parse_loop
if /i "%~1"=="--board"   goto handle_board
if /i "%~1"=="--all-worker" goto all_worker
if /i "%~1"=="--log"     goto handle_log
echo Unknown argument: %~1
goto show_help

:handle_board
set "VARIANT=%~2"
shift
shift
goto parse_loop

:handle_log
set "DO_LOG=1"
shift
set "NEXT=%~1"
if "%NEXT%"=="" goto parse_loop
if "%NEXT:~0,2%"=="--" goto parse_loop
set "LOG_PORT=%NEXT%"
shift
goto parse_loop

:parsed
if /i "%VARIANT%"=="all" goto build_all
if /i "%VARIANT%"=="h753" (
    set "BOARD=nucleo_h753zi" & set "BOARD_NAME=NUCLEO-H753ZI" & set "FAMILY=H7" & set "HAL_DIR=STM32CubeH7" & set "HAL_ENV=%STM32CUBEH7_PATH%" & set "PORTDIR=stm32h7"
) else if /i "%VARIANT%"=="f767" (
    set "BOARD=nucleo_f767zi" & set "BOARD_NAME=NUCLEO-F767ZI" & set "FAMILY=F7" & set "HAL_DIR=STM32CubeF7" & set "HAL_ENV=%STM32CUBEF7_PATH%" & set "PORTDIR=stm32f7"
) else if /i "%VARIANT%"=="f439" (
    set "BOARD=nucleo_f439zi" & set "BOARD_NAME=NUCLEO-F439ZI" & set "FAMILY=F4" & set "HAL_DIR=STM32CubeF4" & set "HAL_ENV=%STM32CUBEF4_PATH%" & set "PORTDIR=stm32f4"
) else (
    echo ERROR: unknown board "%VARIANT%" - use h753, f767 or f439
    exit /b 1
)
set "BUILD=%ROOT%build\stm32_%VARIANT%"
set "BIN=%ROOT%build\bin\ucxapp_%BOARD%.bin"
set "PROG=C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
set "PATH=C:\Program Files\CMake\bin;C:\Program Files\Arm\GNU Toolchain mingw-w64-x86_64-arm-none-eabi\bin;%ROOT%..\u-connectMatter\tools;%PATH%"

if defined DO_CLEAN (
    echo [clean] %BUILD%
    if exist "%BUILD%" rmdir /s /q "%BUILD%"
    if exist "%BIN%" del /q "%ROOT%build\bin\ucxapp_%BOARD%.*"
)

if defined DO_BUILD call :build || exit /b 1
if defined DO_RECOVER call :programmer -c port=SWD mode=UR reset=HWrst -e all || exit /b 1
if defined DO_ERASE call :programmer -c port=SWD mode=UR reset=HWrst -e all || exit /b 1
if defined DO_FLASH call :flash || exit /b 1
if defined DO_LOG call :log || exit /b 1
exit /b 0

:all_worker
call "%SELF%" --board %~2 %ALL_ARGS% > "%ROOT%build\all_%~2.log" 2>&1
>"%ROOT%build\all_%~2.rc" echo %errorlevel%
exit /b 0

:build_all
REM Runs --clean/--build for h753, f767 and f439 in parallel, one log per board in build\.
if defined DO_ERASE echo ERROR: --board all supports only --clean, --build, --rebuild and --flash & exit /b 1
if defined DO_RECOVER echo ERROR: --board all supports only --clean, --build, --rebuild and --flash & exit /b 1
if defined DO_LOG echo ERROR: --board all supports only --clean, --build, --rebuild and --flash & exit /b 1
if not defined DO_CLEAN if not defined DO_BUILD if not defined DO_FLASH (echo ERROR: --board all needs --build, --clean, --rebuild or --flash & exit /b 1)
set "ALL_ARGS="
if defined DO_CLEAN set "ALL_ARGS=--clean"
if defined DO_BUILD set "ALL_ARGS=%ALL_ARGS% --build"
if defined DO_FLASH set "ALL_ARGS=%ALL_ARGS% --flash"
if not exist "%ROOT%build" mkdir "%ROOT%build"
for %%B in (h753 f767 f439) do (
    del /q "%ROOT%build\all_%%B.rc" 2>nul
    start "" /b cmd /c call "%SELF%" --all-worker %%B
)
:wait_all
set "PENDING="
for %%B in (h753 f767 f439) do if not exist "%ROOT%build\all_%%B.rc" set "PENDING=1"
if defined PENDING (
    ping -n 2 127.0.0.1 >nul
    goto wait_all
)
set "ALL_FAILED="
for %%B in (h753 f767 f439) do (
    set "RC="
    set /p RC=<"%ROOT%build\all_%%B.rc"
    set "RC=!RC: =!"
    if "!RC!"=="0" (echo [%%B] ok) else (echo [%%B] FAILED, see build\all_%%B.log & set "ALL_FAILED=1")
)
if defined ALL_FAILED exit /b 1
exit /b 0

:build
where ninja >nul 2>nul || (echo ERROR: ninja not found on PATH & exit /b 1)
where arm-none-eabi-gcc >nul 2>nul || (echo ERROR: arm-none-eabi-gcc not found & exit /b 1)
set "HAL=%HAL_ENV%"
if not defined HAL if exist "%ROOT%u-connectClient\ports\extra\%PORTDIR%\%HAL_DIR%\Drivers" set "HAL=%ROOT%u-connectClient\ports\extra\%PORTDIR%\%HAL_DIR%"
if not defined HAL if exist "%ROOT%..\u-connectMatter\u-connectClient\ports\extra\%PORTDIR%\%HAL_DIR%\Drivers" set "HAL=%ROOT%..\u-connectMatter\u-connectClient\ports\extra\%PORTDIR%\%HAL_DIR%"
if not defined HAL (
    echo ERROR: %HAL_DIR% not found. Set STM32CUBE%FAMILY%_PATH or run: git -C u-connectClient submodule update --init ports/extra/%PORTDIR%/%HAL_DIR%
    exit /b 1
)
if not exist "%BUILD%\build.ninja" (
    cmake -G Ninja -S "%SRC%" -B "%BUILD%" -DCMAKE_TOOLCHAIN_FILE="%ROOT%u-connectClient\cmake\arm-none-eabi-gcc.cmake" -DSTM32_BOARD=%BOARD% -DSTM32_HAL_PATH="%HAL%" -DCMAKE_BUILD_TYPE=Release || exit /b 1
)
cmake --build "%BUILD%" || exit /b 1
echo [build] %BIN%
exit /b 0

:programmer
if not exist "%PROG%" (echo ERROR: STM32CubeProgrammer not found & exit /b 1)
"%PROG%" %*
exit /b %errorlevel%

:flash
if not exist "%BIN%" (echo ERROR: %BIN% missing, use --build & exit /b 1)
call :find_stlink
if not defined STLINK_SN (echo ERROR: no ST-Link found for %BOARD_NAME% & exit /b 1)
echo [flash] %BOARD_NAME% via ST-Link %STLINK_SN%
call :programmer -c port=SWD sn=%STLINK_SN% mode=UR reset=HWrst -d "%BIN%" 0x08000000 -v -rst
exit /b %errorlevel%

:find_stlink
REM The programmer lists each ST-Link as an SN line followed by a Board Name line.
set "STLINK_SN=" & set "LAST_SN="
for /f "tokens=1,* delims=:" %%a in ('cmd /c ""%PROG%" -l st"') do (
    set "K=%%a"
    set "V=%%b"
    set "V=!V: =!"
    if not "!K:ST-LINK SN=!"=="!K!" set "LAST_SN=!V!"
    if not "!K:Board Name=!"=="!K!" if "!V!"=="%BOARD_NAME%" if not defined STLINK_SN set "STLINK_SN=!LAST_SN!"
)
exit /b 0

:log
set "PORT_ARG="
if defined LOG_PORT set "PORT_ARG=-Port %LOG_PORT%"
powershell -NoProfile -ExecutionPolicy Bypass -File "%SRC%\stm32tool.ps1" -Action log %PORT_ARG%
exit /b %errorlevel%, NUCLEO-F439ZI (f439) or all
echo                    (all: --build/--clean/--rebuild/--flash only, the three boards in parallel, logs in build\all_*.log

:show_help
echo Usage: u-connectClientAppSTM32.cmd [--board h753^|f767^|f439] [options]
echo.
echo   --board NAME     NUCLEO-H753ZI (h753, default), NUCLEO-F767ZI (f767) or NUCLEO-F439ZI (f439)
echo   --build          Build for the selected board
echo   --rebuild        Clean, then build
echo   --clean          Remove the board's build directory
echo   --erase          Erase the whole flash
echo   --recover        Same as --erase (unlocks a stuck target)
echo   --flash          Flash the built image over SWD (ST-Link)
echo   --log [COMx]     Interactive console on the ST-Link VCP (auto-detected)
echo.
echo Options combine and run in the order clean, build, erase, flash, log.
echo Example: u-connectClientAppSTM32.cmd --board f767 --build --flash --log
echo HAL: set STM32CUBEH7_PATH / STM32CUBEF7_PATH / STM32CUBEF4_PATH or init the u-connectClient submodules.
exit /b 0
