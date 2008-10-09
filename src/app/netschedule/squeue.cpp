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
 *   NetSchedule queue structure and parameters
 */
#include <ncbi_pch.hpp>

#include "squeue.hpp"

#include "ns_util.hpp"

#include <bdb/bdb_trans.hpp>
#include <util/bitset/bmalgo.h>

BEGIN_NCBI_SCOPE


static const char* kISO8601DateTime = 0;
static string FormatTime(time_t t)
{
    if (!t) return "NULL";
    if (!kISO8601DateTime) {
        const char *format = CTimeFormat::GetPredefined(
            CTimeFormat::eISO8601_DateTimeSec).GetString().c_str();
        kISO8601DateTime = new char[strlen(format)+1];
        strcpy(const_cast<char *>(kISO8601DateTime), format);
    }
    return CTime(t).ToLocalTime().AsString(kISO8601DateTime);
}


//////////////////////////////////////////////////////////////////////////
// CJobRun implementation

CJobRun::CJobRun() :
    m_Dirty(false),
    m_Status(CNetScheduleAPI::eJobNotFound),
    m_TimeStart(0),
    m_TimeDone(0),
    m_ClientIp(0),
    m_ClientPort(0),
    m_RetCode(0)
{
}


void CJobRun::SetStatus(TJobStatus status)
{
    m_Dirty = true;
    m_Status = status;
}


void CJobRun::SetTimeStart(unsigned t)
{
    m_Dirty = true;
    m_TimeStart = t;
}


void CJobRun::SetTimeDone(unsigned t)
{
    m_Dirty = true;
    m_TimeDone = t;
}


void CJobRun::SetClientIp(unsigned node_ip)
{
    m_Dirty = true;
    m_ClientIp = node_ip;
}


void CJobRun::SetClientPort(unsigned short port)
{
    m_Dirty = true;
    m_ClientPort = port;
}


void CJobRun::SetRetCode(int retcode)
{
    m_Dirty = true;
    m_RetCode = retcode;
}


void CJobRun::SetWorkerNodeId(const string& node_id)
{
    m_Dirty = true;
    m_WorkerNodeId = node_id;
}


void CJobRun::SetErrorMsg(const string& msg)
{
    m_Dirty = true;
    m_ErrorMsg = msg;
}


static const char* s_RunFieldNames[] = {
    "run_status",
    "time_start",
    "time_done",
    "client_ip",
    "client_port",
    "ret_code",
    "worker_node_id",
    "err_msg"
};

int CJobRun::GetFieldIndex(const string& name)
{
    for (unsigned n = 0; n < sizeof(s_RunFieldNames)/sizeof(*s_RunFieldNames); ++n) {
        if (name == s_RunFieldNames[n]) return n;
    }
    return -1;
}


string CJobRun::GetField(int index) const
{
    switch (index) {
    case 0: // run_status
        return CNetScheduleAPI::StatusToString(m_Status);
    case 1: // time_start
        return FormatTime(m_TimeStart);
    case 2: // time_done
        return FormatTime(m_TimeDone);
    case 3: // client_ip
        return NStr::IntToString(m_ClientIp);
    case 4: // client_port
        return NStr::IntToString(m_ClientPort);
    case 5: // ret_code
        return NStr::IntToString(m_RetCode);
    case 6: // worker_node_id
        return m_WorkerNodeId;
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
    m_Status(CNetScheduleAPI::ePending),
    m_TimeSubmit(0),
    m_Timeout(0),
    m_RunTimeout(0),
    m_SubmAddr(0),
    m_SubmPort(0),
    m_SubmTimeout(0),
    m_RunCount(0),
    m_ReadGroup(0),
    m_AffinityId(0),
    m_Mask(0)
{
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


void CJob::SetTimeSubmit(unsigned t)
{
    m_TimeSubmit = t;
    m_Dirty |= fJobPart;
}


void CJob::SetTimeout(unsigned t)
{
    m_Timeout = t;
    m_Dirty |= fJobPart;
}


void CJob::SetRunTimeout(unsigned t)
{
    m_RunTimeout = t;
    m_Dirty |= fJobPart;
}


void CJob::SetSubmAddr(unsigned addr)
{
    m_SubmAddr = addr;
    m_Dirty |= fJobPart;
}


void CJob::SetSubmPort(unsigned short port)
{
    m_SubmPort = port;
    m_Dirty |= fJobPart;
}


void CJob::SetSubmTimeout(unsigned t)
{
    m_SubmTimeout = t;
    m_Dirty |= fJobPart;
}


void CJob::SetRunCount(unsigned count)
{
    m_RunCount = count;
    m_Dirty |= fJobPart;
}


void CJob::SetReadGroup(unsigned group)
{
    m_ReadGroup = group;
    m_Dirty |= fJobPart;
}


void CJob::SetProgressMsg(const string& msg)
{
    m_ProgressMsg = msg;
    m_Dirty |= fJobPart;
}


void CJob::SetAffinityId(unsigned aff_id)
{
    m_AffinityId = aff_id;
    m_Dirty |= fJobPart;
}


void CJob::SetAffinityToken(const string& aff_token)
{
    m_AffinityToken = aff_token;
    m_AffinityId = 0;
    m_Dirty |= fJobPart;
}


void CJob::SetMask(unsigned mask)
{
    m_Mask = mask;
    m_Dirty |= fJobPart;
}


void CJob::SetRuns(const vector<CJobRun>& runs)
{
    m_Runs = runs;
    m_Dirty |= fRunsPart;
}


void CJob::SetTags(const TNSTagList& tags)
{
    m_Tags.clear();
    ITERATE(TNSTagList, it, tags) {
        m_Tags.push_back(*it);
    }
    m_Dirty |= fJobInfoPart;
}


void CJob::SetTags(const string& strtags)
{
    x_ParseTags(strtags, m_Tags);
    m_Dirty |= fJobInfoPart;
}


void CJob::SetInput(const string& input)
{
    bool was_overflow = m_Input.size() > kNetScheduleSplitSize;
    bool is_overflow = input.size() > kNetScheduleSplitSize;
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


void CJob::SetOutput(const string& output)
{
    bool was_overflow = m_Output.size() > kNetScheduleSplitSize;
    bool is_overflow = output.size() > kNetScheduleSplitSize;
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


static const char* s_JobFieldNames[] = {
    "id",
    "status",
    "time_submit",
    "timeout",
    "run_timeout",
    "subm_addr",
    "subm_port",
    "subm_timeout",
    "run_count",
    "read_group",
    "affinity",
    "mask",
    "runs",
    "input",
    "output",
    "progress_msg"
};

int CJob::GetFieldIndex(const string& name)
{
    for (unsigned n = 0; n < sizeof(s_JobFieldNames)/sizeof(*s_JobFieldNames); ++n) {
        if (name == s_JobFieldNames[n]) return n;
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
    case 9:  // read_group
        return NStr::IntToString(m_ReadGroup);
    case 10: // affinity
        return m_AffinityToken;
    case 11: // mask
        return NStr::IntToString(m_Mask);
    case 12: // runs
        return NStr::IntToString(m_Runs.size());
    case 13: // input
        return m_Input;
    case 14: // output
        return m_Output;
    case 15: // progress_msg
        return m_ProgressMsg;
    }
    return "NULL";
}


CJobRun& CJob::AppendRun()
{
    m_Runs.push_back(CJobRun());
    m_Dirty |= fRunsPart;
    return m_Runs[m_Runs.size()-1];
}


const CJobRun* CJob::GetLastRun() const
{
    if (m_Runs.size() == 0) return NULL;
    return &(m_Runs[m_Runs.size()-1]);
}


CJobRun* CJob::GetLastRun()
{
    if (m_Runs.size() == 0) return NULL;
    m_Dirty |= fRunsPart;
    return &(m_Runs[m_Runs.size()-1]);
}


void CJob::CheckAffinityToken(SLockedQueue*     queue,
                              CBDB_Transaction& trans)
{
    if (m_AffinityToken.size()) {
        m_AffinityId =
            queue->m_AffinityDict.CheckToken(m_AffinityToken.c_str(), trans);
    }
}


void CJob::FetchAffinityToken(SLockedQueue* queue)
{
    if (m_AffinityId)
        m_AffinityToken = queue->m_AffinityDict.GetAffToken(m_AffinityId);
}


void CJob::Delete()
{
    m_Deleted = true;
    m_Dirty = 0;
}


CJob::EJobFetchResult CJob::Fetch(SLockedQueue* queue)
{
    SQueueDB&   job_db      = queue->m_JobDB;
    SJobInfoDB& job_info_db = queue->m_JobInfoDB;
    SRunsDB&    runs_db     = queue->m_RunsDB;

    m_Id          = job_db.id;

    m_Status      = TJobStatus(int(job_db.status));
    m_TimeSubmit  = job_db.time_submit;
    m_Timeout     = job_db.timeout;
    m_RunTimeout  = job_db.run_timeout;

    m_SubmAddr    = job_db.subm_addr;
    m_SubmPort    = job_db.subm_port;
    m_SubmTimeout = job_db.subm_timeout;

    m_RunCount    = job_db.run_counter;
    m_ReadGroup   = job_db.read_group;
    m_AffinityId  = job_db.aff_id;
    // TODO: May be it is safe (from the locking viewpoint) and easy
    // to load token here rather than require separate FetchAffinityToken
    m_AffinityToken.clear();
    m_Mask        = job_db.mask;

    if (!(char) job_db.input_overflow)
        job_db.input.ToString(m_Input);
    if (!(char) job_db.output_overflow)
        job_db.output.ToString(m_Output);
    m_ProgressMsg = job_db.progress_msg;

    // JobInfoDB, can be optimized by adding lazy load
    EBDB_ErrCode res;
    job_info_db.id = m_Id;
    if ((res = job_info_db.Fetch()) != eBDB_Ok) {
        if (res != eBDB_NotFound) {
            ERR_POST(Error << "Error reading queue jobinfo db, id " << m_Id);
            return eJF_DBErr;
        }
    } else {
        x_ParseTags((const char*) job_info_db.tags, m_Tags);
        if ((char) job_db.input_overflow)
            job_info_db.input.ToString(m_Input);
        if ((char) job_db.output_overflow)
            job_info_db.output.ToString(m_Output);
    }

    // RunsDB
    m_Runs.clear();
    CBDB_FileCursor& cur = queue->GetRunsCursor();
    CBDB_CursorGuard cg(cur);
    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << m_Id;
    unsigned n = 0;
    for (; (res = cur.Fetch()) == eBDB_Ok; ++n) {
        CJobRun& run = AppendRun();
        run.m_Status     = TJobStatus(int(runs_db.status));
        run.m_TimeStart  = runs_db.time_start;
        run.m_TimeDone   = runs_db.time_done;
        run.m_ClientIp   = runs_db.client_ip;
        run.m_ClientPort = runs_db.client_port;
        run.m_RetCode    = runs_db.ret_code;
        runs_db.worker_node_id.ToString(run.m_WorkerNodeId);
        runs_db.err_msg.ToString(run.m_ErrorMsg);
        run.m_Dirty = false;
    }
    if (res != eBDB_NotFound) {
        ERR_POST(Error << "Error reading queue runs db, id " << m_Id);
        return eJF_DBErr;
    }

    m_New   = false;
    m_Dirty = 0;
    return eJF_Ok;
}


CJob::EJobFetchResult CJob::Fetch(SLockedQueue* queue, unsigned id)
{
    SQueueDB& job_db = queue->m_JobDB;
    job_db.id = id;
    EBDB_ErrCode res;
    if ((res = job_db.Fetch()) != eBDB_Ok) {
        if (res != eBDB_NotFound) {
            ERR_POST(Error << "Error reading queue job db, id " << id);
            return eJF_DBErr;
        } else {
            return eJF_NotFound;
        }
    }
    return Fetch(queue);
}


bool CJob::Flush(SLockedQueue* queue)
{
    if (m_Deleted) {
        queue->Erase(m_Id);
        return true;
    }
    if (!m_Dirty) return true;
    // We can not run CheckAffinityToken during flush because of locking
    // constraints, so we assert that it was run before flush.
    if (m_AffinityToken.size() && !m_AffinityId) {
        LOG_POST(Error << "CheckAffinityToken call missed");
    }
    _ASSERT(!m_AffinityToken.size() || m_AffinityId);
    SQueueDB&   job_db      = queue->m_JobDB;
    SJobInfoDB& job_info_db = queue->m_JobInfoDB;
    SRunsDB&    runs_db     = queue->m_RunsDB;

    bool flush_job = (m_Dirty & fJobPart) || m_New;

    bool input_overflow = m_Input.size() > kNetScheduleSplitSize;
    bool output_overflow = m_Output.size() > kNetScheduleSplitSize;

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

        job_db.run_counter = m_RunCount;
        job_db.read_group  = m_ReadGroup;
        job_db.aff_id = m_AffinityId;
        job_db.mask = m_Mask;

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
    bool nonempty_job_info = input_overflow || output_overflow;
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

    // RunsDB
    unsigned n = 0;
    NON_CONST_ITERATE(vector<CJobRun>, it, m_Runs) {
        CJobRun& run = *it;
        if (run.m_Dirty) {
            runs_db.id          = m_Id;
            runs_db.run         = n;
            runs_db.status      = int(run.m_Status);
            runs_db.time_start  = run.m_TimeStart;
            runs_db.time_done   = run.m_TimeDone;
            runs_db.client_ip   = run.m_ClientIp;
            runs_db.client_port = run.m_ClientPort;
            runs_db.ret_code    = run.m_RetCode;
            runs_db.worker_node_id = run.m_WorkerNodeId;
            runs_db.err_msg     = run.m_ErrorMsg;
            runs_db.UpdateInsert();
            run.m_Dirty = false;
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
          (m_TimeSubmit + m_SubmTimeout >= (unsigned)curr);
}


void CJob::x_ParseTags(const string& strtags, TNSTagList& tags)
{
    list<string> tokens;
    NStr::Split(NStr::ParseEscapes(strtags), "\t",
        tokens, NStr::eNoMergeDelims);
    tags.clear();
    for (list<string>::iterator it = tokens.begin(); it != tokens.end(); ++it) {
        string key(*it); ++it;
        if (it != tokens.end()){
            tags.push_back(TNSTag(key, *it));
        } else {
            tags.push_back(TNSTag(key, kEmptyStr));
            break;
        }
    }
}


//////////////////////////////////////////////////////////////////////////
// SQueueDbBlock

void SQueueDbBlock::Open(CBDB_Env& env, const string& path, int pos_)
{
    pos = pos_;
    string prefix = string("jsq_") + NStr::IntToString(pos);
    allocated = false;
    try {
        string fname = prefix + ".db";
        job_db.SetEnv(env);
        // TODO: RevSplitOff make sense only for long living queues,
        // for dynamic ones it slows down the process, but because queue
        // if eventually is disposed of, it does not make sense to save
        // space here
        job_db.RevSplitOff();
        job_db.Open(fname.c_str(), CBDB_RawFile::eReadWriteCreate);

        fname = prefix + "_jobinfo.db";
        job_info_db.SetEnv(env);
        job_info_db.Open(fname.c_str(), CBDB_RawFile::eReadWriteCreate);

        fname = prefix + "_runs.db";
        runs_db.SetEnv(env);
        runs_db.Open(fname.c_str(), CBDB_RawFile::eReadWriteCreate);

        fname = prefix + "_deleted.db";
        deleted_jobs_db.SetEnv(env);
        deleted_jobs_db.Open(fname.c_str(), CBDB_RawFile::eReadWriteCreate);

        fname = prefix + "_affid.idx";
        affinity_idx.SetEnv(env);
        affinity_idx.Open(fname.c_str(), CBDB_RawFile::eReadWriteCreate);

        fname = prefix + "_affdict.db";
        aff_dict_db.SetEnv(env);
        aff_dict_db.Open(fname.c_str(), CBDB_RawFile::eReadWriteCreate);

        fname = prefix + "_affdict_token.idx";
        aff_dict_token_idx.SetEnv(env);
        aff_dict_token_idx.Open(fname.c_str(), CBDB_RawFile::eReadWriteCreate);

        fname = prefix + "_tag.idx";
        tag_db.SetEnv(env);
        tag_db.SetPageSize(32*1024);
        tag_db.RevSplitOff();
        tag_db.Open(fname, CBDB_RawFile::eReadWriteCreate);

    } catch (CBDB_ErrnoException& ex) {
        throw;
    }
}


void SQueueDbBlock::Close()
{
    tag_db.Close();
    aff_dict_token_idx.Close();
    aff_dict_db.Close();
    affinity_idx.Close();
    deleted_jobs_db.Close();
    runs_db.Close();
    job_info_db.Close();
    job_db.Close();
}


void SQueueDbBlock::ResetTransaction()
{
    tag_db.SetTransaction(NULL);
    aff_dict_token_idx.SetTransaction(NULL);
    aff_dict_db.SetTransaction(NULL);
    affinity_idx.SetTransaction(NULL);
    deleted_jobs_db.SetTransaction(NULL);
    runs_db.SetTransaction(NULL);
    job_info_db.SetTransaction(NULL);
    job_db.SetTransaction(NULL);
}


void SQueueDbBlock::Truncate()
{
    tag_db.Truncate();
    aff_dict_token_idx.Truncate();
    aff_dict_db.Truncate();
    affinity_idx.Truncate();
    deleted_jobs_db.Truncate();
    runs_db.Truncate();
    job_info_db.Truncate();
    job_db.Truncate();
    CBDB_Env& env = *job_db.GetEnv();
    env.ForceTransactionCheckpoint();
    env.CleanLog();

}

//////////////////////////////////////////////////////////////////////////
// CQueueDbBlockArray

CQueueDbBlockArray::CQueueDbBlockArray()
  : m_Count(0), m_Array(0)
{
};


CQueueDbBlockArray::~CQueueDbBlockArray()
{
}


void CQueueDbBlockArray::Init(CBDB_Env& env, const string& path,
                             unsigned count)
{
    m_Count = count;
    m_Array = new SQueueDbBlock[m_Count];
    for (unsigned n = 0; n < m_Count; ++n) {
        m_Array[n].Open(env, path, n);
    }
}


void CQueueDbBlockArray::Close()
{
    for (unsigned n = 0; n < m_Count; ++n) {
        m_Array[n].Close();
    }
    delete [] m_Array;
    m_Array = 0;
    m_Count = 0;
}


int CQueueDbBlockArray::Allocate()
{
    for (unsigned n = 0; n < m_Count; ++n) {
        if (!m_Array[n].allocated) {
            m_Array[n].allocated = true;
            return n;
        }
    }
    return -1;
}


void CQueueDbBlockArray::Free(int pos)
{
    if (pos < 0 || unsigned(pos) >= m_Count) return;
    m_Array[pos].ResetTransaction();
    m_Array[pos].allocated = false;
}


SQueueDbBlock* CQueueDbBlockArray::Get(int pos)
{
    if (pos < 0 || unsigned(pos) >= m_Count) return 0;
    return &m_Array[pos];
}

//////////////////////////////////////////////////////////////////////////
// SQueueParameters

void SQueueParameters::Read(const IRegistry& reg, const string& sname)
{
    // When modifying this, modify all places marked with PARAMETERS
#define GetIntNoErr(name, dflt) reg.GetInt(sname, name, dflt, 0, IRegistry::eReturn)
    // Read parameters
    timeout = GetIntNoErr("timeout", 3600);

    notif_timeout = GetIntNoErr("notif_timeout", 7);
    run_timeout = GetIntNoErr("run_timeout", timeout);
    run_timeout_precision = GetIntNoErr("run_timeout_precision", run_timeout);

    program_name = reg.GetString(sname, "program", kEmptyStr);

    delete_done = reg.GetBool(sname, "delete_done", false,
        0, IRegistry::eReturn);

    failed_retries = GetIntNoErr("failed_retries", 0);

    blacklist_time = GetIntNoErr("blacklist_time", 0);

    empty_lifetime = GetIntNoErr("empty_lifetime", -1);

    string s = reg.GetString(sname, "max_input_size", kEmptyStr);
    max_input_size = kNetScheduleMaxDBDataSize;
    try {
        max_input_size = (unsigned) NStr::StringToUInt8_DataSize(s);
    }
    catch (CStringException&) {}
    max_input_size = min(kNetScheduleMaxOverflowSize, max_input_size);

    s =  reg.GetString(sname, "max_output_size", kEmptyStr);
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
    log_job_state          = reg.GetBool(sname, "log_job_state", false,
        0, IRegistry::eReturn);

    subm_hosts = reg.GetString(sname,  "subm_host",  kEmptyStr);
    wnode_hosts = reg.GetString(sname, "wnode_host", kEmptyStr);
}


//////////////////////////////////////////////////////////////////////////
// CNSTagMap

CNSTagMap::CNSTagMap()
{
}


CNSTagMap::~CNSTagMap()
{
    NON_CONST_ITERATE(TNSTagMap, it, m_TagMap) {
        if (it->second) {
            delete it->second;
            it->second = 0;
        }
    }
}


//////////////////////////////////////////////////////////////////////////
CQueueEnumCursor::CQueueEnumCursor(SLockedQueue* queue)
  : CBDB_FileCursor(queue->m_JobDB)
{
    SetCondition(CBDB_FileCursor::eFirst);
}


//////////////////////////////////////////////////////////////////////////
// SLockedQueue

SLockedQueue::SLockedQueue(const string& queue_name,
                           const string& qclass_name,
                           TQueueKind queue_kind)
  :
    m_QueueName(queue_name),
    m_QueueClass(qclass_name),
    m_Kind(queue_kind),
    m_QueueDbBlock(0),

    m_BecameEmpty(-1),
    q_notif("NCBI_JSQ_"),
    run_time_line(NULL),
    delete_database(false),
    m_AffWrapped(false),
    m_CurrAffId(0),
    m_LastAffId(0),

    m_ParamLock(CRWLock::fFavorWriters),
    m_Timeout(3600),
    m_NotifyTimeout(7),
    m_DeleteDone(false),
    m_RunTimeout(3600),
    m_RunTimeoutPrecision(-1),
    m_FailedRetries(0),
    m_BlacklistTime(0),
    m_EmptyLifetime(0),
    m_MaxInputSize(kNetScheduleMaxDBDataSize),
    m_MaxOutputSize(kNetScheduleMaxDBDataSize),
    m_DenyAccessViolations(false),
    m_LogAccessViolations(true)
{
    _ASSERT(!queue_name.empty());
    q_notif.append(queue_name);
    for (TStatEvent n = 0; n < eStatNumEvents; ++n) {
        m_EventCounter[n].Set(0);
    }
    m_StatThread.Reset(new CStatisticsThread(*this));
    m_StatThread->Run();
    m_LastId.Set(0);
    m_GroupLastId.Set(0);
}

SLockedQueue::~SLockedQueue()
{
    delete run_time_line;
    Detach();
    m_StatThread->RequestStop();
    m_StatThread->Join(NULL);
}


void SLockedQueue::Attach(SQueueDbBlock* block)
{
    Detach();
    m_QueueDbBlock = block;
    m_AffinityDict.Attach(&m_QueueDbBlock->aff_dict_db,
                          &m_QueueDbBlock->aff_dict_token_idx);
}


void SLockedQueue::Detach()
{
    m_AffinityDict.Detach();
    if (delete_database) {
        m_QueueDbBlock->ResetTransaction();
        m_QueueDbBlock->Truncate();
    }
    delete_database = false;
    m_QueueDbBlock = 0;
}


void SLockedQueue::SetParameters(const SQueueParameters& params)
{
    // When modifying this, modify all places marked with PARAMETERS
    CWriteLockGuard guard(m_ParamLock);
    m_Timeout = params.timeout;
    m_NotifyTimeout = params.notif_timeout;
    m_DeleteDone = params.delete_done;

    m_RunTimeout = params.run_timeout;
    if (params.run_timeout && !run_time_line) {
        // One time only. Precision can not be reset.
        run_time_line =
            new CJobTimeLine(params.run_timeout_precision, 0);
        m_RunTimeoutPrecision = params.run_timeout_precision;
    }

    m_FailedRetries = params.failed_retries;
    m_BlacklistTime = params.blacklist_time;
    m_EmptyLifetime = params.empty_lifetime;
    m_MaxInputSize  = params.max_input_size;
    m_MaxOutputSize = params.max_output_size;
    m_DenyAccessViolations = params.deny_access_violations;
    m_LogAccessViolations  = params.log_access_violations;
    m_LogJobState          = params.log_job_state;
    // program version control
    m_ProgramVersionList.Clear();
    if (!params.program_name.empty()) {
        m_ProgramVersionList.AddClientInfo(params.program_name);
    }
    m_SubmHosts.SetHosts(params.subm_hosts);
    m_WnodeHosts.SetHosts(params.wnode_hosts);
}


unsigned SLockedQueue::LoadStatusMatrix()
{
    unsigned queue_run_timeout;
    bool log_job_state;
    {{
        CQueueParamAccessor qp(*this);
        queue_run_timeout = qp.GetRunTimeout();
        log_job_state = qp.GetLogJobState();
    }}
    EBDB_ErrCode err;

    static EVectorId all_ids[] = { eVIJob, eVITag, eVIAffinity };
    TNSBitVector all_vects[] = 
        { m_JobsToDelete, m_DeletedJobs, m_AffJobsToDelete };
    for (size_t i = 0; i < sizeof(all_ids) / sizeof(all_ids[0]); ++i) {
        m_DeletedJobsDB.id = all_ids[i];
        err = m_DeletedJobsDB.ReadVector(&all_vects[i]);
        if (err != eBDB_Ok && err != eBDB_NotFound) {
            // TODO: throw db error
        }
        all_vects[i].optimize();
    }

    // scan the queue, load the state machine from DB
    TNSBitVector running_jobs;
    CBDB_FileCursor cur(m_JobDB);
    cur.InitMultiFetch(1024*1024);
    cur.SetCondition(CBDB_FileCursor::eGE);
    cur.From << 0;

    unsigned recs = 0;

    unsigned last_id = 0;
    unsigned group_last_id = 0;
    for (; cur.Fetch() == eBDB_Ok; ) {
        unsigned job_id = m_JobDB.id;
        if (m_JobsToDelete.test(job_id)) continue;
        int i_status = m_JobDB.status;
        if (i_status < (int) CNetScheduleAPI::ePending ||
            i_status >= (int) CNetScheduleAPI::eLastStatus)
        {
            // Invalid job, skip it
            LOG_POST(Warning << "Job " << job_id
                             << " has invalid status " << i_status
                             << ", ignored.");
            continue;
        }
        TJobStatus status = TJobStatus(i_status);
        status_tracker.SetExactStatusNoLock(job_id, status, true);

        if (status == CNetScheduleAPI::eReading) {
            unsigned group_id = m_JobDB.read_group;
            x_AddToReadGroupNoLock(group_id, job_id);
            if (group_last_id < group_id) group_last_id = group_id;
        }
        if ((status == CNetScheduleAPI::eRunning ||
             status == CNetScheduleAPI::eReading) &&
            run_time_line) {
            // Add object to the first available slot;
            // it is going to be rescheduled or dropped
            // in the background control thread
            run_time_line->AddObject(run_time_line->GetHead(), job_id);
        }
        if (status == CNetScheduleAPI::eRunning)
            running_jobs.set(job_id);
        if (last_id < job_id) last_id = job_id;
        ++recs;
    }
    // Recover worker node info using job runs
    TNSBitVector::enumerator en(running_jobs.first());
    for (; en.valid(); ++en) {
        unsigned job_id = *en;
        CJob job;
        CJob::EJobFetchResult res = job.Fetch(this, job_id);
        if (res != CJob::eJF_Ok) {
            // TODO: check for db error here
            ERR_POST(Error << "Can't read job " << DecorateJobId(job_id)
                           << " while loading status matrix");
            continue;
        }
        const CJobRun* run = job.GetLastRun();
        if (!run) {
            ERR_POST(Error << "No job run for Running job "
                           << DecorateJobId(job_id)
                           << " while loading status matrix");
            continue;
        }
        if (run->GetStatus() != CNetScheduleAPI::eRunning)
            continue;
        SWorkerNodeInfo wni;
        // We don't have auth in job run, but it is not that crucial.
        // Either the node is the old style one, and jobs are going to
        // be failed (or retried) on another node registration with the
        // same host:port, or for the new style nodes with same node id,
        // auth will be fixed on the INIT command.
        wni.node_id = run->GetWorkerNodeId();
        wni.host    = run->GetClientIp();
        wni.port    = run->GetClientPort();
        TJobList jobs;
        m_WorkerNodeList.RegisterNode(wni, jobs);
        unsigned run_timeout = job.GetRunTimeout();
        if (run_timeout == 0) run_timeout = queue_run_timeout;
        unsigned exp_time = run->GetTimeStart() + run_timeout;
        m_WorkerNodeList.AddJob(wni.node_id, job_id, exp_time, 0, false);
    }
    m_LastId.Set(last_id);
    m_GroupLastId.Set(group_last_id);
    return recs;
}


TJobStatus
SLockedQueue::GetJobStatus(unsigned job_id) const
{
    return status_tracker.GetStatus(job_id);
}


bool
SLockedQueue::CountStatus(CJobStatusTracker::TStatusSummaryMap* status_map,
                          const char*                           affinity_token)
{
    unsigned aff_id = 0;
    TNSBitVector aff_jobs;
    if (affinity_token && *affinity_token) {
        aff_id = m_AffinityDict.GetTokenId(affinity_token);
        if (!aff_id)
            return false;
        GetJobsWithAffinity(aff_id, &aff_jobs);
    }

    status_tracker.CountStatus(status_map, aff_id!=0 ? &aff_jobs : 0);

    return true;
}


void SLockedQueue::SetPort(unsigned short port)
{
    if (port) {
        udp_socket.SetReuseAddress(eOn);
        udp_socket.Bind(port);
    }
}


bool SLockedQueue::IsExpired()
{
    int empty_lifetime = CQueueParamAccessor(*this).GetEmptyLifetime();
    CQueueGuard guard(this);
    if (m_Kind && empty_lifetime > 0) {
        unsigned cnt = status_tracker.Count();
        if (cnt) {
            m_BecameEmpty = -1;
        } else {
            if (m_BecameEmpty != -1 &&
                m_BecameEmpty + empty_lifetime < time(0))
            {
                LOG_POST(Info << "Queue " << m_QueueName << " expired."
                    << " Became empty: "
                    << CTime(m_BecameEmpty).ToLocalTime().AsString()
                    << " Empty lifetime: " << empty_lifetime
                    << " sec." );
                return true;
            }
            if (m_BecameEmpty == -1)
                m_BecameEmpty = time(0);
        }
    }
    return false;
}


unsigned int SLockedQueue::GetNextId()
{
    if (m_LastId.Get() >= CAtomicCounter::TValue(kMax_I4))
        m_LastId.Set(0);
    return (unsigned) m_LastId.Add(1);
}


unsigned int SLockedQueue::GetNextIdBatch(unsigned count)
{
    // Modified wrap-around zero, so it will work in the
    // case of batch id - client expect monotonously
    // growing range of ids.
    if (m_LastId.Get() >= CAtomicCounter::TValue(kMax_I4 - count))
        m_LastId.Set(0);
    unsigned int id = (unsigned) m_LastId.Add(count);
    id = id - count + 1;
    return id;
}


void SLockedQueue::ReadJobs(unsigned peer_addr,
                            unsigned count, unsigned timeout,
                            unsigned& read_id, TNSBitVector& jobs)
{
    unsigned group_id = 0;
    time_t curr = time(0); // TODO: move it to parameter???
    time_t exp_time = timeout ? curr + timeout : 0;
    // Get jobs from status tracker here
    status_tracker.SwitchJobs(count,
                              CNetScheduleAPI::eDone, CNetScheduleAPI::eReading,
                              jobs);
    CNetSchedule_JSGroupGuard gr_guard(status_tracker, CNetScheduleAPI::eDone, jobs);

    // Fix the db objects
    CJob job;
    CNS_Transaction trans(this);
    {{
        CQueueGuard guard(this, &trans);
        TNSBitVector::enumerator en(jobs.first());
        for (; en.valid(); ++en) {
            unsigned job_id = *en;
            CJob::EJobFetchResult res = job.Fetch(this, job_id);
            if (res != CJob::eJF_Ok) {
                // TODO: check for db error here
                ERR_POST(Error << "Can't read job " << DecorateJobId(job_id));
                continue;
            }
            if (!group_id) {
                if (m_GroupLastId.Get() >= CAtomicCounter::TValue(kMax_I4))
                    m_GroupLastId.Set(0);
                group_id = (unsigned) m_GroupLastId.Add(1);
            }

            unsigned run_count = job.GetRunCount() + 1;
            CJobRun& run = job.AppendRun();
            run.SetStatus(CNetScheduleAPI::eReading);
            run.SetTimeStart(curr);
            run.SetClientIp(peer_addr);
            job.SetStatus(CNetScheduleAPI::eReading);
            job.SetRunTimeout(timeout);
            job.SetRunCount(run_count);
            job.SetReadGroup(group_id);

            job.Flush(this);
            if (exp_time && run_time_line) {
                // TODO: Optimize locking of rtl lock for every object
                // hoist it out of the loop
                CWriteLockGuard rtl_guard(rtl_lock);
                run_time_line->AddObject(exp_time, job_id);
            }
        }
    }}
    trans.Commit();
    gr_guard.Commit();

    if (group_id)
        x_CreateReadGroup(group_id, jobs);
    read_id = group_id;
}

void SLockedQueue::x_ChangeGroupStatus(unsigned            group_id,
                                       const TNSBitVector& jobs,
                                       TJobStatus          status)
{
    time_t curr = time(0);
    {{
        CFastMutexGuard guard(m_GroupMapLock);
        // Check that the group is valid
        TGroupMap::iterator i = m_GroupMap.find(group_id);
        if (i == m_GroupMap.end())
            NCBI_THROW(CNetScheduleException, eInvalidParameter,
                        "No such read group");

        TNSBitVector& bv = (*i).second;

        // Check that all jobs are in read group
        TNSBitVector check(jobs);
        check -= bv;
        if (check.any())
            NCBI_THROW(CNetScheduleException, eInvalidParameter,
                       "Jobs do not belong to group");

        // Remove jobs from read group, remove group if empty
        bv -= jobs;
        if (!bv.any())
            m_GroupMap.erase(i);
    }}

    // TODO: Introduce read group guard here
    // CNetSchedule_ReadGroupGuard rg_guard(this, jobs);
    
    // Switch state
    CNetSchedule_JSGroupGuard gr_guard(status_tracker,
                                       CNetScheduleAPI::eReading,
                                       jobs,
                                       status);
    // Fix the db objects
    CJob job;
    CNS_Transaction trans(this);
    {{
        CQueueGuard guard(this, &trans);
        TNSBitVector::enumerator en(jobs.first());
        for (; en.valid(); ++en) {
            unsigned job_id = *en;
            CJob::EJobFetchResult res = job.Fetch(this, job_id);
            if (res != CJob::eJF_Ok) {
                // TODO: check for db error here
                ERR_POST(Error << "Can't read job " << DecorateJobId(job_id));
                continue;
            }

            CJobRun* run = job.GetLastRun();
            if (!run) {
                ERR_POST(Error << "No JobRun for reading job "
                               << DecorateJobId(job_id));
                // fix it here as well as we can
                run = &job.AppendRun();
                run->SetTimeStart(curr);
            }
            run->SetStatus(status);
            run->SetTimeDone(curr);
            job.SetStatus(status);

            job.Flush(this);
            if (run_time_line) {
                // TODO: Optimize locking of rtl lock for every object
                // hoist it out of the loop
                CWriteLockGuard rtl_guard(rtl_lock);
                // TODO: Ineffective, better learn expiration time from
                // object, then remove it with another method
                run_time_line->RemoveObject(job_id);
            }
        }
    }}
    trans.Commit();
    gr_guard.Commit();
    // rg_guard.Commit();
}


void SLockedQueue::ConfirmJobs(unsigned read_id, TNSBitVector& bv_jobs)
{
    x_ChangeGroupStatus(read_id, bv_jobs, CNetScheduleAPI::eConfirmed);
}


void SLockedQueue::FailReadingJobs(unsigned read_id, TNSBitVector& bv_jobs)
{
    x_ChangeGroupStatus(read_id, bv_jobs, CNetScheduleAPI::eReadFailed);
}


void SLockedQueue::Erase(unsigned job_id)
{
    status_tracker.Erase(job_id);

    {{
        // Request delayed record delete
        CFastMutexGuard jtd_guard(m_JobsToDeleteLock);
        // TODO: Check that object is not already in these vectors,
        // saving us DB flush
        m_JobsToDelete.set_bit(job_id);
        m_DeletedJobs.set_bit(job_id);
        m_AffJobsToDelete.set_bit(job_id);

        // start affinity erase process
        m_AffWrapped = false;
        m_LastAffId = m_CurrAffId;

        FlushDeletedVectors();
    }}
}


void SLockedQueue::Erase(const TNSBitVector& job_ids)
{
    CFastMutexGuard jtd_guard(m_JobsToDeleteLock);
    m_JobsToDelete    |= job_ids;
    m_DeletedJobs     |= job_ids;
    m_AffJobsToDelete |= job_ids;

    // start affinity erase process
    m_AffWrapped = false;
    m_LastAffId = m_CurrAffId;

    FlushDeletedVectors();
}


void SLockedQueue::Clear()
{
    TNSBitVector bv;

    {{
        CWriteLockGuard rtl_guard(rtl_lock);
        // TODO: interdependency btw status_tracker.lock and rtl_lock
        status_tracker.ClearAll(&bv);
        run_time_line->ReInit(0);
    }}

    Erase(bv);
}


void SLockedQueue::FlushDeletedVectors(EVectorId vid)
{
    static EVectorId all_ids[] = { eVIJob, eVITag, eVIAffinity };
    TNSBitVector all_vects[] = 
        { m_JobsToDelete, m_DeletedJobs, m_AffJobsToDelete };
    for (size_t i = 0; i < sizeof(all_ids) / sizeof(all_ids[0]); ++i) {
        if (vid != eVIAll && vid != all_ids[i]) continue;
        m_DeletedJobsDB.id = all_ids[i];
        TNSBitVector& bv = all_vects[i];
        bv.optimize();
        m_DeletedJobsDB.WriteVector(bv, SDeletedJobsDB::eNoCompact);
    }
}


void SLockedQueue::FilterJobs(TNSBitVector& ids)
{
    TNSBitVector alive_jobs;
    status_tracker.GetAliveJobs(alive_jobs);
    ids &= alive_jobs;
    //CFastMutexGuard guard(m_JobsToDeleteLock);
    //ids -= m_DeletedJobs;
}

void SLockedQueue::ClearAffinityIdx()
{
    const unsigned kDeletedJobsThreshold = 10000;
    const unsigned kAffBatchSize = 1000;
    // thread-safe copies of progress pointers
    unsigned curr_aff_id;
    unsigned last_aff_id;
    {{
        // Ensure that we have some job to do
        CFastMutexGuard jtd_guard(m_JobsToDeleteLock);
        // TODO: calculate (job_to_delete_count * maturity) instead
        // of just job count. Provides more safe version - even a
        // single deleted job will be eventually cleaned up.
        if (m_AffJobsToDelete.count() < kDeletedJobsThreshold)
            return;
        curr_aff_id = m_CurrAffId;
        last_aff_id = m_LastAffId;
    }}

    TNSBitVector bv(bm::BM_GAP);

    // mark if we are wrapped and chasing the "tail"
    bool wrapped = curr_aff_id < last_aff_id;

    // get batch of affinity tokens in the index
    {{
        CFastMutexGuard guard(m_AffinityIdxLock);
        m_AffinityIdx.SetTransaction(0);
        CBDB_FileCursor cur(m_AffinityIdx);
        cur.SetCondition(CBDB_FileCursor::eGE);
        cur.From << curr_aff_id;

        unsigned n = 0;
        EBDB_ErrCode ret;
        for (; (ret = cur.Fetch()) == eBDB_Ok && n < kAffBatchSize; ++n) {
            curr_aff_id = m_AffinityIdx.aff_id;
            if (wrapped && curr_aff_id >= last_aff_id) // run over the tail
                break;
            bv.set(curr_aff_id);
        }
        if (ret != eBDB_Ok) {
            if (ret != eBDB_NotFound)
                ERR_POST(Error << "Error reading affinity index");
            if (wrapped) {
                curr_aff_id = last_aff_id;
            } else {
                // wrap-around
                curr_aff_id = 0;
                wrapped = true;
                cur.SetCondition(CBDB_FileCursor::eGE);
                cur.From << curr_aff_id;
                for (; n < kAffBatchSize && (ret = cur.Fetch()) == eBDB_Ok; ++n) {
                    curr_aff_id = m_AffinityIdx.aff_id;
                    if (curr_aff_id >= last_aff_id) // run over the tail
                        break;
                    bv.set(curr_aff_id);
                }
                if (ret != eBDB_NotFound)
                    ERR_POST(Error << "Error reading affinity index");
            }
        }
    }}

    {{
        CFastMutexGuard jtd_guard(m_JobsToDeleteLock);
        m_AffWrapped = wrapped;
        m_CurrAffId = curr_aff_id;
    }}

    // clear all hanging references
    TNSBitVector::enumerator en(bv.first());
    for (; en.valid(); ++en) {
        unsigned aff_id = *en;
        CNS_Transaction trans(this);
        CFastMutexGuard guard(m_AffinityIdxLock);
        m_AffinityIdx.SetTransaction(&trans);

        TNSBitVector bvect(bm::BM_GAP);
        m_AffinityIdx.aff_id = aff_id;
        EBDB_ErrCode ret =
            m_AffinityIdx.ReadVector(&bvect, bm::set_OR, NULL);
        if (ret != eBDB_Ok) {
            if (ret != eBDB_NotFound)
                ERR_POST(Error << "Error reading affinity index");
            continue;
        }
        unsigned old_count = bvect.count();
        bvect -= m_AffJobsToDelete;
        unsigned new_count = bvect.count();
        if (new_count == old_count) {
            continue;
        }
        m_AffinityIdx.aff_id = aff_id;
        if (bvect.any()) {
            bvect.optimize();
            m_AffinityIdx.WriteVector(bvect, SAffinityIdx::eNoCompact);
        } else {
            // TODO: if there is no record in m_AffinityMap,
            // remove record from SAffinityDictDB
            // As of now, token stays indefinitely, even if empty.
            // NB: Potential DEADLOCK, see NB in FindPendingJob
            //{{
            //    CFastMutexGuard aff_guard(m_AffinityMapLock);
            //    if (!m_AffinityMap.CheckAffinity(aff_id);
            //        m_AffinityDict.RemoveToken(aff_id, trans);
            //}}
            m_AffinityIdx.Delete();
        }
        trans.Commit();
//        cout << aff_id << " cleaned" << endl;
    } // for

    {{
        CFastMutexGuard jtd_guard(m_JobsToDeleteLock);
        if (m_AffWrapped && m_CurrAffId >= m_LastAffId) {
            m_AffJobsToDelete.clear(true);
            FlushDeletedVectors(eVIAffinity);
        }
    }}
}


void SLockedQueue::Notify(unsigned addr, unsigned short port, unsigned job_id)
{
    char msg[1024];
    sprintf(msg, "JNTF %u", job_id);

    CFastMutexGuard guard(us_lock);

    udp_socket.Send(msg, strlen(msg)+1,
                    CSocketAPI::ntoa(addr), port);
}


void SLockedQueue::OptimizeMem()
{
    status_tracker.OptimizeMem();
}


void SLockedQueue::AddJobsToAffinity(CBDB_Transaction& trans,
                                     unsigned aff_id, 
                                     unsigned job_id_from,
                                     unsigned job_id_to)

{
    CFastMutexGuard guard(m_AffinityIdxLock);
    m_AffinityIdx.SetTransaction(&trans);
    TNSBitVector bv(bm::BM_GAP);

    // check if vector is in the database

    // read vector from the file
    m_AffinityIdx.aff_id = aff_id;
    /*EBDB_ErrCode ret = */
    m_AffinityIdx.ReadVector(&bv, bm::set_OR, NULL);
    if (job_id_to == 0) {
        bv.set(job_id_from);
    } else {
        bv.set_range(job_id_from, job_id_to);
    }
    m_AffinityIdx.aff_id = aff_id;
    m_AffinityIdx.WriteVector(bv, SAffinityIdx::eNoCompact);
}


void SLockedQueue::AddJobsToAffinity(CBDB_Transaction& trans,
                                     const vector<CJob>& batch)
{
    CFastMutexGuard guard(m_AffinityIdxLock);
    m_AffinityIdx.SetTransaction(&trans);
    typedef map<unsigned, TNSBitVector*> TBVMap;

    TBVMap  bv_map;
    try {
        unsigned bsize = batch.size();
        for (unsigned i = 0; i < bsize; ++i) {
            const CJob& job = batch[i];
            unsigned aff_id = job.GetAffinityId();
            unsigned job_id_start = job.GetId();

            TNSBitVector* aff_bv;

            TBVMap::iterator aff_it = bv_map.find(aff_id);
            if (aff_it == bv_map.end()) { // new element
                auto_ptr<TNSBitVector> bv(new TNSBitVector(bm::BM_GAP));
                m_AffinityIdx.aff_id = aff_id;
                /*EBDB_ErrCode ret = */
                m_AffinityIdx.ReadVector(bv.get(), bm::set_OR, NULL);
                aff_bv = bv.get();
                bv_map[aff_id] = bv.release();
            } else {
                aff_bv = aff_it->second;
            }


            // look ahead for the same affinity id
            unsigned j;
            for (j=i+1; j < bsize; ++j) {
                if (batch[j].GetAffinityId() != aff_id) {
                    break;
                }
                _ASSERT(batch[j].GetId() == (batch[j-1].GetId()+1));
                //job_id_end = batch[j].GetId();
            }
            --j;

            if ((i!=j) && (aff_id == batch[j].GetAffinityId())) {
                unsigned job_id_end = batch[j].GetId();
                aff_bv->set_range(job_id_start, job_id_end);
                i = j;
            } else { // look ahead failed
                aff_bv->set(job_id_start);
            }

        } // for

        // save all changes to the database
        NON_CONST_ITERATE(TBVMap, it, bv_map) {
            unsigned aff_id = it->first;
            TNSBitVector* bv = it->second;
            bv->optimize();

            m_AffinityIdx.aff_id = aff_id;
            m_AffinityIdx.WriteVector(*bv, SAffinityIdx::eNoCompact);

            delete it->second; it->second = 0;
        }
    }
    catch (exception& )
    {
        NON_CONST_ITERATE(TBVMap, it, bv_map) {
            delete it->second; it->second = 0;
        }
        throw;
    }

}


void 
SLockedQueue::GetJobsWithAffinity(const TNSBitVector& aff_id_set,
                                  TNSBitVector*       jobs)
{
    CFastMutexGuard guard(m_AffinityIdxLock);
    m_AffinityIdx.SetTransaction(NULL);
    TNSBitVector::enumerator en(aff_id_set.first());
    for (; en.valid(); ++en) {  // for each affinity id
        unsigned aff_id = *en;
        x_ReadAffIdx_NoLock(aff_id, jobs);
    }
}


void
SLockedQueue::GetJobsWithAffinity(unsigned      aff_id,
                                  TNSBitVector* jobs)
{
    CFastMutexGuard guard(m_AffinityIdxLock);
    m_AffinityIdx.SetTransaction(NULL);
    x_ReadAffIdx_NoLock(aff_id, jobs);
}


unsigned
SLockedQueue::FindPendingJob(const string& node_id, time_t curr)
{
    unsigned job_id = 0;

    TNSBitVector blacklisted_jobs;

    // affinity: get list of job candidates
    // previous FindPendingJob() call may have precomputed candidate jobids
    {{
        CFastMutexGuard aff_guard(m_AffinityMapLock);
        CWorkerNodeAffinity::SAffinityInfo* ai =
            m_AffinityMap.GetAffinity(node_id);

        if (ai) {  // established affinity association
            blacklisted_jobs = ai->GetBlacklistedJobs(curr);
            do {
                // check for candidates
                if (!ai->candidate_jobs.any() && ai->aff_ids.any()) {
                    // there is an affinity association
                    // NB: locks m_AffinityIdxLock, thereby creating
                    // m_AffinityMapLock < m_AffinityIdxLock locking order
                    GetJobsWithAffinity(ai->aff_ids, &ai->candidate_jobs);
                    if (!ai->candidate_jobs.any()) // no candidates
                        break;
                    status_tracker.PendingIntersect(&ai->candidate_jobs);
                    ai->candidate_jobs -= blacklisted_jobs;
                    ai->candidate_jobs.count(); // speed up any()
                    if (!ai->candidate_jobs.any())
                        break;
                    ai->candidate_jobs.optimize(0, TNSBitVector::opt_free_0);
                }
                if (!ai->candidate_jobs.any())
                    break;
                bool pending_jobs_avail =
                    status_tracker.GetPendingJobFromSet(
                        &ai->candidate_jobs, &job_id);
                if (!job_id && !pending_jobs_avail)
                    return 0;
            } while (0);
        }
    }}

    // no affinity association or there are no more jobs with
    // established affinity

    // try to find a vacant(not taken by any other worker node) affinity id
    if (!job_id) {
        TNSBitVector assigned_aff;
        {{
            CFastMutexGuard aff_guard(m_AffinityMapLock);
            m_AffinityMap.GetAllAssignedAffinity(assigned_aff);
        }}

        if (assigned_aff.any()) {
            // get all jobs belonging to other (already assigned) affinities,
            // ORing them with our own blacklisted jobs
            TNSBitVector assigned_candidate_jobs(blacklisted_jobs);
            // GetJobsWithAffinity actually ORs into second argument
            GetJobsWithAffinity(assigned_aff, &assigned_candidate_jobs);
            // we got list of jobs we do NOT want to schedule
            bool pending_jobs_avail =
                status_tracker.GetPendingJob(assigned_candidate_jobs,
                                             &job_id);
            if (!job_id && !pending_jobs_avail)
                return 0;
        }
    }

    // We just take the first available job in the queue, taking into account
    // blacklisted jobs as usual.
    if (!job_id)
    	status_tracker.GetPendingJob(blacklisted_jobs, &job_id);

    return job_id;
}


bool SLockedQueue::FailJob(unsigned      job_id,
                           const string& err_msg,
                           const string& output,
                           int           ret_code,
                           const string* node_id)
{
    unsigned failed_retries;
    unsigned max_output_size;
    unsigned blacklist_time;
    {{
        CQueueParamAccessor qp(*this);
        failed_retries  = qp.GetFailedRetries();
        max_output_size = qp.GetMaxOutputSize();
        blacklist_time  = qp.GetBlacklistTime();
    }}

    if (output.size() > max_output_size) {
        NCBI_THROW(CNetScheduleException, eDataTooLong,
           "Output is too long");
    }
    // We first change memory state to "Failed", it is safer because
    // there is only danger to find job in inconsistent state, and because
    // Failed is terminal, usually you can not allocate job or do anything
    // disturbing from this state.
    CQueueJSGuard js_guard(this, job_id,
                           CNetScheduleAPI::eFailed);

    CJob job;
    CNS_Transaction trans(this);

    time_t curr = time(0);

    bool rescheduled = false;
    {{
        CQueueGuard guard(this, &trans);

        CJob::EJobFetchResult res = job.Fetch(this, job_id);
        if (res != CJob::eJF_Ok) {
            // TODO: Integrity error or job just expired?
            LOG_POST(Error << "Can not fetch job " << DecorateJobId(job_id));
            return false;
        }

        unsigned run_count = job.GetRunCount();
        if (run_count <= failed_retries) {
            time_t exp_time = blacklist_time ? curr + blacklist_time : 0;
            if (node_id) BlacklistJob(*node_id, job_id, exp_time);
            job.SetStatus(CNetScheduleAPI::ePending);
            js_guard.SetStatus(CNetScheduleAPI::ePending);
            rescheduled = true;
            LOG_POST(Warning << "Job " << DecorateJobId(job_id)
                             << " rescheduled with "
                             << (failed_retries - run_count)
                             << " retries left");
        } else {
            job.SetStatus(CNetScheduleAPI::eFailed);
            rescheduled = false;
            LOG_POST(Warning << "Job " << DecorateJobId(job_id)
                             << " failed");
        }

        CJobRun* run = job.GetLastRun();
        if (!run) {
            ERR_POST(Error << "No JobRun for running job "
                           << DecorateJobId(job_id));
            run = &job.AppendRun();
        }

        run->SetStatus(CNetScheduleAPI::eFailed);
        run->SetTimeDone(curr);
        run->SetErrorMsg(err_msg);
        run->SetRetCode(ret_code);
        job.SetOutput(output);

        job.Flush(this);
    }}

    trans.Commit();
    js_guard.Commit();

    if (run_time_line) {
        CWriteLockGuard guard(rtl_lock);
        run_time_line->RemoveObject(job_id);
    }

    if (!rescheduled  &&  job.ShouldNotify(curr)) {
        Notify(job.GetSubmAddr(), job.GetSubmPort(), job_id);
    }

    if (IsMonitoring()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::JobFailed() job id=";
        msg += NStr::IntToString(job_id);
        msg += " err_msg=";
        msg += err_msg;
        msg += " output=";
        msg += output;
        if (rescheduled)
            msg += " rescheduled";
        MonitorPost(msg);
    }
    return true;
}


void
SLockedQueue::x_ReadAffIdx_NoLock(unsigned      aff_id,
                                  TNSBitVector* jobs)
{
    m_AffinityIdx.aff_id = aff_id;
    m_AffinityIdx.ReadVector(jobs, bm::set_OR, NULL);
}


// Worker node


void SLockedQueue::InitWorkerNode(const SWorkerNodeInfo& node_info)
{
    TJobList jobs;
    m_WorkerNodeList.RegisterNode(node_info, jobs);
    x_FailJobsAtNodeClose(jobs);
}


void SLockedQueue::ClearWorkerNode(const string& node_id)
{
    TJobList jobs;
    m_WorkerNodeList.ClearNode(node_id, jobs);
    {{
    	CFastMutexGuard guard(m_AffinityMapLock);
    	m_AffinityMap.ClearAffinity(node_id);
    }}
    x_FailJobsAtNodeClose(jobs);
}


void SLockedQueue::NotifyListeners(bool unconditional, unsigned aff_id)
{
    // TODO: if affinity valency is full for this aff_id, notify only nodes
    // with this aff_id, otherwise notify all nodes in the hope that some
    // of them will pick up the task with this aff_id
    
    int notify_timeout = CQueueParamAccessor(*this).GetNotifyTimeout();

    time_t curr = time(0);

    list<TWorkerNodeHostPort> notify_list;
    if ((unconditional || status_tracker.AnyPending()) &&
        m_WorkerNodeList.GetNotifyList(unconditional, curr,
                                       notify_timeout, notify_list)) {

        const char* msg = q_notif.c_str();
        size_t msg_len = q_notif.length()+1;

        unsigned i = 0;
        ITERATE(list<TWorkerNodeHostPort>, it, notify_list) {
            unsigned host = it->first;
            unsigned short port = it->second;
            {{
                CFastMutexGuard guard(us_lock);
                //EIO_Status status =
                    udp_socket.Send(msg, msg_len,
                                       CSocketAPI::ntoa(host), port);
            }}
            // periodically check if we have no more jobs left
            if ((++i % 10 == 0) && !status_tracker.AnyPending())
                break;
        }
    }
}


void SLockedQueue::PrintWorkerNodeStat(CNcbiOstream& out) const
{
    time_t curr = time(0);

    list<string> nodes_info;
    m_WorkerNodeList.GetNodesInfo(curr, nodes_info);
    ITERATE(list<string>, it, nodes_info) {
        out << *it << "\n";
    }
}


void SLockedQueue::RegisterNotificationListener(const SWorkerNodeInfo& node_info,
                                                unsigned short         port,
                                                unsigned               timeout)
{
    TJobList jobs;
    m_WorkerNodeList.RegisterNotificationListener(node_info, port,
                                                  timeout, jobs);
    x_FailJobsAtNodeClose(jobs);
}


bool SLockedQueue::UnRegisterNotificationListener(const string& node_id)
{
    TJobList jobs;
    bool final =
    	m_WorkerNodeList.UnRegisterNotificationListener(node_id, jobs);
    if (final) {
    	// Clean affinity association only for old style worker nodes
    	// New style nodes should explicitely call ClearWorkerNode
    	{{
    		CFastMutexGuard guard(m_AffinityMapLock);
    		m_AffinityMap.ClearAffinity(node_id);
    	}}
    	x_FailJobsAtNodeClose(jobs);
    }
    return final;
}


void SLockedQueue::RegisterWorkerNodeVisit(SWorkerNodeInfo& node_info)
{
    TJobList jobs;
    if (m_WorkerNodeList.RegisterNodeVisit(node_info, jobs))
        x_FailJobsAtNodeClose(jobs);
}


void SLockedQueue::AddJobToWorkerNode(const SWorkerNodeInfo&  node_info,
                                      CRequestContextFactory* rec_ctx_f,
                                      unsigned                job_id,
                                      time_t                  exp_time)
{
    bool log_job_state = CQueueParamAccessor(*this).GetLogJobState();
    m_WorkerNodeList.AddJob(node_info.node_id, job_id, exp_time,
                            rec_ctx_f, log_job_state);
    CountEvent(eStatGetEvent);
}


void SLockedQueue::UpdateWorkerNodeJob(const SWorkerNodeInfo& node_info,
                                       unsigned               job_id,
                                       time_t                 exp_time)
{
    m_WorkerNodeList.UpdateJob(node_info.node_id, job_id, exp_time);
}


void SLockedQueue::RemoveJobFromWorkerNode(const SWorkerNodeInfo& node_info,
                                           unsigned               job_id,
                                           ENSCompletion          reason)
{
    bool log_job_state = CQueueParamAccessor(*this).GetLogJobState();
    m_WorkerNodeList.RemoveJob(node_info.node_id, job_id, reason, log_job_state);
    CountEvent(eStatPutEvent);
}



void SLockedQueue::x_FailJobsAtNodeClose(TJobList& jobs)
{
    ITERATE(TJobList, it, jobs) {
    	FailJob(*it, "Node closed", "", 0);
    }
}

// Affinity

void SLockedQueue::ClearAffinity(const string& node_id)
{
    CFastMutexGuard guard(m_AffinityMapLock);
    m_AffinityMap.ClearAffinity(node_id);
}


void SLockedQueue::AddAffinity(const string& node_id,
                               unsigned      aff_id,
                               time_t        exp_time)
{
    CFastMutexGuard guard(m_AffinityMapLock);
    m_AffinityMap.AddAffinity(node_id, aff_id, exp_time);
}


void SLockedQueue::BlacklistJob(const string& node_id,
                                unsigned      job_id,
                                time_t        exp_time)
{
    CFastMutexGuard guard(m_AffinityMapLock);
    m_AffinityMap.BlacklistJob(node_id, job_id, exp_time);
}


// Tags

void SLockedQueue::SetTagDbTransaction(CBDB_Transaction* trans)
{
    m_TagDB.SetTransaction(trans);
}


void SLockedQueue::AppendTags(CNSTagMap& tag_map,
                              const TNSTagList& tags,
                              unsigned job_id)
{
    ITERATE(TNSTagList, it, tags) {
        /*
        auto_ptr<TNSBitVector> bv(new TNSBitVector(bm::BM_GAP));
        pair<TNSTagMap::iterator, bool> tag_map_it =
            (*tag_map).insert(TNSTagMap::value_type((*it), bv.get()));
        if (tag_map_it.second) {
            bv.release();
        }
        */
        TNSTagMap::iterator tag_it = (*tag_map).find((*it));
        if (tag_it == (*tag_map).end()) {
            pair<TNSTagMap::iterator, bool> tag_map_it =
//                (*tag_map).insert(TNSTagMap::value_type((*it), new TNSBitVector(bm::BM_GAP)));
                (*tag_map).insert(TNSTagMap::value_type((*it), new TNSShortIntSet));
            tag_it = tag_map_it.first;
        }
//        tag_it->second->set(job_id);
        tag_it->second->push_back(job_id);
    }
}


void SLockedQueue::FlushTags(CNSTagMap& tag_map, CBDB_Transaction& trans)
{
    CFastMutexGuard guard(m_TagLock);
    m_TagDB.SetTransaction(&trans);
    NON_CONST_ITERATE(TNSTagMap, it, *tag_map) {
        m_TagDB.key = it->first.first;
        m_TagDB.val = it->first.second;
        /*
        EBDB_ErrCode err = m_TagDB.ReadVector(it->second, bm::set_OR);
        if (err != eBDB_Ok && err != eBDB_NotFound) {
            // TODO: throw db error
        }
        m_TagDB.key = it->first.first;
        m_TagDB.val = it->first.second;
        it->second->optimize();
        if (it->first.first == "transcript") {
            it->second->stat();
        }
        m_TagDB.WriteVector(*(it->second), STagDB::eNoCompact);
        */

        TNSBitVector bv_tmp(bm::BM_GAP);
        EBDB_ErrCode err = m_TagDB.ReadVector(&bv_tmp);
        if (err != eBDB_Ok && err != eBDB_NotFound) {
            // TODO: throw db error
        }
        bm::combine_or(bv_tmp, it->second->begin(), it->second->end());

        m_TagDB.key = it->first.first;
        m_TagDB.val = it->first.second;
        m_TagDB.WriteVector(bv_tmp, STagDB::eNoCompact);

        delete it->second;
        it->second = 0;
    }
    (*tag_map).clear();
}


bool SLockedQueue::ReadTag(const string& key,
                           const string& val,
                           TBuffer* buf)
{
    // Guarded by m_TagLock through GetTagLock()
    CBDB_FileCursor cur(m_TagDB);
    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << key << val;

    return cur.Fetch(buf) == eBDB_Ok;
}


void SLockedQueue::ReadTags(const string& key, TNSBitVector* bv)
{
    // Guarded by m_TagLock through GetTagLock()
    CBDB_FileCursor cur(m_TagDB);
    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << key;
    TBuffer buf;
    bm::set_operation op_code = bm::set_ASSIGN;
    EBDB_ErrCode err;
    while ((err = cur.Fetch(&buf)) == eBDB_Ok) {
        bm::operation_deserializer<TNSBitVector>::deserialize(
            *bv, &(buf[0]), 0, op_code);
        op_code = bm::set_OR;
    }
    if (err != eBDB_Ok  &&  err != eBDB_NotFound) {
        // TODO: signal disaster somehow, e.g. throw CBDB_Exception
    }
}


void SLockedQueue::x_RemoveTags(CBDB_Transaction& trans,
                                const TNSBitVector& ids)
{
    CFastMutexGuard guard(m_TagLock);
    m_TagDB.SetTransaction(&trans);
    CBDB_FileCursor cur(m_TagDB, trans,
                        CBDB_FileCursor::eReadModifyUpdate);
    // iterate over tags database, deleting ids from every entry
    cur.SetCondition(CBDB_FileCursor::eFirst);
    CBDB_RawFile::TBuffer buf;
    TNSBitVector bv;
    while (cur.Fetch(&buf) == eBDB_Ok) {
        bm::deserialize(bv, &buf[0]);
        unsigned before_remove = bv.count();
        bv -= ids;
        unsigned new_count;
        if ((new_count = bv.count()) != before_remove) {
            if (new_count) {
                TNSBitVector::statistics st;
                bv.optimize(0, TNSBitVector::opt_compress, &st);
                if (st.max_serialize_mem > buf.size()) {
                    buf.resize(st.max_serialize_mem);
                }

                size_t size = bm::serialize(bv, &buf[0]);
                cur.UpdateBlob(&buf[0], size);
            } else {
                cur.Delete(CBDB_File::eIgnoreError);
            }
        }
        bv.clear(true);
    }
}


//

void SLockedQueue::CheckExecutionTimeout()
{
    if (!run_time_line)
        return;
    unsigned queue_run_timeout = CQueueParamAccessor(*this).GetRunTimeout();
    time_t curr = time(0);
    TNSBitVector bv;
    {{
        CReadLockGuard guard(rtl_lock);
        run_time_line->ExtractObjects(curr, &bv);
    }}
    TNSBitVector::enumerator en(bv.first());
    for ( ;en.valid(); ++en) {
        x_CheckExecutionTimeout(queue_run_timeout, *en, curr);
    }
}


void
SLockedQueue::x_CheckExecutionTimeout(unsigned queue_run_timeout,
                                      unsigned job_id, time_t curr_time)
{
    bool log_job_state = CQueueParamAccessor(*this).GetLogJobState();
    TJobStatus status = GetJobStatus(job_id);

    TJobStatus new_status, run_status;
    if (status == CNetScheduleAPI::eRunning) {
        new_status = CNetScheduleAPI::ePending;
        run_status = CNetScheduleAPI::eTimeout;
    } else if (status == CNetScheduleAPI::eReading) {
        new_status = CNetScheduleAPI::eDone;
        run_status = CNetScheduleAPI::eReadTimeout;
    } else {
        return;
    }

    CJob job;
    CNS_Transaction trans(this);

    unsigned time_start, run_timeout;
    time_t   exp_time;
    string worker_node_id;
    {{
        CQueueGuard guard(this, &trans);

        CJob::EJobFetchResult res = job.Fetch(this, job_id);
        if (res != CJob::eJF_Ok)
            return;

        if (job.GetStatus() != status) {
            ERR_POST(Error << "Job status mismatch between status tracker"
                           << " and database for job " << DecorateJobId(job_id));
            return;
        }

        CJobRun* run = job.GetLastRun();
        if (!run) {
            ERR_POST(Error << "No JobRun for running job "
                           << DecorateJobId(job_id));
            // fix it here as well as we can
            run = &job.AppendRun();
            run->SetTimeStart(curr_time);
        }
        time_start = run->GetTimeStart();
        _ASSERT(time_start);
        run_timeout = job.GetRunTimeout();
        if (run_timeout == 0) run_timeout = queue_run_timeout;

        exp_time = run_timeout ? time_start + run_timeout : 0;
        if (curr_time < exp_time) {
           // we need to register job in timeline
            CWriteLockGuard guard(rtl_lock);
            run_time_line->AddObject(exp_time, job_id);
            return;
        }

        if (status == CNetScheduleAPI::eReading) {
            unsigned group = job.GetReadGroup();
            x_RemoveFromReadGroup(group, job_id);
        }

        job.SetStatus(new_status);
        run->SetStatus(run_status);
        run->SetTimeDone(curr_time);

        worker_node_id = run->GetWorkerNodeId();

        job.Flush(this);
    }}

    trans.Commit();

    status_tracker.SetStatus(job_id, new_status);

    if (status == CNetScheduleAPI::eRunning)
        m_WorkerNodeList.RemoveJob(worker_node_id, job_id,
                                   eNSCTimeout, log_job_state);

    {{
        CTime tm(CTime::eCurrent);
        string msg = tm.AsString();
        msg += " CQueue::CheckExecutionTimeout: Job rescheduled for ";
        if (status == CNetScheduleAPI::eRunning)
            msg += "execution";
        else
            msg += "reading";
        msg += " id=";
        msg += NStr::IntToString(job_id);
        tm.SetTimeT(time_start);
        tm.ToLocalTime();
        msg += " time_start=";
        msg += tm.AsString();

        tm.SetTimeT(exp_time);
        tm.ToLocalTime();
        msg += " exp_time=";
        msg += tm.AsString();
        msg += " run_timeout(sec)=";
        msg += NStr::IntToString(run_timeout);
        msg += " run_timeout(minutes)=";
        msg += NStr::IntToString(run_timeout/60);
        LOG_POST(Warning << msg);

        if (IsMonitoring()) {
            MonitorPost(msg);
        }
    }}
}


unsigned 
SLockedQueue::CheckJobsExpiry(unsigned batch_size, TJobStatus status)
{
    unsigned queue_timeout, queue_run_timeout;
    {{
        CQueueParamAccessor qp(*this);
        queue_timeout = qp.GetTimeout();
        queue_run_timeout = qp.GetRunTimeout();
    }}

    TNSBitVector job_ids;
    TNSBitVector not_found_jobs;
    time_t curr = time(0);
    CJob job;
    unsigned del_count = 0;
    bool has_db_error = false;
#ifdef _DEBUG
    static bool debug_job_discrepancy = false;
#endif
    {{
        CQueueGuard guard(this);
        unsigned job_id = 0;
        for (unsigned n = 0; n < batch_size; ++n) {
            job_id = status_tracker.GetNext(status, job_id);
            if (job_id == 0)
                break;
            // FIXME: should fetch only main part of the job and run info,
            // does not need potentially huge tags and input/output
            CJob::EJobFetchResult res = job.Fetch(this, job_id);
            if (res != CJob::eJF_Ok
#ifdef _DEBUG
                || debug_job_discrepancy
#endif
            ) {
                if (res != CJob::eJF_NotFound)
                    has_db_error = true;
                // ? already deleted - report as a batch later
                not_found_jobs.set_bit(job_id);
                status_tracker.Erase(job_id);
                // Do not break the process of erasing expired jobs
                continue;
            }

            // Is the job expired?
            time_t time_update, time_done;
            time_t timeout, run_timeout;

            TJobStatus status = job.GetStatus();

            timeout = job.GetTimeout();
            if (timeout == 0) timeout = queue_timeout;
            run_timeout = job.GetRunTimeout();
            if (run_timeout == 0) run_timeout = queue_run_timeout;

            // Calculate time of last update and effective timeout
            const CJobRun* run = job.GetLastRun();
            time_done = 0;
            if (run) time_done = run->GetTimeDone();
            if (status == CNetScheduleAPI::eRunning ||
                status == CNetScheduleAPI::eReading) {
                // Running/reading job
                if (run) {
                    time_update = run->GetTimeStart();
                } else {
                    ERR_POST(Error << "No JobRun for running/reading job "
                                   << DecorateJobId(job_id));
                    time_update = 0; // ??? force job deletion
                }
                timeout += run_timeout;
            } else if (time_done == 0) {
                // Submitted job
                time_update = job.GetTimeSubmit();
            } else {
                // Done, Failed, Canceled, Reading, Confirmed, ReadFailed
                time_update = time_done;
            }

            if (time_update + timeout <= curr) {
                status_tracker.Erase(job_id);
                job_ids.set_bit(job_id);
                if (status == CNetScheduleAPI::eReading) {
                    unsigned group_id = job.GetReadGroup();
                    x_RemoveFromReadGroup(group_id, job_id);
                }
                ++del_count;
            }
        }
    }}
    if (not_found_jobs.any()) {
        if (has_db_error) {
            LOG_POST(Error << "While deleting, errors in database"
                    << ", status " << CNetScheduleAPI::StatusToString(status)
                    << ", queue "  << m_QueueName
                    << ", job(s) " << NS_EncodeBitVector(not_found_jobs));
        } else {
            LOG_POST(Warning << "While deleting, jobs not found in database"
                    << ", status " << CNetScheduleAPI::StatusToString(status)
                    << ", queue "  << m_QueueName
                    << ", job(s) " << NS_EncodeBitVector(not_found_jobs));
        }
    }
    if (del_count)
        Erase(job_ids);
    return del_count;
}


unsigned SLockedQueue::DeleteBatch(unsigned batch_size)
{
    TNSBitVector batch;
    {{
        CFastMutexGuard guard(m_JobsToDeleteLock);
        unsigned job_id = 0;
        for (unsigned n = 0; n < batch_size &&
                             (job_id = m_JobsToDelete.extract_next(job_id));
             ++n)
        {
            batch.set(job_id);
        }
        if (batch.any())
            FlushDeletedVectors(eVIJob);
    }}
    if (batch.none()) return 0;

    unsigned actual_batch_size = batch.count();
    unsigned chunks = (actual_batch_size + 999) / 1000;
    unsigned chunk_size = actual_batch_size / chunks;
    unsigned residue = actual_batch_size - chunks*chunk_size;

    TNSBitVector::enumerator en = batch.first();
    unsigned del_rec = 0;
    while (en.valid()) {
        unsigned txn_size = chunk_size;
        if (residue) {
            ++txn_size; --residue;
        }

        CNS_Transaction trans(this);
        CQueueGuard guard(this, &trans);

        unsigned n;
        for (n = 0; en.valid() && n < txn_size; ++en, ++n) {
            unsigned job_id = *en;
            m_JobDB.id = job_id;
            try {
                m_JobDB.Delete();
                ++del_rec;
            } catch (CBDB_ErrnoException& ex) {
                ERR_POST(Error << "BDB error " << ex.what());
            }

            m_JobInfoDB.id = job_id;
            try {
                m_JobInfoDB.Delete();
            } catch (CBDB_ErrnoException& ex) {
                ERR_POST(Error << "BDB error " << ex.what());
            }
        }
        trans.Commit();
        // x_RemoveTags(trans, batch);
    }
    return del_rec;
}


CBDB_FileCursor& SLockedQueue::GetRunsCursor()
{
    CBDB_Transaction* trans = m_RunsDB.GetBDBTransaction();
    CBDB_FileCursor* cur = m_RunsCursor.get();
    if (!cur) {
        cur = new CBDB_FileCursor(m_RunsDB);
        m_RunsCursor.reset(cur);
    } else {
        cur->Close();
    }
    cur->ReOpen(trans);
    return *cur;
}


void SLockedQueue::SetMonitorSocket(CSocket& socket)
{
    m_Monitor.SetSocket(socket);
}


bool SLockedQueue::IsMonitoring()
{
    return m_Monitor.IsMonitorActive();
}


void SLockedQueue::MonitorPost(const string& msg)
{
    m_Monitor.SendString(msg+'\n');
}


unsigned SLockedQueue::CountRecs()
{
    CQueueGuard guard(this);
    return m_JobDB.CountRecs();
}


void SLockedQueue::PrintStat(CNcbiOstream& out)
{
    CQueueGuard guard(this);
    m_JobDB.PrintStat(out);
}


const unsigned kMeasureInterval = 1;
// for informational purposes only, see kDecayExp below
const unsigned kDecayInterval = 10;
const unsigned kFixedShift = 7;
const unsigned kFixed_1 = 1 << kFixedShift;
// kDecayExp = 2 ^ kFixedShift / 2 ^ ( kMeasureInterval * log2(e) / kDecayInterval)
const unsigned kDecayExp = 116;

SLockedQueue::CStatisticsThread::CStatisticsThread(TContainer& container)
    : CThreadNonStop(kMeasureInterval),
        m_Container(container)
{
}


void SLockedQueue::CStatisticsThread::DoJob(void) {
    unsigned counter;
    for (TStatEvent n = 0; n < eStatNumEvents; ++n) {
        counter = m_Container.m_EventCounter[n].Get();
        m_Container.m_EventCounter[n].Add(-counter);
        m_Container.m_Average[n] = (kDecayExp * m_Container.m_Average[n] +
                                (kFixed_1-kDecayExp) * (counter << kFixedShift)
                                   ) >> kFixedShift;
    }
}


void SLockedQueue::CountEvent(TStatEvent event, int num)
{
    m_EventCounter[event].Add(num);
}


double SLockedQueue::GetAverage(TStatEvent n_event)
{
    return m_Average[n_event] / double(kFixed_1 * kMeasureInterval);
}


void
SLockedQueue::x_AddToReadGroupNoLock(unsigned group_id, unsigned job_id)
{
    TGroupMap::iterator i = m_GroupMap.find(group_id);
    if (i != m_GroupMap.end()) {
        TNSBitVector& bv = (*i).second;
        bv.set(job_id);
    } else {
        TNSBitVector bv;
        bv.set(job_id);
        m_GroupMap[group_id] = bv;
    }
}


void
SLockedQueue::x_CreateReadGroup(unsigned group_id, const TNSBitVector& bv_jobs)
{
    CFastMutexGuard guard(m_GroupMapLock);
    m_GroupMap[group_id] = bv_jobs;
}


void SLockedQueue::x_RemoveFromReadGroup(unsigned group_id, unsigned job_id)
{
    CFastMutexGuard guard(m_GroupMapLock);
    TGroupMap::iterator i = m_GroupMap.find(group_id);
    if (i != m_GroupMap.end()) {
        TNSBitVector& bv = (*i).second;
        bv.set(job_id, false);
        if (!bv.any())
            m_GroupMap.erase(i);
    }
}

END_NCBI_SCOPE
