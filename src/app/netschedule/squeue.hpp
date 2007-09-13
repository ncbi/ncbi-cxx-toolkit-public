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

#include <util/logrotate.hpp>
#include <util/thread_nonstop.hpp>
#include <util/time_line.hpp>

#include <connect/server_monitor.hpp>

#include "ns_types.hpp"
#include "ns_db.hpp"
#include "job_status.hpp"
#include "queue_vc.hpp"
#include "access_list.hpp"
#include "ns_affinity.hpp"

#include "weak_ref.hpp"

#include <deque>

BEGIN_NCBI_SCOPE


/// Queue parameters
struct SQueueParameters
{
    /// General parameters, reconfigurable at run time
    int timeout;
    int notif_timeout;
    int run_timeout;
    string program_name;
    bool delete_when_done;
    int failed_retries;
    time_t empty_lifetime;
    unsigned max_input_size;
    unsigned max_output_size;
    bool deny_access_violations;
    bool log_access_violations;
    string subm_hosts;
    string wnode_hosts;
    // This parameter is not reconfigurable
    int run_timeout_precision;

    SQueueParameters() :
        timeout(3600),
        notif_timeout(7),
        run_timeout(3600),
        program_name(""),
        delete_when_done(false),
        failed_retries(0),
        empty_lifetime(0),
        max_input_size(kNetScheduleMaxDBDataSize),
        max_output_size(kNetScheduleMaxDBDataSize),
        deny_access_violations(true),
        log_access_violations(false),
        subm_hosts(""),
        wnode_hosts(""),
        run_timeout_precision(3600)
    { }
    ///
    void Read(const IRegistry& reg, const string& sname);
};


/// @internal
typedef unsigned TNetScheduleListenerType;


/// Queue watcher (client) description. Client is predominantly a worker node
/// waiting for new jobs.
///
/// @internal
///
struct SQueueListener
{
    unsigned int               host;         ///< host name (network BO)
    unsigned short             udp_port;     ///< Listening UDP port
    time_t                     last_connect; ///< Last registration timestamp
    int                        timeout;      ///< Notification expiration timeout
    string                     auth;         ///< Authentication string

    SQueueListener(unsigned int             host_addr,
                   unsigned short           udp_port_number,
                   time_t                   curr,
                   int                      expiration_timeout,
                   const string&            client_auth)
    : host(host_addr),
      udp_port(udp_port_number),
      last_connect(curr),
      timeout(expiration_timeout),
      auth(client_auth)
    {}
};


class CQueueComparator
{
public:
    CQueueComparator(unsigned host, unsigned port) :
      m_Host(host), m_Port(port)
    {}
    bool operator()(SQueueListener *l) {
        return l->host == m_Host && l->udp_port == m_Port;
    }
private:
    unsigned m_Host;
    unsigned m_Port;
};


/// Runtime queue statistics
///
/// @internal
struct SQueueStatictics
{
    double   total_run_time; ///< Accumulated job running time
    unsigned run_count;      ///< Number of runs

    double   total_turn_around_time; ///< time from subm to completion

    SQueueStatictics() 
        : total_run_time(0.0), run_count(0), total_turn_around_time(0)
    {}
};


typedef pair<string, string> TNSTag;
typedef list<TNSTag> TNSTagList;

// Forward for bv-pooled structures
struct SLockedQueue;

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


/* obsolete
// another representation key -> value, bitvector
typedef pair<string, TNSBitVector*> TNSTagValue;
typedef vector<TNSTagValue> TNSTagDetails;
// Safe container for tag details
class CNSTagDetails
{
public:
    CNSTagDetails(SLockedQueue& queue);
    ~CNSTagDetails();
    TNSTagDetails& operator*() { return m_TagDetails; }
private:
    TNSTagDetails m_TagDetails;
};
*/


// slight violation of naming convention for porting to util/time_line
typedef CTimeLine<TNSBitVector> CJobTimeLine;
class CNSLB_Coordinator;
/// Mutex protected Queue database with job status FSM 
///
/// Class holds the queue database (open files and indexes), 
/// thread sync mutexes and classes auxiliary queue management concepts
/// (like affinity and job status bit-matrix)
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
    string                       qname;
    string                       qclass;           ///< Parameter class
    TQueueKind                   kind;             ///< 0 - static, 1 - dynamic

    // Databases
    SQueueDB                     db;               ///< Main queue database
    SJobInfoDB                   m_JobInfoDB;      ///< Aux info on jobs, tags etc.
    SQueueAffinityIdx            aff_idx;          ///< Q affinity index
    auto_ptr<CBDB_FileCursor>    m_Cursor;         ///< DB cursor
private:
    friend class CQueueGuard;
    CFastMutex                   lock;             ///< db, cursor lock
public:
    CJobStatusTracker            status_tracker;   ///< status FSA

    // Main DB field info
    map<string, int> m_FieldMap;
    int m_NKeys;

    // affinity dictionary does not need a mutex, because 
    // CAffinityDict is a syncronized class itself (mutex included)
    CAffinityDict                affinity_dict;    ///< Affinity tokens
    CWorkerNodeAffinity          worker_aff_map;   ///< Affinity map
    CFastMutex                   aff_map_lock;     ///< worker_aff_map lck

    STagDB                       m_TagDb;
    CFastMutex                   m_TagLock;

    ///< When it became empty, guarded by 'lock'
    time_t                       became_empty;

    // List of active worker node listeners waiting for pending jobs

    typedef vector<SQueueListener*> TListenerList;

    TListenerList                wnodes;       ///< worker node listeners
    time_t                       last_notif;   ///< last notification time
    mutable CRWLock              wn_lock;      ///< wnodes locker
    string                       q_notif;      ///< Queue notification message

    // Timeline object to control job execution timeout
    CJobTimeLine*                run_time_line;
    CRWLock                      rtl_lock;      ///< run_time_line locker

    // Datagram notification socket 
    // (used to notify worker nodes and waiting clients)
    CDatagramSocket              udp_socket;    ///< UDP notification socket
    CFastMutex                   us_lock;       ///< UDP socket lock

    /// Queue monitor
    CServer_Monitor              m_Monitor;

    SQueueStatictics             qstat;
    CFastMutex                   qstat_lock;

    /// Should we delete db upon close?
    bool                         delete_database;
    vector<string>               files; ///< list of files (paths) for queue

    /// Last valid id for queue
    CAtomicCounter               m_LastId;

    /// Lock for deleted jobs vectors
    CFastMutex                   m_JobsToDeleteLock;
    /// Database for vectors of deleted jobs
    SDeletedJobsDB               m_DeletedJobsDB;
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

public:
    // Constructor/destructor
    SLockedQueue(const string& queue_name,
        const string& qclass_name, TQueueKind queue_kind);
    ~SLockedQueue();

    void Open(CBDB_Env& env, const string& path);
    void x_ReadFieldInfo(void);

    // Thread-safe parameter set
    void SetParameters(const SQueueParameters& params);

    int GetFieldIndex(const string& name);
    string GetField(int index);

    unsigned LoadStatusMatrix();

    /// get next job id (counter increment)
    unsigned int GetNextId();
    /// Returns first id for the batch
    unsigned int GetNextIdBatch(unsigned count);

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

    // Tags methods
    typedef CSimpleBuffer TBuffer;
    void SetTagDbTransaction(CBDB_Transaction* trans);
    void AppendTags(CNSTagMap& tag_map, TNSTagList& tags, unsigned job_id);
    void FlushTags(CNSTagMap& tag_map, CBDB_Transaction& trans);
    bool ReadTag(const string& key, const string& val,
                 TBuffer* buf);
    void ReadTags(const string& key, TNSBitVector* bv);
    void x_RemoveTags(CBDB_Transaction& trans, const TNSBitVector& ids);
    CFastMutex& GetTagLock() { return m_TagLock; }

    unsigned DeleteBatch(unsigned batch_size);

    CBDB_FileCursor* GetCursor(CBDB_Transaction& trans);


    /// Pass socket for monitor (takes ownership)
    void SetMonitorSocket(CSocket& socket);
    /// Are we monitoring?
    bool IsMonitoring();
    /// Send string to monitor
    void MonitorPost(const string& msg);

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
        eStatGetEvent  = 0,
        eStatPutEvent  = 1,
        eStatDBLockEvent = 2,
        eStatNumEvents
    };
    typedef unsigned TStatEvent;
    CAtomicCounter m_EventCounter[eStatNumEvents];
    unsigned m_Average[eStatNumEvents];
    void CountEvent(TStatEvent);
    double GetAverage(TStatEvent);
private:
    // Configurable queue parameters
    friend class CQueueParamAccessor;
    CRWLock                      m_ParamLock;
    int                          m_Timeout;         ///< Result exp. timeout
    int                          m_NotifTimeout;    ///< Notification interval
    bool                         m_DeleteDone;      ///< Delete done jobs
    int                          m_RunTimeout;      ///< Execution timeout
    /// How many attemts to make on different nodes before failure
    unsigned                     m_FailedRetries;
    ///< How long to live after becoming empty, if -1 - infinitely
    int                          m_EmptyLifetime;
    unsigned                     m_MaxInputSize;                    
    unsigned                     m_MaxOutputSize;
    bool                         m_DenyAccessViolations;
    bool                         m_LogAccessViolations;
    /// Client program version control
    CQueueClientInfoList         m_ProgramVersionList;
    /// Host access list for job submission
    CNetSchedule_AccessList      m_SubmHosts;
    /// Host access list for job execution (workers)
    CNetSchedule_AccessList      m_WnodeHosts;
};


class CQueueParamAccessor
{
public:
    CQueueParamAccessor(SLockedQueue& queue) :
        m_Queue(queue), m_Guard(queue.m_ParamLock) { }
    int GetTimeout() { return m_Queue.m_Timeout; }
    int GetNotifTimeout() { return m_Queue.m_NotifTimeout; }
    bool GetDeleteDone() { return m_Queue.m_DeleteDone; }
    int GetRunTimeout() { return m_Queue.m_RunTimeout; }
    unsigned GetFailedRetries() { return m_Queue.m_FailedRetries; }
    int GetEmptyLifetime() { return m_Queue.m_EmptyLifetime; }
    unsigned GetMaxInputSize() { return m_Queue.m_MaxInputSize; }
    unsigned GetMaxOutputSize() { return m_Queue.m_MaxOutputSize; }
    bool GetDenyAccessViolations() { return m_Queue.m_DenyAccessViolations; }
    bool GetLogAccessViolations() { return m_Queue.m_LogAccessViolations; }
    const CQueueClientInfoList& GetProgramVersionList()
        { return m_Queue.m_ProgramVersionList; }
    const CNetSchedule_AccessList& GetSubmHosts()
        { return m_Queue.m_SubmHosts; }
    const CNetSchedule_AccessList& GetWnodeHosts()
        { return m_Queue.m_WnodeHosts; }

private:
    SLockedQueue& m_Queue;
    CReadLockGuard m_Guard;
};


template <>
class CLockerTraits<SLockedQueue>
{
public:
    typedef CIntrusiveLocker<SLockedQueue> TLockerType;
};


class CQueueGuard
{
public:
    CQueueGuard() : m_Queue(0)
    {
    }

    CQueueGuard(CRef<SLockedQueue> q) : m_Queue(0)
    {
        Guard(q);
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
            m_Queue->lock.Unlock();
            m_Queue = 0;
        }
    }

    void Guard(CRef<SLockedQueue> q)
    {
        Release();
        m_Queue = q;
        m_Queue->lock.Lock();
        m_Queue->CountEvent(SLockedQueue::eStatDBLockEvent);
    }

private:
    // No copy
    CQueueGuard(const CQueueGuard&);
    void operator=(const CQueueGuard&);
private:
    SLockedQueue* m_Queue;
};

END_NCBI_SCOPE

#endif /* NETSCHEDULE_SQUEUE__HPP */
