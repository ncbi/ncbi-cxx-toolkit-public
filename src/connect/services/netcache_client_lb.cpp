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
 *   Implementation of net cache client with load balancer interface.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <connect/services/netcache_client.hpp>
#include <connect/ncbi_service.h>

#include <memory>

BEGIN_NCBI_SCOPE

/// @internal
static
bool s_ConnectClient(CNetCacheClient* nc_client, 
                     unsigned int     host,
                     unsigned short   port,
                     STimeout&        to)
{
    auto_ptr<CSocket> sock(new CSocket(host, port, &to));
    EIO_Status st = sock->GetStatus(eIO_Open);
    if (st != eIO_Success) {
        return false;
    }

    nc_client->SetSocket(sock.release(), eTakeOwnership);
    return true;
}

/// @internal
static
bool s_ConnectClient(CNetCacheClient* nc_client, 
                     const string&    host,
                     unsigned short   port,
                     STimeout&        to)
{
    auto_ptr<CSocket> sock(new CSocket(host, port, &to));
    EIO_Status st = sock->GetStatus(eIO_Open);
    if (st != eIO_Success) {
        return false;
    }

    nc_client->SetSocket(sock.release(), eTakeOwnership);
    return true;
}

/// @internal
static
bool s_ConnectClient_Reserve(CNetCacheClient* nc_client, 
                             const string&    host,
                             unsigned short   port,
                             const string&    service_name,
                             STimeout&        to,
                             string*          err_msg)
{
    if (s_ConnectClient(nc_client, host, port, to)) {
        LOG_POST(Warning << "Service " << service_name 
                         << " cannot be found using load balancer"
                         << " reserve service used " 
                         << host << ":" << port);
        return false;
    } 
    *err_msg += ". Reserve netcache instance at ";
    *err_msg += host;
    *err_msg += ":";
    *err_msg += NStr::UIntToString(port);
    *err_msg += " not responding.";
    return true;
}



void NetCache_ConfigureWithLB(CNetCacheClient* nc_client, 
                              const string&    service_name)
{
    SERV_ITER srv_it = SERV_OpenSimple(service_name.c_str());
    STimeout& to = nc_client->SetCommunicationTimeout();
    string err_msg = "Cannot connect to netcache service (";

    if (srv_it == 0) {
        err_msg += "Load balancer cannot find service name ";
        err_msg += service_name;
    } else {
        const SSERV_Info* sinfo;
        while ((sinfo = SERV_GetNextInfoEx(srv_it, 0)) != 0) {

            try {

                if ( !s_ConnectClient(nc_client, sinfo->host, sinfo->port, to)){
                    continue;
                }
                SERV_Close(srv_it);
                return;
                
            } catch(exception&){
                // try another server
            }

        } // while

        SERV_Close(srv_it);

        // failed to find any server responding under the service name

        err_msg += " Failed to find alive netcache server for .";
        err_msg += service_name;
    }

    // LB failed to provide any alive services
    // lets try to call "emergency numbers"

    const unsigned short rport = 9009;
    if (s_ConnectClient_Reserve(nc_client, 
                                "netcache.ncbi.nlm.nih.gov", 
                                rport, 
                                service_name,
                                to, 
                                &err_msg)) {
        return;
    }
    if (s_ConnectClient_Reserve(nc_client, 
                                "service1", 
                                rport, 
                                service_name,
                                to, 
                                &err_msg)) {
        return;
    }

    err_msg += ")";

    // cannot connect
    NCBI_THROW(CNetServiceException, eCommunicationError, err_msg);
}


/// @internal
///
class CNC_BoolGuard {
public:
    CNC_BoolGuard(bool* flag) : m_Flag(*flag) {m_Flag = true;}
    ~CNC_BoolGuard() { m_Flag = false; }
private:
    bool& m_Flag;
};


CNetCacheClient_LB::CNetCacheClient_LB(const string& client_name,
                                       const string& lb_service_name,
                                       unsigned int  rebalance_time,
                                       unsigned int  rebalance_requests,
                                       unsigned int  rebalance_bytes)
: CNetCacheClient(client_name),
  m_LB_ServiceName(lb_service_name),
  m_RebalanceTime(rebalance_time),
  m_RebalanceRequests(rebalance_requests),
  m_RebalanceBytes(rebalance_bytes),
  m_LastRebalanceTime(0),
  m_Requests(0),
  m_RWBytes(0),
  m_StickToHost(false)
{
    if (lb_service_name.empty()) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Missing service name for load balancer.");
    }
    SetClientNameComment(lb_service_name);
}

string CNetCacheClient_LB::PutData(const void*   buf,
                                   size_t        size,
                                   unsigned int  time_to_live)
{
    string k = TParent::PutData(buf, size, time_to_live);
    m_RWBytes += size;
    ++m_Requests;
    return k;
}

string CNetCacheClient_LB::PutData(const string& key,
                                   const void*   buf,
                                   size_t        size,
                                   unsigned int  time_to_live)
{
    CNetCache_Key blob_key;
    if (!key.empty()) {
        CNetCache_ParseBlobKey(&blob_key, key);
    
        if ((blob_key.hostname == m_Host) && 
            (blob_key.port == m_Port)) {

            CNC_BoolGuard bg(&m_StickToHost);
            string k = TParent::PutData(key, buf, size, time_to_live);
            m_RWBytes += size;
            ++m_Requests;
            return k;
        } else {
            // go to an alternative service
            CNetCacheClient cl(m_ClientName);
            return cl.PutData(key, buf, size, time_to_live);
        }
    }

    // key is empty
    string k = TParent::PutData(key, buf, size, time_to_live);
    m_RWBytes += size;
    ++m_Requests;
    return k;
}

IWriter* CNetCacheClient_LB::PutData(string* key, unsigned int time_to_live)
{
    CNetCache_Key blob_key;
    if (!key->empty()) {
        CNetCache_ParseBlobKey(&blob_key, *key);
    
        if ((blob_key.hostname == m_Host) && 
            (blob_key.port == m_Port)) {

            CNC_BoolGuard bg(&m_StickToHost);
            ++m_Requests;
            // TODO: size based rebalancing is an open question in this case
            //m_RWBytes += size;
            return TParent::PutData(key, time_to_live);
        } else {
            // go to an alternative service
            CNetCacheClient cl(m_ClientName);
            IWriter* wrt = cl.PutData(key, time_to_live);
            cl.DetachSocket();
            CNetCacheSock_RW* rw = dynamic_cast<CNetCacheSock_RW*>(wrt);
            if (rw) {
                rw->OwnSocket();
            } else {
                _ASSERT(0);
            }
            return wrt;
        }
    }

    // key is empty

    ++m_Requests;
    // TODO: size based rebalancing is an open question in this case
    //m_RWBytes += size;
    return TParent::PutData(key, time_to_live);
}


IReader* CNetCacheClient_LB::GetData(const string& key, 
                                     size_t*       blob_size)
{
    CNetCache_Key blob_key;
    if (!key.empty()) {
        CNetCache_ParseBlobKey(&blob_key, key);
    
        size_t bsize = 0;
        IReader* rdr;

        if ((blob_key.hostname == m_Host) && 
            (blob_key.port == m_Port)) {

            CNC_BoolGuard bg(&m_StickToHost);
            rdr = TParent::GetData(key, &bsize);
            m_RWBytes += bsize;
            ++m_Requests;

            if (blob_size) {
                *blob_size = bsize;
            }
            return rdr;
        }
    }

    CNetCacheClient cl(m_ClientName);
    IReader* rd = cl.GetData(key, blob_size);
    cl.DetachSocket();
    CNetCacheSock_RW* rw = dynamic_cast<CNetCacheSock_RW*>(rd);
    if (rw) {
        rw->OwnSocket();
    } else {
        _ASSERT(0);
    }
    return rd;
}

void CNetCacheClient_LB::Remove(const string& key)
{
    CNetCache_Key blob_key;
    if (!key.empty()) {
        CNetCache_ParseBlobKey(&blob_key, key);
        if ((blob_key.hostname == m_Host) && 
            (blob_key.port == m_Port)) {

            CNC_BoolGuard bg(&m_StickToHost);
            TParent::Remove(key);
            ++m_Requests;
            return;
        }
    }
    CNetCacheClient cl(m_ClientName);
    cl.Remove(key);
}

CNetCacheClient::EReadResult 
CNetCacheClient_LB::GetData(const string&  key,
                    void*          buf, 
                    size_t         buf_size, 
                    size_t*        n_read,
                    size_t*        blob_size)
{
    CNetCache_Key blob_key;
    if (!key.empty()) {
        CNetCache_ParseBlobKey(&blob_key, key);

        if ((blob_key.hostname == m_Host) && 
            (blob_key.port == m_Port)) {

            CNC_BoolGuard bg(&m_StickToHost);
            size_t bsize = 0;
            CNetCacheClient::EReadResult r =
                TParent::GetData(key, buf, buf_size, n_read, &bsize);
            if (blob_size)
                *blob_size = bsize;
            ++m_Requests;
            m_RWBytes += bsize;
            return r;
        }
    }
    CNetCacheClient cl(m_ClientName);
    return cl.GetData(key, buf, buf_size, n_read, blob_size);
}

void CNetCacheClient_LB::CheckConnect(const string& key)
{
    if (m_StickToHost) { // restore connection to the specified host
        TParent::CheckConnect(key);
        return;
    }

    if (m_Sock && (eIO_Success == m_Sock->GetStatus(eIO_Open))) {
        return; // we are connected, nothing to do
    } 

    // in this implimentaion Get requests should be intercepted 
    // on the upper level
    _ASSERT(key.empty()); 

    time_t curr = time(0);

    if ((m_LastRebalanceTime == 0) ||
        (m_RebalanceTime && 
          (int(curr - m_LastRebalanceTime) >= int(m_RebalanceTime)))||
        (m_RebalanceRequests && (m_Requests >= m_RebalanceRequests)) ||
        (m_RebalanceBytes && (m_RWBytes >= m_RebalanceBytes))
        ) {
        m_LastRebalanceTime = curr;
        m_Requests = m_RWBytes = 0;
        NetCache_ConfigureWithLB(this, m_LB_ServiceName);
        return;
    }

    TParent::CheckConnect(key);
}

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.10  2005/03/22 18:54:07  kuznets
 * Changed project tree layout
 *
 * Revision 1.9  2005/02/07 16:54:59  kuznets
 * Improved diagnostics in LB connect
 *
 * Revision 1.8  2005/02/07 16:20:20  ucko
 * NetCache_ConfigureWithLB: Fix build error introduced in previous
 * revision by moving the declaration of "STimeout& to" further up.
 *
 * Revision 1.7  2005/02/07 15:16:26  kuznets
 * Added service1:9009 as an emergency instance when LB is down
 *
 * Revision 1.6  2005/02/07 13:01:48  kuznets
 * Part of functionality moved to netservice_client.hpp(cpp)
 *
 * Revision 1.5  2005/01/28 14:47:14  kuznets
 * Fixed connection bug
 *
 * Revision 1.4  2005/01/19 12:21:58  kuznets
 * +CNetCacheClient_LB
 *
 * Revision 1.3  2004/12/20 17:29:27  ucko
 * +<memory> (once indirectly included?) for auto_ptr<>
 *
 * Revision 1.2  2004/12/20 13:48:56  kuznets
 * Fixed compilation problem (GCC)
 *
 * Revision 1.1  2004/12/17 15:26:48  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
