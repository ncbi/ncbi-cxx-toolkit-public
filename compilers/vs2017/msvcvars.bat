@echo off
REM
REM $Id$
REM

REM @if not "%VSINSTALLDIR%"=="" goto devenv
REM @call "%VS140COMNTOOLS%vsvars32.bat"

:devenv

if exist "%VSAPPIDDIR%devenv.*" set DEVENV="%VSAPPIDDIR%devenv"

:end
