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

/// Mutex protected Queue database with job status FSM
///
/// @internal
///
struct SLockedQueue
{
    SQueueDB                        db;
    auto_ptr<CBDB_FileCursor>       cur;
    CFastMutex                      lock;
    CNetScheduler_JobStatusTracker  status_tracker;

    // queue parameters
    int                             timeout;

    SLockedQueue() : timeout(3600) {}
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
                    int           timeout);
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
        unsigned CheckDeleteBatch(unsigned batch_size);

        /// Remove all jobs
        void Truncate();
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

    bool                            m_StopPurge;   ///< Purge stop flag
    CFastMutex                      m_PurgeLock;
    unsigned int                    m_PurgeLastId; ///< m_MaxId at last Purge
    unsigned int                    m_PurgeSkipCnt;///< Number of purge skipped

};



END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
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

