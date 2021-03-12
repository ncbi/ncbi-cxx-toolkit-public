@echo off
setlocal ENABLEDELAYEDEXPANSION
REM #########################################################################
REM  $Id$
REM  Configure NCBI C++ toolkit for Visual Studio using CMake build system.
REM  Author: Andrei Gourianov, gouriano@ncbi
REM #########################################################################

set initial_dir=%CD%
set script_name=%~nx0
set script_dir=%~dp0
set tree_root=%initial_dir%
set extension=cmake_configure_ext.bat

REM #########################################################################
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere"
if "%CMAKE_CMD%"=="" (
  for /f "tokens=* USEBACKQ" %%i IN (`%VSWHERE% -latest -property installationPath`) do (
    set CMAKE_CMD=%%i\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
  )
)
if not exist "%CMAKE_CMD%" (
  echo ERROR: CMake is not found 1>&2
  exit /b 1
)

REM #########################################################################
REM defaults
set BUILD_SHARED_LIBS=OFF
set VISUAL_STUDIO=2017
set SKIP_ANALYSIS=OFF

goto :RUN
REM #########################################################################
REM when specifying path, both "/" and "\" are allowed

:USAGE
echo USAGE:
echo   %script_name% [OPTIONS]...
echo SYNOPSIS:
echo   Configure NCBI C++ toolkit for Visual Studio using CMake build system.
echo OPTIONS:
echo   --help                   -- print Usage
echo   --without-dll            -- build all libraries as static ones (default)
echo   --with-dll               -- assemble toolkit libraries into DLLs
echo                               where requested
echo   --with-projects="FILE"   -- build projects listed in %tree_root%\FILE
echo                               FILE can also be a list of subdirectories of
echo                               %tree_root%\src
echo                  examples:    --with-projects="corelib$;serial"
echo                               --with-projects=scripts/projects/ncbi_cpp.lst
echo   --with-tags="tags"       -- build projects which have allowed tags only
echo                  examples:    --with-tags="*;-test"
echo   --with-targets="names"   -- build projects which have allowed names only
echo                  examples:    --with-targets="datatool;xcgi$"
echo   --with-details="names"   -- print detailed information about projects
echo                  examples:    --with-details="datatool;test_hash"
echo   --with-install="DIR"     -- generate rules for installation into "DIR" directory
echo                  examples:    --with-install="D:\CPP toolkit"
echo   --with-components="LIST" -- explicitly enable or disable components
echo                  examples:    --with-components="-Z"
echo   --with-features="LIST"   -- specify compilation features
echo                  examples:    --with-features="StrictGI"
echo   --with-build-root=name   -- specify a non-default build directory name
echo   --without-analysis       -- skip source tree analysis
echo   --with-vs=N              -- use Visual Studio N generator 
echo                  examples:    --with-vs=2017  (default)
echo                               --with-vs=2019
echo   --with-generator="X"     -- use generator X

if defined have_configure_ext_Usage (
  call "%extension%" :configure_ext_Usage
)
echo:

set generatorfound=
for /f "tokens=* delims=" %%a in ('"%CMAKE_CMD%" --help') do (
  call :PRINTGENERATOR "%%a"
)
goto :eof

:ERROR
call :USAGE
if not "%~1"=="" (
  echo ----------------------------------------------------------------------
  echo ERROR:  %* 1>&2
)
goto :eof

:PRINTGENERATOR
if not "%~1"=="" (
  if "%~1"=="Generators" (
    set generatorfound=yes
REM    echo ----------------------------------------------------------------------
    goto :eof
  )
)
if "%generatorfound%"=="" (
  goto :eof
)
echo %~1
goto :eof


:RUN
REM #########################################################################
REM parse arguments

set do_help=
set unknown=
:PARSEARGS
if "%~1"=="" goto :ENDPARSEARGS
if "%~1"==%1             (set unknown=%unknown% ?%~1?& goto :CONTINUEPARSEARGS)
if "%1"=="--help"              (set do_help=YES&       goto :CONTINUEPARSEARGS)
if "%1"=="-help"               (set do_help=YES&       goto :CONTINUEPARSEARGS)
if "%1"=="help"                (set do_help=YES&       goto :CONTINUEPARSEARGS)
if "%1"=="-h"                  (set do_help=YES&       goto :CONTINUEPARSEARGS)
if "%1"=="--rootdir"           (set tree_root=%~2&         shift& goto :CONTINUEPARSEARGS)
if "%1"=="--caller"            (set script_name=%~2&       shift& goto :CONTINUEPARSEARGS)
if "%1"=="--without-dll"       (set BUILD_SHARED_LIBS=OFF&        goto :CONTINUEPARSEARGS)
if "%1"=="--with-dll"          (set BUILD_SHARED_LIBS=ON&         goto :CONTINUEPARSEARGS)
if "%1"=="--with-components"   (set PROJECT_COMPONENTS=%~2& shift& goto :CONTINUEPARSEARGS)
if "%1"=="--with-features"     (set PROJECT_FEATURES=%~2&  shift& goto :CONTINUEPARSEARGS)
if "%1"=="--with-build-root"   (set BUILD_ROOT=%~2&        shift& goto :CONTINUEPARSEARGS)
if "%1"=="--with-projects"     (set PROJECT_LIST=%~2&      shift& goto :CONTINUEPARSEARGS)
if "%1"=="--with-tags"         (set PROJECT_TAGS=%~2&      shift& goto :CONTINUEPARSEARGS)
if "%1"=="--with-targets"      (set PROJECT_TARGETS=%~2&   shift& goto :CONTINUEPARSEARGS)
if "%1"=="--with-details"      (set PROJECT_DETAILS=%~2&   shift& goto :CONTINUEPARSEARGS)
if "%1"=="--with-vs"           (set VISUAL_STUDIO=%~2&     shift& goto :CONTINUEPARSEARGS)
if "%1"=="--with-install"      (set INSTALL_PATH=%~2&      shift& goto :CONTINUEPARSEARGS)
if "%1"=="--without-analysis"  (set SKIP_ANALYSIS=ON&             goto :CONTINUEPARSEARGS)
if "%1"=="--with-generator"    (set CMAKE_GENERATOR=%~2&   shift& goto :CONTINUEPARSEARGS)
if "%1"=="--with-prebuilt"     (set prebuilt_dir=%~dp2& set prebuilt_name=%~nx2&   shift& goto :CONTINUEPARSEARGS)
set first=%1
set first=%first:~0,2%
if "%first%"=="-D"             (set CMAKE_ARGS=%CMAKE_ARGS% %1=%2& shift& goto :CONTINUEPARSEARGS)
set unknown=%unknown% %1
:CONTINUEPARSEARGS
shift
goto :PARSEARGS
:ENDPARSEARGS

set have_configure_host=yes
set extension=%tree_root%\%extension%
if exist "%extension%" (
  call "%extension%"
)

if not "%do_help%"=="" (
  call :USAGE
  goto :DONE
)

if not "%unknown%"=="" (
  if defined have_configure_ext_ParseArgs (
    call "%extension%" :configure_ext_ParseArgs unknown %unknown:?=^"%
  )
)

if not "%unknown%"=="" (
  call :ERROR unknown options: %unknown%
  goto :FAIL
)

if not "%prebuilt_dir%"=="" (
  if exist "%prebuilt_dir%%prebuilt_name%\cmake\buildinfo" (
    copy /Y "%prebuilt_dir%%prebuilt_name%\cmake\buildinfo" "%TEMP%\%prebuilt_name%cmakebuildinfo.bat" >NUL
    call "%TEMP%\%prebuilt_name%cmakebuildinfo.bat"
    del "%TEMP%\%prebuilt_name%cmakebuildinfo.bat"
  ) else (
    call :ERROR Buildinfo not found in %prebuilt_dir%%prebuilt_name%
    goto :FAIL
  )
)

set CMAKE_GENERATOR_ARGS=
if "%CMAKE_GENERATOR%"=="" (
  if "%VISUAL_STUDIO%"=="2019" (
    set CMAKE_GENERATOR=Visual Studio 16 2019
    set CMAKE_GENERATOR_ARGS=-A x64
  )
  if "%VISUAL_STUDIO%"=="2017" (
    set CMAKE_GENERATOR=Visual Studio 15 2017 Win64
  )
  if "%VISUAL_STUDIO%"=="2015" (
    set CMAKE_GENERATOR=Visual Studio 14 2015 Win64
  )
)
set generator_name=%CMAKE_GENERATOR%
if "%CMAKE_GENERATOR%"=="Visual Studio 16 2019" (
  set generator_name=VS2019
)
if "%CMAKE_GENERATOR%"=="Visual Studio 15 2017 Win64" (
  set generator_name=VS2017
)
if "%CMAKE_GENERATOR%"=="Visual Studio 14 2015 Win64" (
  set generator_name=VS2015
)

if not "%PROJECT_LIST%"=="" (
  if exist "%tree_root%\%PROJECT_LIST%" (
    type "%tree_root%\%PROJECT_LIST%" >NUL 2>&1
    if not errorlevel 1 (
      set PROJECT_LIST=%tree_root%\%PROJECT_LIST%
    )
  )
)
if not "%PROJECT_TAGS%"=="" (
  if exist "%tree_root%\%PROJECT_TAGS%" (
    type "%tree_root%\%PROJECT_TAGS%" >NUL 2>&1
    if not errorlevel 1 (
      set PROJECT_TAGS=%tree_root%\%PROJECT_TAGS%
    )
  )
)
if not "%PROJECT_TARGETS%"=="" (
  if exist "%tree_root%\%PROJECT_TARGETS%" (
    type "%tree_root%\%PROJECT_TARGETS%" >NUL 2>&1
    if not errorlevel 1 (
      set PROJECT_TARGETS=%tree_root%\%PROJECT_TARGETS%
    )
  )
)

REM #########################################################################

if not "%CMAKE_GENERATOR%"=="" (
  set CMAKE_ARGS=%CMAKE_ARGS% -G "%CMAKE_GENERATOR%" %CMAKE_GENERATOR_ARGS%
)
set CMAKE_ARGS=%CMAKE_ARGS% -DNCBI_PTBCFG_PROJECT_COMPONENTS="%PROJECT_COMPONENTS%"
set CMAKE_ARGS=%CMAKE_ARGS% -DNCBI_PTBCFG_PROJECT_FEATURES="%PROJECT_FEATURES%"
set CMAKE_ARGS=%CMAKE_ARGS% -DNCBI_PTBCFG_PROJECT_LIST="%PROJECT_LIST%"
set CMAKE_ARGS=%CMAKE_ARGS% -DNCBI_PTBCFG_PROJECT_TAGS="%PROJECT_TAGS%"
set CMAKE_ARGS=%CMAKE_ARGS% -DNCBI_PTBCFG_PROJECT_TARGETS="%PROJECT_TARGETS%"
set CMAKE_ARGS=%CMAKE_ARGS% -DNCBI_VERBOSE_PROJECTS="%PROJECT_DETAILS%"
set CMAKE_ARGS=%CMAKE_ARGS% -DNCBI_PTBCFG_SKIP_ANALYSIS=%SKIP_ANALYSIS%
if not "%INSTALL_PATH%"=="" (
  set CMAKE_ARGS=%CMAKE_ARGS% -DNCBI_PTBCFG_INSTALL_PATH="%INSTALL_PATH%"
)
set CMAKE_ARGS=%CMAKE_ARGS% -DBUILD_SHARED_LIBS=%BUILD_SHARED_LIBS%

if "%BUILD_ROOT%"=="" (
  set BUILD_ROOT=CMake-%generator_name%
  if "%BUILD_SHARED_LIBS%"=="ON" (
    set BUILD_ROOT=CMake-%generator_name%-DLL
  )
)

cd /d "%tree_root%"
if defined have_configure_ext_PreCMake (
  call "%extension%" :configure_ext_PreCMake
)
if not exist "%BUILD_ROOT%\build" (
  mkdir "%BUILD_ROOT%\build"
)
if not exist "%BUILD_ROOT%\build" (
  call :ERROR Failed to create directory: %BUILD_ROOT%\build
  goto :FAIL
)
cd /d "%BUILD_ROOT%\build"

rem echo Running "%CMAKE_CMD%" %CMAKE_ARGS% "%tree_root%\src"
rem goto :DONE
if exist "%tree_root%\CMakeLists.txt" (
  "%CMAKE_CMD%" %CMAKE_ARGS% "%tree_root%"
) else (
  "%CMAKE_CMD%" %CMAKE_ARGS% "%tree_root%\src"
)

:DONE
cd /d %initial_dir%
endlocal
exit /b 0

:FAIL
cd /d %initial_dir%
endlocal
exit /b 1
