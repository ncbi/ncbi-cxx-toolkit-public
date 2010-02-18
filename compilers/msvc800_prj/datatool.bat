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


for %%v in ("%DATATOOL_PATH%" "%TREE_ROOT%" "%BUILD_TREE_ROOT%" "%PTB_PLATFORM%") do (
  if %%v=="" (
    echo ERROR: required environment variable is missing
    echo DO NOT ATTEMPT to run this bat file manually
    exit /b 1
  )
)
set PTB_SLN=%BUILD_TREE_ROOT%\static\build\UtilityProjects\PTB.sln
set DT=datatool.exe

call "%BUILD_TREE_ROOT%\msvcvars.bat"

set DATATOOL_EXE=%DATATOOL_PATH%\%DT%
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

%DATATOOL_EXE% %*