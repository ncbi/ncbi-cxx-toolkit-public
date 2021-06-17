@echo off
rem #########################################################################
rem  $Id$
rem   Template of a CMake configuration extension script
rem   Author: Andrei Gourianov, gouriano@ncbi
rem
rem   Usage:
rem      Based on this template, create an extension script for your team
rem      call it cmake_configure_ext_team.bat, for example.
rem      This script will be included into cmake-cfg-vs.bat
rem      In the root directory of your build tree create cmake_configure_ext.bat
rem      with the following contents:
rem         call "%~dp0src\build-system\cmake\cmake_configure_ext_team.bat" %*
rem
rem   Note:
rem      Be careful not to overwrite parent script variables accidentally.
rem
rem #########################################################################

rem declare script capabilities
rem NOTE:
rem   only the fact that variable is defined matters
rem   its value does not matter
set have_configure_ext_Usage=yes
set have_configure_ext_ParseArgs=yes
set have_configure_ext_PreCMake=yes

rem Verify that script is not being run standalone
if not defined have_configure_host (
  echo This is an extension script. It cannot be run standalone.
  echo Instead, it should be included into another script.
  exit /b 1
)
if "%~1"=="" (
  exit /b 0
)
call %*
goto :eof


rem Print information about additional settings
:configure_ext_Usage
echo Additional settings:
echo   --a                      -- option a
echo   --b=opt                  -- option b
goto :eof


rem Parse command line
rem First argument of the function is the name of the variable which receives
rem arguments which were not recognized by this function.
rem Other arguments are those which were not recognized by the parent script.
rem
rem The function is called after the parent script has parsed arguments which it understands
rem and receives unrecognized arguments only.
:configure_ext_ParseArgs
rem unrecognized arguments will go into this variable
set _ext_retval=%1
rem unrecognized arguments will be collected here
set _ext_unknown=
rem parse arguments, modify parent script variables if needed
:configure_ext_PARSE
shift
if "%~1"==""                                    goto :configure_ext_ENDPARSE
if "%~1"=="--a"    (echo found --a&             goto :configure_ext_PARSE)
if "%~1"=="--b"    (echo found --b %~2&  shift& goto :configure_ext_PARSE)
rem collect unknown arguments
set _ext_unknown=%_ext_unknown% %~1
goto :configure_ext_PARSE
:configure_ext_ENDPARSE
rem put unrecognized arguments into return variable
set %_ext_retval%=%_ext_unknown%
goto :eof


rem The function is called right before running CMake configuration.
rem Add more parameters into CMAKE_ARGS if needed
:configure_ext_PreCMake
set CMAKE_ARGS=%CMAKE_ARGS% --trace
goto :eof
