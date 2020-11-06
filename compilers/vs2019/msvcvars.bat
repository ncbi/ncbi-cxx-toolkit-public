@echo off
REM
REM $Id$
REM

:devenv

for /f "tokens=* USEBACKQ" %%i IN (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere" -version [16.0^,17.0^) -property productPath`) do (
    set DEVENV="%%i"
)

:end
