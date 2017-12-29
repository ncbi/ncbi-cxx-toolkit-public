#ifndef DBAPI_DRIVER_FTDS100_IMPL___RENAME_FTDS_REPLACEMENTS__H
#define DBAPI_DRIVER_FTDS100_IMPL___RENAME_FTDS_REPLACEMENTS__H

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

#ifndef HAVE_GETOPT
#  define optarg                        optarg_ver100
#  define opterr                        opterr_ver100
#  define optind                        optind_ver100
#  define optopt                        optopt_ver100
#  define optreset                      optreset_ver100
#  define tds_getopt                    tds_getopt_ver100
#endif
#ifndef HAVE_ASPRINTF
#  define tds_asprintf                  tds_asprintf_ver100
#endif
#ifndef HAVE_BASENAME
#  define tds_basename                  tds_basename_ver100
#endif
#ifndef HAVE_DAEMON
#  define tds_daemon                    tds_daemon_ver100
#endif
#define tds_getpassarg                  tds_getpassarg_ver100
#ifndef HAVE_GETTIMEOFDAY
#  define tds_gettimeofday              tds_gettimeofday_ver100
#endif
#ifndef HAVE_POLL
#  define tds_poll                      tds_poll_ver100
#endif
#ifdef _WIN32
#  define tds_raw_cond_destroy          tds_raw_cond_destroy_ver100
#  define tds_raw_cond_signal           tds_raw_cond_signal_ver100
#endif
#define tds_raw_cond_init               tds_raw_cond_init_ver100
#if (defined(_THREAD_SAFE) && defined(TDS_HAVE_PTHREAD_MUTEX)) \
    || defined(_WIN32)
#  define tds_raw_cond_timedwait        tds_raw_cond_timedwait_ver100
#endif
#ifndef HAVE_READPASSPHRASE
#  define tds_readpassphrase            tds_readpassphrase_ver100
#endif
#ifndef HAVE_SOCKETPAIR
#  define tds_socketpair                tds_socketpair_ver100
#endif
#ifndef HAVE_STRLCAT
#  define tds_strlcat                   tds_strlcat_ver100
#endif
#ifndef HAVE_STRLCPY
#  define tds_strlcpy                   tds_strlcpy_ver100
#endif
#ifndef HAVE_STRTOK_R
#  define tds_strtok_r                  tds_strtok_r_ver100
#endif
#ifndef HAVE_LIBICONV
#  define tds_sys_iconv                 tds_sys_iconv_ver100
#  define tds_sys_iconv_close           tds_sys_iconv_close_ver100
#  define tds_sys_iconv_open            tds_sys_iconv_open_ver100
#endif
#ifndef HAVE_VASPRINTF
#  define tds_vasprintf                 tds_vasprintf_ver100
#endif
#ifdef _WIN32
#  define tds_win_mutex_lock            tds_win_mutex_lock_ver100
#endif


#endif  /* DBAPI_DRIVER_FTDS100_IMPL___RENAME_FTDS_REPLACEMENTS__H */
