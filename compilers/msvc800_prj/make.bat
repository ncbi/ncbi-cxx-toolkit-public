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
REM Configure/build NCBI C++ tree in specified configuration(s)
REM
REM     make.bat <configure|build|make> <solution> <static|dll> <32|64> [cfgs..]
REM
REM     %1% - Build, configure, or build and configure build tree.
REM     %2% - Solution file name (relative path from build directory).
REM     %3% - Type of used libraries (static, dll).
REM     %4% - 32/64-bits architerture.
REM     %5% - Configuration name(s) (ALL, DebugDLL, ReleaseDLL).
REM           By default build all possible configurations (ALL).
REM
REM ===========================================================================


call msvcvars.bat > NUL

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

CALL %0 %CMD% %SOLUTION% %LIBDLL% %ARCH% ReleaseDLL DebugDLL
GOTO EXIT


:CHECKCMD

SET ARCHW=Win32
if _%ARCH%_ == _64_ SET ARCHW=x64

IF _%CMD% == _configure GOTO CONFIG
IF _%CMD% == _make      GOTO CONFIG
IF _%CMD% == _build     GOTO BUILD
ECHO The following action names are recognized: configure, build, make.
ECHO FATAL: Unknown action name %CMD%. Please correct.
GOTO ABORT


:CONFIG

IF %CFG% == DebugDLL   GOTO CONTCFG
IF %CFG% == DebugMT    GOTO CONTCFG
IF %CFG% == ReleaseDLL GOTO CONTCFG
IF %CFG% == ReleaseMT  GOTO CONTCFG
ECHO The following configuration names are recognized:
ECHO     DebugDLL DebugMT ReleaseDLL ReleaseMT
ECHO FATAL: Unknown configuration name %CFG%. Please correct.
GOTO ABORT
:CONTCFG
TIME /T
ECHO INFO: Configure "%LIBDLL%\%SOLUTION% [%CFG%|%ARCH%]"
%DEVENV% %LIBDLL%\build\%SOLUTION% /build "%CFG%|%ARCHW%" /project "-CONFIGURE-"
IF ERRORLEVEL 1 GOTO ABORT
IF NOT _%CMD% == _make GOTO COMPLETE


:BUILD

:ARGLOOP
IF %CFG% == DebugDLL   GOTO CONTCFG
IF %CFG% == DebugMT    GOTO CONTCFG
IF %CFG% == ReleaseDLL GOTO CONTCFG
IF %CFG% == ReleaseMT  GOTO CONTCFG
ECHO The following configuration names are recognized:
ECHO     DebugDLL DebugMT ReleaseDLL ReleaseMT
ECHO FATAL: Unknown configuration name %CFG%. Please correct.
GOTO ABORT
:CONTBLD
TIME /T
ECHO INFO: Building "%LIBDLL%\%SOLUTION% [%CFG%|%ARCH%]"
%DEVENV% %LIBDLL%\build\%SOLUTION% /build "%CFG%|%ARCHW%" /project "-BUILD-ALL-"
IF ERRORLEVEL 1 GOTO ABORT
SHIFT
IF _%5% == _ GOTO COMPLETE
SET CFG=%5%
GOTO ARGLOOP

:ABORT
ECHO INFO: Build failed.
EXIT 1

:COMPLETE
ECHO INFO: Build complete.

:EXIT
EXIT %ERRORLEVEL%
