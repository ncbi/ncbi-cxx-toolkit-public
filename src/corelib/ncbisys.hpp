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


#if defined(NCBI_OS_MSWIN)


#  if defined(_UNICODE)

#    define NcbiSys_chdir        _wchdir
#    define NcbiSys_chmod        _wchmod
#    define NcbiSys_creat        _wcreat
#    define NcbiSys_fopen        _wfopen
#    define NcbiSys_getcwd       _wgetcwd
#    define NcbiSys_getenv       _wgetenv
#    define NcbiSys_mkdir        _wmkdir
#    define NcbiSys_open         _wopen
#    define NcbiSys_putenv       _wputenv
#    define NcbiSys_remove       _wremove
#    define NcbiSys_rename       _wrename
#    define NcbiSys_rmdir        _wrmdir
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
#    define NcbiSys_chmod         chmod
#    define NcbiSys_creat        _creat
#    define NcbiSys_fopen         fopen
#    define NcbiSys_getcwd        getcwd
#    define NcbiSys_getenv        getenv
#    define NcbiSys_mkdir         mkdir
#    define NcbiSys_open         _open
#    define NcbiSys_putenv       _putenv
#    define NcbiSys_remove        remove
#    define NcbiSys_rename        rename
#    define NcbiSys_rmdir         rmdir
#    define NcbiSys_stat         _stat64
#    define NcbiSys_strcmp        strcmp
#    define NcbiSys_strdup        strdup
#    define NcbiSys_strerror      strerror
#    define NcbiSys_strerror_s    strerror_s
#    define NcbiSys_system        system
#    define NcbiSys_tempnam       tempnam
#    define NcbiSys_unlink        unlink

#  endif // _UNICODE

#else // NCBI_OS_MSWIN

#  define NcbiSys_chdir         chdir
#  define NcbiSys_chmod         chmod
#  define NcbiSys_creat         creat
#  define NcbiSys_fopen         fopen
#  define NcbiSys_getcwd        getcwd
#  define NcbiSys_getenv        getenv
#  define NcbiSys_mkdir         mkdir
#  define NcbiSys_open          open
#  define NcbiSys_putenv        putenv
#  define NcbiSys_remove        remove
#  define NcbiSys_rename        rename
#  define NcbiSys_rmdir         rmdir
#  define NcbiSys_stat          stat
#  define NcbiSys_strdup        strdup
#  define NcbiSys_strerror      strerror
#  define NcbiSys_tempnam       tempnam
#  define NcbiSys_unlink        unlink

#endif // NCBI_OS_MSWIN


#endif  /* CORELIB___NCBISYS__HPP */
