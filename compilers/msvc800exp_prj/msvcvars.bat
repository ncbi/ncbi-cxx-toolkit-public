@echo off
REM
REM $Id: msvcvars.bat,v 1.2 2006/01/26 19:40:25 gouriano Exp $
REM

@if not "%VSINSTALLDIR%"=="" goto end
@call "%VS80COMNTOOLS%vsvars32.bat"

:end
