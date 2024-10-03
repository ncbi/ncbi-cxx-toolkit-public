@echo off
setlocal ENABLEDELAYEDEXPANSION
REM #########################################################################
REM  $Id$
REM  Create new CMake project which uses Conan package of the NCBI C++ toolkit from a sample
REM  Author: Andrei Gourianov, gouriano@ncbi
REM #########################################################################

set initial_dir=%CD%
set script_name=%~nx0
set script_dir=%~dp0
set tree_root=%initial_dir%

set repository=https://svn.ncbi.nlm.nih.gov/repos/toolkit/trunk/c++
set rep_inc=include
set rep_src=src
set rep_sample=sample
set prj_prj=CMakeLists.txt
set prj_con=conanfile.py
set ncbitk=ncbi-cxx-toolkit-core
set tkversion=[^>=28]
set readme=README.txt
goto :RUN

REM #########################################################################
:USAGE
echo USAGE:
echo    %script_name% ^<name^> ^<type^>
echo SYNOPSIS:
echo   Create new CMake project which uses Conan package of the NCBI C++ toolkit from a sample.
echo ARGUMENTS:
echo   --help       -- print Usage
echo   ^<name^>       -- project name (destination directory)
echo   ^<type^>       -- project type
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
if "%pos%"=="0" (
  set prj_name=%~1
) else if "%pos%"=="1" (
  set prj_type=%~1
) else (
  set unknown=%unknown% %1
)
set /a pos=%pos% + 1
:CONTINUEPARSEARGS
shift
goto :PARSEARGS
:ENDPARSEARGS

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

REM create CMakeLists.txt
(
    echo cmake_minimum_required^(VERSION 3.15^)
    echo project^(%prj_name%^)
    echo set^(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin^)
    echo find_package^(%ncbitk% REQUIRED^)
    echo NCBIptb_setup^(^)
    echo NCBI_add_subdirectory^(%rep_src%^)
) >%prj_prj%

REM create conanfile.py
(
  echo from conan import ConanFile
  echo from conan.tools.cmake import cmake_layout
  echo class NCBIapp^(ConanFile^):
  echo     settings = "os", "compiler", "build_type", "arch"
  echo     generators = "CMakeDeps", "CMakeToolchain", "VirtualRunEnv"
  echo     def requirements^(self^):
  echo         self.requires^("%ncbitk%/%tkversion%"^)
  echo     def layout^(self^):
  echo         cmake_layout^(self^)
) >%prj_con%

REM create readme
(
 echo To request a specific version of the toolkit
 echo   edit %prj_con%
 echo:
 echo To install Conan packages:
 echo   conan install . --build missing -s build_type=Release
 echo:
 echo To list available presets:
 echo   cmake --list-presets
 echo:
 echo To run CMake to generate makefiles:
 echo   cmake --preset conan-default
 echo:
 echo To build the project:
 echo   cmake --build --config Release
 echo:
 echo For other command line options, see documentation:
 echo  https://docs.conan.io/2/reference/commands/install.html
 echo  https://cmake.org/cmake/help/latest/manual/cmake.1.html
) >%readme%

REM #########################################################################

echo Created project %prj_name%
echo:
type %readme% 

:DONE
cd %initial_dir%
endlocal
exit /b 0
