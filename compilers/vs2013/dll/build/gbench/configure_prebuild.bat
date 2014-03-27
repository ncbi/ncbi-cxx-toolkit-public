@echo off
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
REM Author:  Andrei Gourianov
REM
REM This script is called by compilers/msvcNNN_prj/ptb.bat
REM when building CONFIGURE project, before performing any meaningful actions.
REM
REM So, this is a good place to put any custom PRE-BUILD operations.
REM For example, set environment variables.
REM
REM ===========================================================================

set initial_dir=%CD%
set script_name=%~nx0
cd %~dp0
for /f "delims=" %%a in ('cd') do (set script_dir=%%a)
cd %initial_dir%

set cfgs=project_tree_builder.ini.custom

set NCBI_CONFIG____ENABLEDUSERREQUESTS__NCBI-UNICODE=
if not exist "%script_dir%\%cfgs%" (
  set NCBI_CONFIG____ENABLEDUSERREQUESTS__NCBI-UNICODE=1
)
