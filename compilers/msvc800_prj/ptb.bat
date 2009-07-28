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
REM Run project_tree_builder.exe to generate MSVC solution and project files
REM
REM DO NOT ATTEMPT to run this bat file manually
REM It should be run by CONFIGURE project only
REM (open a solution and build or rebuild CONFIGURE project)
REM
REM ===========================================================================

set DEFPTB_LOCATION=\\snowman\win-coremake\App\Ncbi\cppcore\ptb
set IDE=800
set PTB_EXTRA=

for %%v in ("%PTB_PATH%" "%SLN_PATH%" "%TREE_ROOT%" "%BUILD_TREE_ROOT%" "%PTB_PLATFORM%") do (
  if %%v=="" (
    echo ERROR: required environment variable is missing
    echo DO NOT ATTEMPT to run this bat file manually
    echo It should be run by CONFIGURE project only
    exit /b 1
  )
)

call "%BUILD_TREE_ROOT%\msvcvars.bat"

set DEFPTB_VERSION=
set DEFPTB_VERSION_FILE=%TREE_ROOT%\src\build-system\ptb_version.txt
if exist "%DEFPTB_VERSION_FILE%" (
  for /f %%a in ('type "%DEFPTB_VERSION_FILE%"') do (set DEFPTB_VERSION=%%a & goto donedf)
  :donedf
  set DEFPTB_VERSION=%DEFPTB_VERSION: =%
)
if exist "%PREBUILT_PTB_EXE%" (
  set ptbver=
  for /f "tokens=2" %%a in ('"%PREBUILT_PTB_EXE%" -version') do (set ptbver=%%a & goto donepb)
  :donepb
  set ptbver=%ptbver: =%
  if not "%DEFPTB_VERSION%"=="%ptbver%" (
    echo WARNING: requested PTB version %ptbver% does not match default one: %DEFPTB_VERSION%
    set DEFPTB_VERSION=%ptbver%
  )
)

if "%DEFPTB_VERSION%"=="" (
  echo ERROR: DEFPTB_VERSION not specified
  exit /b 1
)
for /f "tokens=1-3 delims=." %%a in ('echo %DEFPTB_VERSION%') do set PTB_VER=%%a%%b%%c
if %PTB_VER% GEQ 180 (
  set PTB_EXTRA=%PTB_EXTRA% -ide %IDE% -arch %PTB_PLATFORM%
)
if "%PREBUILT_PTB_EXE%"=="bootstrap" (
  set DEF_PTB=%PTB_PATH%\project_tree_builder.exe
) else if not "%PREBUILT_PTB_EXE%"=="" (
  if exist "%PREBUILT_PTB_EXE%" (
    set DEF_PTB=%PREBUILT_PTB_EXE%
  ) else (
    echo ERROR: "%PREBUILT_PTB_EXE%" not found
    exit /b 1
  )
) else (
  if %PTB_VER% GEQ 180 (
    set DEF_PTB=%DEFPTB_LOCATION%\msvc\%DEFPTB_VERSION%\project_tree_builder.exe
  ) else (
    if "%PTB_PLATFORM%"=="x64" (
      set DEF_PTB=%DEFPTB_LOCATION%\msvc8.64\%DEFPTB_VERSION%\project_tree_builder.exe
    ) else (
      set DEF_PTB=%DEFPTB_LOCATION%\msvc8\%DEFPTB_VERSION%\project_tree_builder.exe
    )
  )
)
if exist "%DEF_PTB%" (
  set PTB_EXE=%DEF_PTB%
) else (
  echo project_tree_builder.exe not found at %DEF_PTB%
  set PTB_EXE=%PTB_PATH%\project_tree_builder.exe
)

set PTB_INI=%TREE_ROOT%\src\build-system\project_tree_builder.ini
if not exist "%PTB_INI%" (
  echo ERROR: "%PTB_INI%" not found
  exit /b 1
)
if "%PTB_PROJECT%"=="" set PTB_PROJECT=%PTB_PROJECT_REQ%

if not exist "%PTB_EXE%" (
  echo ******************************************************************************
  echo Building project tree builder locally, please wait
  echo ******************************************************************************
  rem --- @echo msbuild "%BUILD_TREE_ROOT%\static\build\ncbi_cpp.sln" /t:"project_tree_builder_exe:Rebuild" /p:Configuration=ReleaseDLL;Platform=%PTB_PLATFORM% /maxcpucount:1
  rem --- msbuild "%BUILD_TREE_ROOT%\static\build\ncbi_cpp.sln" /t:"project_tree_builder_exe:Rebuild" /p:Configuration=ReleaseDLL;Platform=%PTB_PLATFORM% /maxcpucount:1
  @echo %DEVENV% "%BUILD_TREE_ROOT%\static\build\ncbi_cpp.sln" /rebuild "ReleaseDLL|%PTB_PLATFORM%" /project "project_tree_builder.exe"
  %DEVENV% "%BUILD_TREE_ROOT%\static\build\ncbi_cpp.sln" /rebuild "ReleaseDLL|%PTB_PLATFORM%" /project "project_tree_builder.exe"
) else (
  echo ******************************************************************************
  echo Using PREBUILT project tree builder at %PTB_EXE%
  echo ******************************************************************************
)
if not exist "%PTB_EXE%" (
  echo ERROR: "%PTB_EXE%" not found
  exit /b 1
)
"%PTB_EXE%" -version
if errorlevel 1 (
  echo ERROR: cannot find working %PTB_EXE%
  exit /b 1
)

call "%BUILD_TREE_ROOT%\lock_ptb_config.bat" ON "%BUILD_TREE_ROOT%\"
if errorlevel 1 exit /b 1

echo ******************************************************************************
echo Running -CONFIGURE- please wait
echo ******************************************************************************
@echo on
"%PTB_EXE%" %PTB_FLAGS% %PTB_EXTRA% -logfile "%SLN_PATH%_configuration_log.txt" -conffile "%PTB_INI%" "%TREE_ROOT%" %PTB_PROJECT% "%SLN_PATH%"
@echo off
if errorlevel 1 (set PTB_RESULT=1) else (set PTB_RESULT=0)

call "%BUILD_TREE_ROOT%\lock_ptb_config.bat" OFF "%BUILD_TREE_ROOT%\"

if "%PTB_RESULT%"=="1" (
  echo ******************************************************************************
  echo -CONFIGURE- has failed
  echo Configuration log was saved at "file://%SLN_PATH%_configuration_log.txt"
  echo ******************************************************************************
  if "%DIAG_SILENT_ABORT%"=="" start "" "%SLN_PATH%_configuration_log.txt"
  exit /b 1
) else (
  echo ******************************************************************************
  echo -CONFIGURE- has succeeded
  echo Configuration log was saved at "file://%SLN_PATH%_configuration_log.txt"
  echo ******************************************************************************
)
