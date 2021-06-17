@echo off
REM #########################################################################
REM  $Id$
REM #########################################################################
set script_dir=%~dp0?
set script_dir=%script_dir:\?=%
set script_name=%~nx0
"%script_dir%\src\build-system\cmake\cmake-cfg-vs.bat" --rootdir="%script_dir%" --caller=%script_name% %*
