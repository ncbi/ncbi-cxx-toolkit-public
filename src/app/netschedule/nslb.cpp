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
 * File Description: Network scheduler load balancer.
 *
 */

#include <ncbi_pch.hpp>
#include "nslb.hpp"

#include <connect/ncbi_service.h>



BEGIN_NCBI_SCOPE

CNSLB_Coordinator::CNSLB_Coordinator(const string&    service_name,
                                     INSLB_Collector* collector, 
                                     unsigned         collect_tm,
                                     ENonLbHostsPolicy non_lb_host)
 :  m_ServiceName(service_name),
    m_Collector(collector),
    m_CollectTm(collect_tm),
    m_CurrSrvList(0),
    m_DenyThreashold(0.20),
    m_NonLB_Policy(non_lb_host)
{
    RunCollectorThread();
}

CNSLB_Coordinator::~CNSLB_Coordinator()
{
    if (m_CurrSrvList) {
        PutSrvList(m_CurrSrvList);
    }
    delete m_Collector;
}

void CNSLB_Coordinator::RunCollectorThread()
{
# ifdef NCBI_THREADS
       LOG_POST(Info << "Starting LB collector thread.");
       m_CollectorThread.Reset(new CCollectorThread(
                                               *m_Collector,
                                               *this,
                                               m_CollectTm));

       m_CollectorThread->Run();
# else
        LOG_POST(Warning << 
                 "Cannot run background thread in non-MT configuration.");
# endif
}

void CNSLB_Coordinator::StopCollectorThread()
{
# ifdef NCBI_THREADS
    if (!m_CollectorThread.Empty()) {
        LOG_POST(Info << "Stopping LB collection thread...");
        m_CollectorThread->RequestStop();
        m_CollectorThread->Join();
        LOG_POST(Info << "Stopped.");
    }
# endif
}

INSLB_Collector::TServerList* CNSLB_Coordinator::GetSrvList()
{
    CFastMutexGuard guard(m_SrvLstPoolLock);
    return m_SrvLstPool.Get();
}

void CNSLB_Coordinator::PutSrvList(INSLB_Collector::TServerList* srv_lst)
{
    CFastMutexGuard guard(m_SrvLstPoolLock);
    m_SrvLstPool.Put(srv_lst);
}

CNSLB_Coordinator::EDecision CNSLB_Coordinator::Evaluate(unsigned host)
{
    // get the updated list from the collector thread, if new list
    // is not available 
    INSLB_Collector::TServerList* slist = m_CollectorThread->GetSrvList();
    if (slist != 0) {
        CWriteLockGuard guard(m_CurrSrvListLock);
        if (m_CurrSrvList) {
            PutSrvList(m_CurrSrvList);
        }
        m_CurrSrvList = slist;
    }

    INSLB_Collector::NSLB_ServerInfo best_host;
    INSLB_Collector::NSLB_ServerInfo worst_host;
    best_host.rate = 0.0;
    worst_host.rate = 9999999999;

    const INSLB_Collector::NSLB_ServerInfo* req_host = 0;

    {{
    CReadLockGuard guard(m_CurrSrvListLock);
    if (m_CurrSrvList == 0 || m_CurrSrvList->size() == 0) {
        return eNoLBInfo;
    }

    // Iterate the available load list, find fittest host, worst host

    ITERATE(INSLB_Collector::TServerList, it, *m_CurrSrvList) {
        if (it->rate > best_host.rate) {
            best_host = *it;
        }
        if (it->rate < worst_host.rate) {
            worst_host = *it;
        }
        if (it->host == host) {
            req_host = &*it;
        }
    }

    }}

    if (req_host == 0) {
        return eHostUnknown;
    }

    if (req_host->host == best_host.host) {
        return eGrantJob;
    }

    // evaluate the persentage between best-current-worst

    double rate_interval = best_host.rate - worst_host.rate;
    _ASSERT(rate_interval > 0);

    if (rate_interval < 0.01) {
        return eGrantJob;
    }
    double req_interval = best_host.rate - req_host->rate;
    double ratio = req_interval / rate_interval;
    if (ratio >= m_DenyThreashold) {
        return eGrantJob;
    }
    return eDenyJob;
}




CNSLB_Coordinator::CCollectorThread::CCollectorThread(
                         INSLB_Collector&     collector,
                         CNSLB_Coordinator&   parent,
                         unsigned             run_delay)
 : CThreadNonStop(run_delay, run_delay), m_Coll(collector), m_Parent(parent)
{
}

CNSLB_Coordinator::CCollectorThread::~CCollectorThread()
{
    if (m_SrvList) {
        m_Parent.PutSrvList(m_SrvList);
    }
}

INSLB_Collector::TServerList* 
CNSLB_Coordinator::CCollectorThread::GetSrvList()
{
    CFastMutexGuard guard(m_SrvLstLock);

    if (m_SrvList == 0) return 0;
    if (m_SrvList->size() == 0) return 0;

    INSLB_Collector::TServerList* tmp = m_SrvList;
    m_SrvList = 0;
    return tmp;
}

void CNSLB_Coordinator::CCollectorThread::DoJob(void)
{
    CFastMutexGuard guard(m_SrvLstLock);
    if (m_SrvList == 0) {
        m_SrvList = m_Parent.GetSrvList();
    }
    if (m_SrvList == 0) {
        return;
    }

    m_SrvList->resize(0);
    m_Coll.GetServerList(m_Parent.GetServiceName(), m_SrvList);
}



CNSLB_LBSMD_Collector::CNSLB_LBSMD_Collector()
: m_Hosts(bm::BM_GAP)
{
}

void 
CNSLB_LBSMD_Collector::GetServerList(const string& service_name, 
                                     TServerList*  slist) const
{
    _ASSERT(slist);

    m_Hosts.clear();

    SConnNetInfo* net_info = ConnNetInfo_Create(service_name.c_str());
    TSERV_Type stype = fSERV_Any;
    SERV_ITER srv_it = SERV_Open(service_name.c_str(), stype, 0, net_info);
    ConnNetInfo_Destroy(net_info);
    if (srv_it == 0) {
        return;
    }
    const SSERV_Info* sinfo;

    while ((sinfo = SERV_GetNextInfoEx(srv_it, 0)) != 0) {
        if (!m_Hosts[sinfo->host]) {
            m_Hosts.set(sinfo->host);
            slist->push_back(NSLB_ServerInfo(sinfo->host, sinfo->rate));
        }

    } // while

    SERV_Close(srv_it);
}


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
