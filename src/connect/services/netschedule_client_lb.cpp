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
 * Author:  Anatoliy Kuznetsov
 *
 * File Description:
 *   Implementation of NetSchedule client integrated with NCBI load balancer.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbitime.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <connect/services/netschedule_client.hpp>
#include <connect/ncbi_service.h>
#include <util/request_control.hpp>

#include <stdlib.h>
#include <memory>


BEGIN_NCBI_SCOPE


/// @internal
///
class CJS_BoolGuard {
public:
    CJS_BoolGuard(bool* flag) : m_Flag(*flag) {m_Flag = true;}
    ~CJS_BoolGuard() { m_Flag = false; }
private:
    bool& m_Flag;
};


CNetScheduleClient_LB::CNetScheduleClient_LB(const string& client_name,
                                             const string& lb_service_name,
                                             const string& queue_name,
                                             unsigned int  rebalance_time,
                                             unsigned int  rebalance_requests)
: CNetScheduleClient(client_name, queue_name),
  m_LB_ServiceName(lb_service_name),
  m_RebalanceTime(rebalance_time),
  m_RebalanceRequests(rebalance_requests),
  m_LastRebalanceTime(0),
  m_Requests(0),
  m_StickToHost(false),
  m_LB_ServiceDiscovery(true)
{
    if (lb_service_name.empty()) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Missing service name for load balancer.");
    }
    SetClientNameComment(lb_service_name);
}

bool CNetScheduleClient_LB::NeedRebalance(time_t curr) const
{
    if ((m_LastRebalanceTime == 0) || (m_ServList.size() == 0) ||
        (m_RebalanceTime && 
          (int(curr - m_LastRebalanceTime) >= int(m_RebalanceTime)))||
        (m_RebalanceRequests && (m_Requests >= m_RebalanceRequests)) 
        ) {
        return true;
    }
    return false;
}

void CNetScheduleClient_LB::CheckConnect(const string& key)
{
    if (m_StickToHost || !key.empty()) { // restore connection to the specified host
        TParent::CheckConnect(key);
        return;
    }
    if (m_Sock && (eIO_Success == m_Sock->GetStatus(eIO_Open))) {
        return; // we are connected, nothing to do
    } 

    time_t curr = time(0);

    if (NeedRebalance(curr)){

        m_LastRebalanceTime = curr;
        m_Requests = 0;
        x_GetServerList(m_LB_ServiceName);

        ITERATE(TServiceList, it, m_ServList) {
            EIO_Status st = Connect(it->host, it->port);
            if (st == eIO_Success) {
                break;
            }
        } // ITERATE

        if (m_Sock && (eIO_Success == m_Sock->GetStatus(eIO_Open))) {
            return; // we are connected
        } 

        NCBI_THROW(CNetServiceException,
                eCommunicationError,
                "Cannot connect to netschedule service " + m_LB_ServiceName);
    }

    TParent::CheckConnect(key);
}

void CNetScheduleClient_LB::x_GetServerList(const string& service_name)
{
    _ASSERT(!service_name.empty());

    if (!m_LB_ServiceDiscovery) {
        if (m_ServList.size() == 0) {
            NCBI_THROW(CNetServiceException, eCommunicationError, 
                       "Incorrect or empty service address list");
        }
        // shuffle server list to imitate load-balancing
        if (m_ServList.size() > 1) {
            unsigned first = rand() % m_ServList.size();
            if (first >= m_ServList.size()) {
                first = 0;
            }
            unsigned second = rand() % m_ServList.size();
            if (second >= m_ServList.size()) {
                second = m_ServList.size()-1;
            }
            if (first == second) {
                return;
            }
            
            swap(m_ServList[first], m_ServList[second]);
        }
        return;
    }

    m_ServList.resize(0);

    SERV_ITER srv_it = SERV_OpenSimple(service_name.c_str());
    string err_msg = "Cannot connect to netschedule service (";
    if (srv_it == 0) {
err_service_not_found:
        err_msg += "Load balancer cannot find service name ";
        err_msg += service_name;
        NCBI_THROW(CNetServiceException, eCommunicationError, err_msg);
    } else {
        const SSERV_Info* sinfo;
        m_ServList.resize(0);
        while ((sinfo = SERV_GetNextInfoEx(srv_it, 0)) != 0) {

            m_ServList.push_back(SServiceAddress(sinfo->host, sinfo->port));

        } // while

        SERV_Close(srv_it);
    }
    if (m_ServList.size() == 0) {
        goto err_service_not_found;
    }
}

void CNetScheduleClient_LB::AddServiceAddress(const string&  hostname,
                                              unsigned short port)
{
    m_LB_ServiceDiscovery = false;
    unsigned addr = CSocketAPI::gethostbyname(hostname);
    if (!addr) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Incorrect host name: " + hostname);
    }

    ITERATE(TServiceList, it, m_ServList) {
        if (it->host == addr && it->port == port) {
            return; // already registered
        }
    }
    m_ServList.push_back(SServiceAddress(addr, port));
}


string CNetScheduleClient_LB::SubmitJob(const string& input)
{
    ++m_Requests;
    return TParent::SubmitJob(input);
}

bool CNetScheduleClient_LB::GetJob(string* job_key, 
                                   string* input, 
                                   unsigned short udp_port)
{
    time_t curr = time(0);
    if (NeedRebalance(curr)) {
        x_GetServerList(m_LB_ServiceName);
        m_Requests = 0;
        m_LastRebalanceTime = curr;
    }

    ++m_Requests;

    unsigned serv_size = m_ServList.size();

    // pick a random pivot element, so we do not always
    // fetch jobs using the same lookup order and some servers do 
    // not get equally "milked"
    // also get random list lookup direction

    unsigned pivot = rand() % serv_size;

    if (pivot >= serv_size) {
        pivot = serv_size - 1;
    }

    unsigned left_right = pivot & 1;

    for (unsigned k = 0; k < 2; ++k, left_right ^= 1) {
        if (left_right) {
            for (int i = (int)pivot; i >= 0; --i) {
                const SServiceAddress& sa = m_ServList[i];
                bool job_received = 
                    x_TryGetJob(sa, job_key, input, udp_port);
                if (job_received) {
                    return job_received;
                }
            }
        } else {
            for (unsigned i = pivot + 1; i < serv_size; ++i) {
                const SServiceAddress& sa = m_ServList[i];
                bool job_received = 
                    x_TryGetJob(sa, job_key, input, udp_port);
                if (job_received) {
                    return job_received;
                }
            }
        }
    } // for k

    return false;
}

bool CNetScheduleClient_LB::x_TryGetJob(const SServiceAddress& sa,
                                        string* job_key, 
                                        string* input, 
                                        unsigned short udp_port)
{
    EIO_Status st = Connect(sa.host, sa.port);
    if (st != eIO_Success) {
        return false;
    }

    CJS_BoolGuard bg(&m_StickToHost);
    bool job_received = TParent::GetJob(job_key, input, udp_port);
    return job_received;
}

bool CNetScheduleClient_LB::WaitJob(string*        job_key, 
                                    string*        input, 
                                    unsigned       wait_time,
                                    unsigned short udp_port)
{
    time_t curr = time(0);
    if (NeedRebalance(curr)) {
        x_GetServerList(m_LB_ServiceName);
        m_Requests = 0;
        m_LastRebalanceTime = curr;
    }

    ++m_Requests;

    unsigned serv_size = m_ServList.size();

    // waiting time increased by the number of watched instances
    unsigned notification_time = wait_time + serv_size; 

    unsigned pivot = rand() % serv_size;

    if (pivot >= serv_size) {
        pivot = serv_size - 1;
    }

    unsigned left_right = pivot & 1;

    for (unsigned k = 0; k < 2; ++k, left_right ^= 1) {
        if (left_right) {
            for (int i = (int)pivot; i >= 0; --i) {
                const SServiceAddress& sa = m_ServList[i];
                bool job_received = 
                    x_GetJobWaitNotify(sa, 
                        job_key, input, notification_time, udp_port);
                if (job_received) {
                    return job_received;
                }
            }
        } else {
            for (unsigned i = pivot + 1; i < serv_size; ++i) {
                const SServiceAddress& sa = m_ServList[i];
                bool job_received = 
                    x_GetJobWaitNotify(sa, 
                        job_key, input, notification_time, udp_port);
                if (job_received) {
                    return job_received;
                }
            }
        }
    } // for k

    WaitQueueNotification(wait_time, udp_port);

    return GetJob(job_key, input, udp_port);
}

bool CNetScheduleClient_LB::x_GetJobWaitNotify(const SServiceAddress& sa,
                                               string*    job_key, 
                                               string*    input, 
                                               unsigned   wait_time,
                                               unsigned short udp_port)
{
    EIO_Status st = Connect(sa.host, sa.port);
    if (st != eIO_Success) {
        return false;
    }

    CJS_BoolGuard bg(&m_StickToHost);
    bool job_received = 
        TParent::GetJobWaitNotify(job_key, input, wait_time, udp_port);
    return job_received;
}



END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2005/03/22 18:54:07  kuznets
 * Changed project tree layout
 *
 * Revision 1.2  2005/03/17 17:18:45  kuznets
 * Implemented load-balanced client
 *
 * Revision 1.1  2005/03/07 17:31:05  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
