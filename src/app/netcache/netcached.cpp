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

const unsigned int kNetCacheBufSize = 2048;
const unsigned int kObjectTimeout = 60 * 60; ///< Default timeout in seconds

DEFINE_STATIC_FAST_MUTEX(x_NetCacheMutex);


/// Netcache threaded server
///
/// @internal
class CNetCacheServer : public CThreadedServer
{
public:
    CNetCacheServer(unsigned int port,
                    ICache*      cache) 
        : CThreadedServer(port),
          m_MaxId(0),
          m_Cache(cache),
          m_Shutdown(false)
    {
        m_MaxThreads = 25;
        m_InitThreads = 10;
        m_QueueSize = 100;
    }

    virtual ~CNetCacheServer() {}

    /// Take request code from the socket and process the request
    virtual void  Process(SOCK sock);
    virtual bool ShutdownRequested(void) { return m_Shutdown; }

private:
    typedef enum {
        eError,
        ePut,
        eGet,
        eShutdown
    } ERequestType;

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
    unsigned        m_MaxId;
    bm::bvector<>   m_UsedIds;   ///< Set of ids in use
    ICache*         m_Cache;
//    char*           m_Buf;       ///< Temp buffer
//    size_t          m_BufLength; ///< Actual buffer length (in bytes)
    bool            m_Shutdown;  ///< Shutdown request
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
bool s_WaitForReadSocket(CSocket& sock)
{
    STimeout to = {10, 0};
    EIO_Status io_st = sock.Wait(eIO_Read, &to);
    switch (io_st) {
    case eIO_Success:
        return true;
    default:
        return false;
    }
    return false;
}        


void CNetCacheServer::Process(SOCK sock)
{
//LOG_POST("Connection...");

    try {
        CSocket socket;
        socket.Reset(sock, eTakeOwnership, eCopyTimeoutsFromSOCK);

        string auth, request;

        s_WaitForReadSocket(socket);

        if (ReadStr(socket, &auth)) {
//LOG_POST(Info << "Client=" << auth);
            
            s_WaitForReadSocket(socket);

            if (ReadStr(socket, &request)) {                
//LOG_POST(Info << "Request=" << request);
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
        STimeout to;
        to.sec = to.usec = 0;
        char buf[1024];
        socket.SetTimeout(eIO_Read, &to);
        size_t n_read;
        socket.Read(buf, sizeof(buf), &n_read);
    } 
    catch (exception& ex)
    {
cerr << "Exception in command processing!" << endl;
        ERR_POST(ex.what());
    }
//LOG_POST("Connection closed.");
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
    


//LOG_POST(Info << "GET request key=" << req_id);
    auto_ptr<IReader> rdr(m_Cache->GetReadStream(req_id, 0, kEmptyStr));
    if (!rdr.get()) {
blob_not_found:
        WriteMsg(sock, "ERR:", "BLOB not found.");
//LOG_POST(Info << "GET failed. BLOB not found." << req_id);

        return;
    }
    
    AutoPtr<char, ArrayDeleter<char> > buf(new char[kNetCacheBufSize+1]);

    bool read_flag = false;
    size_t bytes_read;
    do {
        ERW_Result io_res =
         rdr->Read(buf.get(), kNetCacheBufSize, &bytes_read);
        if (io_res == eRW_Success) {
            if (bytes_read) {
                if (!read_flag) {
                    read_flag = true;
                    WriteMsg(sock, "OK:", "BLOB found.");
                }
                // translate cache to the client
                size_t n_written;

                EIO_Status io_st = 
                  sock.Write(buf.get(), bytes_read, &n_written);
                if (io_st != eIO_Success) {
                    break;
                }
            } else {
                break;
            }
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

    AutoPtr<char, ArrayDeleter<char> > buf(new char[kNetCacheBufSize+1]);
    size_t buffer_length = 0;

    while (ReadBuffer(sock, buf.get(), &buffer_length)) {
        if (buffer_length) {

            size_t bytes_written;
            ERW_Result res = 
                iwrt->Write(buf.get(), buffer_length, &bytes_written);
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

    req->req_type = eError;
    req->err_msg = "Unknown request";
}

bool CNetCacheServer::ReadBuffer(CSocket& sock, char* buf, size_t* buffer_length)
{
    if (!s_WaitForReadSocket(sock)) {
        *buffer_length = 0;
        return true;
    }

    _ASSERT(m_Buf);
    EIO_Status io_st = sock.Read(buf, kNetCacheBufSize, buffer_length);
    switch (io_st) 
    {
    case eIO_Success:
        break;
    case eIO_Timeout:
        *buffer_length = 0;
        break;
    default: // invalid socket or request
        return false;
    };
    return true;
}

bool CNetCacheServer::ReadStr(CSocket& sock, string* str)
{
    _ASSERT(str);

    str->clear();
    char ch;
    EIO_Status io_st;
    size_t  bytes_read;
    unsigned loop_cnt = 0;

    do {
        do {
            io_st = sock.Read(&ch, 1, &bytes_read);
            switch (io_st) 
            {
            case eIO_Success:
                if (ch == 0 || ch == '\n' || ch == 13) {
                    goto out_of_loop;
                }
                *str += ch;
                break;
            case eIO_Timeout:
                // TODO: add repetition counter or another protector here
                break;
            default: // invalid socket or request, bailing out
                return false;
            };
        } while (true);
out_of_loop:
        if (++loop_cnt > 10) // protection from a client feeding empty strings
            return false;
    } while (str->empty());

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

    char hostname[256];
    int status = SOCK_gethostname(hostname, sizeof(hostname));
    if (status == 0) {
        *id_str += "_";
        *id_str += hostname;
    }

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
//    SetDiagPostLevel(eDiag_Warning);
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

        m_PurgeThread.Reset(new CCacheCleanerThread(cache.get(), 30, 5));
        LOG_POST(Info << "Running server on port " << port);
        m_PurgeThread->Run();

        CNetCacheServer* thr_srv = new CNetCacheServer(port, cache.get());
        thr_srv->Run();

        LOG_POST(Info << "Stopping cache maintanace thread...");
        if (bdb_cache) {
            bdb_cache->SetBatchSleep(0);
            bdb_cache->StopPurge();
        }
        m_PurgeThread->RequestStop();
        m_PurgeThread->Join();
        LOG_POST(Info << "Stopped.");
        
        delete thr_srv;

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
