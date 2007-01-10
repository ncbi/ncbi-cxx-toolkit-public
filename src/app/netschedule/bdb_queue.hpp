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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description:
 *   NetSchedule job status database.
 *
 */


/// @file bdb_queue.hpp
/// NetSchedule job status database. 
///
/// @internal


#include <corelib/ncbimtx.hpp>
#include <corelib/ncbicntr.hpp>

#include <connect/services/netschedule_client.hpp>
#include "job_status.hpp"
#include "queue_clean_thread.hpp"
#include "notif_thread.hpp"
#include "squeue.hpp"
#include "queue_vc.hpp"

BEGIN_NCBI_SCOPE


/// Batch submit record
///
/// @internal
///
struct SNS_BatchSubmitRec
{
    char      input[kNetScheduleMaxDBDataSize];
    char      affinity_token[kNetScheduleMaxDBDataSize];
    unsigned  affinity_id;
    unsigned  job_id;

    SNS_BatchSubmitRec()
    {
        input[0] = affinity_token[0] = 0;
        affinity_id = job_id = 0;
    }
};

class CQueueDataBase;

/// Main queue entry point
///
/// @internal
///
class CQueue
{
public:
    /// @param client_host_addr - 0 means internal use
    CQueue(CQueueDataBase& db,
           const string&   queue_name,
           unsigned        client_host_addr);

    unsigned int Submit(const char*   input,
                        unsigned      host_addr = 0,
                        unsigned      port = 0,
                        unsigned      wait_timeout = 0,
                        const char*   progress_msg = 0,
                        const char*   affinity_token = 0,
                        const char*   jout = 0,
                        const char*   jerr = 0,
                        unsigned      mask = 0);

    /// Submit job batch
    /// @return 
    ///    ID of the first job, second is first_id+1 etc.
    unsigned int SubmitBatch(vector<SNS_BatchSubmitRec> & batch,
                             unsigned      host_addr = 0,
                             unsigned      port = 0,
                             unsigned      wait_timeout = 0,
                             const char*   progress_msg = 0);

    void Cancel(unsigned int job_id);

    /// Move job to pending ignoring its current status
    void ForceReschedule(unsigned int job_id);

    void PutResult(unsigned int  job_id,
                   int           ret_code,
                   const char*   output);

    void PutResultGetJob(unsigned int   done_job_id,
                         int            ret_code,
                         const char*    output,
                         unsigned int   worker_node,
                         unsigned int*  job_id, 
                         char*          input,
                         char*          jout,
                         char*          jerr,
                         const string&  client_name,
                         unsigned*      job_mask);


    void PutProgressMessage(unsigned int  job_id,
                            const char*   msg);
    
    void JobFailed(unsigned int  job_id,
                   const string& err_msg,
                   const string& output,
                   int           ret_code,
                   unsigned int  worker_node,
                   const string& client_name);

    void GetJobKey(char* key_buf, unsigned job_id,
                   const string& host, unsigned port);


    void GetJob(unsigned int   worker_node,
                unsigned int*  job_id, 
                char*          input,
                char*          jout,
                char*          jerr,
                const string&  client_name,
                unsigned*      job_mask);

    void ReturnJob(unsigned int job_id);

    /// @param expected_status
    ///    If current status is different from expected try to
    ///    double read the database (to avoid races between nodes
    ///    and submitters)
    bool GetJobDescr(unsigned int job_id,
                     int*         ret_code,
                     char*        input,
                     char*        output,
                     char*        err_msg,
                     char*        progress_msg,
                     char*        jout,
                     char*        jerr,
                     CNetScheduleClient::EJobStatus expected_status 
                                        = CNetScheduleClient::eJobNotFound);

    CNetScheduleClient::EJobStatus 
    GetStatus(unsigned int job_id) const;

    /// count status snapshot for affinity token
    /// returns false if affinity token not found
    bool CountStatus(CJobStatusTracker::TStatusSummaryMap* status_map,
                     const char*                           affinity_token);


    /// Set job run-expiration timeout
    /// @param tm
    ///    Time worker node needs to execute the job (in seconds)
    void SetJobRunTimeout(unsigned job_id, unsigned tm);

    /// Prolong job expiration timeout
    /// @param tm
    ///    Time worker node needs to execute the job (in seconds)
    void JobDelayExpiration(unsigned job_id, unsigned tm);

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
    /// Actually delete batch jobs from jobs-to-delete vector
    /// @return
    ///    Number of deleted jobs
    unsigned DoDeleteBatch(unsigned batch_size);

    /// Delete all job ids already deleted (phisically) from the queue
    void ClearAffinityIdx(void);

    /// Remove all jobs
    void Truncate(void);

    /// Remove job from the queue
    void DropJob(unsigned job_id);

    /// @param host_addr
    ///    host address in network BO
    ///
    void RegisterNotificationListener(unsigned int   host_addr, 
                                      unsigned short udp_port,
                                      int            timeout,
                                      const string&  auth);
    void UnRegisterNotificationListener(unsigned int   host_addr, 
                                        unsigned short udp_port);

    /// Remove affinity association for a specified host
    ///
    /// @note Affinity is based on network host name and 
    /// program name (not UDP port). Presumed that we have one
    /// worker node instance per host.
    void ClearAffinity(unsigned int  host_addr,
                        const string& auth);

    void SetMonitorSocket(SOCK sock);

    /// Return monitor (no ownership transfer)
    CNetScheduleMonitor* GetMonitor(void);

    /// UDP notification to all listeners
    void NotifyListeners(bool unconditional=false);

    /// Check execution timeout.
    /// All jobs failed to execute, go back to pending
    void CheckExecutionTimeout(void);

    /// Check job expiration
    /// Returns new time if job not yet expired (or ended)
    /// 0 means job ended in any way.
    time_t CheckExecutionTimeout(unsigned job_id, time_t curr_time);

    unsigned CountStatus(CNetScheduleClient::EJobStatus) const;

    void StatusStatistics(CNetScheduleClient::EJobStatus status,
                          CJobStatusTracker::TBVector::statistics* st) const;

    /// Count database records
    unsigned CountRecs(void);

    void PrintStat(CNcbiOstream & out);
    void PrintNodeStat(CNcbiOstream & out) const;
    void PrintSubmHosts(CNcbiOstream & out) const;
    void PrintWNodeHosts(CNcbiOstream & out) const;
    void PrintQueue(CNcbiOstream& out, 
                    CNetScheduleClient::EJobStatus job_status,
                    const string& host,
                    unsigned      port);

    /// Queue dump
    void PrintJobDbStat(unsigned job_id, 
                        CNcbiOstream & out,
                        CNetScheduleClient::EJobStatus status 
                                    = CNetScheduleClient::eJobNotFound);
    /// Dump all job records
    void PrintAllJobDbStat(CNcbiOstream & out);

    void PrintJobStatusMatrix(CNcbiOstream & out);

    bool IsVersionControl() const;
    bool IsMatchingClient(const CQueueClientInfo& cinfo) const;
    /// Check if client is a configured submitter
    bool IsSubmitAllowed() const;
    /// Check if client is a configured worker node
    bool IsWorkerAllowed() const;

    typedef SLockedQueue::TStatEvent TStatEvent;
    double GetAverage(TStatEvent);

    // Service for CQueueDataBase
    /// Free unsued memory (status storage)
    void FreeUnusedMem(void)
        { x_GetLQueue()->status_tracker.FreeUnusedMem(); }

    /// All returned jobs come back to pending status
    void Returned2Pending(void)
        { x_GetLQueue()->status_tracker.Returned2Pending(); }

private:
    // Transitional - for support of CQueue iterators
    friend class CQueueIterator;
    CQueue(const CQueueDataBase& db) :
        m_Db(const_cast<CQueueDataBase&>(db)), // HACK! - legitimate here,
                                               // because iterator access is
                                               // const, and this constructor
                                               // is for CQueueIterator only
        m_ClientHostAddr(0),
        m_QueueDbAccessCounter(0) {}

    void x_Assume(CRef<SLockedQueue> slq) { m_LQueue = slq; }

    /// Find the listener if it is registered
    /// @return NULL if not found
    ///
    SLockedQueue::TListenerList::iterator 
    x_FindListener(unsigned int host_addr, unsigned short udp_port);

    void x_DropJob(unsigned job_id);

    /// Find the pending job.
    /// This method takes into account jobs available
    /// in the job status matrix and current 
    /// worker node affinity association
    ///
    /// Sync.Locks: affinity map lock, main queue lock, status matrix
    ///
    /// @return job_id
    unsigned 
    x_FindPendingJob(const string&  client_name,
                     unsigned       client_addr);

    /// Get job with load balancing
    void x_GetJobLB(unsigned int   worker_node,
                    unsigned int*  job_id, 
                    char*          input,
                    char*          jout,
                    char*          jerr,
                    unsigned*      job_mask);

    CBDB_FileCursor* x_GetCursor(CBDB_Transaction& trans);


    time_t x_ComputeExpirationTime(unsigned time_run, 
                                   unsigned run_timeout) const; 


    /// db should be already positioned
    void x_PrintJobDbStat(SQueueDB&     db, 
                          CNcbiOstream& out,
                          const char*   fld_separator = "\n",
                          bool          print_fname = true);

    void x_PrintShortJobDbStat(SQueueDB&     db, 
                               const string& host,
                               unsigned port,
                               CNcbiOstream& out,
                               const char*   fld_separator = "\t");

    /// Delete record using positioned cursor, dumps content
    /// to text file if necessary
    void x_DeleteDBRec(SQueueDB& db, 
                       CBDB_FileCursor& cur);

    void x_AssignSubmitRec(unsigned      job_id,
                           const char*   input,
                           unsigned      host_addr,
                           unsigned      port,
                           unsigned      wait_timeout,
                           const char*   progress_msg,
                           unsigned      aff_id,
                           const char*   jout,
                           const char*   jerr,
                           unsigned      mask);

    /// Info on how to notify job submitter
    struct SSubmitNotifInfo
    {
        unsigned subm_addr;
        unsigned subm_port;
        unsigned subm_timeout;
        unsigned time_submit;
        unsigned time_run;
    };

    /// @return TRUE if job record has been found and updated
    bool x_UpdateDB_PutResultNoLock(SQueueDB&            db,
                                    time_t               curr,
                                    CBDB_FileCursor&     cur,
                                    unsigned             job_id,
                                    int                  ret_code,
                                    const char*          output,
                                    SSubmitNotifInfo*    subm_info);


    enum EGetJobUpdateStatus
    {
        eGetJobUpdate_Ok,
        eGetJobUpdate_NotFound,
        eGetJobUpdate_JobStopped,  
        eGetJobUpdate_JobFailed  
    };
    EGetJobUpdateStatus x_UpdateDB_GetJobNoLock(
                                SQueueDB&            db,
                                time_t               curr,
                                CBDB_FileCursor&     cur,
                                CBDB_Transaction&    trans,
                                unsigned int         worker_node,
                                unsigned             job_id,
                                char*                input,
                                char*                jout,
                                char*                jerr,
                                unsigned*            aff_id,
                                unsigned*            job_mask);

    SQueueDB*        x_GetLocalDb();
    CBDB_FileCursor* x_GetLocalCursor(CBDB_Transaction& trans);

    /// Update the affinity index (no locking)
    void x_AddToAffIdx_NoLock(unsigned aff_id, 
                              unsigned job_id_from,
                              unsigned job_id_to = 0);

    void x_AddToAffIdx_NoLock(const vector<SNS_BatchSubmitRec>& batch);

    /// Read queue affinity index, retrieve all jobs, with
    /// given set of affinity ids
    ///
    /// @param aff_id_set
    ///     set of affinity ids to read
    /// @param job_candidates
    ///     OUT set of jobs associated with specified affinities
    ///
    void x_ReadAffIdx_NoLock(const bm::bvector<>& aff_id_set,
                             bm::bvector<>*       job_candidates);

    void x_ReadAffIdx_NoLock(unsigned             aff_id,
                             bm::bvector<>*       job_candidates);

    void x_Count(TStatEvent event)
        { x_GetLQueue()->CountEvent(event); }

    CRef<SLockedQueue> x_GetLQueue(void);
    const CRef<SLockedQueue> x_GetLQueue(void) const;

    void x_AddToTimeLine(unsigned job_id, time_t curr);
    void x_RemoveFromTimeLine(unsigned job_id);
    void x_TimeLineExchange(unsigned remove_job_id, 
                            unsigned add_job_id,
                            time_t   curr);

private:
    CQueue(const CQueue&);
    CQueue& operator=(const CQueue&);

private:
    // Per queue data
    CQueueDataBase&    m_Db;      ///< Parent structure reference
    mutable CFastMutex m_LQueueLock;
    CWeakRef<SLockedQueue> m_LQueue;
    unsigned           m_ClientHostAddr;

    // Per client data
    /// For private DB
    unsigned        m_QueueDbAccessCounter;
    /// Private database (for long running sessions)
    auto_ptr<SQueueDB>         m_QueueDB;
    /// Private cursor
    auto_ptr<CBDB_FileCursor>  m_QueueDB_Cursor;

}; // CQueue


/// Queue database manager
///
/// @internal
///
class CQueueIterator;
class CQueueCollection
{
public:
    typedef map<string, CRef<SLockedQueue> > TQueueMap;
    typedef CQueueIterator iterator;
    typedef CQueueIterator const_iterator; // HACK

public:
    CQueueCollection(const CQueueDataBase& db);
    ~CQueueCollection();

    void Close();

    CRef<SLockedQueue> GetLockedQueue(const string& qname) const;
    bool QueueExists(const string& qname) const;

    /// Collection takes ownership of queue
    SLockedQueue& AddQueue(const string& name, SLockedQueue* queue);
    // Remove queue from collection, notifying every interested party
    // through week reference mechanism.
    bool RemoveQueue(const string& name);

    CQueueIterator begin() const;
    CQueueIterator end() const;

    size_t GetSize() const { return m_QMap.size(); }

private:
    CQueueCollection(const CQueueCollection&);
    CQueueCollection& operator=(const CQueueCollection&);
private:
    friend class CQueueIterator;
    const CQueueDataBase&  m_QueueDataBase;
    TQueueMap              m_QMap;
    mutable CRWLock        m_Lock;
};



class CQueueIterator
{
public:
    CQueueIterator(const CQueueDataBase& db,
        CQueueCollection::TQueueMap::const_iterator iter, CRWLock* lock);
    // MSVC8 (Studio 2005) can not (or does not want to) perform
    // copy constructor optimization on return, thus it needs
    // explicit copy constructor because CQueue does not have it.
    CQueueIterator(const CQueueIterator& rhs);
    ~CQueueIterator();
    CQueue& operator*();
    const string GetName();
    void operator++();
    bool operator==(const CQueueIterator& rhs) {
        return m_Iter == rhs.m_Iter;
    }
    bool operator!=(const CQueueIterator& rhs) {
        return m_Iter != rhs.m_Iter;
    }
private:
    const CQueueDataBase&                       m_QueueDataBase;
    CQueueCollection::TQueueMap::const_iterator m_Iter;
    CQueue                                      m_Queue;
    mutable CRWLock*                            m_Lock;
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

    /// @param path
    ///    Path to the database
    /// @param cache_ram_size
    ///    Size of database cache
    /// @param max_locks
    ///    Number of locks and lock objects
    /// @param log_mem_size
    ///    Size of in-memory LOG (when 0 log is put to disk)
    ///    In memory LOG is not durable, put it to memory
    ///    only if you need better performance 
    /// @param max_trans
    ///    Maximum number of active transactions
    void Open(const string& path, 
              unsigned      cache_ram_size,
              unsigned      max_locks,
              unsigned      log_mem_size,
              unsigned      max_trans);

    void Configure(const IRegistry& reg, unsigned* min_run_timeout);

    void MountQueue(const string& qname,
                    const string& qclass,
                    const SQueueParameters& params);

    void CreateQueue(const string& qname, const string& qclass,
                     const string& comment = "");
    void DeleteQueue(const string& qname);
    void QueueInfo(const string& qname, int& kind,
                   string* qclass, string* comment);
    void GetQueueNames(string *list, const string& sep) const;

    void UpdateQueueParameters(const string& qname,
                               const SQueueParameters& params);
    void UpdateQueueLBParameters(const string& qname,
                                 const SQueueParameters& params);
    void Close(void);
    bool QueueExists(const string& qname) const 
                { return m_QueueCollection.QueueExists(qname); }

    /// Remove old jobs
    void Purge(void);
    void StopPurge(void);
    void RunPurgeThread(void);
    void StopPurgeThread(void);

    /// Notify all listeners
    void NotifyListeners(void);
    void RunNotifThread(void);
    void StopNotifThread(void);

    void CheckExecutionTimeout(void);
    void RunExecutionWatcherThread(unsigned run_delay);
    void StopExecutionWatcherThread(void);


    void SetUdpPort(unsigned short port) { m_UdpPort = port; }
    unsigned short GetUdpPort(void) const { return m_UdpPort; }

    /// Force transaction checkpoint
    void TransactionCheckPoint();

protected:
    /// get next job id (counter increment)
    unsigned int GetNextId();

    /// Returns first id for the batch
    unsigned int GetNextIdBatch(unsigned count);

private:
    friend class CQueue;
    unsigned x_PurgeUnconditional(unsigned batch_size);
    void x_Returned2Pending(void);
    void x_OptimizeStatusMatrix(void);
    void x_OptimizeAffinity(void);
    bool x_CheckStopPurge(void);
    void x_CleanParamMap(void);

    CBDB_Env*                       m_Env;
    string                          m_Path;
    string                          m_Name;

    mutable CFastMutex              m_ConfigureLock;
    typedef map<string, SQueueParameters*> TQueueParamMap;
    TQueueParamMap                  m_QueueParamMap;
    SQueueDescriptionDB             m_QueueDescriptionDB;
    CQueueCollection                m_QueueCollection;

    bm::bvector<>                   m_UsedIds; /// id access locker
    CRef<CJobQueueCleanerThread>    m_PurgeThread;

    bool                 m_StopPurge;         ///< Purge stop flag
    CFastMutex           m_PurgeLock;
    unsigned int         m_DeleteChkPointCnt; ///< trans. checkpnt counter
    unsigned int         m_FreeStatusMemCnt;  ///< Free memory counter
    time_t               m_LastFreeMem;       ///< time of the last memory opt
    time_t               m_LastR2P;           ///< Return 2 Pending timestamp
    unsigned short       m_UdpPort;           ///< UDP notification port      

    CRef<CJobNotificationThread>             m_NotifThread;
    CRef<CJobQueueExecutionWatcherThread>    m_ExeWatchThread;

}; // CQueueDataBase


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.67  2007/01/10 21:23:00  joukovv
 * Job id is per queue, not per server. Deletion of expired jobs use the same
 * db mechanism as drop queue - delayed background deletion.
 *
 * Revision 1.66  2007/01/08 21:04:09  joukovv
 * Fast notification of idle node cluster implemented.
 *
 * Revision 1.65  2007/01/02 18:50:54  joukovv
 * Queue deletion implemented (does not delete actual database files - need a
 * method of enumerating them). Draft implementation of weak reference. Minor
 * corrections.
 *
 * Revision 1.64  2006/12/07 22:58:10  joukovv
 * comment and kind added to queue database
 *
 * Revision 1.63  2006/12/07 19:28:48  joukovv
 * Build errors fixed, queue info command introduced.
 *
 * Revision 1.62  2006/12/07 16:22:11  joukovv
 * Transparent server-to-client exception report implemented. Version control
 * bug fixed.
 *
 * Revision 1.61  2006/12/01 00:10:58  joukovv
 * Dynamic queue creation implemented.
 *
 * Revision 1.60  2006/11/28 18:03:49  joukovv
 * MSVC8 build fix, grid_worker_sample idle task commented out.
 *
 * Revision 1.59  2006/11/27 16:46:21  joukovv
 * Iterator to CQueueCollection introduced to decouple it with CQueueDataBase;
 * un-nested CQueue from CQueueDataBase; instrumented code to count job
 * throughput average.
 *
 * Revision 1.58  2006/11/13 22:49:15  joukovv
 * Background job deletion code corrected.
 *
 * Revision 1.57  2006/10/31 19:35:26  joukovv
 * Queue creation and reading of its parameters decoupled. Code reorganized to
 * reduce coupling in general. Preparing for queue-on-demand.
 *
 * Revision 1.56  2006/10/19 20:38:20  joukovv
 * Works in thread-per-request mode. Errors in BDB layer fixed.
 *
 * Revision 1.55  2006/10/03 14:56:56  joukovv
 * Delayed job deletion implemented, code restructured preparing to move to
 * thread-per-request model.
 *
 * Revision 1.54  2006/09/21 21:28:59  joukovv
 * Consistency of memory state and database strengthened, ability to retry failed
 * jobs on different nodes (and corresponding queue parameter, failed_retries)
 * added, overall code regularization performed.
 *
 * Revision 1.53  2006/07/19 15:53:34  kuznets
 * Extended database size to accomodate escaped strings
 *
 * Revision 1.52  2006/06/29 21:09:33  kuznets
 * Added queue dump by status(pending, running, etc)
 *
 * Revision 1.51  2006/06/27 15:39:42  kuznets
 * Added int mask to jobs to carry flags(like exclusive)
 *
 * Revision 1.50  2006/06/07 13:00:01  kuznets
 * Implemented command to get status summary based on affinity token
 *
 * Revision 1.49  2006/05/22 15:19:40  kuznets
 * Added return code to failure reporting
 *
 * Revision 1.48  2006/05/22 12:36:33  kuznets
 * Added output argument to PutFailure
 *
 * Revision 1.47  2006/05/10 15:59:06  kuznets
 * Implemented NS call to delay job expiration
 *
 * Revision 1.46  2006/05/08 11:24:52  kuznets
 * Implemented file redirection cout/cerr for worker nodes
 *
 * Revision 1.45  2006/04/17 15:46:54  kuznets
 * Added option to remove job when it is done (similar to LSF)
 *
 * Revision 1.44  2006/03/30 17:38:55  kuznets
 * Set max. transactions according to number of active threads
 *
 * Revision 1.43  2006/03/17 14:25:29  kuznets
 * Force reschedule (to re-try failed jobs)
 *
 * Revision 1.42  2006/03/16 19:37:28  kuznets
 * Fixed possible race condition between client and worker
 *
 * Revision 1.41  2006/03/13 16:01:36  kuznets
 * Fixed queue truncation (transaction log overflow). Added commands to print queue selectively
 *
 * Revision 1.40  2006/02/23 20:05:10  kuznets
 * Added grid client registration-unregistration
 *
 * Revision 1.39  2006/02/23 15:45:04  kuznets
 * Added more frequent and non-intrusive memory optimization of status matrix
 *
 * Revision 1.38  2006/02/21 14:44:57  kuznets
 * Bug fixes, improvements in statistics
 *
 * Revision 1.37  2006/02/08 15:17:33  kuznets
 * Tuning and bug fixing of job affinity
 *
 * Revision 1.36  2006/02/06 14:10:29  kuznets
 * Added job affinity
 *
 * Revision 1.35  2005/08/30 14:19:33  kuznets
 * Added thread-local database for better scalability
 *
 * Revision 1.34  2005/08/26 12:36:10  kuznets
 * Performance optimization
 *
 * Revision 1.33  2005/08/22 14:01:58  kuznets
 * Added JobExchange command
 *
 * Revision 1.32  2005/08/18 16:24:32  kuznets
 * Optimized job retrival out o the bit matrix
 *
 * Revision 1.31  2005/08/15 13:29:50  kuznets
 * Implemented batch job submission
 *
 * Revision 1.30  2005/07/25 16:14:31  kuznets
 * Revisited LB parameters, added options to compute job stall delay as fraction of AVG runtime
 *
 * Revision 1.29  2005/07/14 13:12:56  kuznets
 * Added load balancer
 *
 * Revision 1.28  2005/06/21 16:00:22  kuznets
 * Added archival dump of all deleted records
 *
 * Revision 1.27  2005/06/20 13:31:08  kuznets
 * Added access control for job submitters and worker nodes
 *
 * Revision 1.26  2005/05/16 16:21:26  kuznets
 * Added available queues listing
 *
 * Revision 1.25  2005/05/12 18:37:33  kuznets
 * Implemented config reload
 *
 * Revision 1.24  2005/05/04 19:09:43  kuznets
 * Added queue dumping
 *
 * Revision 1.23  2005/05/02 14:44:40  kuznets
 * Implemented remote monitoring
 *
 * Revision 1.22  2005/04/28 17:40:26  kuznets
 * Added functions to rack down forgotten nodes
 *
 * Revision 1.21  2005/04/20 15:59:33  kuznets
 * Progress message to Submit
 *
 * Revision 1.20  2005/04/19 19:34:05  kuznets
 * Adde progress report messages
 *
 * Revision 1.19  2005/04/11 13:53:25  kuznets
 * Code cleanup
 *
 * Revision 1.18  2005/04/06 12:39:55  kuznets
 * + client version control
 *
 * Revision 1.17  2005/03/29 16:48:28  kuznets
 * Use atomic counter for job id assignment
 *
 * Revision 1.16  2005/03/22 19:02:54  kuznets
 * Reflecting chnages in connect layout
 *
 * Revision 1.15  2005/03/22 16:14:49  kuznets
 * +PrintStat()
 *
 * Revision 1.14  2005/03/21 13:07:28  kuznets
 * Added some statistical functions
 *
 * Revision 1.13  2005/03/17 20:37:07  kuznets
 * Implemented FPUT
 *
 * Revision 1.12  2005/03/15 20:14:30  kuznets
 * Implemented notification to client waiting for job
 *
 * Revision 1.11  2005/03/15 14:52:39  kuznets
 * Better datagram socket management, DropJob implemenetation
 *
 * Revision 1.10  2005/03/10 14:19:57  kuznets
 * Implemented individual run timeouts
 *
 * Revision 1.9  2005/03/09 17:37:17  kuznets
 * Added node notification thread and execution control timeline
 *
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

