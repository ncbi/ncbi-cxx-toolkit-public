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
#include "ns_js_request.hpp"

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

CJobEvent::CJobEvent() :
    m_Dirty(false),
    m_Status(CNetScheduleAPI::eJobNotFound),
    m_Timestamp(0),
    m_NodeAddr(0),
    m_NodePort(0),
    m_RetCode(0)
{}


void CJobEvent::SetStatus(TJobStatus status)
{
    m_Dirty = true;
    m_Status = status;
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


void CJobEvent::SetNodePort(unsigned short  port)
{
    m_Dirty = true;
    m_NodePort = port;
}


void CJobEvent::SetRetCode(int  retcode)
{
    m_Dirty = true;
    m_RetCode = retcode;
}


void CJobEvent::SetNodeId(const string &  node_id)
{
    m_Dirty = true;
    m_NodeId = node_id.substr(0, kMaxWorkerNodeIdSize);
}


void CJobEvent::SetErrorMsg(const string &  msg)
{
    m_Dirty = true;
    m_ErrorMsg = msg.substr(0, kNetScheduleMaxDBErrSize);
}


static string  s_EventFieldNames[] = {
    "event_status",
    "timestamp",
    "node_addr",
    "node_port",
    "ret_code",
    "node_id",
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
    case 0: // event_status
        return CNetScheduleAPI::StatusToString(m_Status);
    case 1: // timestamp
        return FormatTime(m_Timestamp);
    case 2: // node_addr
        return NStr::IntToString(m_NodeAddr);
    case 3: // node_port
        return NStr::IntToString(m_NodePort);
    case 4: // ret_code
        return NStr::IntToString(m_RetCode);
    case 5: // node_id
        return m_NodeId;
    case 6: // err_msg
        return m_ErrorMsg;
    }
    return "NULL";
}


//////////////////////////////////////////////////////////////////////////
// CJob implementation

CJob::CJob() :
    m_New(true), m_Deleted(false), m_Dirty(0),
    m_Id(0),
    m_Status(CNetScheduleAPI::ePending),
    m_TimeSubmit(0),
    m_Timeout(0),
    m_RunTimeout(0),
    m_SubmAddr(0),
    m_SubmPort(0),
    m_SubmTimeout(0),
    m_RunCount(0),
    m_ReadCount(0),
    m_ReadGroup(0),
    m_AffinityId(0),
    m_Mask(0)
{}


CJob::CJob(const SJS_Request&  request, unsigned submAddr) :
    m_New(true), m_Deleted(false), m_Dirty(fJobPart),
    m_Id(0),
    m_Status(CNetScheduleAPI::ePending),
    m_TimeSubmit(0),
    m_Timeout(0),
    m_RunTimeout(0),
    m_SubmAddr(submAddr),
    m_SubmPort(request.port),
    m_SubmTimeout(request.timeout),
    m_RunCount(0),
    m_ReadCount(0),
    m_ReadGroup(0),
    m_ProgressMsg(request.param1),
    m_AffinityId(0),
    m_AffinityToken(NStr::ParseEscapes(request.affinity_token)),
    m_Mask(request.job_mask),
    m_ClientSID(request.param2)
{
    SetInput(NStr::ParseEscapes(request.input));
    SetTags(request.tags);
}


void CJob::SetId(unsigned id)
{
    m_Id = id;
    m_Dirty |= fJobPart;
}


void CJob::SetStatus(TJobStatus status)
{
    m_Status = status;
    m_Dirty |= fJobPart;
}


void CJob::SetTimeSubmit(time_t  t)
{
    // Will suffice until 2038
    m_TimeSubmit = (unsigned) t;
    m_Dirty |= fJobPart;
}


void CJob::SetTimeout(time_t  t)
{
    // Will suffice until 2038
    m_Timeout = (unsigned) t;
    m_Dirty |= fJobPart;
}


void CJob::SetRunTimeout(time_t  t)
{
    // Will suffice until 2038
    m_RunTimeout = (unsigned) t;
    m_Dirty |= fJobPart;
}


void CJob::SetSubmAddr(unsigned  addr)
{
    m_SubmAddr = addr;
    m_Dirty |= fJobPart;
}


void CJob::SetSubmPort(unsigned short  port)
{
    m_SubmPort = port;
    m_Dirty |= fJobPart;
}


void CJob::SetSubmTimeout(unsigned  t)
{
    m_SubmTimeout = t;
    m_Dirty |= fJobPart;
}


void CJob::SetRunCount(unsigned  count)
{
    m_RunCount = count;
    m_Dirty |= fJobPart;
}


void CJob::SetReadCount(unsigned  count)
{
    m_ReadCount = count;
    m_Dirty |= fJobPart;
}


void CJob::SetReadGroup(unsigned  group)
{
    m_ReadGroup = group;
    m_Dirty |= fJobPart;
}


void CJob::SetProgressMsg(const string &  msg)
{
    m_ProgressMsg = msg;
    m_Dirty |= fJobPart;
}


void CJob::SetAffinityId(unsigned  aff_id)
{
    m_AffinityId = aff_id;
    m_Dirty |= fJobPart;
}


void CJob::SetAffinityToken(const string &  aff_token)
{
    m_AffinityToken = aff_token;
    m_AffinityId = 0;
    m_Dirty |= fJobPart;
}


void CJob::SetMask(unsigned  mask)
{
    m_Mask = mask;
    m_Dirty |= fJobPart;
}


void CJob::SetClientIP(const string &  client_ip)
{
    m_ClientIP = client_ip;
    m_Dirty |= fJobPart;
}


void CJob::SetClientSID(const string &  client_sid)
{
    m_ClientSID = client_sid;
    m_Dirty |= fJobPart;
}


void CJob::SetEvents(const vector<CJobEvent> &  events)
{
    m_Events = events;
    m_Dirty |= fEventsPart;
}


void CJob::SetTags(const TNSTagList &  tags)
{
    m_Tags.clear();
    ITERATE(TNSTagList, it, tags) {
        m_Tags.push_back(*it);
    }
    m_Dirty |= fJobInfoPart;
}


void CJob::SetTags(const string &  strtags)
{
    x_ParseTags(strtags, m_Tags);
    m_Dirty |= fJobInfoPart;
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
    "status",
    "time_submit",
    "timeout",
    "run_timeout",
    "subm_addr",
    "subm_port",
    "subm_timeout",
    "run_count",
    "read_count",
    "read_group",
    "affinity",
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
    case 1:  // status
        return CNetScheduleAPI::StatusToString(m_Status);
    case 2:  // time_submit
        return FormatTime(m_TimeSubmit);
    case 3:  // timeout
        return NStr::IntToString(m_Timeout);
    case 4:  // run_timeout
        return NStr::IntToString(m_RunTimeout);
    case 5:  // subm_addr
        return NStr::IntToString(m_SubmAddr);
    case 6:  // subm_port
        return NStr::IntToString(m_SubmPort);
    case 7:  // subm_timeout
        return NStr::IntToString(m_SubmTimeout);
    case 8:  // run_count
        return NStr::IntToString(m_RunCount);
    case 9:  // read-count
        return NStr::IntToString(m_ReadCount);
    case 10:  // read_group
        return NStr::IntToString(m_ReadGroup);
    case 11: // affinity
        return m_AffinityToken;
    case 12: // mask
        return NStr::IntToString(m_Mask);
    case 13: // client_ip
        return m_ClientIP;
    case 14: // client_sid
        return m_ClientSID;
    case 15: // events
        return NStr::IntToString(m_Events.size());
    case 16: // input
        return m_Input;
    case 17: // output
        return m_Output;
    case 18: // progress_msg
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
    if (m_Events.empty())
        return NULL;

    return &(m_Events[m_Events.size()-1]);
}


CJobEvent *  CJob::GetLastEvent()
{
    if (m_Events.empty())
        return NULL;

    m_Dirty |= fEventsPart;
    return &(m_Events[m_Events.size()-1]);
}


time_t  CJob::GetLastUpdateTime(void) const
{
    if (m_Events.empty())
        return m_TimeSubmit;    // The job had no attempts to be executed

    return m_Events[m_Events.size()-1].GetTimestamp();
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

void CJob::CheckAffinityToken(CAffinityDictGuard& aff_dict_guard)
{
    if (!m_AffinityToken.empty()) {
        m_AffinityId =
            aff_dict_guard.CheckToken(m_AffinityToken);
    }
}


void CJob::FetchAffinityToken(CQueue* queue)
{
    if (m_AffinityId)
        m_AffinityToken = queue->m_AffinityDict.GetAffToken(m_AffinityId);
}


void CJob::Delete()
{
    m_Deleted = true;
    m_Dirty = 0;
}


CJob::EJobFetchResult CJob::Fetch(CQueue* queue)
{
    SQueueDB &      job_db      = queue->m_QueueDbBlock->job_db;
    SJobInfoDB &    job_info_db = queue->m_QueueDbBlock->job_info_db;
    SEventsDB &     events_db   = queue->m_QueueDbBlock->events_db;

    m_Id          = job_db.id;

    m_Status      = TJobStatus(int(job_db.status));
    m_TimeSubmit  = job_db.time_submit;
    m_Timeout     = job_db.timeout;
    m_RunTimeout  = job_db.run_timeout;

    m_SubmAddr    = job_db.subm_addr;
    m_SubmPort    = job_db.subm_port;
    m_SubmTimeout = job_db.subm_timeout;

    m_RunCount    = job_db.run_counter;
    m_ReadCount   = job_db.read_counter;
    m_ReadGroup   = job_db.read_group;
    m_AffinityId  = job_db.aff_id;
    // TODO: May be it is safe (from the locking viewpoint) and easy
    // to load token here rather than require separate FetchAffinityToken
    m_AffinityToken.clear();
    m_Mask        = job_db.mask;

    m_ClientIP    = job_db.client_ip;
    m_ClientSID   = job_db.client_sid;

    if (!(char) job_db.input_overflow)
        job_db.input.ToString(m_Input);
    if (!(char) job_db.output_overflow)
        job_db.output.ToString(m_Output);
    m_ProgressMsg = job_db.progress_msg;

    // JobInfoDB, can be optimized by adding lazy load
    EBDB_ErrCode        res;

    job_info_db.id = m_Id;
    if ((res = job_info_db.Fetch()) != eBDB_Ok) {
        if (res != eBDB_NotFound) {
            ERR_POST("Error reading queue jobinfo db, job_key " <<
                     queue->MakeKey(m_Id));
            return eJF_DBErr;
        }
    } else {
        x_ParseTags(job_info_db.tags, m_Tags);
        if ((char) job_db.input_overflow)
            job_info_db.input.ToString(m_Input);
        if ((char) job_db.output_overflow)
            job_info_db.output.ToString(m_Output);
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
        event.m_Timestamp  = events_db.timestamp;
        event.m_NodeAddr   = events_db.node_addr;
        event.m_NodePort   = events_db.node_port;
        event.m_RetCode    = events_db.ret_code;
        events_db.node_id.ToString(event.m_NodeId);
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
    SQueueDB &      job_db = queue->m_QueueDbBlock->job_db;

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

    // We can not run CheckAffinityToken during flush because of locking
    // constraints, so we assert that it was run before flush.
    if (m_AffinityToken.size() && !m_AffinityId) {
        ERR_POST("CheckAffinityToken call missed for job_key " <<
                 queue->MakeKey(m_Id));
    }
    _ASSERT(!m_AffinityToken.size() || m_AffinityId);

    SQueueDB &      job_db      = queue->m_QueueDbBlock->job_db;
    SJobInfoDB &    job_info_db = queue->m_QueueDbBlock->job_info_db;
    SEventsDB &     events_db   = queue->m_QueueDbBlock->events_db;

    bool            flush_job = (m_Dirty & fJobPart) || m_New;
    bool            input_overflow = m_Input.size() > kNetScheduleSplitSize;
    bool            output_overflow = m_Output.size() > kNetScheduleSplitSize;

    // JobDB (QueueDB)
    if (flush_job) {
        job_db.id           = m_Id;
        job_db.status       = int(m_Status);

        job_db.time_submit  = m_TimeSubmit;
        job_db.timeout      = m_Timeout;
        job_db.run_timeout  = m_RunTimeout;

        job_db.subm_addr    = m_SubmAddr;
        job_db.subm_port    = m_SubmPort;
        job_db.subm_timeout = m_SubmTimeout;

        job_db.run_counter  = m_RunCount;
        job_db.read_counter = m_ReadCount;
        job_db.read_group   = m_ReadGroup;
        job_db.aff_id       = m_AffinityId;
        job_db.mask         = m_Mask;

        job_db.client_ip    = m_ClientIP;
        job_db.client_sid   = m_ClientSID;

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
        list<string> tag_list;
        ITERATE(TNSTagList, it, m_Tags) {
            nonempty_job_info = true;
            tag_list.push_back(it->first + "\t" + it->second);
        }
        if (input_overflow)
            job_info_db.input = m_Input;
        else
            job_info_db.input = "";
        if (output_overflow)
            job_info_db.output = m_Output;
        else
            job_info_db.output = "";
        job_info_db.tags = NStr::Join(tag_list, "\t");
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
            events_db.id          = m_Id;
            events_db.event       = n;
            events_db.status      = int(event.m_Status);
            events_db.timestamp   = event.m_Timestamp;
            events_db.node_addr   = event.m_NodeAddr;
            events_db.node_port   = event.m_NodePort;
            events_db.ret_code    = event.m_RetCode;
            events_db.node_id     = event.m_NodeId;
            events_db.err_msg     = event.m_ErrorMsg;
            events_db.UpdateInsert();
            event.m_Dirty = false;
        }
        ++n;
    }

    m_New = false;
    m_Dirty = 0;
    return true;
}


bool CJob::ShouldNotify(time_t curr)
{
    return m_TimeSubmit && m_SubmTimeout && m_SubmAddr && m_SubmPort &&
        (m_TimeSubmit + m_SubmTimeout >= (unsigned) curr);
}


void CJob::x_ParseTags(const string& strtags, TNSTagList& tags)
{
    list<string>        tokens;
    NStr::Split(NStr::ParseEscapes(strtags), "\t",
                tokens, NStr::eNoMergeDelims);
    tags.clear();
    for (list<string>::const_iterator it = tokens.begin(); it != tokens.end(); ++it) {
        string      key(*it);
        ++it;

        if (it != tokens.end())
            tags.push_back(TNSTag(key, *it));
        else {
            tags.push_back(TNSTag(key, kEmptyStr));
            break;
        }
    }
}


END_NCBI_SCOPE

