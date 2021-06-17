@echo off
REM
REM $Id$
REM

set VSVER=15.0

:devenv

for /f "tokens=* USEBACKQ" %%i IN (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere" -version [15.0^,16.0^) -property productPath`) do (
    set DEVENV="%%i"
)

:end
