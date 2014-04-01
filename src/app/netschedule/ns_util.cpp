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
 * Authors:  Victor Joukov
 *
 * File Description: Utility functions for NetSchedule
 *
 */

#include <ncbi_pch.hpp>

#include "ns_util.hpp"
#include "ns_queue.hpp"
#include "ns_ini_params.hpp"
#include <util/bitset/bmalgo.h>
#include <connect/ncbi_socket.hpp>


BEGIN_NCBI_SCOPE

const string    g_LogPrefix = "Validating config file: ";


static bool NS_ValidateServerSection(const IRegistry &  reg);
static bool NS_ValidateQueuesAndClasses(const IRegistry &  reg,
                                        list<string> &  queues);
static bool NS_ValidateClasses(const IRegistry &  reg,
                               list<string> &  qclasses);
static bool NS_ValidateQueues(const IRegistry &  reg,
                              const list<string> &  qclasses,
                              list<string> &  queues);
static bool NS_ValidateQueueParams(const IRegistry &  reg,
                                   const string &  section_name,
                                   const list<string> *  qclasses);
static bool NS_ValidateServiceToQueueSection(const IRegistry &  reg,
                                             const list<string> &  queues);
static bool NS_ValidateBool(const IRegistry &  reg,
                            const string &  section, const string &  entry);
static bool NS_ValidateInt(const IRegistry &  reg,
                           const string &  section, const string &  entry);
static bool NS_ValidateDouble(const IRegistry &  reg,
                              const string &  section, const string &  entry);
static bool NS_ValidateString(const IRegistry &  reg,
                              const string &  section, const string &  entry);
static string NS_RegValName(const string &  section, const string &  entry);
static string NS_OutOfLimitMessage(const string &  section,
                                   const string &  entry,
                                   unsigned int  low_limit,
                                   unsigned int  high_limit);


bool NS_ValidateConfigFile(const IRegistry &  reg)
{
    list<string>    queues;

    bool    server_well_formed = NS_ValidateServerSection(reg);
    bool    classes_well_formed = NS_ValidateQueuesAndClasses(reg, queues);
    bool    queues_well_formed = NS_ValidateServiceToQueueSection(reg, queues);
    return server_well_formed && classes_well_formed && queues_well_formed;
}


// Returns true if the config file is well formed
bool NS_ValidateServerSection(const IRegistry &  reg)
{
    const string    section = "server";
    bool            well_formed = true;

    // port is a unique value in this section. NS must not start
    // if there is a problem with port.
    bool    port_ok = NS_ValidateInt(reg, section, "port");
    if (port_ok) {
        unsigned int    port_val = reg.GetInt(section, "port", 0);
        if (port_val < port_low_limit || port_val > port_high_limit)
            NCBI_THROW(CNetScheduleException, eInvalidParameter,
                       "Invalid " + NS_RegValName(section, "port") +
                       " value. Allowed range: " +
                       NStr::NumericToString(port_low_limit) +
                       " to " +
                       NStr::NumericToString(port_high_limit));
    }

    well_formed = well_formed && NS_ValidateBool(reg, section, "reinit");

    bool    max_conn_ok = NS_ValidateInt(reg, section, "max_connections");
    well_formed = well_formed && max_conn_ok;
    if (max_conn_ok) {
        unsigned int    val = reg.GetInt(section, "max_connections",
                                         default_max_connections);
        if (val < max_connections_low_limit ||
            val > max_connections_high_limit) {
            well_formed = false;
            LOG_POST(Warning <<
                     NS_OutOfLimitMessage(section, "max_connections",
                                          max_connections_low_limit,
                                          max_connections_high_limit));
        }
    }

    bool            max_threads_ok = NS_ValidateInt(reg, section,
                                                    "max_threads");
    unsigned int    max_threads_val = default_max_threads;
    well_formed = well_formed && max_threads_ok;
    if (max_threads_ok) {
        max_threads_val = reg.GetInt(section, "max_threads",
                                     default_max_threads);
        if (max_threads_val < max_threads_low_limit ||
            max_threads_val > max_threads_high_limit) {
            well_formed = false;
            max_threads_ok = false;
            LOG_POST(Warning <<
                     NS_OutOfLimitMessage(section, "max_threads",
                                          max_threads_low_limit,
                                          max_threads_high_limit));
        }
    }

    bool            init_threads_ok = NS_ValidateInt(reg, section,
                                                     "init_threads");
    unsigned int    init_threads_val = default_init_threads;
    well_formed = well_formed && init_threads_ok;
    if (init_threads_ok) {
        init_threads_val = reg.GetInt(section, "init_threads",
                                      default_init_threads);
        if (init_threads_val < init_threads_low_limit ||
            init_threads_val > init_threads_high_limit) {
            well_formed = false;
            init_threads_ok = false;
            LOG_POST(Warning <<
                     NS_OutOfLimitMessage(section, "init_threads",
                                          init_threads_low_limit,
                                          init_threads_high_limit));
        }
    }

    if (max_threads_ok && init_threads_ok) {
        if (init_threads_val > max_threads_val) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " values "
                             << NS_RegValName(section, "max_threads") << " and "
                             << NS_RegValName(section, "init_threads")
                             << " break the mandatory condition "
                                "init_threads <= max_threads");
        }
    }


    well_formed = well_formed && NS_ValidateBool(reg, section, "use_hostname");

    bool    network_timeout_ok = NS_ValidateInt(reg, section,
                                                "network_timeout");
    well_formed = well_formed && network_timeout_ok;
    if (network_timeout_ok) {
        unsigned int    network_timeout_val =
                                    reg.GetInt(section, "network_timeout",
                                               default_network_timeout);
        if (network_timeout_val <= 0) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "network_timeout")
                             << " must be > 0");
        }
    }


    well_formed = well_formed &&
                  NS_ValidateBool(reg, section, "log");
    well_formed = well_formed &&
                  NS_ValidateBool(reg, section, "log_batch_each_job");
    well_formed = well_formed &&
                  NS_ValidateBool(reg, section, "log_notification_thread");
    well_formed = well_formed &&
                  NS_ValidateBool(reg, section, "log_cleaning_thread");
    well_formed = well_formed &&
                  NS_ValidateBool(reg, section, "log_execution_watcher_thread");
    well_formed = well_formed &&
                  NS_ValidateBool(reg, section, "log_statistics_thread");


    bool    del_batch_size_ok = NS_ValidateInt(reg, section,
                                               "del_batch_size");
    well_formed = well_formed && del_batch_size_ok;
    unsigned int    del_batch_size_val = default_del_batch_size;
    if (del_batch_size_ok) {
        del_batch_size_val = reg.GetInt(section, "del_batch_size",
                                        default_del_batch_size);
        if (del_batch_size_val <= 0) {
            well_formed = false;
            del_batch_size_ok = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "del_batch_size")
                             << " must be > 0");
        }
    }

    bool    markdel_batch_size_ok = NS_ValidateInt(reg, section,
                                                   "markdel_batch_size");
    well_formed = well_formed && markdel_batch_size_ok;
    unsigned int    markdel_batch_size_val = default_markdel_batch_size;
    if (markdel_batch_size_ok) {
        markdel_batch_size_val = reg.GetInt(section, "markdel_batch_size",
                                            default_markdel_batch_size);
        if (markdel_batch_size_val <= 0) {
            well_formed = false;
            markdel_batch_size_ok = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "markdel_batch_size")
                             << " must be > 0");
        }
    }

    bool    scan_batch_size_ok = NS_ValidateInt(reg, section,
                                                "scan_batch_size");
    well_formed = well_formed && scan_batch_size_ok;
    unsigned int    scan_batch_size_val = default_scan_batch_size;
    if (scan_batch_size_ok) {
        scan_batch_size_val = reg.GetInt(section, "scan_batch_size",
                                         default_scan_batch_size);
        if (scan_batch_size_val <= 0) {
            well_formed = false;
            scan_batch_size_ok = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "scan_batch_size")
                             << " must be > 0");
        }
    }

    bool    purge_timeout_ok = NS_ValidateDouble(reg, section,
                                                 "purge_timeout");
    well_formed = well_formed && purge_timeout_ok;
    double  purge_timeout_val = default_purge_timeout;
    if (purge_timeout_ok) {
        purge_timeout_val = reg.GetDouble(section, "purge_timeout",
                                          default_purge_timeout);
        if (purge_timeout_val <= 0.0) {
            well_formed = false;
            purge_timeout_ok = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "purge_timeout")
                             << " must be > 0");
        }
    }

    if (scan_batch_size_ok && markdel_batch_size_ok) {
        if (scan_batch_size_val < markdel_batch_size_val) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " values "
                             << NS_RegValName(section, "scan_batch_size") << " and "
                             << NS_RegValName(section, "markdel_batch_size")
                             << " break the mandatory condition "
                                "markdel_batch_size <= scan_batch_size");
        }
    }

    if (markdel_batch_size_ok && del_batch_size_ok) {
        if (markdel_batch_size_val < del_batch_size_val) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " values "
                             << NS_RegValName(section, "markdel_batch_size") << " and "
                             << NS_RegValName(section, "del_batch_size")
                             << " break the mandatory condition "
                                "del_batch_size <= markdel_batch_size");
        }
    }


    bool    stat_interval_ok = NS_ValidateInt(reg, section,
                                              "stat_interval");
    well_formed = well_formed && stat_interval_ok;
    if (stat_interval_ok) {
        unsigned int    stat_interval_val = reg.GetInt(section, "stat_interval",
                                                       default_stat_interval);
        if (stat_interval_val <= 0) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "stat_interval")
                             << " must be > 0");
        }
    }


    bool    max_affinities_ok = NS_ValidateInt(reg, section,
                                               "max_affinities");
    well_formed = well_formed && max_affinities_ok;
    if (max_affinities_ok) {
        unsigned int    max_affinities_val = reg.GetInt(section, "max_affinities",
                                                        default_max_affinities);
        if (max_affinities_val <= 0) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "max_affinities")
                             << " must be > 0");
        }
    }

    bool    max_client_data_ok = NS_ValidateInt(reg, section,
                                                "max_client_data");
    well_formed = well_formed && max_client_data_ok;
    if (max_client_data_ok) {
        unsigned int    max_client_data_val = reg.GetInt(section, "max_client_data",
                                                         default_max_client_data);
        if (max_client_data_val <= 0) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "max_client_data")
                             << " must be > 0");
        }
    }

    bool    affinity_high_mark_percentage_ok =
                    NS_ValidateInt(reg, section,
                                   "affinity_high_mark_percentage");
    well_formed = well_formed && affinity_high_mark_percentage_ok;
    unsigned int    affinity_high_mark_percentage_val =
                                default_affinity_high_mark_percentage;
    if (affinity_high_mark_percentage_ok) {
        affinity_high_mark_percentage_val = reg.GetInt(section,
                         "affinity_high_mark_percentage",
                         default_affinity_high_mark_percentage);
        if (affinity_high_mark_percentage_val <= 0 ||
            affinity_high_mark_percentage_val >= 100) {
            well_formed = false;
            affinity_high_mark_percentage_ok = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section,
                                              "affinity_high_mark_percentage")
                             << " must be between 0 and 100");
        }
    }

    bool    affinity_low_mark_percentage_ok =
                    NS_ValidateInt(reg, section,
                                   "affinity_low_mark_percentage");
    well_formed = well_formed && affinity_low_mark_percentage_ok;
    unsigned int    affinity_low_mark_percentage_val =
                                default_affinity_low_mark_percentage;
    if (affinity_low_mark_percentage_ok) {
        affinity_low_mark_percentage_val = reg.GetInt(section,
                        "affinity_low_mark_percentage",
                        default_affinity_low_mark_percentage);
        if (affinity_low_mark_percentage_val <= 0 ||
            affinity_low_mark_percentage_val >= 100) {
            well_formed = false;
            affinity_low_mark_percentage_ok = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section,
                                              "affinity_low_mark_percentage")
                             << " must be between 0 and 100");
        }
    }

    bool    affinity_high_removal_ok =
                    NS_ValidateInt(reg, section,
                                   "affinity_high_removal");
    well_formed = well_formed && affinity_high_removal_ok;
    unsigned int    affinity_high_removal_val =
                                default_affinity_high_removal;
    if (affinity_high_removal_ok) {
        affinity_high_removal_val = reg.GetInt(section,
                        "affinity_high_removal",
                        default_affinity_high_removal);
        if (affinity_high_removal_val <= 0) {
            well_formed = false;
            affinity_high_removal_ok = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section,
                                              "affinity_high_removal")
                             << " must be > 0");
        }
    }

    bool    affinity_low_removal_ok =
                    NS_ValidateInt(reg, section,
                                   "affinity_low_removal");
    well_formed = well_formed && affinity_low_removal_ok;
    unsigned int    affinity_low_removal_val =
                                default_affinity_low_removal;
    if (affinity_low_removal_ok) {
        affinity_low_removal_val = reg.GetInt(section,
                        "affinity_low_removal",
                        default_affinity_low_removal);
        if (affinity_low_removal_val <= 0) {
            well_formed = false;
            affinity_low_removal_ok = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section,
                                              "affinity_low_removal")
                             << " must be > 0");
        }
    }

    bool    affinity_dirt_percentage_ok =
                    NS_ValidateInt(reg, section,
                                   "affinity_dirt_percentage");
    well_formed = well_formed && affinity_dirt_percentage_ok;
    unsigned int    affinity_dirt_percentage_val =
                                default_affinity_dirt_percentage;
    if (affinity_dirt_percentage_ok) {
        affinity_dirt_percentage_val = reg.GetInt(section,
                        "affinity_dirt_percentage",
                        default_affinity_dirt_percentage);
        if (affinity_dirt_percentage_val <= 0 ||
            affinity_dirt_percentage_val >= 100) {
            well_formed = false;
            affinity_dirt_percentage_ok = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section,
                                              "affinity_dirt_percentage")
                             << " must be between 0 and 100");
        }
    }

    if (affinity_low_mark_percentage_ok &&
        affinity_high_mark_percentage_ok) {
        if (affinity_low_mark_percentage_val >=
            affinity_high_mark_percentage_val) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " values "
                 << NS_RegValName(section, "affinity_low_mark_percentage")
                 << " and "
                 << NS_RegValName(section, "affinity_high_mark_percentage")
                 << " break the mandatory condition "
                    "affinity_low_mark_percentage < "
                    "affinity_high_mark_percentage");
        }
    }

    if (affinity_low_mark_percentage_ok &&
        affinity_dirt_percentage_ok) {
        if (affinity_dirt_percentage_val >=
            affinity_low_mark_percentage_val) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " values "
                 << NS_RegValName(section, "affinity_low_mark_percentage")
                 << " and "
                 << NS_RegValName(section, "affinity_dirt_percentage")
                 << " break the mandatory condition "
                    "affinity_dirt_percentage < affinity_low_mark_percentage");
        }
    }

    if (affinity_high_removal_ok && affinity_low_removal_ok) {
        if (affinity_high_removal_val < affinity_low_removal_val) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " values "
                << NS_RegValName(section, "affinity_high_removal")
                << " and "
                << NS_RegValName(section, "affinity_low_removal")
                << " break the mandatory condition "
                   "affinity_low_removal <= affinity_high_removal");
        }
    }

    well_formed = well_formed &&
                  NS_ValidateString(reg, section, "admin_host");
    well_formed = well_formed &&
                  NS_ValidateString(reg, section, "admin_client_name");

    return well_formed;
}


// Returns true if the config file is well formed
// the queues parameter is filled with what queues will be accepted
bool NS_ValidateQueuesAndClasses(const IRegistry &  reg,
                                 list<string> & queues)
{
    bool            well_formed = true;
    list<string>    qclasses;

    well_formed = well_formed && NS_ValidateClasses(reg, qclasses);
    well_formed = well_formed && NS_ValidateQueues(reg, qclasses, queues);

    return well_formed;
}


bool NS_ValidateClasses(const IRegistry &  reg,
                        list<string> &  qclasses)
{
    bool            well_formed = true;
    list<string>    sections;

    reg.EnumerateSections(&sections);
    for (list<string>::const_iterator  k = sections.begin();
         k != sections.end(); ++k) {
        string      queue_class;
        string      prefix;
        string      section_name = *k;

        NStr::SplitInTwo(section_name, "_", prefix, queue_class);
        if (NStr::CompareNocase(prefix, "qclass") != 0)
           continue;

        if (queue_class.empty()) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << "queue class section "
                             << section_name << " name is malformed. "
                                "Class name is expected.");
            continue;
        }

        well_formed = well_formed && NS_ValidateQueueParams(reg, section_name,
                                                            NULL);
        qclasses.push_back(queue_class);
    }

    return well_formed;
}


// Returns true if the config file is well formed
bool NS_ValidateQueues(const IRegistry &  reg,
                       const list<string> &  qclasses,
                       list<string> &  queues)
{
    bool            well_formed = true;
    list<string>    sections;

    reg.EnumerateSections(&sections);
    for (list<string>::const_iterator  k = sections.begin();
         k != sections.end(); ++k) {
        string      queue_name;
        string      prefix;
        string      section_name = *k;

        NStr::SplitInTwo(section_name, "_", prefix, queue_name);
        if (NStr::CompareNocase(prefix, "queue") != 0)
           continue;

        if (queue_name.empty()) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << "queue section "
                             << section_name << " name is malformed. "
                                "Queue name is expected.");
            continue;
        }

        try {
            // Exception is thrown only if a queue refers to an unknown
            // queue class
            well_formed = well_formed && NS_ValidateQueueParams(reg,
                                                                section_name,
                                                                &qclasses);
            queues.push_back(queue_name);
        }
        catch (...) {
            well_formed = false;
        }
    }

    return well_formed;
}


// Returns true if the config file is well formed
bool NS_ValidateQueueParams(const IRegistry &  reg,
                            const string &  section,
                            const list<string> *  classes)
{
    bool    well_formed = true;
    bool    is_class = (classes == NULL);

    well_formed = well_formed && NS_ValidateString(reg, section, "description");

    bool    timeout_ok = NS_ValidateDouble(reg, section, "timeout");
    well_formed = well_formed && timeout_ok;
    if (timeout_ok) {
        double  value = reg.GetDouble(section, "timeout",
                                      double(default_timeout));
        if (value <= 0.0) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "timeout")
                             << " must be > 0");
        }
    }

    bool    notif_hifreq_interval_ok = NS_ValidateDouble(reg, section,
                                                    "notif_hifreq_interval");
    well_formed = well_formed && notif_hifreq_interval_ok;
    if (notif_hifreq_interval_ok) {
        double  value = reg.GetDouble(section, "notif_hifreq_interval",
                                      double(default_notif_hifreq_interval));
        value = (int(value * 10)) / 10.0;
        if (value <= 0.0) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "notif_hifreq_interval")
                             << " must be > 0");
        }
    }

    bool    notif_hifreq_period_ok = NS_ValidateDouble(reg, section,
                                                    "notif_hifreq_period");
    well_formed = well_formed && notif_hifreq_period_ok;
    if (notif_hifreq_period_ok) {
        double  value = reg.GetDouble(section, "notif_hifreq_period",
                                      double(default_notif_hifreq_period));
        if (value <= 0.0) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "notif_hifreq_period")
                             << " must be > 0");
        }
    }

    bool    notif_lofreq_mult_ok = NS_ValidateInt(reg, section,
                                                    "notif_lofreq_mult");
    well_formed = well_formed && notif_lofreq_mult_ok;
    if (notif_lofreq_mult_ok) {
        int    value = reg.GetInt(section, "notif_lofreq_mult",
                                  default_notif_lofreq_mult);
        if (value <= 0) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "notif_lofreq_mult")
                             << " must be > 0");
        }
    }

    bool    notif_handicap_ok = NS_ValidateDouble(reg, section,
                                                  "notif_handicap");
    well_formed = well_formed && notif_handicap_ok;
    if (notif_handicap_ok) {
        double  value = reg.GetDouble(section, "notif_handicap",
                                      default_notif_handicap);
        if (value < 0.0) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "notif_handicap")
                             << " must be > 0.0");
        }
    }

    bool    dump_buffer_size_ok = NS_ValidateInt(reg, section,
                                                 "dump_buffer_size");
    well_formed = well_formed && dump_buffer_size_ok;
    if (dump_buffer_size_ok) {
        int    value = reg.GetInt(section, "dump_buffer_size",
                                  default_dump_buffer_size);
        if (value < int(default_dump_buffer_size)) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "dump_buffer_size")
                             << " must not be less than "
                             << default_dump_buffer_size);
        }
        else if (value > int(max_dump_buffer_size)) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "dump_buffer_size")
                             << " must not be larger than "
                             << max_dump_buffer_size);
        }
    }

    bool    dump_client_buffer_size_ok = NS_ValidateInt(reg, section,
                                                    "dump_client_buffer_size");
    well_formed = well_formed && dump_client_buffer_size_ok;
    if (dump_client_buffer_size_ok) {
        int    value = reg.GetInt(section, "dump_client_buffer_size",
                                  default_dump_client_buffer_size);
        if (value < int(default_dump_client_buffer_size)) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "dump_client_buffer_size")
                             << " must not be less than "
                             << default_dump_client_buffer_size);
        }
        else if (value > int(max_dump_client_buffer_size)) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "dump_client_buffer_size")
                             << " must not be larger than "
                             << max_dump_client_buffer_size);
        }
    }

    bool    dump_aff_buffer_size_ok = NS_ValidateInt(reg, section,
                                                     "dump_aff_buffer_size");
    well_formed = well_formed && dump_aff_buffer_size_ok;
    if (dump_aff_buffer_size_ok) {
        int    value = reg.GetInt(section, "dump_aff_buffer_size",
                                  default_dump_aff_buffer_size);
        if (value < int(default_dump_aff_buffer_size)) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "dump_aff_buffer_size")
                             << " must not be less than "
                             << default_dump_aff_buffer_size);
        }
        else if (value > int(max_dump_aff_buffer_size)) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "dump_aff_buffer_size")
                             << " must not be larger than "
                             << max_dump_aff_buffer_size);
        }
    }

    bool    dump_group_buffer_size_ok = NS_ValidateInt(reg, section,
                                                    "dump_group_buffer_size");
    well_formed = well_formed && dump_group_buffer_size_ok;
    if (dump_group_buffer_size_ok) {
        int    value = reg.GetInt(section, "dump_group_buffer_size",
                                  default_dump_group_buffer_size);
        if (value < int(default_dump_group_buffer_size)) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "dump_group_buffer_size")
                             << " must not be less than "
                             << default_dump_group_buffer_size);
        }
        else if (value > int(max_dump_group_buffer_size)) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "dump_group_buffer_size")
                             << " must not be larger than "
                             << max_dump_group_buffer_size);
        }
    }

    bool    run_timeout_ok = NS_ValidateDouble(reg, section,
                                               "run_timeout");
    well_formed = well_formed && run_timeout_ok;
    if (run_timeout_ok) {
        double  value = reg.GetDouble(section, "run_timeout",
                                      double(default_run_timeout));
        if (value < 0.0) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "run_timeout")
                             << " must be >= 0.0");
        }
    }

    bool    run_timeout_precision_ok = NS_ValidateDouble(reg, section,
                                                     "run_timeout_precision");
    well_formed = well_formed && run_timeout_precision_ok;
    if (run_timeout_precision_ok) {
        double  value = reg.GetDouble(section, "run_timeout_precision",
                                      double(default_run_timeout_precision));
        if (value < 0.0) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "run_timeout_precision")
                             << " must be >= 0.0");
        }
    }

    well_formed = well_formed && NS_ValidateString(reg, section,
                                                   "program");

    bool    failed_retries_ok = NS_ValidateInt(reg, section,
                                               "failed_retries");
    well_formed = well_formed && failed_retries_ok;
    if (failed_retries_ok) {
        int    value = reg.GetInt(section, "failed_retries",
                                  default_failed_retries);
        if (value < 0) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "failed_retries")
                             << " must be > 0");
        }
    }

    bool    blacklist_time_ok = NS_ValidateDouble(reg, section,
                                                  "blacklist_time");
    well_formed = well_formed && blacklist_time_ok;
    if (blacklist_time_ok) {
        double  value = reg.GetDouble(section, "blacklist_time",
                                      double(default_blacklist_time));
        if (value < 0.0) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "blacklist_time")
                             << " must be >= 0.0");
        }
    }

    bool    max_input_size_ok = NS_ValidateString(reg, section,
                                                  "max_input_size");
    well_formed = well_formed && max_input_size_ok;
    if (max_input_size_ok) {
        string        s = reg.GetString(section, "max_input_size", kEmptyStr);
        unsigned int  value = kNetScheduleMaxDBDataSize;

        try {
            if (!s.empty())
                value = (unsigned int) NStr::StringToUInt8_DataSize(s);
        }
        catch (const CStringException &  ex) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "max_input_size")
                             << " could not be converted to unsigned integer: "
                             << ex.what());
        }

        if (value > kNetScheduleMaxOverflowSize) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "max_input_size")
                             << " must not be larger than "
                             << kNetScheduleMaxOverflowSize);
        }
    }

    bool    max_output_size_ok = NS_ValidateString(reg, section,
                                                   "max_output_size");
    well_formed = well_formed && max_output_size_ok;
    if (max_output_size_ok) {
        string        s = reg.GetString(section, "max_output_size", kEmptyStr);
        unsigned int  value = kNetScheduleMaxDBDataSize;

        try {
            if (!s.empty())
                value = (unsigned int) NStr::StringToUInt8_DataSize(s);
        }
        catch (CStringException &  ex) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "max_output_size")
                             << " could not be converted to unsigned integer: "
                             << ex.what());
        }

        if (value > kNetScheduleMaxOverflowSize) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "max_output_size")
                             << " must not be larger than "
                             << kNetScheduleMaxOverflowSize);
        }
    }

    well_formed = well_formed && NS_ValidateString(reg, section, "subm_host");
    well_formed = well_formed && NS_ValidateString(reg, section, "wnode_host");

    bool    wnode_timeout_ok = NS_ValidateDouble(reg, section, "wnode_timeout");
    well_formed = well_formed && wnode_timeout_ok;
    if (wnode_timeout_ok) {
        double  value = reg.GetDouble(section, "wnode_timeout",
                                      double(default_wnode_timeout));
        if (value <= 0.0) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "wnode_timeout")
                             << " must be > 0.0");
        }
    }

    bool    pending_timeout_ok = NS_ValidateDouble(reg, section,
                                                   "pending_timeout");
    well_formed = well_formed && pending_timeout_ok;
    if (pending_timeout_ok) {
        double  value = reg.GetDouble(section, "pending_timeout",
                                      double(default_pending_timeout));
        if (value <= 0.0) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section, "pending_timeout")
                             << " must be > 0.0");
        }
    }

    bool    max_pending_wait_timeout_ok = NS_ValidateDouble(reg, section,
                                                "max_pending_wait_timeout");
    well_formed = well_formed && max_pending_wait_timeout_ok;
    if (max_pending_wait_timeout_ok) {
        double  value = reg.GetDouble(section, "max_pending_wait_timeout",
                                      double(default_max_pending_wait_timeout));
        if (value < 0.0) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NS_RegValName(section,
                                              "max_pending_wait_timeout")
                             << " must be >= 0.0");
        }
    }

    well_formed = well_formed && NS_ValidateBool(reg, section,
                                                 "scramble_job_keys");


    // Deal with linked sections
    list<string>            available_sections;
    list<string>            entries;

    reg.EnumerateSections(&available_sections);
    reg.EnumerateEntries(section, &entries);


    for (list<string>::const_iterator  k = entries.begin();
         k != entries.end(); ++k) {
        string  entry = *k;

        if (!NStr::StartsWith(entry, "linked_section_", NStr::eCase))
            continue;

        if (entry == "linked_section_") {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " entry name "
                             << NS_RegValName(section, entry)
                             << " is malformed");
            continue;
        }

        bool    link_section_ok = NS_ValidateString(reg, section, entry);
        well_formed = well_formed && link_section_ok;
        if (link_section_ok) {
            string  ref_section = reg.GetString(section, entry, kEmptyStr);

            if (ref_section.empty()) {
                well_formed = false;
                LOG_POST(Warning << g_LogPrefix << " value of entry "
                                 << NS_RegValName(section, entry)
                                 << " must be supplied");
            } else if (find(available_sections.begin(),
                            available_sections.end(),
                            ref_section) == available_sections.end()) {
                well_formed = false;
                LOG_POST(Warning << g_LogPrefix << " value of entry "
                                 << NS_RegValName(section, entry)
                                 << " refers to a non-existing section");
            }
        }
    }

    // queue class must be checked the last
    if (!is_class) {
        bool    qclass_ok = NS_ValidateString(reg, section, "class");
        well_formed = well_formed && qclass_ok;
        if (qclass_ok) {
            string  value = reg.GetString(section, "class", kEmptyStr);

            if (!value.empty()) {
                if (find(classes->begin(),
                         classes->end(), value) == classes->end()) {
                    NCBI_THROW(CNetScheduleException, eInvalidParameter,
                               g_LogPrefix + " entry " +
                               NS_RegValName(section, "class") +
                               " refers to an unknown queue class");
                }
            }
        }
    }

    return well_formed;
}


// Returns true if the config file is well formed
bool NS_ValidateServiceToQueueSection(const IRegistry &  reg,
                                      const list<string> &  queues)
{
    bool                    well_formed = true;
    const string            section = "service_to_queue";

    list<string>            entries;
    reg.EnumerateEntries(section, &entries);

    for (list<string>::const_iterator  k = entries.begin();
         k != entries.end(); ++k) {
        string      service_name = *k;
        string      qname = reg.Get(section, service_name);
        if (qname.empty()) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << "service " << service_name
                             << " does not point to a queue.");
            continue;
        }

        // Check that the queue name has been provided
        if (find(queues.begin(), queues.end(), qname) == queues.end()) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << "service " << service_name
                             << " points to non-existent queue "
                             << qname);
        }
    }

    return well_formed;
}


// Forms the ini file value name for logging
string NS_RegValName(const string &  section, const string &  entry)
{
    return "[" + section + "]/" + entry;
}


// Forms an out of limits message
string NS_OutOfLimitMessage(const string &  section,
                            const string &  entry,
                            unsigned int  low_limit,
                            unsigned int  high_limit)
{
    return g_LogPrefix + NS_RegValName(section, entry) +
           " is out of limits (" + NStr::NumericToString(low_limit) + "..." +
           NStr::NumericToString(high_limit) + ")";

}


// Checks that a boolean value is fine
bool NS_ValidateBool(const IRegistry &  reg,
                     const string &  section, const string &  entry)
{
    try {
        reg.GetBool(section, entry, false);
    }
    catch (...) {
        LOG_POST(Warning << g_LogPrefix << "unexpected value of ["
                         << NS_RegValName(section, entry)
                         << ". Expected boolean value.");
        return false;
    }
    return true;
}


// Checks that an integer value is fine
bool NS_ValidateInt(const IRegistry &  reg,
                    const string &  section, const string &  entry)
{
    try {
        reg.GetInt(section, entry, 0);
    }
    catch (...) {
        LOG_POST(Warning << g_LogPrefix << "unexpected value of ["
                         << NS_RegValName(section, entry)
                         << ". Expected integer value.");
        return false;
    }
    return true;
}


// Checks that a double value is fine
bool NS_ValidateDouble(const IRegistry &  reg,
                       const string &  section, const string &  entry)
{
    try {
        reg.GetDouble(section, entry, 0.0);
    }
    catch (...) {
        LOG_POST(Warning << g_LogPrefix << "unexpected value of "
                         << NS_RegValName(section, entry)
                         << ". Expected floating point value.");
        return false;
    }
    return true;
}


// Checks that a double value is fine
bool NS_ValidateString(const IRegistry &  reg,
                       const string &  section, const string &  entry)
{
    try {
        reg.GetString(section, entry, "");
    }
    catch (...) {
        LOG_POST(Warning << g_LogPrefix << "unexpected value of "
                         << NS_RegValName(section, entry)
                         << ". Expected string value.");
        return false;
    }
    return true;
}

END_NCBI_SCOPE

