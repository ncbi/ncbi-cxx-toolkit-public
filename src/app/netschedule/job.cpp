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
        return "";
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
string CJob::Print(const CQueue &               queue,
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

    result = "OK:id: " + NStr::NumericToString(m_Id) + "\n"
             "OK:key: " + queue.MakeJobKey(m_Id) + "\n"
             "OK:status: " + CNetScheduleAPI::StatusToString(m_Status) + "\n"
             "OK:last_touch: " + NS_FormatPreciseTime(m_LastTouch) +"\n";

    if (m_Status == CNetScheduleAPI::eRunning ||
        m_Status == CNetScheduleAPI::eReading)
        result += "OK:erase_time: n/a (timeout: " +
                  NS_FormatPreciseTimeAsSec(timeout) +
                  " sec, pending timeout: " +
                  NS_FormatPreciseTimeAsSec(pending_timeout) + " sec)\n";
    else
        result += "OK:erase_time: " + NS_FormatPreciseTime(exp_time) +
                  " (timeout: " +
                  NS_FormatPreciseTimeAsSec(timeout) +
                  " sec, pending timeout: " +
                  NS_FormatPreciseTimeAsSec(pending_timeout) + " sec)\n";

    if (m_Status != CNetScheduleAPI::eRunning &&
        m_Status != CNetScheduleAPI::eReading) {
        result += "OK:run_expiration: n/a (timeout: " +
                  NS_FormatPreciseTimeAsSec(run_timeout) + " sec)\n"
                  "OK:read_expiration: n/a (timeout: " +
                  NS_FormatPreciseTimeAsSec(read_timeout) + " sec)\n";
    } else {
        if (m_Status == CNetScheduleAPI::eRunning) {
            result += "OK:run_expiration: " + NS_FormatPreciseTime(exp_time) +
                      " (timeout: " +
                      NS_FormatPreciseTimeAsSec(run_timeout) + " sec)\n"
                      "OK:read_expiration: n/a (timeout: " +
                      NS_FormatPreciseTimeAsSec(read_timeout) + " sec)\n";
        } else {
            // Reading job
            result += "OK:run_expiration: n/a (timeout: " +
                      NS_FormatPreciseTimeAsSec(run_timeout) + " sec)\n"
                      "OK:read_expiration: " + NS_FormatPreciseTime(exp_time) +
                      " (timeout: " +
                      NS_FormatPreciseTimeAsSec(read_timeout) + " sec)\n";
        }
    }

    if (m_SubmNotifPort != 0)
        result += "OK:subm_notif_port: " +
                  NStr::NumericToString(m_SubmNotifPort) + "\n";
    else
        result += "OK:subm_notif_port: n/a\n";

    if (m_SubmNotifTimeout != kTimeZero) {
        CNSPreciseTime  subm_exp_time = m_Events[0].m_Timestamp +
                                        m_SubmNotifTimeout;
        result += "OK:subm_notif_expiration: " +
                  NS_FormatPreciseTime(subm_exp_time) + " (timeout: " +
                  NS_FormatPreciseTimeAsSec(m_SubmNotifTimeout) + " sec)\n";
    }
    else
        result += "OK:subm_notif_expiration: n/a\n";

    if (m_ListenerNotifAddress == 0 || m_ListenerNotifPort == 0)
        result += "OK:listener_notif: n/a\n";
    else
        result += "OK:listener_notif: " +
                  CSocketAPI::gethostbyaddr(m_ListenerNotifAddress) + ":" +
                  NStr::NumericToString(m_ListenerNotifPort) + "\n";

    if (m_ListenerNotifAbsTime != kTimeZero)
        result += "OK:listener_notif_expiration: " +
                  NS_FormatPreciseTime(m_ListenerNotifAbsTime) + "\n";
    else
        result += "OK:listener_notif_expiration: n/a\n";


    // Print detailed information about the job events
    int                         event = 1;

    ITERATE(vector<CJobEvent>, it, m_Events) {
        unsigned int    addr = it->GetNodeAddr();

        result += "OK:event" + NStr::NumericToString(event++) + ": "
                  "client=";
        if (addr == 0)
            result += "ns ";
        else
            result += CSocketAPI::gethostbyaddr(addr) + " ";

        result += "event=" + CJobEvent::EventToString(it->GetEvent()) + " "
                  "status=" +
                  CNetScheduleAPI::StatusToString(it->GetStatus()) + " "
                  "ret_code=" + NStr::NumericToString(it->GetRetCode()) + " ";

        // Time part
        result += "timestamp=";
        CNSPreciseTime  start = it->GetTimestamp();
        if (start == kTimeZero)
            result += "n/a ";
        else
            result += "'" + NS_FormatPreciseTime(start) + "' ";

        // The rest
        result += "node='" + it->GetClientNode() + "' "
                  "session='" + it->GetClientSession() + "' "
                  "err_msg=" + it->GetQuotedErrorMsg() + "\n";
    }

    result += "OK:run_counter: " + NStr::NumericToString(m_RunCount) + "\n"
              "OK:read_counter: " + NStr::NumericToString(m_ReadCount) + "\n";

    if (m_AffinityId != 0)
        result += "OK:affinity: " + NStr::NumericToString(m_AffinityId) + " ('" +
                  NStr::PrintableString(aff_registry.GetTokenByID(m_AffinityId)) +
                  "')\n";
    else
        result += "OK:affinity: n/a\n";

    if (m_GroupId != 0)
        result += "OK:group: " + NStr::NumericToString(m_GroupId) + " ('" +
            NStr::PrintableString(group_registry.ResolveGroup(m_GroupId)) +
            "')\n";
    else
        result += "OK:group: n/a\n";

    result += "OK:mask: " + NStr::NumericToString(m_Mask) + "\n"
              "OK:input: '" + NStr::PrintableString(m_Input) + "'\n"
              "OK:output: '" + NStr::PrintableString(m_Output) + "'\n"
              "OK:progress_msg: '" + m_ProgressMsg + "'\n"
              "OK:remote_client_sid: " + NStr::PrintableString(m_ClientSID) + "\n"
              "OK:remote_client_ip: " + NStr::PrintableString(m_ClientIP) + "\n"
              "OK:ncbi_phid: " + NStr::PrintableString(m_NCBIPHID) + "\n"
              "OK:need_subm_progress_msg_notif: " +
                 NStr::BoolToString(m_NeedSubmProgressMsgNotif) + "\n"
              "OK:need_lsnr_progress_msg_notif: " +
                 NStr::BoolToString(m_NeedLsnrProgressMsgNotif) + "\n"
              "OK:need_stolen_notif: " +
                 NStr::BoolToString(m_NeedStolenNotif) + "\n";
    return result;
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
    memcpy(job_dump.client_ip, m_ClientIP.data(), m_ClientIP.size());
    job_dump.client_sid_size = m_ClientSID.size();
    memcpy(job_dump.client_sid, m_ClientSID.data(), m_ClientSID.size());
    job_dump.ncbi_phid_size = m_NCBIPHID.size();
    memcpy(job_dump.ncbi_phid, m_NCBIPHID.data(), m_NCBIPHID.size());

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
    m_ClientIP = string(job_dump.client_ip, job_dump.client_ip_size);
    m_ClientSID = string(job_dump.client_sid, job_dump.client_sid_size);
    m_NCBIPHID = string(job_dump.ncbi_phid, job_dump.ncbi_phid_size);

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

