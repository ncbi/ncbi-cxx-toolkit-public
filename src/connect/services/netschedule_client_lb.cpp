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
#include <connect/netschedule_client.hpp>
#include <connect/ncbi_service.h>
#include <util/request_control.hpp>
#include <memory>


BEGIN_NCBI_SCOPE

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
  m_StickToHost(false)
{
    if (lb_service_name.empty()) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Missing service name for load balancer.");
    }
    SetClientNameComment(lb_service_name);
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

    if ((m_LastRebalanceTime == 0) ||
        (m_RebalanceTime && 
          (int(curr - m_LastRebalanceTime) >= int(m_RebalanceTime)))||
        (m_RebalanceRequests && (m_Requests >= m_RebalanceRequests)) 
        ) {
        m_LastRebalanceTime = curr;
        m_Requests = 0;
        x_GetServerList(m_LB_ServiceName);

        ITERATE(TServiceList, it, m_ServList) {
            auto_ptr<CSocket> sock(new CSocket(it->host, 
                                               it->port));
            EIO_Status st = sock->GetStatus(eIO_Open);
            if (st != eIO_Success) {
                NCBI_THROW(CNetServiceException, 
                        eCommunicationError, "Cannot connect to server");
            }

            SetSocket(sock.release(), eTakeOwnership);
            break;

        } // ITERATE

        return;
    }

    TParent::CheckConnect(key);

}

void CNetScheduleClient_LB::x_GetServerList(const string& service_name)
{
    _ASSERT(!service_name.empty());

    SERV_ITER srv_it = SERV_OpenSimple(service_name.c_str());
    string err_msg = "Cannot connect to netschedule service (";
    if (srv_it == 0) {
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
}



END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/03/07 17:31:05  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
