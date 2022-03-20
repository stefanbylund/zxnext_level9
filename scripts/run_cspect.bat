@echo off

REM This batch file needs to know in which directory CSpect.exe is located,
REM e.g. C:\cspect\. Add this directory to your PATH environment variable
REM or set the CSPECT_HOME environment variable to point to it.

if not defined CSPECT_HOME (
  for /f %%a in ('where CSpect.exe') do set CSPECT_HOME=%%~dpa
)

if not defined CSPECT_HOME (
  echo CSpect.exe cannot be found.
  exit /b
)

set GAMEDIR=%~dp0

@echo on

%CSPECT_HOME%\CSpect.exe -w4 -tv -zxnext -mmc=%GAMEDIR% %GAMEDIR%\level9_512.nex
