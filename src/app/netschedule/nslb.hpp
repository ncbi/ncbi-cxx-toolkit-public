#ifndef NETSCHEDULE_LB__HPP
#define NETSCHEDULE_LB__HPP

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
 *   Interface with the load balancer(s)
 *
 */

/// @file load_balance.hpp
/// NetSchedule load balancing interface
///
/// @internal

#include <corelib/ncbimtx.hpp>
#include <util/thread_nonstop.hpp>
#include <util/resource_pool.hpp>
#include <util/bitset/ncbi_bitset.hpp>

BEGIN_NCBI_SCOPE

/// Interface to load info collector
///
/// @internal
///
struct INSLB_Collector
{
    /// Server load info
    struct NSLB_ServerInfo
    {
        unsigned int     host;          ///< host address (netw. bo)
        double           rate;          ///< load rate (higher is better)
        unsigned         cpu_count;     ///< Number od CPUs
        double           cpu_wait_proc; ///< AVG numb. of waiting procs.
        double           cpu_run_queue; ///< Number of tasks waiting for CPU

        NSLB_ServerInfo(unsigned h=0, 
                        double   r=0.0,
                        unsigned cpu = 0,
                        double   wait_proc = 0.0,
                        double   run_queue = 0.0) 
        : host(h), rate(r), 
          cpu_count(cpu), cpu_wait_proc(wait_proc), cpu_run_queue(run_queue)
        {}
    };

    typedef vector<NSLB_ServerInfo>  TServerList;
    
    virtual ~INSLB_Collector() {};

    /// Extract load list for servers
    /// All info should be added to the server list
    virtual
    void GetServerList(const string& service_name, TServerList* slist) const = 0;
};


/// Threashold is used to decide if host has resources to be granted a job
/// Curve reflects how threshold changes over time while job sits in the queue
///
/// @internal
///
class CNSLB_ThreasholdCurve
{
public:
    typedef vector<double> TCurve;

public:
    virtual ~CNSLB_ThreasholdCurve() {};

    /// Curve generation
    ///
    /// @param length
    ///    
    virtual void ReGenerateCurve(unsigned length) = 0;
    const TCurve& GetCurve() const { return m_Curve; }
protected:
    TCurve    m_Curve;
};


/// Linear curve
///  y = ax + b
///
/// @internal
///
class CNSLB_ThreasholdCurve_Linear : public CNSLB_ThreasholdCurve
{
public:
    /// Construction
    ///
    /// @param a
    ///    a coefficient
    /// @param b
    ///    b coefficient (initial threahold)
    /// @param derive_a
    ///    flag that a should be derived (recalculated automatically)
    CNSLB_ThreasholdCurve_Linear(double  a, 
                                 double  b, 
                                 bool    derive_a);

    /// Two points curve construction
    CNSLB_ThreasholdCurve_Linear(double  y0, 
                                 double  yN);


    virtual void ReGenerateCurve(unsigned length);
private:
    unsigned m_Constr;

    double m_A;
    double m_B;
    bool   m_DeriveA;

    double m_Y0;
    double m_YN;
};


/// Regression curve
///  Y(N) = Y(N-1) + [ Y(N-1) * A ]
///
/// @internal
///
class CNSLB_ThreasholdCurve_Regression : public CNSLB_ThreasholdCurve
{
public:
    /// Construction
    ///
    CNSLB_ThreasholdCurve_Regression(double  y0, 
                                     double  a = -0.2);

    virtual void ReGenerateCurve(unsigned length);
private:
    double m_Y0;
    double m_A;
};




/// Class making decision to grant job or postpone execution
///
/// @internal
class CNSLB_DecisionModule
{
public:
    /// LB coordinator's decision about petition for a job from a 
    /// worker node host
    ///
    enum EDecision
    {
        eGrantJob,        ///< Suggest that job should be given
        eDenyJob,         ///< Host is overloaded, job should be delayed
        eHostUnknown,     ///< Host unknown, decision is up to the client
        eNoLBInfo,        ///< Collector got no info (yet), no decision
        eInsufficientInfo ///< Not enough information 
    };

    static string DecisionToStrint(EDecision decision);

public:
    virtual ~CNSLB_DecisionModule() {}

public:

    /// Job petition
    ///
    struct SPetition
    {
        ///< Worker ready for a job
        unsigned                                 host;
        ///< Time (in seconds) from the first evaluation
        unsigned                                 time_eval;
        ///< Complete LB info (all hosts)
        INSLB_Collector::TServerList*            lb_host_info;
        /// Threshold curve vector
        const CNSLB_ThreasholdCurve::TCurve*     threshold_curve;
    };

    virtual 
    EDecision Evaluate(const SPetition& petition) const = 0;
};

/// Decision module considers best rate hosts as most favorable candidates
///
/// @internal
///
class CNSLB_DecisionModule_DistributeRate : public CNSLB_DecisionModule
{
public:
    virtual 
    EDecision Evaluate(const SPetition& petition) const;
};


/// Decision module for CPU availability evaluation
///
/// @internal
///
class CNSLB_DecisionModule_CPU_Avail : public CNSLB_DecisionModule
{
public:
    virtual 
    EDecision Evaluate(const SPetition& petition) const;
};


/// LB collector for LBSM
///
/// @internal
class CNSLB_LBSMD_Collector : public INSLB_Collector
{
public:
    CNSLB_LBSMD_Collector();

    virtual
    void GetServerList(const string& service_name, TServerList* slist) const;
private:
    CNSLB_LBSMD_Collector(const CNSLB_LBSMD_Collector&);
    CNSLB_LBSMD_Collector& operator=(const CNSLB_LBSMD_Collector&);
private:
    mutable bm::bvector<>     m_Hosts;
};

/// LB coordinator
///
/// @internal
///
class CNSLB_Coordinator
{
public:
    /// Policy on how to coordinate non-lb hosts
    enum ENonLbHostsPolicy {
        eNonLB_Deny,     ///< Deny access from non-lb hosts
        eNonLB_Allow,    ///< Allow access from non-lb hosts
        eNonLB_Reserve   ///< Allow access when situation dictates
    };
public:

    /// Construction
    ///
    /// @param service_name
    ///   Service name to collect workload information
    /// @param collector
    ///   Load balancing info collector (coordinator takes ownership)
    /// @param collect_tm
    ///   re-collection period in seconds, 0 - means collect all the time
    ///   recollection runs in a separate thread
    /// @param non_lb_host
    ///   Policy towards non load balanced hosts
    ///
    CNSLB_Coordinator(const string&           service_name,
                      INSLB_Collector*        collector, 
                      CNSLB_ThreasholdCurve*  curve,
                      CNSLB_DecisionModule*   decision_maker,
                      unsigned                collect_tm = 3,
                      ENonLbHostsPolicy       non_lb_host = eNonLB_Allow);
    ~CNSLB_Coordinator();

    /// Evaluate if host is good for the job
    /// @param host
    ///    worker node address
    /// @param time_eval
    ///    time in seconds from the first evaluation
    ///
    CNSLB_DecisionModule::EDecision Evaluate(unsigned host, 
                                             unsigned time_eval);

    ENonLbHostsPolicy GetNonLBPolicy() const { return m_NonLB_Policy; }

    /// Regenerate threashold curve 
    /// @param length
    ///    Execution delay time (seconds)
    void ReGenerateCurve(unsigned length);
private:

    INSLB_Collector::TServerList* GetSrvList();

    void PutSrvList(INSLB_Collector::TServerList* srv_lst);

    const string& GetServiceName() const { return m_ServiceName; }

public:
    void RunCollectorThread();
    void StopCollectorThread();

private:


    /// @internal
    class CCollectorThread : public CThreadNonStop
    {
    public:
        CCollectorThread(INSLB_Collector&     collector,
                         CNSLB_Coordinator&   parent,
                         unsigned             run_delay);
        ~CCollectorThread();
        virtual void DoJob(void);

        /// Returns current server list or 0 if server list is empty
        /// caller is responsible for recycling
        INSLB_Collector::TServerList* GetSrvList();

    private:
        INSLB_Collector&                   m_Coll;
        CNSLB_Coordinator&                 m_Parent;
        INSLB_Collector::TServerList*      m_SrvList;
        CFastMutex                         m_SrvLstLock;
    };

    friend class CCollectorThread;

private:
    CNSLB_Coordinator(const CNSLB_Coordinator& coord);
    CNSLB_Coordinator& operator=(const CNSLB_Coordinator& coord);

private:
    CResourcePool<INSLB_Collector::TServerList>   m_SrvLstPool;
    CFastMutex                                    m_SrvLstPoolLock;

    string                                        m_ServiceName;
    INSLB_Collector*                              m_Collector;
    unsigned                                      m_CollectTm;
    CRef<CCollectorThread>                        m_CollectorThread;

    INSLB_Collector::TServerList*                 m_CurrSrvList;
    CNSLB_ThreasholdCurve*                        m_Curve;
    CRWLock                                       m_CurrSrvCurveLock;

    CNSLB_DecisionModule*                         m_DecisionMaker;
    ENonLbHostsPolicy                             m_NonLB_Policy;

};


END_NCBI_SCOPE

#endif
