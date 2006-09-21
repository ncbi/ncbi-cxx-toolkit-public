#ifndef NETSCHEDULE_DB__HPP
#define NETSCHEDULE_DB__HPP

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
 *   Net schedule database structure.
 *
 */

/// @file ns_db.hpp
/// NetSchedule database structure.
///
/// This file collects all BDB data files, index files, and in-memory
/// structures used in netschedule
///
/// @internal

#include <corelib/ncbimtx.hpp>
#include <corelib/ncbicntr.hpp>

#include <util/bitset/ncbi_bitset.hpp>
#include <util/logrotate.hpp>

#include <bdb/bdb_file.hpp>
#include <bdb/bdb_env.hpp>
#include <bdb/bdb_cursor.hpp>
#include <bdb/bdb_bv_store.hpp>

#include <map>
#include <vector>

#include "job_status.hpp"
#include "job_time_line.hpp"
#include "queue_vc.hpp"
#include "access_list.hpp"
#include "queue_monitor.hpp"
#include "nslb.hpp"
#include "ns_affinity.hpp"

BEGIN_NCBI_SCOPE

/// BDB table to store queue information
///
/// @internal
///
struct SQueueDB : public CBDB_File
{
    CBDB_FieldUint4        id;              ///< Job id

    CBDB_FieldInt4         status;          ///< Current job status
    CBDB_FieldUint4        time_submit;     ///< Job submit time
    CBDB_FieldUint4        time_run;        ///<     run time
    CBDB_FieldUint4        time_done;       ///<     result submission time
    CBDB_FieldUint4        timeout;         ///<     individual timeout
    CBDB_FieldUint4        run_timeout;     ///<     job run timeout

    CBDB_FieldUint4        subm_addr;       ///< netw BO (for notification)
    CBDB_FieldUint4        subm_port;       ///< notification port
    CBDB_FieldUint4        subm_timeout;    ///< notification timeout

    CBDB_FieldUint4        worker_node1;    ///< IP of wnode 1 (netw BO)
    CBDB_FieldUint4        worker_node2;    ///<       wnode 2
    CBDB_FieldUint4        worker_node3;
    CBDB_FieldUint4        worker_node4;
    CBDB_FieldUint4        worker_node5;

    CBDB_FieldUint4        run_counter;     ///< Number of execution attempts
    CBDB_FieldInt4         ret_code;        ///< Return code

    CBDB_FieldUint4        time_lb_first_eval;  ///< First LB evaluation time
    /// Affinity token id (refers to the affinity dictionary DB)
    CBDB_FieldUint4        aff_id;
    CBDB_FieldUint4        mask;


    CBDB_FieldString       input;           ///< Input data
    CBDB_FieldString       output;          ///< Result data

    CBDB_FieldString       err_msg;         ///< Error message (exception::what())
    CBDB_FieldString       progress_msg;    ///< Progress report message


    CBDB_FieldString       cout;            ///< Reserved
    CBDB_FieldString       cerr;            ///< Reserved

    SQueueDB()
    {
        DisableNull(); 

        BindKey("id",      &id);

        BindData("status", &status);
        BindData("time_submit", &time_submit);
        BindData("time_run",    &time_run);
        BindData("time_done",   &time_done);
        BindData("timeout",     &timeout);
        BindData("run_timeout", &run_timeout);

        BindData("subm_addr",    &subm_addr);
        BindData("subm_port",    &subm_port);
        BindData("subm_timeout", &subm_timeout);

        BindData("worker_node1", &worker_node1);
        BindData("worker_node2", &worker_node2);
        BindData("worker_node3", &worker_node3);
        BindData("worker_node4", &worker_node4);
        BindData("worker_node5", &worker_node5);

        BindData("run_counter",        &run_counter);
        BindData("ret_code",           &ret_code);
        BindData("time_lb_first_eval", &time_lb_first_eval);
        BindData("aff_id", &aff_id);
        BindData("mask",   &mask);

        BindData("input",  &input,  kNetScheduleMaxDBDataSize);
        BindData("output", &output, kNetScheduleMaxDBDataSize);

        BindData("err_msg", &err_msg, kNetScheduleMaxDBErrSize);
        BindData("progress_msg", &progress_msg, kNetScheduleMaxDBDataSize);

        BindData("cout",  &cout, kNetScheduleMaxDBDataSize);
        BindData("cerr",  &cerr, kNetScheduleMaxDBDataSize);
    }
};

/// Index of queue database (affinity to jobs)
///
/// @internal
///
struct SQueueAffinityIdx : public CBDB_BvStore< bm::bvector<> >
{
    CBDB_FieldUint4     aff_id;

    typedef CBDB_BvStore< bm::bvector<> >  TParent;

    SQueueAffinityIdx()
    {
        DisableNull(); 
        BindKey("aff_id",   &aff_id);
    }
};

/// BDB table to store queue information
///
/// @internal
///
struct SAffinityDictDB : public CBDB_File
{
    CBDB_FieldUint4        aff_id;        ///< Affinity token id
    CBDB_FieldString       token;         ///< Affinity token

    SAffinityDictDB()
    {
        DisableNull(); 
        BindKey("aff_id",  &aff_id);
        BindData("token",  &token,  kNetScheduleMaxDBDataSize);
    }
};

/// BDB affinity token index
///
/// @internal
///
struct SAffinityDictTokenIdx : public CBDB_File
{
    CBDB_FieldString       token;
    CBDB_FieldUint4        aff_id;

    SAffinityDictTokenIdx()
    {
        DisableNull(); 
        BindKey("token",  &token,  kNetScheduleMaxDBDataSize);
        BindData("aff_id",  &aff_id);
    }
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
    SQueueDB                        db;               ///< Main queue database
    SQueueAffinityIdx               aff_idx;          ///< Q affinity index
    auto_ptr<CBDB_FileCursor>       cur;              ///< DB cursor
    CFastMutex                      lock;             ///< db, cursor lock
    CNetScheduler_JobStatusTracker  status_tracker;   ///< status FSA

    // affinity dictionary does not need a mutex, because 
    // CAffinityDict is a syncronized class itself (mutex included)
    CAffinityDict                   affinity_dict;    ///< Affinity tokens
    CWorkerNodeAffinity             worker_aff_map;   ///< Affinity map
    CFastMutex                      aff_map_lock;     ///< worker_aff_map lck

    // queue parameters
    int                             timeout;        ///< Result exp. timeout
    int                             notif_timeout;  ///< Notification interval
    bool                            delete_done;    ///< Delete done jobs
    /// How many attemts to make on different nodes before failure
    unsigned                        failed_retries;

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

    SLockedQueue(const string& queue_name) 
        : timeout(3600), 
          notif_timeout(7), 
          delete_done(false),
          failed_retries(0),
          last_notif(0), 
          q_notif("NCBI_JSQ_"),
          run_time_line(0),

          rec_dump("jsqd_"+queue_name+".dump", 10 * (1024 * 1024)),
          rec_dump_flag(false),
          lb_flag(false),
          lb_coordinator(0),
          lb_stall_delay_type(eNSLB_Constant),
          lb_stall_time(6),
          lb_stall_time_mult(1.0)
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
        delete run_time_line;
        delete lb_coordinator;
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
    TQueueMap              m_QMap;
    mutable CRWLock        m_Lock;
};






END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2006/09/21 21:28:59  joukovv
 * Consistency of memory state and database strengthened, ability to retry failed
 * jobs on different nodes (and corresponding queue parameter, failed_retries)
 * added, overall code regularization performed.
 *
 * Revision 1.4  2006/07/19 15:53:34  kuznets
 * Extended database size to accomodate escaped strings
 *
 * Revision 1.3  2006/06/27 15:39:42  kuznets
 * Added int mask to jobs to carry flags(like exclusive)
 *
 * Revision 1.2  2006/04/17 15:46:54  kuznets
 * Added option to remove job when it is done (similar to LSF)
 *
 * Revision 1.1  2006/02/06 14:10:29  kuznets
 * Added job affinity
 *
 *
 * ===========================================================================
 */

#endif /* NETSCHEDULE_DB__HPP */

