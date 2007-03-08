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

#include "ns_types.hpp"
#include "ns_db.hpp"
#include "job_status.hpp"
#include "queue_vc.hpp"
#include "access_list.hpp"
#include "queue_monitor.hpp"
#include "ns_affinity.hpp"

#include "weak_ref.hpp"

BEGIN_NCBI_SCOPE


/// Queue parameters
struct SQueueParameters
{
    enum ELBCurveType {
        eLBLinear = 0,
        eLBRegression = 1
    };

    /// General parameters
    int timeout;
    int notif_timeout;
    int run_timeout;
    int run_timeout_precision;
    string program_name;
    bool delete_when_done;
    int failed_retries;
    time_t empty_lifetime;
    string subm_hosts;
    string wnode_hosts;
    bool dump_db;

    /// Parameters for Load Balancing
    bool   lb_flag;
    string lb_service;
    int    lb_collect_time;
    string lb_unknown_host;
    string lb_exec_delay_str;
    double lb_stall_time_mult;
    ELBCurveType lb_curve;
    double lb_curve_high;
    double lb_curve_linear_low;
    double lb_curve_regression_a;

    SQueueParameters() :
        timeout(3600),
        notif_timeout(7),
        run_timeout(3600),
        run_timeout_precision(3600),
        program_name(""),
        delete_when_done(false),
        failed_retries(0),
        subm_hosts(""),
        wnode_hosts(""),
        dump_db(false),

        lb_flag(false),
        lb_service(""),
        lb_collect_time(5),
        lb_unknown_host(""),
        lb_exec_delay_str(""),
        lb_stall_time_mult(0.5),
        lb_curve(eLBLinear),
        lb_curve_high(0.6),
        lb_curve_linear_low(0.15),
        lb_curve_regression_a(0)
    { }
    ///
    void Read(const IRegistry& reg, const string& sname);
};


/// @internal
enum ENSLB_RunDelayType
{
    eNSLB_Constant,   ///< Constant delay
    eNSLB_RunTimeAvg  ///< Running time based delay (averaged)
};


/// Types of event notification subscribers
///
/// @internal
enum ENetScheduleListenerType
{
    eNS_Worker        = (1 << 0),  ///< Regular worker node
    eNS_BackupWorker  = (1 << 1)   ///< Backup worker node (second priority notifications)
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
    TNetScheduleListenerType   client_type;  ///< Client type mask

    SQueueListener(unsigned int             host_addr,
                   unsigned short           udp_port_number,
                   time_t                   curr,
                   int                      expiration_timeout,
                   const string&            client_auth,
                   TNetScheduleListenerType ctype = eNS_Worker)
    : host(host_addr),
      udp_port(udp_port_number),
      last_connect(curr),
      timeout(expiration_timeout),
      auth(client_auth),
      client_type(ctype)
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
typedef map<TNSTag, TNSBitVector*> TNSTagMap;
// Safe container for tag map
class CNSTagMap
{
public:
    CNSTagMap(SLockedQueue& queue);
    ~CNSTagMap();
    TNSTagMap& operator*() { return m_TagMap; }
private:
    TNSTagMap m_TagMap;
    TNSBVPool* m_BVPool;
};


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
    TNSBVPool* m_BVPool;
};


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
        eKindStatic = 0,
        eKindDynamic = 1
    };
    typedef int TQueueKind;
    string                       qname;
    string                       qclass;           ///< Parameter class
    TQueueKind                   kind;             ///< 0 - static, 1 - dynamic

    // Databases
    SQueueDB                     db;               ///< Main queue database
    SQueueAffinityIdx            aff_idx;          ///< Q affinity index
    auto_ptr<CBDB_FileCursor>    m_Cursor;         ///< DB cursor
    CFastMutex                   lock;             ///< db, cursor lock
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

    // queue parameters
    int                          timeout;        ///< Result exp. timeout
    int                          notif_timeout;  ///< Notification interval
    bool                         delete_done;    ///< Delete done jobs
    /// How many attemts to make on different nodes before failure
    unsigned                     failed_retries;
    int                          empty_lifetime; ///< How long to live after empty

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
    int                          run_timeout;
    CRWLock                      rtl_lock;      ///< run_time_line locker

    // datagram notification socket 
    // (used to notify worker nodes and waiting clients)
    CDatagramSocket              udp_socket;    ///< UDP notification socket
    CFastMutex                   us_lock;       ///< UDP socket lock

    /// Client program version control
    CQueueClientInfoList         program_version_list;

    /// Host access list for job submission
    CNetSchedule_AccessList      subm_hosts;
    /// Host access list for job execution (workers)
    CNetSchedule_AccessList      wnode_hosts;

    /// Queue monitor
    CNetScheduleMonitor          monitor;

    /// Database records when they are deleted can be dumped to an archive
    /// file for further processing
    CRotatingLogStream           rec_dump;
    CFastMutex                   rec_dump_lock;
    bool                         rec_dump_flag;

    mutable bool                 lb_flag;  ///< Load balancing flag
    CNSLB_Coordinator*           lb_coordinator;

    ENSLB_RunDelayType           lb_stall_delay_type;
    unsigned                     lb_stall_time;      ///< job delay (seconds)
    double                       lb_stall_time_mult; ///< stall coeff
    CFastMutex                   lb_stall_time_lock;

    SQueueStatictics             qstat;
    CFastMutex                   qstat_lock;

    /// should we delete db upon close?
    bool                         delete_database;
    vector<string>               files; ///< list of files (paths) for queue

    /// last valid id for queue
    CAtomicCounter               m_LastId;
    // Moved here from CQueueDataBase
    /// vector of jobs to be deleted from db unconditionally
    TNSBitVector                 m_JobsToDelete;
    /// its lock
    CFastMutex                   m_JobsToDeleteLock;

    TNSBVPool                    m_BVPool;

//public:
    // Constructor/destructor
    SLockedQueue(const string& queue_name,
        const string& qclass_name, TQueueKind queue_kind);
    ~SLockedQueue();

    void Open(CBDB_Env& env, const string& path);
    void x_ReadFieldInfo(void);

    int GetFieldIndex(const string& name);
    string SLockedQueue::GetField(int index);

    /// get next job id (counter increment)
    unsigned int GetNextId();
    /// Returns first id for the batch
    unsigned int GetNextIdBatch(unsigned count);

    // Erase job from all structures, request delayed db deletion
    void Erase(unsigned job_id);

    // Tags methods
    typedef CSimpleBuffer TBuffer;
    void SetTagDbTransaction(CBDB_Transaction* trans);
    void AppendTags(CNSTagMap& tag_map, TNSTagList& tags, unsigned job_id);
    void FlushTags(CNSTagMap& tag_map, CBDB_Transaction& trans);
    bool ReadTag(const string& key, const string& val,
                 TBuffer* buf);
    void ReadTags(const string& key, TNSBitVector* bv);
    void ReadTagDetailsFor(const TNSBitVector* ids, const string& key,
        CNSTagDetails& tag_details);
    void x_RemoveTags(CBDB_Transaction& trans, const TNSBitVector& ids);
    CFastMutex& GetTagLock() { return m_TagLock; }

    unsigned DeleteBatch(unsigned batch_size);

    CBDB_FileCursor* GetCursor(CBDB_Transaction& trans);

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
        eStatNumEvents
    };
    typedef unsigned TStatEvent;
    CAtomicCounter m_EventCounter[eStatNumEvents];
    unsigned m_Average[eStatNumEvents];
    void CountEvent(TStatEvent);
    double GetAverage(TStatEvent);
};


template <>
class CLockerTraits<SLockedQueue>
{
public:
    typedef CIntrusiveLocker<SLockedQueue> TLockerType;
};


END_NCBI_SCOPE

#endif /* NETSCHEDULE_SQUEUE__HPP */
