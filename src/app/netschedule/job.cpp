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
* File Description:
*   NetSchedule job
*
*/

#include <ncbi_pch.hpp>

#include <string.h>

#include <connect/services/netschedule_api_expt.hpp>

#include "job.hpp"
#include "ns_queue.hpp"
#include "ns_handler.hpp"
#include "ns_command_arguments.hpp"
#include "ns_affinity.hpp"
#include "ns_group.hpp"
#include "ns_db_dump.hpp"


BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////
// CJobEvent implementation

static string   s_EventAsString[] = {
    "Submit",           // eSubmit
    "BatchSubmit",      // eBatchSubmit
    "Request",          // eRequest
    "Done",             // eDone
    "Return",           // eReturn
    "Fail",             // eFail
    "FinalFail",        // eFinalFail
    "Read",             // eRead
    "ReadFail",         // eReadFail
    "ReadFinalFail",    // eReadFinalFail
    "ReadDone",         // eReadDone
    "ReadRollback",     // eReadRollback
    "Clear",            // eClear
    "Cancel",           // eCancel
    "Timeout",          // eTimeout
    "ReadTimeout",      // eReadTimeout
    "SessionChanged",   // eSessionChanged
    "NSSubmitRollback", // eNSSubmitRollback
    "NSGetRollback",    // eNSGetRollback
    "NSReadRollback",   // eNSReadRollback
    "ReturnNoBlacklist",// eReturnNoBlacklist
    "Reschedule",       // eReschedule
    "Redo",             // eRedo
    "Reread"            // eReread
};


string CJobEvent::EventToString(EJobEvent  event)
{
    if (event < eSubmit || event > eReread)
        return "UNKNOWN";

    return s_EventAsString[ event ];
}


CJobEvent::CJobEvent() :
    m_Status(CNetScheduleAPI::eJobNotFound),
    m_Event(eUnknown),
    m_Timestamp(0, 0),
    m_NodeAddr(0),
    m_RetCode(0)
{}


CNSPreciseTime
GetJobExpirationTime(const CNSPreciseTime &     last_touch,
                     TJobStatus                 status,
                     const CNSPreciseTime &     job_submit_time,
                     const CNSPreciseTime &     job_timeout,
                     const CNSPreciseTime &     job_run_timeout,
                     const CNSPreciseTime &     job_read_timeout,
                     const CNSPreciseTime &     queue_timeout,
                     const CNSPreciseTime &     queue_run_timeout,
                     const CNSPreciseTime &     queue_read_timeout,
                     const CNSPreciseTime &     queue_pending_timeout,
                     const CNSPreciseTime &     event_time)
{
    CNSPreciseTime  last_update = event_time;
    if (last_update == kTimeZero)
        last_update = last_touch;

    if (status == CNetScheduleAPI::eRunning) {
        if (job_run_timeout != kTimeZero)
            return last_update + job_run_timeout;
        return last_update + queue_run_timeout;
    }

    if (status == CNetScheduleAPI::eReading) {
        if (job_read_timeout != kTimeZero)
            return last_update + job_read_timeout;
        return last_update + queue_read_timeout;
    }

    if (status == CNetScheduleAPI::ePending) {
        CNSPreciseTime  regular_expiration = last_update +
                                             queue_timeout;
        if (job_timeout != kTimeZero)
            regular_expiration = last_update + job_timeout;
        CNSPreciseTime  pending_expiration = job_submit_time +
                                             queue_pending_timeout;

        if (regular_expiration < pending_expiration)
            return regular_expiration;
        return pending_expiration;
    }

    if (job_timeout != kTimeZero)
        return last_update + job_timeout;
    return last_update + queue_timeout;
}




//////////////////////////////////////////////////////////////////////////
// CJob implementation

CJob::CJob() :
    m_Id(0),
    m_Passport(0),
    m_Status(CNetScheduleAPI::ePending),
    m_Timeout(),
    m_RunTimeout(),
    m_ReadTimeout(),
    m_SubmNotifPort(0),
    m_SubmNotifTimeout(),
    m_ListenerNotifAddress(0),
    m_ListenerNotifPort(0),
    m_ListenerNotifAbsTime(),
    m_RunCount(0),
    m_ReadCount(0),
    m_AffinityId(0),
    m_Mask(0),
    m_GroupId(0),
    m_LastTouch(),
    m_NeedSubmProgressMsgNotif(false),
    m_NeedLsnrProgressMsgNotif(false),
    m_NeedStolenNotif(false)
{}


CJob::CJob(const SNSCommandArguments &  request) :
    m_Id(0),
    m_Passport(0),
    m_Status(CNetScheduleAPI::ePending),
    m_Timeout(),
    m_RunTimeout(),
    m_ReadTimeout(),
    m_SubmNotifPort(request.port),
    m_SubmNotifTimeout(request.timeout, 0),
    m_ListenerNotifAddress(0),
    m_ListenerNotifPort(0),
    m_ListenerNotifAbsTime(),
    m_RunCount(0),
    m_ReadCount(0),
    m_ProgressMsg(""),
    m_AffinityId(0),
    m_Mask(request.job_mask),
    m_GroupId(0),
    m_LastTouch(),
    m_Output(""),
    m_NeedSubmProgressMsgNotif(request.need_progress_msg),
    m_NeedLsnrProgressMsgNotif(false),
    m_NeedStolenNotif(false)
{
    SetClientIP(request.ip);
    SetClientSID(request.sid);
    SetNCBIPHID(request.ncbi_phid);
    SetInput(request.input);
}


string CJob::GetErrorMsg() const
{
    if (m_Events.empty())
        return kEmptyStr;
    return GetLastEvent()->GetErrorMsg();
}


int    CJob::GetRetCode() const
{
    if (m_Events.empty())
        return ~0;
    return GetLastEvent()->GetRetCode();
}


void CJob::SetInput(const string &  input)
{
    m_Input = input;
}


void CJob::SetOutput(const string &  output)
{
    m_Output = output;
}


CJobEvent &  CJob::AppendEvent()
{
    m_Events.push_back(CJobEvent());
    return m_Events[m_Events.size() - 1];
}


const CJobEvent *  CJob::GetLastEvent() const
{
    // The first event is attached when a job is submitted, so
    // there is no way to have the events empty
    return &(m_Events[m_Events.size() - 1]);
}


CJobEvent *  CJob::GetLastEvent()
{
    // The first event is attached when a job is submitted, so
    // there is no way to have the events empty
    return &(m_Events[m_Events.size() - 1]);
}


CJob::EAuthTokenCompareResult
CJob::CompareAuthToken(const string &  auth_token) const
{
    vector<string>      parts;

    NStr::Split(auth_token, "_", parts);
    if (parts.size() < 2)
        return eInvalidTokenFormat;

    try {
        if (NStr::StringToUInt(parts[0]) != m_Passport)
            return eNoMatch;
        if (NStr::StringToSizet(parts[1]) != m_Events.size())
            return ePassportOnlyMatch;
    } catch (...) {
        // Cannot convert the value
        return eInvalidTokenFormat;
    }
    return eCompleteMatch;
}


bool CJob::ShouldNotifySubmitter(const CNSPreciseTime &  current_time) const
{
    // The very first event is always a submit
    if (m_SubmNotifTimeout && m_SubmNotifPort)
        return m_Events[0].m_Timestamp +
               CNSPreciseTime(m_SubmNotifTimeout, 0) >= current_time;
    return false;
}


bool CJob::ShouldNotifyListener(const CNSPreciseTime &  current_time) const
{
    if (m_ListenerNotifAddress != 0 && m_ListenerNotifPort != 0)
        return m_ListenerNotifAbsTime >= current_time;
    return false;
}


// Used to DUMP a job
string CJob::Print(TDumpFields                  dump_fields,
                   const CQueue &               queue,
                   const CNSAffinityRegistry &  aff_registry,
                   const CNSGroupsRegistry &    group_registry) const
{
    CNSPreciseTime  timeout = m_Timeout;
    CNSPreciseTime  run_timeout = m_RunTimeout;
    CNSPreciseTime  read_timeout = m_ReadTimeout;
    CNSPreciseTime  pending_timeout = queue.GetPendingTimeout();
    CNSPreciseTime  queue_timeout = queue.GetTimeout();
    CNSPreciseTime  queue_run_timeout = queue.GetRunTimeout();
    CNSPreciseTime  queue_read_timeout = queue.GetReadTimeout();
    string          result;

    result.reserve(2048);   // Voluntary; must be enough for most of the cases

    if (m_Timeout == kTimeZero)
        timeout = queue_timeout;
    if (m_RunTimeout == kTimeZero)
        run_timeout = queue_run_timeout;
    if (m_ReadTimeout == kTimeZero)
        read_timeout = queue_read_timeout;

    CNSPreciseTime  exp_time;
    if (m_Status == CNetScheduleAPI::eRunning ||
        m_Status == CNetScheduleAPI::eReading)
        exp_time = GetExpirationTime(queue_timeout,
                                     queue_run_timeout,
                                     queue_read_timeout,
                                     pending_timeout,
                                     GetLastEventTime());
    else
        exp_time = GetExpirationTime(queue_timeout,
                                     queue_run_timeout,
                                     queue_read_timeout,
                                     pending_timeout,
                                     m_LastTouch);

    if (dump_fields & eId)
        x_AppendId(result);
    if (dump_fields & eKey)
        x_AppendKey(queue, result);
    if (dump_fields & eStatus)
        x_AppendStatus(result);
    if (dump_fields & eLastTouch)
        x_AppendLastTouch(result);
    if (dump_fields & eEraseTime)
        x_AppendEraseTime(timeout, pending_timeout, exp_time, result);
    if (dump_fields & eRunExpiration)
        x_AppendRunExpiration(run_timeout, exp_time, result);
    if (dump_fields & eReadExpiration)
        x_AppendReadExpiration(read_timeout, exp_time, result);
    if (dump_fields & eSubmitNotifPort)
        x_AppendSubmitNotifPort(result);
    if (dump_fields & eSubmitNotifExpiration)
        x_AppendSubmitNotifExpiration(result);
    if (dump_fields & eListenerNotif)
        x_AppendListenerNotif(result);
    if (dump_fields & eListenerNotifExpiration)
        x_AppendListenerNotifExpiration(result);
    if (dump_fields & eEvents)
        x_AppendEvents(result);
    if (dump_fields & eRunCounter)
        x_AppendRunCounter(result);
    if (dump_fields & eReadCounter)
        x_AppendReadCounter(result);
    if (dump_fields & eAffinity)
        x_AppendAffinity(aff_registry, result);
    if (dump_fields & eGroup)
        x_AppendGroup(group_registry, result);
    if (dump_fields & eMask)
        x_AppendMask(result);
    if (dump_fields & eInput)
        x_AppendInput(result);
    if (dump_fields & eOutput)
        x_AppendOutput(result);
    if (dump_fields & eProgressMsg)
        x_AppendProgressMsg(result);
    if (dump_fields & eRemoteClientSID)
        x_AppendRemoteClientSID(result);
    if (dump_fields & eRemoteClientIP)
        x_AppendRemoteClientIP(result);
    if (dump_fields & eNcbiPHID)
        x_AppendNcbiPhid(result);
    if (dump_fields & eNeedSubmitProgressMsgNotif)
        x_AppendNeedSubmitProgressMsgNotif(result);
    if (dump_fields & eNeedListenerProgressMsgNotif)
        x_AppendNeedListenerProgressMsgNotif(result);
    if (dump_fields & eNeedStolenNotif)
        x_AppendNeedStolenNotif(result);

    return result;
}

void CJob::x_AppendId(string & dump) const
{
    static string   prefix = "OK:id: ";
    dump.append(prefix)
        .append(to_string(m_Id))
        .append(kNewLine);
}

void CJob::x_AppendKey(const CQueue & queue, string & dump) const
{
    static string   prefix = "OK:key: ";
    dump.append(prefix)
        .append(queue.MakeJobKey(m_Id))
        .append(kNewLine);
}

void CJob::x_AppendStatus(string & dump) const
{
    static string   prefix = "OK:status: ";
    dump.append(prefix)
        .append(CNetScheduleAPI::StatusToString(m_Status))
        .append(kNewLine);
}

void CJob::x_AppendLastTouch(string & dump) const
{
    static string   prefix = "OK:last_touch: ";
    dump.append(prefix)
        .append(NS_FormatPreciseTime(m_LastTouch))
        .append(kNewLine);
}

void CJob::x_AppendEraseTime(const CNSPreciseTime & timeout,
                             const CNSPreciseTime & pending_timeout,
                             const CNSPreciseTime & exp_time,
                             string & dump) const
{
    static string   prefix = "OK:erase_time: ";
    static string   na_prefix = prefix + "n/a (timeout: ";
    static string   timeout_suffix = " (timeout: ";
    static string   postfix = " sec)" + kNewLine;
    static string   pending_suffix = " sec, pending timeout: ";
    if (m_Status == CNetScheduleAPI::eRunning || m_Status == CNetScheduleAPI::eReading)
        dump.append(na_prefix)
            .append(NS_FormatPreciseTimeAsSec(timeout))
            .append(pending_suffix)
            .append(NS_FormatPreciseTimeAsSec(pending_timeout))
            .append(postfix);
    else
        dump.append(prefix)
            .append(NS_FormatPreciseTime(exp_time))
            .append(timeout_suffix)
            .append(NS_FormatPreciseTimeAsSec(timeout))
            .append(pending_suffix)
            .append(NS_FormatPreciseTimeAsSec(pending_timeout))
            .append(postfix);
}

void CJob::x_AppendRunExpiration(const CNSPreciseTime & run_timeout,
                                 const CNSPreciseTime & exp_time,
                                 string & dump) const
{
    static string   prefix = "OK:run_expiration: ";
    static string   na_prefix = prefix + "n/a (timeout: ";
    static string   suffix = " (timeout: ";
    static string   postfix = " sec)" + kNewLine;
    if (m_Status == CNetScheduleAPI::eRunning)
        dump.append(prefix)
            .append(NS_FormatPreciseTime(exp_time))
            .append(suffix)
            .append(NS_FormatPreciseTimeAsSec(run_timeout))
            .append(postfix);
    else
        dump.append(na_prefix)
            .append(NS_FormatPreciseTimeAsSec(run_timeout))
            .append(postfix);
}

void CJob::x_AppendReadExpiration(const CNSPreciseTime & read_timeout,
                                  const CNSPreciseTime & exp_time,
                                  string & dump) const
{
    static string   prefix = "OK:read_expiration: ";
    static string   na_prefix = prefix + "n/a (timeout: ";
    static string   suffix = " (timeout: ";
    static string   postfix = " sec)" + kNewLine;
    if (m_Status == CNetScheduleAPI::eReading)
        dump.append(prefix)
            .append(NS_FormatPreciseTime(exp_time))
            .append(suffix)
            .append(NS_FormatPreciseTimeAsSec(read_timeout))
            .append(postfix);
    else
        dump.append(na_prefix)
            .append(NS_FormatPreciseTimeAsSec(read_timeout))
            .append(postfix);
}

void CJob::x_AppendSubmitNotifPort(string & dump) const
{
    static string   prefix = "OK:subm_notif_port: ";
    static string   na_reply = prefix + "n/a" + kNewLine;
    if (m_SubmNotifPort != 0)
        dump.append(prefix)
            .append(to_string(m_SubmNotifPort))
            .append(kNewLine);
    else
        dump.append(na_reply);
}

void CJob::x_AppendSubmitNotifExpiration(string & dump) const
{
    static string   prefix = "OK:subm_notif_expiration: ";
    static string   suffix = " (timeout: ";
    static string   postfix = " sec)" + kNewLine;
    static string   na_reply = prefix + "n/a" + kNewLine;
    if (m_SubmNotifTimeout != kTimeZero)
        dump.append(prefix)
            .append(NS_FormatPreciseTime(m_Events[0].m_Timestamp + m_SubmNotifTimeout))
            .append(suffix)
            .append(NS_FormatPreciseTimeAsSec(m_SubmNotifTimeout))
            .append(postfix);
    else
        dump.append(na_reply);
}

void CJob::x_AppendListenerNotif(string & dump) const
{
    static string   prefix = "OK:listener_notif: ";
    static string   na_reply = prefix + "n/a" + kNewLine;
    static string   colon = ":";
    if (m_ListenerNotifAddress == 0 || m_ListenerNotifPort == 0)
        dump.append(na_reply);
    else
        dump.append(prefix)
            .append(CSocketAPI::gethostbyaddr(m_ListenerNotifAddress))
            .append(colon)
            .append(to_string(m_ListenerNotifPort))
            .append(kNewLine);
}

void CJob::x_AppendListenerNotifExpiration(string & dump) const
{
    static string   prefix = "OK:listener_notif_expiration: ";
    static string   na_reply = prefix + "n/a" + kNewLine;
    if (m_ListenerNotifAbsTime != kTimeZero)
        dump.append(prefix)
            .append(NS_FormatPreciseTime(m_ListenerNotifAbsTime))
            .append(kNewLine);
    else
        dump.append(na_reply);
}

void CJob::x_AppendEvents(string & dump) const
{
    static string       prefix = "OK:event";
    static string       client = ": client=";
    static string       ns = "ns";
    static string       event = " event=";
    static string       status = " status=";
    static string       ret_code = " ret_code=";
    static string       timestamp = " timestamp=";
    static string       na = "n/a ";
    static string       node = "' node='";
    static string       session = "' session='";
    static string       err_msg = "' err_msg=";
    int                 event_no = 1;

    for (const auto &  ev : m_Events) {
        unsigned int    addr = ev.GetNodeAddr();

        dump.append(prefix)
            .append(to_string(event_no))
            .append(client);
        if (addr == 0)
            dump.append(ns);
        else
            dump.append(CSocketAPI::gethostbyaddr(addr));

        dump.append(event)
            .append(CJobEvent::EventToString(ev.GetEvent()))
            .append(status)
            .append(CNetScheduleAPI::StatusToString(ev.GetStatus()))
            .append(ret_code)
            .append(to_string(ev.GetRetCode()))
            .append(timestamp);

        CNSPreciseTime  start = ev.GetTimestamp();
        if (start == kTimeZero)
            dump.append(na);
        else
            dump.append(1, '\'')
                .append(NS_FormatPreciseTime(start))
                .append(node)
                .append(ev.GetClientNode())
                .append(session)
                .append(ev.GetClientSession())
                .append(err_msg)
                .append(ev.GetQuotedErrorMsg())
                .append(kNewLine);
        ++event_no;
    }
}

void CJob::x_AppendRunCounter(string & dump) const
{
    static string   prefix = "OK:run_counter: ";
    dump.append(prefix)
        .append(to_string(m_RunCount))
        .append(kNewLine);
}

void CJob::x_AppendReadCounter(string & dump) const
{
    static string   prefix = "OK:read_counter: ";
    dump.append(prefix)
        .append(to_string(m_ReadCount))
        .append(kNewLine);
}

void CJob::x_AppendAffinity(const CNSAffinityRegistry &  aff_registry,
                            string & dump) const
{
    static string   prefix = "OK:affinity: ";
    static string   na_reply = prefix + "n/a" + kNewLine;
    static string   open_paren = " ('";
    static string   close_paren = "')";
    if (m_AffinityId != 0)
        dump.append(prefix)
            .append(to_string(m_AffinityId))
            .append(open_paren)
            .append(NStr::PrintableString(aff_registry.GetTokenByID(m_AffinityId)))
            .append(close_paren)
            .append(kNewLine);
    else
        dump.append(na_reply);
}

void CJob::x_AppendGroup(const CNSGroupsRegistry & group_registry,
                         string & dump) const
{
    static string   prefix = "OK:group: ";
    static string   na_reply = prefix + "n/a" + kNewLine;
    static string   open_paren = " ('";
    static string   close_paren = "')";
    if (m_GroupId != 0)
        dump.append(prefix)
            .append(to_string(m_GroupId))
            .append(open_paren)
            .append(NStr::PrintableString(group_registry.ResolveGroup(m_GroupId)))
            .append(close_paren)
            .append(kNewLine);
    else
        dump.append(na_reply);
}

void CJob::x_AppendMask(string & dump) const
{
    static string   prefix = "OK:mask: ";
    dump.append(prefix)
        .append(to_string(m_Mask))
        .append(kNewLine);
}

void CJob::x_AppendInput(string & dump) const
{
    static string   prefix = "OK:input: '";
    dump.append(prefix)
        .append(NStr::PrintableString(m_Input))
        .append(1, '\'')
        .append(kNewLine);
}

void CJob::x_AppendOutput(string & dump) const
{
    static string   prefix = "OK:output: '";
    dump.append(prefix)
        .append(NStr::PrintableString(m_Output))
        .append(1, '\'')
        .append(kNewLine);
}

void CJob::x_AppendProgressMsg(string & dump) const
{
    static string   prefix = "OK:progress_msg: '";
    dump.append(prefix)
        .append(m_ProgressMsg)
        .append(1, '\'')
        .append(kNewLine);
}

void CJob::x_AppendRemoteClientSID(string & dump) const
{
    static string   prefix = "OK:remote_client_sid: ";
    dump.append(prefix)
        .append(NStr::PrintableString(m_ClientSID))
        .append(kNewLine);
}

void CJob::x_AppendRemoteClientIP(string & dump) const
{
    static string   prefix = "OK:remote_client_ip: ";
    dump.append(prefix)
        .append(NStr::PrintableString(m_ClientIP))
        .append(kNewLine);
}

void CJob::x_AppendNcbiPhid(string & dump) const
{
    static string   prefix = "OK:ncbi_phid: ";
    dump.append(prefix)
        .append(NStr::PrintableString(m_NCBIPHID))
        .append(kNewLine);
}

void CJob::x_AppendNeedSubmitProgressMsgNotif(string & dump) const
{
    static string   prefix = "OK:need_subm_progress_msg_notif: ";
    dump.append(prefix)
        .append(NStr::BoolToString(m_NeedSubmProgressMsgNotif))
        .append(kNewLine);
}

void CJob::x_AppendNeedListenerProgressMsgNotif(string & dump) const
{
    static string   prefix = "OK:need_lsnr_progress_msg_notif: ";
    dump.append(prefix)
        .append(NStr::BoolToString(m_NeedLsnrProgressMsgNotif))
        .append(kNewLine);
}

void CJob::x_AppendNeedStolenNotif(string & dump) const
{
    static string   prefix = "OK:need_stolen_notif: ";
    dump.append(prefix)
        .append(NStr::BoolToString(m_NeedStolenNotif))
        .append(kNewLine);
}


TJobStatus  CJob::GetStatusBeforeReading(void) const
{
    ssize_t     index = m_Events.size() - 1;
    while (index >= 0) {
        if (m_Events[index].GetStatus() == CNetScheduleAPI::eReading)
            break;
        --index;
    }

    --index;
    if (index < 0)
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "inconsistency in the job history. "
                   "No reading status found or no event before reading.");
    return m_Events[index].GetStatus();
}


void CJob::Dump(FILE *  jobs_file) const
{
    // Fill in the job dump structure
    SJobDump        job_dump;

    job_dump.id = m_Id;
    job_dump.passport = m_Passport;
    job_dump.status = (int) m_Status;
    job_dump.timeout = (double)m_Timeout;
    job_dump.run_timeout = (double)m_RunTimeout;
    job_dump.read_timeout = (double)m_ReadTimeout;
    job_dump.subm_notif_port = m_SubmNotifPort;
    job_dump.subm_notif_timeout = (double)m_SubmNotifTimeout;
    job_dump.listener_notif_addr = m_ListenerNotifAddress;
    job_dump.listener_notif_port = m_ListenerNotifPort;
    job_dump.listener_notif_abstime = (double)m_ListenerNotifAbsTime;
    job_dump.need_subm_progress_msg_notif = m_NeedSubmProgressMsgNotif;
    job_dump.need_lsnr_progress_msg_notif = m_NeedLsnrProgressMsgNotif;
    job_dump.need_stolen_notif = m_NeedStolenNotif;
    job_dump.run_counter = m_RunCount;
    job_dump.read_counter = m_ReadCount;
    job_dump.aff_id = m_AffinityId;
    job_dump.mask = m_Mask;
    job_dump.group_id = m_GroupId;
    job_dump.last_touch = (double)m_LastTouch;
    job_dump.progress_msg_size = m_ProgressMsg.size();
    job_dump.number_of_events = m_Events.size();

    job_dump.client_ip_size = m_ClientIP.size();
    memcpy(job_dump.client_ip, m_ClientIP.data(),
           min(static_cast<size_t>(kMaxClientIpSize),
               m_ClientIP.size()));
    job_dump.client_sid_size = m_ClientSID.size();
    memcpy(job_dump.client_sid, m_ClientSID.data(),
           min(static_cast<size_t>(kMaxSessionIdSize),
               m_ClientSID.size()));
    job_dump.ncbi_phid_size = m_NCBIPHID.size();
    memcpy(job_dump.ncbi_phid, m_NCBIPHID.data(),
           min(static_cast<size_t>(kMaxHitIdSize),
               m_NCBIPHID.size()));

    try {
        job_dump.Write(jobs_file, m_ProgressMsg.data());
    } catch (const exception &  ex) {
        throw runtime_error("Writing error while dumping a job properties: " +
                            string(ex.what()));
    }

    // Fill in the events structure
    for (vector<CJobEvent>::const_iterator it = m_Events.begin();
         it != m_Events.end(); ++it) {
        const CJobEvent &   event = *it;
        SJobEventsDump      events_dump;

        events_dump.event = int(event.m_Event);
        events_dump.status = int(event.m_Status);
        events_dump.timestamp = (double)event.m_Timestamp;
        events_dump.node_addr = event.m_NodeAddr;
        events_dump.ret_code = event.m_RetCode;
        events_dump.client_node_size = event.m_ClientNode.size();
        events_dump.client_session_size = event.m_ClientSession.size();
        events_dump.err_msg_size = event.m_ErrorMsg.size();

        try {
            events_dump.Write(jobs_file, event.m_ClientNode.data(),
                                         event.m_ClientSession.data(),
                                         event.m_ErrorMsg.data());
        } catch (const exception &  ex) {
            throw runtime_error("Writing error while dumping a job events: " +
                                string(ex.what()));
        }
    }

    // Fill in the job input/output structure
    SJobIODump      job_io_dump;

    job_io_dump.input_size = m_Input.size();
    job_io_dump.output_size = m_Output.size();

    try {
        job_io_dump.Write(jobs_file, m_Input.data(), m_Output.data());
    } catch (const exception &  ex) {
        throw runtime_error("Writing error while dumping a job "
                            "input/output: " + string(ex.what()));
    }
}


// true => job loaded
// false => EOF
// exception => reading problem
bool CJob::LoadFromDump(FILE *  jobs_file,
                        char *  input_buf,
                        char *  output_buf,
                        const SJobDumpHeader &  header)
{
    SJobDump        job_dump;
    char            progress_msg_buf[kNetScheduleMaxDBDataSize];

    if (job_dump.Read(jobs_file, header.job_props_fixed_size,
                      progress_msg_buf) == 1)
        return false;

    // Fill in the job fields
    m_Id = job_dump.id;
    m_Passport = job_dump.passport;
    m_Status = (TJobStatus)job_dump.status;
    m_Timeout = CNSPreciseTime(job_dump.timeout);
    m_RunTimeout = CNSPreciseTime(job_dump.run_timeout);
    m_ReadTimeout = CNSPreciseTime(job_dump.read_timeout);
    m_SubmNotifPort = job_dump.subm_notif_port;
    m_SubmNotifTimeout = CNSPreciseTime(job_dump.subm_notif_timeout);
    m_ListenerNotifAddress = job_dump.listener_notif_addr;
    m_ListenerNotifPort = job_dump.listener_notif_port;
    m_ListenerNotifAbsTime = CNSPreciseTime(job_dump.listener_notif_abstime);
    m_NeedSubmProgressMsgNotif = job_dump.need_subm_progress_msg_notif;
    m_NeedLsnrProgressMsgNotif = job_dump.need_lsnr_progress_msg_notif;
    m_NeedStolenNotif = job_dump.need_stolen_notif;
    m_RunCount = job_dump.run_counter;
    m_ReadCount = job_dump.read_counter;
    m_ProgressMsg.clear();
    if (job_dump.progress_msg_size > 0)
        m_ProgressMsg = string(progress_msg_buf, job_dump.progress_msg_size);
    m_AffinityId = job_dump.aff_id;
    m_Mask = job_dump.mask;
    m_GroupId = job_dump.group_id;
    m_LastTouch = CNSPreciseTime(job_dump.last_touch);
    m_ClientIP = string(job_dump.client_ip, min(job_dump.client_ip_size,
                                                kMaxClientIpSize));
    m_ClientSID = string(job_dump.client_sid, min(job_dump.client_sid_size,
                                                  kMaxSessionIdSize));
    m_NCBIPHID = string(job_dump.ncbi_phid, min(job_dump.ncbi_phid_size,
                                                kMaxHitIdSize));

    // Read the job events
    m_Events.clear();
    SJobEventsDump  event_dump;
    char            client_node_buf[kMaxWorkerNodeIdSize];
    char            client_session_buf[kMaxWorkerNodeIdSize];
    char            err_msg_buf[kNetScheduleMaxDBErrSize];
    for (size_t  k = 0; k < job_dump.number_of_events; ++k) {
        if (event_dump.Read(jobs_file,
                            header.job_event_fixed_size,
                            client_node_buf, client_session_buf,
                            err_msg_buf) != 0)
            throw runtime_error("Unexpected end of the dump file. "
                                "Cannot read expected job events." );

        // Fill the event and append it to the job events
        CJobEvent       event;
        event.m_Status = (TJobStatus)event_dump.status;
        event.m_Event = (CJobEvent::EJobEvent)event_dump.event;
        event.m_Timestamp = CNSPreciseTime(event_dump.timestamp);
        event.m_NodeAddr = event_dump.node_addr;
        event.m_RetCode = event_dump.ret_code;
        event.m_ClientNode.clear();
        if (event_dump.client_node_size > 0)
            event.m_ClientNode = string(client_node_buf,
                                        event_dump.client_node_size);
        event.m_ClientSession.clear();
        if (event_dump.client_session_size > 0)
            event.m_ClientSession = string(client_session_buf,
                                           event_dump.client_session_size);
        event.m_ErrorMsg.clear();
        if (event_dump.err_msg_size > 0)
            event.m_ErrorMsg = string(err_msg_buf,
                                      event_dump.err_msg_size);

        m_Events.push_back(event);
    }


    // Read the job input/output
    SJobIODump  io_dump;

    if (io_dump.Read(jobs_file, header.job_io_fixed_size,
                     input_buf, output_buf) != 0)
        throw runtime_error("Unexpected end of the dump file. "
                            "Cannot read expected job input/output." );

    SetInput(string(input_buf, io_dump.input_size));
    SetOutput(string(output_buf, io_dump.output_size));
    return true;
}



END_NCBI_SCOPE

