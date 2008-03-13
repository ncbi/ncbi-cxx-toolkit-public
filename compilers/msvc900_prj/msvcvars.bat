@echo off
REM
REM $Id$
REM

@if not "%VSINSTALLDIR%"=="" goto devenv
@call "%VS80COMNTOOLS%vsvars32.bat"

:devenv

if exist "%VS80COMNTOOLS%..\IDE\VCExpress.*" set DEVENV="%VS80COMNTOOLS%..\IDE\VCExpress"
if exist "%VS80COMNTOOLS%..\IDE\devenv.*" set DEVENV="%VS80COMNTOOLS%..\IDE\devenv"

:end
