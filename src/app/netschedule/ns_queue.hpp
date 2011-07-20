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
#include "worker_node.hpp"
#include "job_status.hpp"
#include "queue_vc.hpp"
#include "access_list.hpp"
#include "ns_affinity.hpp"
#include "ns_queue_db_block.hpp"
#include "ns_queue_parameters.hpp"

#include <deque>
#include <map>

BEGIN_NCBI_SCOPE

class CNetScheduleServer;
class CNetScheduleHandler;



// key, value -> bit vector of job ids
// typedef TNSBitVector TNSShortIntSet;
typedef deque<unsigned>                 TNSShortIntSet;
typedef map<TNSTag, TNSShortIntSet*>    TNSTagMap;
// Safe container for tag map
class CNSTagMap
{
public:
    CNSTagMap();
    ~CNSTagMap();
    TNSTagMap& operator*() { return m_TagMap; }
private:
    TNSTagMap   m_TagMap;
};


struct SFieldsDescription
{
    enum EFieldType {
        eFT_Job,
        eFT_Tag,
        eFT_Run,
        eFT_RunNum
    };
    typedef string (*FFormatter)(const string&, SQueueDescription*);

    vector<int>         field_types;
    vector<int>         field_nums;
    vector<FFormatter>  formatters;
    bool                has_tags;
    vector<string>      pos_to_tag;
    bool                has_affinity;
    bool                has_run;

    void Init()
    {
        field_types.clear();
        field_nums.clear();
        formatters.clear();
        pos_to_tag.clear();

        has_tags     = false;
        has_affinity = false;
        has_run      = false;
    }
};


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
/// Mutex priority (a < b means 'a' should be taken before 'b'
/// lock < m_AffinityMapLock (CQueue::JobFailed) - artificial, no longer needed
/// m_AffinityMapLock < m_AffinityIdxLock (FindPendingJob)
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
    enum EVectorId {
        eVIAll      = -1,
        eVIJob      = 0,
        eVITag      = 1,
        eVIAffinity = 2
    };

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
    int GetNotifyTimeout() const;
    bool GetDeleteDone() const;
    int GetRunTimeout() const;
    int GetRunTimeoutPrecision() const;
    unsigned GetFailedRetries() const;
    time_t GetBlacklistTime() const;
    time_t GetEmptyLifetime() const;
    unsigned GetMaxInputSize() const;
    unsigned GetMaxOutputSize() const;
    bool GetDenyAccessViolations() const;
    bool GetLogAccessViolations() const;
    bool GetKeepAffinity() const;
    bool IsVersionControl() const;
    bool IsMatchingClient(const CQueueClientInfo& cinfo) const;
    bool IsSubmitAllowed(unsigned host) const;
    bool IsWorkerAllowed(unsigned host) const;

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
    unsigned Submit(CJob& job);

    /// Submit job batch
    /// @return ID of the first job, second is first_id+1 etc.
    unsigned SubmitBatch(vector<CJob> &  batch);

    void PutResultGetJob(CWorkerNode*               worker_node,
                         // PutResult parameters
                         unsigned                   done_job_id,
                         int                        ret_code,
                         const string*              output,
                         // GetJob parameters
                         const list<string>*        aff_list,
                         CJob*                      new_job);

    /// Extend job expiration timeout
    /// @param tm
    ///    Time worker node needs to execute the job (in seconds)
    void JobDelayExpiration(CWorkerNode*    worker_node,
                            unsigned        job_id,
                            time_t          tm);

    // Worker node-specific methods
    bool PutProgressMessage(unsigned      job_id,
                            const string& msg);

    void ReturnJob(unsigned job_id);

    /// @param expected_status
    ///    If current status is different from expected try to
    ///    double read the database (to avoid races between nodes
    ///    and submitters)
    bool GetJobDescr(unsigned       job_id,
                     int*           ret_code,
                     string*        input,
                     string*        output,
                     string*        err_msg,
                     string*        progress_msg,
                     TJobStatus     expected_status =
                                        CNetScheduleAPI::eJobNotFound);

    /// Remove all jobs
    void Truncate(void);

    /// Cancel job execution (job stays in special Canceled state)
    void Cancel(unsigned job_id);

    TJobStatus GetJobStatus(unsigned job_id) const;

    /// count status snapshot for affinity token
    /// returns false if affinity token not found
    bool CountStatus(CJobStatusTracker::TStatusSummaryMap* status_map,
                     const string&                         affinity_token);

    // Set UDP port for notifications
    void SetPort(unsigned short port);

    /// Is the queue empty long enough to be deleted?
    bool IsExpired();

    /// get next job id (counter increment)
    unsigned int GetNextId();
    /// Returns first id for the batch
    unsigned int GetNextIdBatch(unsigned count);

    // Read-Confirm stage
    /// Request done jobs for reading with timeout
    void ReadJobs(unsigned peer_addr,
                  unsigned count, unsigned timeout,
                  unsigned& read_id, TNSBitVector& jobs);
    /// Confirm reading of these jobs
    void ConfirmJobs(unsigned read_id, const TNSBitVector& jobs);
    /// Fail (negative acknowledge) reading of these jobs
    void FailReadingJobs(unsigned read_id, const TNSBitVector& jobs);
    /// Return jobs to unread state without reservation
    void ReturnReadingJobs(unsigned read_id, const TNSBitVector& jobs);


    /// Erase job from all structures, request delayed db deletion
    void EraseJob(unsigned job_id);

    /// Erase jobs from all structures, request delayed db deletion
    void Erase(const TNSBitVector& job_ids);

    /// Erase all jobs from all structures, request delayed db deletion
    void Clear();

    /// Persist deleted vectors so restarted DB will not reincarnate jobs
    void FlushDeletedVectors(EVectorId vid = eVIAll);

    /// Filter out deleted jobs
    void FilterJobs(TNSBitVector& ids);

    /// Clear part of affinity index, called from periodic Purge
    void ClearAffinityIdx();

    void Notify(unsigned addr, unsigned short port, unsigned job_id);

    void MarkForDeletion() {
        m_DeleteDatabase = true;
    }

    // Optimize bitvectors
    void OptimizeMem();

    // Affinity methods
    CAffinityDict& GetAffinityDict() { return m_AffinityDict; }
    /// Read queue affinity index, retrieve all jobs, with
    /// given set of affinity ids, append it to 'jobs'
    ///
    /// @param aff_id_set
    ///     set of affinity ids to read
    /// @param jobs
    ///     OUT set of jobs associated with specified affinities
    ///
    void GetJobsWithAffinities(const TNSBitVector& aff_id_set,
                               TNSBitVector*       jobs);
    /// Read queue affinity index, retrieve all jobs, with
    /// given affinity id, append it to 'jobs'
    ///
    /// @param aff_id
    ///     affinity id to read
    /// @param jobs
    ///     OUT set of jobs associated with specified affinity
    ///
    void GetJobsWithAffinity(unsigned aff_id, TNSBitVector* jobs);

    /// Find the pending job.
    /// This method takes into account jobs available
    /// in the job status matrix and current
    /// worker node affinity association. If keep_affinity is
    /// true, node affinity will not be modified and in case
    /// there is no suitable job, no new job assigned. It is required
    /// for on-line use case when it is important that nodes will not
    /// be promiscuous. See CXX-2077
    ///
    /// Sync.Locks: affinity map lock, main queue lock, status matrix
    ///
    /// @return job_id
    unsigned
    FindPendingJob(CWorkerNode* worker_node,
                   const list<string>& aff_list,
                   time_t curr, bool keep_affinity);

    /// Calculate affinity preference string - a list
    /// of affinities accompanied by number of jobs belonging to
    /// them and list of nodes processing them, e.g.
    /// "a1 500 n1 n2, a2 600 n2 n3 n4, a3 200 n3"
    ///
    /// @return
    ///     affinity preference string
    string GetAffinityList();

    /// Return the list of worker nodes connected to this queue.
    CQueueWorkerNodeList& GetWorkerNodeList() {return m_WorkerNodeList;}

    /// @return is job modified
    bool FailJob(CWorkerNode*  worker_node,
                 unsigned      job_id,
                 const string& err_msg,
                 const string& output,
                 int           ret_code);


    void x_ReadAffIdx_NoLock(unsigned      aff_id,
                             TNSBitVector* jobs);

    /// Update the affinity index
    void AddJobsToAffinity(CBDB_Transaction &   trans,
                           unsigned             aff_id,
                           unsigned             job_id_from,
                           unsigned             job_id_to = 0);

    void AddJobsToAffinity(CBDB_Transaction &   trans,
                           const vector<CJob>&  batch);

    /// Specified status is OR-ed with the target vector
    void JobsWithStatus(TJobStatus    status,
                        TNSBitVector* bv) const;

    // Query-related
    typedef vector<string>      TRecord;
    typedef vector<TRecord>     TRecordSet;

    TNSBitVector* ExecSelect(const string& query, list<string>& fields);
    void PrepareFields(SFieldsDescription&  field_descr,
                       const list<string>&  fields);
    void ExecProject(TRecordSet&               record_set,
                     const TNSBitVector&       ids,
                     const SFieldsDescription& field_descr);

    /// Clear all jobs, still running for node.
    /// Fails all such jobs, called by external node watcher, can safely
    /// clean out node's record
    void ClearWorkerNode(CWorkerNode* worker_node, const string& reason);
    void ClearWorkerNode(const string& node_id, const string& reason);

    void NotifyListeners(bool unconditional, unsigned aff_id);
    void PrintWorkerNodeStat(CNetScheduleHandler &  handler,
                             time_t                 curr,
                             EWNodeFormat           fmt = eWNF_Old) const;
    void UnRegisterNotificationListener(CWorkerNode* worker_node);
    void AddJobToWorkerNode(CWorkerNode *   worker_node,
                            const CJob &    job,
                            time_t          exp_time);
    void UpdateWorkerNodeJob(unsigned               job_id,
                             time_t                 exp_time);
    void RemoveJobFromWorkerNode(const CJob&        job,
                                 ENSCompletion      reason);

    void GetAllAssignedAffinities(time_t t, TNSBitVector& aff_ids);
    void AddAffinity(CWorkerNode*  worker_node,
                     unsigned      aff_id,
                     time_t        exp_time);
    void BlacklistJob(CWorkerNode*  worker_node,
                      unsigned      job_id,
                      time_t        exp_time);


    // Tags methods
    typedef CSimpleBuffer TBuffer;

    void SetTagDbTransaction(CBDB_Transaction* trans);
    void AppendTags(CNSTagMap& tag_map,
                    const TNSTagList& tags,
                    unsigned job_id);
    void FlushTags(CNSTagMap& tag_map, CBDB_Transaction& trans);
    bool ReadTag(const string& key, const string& val,
                 TBuffer* buf);
    void ReadTags(const string& key, TNSBitVector* bv);
    void x_RemoveTags(CBDB_Transaction& trans, const TNSBitVector& ids);
    CFastMutex& GetTagLock() { return m_TagLock; }

    /// Check execution timeout. Now checks reading timeout as well.
    /// All jobs failed to execute, go back to pending
    void CheckExecutionTimeout(bool logging);

    /// Check timeout for a job
    void x_CheckExecutionTimeout(unsigned  queue_run_timeout,
                                 unsigned  job_id,
                                 time_t    curr_time,
                                 bool      logging);

    /// Check jobs for expiry and if expired, delete up to batch_size jobs
    /// @return
    ///    Number of deleted jobs
    unsigned CheckJobsExpiry(unsigned batch_size, TJobStatus status);

    void TimeLineMove(unsigned job_id, time_t old_time, time_t new_time);
    void TimeLineAdd(unsigned job_id, time_t timeout);
    void TimeLineRemove(unsigned job_id);
    void TimeLineExchange(unsigned remove_job_id,
                          unsigned add_job_id,
                          time_t   timeout);

    unsigned DeleteBatch(unsigned batch_size);

    CBDB_FileCursor& GetRunsCursor();

    void PrintSubmHosts(CNetScheduleHandler &  handler) const;
    void PrintWNodeHosts(CNetScheduleHandler &  handler) const;

    void PrintQueue(CNetScheduleHandler &   handler,
                    TJobStatus              job_status);

    /// Queue dump
    size_t PrintJobDbStat(CNetScheduleHandler &   handler,
                          unsigned                job_id,
                          TJobStatus              status
                                = CNetScheduleAPI::eJobNotFound);
    /// Dump all job records
    void PrintAllJobDbStat(CNetScheduleHandler &   handler);

    unsigned CountStatus(TJobStatus) const;
    void StatusStatistics(TJobStatus status,
        TNSBitVector::statistics* st) const;


    // DB statistics
    unsigned CountRecs();
    void PrintStat(CNcbiOstream& out);

    // Statistics gathering objects
    friend class CStatisticsThread;
    class CStatisticsThread : public CThreadNonStop
    {
        typedef CQueue TContainer;
    public:
        CStatisticsThread(TContainer &  container,
                          const bool &  logging);
        void DoJob(void);
    private:
        TContainer &        m_Container;
        size_t              m_RunCounter;
        const bool &        m_StatisticsLogging;
    };
    CRef<CStatisticsThread>     m_StatThread;

    // Statistics
    enum EStatEvent {
        eStatGetEvent     = 0,
        eStatPutEvent     = 1,
        eStatDBLockEvent  = 2,
        eStatDBWriteEvent = 3,
        eStatNumEvents
    };
    typedef unsigned TStatEvent;
    void CountEvent(TStatEvent event, int num=1);
    double GetAverage(TStatEvent event);
    string MakeKey(unsigned job_id) const
    { return m_KeyGenerator.GenerateV1(job_id); }


private:
    friend class CNS_Transaction;
    CBDB_Env& GetEnv() { return *m_QueueDbBlock->job_db.GetEnv(); }

    void x_ChangeGroupStatus(unsigned            group_id,
                             const TNSBitVector& bv_jobs,
                             TJobStatus          status);

    // Add job to group, creating it if necessary. Used during queue
    // loading, so guaranteed no concurrent queue access.
    void x_AddToReadGroupNoLock(unsigned group_id, unsigned job_id);
    // Thread-safe helpers for read group management
    // Create read group and assign jobs to it
    void x_CreateReadGroup(unsigned group_id, const TNSBitVector& bv_jobs);
    // Remove from group, and if group is empty delete it
    void x_RemoveFromReadGroup(unsigned group_id, unsigned job_id);

    void x_FailJobs(const TJobList& jobs,
                    CWorkerNode* worker_node, const string& err_msg);

    /// @return TRUE if job record has been found and updated
    bool x_UpdateDB_PutResultNoLock(unsigned        job_id,
                                    time_t          curr,
                                    bool            delete_done,
                                    int             ret_code,
                                    const string &  output,
                                    CJob&           job);

    enum EGetJobUpdateStatus {
        eGetJobUpdate_Ok,
        eGetJobUpdate_NotFound,
        eGetJobUpdate_JobStopped,
        eGetJobUpdate_JobFailed
    };

    EGetJobUpdateStatus x_UpdateDB_GetJobNoLock(CWorkerNode *   worker_node,
                                                time_t          curr,
                                                unsigned        job_id,
                                                CJob&           job);

    bool x_FillRecord(vector<string> &              record,
                      const SFieldsDescription &    field_descr,
                      const CJob &                  job,
                      map<string, string> &         tags,
                      const CJobRun *               run,
                      int                           run_num);
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
    void x_DeleteJobRuns(unsigned int  job_id);

private:
    friend class CQueueGuard;
    friend class CQueueJSGuard;
    friend class CJob;
    friend class CQueueEnumCursor;
    friend class CQueueParamAccessor;

    CJobStatusTracker           m_StatusTracker;    ///< status FSA

    // Timeline object to control job execution timeout
    CJobTimeLine*               m_RunTimeLine;
    CRWLock                     m_RunTimeLineLock;

    // Datagram notification socket
    // (used to notify worker nodes and waiting clients)
    bool                        m_HasNotificationPort;
    CDatagramSocket             m_UdpSocket;     ///< UDP notification socket
    CFastMutex                  m_UdpSocketLock;

    /// Should we delete db upon close?
    bool                        m_DeleteDatabase;

    // Statistics
    CAtomicCounter              m_EventCounter[eStatNumEvents];
    unsigned                    m_Average[eStatNumEvents];

    // Background executor
    CRequestExecutor&           m_Executor;

    string                      m_QueueName;
    string                      m_QueueClass;      ///< Parameter class
    TQueueKind                  m_Kind;            ///< 0 - static, 1 - dynamic

    SQueueDbBlock*              m_QueueDbBlock;

    auto_ptr<CBDB_FileCursor>   m_RunsCursor;      ///< DB cursor for RunsDB

    CFastMutex                  m_DbLock;          ///< db, cursor lock
    CFastMutex                  m_AffinityIdxLock;
    CFastMutex                  m_TagLock;

    // affinity dictionary does not need a mutex, because
    // CAffinityDict is a syncronized class itself (mutex included)
    CAffinityDict               m_AffinityDict;    ///< Affinity tokens

    ///< When the queue became empty, guarded by 'm_DbLock'
    time_t                      m_BecameEmpty;

    // List of active worker node listeners waiting for pending jobs
    CQueueWorkerNodeList        m_WorkerNodeList;


    /// Last valid id for queue
    unsigned int                m_LastId;      // Last used job ID
    unsigned int                m_SavedId;     // The ID we will start next time
                                               // the netschedule is loaded
    CFastMutex                  m_lastIdLock;

    // Read group support
    typedef map<unsigned, TNSBitVector>     TGroupMap;
    /// Last valid id for read group
    CAtomicCounter                          m_GroupLastId;
    TGroupMap                               m_GroupMap;
    CFastMutex                              m_GroupMapLock;

    /// Lock for deleted jobs vectors
    CFastMutex                   m_JobsToDeleteLock;
    /// Vector of jobs to be deleted from db unconditionally
    /// keeps jobs still to be deleted from main DB
    TNSBitVector                 m_JobsToDelete;
    /// Vector to mask queries against; keeps jobs not yet deleted
    /// from tags DB
    TNSBitVector                 m_DeletedJobs;
    /// Vector to keep ids to be cleaned from affinity
    TNSBitVector                 m_AffJobsToDelete;
    /// Is m_CurrAffId wrapped around 0?
    bool                         m_AffWrapped;
    /// Current affinity id to start cleaning from
    unsigned                     m_CurrAffId;
    /// Last affinity id cleaning started from
    unsigned                     m_LastAffId;

    // Configurable queue parameters
    // When modifying this, modify all places marked with PARAMETERS
    mutable CRWLock              m_ParamLock;
    int                          m_Timeout;         ///< Result exp. timeout
    int                          m_NotifyTimeout;   ///< Notification interval
    bool                         m_DeleteDone;      ///< Delete done jobs
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
    bool                         m_LogAccessViolations;
    /// Keep the node affinity for a while even if there is no jobs with
    /// such affinity. Helps on-line application.
    bool                         m_KeepAffinity;
    /// Client program version control
    CQueueClientInfoList         m_ProgramVersionList;
    /// Host access list for job submission
    CNetSchedule_AccessList      m_SubmHosts;
    /// Host access list for job execution (workers)
    CNetSchedule_AccessList      m_WnodeHosts;

    CNetScheduleKeyGenerator     m_KeyGenerator;
    const bool &                 m_Log;
    const bool &                 m_LogBatchEachJob;
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
inline int CQueue::GetNotifyTimeout() const
{
    return m_NotifyTimeout;
}
inline bool CQueue::GetDeleteDone() const
{
    return m_DeleteDone;
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
inline unsigned CQueue::GetMaxInputSize() const
{
    return m_MaxInputSize;
}
inline unsigned CQueue::GetMaxOutputSize() const
{
    return m_MaxOutputSize;
}
inline bool CQueue::GetDenyAccessViolations() const
{
    return m_DenyAccessViolations;
}
inline bool CQueue::GetLogAccessViolations() const
{
    return m_LogAccessViolations;
}
inline bool CQueue::GetKeepAffinity() const
{
    return m_KeepAffinity;
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


class CQueueParamAccessor
{
    // When modifying this, modify all places marked with PARAMETERS
public:
    CQueueParamAccessor(const CQueue& queue) :
        m_Queue(queue), m_Guard(queue.m_ParamLock)
    {}

    int GetTimeout() const { return m_Queue.m_Timeout; }
    int GetNotifyTimeout() const { return m_Queue.m_NotifyTimeout; }
    bool GetDeleteDone() const { return m_Queue.m_DeleteDone; }
    int GetRunTimeout() const { return m_Queue.m_RunTimeout; }
    int GetRunTimeoutPrecision() const { return m_Queue.m_RunTimeoutPrecision; }
    unsigned GetFailedRetries() const { return m_Queue.m_FailedRetries; }
    time_t GetBlacklistTime() const { return m_Queue.m_BlacklistTime; }
    time_t GetEmptyLifetime() const { return m_Queue.m_EmptyLifetime; }
    unsigned GetMaxInputSize() const { return m_Queue.m_MaxInputSize; }
    unsigned GetMaxOutputSize() const { return m_Queue.m_MaxOutputSize; }
    bool GetDenyAccessViolations() const { return m_Queue.m_DenyAccessViolations; }
    bool GetLogAccessViolations() const { return m_Queue.m_LogAccessViolations; }
    bool GetKeepAffinity() const { return m_Queue.m_KeepAffinity; }
    const CQueueClientInfoList& GetProgramVersionList() const
        { return m_Queue.m_ProgramVersionList; }
    const CNetSchedule_AccessList& GetSubmHosts() const
        { return m_Queue.m_SubmHosts; }
    const CNetSchedule_AccessList& GetWnodeHosts() const
        { return m_Queue.m_WnodeHosts; }

    unsigned GetNumParams() const {
        return 16;
    }
    string GetParamName(unsigned n) const {
        switch (n) {
        case 0:  return "timeout";
        case 1:  return "notif_timeout";
        case 2:  return "delete_done";
        case 3:  return "run_timeout";
        case 4:  return "run_timeout_precision";
        case 5:  return "failed_retries";
        case 6:  return "blacklist_time";
        case 7:  return "empty_lifetime";
        case 8:  return "max_input_size";
        case 9:  return "max_output_size";
        case 10: return "deny_access_violations";
        case 11: return "log_access_violations";
        case 12: return "keep_affinity";
        case 13: return "program";
        case 14: return "subm_host";
        case 15: return "wnode_host";
        default: return "";
        }
    }
    string GetParamValue(unsigned n) const {
        switch (n) {
        case 0:  return NStr::IntToString(m_Queue.m_Timeout);
        case 1:  return NStr::IntToString(m_Queue.m_NotifyTimeout);
        case 2:  return m_Queue.m_DeleteDone ? "true" : "false";
        case 3:  return NStr::IntToString(m_Queue.m_RunTimeout);
        case 4:  return NStr::IntToString(m_Queue.m_RunTimeoutPrecision);
        case 5:  return NStr::IntToString(m_Queue.m_FailedRetries);
        case 6:  return NStr::Int8ToString(m_Queue.m_BlacklistTime);
        case 7:  return NStr::Int8ToString(m_Queue.m_EmptyLifetime);
        case 8:  return NStr::IntToString(m_Queue.m_MaxInputSize);
        case 9:  return NStr::IntToString(m_Queue.m_MaxOutputSize);
        case 10: return m_Queue.m_DenyAccessViolations ? "true" : "false";
        case 11: return m_Queue.m_LogAccessViolations ? "true" : "false";
        case 12: return m_Queue.m_KeepAffinity ? "true" : "false";
        case 13: return m_Queue.m_ProgramVersionList.Print();
        case 14: return m_Queue.m_SubmHosts.Print();
        case 15: return m_Queue.m_WnodeHosts.Print();
        default: return "";
        }
    }

private:
    const CQueue &      m_Queue;
    CReadLockGuard      m_Guard;
};


// Application specific defaults provider for DB transaction
class CNS_Transaction : public CBDB_Transaction
{
public:
    CNS_Transaction(CBDB_Env&             env,
                    ETransSync            tsync = eEnvDefault,
                    EKeepFileAssociation  assoc = eNoAssociation)
        : CBDB_Transaction(env, tsync, assoc)
    {}

    CNS_Transaction(CQueue*               queue,
                    ETransSync            tsync = eEnvDefault,
                    EKeepFileAssociation  assoc = eNoAssociation)
        : CBDB_Transaction(queue->GetEnv(), tsync, assoc)
    {}

    CNS_Transaction(CBDB_RawFile&         dbf,
                    ETransSync            tsync = eEnvDefault,
                    EKeepFileAssociation  assoc = eNoAssociation)
        : CBDB_Transaction(*dbf.GetEnv(), tsync, assoc)
    {}
};


// Guards which locks the queue and set transaction for main
// queue tables
class CQueueGuard
{
public:
    CQueueGuard() : m_Queue(0)
    {}

    CQueueGuard(CQueue*           q,
                CBDB_Transaction* trans = NULL)
        : m_Queue(0)
    {
        Guard(q);
        q->m_QueueDbBlock->job_db.SetTransaction(trans);
        q->m_QueueDbBlock->job_info_db.SetTransaction(trans);
        q->m_QueueDbBlock->runs_db.SetTransaction(trans);
    }

    ~CQueueGuard()
    {
        try {
            Release();
        } catch (std::exception& ) {
        }
    }

    void Release()
    {
        if (m_Queue) {
            m_Queue->m_DbLock.Unlock();
            m_Queue = 0;
        }
    }

    void Guard(CQueue* q)
    {
        Release();
        m_Queue = q;
        m_Queue->m_DbLock.Lock();
        m_Queue->CountEvent(CQueue::eStatDBLockEvent);
    }

private:
    // No copy
    CQueueGuard(const CQueueGuard&);
    void operator=(const CQueueGuard&);

private:
    CQueue *    m_Queue;
};


// Job status guard
class CQueueJSGuard
{
public:
    CQueueJSGuard(CQueue*    q,
                  unsigned   job_id,
                  TJobStatus status,
                  bool*      updated = 0)
        : m_Guard(q->m_StatusTracker, job_id, status, updated)
    {}

    void Commit() { m_Guard.Commit(); }

    void SetStatus(TJobStatus status)
    {
        m_Guard.SetStatus(status);
    }

    TJobStatus GetOldStatus() const
    {
        return m_Guard.GetOldStatus();
    }

private:
    // No copy
    CQueueJSGuard(const CQueueJSGuard&);
    void operator=(const CQueueJSGuard&);

private:
    CNetSchedule_JS_Guard   m_Guard;
};

END_NCBI_SCOPE

#endif /* NETSCHEDULE_NS_QUEUE__HPP */

