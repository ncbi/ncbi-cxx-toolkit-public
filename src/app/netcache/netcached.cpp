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

#include <util/thread_nonstop.hpp>
#include <util/bitset/ncbi_bitset.hpp>
#include <corelib/ncbimtx.hpp>

#include <util/cache/icache.hpp>
#include <util/cache/icache_clean_thread.hpp>
#include <bdb/bdb_blobcache.hpp>

#include <connect/threaded_server.hpp>
#include <connect/ncbi_socket.hpp>

USING_NCBI_SCOPE;

const unsigned int kNetCacheBufSize = 64 * 1024;
const unsigned int kObjectTimeout = 60 * 60; ///< Default timeout in seconds

DEFINE_STATIC_FAST_MUTEX(x_NetCacheMutex);

/// NetCache internal exception
///
/// @internal
class CNetCacheServerException : public CException
{
public:
    enum EErrCode {
        eTimeout,
        eCommunicationError,
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eTimeout:            return "eTimeout";
        case eCommunicationError: return "eCommunicationError";
        default:                  return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CNetCacheServerException, CException);
};



/// Netcache threaded server
///
/// @internal
class CNetCacheServer : public CThreadedServer
{
public:
    CNetCacheServer(unsigned int port,
                    ICache*      cache,
                    unsigned     max_threads,
                    unsigned     init_threads) 
        : CThreadedServer(port),
          m_MaxId(0),
          m_Cache(cache),
          m_Shutdown(false),
          m_InactivityTimeout(30)
    {
        char hostname[256];
        int status = SOCK_gethostname(hostname, sizeof(hostname));
        if (status == 0) {
            m_Host = hostname;
        }

        m_MaxThreads = max_threads ? max_threads : 25;
        m_InitThreads = init_threads ? 
            (init_threads < m_MaxThreads ? init_threads : 2)  : 10;
        m_QueueSize = m_MaxThreads + 2;
    }

    virtual ~CNetCacheServer() {}

    /// Take request code from the socket and process the request
    virtual void  Process(SOCK sock);
    virtual bool ShutdownRequested(void) { return m_Shutdown; }

    typedef enum {
        eError,
        ePut,
        eGet,
        eShutdown,
        eVersion
    } ERequestType;

private:
    struct Request
    {
        ERequestType    req_type;
        unsigned int    timeout;
        string          req_id;
        string          err_msg;      
    };

    /// Request id string can be parsed into this structure
    struct BlobId
    {
        string    prefix;
        unsigned  version;
        unsigned  id;
        string    hostname;
        unsigned  timeout;
    };

    /// Return TRUE if request parsed correctly
    bool ParseBlobId(const string& rid, BlobId* req_id);

    /// Process "PUT" request
    void ProcessPut(CSocket& sock, const Request& req);

    /// Process "GET" request
    void ProcessGet(CSocket& sock, const Request& req);

    /// Process "SHUTDOWN" request
    void ProcessShutdown(CSocket& sock, const Request& req);

    /// Process "VERSION" request
    void ProcessVersion(CSocket& sock, const Request& req);

    /// Returns FALSE when socket is closed or cannot be read
    bool ReadStr(CSocket& sock, string* str);

    /// Read buffer from the socket.
    bool ReadBuffer(CSocket& sock, char* buf, size_t* buffer_length);

    void ParseRequest(const string& reqstr, Request* req);

    /// Reply to the client
    void WriteMsg(CSocket& sock, 
                  const string& prefix, const string& msg);

    /// Generate unique system-wide request id
    void GenerateRequestId(const Request& req, 
                           string*        id_str,
                           unsigned int*  transaction_id);

    void WaitForId(unsigned int id);

private:
    /// Host name where server runs
    string          m_Host;
    /// ID counter
    unsigned        m_MaxId;
    /// Set of ids in use (PUT)
    bm::bvector<>   m_UsedIds;
    ICache*         m_Cache;
    /// Flags that server received a shutdown request
    bool            m_Shutdown; 
    /// Time to wait for the client (seconds)
    unsigned        m_InactivityTimeout;
};

/// @internal
class CIdBusyGuard
{
public:
    CIdBusyGuard(bm::bvector<>* id_set, unsigned int id)
        : m_IdSet(id_set), m_Id(id)
    {
        if (id) {
            CFastMutexGuard guard(x_NetCacheMutex);
            id_set->set(id);
        }
    }

    ~CIdBusyGuard()
    {
        Release();
    }

    void Release()
    {
        if (m_Id) {
            CFastMutexGuard guard(x_NetCacheMutex);
            m_IdSet->set(m_Id, false);
            m_Id = 0;
        }

    }

private:
    bm::bvector<>*   m_IdSet;
    unsigned int     m_Id;
};

/// @internal
static
bool s_WaitForReadSocket(CSocket& sock, unsigned time_to_wait)
{
    STimeout to = {time_to_wait, 0};
    EIO_Status io_st = sock.Wait(eIO_Read, &to);
    switch (io_st) {
    case eIO_Success:
        return true;
    case eIO_Timeout:
        NCBI_THROW(CNetCacheServerException, eTimeout, kEmptyStr);
    default:
        return false;
    }
    return false;
}        

/// Thread specific data for threaded server
///
/// @internal
///
struct ThreadData
{
    ThreadData() : buffer(new char[kNetCacheBufSize + 256]) {}
    AutoPtr<char, ArrayDeleter<char> >  buffer;
};

///@internal
static
void TlsCleanup(ThreadData* p_value, void* /* data */ )
{
    delete p_value;
}

static
CRef< CTls<ThreadData> > s_tls(new CTls<ThreadData>);


void CNetCacheServer::Process(SOCK sock)
{
    string auth, request;

    try {
        CSocket socket;
        socket.Reset(sock, eTakeOwnership, eCopyTimeoutsFromSOCK);

        // Set socket parameters

        socket.DisableOSSendDelay();
        STimeout to = {m_InactivityTimeout, 0};
        socket.SetTimeout(eIO_ReadWrite , &to);

        // Set thread local data (buffers, etc.)

        ThreadData* tdata = s_tls->GetValue();
        if (tdata) {
        } else {
            tdata = new ThreadData();
            s_tls->SetValue(tdata, TlsCleanup);
        }

        // Process request

        s_WaitForReadSocket(socket, m_InactivityTimeout);

        if (ReadStr(socket, &auth)) {
            
            s_WaitForReadSocket(socket, m_InactivityTimeout);

            if (ReadStr(socket, &request)) {                
                Request req;
                ParseRequest(request, &req);

                switch (req.req_type) {
                case ePut:
                    ProcessPut(socket, req);
                    break;
                case eGet:
                    ProcessGet(socket, req);
                    break;
                case eShutdown:
                    ProcessShutdown(socket, req);
                    break;
                case eVersion:
                    ProcessVersion(socket, req);
                    break;
                case eError:
                    WriteMsg(socket, "ERR:", req.err_msg);
                    break;
                default:
                    _ASSERT(0);
                } // switch
            }
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
        char buf[1024];
        socket.SetTimeout(eIO_Read, &to);
        size_t n_read;
        socket.Read(buf, sizeof(buf), &n_read);
    } 
    catch (CNetCacheServerException &ex)
    {
        LOG_POST("Server error: " << ex.what());
    }
    catch (exception& ex)
    {
        LOG_POST("Execution error in command " << request << " " << ex.what());
    }
}

void CNetCacheServer::ProcessShutdown(CSocket& sock, const Request& req)
{    
    m_Shutdown = true;
 
    // self reconnect to force the listening thread to rescan
    // shutdown flag
    unsigned port = GetPort();
    STimeout to;
    to.sec = 10; to.usec = 0;
    CSocket shut_sock("localhost", port, &to);    
}

void CNetCacheServer::ProcessVersion(CSocket& sock, const Request& req)
{
    WriteMsg(sock, "OK:", "NCBI NetCache server version=1.1");
}

void CNetCacheServer::ProcessGet(CSocket& sock, const Request& req)
{
    const string& req_id = req.req_id;

    if (req_id.empty()) {
        WriteMsg(sock, "ERR:", "BLOB id is empty.");
        return;
    }

    BlobId blob_id;
    bool res = ParseBlobId(req_id, &blob_id);
    if (!res) {
format_err:
        WriteMsg(sock, "ERR:", "BLOB id format error.");
        return;
    }

    if (blob_id.prefix != "NCID" || 
        blob_id.version != 1     ||
        blob_id.hostname.empty() ||
        blob_id.id == 0          ||
        blob_id.timeout == 0) 
    {
        goto format_err;
    }

    // check timeout

    CTime time_stamp(CTime::eCurrent);
    unsigned tm = (unsigned)time_stamp.GetTimeT();

    if (tm > blob_id.timeout) {
        WriteMsg(sock, "ERR:", "BLOB expired.");
        return;
    }

    WaitForId(blob_id.id);
    


    auto_ptr<IReader> rdr(m_Cache->GetReadStream(req_id, 0, kEmptyStr));
    if (!rdr.get()) {
blob_not_found:
        WriteMsg(sock, "ERR:", "BLOB not found.");
        return;
    }
    
    ThreadData* tdata = s_tls->GetValue();
    _ASSERT(tdata);
    char* buf = tdata->buffer.get();


    bool read_flag = false;
    size_t bytes_read;
    do {
        ERW_Result io_res = rdr->Read(buf, kNetCacheBufSize, &bytes_read);
        if (io_res == eRW_Success && bytes_read) {
            if (!read_flag) {
                read_flag = true;
                WriteMsg(sock, "OK:", "BLOB found.");
            }

            // translate BLOB fragment to the network
            do {
                size_t n_written;
                EIO_Status io_st;
                io_st = sock.Write(buf, bytes_read, &n_written);
                switch (io_st) {
                case eIO_Success:
                    break;
                case eIO_Timeout:
                    NCBI_THROW(CNetCacheServerException, 
                                    eTimeout, kEmptyStr);
                    break;
                default:
                    NCBI_THROW(CNetCacheServerException, 
                                    eCommunicationError, kEmptyStr);
                    break;
                } // switch
                bytes_read -= n_written;
            } while (bytes_read > 0);

        } else {
            break;
        }
    } while(1);

    if (!read_flag) {
        goto blob_not_found;
    }
}

void CNetCacheServer::ProcessPut(CSocket& sock, const Request& req)
{
    string rid;
    unsigned int transaction_id;
    GenerateRequestId(req, &rid, &transaction_id);

    CIdBusyGuard guard(&m_UsedIds, transaction_id);

    WriteMsg(sock, "ID:", rid);
    //LOG_POST(Info << "PUT request. Generated key=" << rid);
    auto_ptr<IWriter> 
        iwrt(m_Cache->GetWriteStream(rid, 0, kEmptyStr));

    // Reconfigure socket for no-timeout operation
    STimeout to;
    to.sec = to.usec = 0;
    sock.SetTimeout(eIO_Read, &to);

    ThreadData* tdata = s_tls->GetValue();
    _ASSERT(tdata);
    char* buf = tdata->buffer.get();

    size_t buffer_length = 0;

    while (ReadBuffer(sock, buf, &buffer_length)) {
        if (buffer_length) {
            size_t bytes_written;
            ERW_Result res = 
                iwrt->Write(buf, buffer_length, &bytes_written);
            if (res != eRW_Success) {
                WriteMsg(sock, "Err:", "Server I/O error");
                return;
            }
        }
    } // while

    iwrt->Flush();
    iwrt.reset(0);

}

bool CNetCacheServer::ParseBlobId(const string& rid, BlobId* req_id)
{
    _ASSERT(req_id);

    // NCID_01_1_DIDIMO_1096382642

    const char* ch = rid.c_str();
    req_id->hostname = req_id->prefix = kEmptyStr;

    // prefix

    for (;*ch && *ch != '_'; ++ch) {
        req_id->prefix += *ch;
    }
    if (*ch == 0)
        return false;
    ++ch;

    // version
    req_id->version = atoi(ch);
    while (*ch && *ch != '_') {
        ++ch;
    }
    if (*ch == 0)
        return false;
    ++ch;

    // id
    req_id->id = atoi(ch);
    while (*ch && *ch != '_') {
        ++ch;
    }
    if (*ch == 0)
        return false;
    ++ch;


    // hostname
    for (;*ch && *ch != '_'; ++ch) {
        req_id->hostname += *ch;
    }
    if (*ch == 0)
        return false;
    ++ch;

    // timeout
    req_id->timeout = atoi(ch);

    return true;
}

void CNetCacheServer::WaitForId(unsigned int id)
{
    // Spins in the loop while requested id is in the transaction list
    // (PUT request is still working)
    //
    while (true) {
        {{
        CFastMutexGuard guard(x_NetCacheMutex);
        if (!m_UsedIds[id]) {
            return;
        }
        }}
        SleepMilliSec(10);
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

void CNetCacheServer::ParseRequest(const string& reqstr, Request* req)
{
    const char* s = reqstr.c_str();

    if (strncmp(s, "PUT", 3) == 0) {

        req->req_type = ePut;
        req->timeout = 0;

        s += 3;
        while (*s && isspace(*s)) {
            ++s;
        }
        if (*s) {  // timeout value
            int time_out = atoi(s);
            if (time_out > 0) {
                req->timeout = time_out;
            }
        }
        return;
    } // PUT

    if (strncmp(s, "GET", 3) == 0) {

        req->req_type = eGet;

        s += 3;
        while (*s && isspace(*s)) {
            ++s;
        }
        if (*s) {  // requested id
            req->req_id = s;
        }
        return;
    } // PUT

    if (strncmp(s, "SHUTDOWN", 7) == 0) {
        req->req_type = eShutdown;
        return;
    } // PUT

    if (strncmp(s, "VERSION", 7) == 0) {
        req->req_type = eVersion;
        return;
    } // PUT


    req->req_type = eError;
    req->err_msg = "Unknown request";
}

bool CNetCacheServer::ReadBuffer(CSocket& sock, char* buf, size_t* buffer_length)
{
    if (!s_WaitForReadSocket(sock, m_InactivityTimeout)) {
        *buffer_length = 0;
        return true;
    }

    EIO_Status io_st = sock.Read(buf, kNetCacheBufSize, buffer_length);
    switch (io_st) 
    {
    case eIO_Success:
        break;
    case eIO_Timeout:
        *buffer_length = 0;
        NCBI_THROW(CNetCacheServerException, eTimeout, kEmptyStr);
        break;
    default: // invalid socket or request
        return false;
    };
    return true;
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
            NCBI_THROW(CNetCacheServerException, eTimeout, kEmptyStr);
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
    return true;
}


void CNetCacheServer::GenerateRequestId(const Request& req, 
                                        string*        id_str,
                                        unsigned int*  transaction_id)
{
    long id;
    string tmp;

    {{
    CFastMutexGuard guard(x_NetCacheMutex);
    id = ++m_MaxId;
    }}
    *transaction_id = id;

    *id_str = "NCID_01"; // NetCacheId prefix plus id version

    NStr::IntToString(tmp, id);
    *id_str += "_";
    *id_str += tmp;

    *id_str += "_";
    *id_str += m_Host;    

    CTime time_stamp(CTime::eCurrent);
    unsigned tm = (unsigned)time_stamp.GetTimeT();
    tm += req.timeout ? req.timeout : kObjectTimeout;
    NStr::IntToString(tmp, tm);

    *id_str += "_";
    *id_str += tmp;
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
private:
    CRef<CCacheCleanerThread>  m_PurgeThread;
};

void CNetCacheDApp::Init(void)
{
    SetDiagPostLevel(eDiag_Info);

    // Setup command line arguments and parameters
        
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "netcached");
}



int CNetCacheDApp::Run(void)
{
    CBDB_Cache* bdb_cache = 0;
    try {
        const CNcbiRegistry& reg = GetConfig();

        CConfig conf(reg);

        const CConfig::TParamTree* param_tree = conf.GetTree();
        const TPluginManagerParamTree* bdb_tree = 
            param_tree->FindSubNode(kBDBCacheDriverName);

        auto_ptr<ICache> cache;

        if (bdb_tree) {
            LOG_POST("Initializing BDB cache");

            typedef CPluginManager<ICache> TCachePM;
            CPluginManager<ICache> pm_cache;
            pm_cache.RegisterWithEntryPoint(NCBI_EntryPoint_ICache_bdbcache);

            ICache* ic = 
                pm_cache.CreateInstance(
                   kBDBCacheDriverName,
                   CVersionInfo(TCachePM::TInterfaceVersion::eMajor,
                                TCachePM::TInterfaceVersion::eMinor,
                                TCachePM::TInterfaceVersion::ePatchLevel), 
                                bdb_tree);

            bdb_cache = dynamic_cast<CBDB_Cache*>(ic);
            cache.reset(ic);

            if (bdb_cache) {
                bdb_cache->CleanLog();
            }

        } else {
            LOG_POST("Configuration error. Cannot init storage.");
            return 1;
        }

        // cache storage media has been created

        if (!cache->IsOpen()) {
            ERR_POST("Configuration error. Cache not open.");
            return 1;
        }

        int port = 
            reg.GetInt("server", "port", 9000, CNcbiRegistry::eReturn);

        bool purge_thread = 
            reg.GetBool("server", "run_purge_thread", true, CNcbiRegistry::eReturn);

        if (purge_thread) {
            LOG_POST(Info << "Starting cache cleaning thread.");
            m_PurgeThread.Reset(new CCacheCleanerThread(cache.get(), 30, 5));
            m_PurgeThread->Run();
        }

        unsigned max_threads =
            reg.GetInt("server", "max_threads", 25, CNcbiRegistry::eReturn);
        unsigned init_threads =
            reg.GetInt("server", "init_threads", 5, CNcbiRegistry::eReturn);

        auto_ptr<CNetCacheServer> thr_srv(
            new CNetCacheServer(port, cache.get(), max_threads, init_threads));

        LOG_POST(Info << "Running server on port " << port);
        thr_srv->Run();

        if (!m_PurgeThread.Empty()) {
            LOG_POST(Info << "Stopping cache cleaning thread...");
            if (bdb_cache) {
                bdb_cache->SetBatchSleep(0);
                bdb_cache->StopPurge();
            }

            m_PurgeThread->RequestStop();
            m_PurgeThread->Join();
            LOG_POST(Info << "Stopped.");
        }
        
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
    return CNetCacheDApp().AppMain(argc, argv, 0, eDS_Default, "netcached.ini");
}

/*
 * ===========================================================================
 * $Log$
 * Revision 1.16  2004/10/25 16:06:18  kuznets
 * Better timeout handling, use larger network bufers, VERSION command
 *
 * Revision 1.15  2004/10/21 17:21:42  kuznets
 * Reallocated buffer replaced with TLS data
 *
 * Revision 1.14  2004/10/21 15:51:21  kuznets
 * removed unused variable
 *
 * Revision 1.13  2004/10/21 15:06:10  kuznets
 * New parameter run_purge_thread
 *
 * Revision 1.12  2004/10/21 13:39:27  kuznets
 * New parameters max_threads, init_threads
 *
 * Revision 1.11  2004/10/20 21:21:02  ucko
 * Make CNetCacheServer::ERequestType public so that struct Request can
 * make use of it when building with WorkShop.
 *
 * Revision 1.10  2004/10/20 14:50:22  kuznets
 * Code cleanup
 *
 * Revision 1.9  2004/10/20 13:45:37  kuznets
 * Disabled TCP/IP delay on write
 *
 * Revision 1.8  2004/10/19 18:20:26  kuznets
 * Use ReadPeek to avoid net delays
 *
 * Revision 1.7  2004/10/19 15:53:36  kuznets
 * Code cleanup
 *
 * Revision 1.6  2004/10/18 13:46:57  kuznets
 * Removed common buffer (was shared between threads)
 *
 * Revision 1.5  2004/10/15 14:34:46  kuznets
 * Fine tuning of networking, cleaning thread interruption, etc
 *
 * Revision 1.4  2004/10/04 12:33:24  kuznets
 * Complete implementation of GET/PUT
 *
 * Revision 1.3  2004/09/23 16:37:21  kuznets
 * Reflected changes in ncbi_config.hpp
 *
 * Revision 1.2  2004/09/23 14:17:36  kuznets
 * Fixed compilation bug
 *
 * Revision 1.1  2004/09/23 13:57:01  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
