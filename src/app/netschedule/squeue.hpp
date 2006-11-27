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

#include "ns_db.hpp"
#include "job_status.hpp"
#include "queue_vc.hpp"
#include "access_list.hpp"
#include "queue_monitor.hpp"
#include "ns_affinity.hpp"

BEGIN_NCBI_SCOPE


/// Main parameters
struct SQueueParameters
{
    int timeout;
    int notif_timeout;
    int run_timeout;
    int run_timeout_precision;
    string program_name;
    bool delete_when_done;
    int failed_retries;
    string subm_hosts;
    string wnode_hosts;
    bool dump_db;

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
        dump_db(false)
    { }
    ///
    void Read(const IRegistry& reg, const string& sname);
};


enum ELBCurveType {
    eLBLinear = 0,
    eLBRegression = 1
};
/// Parameters for Load Balancing
struct SQueueLBParameters
{
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

    SQueueLBParameters() :
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

class CJobTimeLine;
class CNSLB_Coordinator;
/// Mutex protected Queue database with job status FSM 
///
/// Class holds the queue database (open files and indexes), 
/// thread sync mutexes and classes auxiliary queue management concepts
/// (like affinity and job status bit-matrix)
///
/// @internal
///
struct SLockedQueue
{
    SQueueDB                     db;               ///< Main queue database
    SQueueAffinityIdx            aff_idx;          ///< Q affinity index
    auto_ptr<CBDB_FileCursor>    cur;              ///< DB cursor
    CFastMutex                   lock;             ///< db, cursor lock
    CJobStatusTracker            status_tracker;   ///< status FSA

    // affinity dictionary does not need a mutex, because 
    // CAffinityDict is a syncronized class itself (mutex included)
    CAffinityDict                affinity_dict;    ///< Affinity tokens
    CWorkerNodeAffinity          worker_aff_map;   ///< Affinity map
    CFastMutex                   aff_map_lock;     ///< worker_aff_map lck

    // queue parameters
    int                          timeout;        ///< Result exp. timeout
    int                          notif_timeout;  ///< Notification interval
    bool                         delete_done;    ///< Delete done jobs
    /// How many attemts to make on different nodes before failure
    unsigned                     failed_retries;

    // List of active worker node listeners waiting for pending jobs

    typedef vector<SQueueListener*> TListenerList;

    TListenerList                wnodes;       ///< worker node listeners
    time_t                       last_notif;   ///< last notification time
    CRWLock                      wn_lock;      ///< wnodes locker
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

    SLockedQueue(const string& queue_name);
    ~SLockedQueue();

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
    CAtomicCounter m_GetCounter;
    CAtomicCounter m_PutCounter;
    unsigned m_GetAverage;
    unsigned m_PutAverage;
//public:
    double GetGetAverage(void);
    double GetPutAverage(void);
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2006/11/27 16:46:21  joukovv
 * Iterator to CQueueCollection introduced to decouple it with CQueueDataBase;
 * un-nested CQueue from CQueueDataBase; instrumented code to count job
 * throughput average.
 *
 * Revision 1.1  2006/10/31 19:35:26  joukovv
 * Queue creation and reading of its parameters decoupled. Code reorganized to
 * reduce coupling in general. Preparing for queue-on-demand.
 *
 * ===========================================================================
 */

#endif /* NETSCHEDULE_SQUEUE__HPP */
