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
REM     %4% - This parameter is ignored on MSVC7, should be "32" always.
REM     %5% - Configuration name(s)
REM           (ALL, Debug, DebugDLL, DebugMT, Release, ReleaseDLL, ReleaseMT).
REM           By default build all possible configurations (ALL).
REM
REM ===========================================================================


@call "%VS71COMNTOOLS%vsvars32.bat" > NUL

SET CMD=%1%
SET SOLUTION=%2%
SET LIBDLL=%3%
SET ARCH=%4%
SET CFG=%5%

IF _%CMD% == _      GOTO USAGE
IF _%SOLUTION% == _ GOTO USAGE
IF _%LIBDLL% == _   GOTO USAGE
IF _%ARCH% == _     GOTO USAGE
IF _%CFG% == _      GOTO BUILDALL
IF _%CFG% == _ALL   GOTO BUILDALL
GOTO CHECKCMD


:USAGE

ECHO FATAL: Invalid parameters. See script description.
ECHO FATAL: %0 %1 %2 %3 %4 %5 %6 %7 %8 %9
GOTO ABORT


:BUILDALL

CALL %0 %CMD% %SOLUTION% %LIBDLL% %ARCH% ReleaseDLL Release DebugDLL Debug
GOTO EXIT


:CHECKCMD

SET ARCH=32
IF _%CMD% == _configure GOTO CONFIG
IF _%CMD% == _make      GOTO CONFIG
IF _%CMD% == _build     GOTO BUILD
IF _%CMD% == _check     GOTO CHECK
ECHO The following action names are recognized: configure, build, make, check.
ECHO FATAL: Unknown action name %CMD%. Please correct.
GOTO ABORT


REM ###########################################################################
:CONFIG

IF %CFG% == Debug      GOTO CONTCFG
IF %CFG% == DebugMT    GOTO CONTCFG
IF %CFG% == DebugDLL   GOTO CONTCFG
IF %CFG% == Release    GOTO CONTCFG
IF %CFG% == ReleaseMT  GOTO CONTCFG
IF %CFG% == ReleaseDLL GOTO CONTCFG
ECHO The following configuration names are recognized:
ECHO     Debug DebugMT DebugDLL Release ReleaseMT ReleaseDLL
ECHO FATAL: Unknown configuration name %CFG%. Please correct.
GOTO ABORT
:CONTCFG
TIME /T
ECHO INFO: Configure "%LIBDLL%\%SOLUTION% [%CFG%|%ARCH%]"
devenv %LIBDLL%\build\%SOLUTION%.sln /rebuild %CFG% /project "-CONFIGURE-"
IF ERRORLEVEL 1 GOTO ABORT
IF NOT _%CMD% == _make GOTO COMPLETE


REM ###########################################################################
:BUILD

:ARGLOOPB
IF %CFG% == Debug      GOTO CONTBLD
IF %CFG% == DebugMT    GOTO CONTBLD
IF %CFG% == DebugDLL   GOTO CONTBLD
IF %CFG% == Release    GOTO CONTBLD
IF %CFG% == ReleaseMT  GOTO CONTBLD
IF %CFG% == ReleaseDLL GOTO CONTBLD
ECHO The following configuration names are recognized:
ECHO     Debug DebugMT DebugDLL Release ReleaseMT ReleaseDLL
ECHO FATAL: Unknown configuration name %CFG%. Please correct.
GOTO ABORT
:CONTBLD
TIME /T
ECHO INFO: Building "%LIBDLL%\%SOLUTION% [%CFG%|%ARCH%]"
devenv %LIBDLL%\build\%SOLUTION%.sln /build %CFG% /project "-BUILD-ALL-"
IF ERRORLEVEL 1 GOTO ABORT
SHIFT
IF _%5% == _ GOTO COMPLETE
SET CFG=%5%
GOTO ARGLOOPB


REM ###########################################################################
:CHECK

ECHO INFO: Checking init
bash -c "../../scripts/common/check/check_make_win_cfg.sh init; exit $?"
SET ERRORLEV=0
:ARGLOOPC
IF %CFG% == Debug      GOTO CONTCH
IF %CFG% == DebugMT    GOTO CONTCH
IF %CFG% == DebugDLL   GOTO CONTCH
IF %CFG% == Release    GOTO CONTCH
IF %CFG% == ReleaseMT  GOTO CONTCH
IF %CFG% == ReleaseDLL GOTO CONTCH
ECHO The following configuration names are recognized:
ECHO     Debug DebugMT DebugDLL Release ReleaseMT ReleaseDLL
ECHO FATAL: Unknown configuration name %CFG%. Please correct.
GOTO ABORT
:CONTCH
ECHO INFO: Create check script for "%LIBDLL%\%SOLUTION% [%CFG%|%ARCH%]"
bash -c "../../scripts/common/check/check_make_win_cfg.sh create %SOLUTION% %LIBDLL% %CFG%"; exit $?"
IF ERRORLEVEL 1 GOTO ABORT
ECHO INFO: Checking "%LIBDLL%\%SOLUTION% [%CFG%|%ARCH%]"
SET CHECKSH=%LIBDLL%/build/%SOLUTION%.check/%CFG%/check.sh
bash -c "%CHECKSH% run; exit $?"
IF ERRORLEVEL 1 SET ERRORLEV=1
bash -c "cp %CHECKSH%.journal check.sh.%LIBDLL%_%CFG%.journal; cp %CHECKSH%.log check.sh.%LIBDLL%_%CFG%.log"
SHIFT
IF _%5% == _ GOTO CHECKEND
SET CFG=%5%
GOTO ARGLOOPC
:CHECKEND
COPY /Y /B check.sh.*.journal check.sh.journal
COPY /Y /B check.sh.*.log     check.sh.log
IF %ERRORLEV%==0 GOTO COMPLETE


REM ###########################################################################

:ABORT
ECHO INFO: %CMD% failed.
EXIT 1

:COMPLETE
ECHO INFO: %CMD% complete.

:EXIT
EXIT %ERRORLEVEL%
