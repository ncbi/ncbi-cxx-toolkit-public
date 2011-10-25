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

#include "job.hpp"
#include "ns_queue.hpp"
#include "ns_command_arguments.hpp"

BEGIN_NCBI_SCOPE

static const char*  kISO8601DateTime = 0;


static string FormatTime(time_t t)
{
    if (!t)
        return "NULL";

    if (!kISO8601DateTime) {
        const char *    format = CTimeFormat::GetPredefined(
                                        CTimeFormat::eISO8601_DateTimeSec)
                                            .GetString().c_str();

        kISO8601DateTime = new char[strlen(format)+1];
        strcpy(const_cast<char *>(kISO8601DateTime), format);
    }
    return CTime(t).ToLocalTime().AsString(kISO8601DateTime);
}


//////////////////////////////////////////////////////////////////////////
// CJobEvent implementation

static string   s_EventAsString[] = {
    "Submit",           // eSubmit
    "BatchSubmit",      // eBatchSubmit
    "Request",          // eRequest
    "Done",             // eDone
    "Return",           // eReturn
    "Fail",             // eFail
    "Read",             // eRead
    "ReadFail",         // eReadFail
    "ReadDone",         // eReadDone
    "ReadRollback",     // eReadRollback
    "Clear",            // eClear
    "Cancel",           // eCancel
    "Timeout",          // eTimeout
    "ReadTimeout",      // eReadTimeout
    "SessionChanged"    // eSessionChanged
};


string CJobEvent::EventToString(EJobEvent  event)
{
    if (event < eSubmit || event > eSessionChanged)
        return "UNKNOWN";

    return s_EventAsString[ event ];
}


CJobEvent::CJobEvent() :
    m_Dirty(false),
    m_Status(CNetScheduleAPI::eJobNotFound),
    m_Event(eUnknown),
    m_Timestamp(0),
    m_NodeAddr(0),
    m_RetCode(0)
{}


void CJobEvent::SetStatus(TJobStatus status)
{
    m_Dirty = true;
    m_Status = status;
}


void CJobEvent::SetEvent(EJobEvent  event)
{
    m_Dirty = true;
    m_Event = event;
}


void CJobEvent::SetTimestamp(time_t  t)
{
    m_Dirty = true;
    // Will suffice until 2038
    m_Timestamp = (unsigned) t;
}


void CJobEvent::SetNodeAddr(unsigned  node_ip)
{
    m_Dirty = true;
    m_NodeAddr = node_ip;
}


void CJobEvent::SetRetCode(int  retcode)
{
    m_Dirty = true;
    m_RetCode = retcode;
}


void CJobEvent::SetClientNode(const string &  client_node)
{
    m_Dirty = true;
    m_ClientNode = client_node.substr(0, kMaxWorkerNodeIdSize);
}


void CJobEvent::SetClientSession(const string &  cliet_session)
{
    m_Dirty = true;
    m_ClientSession = cliet_session.substr(0, kMaxWorkerNodeIdSize);
}


void CJobEvent::SetErrorMsg(const string &  msg)
{
    m_Dirty = true;
    m_ErrorMsg = msg.substr(0, kNetScheduleMaxDBErrSize);
}


static string  s_EventFieldNames[] = {
    "event",
    "event_status",
    "timestamp",
    "node_addr",
    "ret_code",
    "client_node",
    "client_session",
    "err_msg"
};

int CJobEvent::GetFieldIndex(const string &  name)
{
    for (unsigned n = 0; n < sizeof(s_EventFieldNames) /
                             sizeof(*s_EventFieldNames); ++n) {
        if (name == s_EventFieldNames[n])
            return n;
    }
    return -1;
}


string CJobEvent::GetField(int  index) const
{
    switch (index) {
    case 0: // event
        return EventToString(m_Event);
    case 1: // event_status
        return CNetScheduleAPI::StatusToString(m_Status);
    case 2: // timestamp
        return FormatTime(m_Timestamp);
    case 3: // node_addr
        return NStr::IntToString(m_NodeAddr);
    case 4: // ret_code
        return NStr::IntToString(m_RetCode);
    case 5: // client_node
        return m_ClientNode;
    case 6: // client_session
        return m_ClientSession;
    case 7: // err_msg
        return m_ErrorMsg;
    }
    return "NULL";
}


//////////////////////////////////////////////////////////////////////////
// CJob implementation

CJob::CJob() :
    m_New(true), m_Deleted(false), m_Dirty(0),
    m_Id(0),
    m_Passport(0),
    m_Status(CNetScheduleAPI::ePending),
    m_Timeout(0),
    m_RunTimeout(0),
    m_SubmNotifPort(0),
    m_SubmNotifTimeout(0),
    m_RunCount(0),
    m_ReadCount(0),
    m_AffinityId(0),
    m_Mask(0)
{}


CJob::CJob(const SNSCommandArguments &  request) :
    m_New(true), m_Deleted(false), m_Dirty(fJobPart),
    m_Id(0),
    m_Passport(0),
    m_Status(CNetScheduleAPI::ePending),
    m_Timeout(0),
    m_RunTimeout(0),
    m_SubmNotifPort(request.port),
    m_SubmNotifTimeout(request.timeout),
    m_RunCount(0),
    m_ReadCount(0),
    m_ProgressMsg(request.progress_msg),
    m_AffinityId(0),
    m_Mask(request.job_mask),
    m_ClientIP(request.ip),
    m_ClientSID(request.sid),
    m_Output("")
{
    SetInput(NStr::ParseEscapes(request.input));
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
    bool    was_overflow = m_Input.size() > kNetScheduleSplitSize;
    bool    is_overflow = input.size() > kNetScheduleSplitSize;

    if (is_overflow) {
        m_Dirty |= fJobInfoPart;
        if (!was_overflow)
            m_Dirty |= fJobPart;
    } else {
        m_Dirty |= fJobPart;
        if (was_overflow)
            m_Dirty |= fJobInfoPart;
    }
    m_Input = input;
}


void CJob::SetOutput(const string &  output)
{
    if (m_Output.empty() && output.empty())
        return;     // It might be the most common case for failed jobs

    bool    was_overflow = m_Output.size() > kNetScheduleSplitSize;
    bool    is_overflow = output.size() > kNetScheduleSplitSize;

    if (is_overflow) {
        m_Dirty |= fJobInfoPart;
        if (!was_overflow)
            m_Dirty |= fJobPart;
    } else {
        m_Dirty |= fJobPart;
        if (was_overflow)
            m_Dirty |= fJobInfoPart;
    }
    m_Output = output;
}


static string  s_JobFieldNames[] = {
    "id",
    "passport",
    "status",
    "timeout",
    "run_timeout",
    "subm_notif_port",
    "subm_notif_timeout",
    "run_count",
    "read_count",
    "mask",
    "client_ip",
    "client_sid",
    "events",
    "input",
    "output",
    "progress_msg"
};

int CJob::GetFieldIndex(const string &  name)
{
    for (unsigned n = 0; n < sizeof(s_JobFieldNames)/
                             sizeof(*s_JobFieldNames); ++n) {
        if (name == s_JobFieldNames[n])
            return n;
    }
    return -1;
}

string CJob::GetField(int index) const
{
    switch (index) {
    case 0:  // id
        return NStr::IntToString(m_Id);
    case 1:  // passport
        return NStr::IntToString(m_Passport);
    case 2:  // status
        return CNetScheduleAPI::StatusToString(m_Status);
    case 3:  // timeout
        return NStr::IntToString(m_Timeout);
    case 4:  // run_timeout
        return NStr::IntToString(m_RunTimeout);
    case 5:  // subm_notif_port
        return NStr::IntToString(m_SubmNotifPort);
    case 6:  // subm_notif_timeout
        return NStr::IntToString(m_SubmNotifTimeout);
    case 7:  // run_count
        return NStr::IntToString(m_RunCount);
    case 8:  // read-count
        return NStr::IntToString(m_ReadCount);
    case 9:  // mask
        return NStr::IntToString(m_Mask);
    case 10: // client_ip
        return m_ClientIP;
    case 11: // client_sid
        return m_ClientSID;
    case 12: // events
        return NStr::SizetToString(m_Events.size());
    case 13: // input
        return m_Input;
    case 14: // output
        return m_Output;
    case 15: // progress_msg
        return m_ProgressMsg;
    }
    return "NULL";
}


CJobEvent &  CJob::AppendEvent()
{
    m_Events.push_back(CJobEvent());
    m_Dirty |= fEventsPart;
    return m_Events[m_Events.size()-1];
}


const CJobEvent *  CJob::GetLastEvent() const
{
    // The first event is attached when a job is submitted, so
    // there is no way to have the events empty
    return &(m_Events[m_Events.size()-1]);
}


CJobEvent *  CJob::GetLastEvent()
{
    // The first event is attached when a job is submitted, so
    // there is no way to have the events empty
    m_Dirty |= fEventsPart;
    return &(m_Events[m_Events.size()-1]);
}


time_t  CJob::GetJobExpirationTime(time_t  queue_timeout,
                                   time_t  queue_run_timeout) const
{
    if (m_Status == CNetScheduleAPI::eRunning ||
        m_Status == CNetScheduleAPI::eReading) {
        if (m_RunTimeout != 0)
            return GetLastUpdateTime() + m_RunTimeout;
        return GetLastUpdateTime() + queue_run_timeout;
    }

    if (m_Timeout != 0)
        return GetLastUpdateTime() + m_Timeout;
    return GetLastUpdateTime() + queue_timeout;
}

CJob::EAuthTokenCompareResult
CJob::CompareAuthToken(const string &  auth_token) const
{
    vector<string>      parts;

    NStr::Tokenize(auth_token, "_", parts);
    if (parts.size() < 2)
        return eInvalidTokenFormat;

    if (NStr::StringToUInt(parts[0]) != m_Passport)
        return eNoMatch;
    if (NStr::StringToSizet(parts[1]) != m_Events.size())
        return ePassportOnlyMatch;
    return eCompleteMatch;
}


void CJob::Delete()
{
    m_Deleted = true;
    m_Dirty = 0;
}


CJob::EJobFetchResult CJob::Fetch(CQueue* queue)
{
    SJobDB &        job_db      = queue->m_QueueDbBlock->job_db;
    SJobInfoDB &    job_info_db = queue->m_QueueDbBlock->job_info_db;
    SEventsDB &     events_db   = queue->m_QueueDbBlock->events_db;

    m_Id            = job_db.id;

    m_Passport      = job_db.passport;
    m_Status        = TJobStatus(int(job_db.status));
    m_Timeout       = job_db.timeout;
    m_RunTimeout    = job_db.run_timeout;

    m_SubmNotifPort    = job_db.subm_notif_port;
    m_SubmNotifTimeout = job_db.subm_notif_timeout;

    m_RunCount      = job_db.run_counter;
    m_ReadCount     = job_db.read_counter;
    m_AffinityId    = job_db.aff_id;
    m_Mask          = job_db.mask;

    m_ClientIP      = job_db.client_ip;
    m_ClientSID     = job_db.client_sid;

    if (!(char) job_db.input_overflow)
        job_db.input.ToString(m_Input);
    if (!(char) job_db.output_overflow)
        job_db.output.ToString(m_Output);
    m_ProgressMsg = job_db.progress_msg;

    // JobInfoDB, can be optimized by adding lazy load
    EBDB_ErrCode        res;

    if ((char) job_db.input_overflow ||
        (char) job_db.output_overflow) {
        job_info_db.id = m_Id;
        if ((res = job_info_db.Fetch()) != eBDB_Ok) {
            if (res != eBDB_NotFound) {
                ERR_POST("Error reading queue jobinfo db, job_key " <<
                         queue->MakeKey(m_Id));
                return eJF_DBErr;
            }
        } else {
            if ((char) job_db.input_overflow)
                job_info_db.input.ToString(m_Input);
            if ((char) job_db.output_overflow)
                job_info_db.output.ToString(m_Output);
        }
    }

    // EventsDB
    m_Events.clear();
    CBDB_FileCursor &       cur = queue->GetEventsCursor();
    CBDB_CursorGuard        cg(cur);

    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << m_Id;

    for (unsigned n = 0; (res = cur.Fetch()) == eBDB_Ok; ++n) {
        CJobEvent &       event = AppendEvent();

        event.m_Status     = TJobStatus(int(events_db.status));
        event.m_Event      = CJobEvent::EJobEvent(int(events_db.event));
        event.m_Timestamp  = events_db.timestamp;
        event.m_NodeAddr   = events_db.node_addr;
        event.m_RetCode    = events_db.ret_code;
        events_db.client_node.ToString(event.m_ClientNode);
        events_db.client_session.ToString(event.m_ClientSession);
        events_db.err_msg.ToString(event.m_ErrorMsg);
        event.m_Dirty = false;
    }
    if (res != eBDB_NotFound) {
        ERR_POST("Error reading queue events db, job_key " <<
                 queue->MakeKey(m_Id));
        return eJF_DBErr;
    }

    m_New   = false;
    m_Dirty = 0;
    return eJF_Ok;
}


CJob::EJobFetchResult  CJob::Fetch(CQueue *  queue, unsigned  id)
{
    SJobDB &        job_db = queue->m_QueueDbBlock->job_db;

    job_db.id = id;

    EBDB_ErrCode    res = job_db.Fetch();
    if (res != eBDB_Ok) {
        if (res != eBDB_NotFound) {
            ERR_POST("Error reading queue job db, job_key " <<
                     queue->MakeKey(id));
            return eJF_DBErr;
        }
        return eJF_NotFound;
    }
    return Fetch(queue);
}


bool CJob::Flush(CQueue* queue)
{
    if (m_Deleted) {
        queue->EraseJob(m_Id);
        return true;
    }

    if (!m_Dirty)
        return true;

    SJobDB &        job_db      = queue->m_QueueDbBlock->job_db;
    SJobInfoDB &    job_info_db = queue->m_QueueDbBlock->job_info_db;
    SEventsDB &     events_db   = queue->m_QueueDbBlock->events_db;

    bool            flush_job = (m_Dirty & fJobPart) || m_New;
    bool            input_overflow = m_Input.size() > kNetScheduleSplitSize;
    bool            output_overflow = m_Output.size() > kNetScheduleSplitSize;

    // JobDB (QueueDB)
    if (flush_job) {
        job_db.id             = m_Id;
        job_db.passport       = m_Passport;
        job_db.status         = int(m_Status);

        job_db.timeout        = m_Timeout;
        job_db.run_timeout    = m_RunTimeout;

        job_db.subm_notif_port    = m_SubmNotifPort;
        job_db.subm_notif_timeout = m_SubmNotifTimeout;

        job_db.run_counter    = m_RunCount;
        job_db.read_counter   = m_ReadCount;
        job_db.aff_id         = m_AffinityId;
        job_db.mask           = m_Mask;

        job_db.client_ip      = m_ClientIP;
        job_db.client_sid     = m_ClientSID;

        if (!input_overflow) {
            job_db.input_overflow = 0;
            job_db.input = m_Input;
        } else {
            job_db.input_overflow = 1;
            job_db.input = "";
        }
        if (!output_overflow) {
            job_db.output_overflow = 0;
            job_db.output = m_Output;
        } else {
            job_db.output_overflow = 1;
            job_db.output = "";
        }
        job_db.progress_msg = m_ProgressMsg;
    }

    // JobInfoDB
    bool    nonempty_job_info = input_overflow || output_overflow;
    if (m_Dirty & fJobInfoPart) {
        job_info_db.id = m_Id;
        if (input_overflow)
            job_info_db.input = m_Input;
        else
            job_info_db.input = "";
        if (output_overflow)
            job_info_db.output = m_Output;
        else
            job_info_db.output = "";
    }
    if (flush_job)
        job_db.UpdateInsert();
    if (m_Dirty & fJobInfoPart) {
        if (nonempty_job_info)
            job_info_db.UpdateInsert();
        else
            job_info_db.Delete(CBDB_File::eIgnoreError);
    }

    // EventsDB
    unsigned n = 0;
    NON_CONST_ITERATE(vector<CJobEvent>, it, m_Events) {
        CJobEvent &         event = *it;

        if (event.m_Dirty) {
            events_db.id             = m_Id;
            events_db.event_id       = n;
            events_db.status         = int(event.m_Status);
            events_db.event          = int(event.m_Event);
            events_db.timestamp      = event.m_Timestamp;
            events_db.node_addr      = event.m_NodeAddr;
            events_db.ret_code       = event.m_RetCode;
            events_db.client_node    = event.m_ClientNode;
            events_db.client_session = event.m_ClientSession;
            events_db.err_msg        = event.m_ErrorMsg;
            events_db.UpdateInsert();
            event.m_Dirty = false;
        }
        ++n;
    }

    m_New = false;
    m_Dirty = 0;

    queue->CountEvent(CQueue::eStatDBWriteEvent, 1);
    return true;
}


bool CJob::ShouldNotify(time_t curr)
{
    // The very first event is always a submit
    _ASSERT(!m_Events.empty());

    if (m_SubmNotifTimeout && m_SubmNotifPort)
        return m_Events[0].m_Timestamp + m_SubmNotifTimeout >= curr;
    return false;
}


END_NCBI_SCOPE

