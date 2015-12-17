#ifndef DBAPI_DRIVER_FTDS64_IMPL___RENAME_FTDS_REPLACEMENTS__H
#define DBAPI_DRIVER_FTDS64_IMPL___RENAME_FTDS_REPLACEMENTS__H

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
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Macros to rename FreeTDS TDS symbols -- to avoid their clashing
 *   with other versions'
 *
 */

#ifndef HAVE_ASPRINTF
#  define asprintf                      asprintf_ver64
#endif
#ifndef HAVE_ATOLL
#  define atoll                         atoll_ver64
#endif
#ifndef HAVE_POLL
#  define fakepoll                      fakepoll_ver64
#endif
#ifndef HAVE_READPASSPHRASE
#  define readpassphrase                readpassphrase_ver64
#endif
#ifndef HAVE_STRTOK_R
#  define strtok_r                      strtok_r_ver64
#endif
#ifndef HAVE_BASENAME
#  define tds_basename                  tds_basename_ver64
#endif
#ifndef HAVE_STRLCAT
#  define tds_strlcat                   tds_strlcat_ver64
#endif
#ifndef HAVE_STRLCPY
#  define tds_strlcpy                   tds_strlcpy_ver64
#endif
#ifndef HAVE_LIBICONV
#  define tds_sys_iconv                 tds_sys_iconv_ver64
#  define tds_sys_iconv_close           tds_sys_iconv_close_ver64
#  define tds_sys_iconv_open            tds_sys_iconv_open_ver64
#endif
#ifndef HAVE_VASPRINTF
#  define vasprintf                     vasprintf_ver64
#endif


#endif  /* DBAPI_DRIVER_FTDS64_IMPL___RENAME_FTDS_REPLACEMENTS__H */
