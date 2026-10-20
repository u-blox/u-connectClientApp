@echo off
setlocal EnableDelayedExpansion

REM Build, flash and monitor u-connectClientApp on an STM32 Nucleo board.
REM Order of execution: clean, build, erase, flash, log.

set "ROOT=%~dp0"
set "SRC=%ROOT%u-connectClientApp\stm32"
set "BOARD=nucleo_h753zi"
set "BUILD=%ROOT%build\stm32_h753"
set "BIN=%ROOT%build\bin\ucxapp_nucleo_h753zi.bin"
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
if /i "%~1"=="--log"     goto handle_log
echo Unknown argument: %~1
goto show_help

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
set "PROG=C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
set "PATH=C:\Program Files\CMake\bin;C:\Program Files\Arm\GNU Toolchain mingw-w64-x86_64-arm-none-eabi\bin;%ROOT%..\u-connectMatter\tools;%PATH%"

if defined DO_CLEAN (
    echo [clean] %BUILD%
    if exist "%BUILD%" rmdir /s /q "%BUILD%"
    if exist "%ROOT%build\bin\ucxapp_nucleo_h753zi.elf" del /q "%ROOT%build\bin\ucxapp_nucleo_h753zi.*"
)

if defined DO_BUILD call :build || exit /b 1
if defined DO_RECOVER call :programmer -c port=SWD mode=UR reset=HWrst -e all || exit /b 1
if defined DO_ERASE call :programmer -c port=SWD mode=UR reset=HWrst -e all || exit /b 1
if defined DO_FLASH call :flash || exit /b 1
if defined DO_LOG call :log || exit /b 1
exit /b 0

:build
where ninja >nul 2>nul || (echo ERROR: ninja not found on PATH & exit /b 1)
where arm-none-eabi-gcc >nul 2>nul || (echo ERROR: arm-none-eabi-gcc not found & exit /b 1)
set "HAL="
if defined STM32CUBEH7_PATH set "HAL=%STM32CUBEH7_PATH%"
if not defined HAL if exist "%ROOT%u-connectClient\ports\extra\stm32h7\STM32CubeH7\Drivers" set "HAL=%ROOT%u-connectClient\ports\extra\stm32h7\STM32CubeH7"
if not defined HAL if exist "%ROOT%..\u-connectMatter\u-connectClient\ports\extra\stm32h7\STM32CubeH7\Drivers" set "HAL=%ROOT%..\u-connectMatter\u-connectClient\ports\extra\stm32h7\STM32CubeH7"
if not defined HAL (
    echo ERROR: STM32CubeH7 not found. Set STM32CUBEH7_PATH or run: git -C u-connectClient submodule update --init ports/extra/stm32h7/STM32CubeH7
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
call :programmer -c port=SWD mode=UR reset=HWrst -d "%BIN%" 0x08000000 -v -rst
exit /b %errorlevel%

:log
set "PORT_ARG="
if defined LOG_PORT set "PORT_ARG=-Port %LOG_PORT%"
powershell -NoProfile -ExecutionPolicy Bypass -File "%SRC%\stm32tool.ps1" -Action log %PORT_ARG%
exit /b %errorlevel%

:show_help
echo Usage: u-connectClientAppSTM32.cmd [options]
echo.
echo   --build          Build for NUCLEO-H753ZI
echo   --rebuild        Clean, then build
echo   --clean          Remove the STM32 build directory
echo   --erase          Erase the whole flash
echo   --recover        Same as --erase (unlocks a stuck target)
echo   --flash          Flash the built image over SWD (ST-Link)
echo   --log [COMx]     Interactive console on the ST-Link VCP (auto-detected)
echo.
echo Options combine and run in the order clean, build, erase, flash, log.
echo Example: u-connectClientAppSTM32.cmd --build --flash --log
echo HAL: set STM32CUBEH7_PATH or init the u-connectClient submodule.
exit /b 0
