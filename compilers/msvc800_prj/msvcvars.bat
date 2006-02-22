@echo off
REM
REM $Id: msvcvars.bat,v 1.3 2006/02/22 15:14:41 gouriano Exp $
REM

@if not "%VSINSTALLDIR%"=="" goto devenv
@call "%VS80COMNTOOLS%vsvars32.bat"

:devenv

if exist "%VS80COMNTOOLS%..\IDE\VCExpress.*" set DEVENV="%VS80COMNTOOLS%..\IDE\VCExpress"
if exist "%VS80COMNTOOLS%..\IDE\devenv.*" set DEVENV="%VS80COMNTOOLS%..\IDE\devenv"

:end
