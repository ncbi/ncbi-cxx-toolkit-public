#ifndef DBAPI_DRIVER_FTDS14_IMPL___RENAME_FTDS_REPLACEMENTS__H
#define DBAPI_DRIVER_FTDS14_IMPL___RENAME_FTDS_REPLACEMENTS__H

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
 * Authors:  Denis Vakatov, Aaron Ucko
 *
 * File Description:
 *   Macros to rename FreeTDS TDS symbols -- to avoid their clashing
 *   with other versions'
 *
 */

#ifndef HAVE_GETADDRINFO
#  define tds_getaddrinfo               tds_getaddrinfo_ver14
#endif
#ifndef HAVE_GETTIMEOFDAY
#  define tds_gettimeofday              tds_gettimeofday_ver14
#endif
#ifndef HAVE_POLL
#  define tds_poll                      tds_poll_ver14
#endif
#ifndef HAVE_READPASSPHRASE
#  define tds_readpassphrase            tds_readpassphrase_ver14
#endif
#ifndef HAVE_SOCKETPAIR
#  define tds_socketpair                tds_socketpair_ver14
#endif
#ifndef HAVE_STRLCAT
#  define tds_strlcat                   tds_strlcat_ver14
#endif
#ifndef HAVE_STRLCPY
#  define tds_strlcpy                   tds_strlcpy_ver14
#endif
#ifndef HAVE_LIBICONV
#  define tds_sys_iconv                 tds_sys_iconv_ver14
#  define tds_sys_iconv_close           tds_sys_iconv_close_ver14
#  define tds_sys_iconv_open            tds_sys_iconv_open_ver14
#endif


#endif  /* DBAPI_DRIVER_FTDS14_IMPL___RENAME_FTDS_REPLACEMENTS__H */
