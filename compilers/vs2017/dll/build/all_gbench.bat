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
REM Author:  Anton Lavrentiev
REM
REM Build GBENCH
REM
REM ===========================================================================

call "..\..\msvcvars.bat"

IF _%1% == _    GOTO BUILDALL
IF _%1% == _ALL GOTO BUILDALL
GOTO CONFIG

:BUILDALL
CALL %0 DebugDLL ReleaseDLL
GOTO EXIT

:CONFIG
TIME /T
ECHO INFO: Configure "dll\gbench"
msbuild gbench\ncbi_gbench.sln /t:"_CONFIGURE_:Rebuild" /p:Configuration=ReleaseDLL
IF ERRORLEVEL 1 GOTO ABORT

SET CFG=%1%

:ARGLOOP
IF %CFG% == DebugDLL GOTO CONTINUE
IF %CFG% == ReleaseDLL GOTO CONTINUE
ECHO INFO: The following configuration names are recognized:
ECHO       DebugDLL ReleaseDLL
ECHO FATAL: Unknown configuration name %CFG%. Please correct.
GOTO EXIT

:CONTINUE
TIME /T
ECHO INFO: Building "dll\gbench\%CFG%"
msbuild gbench\ncbi_gbench.sln /t:"_BUILD_ALL_" /p:Configuration=%CFG%
IF ERRORLEVEL 1 GOTO ABORT

SHIFT
IF _%1% == _ GOTO COMPLETE
SET CFG=%1%
GOTO ARGLOOP

:ABORT
ECHO INFO: Build failed.
GOTO EXIT
:COMPLETE
ECHO INFO: Build complete.
:EXIT
