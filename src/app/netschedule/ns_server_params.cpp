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
#include <connect/services/netschedule_api.hpp>
#include <corelib/request_ctx.hpp>
#include "ns_server_params.hpp"
#include "ns_ini_params.hpp"
#include "ns_util.hpp"


USING_NCBI_SCOPE;

#define GetIntNoErr(name, dflt) \
    reg.GetInt(sname, name, dflt, 0, IRegistry::eReturn)
#define GetBoolNoErr(name, dflt) \
    reg.GetBool(sname, name, dflt, 0, IRegistry::eReturn)
#define GetDoubleNoErr(name, dflt) \
    reg.GetDouble(sname, name, dflt, 0, IRegistry::eReturn)


#if defined(_DEBUG) && !defined(NDEBUG)
void SErrorEmulatorParameter::ReadInt(const IRegistry &  reg,
                                      const string &     section,
                                      const string &     param_name)
{
    x_ReadCommon(reg, section, param_name);
    if (!value.empty())
        as_int = NStr::StringToInt(value);
}

void SErrorEmulatorParameter::ReadDouble(const IRegistry &  reg,
                                         const string &     section,
                                         const string &     param_name)
{
    x_ReadCommon(reg, section, param_name);
    if (!value.empty())
        as_double = NStr::StringToDouble(value);
}

void SErrorEmulatorParameter::ReadBool(const IRegistry &  reg,
                                       const string &     section,
                                       const string &     param_name)
{
    x_ReadCommon(reg, section, param_name);
    if (!value.empty())
        as_bool = NStr::StringToBool(value);
}

void SErrorEmulatorParameter::x_ReadCommon(const IRegistry &  reg,
                                           const string &     section,
                                           const string &     param_name)
{
    string      reg_value = reg.GetString(section, param_name, "");
    reg_value = NStr::TruncateSpaces(reg_value);
    if (reg_value.empty())
        return;

    list<string>    parts;
    NStr::Split(reg_value, " ", parts,
                NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
    size_t          parts_count = parts.size();

    if (parts_count != 1 && parts_count != 2)
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   "Unexpected format of the [" + section + "]/" +
                   param_name + " parameter. Expected: F:Ff [Fb-Fe]");

    if (parts_count == 2) {
        // Need to read the range
        list<string>    range_parts;
        NStr::Split(parts.back(), "-", range_parts,
                    NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);

        if (range_parts.size() != 2)
            NCBI_THROW(CNetScheduleException, eInvalidParameter,
                       "Unexpected range format of the [" + section + "]/" +
                       param_name + " parameter. Expected: Fb-Fe");
        range_begin = NStr::StringToULong(range_parts.front());
        range_end = NStr::StringToULong(range_parts.back());

        if (range_end < range_begin)
            NCBI_THROW(CNetScheduleException, eInvalidParameter,
                       "Incorrect range specification for [" + section + "]/" +
                       param_name + " parameter. "
                       "End value must be >= begin value.");
    }

    // Read frequency
    list<string>    val_freq_parts;
    NStr::Split(parts.front(), ":", val_freq_parts,
                NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
    if (val_freq_parts.size() != 1 && val_freq_parts.size() != 2)
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   "Unexpected value and frequence format of the [" +
                   section + "]/" + param_name + " parameter. Expected: F:Ff");
    else if (val_freq_parts.size() == 1)
        frequency = 1;  // default
    else
        frequency = NStr::StringToUInt(val_freq_parts.back());
    value = val_freq_parts.front();
}

bool SErrorEmulatorParameter::IsActive(void) const
{
    if (frequency == 0)
        return false;

    Uint8   request_id = CDiagContext::GetRequestContext().GetRequestID();
    if (request_id < range_begin)
        return false;
    if (request_id > range_end)
        return false;

    return (request_id - range_begin) % frequency == 0;
}
#endif



// A configuration file is always already validated so the reading
// must not produce any log output etc.
// See ns_util.cpp NS_ValidateConfigFile(...)
void SNS_Parameters::Read(const IRegistry &  reg)
{
    const string    sname = "server";

    max_connections = GetIntNoErr("max_connections", default_max_connections);
    if (max_connections < max_connections_low_limit)
        max_connections = (max_connections_low_limit +
                           max_connections_high_limit) / 2;
    else if (max_connections > max_connections_high_limit)
        max_connections = max_connections_high_limit;

    max_threads     = GetIntNoErr("max_threads", default_max_threads);

    init_threads = GetIntNoErr("init_threads", default_init_threads);
    if (init_threads > max_threads)
        init_threads = max_threads;

    port = (unsigned short) GetIntNoErr("port", 0);

    use_hostname    = GetBoolNoErr("use_hostname", default_use_hostname);
    network_timeout = GetIntNoErr("network_timeout", default_network_timeout);
    if (network_timeout == 0)
        network_timeout = default_network_timeout;

    // Logging parameters
    is_log = GetBoolNoErr("log", default_is_log);
    log_batch_each_job = GetBoolNoErr("log_batch_each_job",
                                      default_log_batch_each_job);
    log_notification_thread = GetBoolNoErr("log_notification_thread",
                                           default_log_notification_thread);
    log_cleaning_thread = GetBoolNoErr("log_cleaning_thread",
                                        default_log_cleaning_thread);
    log_execution_watcher_thread = GetBoolNoErr("log_execution_watcher_thread",
                                        default_log_execution_watcher_thread);
    log_statistics_thread = GetBoolNoErr("log_statistics_thread",
                                         default_log_statistics_thread);

    // Job deleting parameters
    del_batch_size = GetIntNoErr("del_batch_size", default_del_batch_size);
    markdel_batch_size = GetIntNoErr("markdel_batch_size",
                                     default_markdel_batch_size);
    scan_batch_size = GetIntNoErr("scan_batch_size", default_scan_batch_size);
    purge_timeout = GetDoubleNoErr("purge_timeout", default_purge_timeout);
    x_CheckGarbageCollectorSettings();

    stat_interval = GetIntNoErr("stat_interval", default_stat_interval);
    if (stat_interval < 1)
        stat_interval = 1;

    // Affinity GC settings
    affinity_high_mark_percentage =
                        GetIntNoErr("affinity_high_mark_percentage",
                                    default_affinity_high_mark_percentage);
    affinity_low_mark_percentage =
                        GetIntNoErr("affinity_low_mark_percentage",
                                    default_affinity_low_mark_percentage);
    affinity_high_removal = GetIntNoErr("affinity_high_removal",
                                        default_affinity_high_removal);
    affinity_low_removal = GetIntNoErr("affinity_low_removal",
                                       default_affinity_low_removal);
    affinity_dirt_percentage = GetIntNoErr("affinity_dirt_percentage",
                                           default_affinity_dirt_percentage);
    x_CheckAffinityGarbageCollectorSettings();

    reserve_dump_space = NS_GetDataSize(reg, "server", "reserve_dump_space",
                                        default_reserve_dump_space);
    wst_cache_size = GetIntNoErr("wst_cache_size",
                                 default_wst_cache_size);
    if (wst_cache_size < 0)
        wst_cache_size = default_wst_cache_size;

    // Max affinities
    max_affinities = GetIntNoErr("max_affinities", default_max_affinities);
    if (max_affinities <= 0)
        max_affinities = default_max_affinities;

    max_client_data = GetIntNoErr("max_client_data", default_max_client_data);
    if (max_client_data <= 0)
        max_client_data = default_max_client_data;

    admin_hosts        = reg.GetString(sname, "admin_host", kEmptyStr);
    try {
        admin_client_names = reg.GetEncryptedString(sname, "admin_client_name",
                                                IRegistry::fPlaintextAllowed);
    } catch (...) {
        // All the warnings are collected at the time of validating the
        // configuration file, so here a problem is simply suppressed
    }

    #if defined(_DEBUG) && !defined(NDEBUG)
    ReadErrorEmulatorSection(reg);
    #endif
}


#if defined(_DEBUG) && !defined(NDEBUG)
void SNS_Parameters::ReadErrorEmulatorSection(const IRegistry &  reg)
{
    debug_fd_count.ReadInt(reg, "error_emulator", "fd_report");
    if (!debug_fd_count.value.empty() && debug_fd_count.as_int < -1)
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   "Unexpected value of the [error_emulator]/fd_report "
                   " parameter. It must be >= 0");
    debug_mem_count.ReadInt(reg, "error_emulator", "mem_report");
    if (!debug_mem_count.value.empty() && debug_mem_count.as_int < -1)
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   "Unexpected value of the [error_emulator]/mem_report "
                   " parameter. It must be >= 0");
    debug_write_delay.ReadDouble(reg, "error_emulator", "delay");
    if (!debug_write_delay.value.empty() && debug_write_delay.as_double < 0.0)
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   "Unexpected value of the [error_emulator]/delay "
                   " parameter. It must be >= 0");
    debug_conn_drop_before_write.ReadBool(reg, "error_emulator",
                                          "drop_before_reply");
    debug_conn_drop_after_write.ReadBool(reg, "error_emulator",
                                         "drop_after_reply");
    debug_reply_with_garbage.ReadBool(reg, "error_emulator",
                                      "reply_with_garbage");

    debug_garbage = reg.GetString("error_emulator", "garbage_data",
                                  "please define [error_emulator]/garbage_data "
                                  "parameter value");
}
#endif


void SNS_Parameters::x_CheckAffinityGarbageCollectorSettings(void)
{
    bool    well_formed = true;

    if (affinity_high_mark_percentage >= 100)
        well_formed = false;

    if (affinity_low_mark_percentage >= affinity_high_mark_percentage)
        well_formed = false;

    if (affinity_dirt_percentage >= affinity_low_mark_percentage)
        well_formed = false;

    if (affinity_high_removal < affinity_low_removal)
        well_formed = false;

    if (!well_formed) {
        affinity_high_mark_percentage = default_affinity_high_mark_percentage;
        affinity_low_mark_percentage = default_affinity_low_mark_percentage;
        affinity_high_removal = default_affinity_high_removal;
        affinity_low_removal = default_affinity_low_removal;
        affinity_dirt_percentage = default_affinity_dirt_percentage;
    }
}


void SNS_Parameters::x_CheckGarbageCollectorSettings(void)
{
    bool    well_formed = true;

    if (del_batch_size == 0)
        well_formed = false;

    if (markdel_batch_size == 0)
        well_formed = false;

    if (scan_batch_size == 0)
        well_formed = false;

    if (purge_timeout <= 0.0)
        well_formed = false;

    if (scan_batch_size < markdel_batch_size)
        well_formed = false;

    if (markdel_batch_size < del_batch_size)
        well_formed = false;

    if (well_formed == false) {
        del_batch_size = default_del_batch_size;
        scan_batch_size = default_scan_batch_size;
        purge_timeout = default_purge_timeout;
        markdel_batch_size = default_markdel_batch_size;
    }
}

