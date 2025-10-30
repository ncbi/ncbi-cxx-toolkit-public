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
set script_root=%script_dir%..\..\..
set tree_root=%initial_dir%
set extension=cmake_configure_ext.bat
set prebuilds=

REM #########################################################################
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere"
if "%CMAKE_CMD%"=="" (
  where cmake >NUL 2>&1
  if errorlevel 1 (
    for /f "tokens=* USEBACKQ" %%i IN (`%VSWHERE% -latest -property installationPath`) do (
      set CMAKE_CMD=%%i\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
    )
  ) else (
    set CMAKE_CMD=cmake
  )
)
echo CMake: "%CMAKE_CMD%"
"%CMAKE_CMD%" --version
if errorlevel 1 (
  echo ERROR: CMake is not found 1>&2
  exit /b 1
)

REM #########################################################################
REM defaults
set BUILD_SHARED_LIBS=OFF
set VISUAL_STUDIO=2022
set SKIP_ANALYSIS=OFF
set ALLOW_COMPOSITE=
set generator_multi_cfg=

goto :RUN
REM #########################################################################
REM when specifying path, both "/" and "\" are allowed

:USAGE
echo USAGE:
echo   %script_name% [OPTIONS]...
echo SYNOPSIS:
echo   Configure NCBI C++ toolkit for Visual Studio using CMake build system.
echo   https://ncbi.github.io/cxx-toolkit/pages/ch_cmconfig#ch_cmconfig._Configure
echo OPTIONS:
echo   --help                   -- print Usage
echo   --without-dll            -- build all libraries as static ones (default)
echo   --with-dll               -- assemble toolkit libraries into DLLs
echo                               where requested
echo   --with-composite         -- assemble composite static libraries
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
echo                  examples:    --with-vs=2022  (default)
echo                               --with-vs=2019
echo   --with-generator="X"     -- use generator X
echo   --with-conan             -- use Conan to install required components
echo OPTIONAL ENVIRONMENT VARIABLES:
echo   CMAKE_CMD                -- full path to 'cmake'
echo   CMAKE_ARGS               -- additional arguments to pass to 'cmake'
if not "%prebuilds%"=="" (
    echo   --with-prebuilt=CFG      -- use build settings of an existing build
    echo             CFG is one of:
    for %%a in ( %prebuilds% ) do (
      echo                               %%a
    )
)

if defined have_configure_ext_Usage (
  call "%extension%" :configure_ext_Usage
)
echo:

set generatorfound=
for /f "tokens=* delims=" %%a in ('"%CMAKE_CMD%" --help') do (
  set ttt=%%a
  set ttt=!ttt:"=!
  set ttt=!ttt:^<=!
  set ttt=!ttt:^>=!
  call :PRINTGENERATOR "!ttt!"
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


:ERROR
call :USAGE
if not "%~1"=="" (
  echo ----------------------------------------------------------------------
  echo ERROR:  %* 1>&2
)
goto :eof


:GET_PREBUILD_PATH
for %%a in ( %prebuilds% ) do (
  set build_def=%%a
:GET_PREBUILD_PATH_AGAIN
  echo:
  echo Please pick a build configuration
  echo Available:
  echo:
  for %%b in ( %prebuilds% ) do (
    echo %%b
  )
  echo:
  set /p build_config=Desired configuration ^(default = !build_def!^): 
  if "!build_config!"=="" set build_config=!build_def!
  if not exist %prebuilt_dir%\!build_config! goto :GET_PREBUILD_PATH_AGAIN
  set prebuilt_name=!build_config!
  goto :eof
)
goto :eof


:RUN
REM #########################################################################
REM parse arguments

set do_help=
set unknown=
set PROJECT_COMPONENTS=
set PROJECT_FEATURES=
set BUILD_ROOT=
set BUILD_TYPE=
set PROJECT_LIST=
set PROJECT_TAGS=
set PROJECT_TARGETS=
set PROJECT_DETAILS=
set INSTALL_PATH=
set prebuilt_dir=
set prebuilt_name=
set unknown=

:PARSEARGS
if "%~1"=="" goto :ENDPARSEARGS
if "%~1"==%1             (set unknown=%unknown% ?%~1?& goto :CONTINUEPARSEARGS)
if "%1"=="--help"              (set do_help=YES&       goto :CONTINUEPARSEARGS)
if "%1"=="-help"               (set do_help=YES&       goto :CONTINUEPARSEARGS)
if "%1"=="help"                (set do_help=YES&       goto :CONTINUEPARSEARGS)
if "%1"=="-h"                  (set do_help=YES&       goto :CONTINUEPARSEARGS)
if "%1"=="/?"                  (set do_help=YES&       goto :CONTINUEPARSEARGS)
if "%1"=="--rootdir"           (set tree_root=%~2&         shift& goto :CONTINUEPARSEARGS)
if "%1"=="--caller"            (set script_name=%~2&       shift& goto :CONTINUEPARSEARGS)
if "%1"=="--without-debug"     (set BUILD_TYPE=Release&           goto :CONTINUEPARSEARGS)
if "%1"=="--with-debug"        (set BUILD_TYPE=Debug&             goto :CONTINUEPARSEARGS)
if "%1"=="--without-dll"       (set BUILD_SHARED_LIBS=OFF&        goto :CONTINUEPARSEARGS)
if "%1"=="--with-dll"          (set BUILD_SHARED_LIBS=ON&         goto :CONTINUEPARSEARGS)
if "%1"=="--with-composite"    (set ALLOW_COMPOSITE=ON&           goto :CONTINUEPARSEARGS)
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
if "%1"=="--with-conan"        (set USE_CONAN=ON&                 goto :CONTINUEPARSEARGS)
if "%1"=="--with-prebuilt"     (set prebuilt_dir=%~dp2& set prebuilt_name=%~nx2&   shift& goto :CONTINUEPARSEARGS)
set first=%1
set first=%first:~0,2%
REM if "%first%"=="-D"             (set CMAKE_ARGS=%CMAKE_ARGS% %1=%2& shift& goto :CONTINUEPARSEARGS)
if "%first%"=="-D"             (
    set CMAKE_ARGS=%CMAKE_ARGS% %1=%2
    if "%1"=="-DCMAKE_CONFIGURATION_TYPES" (
        set types=%2
        set types=!types:;= !
        set types=!types:"= !
        set /a cnt=0
        for  %%a in (!types!) do  set /a cnt+=1
        if "!cnt!"=="1" set BUILD_TYPE=%2& set generator_multi_cfg=YES
    ) else if "%1"=="-DNCBI_PTBCFG_CONFIGURATION_TYPES" (
        set types=%2
        set types=!types:;= !
        set types=!types:"= !
        set /a cnt=0
        for  %%a in (!types!) do  set /a cnt+=1
        if "!cnt!"=="1" set BUILD_TYPE=%2& set generator_multi_cfg=YES
    ) else if "%1"=="-DCMAKE_BUILD_TYPE" (
        set BUILD_TYPE=%2
    ) else if "%1"=="-DBUILD_SHARED_LIBS" (
        set BUILD_SHARED_LIBS=%2
    )
    shift
    goto :CONTINUEPARSEARGS
)
set first=%1
set first=%first:~0,8%
if "%first%"=="--debug-"       (set CMAKE_ARGS=%CMAKE_ARGS% %1& goto :CONTINUEPARSEARGS)
set first=%1
set first=%first:~0,6%
if "%first%"=="--log-"         (set CMAKE_ARGS=%CMAKE_ARGS% %1& goto :CONTINUEPARSEARGS)
set first=%1
set first=%first:~0,7%
if "%first%"=="--trace"        (set CMAKE_ARGS=%CMAKE_ARGS% %1& goto :CONTINUEPARSEARGS)
set unknown=%unknown% %1
:CONTINUEPARSEARGS
shift
goto :PARSEARGS
:ENDPARSEARGS

set prebuilds=
for /f "tokens=1" %%a in ('dir /A:D /B  "%script_root%"') do (
  if exist %script_root%\%%a\cmake\buildinfo (
    set prebuilds=!prebuilds! %%a
  )
)
if not "%prebuilt_dir%"=="" (
  if not exist "%prebuilt_dir%%prebuilt_name%" (
    set prebuilt_dir=%script_root%\
  )
  if not exist "%prebuilt_dir%%prebuilt_name%" (
    set prebuilt_dir=%script_root%\
    call :GET_PREBUILD_PATH
  )
)

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

set CMAKE_GENERATOR_ARGS=
if not "%prebuilt_dir%"=="" (
  if exist "%prebuilt_dir%%prebuilt_name%\cmake\buildinfo" (
    copy /Y "%prebuilt_dir%%prebuilt_name%\cmake\buildinfo" "%TEMP%\%prebuilt_name%cmakebuildinfo.bat" >NUL
    call "%TEMP%\%prebuilt_name%cmakebuildinfo.bat"
    del "%TEMP%\%prebuilt_name%cmakebuildinfo.bat"
    set CMAKE_GENERATOR_ARGS=-A x64
  ) else (
    call :ERROR Buildinfo not found in %prebuilt_dir%%prebuilt_name%
    goto :FAIL
  )
)

if "%CMAKE_GENERATOR%"=="" (
  if "%VISUAL_STUDIO%"=="2022" (
    set CMAKE_GENERATOR=Visual Studio 17 2022
    set CMAKE_GENERATOR_ARGS=-A x64
  )
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
if "%CMAKE_GENERATOR%"=="Visual Studio 17 2022" (
  set generator_name=VS2022
) else if "%CMAKE_GENERATOR%"=="Visual Studio 16 2019" (
  set generator_name=VS2019
) else if "%CMAKE_GENERATOR%"=="Visual Studio 15 2017 Win64" (
  set generator_name=VS2017
) else if "%CMAKE_GENERATOR%"=="Visual Studio 14 2015 Win64" (
  set generator_name=VS2015
) else if "%CMAKE_GENERATOR%"=="Ninja Multi-Config" (
  set generator_name=NinjaMulti
) else (
  set generator_name=%CMAKE_GENERATOR%
  set generator_multi_cfg=NO
  if "%BUILD_TYPE%"=="" (
    set BUILD_TYPE=Debug
  )
)
if "%generator_multi_cfg%"=="" (
  set BUILD_TYPE=
)

set ttt=%tree_root%\%PROJECT_LIST%
set ttt=%ttt:/=\%
if not "%PROJECT_LIST%"=="" (
  if exist "%ttt%" (
    type "%ttt%" >NUL 2>&1
    if not errorlevel 1 (
      set PROJECT_LIST=%ttt%
    )
  )
)
set ttt=%tree_root%\%PROJECT_TAGS%
set ttt=%ttt:/=\%
if not "%PROJECT_TAGS%"=="" (
  if exist "%ttt%" (
    type "%ttt%" >NUL 2>&1
    if not errorlevel 1 (
      set PROJECT_TAGS=%ttt%
    )
  )
)
set ttt=%tree_root%\%PROJECT_TARGETS%
set ttt=%ttt:/=\%
if not "%PROJECT_TARGETS%"=="" (
  if exist "%ttt%" (
    type "%ttt%" >NUL 2>&1
    if not errorlevel 1 (
      set PROJECT_TARGETS=%ttt%
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
if not "%USE_CONAN%"=="" (
  set CMAKE_ARGS=%CMAKE_ARGS% -DNCBI_PTBCFG_USECONAN="%USE_CONAN%"
)
if not "%INSTALL_PATH%"=="" (
  set CMAKE_ARGS=%CMAKE_ARGS% -DNCBI_PTBCFG_INSTALL_PATH="%INSTALL_PATH%"
)
set CMAKE_ARGS=%CMAKE_ARGS% -DBUILD_SHARED_LIBS=%BUILD_SHARED_LIBS%
if not "%ALLOW_COMPOSITE%"=="" (
  set CMAKE_ARGS=%CMAKE_ARGS% -DNCBI_PTBCFG_ALLOW_COMPOSITE=%ALLOW_COMPOSITE%
)

if "%BUILD_ROOT%"=="" (
  if "%BUILD_TYPE%"=="" (
    set BUILD_ROOT=CMake-%generator_name%
    if "%BUILD_SHARED_LIBS%"=="ON" (
      set BUILD_ROOT=CMake-%generator_name%-DLL
    )
  ) else (
    set BUILD_ROOT=CMake-%generator_name%-%BUILD_TYPE%
    if "%BUILD_SHARED_LIBS%"=="ON" (
      set BUILD_ROOT=!BUILD_ROOT!DLL
    )
    if "%generator_multi_cfg%"=="NO" (
      set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
    )
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

for /f "tokens=* USEBACKQ" %%i IN (`%VSWHERE% -latest -property installationPath`) do (
  set VSROOT=%%i
)
set genname=%generator_name:~0,2%
where cmake >NUL 2>&1
if errorlevel 1 (
  if "%genname%"=="VS" (
    (
      echo @echo off
      echo "%CMAKE_CMD%" %%*
    ) >cmake.bat
  ) else (
    (
      echo @echo off
      echo setlocal
      echo call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" ^>NUL
      echo "%CMAKE_CMD%" %%*
      echo endlocal
    ) >cmake.bat
  )
) else (
    if exist cmake.bat (del cmake.bat)
)
if "%genname%"=="Ni" (
  where ninja >NUL 2>&1
  if errorlevel 1 (
    set vsvars=YES
    (
      echo @echo off
      echo setlocal
      echo call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" ^>NUL
      echo "%VSROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe" %%*
      echo endlocal
    ) >ninja.bat
  ) else (
    if exist ninja.bat (del ninja.bat)
  )
)
if "%genname%"=="NM" (
  where nmake >NUL 2>&1
  if errorlevel 1 (
    set vsvars=YES
  )
)
if "%vsvars%"=="YES" (
  for /f "tokens=* USEBACKQ" %%i IN (`%VSWHERE% -latest -property installationPath`) do (
    if exist "%%i\VC\Auxiliary\Build\vcvars64.bat" (
      call "%%i\VC\Auxiliary\Build\vcvars64.bat"
    )
  )
)

rem echo Running "%CMAKE_CMD%" %CMAKE_ARGS% "%tree_root%\src"
rem goto :DONE
if exist "%tree_root%\CMakeLists.txt" (
  "%CMAKE_CMD%" %CMAKE_ARGS% "%tree_root%"
) else (
  "%CMAKE_CMD%" %CMAKE_ARGS% "%tree_root%\src"
)
if errorlevel 1 (
  goto :FAIL
)

:DONE
cd /d "%initial_dir%"
endlocal
exit /b 0

:FAIL
cd /d "%initial_dir%"
endlocal
exit /b 1
