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
                                   if "%%c/%%d"=="app/alnmgr"         (echo   app/alnmgr          an application that uses Alignment Manager
                            ) else if "%%c/%%d"=="app/asn"            (echo   app/asn             an application that uses ASN.1 specification
                            ) else if "%%c/%%d"=="app/basic"          (echo   app/basic           a simple application
                            ) else if "%%c/%%d"=="app/blast"          (echo   app/blast           a BLAST application
                            ) else if "%%c/%%d"=="app/cgi"            (echo   app/cgi             a CGI application
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
