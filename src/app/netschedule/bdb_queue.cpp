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
 * File Description: Network scheduler job status database.
 *
 */
#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_limits.h>
#include <corelib/version.hpp>
#include <corelib/ncbireg.hpp>
#include <connect/services/netschedule_api.hpp>
#include <connect/ncbi_socket.hpp>

#include <db.h>
#include <bdb/bdb_trans.hpp>
#include <bdb/bdb_cursor.hpp>
#include <bdb/bdb_util.hpp>

#include <util/qparse/query_parse.hpp>
#include <util/qparse/query_exec.hpp>
#include <util/qparse/query_exec_bv.hpp>
#include <util/time_line.hpp>

#include "bdb_queue.hpp"

#include "nslb.hpp"

BEGIN_NCBI_SCOPE

const unsigned k_max_dead_locks = 100;  // max. dead lock repeats


/// Mutex to guard vector of busy IDs 
DEFINE_STATIC_FAST_MUTEX(x_NetSchedulerMutex_BusyID);


/// Class guards the id, guarantees exclusive access to the object
///
/// @internal
///
class CIdBusyGuard
{
public:
    CIdBusyGuard(TNSBitVector* id_set, 
                 unsigned int  id,
                 unsigned      timeout)
        : m_IdSet(id_set), m_Id(id)
    {
        _ASSERT(id);
        unsigned cnt = 0; unsigned sleep_ms = 10;
        while (true) {
            {{
            CFastMutexGuard guard(x_NetSchedulerMutex_BusyID);
            if (!(*id_set)[id]) {
                id_set->set(id);
                break;
            }
            }}
            cnt += sleep_ms;
            if (cnt > timeout * 1000) {
                NCBI_THROW(CNetServiceException, 
                           eTimeout, "Failed to lock object");
            }
            SleepMilliSec(sleep_ms);
        } // while
    }

    ~CIdBusyGuard()
    {
        Release();
    }

    void Release()
    {
        if (m_Id) {
            CFastMutexGuard guard(x_NetSchedulerMutex_BusyID);
            m_IdSet->set(m_Id, false);
            m_Id = 0;
        }
    }
private:
    CIdBusyGuard(const CIdBusyGuard&);
    CIdBusyGuard& operator=(const CIdBusyGuard&);
private:
    TNSBitVector* m_IdSet;
    unsigned int  m_Id;
};



/////////////////////////////////////////////////////////////////////////////
// CQueueCollection implementation
CQueueCollection::CQueueCollection(const CQueueDataBase& db) :
    m_QueueDataBase(db)
{}


CQueueCollection::~CQueueCollection()
{
}


void CQueueCollection::Close()
{
    CWriteLockGuard guard(m_Lock);
    m_QMap.clear();
}


CRef<SLockedQueue> CQueueCollection::GetLockedQueue(const string& name) const
{
    CReadLockGuard guard(m_Lock);
    TQueueMap::const_iterator it = m_QMap.find(name);
    if (it == m_QMap.end()) {
        string msg = "Job queue not found: ";
        msg += name;
        NCBI_THROW(CNetScheduleException, eUnknownQueue, msg);
    }
    return it->second;
}


bool CQueueCollection::QueueExists(const string& name) const
{
    CReadLockGuard guard(m_Lock);
    TQueueMap::const_iterator it = m_QMap.find(name);
    return (it != m_QMap.end());
}


SLockedQueue& CQueueCollection::AddQueue(const string& name,
                                         SLockedQueue* queue)
{
    CWriteLockGuard guard(m_Lock);
    m_QMap[name] = queue;
    return *queue;
}


bool CQueueCollection::RemoveQueue(const string& name)
{
    CWriteLockGuard guard(m_Lock);
    TQueueMap::iterator it = m_QMap.find(name);
    if (it == m_QMap.end()) return false;
    m_QMap.erase(it);
    return true;
}


CQueueIterator CQueueCollection::begin() const
{
    return CQueueIterator(m_QueueDataBase, m_QMap.begin(), &m_Lock);
}


CQueueIterator CQueueCollection::end() const
{
    return CQueueIterator(m_QueueDataBase, m_QMap.end(), NULL);
}


CQueueIterator::CQueueIterator(const CQueueDataBase& db,
                               CQueueCollection::TQueueMap::const_iterator iter,
                               CRWLock* lock) :
    m_QueueDataBase(db), m_Iter(iter), m_Queue(db), m_Lock(lock)
{
    if (m_Lock) m_Lock->ReadLock();
}


CQueueIterator::CQueueIterator(const CQueueIterator& rhs) :
    m_QueueDataBase(rhs.m_QueueDataBase),
    m_Iter(rhs.m_Iter),
    m_Queue(rhs.m_QueueDataBase),
    m_Lock(rhs.m_Lock)
{
    // Linear on lock
    if (m_Lock) rhs.m_Lock = 0;
}


CQueueIterator::~CQueueIterator()
{
    if (m_Lock) m_Lock->Unlock();
}


CQueue& CQueueIterator::operator*()
{
    m_Queue.x_Assume(m_Iter->second);
    return m_Queue;
}


const string CQueueIterator::GetName()
{
    return m_Iter->first;
}


void CQueueIterator::operator++()
{
    ++m_Iter;
}


/////////////////////////////////////////////////////////////////////////////
// CQueueDataBase implementation
CQueueDataBase::CQueueDataBase()
: m_Env(0),
  m_QueueCollection(*this),
  m_StopPurge(false),
  m_FreeStatusMemCnt(0),
  m_LastFreeMem(time(0)),
  m_LastR2P(time(0)),
  m_UdpPort(0)
{
}


CQueueDataBase::~CQueueDataBase()
{
    try {
        Close();
    } catch (exception& )
    {}
}


void CQueueDataBase::Open(const string& db_path, 
                          const string& db_log_path,
                          const SNSDBEnvironmentParams& params)
{
    m_Path = CDirEntry::AddTrailingPathSeparator(db_path);
    string trailed_log_path = CDirEntry::AddTrailingPathSeparator(db_log_path);

    const string* effective_log_path = trailed_log_path.empty() ?
                                       &m_Path :
                                       &trailed_log_path;
    {{
        CDir dir(m_Path);
        if ( !dir.Exists() ) {
            dir.Create();
        }
    }}

    delete m_Env;
    m_Env = new CBDB_Env();

    if (!trailed_log_path.empty()) {
        m_Env->SetLogDir(trailed_log_path);
    }

    // memory log option. we probably need to reset LSN
    // numbers
/*
    if (log_mem_size) {
    }

    // Private environment for LSN recovery
    {{
        m_Name = "jsqueue";
        string err_file = m_Path + "err" + string(m_Name) + ".log";
        m_Env->OpenErrFile(err_file.c_str());

        m_Env->SetCacheSize(1024*1024);
        m_Env->OpenPrivate(path.c_str());
        
        m_Env->LsnReset("jsq_test.db");

        delete m_Env;
        m_Env = new CBDB_Env();
    }}
*/



    m_Name = "jsqueue";
    string err_file = m_Path + "err" + string(m_Name) + ".log";
    m_Env->OpenErrFile(err_file.c_str());

    if (params.log_mem_size) {
        m_Env->SetLogInMemory(true);
        m_Env->SetLogBSize(params.log_mem_size);
    } else {
        m_Env->SetLogFileMax(200 * 1024 * 1024);
        m_Env->SetLogAutoRemove(true);
    }

    

    // Check if bdb env. files are in place and try to join
    CDir dir(*effective_log_path);
    CDir::TEntries fl = dir.GetEntries("__db.*", CDir::eIgnoreRecursive);

    if (!params.private_env && !fl.empty()) {
        // Opening db with recover flags is unreliable.
        BDB_RecoverEnv(m_Path, false);
        NcbiCout << "Running recovery..." << NcbiEndl;
    }

    CBDB_Env::TEnvOpenFlags opt = CBDB_Env::eThreaded;
    if (params.private_env)
        opt |= CBDB_Env::ePrivate;

    if (params.cache_ram_size)
        m_Env->SetCacheSize(params.cache_ram_size);
    if (params.mutex_max)
        m_Env->MutexSetMax(params.mutex_max);
    if (params.max_locks)
        m_Env->SetMaxLocks(params.max_locks);
    if (params.max_lockers)
        m_Env->SetMaxLockers(params.max_lockers);
    if (params.max_lockobjects)
        m_Env->SetMaxLockObjects(params.max_lockobjects);
    if (params.max_trans)
        m_Env->SetTransactionMax(params.max_trans);
    m_Env->SetTransactionSync(params.sync_transactions ?
                                    CBDB_Transaction::eTransSync :
                                    CBDB_Transaction::eTransASync);

    m_Env->OpenWithTrans(m_Path.c_str(), opt);
    LOG_POST(Info << "Opened " << (params.private_env ? "private " : "")
        << "BDB environment with "
        << m_Env->GetMaxLocks() << " max locks, "
//            << m_Env->GetMaxLockers() << " max lockers, "
//            << m_Env->GetMaxLockObjects() << " max lock objects, "
        << (m_Env->GetTransactionSync() == CBDB_Transaction::eTransSync ?
                                        "" : "a") << "syncronous transactions, "
        << m_Env->MutexGetMax() <<  " max mutexes");


    m_Env->SetDirectDB(params.direct_db);
    m_Env->SetDirectLog(params.direct_log);

    m_Env->SetCheckPointKB(params.checkpoint_kb);
    m_Env->SetCheckPointMin(params.checkpoint_min);

    m_Env->SetLockTimeout(10 * 1000000); // 10 sec

    m_Env->SetTasSpins(5);

    if (m_Env->IsTransactional()) {
        m_Env->SetTransactionTimeout(10 * 1000000); // 10 sec
        m_Env->ForceTransactionCheckpoint();
        m_Env->CleanLog();
    }

    m_QueueDescriptionDB.SetEnv(*m_Env);
    m_QueueDescriptionDB.Open("sys_qdescr.db", CBDB_RawFile::eReadWriteCreate);
}


void CQueueDataBase::Configure(const IRegistry& reg, unsigned* min_run_timeout)
{
    bool no_default_queues = 
        reg.GetBool("server", "no_default_queues", false, 0, IRegistry::eReturn);

    CFastMutexGuard guard(m_ConfigureLock);

    x_CleanParamMap();

    CBDB_Transaction trans(*m_Env, 
                           CBDB_Transaction::eEnvDefault,
                           CBDB_Transaction::eNoAssociation);
    m_QueueDescriptionDB.SetTransaction(&trans);

    list<string> sections;
    reg.EnumerateSections(&sections);

    ITERATE(list<string>, it, sections) {
        string qclass, tmp;
        const string& sname = *it;
        NStr::SplitInTwo(sname, "_", tmp, qclass);
        if (NStr::CompareNocase(tmp, "queue") != 0 &&
            NStr::CompareNocase(tmp, "qclass") != 0) {
            continue;
        }
        if (m_QueueParamMap.find(qclass) != m_QueueParamMap.end()) {
            LOG_POST(Warning << tmp << " section " << sname
                             << " conflicts with previous " << 
                             (NStr::CompareNocase(tmp, "queue") == 0 ?
                                 "qclass" : "queue") <<
                             " section with same queue/qclass name");
            continue;
        }

        SQueueParameters* params = new SQueueParameters;
        params->Read(reg, sname);
        m_QueueParamMap[qclass] = params;
        *min_run_timeout = 
            std::min(*min_run_timeout, (unsigned)params->run_timeout_precision);
        if (!no_default_queues && NStr::CompareNocase(tmp, "queue") == 0) {
            m_QueueDescriptionDB.queue = qclass;
            m_QueueDescriptionDB.kind = SLockedQueue::eKindStatic;
            m_QueueDescriptionDB.qclass = qclass;
            m_QueueDescriptionDB.UpdateInsert();
        }
    }

    list<string> queues;
    reg.EnumerateEntries("queues", &queues);
    ITERATE(list<string>, it, queues) {
        const string& qname = *it;
        string qclass = reg.GetString("queues", qname, "");
        if (!qclass.empty()) {
            m_QueueDescriptionDB.queue = qname;
            m_QueueDescriptionDB.kind = SLockedQueue::eKindStatic;
            m_QueueDescriptionDB.qclass = qclass;
            m_QueueDescriptionDB.UpdateInsert();
        }
    }
    trans.Commit();
    m_QueueDescriptionDB.Sync();

    CBDB_FileCursor cur(m_QueueDescriptionDB);
    cur.SetCondition(CBDB_FileCursor::eFirst);

    while (cur.Fetch() == eBDB_Ok) {
        string qname(m_QueueDescriptionDB.queue);
        string qclass(m_QueueDescriptionDB.qclass);
        int kind = m_QueueDescriptionDB.kind;
        TQueueParamMap::iterator it = m_QueueParamMap.find(qclass);
        if (it == m_QueueParamMap.end()) {
            LOG_POST(Error << "Can not find class " << qclass << " for queue " << qname);
            // TODO: Mark queue as dynamic, so we can delete it
            // m_QueueDescriptionDB.kind = SLockedQueue::eKindDynamic;
            // cur.Update();
            continue;
        }
        const SQueueParameters& params = *(it->second);
        bool qexists = m_QueueCollection.QueueExists(qname);
        if (!qexists) {
            LOG_POST(Info 
                << "Mounting queue:           " << qname                        << "\n"
                << "   Timeout:               " << params.timeout               << "\n"
                << "   Notification timeout:  " << params.notif_timeout         << "\n"
                << "   Run timeout:           " << params.run_timeout           << "\n"
                << "   Run timeout precision: " << params.run_timeout_precision << "\n"
                << "   Programs:              " << params.program_name          << "\n"
                << "   Delete when done:      " << params.delete_when_done      << "\n"
            );
            MountQueue(qname, qclass, kind, params);
        } else {
            UpdateQueueParameters(qname, params);
        }
    }
}


void CQueueDataBase::MountQueue(const string& qname,
                                const string& qclass,
                                TQueueKind    kind,
                                const SQueueParameters& params)
{
    _ASSERT(m_Env);

    auto_ptr<SLockedQueue> q(new SLockedQueue(qname, qclass, kind));
    q->Open(*m_Env, m_Path);

    q->SetParameters(params);

    SLockedQueue& queue = m_QueueCollection.AddQueue(qname, q.release());

    unsigned recs = queue.LoadStatusMatrix();

    queue.udp_socket.SetReuseAddress(eOn);
    unsigned short udp_port = GetUdpPort();
    if (udp_port) {
        queue.udp_socket.Bind(udp_port);
    }

    LOG_POST(Info << "Queue records = " << recs);
}


void CQueueDataBase::CreateQueue(const string& qname, const string& qclass,
                                 const string& comment)
{
    CFastMutexGuard guard(m_ConfigureLock);
    if (m_QueueCollection.QueueExists(qname)) {
        string err = string("Queue \"") + qname + "\" already exists";
        NCBI_THROW(CNetScheduleException, eDuplicateName, err);
    }
    TQueueParamMap::iterator it = m_QueueParamMap.find(qclass);
    if (it == m_QueueParamMap.end()) {
        string err = string("Can not find class \"") + qclass +
                            "\" for queue \"" + qname + "\"";
        NCBI_THROW(CNetScheduleException, eUnknownQueueClass, err);
    }
    CBDB_Transaction trans(*m_Env, 
                           CBDB_Transaction::eEnvDefault,
                           CBDB_Transaction::eNoAssociation);
    m_QueueDescriptionDB.SetTransaction(&trans);
    m_QueueDescriptionDB.queue = qname;
    m_QueueDescriptionDB.kind = SLockedQueue::eKindDynamic;
    m_QueueDescriptionDB.qclass = qclass;
    m_QueueDescriptionDB.comment = comment;
    m_QueueDescriptionDB.UpdateInsert();
    trans.Commit();
    m_QueueDescriptionDB.Sync();
    const SQueueParameters& params = *(it->second);
    MountQueue(qname, qclass, SLockedQueue::eKindDynamic, params);
}


void CQueueDataBase::DeleteQueue(const string& qname)
{
    CFastMutexGuard guard(m_ConfigureLock);
    CBDB_Transaction trans(*m_Env, 
                           CBDB_Transaction::eEnvDefault,
                           CBDB_Transaction::eNoAssociation);
    m_QueueDescriptionDB.SetTransaction(&trans);
    m_QueueDescriptionDB.queue = qname;
    if (m_QueueDescriptionDB.Fetch() != eBDB_Ok) {
        string msg = "Job queue not found: ";
        msg += qname;
        NCBI_THROW(CNetScheduleException, eUnknownQueue, msg);
    }
    int kind = m_QueueDescriptionDB.kind;
    if (!kind) {
        string msg = "Static queue \"";
        msg += qname;
        msg += "\" can not be deleted"; 
        NCBI_THROW(CNetScheduleException, eAccessDenied, msg);
    }
    // Signal queue to wipe out database files.
    CRef<SLockedQueue> queue(m_QueueCollection.GetLockedQueue(qname));
    queue->delete_database = true;
    // Remove it from collection
    if (!m_QueueCollection.RemoveQueue(qname)) {
        string msg = "Job queue not found: ";
        msg += qname;
        NCBI_THROW(CNetScheduleException, eUnknownQueue, msg);
    }
    // Remove it from DB
    m_QueueDescriptionDB.Delete(CBDB_File::eIgnoreError);
    trans.Commit();
}


void CQueueDataBase::QueueInfo(const string& qname, int& kind,
                               string* qclass, string* comment)
{
    CFastMutexGuard guard(m_ConfigureLock);
    m_QueueDescriptionDB.SetTransaction(NULL);
    m_QueueDescriptionDB.queue = qname;
    if (m_QueueDescriptionDB.Fetch() != eBDB_Ok) {
        string msg = "Job queue not found: ";
        msg += qname;
        NCBI_THROW(CNetScheduleException, eUnknownQueue, msg);
    }
    kind = m_QueueDescriptionDB.kind;
    string qclass_str(m_QueueDescriptionDB.qclass);
    *qclass = qclass_str;
    string comment_str(m_QueueDescriptionDB.comment);
    *comment = comment_str;
}


void CQueueDataBase::GetQueueNames(string *list, const string& sep) const
{
    CFastMutexGuard guard(m_ConfigureLock);
    ITERATE(CQueueCollection, it, m_QueueCollection) {
        *list += it.GetName(); *list += sep;
    }
}

void CQueueDataBase::UpdateQueueParameters(const string& qname,
                                           const SQueueParameters& params)
{
    CRef<SLockedQueue> queue(m_QueueCollection.GetLockedQueue(qname));
    queue->SetParameters(params);
}

void CQueueDataBase::Close()
{
    StopNotifThread();
    StopPurgeThread();
    StopExecutionWatcherThread();

    if (m_Env) {
        m_Env->ForceTransactionCheckpoint();
        m_Env->CleanLog();
    }

    x_CleanParamMap();

    m_QueueCollection.Close();
    m_QueueDescriptionDB.Close();
    try {
        if (m_Env) {
            if (m_Env->CheckRemove()) {
                LOG_POST(Info    <<
                         "JS: '" <<
                         m_Name  << "' Unmounted. BDB ENV deleted.");
            } else {
                LOG_POST(Warning << "JS: '" << m_Name 
                                 << "' environment still in use.");
            }
        }
    }
    catch (exception& ex) {
        LOG_POST(Warning << "JS: '" << m_Name 
                         << "' Exception in Close() " << ex.what()
                         << " (ignored.)");
    }

    delete m_Env; m_Env = 0;
}


void CQueueDataBase::TransactionCheckPoint(bool clean_log)
{
    m_Env->TransactionCheckpoint();
    if (clean_log) m_Env->CleanLog();
}


void CQueueDataBase::NotifyListeners(void)
{
    ITERATE(CQueueCollection, it, m_QueueCollection) {
        (*it).NotifyListeners();
    }
}


void CQueueDataBase::CheckExecutionTimeout(void)
{
    ITERATE(CQueueCollection, it, m_QueueCollection) {
        (*it).CheckExecutionTimeout();
    }    
}


void CQueueDataBase::Purge(void)
{
    x_Returned2Pending();

    unsigned global_del_rec = 0;

    // check not to rescan the database too often 
    // when we have low client activity of inserting new jobs.
    /*
    Need to be replaced with load-based logic.
    The logic is flawed anyway - the case when last purged (that is,
    long expired) job is closer than 3000 to new id is VERY rare if
    server is being used at all. 3000 new jobs with lifetime of 10
    days switch off this check for all these 10 days.
    if (m_PurgeLastId + 3000 > (unsigned) m_IdCounter.Get()) {
        ++m_PurgeSkipCnt;

        // probably nothing to do yet, skip purge execution
        if (m_PurgeSkipCnt < 10) { // only 10 skips in a row
            if (unc_del_rec < n_jobs_to_delete) {
                // If there is no unconditional delete jobs - sleep
                SleepMilliSec(1000);
            }
            return;
        }

        m_PurgeSkipCnt = 0;        
    }
    */

    // Delete obsolete job records, based on time stamps 
    // and expiration timeouts
    unsigned n_queue_limit = 10000; // TODO: determine this based on load

    CNetScheduleAPI::EJobStatus statuses_to_delete_from[] = {
        CNetScheduleAPI::eFailed, CNetScheduleAPI::eCanceled,
        CNetScheduleAPI::eDone, CNetScheduleAPI::ePending
    };
    const int kStatusesSize = sizeof(statuses_to_delete_from) /
                              sizeof(CNetScheduleAPI::EJobStatus);

    vector<string> queues_to_delete;
    ITERATE(CQueueCollection, it, m_QueueCollection) {
        unsigned queue_del_rec = 0;
        const unsigned batch_size = 100;
        if ((*it).IsExpired()) {
            queues_to_delete.push_back(it.GetName());
            continue;
        }
        unsigned deleted_by_status[kStatusesSize];
        for (int n = 0; n < kStatusesSize; ++n)
            deleted_by_status[n] = 0;
        for (bool stop_flag = false; !stop_flag; ) {
            // stop if all statuses have less than batch_size jobs to delete
            stop_flag = true;
            for (int n = 0; n < kStatusesSize; ++n) {
                CNetScheduleAPI::EJobStatus s = statuses_to_delete_from[n];
                unsigned del_rec = (*it).CheckDeleteBatch(batch_size, s);
                deleted_by_status[n] += del_rec;
                stop_flag = stop_flag && (del_rec < batch_size);
                queue_del_rec += del_rec;
            }

            // do not delete more than certain number of
            // records from the queue in one Purge
            if (queue_del_rec >= n_queue_limit)
                break;

            if (x_CheckStopPurge()) return;
        }
        for (int n = 0; n < kStatusesSize; ++n) {
            CNetScheduleAPI::EJobStatus s = statuses_to_delete_from[n];
            if (deleted_by_status[n] > 0 && s != CNetScheduleAPI::eDone) {
                LOG_POST(Warning << deleted_by_status[n] <<
                    " jobs deleted from non-final state " <<
                    CNetScheduleAPI::StatusToString(s));
            }
        }
        global_del_rec += queue_del_rec;
    }

    // Delete jobs unconditionally, from internal vector.
    // This is done to spread massive job deletion in time
    // and thus smooth out peak loads
    // TODO: determine batch size based on load
    unsigned n_jobs_to_delete = 1000; 
    unsigned unc_del_rec = x_PurgeUnconditional(n_jobs_to_delete);
    global_del_rec += unc_del_rec;

    // TODO: Handle tags here - see below for affinity

    // Remove unused affinity elements
    ITERATE(CQueueCollection, it, m_QueueCollection) {
        (*it).ClearAffinityIdx();
    }

    TransactionCheckPoint();

    m_FreeStatusMemCnt += global_del_rec;
    x_OptimizeStatusMatrix();

    ITERATE(vector<string>, it, queues_to_delete) {
        try {
            DeleteQueue((*it));
        } catch (...) { // TODO: use more specific exception
            LOG_POST(Warning << "Queue " << (*it) << " already gone.");
        }
    }
}

unsigned CQueueDataBase::x_PurgeUnconditional(unsigned batch_size) {
    // Purge unconditional jobs
    unsigned del_rec = 0;
    ITERATE(CQueueCollection, it, m_QueueCollection) {
        del_rec += (*it).DoDeleteBatch(batch_size);
    }
    return del_rec;
}


void CQueueDataBase::x_Returned2Pending(void)
{
    const int kR2P_delay = 7; // re-submission delay
    time_t curr = time(0);

    if (m_LastR2P + kR2P_delay <= curr) {
        m_LastR2P = curr;
        ITERATE(CQueueCollection, it, m_QueueCollection) {
            (*it).Returned2Pending();
        }
    }
}


void CQueueDataBase::x_OptimizeStatusMatrix(void)
{
    time_t curr = time(0);
    // optimize memory every 15 min. or after 1 million of deleted records
    const int kMemFree_Delay = 15 * 60; 
    const unsigned kRecordThreshold = 1000000;
    if ((m_FreeStatusMemCnt > kRecordThreshold) ||
        (m_LastFreeMem + kMemFree_Delay <= curr)) {
        m_FreeStatusMemCnt = 0;
        m_LastFreeMem = curr;

        ITERATE(CQueueCollection, it, m_QueueCollection) {
            (*it).FreeUnusedMem();
            if (x_CheckStopPurge()) return;
        } // ITERATE
    }
}


void CQueueDataBase::x_CleanParamMap(void)
{
    NON_CONST_ITERATE(TQueueParamMap, it, m_QueueParamMap) {
        SQueueParameters* params = it->second;
        delete params;
    }
    m_QueueParamMap.clear();
}


void CQueueDataBase::StopPurge(void)
{
    CFastMutexGuard guard(m_PurgeLock);
    m_StopPurge = true;
}


bool CQueueDataBase::x_CheckStopPurge(void)
{
    CFastMutexGuard guard(m_PurgeLock);
    bool stop_purge = m_StopPurge;
    m_StopPurge = false;
    return stop_purge;
}


void CQueueDataBase::RunPurgeThread(void)
{
    LOG_POST(Info << "Starting guard and cleaning thread...");
    m_PurgeThread.Reset(
        new CJobQueueCleanerThread(*this, 1));
    m_PurgeThread->Run();
    LOG_POST(Info << "Started.");
}


void CQueueDataBase::StopPurgeThread(void)
{
    if (!m_PurgeThread.Empty()) {
        LOG_POST(Info << "Stopping guard and cleaning thread...");
        StopPurge();
        m_PurgeThread->RequestStop();
        m_PurgeThread->Join();
        LOG_POST(Info << "Stopped.");
    }
}

void CQueueDataBase::RunNotifThread(void)
{
    if (GetUdpPort() == 0) {
        return;
    }

    LOG_POST(Info << "Starting client notification thread...");
    m_NotifThread.Reset(
        new CJobNotificationThread(*this, 1));
    m_NotifThread->Run();
    LOG_POST(Info << "Started.");
}

void CQueueDataBase::StopNotifThread(void)
{
    if (!m_NotifThread.Empty()) {
        LOG_POST(Info << "Stopping notification thread...");
        m_NotifThread->RequestStop();
        m_NotifThread->Join();
        LOG_POST(Info << "Stopped.");
    }
}

void CQueueDataBase::RunExecutionWatcherThread(unsigned run_delay)
{
    LOG_POST(Info << "Starting execution watcher thread...");
    m_ExeWatchThread.Reset(
        new CJobQueueExecutionWatcherThread(*this, run_delay));
    m_ExeWatchThread->Run();
    LOG_POST(Info << "Started.");
}

void CQueueDataBase::StopExecutionWatcherThread(void)
{
    if (!m_ExeWatchThread.Empty()) {
        LOG_POST(Info << "Stopping execution watch thread...");
        m_ExeWatchThread->RequestStop();
        m_ExeWatchThread->Join();
        LOG_POST(Info << "Stopped.");
    }
}


/////////////////////////////////////////////////////////////////////////////
// CQueue implementation

#define DB_TRY(where) for (unsigned n_tries = 0; true; ) { try {
#define DB_END \
    } catch (CBDB_ErrnoException& ex) { \
        if (ex.IsDeadLock()) { \
            if (++n_tries < k_max_dead_locks) { \
                if (m_LQueue->monitor.IsMonitorActive()) { \
                    m_LQueue->monitor.SendString( \
                        "DeadLock repeat in "##where); \
                } \
                SleepMilliSec(250); \
                continue; \
            } \
        } else if (ex.IsNoMem()) { \
            if (++n_tries < k_max_dead_locks) { \
                if (m_LQueue->monitor.IsMonitorActive()) { \
                    m_LQueue->monitor.SendString( \
                        "No resource repeat in "##where); \
                } \
                SleepMilliSec(250); \
                continue; \
            } \
        } \
        ERR_POST("Too many transaction repeats in "##where); \
        throw; \
    } \
    break; \
}


CQueue::CQueue(CQueueDataBase& db, 
               const string&   queue_name,
               unsigned        client_host_addr)
: m_Db(db),
  m_LQueue(db.m_QueueCollection.GetLockedQueue(queue_name)),
  m_ClientHostAddr(client_host_addr),
  m_QueueDbAccessCounter(0)
{
}


unsigned CQueue::CountRecs() const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    CFastMutexGuard guard(q->lock);
    q->db.SetTransaction(NULL);
    return q->db.CountRecs();
}


unsigned CQueue::GetMaxInputSize() const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    return CQueueParamAccessor(*q).GetMaxInputSize();
}


unsigned CQueue::GetMaxOutputSize() const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    return CQueueParamAccessor(*q).GetMaxOutputSize();
}


bool CQueue::IsExpired(void)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    int empty_lifetime = CQueueParamAccessor(*q).GetEmptyLifetime();
    CFastMutexGuard guard(q->lock);
    if (q->kind && empty_lifetime != -1) {
        unsigned cnt = q->status_tracker.Count();
        if (cnt) {
            q->became_empty = -1;
        } else {
            if (q->became_empty != -1 &&
                q->became_empty + empty_lifetime < time(0))
            {
                LOG_POST(Info << "Queue " << q->qname << " expired."
                    << " Became empty: "
                    << CTime(q->became_empty).ToLocalTime().AsString()
                    << " Empty lifetime: " << empty_lifetime
                    << " sec." );
                return true;
            }
            if (q->became_empty == -1)
                q->became_empty = time(0);
        }
    }
    return false;
}


#define NS_PRINT_TIME(msg, t) \
    do \
    { unsigned tt = t; \
      CTime _t(tt); _t.ToLocalTime(); \
      out << msg << (tt ? _t.AsString() : kEmptyStr) << fsp; \
    } while(0)

#define NS_PFNAME(x_fname) \
    (fflag ? (const char*) x_fname : (const char*) "")

void CQueue::x_PrintJobDbStat(SQueueDB&     db,
                              SJobInfoDB&   job_info_db,
                              unsigned      queue_run_timeout,
                              CNcbiOstream& out,
                              const char*   fsp,
                              bool          fflag)
{
    out << fsp << NS_PFNAME("id: ") << (unsigned) db.id << fsp;
    CNetScheduleAPI::EJobStatus status = 
        (CNetScheduleAPI::EJobStatus)(int)db.status;
    out << NS_PFNAME("status: ") << CNetScheduleAPI::StatusToString(status) 
        << fsp;

    NS_PRINT_TIME(NS_PFNAME("time_submit: "), db.time_submit);
    NS_PRINT_TIME(NS_PFNAME("time_run: "), db.time_run);
    NS_PRINT_TIME(NS_PFNAME("time_done: "), db.time_done);

    out << NS_PFNAME("timeout: ") << (unsigned)db.timeout << fsp;
    out << NS_PFNAME("run_timeout: ") << (unsigned)db.run_timeout << fsp;

    unsigned run_timeout = (unsigned)db.run_timeout;
    if (run_timeout == 0) run_timeout = queue_run_timeout;
    time_t exp_time = 
        x_ComputeExpirationTime((unsigned)db.time_run, run_timeout);
    NS_PRINT_TIME(NS_PFNAME("time_run_expire: "), exp_time);


    unsigned subm_addr = db.subm_addr;
    out << NS_PFNAME("subm_addr: ") 
        << (subm_addr ? CSocketAPI::gethostbyaddr(subm_addr) : kEmptyStr) << fsp;
    out << NS_PFNAME("subm_port: ") << (unsigned) db.subm_port << fsp;
    out << NS_PFNAME("subm_timeout: ") << (unsigned) db.subm_timeout << fsp;

    unsigned addr = db.worker_node1;
    out << NS_PFNAME("worker_node1: ")
        << (addr ? CSocketAPI::gethostbyaddr(addr) : kEmptyStr) << fsp;

    addr = db.worker_node2;
    out << NS_PFNAME("worker_node2: ") 
        << (addr ? CSocketAPI::gethostbyaddr(addr) : kEmptyStr) << fsp;

    addr = db.worker_node3;
    out << NS_PFNAME("worker_node3: ") 
        << (addr ? CSocketAPI::gethostbyaddr(addr) : kEmptyStr) << fsp;

    addr = db.worker_node4;
    out << NS_PFNAME("worker_node4: ") 
        << (addr ? CSocketAPI::gethostbyaddr(addr) : kEmptyStr) << fsp;

    addr = db.worker_node5;
    out << NS_PFNAME("worker_node5: ") 
        << (addr ? CSocketAPI::gethostbyaddr(addr) : kEmptyStr) << fsp;

    out << NS_PFNAME("run_counter: ") << (unsigned) db.run_counter << fsp;
    out << NS_PFNAME("ret_code: ") << (unsigned) db.ret_code << fsp;

    unsigned aff_id = (unsigned) db.aff_id;
    if (aff_id) {
        CRef<SLockedQueue> q(x_GetLQueue());
        string token = q->affinity_dict.GetAffToken(aff_id);
        out << NS_PFNAME("aff_token: ") << "'" << token << "'" << fsp;
    }
    out << NS_PFNAME("aff_id: ") << aff_id << fsp;
    out << NS_PFNAME("mask: ") << (unsigned) db.mask << fsp;

    string input, output;
    bool fetched = x_GetInput(db, job_info_db, false, input);
    x_GetOutput(db, job_info_db, fetched, output);

    out << NS_PFNAME("input: ")        << "'" << input       << "'" << fsp;
    out << NS_PFNAME("output: ")       << "'" << output       << "'" << fsp;
    out << NS_PFNAME("err_msg: ")      << "'" <<(string) db.err_msg      << "'" << fsp;
    out << NS_PFNAME("progress_msg: ") << "'" <<(string) db.progress_msg << "'" << fsp;
    out << "\n";
}

void 
CQueue::x_PrintShortJobDbStat(SQueueDB&     db,
                              SJobInfoDB&   job_info_db, 
                              const string& host,
                              unsigned      port,
                              CNcbiOstream& out,
                              const char*   fsp)
{
    char buf[1024];
    sprintf(buf, NETSCHEDULE_JOBMASK, 
                 (unsigned)db.id, host.c_str(), port);
    out << buf << fsp;
    CNetScheduleAPI::EJobStatus status = 
        (CNetScheduleAPI::EJobStatus)(int)db.status;
    out << CNetScheduleAPI::StatusToString(status) << fsp;

    string input, output;
    bool fetched = x_GetInput(db, job_info_db, false, input);
    x_GetOutput(db, job_info_db, fetched, output);

    out << "'" << input    << "'" << fsp;
    out << "'" << output   << "'" << fsp;
    out << "'" << (string) db.err_msg  << "'" << fsp;

    out << "\n";
}


void 
CQueue::PrintJobDbStat(unsigned                    job_id, 
                       CNcbiOstream&               out,
                       CNetScheduleAPI::EJobStatus status)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    unsigned queue_run_timeout = CQueueParamAccessor(*q).GetRunTimeout();

    SQueueDB& db = q->db;
    SJobInfoDB& job_info_db = q->m_JobInfoDB;
    if (status == CNetScheduleAPI::eJobNotFound) {
        CFastMutexGuard guard(q->lock);
        db.SetTransaction(NULL);
        job_info_db.SetTransaction(NULL);
        db.id = job_id;
        if (db.Fetch() == eBDB_Ok) {
            x_PrintJobDbStat(db, job_info_db, queue_run_timeout, out);
        } else {
            out << "Job not found id=" << job_id;
        }
        out << "\n";
    } else {
        TNSBitVector bv;
        q->status_tracker.StatusSnapshot(status, &bv);

        TNSBitVector::enumerator en(bv.first());
        for (;en.valid(); ++en) {
            unsigned id = *en;
            {{
                CFastMutexGuard guard(q->lock);
                db.SetTransaction(NULL);
                job_info_db.SetTransaction(NULL);
                
                db.id = id;

                if (db.Fetch() == eBDB_Ok) {
                    x_PrintJobDbStat(db, job_info_db, queue_run_timeout, out);
                }
            }}
        } // for
        out << "\n";
    }
}


void CQueue::PrintJobStatusMatrix(CNcbiOstream& out)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    q->status_tracker.PrintStatusMatrix(out);
}


bool CQueue::IsDenyAccessViolations() const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    return CQueueParamAccessor(*q).GetDenyAccessViolations();
}


bool CQueue::IsLogAccessViolations() const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    return CQueueParamAccessor(*q).GetLogAccessViolations();
}


bool CQueue::IsVersionControl() const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    CQueueParamAccessor qp(*q);
    return qp.GetProgramVersionList().IsConfigured();
}


bool CQueue::IsMatchingClient(const CQueueClientInfo& cinfo) const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    CQueueParamAccessor qp(*q);
    return qp.GetProgramVersionList().IsMatchingClient(cinfo);
}


bool CQueue::IsSubmitAllowed() const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    CQueueParamAccessor qp(*q);
    return (m_ClientHostAddr == 0) || 
            qp.GetSubmHosts().IsAllowed(m_ClientHostAddr);
}


bool CQueue::IsWorkerAllowed() const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    CQueueParamAccessor qp(*q);
    return (m_ClientHostAddr == 0) || 
            qp.GetWnodeHosts().IsAllowed(m_ClientHostAddr);
}


double CQueue::GetAverage(TStatEvent n_event)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    return q->GetAverage(n_event);
}


CBDB_Env& CQueue::GetBDBEnv()
{
    return *(m_Db.m_Env);
}


void CQueue::PrintAllJobDbStat(CNcbiOstream& out)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    unsigned queue_run_timeout = CQueueParamAccessor(*q).GetRunTimeout();

    SQueueDB& db = q->db;
    SJobInfoDB& job_info_db = q->m_JobInfoDB;

    CFastMutexGuard guard(q->lock);
    db.SetTransaction(NULL);
    job_info_db.SetTransaction(NULL);

    CBDB_FileCursor cur(db);

    cur.SetCondition(CBDB_FileCursor::eFirst);

    while (cur.Fetch() == eBDB_Ok) {
        x_PrintJobDbStat(db, job_info_db, queue_run_timeout, out);
        if (!out.good()) break;
    }
}


void CQueue::PrintStat(CNcbiOstream& out)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    SQueueDB& db = q->db;
    CFastMutexGuard guard(q->lock);
    db.SetTransaction(NULL);
    db.PrintStat(out);
}


void CQueue::PrintQueue(CNcbiOstream&               out, 
                        CNetScheduleAPI::EJobStatus job_status,
                        const string&               host,
                        unsigned                    port)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    TNSBitVector bv;
    q->status_tracker.StatusSnapshot(job_status, &bv);
    SQueueDB& db = q->db;
    SJobInfoDB& job_info_db = q->m_JobInfoDB;

    TNSBitVector::enumerator en(bv.first());
    for (;en.valid(); ++en) {
        unsigned id = *en;
        {{
            CFastMutexGuard guard(q->lock);
            db.SetTransaction(NULL);
            job_info_db.SetTransaction(NULL);
            
            db.id = id;

            if (db.Fetch() == eBDB_Ok) {
                x_PrintShortJobDbStat(db, job_info_db, host, port, out, "\t");
            }
        }}
    }
}


// Functor for EQ
class CQueryFunctionEQ : public CQueryFunction_BV_Base<TNSBitVector>
{
public:
    CQueryFunctionEQ(CRef<SLockedQueue> queue) :
      m_Queue(queue)
    {}
    typedef CQueryFunction_BV_Base<TNSBitVector> TParent;
    typedef TParent::TBVContainer::TBuffer TBuffer;
    typedef TParent::TBVContainer::TBitVector TBitVector;
    virtual void Evaluate(CQueryParseTree::TNode& qnode);
private:
    void x_CheckArgs(const CQueryFunctionBase::TArgVector& args);
    CRef<SLockedQueue> m_Queue;
};

void CQueryFunctionEQ::Evaluate(CQueryParseTree::TNode& qnode)
{
    //NcbiCout << "Key: " << key << " Value: " << val << NcbiEndl;
    CQueryFunctionBase::TArgVector args;
    this->MakeArgVector(qnode, args);
    x_CheckArgs(args);
    const string& key = args[0]->GetValue().GetStrValue();
    const string& val = args[1]->GetValue().GetStrValue();
    auto_ptr<TNSBitVector> bv;
    auto_ptr<TBuffer> buf;
    if (key == "status") {
        // special case for status
        CNetScheduleAPI::EJobStatus status =
            CNetScheduleAPI::StringToStatus(val);
        if (status == CNetScheduleAPI::eJobNotFound)
            NCBI_THROW(CNetScheduleException,
                eQuerySyntaxError, string("Unknown status: ") + val);
        bv.reset(new TNSBitVector);
        m_Queue->status_tracker.StatusSnapshot(status, bv.get());
    } else if (key == "id") {
        unsigned job_id = CNetScheduleKey(val).id;
        bv.reset(new TNSBitVector);
        bv->set(job_id);
    } else {
        if (val == "*") {
            // wildcard
            bv.reset(new TNSBitVector);
            m_Queue->ReadTags(key, bv.get());
        } else {
            buf.reset(new TBuffer);
            if (!m_Queue->ReadTag(key, val, buf.get())) {
                // Signal empty set by setting empty bitvector
                bv.reset(new TNSBitVector());
                buf.reset(NULL);
            }
        }
    }
    if (qnode.GetValue().IsNot()) {
        // Apply NOT here
        if (bv.get()) {
            bv->invert();
        } else if (buf.get()) {
            bv.reset(new TNSBitVector());
            bm::operation_deserializer<TNSBitVector>::deserialize(*bv,
                                                &((*buf.get())[0]),
                                                0,
                                                bm::set_ASSIGN);
            bv.get()->invert();
        }
    }
    if (bv.get())
        this->MakeContainer(qnode)->SetBV(bv.release());
    else if (buf.get())
        this->MakeContainer(qnode)->SetBuffer(buf.release());
}

void CQueryFunctionEQ::x_CheckArgs(const CQueryFunctionBase::TArgVector& args)
{
    if (args.size() != 2 ||
        (args[0]->GetValue().GetType() != CQueryParseNode::eIdentifier  &&
         args[0]->GetValue().GetType() != CQueryParseNode::eString)) {
        NCBI_THROW(CNetScheduleException,
             eQuerySyntaxError, "Wrong arguments for '='");
    }
}


TNSBitVector* CQueue::ExecSelect(const string& query, list<string>& fields)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    CQueryParseTree qtree;
    try {
        qtree.Parse(query.c_str());
    } catch (CQueryParseException& ex) {
        NCBI_THROW(CNetScheduleException, eQuerySyntaxError, ex.GetMsg());
    }
    CQueryParseTree::TNode* top = qtree.GetQueryTree();
    if (!top)
        NCBI_THROW(CNetScheduleException,
            eQuerySyntaxError, "Query syntax error in parse");

    if (top->GetValue().GetType() == CQueryParseNode::eSelect) {
        // Find where clause here
        typedef CQueryParseTree::TNode::TNodeList_I TNodeIterator;
        for (TNodeIterator it = top->SubNodeBegin();
             it != top->SubNodeEnd(); ++it) {
            CQueryParseTree::TNode* node = *it;
            CQueryParseNode::EType node_type = node->GetValue().GetType();
            if (node_type == CQueryParseNode::eList) {
                for (TNodeIterator it2 = node->SubNodeBegin();
                     it2 != node->SubNodeEnd(); ++it2) {
                    fields.push_back((*it2)->GetValue().GetStrValue());
                }
            }
            if (node_type == CQueryParseNode::eWhere) {
                TNodeIterator it2 = node->SubNodeBegin();
                if (it2 == node->SubNodeEnd())
                    NCBI_THROW(CNetScheduleException,
                        eQuerySyntaxError,
                        "Query syntax error in WHERE clause");
                top = (*it2);
                break;
            }
        }
    }

    // Execute 'select' phase.
    CQueryExec qexec;
    qexec.AddFunc(CQueryParseNode::eAnd,
        new CQueryFunction_BV_Logic<TNSBitVector>(bm::set_AND));
    qexec.AddFunc(CQueryParseNode::eOr,
        new CQueryFunction_BV_Logic<TNSBitVector>(bm::set_OR));
    qexec.AddFunc(CQueryParseNode::eSub,
        new CQueryFunction_BV_Logic<TNSBitVector>(bm::set_SUB));
    qexec.AddFunc(CQueryParseNode::eXor,
        new CQueryFunction_BV_Logic<TNSBitVector>(bm::set_XOR));
    qexec.AddFunc(CQueryParseNode::eIn,
        new CQueryFunction_BV_In_Or<TNSBitVector>());
    qexec.AddFunc(CQueryParseNode::eNot,
        new CQueryFunction_BV_Not<TNSBitVector>());

    qexec.AddFunc(CQueryParseNode::eEQ,
        new CQueryFunctionEQ(q));

    {{
        CFastMutexGuard guard(q->GetTagLock());
        q->SetTagDbTransaction(NULL);
        qexec.Evaluate(qtree, *top);
    }}

    IQueryParseUserObject* uo = top->GetValue().GetUserObject();
    if (!uo)
        NCBI_THROW(CNetScheduleException,
            eQuerySyntaxError, "Query syntax error in eval");
    typedef CQueryEval_BV_Value<TNSBitVector> BV_UserObject;
    BV_UserObject* result =
        dynamic_cast<BV_UserObject*>(uo);
    _ASSERT(result);
    auto_ptr<TNSBitVector> bv(result->ReleaseBV());
    if (!bv.get()) {
        bv.reset(new TNSBitVector());
        BV_UserObject::TBuffer *buf = result->ReleaseBuffer();
        if (buf && buf->size()) {
            bm::operation_deserializer<TNSBitVector>::deserialize(*bv,
                                                &((*buf)[0]),
                                                0,
                                                bm::set_ASSIGN);
        }
    }
    // Filter against deleted jobs
    q->FilterJobs(*(bv.get()));

    return bv.release();
}


static string FormatNSId(const string& val, SQueueDescription* qdesc)
{
    string res("JSID_01_");
    res += val;
    res += '_';
    res += qdesc->host;
    res += '_';
    res += NStr::IntToString(qdesc->port);
    return res;
}


static const char* kISO8601DateTime = 0;
static string FormatTime(const string& val, SQueueDescription*)
{
    time_t t = NStr::StringToInt(val);
    if (!t) return "NULL";
    if (!kISO8601DateTime) {
        const char *format = CTimeFormat::GetPredefined(
            CTimeFormat::eISO8601_DateTimeSec).GetString().c_str();
        kISO8601DateTime = new char[strlen(format)+1];
        strcpy(const_cast<char *>(kISO8601DateTime), format);
    }
    return CTime(t).ToLocalTime().AsString(kISO8601DateTime);
}


static string FormatWorkerNode(const string& val, SQueueDescription*)
{
    unsigned host = NStr::StringToInt(val);
    return NStr::IntToString((host >>  0) & 0xff) + '.'
         + NStr::IntToString((host >>  8) & 0xff) + '.'
         + NStr::IntToString((host >> 16) & 0xff) + '.'
         + NStr::IntToString((host >> 24) & 0xff);
}


static string FormatStatus(const string& val, SQueueDescription*)
{
    int status = NStr::StringToInt(val);
    return CNetScheduleAPI::StatusToString(CNetScheduleAPI::EJobStatus(status));
}


#define FIELD_INPUT  10000
#define FIELD_OUTPUT 10001
void CQueue::PrepareFields(SFieldsDescription& field_descr,
                           const list<string>& fields)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    field_descr.field_nums.clear();
    field_descr.formatters.clear();
    field_descr.pos_to_tag.clear();

    // Verify the fields, and convert them into field numbers
    field_descr.has_tags = false;
    ITERATE(list<string>, it, fields) {
        const string& field_name = *it;
        int i = q->GetFieldIndex(field_name);
        string tag_name;
        SFieldsDescription::FFormatter formatter = NULL;
        if (i < 0) {
            if (NStr::StartsWith(field_name, "tag.")) {
                tag_name = field_name.substr(4);
                field_descr.has_tags = true;
            } else {
                NCBI_THROW(CNetScheduleException, eQuerySyntaxError,
                    string("Unknown field: ") + (*it));
            }
        } else {
            if (field_name == "id") {
                formatter = FormatNSId;
            } else if (field_name == "status") {
                formatter = FormatStatus;
            } else if (NStr::StartsWith(field_name, "time") &&
                       field_name != "timeout") {
                formatter = FormatTime;
            } else if (NStr::StartsWith(field_name, "worker_node")) {
                formatter = FormatWorkerNode;
            } else if (field_name == "input") {
                i = FIELD_INPUT;
            } else if (field_name == "output") {
                i = FIELD_OUTPUT;
            }
        }
        field_descr.field_nums.push_back(i);
        field_descr.formatters.push_back(formatter);
        field_descr.pos_to_tag.push_back(tag_name);
    }
}


void CQueue::ExecProject(TRecordSet&               record_set,
                         const TNSBitVector&       ids,
                         const SFieldsDescription& field_descr)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    SQueueDB& job_db = q->db;
    SJobInfoDB& job_info_db = q->m_JobInfoDB;

    int record_size = field_descr.field_nums.size();

    // Retrieve fields
    unsigned first_id, last_id;
    first_id = 0;
    TNSBitVector::enumerator en(ids.first());
    {{
        CFastMutexGuard guard(q->lock);
        job_db.SetTransaction(NULL);
        job_info_db.SetTransaction(NULL);
        for ( ; en.valid(); ++en) {
            map<string, string> tags;
            unsigned id = *en;
            job_db.id = id;

            EBDB_ErrCode res;
            if ((res = job_db.Fetch()) != eBDB_Ok) {
                if (res != eBDB_NotFound)
                    LOG_POST(Error << "Error reading queue job db");
                continue;
            }
            bool job_info_fetched = false;
            if (field_descr.has_tags) {
                job_info_db.id = id;
                if ((res = job_info_db.Fetch()) != eBDB_Ok) {
                    if (res != eBDB_NotFound)
                        LOG_POST(Error << "Error reading queue jobinfo db");
                    continue;
                }
                job_info_fetched = true;
                // Parse tags record
                const char* strtags = job_info_db.tags;
                list<string> tokens;
                NStr::Split(strtags, "\t", tokens, NStr::eNoMergeDelims);
                for (list<string>::iterator it = tokens.begin(); it != tokens.end(); ++it) {
                    string key(*it); ++it;
                    if (it != tokens.end()){
                        tags[key] = *it;
                    } else {
                        tags[key] = kEmptyStr;
                        break;
                    }
                }
            }
            if (!first_id) first_id = id;
            last_id = id;
            // Fill out the record
            vector<string> record(record_size);
            bool complete = true;
            for (int i = 0; i < record_size; ++i) {
                int fnum = field_descr.field_nums[i];
                if (fnum < 0) {
                    map<string, string>::iterator it =
                        tags.find((field_descr.pos_to_tag)[i]);
                    if (it == tags.end()) {
                        complete = false;
                        break;
                    }
                    record[i] = string(it->second);
                } else if (fnum == FIELD_INPUT) {
                    job_info_fetched = x_GetInput(job_db, job_info_db,
                        job_info_fetched, record[i]);
                } else if (fnum == FIELD_OUTPUT) {
                    job_info_fetched = x_GetOutput(job_db, job_info_db,
                        job_info_fetched, record[i]);
                } else {
                    record[i] = q->GetField(fnum);
                }
            }
            if (complete)
                record_set.push_back(record);
        }
    }}
}


void CQueue::PrintNodeStat(CNcbiOstream& out) const
{
    CRef<SLockedQueue> q(x_GetLQueue());

    const SLockedQueue::TListenerList& wnodes = q->wnodes;
    SLockedQueue::TListenerList::size_type lst_size;
    CReadLockGuard guard(q->wn_lock);
    lst_size = wnodes.size();
  
    time_t curr = time(0);

    for (SLockedQueue::TListenerList::size_type i = 0; i < lst_size; ++i) {
        const SQueueListener* ql = wnodes[i];
        time_t last_connect = ql->last_connect;

        // cut off one day old obsolete connections
        if ( (last_connect + (24 * 3600)) < curr) {
            continue;
        }

        CTime lc_time(last_connect);
        lc_time.ToLocalTime();
        out << ql->auth << " @ " << CSocketAPI::gethostbyaddr(ql->host) 
            << "  UDP:" << ql->udp_port << "  " << lc_time.AsString() << "\n";
    }
}


void CQueue::PrintSubmHosts(CNcbiOstream& out) const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    CQueueParamAccessor qp(*q);
    qp.GetSubmHosts().PrintHosts(out);
}


void CQueue::PrintWNodeHosts(CNcbiOstream& out) const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    CQueueParamAccessor qp(*q);
    qp.GetWnodeHosts().PrintHosts(out);
}


unsigned int 
CQueue::Submit(SNS_SubmitRecord* rec,
               unsigned      host_addr,
               unsigned      port,
               unsigned      wait_timeout,
               const char*   progress_msg)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    unsigned max_input_size = CQueueParamAccessor(*q).GetMaxInputSize();

    bool was_empty = !q->status_tracker.AnyPending();

    unsigned int job_id = q->GetNextId();

    rec->job_id = job_id;
    SQueueDB& db = q->db;
    SJobInfoDB& job_info_db = q->m_JobInfoDB;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eEnvDefault,
                           CBDB_Transaction::eNoAssociation);

    rec->affinity_id = 0;
    if (*rec->affinity_token) { // not empty
        rec->affinity_id =
            q->affinity_dict.CheckToken(rec->affinity_token, trans);
    }

    {{
        CFastMutexGuard guard(q->lock);
        db.SetTransaction(&trans);
        job_info_db.SetTransaction(&trans);

        bool need_aux_insert = x_AssignSubmitRec(db, job_info_db, rec,
            max_input_size,
            time(0), host_addr, port, wait_timeout, progress_msg);
        db.Insert();

        if (need_aux_insert)
            job_info_db.Insert();


        // update affinity index
        if (rec->affinity_id) {
            q->aff_idx.SetTransaction(&trans);
            x_AddToAffIdx_NoLock(rec->affinity_id, job_id);
        }

        // update tags
        CNSTagMap tag_map;
        q->AppendTags(tag_map, rec->tags, job_id);
        q->FlushTags(tag_map, trans);
    }}

    trans.Commit();
    q->status_tracker.SetStatus(job_id, CNetScheduleAPI::ePending);

    if (was_empty) NotifyListeners(true);

    return job_id;
}


unsigned int 
CQueue::SubmitBatch(vector<SNS_SubmitRecord>& batch,
                    unsigned                  host_addr,
                    unsigned                  port,
                    unsigned                  wait_timeout,
                    const char*               progress_msg)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    unsigned max_input_size = CQueueParamAccessor(*q).GetMaxInputSize();

    bool was_empty = !q->status_tracker.AnyPending();

    unsigned job_id = q->GetNextIdBatch(batch.size());

    SQueueDB& db = q->db;
    SJobInfoDB& job_info_db = q->m_JobInfoDB;

    unsigned batch_aff_id = 0; // if batch comes with the same affinity
    bool     batch_has_aff = false;

    // process affinity ids
    {{
        CBDB_Transaction trans(*db.GetEnv(), 
                            CBDB_Transaction::eEnvDefault,
                            CBDB_Transaction::eNoAssociation);
        for (unsigned i = 0; i < batch.size(); ++i) {
            SNS_SubmitRecord& subm = batch[i];
            if (subm.affinity_id == (unsigned)kMax_I4) { // take prev. token
                _ASSERT(i > 0);
                subm.affinity_id = batch[i-1].affinity_id;
            } else {
                if (subm.affinity_token[0]) {
                    subm.affinity_id = 
                        q->affinity_dict.CheckToken(subm.affinity_token, trans);
                    batch_has_aff = true;
                    batch_aff_id = (i == 0 )? subm.affinity_id : 0;
                } else {
                    subm.affinity_id = 0;
                    batch_aff_id = 0;
                }

            }
        }
        trans.Commit();
    }}

    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eEnvDefault,
                           CBDB_Transaction::eNoAssociation);

    {{
        CNSTagMap tag_map;
        CFastMutexGuard guard(q->lock);
        db.SetTransaction(&trans);
        job_info_db.SetTransaction(&trans);

        unsigned job_id_cnt = job_id;
        time_t now = time(0);
        NON_CONST_ITERATE(vector<SNS_SubmitRecord>, it, batch) {
            it->job_id = job_id_cnt++;
            bool need_aux_insert = x_AssignSubmitRec(db, job_info_db, &(*it),
                max_input_size,
                now, host_addr, port, wait_timeout, progress_msg);
            //for (unsigned n_tries = 0; true; ) {
            //    try {
                    db.Insert();
            //    } catch (CBDB_ErrnoException& ex) {
            //        if ((ex.IsDeadLock() || ex.IsNoMem()) &&
            //            ++n_tries < k_max_dead_locks) {
            //            SleepMilliSec(250);
            //            continue;
            //        }
            //        ERR_POST("Too many transaction repeats in CQueue::SubmitBatch");
            //        throw;
            //    }
            //    break;
            //}

            if (need_aux_insert)
                job_info_db.Insert();

            q->AppendTags(tag_map, it->tags, it->job_id);
        }

        // Store the affinity index
        q->aff_idx.SetTransaction(&trans);
        if (batch_has_aff) {
            if (batch_aff_id) {  // whole batch comes with the same affinity
                x_AddToAffIdx_NoLock(batch_aff_id,
                                    job_id, 
                                    job_id + batch.size() - 1);
            } else {
                x_AddToAffIdx_NoLock(batch);
            }
        }
        q->FlushTags(tag_map, trans);
    }}
    trans.Commit();
    q->status_tracker.AddPendingBatch(job_id, job_id + batch.size() - 1);

    if (was_empty) NotifyListeners(true);

    return job_id;
}


bool 
CQueue::x_AssignSubmitRec(SQueueDB&     db,
                          SJobInfoDB&   job_info_db,
                          const SNS_SubmitRecord* rec,
                          unsigned      max_input_size,
                          time_t        time_submit,
                          unsigned      host_addr,
                          unsigned      port,
                          unsigned      wait_timeout,
                          const char*   progress_msg)
{
    if (rec->input.size() > max_input_size) {
        NCBI_THROW(CNetScheduleException, eDataTooLong,
           "Input is too long");       
    }
    bool need_job_info_insert = false;

    db.id = rec->job_id;
    db.status = (int) CNetScheduleAPI::ePending;

    db.time_submit = time_submit;
    db.time_run = 0;
    db.time_done = 0;
    db.timeout = 0;
    db.run_timeout = 0;

    db.subm_addr = host_addr;
    db.subm_port = port;
    db.subm_timeout = wait_timeout;

    db.worker_node1 = 0;
    db.worker_node2 = 0;
    db.worker_node3 = 0;
    db.worker_node4 = 0;
    db.worker_node5 = 0;

    db.run_counter = 0;
    db.ret_code = 0;
    db.aff_id = rec->affinity_id;
    db.mask = rec->mask;

    bool input_oveflow = rec->input.size() > kNetScheduleSplitSize;
    if (!input_oveflow) {
        db.input_overflow = 0;
        db.input = rec->input;
    } else {
        db.input_overflow = 1;
        db.input = "";
    }
    db.output_overflow = 0;
    db.output = "";

    db.err_msg = "";

    if (progress_msg) {
        db.progress_msg = progress_msg;
    }

    // job info db
    list<string> tag_list;
    ITERATE(TNSTagList, it, rec->tags) {
        need_job_info_insert = true;
        tag_list.push_back(it->first + "\t" + it->second);
    }
    if (input_oveflow) {
        need_job_info_insert = true;
        job_info_db.input = rec->input;
    }
    if (need_job_info_insert) {
        job_info_db.id = rec->job_id;
        job_info_db.tags = NStr::Join(tag_list, "\t");
        if (!input_oveflow)
            job_info_db.input = "";
        job_info_db.output = "";
    }

    return need_job_info_insert;
}


unsigned 
CQueue::CountStatus(CNetScheduleAPI::EJobStatus st) const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    return q->status_tracker.CountStatus(st);
}


void 
CQueue::StatusStatistics(
    CNetScheduleAPI::EJobStatus status,
    TNSBitVector::statistics* st) const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    q->status_tracker.StatusStatistics(status, st);
}


void CQueue::ForceReschedule(unsigned int job_id)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    {{
        SQueueDB& db = q->db;
        CBDB_Transaction trans(*db.GetEnv(), 
                            CBDB_Transaction::eEnvDefault,
                            CBDB_Transaction::eNoAssociation);

        CFastMutexGuard guard(q->lock);
        db.SetTransaction(NULL);

        db.id = job_id;
        if (db.Fetch() == eBDB_Ok) {
            //int status = db.status;
            db.status = (int) CNetScheduleAPI::ePending;
            db.worker_node5 = 0; 

            unsigned run_counter = db.run_counter;
            if (run_counter) {
                db.run_counter = --run_counter;
            }

            db.SetTransaction(&trans);
            db.UpdateInsert();
            trans.Commit();

        } else {
            // TODO: Integrity error or job just expired?
            return;
        }    
    }}

    q->status_tracker.ForceReschedule(job_id);
}


void CQueue::Cancel(unsigned int job_id)
{
    CRef<SLockedQueue> q(x_GetLQueue());

//    CIdBusyGuard id_guard(&m_Db.m_UsedIds, job_id, 3);

    CNetSchedule_JS_Guard js_guard(q->status_tracker, 
                                   job_id,
                                   CNetScheduleAPI::eCanceled);
    CNetScheduleAPI::EJobStatus st = js_guard.GetOldStatus();
    if (q->status_tracker.IsCancelCode(st)) {
        js_guard.Commit();
        return;
    }

    {{
        SQueueDB& db = q->db;
        CBDB_Transaction trans(*db.GetEnv(), 
                            CBDB_Transaction::eEnvDefault,
                            CBDB_Transaction::eNoAssociation);

        CFastMutexGuard guard(q->lock);
        db.SetTransaction(NULL);

        db.id = job_id;
        if (db.Fetch() == eBDB_Ok) {
            int status = db.status;
            if (status < (int) CNetScheduleAPI::eCanceled) {
                db.status = (int) CNetScheduleAPI::eCanceled;
                db.time_done = time(0);
                
                db.SetTransaction(&trans);
                db.UpdateInsert();
                trans.Commit();
            }
        } else {
            // TODO: Integrity error or job just expired?
        }    
    }}
    js_guard.Commit();

    x_RemoveFromTimeLine(job_id);
}


void CQueue::DropJob(unsigned job_id)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    q->Erase(job_id);
    x_RemoveFromTimeLine(job_id);

    if (IsMonitoring()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::DropJob() job id=";
        msg += NStr::IntToString(job_id);
        MonitorPost(msg);
    }
}


void CQueue::PutResult(unsigned int  job_id,
                       int           ret_code,
                       const string& output)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    bool delete_done;
    unsigned max_output_size;
    {{
        CQueueParamAccessor qp(*q);
        delete_done = qp.GetDeleteDone();
        max_output_size = qp.GetMaxOutputSize();
    }}

    if (output.size() > max_output_size) {
        NCBI_THROW(CNetScheduleException, eDataTooLong,
           "Output is too long");       
    }

    CNetSchedule_JS_Guard js_guard(q->status_tracker, 
                                   job_id,
                                   CNetScheduleAPI::eDone);
    if (delete_done) {
        q->Erase(job_id);
    }
    CNetScheduleAPI::EJobStatus st = js_guard.GetOldStatus();
    if (q->status_tracker.IsCancelCode(st)) {
        js_guard.Commit();
        return;
    }
    bool rec_updated;
    SSubmitNotifInfo si;
    time_t curr = time(0);

    SQueueDB& db = q->db;
    SJobInfoDB& job_info_db = q->m_JobInfoDB;
     

    for (unsigned repeat = 0; true; ) {
        try {
            CBDB_Transaction trans(*db.GetEnv(), 
                                   CBDB_Transaction::eEnvDefault,
                                   CBDB_Transaction::eNoAssociation);

            {{
                CFastMutexGuard guard(q->lock);
                db.SetTransaction(&trans);
                job_info_db.SetTransaction(&trans);
                CBDB_FileCursor& cur = *q->GetCursor(trans);
                CBDB_CursorGuard cg(cur);
                rec_updated = 
                    x_UpdateDB_PutResultNoLock(db, job_info_db, trans,
                                               delete_done, curr, cur, 
                                               job_id, ret_code, output, &si);
            }}

            trans.Commit();
            js_guard.Commit();
            if (job_id) x_Count(SLockedQueue::eStatPutEvent);
        } 
        catch (CBDB_ErrnoException& ex) {
            if (ex.IsDeadLock()) {
                if (++repeat < k_max_dead_locks) {
                    if (IsMonitoring()) {
                        MonitorPost(
                            "DeadLock repeat in CQueue::PutResult");
                    }
                    SleepMilliSec(250);
                    continue;
                }
            } else 
            if (ex.IsNoMem()) {
                if (++repeat < k_max_dead_locks) {
                    if (IsMonitoring()) {
                        MonitorPost(
                            "No resource repeat in CQueue::PutResult");
                    }
                    SleepMilliSec(250);
                    continue;
                }
            }
            ERR_POST("Too many transaction repeats in CQueue::PutResult.");
            throw;
        }
        break;
    } // for

    x_RemoveFromTimeLine(job_id);

    if (rec_updated) {
        // check if we need to send a UDP notification
        if ( si.subm_timeout && si.subm_addr && si.subm_port &&
            (si.time_submit + si.subm_timeout >= (unsigned)curr)) {

            char msg[1024];
            sprintf(msg, "JNTF %u", job_id);

            CFastMutexGuard guard(q->us_lock);

            //EIO_Status status = 
            q->udp_socket.Send(msg, strlen(msg)+1, 
                              CSocketAPI::ntoa(si.subm_addr), si.subm_port);
        }

        // Update runtime statistics

        unsigned run_elapsed = curr - si.time_run;
        unsigned turn_around = curr - si.time_submit;
        {{
            CFastMutexGuard guard(q->qstat_lock);
            q->qstat.run_count++;
            q->qstat.total_run_time += run_elapsed;
            q->qstat.total_turn_around_time += turn_around;
        }}

    }

    if (IsMonitoring()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::PutResult() job id=";
        msg += NStr::IntToString(job_id);
        msg += " ret_code=";
        msg += NStr::IntToString(ret_code);
        msg += " output=";
        msg += output;

        MonitorPost(msg);
    }
}


bool 
CQueue::x_UpdateDB_PutResultNoLock(SQueueDB&            db,
                                   SJobInfoDB&          job_info_db,
                                   CBDB_Transaction&    trans,
                                   bool                 delete_done,
                                   time_t               curr,
                                   CBDB_FileCursor&     cur,
                                   unsigned             job_id,
                                   int                  ret_code,
                                   const string&        output,
                                   SSubmitNotifInfo*    subm_info)
{
    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << job_id;

    if (cur.FetchFirst() != eBDB_Ok) {
        // TODO: Integrity error or job just expired?
        return false;
    }

    if (subm_info) {
        subm_info->time_submit  = db.time_submit;
        subm_info->time_run     = db.time_run;
        subm_info->subm_addr    = db.subm_addr;
        subm_info->subm_port    = db.subm_port;
        subm_info->subm_timeout = db.subm_timeout;
    }

    if (delete_done) {
        cur.Delete();
    } else {
        db.status = (int) CNetScheduleAPI::eDone;
        db.ret_code = ret_code;
        x_SetOutput(db, job_info_db, trans, output);
        db.time_done = curr;

        cur.Update();
    }
    return true;
}


SQueueDB* CQueue::x_GetLocalDb()
{
    return 0;
    CRef<SLockedQueue> q(x_GetLQueue());
    SQueueDB* pqdb = m_QueueDB.get();
    if (pqdb == 0  &&  ++m_QueueDbAccessCounter > 1) {
        printf("Opening private db\n"); // DEBUG
        const string& file_name = q->db.FileName();
        CBDB_Env* env = q->db.GetEnv();
        m_QueueDB.reset(pqdb = new SQueueDB());
        if (env) {
            pqdb->SetEnv(*env);
        }
        pqdb->Open(file_name.c_str(), CBDB_RawFile::eReadWrite);
    }
    return pqdb;
}


CBDB_FileCursor* 
CQueue::x_GetLocalCursor(CBDB_Transaction& trans)
{
    CBDB_FileCursor* pcur = m_QueueDB_Cursor.get();
    if (pcur) {
        pcur->ReOpen(&trans);
        return pcur;
    }

    SQueueDB* pqdb = m_QueueDB.get();
    if (pqdb == 0) {
        return 0;
    }
    pcur = new CBDB_FileCursor(*pqdb, 
                               trans,
                               CBDB_FileCursor::eReadModifyUpdate);
    m_QueueDB_Cursor.reset(pcur);
    return pcur;
}


void 
CQueue::PutResultGetJob(unsigned int   done_job_id,
                        int            ret_code,
                        const string&  output,
                        unsigned int   worker_node,
                        unsigned int*  job_id,
                        string&        input,
                        const string&  client_name,
                        unsigned*      job_mask,
                        const list<string>& aff_list,
                        string&        aff_token)
{
    _ASSERT(job_id);
    _ASSERT(job_mask);

    CRef<SLockedQueue> q(x_GetLQueue());
    bool delete_done;
    unsigned max_output_size;
    {{
        CQueueParamAccessor qp(*q);
        delete_done = qp.GetDeleteDone();
        max_output_size = qp.GetMaxOutputSize();
    }}

    if (output.size() > max_output_size) {
        NCBI_THROW(CNetScheduleException, eDataTooLong,
           "Output is too long");       
    }

    unsigned dead_locks = 0;       // dead lock counter

    time_t curr = time(0);
    
    // 
    bool need_update;
    CNetSchedule_JS_Guard js_guard(q->status_tracker, 
                                   done_job_id,
                                   CNetScheduleAPI::eDone,
                                   &need_update);
    // This is a HACK - if js_guard is not commited, it will rollback
    // to previous state, so it is safe to change status after the guard.
    if (delete_done) {
        q->Erase(done_job_id);
    }
    // TODO: implement transaction wrapper (a la js_guard above)
    // for x_FindPendingJob
    // TODO: move affinity assignment there as well
    unsigned pending_job_id = x_FindPendingJob(client_name, worker_node);
    bool done_rec_updated = false;
    bool use_db_mutex;

    // When working with the same database file concurrently there is
    // chance of internal Berkeley DB deadlock. (They say it's legal!)
    // In this case Berkeley DB returns an error code(DB_LOCK_DEADLOCK)
    // and recovery is up to the application.
    // If it happens I repeat the transaction several times.
    //
repeat_transaction:
    SQueueDB* pqdb = x_GetLocalDb();
    if (pqdb) {
        // we use private (this thread only data file)
        use_db_mutex = false;
    } else {
        use_db_mutex = true;
        pqdb = &q->db;
    }

    SJobInfoDB& job_info_db = q->m_JobInfoDB;

    unsigned job_aff_id = 0;

    try {
        CBDB_Transaction trans(*(pqdb->GetEnv()), 
                            CBDB_Transaction::eEnvDefault,
                            CBDB_Transaction::eNoAssociation);

        if (use_db_mutex) {
            q->lock.Lock();
        }
        pqdb->SetTransaction(&trans);
        job_info_db.SetTransaction(&trans);

        CBDB_FileCursor* pcur;
        if (use_db_mutex) {
            pcur = q->GetCursor(trans);
        } else {
            pcur = x_GetLocalCursor(trans);
        }
        CBDB_FileCursor& cur = *pcur;
        {{
            CBDB_CursorGuard cg(cur);

            // put result
            if (need_update) {
                done_rec_updated = x_UpdateDB_PutResultNoLock(
                    *pqdb, job_info_db, trans, delete_done,
                    curr, cur, done_job_id, ret_code, output, NULL);
            }

            if (pending_job_id) {
                *job_id = pending_job_id;
                EGetJobUpdateStatus upd_status;
                upd_status =
                    x_UpdateDB_GetJobNoLock(*pqdb, job_info_db,
                        curr, cur, trans,
                        worker_node, pending_job_id, input,
                        &job_aff_id, job_mask);
                switch (upd_status) {
                case eGetJobUpdate_JobFailed:
                    q->status_tracker.ChangeStatus(*job_id, 
                                                   CNetScheduleAPI::eFailed);
                    *job_id = 0;
                    break;
                case eGetJobUpdate_JobStopped:
                    *job_id = 0;
                    break;
                case eGetJobUpdate_NotFound:
                    *job_id = 0;
                    break;
                case eGetJobUpdate_Ok:
                    break;
                default:
                    _ASSERT(0);
                } // switch

            } else {
                *job_id = 0;
            }
        }}

        if (use_db_mutex) {
            q->lock.Unlock();
        }

        trans.Commit();
        js_guard.Commit();
        // TODO: commit x_FindPendingJob guard here
        if (done_job_id) x_Count(SLockedQueue::eStatPutEvent);
        if (*job_id) x_Count(SLockedQueue::eStatGetEvent);
    }
    catch (CBDB_ErrnoException& ex) {
        if (use_db_mutex) {
            q->lock.Unlock();
        }
        if (ex.IsDeadLock()) {
            if (++dead_locks < k_max_dead_locks) {
                if (IsMonitoring()) {
                    MonitorPost(
                        "DeadLock repeat in CQueue::JobExchange");
                }
                SleepMilliSec(250);
                goto repeat_transaction;
            } 
        }
        else
        if (ex.IsNoMem()) {
            if (++dead_locks < k_max_dead_locks) {
                if (IsMonitoring()) {
                    MonitorPost(
                        "No resource repeat in CQueue::JobExchange");
                }
                SleepMilliSec(250);
                goto repeat_transaction;
            }
        }
        ERR_POST("Too many transaction repeats in CQueue::JobExchange.");
        throw;
    }
    catch (...) {
        if (use_db_mutex) {
            q->lock.Unlock();
        }
        throw;
    }

    if (job_aff_id) {
        CFastMutexGuard aff_guard(q->aff_map_lock);
        q->worker_aff_map.AddAffinity(worker_node, client_name,
                                      job_aff_id);
        aff_token = q->affinity_dict.GetAffToken(job_aff_id);
    }

    x_TimeLineExchange(done_job_id, *job_id, curr);

    if (done_rec_updated) {
        // TODO: send a UDP notification and update runtime stat
    }

    if (IsMonitoring()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();

        msg += " CQueue::PutResultGetJob() (PUT) job id=";
        msg += NStr::IntToString(done_job_id);
        msg += " ret_code=";
        msg += NStr::IntToString(ret_code);
        msg += " output=";
        msg += output;

        MonitorPost(msg);
    }
}


void CQueue::GetJob(unsigned int   worker_node,
                    // ??? user SNS_SubmitRecord here
                    unsigned int*  job_id, 
                    string&        input,
                    const string&  client_name,
                    unsigned*      job_mask,
                    const list<string>& aff_list,
                    string&        aff_token)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    _ASSERT(worker_node);
    unsigned get_attempts = 0;
    const unsigned kMaxGetAttempts = 100;
    EGetJobUpdateStatus upd_status;

get_job_id:

    ++get_attempts;
    if (get_attempts > kMaxGetAttempts) {
        *job_id = 0;
        return;
    }
    time_t curr = time(0);

    // affinity: get list of job candidates
    // previous GetJob() call may have precomputed sutable job ids
    //

    *job_id = x_FindPendingJob(client_name, worker_node);
    if (!*job_id) {
        return;
    }

    unsigned job_aff_id = 0;

    try {
        SQueueDB& db = q->db;
        SJobInfoDB& job_info_db = q->m_JobInfoDB;

        CBDB_Transaction trans(*db.GetEnv(), 
                               CBDB_Transaction::eEnvDefault,
                               CBDB_Transaction::eNoAssociation);


        {{
            CFastMutexGuard guard(q->lock);
            db.SetTransaction(&trans);
            job_info_db.SetTransaction(&trans);

            CBDB_FileCursor& cur = *q->GetCursor(trans);
            CBDB_CursorGuard cg(cur);

            upd_status =
                x_UpdateDB_GetJobNoLock(db, job_info_db, curr, cur, trans,
                                        worker_node, *job_id, input,
                                        &job_aff_id, job_mask);
        }}
        trans.Commit();

        switch (upd_status) {
        case eGetJobUpdate_JobFailed:
            q->status_tracker.ChangeStatus(*job_id,
                                           CNetScheduleAPI::eFailed);
            *job_id = 0;
            break;
        case eGetJobUpdate_JobStopped:
            *job_id = 0;
            break;
        case eGetJobUpdate_NotFound:
            *job_id = 0;
            break;
        case eGetJobUpdate_Ok:
            break;
        default:
            _ASSERT(0);
        } // switch

        if (*job_id) x_Count(SLockedQueue::eStatGetEvent);

        if (IsMonitoring() && *job_id) {
            CTime tmp_t(CTime::eCurrent);
            string msg = tmp_t.AsString();
            msg += " CQueue::GetJob() job id=";
            msg += NStr::IntToString(*job_id);
            msg += " worker_node=";
            msg += CSocketAPI::gethostbyaddr(worker_node);
            MonitorPost(msg);
        }

    } 
    catch (exception&)
    {
        q->status_tracker.ChangeStatus(*job_id, CNetScheduleAPI::ePending);
        *job_id = 0;
        throw;
    }

    // if we picked up expired job and need to re-get another job id    
    if (*job_id == 0) {
        goto get_job_id;
    }

    if (job_aff_id) {
        CFastMutexGuard aff_guard(q->aff_map_lock);
        q->worker_aff_map.AddAffinity(worker_node,
                                      client_name,
                                      job_aff_id);
        aff_token = q->affinity_dict.GetAffToken(job_aff_id);
    }

    x_AddToTimeLine(*job_id, curr);
}


void CQueue::PutProgressMessage(unsigned int  job_id,
                                const char*   msg)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    SQueueDB& db = q->db;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eEnvDefault,
                           CBDB_Transaction::eNoAssociation);

    CFastMutexGuard guard(q->lock);
    db.SetTransaction(&trans);

    {{
        CBDB_FileCursor& cur = *q->GetCursor(trans);
        CBDB_CursorGuard cg(cur);    

        cur.SetCondition(CBDB_FileCursor::eEQ);
        cur.From << job_id;

        if (cur.FetchFirst() != eBDB_Ok) {
            // TODO: Integrity error or job just expired?
            return;
        }
        db.progress_msg = msg;
        cur.Update();
    }}

    trans.Commit();

    if (IsMonitoring()) {
        CTime tmp_t(CTime::eCurrent);
        string mmsg = tmp_t.AsString();
        mmsg += " CQueue::PutProgressMessage() job id=";
        mmsg += NStr::IntToString(job_id);
        mmsg += " msg=";
        mmsg += msg;

        MonitorPost(mmsg);
    }

}


void CQueue::JobFailed(unsigned int  job_id,
                       const string& err_msg,
                       const string& output,
                       int           ret_code,
                       unsigned int  worker_node,
                       const string& client_name)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    unsigned failed_retries;
    unsigned max_output_size;
    {{
        CQueueParamAccessor qp(*q);
        failed_retries = CQueueParamAccessor(*q).GetFailedRetries();
        max_output_size = qp.GetMaxOutputSize();
    }}

    if (output.size() > max_output_size) {
        NCBI_THROW(CNetScheduleException, eDataTooLong,
           "Output is too long");       
    }
    // We first change memory state to "Failed", it is safer because
    // there is only danger to find job in inconsistent state, and because
    // Failed is terminal, usually you can not allocate job or do anything
    // disturbing from this state.
    CNetSchedule_JS_Guard js_guard(q->status_tracker, 
                                   job_id,
                                   CNetScheduleAPI::eFailed);

    SQueueDB& db = q->db;
    SJobInfoDB& job_info_db = q->m_JobInfoDB;

    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eEnvDefault,
                           CBDB_Transaction::eNoAssociation);

    unsigned subm_addr, subm_port, subm_timeout, time_submit;
    time_t curr = time(0);

    {{
        CFastMutexGuard aff_guard(q->aff_map_lock);
        CFastMutexGuard guard(q->lock);
        db.SetTransaction(&trans);
        job_info_db.SetTransaction(&trans);

        CBDB_FileCursor& cur = *q->GetCursor(trans);
        CBDB_CursorGuard cg(cur);    

        cur.SetCondition(CBDB_FileCursor::eEQ);
        cur.From << job_id;

        if (cur.FetchFirst() != eBDB_Ok) {
            // TODO: Integrity error or job just expired?
            return;
        }

        unsigned run_counter = db.run_counter;
        if (run_counter <= failed_retries) {
            // NB: due to conflict with locking pattern of x_FindPendingJob we can not
            // acquire aff_map_lock here! So we aquire it earlier (see above).
            q->worker_aff_map.BlacklistJob(worker_node, client_name, job_id);
            // Pending status is not a bug here, returned and pending
            // has the same meaning, but returned jobs are getting delayed
            // for a little while (eReturned status)
            db.status = (int) CNetScheduleAPI::ePending;
            // We can do this because js_guard record only old state and
            // on Commit just releases job.
            q->status_tracker.SetStatus(job_id,
                CNetScheduleAPI::eReturned);
        } else {
            db.status = (int) CNetScheduleAPI::eFailed;
        }

        // We don't need to lock affinity map anymore, so to reduce locking
        // region we can manually release it here.
        aff_guard.Release();

        db.time_done = curr;
        db.err_msg = err_msg;
        x_SetOutput(db, job_info_db, trans, output);
        db.ret_code = ret_code;

        time_submit = db.time_submit;
        subm_addr = db.subm_addr;
        subm_port = db.subm_port;
        subm_timeout = db.subm_timeout;

        cur.Update();
    }}

    trans.Commit();
    js_guard.Commit();

    x_RemoveFromTimeLine(job_id);

    // check if we need to send a UDP notification
    if (subm_addr && subm_timeout &&
        (time_submit + subm_timeout >= (unsigned)curr)) {

        char msg[1024];
        sprintf(msg, "JNTF %u", job_id);

        CFastMutexGuard guard(q->us_lock);

        q->udp_socket.Send(msg, strlen(msg)+1, 
                                  CSocketAPI::ntoa(subm_addr), subm_port);
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
        if (db.status == (int) CNetScheduleAPI::ePending)
            msg += " rescheduled";
        MonitorPost(msg);
    }
}


void CQueue::SetJobRunTimeout(unsigned job_id, unsigned tm)
{
    if (IsMonitoring()) {
        CRef<SLockedQueue> q(x_GetLQueue());
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " OBSOLETE CQueue::SetJobRunTimeout: Job id=";
        msg += NStr::IntToString(job_id);
        msg += " job_timeout(sec)=";
        msg += NStr::IntToString(tm);
        msg += " job_timeout(minutes)=";
        msg += NStr::IntToString(tm/60);

        MonitorPost(msg);
    }
    LOG_POST(Warning << "Obsolete API SetRunTimeout called");
    NCBI_THROW(CNetScheduleException,
        eObsoleteCommand, "Use API JobDelayExpiration (cmd JDEX) instead");
}

void CQueue::JobDelayExpiration(unsigned job_id, unsigned tm)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    unsigned queue_run_timeout = CQueueParamAccessor(*q).GetRunTimeout();

    unsigned q_time_descr = 20;
    unsigned run_timeout;
    unsigned time_run;

    CNetScheduleAPI::EJobStatus st = GetStatus(job_id);
    if (st != CNetScheduleAPI::eRunning) {
        return;
    }
    SQueueDB& db = q->db;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eEnvDefault,
                           CBDB_Transaction::eNoAssociation);

    unsigned exp_time = 0;
    time_t curr = time(0);

    {{
        CFastMutexGuard guard(q->lock);
        db.SetTransaction(&trans);

        CBDB_FileCursor& cur = *q->GetCursor(trans);
        CBDB_CursorGuard cg(cur);    

        cur.SetCondition(CBDB_FileCursor::eEQ);
        cur.From << job_id;

        if (cur.FetchFirst() != eBDB_Ok) {
            return;
        }

        //    int status = db.status;

        time_run = db.time_run;
        if (time_run == 0) {
            // Impossible
            LOG_POST(Error
                << "Internal error: time_run==0 for running job id="
                << job_id);
            // Fix it just in case
            time_run = curr;
            db.time_run = curr;
        }
        run_timeout = db.run_timeout;
        if (run_timeout == 0) {
            run_timeout = queue_run_timeout;
        }

        // check if current timeout is enough and job requires no prolongation
        time_t safe_exp_time = 
            curr + std::max(queue_run_timeout, 2*tm) + q_time_descr;
        if (time_run + run_timeout > (unsigned) safe_exp_time) {
            return;
        }

        while (time_run + run_timeout <= (unsigned) safe_exp_time) {
            run_timeout += std::max(queue_run_timeout, tm);
        }
        db.run_timeout = run_timeout;

        cur.Update();
    }}

    trans.Commit();
    exp_time = x_ComputeExpirationTime(time_run, run_timeout);

    {{
        CWriteLockGuard guard(q->rtl_lock);
        q->run_time_line->MoveObject(exp_time, curr + tm, job_id);
    }}

    if (IsMonitoring()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::JobDelayExpiration: Job id=";
        msg += NStr::IntToString(job_id);
        tmp_t.SetTimeT(curr + tm);
        tmp_t.ToLocalTime();
        msg += " new_expiration_time=";
        msg += tmp_t.AsString();
        msg += " job_timeout(sec)=";
        msg += NStr::IntToString(run_timeout);
        msg += " job_timeout(minutes)=";
        msg += NStr::IntToString(run_timeout/60);

        MonitorPost(msg);
    }
}


void CQueue::ReturnJob(unsigned int job_id)
{
    _ASSERT(job_id);

    CRef<SLockedQueue> q(x_GetLQueue());

    CNetSchedule_JS_Guard js_guard(q->status_tracker, 
                                   job_id,
                                   CNetScheduleAPI::eReturned);
    CNetScheduleAPI::EJobStatus st = js_guard.GetOldStatus();
    // if canceled or already returned or done
    if (q->status_tracker.IsCancelCode(st) || 
        (st == CNetScheduleAPI::eReturned) || 
        (st == CNetScheduleAPI::eDone)) {
        js_guard.Commit();
        return;
    }
    {{
        SQueueDB& db = q->db;

        CFastMutexGuard guard(q->lock);
        db.SetTransaction(NULL);

        db.id = job_id;
        if (db.Fetch() == eBDB_Ok) {            
            CBDB_Transaction trans(*db.GetEnv(), 
                                CBDB_Transaction::eEnvDefault,
                                CBDB_Transaction::eNoAssociation);
            db.SetTransaction(&trans);

            // Pending status is not a bug here, returned and pending
            // has the same meaning, but returned jobs are getting delayed
            // for a little while (eReturned status)
            db.status = (int) CNetScheduleAPI::ePending;
            unsigned run_counter = db.run_counter;
            if (run_counter) {
                db.run_counter = --run_counter;
            }

            db.UpdateInsert();

            trans.Commit();
        } else {
            // TODO: Integrity error or job just expired?
        }    
    }}
    js_guard.Commit();
    x_RemoveFromTimeLine(job_id);

    if (IsMonitoring()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::ReturnJob: job id=";
        msg += NStr::IntToString(job_id);
        MonitorPost(msg);
    }
}


unsigned 
CQueue::x_FindPendingJob(const string&  client_name,
                         unsigned       client_addr)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    unsigned job_id = 0;

    TNSBitVector blacklisted_jobs;

    // affinity: get list of job candidates
    // previous x_FindPendingJob() call may have precomputed candidate jobids
    {{
        CFastMutexGuard aff_guard(q->aff_map_lock);
        CWorkerNodeAffinity::SAffinityInfo* ai = 
            q->worker_aff_map.GetAffinity(client_addr, client_name);

        if (ai != 0) {  // established affinity association
            blacklisted_jobs = ai->blacklisted_jobs;
            do {
                // check for candidates
                if (!ai->candidate_jobs.any() && ai->aff_ids.any()) {
                    // there is an affinity association
                    {{
                        CFastMutexGuard guard(q->lock);
                        x_ReadAffIdx_NoLock(ai->aff_ids, &ai->candidate_jobs);
                    }}
                    if (!ai->candidate_jobs.any()) // no candidates
                        break;
                    q->status_tracker.PendingIntersect(&ai->candidate_jobs);
                    ai->candidate_jobs -= blacklisted_jobs;
                    ai->candidate_jobs.count(); // speed up any()
                    if (!ai->candidate_jobs.any())
                        break;
                    ai->candidate_jobs.optimize(0, TNSBitVector::opt_free_0);
                }
                if (!ai->candidate_jobs.any())
                    break;
                bool pending_jobs_avail = 
                    q->status_tracker.GetPendingJobFromSet(
                        &ai->candidate_jobs, &job_id);
                if (job_id)
                    return job_id;
                if (!pending_jobs_avail)
                    return 0;
            } while (0);
        }
    }}

    // no affinity association or there are no more jobs with 
    // established affinity

    // try to find a vacant(not taken by any other worker node) affinity id
    {{
        TNSBitVector assigned_aff;
        {{
            CFastMutexGuard aff_guard(q->aff_map_lock);
            q->worker_aff_map.GetAllAssignedAffinity(&assigned_aff);
        }}

        if (assigned_aff.any()) {
            // get all jobs belonging to other (already assigned) affinities,
            // ORing them with our own blacklisted jobs
            TNSBitVector assigned_candidate_jobs(blacklisted_jobs);
            {{
                CFastMutexGuard guard(q->lock);
                // x_ReadAffIdx_NoLock actually ORs into second argument
                x_ReadAffIdx_NoLock(assigned_aff, &assigned_candidate_jobs);
            }}
            // we got list of jobs we do NOT want to schedule
            bool pending_jobs_avail = 
                q->status_tracker.GetPendingJob(assigned_candidate_jobs,
                                               &job_id);
            if (job_id)
                return job_id;
            if (!pending_jobs_avail) {
                return 0;
            }
        }
    }}

    // We just take the first available job in the queue, taking into account
    // blacklisted jobs as usual.
    _ASSERT(job_id == 0);
    q->status_tracker.GetPendingJob(blacklisted_jobs, &job_id);

    return job_id;
}


CQueue::EGetJobUpdateStatus 
CQueue::x_UpdateDB_GetJobNoLock(SQueueDB&            db,
                                SJobInfoDB&          job_info_db,
                                time_t               curr,
                                CBDB_FileCursor&     cur,
                                CBDB_Transaction&    trans,
                                unsigned int         worker_node,
                                unsigned             job_id,
                                // TODO: ??? use SNS_SubmitRecord here
                                string&              input,
                                unsigned*            aff_id,
                                unsigned*            job_mask)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    unsigned queue_timeout = CQueueParamAccessor(*q).GetTimeout();

    const unsigned kMaxGetAttempts = 100;

    for (unsigned fetch_attempts = 0; fetch_attempts < kMaxGetAttempts;
         ++fetch_attempts) {

        cur.SetCondition(CBDB_FileCursor::eEQ);
        cur.From << job_id;

        EBDB_ErrCode res;
        if ((res = cur.Fetch()) != eBDB_Ok) {
            if (res == eBDB_NotFound) return eGetJobUpdate_NotFound;
            // retry
            cur.Close();
            cur.ReOpen(&trans);
            continue;
        }
        int status = db.status;

        // internal integrity check
        if (!(status == (int)CNetScheduleAPI::ePending ||
              status == (int)CNetScheduleAPI::eReturned)
            ) {
            if (q->status_tracker.IsCancelCode(
                (CNetScheduleAPI::EJobStatus) status)) {
                // this job has been canceled while i'm fetching
                return eGetJobUpdate_JobStopped;
            }
            ERR_POST(Error
                << "x_UpdateDB_GetJobNoLock: Status integrity violation "
                << " job = "     << job_id
                << " status = "  << status
                << " expected status = "
                << (int)CNetScheduleAPI::ePending);
            return eGetJobUpdate_JobStopped;
        }

        unsigned time_submit = db.time_submit;
        unsigned timeout = db.timeout;
        if (timeout == 0) timeout = queue_timeout;

        _ASSERT(timeout);
        // check if job already expired
        if (timeout && (time_submit + timeout < (unsigned)curr)) {
            db.time_done = curr;
            db.status = (int) CNetScheduleAPI::eFailed;
            db.err_msg = "Job expired and cannot be scheduled.";
            q->status_tracker.ChangeStatus(job_id, 
                                           CNetScheduleAPI::eFailed);

            cur.Update();

            if (IsMonitoring()) {
                CTime tmp_t(CTime::eCurrent);
                string msg = tmp_t.AsString();
                msg += 
                 " CQueue::x_UpdateDB_GetJobNoLock() timeout expired job id=";
                msg += NStr::IntToString(job_id);
                MonitorPost(msg);
            }
            return eGetJobUpdate_JobFailed;

        } else {  // job not expired
            unsigned run_counter = (unsigned) db.run_counter + 1;

            switch (run_counter) {
            case 1:
                db.worker_node1 = worker_node;
                break;
            case 2:
                db.worker_node2 = worker_node;
                break;
            case 3:
                db.worker_node3 = worker_node;
                break;
            case 4:
                db.worker_node4 = worker_node;
                break;
            case 5:
                db.worker_node5 = worker_node;
                break;
            default:
                q->status_tracker.ChangeStatus(job_id, 
                                               CNetScheduleAPI::eFailed);
                ERR_POST(Error << "Too many run attempts. job=" << job_id);
                db.status = (int) CNetScheduleAPI::eFailed;
                db.err_msg = "Too many run attempts.";
                db.time_done = curr;

                cur.Update();

                if (IsMonitoring()) {
                    CTime tmp_t(CTime::eCurrent);
                    string msg = tmp_t.AsString();
                    msg += " CQueue::GetJob() Too many run attempts job id=";
                    msg += NStr::IntToString(job_id);
                    MonitorPost(msg);
                }

                return eGetJobUpdate_JobFailed;
            } // switch

            // all checks passed successfully...
            x_GetInput(db, job_info_db, false, input);
            *aff_id = db.aff_id;
            *job_mask = db.mask;

            db.status = (int) CNetScheduleAPI::eRunning;
            db.time_run = curr;
            db.run_timeout = 0;
            db.run_counter = run_counter;

            cur.Update();
            return eGetJobUpdate_Ok;
        } // else
    } // for

    return eGetJobUpdate_NotFound;
}


bool CQueue::x_GetInput(SQueueDB& db, SJobInfoDB& job_info_db,
                        bool fetched, string& str)
{
    if ((char) db.input_overflow) {
        if (!fetched) {
            job_info_db.id = db.id;
            if (job_info_db.Fetch() != eBDB_Ok)
                return false;
            fetched = true;
            job_info_db.input.ToString(str);
        }
    } else {
        db.input.ToString(str);
    }
    return fetched;
}


bool CQueue::x_GetOutput(SQueueDB& db, SJobInfoDB& job_info_db,
                                 bool fetched, string& str)
{
    if ((char) db.output_overflow) {
        if (!fetched) {
            job_info_db.id = db.id;
            if (job_info_db.Fetch() != eBDB_Ok)
                return false;
            fetched = true;
        }
        job_info_db.output.ToString(str);
    } else {
        db.output.ToString(str);
    }
    return fetched;
}


void CQueue::x_SetOutput(SQueueDB& db, SJobInfoDB& job_info_db,
                         CBDB_Transaction& trans, const string& output)
{
        if (output.size() > kNetScheduleSplitSize) {
            CBDB_FileCursor cur(job_info_db, trans,
                                CBDB_FileCursor::eReadModifyUpdate);
            cur.SetCondition(CBDB_FileCursor::eEQ);
            cur.From << db.id;

            db.output_overflow = 1;
            
            EBDB_ErrCode res;
            if ((res = cur.Fetch()) != eBDB_Ok) {
                if (res != eBDB_NotFound) {
                    BDB_ERRNO_THROW(res, "Can't fetch from JobInfoDB");
                } 
                job_info_db.id = db.id;
                job_info_db.tags = "";
                job_info_db.input = "";
                job_info_db.output = output;
                job_info_db.Insert();
            } else {
                job_info_db.output = output;
                cur.Update();
            }
        } else {
            db.output_overflow = 0;
            db.output = output;
        }
}


void CQueue::GetJobKey(char* key_buf, unsigned job_id,
                       const string& host, unsigned port)
{
    sprintf(key_buf, NETSCHEDULE_JOBMASK, job_id, host.c_str(), port);
}


bool 
CQueue::GetJobDescr(unsigned int job_id,
                    int*         ret_code,
                    string*      input,
                    string*      output,
                    string*      err_msg,
                    string*      progress_msg,
                    CNetScheduleAPI::EJobStatus expected_status)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    SQueueDB& db = q->db;
    SJobInfoDB& job_info_db = q->m_JobInfoDB;

    for (unsigned i = 0; i < 3; ++i) {
        {{
        CFastMutexGuard guard(q->lock);
        db.SetTransaction(NULL);
        job_info_db.SetTransaction(NULL);

        db.id = job_id;
        if (db.Fetch() == eBDB_Ok) {
            if (expected_status != CNetScheduleAPI::eJobNotFound) {
                CNetScheduleAPI::EJobStatus status =
                    (CNetScheduleAPI::EJobStatus)(int)db.status;
                if ((status != expected_status) 
                    // The 'Retuned' status does not get saved into db
                    // because it is temporary. (optimization).
                    // this condition reflects that logically Pending == Returned
                    && !(status ==  CNetScheduleAPI::ePending 
                         && expected_status == CNetScheduleAPI::eReturned)) {
                    goto wait_sleep;
                }
            }
            if (ret_code)
                *ret_code = db.ret_code;
            bool fetched = false;
            if (input)
                fetched = x_GetInput(db, job_info_db, fetched, *input);
            if (output)
                x_GetOutput(db, job_info_db, fetched, *output);
            if (err_msg)
                db.err_msg.ToString(*err_msg);
            if (progress_msg)
                db.progress_msg.ToString(*progress_msg);

            return true;
        }
        }}
    wait_sleep:
        // failed to read the record (maybe looks like writer is late, so we 
        // need to retry a bit later)
        SleepMilliSec(300);
    }

    return false; // job not found
}

CNetScheduleAPI::EJobStatus 
CQueue::GetStatus(unsigned int job_id) const
{
    const CRef<SLockedQueue> q(x_GetLQueue());
    return q->status_tracker.GetStatus(job_id);
}

bool CQueue::CountStatus(CJobStatusTracker::TStatusSummaryMap* status_map,
                         const char*                           affinity_token)
{
    _ASSERT(status_map);
    CRef<SLockedQueue> q(x_GetLQueue());

    unsigned aff_id = 0;
    TNSBitVector aff_jobs;
    if (affinity_token && *affinity_token) {
        aff_id = q->affinity_dict.GetTokenId(affinity_token);
        if (!aff_id) {
            return false;
        }
        // read affinity vector
        {{
            CFastMutexGuard guard(q->lock);
            x_ReadAffIdx_NoLock(aff_id, &aff_jobs);
        }}
    }

    q->status_tracker.CountStatus(status_map, aff_id!=0 ? &aff_jobs : 0); 

    return true;
}


unsigned 
CQueue::CheckDeleteBatch(unsigned batch_size,
                         CNetScheduleAPI::EJobStatus status)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    unsigned queue_timeout, queue_run_timeout;
    {{
        CQueueParamAccessor qp(*q);
        queue_timeout = qp.GetTimeout();
        queue_run_timeout = qp.GetRunTimeout();
    }}

    SQueueDB& job_db = q->db;

    TNSBitVector job_ids;

    time_t curr = time(0);

    unsigned dcnt = 0;
    {{
        CFastMutexGuard guard(q->lock);
        job_db.SetTransaction(NULL);
        CBDB_FileCursor cur(job_db);
        // unsigned prev_job_id = 0;
        for (unsigned n = 0; n < batch_size; ++n) {
            unsigned job_id = q->status_tracker.GetFirst(status);
            if (job_id == 0)
                break;
            cur.SetCondition(CBDB_FileCursor::eEQ);
            cur.From << job_id;
            if (cur.FetchFirst() != eBDB_Ok) {
                // ? already deleted
                LOG_POST(Warning << "Discrepancy between matrix and database " <<
                    "for job " << job_id);
                // Do not break the process of erasing expired jobs
                continue;
            }

            // Is the job expired?
            time_t time_update, time_done;
            time_t timeout, run_timeout;

            int status = job_db.status;

            timeout = job_db.timeout;
            if (timeout == 0) timeout = queue_timeout;
            run_timeout = job_db.run_timeout;
            if (run_timeout == 0) run_timeout = queue_run_timeout;

            // Calculate time of last update and effective timeout
            time_done = job_db.time_done;
            if (status == (int) CNetScheduleAPI::eRunning) {
                // Running job
                time_update = job_db.time_run;
                timeout += run_timeout;
            } else if (time_done == 0) {
                // Submitted job
                time_update = job_db.time_submit;
            } else {
                // Done, Failed, ?Returned, Canceled
                time_update = time_done;
            }

            if (time_update + timeout > curr)
                break;

            q->status_tracker.Erase(job_id);
            job_ids.set_bit(job_id);
            ++dcnt;
        }
    }}
    if (dcnt)
        q->Erase(job_ids);
    return dcnt;
}


unsigned
CQueue::DoDeleteBatch(unsigned batch_size)
{

    CRef<SLockedQueue> q(x_GetLQueue());
    unsigned del_rec = q->DeleteBatch(batch_size);
    // monitor this
    if (del_rec > 0 && IsMonitoring()) {
        CTime tm(CTime::eCurrent);
        string msg = tm.AsString();
        msg += " CQueue::DeleteBatch: " +
                NStr::IntToString(del_rec) + " job(s) deleted";
        MonitorPost(msg);
    }
    return del_rec;
}


void CQueue::ClearAffinityIdx()
{
    CRef<SLockedQueue> q(x_GetLQueue());
    q->ClearAffinityIdx();
}


void CQueue::Truncate(void)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    q->Clear();
    // Update 'became_empty' timestamp
    IsExpired(); // locks q->lock
}


void CQueue::x_AddToTimeLine(unsigned job_id, time_t curr)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    unsigned queue_run_timeout = CQueueParamAccessor(*q).GetRunTimeout();
    if (job_id && q->run_time_line) {
        CJobTimeLine& tl = *q->run_time_line;

        CWriteLockGuard guard(q->rtl_lock);
        tl.AddObject(curr + queue_run_timeout, job_id);
    }
}


void CQueue::x_RemoveFromTimeLine(unsigned job_id)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    if (q->run_time_line) {
        CWriteLockGuard guard(q->rtl_lock);
        q->run_time_line->RemoveObject(job_id);
    }

    if (IsMonitoring()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::RemoveFromTimeLine: job id=";
        msg += NStr::IntToString(job_id);
        MonitorPost(msg);
    }
}


void 
CQueue::x_TimeLineExchange(unsigned remove_job_id, 
                           unsigned add_job_id,
                           time_t   curr)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    unsigned queue_run_timeout = CQueueParamAccessor(*q).GetRunTimeout();
    if (q->run_time_line) {
        CJobTimeLine& tl = *q->run_time_line;
        CWriteLockGuard guard(q->rtl_lock);
        tl.RemoveObject(remove_job_id);

        if (add_job_id) {
            tl.AddObject(curr + queue_run_timeout, add_job_id);
        }
    }
    if (IsMonitoring()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::TimeLineExchange: job removed=";
        msg += NStr::IntToString(remove_job_id);
        msg += " job added=";
        msg += NStr::IntToString(add_job_id);
        MonitorPost(msg);
    }
}

SLockedQueue::TListenerList::iterator
CQueue::x_FindListener(unsigned int    host_addr, 
                       unsigned short  udp_port)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    SLockedQueue::TListenerList& wnodes = q->wnodes;
    return find_if(wnodes.begin(), wnodes.end(),
        CQueueComparator(host_addr, udp_port));
}


void CQueue::RegisterNotificationListener(unsigned int    host_addr,
                                          unsigned short  udp_port,
                                          int             timeout,
                                          const string&   auth)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    CWriteLockGuard guard(q->wn_lock);
    SLockedQueue::TListenerList::iterator it =
                                x_FindListener(host_addr, udp_port);
    
    time_t curr = time(0);
    if (it != q->wnodes.end()) {  // update registration timestamp
        SQueueListener& ql = *(*it);
        if ((ql.timeout = timeout) != 0) {
            ql.last_connect = curr;
            ql.auth = auth;
        }
        return;
    }

    // new element
    if (timeout) {
        q->wnodes.push_back(
            new SQueueListener(host_addr, udp_port, curr, timeout, auth));
    }
}

void 
CQueue::UnRegisterNotificationListener(unsigned int    host_addr,
                                       unsigned short  udp_port)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    CWriteLockGuard guard(q->wn_lock);
    SLockedQueue::TListenerList::iterator it =
                                x_FindListener(host_addr, udp_port);
    if (it != q->wnodes.end()) {
        SQueueListener* node = *it;
        delete node;
        q->wnodes.erase(it);
    }
}

void CQueue::ClearAffinity(unsigned int  host_addr,
                           const string& auth)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    CFastMutexGuard guard(q->aff_map_lock);
    q->worker_aff_map.ClearAffinity(host_addr, auth);
}

void CQueue::SetMonitorSocket(CSocket& socket)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    q->SetMonitorSocket(socket);
}

bool CQueue::IsMonitoring()
{
    CRef<SLockedQueue> q(m_LQueue.Lock());
    if (q == NULL)
        return false;
    return q->IsMonitoring();
}

void CQueue::MonitorPost(const string& msg)
{
    CRef<SLockedQueue> q(m_LQueue.Lock());
    if (q == NULL) return;
    return q->MonitorPost(msg);
}

void CQueue::NotifyListeners(bool unconditional)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    int notif_timeout = CQueueParamAccessor(*q).GetNotifTimeout();
    unsigned short udp_port = m_Db.GetUdpPort();
    if (udp_port == 0) {
        return;
    }

    SLockedQueue::TListenerList& wnodes = q->wnodes;

    time_t curr = time(0);

    if (!unconditional &&
        (notif_timeout == 0 ||
         !q->status_tracker.AnyPending())) {
        return;
    }

    SLockedQueue::TListenerList::size_type lst_size;
    {{
        CWriteLockGuard guard(q->wn_lock);
        lst_size = wnodes.size();
        if ((lst_size == 0) ||
            (!unconditional && q->last_notif + notif_timeout > curr)) {
            return;
        } 
        q->last_notif = curr;
    }}

    const char* msg = q->q_notif.c_str();
    size_t msg_len = q->q_notif.length()+1;
    
    for (SLockedQueue::TListenerList::size_type i = 0; 
         i < lst_size; 
         ++i) 
    {
        unsigned host;
        unsigned short port;
        {{
            CReadLockGuard guard(q->wn_lock);
            SQueueListener* ql = wnodes[i];
            if (ql->last_connect + ql->timeout < curr)
                continue;
            host = ql->host;
            port = ql->udp_port;
        }}

        if (port) {
            CFastMutexGuard guard(q->us_lock);
            //EIO_Status status = 
                q->udp_socket.Send(msg, msg_len, 
                                   CSocketAPI::ntoa(host), port);
        }
        // periodically check if we have no more jobs left
        if ((i % 10 == 0) && !q->status_tracker.AnyPending())
            break;
    }
}


void CQueue::CheckExecutionTimeout()
{
    CRef<SLockedQueue> q(x_GetLQueue());
    if (q->run_time_line == 0) {
        return;
    }
    CJobTimeLine& tl = *q->run_time_line;
    time_t curr = time(0);
    TNSBitVector bv;
    {{
        CReadLockGuard guard(q->rtl_lock);
        tl.ExtractObjects(curr, &bv);
    }}
    TNSBitVector::enumerator en(bv.first());
    for ( ;en.valid(); ++en) {
        unsigned job_id = *en;
        unsigned exp_time = CheckExecutionTimeout(job_id, curr);

        // job may need to be moved in the timeline to some future slot
        
        if (exp_time) {
            CWriteLockGuard guard(q->rtl_lock);
            tl.AddObject(curr, job_id);
        }
    } // for
}

time_t CQueue::CheckExecutionTimeout(unsigned job_id, time_t curr_time)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    unsigned queue_run_timeout = CQueueParamAccessor(*q).GetRunTimeout();
    SQueueDB& db = q->db;

    CNetScheduleAPI::EJobStatus status = GetStatus(job_id);

    if (status != CNetScheduleAPI::eRunning) {
        return 0;
    }

    CBDB_Transaction trans(*db.GetEnv(), 
                        CBDB_Transaction::eEnvDefault,
                        CBDB_Transaction::eNoAssociation);

    // TODO: get current job status from the status index
    unsigned time_run, run_timeout;
    time_t   exp_time;
    {{
        CFastMutexGuard guard(q->lock);
        db.SetTransaction(&trans);

        CBDB_FileCursor& cur = *q->GetCursor(trans);
        CBDB_CursorGuard cg(cur);

        cur.SetCondition(CBDB_FileCursor::eEQ);
        cur.From << job_id;
        if (cur.Fetch() != eBDB_Ok) {
            return 0;
        }
        int status = db.status;
        if (status != (int) CNetScheduleAPI::eRunning) {
            return 0;
        }

        time_run = db.time_run;
        _ASSERT(time_run);
        run_timeout = db.run_timeout;
        if (run_timeout == 0) run_timeout = queue_run_timeout;

        exp_time = x_ComputeExpirationTime(time_run, run_timeout);
        if (curr_time < exp_time) { 
            return exp_time;
        }
        // NB! here DB and matrix status deviate. Matrix gets short-lived
        // Returned status for some reason
        db.status = (int) CNetScheduleAPI::ePending;
        db.time_done = 0;

        cur.Update();
    }}

    trans.Commit();

    // See NB above.
    q->status_tracker.SetStatus(job_id, CNetScheduleAPI::eReturned);

    {{
        CTime tm(CTime::eCurrent);
        string msg = tm.AsString();
        msg += " CQueue::CheckExecutionTimeout: Job rescheduled id=";
        msg += NStr::IntToString(job_id);
        tm.SetTimeT(time_run);
        tm.ToLocalTime();
        msg += " time_run=";
        msg += tm.AsString();

        tm.SetTimeT(exp_time);
        tm.ToLocalTime();
        msg += " exp_time=";
        msg += tm.AsString();
        msg += " run_timeout(sec)=";
        msg += NStr::IntToString(run_timeout);
        msg += " run_timeout(minutes)=";
        msg += NStr::IntToString(run_timeout/60);
        ERR_POST(msg);

        if (IsMonitoring()) {
            MonitorPost(msg);
        }
    }}

    return 0;
}

time_t 
CQueue::x_ComputeExpirationTime(unsigned time_run, unsigned run_timeout) const
{
    if (run_timeout == 0) return 0;
    return time_run + run_timeout;
}

void CQueue::x_AddToAffIdx_NoLock(unsigned aff_id, 
                                  unsigned job_id_from,
                                  unsigned job_id_to)

{
    CRef<SLockedQueue> q(x_GetLQueue());
    SQueueAffinityIdx::TParent::TBitVector bv(bm::BM_GAP);

    // check if vector is in the database

    // read vector from the file
    q->aff_idx.aff_id = aff_id;
    /*EBDB_ErrCode ret = */
    q->aff_idx.ReadVector(&bv, bm::set_OR, NULL);
    if (job_id_to == 0) {
        bv.set(job_id_from);
    } else {
        bv.set_range(job_id_from, job_id_to);
    }
    q->aff_idx.aff_id = aff_id;
    q->aff_idx.WriteVector(bv, SQueueAffinityIdx::eNoCompact);
}

void CQueue::x_AddToAffIdx_NoLock(const vector<SNS_SubmitRecord>& batch)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    typedef SQueueAffinityIdx::TParent::TBitVector TBVector;
    typedef map<unsigned, TBVector*>               TBVMap;
    
    TBVMap  bv_map;
    try {
        unsigned bsize = batch.size();
        for (unsigned i = 0; i < bsize; ++i) {
            const SNS_SubmitRecord& bsub = batch[i];
            unsigned aff_id = bsub.affinity_id;
            unsigned job_id_start = bsub.job_id;

            TBVector* aff_bv;

            TBVMap::iterator aff_it = bv_map.find(aff_id);
            if (aff_it == bv_map.end()) { // new element
                auto_ptr<TBVector> bv(new TBVector(bm::BM_GAP));
                q->aff_idx.aff_id = aff_id;
                /*EBDB_ErrCode ret = */
                q->aff_idx.ReadVector(bv.get(), bm::set_OR, NULL);
                aff_bv = bv.get();
                bv_map[aff_id] = bv.release();
            } else {
                aff_bv = aff_it->second;
            }


            // look ahead for the same affinity id
            unsigned j;
            for (j=i+1; j < bsize; ++j) {
                if (batch[j].affinity_id != aff_id) {
                    break;
                }
                _ASSERT(batch[j].job_id == (batch[j-1].job_id+1));
                //job_id_end = batch[j].job_id;
            }
            --j;

            if ((i!=j) && (aff_id == batch[j].affinity_id)) {
                unsigned job_id_end = batch[j].job_id;
                aff_bv->set_range(job_id_start, job_id_end);
                i = j;
            } else { // look ahead failed
                aff_bv->set(job_id_start);
            }

        } // for

        // save all changes to the database
        NON_CONST_ITERATE(TBVMap, it, bv_map) {
            unsigned aff_id = it->first;
            TBVector* bv = it->second;
            bv->optimize();

            q->aff_idx.aff_id = aff_id;
            q->aff_idx.WriteVector(*bv, SQueueAffinityIdx::eNoCompact);

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
CQueue::x_ReadAffIdx_NoLock(const TNSBitVector& aff_id_set,
                            TNSBitVector*       job_candidates)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    q->aff_idx.SetTransaction(NULL);
    TNSBitVector::enumerator en(aff_id_set.first());
    for (; en.valid(); ++en) {  // for each affinity id
        unsigned aff_id = *en;
        x_ReadAffIdx_NoLock(aff_id, job_candidates);
    }
}


void 
CQueue::x_ReadAffIdx_NoLock(unsigned      aff_id,
                            TNSBitVector* job_candidates)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    q->aff_idx.aff_id = aff_id;
    q->aff_idx.SetTransaction(NULL);
    q->aff_idx.ReadVector(job_candidates, bm::set_OR, NULL);
}

CRef<SLockedQueue> CQueue::x_GetLQueue(void)
{
    CRef<SLockedQueue> ref(m_LQueue.Lock());
    if (ref != NULL) {
        return ref;
    } else {
        NCBI_THROW(CNetScheduleException, eUnknownQueue, "Job queue deleted");
    }
}

const CRef<SLockedQueue> CQueue::x_GetLQueue(void) const
{
    const CRef<SLockedQueue> ref(m_LQueue.Lock());
    if (ref != NULL) {
        return ref;
    } else {
        NCBI_THROW(CNetScheduleException, eUnknownQueue, "Job queue deleted");
    }
}



END_NCBI_SCOPE
