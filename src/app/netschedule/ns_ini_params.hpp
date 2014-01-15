#ifndef NETSCHEDULE_INI_PARAMS__HPP
#define NETSCHEDULE_INI_PARAMS__HPP

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
 * Authors:  Sergey Satskiy
 *
 * File Description: ini file default parameters and limits if so
 *
 */


#include "ns_precise_time.hpp"


BEGIN_NCBI_SCOPE


// Server section values
const unsigned int      default_max_connections = 100;
const unsigned int      max_connections_low_limit = 1;
const unsigned int      max_connections_high_limit = 1000;

const unsigned int      default_max_threads = 25;
const unsigned int      max_threads_low_limit = 1;
const unsigned int      max_threads_high_limit = 1000;

const unsigned int      default_init_threads = 10;
const unsigned int      init_threads_low_limit = 1;
const unsigned int      init_threads_high_limit = 1000;

const unsigned short    port_low_limit = 1;
const unsigned short    port_high_limit = 65535;

const bool              default_use_hostname = false;
const unsigned int      default_network_timeout = 10;
const bool              default_is_log = true;
const bool              default_log_batch_each_job = true;
const bool              default_log_notification_thread = false;
const bool              default_log_cleaning_thread = true;
const bool              default_log_execution_watcher_thread = true;
const bool              default_log_statistics_thread = true;

const unsigned int      default_del_batch_size = 100;
const unsigned int      default_markdel_batch_size = 200;
const unsigned int      default_scan_batch_size = 10000;
const double            default_purge_timeout = 0.1;

const unsigned int      default_stat_interval = 10;

const unsigned int      default_max_affinities = 10000;

const unsigned int      default_affinity_high_mark_percentage = 90;
const unsigned int      default_affinity_low_mark_percentage = 50;
const unsigned int      default_affinity_high_removal = 1000;
const unsigned int      default_affinity_low_removal = 100;
const unsigned int      default_affinity_dirt_percentage = 20;



// Queue section values
const CNSPreciseTime    default_timeout(3600, 0);
const CNSPreciseTime    default_notif_hifreq_interval(0, kNSecsPerSecond/10); // 0.1
const CNSPreciseTime    default_notif_hifreq_period(5, 0);
const unsigned int      default_notif_lofreq_mult = 50;
const CNSPreciseTime    default_notif_handicap(0, 0);
const unsigned int      default_dump_buffer_size = 100;
const unsigned int      max_dump_buffer_size = 10000;
const unsigned int      default_dump_client_buffer_size = 100;
const unsigned int      max_dump_client_buffer_size = 10000;
const unsigned int      default_dump_aff_buffer_size = 100;
const unsigned int      max_dump_aff_buffer_size = 10000;
const unsigned int      default_dump_group_buffer_size = 100;
const unsigned int      max_dump_group_buffer_size = 10000;
const CNSPreciseTime    default_run_timeout(3600, 0);
const CNSPreciseTime    default_run_timeout_precision(3, 0);
const unsigned int      default_failed_retries = 0;
const CNSPreciseTime    default_blacklist_time = CNSPreciseTime(2147483647, 0);
const CNSPreciseTime    default_wnode_timeout(40, 0);
const CNSPreciseTime    default_pending_timeout(604800, 0);
const CNSPreciseTime    default_max_pending_wait_timeout(0, 0);
const bool              default_scramble_job_keys = false;




END_NCBI_SCOPE

#endif
