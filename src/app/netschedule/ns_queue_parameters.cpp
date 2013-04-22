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


static CNSPreciseTime   default_timeout(3600, 0);
static CNSPreciseTime   default_notif_hifreq_interval(0, kNSecsPerSecond/10); // 0.1
static CNSPreciseTime   default_notif_hifreq_period(5, 0);
static unsigned int     default_notif_lofreq_mult = 50;
static CNSPreciseTime   default_notif_handicap(0, 0);
static unsigned int     default_dump_buffer_size = 100;
static CNSPreciseTime   default_run_timeout(3600, 0);
static unsigned int     default_failed_retries = 0;
static CNSPreciseTime   default_blacklist_time = CNSPreciseTime(2147483647, 0);
static CNSPreciseTime   default_run_timeout_precision(3, 0);
static CNSPreciseTime   default_wnode_timeout(40, 0);
static CNSPreciseTime   default_pending_timeout(604800, 0);
static CNSPreciseTime   default_max_pending_wait_timeout(0, 0);


SQueueParameters::SQueueParameters() :
    kind(CQueue::eKindStatic),
    position(-1),
    delete_request(false),
    qclass(""),
    timeout(default_timeout),
    notif_hifreq_interval(default_notif_hifreq_interval),
    notif_hifreq_period(default_notif_hifreq_period),
    notif_lofreq_mult(default_notif_lofreq_mult),
    notif_handicap(default_notif_handicap),
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
    notif_handicap = ReadNotifHandicap(reg, sname);
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

    if (timeout.Sec() != other.timeout.Sec())
        AddParameterToDiffString(diff, "timeout",
                                 timeout.Sec(),
                                 other.timeout.Sec());

    if (notif_hifreq_interval != other.notif_hifreq_interval)
        AddParameterToDiffString(
                diff, "notif_hifreq_interval",
                NS_FormatPreciseTimeAsSec(notif_hifreq_interval),
                NS_FormatPreciseTimeAsSec(other.notif_hifreq_interval));

    if (notif_hifreq_period != other.notif_hifreq_period)
        AddParameterToDiffString(
                diff, "notif_hifreq_period",
                NS_FormatPreciseTimeAsSec(notif_hifreq_period),
                NS_FormatPreciseTimeAsSec(other.notif_hifreq_period));

    if (notif_lofreq_mult != other.notif_lofreq_mult)
        AddParameterToDiffString(diff, "notif_lofreq_mult",
                                 notif_lofreq_mult,
                                 other.notif_lofreq_mult);

    if (notif_handicap != other.notif_handicap)
        AddParameterToDiffString(
                diff, "notif_handicap",
                NS_FormatPreciseTimeAsSec(notif_handicap),
                NS_FormatPreciseTimeAsSec(other.notif_handicap));

    if (dump_buffer_size != other.dump_buffer_size)
        AddParameterToDiffString(diff, "dump_buffer_size",
                                 dump_buffer_size,
                                 other.dump_buffer_size);

    if (run_timeout != other.run_timeout)
        AddParameterToDiffString(
                diff, "run_timeout",
                NS_FormatPreciseTimeAsSec(run_timeout),
                NS_FormatPreciseTimeAsSec(other.run_timeout));

    if (program_name != other.program_name)
        AddParameterToDiffString(diff, "program",
                                 program_name,
                                 other.program_name);

    if (failed_retries != other.failed_retries)
        AddParameterToDiffString(diff, "failed_retries",
                                 failed_retries,
                                 other.failed_retries);

    if (blacklist_time != other.blacklist_time)
        AddParameterToDiffString(
                diff, "blacklist_time",
                NS_FormatPreciseTimeAsSec(blacklist_time),
                NS_FormatPreciseTimeAsSec(other.blacklist_time));

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
        AddParameterToDiffString(
                diff, "wnode_timeout",
                NS_FormatPreciseTimeAsSec(wnode_timeout),
                NS_FormatPreciseTimeAsSec(other.wnode_timeout));

    if (pending_timeout != other.pending_timeout)
        AddParameterToDiffString(
                diff, "pending_timeout",
                NS_FormatPreciseTimeAsSec(pending_timeout),
                NS_FormatPreciseTimeAsSec(other.pending_timeout));

    if (max_pending_wait_timeout != other.max_pending_wait_timeout)
        AddParameterToDiffString(
                diff, "max_pending_wait_timeout",
                NS_FormatPreciseTimeAsSec(max_pending_wait_timeout),
                NS_FormatPreciseTimeAsSec(other.max_pending_wait_timeout));

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
SQueueParameters::GetPrintableParameters(bool  include_class,
                                         bool  url_encoded) const
{
    string      result;

    /* Initialized for multi-line output */
    string      prefix("OK:");
    string      suffix(": ");
    string      separator("\n");

    if (url_encoded) {
        prefix    = "";
        suffix    = "=";
        separator = "&";
    }

    if (include_class) {
        // These parameters make sense for queues only
        result = prefix + "kind" + suffix;
        if (kind == CQueue::eKindStatic)
            result += "static" + separator;
        else
            result += "dynamic" + separator;

        result +=
        prefix + "position" + suffix + NStr::NumericToString(position) + separator +
        prefix + "qclass" + suffix + qclass + separator +
        prefix + "refuse_submits" + suffix + NStr::BoolToString(refuse_submits) + separator +
        prefix + "max_aff_slots" + suffix + NStr::NumericToString(max_aff_slots) + separator +
        prefix + "aff_slots_used" + suffix + NStr::NumericToString(aff_slots_used) + separator +
        prefix + "clients" + suffix + NStr::NumericToString(clients) + separator +
        prefix + "groups" + suffix + NStr::NumericToString(groups) + separator +
        prefix + "gc_backlog" + suffix + NStr::NumericToString(gc_backlog) + separator +
        prefix + "notif_count" + suffix + NStr::NumericToString(notif_count) + separator;
    }

    result +=
    prefix + "delete_request" + suffix + NStr::BoolToString(delete_request) + separator +
    prefix + "timeout" + suffix + NStr::NumericToString(timeout.Sec()) + separator +
    prefix + "notif_hifreq_interval" + suffix + NS_FormatPreciseTimeAsSec(notif_hifreq_interval) + separator +
    prefix + "notif_hifreq_period" + suffix + NS_FormatPreciseTimeAsSec(notif_hifreq_period) + separator +
    prefix + "notif_lofreq_mult" + suffix + NStr::NumericToString(notif_lofreq_mult) + separator +
    prefix + "notif_handicap" + suffix + NS_FormatPreciseTimeAsSec(notif_handicap) + separator +
    prefix + "dump_buffer_size" + suffix + NStr::NumericToString(dump_buffer_size) + separator +
    prefix + "run_timeout" + suffix + NS_FormatPreciseTimeAsSec(run_timeout) + separator +
    prefix + "failed_retries" + suffix + NStr::NumericToString(failed_retries) + separator +
    prefix + "blacklist_time" + suffix + NS_FormatPreciseTimeAsSec(blacklist_time) + separator +
    prefix + "max_input_size" + suffix + NStr::NumericToString(max_input_size) + separator +
    prefix + "max_output_size" + suffix + NStr::NumericToString(max_output_size) + separator +
    prefix + "wnode_timeout" + suffix + NS_FormatPreciseTimeAsSec(wnode_timeout) + separator +
    prefix + "pending_timeout" + suffix + NS_FormatPreciseTimeAsSec(pending_timeout) + separator +
    prefix + "max_pending_wait_timeout" + suffix + NS_FormatPreciseTimeAsSec(max_pending_wait_timeout) + separator +
    prefix + "run_timeout_precision" + suffix + NS_FormatPreciseTimeAsSec(run_timeout_precision) + separator;

    if (url_encoded) {
        result +=
        prefix + "program_name" + suffix + NStr::URLEncode(program_name) + separator +
        prefix + "subm_hosts" + suffix + NStr::URLEncode(subm_hosts) + separator +
        prefix + "wnode_hosts" + suffix + NStr::URLEncode(wnode_hosts) + separator +
        prefix + "description" + suffix + NStr::URLEncode(description);
    } else {
        result +=
        prefix + "program_name" + suffix + NStr::PrintableString(program_name) + separator +
        prefix + "subm_hosts" + suffix + NStr::PrintableString(subm_hosts) + separator +
        prefix + "wnode_hosts" + suffix + NStr::PrintableString(wnode_hosts) + separator +
        prefix + "description" + suffix + NStr::PrintableString(description);
    }

    return result;
}


CNSPreciseTime
SQueueParameters::ReadTimeout(const IRegistry &  reg,
                              const string &     sname)
{
    double  val = GetDoubleNoErr("timeout",
                                 double(default_timeout));

    if (val <= 0)
        return default_timeout;
    return CNSPreciseTime(val);
}

CNSPreciseTime
SQueueParameters::ReadNotifHifreqInterval(const IRegistry &  reg,
                                          const string &     sname)
{
    double  val = GetDoubleNoErr("notif_hifreq_interval",
                                 double(default_notif_hifreq_interval));
    val = (int(val * 10)) / 10.0;
    if (val <= 0)
        return default_notif_hifreq_interval;
    return CNSPreciseTime(val);
}

CNSPreciseTime
SQueueParameters::ReadNotifHifreqPeriod(const IRegistry &  reg,
                                        const string &     sname)
{
    double  val = GetDoubleNoErr("notif_hifreq_period",
                                 double(default_notif_hifreq_period));
    if (val <= 0)
        return default_notif_hifreq_period;
    return CNSPreciseTime(val);
}

unsigned int
SQueueParameters::ReadNotifLofreqMult(const IRegistry &  reg,
                                      const string &     sname)
{
    unsigned int    val = GetIntNoErr("notif_lofreq_mult",
                                      default_notif_lofreq_mult);
    if (val <= 0)
        return default_notif_lofreq_mult;
    return val;
}

CNSPreciseTime
SQueueParameters::ReadNotifHandicap(const IRegistry &  reg,
                                    const string &     sname)
{
    double  val = GetDoubleNoErr("notif_handicap",
                                 double(default_notif_handicap));
    if (val <= 0)
        return default_notif_handicap;
    return CNSPreciseTime(val);
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

CNSPreciseTime
SQueueParameters::ReadRunTimeout(const IRegistry &  reg,
                                 const string &     sname)
{
    double  val = GetDoubleNoErr("run_timeout",
                                 double(default_run_timeout));
    if (val < 0)
        return default_run_timeout;
    return CNSPreciseTime(val);
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

CNSPreciseTime
SQueueParameters::ReadBlacklistTime(const IRegistry &  reg,
                                    const string &     sname)
{
    double  val = GetDoubleNoErr("blacklist_time",
                                 double(default_blacklist_time));
    if (val < 0)
        return default_blacklist_time;
    return CNSPreciseTime(val);
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

CNSPreciseTime
SQueueParameters::ReadWnodeTimeout(const IRegistry &  reg,
                                   const string &     sname)
{
    double  val = GetDoubleNoErr("wnode_timeout",
                                 double(default_wnode_timeout));
    if (val <= 0)
        return default_wnode_timeout;
    return CNSPreciseTime(val);
}

CNSPreciseTime
SQueueParameters::ReadPendingTimeout(const IRegistry &  reg,
                                     const string &     sname)
{
    double  val = GetDoubleNoErr("pending_timeout",
                                 double(default_pending_timeout));
    if (val <= 0)
        return default_pending_timeout;
    return CNSPreciseTime(val);
}

CNSPreciseTime
SQueueParameters::ReadMaxPendingWaitTimeout(const IRegistry &  reg,
                                            const string &     sname)
{
    double  val = GetDoubleNoErr("max_pending_wait_timeout",
                                 double(default_max_pending_wait_timeout));
    if (val < 0)
        val = 0;
    return CNSPreciseTime(val);
}

string
SQueueParameters::ReadDescription(const IRegistry &  reg,
                                  const string &     sname)
{
    return reg.GetString(sname, "description", kEmptyStr);
}


CNSPreciseTime
SQueueParameters::ReadRunTimeoutPrecision(const IRegistry &  reg,
                                          const string &     sname)
{
    double  val = GetDoubleNoErr("run_timeout_precision",
                                 double(default_run_timeout_precision));
    if (val < 0)
        val = 3.0;
    return CNSPreciseTime(val);
}

END_NCBI_SCOPE

