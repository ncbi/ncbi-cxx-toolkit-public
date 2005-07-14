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

/// Interface to load collector
///
/// @internal
///
struct INSLB_Collector
{
    /// Server load info
    struct NSLB_ServerInfo
    {
        unsigned int     host;  ///< host address (netw. bo)
        double           rate;  ///< load rate (higher is better)

        NSLB_ServerInfo(unsigned h=0, double r=0.0) : host(h), rate(r) {}
    };

    typedef vector<NSLB_ServerInfo>  TServerList;

    /// Extract load list for servers
    /// All info should be added to the server list
    virtual
    void GetServerList(const string& service_name, TServerList* slist) const = 0;
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
    CNSLB_Coordinator(const string&     service_name,
                      INSLB_Collector*  collector, 
                      unsigned          collect_tm = 3,
                      ENonLbHostsPolicy non_lb_host = eNonLB_Allow);
    ~CNSLB_Coordinator();

    /// LB coordinator's decision about petition for a job from a 
    /// worker node host
    enum EDecision
    {
        eGrantJob,    ///< Suggest that job should be given
        eDenyJob,     ///< Host is overloaded, job should be delayed
        eHostUnknown, ///< Host unknown, decision is up to the client
        eNoLBInfo     ///< Collector got no info (yet), no decision
    };

    /// Evaluate if host is good for the job
    EDecision Evaluate(unsigned host);

    ENonLbHostsPolicy GetNonLBPolicy() const { return m_NonLB_Policy; }
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
//    auto_ptr<INSLB_Collector>                     m_Collector;
    INSLB_Collector*                     m_Collector;
    unsigned                                      m_CollectTm;
    CRef<CCollectorThread>                        m_CollectorThread;

    INSLB_Collector::TServerList*                 m_CurrSrvList;
    CRWLock                                       m_CurrSrvListLock;
    double                                        m_DenyThreashold;
    ENonLbHostsPolicy                             m_NonLB_Policy;

};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/07/14 13:12:56  kuznets
 * Added load balancer
 *
 *
 * ===========================================================================
 */

#endif