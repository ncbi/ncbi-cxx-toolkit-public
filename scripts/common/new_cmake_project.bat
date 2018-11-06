@echo off
setlocal ENABLEDELAYEDEXPANSION
REM #########################################################################
REM  $Id$
REM  Create new CMake project which uses prebuilt NCBI C++ toolkit from a sample
REM  Author: Andrei Gourianov, gouriano@ncbi
REM #########################################################################

set initial_dir=%CD%
set script_name=%~nx0
cd %0\..
set script_dir=%CD%
cd %initial_dir%
set tree_root=%initial_dir%

set repository=https://svn.ncbi.nlm.nih.gov/repos/toolkit/trunk/c++
set rep_inc=include
set rep_src=src
set rep_sample=sample
set prj_tmp=CMakeLists.tmp
set prj_prj=CMakeLists.txt
set prj_cfg=configure.bat
goto :RUN

REM #########################################################################
:USAGE
echo USAGE:
echo    %script_name% ^<name^> ^<type^> [builddir]
echo SYNOPSIS:
echo   Create new CMake project which uses prebuilt NCBI C++ toolkit from a sample.
echo ARGUMENTS:
echo   --help       -- print Usage
echo   ^<name^>       -- project name (destination directory)
echo   ^<type^>       -- project type
echo   [builddir]   -- root directory of the pre-built NCBI C++ toolkit
if "%~1"=="" (
    echo:
    echo The following project types are available:
    for /f %%a in ('svn list -R %repository%/%rep_src%/%rep_sample%') do (
        for /f %%b in ('echo %%a^| findstr /r ".*/$"') do (
            if not "%%b"=="" (
                for /f "tokens=1,2,* delims=/" %%c in ('echo %%b') do (
                    if not "%%d"=="" (
                        if "%%e"=="" (
                            echo   %%c/%%d
                        )
                    )
                )
            )
        )
    )
)
echo:
goto :eof

:ERROR
call :USAGE notype
if not "%~1"=="" (
  echo ----------------------------------------------------------------------
  echo ERROR:  %* 1>&2
)
goto :eof

REM -------------------------------------------------------------------------
:RUN
if "%1"=="--help" (
  call :USAGE
  goto :DONE
)
if "%1"=="-help" (
  call :USAGE
  goto :DONE
)
if "%1"=="help" (
  call :USAGE
  goto :DONE
)
set prj_name=%~1
set prj_type=%~2
set toolkit=%~3
if "%prj_name%"=="" (
  call :ERROR Mandatory argument 'name' is missing
  goto :DONE
)
if "%prj_type%"=="" (
  call :ERROR Mandatory argument 'type' is missing
  goto :DONE
)
if "%toolkit%"=="" (
  call :ERROR Mandatory argument 'builddir' is missing
  goto :DONE
)
if exist %prj_name% (
  call :ERROR File or directory %prj_name% already exists
  goto :DONE
)
mkdir %prj_name%
if not exist %prj_name% (
  call :ERROR Failed to create directory %prj_name%
  goto :DONE
)
cd %prj_name%

REM -------------------------------------------------------------------------
svn co %repository%/%rep_inc%/%rep_sample%/%prj_type& %rep_inc% >NUL 2>&1
svn co %repository%/%rep_src%/%rep_sample%/%prj_type% %rep_src% >NUL
for /f %%a in ('dir /s /a:h /b .svn') do (
  rmdir /S /Q %%a
)
del /S /Q Makefile.* >NUL

REM create configure script
(
    echo @echo off
    echo set srcdir=%%CD%%
    echo set script_name=%%~nx0
    echo "%toolkit%\src\build-system\cmake\cmake-cfg-vs.bat" --rootdir="%%srcdir%%" --caller=%%script_name%% %%*
) > %prj_cfg%

REM modify CMakeLists.txt
cd %rep_src%
(
    echo cmake_minimum_required^(VERSION 3.3^)
    echo:
    echo project^(%prj_name%^)
    echo:
    echo set^(NCBI_EXTERNAL_TREE_ROOT   "%toolkit:\=/%"^)
    echo include^(${NCBI_EXTERNAL_TREE_ROOT}/src/build-system/cmake/CMake.NCBItoolkit.cmake^)
    echo:
    type %prj_prj%
) >%prj_tmp%
del %prj_prj%
move /Y %prj_tmp% %prj_prj% >NUL
REM #########################################################################

echo Created project %prj_name%
echo To configure:  cd %prj_name%; %prj_cfg% ^<arguments^>
echo For help:      cd %prj_name%; %prj_cfg% --help
:DONE
cd %initial_dir%
endlocal
exit /b 0
