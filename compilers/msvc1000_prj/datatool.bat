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
REM Run datatool.exe to generate sources from ASN/DTD/Schema specifications
REM
REM DO NOT ATTEMPT to run this bat file manually
REM
REM ===========================================================================

set DEFDT_LOCATION=\\snowman\win-coremake\App\Ncbi\cppcore\datatool

for %%v in ("%DATATOOL_PATH%" "%TREE_ROOT%" "%BUILD_TREE_ROOT%" "%PTB_PLATFORM%") do (
  if %%v=="" (
    echo ERROR: required environment variable is missing
    echo DO NOT ATTEMPT to run this bat file manually
    exit /b 1
  )
)
set DEFDT_VERSION_FILE=%TREE_ROOT%\src\build-system\datatool_version.txt
set PTB_SLN=%BUILD_TREE_ROOT%\static\build\UtilityProjects\PTB.sln
set DT=datatool.exe

call "%BUILD_TREE_ROOT%\msvcvars.bat"


REM -------------------------------------------------------------------------
REM get DT version: from DEFDT_VERSION_FILE  or from PREBUILT_DATATOOL_EXE

set DEFDT_VERSION=
if exist "%DEFDT_VERSION_FILE%" (
  for /f %%a in ('type "%DEFDT_VERSION_FILE%"') do (set DEFDT_VERSION=%%a& goto donedf)
  :donedf
  set DEFDT_VERSION=%DEFDT_VERSION: =%
)
if exist "%PREBUILT_DATATOOL_EXE%" (
  set ptbver=
  for /f "tokens=2" %%a in ('"%PREBUILT_DATATOOL_EXE%" -version') do (set ptbver=%%a& goto donepb)
  :donepb
  set ptbver=%ptbver: =%
  if not "%DEFDT_VERSION%"=="%ptbver%" (
    echo WARNING: requested %DT% version %ptbver% does not match default one: %DEFDT_VERSION%
    set DEFDT_VERSION=%ptbver%
  )
)

if "%DEFDT_VERSION%"=="" (
  echo ERROR: DEFDT_VERSION not specified
  exit /b 1
)
for /f "tokens=1-3 delims=." %%a in ('echo %DEFDT_VERSION%') do (set DT_VER=%%a%%b%%c& set DT_VER_MAJOR=%%a)


REM -------------------------------------------------------------------------
REM Identify DATATOOL_EXE

if "%PREBUILT_DATATOOL_EXE%"=="bootstrap" (
  set DEF_DT=%DATATOOL_PATH%\%DT%
) else if not "%PREBUILT_DATATOOL_EXE%"=="" (
  if exist "%PREBUILT_DATATOOL_EXE%" (
    set DEF_DT=%PREBUILT_DATATOOL_EXE%
  ) else (
    echo ERROR: "%PREBUILT_DATATOOL_EXE%" not found
    exit /b 1
  )
) else (
  set DEF_DT=%DEFDT_LOCATION%\msvc\%DEFDT_VERSION%\%DT%
)
if exist "%DEF_DT%" (
  set DATATOOL_EXE=%DEF_DT%
) else (
  echo %DT% not found at %DEF_DT%
  set DATATOOL_EXE=%DATATOOL_PATH%\%DT%
)


REM -------------------------------------------------------------------------
REM Build DATATOOL_EXE if needed

if not exist "%DATATOOL_EXE%" (
  if exist "%PTB_SLN%" (
    echo ******************************************************************************
    echo Building %DT% locally, please wait
    echo ******************************************************************************
    @echo %DEVENV% "%PTB_SLN%" /rebuild "ReleaseDLL|%PTB_PLATFORM%" /project "datatool.exe"
    %DEVENV% "%PTB_SLN%" /rebuild "ReleaseDLL|%PTB_PLATFORM%" /project "datatool.exe"
  ) else (
    echo ERROR: do not know how to build %DT%
  )
) else (
  echo ******************************************************************************
  echo Using PREBUILT %DT% at %DATATOOL_EXE%
  echo ******************************************************************************
)
if not exist "%DATATOOL_EXE%" (
  echo ERROR: "%DATATOOL_EXE%" not found
  exit /b 1
)
"%DATATOOL_EXE%" -version
if errorlevel 1 (
  echo ERROR: cannot find working %DT%
  exit /b 1
)


REM -------------------------------------------------------------------------
REM Run DATATOOL_EXE

"%DATATOOL_EXE%" %*
