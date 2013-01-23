@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION

REM assumes we're being run from top-level directory as:
REM scripts\build-release.bat

REM You can add Python to PATH if it's not already there
REM SET PATH=C:\Python;%PATH%

SET var1=%1
SET var2=%2
SET var3=%3
SET var4=%4
SET var5=%5
SET var6=%6
SET var7=%7
SET var8=%8
SET var9=%9

SET VCINSTALL=""

FOR %%A IN (var1 var2 var3 var4 var5 var6 var7 var8 var9) DO (

  SET var=!%%A!

  IF "!var!"=="" (
    @ECHO "Parameter is empty."
    @ECHO "Set parameter to "test.""
    SET var="test"
  )

  IF NOT "!var!"=="test" (

    @ECHO "Parameter is not Empty. Need to check if it ends with \""

    SET varEnd=!var:~-2,1!

    IF !varEnd!==\ (
      @ECHO "Parameter ends with N\""
      SET %%A=!var:~0,-2!"
      SET VCINSTALL=!var!
    )
  )
)

SET VCPATH="!VCINSTALL:"=!\bin\x86_amd64\vcvarsx86_amd64.bat"
CALL scripts\vc64.bat %VCPATH%
REM IF ERRORLEVEL 1 EXIT /B 1

REM add our nasm.exe, mpress.exe and StripReloc.exe to the path
SET PATH=%CD%\bin;%PATH%

python -u -B scripts\build-release.py %var1% %var2% %var3% %var4% %var5% %var6% %var7% %var8% %var9%