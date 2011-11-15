#ifndef NETSCHEDULE_NS_QUEUE__HPP
#define NETSCHEDULE_NS_QUEUE__HPP

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
 *
 */

#include <corelib/ncbistl.hpp>
#include <corelib/ncbireg.hpp>

#include <util/thread_nonstop.hpp>
#include <util/time_line.hpp>

#include <connect/server_monitor.hpp>

#include "ns_types.hpp"
#include "ns_util.hpp"
#include "ns_format.hpp"
#include "ns_db.hpp"
#include "background_host.hpp"
#include "job.hpp"
#include "job_status.hpp"
#include "queue_vc.hpp"
#include "access_list.hpp"
#include "ns_affinity.hpp"
#include "ns_queue_db_block.hpp"
#include "ns_queue_parameters.hpp"
#include "ns_clients_registry.hpp"
#include "ns_notifications.hpp"
#include "queue_clean_thread.hpp"
#include "ns_statistics_thread.hpp"

#include <deque>
#include <map>

BEGIN_NCBI_SCOPE

class CNetScheduleServer;
class CNetScheduleHandler;


class CQueue;
class CQueueEnumCursor : public CBDB_FileCursor
{
public:
    CQueueEnumCursor(CQueue* queue);
};


// slight violation of naming convention for porting to util/time_line
typedef CTimeLine<TNSBitVector>     CJobTimeLine;

/// Mutex protected Queue database with job status FSM
///
/// Class holds the queue database (open files and indexes),
/// thread sync mutexes and classes auxiliary queue management concepts
/// (like affinity and job status bit-matrix)
///
/// @internal
///
class CQueue : public CObjectEx
{
public:
    enum EQueueKind {
        eKindStatic  = 0,
        eKindDynamic = 1
    };
    typedef int TQueueKind;

public:
    // Constructor/destructor
    CQueue(CRequestExecutor&     executor,
           const string&         queue_name,
           const string&         qclass_name,
           TQueueKind            queue_kind,
           CNetScheduleServer *  server);
    ~CQueue();

    void Attach(SQueueDbBlock* block);
    void Detach();
    int  GetPos() const { return m_QueueDbBlock->pos; }

    void x_ReadFieldInfo(void);

    // Thread-safe parameter access
    typedef list<pair<string, string> > TParameterList;
    void SetParameters(const SQueueParameters& params);
    TParameterList GetParameters() const;
    int GetTimeout() const;
    int GetRunTimeout() const;
    int GetRunTimeoutPrecision() const;
    unsigned GetFailedRetries() const;
    time_t GetBlacklistTime() const;
    time_t GetEmptyLifetime() const;
    bool GetDenyAccessViolations() const;
    bool IsVersionControl() const;
    bool IsMatchingClient(const CQueueClientInfo& cinfo) const;
    bool IsSubmitAllowed(unsigned host) const;
    bool IsWorkerAllowed(unsigned host) const;
    void GetMaxIOSizes(unsigned int &  max_input_size,
                       unsigned int &  max_output_size) const;

    ////
    // Status matrix related
    unsigned LoadStatusMatrix();

    const string& GetQueueName() const {
        return m_QueueName;
    }

    string DecorateJobId(unsigned job_id) {
        return m_QueueName + '/' + MakeKey(job_id);
    }

    // Submit job, return numeric job id
    unsigned int  Submit(const CNSClientId &  client,
                         CJob &               job,
                         const string &       aff_token);

    /// Submit job batch
    /// @return ID of the first job, second is first_id+1 etc.
    unsigned SubmitBatch(const CNSClientId &             client,
                         vector< pair<CJob, string> > &  batch);

    TJobStatus  PutResultGetJob(const CNSClientId &        client,
                                // PutResult parameters
                                unsigned                   done_job_id,
                                int                        ret_code,
                                const string *             output,
                                // GetJob parameters
                                const list<string> *       aff_list,
                                CJob *                     new_job);

    TJobStatus  PutResult(const CNSClientId &  client,
                          time_t               curr,
                          unsigned             job_id,
                          int                  ret_code,
                          const string *       output);

    void GetJob(const CNSClientId &     client,
                time_t                  curr,
                const list<string> *    aff_list,
                CJob *                  new_job);

    TJobStatus  JobDelayExpiration(unsigned        job_id,
                                   time_t          tm);

    // Worker node-specific methods
    bool PutProgressMessage(unsigned      job_id,
                            const string& msg);

    TJobStatus  ReturnJob(const CNSClientId &     client,
                          unsigned int            peer_addr);

    /// 0 - job not found
    unsigned int  ReadJobFromDB(unsigned int  job_id, CJob &  job);

    /// Remove all jobs
    void Truncate(void);

    /// Cancel job execution (job stays in special Canceled state)
    /// Returns the previous job status
    TJobStatus  Cancel(const CNSClientId &  client,
                       unsigned int         job_id);

    TJobStatus GetJobStatus(unsigned job_id) const;

    /// count status snapshot for affinity token
    /// returns false if affinity token not found
    bool CountStatus(CJobStatusTracker::TStatusSummaryMap* status_map,
                     const string&                         affinity_token);

    /// Is the queue empty long enough to be deleted?
    bool IsExpired();

    /// get next job id (counter increment)
    unsigned int GetNextId();
    /// Returns first id for the batch
    unsigned int GetNextIdBatch(unsigned count);

    // Read-Confirm stage
    /// Request done jobs for reading with timeout
    void GetJobForReading(const CNSClientId &   client,
                          unsigned int          read_timeout,
                          CJob *                job);
    /// Confirm reading of these jobs
    TJobStatus  ConfirmReadingJob(const CNSClientId &   client,
                                  unsigned int    job_id,
                                  const string &  auth_token);
    /// Fail (negative acknowledge) reading of these jobs
    TJobStatus  FailReadingJob(const CNSClientId &   client,
                               unsigned int          job_id,
                               const string &        auth_token);
    /// Return jobs to unread state without reservation
    TJobStatus  ReturnReadingJob(const CNSClientId &   client,
                                 unsigned int          job_id,
                                 const string &        auth_token);

    /// Erase job from all structures, request delayed db deletion
    void EraseJob(unsigned job_id);

    /// Erase jobs from all structures, request delayed db deletion
    void Erase(const TNSBitVector& job_ids);

    void MarkForDeletion() {
        m_DeleteDatabase = true;
    }

    // Optimize bitvectors
    void OptimizeMem();

    /// Find the pending job.
    /// This method takes into account jobs available
    /// in the job status matrix and current
    /// worker node affinity association.
    /// Sync.Locks: affinity map lock, main queue lock, status matrix
    ///
    /// @return job_id
    unsigned
    FindPendingJob(const CNSClientId &    client,
                   const list<string> &   aff_list,
                   time_t                 curr);

    /// Prepares affinity list of affinities accompanied by number
    /// of jobs belonging to them, e.g.
    /// "a1=500&a2=600a3=200"
    ///
    /// @return
    ///     affinity preference string
    string GetAffinityList();

    TJobStatus FailJob(const CNSClientId &    client,
                       unsigned               job_id,
                       const string &         err_msg,
                       const string &         output,
                       int                    ret_code);

    string  GetAffinityTokenByID(unsigned int  aff_id) const;

    /// Specified status is OR-ed with the target vector
    void JobsWithStatus(TJobStatus    status,
                        TNSBitVector* bv) const;

    void ClearWorkerNode(const CNSClientId &  client);

    void NotifyListeners(bool unconditional, unsigned aff_id);
    void PrintClientsList(CNetScheduleHandler &  handler,
                          bool                   verbose) const;

    /// Check execution timeout. Now checks reading timeout as well.
    /// All jobs failed to execute, go back to pending
    void CheckExecutionTimeout(bool logging);

    /// Check timeout for a job
    void x_CheckExecutionTimeout(unsigned  queue_run_timeout,
                                 unsigned  job_id,
                                 time_t    curr_time,
                                 bool      logging);

    // Checks up to given # of jobs at the given status for expiration and
    // marks up to given # of jobs for deletion.
    // Returns the # of performed scans, the # of jobs marked for deletion and
    // the last scanned job id.
    SPurgeAttributes CheckJobsExpiry(time_t             current_time,
                                     SPurgeAttributes   attributes,
                                     TJobStatus         status);

    void TimeLineMove(unsigned job_id, time_t old_time, time_t new_time);
    void TimeLineAdd(unsigned job_id, time_t job_time);
    void TimeLineRemove(unsigned job_id);
    void TimeLineExchange(unsigned remove_job_id,
                          unsigned add_job_id,
                          time_t   new_time);

    unsigned int  DeleteBatch(void);

    CBDB_FileCursor& GetEventsCursor();

    void PrintSubmHosts(CNetScheduleHandler &  handler) const;
    void PrintWNodeHosts(CNetScheduleHandler &  handler) const;

    void PrintQueue(CNetScheduleHandler &   handler,
                    TJobStatus              job_status);

    /// Dump a single job
    size_t PrintJobDbStat(CNetScheduleHandler &   handler,
                          unsigned                job_id);
    /// Dump all job records
    void PrintAllJobDbStat(CNetScheduleHandler &   handler);

    unsigned CountStatus(TJobStatus) const;
    void StatusStatistics(TJobStatus                  status,
                          TNSBitVector::statistics *  st) const;


    string MakeKey(unsigned job_id) const
    { return m_KeyGenerator.GenerateV1(job_id); }

    void TouchClientsRegistry(const CNSClientId &  client);
    void ResetRunningDueToClear(const CNSClientId &   client,
                                const TNSBitVector &  jobs);
    void ResetReadingDueToClear(const CNSClientId &   client,
                                const TNSBitVector &  jobs);
    void ResetRunningDueToNewSession(const CNSClientId &   client,
                                     const TNSBitVector &  jobs);
    void ResetReadingDueToNewSession(const CNSClientId &   client,
                                     const TNSBitVector &  jobs);

    void RegisterGetListener(const CNSClientId &  client,
                             unsigned short       port,
                             unsigned int         timeout);
    void UnregisterGetListener(const CNSClientId &  client);
    void PrintStatistics(void);
    void CountTransition(CNetScheduleAPI::EJobStatus  from,
                         CNetScheduleAPI::EJobStatus  to)
    { m_StatisticsCounters.CountTransition(from, to); }

private:
    friend class CNSTransaction;
    CBDB_Env &  GetEnv() { return *m_QueueDbBlock->job_db.GetEnv(); }

    TJobStatus  x_ChangeReadingStatus(const CNSClientId &  client,
                                      unsigned int         job_id,
                                      const string &       auth_token,
                                      TJobStatus           target_status);

    /// @return TRUE if job record has been found and updated
    bool x_UpdateDB_PutResultNoLock(unsigned                job_id,
                                    time_t                  curr,
                                    int                     ret_code,
                                    const string &          output,
                                    CJob &                  job,
                                    const CNSClientId &     client);

    bool  x_UpdateDB_GetJobNoLock(const CNSClientId &  client,
                                  time_t               curr,
                                  unsigned int         job_id,
                                  CJob &               job);

    void x_PrintJobStat(CNetScheduleHandler &   handler,
                        const CJob&             job,
                        unsigned                queue_run_timeout);
    void x_PrintShortJobStat(CNetScheduleHandler &  handler,
                             const CJob&            job);
    void x_LogSubmit(const CJob &   job,
                     unsigned int   batch_id,
                     bool           separate_request);
    void x_UpdateStartFromCounter(void);
    unsigned int x_ReadStartFromCounter(void);
    void x_DeleteJobEvents(unsigned int  job_id);
    TJobStatus x_ResetDueTo(const CNSClientId &   client,
                            unsigned int          job_id,
                            time_t                current_time,
                            TJobStatus            status_from,
                            CJobEvent::EJobEvent  event_type);

private:
    friend class CJob;
    friend class CQueueEnumCursor;
    friend class CQueueParamAccessor;

    CJobStatusTracker           m_StatusTracker;    ///< status FSA

    // Timeline object to control job execution timeout
    CJobTimeLine*               m_RunTimeLine;
    CRWLock                     m_RunTimeLineLock;

    // Notifications support
    CNSNotificationList         m_NotificationsList;

    // Should we delete db upon close?
    bool                        m_DeleteDatabase;

    // Background executor
    CRequestExecutor&           m_Executor;

    string                      m_QueueName;
    string                      m_QueueClass;      ///< Parameter class
    TQueueKind                  m_Kind;            ///< 0 - static, 1 - dynamic

    SQueueDbBlock*              m_QueueDbBlock;

    auto_ptr<CBDB_FileCursor>   m_EventsCursor;    ///< DB cursor for EventsDB

    CFastMutex                  m_OperationLock;   ///< Lock for a queue operations.

    ///< When the queue became empty, guarded by 'm_OperationLock'
    time_t                      m_BecameEmpty;

    // Registry of all the clients for the queue
    CNSClientsRegistry          m_ClientsRegistry;

    // Registry of all the job affinities
    CNSAffinityRegistry         m_AffinityRegistry;

    /// Last valid id for queue
    unsigned int                m_LastId;      // Last used job ID
    unsigned int                m_SavedId;     // The ID we will start next time
                                               // the netschedule is loaded
    CFastMutex                  m_lastIdLock;

    /// Lock for deleted jobs vectors
    CFastMutex                   m_JobsToDeleteLock;
    /// Vector of jobs to be deleted from db unconditionally
    /// keeps jobs still to be deleted from main DB
    TNSBitVector                 m_JobsToDelete;

    // Configurable queue parameters
    // When modifying this, modify all places marked with PARAMETERS
    mutable CRWLock              m_ParamLock;
    int                          m_Timeout;         ///< Result exp. timeout
    double                       m_NotifyTimeout;   ///< Notification interval
    int                          m_RunTimeout;      ///< Execution timeout
    /// Its precision, set at startup only, not reconfigurable
    int                          m_RunTimeoutPrecision;
    /// How many attempts to make on different nodes before failure
    unsigned                     m_FailedRetries;
    /// How long a job lives in blacklist
    time_t                       m_BlacklistTime;
    /// How long to live after becoming empty, if -1 - infinitely
    time_t                       m_EmptyLifetime;
    unsigned                     m_MaxInputSize;
    unsigned                     m_MaxOutputSize;
    bool                         m_DenyAccessViolations;
    /// Client program version control
    CQueueClientInfoList         m_ProgramVersionList;
    /// Host access list for job submission
    CNetSchedule_AccessList      m_SubmHosts;
    /// Host access list for job execution (workers)
    CNetSchedule_AccessList      m_WnodeHosts;

    CNetScheduleKeyGenerator     m_KeyGenerator;
    const bool &                 m_Log;
    const bool &                 m_LogBatchEachJob;

    CStatisticsCounters          m_StatisticsCounters;
};


// Thread-safe parameter access. The majority of parameters are single word,
// so if you need a single parameter, it is safe to use these methods, which
// do not lock anything. In such cases, where the parameter is not single-word,
// we lock m_ParamLock for reading. In cases where you need more than one
// parameter, to provide consistency use CQueueParamAccessor, which is a smart
// guard around the parameter block.
inline int CQueue::GetTimeout() const
{
    return m_Timeout;
}
inline int CQueue::GetRunTimeout()  const
{
    return m_RunTimeout;
}
inline int CQueue::GetRunTimeoutPrecision() const
{
    return m_RunTimeoutPrecision;
}
inline unsigned CQueue::GetFailedRetries() const
{
    return m_FailedRetries;
}
inline time_t CQueue::GetBlacklistTime() const
{
    // On some platforms time_t is 64-bit disregarding of
    // processor word size, so we need to lock parameters
    CReadLockGuard guard(m_ParamLock);
    return m_BlacklistTime;
}
inline time_t CQueue::GetEmptyLifetime() const
{
    CReadLockGuard guard(m_ParamLock);
    return m_EmptyLifetime;
}
inline bool CQueue::GetDenyAccessViolations() const
{
    return m_DenyAccessViolations;
}
inline bool CQueue::IsVersionControl() const
{
    CReadLockGuard guard(m_ParamLock);
    return m_ProgramVersionList.IsConfigured();
}
inline bool CQueue::IsMatchingClient(const CQueueClientInfo& cinfo) const
{
    CReadLockGuard guard(m_ParamLock);
    return m_ProgramVersionList.IsMatchingClient(cinfo);
}
inline bool CQueue::IsSubmitAllowed(unsigned host) const
{
    CReadLockGuard guard(m_ParamLock);
    return host == 0  ||  m_SubmHosts.IsAllowed(host);
}
inline bool CQueue::IsWorkerAllowed(unsigned host) const
{
    CReadLockGuard guard(m_ParamLock);
    return host == 0  ||  m_WnodeHosts.IsAllowed(host);
}



// Application specific defaults provider for DB transaction
class CNSTransaction : public CBDB_Transaction
{
public:
    CNSTransaction(CQueue *              queue,
                   int                   what_tables = eAllTables,
                   ETransSync            tsync = eEnvDefault,
                   EKeepFileAssociation  assoc = eNoAssociation)
        : CBDB_Transaction(queue->GetEnv(), tsync, assoc)
    {
        if (what_tables & eJobTable)
            queue->m_QueueDbBlock->job_db.SetTransaction(this);

        if (what_tables & eJobInfoTable)
            queue->m_QueueDbBlock->job_info_db.SetTransaction(this);

        if (what_tables & eJobEventsTable)
            queue->m_QueueDbBlock->events_db.SetTransaction(this);

        if (what_tables & eAffinityTable)
            queue->m_QueueDbBlock->aff_dict_db.SetTransaction(this);
    }
};


END_NCBI_SCOPE

#endif /* NETSCHEDULE_NS_QUEUE__HPP */

