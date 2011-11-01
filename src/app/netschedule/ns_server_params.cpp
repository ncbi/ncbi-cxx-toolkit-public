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


void SNS_Parameters::Read(const IRegistry& reg, const string& sname)
{
    reinit          = GetBoolNoErr("reinit", false);
    max_connections = GetIntNoErr("max_connections", 100);
    max_threads     = GetIntNoErr("max_threads", 25);

    init_threads = GetIntNoErr("init_threads", 10);
    if (init_threads > max_threads)
        init_threads = max_threads;

    port = (unsigned short) GetIntNoErr("port", 9100);

    use_hostname    = GetBoolNoErr("use_hostname", false);
    network_timeout = GetIntNoErr("network_timeout", 10);
    if (network_timeout == 0) {
        LOG_POST(Warning <<
            "INI file sets 0 sec. network timeout. Assume 10 seconds.");
        network_timeout =  10;
    }

    // Logging parameters
    is_log                       = GetBoolNoErr("log", false);
    log_batch_each_job           = GetBoolNoErr("log_batch_each_job", true);
    log_notification_thread      = GetBoolNoErr("log_notification_thread", false);
    log_cleaning_thread          = GetBoolNoErr("log_cleaning_thread", true);
    log_execution_watcher_thread = GetBoolNoErr("log_execution_watcher_thread", true);
    log_statistics_thread        = GetBoolNoErr("log_statistics_thread", true);

    // Job deleting parameters
    del_batch_size = GetIntNoErr("del_batch_size", 100);
    scan_batch_size = GetIntNoErr("scan_batch_size", 10000);
    purge_timeout  = GetDoubleNoErr("purge_timeout", 0.1);

    is_daemon   = GetBoolNoErr("daemon", false);
    admin_hosts = reg.GetString(sname, "admin_host", kEmptyStr);
    return;
}


static string s_NSParameters[] =
{
    "reinit",                       // 0
    "max_connections",              // 1
    "max_threads",                  // 2
    "init_threads",                 // 3
    "port",                         // 4
    "use_hostname",                 // 5
    "network_timeout",              // 6
    "log",                          // 7
    "daemon",                       // 8
    "admin_host",                   // 9
    "log_batch_each_job",           // 10
    "log_notification_thread",      // 11
    "log_cleaning_thread",          // 12
    "log_execution_watcher_thread", // 13
    "log_statistics_thread",        // 14
    "del_batch_size",               // 15
    "scan_batch_size",              // 16
    "purge_timeout"                 // 17
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
    case 8:  return NStr::BoolToString(is_daemon);
    case 9:  return admin_hosts;
    case 10: return NStr::BoolToString(log_batch_each_job);
    case 11: return NStr::BoolToString(log_notification_thread);
    case 12: return NStr::BoolToString(log_cleaning_thread);
    case 13: return NStr::BoolToString(log_execution_watcher_thread);
    case 14: return NStr::BoolToString(log_statistics_thread);
    case 15: return NStr::UIntToString(del_batch_size);
    case 16: return NStr::UIntToString(scan_batch_size);
    case 17: return NStr::DoubleToString(purge_timeout);
    default: return kEmptyStr;
    }
}

