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

set ORIGINAL_ARGS=%*

REM ---------------- begin workaround FOR MSVC2010! ---------------------------------
set input_asn_path=
set input_asn_name=
set input_def_path=
set subtree=
set srcroot=
:PARSEARGS
if _%1==_ goto ENDPARSEARGS
if "%dest%"=="inASN"    (set input_asn_path=%~1& set input_asn_name=%~n1& set dest=& goto CONTINUEPARSEARGS)
if "%dest%"=="inDEF"    (set input_def_path=%~1& set dest=& goto CONTINUEPARSEARGS)
if "%dest%"=="subtree"  (set subtree=%1&     set dest=& goto CONTINUEPARSEARGS)
if "%dest%"=="srcroot"  (set srcroot=%1&     set dest=& goto CONTINUEPARSEARGS)
if "%1"=="-m"           (set dest=inASN&                goto CONTINUEPARSEARGS)
if "%1"=="-od"          (set dest=inDEF&                goto CONTINUEPARSEARGS)
if "%1"=="-or"          (set dest=subtree&              goto CONTINUEPARSEARGS)
if "%1"=="-oR"          (set dest=srcroot&              goto CONTINUEPARSEARGS)
if "%1"=="-M"           (goto ENDPARSEARGS)
:CONTINUEPARSEARGS
shift
REM echo parsing %1
goto PARSEARGS
:ENDPARSEARGS
set src_subtree=%CD%\%srcroot%src\%subtree%
set dest_spec=%BUILD_TREE_ROOT%\static\build\%subtree%
if not exist "%dest_spec%" mkdir "%dest_spec%"
if not exist "%dest_spec%" set dest_spec=.

set copied_asn=0
for /f %%a in ('xcopy "%input_asn_path%" "%dest_spec%" /q /d /y') do (set copied_asn=%%a)
set copied_def=0
if not exist "%input_def_path%" echo [-] > "%input_def_path%"
if exist "%input_def_path%" for /f %%a in ('xcopy "%input_def_path%" "%dest_spec%" /q /d /y') do (set copied_def=%%a)
if not %copied_asn%==0 goto DOGENERATE
if not %copied_def%==0 goto DOGENERATE
if not exist "%src_subtree%%input_asn_name%.files"   goto DOGENERATE
if not exist "%src_subtree%%input_asn_name%__.cpp"   goto DOGENERATE
if not exist "%src_subtree%%input_asn_name%___.cpp"  goto DOGENERATE
echo generation NOT needed
exit /b 0
:DOGENERATE
REM ----------------   end workaround --------------------------------------

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

set DT_COPY_HERE=NO
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
  if not "%COPY_DATATOOL_EXE%"=="NO" (
    set DT_COPY_HERE=YES
  )
)
if exist "%DEF_DT%" (
  set DATATOOL_EXE=%DEF_DT%
) else (
  echo %DT% not found at %DEF_DT%
  set DATATOOL_EXE=%DATATOOL_PATH%\%DT%
  set DT_COPY_HERE=NO
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

REM -------------------------------------------------------------------------
REM Copy datatool from network to the local tree (to make it work faster)

set DT_LOCAL=%BUILD_TREE_ROOT%\static\build\UtilityProjects\%DEFDT_VERSION%_%DT%
if "%DT_COPY_HERE%"=="YES" (
  if not exist "%DT_LOCAL%" (
    xcopy "%DATATOOL_EXE%" "%BUILD_TREE_ROOT%\static\build\UtilityProjects\" /q /d /y >NUL
    rename "%BUILD_TREE_ROOT%\static\build\UtilityProjects\%DT%" "%DEFDT_VERSION%_%DT%"
  )
  set DATATOOL_EXE=%DT_LOCAL%
)

REM -------------------------------------------------------------------------
REM Run DATATOOL_EXE

"%DATATOOL_EXE%" %ORIGINAL_ARGS%
