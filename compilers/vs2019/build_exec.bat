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
REM Auxiliary script for build.sh to run C++ build for specified project
REM and configuration. Cygwin cannot run devenv directly inside shell-script.
REM
REM ===========================================================================


call msvcvars.bat


if _%1% == _  goto be_abort
goto be_prep

:be_abort
rem You should specify logfile or you will not see an output
echo Usage: "%0 <solution> <command> <arch> <cfg> <target> <logfile>"
exit 1


:be_prep
rem Additional safety to have a correct path for Windows full builds:
rem devenv should be called from a build directory defined on the upper level, 
rem abd include substed path. New Cygwin automatically "unfollow" that substitution,
rem that makes a problem to find correct .pdb files after installation.
rem BLD_DIR sets in pc_build.bat.
if not exist "%BLD_DIR%" goto be_build
cd %BLD_DIR%


:be_build
set arch=Win32
if _%5_ == __CONFIGURE__ set arch=x86
if _%3_ == _64_ set arch=x64

rem Next command should be executed last! No other code after it, please.
%DEVENV% %1 /%2 "%4|%arch%" /project "%5" /out "%6"
