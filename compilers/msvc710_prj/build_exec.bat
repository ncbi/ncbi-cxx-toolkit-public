@ECHO OFF
REM $Id: build_exec.bat,v 1.1 2004/08/11 13:45:19 ivanov Exp $
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
REM Auxiliary script for build.sh to run C++ build for specified project
REM and configuration. Cygwin cannot run devenv directly inside shell-script.
REM
REM ===========================================================================

if _%1% == _  exit 1

devenv %1 /%2 %3 /project "%4"
goto be_done

:be_abort
echo Usage: "%0 <solution> <command> <cfg> <target>"
exit 1

:be_done
