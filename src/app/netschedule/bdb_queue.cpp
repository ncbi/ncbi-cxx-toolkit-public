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
#include <connect/netschedule_client.hpp>

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
  m_MaxId(0)
{}

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

void CQueueDataBase::MountQueue(const string& queue_name)
{
    _ASSERT(m_Env);

    if (m_QueueCollection.QueueExists(queue_name)) {
        LOG_POST(Warning << 
                 "JS: Queue " << queue_name << " already exists.");
        return;
    }

    auto_ptr<SLockedQueue> q(new SLockedQueue());
    q->db.SetEnv(*m_Env);
    string fname = string("jsq_") + queue_name + string(".db");
    q->db.Open(fname.c_str(), CBDB_RawFile::eReadWriteCreate);

    m_QueueCollection.AddQueue(queue_name, q.release());

    // scan the queue, restore the state machine

    SLockedQueue& queue = m_QueueCollection.GetLockedQueue(queue_name);

    CBDB_FileCursor cur(queue.db);
    cur.SetCondition(CBDB_FileCursor::eGE);
    cur.From << 0;

    while (cur.Fetch() == eBDB_Ok) {
        unsigned job_id = queue.db.id;
        int status = queue.db.status;

        if (job_id > m_MaxId) {
            m_MaxId = job_id;
        }
        m_StatusTracker.SetExactStatusNoLock(job_id, 
                      (CNetScheduleClient::EJobStatus) status, 
                      true);
    } // while
    
}

void CQueueDataBase::Close()
{
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

    CFastMutexGuard guard(m_IdLock);

    if (++m_MaxId == 0) {
        ++m_MaxId;
    }
    id = m_MaxId;
    return id;
}


CQueueDataBase::CQueue::CQueue(CQueueDataBase& db, const string& queue_name)
: m_Db(db),
  m_LQueue(db.m_QueueCollection.GetLockedQueue(queue_name))
{
}

unsigned int CQueueDataBase::CQueue::Submit(const string& input)
{
    unsigned int job_id = m_Db.GetNextId();

    CIdBusyGuard id_guard(&m_Db.m_UsedIds, job_id, 3);

    CNetSchedule_JS_Guard js_guard(m_Db.m_StatusTracker, 
                                   job_id,
                                   CNetScheduleClient::ePending);
    {{
    SQueueDB& db = m_LQueue.db;
    CFastMutexGuard guard(m_LQueue.lock);

    CBDB_Transaction trans(*db.GetEnv(), CBDB_Transaction::eTransASync);
	db.SetTransaction(&trans);

    db.id = job_id;

    db.status = (int) CNetScheduleClient::ePending;
    db.failed = 0;

    db.time_submit = time(0);
    db.time_run = 0;
    db.time_done = 0;

    db.worker_node1 = 0;
    db.worker_node2 = 0;
    db.worker_node3 = 0;
    db.worker_node4 = 0;
    db.worker_node5 = 0;

    db.run_counter = 0;
    db.ret_code = 0;

    db.input = input;
    db.output = "";

    db.cout = "";
    db.cerr = "";

    db.Insert();
    trans.Commit();

    }}
    js_guard.Release();

    return job_id;
}

void CQueueDataBase::CQueue::Cancel(unsigned int job_id)
{
    CIdBusyGuard id_guard(&m_Db.m_UsedIds, job_id, 3);

    CNetSchedule_JS_Guard js_guard(m_Db.m_StatusTracker, 
                                   job_id,
                                   CNetScheduleClient::eCanceled);

    {{
    SQueueDB& db = m_LQueue.db;
    CFastMutexGuard guard(m_LQueue.lock);

    db.id = job_id;
    if (db.Fetch() == eBDB_Ok) {
        int status = db.status;
        if (status < (int) CNetScheduleClient::eCanceled) {
            db.status = (int) CNetScheduleClient::eCanceled;
            db.time_done = time(0);
            
            CBDB_Transaction trans(*db.GetEnv(), CBDB_Transaction::eTransASync);
	        db.SetTransaction(&trans);

            db.UpdateInsert();

            trans.Commit();
        }
    } else {
        // TODO: Integrity error or job just expired?
    }    
    }}
    js_guard.Release();
}


void CQueueDataBase::CQueue::PutResult(unsigned int  job_id,
                                       int           ret_code,
                                       const string& output)
{
    CIdBusyGuard id_guard(&m_Db.m_UsedIds, job_id, 3);

    CNetSchedule_JS_Guard js_guard(m_Db.m_StatusTracker, 
                                   job_id,
                                   CNetScheduleClient::eDone);

    {{
    SQueueDB& db = m_LQueue.db;
    CFastMutexGuard guard(m_LQueue.lock);

    db.id = job_id;
    if (db.Fetch() == eBDB_Ok) {
        int status = db.status;
        if (status != (int)CNetScheduleClient::eCanceled) {
            
            CBDB_Transaction trans(*db.GetEnv(), CBDB_Transaction::eTransASync);
            db.SetTransaction(&trans);

            db.status = (int) CNetScheduleClient::eDone;
            db.ret_code = ret_code;
            db.output = output;
            db.time_done = time(0);
            db.UpdateInsert();

            trans.Commit();
        }            
    } else {
        // TODO: Integrity error or job just expired?
    }    
    }}
    js_guard.Release();
}

void CQueueDataBase::CQueue::ReturnJob(unsigned int job_id)
{
    _ASSERT(job_id);

    CIdBusyGuard id_guard(&m_Db.m_UsedIds, job_id, 3);
    CNetSchedule_JS_Guard js_guard(m_Db.m_StatusTracker, 
                                   job_id,
                                   CNetScheduleClient::ePending);
    {{
    SQueueDB& db = m_LQueue.db;
    CFastMutexGuard guard(m_LQueue.lock);

    db.id = job_id;
    if (db.Fetch() == eBDB_Ok) {            
        CBDB_Transaction trans(*db.GetEnv(), CBDB_Transaction::eTransASync);
        db.SetTransaction(&trans);

        db.status = (int) CNetScheduleClient::ePending;

        db.UpdateInsert();

        trans.Commit();
    } else {
        // TODO: Integrity error or job just expired?
    }    
    }}
    js_guard.Release();

}

void CQueueDataBase::CQueue::GetJob(unsigned int   worker_node,
                                    unsigned int*  job_id, 
                                    string*        input)
{
    _ASSERT(worker_node && input);

    *job_id = m_Db.m_StatusTracker.GetPendingJob();
    if (!*job_id) {
        return;
    }
    CIdBusyGuard id_guard(&m_Db.m_UsedIds, *job_id, 3);

    try {
        SQueueDB& db = m_LQueue.db;
        CFastMutexGuard guard(m_LQueue.lock);

        db.id = *job_id;
        if (db.Fetch() == eBDB_Ok) {
            int status = db.status;
            *input = db.input;
            unsigned run_counter = db.run_counter;

            if (status != (int)CNetScheduleClient::ePending) {
                LOG_POST(Error << "Status integrity violation " 
                                << " job = " << *job_id 
                                << " status = " << status
                                << " expected status = " 
                                << (int)CNetScheduleClient::ePending);
                *job_id = 0;
            } else {
                CBDB_Transaction trans(*db.GetEnv(), 
                                        CBDB_Transaction::eTransASync);
                db.SetTransaction(&trans);

                db.status = (int) CNetScheduleClient::ePending;
                db.time_run = time(0);
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
                    db.worker_node1 = worker_node;
                    break;
                default:
                    LOG_POST(Error << "Too many run attempts..");
                    break;
                } // switch

                db.UpdateInsert();
                trans.Commit();                    
            }
        }
    } 
    catch (exception&)
    {
        m_Db.m_StatusTracker.ChangeStatus(*job_id, 
                                          CNetScheduleClient::ePending);
        *job_id = 0;
        throw;
    }
    
}



END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2005/02/09 18:55:18  kuznets
 * Implemented correct database shutdown
 *
 * Revision 1.1  2005/02/08 16:42:55  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
