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
set use_debug=yes
set use_dll=no
set use_64=no
set use_staticstd=no
set use_arch=Win32
set use_ide=1100
set use_flags=
set help_req=no
set srcroot=../..

REM -----------------------------------------------------------------------------
REM  silently ignored  options
set noops=
set noops=%noops% --without-optimization
set noops=%noops% --with-profiling
set noops=%noops% --with-tcheck
set noops=%noops% --with-static
set noops=%noops% --with-plugin-auto-load
set noops=%noops% --with-bin-release
set noops=%noops% --with-mt
set noops=%noops% --without-exe
set noops=%noops% --with-runpath
set noops=%noops% --with-lfs
set noops=%noops% --with-autodep
set noops=%noops% --with-build-root
set noops=%noops% --with-fake-root
set noops=%noops% --without-suffix
set noops=%noops% --with-hostspec
set noops=%noops% --without-version
set noops=%noops% --with-build-root-sfx
set noops=%noops% --without-execopy
set noops=%noops% --with-bincopy
set noops=%noops% --with-lib-rebuilds
set noops=%noops% --with-lib-rebuilds
set noops=%noops% --without-deactivation
set noops=%noops% --without-makefile-auto-update
set noops=%noops% --without-flat-makefile
set noops=%noops% --with-check
set noops=%noops% --with-check-tools
set noops=%noops% --with-ncbi-public
set noops=%noops% --with-strip
set noops=%noops% --with-pch
set noops=%noops% --with-caution
set noops=%noops% --without-caution
set noops=%noops% --without-ccache
set noops=%noops% --with-distcc
set noops=%noops% --without-ncbi-c
set noops=%noops% --without-sss
set noops=%noops% --without-utils
set noops=%noops% --without-sssdb
set noops=%noops% --with-included-sss
set noops=%noops% --with-z
set noops=%noops% --without-z
set noops=%noops% --with-bz2
set noops=%noops% --without-bz2
set noops=%noops% --with-lzo
set noops=%noops% --without-lzo
set noops=%noops% --with-pcre
set noops=%noops% --without-pcre
set noops=%noops% --with-gnutls
set noops=%noops% --without-gnutls
set noops=%noops% --with-openssl
set noops=%noops% --without-openssl
set noops=%noops% --without-sybase
set noops=%noops% --with-sybase-local
set noops=%noops% --with-sybase-new
set noops=%noops% --without-ftds
set noops=%noops% --with-ftds
set noops=%noops% --without-ftds-renamed
set noops=%noops% --without-mysql
set noops=%noops% --with-mysql
set noops=%noops% --without-opengl
set noops=%noops% --with-opengl
set noops=%noops% --without-mesa
set noops=%noops% --with-mesa
set noops=%noops% --without-glut
set noops=%noops% --with-glut
set noops=%noops% --without-wxwin
set noops=%noops% --with-wxwin
set noops=%noops% --without-wxwidgets
set noops=%noops% --with-wxwidgets
set noops=%noops% --with-wxwidgets-ucs
set noops=%noops% --without-wxwidgets-ucs
set noops=%noops% --without-freetype
set noops=%noops% --with-freetype
set noops=%noops% --without-fastcgi
set noops=%noops% --with-fastcgi
set noops=%noops% --with-fastcgi
set noops=%noops% --without-bdb
set noops=%noops% --with-bdb
set noops=%noops% --without-sp
set noops=%noops% --without-orbacus
set noops=%noops% --with-orbacus
set noops=%noops% --with-odbc
set noops=%noops% --with-python
set noops=%noops% --without-python
set noops=%noops% --with-boost
set noops=%noops% --without-boost
set noops=%noops% --with-boost-tag
set noops=%noops% --without-boost-tag
set noops=%noops% --with-sqlite
set noops=%noops% --without-sqlite
set noops=%noops% --with-sqlite3
set noops=%noops% --without-sqlite3
set noops=%noops% --with-icu
set noops=%noops% --without-icu
set noops=%noops% --with-expat
set noops=%noops% --without-expat
set noops=%noops% --with-sablot
set noops=%noops% --without-sablot
set noops=%noops% --with-libxml
set noops=%noops% --without-libxml
set noops=%noops% --with-libxslt
set noops=%noops% --without-libxslt
set noops=%noops% --with-xerces
set noops=%noops% --without-xerces
set noops=%noops% --with-xalan
set noops=%noops% --without-xalan
set noops=%noops% --with-oechem
set noops=%noops% --without-oechem
set noops=%noops% --with-sge
set noops=%noops% --without-sge
set noops=%noops% --with-muparser
set noops=%noops% --without-muparser
set noops=%noops% --with-hdf5
set noops=%noops% --without-hdf5
set noops=%noops% --with-gif
set noops=%noops% --without-gif
set noops=%noops% --with-jpeg
set noops=%noops% --without-jpeg \
set noops=%noops% --with-png
set noops=%noops% --without-png
set noops=%noops% --with-tiff
set noops=%noops% --without-tiff
set noops=%noops% --with-xpm
set noops=%noops% --without-xpm
set noops=%noops% --without-local-lbsm
set noops=%noops% --without-ncbi-crypt
set noops=%noops% --without-connext
set noops=%noops% --without-serial
set noops=%noops% --without-objects
set noops=%noops% --without-dbapi
set noops=%noops% --without-app
set noops=%noops% --without-ctools
set noops=%noops% --without-gui
set noops=%noops% --without-algo
set noops=%noops% --without-internal
set noops=%noops% --with-gbench
set noops=%noops% --without-gbench
set noops=%noops% --with-x


set initial_dir=%CD%
set script_name=%0
cd %~p0
for /f "delims=" %%a in ('cd') do (set script_dir=%%a)
cd %srcroot%
for /f "delims=" %%a in ('cd') do (set srcroot=%%a)
cd %initial_dir%

REM --------------------------------------------------------------------------------
REM parse arguments

set unknown=
set ignore_unknown=no
set dest=
:PARSEARGS
if "%1"=="" goto ENDPARSEARGS
if "%dest%"=="lst"                      (set use_projectlst=%1&  set dest=& goto CONTINUEPARSEARGS)
if "%dest%"=="cfg"                      (set use_savedcfg=%~1&   set dest=& goto CONTINUEPARSEARGS)
if "%1"=="--help"                       (set help_req=yes&       goto CONTINUEPARSEARGS)
if "%1"=="--with-configure-dialog"      (set use_gui=yes&        goto CONTINUEPARSEARGS)
if "%1"=="--without-configure-dialog"   (set use_gui=no&         goto CONTINUEPARSEARGS)
if "%1"=="--with-saved-settings"        (set dest=cfg&           goto CONTINUEPARSEARGS)
if "%1"=="--without-debug"              (set use_debug=no&       goto CONTINUEPARSEARGS)
if "%1"=="--with-debug"                 (set use_debug=yes&      goto CONTINUEPARSEARGS)
if "%1"=="--without-dll"                (set use_dll=no&         goto CONTINUEPARSEARGS)
if "%1"=="--with-dll"                   (set use_dll=yes&        goto CONTINUEPARSEARGS)
if "%1"=="--with-64"                    (set use_64=yes&         goto CONTINUEPARSEARGS)
if "%1"=="--with-static-exe"            (set use_staticstd=yes&  goto CONTINUEPARSEARGS)
if "%1"=="--with-projects"              (set dest=lst&           goto CONTINUEPARSEARGS)
if "%1"=="--ignore-unsupported-options" (set ignore_unknown=yes& goto CONTINUEPARSEARGS)
set unknown=%unknown% %1
:CONTINUEPARSEARGS
set maybe_gui=no
shift
goto PARSEARGS
:ENDPARSEARGS
if "%maybe_gui%"=="yes" (
  set use_gui=yes
)

REM --------------------------------------------------------------------------------
REM check and report unknown options

set invalid_unknown=no
for %%u in (%unknown%) do (
  call :CHECKUNKNOWN %%u
)
if "%invalid_unknown%"=="yes" exit /b 1
goto DONEUNKNOWN

:CHECKUNKNOWN
for %%n in (%noops%) do (
  if "%1"=="%%n" (
    echo Ignored:  %1
    goto :eof
  )
)
for /f "eol=-" %%a in ('echo %1') do goto :eof
if "%ignore_unknown%"=="no" (
  echo Unsupported option:  %1
  set invalid_unknown=yes
) else (
  echo Ignored unsupported:  %1
)
goto :eof
:DONEUNKNOWN

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
  echo    --without-debug             -- build non-debug versions of libs and apps
  echo    --with-debug                -- build debug versions of libs and apps
  echo    --without-dll               -- build all toolkit libraries as static ones
  echo    --with-dll                  -- assemble toolkit libraries into DLLs
  echo                                     where requested
  echo    --with-64                   -- compile to 64-bit code
  echo    --with-static-exe           -- use static C++ standard libraries
  echo    --with-projects=FILE        -- build projects listed in "%srcroot%\FILE"
  echo             FILE can also be a name of a subdirectory
  echo             examples:   --with-projects=src/corelib
  echo                         --with-projects=scripts/projects/ncbi_cpp.lst
  echo    --ignore-unsupported-options   -- ignore unsupported options
  exit /b 0
)

REM --------------------------------------------------------------------------------
REM identify target MSVC version (based on the script location msvcNNN_prj)

for /f "delims=" %%a in ('echo %script_dir%') do (set msvc_ver=%%~na)
set msvc_ver=%msvc_ver:msvc=%
set msvc_ver=%msvc_ver:_prj=%
if not "%msvc_ver%"=="" (set use_ide=%msvc_ver%)

REM --------------------------------------------------------------------------------
REM target architecture, solution path, configuration and flags

if "%use_64%"=="yes" (
  set use_arch=x64
) else (
  set use_arch=Win32
)
if "%use_dll%"=="yes" (
  if "%use_debug%"=="yes" (
    set CONFIGURATION=DebugDLL
  ) else (
    set CONFIGURATION=ReleaseDLL
  )
) else (
  if "%use_debug%"=="yes" (
    if "%use_staticstd%"=="yes" (
      set CONFIGURATION=DebugMT
    ) else (
      set CONFIGURATION=DebugDLL
    )
  ) else (
    if "%use_staticstd%"=="yes" (
      set CONFIGURATION=ReleaseMT
    ) else (
      set CONFIGURATION=ReleaseDLL
    )
  )
)
if "%use_gui%"=="yes" (
  set use_flags=%use_flags% -cfg
)
if "%use_dll%"=="yes" (
  set build_results=dll
  set sln_name=%sln_name%_dll
  set use_flags=%use_flags% -dll
) else (
  set build_results=static
)
set use_projectlst=%use_projectlst:/=\%


REM --------------------------------------------------------------------------------
REM prepare and run ptb.bat
cd %script_dir%
set PTB_PLATFORM=%use_arch%
set PTB_FLAGS=%use_flags%
set PTB_PATH=./static/bin/ReleaseDLL
set SLN_PATH=%script_dir%\%build_results%\build\%sln_name%.sln
set TREE_ROOT=%srcroot%
set BUILD_TREE_ROOT=.
set PTB_PROJECT_REQ=%use_projectlst%

if "%use_savedcfg%"=="" (
  set PTB_SAVED_CFG_REQ=
) else (
  if exist "%use_savedcfg%" (
    set PTB_SAVED_CFG_REQ="%use_savedcfg%"
  ) else (
    if exist "%initial_dir%\%use_savedcfg%" (
      set PTB_SAVED_CFG_REQ="%initial_dir%\%use_savedcfg%"
    ) else (
      echo ERROR: "%use_savedcfg%" not found
      exit /b 1
    )
  )
)

call ./ptb.bat
if errorlevel 1 (
  cd %initial_dir%
  exit /b 1
)

REM --------------------------------------------------------------------------------
REM generate configure_make.bat

cd %script_dir%
set mk_cmnd=make.bat build %sln_name% %build_results%
if "%use_64%"=="yes" (
  set mk_cmnd=%mk_cmnd% 64
) else (
  set mk_cmnd=%mk_cmnd% 32
)
set mk_cmnd=%mk_cmnd% %CONFIGURATION%
echo %mk_cmnd% > configure_make.bat


REM ------------------------------------------------------------------------------
echo To build the solution %SLN_PATH%
echo execute the following commands:
echo cd %script_dir%
echo make

cd %initial_dir%
endlocal
exit /b 0
