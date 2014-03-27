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

setlocal
set DEFPTB_LOCATION=\\snowman\win-coremake\App\Ncbi\cppcore\ptb
set IDE=1200
set PTB_EXTRA=

for %%v in ("%PTB_PATH%" "%SLN_PATH%" "%TREE_ROOT%" "%BUILD_TREE_ROOT%" "%PTB_PLATFORM%") do (
  if %%v=="" (
    echo ERROR: required environment variable is missing
    echo DO NOT ATTEMPT to run this bat file manually
    echo It should be run by CONFIGURE project only
    exit /b 1
  )
)
set PTBGUI="%TREE_ROOT%\src\build-system\project_tree_builder_gui\bin\ptbgui.jar"
set DEFPTB_VERSION_FILE=%TREE_ROOT%\src\build-system\ptb_version.txt
set PTB_INI=%TREE_ROOT%\src\build-system\project_tree_builder.ini
set PTB_SLN=%BUILD_TREE_ROOT%\static\build\UtilityProjects\PTB.sln

REM --- get solution dir ---
call :XSLNPATH %SLN_PATH%
goto DONE
:XSLNPATH
set SLN_DIR=%~dp1
goto :eof
:DONE
REM --- call pre-configure script if it exists ---
if exist "%SLN_DIR%configure_prebuild.bat" (
  call "%SLN_DIR%configure_prebuild.bat"
)

call "%BUILD_TREE_ROOT%\msvcvars.bat"

REM -------------------------------------------------------------------------
REM get PTB version: from DEFPTB_VERSION_FILE  or from PREBUILT_PTB_EXE

set DEFPTB_VERSION=
if exist "%DEFPTB_VERSION_FILE%" (
  for /f %%a in ('type "%DEFPTB_VERSION_FILE%"') do (set DEFPTB_VERSION=%%a& goto donedf)
  :donedf
  set DEFPTB_VERSION=%DEFPTB_VERSION: =%
)
if exist "%PREBUILT_PTB_EXE%" (
  set ptbver=
  for /f "tokens=2" %%a in ('"%PREBUILT_PTB_EXE%" -version') do (set ptbver=%%a& goto donepb)
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
for /f "tokens=1-3 delims=." %%a in ('echo %DEFPTB_VERSION%') do (set PTB_VER=%%a%%b%%c& set PTB_VER_MAJOR=%%a)


REM -------------------------------------------------------------------------
REM See if we should and can use Java GUI

set REQ_GUI_CFG=NO
set USE_GUI_CFG=NO
for /f "tokens=*" %%i in ('echo %PTB_FLAGS%') do call :PARSE %%i
goto :endparse
:PARSE
if "%1"=="" goto :eof
if "%1"=="-cfg" (set REQ_GUI_CFG=YES& goto :eof)
shift
goto :PARSE
:endparse
if "%REQ_GUI_CFG%"=="YES" (
  if %PTB_VER_MAJOR% GEQ 2 (
    if exist "%PTBGUI%" (
      java -version >NUL 2>&1
      if errorlevel 1 (
        echo WARNING: Java not found, cannot run configuration GUI
      ) else (
        set USE_GUI_CFG=YES
      )
    ) else (
      echo WARNING: "%PTBGUI%" not found
    )
  )
)


REM -------------------------------------------------------------------------
REM See if we should and can use saved settings

set PTB_SAVED_CFG=
if not "%PTB_SAVED_CFG_REQ%"=="" (
  if not exist "%PTB_SAVED_CFG_REQ%" (
    echo ERROR: %PTB_SAVED_CFG_REQ% not found
    exit /b 1
  )
  if %PTB_VER_MAJOR% GEQ 2 (
    if %PTB_VER% GEQ 220 (
      set PTB_SAVED_CFG=-args %PTB_SAVED_CFG_REQ%
REM PTB will read PTB_PROJECT from the saved settings
      set PTB_PROJECT_REQ=""
    )
  )
)


REM -------------------------------------------------------------------------
REM Identify PTB_EXE

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
      set DEF_PTB=%DEFPTB_LOCATION%\msvc9.64\%DEFPTB_VERSION%\project_tree_builder.exe
    ) else (
      set DEF_PTB=%DEFPTB_LOCATION%\msvc9\%DEFPTB_VERSION%\project_tree_builder.exe
    )
  )
)
if exist "%DEF_PTB%" (
  set PTB_EXE=%DEF_PTB%
) else (
  echo project_tree_builder.exe not found at %DEF_PTB%
  set PTB_EXE=%PTB_PATH%\project_tree_builder.exe
)

REM -------------------------------------------------------------------------
REM Misc settings

if %PTB_VER% GEQ 180 (
  set PTB_EXTRA=%PTB_EXTRA% -ide %IDE% -arch %PTB_PLATFORM%
)
if not exist "%PTB_INI%" (
  echo ERROR: "%PTB_INI%" not found
  exit /b 1
)
set PTB_PROJECT_LST=
if "%PTB_PROJECT%"=="" (
  set PTB_PROJECT_LST=%PTB_PROJECT_REQ%
) else (
  set PTB_PROJECT_LST=%PTB_PROJECT%
)


REM -------------------------------------------------------------------------
REM Build PTB_EXE if needed

if not exist "%PTB_EXE%" (
  echo ******************************************************************************
  echo Building project tree builder locally, please wait
  echo ******************************************************************************
  rem --- @echo msbuild "%BUILD_TREE_ROOT%\static\build\ncbi_cpp.sln" /t:"project_tree_builder_exe:Rebuild" /p:Configuration=ReleaseDLL;Platform=%PTB_PLATFORM% /maxcpucount:1
  rem --- msbuild "%BUILD_TREE_ROOT%\static\build\ncbi_cpp.sln" /t:"project_tree_builder_exe:Rebuild" /p:Configuration=ReleaseDLL;Platform=%PTB_PLATFORM% /maxcpucount:1
  if exist "%PTB_SLN%" (
    @echo %DEVENV% "%PTB_SLN%" /rebuild "ReleaseDLL|%PTB_PLATFORM%" /project "project_tree_builder.exe"
    %DEVENV% "%PTB_SLN%" /rebuild "ReleaseDLL|%PTB_PLATFORM%" /project "project_tree_builder.exe"
  ) else (
    @echo %DEVENV% "%BUILD_TREE_ROOT%\static\build\ncbi_cpp.sln" /rebuild "ReleaseDLL|%PTB_PLATFORM%" /project "project_tree_builder.exe"
    %DEVENV% "%BUILD_TREE_ROOT%\static\build\ncbi_cpp.sln" /rebuild "ReleaseDLL|%PTB_PLATFORM%" /project "project_tree_builder.exe"
  )
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


REM -------------------------------------------------------------------------
REM Run PTB_EXE

call "%BUILD_TREE_ROOT%\lock_ptb_config.bat" ON "%BUILD_TREE_ROOT%\"
if errorlevel 1 exit /b 1

echo ******************************************************************************
echo Running -CONFIGURE- please wait
echo ******************************************************************************
echo "%PTB_EXE%" %PTB_FLAGS% %PTB_EXTRA% %PTB_SAVED_CFG% -logfile "%SLN_PATH%_configuration_log.txt" -conffile "%PTB_INI%" "%TREE_ROOT%" %PTB_PROJECT_LST% "%SLN_PATH%"
if "%USE_GUI_CFG%"=="YES" (
  java -jar %PTBGUI% "%PTB_EXE%" -i %PTB_FLAGS% %PTB_EXTRA% %PTB_SAVED_CFG% -logfile "%SLN_PATH%_configuration_log.txt" -conffile "%PTB_INI%" "%TREE_ROOT%" %PTB_PROJECT_LST% "%SLN_PATH%"
) else (
  "%PTB_EXE%" %PTB_FLAGS% %PTB_EXTRA% %PTB_SAVED_CFG% -logfile "%SLN_PATH%_configuration_log.txt" -conffile "%PTB_INI%" "%TREE_ROOT%" %PTB_PROJECT_LST% "%SLN_PATH%"
)
if errorlevel 1 (set PTB_RESULT=1) else (set PTB_RESULT=0)

call "%BUILD_TREE_ROOT%\lock_ptb_config.bat" OFF "%BUILD_TREE_ROOT%\"

if "%PTB_RESULT%"=="1" (
  echo ******************************************************************************
  echo -CONFIGURE- has failed
  echo Configuration log was saved at "file://%SLN_PATH%_configuration_log.txt"
  echo ******************************************************************************
  if exist "%SLN_PATH%_configuration_log.txt" (
    if "%DIAG_SILENT_ABORT%"=="" start "" "%SLN_PATH%_configuration_log.txt"
  )
  exit /b 1
) else (
  echo ******************************************************************************
  echo -CONFIGURE- has succeeded
  echo Configuration log was saved at "file://%SLN_PATH%_configuration_log.txt"
  echo ******************************************************************************
)

set ALLOBJ="_generate_all_objects.dataspec"
type "%SLN_PATH%" | %SystemRoot%\system32\find /C %ALLOBJ% >NUL 2>&1
if not errorlevel 1 (
  echo ******************************************************************************
  echo ******************************************************************************
  echo ==============  Generating objects source code.                 ==============
  echo ==============  DO NOT RELOAD THE SOLUTION NOW!                 ============== 
  echo ******************************************************************************
  echo %DEVENV% "%SLN_PATH%" /build "ReleaseDLL|%PTB_PLATFORM%" /project %ALLOBJ%
  %DEVENV% "%SLN_PATH%" /build "ReleaseDLL|%PTB_PLATFORM%" /project %ALLOBJ%
)
echo -
echo -
echo ******************************************************************************
echo ==============  It is now safe to reload the solution:          ==============
echo ==============  Please, close it and open again                 ============== 
echo ******************************************************************************
