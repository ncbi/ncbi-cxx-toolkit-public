@echo off
setlocal ENABLEDELAYEDEXPANSION
REM #########################################################################
REM  $Id$
REM  Create new CMake project which uses prebuilt NCBI C++ toolkit from a sample
REM  Author: Andrei Gourianov, gouriano@ncbi
REM #########################################################################

set initial_dir=%CD%
set script_name=%~nx0
set script_dir=%~dp0
set tree_root=%initial_dir%

set vsver=2019
set build_root=%NCBI%\Lib\Ncbi\CXX_Toolkit
set build_vs=vs%vsver%.64
set build_pfx=cxx.cmake.
set build_def=stable
set build_dir=%build_pfx%%build_def%

set repository=https://svn.ncbi.nlm.nih.gov/repos/toolkit/trunk/c++
set rep_inc=include
set rep_src=src
set rep_sample=sample
set prj_prj=CMakeLists.txt
set cfg_cfg=configure.bat
goto :RUN

REM #########################################################################
:USAGE
echo USAGE:
echo    %script_name% ^<name^> ^<type^> [builddir] [OPTIONS]
echo SYNOPSIS:
echo   Create new CMake project which uses prebuilt NCBI C++ toolkit from a sample.
echo ARGUMENTS:
echo   --help       -- print Usage
echo   ^<name^>       -- project name (destination directory)
echo   ^<type^>       -- project type
echo   builddir     -- root directory of the pre-built NCBI C++ toolkit
echo                   default: %build_def%
echo OPTIONS:
echo   --noconfig       -- skip configuring build
echo   --with-vs=N      -- use Visual Studio N generator 
echo          examples:    --with-vs=2019  (default)
echo                       --with-vs=2017
echo:
if "%~1"=="" (
    echo:
    echo The following project types are available:
    for /f %%a in ('svn list -R %repository%/%rep_src%/%rep_sample%') do (
        for /f %%b in ('echo %%a^| findstr /r ".*/$"') do (
            if not "%%b"=="" (
                for /f "tokens=1,2,* delims=/" %%c in ('echo %%b') do (
                    if not "%%d"=="" (
                        if "%%e"=="" (
                                   if "%%c/%%d"=="app/alnmgr"         (echo   app/alnmgr          an application that uses Alignment Manager
                            ) else if "%%c/%%d"=="app/asn"            (echo   app/asn             an application that uses ASN.1 specification
                            ) else if "%%c/%%d"=="app/basic"          (echo   app/basic           a simple application
                            ) else if "%%c/%%d"=="app/blast"          (echo   app/blast           a BLAST application
                            ) else if "%%c/%%d"=="app/cgi"            (echo   app/cgi             a CGI application
                            ) else if "%%c/%%d"=="app/connect"        (echo   app/connect         a Usage report logging
                            ) else if "%%c/%%d"=="app/dbapi"          (echo   app/dbapi           a DBAPI application
                            ) else if "%%c/%%d"=="app/deployable_cgi" (echo   app/deployable_cgi  a CD-deployable CGI application
                            ) else if "%%c/%%d"=="app/eutils"         (echo   app/eutils          an eUtils client application
                            ) else if "%%c/%%d"=="app/http_session"   (echo   app/http_session    an application that uses CHttpSession
                            ) else if "%%c/%%d"=="app/lds"            (echo   app/lds             an application that uses local data store
                            ) else if "%%c/%%d"=="app/multicmd"       (echo   app/multicmd        a simple command-based application
                            ) else if "%%c/%%d"=="app/netcache"       (echo   app/netcache        an application that uses NetCache
                            ) else if "%%c/%%d"=="app/netschedule"    (echo   app/netschedule     an NCBI GRID ^(NetSchedule^) application
                            ) else if "%%c/%%d"=="app/objects"        (echo   app/objects         an application that uses ASN.1 objects
                            ) else if "%%c/%%d"=="app/objmgr"         (echo   app/objmgr          an application that uses Object Manager
                            ) else if "%%c/%%d"=="app/sdbapi"         (echo   app/sdbapi          a Simple-DBAPI application
                            ) else if "%%c/%%d"=="app/serial"         (echo   app/serial          a data serialization application
                            ) else if "%%c/%%d"=="app/soap"           (echo   app/soap/client     a SOAP client
                                                                       echo   app/soap/server     a SOAP server
                            ) else if "%%c/%%d"=="app/unit_test"      (echo   app/unit_test       a Boost-based unit test application
                            ) else if "%%c/%%d"=="lib/asn_lib"        (echo   lib/asn_lib         a static library from ASN.1 specification
                            ) else if "%%c/%%d"=="lib/basic"          (echo   lib/basic           a simple static library
                            ) else if "%%c/%%d"=="lib/dtd"            (echo   lib/dtd             a static library from DTD specification
                            ) else if "%%c/%%d"=="lib/jsd"            (echo   lib/jsd             a static library from JSON Schema specification
                            ) else if "%%c/%%d"=="lib/xsd"            (echo   lib/xsd             a static library from XML Schema specification
                            ) else (echo   %%c/%%d)
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


:GET_BUILD_DIR
set all=
for /f "tokens=1" %%a in ('dir /A:D /B "%build_root%\%build_vs%\%build_pfx%*"') do (
  set folder=%%a
  set folder=!folder:%build_pfx%=!
  set all=!all! !folder!
)
:GET_BUILD_DIR_AGAIN
echo:
echo Please pick a stability level
echo Available:
echo:
for %%b in ( %all% ) do (
echo %%b
)
echo:
set /p stability=Desired stability ^(default = %build_def%^): 
if "!stability!"=="" set stability=%build_def%
if not exist "%build_root%\%build_vs%\%build_pfx%!stability!" (
goto :GET_BUILD_DIR_AGAIN
)
set builddir=%build_root%\%build_vs%\%build_pfx%!stability!
goto :eof


:GET_PREBUILD_PATH
for %%a in ( %prebuilds% ) do (
  set cfg_def=%%a
:GET_PREBUILD_PATH_AGAIN
  echo:
  echo Please pick a build configuration
  echo Available:
  echo:
  for %%b in ( %prebuilds% ) do (
    echo %%b
  )
  echo:
  set /p build_config=Desired configuration ^(default = !cfg_def!^): 
  if "!build_config!"=="" set build_config=!cfg_def!
  if not exist %prebuilt_dir%\!build_config! goto :GET_PREBUILD_PATH_AGAIN
  set prebuilt_name=!build_config!
  goto :eof
)
goto :eof

REM -------------------------------------------------------------------------
:RUN

if "%1"=="" (
  set do_help=YES
)
set pos=0
set unknown=
set prj_name=
set prj_type=
set toolkit=
set unknown=

:PARSEARGS
if "%~1"=="" goto :ENDPARSEARGS
if "%1"=="--help"              (set do_help=YES&  echo:&      goto :CONTINUEPARSEARGS)
if "%1"=="-help"               (set do_help=YES&  echo:&      goto :CONTINUEPARSEARGS)
if "%1"=="help"                (set do_help=YES&  echo:&      goto :CONTINUEPARSEARGS)
if "%1"=="-h"                  (set do_help=YES&  echo:&      goto :CONTINUEPARSEARGS)
if "%1"=="/?"                  (set do_help=YES&  echo:&      goto :CONTINUEPARSEARGS)
if "%1"=="--noconfig"          (set noconfig=YES&      goto :CONTINUEPARSEARGS)
if "%1"=="--with-vs"           (set vsver=%~2& set build_vs=vs%~2.64& shift& goto :CONTINUEPARSEARGS)
if "%pos%"=="0" (
  set prj_name=%~1
) else if "%pos%"=="1" (
  set prj_type=%~1
) else if "%pos%"=="2" (
  set toolkit=%~1
) else (
  set unknown=%unknown% %1
)
set /a pos=%pos% + 1
:CONTINUEPARSEARGS
shift
goto :PARSEARGS
:ENDPARSEARGS

if not exist "%build_root%\%build_vs%\%build_pfx%%build_def%" (
    for /f "tokens=1" %%a in ('dir /A:D /B "%build_root%\%build_vs%\%build_pfx%*"') do (
      set folder=%%a
      set folder=!folder:%build_pfx%=!
      set build_def=!folder!
    )
)
REM -------------------------------------------------------------------------
if not "%do_help%"=="" (
  call :USAGE
  goto :DONE
)
echo:
if not "%unknown%"=="" (
  call :ERROR unknown options: %unknown%
  goto :DONE
)
if "%prj_name%"=="" (
  call :ERROR Mandatory argument 'name' is missing
  goto :DONE
)
if "%prj_type%"=="" (
  call :ERROR Mandatory argument 'type' is missing
  goto :DONE
)
if exist %prj_name% (
  call :ERROR File or directory %prj_name% already exists
  goto :DONE
)
if "%toolkit%"=="" (
  call :GET_BUILD_DIR
) else (
  if exist "%toolkit%" (
    set builddir=%toolkit%
  ) else if exist "%build_root%\%build_vs%\%toolkit%" (
    set builddir=%build_root%\%build_vs%\%toolkit%
  ) else if exist "%build_root%\%build_vs%\cxx.cmake.%toolkit%" (
    set builddir=%build_root%\%build_vs%\cxx.cmake.%toolkit%
  )
)
if not exist "%builddir%" (
  echo ----------------------------------------------------------------------
  if "%toolkit%"=="" (
    echo ERROR:  Directory not found: %builddir% 1>&2
  ) else (
    echo ERROR:  Directory not found: %toolkit% 1>&2
  )
  echo Try one of these:
  for /f "tokens=1" %%a in ('dir /A:D /B "%build_root%\%build_vs%\%build_pfx%*"') do (
    set folder=%%a
    set folder=!folder:%build_pfx%=!
    echo !folder!
  )
  goto :DONE
)

set prebuilds=
if "%noconfig%"=="" (
  for /f "tokens=1" %%a in ('dir /A:D /B  %builddir%') do (
    if exist "%builddir%\%%a\cmake\buildinfo" (
      set prebuilds=!prebuilds! %%a
    )
  )
  set prebuilt_dir=%builddir%
  call :GET_PREBUILD_PATH
)

mkdir %prj_name%
if not exist %prj_name% (
  call :ERROR Failed to create directory %prj_name%
  goto :DONE
)
cd %prj_name%

REM -------------------------------------------------------------------------
mkdir %rep_inc% 2>NUL
mkdir %rep_src% 2>NUL
svn co %repository%/%rep_inc%/%rep_sample%/%prj_type& %rep_inc% >NUL 2>&1
svn co %repository%/%rep_src%/%rep_sample%/%prj_type% %rep_src% >NUL
for /f %%a in ('dir /s /a:h /b .svn') do (
  rmdir /S /Q %%a
)
del /S /Q Makefile.* >NUL

REM create configure script
(
    echo @echo off
    echo set script_dir=%%~dp0?
    echo set script_dir=%%script_dir:\?=%%
    echo set script_name=%%~nx0
    echo "%builddir%\src\build-system\cmake\cmake-cfg-vs.bat" --rootdir="%%script_dir%%" --caller=%%script_name%% %%*
) >%cfg_cfg%

REM modify CMakeLists.txt
(
    echo cmake_minimum_required^(VERSION 3.7^)
    echo project^(%prj_name%^)
    echo include^(%builddir:\=/%/src/build-system/cmake/CMake.NCBItoolkit.cmake^)
    echo NCBI_add_subdirectory^(%rep_src%^)
) >%prj_prj%
REM #########################################################################

echo Created project %prj_name%
echo To configure:  cd %prj_name%^& %cfg_cfg% ^<arguments^>
echo For help:      cd %prj_name%^& %cfg_cfg% --help

if "%noconfig%"=="" (
  echo ----------------------------------------------------------------------
  call configure.bat --with-vs=%vsver% --with-prebuilt=%prebuilt_dir%\%prebuilt_name%
)

:DONE
cd %initial_dir%
endlocal
exit /b 0
