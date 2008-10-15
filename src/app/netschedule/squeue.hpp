#ifndef NETSCHEDULE_SQUEUE__HPP
#define NETSCHEDULE_SQUEUE__HPP

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
#include "job.hpp"
#include "worker_node.hpp"
#include "job_status.hpp"
#include "queue_vc.hpp"
#include "access_list.hpp"
#include "ns_affinity.hpp"

#include "weak_ref_orig.hpp"

#include <deque>
#include <map>

BEGIN_NCBI_SCOPE


/// Queue databases
// BerkeleyDB does not like to mix DB open/close with regular operations,
// so we open a set of db files on startup to be used as actual databases.
// This block represent a set of db files to serve one queue.
struct SQueueDbBlock
{
    void Open(CBDB_Env& env, const string& path, int pos);
    void Close();
    void ResetTransaction();
    void Truncate();

    bool                  allocated; // Am I allocated?
    int                   pos;       // My own pos in array
    SQueueDB              job_db;
    SJobInfoDB            job_info_db;
    SRunsDB               runs_db;
    SDeletedJobsDB        deleted_jobs_db;
    SAffinityIdx          affinity_idx;
    SAffinityDictDB       aff_dict_db;
    SAffinityDictTokenIdx aff_dict_token_idx;
    STagDB                tag_db;
};


class CQueueDbBlockArray
{
public:
    CQueueDbBlockArray();
    ~CQueueDbBlockArray();
    void Init(CBDB_Env& env, const string& path, unsigned count);
    void Close();
    // Allocate a block from array. Negative means no more free blocks
    int  Allocate();
    // Return block at position 'pos' to the array
    void Free(int pos);
    SQueueDbBlock* Get(int pos);
private:
    unsigned       m_Count;
    SQueueDbBlock* m_Array;

};


/// Queue parameters
struct SQueueParameters
{
    /// General parameters, reconfigurable at run time
    int timeout;
    int notif_timeout;
    int run_timeout;
    string program_name;
    bool delete_done;
    int failed_retries;
    time_t blacklist_time;
    time_t empty_lifetime;
    unsigned max_input_size;
    unsigned max_output_size;
    bool deny_access_violations;
    bool log_access_violations;
    bool log_job_state;
    string subm_hosts;
    string wnode_hosts;
    // This parameter is not reconfigurable
    int run_timeout_precision;

    SQueueParameters() :
        timeout(3600),
        notif_timeout(7),
        run_timeout(3600),
        delete_done(false),
        failed_retries(0),
        empty_lifetime(0),
        max_input_size(kNetScheduleMaxDBDataSize),
        max_output_size(kNetScheduleMaxDBDataSize),
        deny_access_violations(false),
        log_access_violations(true),
        log_job_state(false),
        run_timeout_precision(3600)
    { }
    ///
    void Read(const IRegistry& reg, const string& sname);
};


// key, value -> bitvector of job ids
// typedef TNSBitVector TNSShortIntSet;
typedef deque<unsigned> TNSShortIntSet;
typedef map<TNSTag, TNSShortIntSet*> TNSTagMap;
// Safe container for tag map
class CNSTagMap
{
public:
    CNSTagMap();
    ~CNSTagMap();
    TNSTagMap& operator*() { return m_TagMap; }
private:
    TNSTagMap m_TagMap;
};


struct SLockedQueue;
class CQueueEnumCursor : public CBDB_FileCursor
{
public:
    CQueueEnumCursor(SLockedQueue* queue);
};


// slight violation of naming convention for porting to util/time_line
typedef CTimeLine<TNSBitVector> CJobTimeLine;
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
struct SLockedQueue : public CWeakObjectBase<SLockedQueue>
{
    enum EQueueKind {
        eKindStatic  = 0,
        eKindDynamic = 1
    };
    enum EVectorId {
        eVIAll      = -1,
        eVIJob      = 0,
        eVITag      = 1,
        eVIAffinity = 2
    };
    typedef int TQueueKind;
private:
    friend class CQueueGuard;
    friend class CQueueJSGuard;
    friend class CJob;

    string                       m_QueueName;
    string                       m_QueueClass;      ///< Parameter class
    TQueueKind                   m_Kind;            ///< 0 - static, 1 - dynamic

    friend class CQueueEnumCursor;
    SQueueDbBlock*               m_QueueDbBlock;

    // Databases
//    SQueueDB                     m_JobDB;           ///< Main queue database
#define m_JobDB m_QueueDbBlock->job_db
    SJobInfoDB                   m_JobInfoDB;       ///< Aux info on jobs, tags etc.
#define m_JobInfoDB m_QueueDbBlock->job_info_db

//    SRunsDB                      m_RunsDB;          ///< Info on jobs runs
#define m_RunsDB m_QueueDbBlock->runs_db
    auto_ptr<CBDB_FileCursor>    m_RunsCursor;      ///< DB cursor for RunsDB

    CFastMutex                   m_DbLock;          ///< db, cursor lock

public:
    CJobStatusTracker            status_tracker;    ///< status FSA

private:
    // Affinity
//    SAffinityIdx                 m_AffinityIdx;     ///< Q affinity index
#define m_AffinityIdx m_QueueDbBlock->affinity_idx
    CFastMutex                   m_AffinityIdxLock;


    // affinity dictionary does not need a mutex, because
    // CAffinityDict is a syncronized class itself (mutex included)
    CAffinityDict                m_AffinityDict;    ///< Affinity tokens

    CWorkerNodeAffinity          m_AffinityMap;     ///< Affinity map
    CFastMutex                   m_AffinityMapLock; ///< m_AffinityMap lock

    // Tags
//    STagDB                       m_TagDB;
#define m_TagDB m_QueueDbBlock->tag_db
    CFastMutex                   m_TagLock;

    ///< When it became empty, guarded by 'lock'
    time_t                       m_BecameEmpty;

    // List of active worker node listeners waiting for pending jobs
    CQueueWorkerNodeList         m_WorkerNodeList;
    string                       q_notif;       ///< Queue notification message

public:
    // Timeline object to control job execution timeout
    CJobTimeLine*                run_time_line;
    CRWLock                      rtl_lock;      ///< run_time_line locker

    // Datagram notification socket
    // (used to notify worker nodes and waiting clients)
    CDatagramSocket              udp_socket;    ///< UDP notification socket
    CFastMutex                   us_lock;       ///< UDP socket lock

    /// Queue monitor
    CServer_Monitor              m_Monitor;

    /// Should we delete db upon close?
    bool                         delete_database;
    vector<string>               files; ///< list of files (paths) for queue

public:
    // Constructor/destructor
    SLockedQueue(const string& queue_name,
        const string& qclass_name, TQueueKind queue_kind);
    ~SLockedQueue();

    void Attach(SQueueDbBlock* block);
    void Detach();
    int  GetPos() { return m_QueueDbBlock->pos; }

    void x_ReadFieldInfo(void);

    // Thread-safe parameter set
    void SetParameters(const SQueueParameters& params);

    ////
    // Status matrix related
    unsigned LoadStatusMatrix();

    const string& GetQueueName() const {
        return m_QueueName;
    }

    string DecorateJobId(unsigned job_id) {
        return m_QueueName + '/' + NStr::IntToString(job_id);
    }

    TJobStatus GetJobStatus(unsigned job_id) const;

    /// count status snapshot for affinity token
    /// returns false if affinity token not found
    bool CountStatus(CJobStatusTracker::TStatusSummaryMap* status_map,
                     const char*                           affinity_token);

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
                  unsigned& read_id, TNSBitVector& bv_jobs);
    /// Confirm reading of these jobs
    void ConfirmJobs(unsigned read_id, TNSBitVector& bv_jobs);
    /// Fail (negative acknoledge) reading of these jobs
    void FailReadingJobs(unsigned read_id, TNSBitVector& bv_jobs);


    /// Erase job from all structures, request delayed db deletion
    void Erase(unsigned job_id);

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

    // Optimize bitvectors
    void OptimizeMem();

    // Affinity methods
    /// Read queue affinity index, retrieve all jobs, with
    /// given set of affinity ids, append it to 'jobs'
    ///
    /// @param aff_id_set
    ///     set of affinity ids to read
    /// @param jobs
    ///     OUT set of jobs associated with specified affinities
    ///
    void GetJobsWithAffinity(const TNSBitVector& aff_id_set,
                             TNSBitVector*       jobs);
    /// Read queue affinity index, retrieve all jobs, with
    /// given affinity id, append it to 'jobs'
    ///
    /// @param aff_id_set
    ///     set of affinity ids to read
    /// @param jobs
    ///     OUT set of jobs associated with specified affinities
    ///
    void GetJobsWithAffinity(unsigned            aff_id,
                             TNSBitVector*       jobs);

    /// Find the pending job.
    /// This method takes into account jobs available
    /// in the job status matrix and current
    /// worker node affinity association
    ///
    /// Sync.Locks: affinity map lock, main queue lock, status matrix
    ///
    /// @return job_id
    unsigned
    FindPendingJob(const string& node_id, time_t curr);

    /// @return is job modified
    bool FailJob(unsigned      job_id,
                 const string& err_msg,
                 const string& output,
                 int           ret_code,
                 const string* node_id = 0);


    void x_ReadAffIdx_NoLock(unsigned      aff_id,
                             TNSBitVector* jobs);

    /// Update the affinity index
    void AddJobsToAffinity(CBDB_Transaction& trans,
                           unsigned aff_id,
                           unsigned job_id_from,
                           unsigned job_id_to = 0);

    void AddJobsToAffinity(CBDB_Transaction& trans,
                           const vector<CJob>& batch);

    // Worker node methods
    void InitWorkerNode(const SWorkerNodeInfo& node_info);
    void ClearWorkerNode(const string& node_id);
    void NotifyListeners(bool unconditional, unsigned aff_id);
    void PrintWorkerNodeStat(CNcbiOstream& out) const;
    void RegisterNotificationListener(const SWorkerNodeInfo& node_info,
                                      unsigned short         port,
                                      unsigned               timeout);
    // Is the unregistration final? True for old style nodes.
    bool UnRegisterNotificationListener(const string& node_id);
    void RegisterWorkerNodeVisit(SWorkerNodeInfo& node_info);
    void AddJobToWorkerNode(const string&           node_id,
                            CRequestContextFactory* rec_ctx_f,
                            unsigned                job_id,
                            time_t                  exp_time);
    void UpdateWorkerNodeJob(const string&          node_id,
                             unsigned               job_id,
                             time_t                 exp_time);
    void RemoveJobFromWorkerNode(const string&      node_id,
                                 const CJob&        job,
                                 ENSCompletion      reason);
    void x_FailJobsAtNodeClose(TJobList& jobs);
    //

    typedef CWorkerNodeAffinity::TNetAddress TNetAddress;
    void ClearAffinity(const string& node_id);
    void AddAffinity(const string& node_id,
                     unsigned      aff_id,
                     time_t        exp_time);
    void BlacklistJob(const string& node_id,
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
    void CheckExecutionTimeout(void);

    /// Check timeout for a job
    void x_CheckExecutionTimeout(unsigned queue_run_timeout,
                                 unsigned job_id, time_t curr_time);

    unsigned CheckJobsExpiry(unsigned batch_size, TJobStatus status);

    unsigned DeleteBatch(unsigned batch_size);

    CBDB_FileCursor& GetRunsCursor();

    /// Pass socket for monitor (takes ownership)
    void SetMonitorSocket(CSocket& socket);
    /// Are we monitoring?
    bool IsMonitoring();
    /// Send string to monitor
    void MonitorPost(const string& msg);

    // DB statistics
    unsigned CountRecs();
    void PrintStat(CNcbiOstream& out);

    // Statistics gathering objects
    friend class CStatisticsThread;
    class CStatisticsThread : public CThreadNonStop
    {
        typedef SLockedQueue TContainer;
    public:
        CStatisticsThread(TContainer& container);
        void DoJob(void);
    private:
        TContainer& m_Container;
    };
    CRef<CStatisticsThread> m_StatThread;

    // Statistics
    enum EStatEvent {
        eStatGetEvent     = 0,
        eStatPutEvent     = 1,
        eStatDBLockEvent  = 2,
        eStatDBWriteEvent = 3,
        eStatNumEvents
    };
    typedef unsigned TStatEvent;
    CAtomicCounter m_EventCounter[eStatNumEvents];
    unsigned m_Average[eStatNumEvents];
    void CountEvent(TStatEvent event, int num=1);
    double GetAverage(TStatEvent event);

private:
    friend class CNS_Transaction;
    CBDB_Env& GetEnv() { return *m_JobDB.GetEnv(); }

    void x_ChangeGroupStatus(unsigned            group_id,
                             const TNSBitVector& bv_jobs,
                             TJobStatus          status);

    // Add job to group, creating it if necessary. Used during queue
    // loading, so guaranteed no concurrent queue acces.
    void x_AddToReadGroupNoLock(unsigned group_id, unsigned job_id);
    // Thread-safe helpers for read group management
    // Create read group and assign jobs to it
    void x_CreateReadGroup(unsigned group_id, const TNSBitVector& bv_jobs);
    // Remove from group, and if group is empty delete it
    void x_RemoveFromReadGroup(unsigned group_id, unsigned job_id);

private:
    typedef map<unsigned, TNSBitVector> TGroupMap;
    /// Last valid id for queue
    CAtomicCounter               m_LastId;

    /// Last valid id for read group
    CAtomicCounter               m_GroupLastId;
    TGroupMap                    m_GroupMap;
    CFastMutex                   m_GroupMapLock;

    /// Lock for deleted jobs vectors
    CFastMutex                   m_JobsToDeleteLock;
    /// Database for vectors of deleted jobs
//    SDeletedJobsDB               m_DeletedJobsDB;
#define m_DeletedJobsDB m_QueueDbBlock->deleted_jobs_db
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
    /// Current aff id to start cleaning from
    unsigned                     m_CurrAffId;
    /// Last aff id cleaning started from
    unsigned                     m_LastAffId;

    // When modifying this, modify all places marked with PARAMETERS
    // Configurable queue parameters
    friend class CQueueParamAccessor;
    mutable CRWLock              m_ParamLock;
    int                          m_Timeout;         ///< Result exp. timeout
    int                          m_NotifyTimeout;   ///< Notification interval
    bool                         m_DeleteDone;      ///< Delete done jobs
    int                          m_RunTimeout;      ///< Execution timeout
    /// Its precision, set at startup only, not reconfigurable
    int                          m_RunTimeoutPrecision;
    /// How many attemts to make on different nodes before failure
    unsigned                     m_FailedRetries;
    /// How long a job lives in blacklist
    time_t                       m_BlacklistTime;
    /// How long to live after becoming empty, if -1 - infinitely
    time_t                       m_EmptyLifetime;
    unsigned                     m_MaxInputSize;
    unsigned                     m_MaxOutputSize;
    bool                         m_DenyAccessViolations;
    bool                         m_LogAccessViolations;
    bool                         m_LogJobState;
    /// Client program version control
    CQueueClientInfoList         m_ProgramVersionList;
    /// Host access list for job submission
    CNetSchedule_AccessList      m_SubmHosts;
    /// Host access list for job execution (workers)
    CNetSchedule_AccessList      m_WnodeHosts;
};


class CQueueParamAccessor
{
    // When modifying this, modify all places marked with PARAMETERS
public:
    CQueueParamAccessor(const SLockedQueue& queue) :
        m_Queue(queue), m_Guard(queue.m_ParamLock) { }
    int GetTimeout() { return m_Queue.m_Timeout; }
    int GetNotifyTimeout() { return m_Queue.m_NotifyTimeout; }
    bool GetDeleteDone() { return m_Queue.m_DeleteDone; }
    int GetRunTimeout() { return m_Queue.m_RunTimeout; }
    int GetRunTimeoutPrecision() { return m_Queue.m_RunTimeoutPrecision; }
    unsigned GetFailedRetries() { return m_Queue.m_FailedRetries; }
    time_t GetBlacklistTime() { return m_Queue.m_BlacklistTime; }
    int GetEmptyLifetime() { return m_Queue.m_EmptyLifetime; }
    unsigned GetMaxInputSize() { return m_Queue.m_MaxInputSize; }
    unsigned GetMaxOutputSize() { return m_Queue.m_MaxOutputSize; }
    bool GetDenyAccessViolations() { return m_Queue.m_DenyAccessViolations; }
    bool GetLogAccessViolations() { return m_Queue.m_LogAccessViolations; }
    bool GetLogJobState() { return m_Queue.m_LogJobState; }
    const CQueueClientInfoList& GetProgramVersionList()
        { return m_Queue.m_ProgramVersionList; }
    const CNetSchedule_AccessList& GetSubmHosts()
        { return m_Queue.m_SubmHosts; }
    const CNetSchedule_AccessList& GetWnodeHosts()
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
        case 12: return "log_job_state";
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
        case 6:  return NStr::IntToString(m_Queue.m_BlacklistTime);
        case 7:  return NStr::IntToString(m_Queue.m_EmptyLifetime);
        case 8:  return NStr::IntToString(m_Queue.m_MaxInputSize);
        case 9:  return NStr::IntToString(m_Queue.m_MaxOutputSize);
        case 10: return m_Queue.m_DenyAccessViolations ? "true" : "false";
        case 11: return m_Queue.m_LogAccessViolations ? "true" : "false";
        case 12: return m_Queue.m_LogJobState ? "true" : "false";
        case 13: return m_Queue.m_ProgramVersionList.Print();
        case 14: return m_Queue.m_SubmHosts.Print();
        case 15: return m_Queue.m_WnodeHosts.Print();
        default: return "";
        }
    }

private:
    const SLockedQueue& m_Queue;
    CReadLockGuard m_Guard;
};


// Application specific defaults provider for DB transaction
class CNS_Transaction : public CBDB_Transaction
{
public:
    CNS_Transaction(CBDB_Env&             env,
                    ETransSync            tsync = eEnvDefault,
                    EKeepFileAssociation  assoc = eNoAssociation)
        : CBDB_Transaction(env, tsync, assoc)
    {
    }
    CNS_Transaction(SLockedQueue*         queue,
                    ETransSync            tsync = eEnvDefault,
                    EKeepFileAssociation  assoc = eNoAssociation)
        : CBDB_Transaction(queue->GetEnv(), tsync, assoc)
    {
    }
};


template <>
class CLockerTraits<SLockedQueue>
{
public:
    typedef CIntrusiveLocker<SLockedQueue> TLockerType;
};


// Guards which locks the queue and set transaction for main
// queue tables
class CQueueGuard
{
public:
    CQueueGuard() : m_Queue(0)
    {
    }

    CQueueGuard(SLockedQueue* q,
                CBDB_Transaction* trans = NULL)
        : m_Queue(0)
    {
        Guard(q);
        q->m_JobDB.SetTransaction(trans);
        q->m_JobInfoDB.SetTransaction(trans);
        q->m_RunsDB.SetTransaction(trans);
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

    void Guard(SLockedQueue* q)
    {
        Release();
        m_Queue = q;
        m_Queue->m_DbLock.Lock();
        m_Queue->CountEvent(SLockedQueue::eStatDBLockEvent);
    }

private:
    // No copy
    CQueueGuard(const CQueueGuard&);
    void operator=(const CQueueGuard&);
private:
    SLockedQueue* m_Queue;
};


// Job status guard
class CQueueJSGuard
{
public:
    CQueueJSGuard(SLockedQueue* q,
                  unsigned      job_id,
                  TJobStatus    status,
                  bool*         updated = 0)
        : m_Guard(q->status_tracker, job_id, status, updated)
    {
    }

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
    CNetSchedule_JS_Guard m_Guard;
};

END_NCBI_SCOPE

#endif /* NETSCHEDULE_SQUEUE__HPP */
