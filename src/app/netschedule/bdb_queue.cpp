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
#include <connect/services/netschedule_client.hpp>
#include <connect/ncbi_socket.hpp>

#include <db.h>
#include <bdb/bdb_trans.hpp>
#include <bdb/bdb_cursor.hpp>
#include <bdb/bdb_util.hpp>

#include "bdb_queue.hpp"

#include "job_time_line.hpp"
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
    CIdBusyGuard(bm::bvector<>* id_set, 
                 unsigned int   id,
                 unsigned       timeout)
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
    bm::bvector<>*   m_IdSet;
    unsigned int     m_Id;
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
    NON_CONST_ITERATE(TQueueMap, it, m_QMap) {
        SLockedQueue* q = it->second;
        if (q->lb_coordinator) {
            q->lb_coordinator->StopCollectorThread();
        }
    }
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
  m_PurgeLastId(0),
  m_PurgeSkipCnt(0),
  m_DeleteChkPointCnt(0),
  m_FreeStatusMemCnt(0),
  m_LastFreeMem(time(0)),
  m_LastR2P(time(0)),
  m_UdpPort(0)
{
    m_IdCounter.Set(0);
}


CQueueDataBase::~CQueueDataBase()
{
    try {
        Close();
    } catch (exception& )
    {}
}


void CQueueDataBase::Open(const string& path, 
                          unsigned      cache_ram_size,
                          unsigned      max_locks,
                          unsigned      log_mem_size,
                          unsigned      max_trans)
{
    m_Path = CDirEntry::AddTrailingPathSeparator(path);

    {{
        CDir dir(m_Path);
        if ( !dir.Exists() ) {
            dir.Create();
        }
    }}

    delete m_Env;
    m_Env = new CBDB_Env();


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

    if (log_mem_size) {
        m_Env->SetLogInMemory(true);
        m_Env->SetLogBSize(log_mem_size);
    } else {
        m_Env->SetLogFileMax(200 * 1024 * 1024);
        m_Env->SetLogAutoRemove(true);
    }

    

    // Check if bdb env. files are in place and try to join
    CDir dir(m_Path);
    CDir::TEntries fl = dir.GetEntries("__db.*", CDir::eIgnoreRecursive);

    if (fl.empty()) {
        if (cache_ram_size) {
            m_Env->SetCacheSize(cache_ram_size);
        }
        //unsigned max_locks = m_Env->GetMaxLocks();
        if (max_locks) {
            m_Env->SetMaxLocks(max_locks);
            m_Env->SetMaxLockObjects(max_locks);
        }
        if (max_trans) {
            m_Env->SetTransactionMax(max_trans);
        }

        m_Env->OpenWithTrans(path.c_str(), CBDB_Env::eThreaded);
    } else {
        if (cache_ram_size) {
            m_Env->SetCacheSize(cache_ram_size);
        }
        try {
            m_Env->JoinEnv(path.c_str(), CBDB_Env::eThreaded);
            if (!m_Env->IsTransactional()) {
                LOG_POST(Info << 
                         "JS: '" << 
                         "' Warning: Joined non-transactional environment ");
            }
        } 
        catch (CBDB_ErrnoException& err_ex) 
        {
            if (err_ex.BDB_GetErrno() == DB_RUNRECOVERY) {
                LOG_POST(Warning << 
                         "JS: '" << 
                         "'Warning: DB_ENV returned DB_RUNRECOVERY code."
                         " Running the recovery procedure.");
            }
            m_Env->OpenWithTrans(path.c_str(), 
                                CBDB_Env::eThreaded | CBDB_Env::eRunRecovery);

        }
        catch (CBDB_Exception&)
        {
            m_Env->OpenWithTrans(path.c_str(), 
                                 CBDB_Env::eThreaded | CBDB_Env::eRunRecovery);
        }

    } // if else
    m_Env->SetDirectDB(true);
    m_Env->SetDirectLog(true);

    m_Env->SetLockTimeout(10 * 1000000); // 10 sec

    m_Env->SetTasSpins(5);

    if (m_Env->IsTransactional()) {
        m_Env->SetTransactionTimeout(10 * 1000000); // 10 sec
        m_Env->TransactionCheckpoint();
    }

    m_QueueDescriptionDB.SetEnv(*m_Env);
    m_QueueDescriptionDB.Open("sys_qdescr.db", CBDB_RawFile::eReadWriteCreate);
}


static
CNSLB_ThreasholdCurve* s_ConfigureCurve(const SQueueParameters& params,
                                        unsigned                exec_delay)
{
    auto_ptr<CNSLB_ThreasholdCurve> curve;
    do {

    if (params.lb_curve == eLBLinear) {
        curve.reset(new CNSLB_ThreasholdCurve_Linear(params.lb_curve_high,
                                                     params.lb_curve_linear_low));
        break;
    }

    if (params.lb_curve == eLBRegression) {
        curve.reset(new CNSLB_ThreasholdCurve_Regression(params.lb_curve_high,
                                                params.lb_curve_regression_a));
        break;
    }

    } while(0);

    if (curve.get()) {
        curve->ReGenerateCurve(exec_delay);
    }
    return curve.release();
}

/*
static
CNSLB_DecisionModule* s_ConfigureDecision(const IRegistry& reg, 
                                          const string&    sname)
{
    auto_ptr<CNSLB_DecisionModule> decision;
    string lb_policy = 
        reg.GetString(sname, "lb_policy", kEmptyStr);

    do {
    if (lb_policy.empty() ||
        NStr::CompareNocase(lb_policy, "rate")==0) {
        decision.reset(new CNSLB_DecisionModule_DistributeRate());
        break;
    }
    if (NStr::CompareNocase(lb_policy, "cpu_avail")==0) {
        decision.reset(new CNSLB_DecisionModule_CPU_Avail());
        break;
    }

    } while(0);

    return decision.release();
}
*/

void CQueueDataBase::Configure(const IRegistry& reg, unsigned* min_run_timeout)
{
    bool no_default_queues = 
        reg.GetBool("server", "no_default_queues", false, 0, IRegistry::eReturn);

    CFastMutexGuard guard(m_ConfigureLock);

    x_CleanParamMap();

    CBDB_Transaction trans(*m_Env, 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);
    m_QueueDescriptionDB.SetTransaction(&trans);

    string qclass;
    string tmp;
    list<string> sections;
    reg.EnumerateSections(&sections);

    ITERATE(list<string>, it, sections) {
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
            m_QueueDescriptionDB.kind = 0;
            m_QueueDescriptionDB.qclass = qclass;
            m_QueueDescriptionDB.UpdateInsert();
        }
    }
    trans.Commit();
    m_QueueDescriptionDB.Sync();

    CBDB_FileCursor cur(m_QueueDescriptionDB);
    cur.SetCondition(CBDB_FileCursor::eFirst);

    string qname;
    while (cur.Fetch() == eBDB_Ok) {
        qname = m_QueueDescriptionDB.queue;
        qclass = m_QueueDescriptionDB.qclass;
        TQueueParamMap::iterator it = m_QueueParamMap.find(qclass);
        if (it == m_QueueParamMap.end()) {
            LOG_POST(Error << "Can not find class " << qclass << " for queue " << qname);
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
            MountQueue(qname, qclass, params);
        } else {
            // update non-critical queue parameters
            UpdateQueueParameters(qname, params);
        }

        // re-load load balancing settings
        UpdateQueueLBParameters(qname, params);
    }
}


void CQueueDataBase::MountQueue(const string& qname,
                                const string& qclass,
                                const SQueueParameters& params)
{
    _ASSERT(m_Env);

    auto_ptr<SLockedQueue> q(new SLockedQueue(qname, qclass));
    string fname = string("jsq_") + qname + string(".db");
    q->db.SetEnv(*m_Env);

    q->db.RevSplitOff();
    q->db.Open(fname.c_str(), CBDB_RawFile::eReadWriteCreate);

    fname = string("jsq_") + qname + string("_affid.idx");
    q->aff_idx.SetEnv(*m_Env);
    q->aff_idx.Open(fname.c_str(), CBDB_RawFile::eReadWriteCreate);

    q->timeout = params.timeout;
    q->notif_timeout = params.notif_timeout;
    q->delete_done = params.delete_when_done;
    q->last_notif = time(0);

    q->affinity_dict.Open(*m_Env, qname);

    SLockedQueue& queue = m_QueueCollection.AddQueue(qname, q.release());

    queue.run_timeout = params.run_timeout;
    if (params.run_timeout) {
        queue.run_time_line =
            new CJobTimeLine(params.run_timeout_precision, 0);
    }

    // scan the queue, load the state machine from DB

    CBDB_FileCursor cur(queue.db);
    cur.SetCondition(CBDB_FileCursor::eGE);
    cur.From << 0;

    unsigned recs = 0;

    for (;cur.Fetch() == eBDB_Ok; ++recs) {
        unsigned job_id = queue.db.id;
        int status = queue.db.status;


        if (job_id > (unsigned)m_IdCounter.Get()) {
            m_IdCounter.Set(job_id);
        }
        queue.status_tracker.SetExactStatusNoLock(job_id, 
                      (CNetScheduleClient::EJobStatus) status, 
                      true);

        if (status == (int) CNetScheduleClient::eRunning && 
            queue.run_time_line) {
            // Add object to the first available slot
            // it is going to be rescheduled or dropped
            // in the background control thread
            queue.run_time_line->AddObjectToSlot(0, job_id);
        }
    }

    queue.udp_socket.SetReuseAddress(eOn);
    unsigned short udp_port = GetUdpPort();
    if (udp_port) {
        queue.udp_socket.Bind(udp_port);
    }

    // program version control
    if (!params.program_name.empty()) {
        queue.program_version_list.AddClientInfo(params.program_name);
    }

    queue.subm_hosts.SetHosts(params.subm_hosts);
    queue.failed_retries = params.failed_retries;
    queue.wnode_hosts.SetHosts(params.wnode_hosts);
    queue.rec_dump_flag = params.dump_db;


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
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);
    m_QueueDescriptionDB.SetTransaction(&trans);
    m_QueueDescriptionDB.queue = qname;
    m_QueueDescriptionDB.kind = 1;
    m_QueueDescriptionDB.qclass = qclass;
    m_QueueDescriptionDB.comment = comment;
    m_QueueDescriptionDB.UpdateInsert();
    trans.Commit();
    m_QueueDescriptionDB.Sync();
    const SQueueParameters& params = *(it->second);
    MountQueue(qname, qclass, params);
    UpdateQueueLBParameters(qname, params);
}


void CQueueDataBase::DeleteQueue(const string& qname)
{
    CFastMutexGuard guard(m_ConfigureLock);
    CBDB_Transaction trans(*m_Env, 
                           CBDB_Transaction::eTransASync,
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
        NCBI_THROW(CNetScheduleException, eOperationAccessDenied, msg);
    }
    if (!m_QueueCollection.RemoveQueue(qname)) {
        string msg = "Job queue not found: ";
        msg += qname;
        NCBI_THROW(CNetScheduleException, eUnknownQueue, msg);
    }
    m_QueueDescriptionDB.Delete(CBDB_File::eIgnoreError);
    trans.Commit();
}


void CQueueDataBase::QueueInfo(const string& qname, int& kind,
                               string* qclass, string* comment)
{
    CFastMutexGuard guard(m_ConfigureLock);
    m_QueueDescriptionDB.SetTransaction(0);
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

    queue->program_version_list.Clear();
    if (!params.program_name.empty()) {
        queue->program_version_list.AddClientInfo(params.program_name);
    }
    queue->subm_hosts.SetHosts(params.subm_hosts);
    queue->failed_retries = params.failed_retries;
    queue->wnode_hosts.SetHosts(params.wnode_hosts);
    {{
        CFastMutexGuard guard(queue->rec_dump_lock);
        queue->rec_dump_flag = params.dump_db;
    }}
}

void CQueueDataBase::UpdateQueueLBParameters(const string& qname,
                                             const SQueueParameters& params)
{
    CRef<SLockedQueue> queue(m_QueueCollection.GetLockedQueue(qname));

    if (params.lb_flag == queue->lb_flag) return;

    if (params.lb_service.empty()) {
        if (params.lb_flag) {
            LOG_POST(Error << "Queue:" << qname 
                << "cannot be load balanced. Missing lb_service ini setting");
        }
        queue->lb_flag = false;
    } else {
        ENSLB_RunDelayType lb_delay_type = eNSLB_Constant;
        unsigned lb_stall_time = 6;
        if (NStr::CompareNocase(params.lb_exec_delay_str, "run_time")) {
            lb_delay_type = eNSLB_RunTimeAvg;
        } else {
            try {
                int stall_time =
                    NStr::StringToInt(params.lb_exec_delay_str);
                if (stall_time > 0) {
                    lb_stall_time = stall_time;
                }
            } 
            catch(exception& ex)
            {
                ERR_POST("Invalid value of lb_exec_delay " 
                        << ex.what()
                        << " Offending value:"
                        << params.lb_exec_delay_str
                        );
            }
        }

        CNSLB_Coordinator::ENonLbHostsPolicy non_lb_hosts = 
            CNSLB_Coordinator::eNonLB_Allow;
        if (NStr::CompareNocase(params.lb_unknown_host, "deny") == 0) {
            non_lb_hosts = CNSLB_Coordinator::eNonLB_Deny;
        } else
        if (NStr::CompareNocase(params.lb_unknown_host, "allow") == 0) {
            non_lb_hosts = CNSLB_Coordinator::eNonLB_Allow;
        } else
        if (NStr::CompareNocase(params.lb_unknown_host, "reserve") == 0) {
            non_lb_hosts = CNSLB_Coordinator::eNonLB_Reserve;
        }

        if (queue->lb_flag == false) { // LB is OFF
            LOG_POST(Error << "Queue:" << qname 
                        << " is load balanced. " << params.lb_service);

            if (queue->lb_coordinator == 0) {
                auto_ptr<CNSLB_ThreasholdCurve> 
                deny_curve(s_ConfigureCurve(params, lb_stall_time));

                //CNSLB_DecisionModule*   decision_maker = 0;

                auto_ptr<CNSLB_DecisionModule_DistributeRate> 
                    decision_distr_rate(
                        new CNSLB_DecisionModule_DistributeRate);

                auto_ptr<INSLB_Collector>   
                    collect(new CNSLB_LBSMD_Collector());

                auto_ptr<CNSLB_Coordinator> 
                coord(new CNSLB_Coordinator(
                                params.lb_service,
                                collect.release(),
                                deny_curve.release(),
                                decision_distr_rate.release(),
                                params.lb_collect_time,
                                non_lb_hosts));

                queue->lb_coordinator = coord.release();
                queue->lb_stall_delay_type = lb_delay_type;
                queue->lb_stall_time = lb_stall_time;
                queue->lb_stall_time_mult = params.lb_stall_time_mult;
                queue->lb_flag = true;

            } else {  // LB is ON
                // reconfigure the LB delay
                CFastMutexGuard guard(queue->lb_stall_time_lock);
                queue->lb_stall_delay_type = lb_delay_type;
                if (lb_delay_type == eNSLB_Constant) {
                    queue->lb_stall_time = lb_stall_time;
                }
                queue->lb_stall_time_mult = params.lb_stall_time_mult;
            }
        }
    }
}


void CQueueDataBase::Close()
{
    StopNotifThread();
    StopPurgeThread();
    StopExecutionWatcherThread();

    if (m_Env) {
        m_Env->TransactionCheckpoint();
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


unsigned int CQueueDataBase::GetNextId()
{
    unsigned int id;

    if (m_IdCounter.Get() >= kMax_I4) {
        m_IdCounter.Set(0);
    }
    id = (unsigned) m_IdCounter.Add(1); 

    if ((id % 1000) == 0) {
        m_Env->TransactionCheckpoint();
    }
    if ((id % 1000000) == 0) {
        m_Env->CleanLog();
    }

    return id;
}


unsigned int CQueueDataBase::GetNextIdBatch(unsigned count)
{
    if (m_IdCounter.Get() >= kMax_I4) {
        m_IdCounter.Set(0);
    }
    unsigned int id = (unsigned) m_IdCounter.Add(count);
    id = id - count + 1;
    return id;
}


void CQueueDataBase::TransactionCheckPoint()
{
    m_Env->TransactionCheckpoint();
    m_Env->CleanLog();
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

    // Delete jobs unconditionally, from internal vector.
    // This is done to spread massive job deletion in time
    // and thus smooth out peak loads
    // TODO: determine batch size based on load
    unsigned n_jobs_to_delete = 10000; 
    unsigned unc_del_rec = x_PurgeUnconditional(n_jobs_to_delete);
    global_del_rec += unc_del_rec;

    // check not to rescan the database too often 
    // when we have low client activity of inserting new jobs.
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

    // Delete obsolete job records, based on time stamps 
    // and expiration timeouts
    unsigned n_queue_limit = 2000; // TODO: determine this based on load

    CNetScheduleClient::EJobStatus statuses_to_delete_from[] = {
        CNetScheduleClient::eFailed, CNetScheduleClient::eCanceled,
        CNetScheduleClient::eDone, CNetScheduleClient::ePending
    };
    const int kStatusesSize = sizeof(statuses_to_delete_from) /
                              sizeof(CNetScheduleClient::EJobStatus);

    ITERATE(CQueueCollection, it, m_QueueCollection) {

        unsigned queue_del_rec = 0;
        const unsigned batch_size = 100;
        for (bool stop_flag = false; !stop_flag; ) {
            // stop if all statuses have less than batch_size jobs to delete
            stop_flag = true;
            for (int n = 0; n < kStatusesSize; ++n) {
                unsigned del_rec = (*it).CheckDeleteBatch(batch_size, 
                                              statuses_to_delete_from[n],
                                              m_PurgeLastId);
                stop_flag &= del_rec < batch_size;
                queue_del_rec += del_rec;
            }

            // do not delete more than certain number of 
            // records from the queue in one Purge
            if (queue_del_rec >= n_queue_limit) {
                break;
            } else {
                SleepMilliSec(200);
            }

            if (x_CheckStopPurge()) return;
        }
        global_del_rec += queue_del_rec;

    } // ITERATE

    m_DeleteChkPointCnt += global_del_rec;
    if (m_DeleteChkPointCnt > 1000) {
        m_DeleteChkPointCnt = 0;
        x_OptimizeAffinity();
        m_Env->TransactionCheckpoint();
    }

    m_FreeStatusMemCnt += global_del_rec;
    x_OptimizeStatusMatrix();

    if (global_del_rec > n_jobs_to_delete + n_queue_limit * m_QueueCollection.GetSize()) {
        SleepMilliSec(1000);
    }
}

unsigned CQueueDataBase::x_PurgeUnconditional(unsigned batch_size) {
    // Purge unconditional jobs
    bm::bvector<> jobs_to_delete;
    {{
        CFastMutexGuard guard(m_JobsToDeleteLock);
        unsigned job_id = 0;
        for (unsigned n = 0; n < batch_size &&
                        (job_id = m_JobsToDelete.extract_next(job_id));
                        ++n) {
            jobs_to_delete.set(job_id);
        }
    }}
    if (jobs_to_delete.none()) return 0;

    unsigned del_rec = 0;
    // We don't have info on job's queue, so we try all queues
    // NB: it is suboptimal at best - better to extend state matrix with
    // jobs to delete vector and do this per queue with jobs to be deleted
    // definitely belonging to given queue.
    ITERATE(CQueueCollection, it, m_QueueCollection) {
        del_rec += (*it).DeleteBatch(jobs_to_delete);
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


void CQueueDataBase::x_OptimizeAffinity(void)
{
    // remove unused affinity elements
    ITERATE(CQueueCollection, it, m_QueueCollection) {
        (*it).ClearAffinityIdx();
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


unsigned CQueue::CountRecs()
{
    CRef<SLockedQueue> q(x_GetLQueue());
    CFastMutexGuard guard(q->lock);
	q->db.SetTransaction(0);
    return q->db.CountRecs();
}


#define NS_PRINT_TIME(msg, t) \
    do \
    { unsigned tt = t; \
      CTime _t(tt); _t.ToLocalTime(); \
      out << msg << (tt ? _t.AsString() : kEmptyStr) << fsp; \
    } while(0)

#define NS_PFNAME(x_fname) \
    (fflag ? (const char*) x_fname : (const char*) "")

void CQueue::x_PrintJobDbStat(SQueueDB&      db, 
                              CNcbiOstream&  out,
                              const char*    fsp,
                              bool           fflag)
{
    out << fsp << NS_PFNAME("id: ") << (unsigned) db.id << fsp;
    CNetScheduleClient::EJobStatus status = 
        (CNetScheduleClient::EJobStatus)(int)db.status;
    out << NS_PFNAME("status: ") << CNetScheduleClient::StatusToString(status) 
        << fsp;

    NS_PRINT_TIME(NS_PFNAME("time_submit: "), db.time_submit);
    NS_PRINT_TIME(NS_PFNAME("time_run: "), db.time_run);
    NS_PRINT_TIME(NS_PFNAME("time_done: "), db.time_done);

    out << NS_PFNAME("timeout: ") << (unsigned)db.timeout << fsp;
    out << NS_PFNAME("run_timeout: ") << (unsigned)db.run_timeout << fsp;

    time_t exp_time = 
        x_ComputeExpirationTime((unsigned)db.time_run, 
                                (unsigned)db.run_timeout);
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

    out << NS_PFNAME("input: ")        << "'" <<(string) db.input       << "'" << fsp;
    out << NS_PFNAME("output: ")       << "'" <<(string) db.output       << "'" << fsp;
    out << NS_PFNAME("err_msg: ")      << "'" <<(string) db.err_msg      << "'" << fsp;
    out << NS_PFNAME("progress_msg: ") << "'" <<(string) db.progress_msg << "'" << fsp;
    out << NS_PFNAME("cout: ")         << "'" <<(string) db.cout         << "'" << fsp;
    out << NS_PFNAME("cerr: ")         << "'" <<(string) db.cerr         << "'" << fsp;
    out << "\n";
}

void 
CQueue::x_PrintShortJobDbStat(SQueueDB&     db, 
                              const string& host,
                              unsigned      port,
                              CNcbiOstream& out,
                              const char*   fsp)
{
    char buf[1024];
    sprintf(buf, NETSCHEDULE_JOBMASK, 
                 (unsigned)db.id, host.c_str(), port);
    out << buf << fsp;
    CNetScheduleClient::EJobStatus status = 
        (CNetScheduleClient::EJobStatus)(int)db.status;
    out << CNetScheduleClient::StatusToString(status) << fsp;

    out << "'" << (string) db.input    << "'" << fsp;
    out << "'" << (string) db.output   << "'" << fsp;
    out << "'" << (string) db.err_msg  << "'" << fsp;

    out << "\n";
}


void 
CQueue::PrintJobDbStat(unsigned                       job_id, 
                       CNcbiOstream&                  out,
                       CNetScheduleClient::EJobStatus status)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    SQueueDB& db = q->db;
    if (status == CNetScheduleClient::eJobNotFound) {
        CFastMutexGuard guard(q->lock);
        db.SetTransaction(0);
        db.id = job_id;
        if (db.Fetch() == eBDB_Ok) {
            x_PrintJobDbStat(db, out);
        } else {
            out << "Job not found id=" << job_id;
        }
        out << "\n";
    } else {
        CJobStatusTracker::TBVector bv;
        q->status_tracker.StatusSnapshot(status, &bv);

        CJobStatusTracker::TBVector::enumerator en(bv.first());
        for (;en.valid(); ++en) {
            unsigned id = *en;
            {{
                CFastMutexGuard guard(q->lock);
	            db.SetTransaction(0);
                
                db.id = id;

                if (db.Fetch() == eBDB_Ok) {
                    x_PrintJobDbStat(db, out);
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


bool CQueue::IsVersionControl() const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    return q->program_version_list.IsConfigured();
}


bool CQueue::IsMatchingClient(const CQueueClientInfo& cinfo) const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    return q->program_version_list.IsMatchingClient(cinfo);
}


bool CQueue::IsSubmitAllowed() const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    return (m_ClientHostAddr == 0) || 
            q->subm_hosts.IsAllowed(m_ClientHostAddr);
}


bool CQueue::IsWorkerAllowed() const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    return (m_ClientHostAddr == 0) || 
            q->wnode_hosts.IsAllowed(m_ClientHostAddr);
}


double CQueue::GetAverage(TStatEvent n_event)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    return q->GetAverage(n_event);
}


void CQueue::PrintAllJobDbStat(CNcbiOstream& out)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    SQueueDB& db = q->db;
    CFastMutexGuard guard(q->lock);

    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    db.SetTransaction(&trans);

    CBDB_FileCursor& cur = *x_GetCursor(trans);
    CBDB_CursorGuard cg(cur);    

    cur.SetCondition(CBDB_FileCursor::eGE);
    cur.From << 0;

    while (cur.Fetch() == eBDB_Ok) {
        x_PrintJobDbStat(db, out);
        if (!out.good()) break;
    }
}


void CQueue::PrintStat(CNcbiOstream& out)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    SQueueDB& db = q->db;
    CFastMutexGuard guard(q->lock);
	db.SetTransaction(0);
    db.PrintStat(out);
}


void CQueue::PrintQueue(CNcbiOstream&                  out, 
                        CNetScheduleClient::EJobStatus job_status,
                        const string&                  host,
                        unsigned                       port)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    CJobStatusTracker::TBVector bv;
    q->status_tracker.StatusSnapshot(job_status, &bv);
    SQueueDB& db = q->db;

    CJobStatusTracker::TBVector::enumerator en(bv.first());
    for (;en.valid(); ++en) {
        unsigned id = *en;
        {{
            CFastMutexGuard guard(q->lock);
	        db.SetTransaction(0);
            
            db.id = id;

            if (db.Fetch() == eBDB_Ok) {
                x_PrintShortJobDbStat(db, host, port, out, "\t");
            }
        }}
    }
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
    q->subm_hosts.PrintHosts(out);
}


void CQueue::PrintWNodeHosts(CNcbiOstream& out) const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    q->wnode_hosts.PrintHosts(out);
}


unsigned int 
CQueue::Submit(const char*   input,
               unsigned      host_addr,
               unsigned      port,
               unsigned      wait_timeout,
               const char*   progress_msg,
               const char*   affinity_token,
               const char*   jout,
               const char*   jerr,
               unsigned      mask)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    bool was_empty = !q->status_tracker.AnyPending();

    unsigned int job_id = m_Db.GetNextId();

    CNetSchedule_JS_Guard js_guard(q->status_tracker, 
                                   job_id,
                                   CNetScheduleClient::ePending);
    SQueueDB& db = q->db;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    unsigned aff_id = 0;
    if (affinity_token && *affinity_token) {
        aff_id = q->affinity_dict.CheckToken(affinity_token, trans);
    }

    {{
        CFastMutexGuard guard(q->lock);
	    db.SetTransaction(&trans);

        x_AssignSubmitRec(
            job_id, input, host_addr, port, wait_timeout, 
            progress_msg, aff_id, jout, jerr, mask);

        db.Insert();

        // update affinity index
        if (aff_id) {
            q->aff_idx.SetTransaction(&trans);
            x_AddToAffIdx_NoLock(aff_id, job_id);
        }
    }}

    trans.Commit();
    js_guard.Commit();

    if (was_empty) NotifyListeners(true);

    return job_id;
}


unsigned int 
CQueue::SubmitBatch(vector<SNS_BatchSubmitRec>& batch,
                    unsigned                    host_addr,
                    unsigned                    port,
                    unsigned                    wait_timeout,
                    const char*                 progress_msg)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    bool was_empty = !q->status_tracker.AnyPending();

    unsigned job_id = m_Db.GetNextIdBatch(batch.size());

    SQueueDB& db = q->db;

    unsigned batch_aff_id = 0; // if batch comes with the same affinity
    bool     batch_has_aff = false;

    // process affinity ids
    {{
        CBDB_Transaction trans(*db.GetEnv(), 
                            CBDB_Transaction::eTransASync,
                            CBDB_Transaction::eNoAssociation);
        for (unsigned i = 0; i < batch.size(); ++i) {
            SNS_BatchSubmitRec& subm = batch[i];
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
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    {{
        CFastMutexGuard guard(q->lock);
	    db.SetTransaction(&trans);

        unsigned job_id_cnt = job_id;
        NON_CONST_ITERATE(vector<SNS_BatchSubmitRec>, it, batch) {
            it->job_id = job_id_cnt;
            x_AssignSubmitRec(
                job_id_cnt, 
                it->input, host_addr, port, wait_timeout, 0/*progress_msg*/, 
                it->affinity_id,
                0, // cout
                0, // cerr
                0  // mask
                );
            ++job_id_cnt;
            db.Insert();
        } // ITERATE

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
    }}
    trans.Commit();

    q->status_tracker.AddPendingBatch(job_id, job_id + batch.size() - 1);

    if (was_empty) NotifyListeners(true);

    return job_id;
}


void 
CQueue::x_AssignSubmitRec(unsigned      job_id,
                          const char*   input,
                          unsigned      host_addr,
                          unsigned      port,
                          unsigned      wait_timeout,
                          const char*   progress_msg,
                          unsigned      aff_id,
                          const char*   jout,
                          const char*   jerr, 
                          unsigned      mask)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    SQueueDB& db = q->db;

    db.id = job_id;
    db.status = (int) CNetScheduleClient::ePending;

    db.time_submit = time(0);
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
    db.time_lb_first_eval = 0;
    db.aff_id = aff_id;
    db.mask = mask;

    if (input) {
        db.input = input;
    }
    db.output = "";

    db.err_msg = "";

    db.cout = jout? jout : "";
    db.cerr = jerr? jerr : "";

    if (progress_msg) {
        db.progress_msg = progress_msg;
    }
}


unsigned 
CQueue::CountStatus(CNetScheduleClient::EJobStatus st) const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    return q->status_tracker.CountStatus(st);
}


void 
CQueue::StatusStatistics(
    CNetScheduleClient::EJobStatus status,
    CJobStatusTracker::TBVector::statistics* st) const
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
                            CBDB_Transaction::eTransASync,
                            CBDB_Transaction::eNoAssociation);

        CFastMutexGuard guard(q->lock);
        db.SetTransaction(0);

        db.id = job_id;
        if (db.Fetch() == eBDB_Ok) {
            //int status = db.status;
            db.status = (int) CNetScheduleClient::ePending;
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
                                   CNetScheduleClient::eCanceled);
    CNetScheduleClient::EJobStatus st = js_guard.GetOldStatus();
    if (q->status_tracker.IsCancelCode(st)) {
        js_guard.Commit();
        return;
    }

    {{
        SQueueDB& db = q->db;
        CBDB_Transaction trans(*db.GetEnv(), 
                            CBDB_Transaction::eTransASync,
                            CBDB_Transaction::eNoAssociation);

        CFastMutexGuard guard(q->lock);
        db.SetTransaction(0);

        db.id = job_id;
        if (db.Fetch() == eBDB_Ok) {
            int status = db.status;
            if (status < (int) CNetScheduleClient::eCanceled) {
                db.status = (int) CNetScheduleClient::eCanceled;
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

    x_DropJob(job_id);

    if (q->monitor.IsMonitorActive()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::DropJob() job id=";
        msg += NStr::IntToString(job_id);
        q->monitor.SendString(msg);
    }
}


void CQueue::x_DropJob(unsigned job_id)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    SQueueDB& db = q->db;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    q->status_tracker.SetStatus(job_id, CNetScheduleClient::eJobNotFound);
    {{
        CFastMutexGuard guard(q->lock);
        db.SetTransaction(&trans);

        CBDB_FileCursor& cur = *x_GetCursor(trans);
        CBDB_CursorGuard cg(cur);    
        cur.SetCondition(CBDB_FileCursor::eEQ);
        cur.From << job_id;

        if (cur.FetchFirst() != eBDB_Ok) {
            return;
        }

        x_DeleteDBRec(db, cur);
    }}
    trans.Commit();
}


void CQueue::PutResult(unsigned int  job_id,
                       int           ret_code,
                       const char*   output)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    CNetSchedule_JS_Guard js_guard(q->status_tracker, 
                                   job_id,
                                   CNetScheduleClient::eDone);
    if (q->delete_done) {
        q->status_tracker.SetStatus(job_id, 
                                    CNetScheduleClient::eJobNotFound);
    }
    CNetScheduleClient::EJobStatus st = js_guard.GetOldStatus();
    if (q->status_tracker.IsCancelCode(st)) {
        js_guard.Commit();
        return;
    }
    bool rec_updated;
    SSubmitNotifInfo si;
    time_t curr = time(0);

    SQueueDB& db = q->db;

    for (unsigned repeat = 0; true; ) {
        try {
            CBDB_Transaction trans(*db.GetEnv(), 
                                   CBDB_Transaction::eTransASync,
                                   CBDB_Transaction::eNoAssociation);

            {{
                CFastMutexGuard guard(q->lock);
                db.SetTransaction(&trans);
                CBDB_FileCursor& cur = *x_GetCursor(trans);
                CBDB_CursorGuard cg(cur);
                rec_updated = 
                    x_UpdateDB_PutResultNoLock(db, curr, cur, 
                                               job_id, ret_code, output, &si);
            }}

            trans.Commit();
            js_guard.Commit();
            if (job_id) x_Count(SLockedQueue::eStatPutEvent);
        } 
        catch (CBDB_ErrnoException& ex) {
            if (ex.IsDeadLock()) {
                if (++repeat < k_max_dead_locks) {
                    if (q->monitor.IsMonitorActive()) {
                        q->monitor.SendString(
                            "DeadLock repeat in CQueue::PutResult");
                    }
                    SleepMilliSec(250);
                    continue;
                }
            } else 
            if (ex.IsNoMem()) {
                if (++repeat < k_max_dead_locks) {
                    if (q->monitor.IsMonitorActive()) {
                        q->monitor.SendString(
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

    if (q->monitor.IsMonitorActive()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::PutResult() job id=";
        msg += NStr::IntToString(job_id);
        msg += " ret_code=";
        msg += NStr::IntToString(ret_code);
        msg += " output=";
        msg += output;

        q->monitor.SendString(msg);
    }
}


bool 
CQueue::x_UpdateDB_PutResultNoLock(SQueueDB&            db,
                                   time_t               curr,
                                   CBDB_FileCursor&     cur,
                                   unsigned             job_id,
                                   int                  ret_code,
                                   const char*          output,
                                   SSubmitNotifInfo*    subm_info)
{
    CRef<SLockedQueue> q(x_GetLQueue());

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

    if (q->delete_done) {
        cur.Delete();
    } else {
        db.status = (int) CNetScheduleClient::eDone;
        db.ret_code = ret_code;
        db.output = output;
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
                        const char*    output,
                        unsigned int   worker_node,
                        unsigned int*  job_id,
                        char*          input,
                        char*          jout,
                        char*          jerr,
                        const string&  client_name,
                        unsigned*      job_mask)
{
    _ASSERT(job_id);
    _ASSERT(input);
    _ASSERT(job_mask);

    CRef<SLockedQueue> q(x_GetLQueue());

    unsigned dead_locks = 0;       // dead lock counter

    time_t curr = time(0);
    
    // 
    bool need_update;
    CNetSchedule_JS_Guard js_guard(q->status_tracker, 
                                   done_job_id,
                                   CNetScheduleClient::eDone,
                                   &need_update);
    // This is a HACK - if js_guard is not commited, it will rollback
    // to previous state, so it is safe to change status after the guard.
    if (q->delete_done) {
        q->status_tracker.SetStatus(done_job_id, 
                                    CNetScheduleClient::eJobNotFound);
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

    unsigned job_aff_id = 0;

    try {
        CBDB_Transaction trans(*(pqdb->GetEnv()), 
                            CBDB_Transaction::eTransASync,
                            CBDB_Transaction::eNoAssociation);

        if (use_db_mutex) {
            q->lock.Lock();
        }
        pqdb->SetTransaction(&trans);

        CBDB_FileCursor* pcur;
        if (use_db_mutex) {
            pcur = x_GetCursor(trans);
        } else {
            pcur = x_GetLocalCursor(trans);
        }
        CBDB_FileCursor& cur = *pcur;
        {{
            CBDB_CursorGuard cg(cur);

            // put result
            if (need_update) {
                done_rec_updated =
                    x_UpdateDB_PutResultNoLock(
                      *pqdb, curr, cur, done_job_id, ret_code, output, NULL);
            }

            if (pending_job_id) {
                *job_id = pending_job_id;
                EGetJobUpdateStatus upd_status;
                upd_status =
                    x_UpdateDB_GetJobNoLock(*pqdb, curr, cur, trans,
                                         worker_node, pending_job_id, input,
                                         jout, jerr,
                                         &job_aff_id,
                                         job_mask);
                switch (upd_status) {
                case eGetJobUpdate_JobFailed:
                    q->status_tracker.ChangeStatus(*job_id, 
                                                   CNetScheduleClient::eFailed);
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
                if (q->monitor.IsMonitorActive()) {
                    q->monitor.SendString(
                        "DeadLock repeat in CQueue::JobExchange");
                }
                SleepMilliSec(250);
                goto repeat_transaction;
            } 
        }
        else
        if (ex.IsNoMem()) {
            if (++dead_locks < k_max_dead_locks) {
                if (q->monitor.IsMonitorActive()) {
                    q->monitor.SendString(
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
    }

    x_TimeLineExchange(done_job_id, *job_id, curr);

    if (done_rec_updated) {
        // TODO: send a UDP notification and update runtime stat
    }

    if (q->monitor.IsMonitorActive()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();

        msg += " CQueue::PutResultGetJob() (PUT) job id=";
        msg += NStr::IntToString(done_job_id);
        msg += " ret_code=";
        msg += NStr::IntToString(ret_code);
        msg += " output=";
        msg += output;

        q->monitor.SendString(msg);
    }
}


void CQueue::PutProgressMessage(unsigned int  job_id,
                                const char*   msg)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    SQueueDB& db = q->db;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    CFastMutexGuard guard(q->lock);
    db.SetTransaction(&trans);

    {{
    CBDB_FileCursor& cur = *x_GetCursor(trans);
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

    if (q->monitor.IsMonitorActive()) {
        CTime tmp_t(CTime::eCurrent);
        string mmsg = tmp_t.AsString();
        mmsg += " CQueue::PutProgressMessage() job id=";
        mmsg += NStr::IntToString(job_id);
        mmsg += " msg=";
        mmsg += msg;

        q->monitor.SendString(mmsg);
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

    // We first change memory state to "Failed", it is safer because
    // there is only danger to find job in inconsistent state, and because
    // Failed is terminal, usually you can not allocate job or do anything
    // disturbing from this state.
    CNetSchedule_JS_Guard js_guard(q->status_tracker, 
                                   job_id,
                                   CNetScheduleClient::eFailed);

    SQueueDB& db = q->db;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    unsigned subm_addr, subm_port, subm_timeout, time_submit;
    time_t curr = time(0);

    {{
        CFastMutexGuard aff_guard(q->aff_map_lock);
        CFastMutexGuard guard(q->lock);
        db.SetTransaction(&trans);

        CBDB_FileCursor& cur = *x_GetCursor(trans);
        CBDB_CursorGuard cg(cur);    

        cur.SetCondition(CBDB_FileCursor::eEQ);
        cur.From << job_id;

        if (cur.FetchFirst() != eBDB_Ok) {
            // TODO: Integrity error or job just expired?
            return;
        }

        db.time_done = curr;
        db.err_msg = err_msg;
        db.output = output;
        db.ret_code = ret_code;

        unsigned run_counter = db.run_counter;
        if (run_counter <= q->failed_retries) {
            // NB: due to conflict with locking pattern of x_FindPendingJob we can not
            // acquire aff_map_lock here! So we aquire it earlier (see above).
            q->worker_aff_map.BlacklistJob(worker_node, client_name, job_id);
            // Pending status is not a bug here, returned and pending
            // has the same meaning, but returned jobs are getting delayed
            // for a little while (eReturned status)
            db.status = (int) CNetScheduleClient::ePending;
            // We can do this because js_guard record only old state and
            // on Commit just releases job.
            q->status_tracker.SetStatus(job_id,
                CNetScheduleClient::eReturned);
        } else {
            db.status = (int) CNetScheduleClient::eFailed;
        }

        // We don't need to lock affinity map anymore, so to reduce locking
        // region we can manually release it here.
        aff_guard.Release();

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

    if (q->monitor.IsMonitorActive()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::JobFailed() job id=";
        msg += NStr::IntToString(job_id);
        msg += " err_msg=";
        msg += err_msg;
        msg += " output=";
        msg += output;
        if (db.status == (int) CNetScheduleClient::ePending)
            msg += " rescheduled";
        q->monitor.SendString(msg);
    }
}


void CQueue::SetJobRunTimeout(unsigned job_id, unsigned tm)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    CNetScheduleClient::EJobStatus st = GetStatus(job_id);
    if (st != CNetScheduleClient::eRunning) {
        return;
    }
    SQueueDB& db = q->db;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    unsigned exp_time = 0;
    time_t curr = time(0);

    {{
    CFastMutexGuard guard(q->lock);
    db.SetTransaction(&trans);

    CBDB_FileCursor& cur = *x_GetCursor(trans);
    CBDB_CursorGuard cg(cur);    

    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << job_id;

    if (cur.FetchFirst() != eBDB_Ok) {
        return;
    }

    unsigned time_run = db.time_run;
    unsigned run_timeout = db.run_timeout;

    exp_time = x_ComputeExpirationTime(time_run, run_timeout);
    if (exp_time == 0) {
        return;
    }

    db.run_timeout = tm;
    db.time_run = curr;

    cur.Update();

    }}

    trans.Commit();

    {{
        CJobTimeLine& tl = *q->run_time_line;

        CWriteLockGuard guard(q->rtl_lock);
        tl.MoveObject(exp_time, curr + tm, job_id);
    }}

    if (q->monitor.IsMonitorActive()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::SetJobRunTimeout: Job id=";
        msg += NStr::IntToString(job_id);
        tmp_t.SetTimeT(curr + tm);
        tmp_t.ToLocalTime();
        msg += " new_expiration_time=";
        msg += tmp_t.AsString();
        msg += " job_timeout(sec)=";
        msg += NStr::IntToString(tm);
        msg += " job_timeout(minutes)=";
        msg += NStr::IntToString(tm/60);

        q->monitor.SendString(msg);
    }
}

void CQueue::JobDelayExpiration(unsigned job_id, unsigned tm)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    unsigned q_time_descr = 20;
    unsigned run_timeout;
    unsigned time_run;
    unsigned old_run_timeout;

    CNetScheduleClient::EJobStatus st = GetStatus(job_id);
    if (st != CNetScheduleClient::eRunning) {
        return;
    }
    SQueueDB& db = q->db;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    unsigned exp_time = 0;
    time_t curr = time(0);

    {{
    CFastMutexGuard guard(q->lock);
    db.SetTransaction(&trans);

    CBDB_FileCursor& cur = *x_GetCursor(trans);
    CBDB_CursorGuard cg(cur);    

    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << job_id;

    if (cur.FetchFirst() != eBDB_Ok) {
        return;
    }

    //    int status = db.status;

    time_run = db.time_run;
    run_timeout = db.run_timeout;
    if (run_timeout == 0) {
        run_timeout = q->run_timeout;
    }
    old_run_timeout = run_timeout;

    // check if current timeout is enought and job requires no prolongation
    time_t safe_exp_time = 
        curr + std::max((unsigned)q->run_timeout, 2*tm) + q_time_descr;
    if (time_run + run_timeout > (unsigned) safe_exp_time) {
        return;
    }

    if (time_run == 0) {
        time_run = curr;
        db.time_run = curr;
    }

    while (time_run + run_timeout <= (unsigned) safe_exp_time) {
        run_timeout += std::max((unsigned)q->run_timeout, tm);
    }
    db.run_timeout = run_timeout;

    cur.Update();

    }}

    trans.Commit();
    exp_time = x_ComputeExpirationTime(time_run, run_timeout);


    {{
        CJobTimeLine& tl = *q->run_time_line;

        CWriteLockGuard guard(q->rtl_lock);
        tl.MoveObject(exp_time, curr + tm, job_id);
    }}

    if (q->monitor.IsMonitorActive()) {
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

        q->monitor.SendString(msg);
    }
}


void CQueue::ReturnJob(unsigned int job_id)
{
    _ASSERT(job_id);

    CRef<SLockedQueue> q(x_GetLQueue());

    CNetSchedule_JS_Guard js_guard(q->status_tracker, 
                                   job_id,
                                   CNetScheduleClient::eReturned);
    CNetScheduleClient::EJobStatus st = js_guard.GetOldStatus();
    // if canceled or already returned or done
    if (q->status_tracker.IsCancelCode(st) || 
        (st == CNetScheduleClient::eReturned) || 
        (st == CNetScheduleClient::eDone)) {
        js_guard.Commit();
        return;
    }
    {{
        SQueueDB& db = q->db;

        CFastMutexGuard guard(q->lock);
        db.SetTransaction(0);

        db.id = job_id;
        if (db.Fetch() == eBDB_Ok) {            
            CBDB_Transaction trans(*db.GetEnv(), 
                                CBDB_Transaction::eTransASync,
                                CBDB_Transaction::eNoAssociation);
            db.SetTransaction(&trans);

            // Pending status is not a bug here, returned and pending
            // has the same meaning, but returned jobs are getting delayed
            // for a little while (eReturned status)
            db.status = (int) CNetScheduleClient::ePending;
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

    if (q->monitor.IsMonitorActive()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::ReturnJob: job id=";
        msg += NStr::IntToString(job_id);
        q->monitor.SendString(msg);
    }
}

void CQueue::x_GetJobLB(unsigned int   worker_node,
                        unsigned int*  job_id, 
                        char*          input,
                        char*          jout,
                        char*          jerr,
                        unsigned*      job_mask)
{
    _ASSERT(worker_node && input);
    _ASSERT(job_mask);

    CRef<SLockedQueue> q(x_GetLQueue());

    unsigned get_attempts = 0;
    unsigned fetch_attempts = 0;
    const unsigned kMaxGetAttempts = 100;


    ++get_attempts;
    if (get_attempts > kMaxGetAttempts) {
        *job_id = 0;
        return;
    }

    *job_id = q->status_tracker.BorrowPendingJob();

    if (!*job_id) {
        return;
    }
    CNetSchedule_JS_BorrowGuard bguard(q->status_tracker, 
                                       *job_id,
                                       CNetScheduleClient::ePending);

    time_t curr = time(0);
    unsigned lb_stall_time = 0;
    ENSLB_RunDelayType stall_delay_type;


    //
    // Define current execution delay
    //

    {{
        CFastMutexGuard guard(q->lb_stall_time_lock);
        stall_delay_type = q->lb_stall_delay_type;
        lb_stall_time    = q->lb_stall_time;
    }}

    switch (stall_delay_type) {
    case eNSLB_Constant:
        break;
    case eNSLB_RunTimeAvg:
        {
            double total_run_time, run_count, lb_stall_time_mult;

            {{
            CFastMutexGuard guard(q->qstat_lock);
            if (q->qstat.run_count) {
                total_run_time = q->qstat.total_run_time;
                run_count = q->qstat.run_count;
                lb_stall_time_mult = q->lb_stall_time_mult;
            } else {
                // no statistics yet, nothing to do, take default queue value
                break;
            }
            }}

            double avg_time = total_run_time / run_count;
            lb_stall_time = (unsigned)(avg_time * lb_stall_time_mult);
        }
        break;
    default:
        _ASSERT(0);
    } // switch

    if (lb_stall_time == 0) {
        lb_stall_time = 6;
    }


fetch_db:
    ++fetch_attempts;
    if (fetch_attempts > kMaxGetAttempts) {
        LOG_POST(Error << "Failed to fetch the job record job_id=" << *job_id);
        *job_id = 0;
        return;
    }
 

    try {
        SQueueDB& db = q->db;
        CBDB_Transaction trans(*db.GetEnv(),
                            CBDB_Transaction::eTransASync,
                            CBDB_Transaction::eNoAssociation);
        {{
        CFastMutexGuard guard(q->lock);
        db.SetTransaction(&trans);

        CBDB_FileCursor& cur = *x_GetCursor(trans);
        CBDB_CursorGuard cg(cur);    

        cur.SetCondition(CBDB_FileCursor::eEQ);
        cur.From << *job_id;
        if (cur.Fetch() != eBDB_Ok) {
            if (fetch_attempts < kMaxGetAttempts) {
                goto fetch_db;
            }
            *job_id = 0;
            return;
        }
        CNetScheduleClient::EJobStatus status = 
            (CNetScheduleClient::EJobStatus)(int)db.status;

        // internal integrity check
        if (!(status == CNetScheduleClient::ePending ||
              status == CNetScheduleClient::eReturned)
            ) {
            if (q->status_tracker.IsCancelCode(
                (CNetScheduleClient::EJobStatus) status)) {
                // this job has been canceled while i'm fetching
                *job_id = 0;
                return;
            }
                ERR_POST(Error << "x_GetJobLB()::Status integrity violation "
                            << " job = " << *job_id
                            << " status = " << status
                            << " expected status = "
                            << (int)CNetScheduleClient::ePending);
            *job_id = 0;
            return;
        }

        const char* fld_str = db.input;
        ::strcpy(input, fld_str);
        if (jout) {
            fld_str = db.cout;
            ::strcpy(jout, fld_str);
        }
        if (jerr) {
            fld_str = db.cerr;
            ::strcpy(jerr, fld_str);
        }

        unsigned run_counter = db.run_counter;

        unsigned time_submit = db.time_submit;
        unsigned timeout = db.timeout;
        if (timeout == 0) {
            timeout = q->timeout;
        }
        *job_mask = db.mask;

        _ASSERT(timeout);

        // check if job already expired
        if (timeout && (time_submit + timeout < (unsigned)curr)) {
            *job_id = 0; 
            db.time_run = 0;
            db.time_done = curr;
            db.status = (int) CNetScheduleClient::eFailed;
            db.err_msg = "Job expired and cannot be scheduled.";

            bguard.ReturnToStatus(CNetScheduleClient::eFailed);

            if (q->monitor.IsMonitorActive()) {
                CTime tmp_t(CTime::eCurrent);
                string msg = tmp_t.AsString();
                msg += " CQueue::x_GetJobLB() timeout expired job id=";
                msg += NStr::IntToString(*job_id);
                q->monitor.SendString(msg);
            }

            cur.Update();
            trans.Commit();

            return;
        } 

        CNSLB_Coordinator::ENonLbHostsPolicy unk_host_policy =
            q->lb_coordinator->GetNonLBPolicy();

        // check if job must be scheduled immediatelly, 
        // because it's stalled for too long
        //
        unsigned time_lb_first_eval = db.time_lb_first_eval;
        unsigned stall_time = 0; 
        if (time_lb_first_eval) {
            stall_time = curr - time_lb_first_eval;
            if (lb_stall_time) {
                if (stall_time > lb_stall_time) {
                    // stalled for too long! schedule the job
                    if (unk_host_policy != CNSLB_Coordinator::eNonLB_Deny) {
                        goto grant_job;
                    }
                }
            } else {
                // exec delay not set, schedule the job
                goto grant_job;
            }
        } else { // first LB evaluation
            db.time_lb_first_eval = curr;
        }

        // job can be scheduled now, if load balancing situation is permitting
        {{
        CNSLB_Coordinator* coordinator = q->lb_coordinator;
        if (coordinator) {
            CNSLB_DecisionModule::EDecision lb_decision = 
                coordinator->Evaluate(worker_node, stall_time);
            switch (lb_decision) {
            case CNSLB_DecisionModule::eGrantJob:
                break;
            case CNSLB_DecisionModule::eDenyJob:
                *job_id = 0;
                break;
            case CNSLB_DecisionModule::eHostUnknown:
                if (unk_host_policy != CNSLB_Coordinator::eNonLB_Allow) {
                    *job_id = 0;
                }
                break;
            case CNSLB_DecisionModule::eNoLBInfo:
                break;
            case CNSLB_DecisionModule::eInsufficientInfo:
                break;
            default:
                _ASSERT(0);
            } // switch

            if (q->monitor.IsMonitorActive()) {
                CTime tmp_t(CTime::eCurrent);
                string msg = " CQueue::x_GetJobLB() LB decision = ";
                msg += CNSLB_DecisionModule::DecisionToStrint(lb_decision);
                msg += " job id = ";
                msg += NStr::IntToString(*job_id);
                msg += " worker_node=";
                msg += CSocketAPI::gethostbyaddr(worker_node);
                q->monitor.SendString(msg);
            }

        }
        }}
grant_job:
        // execution granted, update job record information
        if (*job_id) {
            db.status = (int) CNetScheduleClient::eRunning;
            db.time_run = curr;
            db.run_timeout = 0;
            db.run_counter = ++run_counter;

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

                bguard.ReturnToStatus(CNetScheduleClient::eFailed);
                ERR_POST(Error << "Too many run attempts. job=" << *job_id);
                db.status = (int) CNetScheduleClient::eFailed;
                db.err_msg = "Too many run attempts.";
                db.time_done = curr;
                db.run_counter = --run_counter;

                if (q->monitor.IsMonitorActive()) {
                    CTime tmp_t(CTime::eCurrent);
                    string msg = tmp_t.AsString();
                    msg += " CQueue::x_GetJobLB() Too many run attempts job id=";
                    msg += NStr::IntToString(*job_id);
                    q->monitor.SendString(msg);
                }

                *job_id = 0; 

            } // switch

        } // if (*job_id)

        cur.Update();

        }}
        trans.Commit();

    } 
    catch (exception&)
    {
        *job_id = 0;
        throw;
    }
    bguard.ReturnToStatus(CNetScheduleClient::eRunning);

    if (q->monitor.IsMonitorActive() && *job_id) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::x_GetJobLB() job id=";
        msg += NStr::IntToString(*job_id);
        msg += " worker_node=";
        msg += CSocketAPI::gethostbyaddr(worker_node);
        q->monitor.SendString(msg);
    }


    x_AddToTimeLine(*job_id, curr);
}


unsigned 
CQueue::x_FindPendingJob(const string&  client_name,
                       unsigned       client_addr)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    unsigned job_id = 0;

    bm::bvector<> blacklisted_jobs;

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
                    ai->candidate_jobs.optimize(0, bm::bvector<>::opt_free_0);
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
        bm::bvector<> assigned_aff;
        {{
            CFastMutexGuard aff_guard(q->aff_map_lock);
            q->worker_aff_map.GetAllAssignedAffinity(&assigned_aff);
        }}

        if (assigned_aff.any()) {
            // get all jobs belonging to other (already assigned) affinities,
            // ORing them with our own blacklisted jobs
            bm::bvector<> assigned_candidate_jobs(blacklisted_jobs);
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


void CQueue::GetJob(unsigned int   worker_node,
                    unsigned int*  job_id, 
                    char*          input,
                    char*          jout,
                    char*          jerr,
                    const string&  client_name,
                    unsigned*      job_mask)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    if (q->lb_flag) {
        x_GetJobLB(worker_node, job_id, input, jout, jerr, job_mask);
        return;
    }

    _ASSERT(worker_node && input);
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
        CBDB_Transaction trans(*db.GetEnv(), 
                               CBDB_Transaction::eTransASync,
                               CBDB_Transaction::eNoAssociation);


        {{
        CFastMutexGuard guard(q->lock);
        db.SetTransaction(&trans);

        CBDB_FileCursor& cur = *x_GetCursor(trans);
        CBDB_CursorGuard cg(cur); 

        upd_status = 
            x_UpdateDB_GetJobNoLock(db, curr, cur, trans, 
                                    worker_node, *job_id, input, jout, jerr, 
                                    &job_aff_id, job_mask);
        }}
        trans.Commit();

        switch (upd_status) {
        case eGetJobUpdate_JobFailed:
            q->status_tracker.ChangeStatus(*job_id, 
                                           CNetScheduleClient::eFailed);
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

        if (q->monitor.IsMonitorActive() && *job_id) {
            CTime tmp_t(CTime::eCurrent);
            string msg = tmp_t.AsString();
            msg += " CQueue::GetJob() job id=";
            msg += NStr::IntToString(*job_id);
            msg += " worker_node=";
            msg += CSocketAPI::gethostbyaddr(worker_node);
            q->monitor.SendString(msg);
        }

    } 
    catch (exception&)
    {
        q->status_tracker.ChangeStatus(*job_id, CNetScheduleClient::ePending);
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
    }

    x_AddToTimeLine(*job_id, curr);
}


CQueue::EGetJobUpdateStatus 
CQueue::x_UpdateDB_GetJobNoLock(SQueueDB&            db,
                                time_t               curr,
                                CBDB_FileCursor&     cur,
                                CBDB_Transaction&    trans,
                                unsigned int         worker_node,
                                unsigned             job_id,
                                char*                input,
                                char*                jout,
                                char*                jerr,
                                unsigned*            aff_id,
                                unsigned*            job_mask)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    const unsigned kMaxGetAttempts = 100;

    for (unsigned fetch_attempts = 0; fetch_attempts < kMaxGetAttempts;
         ++fetch_attempts) {

        cur.SetCondition(CBDB_FileCursor::eEQ);
        cur.From << job_id;

        if (cur.Fetch() != eBDB_Ok) {
            cur.Close();
            cur.ReOpen(&trans);
            continue;
        }
        int status = db.status;
        *aff_id = db.aff_id;

        // internal integrity check
        if (!(status == (int)CNetScheduleClient::ePending ||
              status == (int)CNetScheduleClient::eReturned)
            ) {
            if (q->status_tracker.IsCancelCode(
                (CNetScheduleClient::EJobStatus) status)) {
                // this job has been canceled while i'm fetching
                return eGetJobUpdate_JobStopped;
            }
            ERR_POST(Error 
                << "x_UpdateDB_GetJobNoLock::Status integrity violation "
                << " job = "     << job_id
                << " status = "  << status
                << " expected status = "
                << (int)CNetScheduleClient::ePending);
            return eGetJobUpdate_JobStopped;
        }

        const char* fld_str = db.input;
        ::strcpy(input, fld_str);
        if (jout) {
            strcpy(jout, (const char*) db.cout);
        }
        if (jerr) {
            strcpy(jerr, (const char*) db.cerr);
        }

        unsigned run_counter = db.run_counter;
        *job_mask = db.mask;

        db.status = (int) CNetScheduleClient::eRunning;
        db.time_run = curr;
        db.run_timeout = 0;
        db.run_counter = ++run_counter;

        unsigned time_submit = db.time_submit;
        unsigned timeout = db.timeout;
        if (timeout == 0) {
            timeout = q->timeout;
        }

        _ASSERT(timeout);
        // check if job already expired
        if (timeout && (time_submit + timeout < (unsigned)curr)) {
            db.time_run = 0;
            db.time_done = curr;
            db.run_counter = --run_counter;
            db.status = (int) CNetScheduleClient::eFailed;
            db.err_msg = "Job expired and cannot be scheduled.";
            q->status_tracker.ChangeStatus(job_id, 
                                           CNetScheduleClient::eFailed);

            cur.Update();

            if (q->monitor.IsMonitorActive()) {
                CTime tmp_t(CTime::eCurrent);
                string msg = tmp_t.AsString();
                msg += 
                 " CQueue::x_UpdateDB_GetJobNoLock() timeout expired job id=";
                msg += NStr::IntToString(job_id);
                q->monitor.SendString(msg);
            }
            return eGetJobUpdate_JobFailed;

        } else {  // job not expired
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
                                               CNetScheduleClient::eFailed);
                ERR_POST(Error << "Too many run attempts. job=" << job_id);
                db.status = (int) CNetScheduleClient::eFailed;
                db.err_msg = "Too many run attempts.";
                db.time_done = curr;
                db.run_counter = --run_counter;

                cur.Update();

                if (q->monitor.IsMonitorActive()) {
                    CTime tmp_t(CTime::eCurrent);
                    string msg = tmp_t.AsString();
                    msg += " CQueue::GetJob() Too many run attempts job id=";
                    msg += NStr::IntToString(job_id);
                    q->monitor.SendString(msg);
                }

                return eGetJobUpdate_JobFailed;
            } // switch

            // all checks passed successfully...
            cur.Update();
            return eGetJobUpdate_Ok;
        } // else
    } // for

    return eGetJobUpdate_NotFound;
}


void CQueue::GetJobKey(char* key_buf, unsigned job_id,
                       const string& host, unsigned port)
{
    sprintf(key_buf, NETSCHEDULE_JOBMASK, job_id, host.c_str(), port);
}

bool 
CQueue::GetJobDescr(unsigned int job_id,
                    int*         ret_code,
                    char*        input,
                    char*        output,
                    char*        err_msg,
                    char*        progress_msg,
                    char*        jout,
                    char*        jerr,
                    CNetScheduleClient::EJobStatus expected_status)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    SQueueDB& db = q->db;

    for (unsigned i = 0; i < 3; ++i) {
        {{
        CFastMutexGuard guard(q->lock);
        db.SetTransaction(0);

        db.id = job_id;
        if (db.Fetch() == eBDB_Ok) {
            if (expected_status != CNetScheduleClient::eJobNotFound) {
                CNetScheduleClient::EJobStatus status =
                    (CNetScheduleClient::EJobStatus)(int)db.status;
                if ((status != expected_status) 
                    // The 'Retuned' status does not get saved into db
                    // because it is temporary. (optimization).
                    // this condition reflects that logically Pending == Returned
                    && !(status ==  CNetScheduleClient::ePending 
                         && expected_status == CNetScheduleClient::eReturned)) {
                    goto wait_sleep;
                }
            }
            if (ret_code)
                *ret_code = db.ret_code;

            if (input) {
                ::strcpy(input, (const char* )db.input);
            }
            if (output) {
                ::strcpy(output, (const char* )db.output);
            }
            if (err_msg) {
                ::strcpy(err_msg, (const char* )db.err_msg);
            }
            if (progress_msg) {
                ::strcpy(progress_msg, (const char* )db.progress_msg);
            }
            if (jout) {
                ::strcpy(jout, (const char*) db.cout);
            }
            if (jerr) {
                ::strcpy(jerr, (const char*) db.cerr);
            }

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

CNetScheduleClient::EJobStatus 
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
    bm::bvector<> aff_jobs;
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


CBDB_FileCursor* CQueue::x_GetCursor(CBDB_Transaction& trans)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    CBDB_FileCursor* cur = q->cur.get();
    if (cur) { 
        cur->ReOpen(&trans);
        return cur;
    }
    cur = new CBDB_FileCursor(q->db, trans,
                              CBDB_FileCursor::eReadModifyUpdate);
    q->cur.reset(cur);
    return cur;
}


bool CQueue::CheckDelete(unsigned int job_id)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    SQueueDB& db = q->db;
    CBDB_Transaction trans(*db.GetEnv(),
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    time_t curr = time(0);
    unsigned time_done, time_submit;
    int job_ttl;

    {{
    CFastMutexGuard guard(q->lock);
    db.SetTransaction(&trans);

    CBDB_FileCursor& cur = *x_GetCursor(trans);
    CBDB_CursorGuard cg(cur);    
    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << job_id;

    if (cur.FetchFirst() != eBDB_Ok) {
        return true; // already deleted
    }

    int status = db.status;
/*
    if (! (
           (status == (int) CNetScheduleClient::eDone) ||
           (status == (int) CNetScheduleClient::eCanceled) ||
           (status == (int) CNetScheduleClient::eFailed) 
           )
        ) {
        return true;
    }
*/
    unsigned queue_ttl = q->timeout;
    job_ttl = db.timeout;
    if (job_ttl <= 0) {
        job_ttl = queue_ttl;
    }


    time_submit = db.time_submit;
    time_done = db.time_done;

    // pending jobs expire just as done jobs
    if (time_done == 0 && status == (int) CNetScheduleClient::ePending) {
        time_done = time_submit;
    }

    if (time_done == 0) { 
        if (time_submit + (job_ttl * 10) > (unsigned)curr) {
            return false;
        }
    } else {
        if (time_done + job_ttl > (unsigned)curr) {
            return false;
        }
    }

    x_DeleteDBRec(db, cur);

    }}
    trans.Commit();

    q->status_tracker.SetStatus(job_id, CNetScheduleClient::eJobNotFound);

    if (q->monitor.IsMonitorActive()) {
        CTime tm(CTime::eCurrent);
        string msg = tm.AsString();
        msg += " CQueue::CheckDelete: Job deleted id=";
        msg += NStr::IntToString(job_id);
        tm.SetTimeT(time_done);
        tm.ToLocalTime();
        msg += " time_done=";
        msg += tm.AsString();
        msg += " job_ttl(sec)=";
        msg += NStr::IntToString(job_ttl);
        msg += " job_ttl(minutes)=";
        msg += NStr::IntToString(job_ttl/60);

        q->monitor.SendString(msg);
    }

    return true;
}

void CQueue::x_DeleteDBRec(SQueueDB&  db, CBDB_FileCursor& cur)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    // dump the record
    {{
        CFastMutexGuard dguard(q->rec_dump_lock);
        if (q->rec_dump_flag) {
            x_PrintJobDbStat(db,
                             q->rec_dump,
                             ";",               // field separator
                             false              // no field names
                             );
        }
    }}

    cur.Delete(CBDB_File::eIgnoreError);
}


unsigned 
CQueue::CheckDeleteBatch(unsigned batch_size,
                         CNetScheduleClient::EJobStatus status,
                         unsigned &last_deleted)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    unsigned dcnt;
    for (dcnt = 0; dcnt < batch_size; ++dcnt) {
        unsigned job_id = q->status_tracker.GetFirst(status);
        if (job_id == 0) {
            break;
        }
        bool deleted = CheckDelete(job_id);
        if (!deleted) {
            break;
        }
        if (job_id > last_deleted) last_deleted = job_id;
    } // for
    return dcnt;
}


unsigned
CQueue::DeleteBatch(bm::bvector<>& batch)
{
    unsigned del_rec = 0;
    CRef<SLockedQueue> q(x_GetLQueue());
    SQueueDB& db = q->db;
    CBDB_Transaction trans(*db.GetEnv(), 
                        CBDB_Transaction::eTransASync,
                        CBDB_Transaction::eNoAssociation);

    {{
        CFastMutexGuard guard(q->lock);
        db.SetTransaction(&trans);

        CBDB_FileCursor& cur = *x_GetCursor(trans);
        CBDB_CursorGuard cg(cur);    
        for (bm::bvector<>::enumerator en = batch.first();
                en < batch.end();
                ++en) {
            unsigned job_id = *en;
            cur.SetCondition(CBDB_FileCursor::eEQ);
            cur.From << job_id;
            if (cur.FetchFirst() == eBDB_Ok) {
                cur.Delete(CBDB_File::eIgnoreError);
                ++del_rec;
            }
        }
    }}
    trans.Commit();
    // monitor this
    if (q->monitor.IsMonitorActive()) {
        CTime tm(CTime::eCurrent);
        string msg = tm.AsString();
        msg += " CQueue::DeleteBatch: " +
                NStr::IntToString(del_rec) + " job(s) deleted";
        q->monitor.SendString(msg);
    }
    return del_rec;
}


void CQueue::ClearAffinityIdx()
{
    // read the queue database, find the first physically available job_id,
    // then scan all index records, delete all the old (obsolete) record 
    // (job_id) references

    CRef<SLockedQueue> q(x_GetLQueue());
    SQueueDB& db = q->db;

    unsigned first_job_id = 0;
    {{
        CFastMutexGuard guard(q->lock);
        CBDB_Transaction trans(*db.GetEnv(), 
                            CBDB_Transaction::eTransASync,
                            CBDB_Transaction::eNoAssociation);

        db.SetTransaction(&trans);

        // get the first phisically available record in the queue database

        CBDB_FileCursor& cur = *x_GetCursor(trans);
        CBDB_CursorGuard cg(cur);
        cur.SetCondition(CBDB_FileCursor::eGE);
        cur.From << 0;

        if (cur.Fetch() == eBDB_Ok) {
            first_job_id = db.id;
        }
    }}

    if (first_job_id <= 1) {
        return;
    }


    bm::bvector<> bv(bm::BM_GAP);

    // get list of all affinity tokens in the index
    {{
        CFastMutexGuard guard(q->lock);
        CBDB_Transaction trans(*db.GetEnv(), 
                            CBDB_Transaction::eTransASync,
                            CBDB_Transaction::eNoAssociation);
        q->aff_idx.SetTransaction(&trans);
        CBDB_FileCursor cur(q->aff_idx);
        cur.SetCondition(CBDB_FileCursor::eGE);
        while (cur.Fetch() == eBDB_Ok) {
            unsigned aff_id = q->aff_idx.aff_id;
            bv.set(aff_id);
        }
    }}

    // clear all hanging references
    bm::bvector<>::enumerator en(bv.first());
    for(; en.valid(); ++en) {
        unsigned aff_id = *en;
        CFastMutexGuard guard(q->lock);
        CBDB_Transaction trans(*db.GetEnv(), 
                                CBDB_Transaction::eTransASync,
                                CBDB_Transaction::eNoAssociation);
        q->aff_idx.SetTransaction(&trans);
        SQueueAffinityIdx::TParent::TBitVector bvect(bm::BM_GAP);
        q->aff_idx.aff_id = aff_id;
        /*EBDB_ErrCode ret = */
            q->aff_idx.ReadVector(&bvect, bm::set_OR, NULL);
        unsigned old_count = bvect.count();
        bvect.set_range(0, first_job_id-1, false);
        unsigned new_count = bvect.count();
        if (new_count == old_count) {
            continue;
        }
        bvect.optimize();
        q->aff_idx.aff_id = aff_id;
        if (bvect.any()) {
            q->aff_idx.WriteVector(bvect, SQueueAffinityIdx::eNoCompact);
        } else {
            q->aff_idx.Delete();
        }
        trans.Commit();
    } // for
}


void CQueue::Truncate(void)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    CFastMutexGuard guard(m_Db.m_JobsToDeleteLock);
    q->status_tracker.ClearAll(&m_Db.m_JobsToDelete);
}


void CQueue::x_AddToTimeLine(unsigned job_id, time_t curr)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    if (job_id && q->run_time_line) {
        CJobTimeLine& tl = *q->run_time_line;
        time_t projected_time_done = curr + q->run_timeout;

        CWriteLockGuard guard(q->rtl_lock);
        tl.AddObject(projected_time_done, job_id);
    }
}


void CQueue::x_RemoveFromTimeLine(unsigned job_id)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    if (q->run_time_line) {
        CWriteLockGuard guard(q->rtl_lock);
        q->run_time_line->RemoveObject(job_id);
    }

    if (q->monitor.IsMonitorActive()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::RemoveFromTimeLine: job id=";
        msg += NStr::IntToString(job_id);
        q->monitor.SendString(msg);
    }
}


void 
CQueue::x_TimeLineExchange(unsigned remove_job_id, 
                           unsigned add_job_id,
                           time_t   curr)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    if (q->run_time_line) {
        CWriteLockGuard guard(q->rtl_lock);
        q->run_time_line->RemoveObject(remove_job_id);

        if (add_job_id) {
            CJobTimeLine& tl = *q->run_time_line;
            time_t projected_time_done = curr + q->run_timeout;
            tl.AddObject(projected_time_done, add_job_id);
        }
    }
    if (q->monitor.IsMonitorActive()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::TimeLineExchange: job removed=";
        msg += NStr::IntToString(remove_job_id);
        msg += " job added=";
        msg += NStr::IntToString(add_job_id);
        q->monitor.SendString(msg);
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

void CQueue::SetMonitorSocket(SOCK sock)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    auto_ptr<CSocket> s(new CSocket());
    s->Reset(sock, eTakeOwnership, eCopyTimeoutsFromSOCK);
    q->monitor.SetSocket(s.get());
    s.release();
}

CNetScheduleMonitor* CQueue::GetMonitor(void)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    return &q->monitor;
}


void CQueue::NotifyListeners(bool unconditional)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    unsigned short udp_port = m_Db.GetUdpPort();
    if (udp_port == 0) {
        return;
    }

    SLockedQueue::TListenerList& wnodes = q->wnodes;

    time_t curr = time(0);

    if (!unconditional &&
        (q->notif_timeout == 0 ||
         !q->status_tracker.AnyPending())) {
        return;
    }

    SLockedQueue::TListenerList::size_type lst_size;
    {{
        CWriteLockGuard guard(q->wn_lock);
        lst_size = wnodes.size();
        if ((lst_size == 0) ||
            (!unconditional && q->last_notif + q->notif_timeout > curr)) {
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
        SQueueListener* ql;
        {{
            CReadLockGuard guard(q->wn_lock);
            ql = wnodes[i];
            if (ql->last_connect + ql->timeout < curr) {
                continue;
            }
        }}
        host = ql->host;
        port = ql->udp_port;

        if (port) {
            CFastMutexGuard guard(q->us_lock);
            //EIO_Status status = 
                q->udp_socket.Send(msg, msg_len, 
                                   CSocketAPI::ntoa(host), port);
        }
        // periodically check if we have no more jobs left
        if ((i % 10 == 0) &&
            !q->status_tracker.AnyPending()) {
            break;
        }

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
    unsigned curr_slot;
    {{
        CReadLockGuard guard(q->rtl_lock);
        curr_slot = tl.TimeLineSlot(curr);
    }}
    if (curr_slot == 0) {
        return;
    }
    --curr_slot;

    CJobTimeLine::TObjVector bv;
    {{
        CReadLockGuard guard(q->rtl_lock);
        tl.EnumerateObjects(&bv, curr_slot);
    }}
    CJobTimeLine::TObjVector::enumerator en(bv.first());
    for ( ;en.valid(); ++en) {
        unsigned job_id = *en;
        unsigned exp_time = CheckExecutionTimeout(job_id, curr);

        // job may need to moved in the timeline to some future slot
        
        if (exp_time) {
            CWriteLockGuard guard(q->rtl_lock);
            unsigned job_slot = tl.TimeLineSlot(exp_time);
            while (job_slot <= curr_slot) {
                ++job_slot;
            }
            tl.AddObjectToSlot(job_slot, job_id);
        }
    } // for

    CWriteLockGuard guard(q->rtl_lock);
    tl.HeadTruncate(curr_slot);
}

time_t CQueue::CheckExecutionTimeout(unsigned job_id, time_t curr_time)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    SQueueDB& db = q->db;

    CNetScheduleClient::EJobStatus status = GetStatus(job_id);

    if (status != CNetScheduleClient::eRunning) {
        return 0;
    }

    CBDB_Transaction trans(*db.GetEnv(), 
                        CBDB_Transaction::eTransASync,
                        CBDB_Transaction::eNoAssociation);

    // TODO: get current job status from the status index
    unsigned time_run, run_timeout;
    time_t   exp_time;
    {{
    CFastMutexGuard guard(q->lock);
    db.SetTransaction(&trans);

    CBDB_FileCursor& cur = *x_GetCursor(trans);
    CBDB_CursorGuard cg(cur);

    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << job_id;
    if (cur.Fetch() != eBDB_Ok) {
        return 0;
    }
    int status = db.status;
    if (status != (int)CNetScheduleClient::eRunning) {
        return 0;
    }

    time_run = db.time_run;
    _ASSERT(time_run);
    run_timeout = db.run_timeout;

    exp_time = x_ComputeExpirationTime(time_run, run_timeout);
    if (curr_time < exp_time) { 
        return exp_time;
    }
    db.status = (int) CNetScheduleClient::ePending;
    db.time_done = 0;

    cur.Update();

    }}

    trans.Commit();

    q->status_tracker.SetStatus(job_id, CNetScheduleClient::eReturned);

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

        if (q->monitor.IsMonitorActive()) {
            q->monitor.SendString(msg);
        }
    }}

    return 0;
}

time_t 
CQueue::x_ComputeExpirationTime(unsigned time_run, unsigned run_timeout) const
{
    if (run_timeout == 0) {
        run_timeout = x_GetLQueue()->run_timeout;
    }
    if (run_timeout == 0) {
        return 0;
    }
    time_t exp_time = time_run + run_timeout;
    return exp_time;
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

void CQueue::x_AddToAffIdx_NoLock(const vector<SNS_BatchSubmitRec>& batch)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    typedef SQueueAffinityIdx::TParent::TBitVector TBVector;
    typedef map<unsigned, TBVector*>               TBVMap;
    
    TBVMap  bv_map;
    try {

        unsigned bsize = batch.size();
        for (unsigned i = 0; i < bsize; ++i) {
            const SNS_BatchSubmitRec& bsub = batch[i];
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
CQueue::x_ReadAffIdx_NoLock(const bm::bvector<>& aff_id_set,
                            bm::bvector<>*       job_candidates)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    q->aff_idx.SetTransaction(0);
    bm::bvector<>::enumerator en(aff_id_set.first());
    for(; en.valid(); ++en) {  // for each affinity id
        unsigned aff_id = *en;
        x_ReadAffIdx_NoLock(aff_id, job_candidates);
    }
}


void 
CQueue::x_ReadAffIdx_NoLock(unsigned       aff_id,
                            bm::bvector<>* job_candidates)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    q->aff_idx.aff_id = aff_id;
    q->aff_idx.SetTransaction(0);
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

/*
 * ===========================================================================
 * $Log$
 * Revision 1.105  2007/01/08 21:04:09  joukovv
 * Fast notification of idle node cluster implemented.
 *
 * Revision 1.104  2007/01/05 23:02:45  joukovv
 * Circular deletion with engaged lock in weak ref code fixed. Attempt to
 * enchance server reaction for new tasks.
 *
 * Revision 1.103  2007/01/02 18:50:54  joukovv
 * Queue deletion implemented (does not delete actual database files - need a
 * method of enumerating them). Draft implementation of weak reference. Minor
 * corrections.
 *
 * Revision 1.102  2006/12/13 17:18:10  didenko
 * reduced run frequency for the job notification thread
 *
 * Revision 1.101  2006/12/07 22:58:10  joukovv
 * comment and kind added to queue database
 *
 * Revision 1.100  2006/12/07 19:28:48  joukovv
 * Build errors fixed, queue info command introduced.
 *
 * Revision 1.99  2006/12/07 16:22:10  joukovv
 * Transparent server-to-client exception report implemented. Version control
 * bug fixed.
 *
 * Revision 1.98  2006/12/04 23:31:30  joukovv
 * Access control/version control checks corrected.
 *
 * Revision 1.97  2006/12/01 00:10:58  joukovv
 * Dynamic queue creation implemented.
 *
 * Revision 1.96  2006/11/28 18:03:49  joukovv
 * MSVC8 build fix, grid_worker_sample idle task commented out.
 *
 * Revision 1.95  2006/11/27 16:46:21  joukovv
 * Iterator to CQueueCollection introduced to decouple it with CQueueDataBase;
 * un-nested CQueue from CQueueDataBase; instrumented code to count job
 * throughput average.
 *
 * Revision 1.94  2006/11/13 22:49:15  joukovv
 * Background job deletion code corrected.
 *
 * Revision 1.93  2006/11/13 19:15:35  joukovv
 * Protocol parser re-implemented. Remnants of ThreadData removed.
 *
 * Revision 1.92  2006/10/31 19:35:26  joukovv
 * Queue creation and reading of its parameters decoupled. Code reorganized to
 * reduce coupling in general. Preparing for queue-on-demand.
 *
 * Revision 1.91  2006/10/19 20:38:20  joukovv
 * Works in thread-per-request mode. Errors in BDB layer fixed.
 *
 * Revision 1.90  2006/10/03 14:56:56  joukovv
 * Delayed job deletion implemented, code restructured preparing to move to
 * thread-per-request model.
 *
 * Revision 1.89  2006/09/21 21:28:59  joukovv
 * Consistency of memory state and database strengthened, ability to retry failed
 * jobs on different nodes (and corresponding queue parameter, failed_retries)
 * added, overall code regularization performed.
 *
 * Revision 1.88  2006/08/28 19:14:45  didenko
 * Fixed a bug in GetJobDescr logic
 *
 * Revision 1.87  2006/06/29 21:09:33  kuznets
 * Added queue dump by status(pending, running, etc)
 *
 * Revision 1.86  2006/06/27 15:39:42  kuznets
 * Added int mask to jobs to carry flags(like exclusive)
 *
 * Revision 1.85  2006/06/26 13:51:21  kuznets
 * minor cleaning
 *
 * Revision 1.84  2006/06/26 13:46:01  kuznets
 * Fixed job expiration and restart mechanism
 *
 * Revision 1.83  2006/06/19 16:15:49  kuznets
 * fixed crash when working with affinity
 *
 * Revision 1.82  2006/06/07 13:00:01  kuznets
 * Implemented command to get status summary based on affinity token
 *
 * Revision 1.81  2006/05/22 16:51:04  kuznets
 * Fixed bug in reporting worker nodes
 *
 * Revision 1.80  2006/05/22 15:19:40  kuznets
 * Added return code to failure reporting
 *
 * Revision 1.79  2006/05/22 12:36:33  kuznets
 * Added output argument to PutFailure
 *
 * Revision 1.78  2006/05/11 14:31:51  kuznets
 * Fixed bug in job prolongation
 *
 * Revision 1.77  2006/05/10 15:59:06  kuznets
 * Implemented NS call to delay job expiration
 *
 * Revision 1.76  2006/05/08 11:24:52  kuznets
 * Implemented file redirection cout/cerr for worker nodes
 *
 * Revision 1.75  2006/05/04 15:36:03  kuznets
 * Fixed bug in deleting done jobs
 *
 * Revision 1.74  2006/05/03 15:18:32  kuznets
 * Fixed deletion of done jobs
 *
 * Revision 1.73  2006/04/17 15:46:54  kuznets
 * Added option to remove job when it is done (similar to LSF)
 *
 * Revision 1.72  2006/04/14 12:43:28  kuznets
 * Fixed crash when deleting affinity records
 *
 * Revision 1.71  2006/03/30 20:54:23  kuznets
 * Improved handling of BDB resource conflicts
 *
 * Revision 1.70  2006/03/30 17:38:55  kuznets
 * Set max. transactions according to number of active threads
 *
 * Revision 1.69  2006/03/30 16:12:40  didenko
 * Increased the max_dead_locks number
 *
 * Revision 1.68  2006/03/29 17:39:42  kuznets
 * Turn off reverse splitting for main queue file to reduce collisions
 *
 * Revision 1.67  2006/03/28 21:41:17  kuznets
 * Use default page size.Hope smaller p-size reduce collisions
 *
 * Revision 1.66  2006/03/28 21:21:22  kuznets
 * cleaned up comments
 *
 * Revision 1.65  2006/03/28 20:37:56  kuznets
 * Trying to work around deadlock by repeating transaction
 *
 * Revision 1.64  2006/03/28 19:35:17  kuznets
 * Commented out use of local database to fix dead lock
 *
 * Revision 1.63  2006/03/17 14:40:25  kuznets
 * fixed warning
 *
 * Revision 1.62  2006/03/17 14:25:29  kuznets
 * Force reschedule (to re-try failed jobs)
 *
 * Revision 1.61  2006/03/16 19:37:28  kuznets
 * Fixed possible race condition between client and worker
 *
 * Revision 1.60  2006/03/13 16:01:36  kuznets
 * Fixed queue truncation (transaction log overflow). Added commands to print queue selectively
 *
 * Revision 1.59  2006/02/23 20:05:10  kuznets
 * Added grid client registration-unregistration
 *
 * Revision 1.58  2006/02/23 15:45:04  kuznets
 * Added more frequent and non-intrusive memory optimization of status matrix
 *
 * Revision 1.57  2006/02/21 14:44:57  kuznets
 * Bug fixes, improvements in statistics
 *
 * Revision 1.56  2006/02/09 17:07:42  kuznets
 * Various improvements in job scheduling with respect to affinity
 *
 * Revision 1.55  2006/02/08 15:17:33  kuznets
 * Tuning and bug fixing of job affinity
 *
 * Revision 1.54  2006/02/06 14:10:29  kuznets
 * Added job affinity
 *
 * Revision 1.53  2005/11/01 15:16:14  kuznets
 * Cleaned unused variables
 *
 * Revision 1.52  2005/09/20 14:05:25  kuznets
 * Minor bug fix
 *
 * Revision 1.51  2005/08/30 14:19:33  kuznets
 * Added thread-local database for better scalability
 *
 * Revision 1.50  2005/08/29 12:10:18  kuznets
 * Removed dead code
 *
 * Revision 1.49  2005/08/26 12:36:10  kuznets
 * Performance optimization
 *
 * Revision 1.48  2005/08/25 16:35:01  kuznets
 * Fixed bug (uncommited transaction)
 *
 * Revision 1.47  2005/08/24 18:17:26  kuznets
 * Reduced priority of notifiction thread, increased Tas spins
 *
 * Revision 1.46  2005/08/22 14:01:58  kuznets
 * Added JobExchange command
 *
 * Revision 1.45  2005/08/15 13:29:46  kuznets
 * Implemented batch job submission
 *
 * Revision 1.44  2005/07/25 16:14:31  kuznets
 * Revisited LB parameters, added options to compute job stall delay as fraction of AVG runtime
 *
 * Revision 1.43  2005/07/21 15:41:02  kuznets
 * Added monitoring for LB info
 *
 * Revision 1.42  2005/07/21 12:39:27  kuznets
 * Improved load balancing module
 *
 * Revision 1.41  2005/07/14 13:40:07  kuznets
 * compilation bug fixes
 *
 * Revision 1.40  2005/07/14 13:12:56  kuznets
 * Added load balancer
 *
 * Revision 1.39  2005/06/22 15:36:09  kuznets
 * Minor tweaks in record print formatting
 *
 * Revision 1.38  2005/06/21 16:00:22  kuznets
 * Added archival dump of all deleted records
 *
 * Revision 1.37  2005/06/20 15:49:43  kuznets
 * Node statistics: do not show obsolete connections
 *
 * Revision 1.36  2005/06/20 13:31:08  kuznets
 * Added access control for job submitters and worker nodes
 *
 * Revision 1.35  2005/05/12 18:37:33  kuznets
 * Implemented config reload
 *
 * Revision 1.34  2005/05/06 13:07:32  kuznets
 * Fixed bug in cleaning database
 *
 * Revision 1.33  2005/05/05 16:52:41  kuznets
 * Added error messages when job expires
 *
 * Revision 1.32  2005/05/05 16:07:15  kuznets
 * Added individual job dumping
 *
 * Revision 1.31  2005/05/04 19:09:43  kuznets
 * Added queue dumping
 *
 * Revision 1.30  2005/05/02 14:44:40  kuznets
 * Implemented remote monitoring
 *
 * Revision 1.29  2005/04/28 17:40:26  kuznets
 * Added functions to rack down forgotten nodes
 *
 * Revision 1.28  2005/04/25 14:42:53  kuznets
 * Fixed bug in GetJob()
 *
 * Revision 1.27  2005/04/21 13:37:35  kuznets
 * Fixed race condition between Submit job and Get job
 *
 * Revision 1.26  2005/04/20 15:59:33  kuznets
 * Progress message to Submit
 *
 * Revision 1.25  2005/04/19 19:34:05  kuznets
 * Adde progress report messages
 *
 * Revision 1.24  2005/04/11 13:53:25  kuznets
 * Code cleanup
 *
 * Revision 1.23  2005/04/06 12:39:55  kuznets
 * + client version control
 *
 * Revision 1.22  2005/03/29 16:48:28  kuznets
 * Use atomic counter for job id assignment
 *
 * Revision 1.21  2005/03/22 19:02:54  kuznets
 * Reflecting chnages in connect layout
 *
 * Revision 1.20  2005/03/22 16:14:49  kuznets
 * +PrintStat()
 *
 * Revision 1.19  2005/03/21 13:07:28  kuznets
 * Added some statistical functions
 *
 * Revision 1.18  2005/03/17 20:37:07  kuznets
 * Implemented FPUT
 *
 * Revision 1.17  2005/03/17 16:04:38  kuznets
 * Get job algorithm improved to re-get if job expired
 *
 * ===========================================================================
 */
