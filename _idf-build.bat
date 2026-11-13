@echo off
rem =====================================================================
rem  microByte / retro-go build helper (Windows + ESP-IDF 5.2.x)
rem ---------------------------------------------------------------------
rem  Runs whatever command you pass, with the working directory set to
rem  this folder.
rem
rem  Usage (run from cmd, PowerShell or the IDE terminal - double-clicking
rem  is useless because there'd be no command to run):
rem      _idf-build.bat python rg_tool.py --target=microbyte build-img
rem      _idf-build.bat python rg_tool.py --target=microbyte build launcher retro-core
rem      _idf-build.bat cmd            (just drop into an IDF shell here)
rem
rem  Two ways to get an IDF environment, tried in this order:
rem   1. If IDF_PATH is already set (you exported ESP-IDF yourself, e.g.
rem      from an "ESP-IDF Command Prompt"), it is used as-is.
rem   2. Otherwise Espressif's installer script is called. Set these two
rem      before running, or edit the defaults below:
rem          IDF_INSTALL_PATH  where ESP-IDF is installed (default D:\Espressif)
rem          IDF_TOOLS_ID      the esp-idf-XXXX id of your install. Run
rem                            "%IDF_INSTALL_PATH%\idf_cmd_init.bat" with no
rem                            arguments to list the available ids.
rem
rem  Why this exists:
rem   1. Git Bash sets MSYSTEM, and ESP-IDF's export.bat bails out with
rem      "This .bat file is for Windows CMD.EXE shell only." if it sees it.
rem      So MSYSTEM must be cleared first.
rem   2. idf_cmd_init.bat ends by calling export.bat without `call`, so the
rem      whole sequence has to live in one batch file.
rem   3. %~dp0 makes this folder-relative, so the tree can be moved freely.
rem =====================================================================

set MSYSTEM=

if not "%IDF_PATH%"=="" goto run

if "%IDF_INSTALL_PATH%"=="" set IDF_INSTALL_PATH=D:\Espressif
if "%IDF_TOOLS_ID%"=="" set IDF_TOOLS_ID=esp-idf-e4f2000e27312d9e332099c79f429b8b

if not exist "%IDF_INSTALL_PATH%\idf_cmd_init.bat" (
    echo.
    echo *** "%IDF_INSTALL_PATH%\idf_cmd_init.bat" not found. ***
    echo     Set IDF_INSTALL_PATH to your ESP-IDF install directory, or
    echo     export ESP-IDF yourself before calling this script.
    pause
    goto :eof
)

call "%IDF_INSTALL_PATH%\idf_cmd_init.bat" %IDF_TOOLS_ID%
if errorlevel 1 (
    echo.
    echo *** ESP-IDF environment setup failed. ***
    echo     Check IDF_TOOLS_ID - calling idf_cmd_init.bat with no id lists them.
    pause
    goto :eof
)

:run
cd /d "%~dp0"
%*
