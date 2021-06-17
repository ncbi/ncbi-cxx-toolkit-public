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
REM Run protoc.exe to generate sources from PROTO specification
REM
REM DO NOT ATTEMPT to run this bat file manually
REM
REM ===========================================================================

set PROTOC_APP=protoc.exe
set GRPC_APP=grpc_cpp_plugin.exe

REM remove the following after the transition period!
if "%GENERATOR_PATH%"=="" (
  set GENERATOR_PATH=\\snowman\win-coremake\Lib\ThirdParty\grpc\vs2019.64\1.36.4-ncbi1\bin
)

for %%v in ("%GENERATOR_PATH%" "%TREE_ROOT%" "%BUILD_TREE_ROOT%" "%PTB_PLATFORM%") do (
  if %%v=="" (
    echo ERROR: required environment variable is missing
    echo DO NOT ATTEMPT to run this bat file manually
    exit /b 1
  )
)


set PROTOC_EXE=%GENERATOR_PATH%\%PROTOC_APP%
set GRPC_PLUGIN=%GENERATOR_PATH%\%GRPC_APP%

set input_spec_path=
set input_spec_name=
set input_def_path=
set subtree=
set srcroot=
:PARSEARGS
if _%1==_ goto ENDPARSEARGS
if "%dest%"=="inSPEC"   (set input_spec_path=%~1& set input_spec_name=%~n1& set dest=& goto CONTINUEPARSEARGS)
if "%dest%"=="inDEF"    (set input_def_path=%~1& set dest=& goto CONTINUEPARSEARGS)
if "%dest%"=="subtree"  (set subtree=%1&     set dest=& goto CONTINUEPARSEARGS)
if "%dest%"=="srcroot"  (set srcroot=%1&     set dest=& goto CONTINUEPARSEARGS)
if "%1"=="-m"           (set dest=inSPEC&               goto CONTINUEPARSEARGS)
if "%1"=="-od"          (set dest=inDEF&                goto CONTINUEPARSEARGS)
if "%1"=="-or"          (set dest=subtree&              goto CONTINUEPARSEARGS)
if "%1"=="-oR"          (set dest=srcroot&              goto CONTINUEPARSEARGS)
if "%1"=="-M"           (goto ENDPARSEARGS)
:CONTINUEPARSEARGS
shift
REM echo parsing %1
goto PARSEARGS
:ENDPARSEARGS

set initial_dir=%CD%
cd %input_spec_path%\..
set input_spec_dir=%CD%
cd %initial_dir%
for %%i in ("%input_spec_path%") do set input_spec_spec=%%~nxi

cd %TREE_ROOT%\src

set have_protoc=no
set have_grpc=no
if exist "%PROTOC_EXE%" (
  set have_protoc=yes
  if exist "%GRPC_PLUGIN%" (
    set have_grpc=yes
  )
)

if "%have_protoc%"=="yes" (
  %PROTOC_EXE% --version
  %PROTOC_EXE% --cpp_out=. -I. %subtree%%input_spec_spec%
) else (
  if not exist %subtree%%input_spec_name%.pb.cc (
    type nul > %subtree%%input_spec_name%.pb.cc
    type nul > %subtree%%input_spec_name%.pb.h
  )
)

if "%have_grpc%"=="yes" (
  %PROTOC_EXE% --grpc_out=generate_mock_code=true:. --plugin=protoc-gen-grpc="%GRPC_PLUGIN%" -I. %subtree%%input_spec_spec%
) else (
    if not exist %subtree%%input_spec_name%.grpc.pb.cc (
      type nul > %subtree%%input_spec_name%.grpc.pb.cc
      type nul > %subtree%%input_spec_name%.grpc.pb.h
      type nul > %subtree%%input_spec_name%_mock.grpc.pb.h
  )
)

if not exist "%TREE_ROOT%\include\%subtree%" (
  mkdir "%TREE_ROOT%\include\%subtree%"
)
xcopy /Y /D /F "%input_spec_dir%\*.h" "%TREE_ROOT%\include\%subtree%"
del "%input_spec_dir%\*.h"
cd %initial_dir%
