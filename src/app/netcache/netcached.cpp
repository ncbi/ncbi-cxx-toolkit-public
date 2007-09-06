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
 * File Description: Network cache daemon
 *
 */
#include <ncbi_pch.hpp>
#include <stdio.h>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/plugin_manager.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_config.hpp>
#include <corelib/ncbimtx.hpp>

#include <util/thread_nonstop.hpp>
#include <util/transmissionrw.hpp>

#include <bdb/bdb_blobcache.hpp>

#if defined(NCBI_OS_UNIX)
# include <corelib/ncbi_os_unix.hpp>
# include <signal.h>
#endif

#include "netcached.hpp"

#define NETCACHED_VERSION \
      "NCBI NetCache server version=3.0.0  " __DATE__ " " __TIME__


USING_NCBI_SCOPE;

//const unsigned int kNetCacheBufSize = 64 * 1024;
const unsigned int kObjectTimeout = 60 * 60; ///< Default timeout in seconds

/// General purpose NetCache Mutex
DEFINE_STATIC_FAST_MUTEX(x_NetCacheMutex);

/// Mutex to guard vector of busy IDs 
DEFINE_STATIC_FAST_MUTEX(x_NetCacheMutex_ID);



void CNetCache_Logger::Put(const string&               auth, 
                           const NetCache_RequestStat& stat,
                           const string&               blob_key)
{
    string msg, tmp;
    msg += auth;
    msg += ';';
    msg += stat.peer_address;
    msg += ';';
    msg += stat.conn_time.AsString();
    msg += ';';
    msg += (char)stat.req_code;
    msg += ';';
    NStr::UInt8ToString(tmp, stat.blob_size);
    msg += tmp;
    msg += ';';
    msg += NStr::DoubleToString(stat.elapsed, 5);
    msg += ';';
    msg += NStr::DoubleToString(stat.comm_elapsed, 5);
    msg += ';';
    msg += blob_key;
    msg += "\n";

    m_Log << msg;

    if (stat.req_code == 'V') {
        m_Log << NcbiFlush;
    }
}


static CNetCacheServer* s_netcache_server = 0;


///@internal
static
void TlsCleanup(SNC_ThreadData* p_value, void* /* data */ )
{
    delete p_value;
}

/// @internal
static
CRef< CTls<SNC_ThreadData> > s_tls(new CTls<SNC_ThreadData>);


CNetCacheServer::CNetCacheServer(unsigned int     port,
                                 bool             use_hostname,
                                 CBDB_Cache*      cache,
                                 unsigned         max_threads,
                                 unsigned         init_threads,
                                 unsigned         network_timeout,
                                 bool             is_log,
                                 const IRegistry& reg) 
: CThreadedServer(port),
    m_MaxId(0),
    m_Cache(cache),
    m_Shutdown(false),
    m_InactivityTimeout(network_timeout),
    m_LogFlag(is_log),
    m_TLS_Size(64 * 1024),
    m_Reg(reg)
{
    if (use_hostname) {
        char hostname[256];
        if (SOCK_gethostname(hostname, sizeof(hostname)) == 0) {
            m_Host = hostname;
        }
    } else {
        unsigned int hostaddr = SOCK_HostToNetLong(SOCK_gethostbyname(0));
        char ipaddr[32];
        sprintf(ipaddr, "%u.%u.%u.%u", (hostaddr >> 24) & 0xff,
                                       (hostaddr >> 16) & 0xff,
                                       (hostaddr >> 8)  & 0xff,
                                        hostaddr        & 0xff);
        m_Host = ipaddr;
    }

    m_MaxThreads = max_threads ? max_threads : 25;
    m_InitThreads = init_threads ? 
        (init_threads < m_MaxThreads ? init_threads : 2)  : 10;
    m_QueueSize = m_MaxThreads * 2;

    x_CreateLog();

    s_netcache_server = this;
    m_AcceptTimeout = &m_ThrdSrvAcceptTimeout;
}

CNetCacheServer::~CNetCacheServer()
{
    try {
        NON_CONST_ITERATE(TLocalCacheMap, it, m_LocalCacheMap) {
            ICache *ic = it->second;
            delete ic; it->second = 0;
        }
        StopSessionManagement();
    } catch (...) {
        ERR_POST("Exception in ~CNetCacheServer()");
        _ASSERT(0);
    }
}

void CNetCacheServer::StartSessionManagement(
                                unsigned session_shutdown_timeout)
{
    LOG_POST(Info << "Starting session management thread. timeout=" 
                  << session_shutdown_timeout);
    m_SessionMngThread.Reset(
        new CSessionManagementThread(*this,
                            session_shutdown_timeout, 10, 2));
    m_SessionMngThread->Run();
}

void CNetCacheServer::StopSessionManagement()
{
# ifdef NCBI_THREADS
    if (!m_SessionMngThread.Empty()) {
        LOG_POST(Info << "Stopping session management thread...");
        m_SessionMngThread->RequestStop();
        m_SessionMngThread->Join();
        LOG_POST(Info << "Stopped.");
    }
# endif
}


void CNetCacheServer::SetParams()
{
    m_ThrdSrvAcceptTimeout.sec = 1;
    m_ThrdSrvAcceptTimeout.usec = 0;
}

void CNetCacheServer::MountICache(CConfig&                conf,
                                  CPluginManager<ICache>& pm_cache)
{
    CFastMutexGuard guard(m_LocalCacheMap_Lock);

    const CConfig::TParamTree* param_tree = conf.GetTree();
    x_GetICacheNames(&m_LocalCacheMap);
    NON_CONST_ITERATE(TLocalCacheMap, it, m_LocalCacheMap) {
        const string& cache_name = it->first;
        if (it->second != 0) {
            continue;  // already created
        }
        string section_name("icache_");
        section_name.append(cache_name);

        const TPluginManagerParamTree* bdb_tree = 
            param_tree->FindSubNode(section_name);
        if (!bdb_tree) {
            ERR_POST("Configuration error. Cannot find registry section " 
                     << section_name);
            continue;
        }

        ICache* ic;
        try {
            ic = pm_cache.CreateInstance(
                                kBDBCacheDriverName,
                                TCachePM::GetDefaultDrvVers(),
                                bdb_tree);

            it->second = ic;
            LOG_POST("Local cache mounted: " << cache_name);
            CBDB_Cache* bdb_cache = dynamic_cast<CBDB_Cache*>(ic);
            if (bdb_cache) {
                bdb_cache->SetMonitor(&m_Monitor);
            }

        } 
        catch (exception& ex)
        {
            ERR_POST("Error mounting local cache:" << cache_name << 
                     ":\n" << ex.what()
                    );
            throw;
        }

    } // ITERATE
}

ICache* CNetCacheServer::GetLocalCache(const string& cache_name)
{
    CFastMutexGuard guard(m_LocalCacheMap_Lock);

    TLocalCacheMap::iterator it = m_LocalCacheMap.find(cache_name);
    if (it == m_LocalCacheMap.end()) {
        return 0;
    }
    return it->second;
}


void CNetCacheServer::ProcessOverflow(SOCK sock) 
{
    const char* msg = "ERR:Server busy";
    
    size_t n_written;
    SOCK_Write(sock, (void *)msg, 16, &n_written, eIO_WritePlain);
    SOCK_Close(sock); 
    ERR_POST("ProcessOverflow! Server is busy.");
}



/// @internal
bool CNetCacheServer::WaitForReadSocket(CSocket& sock, unsigned time_to_wait)
{
    STimeout to = {time_to_wait, 0};
    EIO_Status io_st = sock.Wait(eIO_Read, &to);
    switch (io_st) {    
    case eIO_Success:
    case eIO_Closed: // following read will return EOF
        return true;  
    case eIO_Timeout:
        NCBI_THROW(CNetServiceException, eTimeout, kEmptyStr);
    default:
        NCBI_THROW(CNetServiceException, 
                   eCommunicationError, "Socket Wait error.");
        return false;
    }
    return false;
}        


void CNetCacheServer::ProcessNC(CSocket&              socket,
                                ENC_RequestType&      req_type,
                                SNC_Request&          req,
                                SNC_ThreadData&       tdata,
                                NetCache_RequestStat& stat)
{
    ParseRequestNC(tdata.request, &(tdata.req));

    switch ((req_type = tdata.req.req_type)) {
    case ePut:
        stat.req_code = 'P';
        ProcessPut(socket, tdata.req, tdata, stat);
        break;
    case ePut2:
        stat.req_code = 'P';
        ProcessPut2(socket, tdata.req, tdata, stat);
        break;
    case ePut3:
        stat.req_code = 'P';
        ProcessPut3(socket, tdata.req, tdata, stat);
        break;
    case eGet:
        stat.req_code = 'G';
        ProcessGet(socket, tdata.req, tdata, stat);
        break;
    case eGet2:
        stat.req_code = 'G';
        ProcessGet2(socket, tdata.req, tdata, stat);
        break;
    case eShutdown:
        stat.req_code = 'S';
        ProcessShutdown(socket);
        break;
    case eGetConfig:
        stat.req_code = 'C';
        ProcessGetConfig(socket);
        break;
    case eGetStat:
        stat.req_code = 'T';
        ProcessGetStat(socket, tdata.req);
        break;
    case eDropStat:
        stat.req_code = 'D';
        ProcessDropStat(socket);
        break;
    case eGetBlobOwner:
        ProcessGetBlobOwner(socket, tdata.req);
        break;
    case eVersion:
        stat.req_code = 'V';
        ProcessVersion(socket, tdata.req);
        break;
    case eRemove:
        stat.req_code = 'R';
        ProcessRemove(socket, tdata.req);
        break;
    case eRemove2:
        stat.req_code = 'R';
        ProcessRemove2(socket, tdata.req);
        break;
    case eLogging:
        stat.req_code = 'L';
        ProcessLog(socket, tdata.req);
        break;
    case eIsLock:
        stat.req_code = 'K';
        ProcessIsLock(socket, tdata.req);
        break;
    case eError:
        WriteMsg(socket, "ERR:", tdata.req.err_msg);
        x_RegisterProtocolErr(eError, tdata.auth);
        break;
    default:
        _ASSERT(0);
    } // switch

}

void CNetCacheServer::ProcessIC(CSocket&              socket,
                                EIC_RequestType&      req_type,
                                SIC_Request&          req,
                                SNC_ThreadData&       tdata,
                                NetCache_RequestStat& stat)
{
    ParseRequestIC(tdata.request, &req);

    ICache* ic = GetLocalCache(req.cache_name);
    if (ic == 0) {
        tdata.ic_req.err_msg = "Cache unknown: ";
        tdata.ic_req.err_msg.append(req.cache_name);
        WriteMsg(socket, "ERR:", tdata.ic_req.err_msg);
        return;
    }

    switch ((req_type = tdata.ic_req.req_type)) {
    case eIC_SetTimeStampPolicy:
        Process_IC_SetTimeStampPolicy(*ic, socket, req, tdata);
        break;
    case eIC_GetTimeStampPolicy:
        Process_IC_GetTimeStampPolicy(*ic, socket, req, tdata);
        break;
    case eIC_SetVersionRetention:
        Process_IC_SetVersionRetention(*ic, socket, req, tdata);
        break;
    case eIC_GetVersionRetention:
        Process_IC_GetVersionRetention(*ic, socket, req, tdata);
        break;
    case eIC_GetTimeout:
        Process_IC_GetTimeout(*ic, socket, req, tdata);
        break;
    case eIC_IsOpen:
        Process_IC_IsOpen(*ic, socket, req, tdata);
        break;
    case eIC_Store:
        Process_IC_Store(*ic, socket, req, tdata);
        break;
    case eIC_StoreBlob:
        Process_IC_StoreBlob(*ic, socket, req, tdata);
        break;
    case eIC_GetSize:
        Process_IC_GetSize(*ic, socket, req, tdata);
        break;
    case eIC_GetBlobOwner:
        Process_IC_GetBlobOwner(*ic, socket, req, tdata);
        break;
    case eIC_Read:
        Process_IC_Read(*ic, socket, req, tdata);
        break;
    case eIC_Remove:
        Process_IC_Remove(*ic, socket, req, tdata);
        break;
    case eIC_RemoveKey:
        Process_IC_RemoveKey(*ic, socket, req, tdata);
        break;
    case eIC_GetAccessTime:
        Process_IC_GetAccessTime(*ic, socket, req, tdata);
        break;
    case eIC_HasBlobs:
        Process_IC_HasBlobs(*ic, socket, req, tdata);
        break;
    case eIC_Purge1:
        Process_IC_Purge1(*ic, socket, req, tdata);
        break;

    case eIC_Error:
        WriteMsg(socket, "ERR:", tdata.ic_req.err_msg);
        break;
    default:
        _ASSERT(0);
    } // switch

}

void CNetCacheServer::ProcessSM(CSocket& socket, string& req)
{
    // SMR host pid     -- registration
    // or
    // SMU host pid     -- unregistration
    //
    if (m_SessionMngThread.Empty()) {
        WriteMsg(socket, "ERR:", "Server does not support sessions ");
        return;
    }

    if (req[0] == 'S' && req[1] == 'M') {
        bool reg;
        if (req[2] == 'R') {
            reg = true;
        } else
        if (req[2] == 'U') {
            reg = false;
        } else {
            goto err;
        }
        req.erase(0, 3);
        NStr::TruncateSpacesInPlace(req, NStr::eTrunc_Begin);
        string host, port_str;
        bool split = NStr::SplitInTwo(req, " ", host, port_str);
        if (!split) {
            goto err;
        }
        unsigned port = NStr::StringToUInt(port_str);

        if (!port || host.empty()) {
            goto err;
        }

        if (reg) {
            m_SessionMngThread->RegisterSession(host, port);
        } else {
            m_SessionMngThread->UnRegisterSession(host, port);
        }
        WriteMsg(socket, "OK:", "");
    } else {
        err:
        WriteMsg(socket, "ERR:", "Invalid request ");
    }
}


void CNetCacheServer::Process(SOCK sock)
{
    SNC_ThreadData* tdata = x_GetThreadData();

    ENC_RequestType nc_req_type = eError;
    EIC_RequestType ic_req_type = eIC_Error;
    EServed_RequestType service_req_type = eNCServer_Unknown;

    NetCache_RequestStat    stat;
    stat.blob_size = stat.io_blocks = 0;

    CSocket socket;
    socket.Reset(sock, eTakeOwnership, eCopyTimeoutsFromSOCK);
    STimeout zero = {0,0};
    socket.SetTimeout(eIO_Close,&zero);

    try {
        tdata->auth.erase();
        
        CStopWatch  sw(CStopWatch::eStop);
        bool is_log = IsLog();

        if (is_log || m_Monitor.IsMonitorActive()) {
            stat.conn_time = m_LocalTimer.GetLocalTime();
            stat.comm_elapsed = stat.elapsed = 0;
            stat.peer_address = socket.GetPeerAddress();
                       
            // get only host name
            string::size_type offs = stat.peer_address.find_first_of(":");
            if (offs != string::npos) {
                stat.peer_address.erase(offs, stat.peer_address.length());
            }
            stat.req_code = '?';

            sw.Start();
        }

        // Set socket parameters

        socket.DisableOSSendDelay();
        STimeout to = {m_InactivityTimeout, 0};
        socket.SetTimeout(eIO_ReadWrite , &to);

        WaitForReadSocket(socket, m_InactivityTimeout);

        if (ReadStr(socket, &(tdata->auth))) {
            
            WaitForReadSocket(socket, m_InactivityTimeout);

            while (ReadStr(socket, &(tdata->request))) {
                string& rq = tdata->request;
                if (rq.length() < 2) { 
                    WriteMsg(socket, "ERR:", "Invalid request");
                    x_RegisterProtocolErr(eError, rq);
                    return;
                }

                // check if it is NC or IC

                if (rq[0] == 'I' && rq[1] == 'C') {  // ICache request
                    tdata->ic_req.Init();
                    service_req_type = eNCServer_ICache;
                    ProcessIC(socket, 
                              ic_req_type, tdata->ic_req, *tdata, stat);
                } else 
                if (rq[0] == 'A' && rq[1] == '?') {  // Alive?
                    WriteMsg(socket, "OK:", "");
                } else
                if (rq[0] == 'S' && rq[1] == 'M') {  // Session management
                    service_req_type = eNCServer_Session;
                    ProcessSM(socket, rq);
                    break; // need to disconnect after reg-unreg
                } else 
                if (rq[0] == 'O' && rq[1] == 'K') {
                    continue;
                } else
                if (rq == "MONI") {
                    m_Monitor.SetSocket(socket);
                    m_Monitor.SendString("Monitor for " NETCACHED_VERSION "\n");
                    // Avoid handling closing connection
                    return;
                } else {
                    tdata->req.Init();
                    service_req_type = eNCServer_NetCache;
                    ProcessNC(socket, 
                              nc_req_type, tdata->req, *tdata, stat);
                }

                // Monitoring
                //
                if (m_Monitor.IsMonitorActive()) {

                    stat.elapsed = sw.Elapsed();

                    string msg, tmp;
                    msg += tdata->auth;
                    msg += "\"";
                    msg += tdata->request;
                    msg += "\" ";
                    msg += stat.peer_address;
                    msg += "\n\t";
                    msg += "ConnTime=" + stat.conn_time.AsString();
                    msg += " BLOB size=";
                    NStr::UInt8ToString(tmp, stat.blob_size);
                    msg += tmp;
                    msg += " elapsed=";
                    msg += NStr::DoubleToString(stat.elapsed, 5);
                    msg += " comm.elapsed=";
                    msg += NStr::DoubleToString(stat.comm_elapsed, 5);
                    msg += "\n\t";
                    switch (service_req_type) {
                    case eNCServer_NetCache:                
                        msg += "NC:" + tdata->req.req_id;
                        break;
                    case eNCServer_Session:
                        msg += "Session request";
                        break;
                    case eNCServer_ICache:
                        msg + "IC:" + "Cache=\"" + tdata->ic_req.cache_name + "\"";
                        msg += " key=\"" + tdata->ic_req.key;
                        msg += "\" version=" + NStr::IntToString(tdata->ic_req.version);
                        msg += " subkey=\"" + tdata->ic_req.subkey +"\"";
                        break;
                    case eNCServer_Unknown:
                        msg += "UNK?";
                        break;
                    default:
                        _ASSERT(0);
                    }
                    msg += "\n";

                    m_Monitor.SendString(msg);
                }

            } // while

        }

        // cleaning up the input wire, in case if there is some
        // trailing input.
        // i don't want to know wnat is it, but if i don't read it
        // some clients will receive PEER RESET error trying to read
        // servers answer.
        //
        // I'm reading fixed length sized buffer, so any huge ill formed 
        // request still will end up with an error on the client side
        // (considered a client's fault (DDOS attempt))
        to.sec = to.usec = 0;
        socket.SetTimeout(eIO_Read, &to);
        socket.Read(NULL, 1024);

        //
        // Logging.
        //
        if (is_log) {
            stat.elapsed = sw.Elapsed();
            CNetCache_Logger* lg = GetLogger();
            _ASSERT(lg);
            lg->Put(tdata->auth, stat, tdata->req.req_id);
        }
    } 
    catch (CNetCacheException &ex)
    {
        string msg;
        msg = "NC Server error: ";
        msg.append(ex.what());
        msg.append(" client=");  msg.append(tdata->auth);
        msg.append(" request='");msg.append(tdata->request); msg.append("'");
        msg.append(" peer="); msg.append(socket.GetPeerAddress());
        msg.append(" blobsize=");
            msg.append(NStr::UIntToString(stat.blob_size));
        msg.append(" io blocks=");
            msg.append(NStr::UIntToString(stat.io_blocks));
        msg.append("\n");

        ERR_POST(msg);

        x_RegisterException(nc_req_type, tdata->auth, ex);

        if (m_Monitor.IsMonitorActive()) {
            m_Monitor.SendString(msg);
        }
    }
    catch (CNetServiceException& ex)
    {
        string msg;
        msg = "NC Service exception: ";
        msg.append(ex.what());
        msg.append(" client=");  msg.append(tdata->auth);
        msg.append(" request='");msg.append(tdata->request); msg.append("'");
        msg.append(" peer="); msg.append(socket.GetPeerAddress());
        msg.append(" blobsize=");
            msg.append(NStr::UIntToString(stat.blob_size));
        msg.append(" io blocks=");
            msg.append(NStr::UIntToString(stat.io_blocks));
        msg.append("\n");

        ERR_POST(msg);

        x_RegisterException(nc_req_type, tdata->auth, ex);

        if (m_Monitor.IsMonitorActive()) {
            m_Monitor.SendString(msg);
        }
    }
    catch (CBDB_ErrnoException& ex)
    {
        if (ex.IsRecovery()) {
            string msg = "Fatal Berkeley DB error: DB_RUNRECOVERY. " 
                         "Emergency shutdown initiated!";
            ERR_POST(msg);
            if (m_Monitor.IsMonitorActive()) {
                m_Monitor.SendString(msg);
            }
            SetShutdownFlag();
        } else {
            string msg;
            msg = "NC BerkeleyDB error: ";
            msg.append(ex.what());
            msg.append(" client=");  msg.append(tdata->auth);
            msg.append(" request='");msg.append(tdata->request); msg.append("'");
            msg.append(" peer="); msg.append(socket.GetPeerAddress());
            msg.append(" blobsize=");
                msg.append(NStr::UIntToString(stat.blob_size));
            msg.append(" io blocks=");
                msg.append(NStr::UIntToString(stat.io_blocks));
            msg.append("\n");
            ERR_POST(msg);

            if (m_Monitor.IsMonitorActive()) {
                m_Monitor.SendString(msg);
            }

            x_RegisterInternalErr(nc_req_type, tdata->auth);
        }
        throw;
    }
    catch (exception& ex)
    {
        string msg;
        msg = "NC std::exception: ";
        msg.append(ex.what());
        msg.append(" client=");  msg.append(tdata->auth);
        msg.append(" request='");msg.append(tdata->request); msg.append("'");
        msg.append(" peer="); msg.append(socket.GetPeerAddress());
        msg.append(" blobsize=");
            msg.append(NStr::UIntToString(stat.blob_size));
        msg.append(" io blocks=");
            msg.append(NStr::UIntToString(stat.io_blocks));
        msg.append("\n");

        ERR_POST(msg);

        x_RegisterInternalErr(nc_req_type, tdata->auth);

        if (m_Monitor.IsMonitorActive()) {
            m_Monitor.SendString(msg);
        }
    }
}

void CNetCacheServer::ProcessShutdown(CSocket& sock)
{    
    SetShutdownFlag();
    WriteMsg(sock, "OK:", "");
}

void CNetCacheServer::ProcessVersion(CSocket& sock, const SNC_Request& req)
{
    WriteMsg(sock, "OK:", NETCACHED_VERSION); 
}

void CNetCacheServer::ProcessGetConfig(CSocket& sock)
{
    SOCK sk = sock.GetSOCK();
    sock.SetOwnership(eNoOwnership);
    sock.Reset(0, eTakeOwnership, eCopyTimeoutsToSOCK);

    CConn_SocketStream ios(sk);
    m_Reg.Write(ios);
}

void CNetCacheServer::ProcessGetStat(CSocket& sock, const SNC_Request& req)
{
    //CNcbiRegistry reg;

    SOCK sk = sock.GetSOCK();
    sock.SetOwnership(eNoOwnership);
    sock.Reset(0, eTakeOwnership, eCopyTimeoutsToSOCK);

    CConn_SocketStream ios(sk);

    CBDB_Cache* bdb_cache = dynamic_cast<CBDB_Cache*>(m_Cache);
    if (!bdb_cache) {
        return;
    }
    bdb_cache->Lock();

    try {

        const SBDB_CacheStatistics& cs = bdb_cache->GetStatistics();
        cs.PrintStatistics(ios);
        //cs.ConvertToRegistry(&reg);

    } catch(...) {
        bdb_cache->Unlock();
        throw;
    }
    bdb_cache->Unlock();

    //reg.Write(ios,  CNcbiRegistry::fTransient | CNcbiRegistry::fPersistent);
}

void CNetCacheServer::ProcessDropStat(CSocket& sock)
{
    CBDB_Cache* bdb_cache = dynamic_cast<CBDB_Cache*>(m_Cache);
    if (!bdb_cache) {
        return;
    }
	bdb_cache->InitStatistics();
    WriteMsg(sock, "OK:", "");
}


void CNetCacheServer::ProcessRemove(CSocket& sock, const SNC_Request& req)
{
    const string& req_id = req.req_id;

    if (req_id.empty()) {
        WriteMsg(sock, "ERR:", "BLOB id is empty.");
        return;
    }

    CNetCache_Key blob_id(req_id);
    if (!x_CheckBlobId(sock, &blob_id, req_id))
        return;

    m_Cache->Remove(req_id);
}


void CNetCacheServer::ProcessRemove2(CSocket& sock, const SNC_Request& req)
{
    ProcessRemove(sock, req);
    WriteMsg(sock, "OK:", "");
}


void CNetCacheServer::x_WriteBuf(CSocket& sock,
                                 char*    buf,
                                 size_t   bytes)
{
    do {
        size_t n_written;
        EIO_Status io_st;
        io_st = sock.Write(buf, bytes, &n_written);
        switch (io_st) {
        case eIO_Success:
            break;
        case eIO_Timeout:
            {
            string msg = "Communication timeout error (cannot send)";
            msg.append(" IO block size=");
            msg.append(NStr::UIntToString(bytes));
            
            NCBI_THROW(CNetServiceException, eTimeout, msg);
            }
            break;
        case eIO_Closed:
            {
            string msg = 
                "Communication error: peer closed connection (cannot send)";
            msg.append(" IO block size=");
            msg.append(NStr::UIntToString(bytes));
            
            NCBI_THROW(CNetServiceException, eCommunicationError, msg);
            }
            break;
        case eIO_Interrupt:
            {
            string msg = 
                "Communication error: interrupt signal (cannot send)";
            msg.append(" IO block size=");
            msg.append(NStr::UIntToString(bytes));
            
            NCBI_THROW(CNetServiceException, eCommunicationError, msg);
            }
            break;
        case eIO_InvalidArg:
            {
            string msg = 
                "Communication error: invalid argument (cannot send)";
            msg.append(" IO block size=");
            msg.append(NStr::UIntToString(bytes));
            
            NCBI_THROW(CNetServiceException, eCommunicationError, msg);
            }
            break;
        default:
            {
            string msg = "Communication error (cannot send)";
            msg.append(" IO block size=");
            msg.append(NStr::UIntToString(bytes));
            
            NCBI_THROW(CNetServiceException, eCommunicationError, msg);
            }
            break;
        } // switch
        bytes -= n_written;
        buf += n_written;
    } while (bytes > 0);
}


void CNetCacheServer::ProcessGet2(CSocket&               sock, 
                                  const SNC_Request&     req,
                                  SNC_ThreadData&        tdata,
                                  NetCache_RequestStat&  stat)
{
    // The key difference between Get2 and Get is that Get2 protocol
    // requires the client to send a string "OK:" when client receives
    // all the data. We do that to prevent server side disconnect 
    // which freezes socket for 2 minutes on the kernel level

    const string& req_id = req.req_id;

    if (req_id.empty()) {
        WriteMsg(sock, "ERR:", "BLOB id is empty.");
        x_RegisterProtocolErr(req.req_type, tdata.auth);
        return;
    }

    if (req.no_lock) {
        bool locked = m_Cache->IsLocked(req_id, 0, kEmptyStr);
        if (locked) {
            WriteMsg(sock, "ERR:", "BLOB locked by another client"); 
            return;
        }
/*
        bool lock_accuired = guard.Lock(req_id, 0);
        if (!lock_accuired) {  // BLOB is locked by someone else
            WriteMsg(sock, "ERR:", "BLOB locked by another client"); 
            return;
        }
*/
    } else {
//        guard.Lock(req_id, m_InactivityTimeout);
    }
    char* buf = tdata.buffer.get();

    ICache::SBlobAccessDescr ba_descr;
    buf += 100;
    ba_descr.buf = buf;
    ba_descr.buf_size = GetTLS_Size() - 100;

    unsigned blob_id = CNetCache_Key::GetBlobId(req.req_id);

    for (int repeats = 0; repeats < 1000; ++repeats) {
        m_Cache->GetBlobAccess(req_id, 0, kEmptyStr, &ba_descr);

        if (!ba_descr.blob_found) {
            // check if id is locked maybe blob record not
            // yet commited
            {{
                if (m_Cache->IsLocked(blob_id)) {
                    if (repeats < 999) {
                        SleepMilliSec(repeats);
                        continue;
                    }
                } else {
                    m_Cache->GetBlobAccess(req_id, 0, kEmptyStr, &ba_descr);
                    if (ba_descr.blob_found) {
                        break;
                    }

                }
            }}
            string msg = "BLOB not found. ";
            msg += req_id;
            WriteMsg(sock, "ERR:", msg);
            return;
        } else {
            break;
        }
    }

    if (ba_descr.blob_size == 0) {
        WriteMsg(sock, "OK:", "BLOB found. SIZE=0");
        return;
    }

/*
    if (ba_descr.blob_size == 0) { // not found
        if (ba_descr.reader == 0) {
blob_not_found:
            string msg = "BLOB not found. ";
            msg += req_id;
            WriteMsg(sock, "ERR:", msg);
        } else {
            WriteMsg(sock, "OK:", "BLOB found. SIZE=0");
        }
        x_RegisterNoBlobErr(req.req_type, tdata.auth);
        return;
    }
*/
    stat.blob_size = ba_descr.blob_size;

    if (ba_descr.reader.get() == 0) {  // all in buffer
        string msg("OK:BLOB found. SIZE=");
        string sz;
        NStr::UIntToString(sz, ba_descr.blob_size);
        msg += sz;

        const char* msg_begin = msg.c_str();
        const char* msg_end = msg_begin + msg.length();

        for (; msg_end >= msg_begin; --msg_end) {
            --buf;
            *buf = *msg_end;
            ++ba_descr.blob_size;
        }

        // translate BLOB fragment to the network
        CStopWatch  sw(CStopWatch::eStart);

        x_WriteBuf(sock, buf, ba_descr.blob_size);

        stat.comm_elapsed += sw.Elapsed();
        ++stat.io_blocks;
        return;

    } // inline BLOB


    // re-translate reader to the network

    auto_ptr<IReader> rdr(ba_descr.reader.release());
    if (!rdr.get()) {
        string msg = "BLOB not found. ";
        msg += req_id;
        WriteMsg(sock, "ERR:", msg);
        return;
    }
    size_t blob_size = ba_descr.blob_size;

    bool read_flag = false;
    
    buf = tdata.buffer.get();

    size_t bytes_read;
    do {
        ERW_Result io_res = rdr->Read(buf, GetTLS_Size(), &bytes_read);
        if (io_res == eRW_Success && bytes_read) {
            if (!read_flag) {
                read_flag = true;
                string msg("BLOB found. SIZE=");
                string sz;
                NStr::UIntToString(sz, blob_size);
                msg += sz;
                WriteMsg(sock, "OK:", msg);
            }

            // translate BLOB fragment to the network
            CStopWatch  sw(CStopWatch::eStart);

            x_WriteBuf(sock, buf, bytes_read);

            stat.comm_elapsed += sw.Elapsed();
            ++stat.io_blocks;

        } else {
            break;
        }
    } while(1);
    if (!read_flag) {
        string msg = "BLOB not found. ";
        msg += req_id;
        WriteMsg(sock, "ERR:", msg);
        return;
    }
}




void CNetCacheServer::ProcessGet(CSocket&               sock, 
                                 const SNC_Request&     req,
                                 SNC_ThreadData&        tdata,
                                 NetCache_RequestStat&  stat)
{
    const string& req_id = req.req_id;

    if (req_id.empty()) {
        WriteMsg(sock, "ERR:", "BLOB id is empty.");
        x_RegisterProtocolErr(req.req_type, tdata.auth);
        return;
    }

//    CIdBusyGuard guard(&m_UsedIds);
    if (req.no_lock) {
        bool locked = m_Cache->IsLocked(req_id, 0, kEmptyStr);
        if (locked) {
            WriteMsg(sock, "ERR:", "BLOB locked by another client"); 
            return;
        }
/*
        bool lock_accuired = guard.Lock(req_id, 0);
        if (!lock_accuired) {  // BLOB is locked by someone else
            WriteMsg(sock, "ERR:", "BLOB locked by another client");
            return;
        }
*/
    } else {
//        guard.Lock(req_id, m_InactivityTimeout);
    }
    char* buf = tdata.buffer.get();

    ICache::SBlobAccessDescr ba_descr;
    buf += 100;
    ba_descr.buf = buf;
    ba_descr.buf_size = GetTLS_Size() - 100;

    unsigned blob_id = CNetCache_Key::GetBlobId(req.req_id);

    for (int repeats = 0; repeats < 1000; ++repeats) {
        m_Cache->GetBlobAccess(req_id, 0, kEmptyStr, &ba_descr);

        if (!ba_descr.blob_found) {
            // check if id is locked maybe blob record not
            // yet commited
            {{
                if (m_Cache->IsLocked(blob_id)) {
                    if (repeats < 999) {
                        SleepMilliSec(repeats);
                        continue;
                    }
                } else {
                    m_Cache->GetBlobAccess(req_id, 0, kEmptyStr, &ba_descr);
                    if (ba_descr.blob_found) {
                        break;
                    }

                }
            }}
            string msg = "BLOB not found. ";
            msg += req_id;
            WriteMsg(sock, "ERR:", msg);
            return;
        } else {
            break;
        }
    }

/*
    m_Cache->GetBlobAccess(req_id, 0, kEmptyStr, &ba_descr);

    if (!ba_descr.blob_found) {
blob_not_found:
            string msg = "BLOB not found. ";
            msg += req_id;
            WriteMsg(sock, "ERR:", msg);
            return;
    }
*/

    if (ba_descr.blob_size == 0) {
        WriteMsg(sock, "OK:", "BLOB found. SIZE=0");
        return;
    }

/*
    if (ba_descr.blob_size == 0) { // not found
        if (ba_descr.reader == 0) {
blob_not_found:
            string msg = "BLOB not found. ";
            msg += req_id;
            WriteMsg(sock, "ERR:", msg);
        } else {
            WriteMsg(sock, "OK:", "BLOB found. SIZE=0");
        }
        x_RegisterNoBlobErr(req.req_type, tdata.auth);
        return;
    }
*/
    stat.blob_size = ba_descr.blob_size;

    if (ba_descr.reader.get() == 0) {  // all in buffer
        string msg("OK:BLOB found. SIZE=");
        string sz;
        NStr::UIntToString(sz, ba_descr.blob_size);
        msg += sz;

        const char* msg_begin = msg.c_str();
        const char* msg_end = msg_begin + msg.length();

        for (; msg_end >= msg_begin; --msg_end) {
            --buf;
            *buf = *msg_end;
            ++ba_descr.blob_size;
        }

        // translate BLOB fragment to the network
        CStopWatch  sw(CStopWatch::eStart);

        x_WriteBuf(sock, buf, ba_descr.blob_size);

        stat.comm_elapsed += sw.Elapsed();
        ++stat.io_blocks;

        return;

    } // inline BLOB


    // re-translate reader to the network

    auto_ptr<IReader> rdr(ba_descr.reader.release());
    if (!rdr.get()) {
        string msg = "BLOB not found. ";
        msg += req_id;
        WriteMsg(sock, "ERR:", msg);
        return;
    }
    size_t blob_size = ba_descr.blob_size;

    bool read_flag = false;
    
    buf = tdata.buffer.get();

    size_t bytes_read;
    do {
        ERW_Result io_res = rdr->Read(buf, GetTLS_Size(), &bytes_read);
        if (io_res == eRW_Success && bytes_read) {
            if (!read_flag) {
                read_flag = true;
                string msg("BLOB found. SIZE=");
                string sz;
                NStr::UIntToString(sz, blob_size);
                msg += sz;
                WriteMsg(sock, "OK:", msg);
            }

            // translate BLOB fragment to the network
            CStopWatch  sw(CStopWatch::eStart);

            x_WriteBuf(sock, buf, bytes_read);

            stat.comm_elapsed += sw.Elapsed();
            ++stat.io_blocks;

        } else {
            break;
        }
    } while(1);
    if (!read_flag) {
        string msg = "BLOB not found. ";
        msg += req_id;
        WriteMsg(sock, "ERR:", msg);
        return;
    }

}

void CNetCacheServer::ProcessPut(CSocket&              sock, 
                                 SNC_Request&          req,
                                 SNC_ThreadData&       tdata,
                                 NetCache_RequestStat& stat)
{
    string& rid = req.req_id;

    if (req.req_id.empty()) {
        unsigned int id = m_Cache->GetNextBlobId(false);
        CNetCache_Key::GenerateBlobKey(&rid, id, m_Host, GetPort());
    }

    WriteMsg(sock, "ID:", rid);

    auto_ptr<IWriter> iwrt;

    // Reconfigure socket for no-timeout operation

    STimeout to;
    to.sec = to.usec = 0;
    sock.SetTimeout(eIO_Read, &to);
    char* buf = tdata.buffer.get();
    size_t buf_size = GetTLS_Size();

    bool not_eof;

    do {
        size_t nn_read;

        CStopWatch  sw(CStopWatch::eStart);

        not_eof = ReadBuffer(sock, buf, buf_size, &nn_read);

        if (nn_read == 0 && !not_eof && (iwrt.get() == 0)) {
            m_Cache->Store(rid, 0, kEmptyStr, 
                           buf, nn_read, req.timeout, tdata.auth);
            break;
        }

        stat.comm_elapsed += sw.Elapsed();
        stat.blob_size += nn_read;

        if (nn_read) {
            if (iwrt.get() == 0) { // first read

                if (not_eof == false) { // complete read
                    m_Cache->Store(rid, 0, kEmptyStr, 
                                   buf, nn_read, req.timeout, tdata.auth);
                    return;
                }

                iwrt.reset(
                    m_Cache->GetWriteStream(rid, 0, kEmptyStr, 
                                            req.timeout, tdata.auth));
            }
            size_t bytes_written;
            ERW_Result res = 
                iwrt->Write(buf, nn_read, &bytes_written);
            if (res != eRW_Success) {
                WriteMsg(sock, "ERR:", "Server I/O error");
                x_RegisterInternalErr(req.req_type, tdata.auth);
                return;
            }
        } // if (nn_read)

    } while (not_eof);

    if (iwrt.get()) {
        iwrt->Flush();
        iwrt.reset(0);
    }
}


void CNetCacheServer::ProcessPut2(CSocket&              sock, 
                                  SNC_Request&          req,
                                  SNC_ThreadData&       tdata,
                                  NetCache_RequestStat& stat)
{
    string& rid = req.req_id;
    bool do_id_lock = true;

    unsigned int id = 0;
    if (req.req_id.empty()) {
        id = m_Cache->GetNextBlobId(do_id_lock);
        CNetCache_Key::GenerateBlobKey(&rid, id, m_Host, GetPort());
        do_id_lock = false;
    } else {
        id = CNetCache_Key::GetBlobId(req.req_id);
    }

    // BLOB already locked, it is safe to return BLOB id
    if (!do_id_lock) {
        WriteMsg(sock, "ID:", rid);
    }

    auto_ptr<IWriter> iwrt;

    // create the reader up front to guarantee correct BLOB locking
    // the possible problem (?) here is that we have to do double buffering
    // of the input stream
    iwrt.reset(
        m_Cache->GetWriteStream(id, rid, 0, kEmptyStr, do_id_lock,
                                req.timeout, tdata.auth));

    if (do_id_lock) {
        WriteMsg(sock, "ID:", rid);
    }

    char* buf = tdata.buffer.get();
    size_t buf_size = GetTLS_Size();

    bool not_eof;

    CSocketReaderWriter  comm_reader(&sock, eNoOwnership);
    CTransmissionReader  transm_reader(&comm_reader, eNoOwnership);

    do {
        size_t nn_read = 0;

        CStopWatch  sw(CStopWatch::eStart);

        // here we have double bufferting, reading into a TLS memory
        // and then copy it to IWriter which immediately calls Store
        // 
        not_eof = ReadBuffer(sock, &transm_reader, buf, buf_size, &nn_read);

        stat.comm_elapsed += sw.Elapsed();
        stat.blob_size += nn_read;
        

        // never happens with the upfront created writer...
        // 
        // in the future it's best to create Store function in the writer
        // (BDB specific writer which will do one call store-flush
        // (after this call cache writer cannot be re-used again)
        //
        if (nn_read == 0 && !not_eof && (iwrt.get() == 0)) {
            _ASSERT(id);
            m_Cache->Store(id, rid, 0, kEmptyStr, 
                           buf, nn_read, req.timeout, tdata.auth);
            break;
        }

        if (nn_read) {
            // this branch
            // never happens with unconditionally upfront created IWriter
            //
            if (iwrt.get() == 0) { // first read

                if (not_eof == false) { // complete read
                    _ASSERT(id);
                    m_Cache->Store(id, rid, 0, kEmptyStr, 
                                   buf, nn_read, req.timeout, tdata.auth);
                    break;
                }

                iwrt.reset(
                    m_Cache->GetWriteStream(id, rid, 0, kEmptyStr, false,
                                            req.timeout, tdata.auth));
            }
            size_t bytes_written;
            ERW_Result res = 
                iwrt->Write(buf, nn_read, &bytes_written);
            if (res != eRW_Success) {
                WriteMsg(sock, "Err:", "Server I/O error");
                x_RegisterInternalErr(req.req_type, tdata.auth);
                return;
            }
        } // if (nn_read)

    } while (not_eof);

    if (iwrt.get()) {
        iwrt->Flush();
        iwrt.reset(0);
    }
}

void CNetCacheServer::ProcessPut3(CSocket&              sock, 
                                  SNC_Request&          req,
                                  SNC_ThreadData&       tdata,
                                  NetCache_RequestStat& stat)
{
    ProcessPut2(sock, req, tdata, stat);
    WriteMsg(sock, "OK:", "");
}


void CNetCacheServer::ProcessGetBlobOwner(CSocket&           sock, 
                                          const SNC_Request& req)
{
    const string& req_id = req.req_id;

    if (req_id.empty()) {
        WriteMsg(sock, "ERR:", "BLOB id is empty.");
        return;
    }
    CNetCache_Key blob_id(req_id);
    if (!x_CheckBlobId(sock, &blob_id, req_id))
        return;

    string owner;
    m_Cache->GetBlobOwner(req_id, 0, kEmptyStr, &owner);

    WriteMsg(sock, "OK:", owner);
}


void CNetCacheServer::ProcessIsLock(CSocket& sock, const SNC_Request& req)
{
    const string& req_id = req.req_id;

    if (req_id.empty()) {
        WriteMsg(sock, "ERR:", "BLOB id is empty.");
        return;
    }
    CNetCache_Key blob_id(req_id);
    if (!x_CheckBlobId(sock, &blob_id, req_id))
        return;

    if (m_Cache->IsLocked(blob_id.id)) {
        WriteMsg(sock, "OK:", "1");
    } else {
        WriteMsg(sock, "OK:", "0");
    }

}



void CNetCacheServer::WriteMsg(CSocket&       sock, 
                               const string&  prefix, 
                               const string&  msg)
{
    string err_msg(prefix);
    err_msg.append(msg);
    err_msg.append("\r\n");
    size_t n_written;
    /* EIO_Status io_st = */
        sock.Write(err_msg.c_str(), err_msg.length(), &n_written);
}

/*
bool CNetCacheServer::ReadBuffer(CSocket& sock,
                                 IReader* rdr, 
                                 char*    buf, 
                                 size_t   buf_size,
                                 size_t*  read_length,
                                 size_t   expected_size)
{
    *read_length = 0;
    size_t nn_read = 0;

    bool ret_flag = true;

    while (ret_flag && expected_size) {
        if (!WaitForReadSocket(sock, m_InactivityTimeout)) {
            break;
        }

        ERW_Result io_res = rdr->Read(buf, buf_size, &nn_read);
        switch (io_res) 
        {
        case eRW_Success:
            break;
        case eRW_Timeout:
            if (*read_length == 0) {
                NCBI_THROW(CNetServiceException, eTimeout, "IReader timeout");
            }
            break;
        case eRW_Eof:
            ret_flag = false;
            break;
        default: // invalid socket or request
            NCBI_THROW(CNetServiceException, eCommunicationError, kEmptyStr);

        } // switch
        *read_length += nn_read;
        buf_size -= nn_read;
        expected_size -= nn_read;

        if (buf_size <= 10) {  // buffer too small to read again
            break;
        }
        buf += nn_read;
    }

    return ret_flag;  // false means we hit "eIO_Closed"
}
*/

bool CNetCacheServer::ReadBuffer(CSocket& sock,
                                 IReader* rdr, 
                                 char*    buf, 
                                 size_t   buf_size,
                                 size_t*  read_length)
{
    *read_length = 0;
    size_t nn_read = 0;

    bool ret_flag = true;

    while (ret_flag) {
        if (!WaitForReadSocket(sock, m_InactivityTimeout)) {
            break;
        }

        ERW_Result io_res = rdr->Read(buf, buf_size, &nn_read);
        switch (io_res) 
        {
        case eRW_Success:
            break;
        case eRW_Timeout:
            if (*read_length == 0) {
                NCBI_THROW(CNetServiceException, eTimeout, "IReader timeout");
            }
            break;
        case eRW_Eof:
            ret_flag = false;
            break;
        default: // invalid socket or request
            NCBI_THROW(CNetServiceException, eCommunicationError, kEmptyStr);

        } // switch
        *read_length += nn_read;
        buf_size -= nn_read;

        if (buf_size <= 10) {  // buffer too small to read again
            break;
        }
        buf += nn_read;
    }

    return ret_flag;  // false means we hit "eIO_Closed"
}


bool CNetCacheServer::ReadBuffer(CSocket& sock, 
                                 char*    buf, 
                                 size_t   buf_size,
                                 size_t*  read_length)
{
    *read_length = 0;
    size_t nn_read = 0;

    bool ret_flag = true;

    while (ret_flag) {
        if (!WaitForReadSocket(sock, m_InactivityTimeout)) {
            break;
        }

        EIO_Status io_st = sock.Read(buf, buf_size, &nn_read);
        *read_length += nn_read;
        switch (io_st) 
        {
        case eIO_Success:
            break;
        case eIO_Timeout:
            if (*read_length == 0) {
                NCBI_THROW(CNetServiceException, eTimeout, kEmptyStr);
            }
            break;
        case eIO_Closed:
            ret_flag = false;
            break;
        default: // invalid socket or request
            NCBI_THROW(CNetServiceException, eCommunicationError, kEmptyStr);
        };
        buf_size -= nn_read;

        if (buf_size <= 10) {  // buffer too small to read again
            break;
        }
        buf += nn_read;
    }

    return ret_flag;  // false means we hit "eIO_Closed"

}



bool CNetCacheServer::ReadStr(CSocket& sock, string* str)
{
    _ASSERT(str);

    str->erase();
    char ch;
    EIO_Status io_st;

    char szBuf[1024] = {0,};
    unsigned str_len = 0;
    size_t n_read = 0;

    for (bool flag = true; flag; ) {
        io_st = sock.Read(szBuf, 256, &n_read, eIO_ReadPeek);
        switch (io_st) 
        {
        case eIO_Success:
            flag = false;
            break;
        case eIO_Timeout:
            return false;
            /*
            NCBI_THROW(CNetServiceException, eTimeout, "Connection timeout");
            */
            break;
        default: // invalid socket or request, bailing out
            return false;
        };
    }

    for (str_len = 0; str_len < n_read; ++str_len) {
        ch = szBuf[str_len];
        if (ch == 0)
            break;
        if (ch == '\n' || ch == '\r') {
            // analyse next char for \r\n sequence
            ++str_len;
            ch = szBuf[str_len];
            if (ch == '\n' || ch == '\r') {} else {--str_len;}
            break;
        }
        *str += ch;
    }

    if (str_len == 0) {
        return false;
    }
    io_st = sock.Read(szBuf, str_len + 1);
    if (io_st != eIO_Success) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Cannot read string");
    }
    return true;
}


void CNetCacheServer::GenerateRequestId(const SNC_Request& req, 
                                        string*            id_str,
                                        unsigned int*      transaction_id)
{
    long id;
    {{
    CFastMutexGuard guard(x_NetCacheMutex);
    id = ++m_MaxId;
    }}
    *transaction_id = id;

    CNetCache_Key::GenerateBlobKey(id_str, id, m_Host, GetPort());
}


bool CNetCacheServer::x_CheckBlobId(CSocket&       sock,
                                    CNetCache_Key* blob_id, 
                                    const string&  blob_key)
{
    try {
        //CNetCache_ParseBlobKey(blob_id, blob_key);
        if (blob_id->version != 1     ||
            blob_id->hostname.empty() ||
            blob_id->id == 0          ||
            blob_id->port == 0) 
        {
            NCBI_THROW(CNetCacheException, eKeyFormatError, kEmptyStr);
        }
    } 
    catch (CNetCacheException& )
    {
        WriteMsg(sock, "ERR:", "BLOB id format error.");
        return false;
    }
    return true;
}

void CNetCacheServer::ProcessLog(CSocket&  sock, const SNC_Request&  req)
{
    const char* str = req.req_id.c_str();
    if (NStr::strcasecmp(str, "ON")==0) {
        CFastMutexGuard guard(x_NetCacheMutex);
        if (!m_LogFlag) {
            m_LogFlag = true;
            m_Logger->Rotate();
        }
    }
    if (NStr::strcasecmp(str, "OFF")==0) {
        CFastMutexGuard guard(x_NetCacheMutex);
        m_LogFlag = false;
    }
    WriteMsg(sock, "OK:", "");
}


CNetCache_Logger* CNetCacheServer::GetLogger()
{
    CFastMutexGuard guard(x_NetCacheMutex);

    return m_Logger.get();
}

bool CNetCacheServer::IsLog() const
{
    CFastMutexGuard guard(x_NetCacheMutex);
    return m_LogFlag;
}

void CNetCacheServer::x_CreateLog()
{
    CFastMutexGuard guard(x_NetCacheMutex);

    if (m_Logger.get()) {
        return; // nothing to do
    }

    string log_path = 
            m_Reg.GetString("server", "log_path", kEmptyStr, 
                            CNcbiRegistry::eReturn);
    log_path = CDirEntry::AddTrailingPathSeparator(log_path);
    log_path += "netcached.log";

    m_Logger.reset(
        new CNetCache_Logger(log_path, 100 * 1024 * 1024));
}

SNC_ThreadData* CNetCacheServer::x_GetThreadData()
{
    SNC_ThreadData* tdata = s_tls->GetValue();
    if (tdata) {
    } else {
        tdata = new SNC_ThreadData(GetTLS_Size());
        s_tls->SetValue(tdata, TlsCleanup);
    }
    return tdata;
}

static
SBDB_CacheUnitStatistics::EErrGetPut s_GetStatType(ENC_RequestType   req_type)
{
    SBDB_CacheUnitStatistics::EErrGetPut op;
    switch (req_type) {
    case ePut:
    case ePut2:
        op = SBDB_CacheUnitStatistics::eErr_Put;
        break;
    case eGet:
        op = SBDB_CacheUnitStatistics::eErr_Get;
        break;
    default:
        op = SBDB_CacheUnitStatistics::eErr_Unknown;
        break;
    }
    return op;
}

void CNetCacheServer::x_RegisterProtocolErr(
                               ENC_RequestType   req_type,
                               const string&     auth)
{
    CBDB_Cache* bdb_cache = dynamic_cast<CBDB_Cache*>(m_Cache);
    if (!bdb_cache) {
        return;
    }
    SBDB_CacheUnitStatistics::EErrGetPut op = s_GetStatType(req_type);
    bdb_cache->RegisterProtocolError(op, auth);
}

void CNetCacheServer::x_RegisterInternalErr(
                               ENC_RequestType   req_type,
                               const string&     auth)
{
    CBDB_Cache* bdb_cache = dynamic_cast<CBDB_Cache*>(m_Cache);
    if (!bdb_cache) {
        return;
    }
    SBDB_CacheUnitStatistics::EErrGetPut op = s_GetStatType(req_type);
    bdb_cache->RegisterInternalError(op, auth);
}

void CNetCacheServer::x_RegisterNoBlobErr(ENC_RequestType   req_type,
                                          const string&     auth)
{
    CBDB_Cache* bdb_cache = dynamic_cast<CBDB_Cache*>(m_Cache);
    if (!bdb_cache) {
        return;
    }
    SBDB_CacheUnitStatistics::EErrGetPut op = s_GetStatType(req_type);
    bdb_cache->RegisterNoBlobError(op, auth);
}

void CNetCacheServer::x_RegisterException(ENC_RequestType           req_type,
                                          const string&             auth,
                                          const CNetServiceException& ex)
{
    CBDB_Cache* bdb_cache = dynamic_cast<CBDB_Cache*>(m_Cache);
    if (!bdb_cache) {
        return;
    }
    SBDB_CacheUnitStatistics::EErrGetPut op = s_GetStatType(req_type);
    switch (ex.GetErrCode()) {
    case CNetServiceException::eTimeout:
    case CNetServiceException::eCommunicationError:
        bdb_cache->RegisterCommError(op, auth);
        break;
    default:
        ERR_POST("Unknown err code in CNetCacheServer::x_RegisterException");
        bdb_cache->RegisterInternalError(op, auth);
        break;
    } // switch
}


void CNetCacheServer::x_RegisterException(ENC_RequestType           req_type,
                                          const string&             auth,
                                          const CNetCacheException& ex)
{
    CBDB_Cache* bdb_cache = dynamic_cast<CBDB_Cache*>(m_Cache);
    if (!bdb_cache) {
        return;
    }
    SBDB_CacheUnitStatistics::EErrGetPut op = s_GetStatType(req_type);

    switch (ex.GetErrCode()) {
    case CNetCacheException::eAuthenticationError:
    case CNetCacheException::eKeyFormatError:
        bdb_cache->RegisterProtocolError(op, auth);
        break;
    case CNetCacheException::eServerError:
        bdb_cache->RegisterInternalError(op, auth);
        break;
    default:
        ERR_POST("Unknown err code in CNetCacheServer::x_RegisterException");
        bdb_cache->RegisterInternalError(op, auth);
        break;
    } // switch
}

/// Read the registry for icache_XXXX entries
void CNetCacheServer::x_GetICacheNames(TLocalCacheMap* cache_map)
{
    string cache_name;
    list<string> sections;
    m_Reg.EnumerateSections(&sections);

    string tmp;
    ITERATE(list<string>, it, sections) {
        const string& sname = *it;
        NStr::SplitInTwo(sname, "_", tmp, cache_name);
        if (NStr::CompareNocase(tmp, "icache") != 0) {
            continue;
        }

        NStr::ToLower(cache_name);
        (*cache_map)[cache_name] = 0;

    } // ITERATE
}



///////////////////////////////////////////////////////////////////////

/// @internal
extern "C" 
void Threaded_Server_SignalHandler( int )
{
    if (s_netcache_server && 
        (!s_netcache_server->ShutdownRequested()) ) {
        s_netcache_server->SetShutdownFlag();
        LOG_POST("Interrupt signal. Shutdown requested.");
    }
}

///////////////////////////////////////////////////////////////////////


/// NetCache daemon application
///
/// @internal
///
class CNetCacheDApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
protected:
    void StopSessionMngThread();

private:
    CRef<CCacheCleanerThread>  m_PurgeThread;
    /// Error message logging
    auto_ptr<CNetCacheLogStream> m_ErrLog;
};

void CNetCacheDApp::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);
    SetDiagPostFlag(eDPF_DateTime);

    // Setup command line arguments and parameters
        
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "netcached");

    arg_desc->AddFlag("reinit", "Recreate the storage directory.");

    SetupArgDescriptions(arg_desc.release());
}


int CNetCacheDApp::Run(void)
{
    CBDB_Cache* bdb_cache = 0;
    CArgs args = GetArgs();

    LOG_POST(NETCACHED_VERSION);

    try {
        const CNcbiRegistry& reg = GetConfig();

        CConfig conf(reg);
        
#if defined(NCBI_OS_UNIX)
        bool is_daemon =
            reg.GetBool("server", "daemon", false, 0, CNcbiRegistry::eReturn);
        if (is_daemon) {
            LOG_POST("Entering UNIX daemon mode...");
            bool daemon = Daemonize(0, fDaemon_DontChroot);
            if (!daemon) {
                return 0;
            }
        }
        
        // attempt to get server gracefully shutdown on signal
        signal( SIGINT, Threaded_Server_SignalHandler);
        signal( SIGTERM, Threaded_Server_SignalHandler);
#endif
        int port = 
            reg.GetInt("server", "port", 9000, 0, CNcbiRegistry::eReturn);
        bool is_port_free = true;

        // try server port to check if it is busy
        {{
        CListeningSocket lsock(port);
        if (lsock.GetStatus() != eIO_Success) {
            is_port_free = false;
            ERR_POST(("Startup problem: listening port is busy. Port=" + 
                                    NStr::IntToString(port)));
        }
        }}

        string log_path = 
                reg.GetString("server", "log_path", kEmptyStr, 
                              CNcbiRegistry::eReturn);
        log_path = CDirEntry::AddTrailingPathSeparator(log_path);
        log_path += "nc_err.log";
        m_ErrLog.reset(new CNetCacheLogStream(log_path, 25 * 1024 * 1024));
        // All errors redirected to rotated log
        // from this moment on the server is silent...
        SetDiagStream(m_ErrLog.get());

        if (!is_port_free) {
            LOG_POST(("Startup problem: listening port is busy. Port=" + 
                NStr::IntToString(port)));
            return 1;
        }



        // Mount BDB Cache

        const CConfig::TParamTree* param_tree = conf.GetTree();
        const TPluginManagerParamTree* bdb_tree = 
            param_tree->FindSubNode(kBDBCacheDriverName);


        auto_ptr<ICache> cache;

        CPluginManager<ICache> pm_cache;
        pm_cache.RegisterWithEntryPoint(NCBI_EntryPoint_xcache_bdb);

        if (bdb_tree) {

            CConfig bdb_conf((CConfig::TParamTree*)bdb_tree, eNoOwnership);
            const string& db_path = 
                bdb_conf.GetString("netcached", "path", 
                                    CConfig::eErr_Throw, kEmptyStr);
            bool reinit =
            reg.GetBool("server", "reinit", false, 0, CNcbiRegistry::eReturn);

            if (args["reinit"] || reinit) {  // Drop the database directory
                LOG_POST(("Removing BDB database directory " + db_path));
                CDir dir(db_path);
                dir.Remove();
            }
            

            LOG_POST("Initializing BDB cache");
            ICache* ic;

            try {
                ic = 
                    pm_cache.CreateInstance(
                        kBDBCacheDriverName,
                        CNetCacheServer::TCachePM::GetDefaultDrvVers(),
                        bdb_tree);
            } 
            catch (CBDB_Exception& ex)
            {
                bool drop_db = reg.GetBool("server", "drop_db", 
                                           true, 0, CNcbiRegistry::eReturn);
                if (drop_db) {
                    LOG_POST("Error initializing BDB ICache interface.");
                    LOG_POST(ex.what());
                    LOG_POST("Database directory will be droppped.");

                    CDir dir(db_path);
                    dir.Remove();

                    ic = 
                      pm_cache.CreateInstance(
                        kBDBCacheDriverName,
                        CNetCacheServer::TCachePM::GetDefaultDrvVers(),
                        bdb_tree);

                } else {
                    throw;
                }
            }

            bdb_cache = dynamic_cast<CBDB_Cache*>(ic);
            cache.reset(ic);


        } else {
            string msg = 
                "Configuration error. Cannot init storage. Driver name:";
            msg += kBDBCacheDriverName;
            ERR_POST(msg);
            return 1;
        }

        // cache storage media has been created

        if (!cache->IsOpen()) {
            ERR_POST("Configuration error. Cache not open.");
            return 1;
        }

        // start threaded server


        unsigned max_threads =
            reg.GetInt("server", "max_threads", 25, 0, CNcbiRegistry::eReturn);
        unsigned init_threads =
            reg.GetInt("server", "init_threads", 5, 0, CNcbiRegistry::eReturn);
        unsigned network_timeout =
            reg.GetInt("server", "network_timeout", 10, 0, CNcbiRegistry::eReturn);
        if (network_timeout == 0) {
            LOG_POST(Warning << 
                "INI file sets 0 sec. network timeout. Assume 10 seconds.");
            network_timeout =  10;
        } else {
            LOG_POST("Network IO timeout " << network_timeout);
        }
        bool use_hostname =
            reg.GetBool("server", "use_hostname", false, 0, CNcbiRegistry::eReturn);
        bool is_log =
            reg.GetBool("server", "log", false, 0, CNcbiRegistry::eReturn);
        unsigned tls_size =
            reg.GetInt("server", "tls_size", 64 * 1024, 0, CNcbiRegistry::eReturn);

        auto_ptr<CNetCacheServer> thr_srv(
            new CNetCacheServer(port,
                                use_hostname,
                                bdb_cache,
                                max_threads,
                                init_threads,
                                network_timeout,
                                is_log,
                                reg));

        thr_srv->SetTLS_Size(tls_size);

        if (bdb_cache) {
            bdb_cache->SetMonitor(&thr_srv->GetMonitor());
        }

        // create ICache instances

        thr_srv->MountICache(conf, pm_cache);

        // Start session management

        {{
            bool session_mng = 
                reg.GetBool("server", "session_mng", 
                            false, 0, IRegistry::eReturn);
            if (session_mng) {
                unsigned session_shutdown_timeout =
                    reg.GetInt("server", "session_shutdown_timeout", 
                               60, 0, IRegistry::eReturn);

                thr_srv->StartSessionManagement(session_shutdown_timeout);
            }
        }}

        NcbiCerr << "Running server on port " << port << NcbiEndl;
        LOG_POST(Info << "Running server on port " << port);
        thr_srv->Run();

        LOG_POST(Info << "Server stopped. Closing storage.");
        bdb_cache->Close();
    }
    catch (CBDB_ErrnoException& ex)
    {
        NcbiCerr << "Error: DBD errno exception:" << ex.what();
        return 1;
    }
    catch (CBDB_LibException& ex)
    {
        NcbiCerr << "Error: DBD library exception:" << ex.what();
        return 1;
    }

    return 0;
}

int main(int argc, const char* argv[])
{
    return CNetCacheDApp().AppMain(argc, argv, 0, eDS_Default);
}
