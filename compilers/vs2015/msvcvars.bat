@echo off
REM
REM $Id$
REM

@if not "%VSINSTALLDIR%"=="" goto devenv
@call "%VS140COMNTOOLS%vsvars32.bat"

:devenv

if exist "%VS140COMNTOOLS%..\IDE\VCExpress.*" set DEVENV="%VS140COMNTOOLS%..\IDE\VCExpress"
if exist "%VS140COMNTOOLS%..\IDE\devenv.*" set DEVENV="%VS140COMNTOOLS%..\IDE\devenv"

:end
