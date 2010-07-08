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
REM Configure/build/check NCBI C++ core tree in specified configuration(s)
REM
REM     make_ncbi.bat <configure|build|make|check> <static|dll> <32|64> [cfgs..]
REM
REM     %1% - Configure, build, check or build and configure (make) build tree.
REM     %2% - Type of used libraries (static, dll).
REM     %3% - This parameter is ignored on MSVC7, should be "32" always.
REM     %4% - Configuration name(s)
REM           (ALL, Debug, DebugDLL, DebugMT, Release, ReleaseDLL, ReleaseMT).
REM           By default build all possible configurations (ALL).
REM
REM ===========================================================================


IF _%2 == _dll GOTO DLL

:STATIC
@call make.bat %1 ncbi_cpp %2 %3 %4 %5 %6 %7 %8 %9
EXIT %ERRORLEVEL%

:DLL
@call make.bat %1 ncbi_cpp_dll %2 %3 %4 %5 %6 %7 %8 %9
EXIT %ERRORLEVEL%
