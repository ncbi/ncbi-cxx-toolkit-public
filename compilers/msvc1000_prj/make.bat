@ECHO OFF
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
REM Author:  Vladimir Ivanov
REM
REM Configure/build/check NCBI C++ tree in specified configuration(s)
REM
REM     make.bat <configure|build|make|check> <solution> <static|dll> <32|64> [cfgs..]
REM
REM     %1% - Configure, build, make (configure and build_ or check build tree.
REM     %2% - Solution file name without extention (relative path from build directory).
REM     %3% - Type of used libraries (static, dll).
REM     %4% - 32/64-bits architerture.
REM     %5% - Configuration name(s)
REM           (DebugDLL, DebugMT, ReleaseDLL, ReleaseMT, Unicode_*).
REM           By default (if not specified) build DebugDLL and ReleaseDLL only.
REM     ... - Options (--with-openmp)
REM
REM ===========================================================================



rem --- Configuration

set compiler=msvc10
set default_cfgs=ReleaseDLL DebugDLL

rem Always configure with additional Unicode configurations
if _%SRV_NAME% == _ set SRV_NAME=%COMPUTERNAME%

call msvcvars.bat > NUL
set archw=Win32
if _%arch%_ == _64_ set archw=x64


rem --- Required parameters

set cmd=%~1%
set solution=%~2
set libdll=%~3
set arch=%~4

shift
shift
shift
shift

if "%cmd%"      == ""  goto NOARGS
if "%solution%" == ""  goto USAGE
if "%libdll%"   == ""  goto USAGE
if "%arch%"     == ""  goto USAGE

goto PARSEARGS


rem --------------------------------------------------------------------------------
:NOARGS

if exist configure_make.bat (
  configure_make.bat
) else (
  goto USAGE
)

:USAGE

echo FATAL: Invalid parameters. See script description.
echo FATAL: Passed arguments: %*
goto ABORT



rem --------------------------------------------------------------------------------
rem Parse arguments
:PARSEARGS

set with_openmp=
set cfgs=
set unknown=

:PARSEARGSLOOP
if "%1" == "" goto ENDPARSEARGS
if "%1" == "--with-openmp"       (set with_openmp=%1 & goto CONTINUEPARSEARGS)
if "%1" == "DebugDLL"            (set cfgs=%cfgs% %1 & goto CONTINUEPARSEARGS)
if "%1" == "DebugMT"             (set cfgs=%cfgs% %1 & goto CONTINUEPARSEARGS)
if "%1" == "ReleaseDLL"          (set cfgs=%cfgs% %1 & goto CONTINUEPARSEARGS)
if "%1" == "ReleaseMT"           (set cfgs=%cfgs% %1 & goto CONTINUEPARSEARGS)
if "%1" == "Unicode_DebugDLL"    (set cfgs=%cfgs% %1 & goto CONTINUEPARSEARGS)
if "%1" == "Unicode_DebugMT"     (set cfgs=%cfgs% %1 & goto CONTINUEPARSEARGS)
if "%1" == "Unicode_ReleaseDLL"  (set cfgs=%cfgs% %1 & goto CONTINUEPARSEARGS) 
if "%1" == "Unicode_ReleaseMT"   (set cfgs=%cfgs% %1 & goto CONTINUEPARSEARGS)
set unknown=%unknown% %1
:CONTINUEPARSEARGS
shift
goto PARSEARGSLOOP

:ENDPARSEARGS

if not "%unknown%" == "" (
   echo FATAL: Unknown configuration names or options specified:%unknown%.
   echo %cmd%
   goto ABORT
)

if "%cfgs%" == "" set cfgs=%default_cfgs%

rem -- Check command

if _%cmd% == _configure goto CONFIG
if _%cmd% == _make      goto CONFIG
if _%cmd% == _build     goto CFGLOOP
if _%cmd% == _check     goto CFGLOOP
echo FATAL: Unknown action name %cmd%. Please correct.
echo The following action names are recognized: configure, build, make, check.
goto ABORT


rem --------------------------------------------------------------------------------
rem Configure: always use ReleaseDLL
:CONFIG

rem --- Process options
if not "%with_openmp%" == "" (
   echo INFO: Ebable OpenMP.
   bash -c "./build_util.sh %with_openmp%; exit $?"
   if errorlevel 1 goto ABORT
)

time /t
echo INFO: Configure "%libdll%\%solution% [ReleaseDLL|%arch%]"
%DEVENV% %libdll%\build\%solution%.sln /build "ReleaseDLL|%archw%" /project "-CONFIGURE-"
if errorlevel 1 goto ABORT
if not _%cmd% == _make goto COMPLETE


rem --------------------------------------------------------------------------------
rem Process all configurations
:CFGLOOP

for %%c in (%cfgs%) do (
   time /t
   if _%cmd% == _make (
      call :make %%c
   )
   if _%cmd% == _check (
      call :check %%c
   )
   if _%cmd% == _build (
      call :make %%c
      if errorlevel 1 goto ABORT
      call :check %%c
   )
   if errorlevel 1 goto ABORT
)
goto COMPLETE


rem --------------------------------------------------------------------------------
rem Subroutines

:make
   echo INFO: Building "%libdll%\%solution% [%1|%arch%]"
   %DEVENV% %libdll%\build\%solution%.sln /build "%1|%archw%" /project "-BUILD-ALL-"
   exit /b %errorlevel%

:check
   echo INFO: Checking init
   bash -c "../../scripts/common/check/check_make_win_cfg.sh init; exit $?"
   set err=0
   echo INFO: Create check script for "%libdll%\%solution% [%1|%arch%]"
   bash -c "../../scripts/common/check/check_make_win_cfg.sh create %solution% %libdll% %1; exit $?"
   if errorlevel 1 exit /b %errorlevel%
   echo INFO: Checking "%libdll%\%solution% [%1|%arch%]"
   SET CHECKSH=%libdll%/build/%solution%.check/%1/check.sh
   bash -c "%CHECKSH% run; exit $?"
   if errorlevel 1 set err=1
   bash -c "cp %CHECKSH%.journal check.sh.%libdll%_%1.journal; cp %CHECKSH%.log check.sh.%libdll%_%1.log"
   rem Load testsuite results into DB works only if NCBI_AUTOMATED_BUILD is set to 1
   if _%NCBI_AUTOMATED_BUILD% == _1 (
      bash -c "%CHECKSH% load_to_db; exit $?"
      if errorlevel 1 set err=1
   )
   copy /y /b check.sh.*.journal check.sh.journal
   copy /y /b check.sh.*.log     check.sh.log
   exit /b %err%


rem --------------------------------------------------------------------------------

:ABORT
echo INFO: %cmd% failed.
exit /b 1

:COMPLETE
echo INFO: %cmd% complete.
exit /b 0

