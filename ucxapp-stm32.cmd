@echo off
REM Kept for old scripts and habits; same as: ucxapp.cmd --stm32 ...
call "%~dp0ucxapp.cmd" --stm32 %*
exit /b %errorlevel%
