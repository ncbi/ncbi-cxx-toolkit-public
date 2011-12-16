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
 * File Description: Queue parameters
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/netschedule_api.hpp>

#include "ns_queue_parameters.hpp"
#include "ns_types.hpp"
#include "ns_db.hpp"


BEGIN_NCBI_SCOPE


static int              default_timeout = 3600;
static double           default_notif_hifreq_interval = 0.1;
static unsigned int     default_notif_hifreq_period = 5;
static unsigned int     default_notif_lofreq_mult = 50;
static int              default_run_timeout = 3600;
static int              default_failed_retries = 0;
static time_t           default_empty_lifetime = -1;
static time_t           default_blacklist_time = 0;
static int              default_run_timeout_precision = 3600;


SQueueParameters::SQueueParameters() :
    timeout(default_timeout),
    notif_hifreq_interval(default_notif_hifreq_interval),
    notif_hifreq_period(default_notif_hifreq_period),
    notif_lofreq_mult(default_notif_lofreq_mult),
    run_timeout(default_run_timeout),
    failed_retries(default_failed_retries),
    blacklist_time(default_blacklist_time),
    empty_lifetime(default_empty_lifetime),
    max_input_size(kNetScheduleMaxDBDataSize),
    max_output_size(kNetScheduleMaxDBDataSize),
    deny_access_violations(false),
    log_access_violations(true),
    run_timeout_precision(default_run_timeout_precision)
{}



#define GetIntNoErr(name, dflt)    reg.GetInt(sname, name, dflt, 0, IRegistry::eReturn)
#define GetDoubleNoErr(name, dflt) reg.GetDouble(sname, name, dflt, 0, IRegistry::eReturn)


void SQueueParameters::Read(const IRegistry& reg, const string& sname)
{
    // When modifying this, modify all places marked with PARAMETERS

    // Read parameters
    timeout = GetIntNoErr("timeout", default_timeout);

    // Notification timeout
    notif_hifreq_interval = GetDoubleNoErr("notif_hifreq_interval",
                                           default_notif_hifreq_interval);
    notif_hifreq_interval = (int(notif_hifreq_interval * 10)) / 10.0;
    if (notif_hifreq_interval <= 0)
        notif_hifreq_interval = default_notif_hifreq_interval;

    notif_hifreq_period = GetIntNoErr("notif_hifreq_period",
                                      default_notif_hifreq_period);
    if (notif_hifreq_period <= 0)
        notif_hifreq_period = default_notif_hifreq_period;

    notif_lofreq_mult = GetIntNoErr("notif_lofreq_mult",
                                    default_notif_lofreq_mult);
    if (notif_lofreq_mult <= 0)
        notif_lofreq_mult = default_notif_lofreq_mult;

    run_timeout           = GetIntNoErr("run_timeout",
                                        default_run_timeout);
    run_timeout_precision = GetIntNoErr("run_timeout_precision",
                                        default_run_timeout_precision);
    program_name          = reg.GetString(sname, "program", kEmptyStr);

    failed_retries = GetIntNoErr("failed_retries", default_failed_retries);
    blacklist_time = GetIntNoErr("blacklist_time", default_blacklist_time);
    empty_lifetime = GetIntNoErr("empty_lifetime", default_empty_lifetime);


    // Max input size
    string  s = reg.GetString(sname, "max_input_size", kEmptyStr);
    max_input_size = kNetScheduleMaxDBDataSize;
    try {
        max_input_size = (unsigned) NStr::StringToUInt8_DataSize(s);
    }
    catch (CStringException&) {}
    max_input_size = min(kNetScheduleMaxOverflowSize, max_input_size);


    // Max output size
    s = reg.GetString(sname, "max_output_size", kEmptyStr);
    max_output_size = kNetScheduleMaxDBDataSize;
    try {
        max_output_size = (unsigned) NStr::StringToUInt8_DataSize(s);
    }
    catch (CStringException&) {}
    max_output_size = min(kNetScheduleMaxOverflowSize, max_output_size);


    deny_access_violations = reg.GetBool(sname, "deny_access_violations", false,
                                         0, IRegistry::eReturn);
    log_access_violations  = reg.GetBool(sname, "log_access_violations", true,
                                         0, IRegistry::eReturn);

    subm_hosts = reg.GetString(sname,  "subm_host",  kEmptyStr);
    wnode_hosts = reg.GetString(sname, "wnode_host", kEmptyStr);
    return;
}


END_NCBI_SCOPE

