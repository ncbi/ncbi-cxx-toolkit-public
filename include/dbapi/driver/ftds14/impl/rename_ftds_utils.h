#ifndef DBAPI_DRIVER_FTDS14_IMPL___RENAME_FTDS_UTILS__H
#define DBAPI_DRIVER_FTDS14_IMPL___RENAME_FTDS_UTILS__H

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
 * Author:  Aaron Ucko
 *
 * File Description:
 *   Macros to rename FreeTDS TDS symbols -- to avoid their clashing
 *   with other versions'
 *
 */

#define MD4Final                 FTDS14_MD4Final
#define MD4Init                  FTDS14_MD4Init
#define MD4Update                FTDS14_MD4Update
#define MD5Final                 FTDS14_MD5Final
#define MD5Init                  FTDS14_MD5Init
#define MD5Update                FTDS14_MD5Update
#if ENABLE_EXTRA_CHECKS
#  define dlist_ring_check       dlist_ring_check_ver14
#endif
#define hmac_md5                 hmac_md5_ver14
#define smp_add                  smp_add_ver14
#define smp_cmp                  smp_cmp_ver14
#define smp_from_int             smp_from_int_ver14
#define smp_from_string          smp_from_string_ver14
#define smp_is_negative          smp_is_negative_ver14
#define smp_is_zero              smp_is_zero_ver14
#define smp_negate               smp_negate_ver14
#define smp_not                  smp_not_ver14
#define smp_one                  smp_one_ver14
#define smp_sub                  smp_sub_ver14
#define smp_to_double            smp_to_double_ver14
#define smp_to_string            smp_to_string_ver14
#define smp_zero                 smp_zero_ver14
#define tds_des_ecb_encrypt      tds_des_ecb_encrypt_ver14
#define tds_des_encrypt          tds_des_encrypt_ver14
#define tds_des_set_key          tds_des_set_key_ver14
#define tds_des_set_odd_parity   tds_des_set_odd_parity_ver14
#define tds_dstr_alloc           tds_dstr_alloc_ver14
#define tds_dstr_copy            tds_dstr_copy_ver14
#define tds_dstr_copyn           tds_dstr_copyn_ver14
#define tds_dstr_dup             tds_dstr_dup_ver14
#define tds_dstr_free            tds_dstr_free_ver14
#define tds_dstr_set             tds_dstr_set_ver14
#define tds_dstr_setlen          tds_dstr_setlen_ver14
#define tds_dstr_zero            tds_dstr_zero_ver14
#define tds_get_homedir          tds_get_homedir_ver14
#define tds_getpassarg           tds_getpassarg_ver14
#define tds_getservice           tds_getservice_ver14
#define tds_localtime_r          tds_localtime_r_ver14
#ifdef _WIN32
#  define tds_raw_cond_destroy   tds_raw_cond_destroy_ver14
#  define tds_raw_cond_signal    tds_raw_cond_signal_ver14
#endif
#define tds_raw_cond_init        tds_raw_cond_init_ver14
#if (defined(_THREAD_SAFE) && defined(TDS_HAVE_PTHREAD_MUTEX)) \
    || defined(_WIN32)
#  define tds_raw_cond_timedwait tds_raw_cond_timedwait_ver14
#endif
#ifdef _WIN32
#  define tds_raw_mutex_trylock  tds_raw_mutex_trylock_ver14
#endif
#define tds_sleep_ms             tds_sleep_ms_ver14
#define tds_sleep_s              tds_sleep_s_ver14
#define tds_socket_set_nosigpipe tds_socket_set_nosigpipe_ver14
#define tds_str_empty            tds_str_empty_ver14
#define tds_timestamp_str        tds_timestamp_str_ver14
#ifdef _WIN32
#  define tds_win_mutex_lock     tds_win_mutex_lock_ver14
#endif
#define utf8_table               utf8_table_ver14


#endif  /* DBAPI_DRIVER_FTDS14_IMPL___RENAME_FTDS_UTILS__H */
