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

#include <corelib/ncbi_system.hpp> // SleepMilliSec
#include <corelib/ncbireg.hpp>
#include <corelib/request_ctx.hpp>

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
#include "ns_util.hpp"

BEGIN_NCBI_SCOPE

const unsigned k_max_dead_locks = 100;  // max. dead lock repeats


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


CRef<SLockedQueue> CQueueCollection::GetQueue(const string& name) const
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


// FIXME: remove this arcane construct, SLockedQueue now has all
// access methods required by ITERATE(CQueueCollection, it, m_QueueCollection)
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
// SNSDBEnvironmentParams implemenetation

bool SNSDBEnvironmentParams::Read(const IRegistry& reg, const string& sname)
{
    CConfig conf(reg);
    const CConfig::TParamTree* param_tree = conf.GetTree();
    const TPluginManagerParamTree* bdb_tree =
        param_tree->FindSubNode(sname);

    if (!bdb_tree)
        return false;

    CConfig bdb_conf((CConfig::TParamTree*)bdb_tree, eNoOwnership);
    db_path = bdb_conf.GetString("netschedule", "path", CConfig::eErr_Throw);
    db_log_path = ""; // doesn't work yet
        // bdb_conf.GetString("netschedule", "transaction_log_path",
        // CConfig::eErr_NoThrow, "");
#define GetUIntNoErr(name, dflt) \
    (unsigned) bdb_conf.GetInt("netschedule", name, CConfig::eErr_NoThrow, dflt)
#define GetBoolNoErr(name, dflt) \
    bdb_conf.GetBool("netschedule", name, CConfig::eErr_NoThrow, dflt)

    max_queues        = GetUIntNoErr("max_queues", 50);

    cache_ram_size    = (unsigned)
        bdb_conf.GetDataSize("netschedule", "mem_size",
                                CConfig::eErr_NoThrow, 0);
    mutex_max         = GetUIntNoErr("mutex_max", 0);
    max_locks         = GetUIntNoErr("max_locks", 0);
    max_lockers       = GetUIntNoErr("max_lockers", 0);
    max_lockobjects   = GetUIntNoErr("max_lockobjects", 0);
    log_mem_size      = GetUIntNoErr("log_mem_size", 0);
    // max_trans is derivative, so we do not read it here

    checkpoint_kb     = GetUIntNoErr("checkpoint_kb", 5000);
    checkpoint_min    = GetUIntNoErr("checkpoint_min", 5);

    sync_transactions = GetBoolNoErr("sync_transactions", false);
    direct_db         = GetBoolNoErr("direct_db", false);
    direct_log        = GetBoolNoErr("direct_log", false);
    private_env       = GetBoolNoErr("private_env", false);
    return true;
}


unsigned SNSDBEnvironmentParams::GetNumParams() const
{
    return 15; // do not count max_trans
}


string SNSDBEnvironmentParams::GetParamName(unsigned n) const
{
    switch (n) {
    case 0:  return "path";
    case 1:  return "transaction_log_path";
    case 2:  return "max_queues";
    case 3:  return "mem_size";
    case 4:  return "mutex_max";
    case 5:  return "max_locks";
    case 6:  return "max_lockers";
    case 7:  return "max_lockobjects";
    case 8:  return "log_mem_size";
    case 9:  return "checkpoint_kb";
    case 10: return "checkpoint_min";
    case 11: return "sync_transactions";
    case 12: return "direct_db";
    case 13: return "direct_log";
    case 14: return "private_env";
    default: return "";
    }
}


string SNSDBEnvironmentParams::GetParamValue(unsigned n) const
{
    switch (n) {
    case 0:  return db_path;
    case 1:  return db_log_path;
    case 2:  return NStr::UIntToString(max_queues);
    case 3:  return NStr::UIntToString(cache_ram_size);
    case 4:  return NStr::UIntToString(mutex_max);
    case 5:  return NStr::UIntToString(max_locks);
    case 6:  return NStr::UIntToString(max_lockers);
    case 7:  return NStr::UIntToString(max_lockobjects);
    case 8:  return NStr::UIntToString(log_mem_size);
    case 9:  return NStr::UIntToString(checkpoint_kb);
    case 10: return NStr::UIntToString(checkpoint_min);
    case 11: return NStr::BoolToString(sync_transactions);
    case 12: return NStr::BoolToString(direct_db);
    case 13: return NStr::BoolToString(direct_log);
    case 14: return NStr::BoolToString(private_env);
    default: return "";
    }
}

/////////////////////////////////////////////////////////////////////////////
// CQueueDataBase implementation

CQueueDataBase::CQueueDataBase(CBackgroundHost& host)
: m_Host(host),
  m_Env(0),
  m_QueueCollection(*this),
  m_StopPurge(false),
  m_FreeStatusMemCnt(0),
  m_LastFreeMem(time(0)),
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


void CQueueDataBase::Open(const SNSDBEnvironmentParams& params,
                          bool reinit)
{
    const string& db_path     = params.db_path;
    const string& db_log_path = params.db_log_path;
    if (reinit) {
        CDir dir(db_path);
        dir.Remove();
        LOG_POST(Error << "Reinitialization. " << db_path
                        << " removed.");
    }

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

    // Allocate SQueueDbBlock's here, open/create corresponding databases
    m_QueueDbBlockArray.Init(*m_Env, m_Path, params.max_queues);
}


int CQueueDataBase::x_AllocateQueue(const string& qname, const string& qclass,
                                    int kind, const string& comment)
{
    int pos = m_QueueDbBlockArray.Allocate();
    if (pos < 0) return pos;
    m_QueueDescriptionDB.queue   = qname;
    m_QueueDescriptionDB.kind    = kind;
    m_QueueDescriptionDB.pos     = pos;
    m_QueueDescriptionDB.qclass  = qclass;
    m_QueueDescriptionDB.comment = comment;
    m_QueueDescriptionDB.UpdateInsert();
    return pos;
}


unsigned CQueueDataBase::Configure(const IRegistry& reg)
{
    unsigned min_run_timeout = 3600;
    bool no_default_queues =
        reg.GetBool("server", "no_default_queues", false, 0, IRegistry::eReturn);

    CFastMutexGuard guard(m_ConfigureLock);

    x_CleanParamMap();

    // Merge queue data from config file into queue description database
    CNS_Transaction trans(*m_Env);
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
                                 "qclass_" : "queue_") << qclass
                             << " section. Ignored.");
            continue;
        }

        SQueueParameters* params = new SQueueParameters;
        params->Read(reg, sname);
        m_QueueParamMap[qclass] = params;
        min_run_timeout =
            std::min(min_run_timeout, (unsigned)params->run_timeout_precision);
        // Compatibility with previous convention - create a queue for every
        // class, declared as queue_*
        if (!no_default_queues && NStr::CompareNocase(tmp, "queue") == 0) {
            int pos = x_AllocateQueue(qclass, qclass,
                                      SLockedQueue::eKindStatic, "");
            if (pos < 0) {
                LOG_POST(Warning << "Queue " << qclass
                         << " can not be allocated: max_queues limit");
                break;
            }
        }
    }

    list<string> queues;
    reg.EnumerateEntries("queues", &queues);
    ITERATE(list<string>, it, queues) {
        const string& qname = *it;
        string qclass = reg.GetString("queues", qname, "");
        if (!qclass.empty()) {
            int pos = x_AllocateQueue(qname, qclass,
                                      SLockedQueue::eKindStatic, "");
            if (pos < 0) {
                LOG_POST(Warning << "Queue " << qname
                         << " can not be allocated: max_queues limit");
                break;
            }
        }
    }
    trans.Commit();
    m_QueueDescriptionDB.Sync();

    CBDB_FileCursor cur(m_QueueDescriptionDB);
    cur.SetCondition(CBDB_FileCursor::eFirst);

    while (cur.Fetch() == eBDB_Ok) {
        string qname(m_QueueDescriptionDB.queue);
        int kind = m_QueueDescriptionDB.kind;
        unsigned pos = m_QueueDescriptionDB.pos;
        string qclass(m_QueueDescriptionDB.qclass);
        TQueueParamMap::iterator it = m_QueueParamMap.find(qclass);
        if (it == m_QueueParamMap.end()) {
            LOG_POST(Error << "Can not find class " << qclass << " for queue " << qname);
            // Mark queue as dynamic, so we can delete it
            m_QueueDescriptionDB.kind = SLockedQueue::eKindDynamic;
            cur.Update();
            continue;
        }
        const SQueueParameters& params = *(it->second);
        bool qexists = m_QueueCollection.QueueExists(qname);
        if (!qexists) {
            MountQueue(qname, qclass, kind, params, m_QueueDbBlockArray.Get(pos));
        } else {
            UpdateQueueParameters(qname, params);
        }

        // Logging queue parameters
        const char* action = qexists ? "Reconfiguring" : "Mounting";
        string sparams;
        {{
            CRef<SLockedQueue> queue(m_QueueCollection.GetQueue(qname));
            CQueueParamAccessor qp(*queue);
            unsigned nParams = qp.GetNumParams();
            for (unsigned n = 0; n < nParams; ++n) {
                if (n > 0) sparams += ';';
                sparams += qp.GetParamName(n);
                sparams += '=';
                sparams += qp.GetParamValue(n);
            }
        }}
        LOG_POST(Info << action << " queue '" << qname << "' " << sparams);
    }
    return min_run_timeout;
}


CQueue* CQueueDataBase::OpenQueue(const string& name, unsigned peer_addr)
{
    CRef<SLockedQueue> queue(m_QueueCollection.GetQueue(name));
    return new CQueue(*this, queue, peer_addr);
}


void CQueueDataBase::MountQueue(const string& qname,
                                const string& qclass,
                                TQueueKind    kind,
                                const SQueueParameters& params,
                                SQueueDbBlock* queue_db_block)
{
    _ASSERT(m_Env);

    auto_ptr<SLockedQueue> q(new SLockedQueue(qname, qclass, kind));
    q->Attach(queue_db_block);
    q->SetParameters(params);

    SLockedQueue& queue = m_QueueCollection.AddQueue(qname, q.release());

    unsigned recs = queue.LoadStatusMatrix();

    queue.SetPort(GetUdpPort());

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
    // Find vacant position in queue block for new queue
    CNS_Transaction trans(*m_Env);
    m_QueueDescriptionDB.SetTransaction(&trans);
    int pos = x_AllocateQueue(qname, qclass,
                              SLockedQueue::eKindDynamic, comment);

    if (pos < 0) NCBI_THROW(CNetScheduleException, eUnknownQueue,
        "Cannot allocate queue: max_queues limit");
    trans.Commit();
    m_QueueDescriptionDB.Sync();
    const SQueueParameters& params = *(it->second);
    MountQueue(qname, qclass,
        SLockedQueue::eKindDynamic, params, m_QueueDbBlockArray.Get(pos));
}


void CQueueDataBase::DeleteQueue(const string& qname)
{
    CFastMutexGuard guard(m_ConfigureLock);
    CNS_Transaction trans(*m_Env);
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
    CRef<SLockedQueue> queue(m_QueueCollection.GetQueue(qname));
    queue->delete_database = true;
    // Remove it from collection
    if (!m_QueueCollection.RemoveQueue(qname)) {
        string msg = "Job queue not found: ";
        msg += qname;
        NCBI_THROW(CNetScheduleException, eUnknownQueue, msg);
    }
    m_QueueDbBlockArray.Free(queue->GetPos());
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


string CQueueDataBase::GetQueueNames(const string& sep) const
{
    string names;
    ITERATE(CQueueCollection, it, m_QueueCollection) {
        names += it.GetName(); names += sep;
    }
    return names;
}

void CQueueDataBase::UpdateQueueParameters(const string& qname,
                                           const SQueueParameters& params)
{
    CRef<SLockedQueue> queue(m_QueueCollection.GetQueue(qname));
    queue->SetParameters(params);
}

void CQueueDataBase::Close()
{
    // Check that we're still open
    if (!m_Env) return;

    StopNotifThread();
    StopPurgeThread();
    StopExecutionWatcherThread();

    m_Env->ForceTransactionCheckpoint();
    m_Env->CleanLog();

    x_CleanParamMap();

    m_QueueCollection.Close();

    // Close pre-allocated databases
    m_QueueDbBlockArray.Close();

    m_QueueDescriptionDB.Close();
    try {
        if (m_Env->CheckRemove()) {
            LOG_POST(Info    <<
                        "JS: '" <<
                        m_Name  << "' Unmounted. BDB ENV deleted.");
        } else {
            LOG_POST(Warning << "JS: '" << m_Name 
                                << "' environment still in use.");
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
    unsigned global_del_rec = 0;

    // Delete obsolete job records, based on time stamps
    // and expiration timeouts
    unsigned n_queue_limit = 10000; // TODO: determine this based on load

    CNetScheduleAPI::EJobStatus statuses_to_delete_from[] = {
        CNetScheduleAPI::eFailed, CNetScheduleAPI::eCanceled,
        CNetScheduleAPI::eDone, CNetScheduleAPI::ePending,
        CNetScheduleAPI::eReadFailed, CNetScheduleAPI::eConfirmed
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
                unsigned del_rec = (*it).CheckJobsExpiry(batch_size, s);
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
            if (deleted_by_status[n] > 0
                &&  s == CNetScheduleAPI::ePending) { // ? other states
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
            (*it).OptimizeMem();
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
        new CJobQueueCleanerThread(m_Host, *this, 1));
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
        m_PurgeThread.Reset(0);
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
        m_NotifThread.Reset(0);
        LOG_POST(Info << "Stopped.");
    }
}

void CQueueDataBase::RunExecutionWatcherThread(unsigned run_delay)
{
    LOG_POST(Info << "Starting execution watcher thread...");
    m_ExeWatchThread.Reset(
        new CJobQueueExecutionWatcherThread(m_Host, *this, run_delay));
    m_ExeWatchThread->Run();
    LOG_POST(Info << "Started.");
}

void CQueueDataBase::StopExecutionWatcherThread(void)
{
    if (!m_ExeWatchThread.Empty()) {
        LOG_POST(Info << "Stopping execution watch thread...");
        m_ExeWatchThread->RequestStop();
        m_ExeWatchThread->Join();
        m_ExeWatchThread.Reset(0);
        LOG_POST(Info << "Stopped.");
    }
}


/////////////////////////////////////////////////////////////////////////////
// CQueue implementation

CQueue::CQueue(CQueueDataBase&    db,
               CRef<SLockedQueue> queue,
               unsigned           client_host_addr)
: m_Db(db),
  m_LQueue(queue),
  m_ClientHostAddr(client_host_addr)
{
}


unsigned CQueue::CountRecs() const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    return q->CountRecs();
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


unsigned CQueue::GetNumParams() const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    return CQueueParamAccessor(*q).GetNumParams();
}


string CQueue::GetParamName(unsigned n) const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    return CQueueParamAccessor(*q).GetParamName(n);
}


string CQueue::GetParamValue(unsigned n) const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    return CQueueParamAccessor(*q).GetParamValue(n);
}


bool CQueue::IsExpired(void)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    return q->IsExpired();
}


#define NS_PRINT_TIME(msg, t) \
    do \
    { unsigned tt = t; \
      CTime _t(tt); _t.ToLocalTime(); \
      out << msg << (tt ? _t.AsString() : kEmptyStr) << fsp; \
    } while(0)

#define NS_PFNAME(x_fname) \
    (fflag ? x_fname : "")

void CQueue::x_PrintJobStat(const CJob&   job,
                            unsigned      queue_run_timeout,
                            CNcbiOstream& out,
                            const char*   fsp,
                            bool          fflag)
{
    out << fsp << NS_PFNAME("id: ") << job.GetId() << fsp;
    TJobStatus status = job.GetStatus();
    out << NS_PFNAME("status: ") << CNetScheduleAPI::StatusToString(status)
        << fsp;

    const CJobRun* last_run = job.GetLastRun();

    NS_PRINT_TIME(NS_PFNAME("time_submit: "), job.GetTimeSubmit());
    if (last_run) {
        NS_PRINT_TIME(NS_PFNAME("time_start: "), last_run->GetTimeStart());
        NS_PRINT_TIME(NS_PFNAME("time_done: "), last_run->GetTimeDone());
    }

    out << NS_PFNAME("timeout: ") << job.GetTimeout() << fsp;
    unsigned run_timeout = job.GetRunTimeout();
    out << NS_PFNAME("run_timeout: ") << run_timeout << fsp;

    if (last_run) {
        if (run_timeout == 0) run_timeout = queue_run_timeout;
        time_t exp_time =
            x_ComputeExpirationTime(last_run->GetTimeStart(), run_timeout);
        NS_PRINT_TIME(NS_PFNAME("time_run_expire: "), exp_time);
    }

    unsigned subm_addr = job.GetSubmAddr();
    out << NS_PFNAME("subm_addr: ")
        << (subm_addr ? CSocketAPI::gethostbyaddr(subm_addr) : kEmptyStr) << fsp;
    out << NS_PFNAME("subm_port: ") << job.GetSubmPort() << fsp;
    out << NS_PFNAME("subm_timeout: ") << job.GetSubmTimeout() << fsp;

    int node_num = 1;
    ITERATE(vector<CJobRun>, it, job.GetRuns()) {
        unsigned addr = it->GetClientIp();
        string node_name = "worker_node" + NStr::IntToString(node_num++) + ": ";
        out << NS_PFNAME(node_name)
            << (addr ? CSocketAPI::gethostbyaddr(addr) : kEmptyStr) << fsp;
    }

    out << NS_PFNAME("run_counter: ") << job.GetRunCount() << fsp;
    if (last_run)
        out << NS_PFNAME("ret_code: ") << last_run->GetRetCode() << fsp;

    unsigned aff_id = job.GetAffinityId();
    if (aff_id) {
        out << NS_PFNAME("aff_token: ") << "'" << job.GetAffinityToken()
            << "'" << fsp;
    }
    out << NS_PFNAME("aff_id: ") << aff_id << fsp;
    out << NS_PFNAME("mask: ") << job.GetMask() << fsp;

    out << NS_PFNAME("input: ")  << "'" << job.GetInput()  << "'" << fsp;
    out << NS_PFNAME("output: ") << "'" << job.GetOutput() << "'" << fsp;
    //out << NS_PFNAME("tags: ") << "'" << job.GetTags() << "'" << fsp;
    if (last_run) {
        out << NS_PFNAME("err_msg: ")
            << "'" << last_run->GetErrorMsg() << "'" << fsp;
    }
    out << NS_PFNAME("progress_msg: ")
        << "'" << job.GetProgressMsg() << "'" << fsp;
    out << "\n";
}


void CQueue::x_PrintShortJobStat(const CJob&   job,
                                 const string& host,
                                 unsigned      port,
                                 CNcbiOstream& out,
                                 const char*   fsp)
{
    out << string(CNetScheduleKey(job.GetId(), host, port)) << fsp;
    TJobStatus status = job.GetStatus();
    out << CNetScheduleAPI::StatusToString(status) << fsp;

    out << "'" << job.GetInput()    << "'" << fsp;
    out << "'" << job.GetOutput()   << "'" << fsp;
    const CJobRun* last_run = job.GetLastRun();
    if (last_run)
        out << "'" << last_run->GetErrorMsg() << "'" << fsp;
    else
        out << "''" << fsp;

    out << "\n";
}


void
CQueue::PrintJobDbStat(unsigned      job_id,
                       CNcbiOstream& out,
                       TJobStatus    status)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    unsigned queue_run_timeout = CQueueParamAccessor(*q).GetRunTimeout();

    CJob job;
    if (status == CNetScheduleAPI::eJobNotFound) {
        CQueueGuard guard(q);
        CJob::EJobFetchResult res = job.Fetch(q, job_id);

        if (res == CJob::eJF_Ok) {
            job.FetchAffinityToken(q);
            x_PrintJobStat(job, queue_run_timeout, out);
        } else {
            out << "Job not found id=" << q->DecorateJobId(job_id);
        }
        out << "\n";
    } else {
        TNSBitVector bv;
        q->status_tracker.StatusSnapshot(status, &bv);

        TNSBitVector::enumerator en(bv.first());
        for (;en.valid(); ++en) {
            CQueueGuard guard(q);
            CJob::EJobFetchResult res = job.Fetch(q, *en);
            if (res == CJob::eJF_Ok) {
                job.FetchAffinityToken(q);
                x_PrintJobStat(job, queue_run_timeout, out);
            }
        }
        out << "\n";
    }
}


// TODO: Move to SLockedQueue
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


void CQueue::PrintMutexStat(CNcbiOstream& out)
{
    m_Db.PrintMutexStat(out);
}


void CQueue::PrintLockStat(CNcbiOstream& out)
{
    m_Db.PrintLockStat(out);
}


void CQueue::PrintMemStat(CNcbiOstream& out)
{
    m_Db.PrintMemStat(out);
}


void CQueue::PrintAllJobDbStat(CNcbiOstream& out)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    unsigned queue_run_timeout = CQueueParamAccessor(*q).GetRunTimeout();

    CJob job;
    CQueueGuard guard(q);

    CQueueEnumCursor cur(q);
    while (cur.Fetch() == eBDB_Ok) {
        CJob::EJobFetchResult res = job.Fetch(q);
        if (res == CJob::eJF_Ok) {
            job.FetchAffinityToken(q);
            x_PrintJobStat(job, queue_run_timeout, out);
        }
        if (!out.good()) break;
    }
}


void CQueue::PrintStat(CNcbiOstream& out)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    q->PrintStat(out);
}


void CQueue::PrintQueue(CNcbiOstream& out,
                        TJobStatus    job_status,
                        const string& host,
                        unsigned      port)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    TNSBitVector bv;
    q->status_tracker.StatusSnapshot(job_status, &bv);

    CJob job;
    TNSBitVector::enumerator en(bv.first());
    for (;en.valid(); ++en) {
        CQueueGuard guard(q);

        CJob::EJobFetchResult res = job.Fetch(q, *en);
        if (res == CJob::eJF_Ok)
            x_PrintShortJobStat(job, host, port, out);
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


static string FormatHostName(unsigned host, SQueueDescription* qdesc)
{
    if (!host) return "0.0.0.0";
    string host_name;
    map<unsigned, string>::iterator it =
        qdesc->host_name_cache.find(host);
    if (it == qdesc->host_name_cache.end()) {
        host_name = CSocketAPI::gethostbyaddr(host);
        qdesc->host_name_cache[host] = host_name;
    } else {
        host_name = it->second;
    }
    return host_name;
}


static string FormatHostName(const string& val, SQueueDescription* qdesc)
{
    return FormatHostName(NStr::StringToUInt(val), qdesc);
}


void CQueue::PrepareFields(SFieldsDescription& field_descr,
                           const list<string>& fields)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    field_descr.Init();

    // Verify the fields, and convert them into field numbers
    ITERATE(list<string>, it, fields) {
        const string& field_name = NStr::TruncateSpaces(*it);
        string tag_name;
        SFieldsDescription::EFieldType ftype;
        int i;
        if ((i = CJob::GetFieldIndex(field_name)) >= 0) {
            ftype = SFieldsDescription::eFT_Job;
            if (field_name == "affinity")
                field_descr.has_affinity = true;
        } else if ((i = CJobRun::GetFieldIndex(field_name)) >= 0) {
            ftype = SFieldsDescription::eFT_Run;
            // field_descr.has_run = true;
        } else if (field_name == "run") {
            ftype = SFieldsDescription::eFT_RunNum;
            field_descr.has_run = true;
        } else if (NStr::StartsWith(field_name, "tag.")) {
            ftype = SFieldsDescription::eFT_Tag;
            tag_name = field_name.substr(4);
            field_descr.has_tags = true;
        } else {
            NCBI_THROW(CNetScheduleException, eQuerySyntaxError,
                string("Unknown field: ") + (*it));
        }
        SFieldsDescription::FFormatter formatter = NULL;
        if (i >= 0) {
            if (field_name == "id") {
                formatter = FormatNSId;
            } else if (field_name == "client_ip" ||
                       field_name == "subm_addr") {
                formatter = FormatHostName;
            }
        }
        field_descr.field_types.push_back(ftype);
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

    int record_size = field_descr.field_nums.size();
    // Retrieve fields
    CJob job;
    TNSBitVector::enumerator en(ids.first());
    {{
        CQueueGuard guard(q);
        for ( ; en.valid(); ++en) {
            map<string, string> tags;
            unsigned job_id = *en;

            // FIXME: fetch minimal required part of the job based on
            // field_descr.has_* flags, may be add has_input_output flag
            CJob::EJobFetchResult res = job.Fetch(q, job_id);
            if (res != CJob::eJF_Ok) {
                if (res != CJob::eJF_NotFound)
                    ERR_POST(Error << "Error reading queue job db");
                continue;
            }
            if (field_descr.has_affinity)
                job.FetchAffinityToken(q);
            if (field_descr.has_tags) {
                // Parse tags record
                ITERATE(TNSTagList, it, job.GetTags()) {
                    tags[it->first] = it->second;
                }
            }
            // Fill out the record
            if (!field_descr.has_run) {
                vector<string> record(record_size);
                if (x_FillRecord(record, field_descr, job, tags, job.GetLastRun(), 0))
                    record_set.push_back(record);
            } else {
                int run_num = 1;
                ITERATE(vector<CJobRun>, it, job.GetRuns()) {
                    vector<string> record(record_size);
                    if (x_FillRecord(record, field_descr, job, tags, &(*it), run_num++))
                        record_set.push_back(record);
                }
            }
        }
    }}
}


bool CQueue::x_FillRecord(vector<string>& record,
                          const SFieldsDescription& field_descr,
                          const CJob& job,
                          map<string, string>& tags,
                          const CJobRun* run,
                          int run_num)
{
    int record_size = field_descr.field_nums.size();

    bool complete = true;
    for (int i = 0; i < record_size; ++i) {
        int fnum = field_descr.field_nums[i];
        switch(field_descr.field_types[i]) {
        case SFieldsDescription::eFT_Job:
            record[i] = job.GetField(fnum);
            break;
        case SFieldsDescription::eFT_Run:
            if (run)
                record[i] = run->GetField(fnum);
            else
                record[i] = "NULL";
            break;
        case SFieldsDescription::eFT_Tag:
            {
                map<string, string>::iterator it =
                    tags.find((field_descr.pos_to_tag)[i]);
                if (it == tags.end()) {
                    complete = false;
                } else {
                    record[i] = it->second;
                }
            }
            break;
        case SFieldsDescription::eFT_RunNum:
            record[i] = NStr::IntToString(run_num);
            break;
        }
        if (!complete) break;
    }
    return complete;
}


void CQueue::PrintWorkerNodeStat(CNcbiOstream& out) const
{
    CRef<SLockedQueue> q(x_GetLQueue());

    q->PrintWorkerNodeStat(out);
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


static CDiagContext_Extra& s_PrintTags(CDiagContext_Extra& extra,
                                       const TNSTagList& tags)
{
    ITERATE(TNSTagList, it, tags) {
        extra.Print(string("tag_")+it->first, it->second);
    }
    return extra;
}


static void s_LogSubmit(SLockedQueue& q, 
                        CJob& job,
                        SQueueDescription& qdesc)
{
    s_PrintTags(GetDiagContext().Extra().SetType("submit")
        .Print("queue", q.GetQueueName())
        .Print("job_id", NStr::UIntToString(job.GetId()))
        .Print("input", job.GetInput())
        .Print("aff", job.GetAffinityToken())
        .Print("mask", NStr::UIntToString(job.GetMask()))
        .Print("progress_msg", job.GetProgressMsg())
        .Print("subm_addr", FormatHostName(job.GetSubmAddr(), &qdesc))
        .Print("subm_port", NStr::UIntToString(job.GetSubmPort()))
        .Print("subm_timeout", NStr::UIntToString(job.GetSubmTimeout())),
        job.GetTags());
}


unsigned CQueue::Submit(CJob& job)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    unsigned max_input_size;
    bool log_job_state;
    {{
        CQueueParamAccessor qp(*q);
        log_job_state = qp.GetLogJobState();
        max_input_size = qp.GetMaxInputSize();
    }}
    if (job.GetInput().size() > max_input_size) 
        NCBI_THROW(CNetScheduleException, eDataTooLong,
           "Input is too long");

    bool was_empty = !q->status_tracker.AnyPending();

    unsigned job_id = q->GetNextId();
    job.SetId(job_id);
    job.SetTimeSubmit(time(0));

    // Special treatment for system job masks
    unsigned mask = job.GetMask();
    if (mask & CNetScheduleAPI::eOutOfOrder)
    {
        // TODO: put job id into OutOfOrder list
    }

    CNS_Transaction trans(q);
    job.CheckAffinityToken(q, &trans);
    unsigned affinity_id = job.GetAffinityId();
    {{
        CQueueGuard guard(q, &trans);

        job.Flush(q);
        // TODO: move event count to Flush
        q->CountEvent(SLockedQueue::eStatDBWriteEvent, 1);
//            need_aux_insert ? 2 : 1);

        // update affinity index
        if (affinity_id)
            q->AddJobsToAffinity(trans, affinity_id, job_id);

        // update tags
        CNSTagMap tag_map;
        q->AppendTags(tag_map, job.GetTags(), job_id);
        q->FlushTags(tag_map, trans);
    }}

    trans.Commit();
    q->status_tracker.SetStatus(job_id, CNetScheduleAPI::ePending);
    if (log_job_state) {
        CRequestContext rq;
        string client_ip;
        // NS_FormatIPAddress(m_PeerAddr, client_ip);
        // rq.SetClientIP(client_ip);
        rq.SetRequestID(CRequestContext::GetNextRequestID());

        CDiagContext::SetRequestContext(&rq);

        SQueueDescription qdesc;
        s_LogSubmit(*q, job, qdesc);
        CDiagContext::SetRequestContext(0);
    }
    if (was_empty) NotifyListeners(true, affinity_id);

    return job_id;
}


unsigned CQueue::SubmitBatch(vector<CJob>& batch)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    unsigned max_input_size = CQueueParamAccessor(*q).GetMaxInputSize();

    bool was_empty = !q->status_tracker.AnyPending();

    unsigned job_id = q->GetNextIdBatch(batch.size());

    unsigned batch_aff_id = 0; // if batch comes with the same affinity
    bool     batch_has_aff = false;

    // process affinity ids
    {{
        CNS_Transaction trans(q);
        for (unsigned i = 0; i < batch.size(); ++i) {
            CJob& job = batch[i];
            if (job.GetAffinityId() == (unsigned)kMax_I4) { // take prev. token
                _ASSERT(i > 0);
                unsigned prev_aff_id = 0;
                if (i > 0) {
                    prev_aff_id = batch[i-1].GetAffinityId();
                    if (!prev_aff_id) {
                        prev_aff_id = 0;
                        LOG_POST(Warning << "Reference to empty previous "
                                            "affinity token");
                    }
                } else {
                    LOG_POST(Warning << "First job in batch cannot have "
                                        "reference to previous affinity token");
                }
                job.SetAffinityId(prev_aff_id);
            } else {
                if (job.HasAffinityToken()) {
                    job.CheckAffinityToken(q, &trans);
                    batch_has_aff = true;
                    batch_aff_id = (i == 0 )? job.GetAffinityId() : 0;
                } else {
                    batch_aff_id = 0;
                }

            }
        }
        trans.Commit();
    }}

    CNS_Transaction trans(q);
    {{
        CNSTagMap tag_map;
        CQueueGuard guard(q, &trans);

        unsigned job_id_cnt = job_id;
        time_t now = time(0);
        int aux_inserts = 0;
        NON_CONST_ITERATE(vector<CJob>, it, batch) {
            if (it->GetInput().size() > max_input_size) 
                NCBI_THROW(CNetScheduleException, eDataTooLong,
                "Input is too long");
            unsigned cur_job_id = job_id_cnt++;
            it->SetId(cur_job_id);
            it->SetTimeSubmit(now);
            it->Flush(q);

            q->AppendTags(tag_map, it->GetTags(), cur_job_id);
        }
        q->CountEvent(SLockedQueue::eStatDBWriteEvent,
            batch.size() + aux_inserts);

        // Store the affinity index
        if (batch_has_aff) {
            if (batch_aff_id) {  // whole batch comes with the same affinity
                q->AddJobsToAffinity(trans,
                                     batch_aff_id,
                                     job_id,
                                     job_id + batch.size() - 1);
            } else {
                q->AddJobsToAffinity(trans, batch);
            }
        }
        q->FlushTags(tag_map, trans);
    }}
    trans.Commit();
    q->status_tracker.AddPendingBatch(job_id, job_id + batch.size() - 1);

    // This case is a bit complicated. If whole batch has the same
    // affinity, we include it in notification broadcast.
    // If not, or it has no affinity at all - 0.
    if (was_empty) NotifyListeners(true, batch_aff_id);

    return job_id;
}


unsigned
CQueue::CountStatus(TJobStatus st) const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    return q->status_tracker.CountStatus(st);
}


void
CQueue::StatusStatistics(TJobStatus status,
                         TNSBitVector::statistics* st) const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    q->status_tracker.StatusStatistics(status, st);
}


void CQueue::ForceReschedule(unsigned job_id)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    CJob job;
    CNS_Transaction trans(q);
    {{
        CQueueGuard guard(q, &trans);
        CJob::EJobFetchResult res = job.Fetch(q, job_id);

        if (res == CJob::eJF_Ok) {
            job.SetStatus(CNetScheduleAPI::ePending);
            job.SetRunCount(0);
        } else {
            // TODO: Integrity error or job just expired?
            return;
        }
    }}
    trans.Commit();

    q->status_tracker.SetStatus(job_id, CNetScheduleAPI::ePending);
}


void CQueue::Cancel(unsigned job_id)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    CQueueJSGuard js_guard(q, job_id,
                           CNetScheduleAPI::eCanceled);
    TJobStatus st = js_guard.GetOldStatus();
    if (st != CNetScheduleAPI::ePending &&
        st != CNetScheduleAPI::eRunning) {
        return;
    }
    CJob job;
    CNS_Transaction trans(q);
    {{
        CQueueGuard guard(q, &trans);
        CJob::EJobFetchResult res = job.Fetch(q, job_id);
        if (res != CJob::eJF_Ok) {
            // TODO: Integrity error or job just expired?
            return;
        }

        TJobStatus status = job.GetStatus();
        if (status != st) {
            ERR_POST(Error
                << "Status mismatch for job " << q->DecorateJobId(job_id)
                << " matrix " << st
                << " db " << status);
            // TODO: server error exception?
            return;
        }
        CJobRun* run = job.GetLastRun();
        if (!run) run = &job.AppendRun();
        run->SetStatus(CNetScheduleAPI::eCanceled);
        run->SetTimeDone(time(0));
        run->SetErrorMsg(string("Job canceled from ")
                         + CNetScheduleAPI::StatusToString(status)
                         + " state");
        job.SetStatus(CNetScheduleAPI::eCanceled);
        job.Flush(q);
    }}
    trans.Commit();
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


void CQueue::ReadJobs(unsigned peer_addr,
                      unsigned count, unsigned timeout,
                      unsigned& read_id, TNSBitVector& jobs)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    q->ReadJobs(peer_addr, count, timeout, read_id, jobs);
}


void CQueue::ConfirmJobs(unsigned read_id, TNSBitVector& jobs)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    q->ConfirmJobs(read_id, jobs);
}


void CQueue::FailReadingJobs(unsigned read_id, TNSBitVector& jobs)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    q->FailReadingJobs(read_id, jobs);
}


void CQueue::InitWorkerNode(const SWorkerNodeInfo& node_info)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    q->InitWorkerNode(node_info);
}


void CQueue::ClearWorkerNode(const string& node_id)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    q->ClearWorkerNode(node_id);
}


void CQueue::PutResult(SWorkerNodeInfo& node_info,
                       unsigned         job_id,
                       int              ret_code,
                       const string*    output)
{
    PutResultGetJob(node_info,
                    job_id, ret_code, output,
                    0, 0, 0);
}


void CQueue::GetJob(SWorkerNodeInfo&    node_info,
                    CRequestContextFactory* rec_ctx_f,
                    const list<string>* aff_list,
                    CJob*               job)
{
    PutResultGetJob(node_info,
                    0, 0, 0,
                    rec_ctx_f, aff_list, job);
}


bool
CQueue::x_UpdateDB_PutResultNoLock(unsigned             job_id,
                                   time_t               curr,
                                   bool                 delete_done,
                                   int                  ret_code,
                                   const string&        output,
                                   CJob&                job)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    CJob::EJobFetchResult res = job.Fetch(q, job_id);
    if (res != CJob::eJF_Ok) {
        // TODO: Integrity error or job just expired?
        return false;
    }

    CJobRun* run = job.GetLastRun();
    if (!run) {
        ERR_POST(Error << "No JobRun for running job "
                       << q->DecorateJobId(job_id));
        NCBI_THROW(CNetScheduleException, eInvalidJobStatus, "Job never ran");
    }

    if (delete_done) {
        job.Delete();
    } else {
        run->SetStatus(CNetScheduleAPI::eDone);
        run->SetTimeDone(curr);
        run->SetRetCode(ret_code);
        job.SetStatus(CNetScheduleAPI::eDone);
        job.SetOutput(output);

        job.Flush(q);
    }
    return true;
}


void
CQueue::PutResultGetJob(SWorkerNodeInfo& node_info,
                        // PutResult parameters
                        unsigned         done_job_id,
                        int              ret_code,
                        const string*    output,
                        // GetJob parameters
                        // in
                        CRequestContextFactory* rec_ctx_f,
                        const list<string>* aff_list,
                        // out
                        CJob*            new_job)
{
    // PutResult parameter check
    _ASSERT(!done_job_id || output);

    CRef<SLockedQueue> q(x_GetLQueue());
    bool delete_done;
    unsigned max_output_size;
    unsigned run_timeout;
    {{
        CQueueParamAccessor qp(*q);
        delete_done = qp.GetDeleteDone();
        max_output_size = qp.GetMaxOutputSize();
        run_timeout = qp.GetRunTimeout();
    }}

    if (done_job_id && output->size() > max_output_size) {
        NCBI_THROW(CNetScheduleException, eDataTooLong,
           "Output is too long");
    }

    unsigned dead_locks = 0; // dead lock counter

    time_t curr = time(0);

    //
    bool need_update = false;
    CQueueJSGuard js_guard(q, done_job_id,
                           CNetScheduleAPI::eDone,
                           &need_update);
    // This is a HACK - if js_guard is not committed, it will rollback
    // to previous state, so it is safe to change status after the guard.
    if (delete_done) {
        q->Erase(done_job_id);
    }
    // TODO: implement transaction wrapper (a la js_guard above)
    // for FindPendingJob
    // TODO: move affinity assignment there as well
    unsigned pending_job_id = 0;
    if (new_job)
        pending_job_id = q->FindPendingJob(node_info.node_id, curr);
    bool done_rec_updated = false;
    CJob job;

    // When working with the same database file concurrently there is
    // chance of internal Berkeley DB deadlock. (They say it's legal!)
    // In this case Berkeley DB returns an error code(DB_LOCK_DEADLOCK)
    // and recovery is up to the application.
    // If it happens I repeat the transaction several times.
    //
    for (;;) {
        try {
            CNS_Transaction trans(q);

            EGetJobUpdateStatus upd_status = eGetJobUpdate_Ok;
            {{
                CQueueGuard guard(q, &trans);

                if (need_update) {
                    done_rec_updated = x_UpdateDB_PutResultNoLock(
                        done_job_id, curr, delete_done,
                        ret_code, *output, job);
                }

                if (pending_job_id) {
                    upd_status =
                        x_UpdateDB_GetJobNoLock(node_info,
                                                curr, pending_job_id,
                                                *new_job);
                }
            }}

            trans.Commit();
            js_guard.Commit();
            // TODO: commit FindPendingJob guard here
            switch (upd_status) {
            case eGetJobUpdate_JobFailed:
                q->status_tracker.ChangeStatus(pending_job_id,
                                            CNetScheduleAPI::eFailed);
                /* FALLTHROUGH */
            case eGetJobUpdate_JobStopped:
            case eGetJobUpdate_NotFound:
                pending_job_id = 0;
                break;
            case eGetJobUpdate_Ok:
                break;
            default:
                _ASSERT(0);
            }

            if (done_job_id) q->RemoveJobFromWorkerNode(node_info, done_job_id,
                                                        eNSCDone, ret_code);
            if (pending_job_id) q->AddJobToWorkerNode(node_info, rec_ctx_f,
                                                      pending_job_id,
                                                      curr + run_timeout);
            break;
        }
        catch (CBDB_ErrnoException& ex) {
            if (ex.IsDeadLock()) {
                if (++dead_locks < k_max_dead_locks) {
                    if (IsMonitoring()) {
                        MonitorPost(
                            "DeadLock repeat in CQueue::JobExchange");
                    }
                    SleepMilliSec(250);
                    continue;
                } 
            } else if (ex.IsNoMem()) {
                if (++dead_locks < k_max_dead_locks) {
                    if (IsMonitoring()) {
                        MonitorPost(
                            "No resource repeat in CQueue::JobExchange");
                    }
                    SleepMilliSec(250);
                    continue;
                }
            }
            ERR_POST("Too many transaction repeats in CQueue::JobExchange.");
            throw;
        }
    }

    unsigned job_aff_id;
    if (new_job && (job_aff_id = new_job->GetAffinityId())) {
        time_t exp_time = run_timeout ? curr + 2*run_timeout : 0;
        q->AddAffinity(node_info.node_id, job_aff_id, exp_time);
    }

    x_TimeLineExchange(done_job_id, pending_job_id, curr);

    if (done_rec_updated  &&  job.ShouldNotify(curr)) {
        q->Notify(job.GetSubmAddr(), job.GetSubmPort(), done_job_id);
    }

    if (IsMonitoring()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();

        msg += " CQueue::PutResultGetJob()";
        if (done_job_id) {
            msg += " (PUT) job id=";
            msg += NStr::IntToString(done_job_id);
            msg += " ret_code=";
            msg += NStr::IntToString(ret_code);
            msg += " output=\"";
            msg += *output + '"';
        }
        if (pending_job_id) {
            msg += " (GET) job id=";
            msg += NStr::IntToString(pending_job_id);
            msg += " worker_node=";
            msg += node_info.node_id;
        }
        MonitorPost(msg);
    }
}


bool CQueue::PutProgressMessage(unsigned      job_id,
                                const string& msg)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    CJob job;
    CNS_Transaction trans(q);
    {{
        CQueueGuard guard(q, &trans);

        CJob::EJobFetchResult res = job.Fetch(q, job_id);
        if (res != CJob::eJF_Ok) return false;
        job.SetProgressMsg(msg);
        job.Flush(q);
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
    return true;
}


void CQueue::FailJob(const SWorkerNodeInfo& node_info,
                     unsigned               job_id,
                     const string&          err_msg,
                     const string&          output,
                     int                    ret_code)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    q->RemoveJobFromWorkerNode(node_info, job_id, eNSCFailed, ret_code);
    q->FailJob(job_id, err_msg, output, ret_code, &node_info.node_id);
}


void CQueue::JobDelayExpiration(SWorkerNodeInfo& node_info,
                                unsigned job_id,
                                unsigned tm)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    unsigned queue_run_timeout = CQueueParamAccessor(*q).GetRunTimeout();

    if (tm == 0) return;

    time_t run_timeout = 0;
    time_t time_start = 0;

    TJobStatus st = GetStatus(job_id);
    if (st != CNetScheduleAPI::eRunning)
        return;

    CJob job;
    CNS_Transaction trans(q);

    unsigned exp_time = 0;
    time_t curr = time(0);

    bool job_updated = false;
    {{
        CQueueGuard guard(q, &trans);

        CJob::EJobFetchResult res;
        if ((res = job.Fetch(q, job_id)) != CJob::eJF_Ok)
            return;

        CJobRun* run = job.GetLastRun();
        if (!run) {
            ERR_POST(Error << "No JobRun for running job " 
                           << q->DecorateJobId(job_id));
            // Fix it
            run = &job.AppendRun();
    		job_updated = true;
        }

        time_start = run->GetTimeStart();
        if (time_start == 0) {
            // Impossible
            ERR_POST(Error
                << "Internal error: time_start == 0 for running job id="
                << q->DecorateJobId(job_id));
            // Fix it just in case
            time_start = curr;
            run->SetTimeStart(curr);
    		job_updated = true;
        }
        run_timeout = job.GetRunTimeout();
        if (run_timeout == 0) run_timeout = queue_run_timeout;

        if (time_start + run_timeout > curr + tm) {
            // Old timeout is enough to cover this request, keep it.
            // If we already changed job object (fixing it), we flush it.
    		if (job_updated) job.Flush(q);
            return;
        }

        job.SetRunTimeout(curr + tm - time_start);
        job.Flush(q);
    }}

    trans.Commit();
    q->UpdateWorkerNodeJob(node_info, job_id, curr + tm);

    exp_time = x_ComputeExpirationTime(time_start, run_timeout);

    {{
        CWriteLockGuard guard(q->rtl_lock);
        q->run_time_line->MoveObject(exp_time, curr + tm, job_id);
    }}
    q->RegisterWorkerNodeVisit(node_info);

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
    return;
}


void CQueue::ReturnJob(const SWorkerNodeInfo& node_info, unsigned job_id)
{
    // FIXME: move the body to SLockedQueue, provide fallback to
    // RegisterWorkerNodeVisit if unsuccessful
    if (!job_id) return;

    CRef<SLockedQueue> q(x_GetLQueue());

    CQueueJSGuard js_guard(q, job_id,
                           CNetScheduleAPI::ePending);
    TJobStatus st = js_guard.GetOldStatus();

    if (st != CNetScheduleAPI::eRunning) {
        return;
    }
    CJob job;
    CNS_Transaction trans(q);
    {{
        CQueueGuard guard(q, &trans);

        CJob::EJobFetchResult res = job.Fetch(q, job_id);
        if (res != CJob::eJF_Ok)
            return;

        job.SetStatus(CNetScheduleAPI::ePending);
        unsigned run_count = job.GetRunCount();
        CJobRun* run = job.GetLastRun();
        if (!run) {
            ERR_POST(Error << "No JobRun for running job "
                           << q->DecorateJobId(job_id));
            run = &job.AppendRun();
        }
        // This is the only legitimate place where Returned status appears
        // as a signal that the job was actually returned
        run->SetStatus(CNetScheduleAPI::eReturned);
        run->SetTimeDone(time(0));

        if (run_count) {
            job.SetRunCount(run_count-1);
        }
        job.Flush(q);
    }}
    trans.Commit();
    js_guard.Commit();
    q->RemoveJobFromWorkerNode(node_info, job_id, eNSCReturned);
    x_RemoveFromTimeLine(job_id);

    if (IsMonitoring()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::ReturnJob: job id=";
        msg += NStr::IntToString(job_id);
        MonitorPost(msg);
    }
}


CQueue::EGetJobUpdateStatus CQueue::x_UpdateDB_GetJobNoLock(
                            const SWorkerNodeInfo& node_info,
                            time_t            curr,
                            unsigned          job_id,
                            CJob&             job)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    unsigned queue_timeout = CQueueParamAccessor(*q).GetTimeout();

    const unsigned kMaxGetAttempts = 100;


    for (unsigned fetch_attempts = 0; fetch_attempts < kMaxGetAttempts;
         ++fetch_attempts)
    {
        CJob::EJobFetchResult res;
        if ((res = job.Fetch(q, job_id)) != CJob::eJF_Ok) {
            if (res == CJob::eJF_NotFound) return eGetJobUpdate_NotFound;
            // FIXME: does it make sense to retry right after DB error?
            continue;
        }
        TJobStatus status = job.GetStatus();

        // integrity check
        if (status != CNetScheduleAPI::ePending) {
            if (CJobStatusTracker::IsCancelCode(status)) {
                // this job has been canceled while i'm fetching it
                return eGetJobUpdate_JobStopped;
            }
            ERR_POST(Error
                << "x_UpdateDB_GetJobNoLock: Status integrity violation "
                << " job = "     << q->DecorateJobId(job_id)
                << " status = "  << status
                << " expected status = "
                << (int)CNetScheduleAPI::ePending);
            return eGetJobUpdate_JobStopped;
        }

        unsigned time_submit = job.GetTimeSubmit();
        unsigned timeout = job.GetTimeout();
        if (timeout == 0) timeout = queue_timeout;

        _ASSERT(timeout);
        // check if job already expired
        if (timeout && (time_submit + timeout < (unsigned) curr)) {
            // Job expired, fail it
            CJobRun& run = job.AppendRun();
            run.SetStatus(CNetScheduleAPI::eFailed);
            run.SetTimeDone(curr);
            run.SetErrorMsg("Job expired and cannot be scheduled.");
            job.SetStatus(CNetScheduleAPI::eFailed);
            q->status_tracker.ChangeStatus(job_id, CNetScheduleAPI::eFailed);
            job.Flush(q);

            if (IsMonitoring()) {
                CTime tmp_t(CTime::eCurrent);
                string msg = tmp_t.AsString();
                msg +=
                 " CQueue::x_UpdateDB_GetJobNoLock() timeout expired job id=";
                msg += NStr::IntToString(job_id);
                MonitorPost(msg);
            }
            return eGetJobUpdate_JobFailed;
        }

        // job not expired
        job.FetchAffinityToken(q);

        unsigned run_count = job.GetRunCount() + 1;
        CJobRun& run = job.AppendRun();
        run.SetStatus(CNetScheduleAPI::eRunning);
        run.SetTimeStart(curr);

        // We're setting host:port here using node_info. It is faster
        // than looking up this info by node_id in worker node list and
        // should provide exactly same info.
        run.SetWorkerNodeId(node_info.node_id);
        run.SetClientIp(node_info.host);
        run.SetClientPort(node_info.port);

        job.SetStatus(CNetScheduleAPI::eRunning);
        job.SetRunTimeout(0);
        job.SetRunCount(run_count);

        job.Flush(q);
        return eGetJobUpdate_Ok;
    }

    return eGetJobUpdate_NotFound;
}


bool
CQueue::GetJobDescr(unsigned   job_id,
                    int*       ret_code,
                    string*    input,
                    string*    output,
                    string*    err_msg,
                    string*    progress_msg,
                    TJobStatus expected_status)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    for (unsigned i = 0; i < 3; ++i) {
        if (i) {
            // TODO: move all DB retry code out to SLockedQueue level
            // failed to read the record (maybe looks like writer is late,
            // so we need to retry a bit later)
            if (IsMonitoring()) {
                MonitorPost(string("GetJobDescr sleep for job_id ") +
                    NStr::IntToString(job_id));
            }
            SleepMilliSec(300);
        }

        CJob job;
        CJob::EJobFetchResult res;
        {{
            CQueueGuard guard(q);
            res = job.Fetch(q, job_id);
        }}
        if (res == CJob::eJF_Ok) {
            if (expected_status != CNetScheduleAPI::eJobNotFound) {
                TJobStatus status = job.GetStatus();
                if (status != expected_status) {
                    // Retry after sleep
                    continue;
                }
            }
            const CJobRun *last_run = job.GetLastRun();
            if (ret_code && last_run)
                *ret_code = last_run->GetRetCode();
            if (input)
                *input = job.GetInput();
            if (output)
                *output = job.GetOutput();
            if (err_msg && last_run)
                last_run->GetErrorMsg();
            if (progress_msg)
                *progress_msg = job.GetProgressMsg();

            return true;
        } // ?TODO: else if (res == CJob::eJF_DBErr)
    }

    return false; // job not found
}


TJobStatus CQueue::GetStatus(unsigned job_id) const
{
    const CRef<SLockedQueue> q(x_GetLQueue());
    return q->GetJobStatus(job_id);
}


bool CQueue::CountStatus(CJobStatusTracker::TStatusSummaryMap* status_map,
                         const char*                           affinity_token)
{
    _ASSERT(status_map);
    CRef<SLockedQueue> q(x_GetLQueue());
    return q->CountStatus(status_map, affinity_token);
}


unsigned 
CQueue::CheckJobsExpiry(unsigned batch_size, TJobStatus status)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    return q->CheckJobsExpiry(batch_size, status);
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
    // Next call updates 'm_BecameEmpty' timestamp
    IsExpired(); // locks SLockedQueue lock
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
    if (!q->run_time_line) return;

    CJobTimeLine& tl = *q->run_time_line;
    CWriteLockGuard guard(q->rtl_lock);
    if (remove_job_id)
        tl.RemoveObject(remove_job_id);
    if (add_job_id)
        tl.AddObject(curr + queue_run_timeout, add_job_id);

    if (IsMonitoring()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::TimeLineExchange:";
        if (remove_job_id) {
            msg += " job removed=";
            msg += NStr::IntToString(remove_job_id);
        }
        if (add_job_id) {
            msg += " job added=";
            msg += NStr::IntToString(add_job_id);
        }
        MonitorPost(msg);
    }
}


void CQueue::RegisterNotificationListener(const SWorkerNodeInfo& node_info,
                                          unsigned short         port,
                                          unsigned               timeout)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    q->RegisterNotificationListener(node_info, port, timeout);
}


void
CQueue::UnRegisterNotificationListener(const SWorkerNodeInfo& node_info)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    if (!q->UnRegisterNotificationListener(node_info.node_id)) {
        SWorkerNodeInfo ni(node_info);
        q->RegisterWorkerNodeVisit(ni);
    }
}


void
CQueue::RegisterWorkerNodeVisit(SWorkerNodeInfo& node_info)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    q->RegisterWorkerNodeVisit(node_info);
}



void CQueue::ClearAffinity(const string& node_id)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    q->ClearAffinity(node_id);
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


void CQueue::NotifyListeners(bool unconditional, unsigned aff_id)
{
    if (m_Db.GetUdpPort() == 0)
        return;

    CRef<SLockedQueue> q(x_GetLQueue());
    q->NotifyListeners(unconditional, aff_id);
}


void CQueue::CheckExecutionTimeout()
{
    CRef<SLockedQueue> q(x_GetLQueue());
    q->CheckExecutionTimeout();
}


time_t 
CQueue::x_ComputeExpirationTime(unsigned time_run, unsigned run_timeout) const
{
    if (run_timeout == 0) return 0;
    return time_run + run_timeout;
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
