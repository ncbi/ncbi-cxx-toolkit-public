@echo off
setlocal ENABLEDELAYEDEXPANSION
REM #########################################################################
REM  $Id$
REM  Configure NCBI C++ toolkit for Visual Studio using CMake build system.
REM  Author: Andrei Gourianov, gouriano@ncbi
REM #########################################################################

set initial_dir=%CD%
set script_name=%~nx0
REM set script_dir=%~dp0
cd %0\..
set script_dir=%CD%
set tree_root=%initial_dir%

REM #########################################################################
if "%CMAKE_CMD%"=="" (
  set CMAKE_CMD=C:\Program Files ^(x86^)\Microsoft Visual Studio\2017\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
)
if not exist "%CMAKE_CMD%" (
  echo ERROR: CMake is not found
  goto :DONE
)

REM #########################################################################
REM defaults
set BUILD_SHARED_LIBS=OFF
set VISUAL_STUDIO=2017

goto :RUN
REM #########################################################################
REM when specifying path, both "/" and "\" are allowed

:USAGE
echo USAGE:
echo   %script_name% [OPTION]...
echo SYNOPSIS:
echo   Configure NCBI C++ toolkit for Visual Studio using CMake build system.
echo OPTIONS:
echo   --help                  -- print Usage
echo   --without-dll           -- build all libraries as static ones (default)
echo   --with-dll              -- assemble toolkit libraries into DLLs
echo                              where requested
echo   --with-projects="FILE"  -- build projects listed in %tree_root%\FILE
echo                              FILE can also be a list of subdirectories of
echo                              %tree_root%\src
echo                  examples:   --with-projects="corelib$;serial"
echo                              --with-projects=scripts/projects/ncbi_cpp.lst
echo   --with-tags="tags"      -- build projects which have allowed tags only
echo                  examples:   --with-tags="*;-test"
echo   --with-targets="names"  -- build projects which have allowed names only
echo                  examples:   --with-targets="datatool;xcgi$"
echo   --with-details="names"  -- print detailed information about projects
echo                  examples:   --with-details="datatool;test_hash"
echo   --with-vs=N             -- use Visual Studio N generator 
echo                  examples:   --with-vs=2017  (default)
echo                              --with-vs=2015
echo   --with-install="DIR"    -- generate rules for installation into "DIR" directory
echo                  examples:   --with-install="D:\CPP toolkit"
echo   --with-generator="X"    -- use generator X
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
  echo ERROR:  %*
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
if "%1"=="--help"              (set do_help=YES&       goto :CONTINUEPARSEARGS)
if "%1"=="-help"               (set do_help=YES&       goto :CONTINUEPARSEARGS)
if "%1"=="help"                (set do_help=YES&       goto :CONTINUEPARSEARGS)
if "%1"=="--rootdir"           (set tree_root=%~2&         shift& goto :CONTINUEPARSEARGS)
if "%1"=="--caller"            (set script_name=%~2&       shift& goto :CONTINUEPARSEARGS)
if "%1"=="--without-dll"       (set BUILD_SHARED_LIBS=OFF&        goto :CONTINUEPARSEARGS)
if "%1"=="--with-dll"          (set BUILD_SHARED_LIBS=ON&         goto :CONTINUEPARSEARGS)
if "%1"=="--with-projects"     (set project_list=%~2&      shift& goto :CONTINUEPARSEARGS)
if "%1"=="--with-tags"         (set project_tags=%~2&      shift& goto :CONTINUEPARSEARGS)
if "%1"=="--with-targets"      (set project_targets=%~2&   shift& goto :CONTINUEPARSEARGS)
if "%1"=="--with-details"      (set project_details=%~2&   shift& goto :CONTINUEPARSEARGS)
if "%1"=="--with-vs"           (set VISUAL_STUDIO=%~2&     shift& goto :CONTINUEPARSEARGS)
if "%1"=="--with-install"      (set INSTALL_PATH=%~2&      shift& goto :CONTINUEPARSEARGS)
if "%1"=="--with-generator"    (set generator=%~2&         shift& goto :CONTINUEPARSEARGS)
set unknown=%unknown% %1
:CONTINUEPARSEARGS
shift
goto :PARSEARGS
:ENDPARSEARGS

if not "%do_help%"=="" (
  call :USAGE
  goto :DONE
)

if not "%unknown%"=="" (
  call :ERROR unknown option: %unknown%
  goto :DONE
)

if "%generator%"=="" (
  if "%VISUAL_STUDIO%"=="2017" (
    set generator=Visual Studio 15 2017 Win64
    set generator_name=vs2017
  )
  if "%VISUAL_STUDIO%"=="2015" (
    set generator=Visual Studio 14 2015 Win64
    set generator_name=vs2015
  )
) else (
  set generator_name=%generator%
)

if not "%project_list%"=="" (
  if exist "%tree_root%\%project_list%" (
    set project_list=%tree_root%\%project_list%
  )
)
if not "%project_tags%"=="" (
  if exist "%tree_root%\%project_tags%" (
    set project_tags=%tree_root%\%project_tags%
  )
)
if not "%project_targets%"=="" (
  if exist "%tree_root%\%project_targets%" (
    set project_targets=%tree_root%\%project_targets%
  )
)

REM #########################################################################

set CMAKE_ARGS=-DNCBI_EXPERIMENTAL=ON

if not "%generator%"=="" (
  set CMAKE_ARGS=%CMAKE_ARGS% -G "%generator%"
)
set CMAKE_ARGS=%CMAKE_ARGS% -DNCBI_PTBCFG_PROJECT_LIST="%project_list%"
set CMAKE_ARGS=%CMAKE_ARGS% -DNCBI_PTBCFG_PROJECT_TAGS="%project_tags%"
set CMAKE_ARGS=%CMAKE_ARGS% -DNCBI_PTBCFG_PROJECT_TARGETS="%project_targets%"
set CMAKE_ARGS=%CMAKE_ARGS% -DNCBI_VERBOSE_PROJECTS="%project_details%"
if not "%INSTALL_PATH%"=="" (
  set CMAKE_ARGS=%CMAKE_ARGS% -DNCBI_PTBCFG_INSTALL_PATH="%INSTALL_PATH%"
)
set CMAKE_ARGS=%CMAKE_ARGS% -DBUILD_SHARED_LIBS=%BUILD_SHARED_LIBS%

set build_root=CMake-%generator_name%
if "%BUILD_SHARED_LIBS%"=="ON" (
  set build_root=%build_root%\dll
) else (
  set build_root=%build_root%\static
)

if not exist "%tree_root%\%build_root%\build" (
  mkdir "%tree_root%\%build_root%\build"
)
cd "%tree_root%\%build_root%\build"

REM echo Running "%CMAKE_CMD%" %CMAKE_ARGS% "%tree_root%\src"
"%CMAKE_CMD%" %CMAKE_ARGS% "%tree_root%\src"

:DONE
cd %initial_dir%
endlocal
exit /b 0
