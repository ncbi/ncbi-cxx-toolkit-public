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
#include "ns_queue.hpp"


BEGIN_NCBI_SCOPE


static unsigned int     default_timeout = 3600;
static double           default_notif_hifreq_interval = 0.1;
static unsigned int     default_notif_hifreq_period = 5;
static unsigned int     default_notif_lofreq_mult = 50;
static unsigned int     default_dump_buffer_size = 100;
static unsigned int     default_run_timeout = 3600;
static unsigned int     default_failed_retries = 0;
static time_t           default_blacklist_time = 0;
static int              default_run_timeout_precision = 3600;
static time_t           default_wnode_timeout = 40;
static time_t           default_pending_timeout = 604800;
static double           default_max_pending_wait_timeout = 0.0;


SQueueParameters::SQueueParameters() :
    kind(CQueue::eKindStatic),
    position(-1),
    delete_request(false),
    qclass(""),
    timeout(default_timeout),
    notif_hifreq_interval(default_notif_hifreq_interval),
    notif_hifreq_period(default_notif_hifreq_period),
    notif_lofreq_mult(default_notif_lofreq_mult),
    dump_buffer_size(default_dump_buffer_size),
    run_timeout(default_run_timeout),
    program_name(""),
    failed_retries(default_failed_retries),
    blacklist_time(default_blacklist_time),
    max_input_size(kNetScheduleMaxDBDataSize),
    max_output_size(kNetScheduleMaxDBDataSize),
    subm_hosts(""),
    wnode_hosts(""),
    wnode_timeout(default_wnode_timeout),
    pending_timeout(default_pending_timeout),
    max_pending_wait_timeout(default_max_pending_wait_timeout),
    description(""),
    run_timeout_precision(default_run_timeout_precision)
{}



#define GetIntNoErr(name, dflt)    reg.GetInt(sname, name, dflt, 0, IRegistry::eReturn)
#define GetDoubleNoErr(name, dflt) reg.GetDouble(sname, name, dflt, 0, IRegistry::eReturn)


void SQueueParameters::Read(const IRegistry& reg, const string& sname)
{
    qclass = reg.GetString(sname, "class", kEmptyStr);

    timeout = ReadTimeout(reg, sname);
    notif_hifreq_interval = ReadNotifHifreqInterval(reg, sname);
    notif_hifreq_period = ReadNotifHifreqPeriod(reg, sname);
    notif_lofreq_mult = ReadNotifLofreqMult(reg, sname);
    dump_buffer_size = ReadDumpBufferSize(reg, sname);
    run_timeout = ReadRunTimeout(reg, sname);
    program_name = ReadProgram(reg, sname);
    failed_retries = ReadFailedRetries(reg, sname);
    blacklist_time = ReadBlacklistTime(reg, sname);
    max_input_size = ReadMaxInputSize(reg, sname);
    max_output_size = ReadMaxOutputSize(reg, sname);
    subm_hosts = ReadSubmHosts(reg, sname);
    wnode_hosts = ReadWnodeHosts(reg, sname);
    wnode_timeout = ReadWnodeTimeout(reg, sname);
    pending_timeout = ReadPendingTimeout(reg, sname);
    max_pending_wait_timeout = ReadMaxPendingWaitTimeout(reg, sname);
    description = ReadDescription(reg, sname);
    run_timeout_precision = ReadRunTimeoutPrecision(reg, sname);
    return;
}


// Returns descriptive diff
// Empty string if there is no difference
string
SQueueParameters::Diff(const SQueueParameters &  other,
                       bool                      include_class_cmp,
                       bool                      include_description) const
{
    string      diff;

    // It does not make sense to compare the qclass field for a queue class
    if (include_class_cmp && qclass != other.qclass)
        AddParameterToDiffString(diff, "class",
                                 qclass,
                                 other.qclass);

    if (timeout != other.timeout)
        AddParameterToDiffString(diff, "timeout",
                                 timeout,
                                 other.timeout);

    if (notif_hifreq_interval != other.notif_hifreq_interval)
        AddParameterToDiffString(diff, "notif_hifreq_interval",
                                 notif_hifreq_interval,
                                 other.notif_hifreq_interval);

    if (notif_hifreq_period != other.notif_hifreq_period)
        AddParameterToDiffString(diff, "notif_hifreq_period",
                                 notif_hifreq_period,
                                 other.notif_hifreq_period);

    if (notif_lofreq_mult != other.notif_lofreq_mult)
        AddParameterToDiffString(diff, "notif_lofreq_mult",
                                 notif_lofreq_mult,
                                 other.notif_lofreq_mult);

    if (dump_buffer_size != other.dump_buffer_size)
        AddParameterToDiffString(diff, "dump_buffer_size",
                                 dump_buffer_size,
                                 other.dump_buffer_size);

    if (run_timeout != other.run_timeout)
        AddParameterToDiffString(diff, "run_timeout",
                                 run_timeout,
                                 other.run_timeout);

    if (program_name != other.program_name)
        AddParameterToDiffString(diff, "program",
                                 program_name,
                                 other.program_name);

    if (failed_retries != other.failed_retries)
        AddParameterToDiffString(diff, "failed_retries",
                                 failed_retries,
                                 other.failed_retries);

    if (blacklist_time != other.blacklist_time)
        AddParameterToDiffString(diff, "blacklist_time",
                                 blacklist_time,
                                 other.blacklist_time);

    if (max_input_size != other.max_input_size)
        AddParameterToDiffString(diff, "max_input_size",
                                 max_input_size,
                                 other.max_input_size);

    if (max_output_size != other.max_output_size)
        AddParameterToDiffString(diff, "max_output_size",
                                 max_output_size,
                                 other.max_output_size);

    if (subm_hosts != other.subm_hosts)
        AddParameterToDiffString(diff, "subm_host",
                                 subm_hosts,
                                 other.subm_hosts);

    if (wnode_hosts != other.wnode_hosts)
        AddParameterToDiffString(diff, "wnode_host",
                                 wnode_hosts,
                                 other.wnode_hosts);

    if (wnode_timeout != other.wnode_timeout)
        AddParameterToDiffString(diff, "wnode_timeout",
                                 wnode_timeout,
                                 other.wnode_timeout);

    if (pending_timeout != other.pending_timeout)
        AddParameterToDiffString(diff, "pending_timeout",
                                 pending_timeout,
                                 other.pending_timeout);

    if (max_pending_wait_timeout != other.max_pending_wait_timeout)
        AddParameterToDiffString(diff, "max_pending_wait_timeout",
                                 max_pending_wait_timeout,
                                 other.max_pending_wait_timeout);

    if (include_description && description != other.description)
        AddParameterToDiffString(diff, "description",
                                 description,
                                 other.description);

    // The run_timeout_precision parameter cannot be changed at run time so it
    // is not compared here.

    return diff;
}


// Classes are included for queues only (not for queue classes)
string
SQueueParameters::GetPrintableParameters(bool  include_class) const
{
    string      result;

    if (include_class) {
        // These parameters make sense for queues only
        result = "OK:kind: ";
        if (kind == CQueue::eKindStatic)
            result += "static\n";
        else
            result += "dynamic\n";

        result +=
        "OK:position: " + NStr::NumericToString(position) + "\n"
        "OK:qclass: " + qclass + "\n";
    }

    result +=
    "OK:delete_request: " + NStr::BoolToString(delete_request) + "\n"
    "OK:timeout: " + NStr::NumericToString(timeout) + "\n"
    "OK:notif_hifreq_interval: " + NStr::NumericToString(notif_hifreq_interval) + "\n"
    "OK:notif_hifreq_period: " + NStr::NumericToString(notif_hifreq_period) + "\n"
    "OK:notif_lofreq_mult: " + NStr::NumericToString(notif_lofreq_mult) + "\n"
    "OK:dump_buffer_size: " + NStr::NumericToString(dump_buffer_size) + "\n"
    "OK:run_timeout: " + NStr::NumericToString(run_timeout) + "\n"
    "OK:program_name: " + NStr::PrintableString(program_name) + "\n"
    "OK:failed_retries: " + NStr::NumericToString(failed_retries) + "\n"
    "OK:blacklist_time: " + NStr::NumericToString(blacklist_time) + "\n"
    "OK:max_input_size: " + NStr::NumericToString(max_input_size) + "\n"
    "OK:max_output_size: " + NStr::NumericToString(max_output_size) + "\n"
    "OK:subm_hosts: " + NStr::PrintableString(subm_hosts) + "\n"
    "OK:wnode_hosts: " + NStr::PrintableString(wnode_hosts) + "\n"
    "OK:wnode_timeout: " + NStr::NumericToString(wnode_timeout) + "\n"
    "OK:pending_timeout: " + NStr::NumericToString(pending_timeout) + "\n"
    "OK:max_pending_wait_timeout: " + NStr::NumericToString(max_pending_wait_timeout) + "\n"
    "OK:description: " + NStr::PrintableString(description) + "\n"
    "OK:run_timeout_precision: " + NStr::NumericToString(run_timeout_precision);
    return result;
}


unsigned int
SQueueParameters::ReadTimeout(const IRegistry &  reg,
                              const string &     sname)
{
    return GetIntNoErr("timeout", default_timeout);
}

double
SQueueParameters::ReadNotifHifreqInterval(const IRegistry &  reg,
                                          const string &     sname)
{
    double  val = GetDoubleNoErr("notif_hifreq_interval",
                                 default_notif_hifreq_interval);
    val = (int(notif_hifreq_interval * 10)) / 10.0;
    if (val <= 0)
        val = default_notif_hifreq_interval;
    return val;
}

unsigned int
SQueueParameters::ReadNotifHifreqPeriod(const IRegistry &  reg,
                                        const string &     sname)
{
    unsigned int    val = GetIntNoErr("notif_hifreq_period",
                                      default_notif_hifreq_period);
    if (val <= 0)
        val = default_notif_hifreq_period;
    return val;
}

unsigned int
SQueueParameters::ReadNotifLofreqMult(const IRegistry &  reg,
                                      const string &     sname)
{
    unsigned int    val = GetIntNoErr("notif_lofreq_mult",
                                      default_notif_lofreq_mult);
    if (val <= 0)
        val = default_notif_lofreq_mult;
    return val;
}

unsigned int
SQueueParameters::ReadDumpBufferSize(const IRegistry &  reg,
                                     const string &     sname)
{
    unsigned int    val = GetIntNoErr("dump_buffer_size",
                                      default_dump_buffer_size);
    if (val < default_dump_buffer_size)
        val = default_dump_buffer_size;  // Avoid too small buffer
    else if (val > 10000)
        val = 10000;                     // Avoid too large buffer
    return val;
}

unsigned int
SQueueParameters::ReadRunTimeout(const IRegistry &  reg,
                                 const string &     sname)
{
    return GetIntNoErr("run_timeout", default_run_timeout);
}

string
SQueueParameters::ReadProgram(const IRegistry &  reg,
                              const string &     sname)
{
    return reg.GetString(sname, "program", kEmptyStr);
}

unsigned int
SQueueParameters::ReadFailedRetries(const IRegistry &  reg,
                                    const string &     sname)
{
    return GetIntNoErr("failed_retries", default_failed_retries);
}

time_t
SQueueParameters::ReadBlacklistTime(const IRegistry &  reg,
                                    const string &     sname)
{
    return GetIntNoErr("blacklist_time", default_blacklist_time);
}

unsigned int
SQueueParameters::ReadMaxInputSize(const IRegistry &  reg,
                                   const string &     sname)
{
    string        s = reg.GetString(sname, "max_input_size", kEmptyStr);
    unsigned int  val = kNetScheduleMaxDBDataSize;

    try {
        val = (unsigned) NStr::StringToUInt8_DataSize(s);
    }
    catch (CStringException&)
    {}

    val = min(kNetScheduleMaxOverflowSize, val);
    return val;
}

unsigned int
SQueueParameters::ReadMaxOutputSize(const IRegistry &  reg,
                                    const string &     sname)
{
    string        s = reg.GetString(sname, "max_output_size", kEmptyStr);
    unsigned int  val = kNetScheduleMaxDBDataSize;

    try {
        val = (unsigned) NStr::StringToUInt8_DataSize(s);
    }
    catch (CStringException&)
    {}

    val = min(kNetScheduleMaxOverflowSize, val);
    return val;
}

string
SQueueParameters::ReadSubmHosts(const IRegistry &  reg,
                                const string &     sname)
{
    return reg.GetString(sname, "subm_host", kEmptyStr);
}

string
SQueueParameters::ReadWnodeHosts(const IRegistry &  reg,
                                 const string &     sname)
{
    return reg.GetString(sname, "wnode_host", kEmptyStr);
}

time_t
SQueueParameters::ReadWnodeTimeout(const IRegistry &  reg,
                                   const string &     sname)
{
    time_t  val = GetIntNoErr("wnode_timeout", default_wnode_timeout);

    if (val <= 0)
        val = default_wnode_timeout;
    return val;
}

time_t
SQueueParameters::ReadPendingTimeout(const IRegistry &  reg,
                                     const string &     sname)
{
    time_t  val = GetIntNoErr("pending_timeout", default_pending_timeout);

    if (val <= 0)
        val = default_pending_timeout;
    return val;
}

double
SQueueParameters::ReadMaxPendingWaitTimeout(const IRegistry &  reg,
                                            const string &     sname)
{
    double  val = GetDoubleNoErr("max_pending_wait_timeout",
                                 default_max_pending_wait_timeout);

    if (val < 0.0)
        val = 0.0;
    return val;
}

string
SQueueParameters::ReadDescription(const IRegistry &  reg,
                                  const string &     sname)
{
    return reg.GetString(sname, "description", kEmptyStr);
}


unsigned int
SQueueParameters::ReadRunTimeoutPrecision(const IRegistry &  reg,
                                          const string &     sname)
{
    return GetIntNoErr("run_timeout_precision",
                       default_run_timeout_precision);
}

END_NCBI_SCOPE

