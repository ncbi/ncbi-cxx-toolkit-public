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

CNSLB_Coordinator::CNSLB_Coordinator(const string&           service_name,
                                     INSLB_Collector*        collector, 
                                     CNSLB_ThreasholdCurve*  curve,
                                     CNSLB_DecisionModule*   decision_maker,
                                     unsigned                collect_tm,
                                     ENonLbHostsPolicy       non_lb_host)
 :  m_ServiceName(service_name),
    m_Collector(collector),
    m_CollectTm(collect_tm),
    m_CurrSrvList(0),
    m_Curve(curve),
    m_DecisionMaker(decision_maker),
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
    delete m_Curve;
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

CNSLB_DecisionModule::EDecision 
CNSLB_Coordinator::Evaluate(unsigned host,
                            unsigned time_eval)
{
    // get the updated list from the collector thread, if new list
    // is not available 
    INSLB_Collector::TServerList* slist = m_CollectorThread->GetSrvList();
    if (slist != 0) {
        CWriteLockGuard guard(m_CurrSrvCurveLock);
        if (m_CurrSrvList) {
            PutSrvList(m_CurrSrvList);
        }
        m_CurrSrvList = slist;
    }

    if (m_DecisionMaker == 0) {
        return CNSLB_DecisionModule::eGrantJob;
    }

    CReadLockGuard guard(m_CurrSrvCurveLock);
    if (m_CurrSrvList == 0 || m_CurrSrvList->size() == 0) {
        return CNSLB_DecisionModule::eNoLBInfo;
    }

    
    CNSLB_DecisionModule::SPetition petition;
    petition.host = host;
    petition.time_eval = time_eval;
    petition.lb_host_info = m_CurrSrvList;
    petition.threshold_curve = m_Curve ? &m_Curve->GetCurve() : 0;

    return m_DecisionMaker->Evaluate(petition);
}

void CNSLB_Coordinator::ReGenerateCurve(unsigned length)
{
    _ASSERT(length);

    if (!length || !m_Curve)
        return;

    CWriteLockGuard guard(m_CurrSrvCurveLock);

    m_Curve->ReGenerateCurve(length);
}


CNSLB_Coordinator::CCollectorThread::CCollectorThread(
                         INSLB_Collector&     collector,
                         CNSLB_Coordinator&   parent,
                         unsigned             run_delay)
 : CThreadNonStop(run_delay), m_Coll(collector), m_Parent(parent)
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
    HOST_INFO hinfo;

    while ((sinfo = SERV_GetNextInfoEx(srv_it, &hinfo)) != 0) {

        if (!m_Hosts[sinfo->host]) {
            m_Hosts.set(sinfo->host);
            if (hinfo) {
                unsigned cpu_count = HINFO_CpuCount(hinfo);
                if (cpu_count != 0) {
                }
     
                double array[2];  // 0 load average - is number of waiting processes
                                  // 1 - momentary run queue 
                                  // if < than CPU_NUMBER - machine is available
                                  // (non integer!)
                if (HINFO_LoadAverage(hinfo, array)) {
                }
                free(hinfo);

                slist->push_back(
                    NSLB_ServerInfo(sinfo->host, sinfo->rate,
                                    cpu_count, array[0], array[1]));
            } else {
                slist->push_back(NSLB_ServerInfo(sinfo->host, sinfo->rate));
            }
        }

    } // while

    SERV_Close(srv_it);
}


CNSLB_ThreasholdCurve_Linear::CNSLB_ThreasholdCurve_Linear(double a, 
                                                           double b,
                                                           bool   derive_a)
: m_Constr(0),
  m_A(a),
  m_B(b),
  m_DeriveA(derive_a)
{
}

CNSLB_ThreasholdCurve_Linear::CNSLB_ThreasholdCurve_Linear(double  y0, 
                                                           double  yN)
 : m_Constr(1),
   m_Y0(y0),
   m_YN(yN)
{
}


void CNSLB_ThreasholdCurve_Linear::ReGenerateCurve(unsigned length)
{
    _ASSERT(length);

    if (length == 0) {
        return;
    }

    switch (m_Constr) {
    case 0:
        if (m_DeriveA) {
            // assume the last threshold is 0
            //  y = ax + b
            //  y = 0
            //  a = -(b/length)
            m_A = -(m_B / length);
        }
        break;
    case 1:
        m_B = m_Y0;
        m_A = (m_YN - m_B) / length;
        break;
    default:
        _ASSERT(0);
    } // switch

    // compute the curve vector
    m_Curve.resize(length);
    for (unsigned x = 0; x < length; ++x) {
        double y = m_A * double(x) + m_B;
        if (y > 0.99) {
            y = 1;
        }
        if (y < 0.01) {
            y = 0.0;
        }
        m_Curve[x] = y;
    } // for

}

CNSLB_ThreasholdCurve_Regression::CNSLB_ThreasholdCurve_Regression(
                                                                  double  y0,
                                                                  double  a)
 : m_Y0(y0),
   m_A(a)
{
}

void CNSLB_ThreasholdCurve_Regression::ReGenerateCurve(unsigned length)
{
    _ASSERT(length);

    if (length == 0) {
        return;
    }
    m_Curve.resize(length);
    m_Curve[0] = m_Y0;

    for (unsigned x = 1; x < length; ++x) {
        double y = m_Curve[x-1] + (m_A * m_Curve[x-1]);
        if (y > 0.99) {
            y = 1;
        }
        if (y < 0.01) {
            y = 0.0;
        }
        m_Curve[x] = y;
    }
}


string CNSLB_DecisionModule::DecisionToStrint(EDecision decision)
{
    switch (decision) {
    case eGrantJob:         return "GrantJob";
    case eDenyJob:          return "DenyJob";
    case eHostUnknown:      return "HostUnknown";
    case eNoLBInfo:         return "NoLBInfo";
    case eInsufficientInfo: return "eInsufficientInfo";
    default:
        _ASSERT(0);
    }
    return "NoSuchCode";
}


CNSLB_DecisionModule::EDecision 
CNSLB_DecisionModule_DistributeRate::Evaluate(const SPetition& petition) const
{
    if (petition.lb_host_info == 0) {
        return eNoLBInfo;
    }

    INSLB_Collector::NSLB_ServerInfo best_host;
    INSLB_Collector::NSLB_ServerInfo worst_host;
    best_host.rate = 0.0;
    worst_host.rate = 99999999.999;

    const INSLB_Collector::NSLB_ServerInfo* req_host = 0;


    // Iterate the available load list, find fittest host, worst host

    ITERATE(INSLB_Collector::TServerList, it, *petition.lb_host_info) {
        if (it->rate > best_host.rate) {
            best_host = *it;
        }
        if (it->rate < worst_host.rate) {
            worst_host = *it;
        }
        if (it->host == petition.host) {
            req_host = &*it;
        }
    }
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
        // no difference between best-worst
        return eGrantJob;
    }
    double req_interval = best_host.rate - req_host->rate;
    double ratio = req_interval / rate_interval;

    double deny_threashold = 0.20;

    if (petition.threshold_curve) {
        if (petition.threshold_curve->size() < petition.time_eval) {
            return eGrantJob;
        }
        deny_threashold = (*petition.threshold_curve)[petition.time_eval];
        if (deny_threashold > 1) {
            deny_threashold = 0.99;
        }
    }

    if (ratio >= deny_threashold) {
        return eGrantJob;
    }
    return eDenyJob;
}


CNSLB_DecisionModule::EDecision 
CNSLB_DecisionModule_CPU_Avail::Evaluate(const SPetition& petition) const
{
    if (petition.lb_host_info == 0) {
        return eNoLBInfo;
    }
    const INSLB_Collector::NSLB_ServerInfo* req_host = 0;

    ITERATE(INSLB_Collector::TServerList, it, *petition.lb_host_info) {
        if (it->host == petition.host) {
            req_host = &*it;
            break;
        }
    }

    if (req_host == 0) {
        return eHostUnknown;
    }
    
    if (req_host->cpu_count == 0) {
        return eInsufficientInfo;
    }

    if (req_host->cpu_run_queue < double(req_host->cpu_count)) {
        return eGrantJob;
    }
    return eDenyJob;
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2006/10/03 14:56:57  joukovv
 * Delayed job deletion implemented, code restructured preparing to move to
 * thread-per-request model.
 *
 * Revision 1.5  2005/07/25 16:14:31  kuznets
 * Revisited LB parameters, added options to compute job stall delay as fraction of AVG runtime
 *
 * Revision 1.4  2005/07/21 15:41:02  kuznets
 * Added monitoring for LB info
 *
 * Revision 1.3  2005/07/21 12:39:27  kuznets
 * Improved load balancing module
 *
 * Revision 1.2  2005/07/14 13:40:07  kuznets
 * compilation bug fixes
 *
 * Revision 1.1  2005/07/14 13:12:56  kuznets
 * Added load balancer
 *
 * ===========================================================================
 */
