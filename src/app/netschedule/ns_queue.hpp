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
#include "ns_statistics_counters.hpp"
#include "ns_group.hpp"
#include "ns_gc_registry.hpp"
#include "ns_precise_time.hpp"

#include <deque>
#include <map>

BEGIN_NCBI_SCOPE

class CNetScheduleServer;
class CNSRollbackInterface;


class CQueue;
class CQueueEnumCursor : public CBDB_FileCursor
{
public:
    CQueueEnumCursor(CQueue *      queue,
                     unsigned int  start_after);
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
    enum EPauseStatus {
        eNoPause              = 0,
        ePauseWithPullback    = 1,
        ePauseWithoutPullback = 2
    };
    typedef int TPauseStatus;

public:
    enum EJobReturnOption {
        eWithBlacklist    = 0,
        eWithoutBlacklist = 1,
        eRollback         = 2
    };
    typedef int TJobReturnOption;

public:
    // Constructor/destructor
    CQueue(CRequestExecutor&     executor,
           const string&         queue_name,
           TQueueKind            queue_kind,
           CNetScheduleServer *  server,
           CQueueDataBase &      qdb);
    ~CQueue();

    void Attach(SQueueDbBlock* block);
    int  GetPos() const { return m_QueueDbBlock->pos; }

    // Thread-safe parameter access
    typedef list<pair<string, string> > TParameterList;
    void SetParameters(const SQueueParameters& params);
    TParameterList GetParameters() const;
    CNSPreciseTime GetTimeout() const;
    CNSPreciseTime GetRunTimeout() const;
    CNSPreciseTime GetPendingTimeout() const;
    CNSPreciseTime  GetMaxPendingWaitTimeout() const;
    unsigned GetFailedRetries() const;
    bool IsSubmitAllowed(unsigned host) const;
    bool IsWorkerAllowed(unsigned host) const;
    bool IsProgramAllowed(const string &  program_name) const;
    void GetMaxIOSizesAndLinkedSections(
                unsigned int &  max_input_size,
                unsigned int &  max_output_size,
                map< string, map<string, string> > & linked_sections) const;
    void GetLinkedSections(map< string,
                                map<string, string> > & linked_sections) const;

    bool GetRefuseSubmits(void) const { return m_RefuseSubmits; }
    void SetRefuseSubmits(bool  val)  { m_RefuseSubmits = val;  }
    size_t GetAffSlotsUsed(void) const { return m_AffinityRegistry.size(); }
    size_t GetClientsCount(void) const { return m_ClientsRegistry.size(); }
    size_t GetGroupsCount(void) const { return m_GroupRegistry.size(); }
    size_t GetNotifCount(void) const { return m_NotificationsList.size(); }
    size_t GetGCBacklogCount(void) const
    {
        CFastMutexGuard     guard(m_JobsToDeleteLock);
        return m_JobsToDelete.count();
    }

    // Status matrix related
    unsigned int  LoadStatusMatrix(void);

    const string& GetQueueName() const {
        return m_QueueName;
    }

    string DecorateJob(unsigned int    job_id) const
    { return m_QueueName + "/" + MakeJobKey(job_id); }

    // Submit job, return numeric job id
    unsigned int  Submit(const CNSClientId &        client,
                         CJob &                     job,
                         const string &             aff_token,
                         const string &             group,
                         CNSRollbackInterface * &   rollback_action);

    /// Submit job batch
    /// @return ID of the first job, second is first_id+1 etc.
    unsigned SubmitBatch(const CNSClientId &             client,
                         vector< pair<CJob, string> > &  batch,
                         const string &                  group,
                         CNSRollbackInterface * &        rollback_action);

    TJobStatus  PutResult(const CNSClientId &     client,
                          const CNSPreciseTime &  curr,
                          unsigned int            job_id,
                          const string &          job_key,
                          const string &          auth_token,
                          int                     ret_code,
                          const string &          output);

    bool GetJobOrWait(const CNSClientId &       client,
                      unsigned short            port, // Port the client
                                                      // will wait on
                      unsigned int              timeout,
                      const CNSPreciseTime &    curr,
                      const list<string> *      aff_list,
                      bool                      wnode_affinity,
                      bool                      any_affinity,
                      bool                      exclusive_new_affinity,
                      bool                      new_format,
                      CJob *                    new_job,
                      CNSRollbackInterface * &  rollback_action,
                      string &                  added_pref_aff);

    void CancelWaitGet(const CNSClientId &  client);

    string ChangeAffinity(const CNSClientId &     client,
                          const list<string> &    aff_to_add,
                          const list<string> &    aff_to_del);
    void SetAffinity(const CNSClientId &     client,
                     const list<string> &    aff);
    int  SetClientData(const CNSClientId &  client,
                       const string &  data, int  data_version);

    TJobStatus  JobDelayExpiration(unsigned int            job_id,
                                   const CNSPreciseTime &  tm);

    TJobStatus  GetStatusAndLifetime(unsigned int      job_id,
                                     const string &    job_key,
                                     bool              need_touch,
                                     CNSPreciseTime *  lifetime);

    TJobStatus  SetJobListener(unsigned int            job_id,
                               unsigned int            address,
                               unsigned short          port,
                               const CNSPreciseTime &  timeout,
                               size_t *                last_event_index);

    // Worker node-specific methods
    bool PutProgressMessage(unsigned      job_id,
                            const string& msg);

    TJobStatus  ReturnJob(const CNSClientId &     client,
                          unsigned int            job_id,
                          const string &          job_key,
                          const string &          auth_token,
                          string &                warning,
                          TJobReturnOption        how);

    TJobStatus  ReadAndTouchJob(unsigned int      job_id,
                                const string &    job_key,
                                CJob &            job,
                                CNSPreciseTime *  lifetime);

    // Remove all jobs
    void Truncate(void);

    // Cancel job execution (job stays in special Canceled state)
    // Returns the previous job status
    TJobStatus  Cancel(const CNSClientId &  client,
                       unsigned int         job_id,
                       const string &       job_key,
                       bool                 is_ns_rollback = false);

    void CancelAllJobs(const CNSClientId &  client);
    void CancelGroup(const CNSClientId &  client,
                     const string &       group);

    TJobStatus GetJobStatus(unsigned job_id) const;

    bool IsEmpty() const;

    // get next job id (counter increment)
    unsigned int GetNextId();
    // Returns first id for the batch
    unsigned int GetNextJobIdForBatch(unsigned count);

    // Read-Confirm stage
    // Request done jobs for reading with timeout
    void GetJobForReading(const CNSClientId &       client,
                          const CNSPreciseTime &    read_timeout,
                          const string &            group,
                          CJob *                    job,
                          CNSRollbackInterface * &  rollback_action);
    // Confirm reading of these jobs
    TJobStatus  ConfirmReadingJob(const CNSClientId &   client,
                                  unsigned int          job_id,
                                  const string &        job_key,
                                  const string &        auth_token);
    // Fail (negative acknowledge) reading of these jobs
    TJobStatus  FailReadingJob(const CNSClientId &   client,
                               unsigned int          job_id,
                               const string &        job_key,
                               const string &        auth_token,
                               const string &        err_msg);
    // Return jobs to unread state without reservation
    TJobStatus  ReturnReadingJob(const CNSClientId &   client,
                                 unsigned int          job_id,
                                 const string &        job_key,
                                 const string &        auth_token,
                                 bool                  is_ns_rollback = false);

    // Erase job from all structures, request delayed db deletion
    void EraseJob(unsigned job_id);

    // Optimize bitvectors
    void OptimizeMem();

    // Prepares affinity list of affinities accompanied by number
    // of jobs belonging to them, e.g.
    // "a1=500&a2=600a3=200"
    //
    // @return
    //     affinity preference string
    string GetAffinityList();

    TJobStatus FailJob(const CNSClientId &    client,
                       unsigned int           job_id,
                       const string &         job_key,
                       const string &         auth_token,
                       const string &         err_msg,
                       const string &         output,
                       int                    ret_code,
                       string                 warning);

    string  GetAffinityTokenByID(unsigned int  aff_id) const;

    void ClearWorkerNode(const CNSClientId &  client,
                         bool &               client_was_found,
                         string &             old_session,
                         bool &               pref_affs_were_reset);

    void NotifyListenersPeriodically(const CNSPreciseTime &  current_time);
    CNSPreciseTime NotifyExactListeners(void);
    string PrintClientsList(bool verbose) const;
    string PrintNotificationsList(bool verbose) const;
    string PrintAffinitiesList(bool verbose) const;
    string PrintGroupsList(bool verbose) const;

    // Check execution timeout. Now checks reading timeout as well.
    // All jobs failed to execute, go back to pending
    void CheckExecutionTimeout(bool logging);

    // Checks up to given # of jobs at the given status for expiration and
    // marks up to given # of jobs for deletion. Check no further than the
    // given last_job id
    // Returns the # of performed scans, the # of jobs marked for deletion and
    // the last scanned job id.
    SPurgeAttributes CheckJobsExpiry(const CNSPreciseTime &  current_time,
                                     SPurgeAttributes        attributes,
                                     unsigned int            last_job,
                                     TJobStatus              status);

    void TimeLineMove(unsigned int            job_id,
                      const CNSPreciseTime &  old_time,
                      const CNSPreciseTime &  new_time);
    void TimeLineAdd(unsigned int            job_id,
                     const CNSPreciseTime &  job_time);
    void TimeLineRemove(unsigned int  job_id);
    void TimeLineExchange(unsigned int            remove_job_id,
                          unsigned int            add_job_id,
                          const CNSPreciseTime &  new_time);

    unsigned int  DeleteBatch(unsigned int  max_deleted);
    unsigned int  PurgeAffinities(void);
    unsigned int  PurgeGroups(void);
    void          PurgeWNodes(const CNSPreciseTime &  current_time);
    void          PurgeBlacklistedJobs(void);

    CBDB_FileCursor& GetEventsCursor();

    // Dump a single job
    string PrintJobDbStat(unsigned int job_id);
    // Dump all job records
    string PrintAllJobDbStat(const string &  group,
                             TJobStatus      job_status,
                             unsigned int    start_after_job_id,
                             unsigned int    count);

    unsigned CountStatus(TJobStatus) const;
    void StatusStatistics(TJobStatus                  status,
                          TNSBitVector::statistics *  st) const;

    string MakeJobKey(unsigned int  job_id) const;

    void TouchClientsRegistry(CNSClientId &  client,
                              bool &         client_was_found,
                              bool &         session_was_reset,
                              string &       old_session,
                              bool &         pref_affs_were_reset);
    void RegisterSocketWriteError(const CNSClientId &  client);

    void PrintStatistics(size_t &  aff_count) const;
    unsigned int GetJobsToDeleteCount(void) const;
    string PrintTransitionCounters(void) const;
    string PrintJobsStat(const string &  group_token,
                         const string &  aff_token) const;
    void GetJobsPerState(const string &  group_token,
                         const string &  aff_token,
                         size_t *        jobs) const;
    void CountTransition(CNetScheduleAPI::EJobStatus  from,
                         CNetScheduleAPI::EJobStatus  to)
    { m_StatisticsCounters.CountTransition(from, to); }
    unsigned int  CountActiveJobs(void) const;
    unsigned int  CountAllJobs(void) const
    { return m_StatusTracker.Count(); }
    bool  AnyJobs(void) const
    { return m_StatusTracker.AnyJobs(); }

    void MarkForTruncating(void)
    { m_TruncateAtDetach = true; }

    TPauseStatus GetPauseStatus(void) const
    { return m_PauseStatus; }
    void SetPauseStatus(TPauseStatus  status)
    { m_PauseStatus = status; }

private:
    void x_Detach(void);

    friend class CNSTransaction;
    CBDB_Env &  GetEnv() { return *m_QueueDbBlock->job_db.GetEnv(); }

    TJobStatus  x_ChangeReadingStatus(const CNSClientId &  client,
                                      unsigned int         job_id,
                                      const string &       job_key,
                                      const string &       auth_token,
                                      const string &       err_msg,
                                      TJobStatus           target_status,
                                      bool                 is_ns_rollback = false);

    struct x_SJobPick
    {
        unsigned int    job_id;
        bool            exclusive;
        unsigned int    aff_id;
    };

    x_SJobPick
    x_FindPendingJob(const CNSClientId &    client,
                     const TNSBitVector &   aff_ids,
                     bool                   wnode_affinity,
                     bool                   any_affinity,
                     bool                   exclusive_new_affinity);
    x_SJobPick
    x_FindOutdatedPendingJob(const CNSClientId &  client,
                             unsigned int         picked_earlier);

    void x_UpdateDB_PutResultNoLock(unsigned                job_id,
                                    const string &          job_key,
                                    const string &          auth_token,
                                    const CNSPreciseTime &  curr,
                                    int                     ret_code,
                                    const string &          output,
                                    CJob &                  job,
                                    const CNSClientId &     client);

    bool  x_UpdateDB_GetJobNoLock(const CNSClientId &     client,
                                  const CNSPreciseTime &  curr,
                                  unsigned int            job_id,
                                  CJob &                  job);

    void x_CheckExecutionTimeout(const CNSPreciseTime &  queue_run_timeout,
                                 unsigned                job_id,
                                 const CNSPreciseTime &  curr_time,
                                 bool                    logging);

    void x_LogSubmit(const CJob &       job,
                     const string &     aff,
                     const string &     group);
    void x_UpdateStartFromCounter(void);
    unsigned int x_ReadStartFromCounter(void);
    void x_DeleteJobEvents(unsigned int  job_id);
    void x_ResetRunningDueToClear(const CNSClientId &   client,
                                  const TNSBitVector &  jobs);
    void x_ResetReadingDueToClear(const CNSClientId &   client,
                                  const TNSBitVector &  jobs);
    void x_ResetRunningDueToNewSession(const CNSClientId &   client,
                                       const TNSBitVector &  jobs);
    void x_ResetReadingDueToNewSession(const CNSClientId &   client,
                                       const TNSBitVector &  jobs);
    TJobStatus x_ResetDueTo(const CNSClientId &     client,
                            unsigned int            job_id,
                            const CNSPreciseTime &  current_time,
                            TJobStatus              status_from,
                            CJobEvent::EJobEvent    event_type);

    void x_RegisterGetListener(const CNSClientId &   client,
                               unsigned short        port,
                               unsigned int          timeout,
                               const TNSBitVector &  aff_ids,
                               bool                  wnode_aff,
                               bool                  any_aff,
                               bool                  exclusive_new_affinity,
                               bool                  new_format);
    bool x_UnregisterGetListener(const CNSClientId &  client,
                                 unsigned short       port);

    /// Erase jobs from all structures, request delayed db deletion
    void x_Erase(const TNSBitVector& job_ids);

    string x_DumpJobs(const TNSBitVector &   jobs_to_dump,
                      unsigned int           start_after_job_id,
                      unsigned int           count);
    void x_CancelJobs(const CNSClientId &   client,
                      const TNSBitVector &  jobs_to_cancel);
    CNSPreciseTime x_GetEstimatedJobLifetime(unsigned int   job_id,
                                             TJobStatus     status) const;
    CNSPreciseTime x_GetSubmitTime(unsigned int  job_id);

private:
    friend class CJob;
    friend class CQueueEnumCursor;
    friend class CQueueParamAccessor;

    CNetScheduleServer *        m_Server;
    CJobStatusTracker           m_StatusTracker;    // status FSA
    CQueueDataBase &            m_QueueDB;

    // Timeline object to control job execution timeout
    CJobTimeLine*               m_RunTimeLine;
    CRWLock                     m_RunTimeLineLock;

    // Background executor
    CRequestExecutor&           m_Executor;

    string                      m_QueueName;
    TQueueKind                  m_Kind;            // 0 - static, 1 - dynamic

    SQueueDbBlock *             m_QueueDbBlock;
    bool                        m_TruncateAtDetach;

    auto_ptr<CBDB_FileCursor>   m_EventsCursor;    // DB cursor for EventsDB

    // Lock for a queue operations
    mutable CFastMutex          m_OperationLock;

    // Registry of all the clients for the queue
    CNSClientsRegistry          m_ClientsRegistry;

    // Registry of all the job affinities
    CNSAffinityRegistry         m_AffinityRegistry;

    // Last valid id for queue
    unsigned int                m_LastId;      // Last used job ID
    unsigned int                m_SavedId;     // The ID we will start next time
                                               // the netschedule is loaded
    CFastMutex                  m_LastIdLock;

    // Lock for deleted jobs vectors
    mutable CFastMutex          m_JobsToDeleteLock;
    // Vector of jobs to be deleted from db unconditionally
    // keeps jobs still to be deleted from main DB
    TNSBitVector                m_JobsToDelete;
    unsigned int                m_JobsToDeleteOps;

    // Vector of jobs which have been set for notifications
    TNSBitVector                m_JobsToNotify;

    // Configurable queue parameters
    mutable CFastMutex          m_ParamLock;
    CNSPreciseTime              m_Timeout;         // Result exp. timeout
    CNSPreciseTime              m_RunTimeout;      // Execution timeout
    // Its precision, set at startup only, not reconfigurable
    CNSPreciseTime              m_RunTimeoutPrecision;
    // How many attempts to make on different nodes before failure
    unsigned                    m_FailedRetries;
    CNSPreciseTime              m_BlacklistTime;
    unsigned                    m_MaxInputSize;
    unsigned                    m_MaxOutputSize;
    CNSPreciseTime              m_WNodeTimeout;
    CNSPreciseTime              m_PendingTimeout;
    CNSPreciseTime              m_MaxPendingWaitTimeout;
    // Client program version control
    CQueueClientInfoList        m_ProgramVersionList;
    // Host access list for job submission
    CNetScheduleAccessList      m_SubmHosts;
    // Host access list for job execution (workers)
    CNetScheduleAccessList      m_WnodeHosts;

    CNetScheduleKeyGenerator    m_KeyGenerator;

    const bool &                m_Log;
    const bool &                m_LogBatchEachJob;

    bool                        m_RefuseSubmits;

    CStatisticsCounters         m_StatisticsCounters;

    unsigned int                m_MaxAffinities;
    unsigned int                m_AffinityHighMarkPercentage;
    unsigned int                m_AffinityLowMarkPercentage;
    unsigned int                m_AffinityHighRemoval;
    unsigned int                m_AffinityLowRemoval;
    unsigned int                m_AffinityDirtPercentage;

    // Notifications support
    CNSNotificationList         m_NotificationsList;
    CNSPreciseTime              m_NotifHifreqInterval;
    CNSPreciseTime              m_NotifHifreqPeriod;
    unsigned int                m_NotifLofreqMult;
    CNSPreciseTime              m_HandicapTimeout;

    unsigned int                m_DumpBufferSize;
    unsigned int                m_DumpClientBufferSize;
    unsigned int                m_DumpAffBufferSize;
    unsigned int                m_DumpGroupBufferSize;
    bool                        m_ScrambleJobKeys;
    map<string, string>         m_LinkedSections;

    // Group registry
    CNSGroupsRegistry           m_GroupRegistry;

    // Garbage collector registry
    CJobGCRegistry              m_GCRegistry;

    TPauseStatus                m_PauseStatus;
};


// Thread-safe parameter access. The majority of parameters are single word,
// so if you need a single parameter, it is safe to use these methods, which
// do not lock anything. In such cases, where the parameter is not single-word,
// we lock m_ParamLock for reading.
inline CNSPreciseTime CQueue::GetTimeout() const
{
    return m_Timeout;
}
inline CNSPreciseTime CQueue::GetRunTimeout()  const
{
    return m_RunTimeout;
}
inline CNSPreciseTime CQueue::GetPendingTimeout() const
{
    return m_PendingTimeout;
}
inline CNSPreciseTime  CQueue::GetMaxPendingWaitTimeout() const
{
    return m_MaxPendingWaitTimeout;
}
inline unsigned CQueue::GetFailedRetries() const
{
    return m_FailedRetries;
}
inline bool CQueue::IsSubmitAllowed(unsigned host) const
{
    // The m_SubmHosts has internal lock anyway
    return host == 0  ||  m_SubmHosts.IsAllowed(host);
}
inline bool CQueue::IsWorkerAllowed(unsigned host) const
{
    // The m_WnodeHosts has internal lock anyway
    return host == 0  ||  m_WnodeHosts.IsAllowed(host);
}
inline bool CQueue::IsProgramAllowed(const string &  program_name) const
{
    if (!m_ProgramVersionList.IsConfigured())
        return true;    // No need to check

    if (program_name.empty())
        return false;

    try {
        CQueueClientInfo    auth_prog_info;

        ParseVersionString(program_name,
                           &auth_prog_info.client_name,
                           &auth_prog_info.version_info);
        return m_ProgramVersionList.IsMatchingClient(auth_prog_info);
    }
    catch (...) {
        // There could be parsing errors
        return false;
    }
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

        if (what_tables & eGroupTable)
            queue->m_QueueDbBlock->group_dict_db.SetTransaction(this);
    }
};


END_NCBI_SCOPE

#endif /* NETSCHEDULE_NS_QUEUE__HPP */

