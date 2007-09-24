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

#include <bdb/bdb_trans.hpp>
#include <util/bitset/bmalgo.h>

BEGIN_NCBI_SCOPE


void SQueueParameters::Read(const IRegistry& reg, const string& sname)
{
#define GetIntNoErr(name, dflt) reg.GetInt(sname, name, dflt, 0, IRegistry::eReturn)
    // Read parameters
    timeout = GetIntNoErr("timeout", 3600);

    notif_timeout = GetIntNoErr("notif_timeout", 7);
    run_timeout = GetIntNoErr("run_timeout", timeout);
    run_timeout_precision = GetIntNoErr("run_timeout_precision", run_timeout);

    program_name = reg.GetString(sname, "program", kEmptyStr);

    delete_when_done = reg.GetBool(sname, "delete_done", false,
        0, IRegistry::eReturn);
    failed_retries = GetIntNoErr("failed_retries", 0);

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
    log_access_violations = reg.GetBool(sname, "log_access_violations", true,
        0, IRegistry::eReturn);

    subm_hosts = reg.GetString(sname,  "subm_host",  kEmptyStr);
    wnode_hosts = reg.GetString(sname, "wnode_host", kEmptyStr);
}



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


SLockedQueue::SLockedQueue(const string& queue_name,
                           const string& qclass_name,
                           TQueueKind queue_kind)
  :
    qname(queue_name),
    qclass(qclass_name),
    kind(queue_kind),

    m_BecameEmpty(-1),
    last_notif(0), 
    q_notif("NCBI_JSQ_"),
    run_time_line(NULL),
    delete_database(false),
    m_AffWrapped(false),
    m_CurrAffId(0),
    m_LastAffId(0),

    m_ParamLock(CRWLock::fFavorWriters),
    m_Timeout(3600), 
    m_NotifTimeout(7), 
    m_DeleteDone(false),
    m_RunTimeout(3600),
    m_FailedRetries(0),
    m_EmptyLifetime(-1),
    m_MaxInputSize(kNetScheduleMaxDBDataSize),
    m_MaxOutputSize(kNetScheduleMaxDBDataSize)
{
    _ASSERT(!queue_name.empty());
    q_notif.append(queue_name);
    for (TStatEvent n = 0; n < eStatNumEvents; ++n) {
        m_EventCounter[n].Set(0);
    }
    m_StatThread.Reset(new CStatisticsThread(*this));
    m_StatThread->Run();
    m_LastId.Set(0);
}

SLockedQueue::~SLockedQueue()
{
    NON_CONST_ITERATE(TListenerList, it, wnodes) {
        SQueueListener* node = *it;
        delete node;
    }
    delete run_time_line;
    if (delete_database) {
        //CBDB_File deleter;
        db.Close();
        aff_idx.Close();
        affinity_dict.Close();
        m_TagDb.Close();
        ITERATE(vector<string>, it, files) {
            // NcbiCout << "Wipig out " << *it << NcbiEndl;
            //deleter.Remove(*it);
            CFile(*it).Remove();
        }
    }
    m_StatThread->RequestStop();
    m_StatThread->Join(NULL);
}

void SLockedQueue::Open(CBDB_Env& env, const string& path)
{
    string prefix = string("jsq_") + qname;
    string fname = prefix + ".db";
    db.SetEnv(env);

    db.RevSplitOff();
    db.Open(fname.c_str(), CBDB_RawFile::eReadWriteCreate);
    x_ReadFieldInfo();
    files.push_back(path + fname);

    fname = prefix + "_jobinfo.db";
    m_JobInfoDB.SetEnv(env);
    m_JobInfoDB.Open(fname.c_str(), CBDB_RawFile::eReadWriteCreate);
    files.push_back(path + fname);

    fname = prefix + "_deleted.db";
    m_DeletedJobsDB.SetEnv(env);
    m_DeletedJobsDB.Open(fname.c_str(), CBDB_RawFile::eReadWriteCreate);
    files.push_back(path + fname);

    fname = prefix + "_affid.idx";
    aff_idx.SetEnv(env);
    aff_idx.Open(fname.c_str(), CBDB_RawFile::eReadWriteCreate);
    files.push_back(path + fname);

    affinity_dict.Open(env, qname);
    files.push_back(path + prefix + "_affdict.db");
    files.push_back(path + prefix + "_affdict_token.idx");

    fname = prefix + "_tag.idx";
    m_TagDb.SetEnv(env);
    m_TagDb.SetPageSize(32*1024);
    m_TagDb.RevSplitOff();
    m_TagDb.Open(fname, CBDB_RawFile::eReadWriteCreate);
    files.push_back(path + fname);

    last_notif = time(0);
}


void SLockedQueue::x_ReadFieldInfo(void)
{
    // Build field map
    const CBDB_BufferManager* key  = db.GetKeyBuffer();
    const CBDB_BufferManager* data = db.GetDataBuffer();

    m_NKeys = 0;
    if (key) {
        for (unsigned i = 0; i < key->FieldCount(); ++i) {
            const CBDB_Field& fld = key->GetField(i);
            m_FieldMap[fld.GetName()] = i;
        }
        m_NKeys = key->FieldCount();
    }

    if (data) {
        for (unsigned i = 0; i < data->FieldCount(); ++i) {
            const CBDB_Field& fld = data->GetField(i);
            m_FieldMap[fld.GetName()] = m_NKeys + i;
        }
    }
}


void SLockedQueue::SetParameters(const SQueueParameters& params)
{
    CWriteLockGuard guard(m_ParamLock);
    m_Timeout = params.timeout;
    m_NotifTimeout = params.notif_timeout;
    m_DeleteDone = params.delete_when_done;

    m_RunTimeout = params.run_timeout;
    if (params.run_timeout && !run_time_line) {
        // One time only. Precision can not be reset.
        run_time_line =
            new CJobTimeLine(params.run_timeout_precision, 0);
    }

    // program version control
    m_ProgramVersionList.Clear();
    if (!params.program_name.empty()) {
        m_ProgramVersionList.AddClientInfo(params.program_name);
    }

    m_SubmHosts.SetHosts(params.subm_hosts);
    m_FailedRetries = params.failed_retries;
    m_EmptyLifetime = params.empty_lifetime;
    m_MaxInputSize  = params.max_input_size;
    m_MaxOutputSize = params.max_output_size;
    m_DenyAccessViolations = params.deny_access_violations;
    m_LogAccessViolations  = params.log_access_violations;
    m_WnodeHosts.SetHosts(params.wnode_hosts);
}


int SLockedQueue::GetFieldIndex(const string& name)
{
    map<string, int>::iterator i = m_FieldMap.find(name);
    if (i == m_FieldMap.end()) return -1;
    return i->second;
}


string SLockedQueue::GetField(int index)
{
    const CBDB_BufferManager* bm;
    if (index < m_NKeys) {
        bm = db.GetKeyBuffer();
    } else {
        bm = db.GetDataBuffer();
        index -= m_NKeys;
    }
    const CBDB_Field& fld = bm->GetField(index);
    return fld.GetString();
}


unsigned SLockedQueue::LoadStatusMatrix()
{
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

    CBDB_FileCursor cur(db);
    cur.InitMultiFetch(1024*1024);
    cur.SetCondition(CBDB_FileCursor::eGE);
    cur.From << 0;

    unsigned recs = 0;

    for (;cur.Fetch() == eBDB_Ok; ++recs) {
        unsigned job_id = db.id;
        if (m_JobsToDelete.test(job_id)) continue;
        int status = db.status;

        if (job_id > (unsigned)m_LastId.Get()) {
            m_LastId.Set(job_id);
        }
        status_tracker.SetExactStatusNoLock(job_id, 
                      (CNetScheduleAPI::EJobStatus) status, 
                      true);

        if (status == (int) CNetScheduleAPI::eRunning && 
            run_time_line) {
            // Add object to the first available slot
            // it is going to be rescheduled or dropped
            // in the background control thread
            run_time_line->AddObject(run_time_line->GetHead(), job_id);
        }
    }
    return recs;
}


CNetScheduleAPI::EJobStatus
SLockedQueue::GetJobStatus(unsigned job_id) const
{
    return status_tracker.GetStatus(job_id);
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
    if (kind && empty_lifetime != -1) {
        unsigned cnt = status_tracker.Count();
        if (cnt) {
            m_BecameEmpty = -1;
        } else {
            if (m_BecameEmpty != -1 &&
                m_BecameEmpty + empty_lifetime < time(0))
            {
                LOG_POST(Info << "Queue " << qname << " expired."
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
    if (m_LastId.Get() >= kMax_I4) {
        m_LastId.Set(0);
    }

    return (unsigned) m_LastId.Add(1);
}


unsigned int SLockedQueue::GetNextIdBatch(unsigned count)
{
    if (m_LastId.Get() >= kMax_I4) {
        // NB: This wrap-around zero will not work in the
        // case of batch id - client expect monotonously
        // growing range of ids.
        m_LastId.Set(0);
    }
    unsigned int id = (unsigned) m_LastId.Add(count);
    id = id - count + 1;
    return id;
}


void SLockedQueue::Erase(unsigned job_id)
{
    status_tracker.Erase(job_id);

    {{
        // Request delayed record delete
        CFastMutexGuard jtd_guard(m_JobsToDeleteLock);
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
        // TODO: calculate job_to_delete_count * maturity instead
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
        CFastMutexGuard guard(lock); // TODO: replace by CQueueGuard
        aff_idx.SetTransaction(0);
        CBDB_FileCursor cur(aff_idx);
        cur.SetCondition(CBDB_FileCursor::eGE);
        cur.From << curr_aff_id;

        unsigned n = 0;
        EBDB_ErrCode ret;
        for (; (ret = cur.Fetch()) == eBDB_Ok && n < kAffBatchSize; ++n) {
            curr_aff_id = aff_idx.aff_id;
            if (wrapped && curr_aff_id >= last_aff_id) // run over the tail
                break;
            bv.set(curr_aff_id);
        }
        if (ret != eBDB_Ok) {
            if (ret != eBDB_NotFound)
                LOG_POST(Error << "Error reading affinity index");
            if (wrapped) {
                curr_aff_id = last_aff_id;
            } else {
                // wrap-around
                curr_aff_id = 0;
                wrapped = true;
                cur.SetCondition(CBDB_FileCursor::eGE);
                cur.From << curr_aff_id;
                for (; n < kAffBatchSize && (ret = cur.Fetch()) == eBDB_Ok; ++n) {
                    curr_aff_id = aff_idx.aff_id;
                    if (curr_aff_id >= last_aff_id) // run over the tail
                        break;
                    bv.set(curr_aff_id);
                }
                if (ret != eBDB_NotFound)
                    LOG_POST(Error << "Error reading affinity index");
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
        CFastMutexGuard guard(lock); // TODO: replace by CQueueGuard
        CBDB_Transaction trans(*db.GetEnv(), 
                                CBDB_Transaction::eEnvDefault,
                                CBDB_Transaction::eNoAssociation);
        aff_idx.SetTransaction(&trans);
        SQueueAffinityIdx::TParent::TBitVector bvect(bm::BM_GAP);
        aff_idx.aff_id = aff_id;
        EBDB_ErrCode ret =
            aff_idx.ReadVector(&bvect, bm::set_OR, NULL);
        if (ret != eBDB_Ok) {
            if (ret != eBDB_NotFound)
                LOG_POST(Error << "Error reading affinity index");
            continue;
        }
        unsigned old_count = bvect.count();
        bvect -= m_AffJobsToDelete;
        unsigned new_count = bvect.count();
        if (new_count == old_count) {
            continue;
        }
        aff_idx.aff_id = aff_id;
        if (bvect.any()) {
            bvect.optimize();
            aff_idx.WriteVector(bvect, SQueueAffinityIdx::eNoCompact);
        } else {
            // TODO: if there is no record in worker_aff_map, 
            // remove record from SAffinityDictDB
            //{{
            //    CFastMutexGuard aff_guard(aff_map_lock);
            //    if (!worker_aff_map.CheckAffinity(aff_id);
            //        affinity_dict.RemoveToken(aff_id, trans);
            //}}
            aff_idx.Delete();
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


void SLockedQueue::NotifyListeners(bool unconditional)
{
    int notif_timeout = CQueueParamAccessor(*this).GetNotifTimeout();

    time_t curr = time(0);

    if (!unconditional &&
        (notif_timeout == 0 ||
         !status_tracker.AnyPending())) {
        return;
    }

    SLockedQueue::TListenerList::size_type lst_size;
    {{
        CWriteLockGuard guard(wn_lock);
        lst_size = wnodes.size();
        if ((lst_size == 0) ||
            (!unconditional && last_notif + notif_timeout > curr)) {
            return;
        }
        last_notif = curr;
    }}

    const char* msg = q_notif.c_str();
    size_t msg_len = q_notif.length()+1;

    for (SLockedQueue::TListenerList::size_type i = 0;
         i < lst_size;
         ++i)
    {
        unsigned host;
        unsigned short port;
        {{
            CReadLockGuard guard(wn_lock);
            SQueueListener* ql = wnodes[i];
            if (ql->last_connect + ql->timeout < curr)
                continue;
            host = ql->host;
            port = ql->udp_port;
        }}

        if (port) {
            CFastMutexGuard guard(us_lock);
            //EIO_Status status =
                udp_socket.Send(msg, msg_len,
                                   CSocketAPI::ntoa(host), port);
        }
        // periodically check if we have no more jobs left
        if ((i % 10 == 0) && !status_tracker.AnyPending())
            break;
    }
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


void SLockedQueue::SetTagDbTransaction(CBDB_Transaction* trans)
{
    m_TagDb.SetTransaction(trans);
}


void SLockedQueue::AppendTags(CNSTagMap& tag_map, TNSTagList& tags, unsigned job_id)
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
    m_TagDb.SetTransaction(&trans);
    NON_CONST_ITERATE(TNSTagMap, it, *tag_map) {
        m_TagDb.key = it->first.first;
        m_TagDb.val = it->first.second;
        /*
        EBDB_ErrCode err = m_TagDb.ReadVector(it->second, bm::set_OR);
        if (err != eBDB_Ok && err != eBDB_NotFound) {
            // TODO: throw db error
        }
        m_TagDb.key = it->first.first;
        m_TagDb.val = it->first.second;
        it->second->optimize();
        if (it->first.first == "transcript") {
            it->second->stat();
        }
        m_TagDb.WriteVector(*(it->second), STagDB::eNoCompact);
        */

        TNSBitVector bv_tmp(bm::BM_GAP);
        EBDB_ErrCode err = m_TagDb.ReadVector(&bv_tmp);
        if (err != eBDB_Ok && err != eBDB_NotFound) {
            // TODO: throw db error
        }
        bm::combine_or(bv_tmp, it->second->begin(), it->second->end());

        m_TagDb.key = it->first.first;
        m_TagDb.val = it->first.second;
        m_TagDb.WriteVector(bv_tmp, STagDB::eNoCompact);

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
    CBDB_FileCursor cur(m_TagDb);
    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << key << val;
    
    return cur.Fetch(buf) == eBDB_Ok;
}


void SLockedQueue::ReadTags(const string& key, TNSBitVector* bv)
{
    // Guarded by m_TagLock through GetTagLock()
    CBDB_FileCursor cur(m_TagDb);
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
    m_TagDb.SetTransaction(&trans);
    CBDB_FileCursor cur(m_TagDb, trans,
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
        CFastMutexGuard guard(lock); // TODO: replace by CQueueGuard
        CBDB_Transaction trans(*db.GetEnv(),
            CBDB_Transaction::eEnvDefault,
            CBDB_Transaction::eNoAssociation);

        db.SetTransaction(&trans);
        m_JobInfoDB.SetTransaction(&trans);

        unsigned n;
        for (n = 0; en.valid() && n < txn_size; ++en, ++n) {
            unsigned job_id = *en;
            db.id = job_id;
            try {
                db.Delete();
                ++del_rec;
            } catch (CBDB_ErrnoException& ex) {
                LOG_POST(Error << "BDB error " << ex.what());
            }

            m_JobInfoDB.id = job_id;
            try {
                m_JobInfoDB.Delete();
            } catch (CBDB_ErrnoException& ex) {
                LOG_POST(Error << "BDB error " << ex.what());
            }
        }
        trans.Commit();
        // x_RemoveTags(trans, batch);
    }
    return del_rec;
}


CBDB_FileCursor* SLockedQueue::GetCursor(CBDB_Transaction& trans)
{
    CBDB_FileCursor* cur = m_Cursor.get();
    if (cur) { 
        cur->ReOpen(&trans);
        return cur;
    }
    cur = new CBDB_FileCursor(db, trans,
                              CBDB_FileCursor::eReadModifyUpdate);
    m_Cursor.reset(cur);
    return cur;
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


END_NCBI_SCOPE
