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
#include <corelib/plugin_manager.hpp>

#include <util/cache/icache.hpp>
#include <bdb/bdb_blobcache.hpp>

#include <connect/threaded_server.hpp>
#include <connect/ncbi_socket.hpp>

USING_NCBI_SCOPE;

const unsigned int kNetCacheBufSize = 2048;
const unsigned int kObjectTimeout = 60 * 60; ///< Default timeout in seconds

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
          m_BufLength(0)
    {
        m_Buf = new char[kNetCacheBufSize+1];
    }

    ~CNetCacheServer()
    {
        delete [] m_Buf;
    }

    /// Take request code from the socket and process the request
    virtual void  Process(SOCK sock);

private:
    typedef enum {
        eError,
        ePut,
        eGet
    } ERequestType;

    struct Request
    {
        ERequestType    req_type;
        unsigned int    timeout;
        string          req_id;
        string          err_msg;      
    };

    /// Process "PUT" request
    void ProcessPut(CSocket& sock, const Request& req);

    /// Returns FALSE when socket is closed or cannot be read
    bool ReadStr(CSocket& sock, string* str);

    /// Read buffer from the socket.
    bool ReadBuffer(CSocket& sock);

    void ParseRequest(const string& reqstr, Request* req);

    /// Reply to the client
    void WriteMsg(CSocket& sock, 
                  const string& prefix, const string& msg);

    /// Generate unique system-wide request id
    void GenerateRequestId(const Request& req, string* id_str);

private:
    unsigned   m_MaxId;
    ICache*    m_Cache;
    char*      m_Buf;       /// Temp buffer
    size_t     m_BufLength; ///< Actual buffer length (in bytes)
};



void CNetCacheServer::Process(SOCK sock)
{
    LOG_POST("Connection...");

    try {
        CSocket socket;
        socket.Reset(sock, eTakeOwnership, eCopyTimeoutsFromSOCK);

        string auth, request;

        if (ReadStr(socket, &auth)) {
            if (ReadStr(socket, &request)) {
                Request req;
                ParseRequest(request, &req);

                switch (req.req_type) {
                case ePut:
                    ProcessPut(socket, req);
                    break;
                case eGet:
                    break;
                case eError:
                    WriteMsg(socket, "ERR:", req.err_msg);
                    break;
                default:
                    _ASSERT(0);
                } // switch
            }
        }
    } 
    catch (exception& ex)
    {
        ERR_POST(ex.what());
    }
}

void CNetCacheServer::ProcessPut(CSocket& sock, const Request& req)
{
    string rid;
    GenerateRequestId(req, &rid);
    WriteMsg(sock, "ID:", rid);

    while (ReadBuffer(sock)) {
        if (m_BufLength) {
            // translate it to ICache
        }
    } // while
}


void CNetCacheServer::WriteMsg(CSocket&       sock, 
                               const string&  prefix, 
                               const string&  msg)
{
    size_t n_written;
    /* EIO_Status io_st = */
        sock.Write(prefix.c_str(), prefix.length(), &n_written);
    if (msg.length()) {
        /* EIO_Status io_st = */
            sock.Write(msg.c_str(), msg.length(), &n_written);
    }
    /* EIO_Status io_st = */
       sock.Write("\n", 1, &n_written);
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

    req->req_type = eError;
    req->err_msg = "Unknown request";
}

bool CNetCacheServer::ReadBuffer(CSocket& sock)
{
    _ASSERT(m_Buf);
    EIO_Status io_st = sock.Read(m_Buf, kNetCacheBufSize, &m_BufLength);
    switch (io_st) 
    {
    case eIO_Success:
        break;
    case eIO_Timeout:
        m_BufLength = 0;
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

    do {
        io_st = sock.Read(&ch, 1, &bytes_read);
        switch (io_st) 
        {
        case eIO_Success:
            if (ch == 0 || ch == '\n') { // end of AUTH
                return true;
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

    return true;

}

DEFINE_STATIC_FAST_MUTEX(x_NetCacheMutex);

void CNetCacheServer::GenerateRequestId(const Request& req, string* id_str)
{
    long id;
    string tmp;

    {{
    CFastMutexGuard guard(x_NetCacheMutex);
    id = ++m_MaxId;
    }}

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
};

void CNetCacheDApp::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);

    // Setup command line arguments and parameters
        
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "netcached");
}



int CNetCacheDApp::Run(void)
{
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

            cache.reset(ic);

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

        CNetCacheServer thr_srv(port, cache.get());
        thr_srv.Run();

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
