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
 * File Description: NetSchedule queue collection and database managenement.
 *
 */
#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp> // SleepMilliSec
#include <corelib/ncbireg.hpp>

#include <connect/services/netschedule_api.hpp>
#include <connect/ncbi_socket.hpp>

#include <db.h>
#include <db/bdb/bdb_trans.hpp>
#include <db/bdb/bdb_cursor.hpp>
#include <db/bdb/bdb_util.hpp>

#include <util/time_line.hpp>

#include "queue_database.hpp"
#include "ns_util.hpp"
#include "netschedule_version.hpp"
#include "ns_server.hpp"


BEGIN_NCBI_SCOPE


#define GetUIntNoErr(name, dflt) \
    (unsigned) bdb_conf.GetInt("netschedule", name, CConfig::eErr_NoThrow, dflt)
#define GetSizeNoErr(name, dflt) \
    (unsigned) bdb_conf.GetDataSize("netschedule", name, CConfig::eErr_NoThrow, dflt)
#define GetBoolNoErr(name, dflt) \
    bdb_conf.GetBool("netschedule", name, CConfig::eErr_NoThrow, dflt)


/////////////////////////////////////////////////////////////////////////////
// SNSDBEnvironmentParams implementation

bool SNSDBEnvironmentParams::Read(const IRegistry& reg, const string& sname)
{
    CConfig                         conf(reg);
    const CConfig::TParamTree*      param_tree = conf.GetTree();
    const TPluginManagerParamTree*  bdb_tree = param_tree->FindSubNode(sname);

    if (!bdb_tree)
        return false;

    CConfig bdb_conf((CConfig::TParamTree*)bdb_tree, eNoOwnership);
    db_storage_ver = bdb_conf.GetString("netschedule", "force_storage_version",
        CConfig::eErr_NoThrow, NETSCHEDULED_STORAGE_VERSION);

    db_path = bdb_conf.GetString("netschedule", "path", CConfig::eErr_Throw);
    db_log_path = ""; // doesn't work yet
        // bdb_conf.GetString("netschedule", "transaction_log_path",
        // CConfig::eErr_NoThrow, "");

    max_queues        = GetUIntNoErr("max_queues", 50);

    cache_ram_size    = GetSizeNoErr("mem_size", 0);
    mutex_max         = GetUIntNoErr("mutex_max", 0);
    max_locks         = GetUIntNoErr("max_locks", 0);
    max_lockers       = GetUIntNoErr("max_lockers", 0);
    max_lockobjects   = GetUIntNoErr("max_lockobjects", 0);
    log_mem_size      = GetSizeNoErr("log_mem_size", 0);
    // max_trans is derivative, so we do not read it here

    checkpoint_kb     = GetUIntNoErr("checkpoint_kb", 5000);
    checkpoint_min    = GetUIntNoErr("checkpoint_min", 5);

    sync_transactions = GetBoolNoErr("sync_transactions", false);
    direct_db         = GetBoolNoErr("direct_db", false);
    direct_log        = GetBoolNoErr("direct_log", false);
    private_env       = GetBoolNoErr("private_env", false);
    return true;
}


static string  s_DBEnvParameters[] =
{
    "path",                     // 0
    "transaction_log_path",     // 1
    "max_queues",               // 2
    "mem_size",                 // 3
    "mutex_max",                // 4
    "max_locks",                // 5
    "max_lockers",              // 6
    "max_lockobjects",          // 7
    "log_mem_size",             // 8
    "checkpoint_kb",            // 9
    "checkpoint_min",           // 10
    "sync_transactions",        // 11
    "direct_db",                // 12
    "direct_log",               // 13
    "private_env"               // 14
};
static unsigned s_NumDBEnvParameters = sizeof(s_DBEnvParameters) /
                                       sizeof(string);


unsigned SNSDBEnvironmentParams::GetNumParams() const
{
    return s_NumDBEnvParameters;
}


string SNSDBEnvironmentParams::GetParamName(unsigned n) const
{
    if (n >= s_NumDBEnvParameters)
        return kEmptyStr;
    return s_DBEnvParameters[n];
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
    default: return kEmptyStr;
    }
}

/////////////////////////////////////////////////////////////////////////////
// CQueueDataBase implementation

CQueueDataBase::CQueueDataBase(CNetScheduleServer *  server)
: m_Host(server->GetBackgroundHost()),
  m_Executor(server->GetRequestExecutor()),
  m_Env(0),
  m_QueueCollection(*this),
  m_StopPurge(false),
  m_FreeStatusMemCnt(0),
  m_LastFreeMem(time(0)),
  m_Server(server),
  m_PurgeQueue(""),
  m_PurgeStatusIndex(0),
  m_PurgeJobScanned(0),
  m_PurgeLoopStartTime(0)
{}


CQueueDataBase::~CQueueDataBase()
{
    try {
        Close();
    } catch (exception& )
    {}
}


bool CQueueDataBase::Open(const SNSDBEnvironmentParams& params,
                          bool reinit)
{
    const string&   db_path     = params.db_path;
    const string&   db_log_path = params.db_log_path;
    if (reinit) {
        CDir dir(db_path);

        dir.Remove();
        LOG_POST(Message << Warning
                         << "Reinitialization. " << db_path << " removed.");
    }

    m_Path = CDirEntry::AddTrailingPathSeparator(db_path);
    string trailed_log_path = CDirEntry::AddTrailingPathSeparator(db_log_path);

    const string* effective_log_path = trailed_log_path.empty() ?
                                       &m_Path :
                                       &trailed_log_path;

    bool        fresh_db(false);
    {{
        CDir    dir(m_Path);
        if ( !dir.Exists() ) {
            fresh_db = true;
            dir.Create();
        }
    }}

    string      db_storage_ver;
    CFile       ver_file(CFile::MakePath(m_Path, "DB_STORAGE_VER"));
    if (fresh_db) {
        db_storage_ver = NETSCHEDULED_STORAGE_VERSION;
        CFileIO f;
        f.Open(ver_file.GetPath(), CFileIO_Base::eCreate,
                                   CFileIO_Base::eReadWrite);
        f.Write(db_storage_ver.data(), db_storage_ver.size());
        f.Close();
    } else {
        if (!ver_file.Exists()) {
            // Last storage version which does not have DB_STORAGE_VER file
            db_storage_ver = "4.0.0";
        } else {
            CFileIO f;
            char buf[32];
            f.Open(ver_file.GetPath(), CFileIO_Base::eOpen, CFileIO_Base::eRead);
            size_t n = f.Read(buf, sizeof(buf));
            db_storage_ver.append(buf, n);
            NStr::TruncateSpacesInPlace(db_storage_ver, NStr::eTrunc_End);
            f.Close();
        }
        if (db_storage_ver != params.db_storage_ver) {
            ERR_POST("Storage version mismatch, required: " <<
                     params.db_storage_ver <<
                     ", present: " << db_storage_ver);
            return false;
        }
    }

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

    m_Env->SetLogRegionMax(512 * 1024);
    if (params.log_mem_size) {
        m_Env->SetLogInMemory(true);
        m_Env->SetLogBSize(params.log_mem_size);
    } else {
        m_Env->SetLogFileMax(200 * 1024 * 1024);
        m_Env->SetLogAutoRemove(true);
    }

    // Check if bdb env. files are in place and try to join
    CDir            dir(*effective_log_path);
    CDir::TEntries  fl = dir.GetEntries("__db.*", CDir::eIgnoreRecursive);

    if (!params.private_env && !fl.empty()) {
        // Opening db with recover flags is unreliable.
        BDB_RecoverEnv(m_Path, false);
        LOG_POST(Message << Warning << "Running recovery...");
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
    LOG_POST(Message << Warning
                     << "Opened " << (params.private_env ? "private " : "")
                     << "BDB environment with "
                     << m_Env->GetMaxLocks() << " max locks, "
//                    << m_Env->GetMaxLockers() << " max lockers, "
//                    << m_Env->GetMaxLockObjects() << " max lock objects, "
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
    return true;
}


int CQueueDataBase::x_AllocateQueue(const string& qname, const string& qclass,
                                    int kind, const string& comment)
{
    int pos = m_QueueDbBlockArray.Allocate();
    if (pos < 0)
        return pos;

    m_QueueDescriptionDB.queue      = qname;
    m_QueueDescriptionDB.kind       = kind;
    m_QueueDescriptionDB.pos        = pos;
    m_QueueDescriptionDB.qclass     = qclass;
    m_QueueDescriptionDB.comment    = comment;
    m_QueueDescriptionDB.UpdateInsert();
    return pos;
}


struct SQueueInfo
{
    int    kind;
    int    pos;
    bool   remove; ///< if this queue has a conflict and should be removed?
    string qclass;
};

typedef map<string, SQueueInfo> TDbQueuesMap;

// Add queue to interim map. If the queue with the same name is already
// present, check for consistency.
// @return true if queue was not there, or duplicate is consistent
bool x_AddQueueToMap(TDbQueuesMap& db_queues_map,
                     const string& qname,
                     const string& qclass,
                     int           kind,
                     int           pos)
{
    bool                    res = true;
    TDbQueuesMap::iterator  it;

    if ((it = db_queues_map.find(qname)) != db_queues_map.end()) {
        // check that queue info matches
        SQueueInfo &    qi = it->second;
        if (qi.qclass != qclass) {
            LOG_POST(Error << "Class mismatch for queue '"
                           << qname << "', expected '"
                           << qi.qclass << "', registered '"
                           << qclass << "'.");
            res = false;
        }
        // Definite positions must match, if one of positions is indefinite,
        // i.e. < 0, they are incomparable so it is not an error.
        if (qi.pos != pos  &&  qi.pos >= 0  &&  pos >= 0) {
            LOG_POST(Error << "Position mismatch for queue '"
                           << qname << "', expected "
                           << qi.pos << ", registered "
                           << pos << ".");
            res = false;
        }
    } else {
        db_queues_map[qname] = SQueueInfo();
        db_queues_map[qname].kind = kind;
        // Mark queue for allocation
        db_queues_map[qname].pos = pos;
        db_queues_map[qname].remove = false;
        db_queues_map[qname].qclass = qclass;
    }
    return res;
}


unsigned CQueueDataBase::Configure(const IRegistry& reg)
{
    unsigned    min_run_timeout = 3600;
    bool        no_default_queues = reg.GetBool("server", "no_default_queues",
                                                false, 0, IRegistry::eReturn);

    CFastMutexGuard     guard(m_ConfigureLock);

    // Temporary storage for merging info from database and config file
    TDbQueuesMap        db_queues_map;

    // Read in registered queues from database into db_queues_map
    {{
        m_QueueDescriptionDB.SetTransaction(NULL);
        CBDB_FileCursor cur(m_QueueDescriptionDB);
        cur.SetCondition(CBDB_FileCursor::eFirst);

        while (cur.Fetch() == eBDB_Ok) {
            x_AddQueueToMap(db_queues_map,
                            m_QueueDescriptionDB.queue,
                            m_QueueDescriptionDB.qclass,
                            m_QueueDescriptionDB.kind,
                            m_QueueDescriptionDB.pos);
        }
    }}

    x_CleanParamMap();

    // Merge queue data from config file into db_queues_map, filling class
    // info (m_QueueParamMap) too.
    list<string>        sections;
    reg.EnumerateSections(&sections);

    ITERATE(list<string>, it, sections) {
        string              qclass, tmp;
        const string&       sname = *it;

        NStr::SplitInTwo(sname, "_", tmp, qclass);
        if (NStr::CompareNocase(tmp, "queue") != 0 &&
            NStr::CompareNocase(tmp, "qclass") != 0) {
            continue;
        }
        if (m_QueueParamMap.find(qclass) != m_QueueParamMap.end()) {
            LOG_POST(Error << tmp << " section " << sname
                           << " conflicts with previous "
                           << (NStr::CompareNocase(tmp, "queue") == 0 ?
                                 "qclass_" : "queue_") << qclass
                           << " section. Ignored.");
            continue;
        }

        // Register queue class
        SQueueParameters*   params = new SQueueParameters;
        params->Read(reg, sname);
        m_QueueParamMap[qclass] = params;
        min_run_timeout = std::min(min_run_timeout,
                                   (unsigned)params->run_timeout_precision);

        // Compatibility with previous convention - create a queue for every
        // class, declared as queue_*
        if (!no_default_queues && NStr::CompareNocase(tmp, "queue") == 0) {
            // The queue name for this case is the same as class name
            string&     qname = qclass;
            x_AddQueueToMap(db_queues_map, qname, qclass,
                            CQueue::eKindStatic, -1);
        }
    }

    list<string>        queues;
    reg.EnumerateEntries("queues", &queues);
    ITERATE(list<string>, it, queues) {
        const string&   qname = *it;
        string          qclass = reg.GetString("queues", qname, "");
        if (!qclass.empty()  &&
            m_QueueParamMap.find(qclass) != m_QueueParamMap.end())
        {
            x_AddQueueToMap(db_queues_map, qname, qclass,
                            CQueue::eKindStatic, -1);
        }
    }

    {{
        CBDB_Transaction    trans(*m_Env, CBDB_Transaction::eEnvDefault,
                                          CBDB_Transaction::eNoAssociation);
        m_QueueDescriptionDB.SetTransaction(&trans);
        // Allocate/deallocate queue db blocks according to merged info,
        // merge this info back into database.
        // NB! We don't actually remove conflicting queues from the database because
        // corrective actions in config file can be taken. We just mask queues and do
        // not attempt to load them.
        NON_CONST_ITERATE(TDbQueuesMap, it, db_queues_map) {
            const string& qname = it->first;
            SQueueInfo& qi = it->second;
            int pos = qi.pos;
            bool qexists = m_QueueCollection.QueueExists(qname);
            if (pos < 0) {
                pos = x_AllocateQueue(qname, qi.qclass, qi.kind, "");
                if (pos < 0) {
                    qi.remove = true;
                    LOG_POST(Error << "Queue '" << qname << "' can not "
                                      "be allocated: max_queues limit");
                }
            } else {
                if (!qexists  &&  !m_QueueDbBlockArray.Allocate(pos)) {
                    qi.remove = true;
                    LOG_POST(Error << "Queue '" << qname
                                   << "' position conflict at block #" << pos);
                }
            }
            qi.pos = pos;
        }
        trans.Commit();
        m_QueueDescriptionDB.Sync();
        m_QueueDescriptionDB.SetTransaction(NULL);
    }}

//    CBDB_FileCursor cur(m_QueueDescriptionDB);
//    cur.SetCondition(CBDB_FileCursor::eFirst);
//    while (cur.Fetch() == eBDB_Ok) {
//        string qname(m_QueueDescriptionDB.queue);
//        int kind = m_QueueDescriptionDB.kind;
//        unsigned pos = m_QueueDescriptionDB.pos;
//        string qclass(m_QueueDescriptionDB.qclass);
    ITERATE(TDbQueuesMap, it, db_queues_map) {
        const string&       qname = it->first;
        const SQueueInfo&   qi = it->second;

        if (qi.remove)
            continue;

        int                         kind = qi.kind;
        int                         pos = qi.pos;
        const string &              qclass = qi.qclass;
        TQueueParamMap::iterator    it1 = m_QueueParamMap.find(qclass);

        if (it1 == m_QueueParamMap.end()) {
            LOG_POST(Error << "Can not find class " << qclass << " for queue " << qname);
            // NB: Class (defined in config file) does not exist anymore for the already
            // loaded queue. I do not know how intelligently handle it, postpone it.
            // ??? Mark queue as dynamic, so we can delete it
//            m_QueueDescriptionDB.kind = CQueue::eKindDynamic;
//            cur.Update();
            continue;
        }

        const SQueueParameters&     params = *(it1->second);
        bool                        qexists = m_QueueCollection.QueueExists(qname);

        if (!qexists) {
            _ASSERT(m_QueueDbBlockArray.Get(pos) != NULL);
            MountQueue(qname, qclass, kind, params, m_QueueDbBlockArray.Get(pos));
        } else {
            UpdateQueueParameters(qname, params);
        }

        // Logging queue parameters
        const char*             action = qexists ? "Reconfiguring" : "Mounting";
        string                  sparams;
        CRef<CQueue>            queue(m_QueueCollection.GetQueue(qname));
        CQueue::TParameterList  parameters;

        parameters = queue->GetParameters();
        ITERATE(CQueue::TParameterList, it1, parameters) {
            if (!sparams.empty())
                sparams += ';';
            sparams += it1->first + '=' + it1->second;
        }
        LOG_POST(Message << Warning << action << " queue '" << qname
                         << "' of class '" << qclass
                         << "' " << sparams);
    }
    return min_run_timeout;
}


unsigned CQueueDataBase::CountActiveJobs() const
{
    unsigned            cnt = 0;
    CFastMutexGuard     guard(m_ConfigureLock);

    ITERATE(CQueueCollection, it, m_QueueCollection) {
        TNSBitVector bv;
        (*it).JobsWithStatus(CNetScheduleAPI::ePending, &bv);
        (*it).JobsWithStatus(CNetScheduleAPI::eRunning, &bv);
        cnt += bv.count();
    }
    return cnt;
}


CRef<CQueue> CQueueDataBase::OpenQueue(const string& name)
{
    return m_QueueCollection.GetQueue(name);
}


void CQueueDataBase::MountQueue(const string&               qname,
                                const string&               qclass,
                                TQueueKind                  kind,
                                const SQueueParameters&     params,
                                SQueueDbBlock*              queue_db_block)
{
    _ASSERT(m_Env);

    auto_ptr<CQueue>    q(new CQueue(m_Executor, qname, qclass, kind, m_Server));

    q->Attach(queue_db_block);
    q->SetParameters(params);

    CQueue&             queue = m_QueueCollection.AddQueue(qname, q.release());
    unsigned            recs = queue.LoadStatusMatrix();

    LOG_POST(Message << Warning << "Queue records = " << recs);
}


void CQueueDataBase::CreateQueue(const string&  qname,
                                 const string&  qclass,
                                 const string&  comment)
{
    CFastMutexGuard     guard(m_ConfigureLock);

    if (m_QueueCollection.QueueExists(qname))
        NCBI_THROW(CNetScheduleException, eDuplicateName,
                   "Queue \"" + qname + "\" already exists");


    TQueueParamMap::iterator    it = m_QueueParamMap.find(qclass);
    if (it == m_QueueParamMap.end())
        NCBI_THROW(CNetScheduleException, eUnknownQueueClass,
                   "Can not find class \"" + qclass +
                   "\" for queue \"" + qname + "\"");

    // Find vacant position in queue block for new queue
    CBDB_Transaction    trans(*m_Env, CBDB_Transaction::eEnvDefault,
                                      CBDB_Transaction::eNoAssociation);
    m_QueueDescriptionDB.SetTransaction(&trans);

    int     pos = x_AllocateQueue(qname, qclass,
                                  CQueue::eKindDynamic, comment);

    if (pos < 0)
        NCBI_THROW(CNetScheduleException, eUnknownQueue,
                   "Cannot allocate queue: max_queues limit");

    trans.Commit();
    m_QueueDescriptionDB.Sync();
    const SQueueParameters&     params = *(it->second);
    MountQueue(qname, qclass,
               CQueue::eKindDynamic, params, m_QueueDbBlockArray.Get(pos));
}


void CQueueDataBase::DeleteQueue(const string&      qname)
{
    CFastMutexGuard         guard(m_ConfigureLock);
    CBDB_Transaction        trans(*m_Env, CBDB_Transaction::eEnvDefault,
                                          CBDB_Transaction::eNoAssociation);

    m_QueueDescriptionDB.SetTransaction(&trans);
    m_QueueDescriptionDB.queue = qname;
    if (m_QueueDescriptionDB.Fetch() != eBDB_Ok)
        NCBI_THROW(CNetScheduleException, eUnknownQueue,
                   "Job queue not found: " + qname);

    int                     kind = m_QueueDescriptionDB.kind;
    if (!kind)
        NCBI_THROW(CNetScheduleException, eAccessDenied,
                   "Static queue \"" + qname + "\" can not be deleted");

    // Signal queue to wipe out database files.
    CRef<CQueue>            queue(m_QueueCollection.GetQueue(qname));
    queue->MarkForDeletion();
    // Remove it from collection
    if (!m_QueueCollection.RemoveQueue(qname))
        NCBI_THROW(CNetScheduleException, eUnknownQueue,
                   "Job queue not found: " + qname);

    // To call CQueueDbBlockArray::Free here was a deadlock error - we can't
    // reset transaction until it is executing. So we moved setting 'allocated'
    // flag to CQueue::Detach where everything is in single threaded mode,
    // and access to boolean seems to be atomic anyway.

    // Remove it from DB
    m_QueueDescriptionDB.Delete(CBDB_File::eIgnoreError);
    trans.Commit();
}


void CQueueDataBase::QueueInfo(const string& qname, int& kind,
                               string* qclass, string* comment)
{
    CFastMutexGuard     guard(m_ConfigureLock);

    m_QueueDescriptionDB.SetTransaction(NULL);
    m_QueueDescriptionDB.queue = qname;
    if (m_QueueDescriptionDB.Fetch() != eBDB_Ok)
        NCBI_THROW(CNetScheduleException, eUnknownQueue,
                   "Job queue not found: " + qname);

    kind = m_QueueDescriptionDB.kind;
    *qclass = m_QueueDescriptionDB.qclass;
    *comment = m_QueueDescriptionDB.comment;
}


string CQueueDataBase::GetQueueNames(const string& sep) const
{
    string      names;

    ITERATE(CQueueCollection, it, m_QueueCollection) {
        names += it.GetName(); names += sep;
    }
    return names;
}


void CQueueDataBase::UpdateQueueParameters(const string& qname,
                                           const SQueueParameters& params)
{
    CRef<CQueue> queue(m_QueueCollection.GetQueue(qname));
    queue->SetParameters(params);
}


void CQueueDataBase::Close()
{
    // Check that we're still open
    if (!m_Env)
        return;

    StopNotifThread();
    StopPurgeThread();
    StopStatisticsThread();
    StopExecutionWatcherThread();

    m_Env->ForceTransactionCheckpoint();
    m_Env->CleanLog();

    x_CleanParamMap();

    m_QueueCollection.Close();

    // Close pre-allocated databases
    m_QueueDbBlockArray.Close();

    m_QueueDescriptionDB.Close();
    try {
        if (m_Env->CheckRemove())
            LOG_POST(Message << Warning << "JS: '" << m_Name
                             << "' Unmounted. BDB ENV deleted.");
        else
            LOG_POST(Message << Warning << "JS: '" << m_Name
                             << "' environment still in use.");
    }
    catch (exception& ex) {
        LOG_POST(Error << "JS: '" << m_Name
                       << "' Exception in Close() " << ex.what()
                       << " (ignored.)");
    }

    delete m_Env; m_Env = 0;
}


void CQueueDataBase::TransactionCheckPoint(bool clean_log)
{
    m_Env->TransactionCheckpoint();
    if (clean_log)
        m_Env->CleanLog();
}


void CQueueDataBase::NotifyListeners(void)
{
    NON_CONST_ITERATE(CQueueCollection, it, m_QueueCollection) {
        (*it).NotifyListeners(false, 0);
    }
}


void CQueueDataBase::PrintStatistics(void)
{
    NON_CONST_ITERATE(CQueueCollection, it, m_QueueCollection) {
        (*it).PrintStatistics();
    }
}


void CQueueDataBase::CheckExecutionTimeout(bool  logging)
{
    NON_CONST_ITERATE(CQueueCollection, it, m_QueueCollection) {
        (*it).CheckExecutionTimeout(logging);
    }
}



/* Data used in CQueueDataBase::Purge() only */
static CNetScheduleAPI::EJobStatus statuses_to_delete_from[] = {
    CNetScheduleAPI::eFailed,
    CNetScheduleAPI::eCanceled,
    CNetScheduleAPI::eDone,
    CNetScheduleAPI::ePending,
    CNetScheduleAPI::eReadFailed,
    CNetScheduleAPI::eConfirmed
};
const size_t kStatusesSize = sizeof(statuses_to_delete_from) /
                             sizeof(CNetScheduleAPI::EJobStatus);

void CQueueDataBase::Purge(void)
{
    vector<string>      queues_to_delete;
    size_t              max_deleted = m_Server->GetDeleteBatchSize();
    size_t              max_scanned = m_Server->GetScanBatchSize();
    SPurgeAttributes    purge_io;
    size_t              total_scanned = 0;
    size_t              total_deleted = 0;
    time_t              current_time = time(0);

    // Part I: from the last end till the end of the list
    CQueueCollection::iterator      start_iterator = x_GetPurgeQueueIterator();
    size_t                          start_status_index = m_PurgeStatusIndex;

    if (start_iterator == m_QueueCollection.begin()) {
        if (m_PurgeLoopStartTime == current_time)
            return;
    }

    for (CQueueCollection::iterator  it = start_iterator;
         it != m_QueueCollection.end(); ++it) {
        if ((*it).IsExpired()) {
            queues_to_delete.push_back(it.GetName());
            continue;
        }

        m_PurgeQueue = it.GetName();
        if (x_PurgeQueue(*it, m_PurgeStatusIndex, kStatusesSize - 1,
                         max_scanned, max_deleted,
                         current_time,
                         total_scanned, total_deleted) == true)
            return;

        if (total_deleted >= max_deleted ||
            total_scanned >= max_scanned)
            break;
    }

    // This is the point of the queue list beginning. If we cross this point
    // within the same second twice or more we can safely skip scanning as it
    // would be too often.
    if (m_PurgeLoopStartTime != current_time) {
        m_PurgeLoopStartTime = current_time;


        // Part II: from the beginning of the list till the last end
        if (total_deleted < max_deleted && total_scanned < max_scanned) {
            for (CQueueCollection::iterator  it = m_QueueCollection.begin();
                 it != start_iterator; ++it) {
                if ((*it).IsExpired()) {
                    queues_to_delete.push_back(it.GetName());
                    continue;
                }

                m_PurgeQueue = it.GetName();
                if (x_PurgeQueue(*it, m_PurgeStatusIndex, kStatusesSize - 1,
                                 max_scanned, max_deleted,
                                 current_time,
                                 total_scanned, total_deleted) == true)
                    return;

                if (total_deleted >= max_deleted ||
                    total_scanned >= max_scanned)
                    break;
            }
        }

        // Part III: it might need to check the statuses in the queue we started
        // with if the start status was not the first one.
        if (total_deleted < max_deleted && total_scanned < max_scanned) {
            if (start_iterator != m_QueueCollection.end()) {
                m_PurgeQueue = start_iterator.GetName();
                if (start_status_index > 0) {
                    if (x_PurgeQueue(*start_iterator, 0, start_status_index - 1,
                                     max_scanned, max_deleted,
                                     current_time,
                                     total_scanned, total_deleted) == true)
                        return;
                }
            }
        }
    }

    // Part IV: purge the found candidates and optimize the memory if required
    m_FreeStatusMemCnt += x_PurgeUnconditional();
    TransactionCheckPoint();

    x_OptimizeStatusMatrix(current_time);

    ITERATE(vector<string>, q_it, queues_to_delete) {
        try {
            DeleteQueue(*q_it);
        }
        catch (const CNetScheduleException &  ex) {
            ERR_POST("Error deleting the queue '" << *q_it << "': " << ex);
        }
        catch (const CBDB_Exception &  ex) {
            ERR_POST("Error deleting the queue '" << *q_it << "': " << ex);
        }
        catch (...) {
            ERR_POST("Unknown error while deleting the queue '" << (*q_it)
                                                                << "'.");
        }
    }

    return;
}


CQueueCollection::iterator  CQueueDataBase::x_GetPurgeQueueIterator(void)
{
    if (m_PurgeQueue.empty())
        return m_QueueCollection.begin();

    for (CQueueCollection::iterator  it = m_QueueCollection.begin();
         it != m_QueueCollection.end(); ++it)
        if (it.GetName() == m_PurgeQueue)
            return it;

    // Not found, which means the queue was deleted. Pick a random one
    m_PurgeStatusIndex = 0;

    int     queue_num = ((rand() * 1.0) / RAND_MAX) * m_QueueCollection.GetSize();
    int     k = 1;
    for (CQueueCollection::iterator  it = m_QueueCollection.begin();
         it != m_QueueCollection.end(); ++it) {
        if (k >= queue_num)
            return it;
        ++k;
    }

    // Cannot happened, so just be consistent with the return value
    return m_QueueCollection.begin();
}


// Purges jobs from a queue starting from the given status.
// Returns true if the purge should be stopped.
// The status argument is a status to start from
bool  CQueueDataBase::x_PurgeQueue(CQueue &  queue,
                                   size_t    status,
                                   size_t    status_to_end,
                                   size_t    max_scanned,
                                   size_t    max_deleted,
                                   time_t    current_time,
                                   size_t &  total_scanned,
                                   size_t &  total_deleted)
{
    SPurgeAttributes    purge_io;

    for (; status <= status_to_end; ++status) {
        purge_io.scans = max_scanned - total_scanned;
        purge_io.deleted = max_deleted - total_deleted;
        purge_io.job_id = m_PurgeJobScanned;

        purge_io = queue.CheckJobsExpiry(current_time, purge_io,
                                         statuses_to_delete_from[status]);
        total_scanned += purge_io.scans;
        total_deleted += purge_io.deleted;
        m_PurgeJobScanned = purge_io.job_id;

        if (x_CheckStopPurge())
            return true;

        if (total_deleted >= max_deleted || total_scanned >= max_scanned) {
            m_PurgeStatusIndex = status;
            return false;
        }
    }
    m_PurgeStatusIndex = 0;
    return false;
}


unsigned CQueueDataBase::x_PurgeUnconditional(void) {
    // Purge unconditional jobs
    unsigned        del_rec = 0;

    NON_CONST_ITERATE(CQueueCollection, it, m_QueueCollection) {
        del_rec += (*it).DeleteBatch();
    }
    return del_rec;
}


void CQueueDataBase::x_OptimizeStatusMatrix(time_t  current_time)
{
    // optimize memory every 15 min. or after 1 million of deleted records
    static const int        kMemFree_Delay = 15 * 60;
    static const unsigned   kRecordThreshold = 1000000;

    if ((m_FreeStatusMemCnt > kRecordThreshold) ||
        (m_LastFreeMem + kMemFree_Delay <= current_time)) {
        m_FreeStatusMemCnt = 0;
        m_LastFreeMem = current_time;

        NON_CONST_ITERATE(CQueueCollection, it, m_QueueCollection) {
            (*it).OptimizeMem();
            if (x_CheckStopPurge())
                return;
        }
    }
}


void CQueueDataBase::x_CleanParamMap(void)
{
    NON_CONST_ITERATE(TQueueParamMap, it, m_QueueParamMap) {
        delete it->second;
    }
    m_QueueParamMap.clear();
}


void CQueueDataBase::StopPurge(void)
{
    // No need to guard, this operation is thread safe
    m_StopPurge = true;
}


bool CQueueDataBase::x_CheckStopPurge(void)
{
    CFastMutexGuard     guard(m_PurgeLock);
    bool                stop_purge = m_StopPurge;

    m_StopPurge = false;
    return stop_purge;
}


void CQueueDataBase::RunPurgeThread(void)
{
    double              purge_timeout = m_Server->GetPurgeTimeout();
    unsigned int        sec_delay = purge_timeout;
    unsigned int        nanosec_delay = (purge_timeout - sec_delay)*1000000000;

    m_PurgeThread.Reset(new CJobQueueCleanerThread(
                                m_Host, *this, sec_delay, nanosec_delay,
                                m_Server->IsLogCleaningThread()));
    m_PurgeThread->Run();
}


void CQueueDataBase::StopPurgeThread(void)
{
    if (!m_PurgeThread.Empty()) {
        StopPurge();
        m_PurgeThread->RequestStop();
        m_PurgeThread->Join();
        m_PurgeThread.Reset(0);
    }
}


void CQueueDataBase::RunNotifThread(void)
{
    // 10 times per second
    m_NotifThread.Reset(new CGetJobNotificationThread(
                                *this, 0, 100000000,
                                m_Server->IsLogNotificationThread()));
    m_NotifThread->Run();
}


void CQueueDataBase::StopNotifThread(void)
{
    if (!m_NotifThread.Empty()) {
        m_NotifThread->RequestStop();
        m_NotifThread->Join();
        m_NotifThread.Reset(0);
    }
}


void CQueueDataBase::RunStatisticsThread(void)
{
    // Once in 100 seconds
    m_StatisticsThread.Reset(new CStatisticsThread(
                                m_Host, *this, 100,
                                m_Server->IsLogStatisticsThread()));
    m_StatisticsThread->Run();
}


void CQueueDataBase::StopStatisticsThread(void)
{
    if (!m_StatisticsThread.Empty()) {
        m_StatisticsThread->RequestStop();
        m_StatisticsThread->Join();
        m_StatisticsThread.Reset(0);
    }
}


void CQueueDataBase::RunExecutionWatcherThread(unsigned run_delay)
{
    m_ExeWatchThread.Reset(new CJobQueueExecutionWatcherThread(
                                    m_Host, *this, run_delay,
                                    m_Server->IsLogExecutionWatcherThread()));
    m_ExeWatchThread->Run();
}


void CQueueDataBase::StopExecutionWatcherThread(void)
{
    if (!m_ExeWatchThread.Empty()) {
        m_ExeWatchThread->RequestStop();
        m_ExeWatchThread->Join();
        m_ExeWatchThread.Reset(0);
    }
}


END_NCBI_SCOPE

