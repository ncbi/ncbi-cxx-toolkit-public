#ifndef CORELIB___NCBISYS__HPP
#define CORELIB___NCBISYS__HPP

/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors: Andrei Gourianov
 *
 * File Description:
 *      Wrappers around standard functions
 */

#include <ncbiconf.h>


#if defined(NCBI_OS_MSWIN)

#    define NcbiSys_close        _close
#    define NcbiSys_dup          _dup
#    define NcbiSys_dup2         _dup2
#    define NcbiSys_fileno       _fileno
#    define NcbiSys_fstat        _fstat64
#    define NcbiSys_getpid       _getpid
#    define NcbiSys_lseek        _lseek
#    define NcbiSys_setmode      _setmode
#    define NcbiSys_swab         _swab
#    define NcbiSys_tzset        _tzset
#    define NcbiSys_umask        _umask
#    define NcbiSys_write        _write

#  if defined(_UNICODE)

#    define NcbiSys_chdir        _wchdir
#    define NcbiSys_chmod        _wchmod
#    define NcbiSys_creat        _wcreat
#    define NcbiSys_fopen        _wfopen
#    define NcbiSys_fdopen       _wfdopen
#    define NcbiSys_getcwd       _wgetcwd
#    define NcbiSys_getenv       _wgetenv
#    define NcbiSys_mkdir        _wmkdir
#    define NcbiSys_open         _wopen
#    define NcbiSys_putenv       _wputenv
#    define NcbiSys_remove       _wremove
#    define NcbiSys_rename       _wrename
#    define NcbiSys_rmdir        _wrmdir
#    define NcbiSys_spawnv       _wspawnv
#    define NcbiSys_spawnve      _wspawnve
#    define NcbiSys_spawnvp      _wspawnvp
#    define NcbiSys_spawnve      _wspawnve
#    define NcbiSys_spawnvpe     _wspawnvpe
#    define NcbiSys_stat         _wstat64
#    define NcbiSys_strcmp        wcscmp
#    define NcbiSys_strdup       _wcsdup
#    define NcbiSys_strerror     _wcserror
#    define NcbiSys_strerror_s   _wcserror_s
#    define NcbiSys_system       _wsystem
#    define NcbiSys_tempnam      _wtempnam
#    define NcbiSys_unlink       _wunlink

#  else // _UNICODE

#    define NcbiSys_chdir        _chdir
#    define NcbiSys_chmod        _chmod
#    define NcbiSys_creat        _creat
#    define NcbiSys_fopen         fopen
#    define NcbiSys_fdopen       _fdopen
#    define NcbiSys_getcwd       _getcwd
#    define NcbiSys_getenv        getenv
#    define NcbiSys_mkdir        _mkdir
#    define NcbiSys_open         _open
#    define NcbiSys_putenv       _putenv
#    define NcbiSys_remove        remove
#    define NcbiSys_rename        rename
#    define NcbiSys_rmdir        _rmdir
#    define NcbiSys_spawnv       _spawnv
#    define NcbiSys_spawnve      _spawnve
#    define NcbiSys_spawnvp      _spawnvp
#    define NcbiSys_spawnve      _spawnve
#    define NcbiSys_spawnvpe     _spawnvpe
#    define NcbiSys_stat         _stat64
#    define NcbiSys_strcmp        strcmp
#    define NcbiSys_strdup       _strdup
#    define NcbiSys_strerror      strerror
#    define NcbiSys_strerror_s    strerror_s
#    define NcbiSys_system        system
#    define NcbiSys_tempnam      _tempnam
#    define NcbiSys_unlink       _unlink

#  endif // _UNICODE

#else // NCBI_OS_MSWIN

#  define NcbiSys_chdir         chdir
#  define NcbiSys_chmod         chmod
#  define NcbiSys_close         close
#  define NcbiSys_creat         creat
#  define NcbiSys_dup           dup
#  define NcbiSys_dup2          dup2
#  define NcbiSys_fileno        fileno
#  define NcbiSys_fstat         fstat
#  define NcbiSys_fopen         fopen
#  define NcbiSys_fdopen        fdopen
#  define NcbiSys_getcwd        getcwd
#  define NcbiSys_getenv        getenv
#  define NcbiSys_getpid        getpid
#  define NcbiSys_lseek         lseek
#  define NcbiSys_mkdir         mkdir
#  define NcbiSys_open          open
#  define NcbiSys_putenv        putenv
#  define NcbiSys_remove        remove
#  define NcbiSys_rename        rename
#  define NcbiSys_rmdir         rmdir
#  define NcbiSys_setmode       setmode
#  define NcbiSys_spawnv        spawnv
#  define NcbiSys_spawnve       spawnve
#  define NcbiSys_spawnvp       spawnvp
#  define NcbiSys_spawnve       spawnve
#  define NcbiSys_spawnvpe      spawnvpe
#  define NcbiSys_stat          stat
#  define NcbiSys_strdup        strdup
#  define NcbiSys_strerror      strerror
#  define NcbiSys_swab          swab
#  define NcbiSys_tempnam       tempnam
#  define NcbiSys_tzset         tzset
#  define NcbiSys_umask         umask
#  define NcbiSys_unlink        unlink
#  define NcbiSys_write         write

#endif // NCBI_OS_MSWIN



/// C++ Toolkit Unicode is different, all strings are still char*

#if defined(NCBI_OS_MSWIN)

#  define NcbiSysChar_chdir        _chdir
#  define NcbiSysChar_creat        _creat
#  define NcbiSysChar_fopen         fopen
#  define NcbiSysChar_getcwd       _getcwd
#  define NcbiSysChar_getenv        getenv
#  define NcbiSysChar_mkdir        _mkdir
#  define NcbiSysChar_open         _open
#  define NcbiSysChar_putenv       _putenv
#  define NcbiSysChar_remove        remove
#  define NcbiSysChar_rename        rename
#  define NcbiSysChar_rmdir        _rmdir
#  define NcbiSysChar_spawnv       _spawnv
#  define NcbiSysChar_spawnve      _spawnve
#  define NcbiSysChar_spawnvp      _spawnvp
#  define NcbiSysChar_spawnve      _spawnve
#  define NcbiSysChar_spawnvpe     _spawnvpe
#  define NcbiSysChar_stat         _stat64
#  define NcbiSysChar_strcmp        strcmp
#  define NcbiSysChar_strdup       _strdup
#  define NcbiSysChar_strerror      strerror
#  define NcbiSysChar_strerror_s    strerror_s
#  define NcbiSysChar_system        system
#  define NcbiSysChar_tempnam      _tempnam
#  define NcbiSysChar_unlink       _unlink

#else // NCBI_OS_MSWIN

#  define NcbiSysChar_chdir         chdir
#  define NcbiSysChar_creat         creat
#  define NcbiSysChar_fstat         fstat
#  define NcbiSysChar_fopen         fopen
#  define NcbiSysChar_getcwd        getcwd
#  define NcbiSysChar_getenv        getenv
#  define NcbiSysChar_lseek         lseek
#  define NcbiSysChar_mkdir         mkdir
#  define NcbiSysChar_open          open
#  define NcbiSysChar_putenv        putenv
#  define NcbiSysChar_remove        remove
#  define NcbiSysChar_rename        rename
#  define NcbiSysChar_rmdir         rmdir
#  define NcbiSysChar_setmode       setmode
#  define NcbiSysChar_spawnv        spawnv
#  define NcbiSysChar_spawnve       spawnve
#  define NcbiSysChar_spawnvp       spawnvp
#  define NcbiSysChar_spawnve       spawnve
#  define NcbiSysChar_spawnvpe      spawnvpe
#  define NcbiSysChar_stat          stat
#  define NcbiSysChar_strdup        strdup
#  define NcbiSysChar_strerror      strerror
#  define NcbiSysChar_tempnam       tempnam
#  define NcbiSysChar_unlink        unlink

#endif // NCBI_OS_MSWIN





// TNcbiSys_(f)stat structures

#if defined(NCBI_OS_MSWIN)
typedef struct _fstat64 TNcbiSys_fstat;
typedef struct _stat64  TNcbiSys_stat;
#else
typedef struct stat     TNcbiSys_fstat;
typedef struct stat     TNcbiSys_stat;
#endif


#endif  /* CORELIB___NCBISYS__HPP */
