@echo off
REM
REM $Id$
REM

@if not "%VSINSTALLDIR%"=="" goto devenv
@call "%VS120COMNTOOLS%vsvars32.bat"

:devenv

if exist "%VS120COMNTOOLS%..\IDE\VCExpress.*" set DEVENV="%VS120COMNTOOLS%..\IDE\VCExpress"
if exist "%VS120COMNTOOLS%..\IDE\devenv.*" set DEVENV="%VS120COMNTOOLS%..\IDE\devenv"

:end
