@echo off
setlocal
REM #########################################################################
REM  $Id$
REM  Configure NCBI C++ toolkit for Visual Studio using CMake build system.
REM #########################################################################

set initial_dir=%CD%
set script_name=%~nx0
REM set script_dir=%~dp0
cd %0\..
set script_dir=%CD%
set tree_root=%initial_dir%

REM #########################################################################
if "%CMAKE_CMD%"=="" (
  set CMAKE_CMD=C:\Users\gouriano\Downloads\cmake-3.10.2-win64-x64\bin\cmake.exe
)
if not exist "%CMAKE_CMD%" (
  echo ERROR: CMake is not found
  goto :DONE
)
set CMAKE_ARGS=

REM #########################################################################
REM defaults
set BUILD_TYPE=Debug
set BUILD_SHARED_LIBS=OFF
set USE_CCACHE=OFF
set USE_DISTCC=OFF
set VISUAL_STUDIO=2017

goto :RUN
REM #########################################################################

:USAGE
echo USAGE:
echo   %script_name% [OPTION]...
echo SYNOPSIS:
echo   Configure NCBI C++ toolkit for Visual Studio using CMake build system.
echo OPTIONS:
echo   --help                  -- print Usage
echo   --without-dll           -- build all libraries as static ones
echo   --with-dll              -- build all libraries as shared ones,
echo                              unless explicitely requested otherwise
echo   --with-projects="FILE"  -- build projects listed in %tree_root%\FILE
echo                              FILE can also be a list of subdirectories of
echo                              %tree_root%\src
echo                  examples:   --with-projects="corelib$;serial"
echo                              --with-projects=scripts/projects/ncbi_cpp.lst
echo   --with-vs=N             -- use Visual Studio N generator 
echo                  examples:   --with-vs=2017
echo                              --with-vs=2015
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

set unknown=
set dest=
:PARSEARGS
if "%~1"=="" goto :ENDPARSEARGS
if "%dest%"=="lst"                      (set project_list=%~1&  set dest=& goto :CONTINUEPARSEARGS)
if "%dest%"=="vs"                       (set VISUAL_STUDIO=%~1& set dest=& goto :CONTINUEPARSEARGS)
if "%dest%"=="gen"                      (set generator=%~1&     set dest=& goto :CONTINUEPARSEARGS)
if "%1"=="--help"                       (call :USAGE&       exit /b 0)
if "%1"=="--without-dll"                (set BUILD_SHARED_LIBS=OFF&  goto :CONTINUEPARSEARGS)
if "%1"=="--with-dll"                   (set BUILD_SHARED_LIBS=ON&   goto :CONTINUEPARSEARGS)
if "%1"=="--with-projects"              (set dest=lst&               goto :CONTINUEPARSEARGS)
if "%1"=="--with-vs"                    (set dest=vs&                goto :CONTINUEPARSEARGS)
if "%1"=="--with-generator"             (set dest=gen&               goto :CONTINUEPARSEARGS)
set unknown=%unknown% %1
:CONTINUEPARSEARGS
shift
goto :PARSEARGS
:ENDPARSEARGS

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

if exist "%tree_root%\%project_list%" (
  set project_list=%tree_root%\%project_list%
)

REM #########################################################################

set CMAKE_ARGS=
if not "%generator%"=="" (
  set CMAKE_ARGS=%CMAKE_ARGS% -G "%generator%"
)
if not "%project_list%"=="" (
  set CMAKE_ARGS=%CMAKE_ARGS% -DNCBI_PROJECT_LIST="%project_list%"
)
set CMAKE_ARGS=%CMAKE_ARGS% -DBUILD_SHARED_LIBS=%BUILD_SHARED_LIBS%

set build_root=compilers\CMake-%generator_name%
if "%BUILD_SHARED_LIBS%"=="ON" (
  set build_root=%build_root%\dll
  set project_name=ncbi_cpp_dll
) else (
  set build_root=%build_root%\static
  set project_name=ncbi_cpp
)
set CMAKE_ARGS=%CMAKE_ARGS% -DNCBI_CMAKEPROJECT_NAME=%project_name%

if not exist "%tree_root%\%build_root%\build" (
  mkdir "%tree_root%\%build_root%\build"
)
cd "%tree_root%\%build_root%\build"

if exist "CMakeCache.txt" (
  del CMakeCache.txt
)

echo Running "%CMAKE_CMD%" %CMAKE_ARGS% "%tree_root%\src"
"%CMAKE_CMD%" %CMAKE_ARGS% "%tree_root%\src"

:DONE
cd %initial_dir%
endlocal
exit /b 0
