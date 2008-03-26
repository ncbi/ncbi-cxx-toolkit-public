@echo off
REM
REM $Id$
REM

@if not "%VSINSTALLDIR%"=="" goto devenv
@call "%VS90COMNTOOLS%vsvars32.bat"

:devenv

if exist "%VS90COMNTOOLS%..\IDE\VCExpress.*" set DEVENV="%VS90COMNTOOLS%..\IDE\VCExpress"
if exist "%VS90COMNTOOLS%..\IDE\devenv.*" set DEVENV="%VS90COMNTOOLS%..\IDE\devenv"

:end
