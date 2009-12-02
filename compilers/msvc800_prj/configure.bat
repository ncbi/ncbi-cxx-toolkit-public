@echo off
REM $Id$
REM ===========================================================================
REM 
REM                            PUBLIC DOMAIN NOTICE
REM               National Center for Biotechnology Information
REM 
REM  This software/database is a "United States Government Work" under the
REM  terms of the United States Copyright Act.  It was written as part of
REM  the author's official duties as a United States Government employee and
REM  thus cannot be copyrighted.  This software/database is freely available
REM  to the public for use. The National Library of Medicine and the U.S.
REM  Government have not placed any restriction on its use or reproduction.
REM 
REM  Although all reasonable efforts have been taken to ensure the accuracy
REM  and reliability of the software and data, the NLM and the U.S.
REM  Government do not and cannot warrant the performance or results that
REM  may be obtained by using this software or data. The NLM and the U.S.
REM  Government disclaim all warranties, express or implied, including
REM  warranties of performance, merchantability or fitness for any particular
REM  purpose.
REM 
REM  Please cite the author in any work or product based on this material.
REM  
REM ===========================================================================
REM 
REM Author:  Andrei Gourianov
REM
REM Configure MSVC solution and projects
REM
REM ===========================================================================

setlocal

set sln_name=ncbi_cpp
set use_projectlst=scripts/projects/ncbi_cpp.lst

set use_savedcfg=
set use_gui=no
set maybe_gui=yes
set use_dll=no
set use_64=no
set use_ide=800
set use_flags=
set help_req=no
set srcroot=../..

set initial_dir=%CD%
set script_name=%0
cd %~p0
for /f "delims=" %%a in ('cd') do (set script_dir=%%a)
cd %srcroot%
for /f "delims=" %%a in ('cd') do (set srcroot=%%a)
cd %initial_dir%

REM --------------------------------------------------------------------------------
REM parse arguments

set dest=
:PARSEARGS
if "%1"=="" goto :ENDPARSEARGS
if "%dest%"=="lst"                      (set use_projectlst=%1& set dest=& goto :CONTINUEPARSEARGS)
if "%dest%"=="cfg"                      (set use_savedcfg=%1&   set dest=& goto :CONTINUEPARSEARGS)
if "%1"=="--help"                       (set help_req=yes& goto :CONTINUEPARSEARGS)
if "%1"=="--with-configure-dialog"      (set use_gui=yes&  goto :CONTINUEPARSEARGS)
if "%1"=="--without-configure-dialog"   (set use_gui=no&   goto :CONTINUEPARSEARGS)
if "%1"=="--with-saved-settings"        (set dest=cfg&     goto :CONTINUEPARSEARGS)
if "%1"=="--with-dll"                   (set use_dll=yes&  goto :CONTINUEPARSEARGS)
if "%1"=="--without-dll"                (set use_dll=no&   goto :CONTINUEPARSEARGS)
if "%1"=="--with-64"                    (set use_64=yes&   goto :CONTINUEPARSEARGS)
if "%1"=="--with-projects"              (set dest=lst&     goto :CONTINUEPARSEARGS)
:CONTINUEPARSEARGS
set maybe_gui=no
shift
goto :PARSEARGS
:ENDPARSEARGS
if "%maybe_gui%"=="yes" (
  set use_gui=yes
)

REM --------------------------------------------------------------------------------
REM print usage

:PRINTUSAGE
if "%help_req%"=="yes" (
  echo  USAGE:
  echo    %script_name% [OPTION]...
  echo  SYNOPSIS:
  echo    configure NCBI C++ toolkit for MSVC build system.
  echo  OPTIONS:
  echo    --help                      -- print Usage
  echo    --with-configure-dialog     -- use Configuration GUI application
  echo    --without-configure-dialog  -- do not use Configuration GUI application
  echo    --with-saved-settings=FILE  -- load configuration settings from FILE
  echo    --with-dll                  -- assemble toolkit libraries into DLLs
  echo                                     where requested
  echo    --without-dll               -- build all toolkit libraries as static ones
  echo    --with-64                   -- compile to 64-bit code
  echo    --with-projects=FILE        -- build projects listed in "%srcroot%\FILE"
  echo             FILE can also be a name of a subdirectory
  echo             examples:   --with-projects=src/corelib
  echo                         --with-projects=scripts/projects/ncbi_cpp.lst
  exit /b 0
)

REM --------------------------------------------------------------------------------
REM identify target MSVC version (based on the script location msvcNNN_prj)

for /f "delims=" %%a in ('echo %script_dir%') do (set msvc_ver=%%~na)
set msvc_ver=%msvc_ver:msvc=%
set msvc_ver=%msvc_ver:_prj=%
if not "%msvc_ver%"=="" (set use_ide=%msvc_ver%)

REM --------------------------------------------------------------------------------
REM target architecture, solution path, configuration flags

if "%use_64%"=="yes" (
  set use_arch=x64
) else (
  set use_arch=Win32
)
if "%use_dll%"=="yes" (
  set sln_path=dll
  set sln_name=%sln_name%_dll
  set use_flags=%use_flags% -dll
) else (
  set sln_path=static
)
if "%use_gui%"=="yes" (
  set use_flags=%use_flags% -cfg
)

REM --------------------------------------------------------------------------------
REM prepare and run ptb.bat
cd %script_dir%
set PTB_PLATFORM=%use_arch%
set PTB_FLAGS=%use_flags%
set PTB_PATH=./static/bin/ReleaseDLL
set SLN_PATH=./%sln_path%/build/%sln_name%.sln
set TREE_ROOT=%srcroot%
set BUILD_TREE_ROOT=.
set PTB_PROJECT_REQ=%use_projectlst%

if "%use_savedcfg%"=="" (
  set PTB_SAVED_CFG_REQ=
) else (
  if exist %use_savedcfg% (
    set PTB_SAVED_CFG_REQ=%use_savedcfg%
  ) else (
    if exist %initial_dir%\%use_savedcfg% (
      set PTB_SAVED_CFG_REQ=%initial_dir%\%use_savedcfg%
    ) else (
      echo ERROR: %use_savedcfg% not found
    )
  )
)

call ./ptb.bat
if errorlevel 1 (
  cd %initial_dir%
  exit /b 1
)
cd %initial_dir%
endlocal
exit /b 0
