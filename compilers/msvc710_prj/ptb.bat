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

if "%PREBUILT_PTB_EXE%"=="" (
  set PREBUILT_PTB_EXE=\\snowman\win-coremake\App\Ncbi\cppcore\ptb\msvc71\project_tree_builder.exe
) else (
  if not exist "%PREBUILT_PTB_EXE%" (
    if not "%PREBUILT_PTB_EXE%"=="bootstrap" (
      echo ERROR: "%PREBUILT_PTB_EXE%" not found
      exit /b 1
    )
  )
)
set PTB_EXE=%PTB_PATH%\project_tree_builder.exe
if not "%PREBUILT_PTB_EXE%"=="bootstrap" (
  "%PREBUILT_PTB_EXE%" -version
  if errorlevel 1 (
    set PTB_EXE=%PTB_PATH%\project_tree_builder.exe
  ) else (
    set PTB_EXE=%PREBUILT_PTB_EXE%
  )
)

if "%PTB_PROJECT%"=="" set PTB_PROJECT=%PTB_PROJECT_REQ%
if not exist "%PTB_EXE%" (
  echo ******************************************************************************
  echo Building project tree builder locally, please wait
  echo ******************************************************************************
  @echo devenv /rebuild Release /project project_tree_builder.exe %BUILD_TREE_ROOT%\static\build\build-system\project_tree_builder\project_tree_builder.sln
  devenv /rebuild Release /project project_tree_builder.exe %BUILD_TREE_ROOT%\static\build\build-system\project_tree_builder\project_tree_builder.sln
) else (
  echo ******************************************************************************
  echo Using PREBUILT project tree builder at %PTB_EXE%
  echo ******************************************************************************
)
"%PTB_EXE%" -version
if errorlevel 1 (
  echo ERROR: cannot find working %PTB_EXE%
  exit /b 1
)
call "%BUILD_TREE_ROOT%\lock_ptb_config.bat" ON "%BUILD_TREE_ROOT%\"
if errorlevel 1 exit /b 1
echo ******************************************************************************
echo Running -CONFIGURE-, please wait
echo ******************************************************************************
@echo on
"%PTB_EXE%" %PTB_FLAGS% -logfile "%SLN_PATH%_configuration_log.txt" -conffile "%TREE_ROOT%\src\build-system\project_tree_builder.ini" "%TREE_ROOT%" %PTB_PROJECT% "%SLN_PATH%"
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
