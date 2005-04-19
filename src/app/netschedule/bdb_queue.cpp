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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description: Network scheduler job status database.
 *
 */
#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_limits.h>
#include <corelib/version.hpp>
#include <connect/services/netschedule_client.hpp>
#include <connect/ncbi_socket.hpp>

#include <db.h>
#include <bdb/bdb_trans.hpp>
#include <bdb/bdb_cursor.hpp>

#include "bdb_queue.hpp"

BEGIN_NCBI_SCOPE

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

/// @internal
class CCursorGuard
{
public:
    CCursorGuard(CBDB_FileCursor& cur) : m_Cur(cur) {}
    ~CCursorGuard() { m_Cur.Close(); }
private:
    CCursorGuard(CCursorGuard&);
    CCursorGuard& operator=(const CCursorGuard&);
private:
    CBDB_FileCursor& m_Cur;
};



CQueueCollection::CQueueCollection()
{}

CQueueCollection::~CQueueCollection()
{
    NON_CONST_ITERATE(TQueueMap, it, m_QMap) {
        SLockedQueue* q = it->second;
        delete q;
    }
}

void CQueueCollection::Close()
{
    NON_CONST_ITERATE(TQueueMap, it, m_QMap) {
        SLockedQueue* q = it->second;
        delete q;
    }
    m_QMap.clear();

}

SLockedQueue& CQueueCollection::GetLockedQueue(const string& name)
{
    TQueueMap::iterator it = m_QMap.find(name);
    if (it == m_QMap.end()) {
        string msg = "Job queue not found: ";
        msg += name;
        NCBI_THROW(CNetScheduleException, eUnknownQueue, msg);
    }
    return *(it->second);
}


bool CQueueCollection::QueueExists(const string& name) const
{
    TQueueMap::const_iterator it = m_QMap.find(name);
    return (it != m_QMap.end());
}

void CQueueCollection::AddQueue(const string& name, SLockedQueue* queue)
{
    m_QMap[name] = queue;
}






CQueueDataBase::CQueueDataBase()
: m_Env(0),
//  m_MaxId(0),
  m_StopPurge(false),
  m_PurgeLastId(0),
  m_PurgeSkipCnt(0),
  m_DeleteChkPointCnt(0),
  m_FreeStatusMemCnt(0),
  m_LastR2P(time(0)),
  m_UdpPort(0)
{
    m_IdCounter.Set(0);
}

CQueueDataBase::~CQueueDataBase()
{
    try {
        Close();
        delete m_Env; // paranoiya check
    } catch (exception& )
    {}
}

void CQueueDataBase::Open(const string& path, unsigned cache_ram_size)
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


    m_Name = "jsqueue";
    string err_file = m_Path + "err" + string(m_Name) + ".log";
    m_Env->OpenErrFile(err_file.c_str());
    m_Env->SetLogFileMax(200 * 1024 * 1024);

    // Check if bdb env. files are in place and try to join
    CDir dir(m_Path);
    CDir::TEntries fl = dir.GetEntries("__db.*", CDir::eIgnoreRecursive);

    if (fl.empty()) {
        if (cache_ram_size) {
            m_Env->SetCacheSize(cache_ram_size);
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

    if (m_Env->IsTransactional()) {
        m_Env->SetTransactionTimeout(10 * 1000000); // 10 sec
        m_Env->TransactionCheckpoint();
    }
}

void CQueueDataBase::MountQueue(const string& queue_name, 
                                int           timeout,
                                int           notif_timeout,
                                int           run_timeout,
                                int           run_timeout_precision,
                                const string& program_name)
{
    _ASSERT(m_Env);

    if (m_QueueCollection.QueueExists(queue_name)) {
        LOG_POST(Warning << 
                 "JS: Queue " << queue_name << " already exists.");
        return;
    }

    auto_ptr<SLockedQueue> q(new SLockedQueue(queue_name));
    q->db.SetEnv(*m_Env);
    string fname = string("jsq_") + queue_name + string(".db");
    q->db.Open(fname.c_str(), CBDB_RawFile::eReadWriteCreate);

    q->timeout = timeout;
    q->notif_timeout = notif_timeout;
    q->last_notif = time(0);

    m_QueueCollection.AddQueue(queue_name, q.release());

    SLockedQueue& queue = m_QueueCollection.GetLockedQueue(queue_name);

    queue.run_timeout = run_timeout;
    if (run_timeout) {
        queue.run_time_line = new CJobTimeLine(run_timeout_precision, 0);
    }


    // scan the queue, restore the state machine

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
    } // while


    queue.udp_socket.SetReuseAddress(eOn);
    unsigned short udp_port = GetUdpPort();
    if (udp_port) {
        queue.udp_socket.Bind(udp_port);
    }

    // program version control
    if (!program_name.empty()) {
        list<string> programs;
        NStr::Split(program_name, ";,", programs);
        CQueueClientInfo program_info;
        ITERATE(list<string>, it, programs) {
            const string& vstr = *it;
            try {
                ParseVersionString(vstr, 
                    &program_info.client_name, &program_info.version_info);
                NStr::TruncateSpacesInPlace(program_info.client_name);
                queue.program_version_list.AddClientInfo(program_info);
            } 
            catch (CStringException&) {
                LOG_POST(Error << "Program string '" << vstr << "'" 
                               << " cannot be parsed and ignored.");
            }

        }
    }

    LOG_POST(Info << "Queue records = " << recs);
    
}

void CQueueDataBase::Close()
{
    StopNotifThread();
    StopPurgeThread();
    StopExecutionWatcherThread();

    m_QueueCollection.Close();
    try {
        if (m_Env) {
            m_Env->TransactionCheckpoint();
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

void CQueueDataBase::NotifyListeners()
{
    const CQueueCollection::TQueueMap& qm = m_QueueCollection.GetMap();
    ITERATE(CQueueCollection::TQueueMap, it, qm) {
        const string qname = it->first;
        CQueue jq(*this, qname);

        jq.NotifyListeners();
    }
}

void CQueueDataBase::CheckExecutionTimeout()
{
    const CQueueCollection::TQueueMap& qm = m_QueueCollection.GetMap();
    ITERATE(CQueueCollection::TQueueMap, it, qm) {
        const string qname = it->first;
        CQueue jq(*this, qname);

        jq.CheckExecutionTimeout();
    }    
}

void CQueueDataBase::Purge()
{
    unsigned curr_id = m_IdCounter.Get();

    // Re-submit returned jobs 
    {{

    const int kR2P_delay = 7; // re-submission delay
    time_t curr = time(0);

    if (m_LastR2P + kR2P_delay <= curr) {
        m_LastR2P = curr;
        const CQueueCollection::TQueueMap& qm = m_QueueCollection.GetMap();
        ITERATE(CQueueCollection::TQueueMap, it, qm) {
            const string qname = it->first;
            CQueue jq(*this, qname);

            jq.Return2Pending();
        }
    }
    }}


     // check not to rescan the database too often 
     // when we have low client activity

    if (m_PurgeLastId + 3000 > curr_id) {
        ++m_PurgeSkipCnt;
        
        // probably nothing to do yet, skip purge execution

        if (m_PurgeSkipCnt < 10) { // only 10 skips in a row
            return;
        }

        m_PurgeSkipCnt = 0;        
    }


    // Delete obsolete job records, based on time stamps 
    // and expiration timeouts

    unsigned global_del_rec = 0;

    const CQueueCollection::TQueueMap& qm = m_QueueCollection.GetMap();
    ITERATE(CQueueCollection::TQueueMap, it, qm) {
        const string qname = it->first;
        CQueue jq(*this, qname);

        unsigned del_rec, total_del_rec = 0;
        const unsigned batch_size = 100;
        do {
            // ----------------------------------------------
            // Clean failed jobs

            del_rec = jq.CheckDeleteBatch(batch_size, 
                                          CNetScheduleClient::eFailed);
            total_del_rec += del_rec;
            global_del_rec += del_rec;
            m_PurgeLastId += del_rec;

            // ----------------------------------------------
            // Clean canceled jobs

            del_rec = jq.CheckDeleteBatch(batch_size, 
                                          CNetScheduleClient::eCanceled);
            total_del_rec += del_rec;
            global_del_rec += del_rec;
            m_PurgeLastId += del_rec;

            // ----------------------------------------------
            // Clean finished jobs

            del_rec = jq.CheckDeleteBatch(batch_size, 
                                          CNetScheduleClient::eDone);
            total_del_rec += del_rec;
            global_del_rec += del_rec;
            m_PurgeLastId += del_rec;

            // do not delete more than certain number of 
            // records from the queue in one Purge
            if (total_del_rec >= 1000) {
                SleepMilliSec(1000);
                break;
            }


            {{
                CFastMutexGuard guard(m_PurgeLock);
                if (m_StopPurge) {
                    m_StopPurge = false;
                    return;
                }
            }}

        } while (del_rec == batch_size);

        m_DeleteChkPointCnt += total_del_rec;
        if (m_DeleteChkPointCnt > 1000) {
            m_DeleteChkPointCnt = 0;
            m_Env->TransactionCheckpoint();
        }

    } // ITERATE

    m_FreeStatusMemCnt += global_del_rec;
    if (m_FreeStatusMemCnt > 1000000) {
        m_FreeStatusMemCnt = 0;

        const CQueueCollection::TQueueMap& qm = m_QueueCollection.GetMap();
        ITERATE(CQueueCollection::TQueueMap, it, qm) {
            const string qname = it->first;
            CQueue jq(*this, qname);
            jq.FreeUnusedMem();
            {{
                CFastMutexGuard guard(m_PurgeLock);
                if (m_StopPurge) {
                    m_StopPurge = false;
                    return;
                }
            }}
        } // ITERATE
    }

}

void CQueueDataBase::StopPurge()
{
    CFastMutexGuard guard(m_PurgeLock);
    m_StopPurge = true;
}

void CQueueDataBase::RunPurgeThread()
{
# ifdef NCBI_THREADS
       LOG_POST(Info << "Starting guard and cleaning thread.");
       m_PurgeThread.Reset(
           new CJobQueueCleanerThread(*this, 15, 5));
       m_PurgeThread->Run();
# else
        LOG_POST(Warning << 
                 "Cannot run background thread in non-MT configuration.");
# endif

}


void CQueueDataBase::StopPurgeThread()
{
# ifdef NCBI_THREADS
    if (!m_PurgeThread.Empty()) {
        LOG_POST(Info << "Stopping guard and cleaning thread...");
        StopPurge();
        m_PurgeThread->RequestStop();
        m_PurgeThread->Join();
        LOG_POST(Info << "Stopped.");
    }
# endif
}

void CQueueDataBase::RunNotifThread()
{
    if (GetUdpPort() == 0) {
        return;
    }

# ifdef NCBI_THREADS
       LOG_POST(Info << "Starting client notification thread.");
       m_NotifThread.Reset(
           new CJobNotificationThread(*this, 2, 2));
       m_NotifThread->Run();
# else
        LOG_POST(Warning << 
                 "Cannot run background thread in non-MT configuration.");
# endif

}

void CQueueDataBase::StopNotifThread()
{
# ifdef NCBI_THREADS
    if (!m_NotifThread.Empty()) {
        LOG_POST(Info << "Stopping notification thread...");
        m_NotifThread->RequestStop();
        m_NotifThread->Join();
        LOG_POST(Info << "Stopped.");
    }
# endif
}

void CQueueDataBase::RunExecutionWatcherThread(unsigned run_delay)
{
# ifdef NCBI_THREADS
       LOG_POST(Info << "Starting execution watcher thread.");
       m_ExeWatchThread.Reset(
           new CJobQueueExecutionWatcherThread(*this, run_delay, 2));
       m_ExeWatchThread->Run();
# else
        LOG_POST(Warning << 
                 "Cannot run background thread in non-MT configuration.");
# endif

}

void CQueueDataBase::StopExecutionWatcherThread()
{
# ifdef NCBI_THREADS
    if (!m_ExeWatchThread.Empty()) {
        LOG_POST(Info << "Stopping execution watch thread...");
        m_ExeWatchThread->RequestStop();
        m_ExeWatchThread->Join();
        LOG_POST(Info << "Stopped.");
    }
# endif
}






CQueueDataBase::CQueue::CQueue(CQueueDataBase& db, const string& queue_name)
: m_Db(db),
  m_LQueue(db.m_QueueCollection.GetLockedQueue(queue_name))
{
}

unsigned CQueueDataBase::CQueue::CountRecs()
{
    SQueueDB& db = m_LQueue.db;
    CFastMutexGuard guard(m_LQueue.lock);
	db.SetTransaction(0);
    return db.CountRecs();
}

void CQueueDataBase::CQueue::PrintStat(CNcbiOstream & out)
{
    SQueueDB& db = m_LQueue.db;
    CFastMutexGuard guard(m_LQueue.lock);
	db.SetTransaction(0);
    db.PrintStat(out);
}


unsigned int 
CQueueDataBase::CQueue::Submit(const string& input,
                               unsigned      host_addr,
                               unsigned      port,
                               unsigned      wait_timeout)
{
    unsigned int job_id = m_Db.GetNextId();

//    CIdBusyGuard id_guard(&m_Db.m_UsedIds, job_id, 3);

    CNetSchedule_JS_Guard js_guard(m_LQueue.status_tracker, 
                                   job_id,
                                   CNetScheduleClient::ePending);
    SQueueDB& db = m_LQueue.db;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    {{
    CFastMutexGuard guard(m_LQueue.lock);
	db.SetTransaction(&trans);

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

    db.input = input;
    db.output = "";

    db.err_msg = "";

    db.cout = "";
    db.cerr = "";

    db.Insert();
    }}

    trans.Commit();

    js_guard.Release();

    return job_id;
}

unsigned 
CQueueDataBase::CQueue::CountStatus(CNetScheduleClient::EJobStatus st) const
{
    return m_LQueue.status_tracker.CountStatus(st);
}

void CQueueDataBase::CQueue::Cancel(unsigned int job_id)
{
//    CIdBusyGuard id_guard(&m_Db.m_UsedIds, job_id, 3);

    CNetSchedule_JS_Guard js_guard(m_LQueue.status_tracker, 
                                   job_id,
                                   CNetScheduleClient::eCanceled);
    CNetScheduleClient::EJobStatus st = js_guard.GetOldStatus();
    if (m_LQueue.status_tracker.IsCancelCode(st)) {
        js_guard.Release();
        return;
    }

    {{
    SQueueDB& db = m_LQueue.db;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    CFastMutexGuard guard(m_LQueue.lock);
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
    js_guard.Release();

    RemoveFromTimeLine(job_id);
}

void CQueueDataBase::CQueue::DropJob(unsigned job_id)
{
    SQueueDB& db = m_LQueue.db;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    m_LQueue.status_tracker.SetStatus(job_id, 
                                      CNetScheduleClient::eJobNotFound);

    CFastMutexGuard guard(m_LQueue.lock);
    db.SetTransaction(&trans);
    db.id = job_id;
    db.Delete(CBDB_File::eIgnoreError);
    trans.Commit();
}

void CQueueDataBase::CQueue::PutResult(unsigned int  job_id,
                                       int           ret_code,
                                       const char*   output)
{
//    CIdBusyGuard id_guard(&m_Db.m_UsedIds, job_id, 3);

    CNetSchedule_JS_Guard js_guard(m_LQueue.status_tracker, 
                                   job_id,
                                   CNetScheduleClient::eDone);
    CNetScheduleClient::EJobStatus st = js_guard.GetOldStatus();
    if (m_LQueue.status_tracker.IsCancelCode(st)) {
        js_guard.Release();
        return;
    }

    SQueueDB& db = m_LQueue.db;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    unsigned subm_addr, subm_port, subm_timeout, time_submit;
    time_t curr = time(0);

    {{
    CFastMutexGuard guard(m_LQueue.lock);
    db.SetTransaction(&trans);

    CBDB_FileCursor& cur = *GetCursor(trans);
    CCursorGuard cg(cur);    

    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << job_id;

    if (cur.FetchFirst() != eBDB_Ok) {
        // TODO: Integrity error or job just expired?
        return;
    }
    
    time_submit = db.time_submit;
    subm_addr = db.subm_addr;
    subm_port = db.subm_port;
    subm_timeout = db.subm_timeout;

    db.status = (int) CNetScheduleClient::eDone;
    db.ret_code = ret_code;
    db.output = output;
    db.time_done = curr;

    cur.Update();

    }}

    trans.Commit();
    js_guard.Release();

    RemoveFromTimeLine(job_id);

    // check if we need to send a UDP notification

    if ( subm_addr && subm_timeout &&
        (time_submit + subm_timeout >= (unsigned)curr)) {

        char msg[1024];
        sprintf(msg, "JNTF %u", job_id);

        CFastMutexGuard guard(m_LQueue.us_lock);

        //EIO_Status status = 
            m_LQueue.udp_socket.Send(msg, strlen(msg)+1, 
                                     CSocketAPI::ntoa(subm_addr), subm_port);
    }
}

void CQueueDataBase::CQueue::PutProgressMessage(unsigned int  job_id,
                                                const char*   msg)
{
    SQueueDB& db = m_LQueue.db;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    CFastMutexGuard guard(m_LQueue.lock);
    db.SetTransaction(&trans);

    {{
    CBDB_FileCursor& cur = *GetCursor(trans);
    CCursorGuard cg(cur);    

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
}


void CQueueDataBase::CQueue::JobFailed(unsigned int  job_id,
                                       const string& err_msg)
{
    m_LQueue.status_tracker.SetStatus(job_id, CNetScheduleClient::eFailed);

    SQueueDB& db = m_LQueue.db;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    unsigned subm_addr, subm_port, subm_timeout, time_submit;
    time_t curr = time(0);

    {{
    CFastMutexGuard guard(m_LQueue.lock);
    db.SetTransaction(&trans);

    CBDB_FileCursor& cur = *GetCursor(trans);
    CCursorGuard cg(cur);    

    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << job_id;

    if (cur.FetchFirst() != eBDB_Ok) {
        // TODO: Integrity error or job just expired?
        return;
    }
    time_submit = db.time_submit;
    subm_addr = db.subm_addr;
    subm_port = db.subm_port;
    subm_timeout = db.subm_timeout;
    
    db.status = (int) CNetScheduleClient::eFailed;
    db.time_done = curr;
    db.err_msg = err_msg;

    cur.Update();

    }}

    trans.Commit();

    RemoveFromTimeLine(job_id);

    // check if we need to send a UDP notification

    if ( subm_addr && subm_timeout &&
        (time_submit + subm_timeout >= (unsigned)curr)) {

        char msg[1024];
        sprintf(msg, "JNTF %u", job_id);

        CFastMutexGuard guard(m_LQueue.us_lock);

        //EIO_Status status = 
            m_LQueue.udp_socket.Send(msg, strlen(msg)+1, 
                                     CSocketAPI::ntoa(subm_addr), subm_port);
    }

}


void CQueueDataBase::CQueue::SetJobRunTimeout(unsigned job_id, unsigned tm)
{
    CNetScheduleClient::EJobStatus st = GetStatus(job_id);
    if (st != CNetScheduleClient::eRunning) {
        return;
    }
    SQueueDB& db = m_LQueue.db;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    unsigned exp_time = 0;
    time_t curr = time(0);

    {{
    CFastMutexGuard guard(m_LQueue.lock);
    db.SetTransaction(&trans);

    CBDB_FileCursor& cur = *GetCursor(trans);
    CCursorGuard cg(cur);    

    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << job_id;

    if (cur.FetchFirst() != eBDB_Ok) {
        return;
    }

    int status = db.status;

    if (status != (int)CNetScheduleClient::eRunning) {
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
        CJobTimeLine& tl = *m_LQueue.run_time_line;

        CWriteLockGuard guard(m_LQueue.rtl_lock);
        tl.MoveObject(exp_time, curr + tm, job_id);
    }}

}

void CQueueDataBase::CQueue::ReturnJob(unsigned int job_id)
{
    _ASSERT(job_id);

//    CIdBusyGuard id_guard(&m_Db.m_UsedIds, job_id, 3);
    CNetSchedule_JS_Guard js_guard(m_LQueue.status_tracker, 
                                   job_id,
                                   CNetScheduleClient::eReturned);
    CNetScheduleClient::EJobStatus st = js_guard.GetOldStatus();
    // if canceled or already returned or done
    if ( m_LQueue.status_tracker.IsCancelCode(st) || 
        (st == CNetScheduleClient::eReturned) || 
        (st == CNetScheduleClient::eDone)) {
        js_guard.Release();
        return;
    }
    {{
    SQueueDB& db = m_LQueue.db;

    CFastMutexGuard guard(m_LQueue.lock);
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
    js_guard.Release();
    RemoveFromTimeLine(job_id);

}

void CQueueDataBase::CQueue::GetJob(unsigned int   worker_node,
                                    unsigned int*  job_id, 
                                    char*          input)
{
    _ASSERT(worker_node && input);
    unsigned get_attempts = 0;
    const unsigned kMaxGetAttempts = 1000;

get_job_id:

    ++get_attempts;
    if (get_attempts > kMaxGetAttempts) {
        *job_id = 0;
        return;
    }

    *job_id = m_LQueue.status_tracker.GetPendingJob();
    if (!*job_id) {
        return;
    }
//    CIdBusyGuard id_guard(&m_Db.m_UsedIds, *job_id, 3);

    time_t curr = time(0);

    try {
        SQueueDB& db = m_LQueue.db;
        CBDB_Transaction trans(*db.GetEnv(), 
                            CBDB_Transaction::eTransASync,
                            CBDB_Transaction::eNoAssociation);


        {{
        CFastMutexGuard guard(m_LQueue.lock);
        db.SetTransaction(&trans);

        CBDB_FileCursor& cur = *GetCursor(trans);
        CCursorGuard cg(cur);    

        cur.SetCondition(CBDB_FileCursor::eEQ);
        cur.From << *job_id;
        if (cur.Fetch() != eBDB_Ok) {
            m_LQueue.status_tracker.ChangeStatus(*job_id, 
                                         CNetScheduleClient::ePending);
            *job_id = 0; 
            return;
        }
        int status = db.status;

        // internal integrity check
        if (!(status == (int)CNetScheduleClient::ePending ||
              status == (int)CNetScheduleClient::eReturned)
            ) {
            if (m_LQueue.status_tracker.IsCancelCode(
                (CNetScheduleClient::EJobStatus) status)) {
                // this job has been canceled while i'm fetching
                goto get_job_id;
            }
            LOG_POST(Error << "Status integrity violation " 
                            << " job = " << *job_id 
                            << " status = " << status
                            << " expected status = " 
                            << (int)CNetScheduleClient::ePending);
            *job_id = 0;
            return;
        }

        const char* fld_str = db.input;
        ::strcpy(input, fld_str);
        unsigned run_counter = db.run_counter;

        db.status = (int) CNetScheduleClient::eRunning;
        db.time_run = curr;
        db.run_timeout = 0;
        db.run_counter = ++run_counter;

        unsigned time_submit = db.time_submit;
        unsigned timeout = db.timeout;
        if (timeout == 0) {
            timeout = m_LQueue.timeout;
        }

        _ASSERT(timeout);

        // check if job already expired
        if (timeout && (time_submit + timeout < (unsigned)curr)) {
            *job_id = 0; 
            db.time_run = 0;
            db.run_counter = --run_counter;
            db.status = (int) CNetScheduleClient::eFailed;
            m_LQueue.status_tracker.ChangeStatus(*job_id, 
                                         CNetScheduleClient::eFailed);
        } else {

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
                m_LQueue.status_tracker.ChangeStatus(*job_id, 
                                            CNetScheduleClient::eFailed);
                LOG_POST(Error << "Too many run attempts. job=" << *job_id);
                *job_id = 0; 
                db.status = (int) CNetScheduleClient::eFailed;
                db.time_run = 0;
                db.run_counter = --run_counter;
            } // switch

        }

        cur.Update();

        }}
        trans.Commit();

    } 
    catch (exception&)
    {
        m_LQueue.status_tracker.ChangeStatus(*job_id, 
                                             CNetScheduleClient::ePending);
        *job_id = 0;
        throw;
    }

    // if we picked up expired job and need to re-get another job id    
    if (*job_id == 0) {
        goto get_job_id;
    }


    // setup the job in the timeline
    if (*job_id && m_LQueue.run_time_line) {
        CJobTimeLine& tl = *m_LQueue.run_time_line;
        time_t projected_time_done = curr + m_LQueue.run_timeout;

        CWriteLockGuard guard(m_LQueue.rtl_lock);
        tl.AddObject(projected_time_done, *job_id);
    }
}

void CQueueDataBase::CQueue::GetJob(char*          key_buf, 
                                    unsigned int   worker_node,
                                    unsigned int*  job_id,
                                    char*          input,
                                    const string&  host,
                                    unsigned       port)
{
    GetJob(worker_node, job_id, input);
    if (*job_id) {
        sprintf(key_buf, NETSCHEDULE_JOBMASK, *job_id, host.c_str(), port);
    }
}

bool CQueueDataBase::CQueue::GetJobDescr(unsigned int job_id,
                                         int*         ret_code,
                                         char*        input,
                                         char*        output,
                                         char*        err_msg,
                                         char*        progress_msg)
{
    SQueueDB& db = m_LQueue.db;
    CFastMutexGuard guard(m_LQueue.lock);
    db.SetTransaction(0);

    db.id = job_id;
    if (db.Fetch() == eBDB_Ok) {
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

        return true;
    }

    return false; // job not found
}

/*
bool CQueueDataBase::CQueue::GetOutput(unsigned int job_id,
                                       int*         ret_code,
                                       char*        output)
{
    _ASSERT(ret_code);
    _ASSERT(output);

    SQueueDB& db = m_LQueue.db;
    CFastMutexGuard guard(m_LQueue.lock);
    db.SetTransaction(0);

    db.id = job_id;
    if (db.Fetch() == eBDB_Ok) {
        *ret_code = db.ret_code;
        const char* out_str = db.output;
        ::strcpy(output, out_str);
        return true;
    }

    return false; // job not found

}

bool CQueueDataBase::CQueue::GetErrMsg(unsigned int job_id,
                                       char*        err_msg)
{
    _ASSERT(err_msg);

    SQueueDB& db = m_LQueue.db;
    CFastMutexGuard guard(m_LQueue.lock);
    db.SetTransaction(0);

    db.id = job_id;
    if (db.Fetch() == eBDB_Ok) {
        const char* str = db.err_msg;
        ::strcpy(err_msg, str);
        return true;
    }

    return false; // job not found
}
*/

CNetScheduleClient::EJobStatus 
CQueueDataBase::CQueue::GetStatus(unsigned int job_id) const
{
//    CIdBusyGuard id_guard(&m_Db.m_UsedIds, job_id, 3);
    return m_LQueue.status_tracker.GetStatus(job_id);
}

CBDB_FileCursor* CQueueDataBase::CQueue::GetCursor(CBDB_Transaction& trans)
{
    CBDB_FileCursor* cur = m_LQueue.cur.get();
    if (cur) { 
        cur->ReOpen(&trans);
        return cur;
    }
    cur = new CBDB_FileCursor(m_LQueue.db, 
                              trans,
                              CBDB_FileCursor::eReadModifyUpdate);
    m_LQueue.cur.reset(cur);
    return cur;
    
}

bool CQueueDataBase::CQueue::CheckDelete(unsigned int job_id)
{
    SQueueDB& db = m_LQueue.db;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    time_t curr = time(0);

    {{
    CFastMutexGuard guard(m_LQueue.lock);
    db.SetTransaction(&trans);

    CBDB_FileCursor& cur = *GetCursor(trans);
    CCursorGuard cg(cur);    
    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << job_id;

    if (cur.FetchFirst() != eBDB_Ok) {
        return true; // already deleted
    }
    int status = db.status;
    if (status != (int) CNetScheduleClient::eDone) {
        return true;
    }

    unsigned queue_ttl = m_LQueue.timeout;
    int job_ttl = db.timeout;
    if (job_ttl == 0) {
        job_ttl = queue_ttl;
    }

    unsigned time_done = db.time_done;
    if (time_done == 0) {  // job not done yet
        return false;
    }
    if (time_done + job_ttl > (unsigned)curr) {
        return false;
    }
    cur.Delete(CBDB_File::eIgnoreError);
//cerr << "Job deleted: " << job_id << endl;

    }}
    trans.Commit();

    m_LQueue.status_tracker.SetStatus(job_id, 
                                      CNetScheduleClient::eJobNotFound);
    return true;
}

unsigned 
CQueueDataBase::CQueue::CheckDeleteBatch(unsigned batch_size,
                                        CNetScheduleClient::EJobStatus status)
{
    unsigned dcnt;
    for (dcnt = 0; dcnt < batch_size; ++dcnt) {
        unsigned job_id = m_LQueue.status_tracker.GetFirst(status);
        if (job_id == 0) {
            break;
        }
        bool deleted = CheckDelete(job_id);
        if (!deleted) {
            break;
        }
    } // for
    return dcnt;
}

void CQueueDataBase::CQueue::Truncate()
{
    m_LQueue.status_tracker.ClearAll();
    SQueueDB& db = m_LQueue.db;
    CFastMutexGuard guard(m_LQueue.lock);
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransSync);
    db.SetTransaction(&trans);
    db.Truncate();
    trans.Commit();
}

void CQueueDataBase::CQueue::RemoveFromTimeLine(unsigned job_id)
{
    if (m_LQueue.run_time_line) {
        CWriteLockGuard guard(m_LQueue.rtl_lock);
        m_LQueue.run_time_line->RemoveObject(job_id);
    }
}


void CQueueDataBase::CQueue::RegisterNotificationListener(
                                            unsigned int    host_addr,
                                            unsigned short  udp_port,
                                            int             timeout)
{
    // check if listener is already in the list

    SLockedQueue::TListenerList&  wnodes = m_LQueue.wnodes;

    SQueueListener* ql_ptr = 0;
    
    {{
    CReadLockGuard guard(m_LQueue.wn_lock);
    for (SLockedQueue::TListenerList::size_type i = 0; 
         i < wnodes.size(); 
         ++i) 
    {
        SQueueListener& ql = *wnodes[i];
        if ((ql.udp_port == udp_port) &&
            (ql.host == host_addr)
            ) {
            ql_ptr = &ql;
            break;
        }
    } // for
    }}


    time_t curr = time(0);
    CWriteLockGuard guard(m_LQueue.wn_lock);
    if (ql_ptr) {  // update registration timestamp
        if ( (ql_ptr->timeout = timeout)!= 0 ) {
            ql_ptr->last_connect = curr;
        } else {
        }

        return;
    }

    // new element

    if (timeout) {
        wnodes.push_back(
            new SQueueListener(host_addr, udp_port, curr, timeout));
    }
}


void CQueueDataBase::CQueue::NotifyListeners()
{
    unsigned short udp_port = m_Db.GetUdpPort();
    if (udp_port == 0) {
        return;
    }

    SLockedQueue::TListenerList&  wnodes = m_LQueue.wnodes;

    time_t curr = time(0);

    if (m_LQueue.notif_timeout == 0 ||
        !m_LQueue.status_tracker.AnyPending()) {
        return;
    }


    SLockedQueue::TListenerList::size_type lst_size;

    {{
        CWriteLockGuard guard(m_LQueue.wn_lock);
        lst_size = wnodes.size();
        if ((lst_size == 0) ||
            (m_LQueue.last_notif + m_LQueue.notif_timeout > curr)) {
            return;
        } 
        m_LQueue.last_notif = curr;
    }}

    const char* msg = m_LQueue.q_notif.c_str();
    size_t msg_len = m_LQueue.q_notif.length()+1;
    
    for (SLockedQueue::TListenerList::size_type i = 0; 
         i < lst_size; 
         ++i) 
    {
        unsigned host;
        unsigned short port;
        SQueueListener* ql;
        {{
            CReadLockGuard guard(m_LQueue.wn_lock);
            ql = wnodes[i];
            if (ql->last_connect + ql->timeout < curr) {
                continue;
            }
        }}
        host = ql->host;
        port = ql->udp_port;

        if (port) {
            CFastMutexGuard guard(m_LQueue.us_lock);
            //EIO_Status status = 
                m_LQueue.udp_socket.Send(msg, msg_len, 
                                     CSocketAPI::ntoa(host), port);
        }
        // periodically check if we have no more jobs left
        if ((i % 10 == 0) &&
            !m_LQueue.status_tracker.AnyPending()) {
            break;
        }

    }
}

void CQueueDataBase::CQueue::CheckExecutionTimeout()
{
    if (m_LQueue.run_time_line == 0) {
        return;
    }
    CJobTimeLine& tl = *m_LQueue.run_time_line;
    time_t curr = time(0);
    unsigned curr_slot;
    {{
        CReadLockGuard guard(m_LQueue.rtl_lock);
        curr_slot = tl.TimeLineSlot(curr);
    }}
    if (curr_slot == 0) {
        return;
    }
    --curr_slot;

    CJobTimeLine::TObjVector bv;
    {{
        CReadLockGuard guard(m_LQueue.rtl_lock);
        tl.EnumerateObjects(&bv, curr_slot);
    }}
    CJobTimeLine::TObjVector::enumerator en(bv.first());
    for (;en.valid(); ++en) {
        unsigned job_id = *en;
        unsigned exp_time = CheckExecutionTimeout(job_id, curr_slot);

        // job may need to moved in the timeline to some future slot
        
        if (exp_time) {
            CWriteLockGuard guard(m_LQueue.rtl_lock);
            unsigned job_slot = tl.TimeLineSlot(exp_time);
            if (job_slot <= curr_slot) {
                ++job_slot;
            }
            tl.AddObjectToSlot(job_slot, job_id);
        }
    } // for

    CWriteLockGuard guard(m_LQueue.rtl_lock);
    tl.HeadTruncate(curr_slot);
}

time_t CQueueDataBase::CQueue::CheckExecutionTimeout(unsigned job_id,
                                                   time_t   curr_time)
{
    SQueueDB& db = m_LQueue.db;

    CNetScheduleClient::EJobStatus status = GetStatus(job_id);

    if (status != CNetScheduleClient::eRunning) {
        return 0;
    }

    CBDB_Transaction trans(*db.GetEnv(), 
                        CBDB_Transaction::eTransASync,
                        CBDB_Transaction::eNoAssociation);

    // TODO: get current job status from the status index

    {{
    CFastMutexGuard guard(m_LQueue.lock);
    db.SetTransaction(&trans);

    CBDB_FileCursor& cur = *GetCursor(trans);
    CCursorGuard cg(cur);    

    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << job_id;
    if (cur.Fetch() != eBDB_Ok) {
        return 0;
    }
    int status = db.status;
    if (status != (int)CNetScheduleClient::eRunning) {
        return 0;
    }

    unsigned time_run = db.time_run;
    unsigned run_timeout = db.run_timeout;
/*
    if (run_timeout == 0) {
        run_timeout = m_LQueue.run_timeout;
    }
    if (run_timeout == 0) {
        return 0;
    }
*/
    time_t exp_time = x_ComputeExpirationTime(time_run, run_timeout);
        //time_run + run_timeout;
    if (!(curr_time < exp_time)) { 
        return exp_time;
    }
    db.status = (int) CNetScheduleClient::ePending;
    cur.Update();

    }}

    trans.Commit();

    m_LQueue.status_tracker.SetStatus(job_id, CNetScheduleClient::eReturned);

    return 0;
}

time_t 
CQueueDataBase::CQueue::x_ComputeExpirationTime(unsigned time_run, 
                                                unsigned run_timeout) const
{
    if (run_timeout == 0) {
        run_timeout = m_LQueue.run_timeout;
    }
    if (run_timeout == 0) {
        return 0;
    }
    time_t exp_time = time_run + run_timeout;
    return exp_time;
}



END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
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
 * Revision 1.16  2005/03/15 20:14:30  kuznets
 * Implemented notification to client waiting for job
 *
 * Revision 1.15  2005/03/15 14:52:39  kuznets
 * Better datagram socket management, DropJob implemenetation
 *
 * Revision 1.14  2005/03/10 14:19:57  kuznets
 * Implemented individual run timeouts
 *
 * Revision 1.13  2005/03/09 19:05:36  kuznets
 * Fixed unintialized variable
 *
 * Revision 1.12  2005/03/09 17:40:08  kuznets
 * Fixed GCC warning
 *
 * Revision 1.11  2005/03/09 17:37:16  kuznets
 * Added node notification thread and execution control timeline
 *
 * Revision 1.10  2005/03/09 15:22:07  kuznets
 * Use another datagram Send
 *
 * Revision 1.9  2005/03/04 12:06:41  kuznets
 * Implemented UDP callback to clients
 *
 * Revision 1.8  2005/02/28 12:24:17  kuznets
 * New job status Returned, better error processing and queue management
 *
 * Revision 1.7  2005/02/24 12:35:10  kuznets
 * Cosmetics..
 *
 * Revision 1.6  2005/02/23 19:16:38  kuznets
 * Implemented garbage collection thread
 *
 * Revision 1.5  2005/02/22 16:13:00  kuznets
 * Performance optimization
 *
 * Revision 1.4  2005/02/14 17:57:41  kuznets
 * Fixed a bug in queue procesing
 *
 * Revision 1.3  2005/02/10 19:58:51  kuznets
 * +GetOutput()
 *
 * Revision 1.2  2005/02/09 18:55:18  kuznets
 * Implemented correct database shutdown
 *
 * Revision 1.1  2005/02/08 16:42:55  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
