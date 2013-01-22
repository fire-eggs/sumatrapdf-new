@ECHO OFF
SETLOCAL

REM assumes we're being run from top-level directory as:
REM scripts\build-release.bat

REM You can add Python to PATH if it's not already there
REM SET PATH=C:\Python;%PATH%

SET VCPATH="%~2\bin\x86_amd64\vcvarsx86_amd64.bat"
CALL scripts\vc64.bat %VCPATH%
IF ERRORLEVEL 1 EXIT /B 1

REM add our nasm.exe, mpress.exe and StripReloc.exe to the path
SET PATH=%CD%\bin;%PATH%

python -u -B scripts\build-release.py %1 %2 %3 %4 %5 %6 %7 %8