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
REM Check platform name and create a lock to prevent
REM running more than one instance of project_tree_builder.exe at the same time.
REM
REM ===========================================================================

set CFG_PLATFORM=__configured_platform
set CFG_LIST=Win32 x64
set PTB_RUNNING=__configure.lock

if _%PTB_PLATFORM%==_ (
  echo PTB_PLATFORM is undefined
  goto return_error
)
if _%1%==_  goto report_usage
if _%2%==_  goto report_usage
set MSVC_PRJ=%~2%
if %1%==ON  goto do_ON
if %1%==OFF goto do_OFF


:report_usage
echo The script checks MSVC platform name and creates a lock to prevent
echo running more than one -CONFIGURE- at the same time.
echo Usage:
echo    lock_ptb_config ON msvc_prj_folder, or
echo    lock_ptb_config OFF msvc_prj_folder
goto return_error

:return_error
exit /b 1

:do_ON
for %%c in ( %CFG_LIST% ) do (
  if exist "%MSVC_PRJ%%CFG_PLATFORM%.%%c" (
    if not %%c==%PTB_PLATFORM% (
      echo ******************************************************************************
      echo Requested platform %PTB_PLATFORM% does not match already configured %%c
      echo If you believe it is not so, - delete '%MSVC_PRJ%%CFG_PLATFORM%.%%c' file
      echo ******************************************************************************
      goto return_error
    )
    goto do_ON_lock
  )
)
echo %CFG_PLATFORM% > "%MSVC_PRJ%%CFG_PLATFORM%.%PTB_PLATFORM%"

:do_ON_lock
if exist "%MSVC_PRJ%%PTB_RUNNING%" (
  echo ******************************************************************************
  echo There is another CONFIGURE process running in this tree.
  echo If you believe it is not so, - delete '%MSVC_PRJ%%PTB_RUNNING%' file
  echo ******************************************************************************
  goto return_error
)
echo ptb_running > "%MSVC_PRJ%%PTB_RUNNING%"
goto done


:do_OFF
if exist "%MSVC_PRJ%%PTB_RUNNING%" (
  del "%MSVC_PRJ%%PTB_RUNNING%"
)
goto done

:done

