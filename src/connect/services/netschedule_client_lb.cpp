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
#include <corelib/plugin_manager_impl.hpp>
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
  m_LB_ServiceDiscovery(true),
  m_ConnFailPenalty(5 * 60),
  m_MaxRetry(3),
  m_DiscoverLowPriorityServers(false)
{
    if (lb_service_name.empty()) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Missing service name for load balancer.");
    }
    SetClientNameComment(lb_service_name);
}

CNetScheduleClient_LB::~CNetScheduleClient_LB()
{
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
        ObtainServerList(m_LB_ServiceName);

        m_ServListCurr = 0;
        ITERATE(TServiceList, it, m_ServList) {
            const SServiceAddress& sa = *it;
            EIO_Status st = Connect(sa.host, sa.port);
            if (st == eIO_Success) {
                break;
            }
            ++m_ServListCurr;
        } // ITERATE

        if (m_Sock && (eIO_Success == m_Sock->GetStatus(eIO_Open))) {
            return; // we are connected
        } 

        string warn = "Failed to discover the service ";
        warn += m_LB_ServiceName;
        warn += ". Trying fallback(reserve) server.";
        ERR_POST(Warning << warn);

        CreateSocket("netschedule", 9051);
        if (m_Sock && (eIO_Success == m_Sock->GetStatus(eIO_Open))) {
            return; // we are connected
        } 

        NCBI_THROW(CNetServiceException,
                   eCommunicationError,
                   "Cannot connect to netschedule service " + m_LB_ServiceName);
    }


    _ASSERT(key.empty());
    _ASSERT(m_ServListCurr < m_ServList.size());

    // check if service list candidate changed, so we try
    // another server

    for (;m_ServListCurr < m_ServList.size(); ++m_ServListCurr) {
        const SServiceAddress& sa = m_ServList[m_ServListCurr];
        string sa_host = CSocketAPI::gethostbyaddr(sa.host);
        if (sa.port != m_Port || 
            sa_host != m_Host) {
            EIO_Status st = Connect(sa.host, sa.port);
            if (st == eIO_Success) {
                if (m_Sock && (eIO_Success == m_Sock->GetStatus(eIO_Open))) {
                    return; // we are connected
                } 
            }
        } else {
            break;
        }
    } // while

    TParent::CheckConnect(key);
}

void CNetScheduleClient_LB::ObtainServerList(const string& service_name)
{
    _ASSERT(!service_name.empty());

    m_ServListCurr = 0;

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


    SConnNetInfo* net_info = ConnNetInfo_Create(service_name.c_str());
    TSERV_Type stype = fSERV_Any;
    if (m_DiscoverLowPriorityServers) {
        stype |= fSERV_Promiscuous;
    }
    SERV_ITER srv_it = SERV_Open(service_name.c_str(), stype, 0, net_info);
    ConnNetInfo_Destroy(net_info);



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


string CNetScheduleClient_LB::SubmitJob(const string& input,
                                        const string& progress_msg)
{
    ++m_Requests;
    // try to submit this job even if server does not take it
    unsigned max_retry = m_MaxRetry ? m_MaxRetry : 1;
    for (unsigned retry = 0; retry < max_retry; ++retry) {
        try {
            return TParent::SubmitJob(input, progress_msg);
        } 
        catch (CNetScheduleException& ex) {
            CNetScheduleException::TErrCode ec = ex.GetErrCode();
            if (ec == CNetScheduleException::eUnknownQueue || 
                ec == CNetScheduleException::eTooManyPendingJobs) {

                // try another server
                ++m_ServListCurr;
                if (m_ServListCurr < m_ServList.size()) {
                    ERR_POST(ex.what());
                    continue;
                }
            }

            throw;
        }
        break;
    } // for
    return kEmptyStr;
}

bool CNetScheduleClient_LB::GetJob(string* job_key, 
                                   string* input, 
                                   unsigned short udp_port)
{
    time_t curr = time(0);
    if (NeedRebalance(curr)) {
        ObtainServerList(m_LB_ServiceName);
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
    unsigned exp_count = 0;

    unsigned left_right = pivot & 1;

    for (unsigned k = 0; k < 2; ++k, left_right ^= 1) {
        if (left_right) {
            for (int i = (int)pivot; i >= 0; --i) {
                SServiceAddress& sa = m_ServList[i];
                // if service connection failed before we do not make
                // another attempts to connect for some time
                // (service may recover)
                if (sa.conn_fail_time) {
                    if (sa.conn_fail_time + m_ConnFailPenalty > (unsigned)curr) {
                        continue;
                    }
                }
                try {
                    bool job_received = 
                        x_TryGetJob(sa, job_key, input, udp_port);
                    if (job_received) {
                        return job_received;
                    }
                } catch (CNetScheduleException& ex) {
                    CNetScheduleException::TErrCode ec = ex.GetErrCode();
                    if (ec != CNetScheduleException::eUnknownQueue) {
                        throw;
                    } else {
                        if (++exp_count < m_MaxRetry) {
                            ERR_POST(ex.what());
                        } else {
                            throw;
                        }
                    }
                }
            }
        } else {
            for (unsigned i = pivot + 1; i < serv_size; ++i) {
                SServiceAddress& sa = m_ServList[i];
                if (sa.conn_fail_time) {
                    if (sa.conn_fail_time + m_ConnFailPenalty > (unsigned)curr) {
                        continue;
                    }
                }
                try {
                    bool job_received = 
                        x_TryGetJob(sa, job_key, input, udp_port);
                    if (job_received) {
                        return job_received;
                    }
                } catch (CNetScheduleException& ex) {
                    CNetScheduleException::TErrCode ec = ex.GetErrCode();
                    if (ec != CNetScheduleException::eUnknownQueue) {
                        throw;
                    } else {
                        if (++exp_count < m_MaxRetry) {
                            ERR_POST(ex.what());
                        } else {
                            throw;
                        }
                    }
                }
            }
        }
    } // for k

    return false;
}

bool CNetScheduleClient_LB::x_TryGetJob(SServiceAddress& sa,
                                        string* job_key, 
                                        string* input, 
                                        unsigned short udp_port)
{
    EIO_Status st = Connect(sa.host, sa.port);
    if (st != eIO_Success) {
        sa.conn_fail_time = time(0);
        return false;
    } else {
        sa.conn_fail_time = 0;
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
        ObtainServerList(m_LB_ServiceName);
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

    unsigned exp_count = 0;

    unsigned left_right = pivot & 1;

    for (unsigned k = 0; k < 2; ++k, left_right ^= 1) {
        if (left_right) {
            for (int i = (int)pivot; i >= 0; --i) {
                SServiceAddress& sa = m_ServList[i];
                if (sa.conn_fail_time) {
                    if (sa.conn_fail_time + m_ConnFailPenalty > (unsigned)curr) {
                        continue;
                    }
                }
                try {
                    bool job_received = 
                        x_GetJobWaitNotify(sa, 
                            job_key, input, notification_time, udp_port);
                    if (job_received) {
                        return job_received;
                    }
                } catch (CNetScheduleException& ex) {
                    CNetScheduleException::TErrCode ec = ex.GetErrCode();
                    if (ec != CNetScheduleException::eUnknownQueue) {
                        throw;
                    } else {
                        if (++exp_count < m_MaxRetry) {
                            ERR_POST(ex.what());
                        } else {
                            throw;
                        }
                    }
                }
            }
        } else {
            for (unsigned i = pivot + 1; i < serv_size; ++i) {
                SServiceAddress& sa = m_ServList[i];
                if (sa.conn_fail_time) {
                    if (sa.conn_fail_time + m_ConnFailPenalty > (unsigned)curr) {
                        continue;
                    }
                }
                try {
                    bool job_received = 
                        x_GetJobWaitNotify(sa, 
                            job_key, input, notification_time, udp_port);
                    if (job_received) {
                        return job_received;
                    }
                } catch (CNetScheduleException& ex) {
                    CNetScheduleException::TErrCode ec = ex.GetErrCode();
                    if (ec != CNetScheduleException::eUnknownQueue) {
                        throw;
                    } else {
                        if (++exp_count < m_MaxRetry) {
                            ERR_POST(ex.what());
                        } else {
                            throw;
                        }
                    }
                }

            }
        }
    } // for k

    WaitQueueNotification(wait_time, udp_port);

    return GetJob(job_key, input, udp_port);
}

bool CNetScheduleClient_LB::x_GetJobWaitNotify(SServiceAddress& sa,
                                               string*    job_key, 
                                               string*    input, 
                                               unsigned   wait_time,
                                               unsigned short udp_port)
{
    EIO_Status st = Connect(sa.host, sa.port);
    if (st != eIO_Success) {
        sa.conn_fail_time = time(0);
        return false;
    } else {
        sa.conn_fail_time = 0;
    }

    CJS_BoolGuard bg(&m_StickToHost);
    bool job_received = 
        TParent::GetJobWaitNotify(job_key, input, wait_time, udp_port);
    return job_received;
}




///////////////////////////////////////////////////////////////////////////////

const char* kNetScheduleDriverName = "netschedule_client";

/// @internal
class CNetScheduleClientCF : public IClassFactory<CNetScheduleClient>
{
public:

    typedef CNetScheduleClient                 TDriver;
    typedef CNetScheduleClient                 IFace;
    typedef IFace                              TInterface;
    typedef IClassFactory<CNetScheduleClient>  TParent;
    typedef TParent::SDriverInfo               TDriverInfo;
    typedef TParent::TDriverList               TDriverList;

    /// Construction
    ///
    /// @param driver_name
    ///   Driver name string
    /// @param patch_level
    ///   Patch level implemented by the driver.
    ///   By default corresponds to interface patch level.
    CNetScheduleClientCF(const string& driver_name = kNetScheduleDriverName,
                         int patch_level = -1)
        : m_DriverVersionInfo
        (ncbi::CInterfaceVersion<IFace>::eMajor,
         ncbi::CInterfaceVersion<IFace>::eMinor,
         patch_level >= 0 ?
            patch_level : ncbi::CInterfaceVersion<IFace>::ePatchLevel),
          m_DriverName(driver_name)
    {
        _ASSERT(!m_DriverName.empty());
    }

    /// Create instance of TDriver
    virtual TInterface*
    CreateInstance(const string& driver  = kEmptyStr,
                   CVersionInfo version = NCBI_INTERFACE_VERSION(IFace),
                   const TPluginManagerParamTree* params = 0) const
    {
        auto_ptr<TDriver> drv;
        if (params && (driver.empty() || driver == m_DriverName)) {
            if (version.Match(NCBI_INTERFACE_VERSION(IFace))
                                != CVersionInfo::eNonCompatible) {

            CConfig conf(params);
            string client_name = 
                conf.GetString(m_DriverName, 
                               "client_name", CConfig::eErr_Throw, "noname");
            string queue_name = 
                conf.GetString(m_DriverName, 
                               "queue_name", CConfig::eErr_Throw, "noname");
            string service = 
                conf.GetString(m_DriverName, 
                               "service", CConfig::eErr_NoThrow, "");
            if (!service.empty()) {
                unsigned int rebalance_time = conf.GetInt(m_DriverName, 
                                                "rebalance_time",
                                                CConfig::eErr_NoThrow, 10);
                unsigned int rebalance_requests = conf.GetInt(m_DriverName,
                                                "rebalance_requests",
                                                CConfig::eErr_NoThrow, 100);
                CNetScheduleClient_LB* lb_drv =
                      new CNetScheduleClient_LB(client_name, 
                                                service, queue_name,
                                                rebalance_time, 
                                                rebalance_requests);
                drv.reset(lb_drv);
                unsigned max_retry = conf.GetInt(m_DriverName, 
                                                 "max_retry",
                                                 CConfig::eErr_NoThrow, 0);
                if (max_retry) {
                    lb_drv->SetMaxRetry(max_retry);
                }

                bool discover_lp_servers =
                    conf.GetBool(m_DriverName, "discover_low_priority_servers", 
                                CConfig::eErr_NoThrow, false);

                lb_drv->DiscoverLowPriorityServers(discover_lp_servers);

                string services_list = conf.GetString(m_DriverName,
                                                "sevices_list",
                                                 CConfig::eErr_NoThrow, kEmptyStr);
                vector<string> services;
                NStr::Tokenize(services_list, ",;", services);
                for(vector<string>::const_iterator it = services.begin();
                                                   it != services.end(); ++it) {
                    string host, sport;
                    if (NStr::SplitInTwo(*it,":",host,sport)) {
                        try {
                            unsigned int port = NStr::StringToUInt(sport);
                            lb_drv->AddServiceAddress(host, port);
                        } catch(CStringException&) {}
                    }
                }
            } else { // non lb client
                string host = 
                    conf.GetString(m_DriverName, 
                                  "host", CConfig::eErr_Throw, kEmptyStr);
                unsigned int port = conf.GetInt(m_DriverName,
                                               "port",
                                               CConfig::eErr_Throw, 9100);

                drv.reset(new CNetScheduleClient(host, port, 
                                             client_name, queue_name));
            }
            }                               
        }
        return drv.release();
    }

    void GetDriverVersions(TDriverList& info_list) const
    {
        info_list.push_back(TDriverInfo(m_DriverName, m_DriverVersionInfo));
    }
protected:
    CVersionInfo  m_DriverVersionInfo;
    string        m_DriverName;

};

void NCBI_XCONNECT_EXPORT NCBI_EntryPoint_xnetschedule(
     CPluginManager<CNetScheduleClient>::TDriverInfoList&   info_list,
     CPluginManager<CNetScheduleClient>::EEntryPointRequest method)
{
       CHostEntryPointImpl<CNetScheduleClientCF>::
       NCBI_EntryPointImpl(info_list, method);

}



END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.13  2005/05/11 18:56:06  didenko
 * Added calling DescoverLowPriortyServers method when a worker node is created
 *
 * Revision 1.12  2005/05/10 17:41:27  kuznets
 * Added option to discover low priority services
 *
 * Revision 1.11  2005/05/10 14:13:52  kuznets
 * Added fallback server if LB connection is not available
 *
 * Revision 1.10  2005/05/06 15:13:55  didenko
 * Fixed possible uninitialized variable bugs
 *
 * Revision 1.9  2005/04/20 15:42:29  kuznets
 * Added progress message to SubmitJob()
 *
 * Revision 1.8  2005/04/13 13:37:54  didenko
 * Changed NetSchedule PluginManager driver name to netschedule_client
 *
 * Revision 1.7  2005/04/11 13:50:45  kuznets
 * Implemented retries when queue cannot take job
 *
 * Revision 1.6  2005/04/06 12:38:13  kuznets
 * LB class factory moved from ns_client.cpp
 *
 * Revision 1.5  2005/04/01 15:16:52  kuznets
 * Added penalty to unavailable service
 *
 * Revision 1.4  2005/03/28 15:32:27  didenko
 * Made destructors virtual
 *
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
