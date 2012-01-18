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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description: [server] section of the configuration
 *
 */

#include <ncbi_pch.hpp>
#include "ns_server_params.hpp"


USING_NCBI_SCOPE;

#define GetIntNoErr(name, dflt) \
    reg.GetInt(sname, name, dflt, 0, IRegistry::eReturn)
#define GetBoolNoErr(name, dflt) \
    reg.GetBool(sname, name, dflt, 0, IRegistry::eReturn)
#define GetDoubleNoErr(name, dflt) \
    reg.GetDouble(sname, name, dflt, 0, IRegistry::eReturn)


static unsigned int     default_max_connections = 100;
static unsigned int     default_max_threads = 25;
static unsigned int     default_init_threads = 10;
static unsigned short   default_port = 9100;
static unsigned int     default_network_timeout = 10;
static bool             default_use_hostname = false;
static bool             default_is_log = false;
static bool             default_log_batch_each_job = true;
static bool             default_log_notification_thread = false;
static bool             default_log_cleaning_thread = true;
static bool             default_log_execution_watcher_thread = true;
static bool             default_log_statistics_thread = true;
static unsigned int     default_del_batch_size = 100;
static unsigned int     default_scan_batch_size = 10000;
static double           default_purge_timeout = 0.1;
static unsigned int     default_max_affinities = 10000;
static unsigned int     default_affinity_high_mark_percentage = 90;
static unsigned int     default_affinity_low_mark_percentage = 50;
static unsigned int     default_affinity_high_removal = 1000;
static unsigned int     default_affinity_low_removal = 100;
static unsigned int     default_affinity_dirt_percentage = 20;


void SNS_Parameters::Read(const IRegistry& reg, const string& sname)
{
    reinit          = GetBoolNoErr("reinit", false);
    max_connections = GetIntNoErr("max_connections", default_max_connections);
    max_threads     = GetIntNoErr("max_threads", default_max_threads);

    init_threads = GetIntNoErr("init_threads", default_init_threads);
    if (init_threads > max_threads) {
        LOG_POST(Message << Warning <<
                 "INI file sets init_threads > max_threads. "
                 "Assume init_threads = max_threads(" << max_threads <<
                 ") instead of given " << init_threads);
        init_threads = max_threads;
    }

    port = (unsigned short) GetIntNoErr("port", default_port);

    use_hostname    = GetBoolNoErr("use_hostname", default_use_hostname);
    network_timeout = GetIntNoErr("network_timeout", default_network_timeout);
    if (network_timeout == 0) {
        LOG_POST(Message << Warning <<
            "INI file sets 0 sec. network timeout. Assume " <<
            default_network_timeout << " seconds.");
        network_timeout = default_network_timeout;
    }

    // Logging parameters
    is_log                       = GetBoolNoErr("log",
                                                default_is_log);
    log_batch_each_job           = GetBoolNoErr("log_batch_each_job",
                                                default_log_batch_each_job);
    log_notification_thread      = GetBoolNoErr("log_notification_thread",
                                                default_log_notification_thread);
    log_cleaning_thread          = GetBoolNoErr("log_cleaning_thread",
                                                default_log_cleaning_thread);
    log_execution_watcher_thread = GetBoolNoErr("log_execution_watcher_thread",
                                                default_log_execution_watcher_thread);
    log_statistics_thread        = GetBoolNoErr("log_statistics_thread",
                                                default_log_statistics_thread);

    // Job deleting parameters
    del_batch_size = GetIntNoErr("del_batch_size", default_del_batch_size);
    scan_batch_size = GetIntNoErr("scan_batch_size", default_scan_batch_size);
    purge_timeout = GetDoubleNoErr("purge_timeout", default_purge_timeout);

    if (del_batch_size == 0) {
        LOG_POST(Message << Warning <<
                 "INI file sets the del_batch_size = 0. "
                 "Assume " << default_del_batch_size << " instead." );
        del_batch_size = default_del_batch_size;
    }
    if (scan_batch_size == 0) {
        LOG_POST(Message << Warning <<
                 "INI file sets the scan_batch_size = 0. "
                 "Assume " << default_scan_batch_size << " instead." );
        scan_batch_size = default_scan_batch_size;
    }
    if (purge_timeout <= 0.0) {
        LOG_POST(Message << Warning <<
                 "INI file sets purge_timeout <= 0.0. "
                 "Assume " << default_purge_timeout << " instead." );
        purge_timeout = default_purge_timeout;
    }

    max_affinities = GetIntNoErr("max_affinities", default_max_affinities);
    if (max_affinities <= 0) {
        LOG_POST(Message << Warning <<
            "INI file sets the max number of preferred affinities <= 0."
            " Assume " << default_max_affinities << " instead.");
        max_affinities = default_max_affinities;
    }

    node_id            = reg.GetString(sname, "node_id", kEmptyStr);
    admin_hosts        = reg.GetString(sname, "admin_host", kEmptyStr);
    admin_client_names = reg.GetString(sname, "admin_client_name", kEmptyStr);

    // Affinity GC settings
    affinity_high_mark_percentage = GetIntNoErr("affinity_high_mark_percentage",
                                                default_affinity_high_mark_percentage);
    affinity_low_mark_percentage = GetIntNoErr("affinity_low_mark_percentage",
                                               default_affinity_low_mark_percentage);
    affinity_high_removal = GetIntNoErr("affinity_high_removal",
                                        default_affinity_high_removal);
    affinity_low_removal = GetIntNoErr("affinity_low_removal",
                                       default_affinity_low_removal);
    affinity_dirt_percentage = GetIntNoErr("affinity_dirt_percentage",
                                           default_affinity_dirt_percentage);
    CheckAffinityGarbageCollectorSettings();
    return;
}


void SNS_Parameters::CheckAffinityGarbageCollectorSettings(void)
{
    bool    well_formed = true;

    if (affinity_high_mark_percentage >= 100) {
        LOG_POST(Message << Warning <<
                 "INI file sets affinity_high_mark_percentage >= 100. "
                 "All the affinity garbage collector settings are reset to default.");
        well_formed = false;
    }

    if (affinity_low_mark_percentage >= affinity_high_mark_percentage) {
        LOG_POST(Message << Warning <<
                 "INI file sets affinity_low_mark_percentage >= affinity_high_mark_percentage. "
                 "All the affinity garbage collector settings are reset to default.");
        well_formed = false;
    }

    if (affinity_dirt_percentage >= affinity_low_mark_percentage) {
        LOG_POST(Message << Warning <<
                 "INI file sets affinity_dirt_percentage >= affinity_low_mark_percentage. "
                 "All the affinity garbage collector settings are reset to default.");
        well_formed = false;
    }

    if (well_formed == false) {
        affinity_high_mark_percentage = default_affinity_high_mark_percentage;
        affinity_low_mark_percentage = default_affinity_low_mark_percentage;
        affinity_high_removal = default_affinity_high_removal;
        affinity_low_removal = default_affinity_low_removal;
        affinity_dirt_percentage = default_affinity_dirt_percentage;
    }
}


static string s_NSParameters[] =
{
    "reinit",                           // 0
    "max_connections",                  // 1
    "max_threads",                      // 2
    "init_threads",                     // 3
    "port",                             // 4
    "use_hostname",                     // 5
    "network_timeout",                  // 6
    "log",                              // 7
    "admin_host",                       // 8
    "admin_client_name",                // 9
    "log_batch_each_job",               // 10
    "log_notification_thread",          // 11
    "log_cleaning_thread",              // 12
    "log_execution_watcher_thread",     // 13
    "log_statistics_thread",            // 14
    "del_batch_size",                   // 15
    "scan_batch_size",                  // 16
    "purge_timeout",                    // 17
    "max_affinities",                   // 18
    "node_id",                          // 19
    "affinity_high_mark_percentage",    // 20
    "affinity_low_mark_percentage",     // 21
    "affinity_high_removal",            // 22
    "affinity_low_removal",             // 23
    "affinity_dirt_percentage"          // 24
};
static unsigned s_NumNSParameters = sizeof(s_NSParameters) / sizeof(string);


unsigned SNS_Parameters::GetNumParams() const
{
    return s_NumNSParameters;
}


string SNS_Parameters::GetParamName(unsigned n) const
{
    if (n >= s_NumNSParameters)
        return kEmptyStr;
    return s_NSParameters[n];
}


string SNS_Parameters::GetParamValue(unsigned n) const
{
    switch (n) {
    case 0:  return NStr::BoolToString(reinit);
    case 1:  return NStr::UIntToString(max_connections);
    case 2:  return NStr::UIntToString(max_threads);
    case 3:  return NStr::UIntToString(init_threads);
    case 4:  return NStr::UIntToString(port);
    case 5:  return NStr::BoolToString(use_hostname);
    case 6:  return NStr::UIntToString(network_timeout);
    case 7:  return NStr::BoolToString(is_log);
    case 8:  return admin_hosts;
    case 9:  return admin_client_names;
    case 10: return NStr::BoolToString(log_batch_each_job);
    case 11: return NStr::BoolToString(log_notification_thread);
    case 12: return NStr::BoolToString(log_cleaning_thread);
    case 13: return NStr::BoolToString(log_execution_watcher_thread);
    case 14: return NStr::BoolToString(log_statistics_thread);
    case 15: return NStr::UIntToString(del_batch_size);
    case 16: return NStr::UIntToString(scan_batch_size);
    case 17: return NStr::DoubleToString(purge_timeout);
    case 18: return NStr::UIntToString(max_affinities);
    case 19: return node_id;
    case 20: return NStr::UIntToString(affinity_high_mark_percentage);
    case 21: return NStr::UIntToString(affinity_low_mark_percentage);
    case 22: return NStr::UIntToString(affinity_high_removal);
    case 23: return NStr::UIntToString(affinity_low_removal);
    case 24: return NStr::UIntToString(affinity_dirt_percentage);
    default: return kEmptyStr;
    }
}

