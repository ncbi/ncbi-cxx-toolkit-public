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
REM Build wxWindows
REM
REM ---------------------------------------------------------------------------
REM $Log$
REM ===========================================================================

IF NOT _%1% == _ GOTO DLL
SET DSWFILE=
GOTO NEXTARG
:DLL
IF NOT _%1% == _DLL GOTO LIB
SET DSWFILE=wxvc_dll.dsw
GOTO NEXTARG
:LIB
IF NOT _%1% == _LIB GOTO ERR
SET DSWFILE=wxvc.dsw
GOTO NEXTARG
:ERR
ECHO FATAL: Unknown parameter "%1%". Please specify ALL, LIB or DLL.
GOTO EXIT

:NEXTARG
IF _%2% == _ GOTO DEFAULT
IF NOT _%FILE% == _ GOTO SETCFG
ECHO ERROR: Target ALL. Configuration(s) ignored.
GOTO DEFAULT
:SETCFG
SET CFG=%2%
GOTO LOOP
:DEFAULT
SET CFG=ALL

:LOOP
IF %CFG% == ALL GOTO CONTINUE
IF %CFG% == Debug GOTO CONTINUE
IF %CFG% == DebugMT GOTO CONTINUE
IF %CFG% == DebugDLL GOTO CONTINUE
IF %CFG% == Release GOTO CONTINUE
IF %CFG% == ReleaseMT GOTO CONTINUE
IF %CFG% == ReleaseDLL GOTO CONTINUE
ECHO INFO: The following configuration names are recognized:
ECHO       Debug DebugMT DebugDLL Release ReleaseMT ReleaseDLL ALL
ECHO FATAL: Unknown configuration name %CFG%. Please correct.
GOTO EXIT

:CONTINUE
IF NOT _%DSWFILE% == _ GOTO BUILD
IF _%CFG% == _ GOTO SETDLL
SET FILE=wxvc.dsw
GOTO BUILD
:SETDLL
SET DSWFILE=dummy
SET FILE=wxvc_dll.dsw
SET CFG=ALL
GOTO BUILD

:BUILD
ECHO INFO: Building "all - %CFG%" using %FILE%.
IF EXIST %FILE% GOTO START
ECHO FATAL: File %FILE% not found. Please check your settings.
GOTO EXIT
:START
msdev.exe %FILE% /MAKE "all - %CFG%"
IF NOT ERRORLEVEL 0 GOTO ABORT

IF NOT %CFG% == ALL GOTO ITERATE
IF NOT _%DSWFILE% == _ GOTO COMPLETE
SET CFG=
GOTO CONTINUE

:ITERATE
SHIFT
IF _%2% == _ GOTO COMPLETE
SET CFG=%2%
GOTO LOOP

:ABORT
ECHO INFO: Aborted, please see error log. Hit any key to quit...
PAUSE > NUL
GOTO EXIT
:COMPLETE
ECHO INFO: Build complete.
:EXIT
