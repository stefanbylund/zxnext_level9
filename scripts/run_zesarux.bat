@echo off

REM This batch file needs to know in which directory zesarux.exe is located,
REM e.g. C:\ZEsarUX_win-7.0\. Add this directory to your PATH environment
REM variable or set the ZESARUX_HOME environment variable to point to it.

if not defined ZESARUX_HOME (
  for /f %%a in ('where zesarux.exe') do set ZESARUX_HOME=%%~dpa
)

if not defined ZESARUX_HOME (
  echo zesarux.exe cannot be found.
  exit /b
)

set GAMEDIR=%~dp0

@echo on

cd %ZESARUX_HOME% && zesarux.exe --noconfigfile --machine tbblue --enabletimexvideo --enablekempstonmouse --tbblue-fast-boot-mode --quickexit --enable-esxdos-handler --esxdos-root-dir %GAMEDIR% %GAMEDIR%\level9_512.nex
