#ifndef NETSCHEDULE_BDB_QUEUE__HPP
#define NETSCHEDULE_BDB_QUEUE__HPP


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
 * File Description:
 *   Net schedule job status database.
 *
 */


/// @file bdb_queue.hpp
/// NetSchedule job status database. 
///
/// @internal


#include <corelib/ncbimtx.hpp>
#include <bdb/bdb_file.hpp>
#include <bdb/bdb_env.hpp>
#include <map>
#include <vector>

#include "job_status.hpp"
#include "queue_clean_thread.hpp"


BEGIN_NCBI_SCOPE

/// BDB table to store queue information
///
/// @internal
///
struct SQueueDB : public CBDB_File
{
    CBDB_FieldUint4        id;              ///< Job id

    CBDB_FieldInt4         status;          ///< Current job status
    CBDB_FieldInt4         failed;          ///< Number of job failures
    CBDB_FieldUint4        time_submit;     ///< Job submit time
    CBDB_FieldUint4        time_run;        ///<     run time
    CBDB_FieldUint4        time_done;       ///<     result submission time
    CBDB_FieldUint4        timeout;         ///<     individual timeout

    CBDB_FieldUint4        worker_node1;    ///< IP address of worker node 1
    CBDB_FieldUint4        worker_node2;    ///< reserved
    CBDB_FieldUint4        worker_node3;
    CBDB_FieldUint4        worker_node4;
    CBDB_FieldUint4        worker_node5;

    CBDB_FieldUint4        run_counter;     ///< Number of execution attempts
    CBDB_FieldInt4         ret_code;        ///< Return code

    CBDB_FieldString       input;           ///< Input data
    CBDB_FieldString       output;          ///< Result data

    CBDB_FieldString       cout;            ///< Reserved
    CBDB_FieldString       cerr;            ///< Reserved

    SQueueDB()
    {
        DisableNull(); 

        BindKey("id",      &id);

        BindData("status", &status);
        BindData("failed", &failed);
        BindData("time_submit", &time_submit);
        BindData("time_run",    &time_run);
        BindData("time_done",   &time_done);
        BindData("timeout",     &timeout);

        BindData("worker_node1", &worker_node1);
        BindData("worker_node2", &worker_node2);
        BindData("worker_node3", &worker_node3);
        BindData("worker_node4", &worker_node4);
        BindData("worker_node5", &worker_node5);

        BindData("run_counter", &run_counter);
        BindData("ret_code",    &ret_code);

        BindData("input",  &input,  kNetScheduleMaxDataSize);
        BindData("output", &output, kNetScheduleMaxDataSize);

        BindData("cout",  &cout, 128);
        BindData("cerr",  &cerr, 128);
    }
};

/// Queue watcher description
///
/// @internal
///
struct SQueueListener
{
    unsigned int   host;         ///< host name (network BO)
    unsigned short port;         ///< Listening UDP port
    time_t         last_connect; ///< Last registration timestamp
    int            timeout;      ///< Notification expiration timeout

    SQueueListener(unsigned int   host_addr,
                   unsigned short port_number,
                   time_t         curr,
                   int            expiration_timeout)
    : host(host_addr),
      port(port_number),
      last_connect(curr),
      timeout(expiration_timeout)
    {}
};

/// Mutex protected Queue database with job status FSM
///
/// @internal
///
struct SLockedQueue
{
    SQueueDB                        db;               ///< Database
    auto_ptr<CBDB_FileCursor>       cur;              ///< DB cursor
    CFastMutex                      lock;             ///< db, cursor lock
    CNetScheduler_JobStatusTracker  status_tracker;   ///< status FSA

    // queue parameters
    int                             timeout;       ///< Result exp. timeout
    int                             notif_timeout; ///< Notification interval

    // List of active worker node listeners waiting for pending jobs

    typedef vector<SQueueListener*> TListenerList;

    TListenerList                wnodes;       ///< worker node listeners
    time_t                       last_notif;   ///< last notification time
    CRWLock                      wn_lock;      ///< wnodes locker
    string                       q_notif;      ///< Queue notification message

    SLockedQueue(const string& queue_name) 
        : timeout(3600), 
          notif_timeout(7), 
          last_notif(0), 
          q_notif("NCBI_JSQ_")
    {
        _ASSERT(!queue_name.empty());
        q_notif.append(queue_name);
    }

    ~SLockedQueue()
    {
        NON_CONST_ITERATE(TListenerList, it, wnodes) {
            SQueueListener* node = *it;
            delete node;
        }
    }
};

/// Queue database manager
///
/// @internal
///
class CQueueCollection
{
public:
    typedef map<string, SLockedQueue*> TQueueMap;

public:
    CQueueCollection();
    ~CQueueCollection();

    void Close();

    SLockedQueue& GetLockedQueue(const string& qname);
    bool QueueExists(const string& qname) const;

    /// Collection takes ownership of queue
    void AddQueue(const string& name, SLockedQueue* queue);

    const TQueueMap& GetMap() const { return m_QMap; }

private:
    CQueueCollection(const CQueueCollection&);
    CQueueCollection& operator=(const CQueueCollection&);
private:
    TQueueMap      m_QMap;
};




/// Top level queue database.
/// (Thread-Safe, syncronized.)
///
/// @internal
///
class CQueueDataBase
{
public:
    CQueueDataBase();
    ~CQueueDataBase();

    void Open(const string& path, unsigned cache_ram_size);
    void MountQueue(const string& queue_name,
                    int           timeout,
                    int           notif_timeout);
    void Close();
    bool QueueExists(const string& qname) const 
                { return m_QueueCollection.QueueExists(qname); }

    /// Remove old jobs
    void Purge();
    void StopPurge();

    void RunPurgeThread();
    void StopPurgeThread();

    /// Main queue entry point
    ///
    /// @internal
    ///
    class CQueue
    {
    public:
        CQueue(CQueueDataBase& db, const string& queue_name);

        unsigned int Submit(const string& input);
        void Cancel(unsigned int job_id);
        void PutResult(unsigned int  job_id,
                       int           ret_code,
                       const char*   output);
        void GetJob(unsigned int   worker_node,
                    unsigned int*  job_id, 
                    char*          input);
        // Get job and generate key
        void GetJob(char* key_buf, 
                    unsigned int   worker_node,
                    unsigned int*  job_id, 
                    char*          input,
                    const string&  host,
                    unsigned       port
                    );
        void ReturnJob(unsigned int job_id);

        // Get output info for compeleted job
        bool GetOutput(unsigned int job_id,
                       int*         ret_code,
                       char*        output);

        CNetScheduleClient::EJobStatus 
        GetStatus(unsigned int job_id) const;

        /// Delete if job is done and timeout expired
        ///
        /// @return TRUE if job has been deleted
        ///
        bool CheckDelete(unsigned int job_id);

        /// Delete batch_size jobs
        /// @return
        ///    Number of deleted jobs
        unsigned CheckDeleteBatch(unsigned batch_size,
                                  CNetScheduleClient::EJobStatus status);

        /// Remove all jobs
        void Truncate();

        /// Free unsued memory (status storage)
        void FreeUnusedMem() { m_LQueue.status_tracker.FreeUnusedMem(); }

        /// All returned jobs come back to pending status
        void Return2Pending() { m_LQueue.status_tracker.Return2Pending(); }

        /// @param host_addr
        ///    host address in network BO
        ///
        void RegisterNotificationListener(unsigned int    host_addr, 
                                          unsigned short  port,
                                          int             timeout);

        /// UDP notification to all listeners
        void NotifyListeners();

    private:
        CBDB_FileCursor* GetCursor(CBDB_Transaction& trans);

    private:
        CQueue(const CQueue&);
        CQueue& operator=(const CQueue&);

    private:
        CQueueDataBase& m_Db;      ///< Parent structure reference
        SLockedQueue&   m_LQueue;  
    };

protected:
    /// get next job id (counter increment)
    unsigned int GetNextId();

    friend class CQueue;
private:
    CBDB_Env*                       m_Env;
    string                          m_Path;
    string                          m_Name;

    CQueueCollection                m_QueueCollection;

    unsigned int                    m_MaxId;   ///< job id counter
    CFastMutex                      m_IdLock;

    bm::bvector<>                   m_UsedIds; /// id access locker
    CRef<CJobQueueCleanerThread>    m_PurgeThread;

    bool                 m_StopPurge;         ///< Purge stop flag
    CFastMutex           m_PurgeLock;
    unsigned int         m_PurgeLastId;       ///< m_MaxId at last Purge
    unsigned int         m_PurgeSkipCnt;      ///< Number of purge skipped
    unsigned int         m_DeleteChkPointCnt; ///< trans. checkpnt counter
    unsigned int         m_FreeStatusMemCnt;  ///< Free memory counter
    time_t               m_LastR2P;           ///< Return 2 Pending timestamp

};



END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2005/03/04 12:06:41  kuznets
 * Implenyed UDP callback to clients
 *
 * Revision 1.7  2005/02/28 12:24:17  kuznets
 * New job status Returned, better error processing and queue management
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
 * Revision 1.3  2005/02/10 19:58:45  kuznets
 * +GetOutput()
 *
 * Revision 1.2  2005/02/09 18:56:08  kuznets
 * private->public fix
 *
 * Revision 1.1  2005/02/08 16:42:55  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif /* NETSCHEDULE_BDB_QUEUE__HPP */

