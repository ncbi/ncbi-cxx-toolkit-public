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
 * File Description: Network split storage for bit-vectors
 *
 */
#include <ncbi_pch.hpp>
#include <stdio.h>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_config.hpp>
#include <corelib/ncbimtx.hpp>

#include <connect/services/netservice_client.hpp>

#include <util/thread_nonstop.hpp>


#if defined(NCBI_OS_UNIX)
# include <corelib/ncbi_os_unix.hpp>
# include <signal.h>
#endif

#include "netbvstore.hpp"

#define NETBVSTORE_VERSION \
      "NCBI NetBVStore server version=0.0.1  " __DATE__ " " __TIME__


USING_NCBI_SCOPE;



static CNetBVSServer* s_netbvs_server = 0;


///@internal
static
void TlsCleanup(SBVS_ThreadData* p_value, void* /* data */ )
{
    delete p_value;
}

/// @internal
static
CRef< CTls<SBVS_ThreadData> > s_tls(new CTls<SBVS_ThreadData>);


CNetBVSServer::CNetBVSServer(unsigned int     port,
                             unsigned         max_threads,
                             unsigned         init_threads,
                             unsigned         network_timeout,
                             const IRegistry& reg)
: CThreadedServer(port),
    m_Shutdown(false),
    m_InactivityTimeout(network_timeout),
    m_Reg(reg)
{
    char hostname[256];
    int status = SOCK_gethostname(hostname, sizeof(hostname));
    if (status == 0) {
        m_Host = hostname;
    }

    m_MaxThreads = max_threads ? max_threads : 25;
    m_InitThreads = init_threads ? 
        (init_threads < m_MaxThreads ? init_threads : 2)  : 10;

    s_netbvs_server = this;
    m_AcceptTimeout = &m_ThrdSrvAcceptTimeout;
}



CNetBVSServer::~CNetBVSServer()
{
    try {
        NON_CONST_ITERATE(TBlobStoreMap, it, m_BVS_Map) {
            //TBlobStoreMap::value_type bvs = 
                delete it->second; it->second = 0;
            //delete bvs; it->bvs = 0;
        }
    } catch (...) {
        ERR_POST("Exception in ~CNetBVSServer()");
        _ASSERT(0);
    }
}


void CNetBVSServer::SetParams()
{
    m_ThrdSrvAcceptTimeout.sec = 1;
    m_ThrdSrvAcceptTimeout.usec = 0;
}


CNetBVSServer::TBlobStore* 
CNetBVSServer::GetBVS(const string& bvs_name)
{
    CFastMutexGuard guard(m_BVS_Map_Lock);

    TBlobStoreMap::iterator it(m_BVS_Map.find(bvs_name));
    if (it == m_BVS_Map.end()) {
        return 0;
    }
    return it->second;
}


void CNetBVSServer::ProcessOverflow(SOCK sock) 
{
    const char* msg = "ERR:Server busy";
    
    size_t n_written;
    SOCK_Write(sock, (void *)msg, 16, &n_written, eIO_WritePlain);
    SOCK_Close(sock); 
    ERR_POST("ProcessOverflow! Server is busy.");
}



/// @internal
bool CNetBVSServer::WaitForReadSocket(CSocket& sock, unsigned time_to_wait)
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


void CNetBVSServer::Process(CSocket&               socket,
                            EBVS_RequestType&      req_type,
                            SBVS_Request&          req,
                            SBVS_ThreadData&       tdata,
                            TBlobStore*            bvs)
{
    ParseRequest(tdata.request, &(tdata.req));

    switch ((req_type = tdata.req.req_type)) {
    case eGet:
        ProcessGet(socket, tdata.req, tdata, bvs);
        break;
    case eShutdown:
        ProcessShutdown();
        break;
    case eVersion:
        ProcessVersion(socket, tdata.req);
        break;
    case eError:
        WriteMsg(socket, "ERR:", tdata.req.err_msg);
        break;
    default:
        _ASSERT(0);
    } // switch

}


void CNetBVSServer::Process(SOCK sock)
{
    SBVS_ThreadData* tdata = x_GetThreadData();
    EBVS_RequestType bvs_req_type = eError;

    CSocket socket;
    socket.Reset(sock, eTakeOwnership, eCopyTimeoutsFromSOCK);

    try {
        tdata->auth.erase();
     
        // Set socket parameters

        //socket.DisableOSSendDelay();
        STimeout to = {m_InactivityTimeout, 0};
        socket.SetTimeout(eIO_ReadWrite , &to);

        WaitForReadSocket(socket, m_InactivityTimeout);

        if (ReadStr(socket, &(tdata->auth))) {
            
            WaitForReadSocket(socket, m_InactivityTimeout);

            if (ReadStr(socket, &(tdata->req.store_name))) {

                // check if store exists

                TBlobStore* bvs = GetBVS(tdata->req.store_name);
                if (bvs == 0) {
                    WriteMsg(socket, "ERR:", "Store not found");
                    return;                    
                }

                while (true) {
                    size_t n_read;
                    EIO_Status io_st;
                    io_st = socket.ReadLine(tdata->request,
                                            sizeof(tdata->request),
                                            &n_read);
                    if (io_st == eIO_Closed || io_st == eIO_Timeout) {
                        return;
                    }
                    if (n_read < 2) { 
                        WriteMsg(socket, "ERR:", "Invalid request");
                        return;
                    }

                    // check if it is NC or IC
                    const char* rq = tdata->request;
                    if (rq[0] == 'A' && rq[1] == '?') {  // Alive?
                        WriteMsg(socket, "OK:", "");
                    } else
                    if (rq[0] == 'O' && rq[1] == 'K') {
                        continue;
                    } else {
                        tdata->req.Init();
                        Process(socket, 
                                bvs_req_type, tdata->req, *tdata, bvs);
                    }
                } // while
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
        socket.SetTimeout(eIO_Read, &to);
        socket.Read(NULL, 1024);

    } 
/*
    catch (CNetCacheException &ex)
    {
        ERR_POST("NC Server error: " << ex.what() 
                 << " client="       << tdata->auth
                 << " request='"     << tdata->request << "'"
                 << " peer="         << socket.GetPeerAddress()
                 << " blobsize="     << stat.blob_size
                 << " io blocks="    << stat.io_blocks
                 );
        x_RegisterException(nc_req_type, tdata->auth, ex);
    }
*/
    catch (CNetServiceException& ex)
    {
        ERR_POST("Server error: " << ex.what() 
                 << " client="    << tdata->auth
                 << " request='"  << tdata->request << "'"
                 << " peer="      << socket.GetPeerAddress()
                 );
    }
    catch (exception& ex)
    {
        ERR_POST("Execution error in command "
                 << ex.what() << " client=" << tdata->auth
                 << " request='" << tdata->request << "'"
                 << " peer="     << socket.GetPeerAddress()
                 );
    }
}

void CNetBVSServer::ProcessShutdown()
{    
    SetShutdownFlag();
}

void CNetBVSServer::ProcessVersion(CSocket& sock, const SBVS_Request& req)
{
    WriteMsg(sock, "OK:", NETBVSTORE_VERSION); 
}


void CNetBVSServer::x_WriteBuf(CSocket& sock,
                               const CBDB_RawFile::TBuffer::value_type* buf,
                               size_t                                   bytes)
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


struct SBVS_Commands
{
    unsigned  BGET;

    SBVS_Commands()
    {
        ::memcpy(&BGET, "BGET", 4);
    }

    /// 4 byte command comparison (make sure str is aligned)
    /// otherwise kaaboom on some platforms
    static
    bool IsCmd(unsigned cmd, const char* str) 
    {
        return cmd == *(unsigned*)str;
    }
};

/// @internal
static SBVS_Commands  s_BVS_cmd_codes;


#define NS_RETURN_ERROR(err) \
    { req->req_type = eError; req->err_msg = err; return; }
#define NS_SKIPSPACE(x)  \
    while (*x && (*x == ' ' || *x == '\t')) { ++x; }
#define NS_CHECKEND(x, msg) \
    if (!*s) { req->req_type = eError; req->err_msg = msg; return; }
#define NS_CHECKSIZE(size, max_size) \
    if (unsigned(size) >= unsigned(max_size)) \
        { req->req_type = eError; req->err_msg = "Message too long"; return; }
#define NS_GETSTRING(x, str) \
    for (;*x && !(*x == ' ' || *x == '\t'); ++x) { str.push_back(*x); }
#define NS_SKIPNUM(x)  \
    for (; *x && isdigit((unsigned char)(*x)); ++x) {}
#define NS_RETEND(x) \
    if (!*x) return;


void CNetBVSServer::ParseRequest(const char* reqstr, SBVS_Request* req)
{
    // Request formats and types:
    //
    // 1. BGET id range_from range_to
    //
    const char* s = reqstr;

    switch (*s) {
    case 'B':
        if (SBVS_Commands::IsCmd(s_BVS_cmd_codes.BGET, s)) {
            s += 4;
            req->req_type = eGet;
            NS_SKIPSPACE(s)
            NS_CHECKEND(s, "Misformed BGET request")
            req->id = (unsigned)atoi(s);
            NS_SKIPNUM(s)
            NS_SKIPSPACE(s)
            req->range_from = (unsigned)atoi(s);
            NS_SKIPNUM(s)
            NS_SKIPSPACE(s)
            req->range_to = (unsigned)atoi(s);
            return;
        }
        break;
    default:
        req->req_type = eError;
        req->err_msg = "Unknown request";
        return;
    } // switch
    req->req_type = eError;
    req->err_msg = "Unknown request";
}


void CNetBVSServer::ProcessGet(CSocket&               sock,
                               const SBVS_Request&    req,
                               SBVS_ThreadData&       tdata,
                               TBlobStore*            bvs)
{
    _ASSERT(bvs);

    size_t buf_size;
    EBDB_ErrCode err = bvs->ReadRealloc(req.id, tdata.buf, &buf_size);
    if (err != eBDB_Ok) {
        string msg = "BLOB not found. ";
        WriteMsg(sock, "ERR:", msg);
        return;
    }

    string msg("BLOB found. SIZE=");
    string sz;
    NStr::UIntToString(sz, buf_size);
    msg += sz;
    WriteMsg(sock, "OK:", msg);

    x_WriteBuf(sock, &(tdata.buf[0]), buf_size);

}





void CNetBVSServer::WriteMsg(CSocket&       sock, 
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

void CNetBVSServer::MountBVStore(const string& data_path,
                                 const string& storage_name, 
                                 unsigned      cache_size)
{
    auto_ptr<TBlobStore> bvs(new TBlobStore(new CBDB_BlobDeMux)); 
    if (cache_size) {
        bvs->SetVolumeCacheSize(cache_size);
    }
    string fn = CDir::ConcatPath(data_path, storage_name);

    bvs->Open(fn, CBDB_RawFile::eReadOnly);
    CFastMutexGuard guard(m_BVS_Map_Lock);
    m_BVS_Map[storage_name] = bvs.release();
}


bool CNetBVSServer::ReadStr(CSocket& sock, string* str)
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


SBVS_ThreadData* CNetBVSServer::x_GetThreadData()
{
    SBVS_ThreadData* tdata = s_tls->GetValue();
    if (tdata) {
    } else {
        tdata = new SBVS_ThreadData(10000);
        s_tls->SetValue(tdata, TlsCleanup);
    }
    return tdata;
}



///////////////////////////////////////////////////////////////////////

/// @internal
extern "C" 
void Threaded_Server_SignalHandler( int )
{
    if (s_netbvs_server && 
        (!s_netbvs_server->ShutdownRequested()) ) {
        s_netbvs_server->SetShutdownFlag();
        LOG_POST("Interrupt signal. Shutdown requested.");
    }
}

///////////////////////////////////////////////////////////////////////


/// NetBVS daemon application
///
/// @internal
///
class CNetBVStoreDApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
protected:
    /// Error message logging
    auto_ptr<CNetBVSLogStream> m_ErrLog;
};

void CNetBVStoreDApp::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);
    SetDiagPostFlag(eDPF_DateTime);

    // Setup command line arguments and parameters
        
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "netbvstore");

    //arg_desc->AddFlag("reinit", "Recreate the storage directory.");
    arg_desc->AddOptionalKey("bvsname", "bvsname", 
        "Name and path of storage (path=name) ",
                             CArgDescriptions::eString);

    SetupArgDescriptions(arg_desc.release());
}


int CNetBVStoreDApp::Run(void)
{
    CArgs args = GetArgs();

    LOG_POST(NETBVSTORE_VERSION);

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
        string log_path = 
                reg.GetString("server", "log_path", kEmptyStr, 
                              CNcbiRegistry::eReturn);
        log_path = CDirEntry::AddTrailingPathSeparator(log_path);
        log_path += "bvs_err.log";
        m_ErrLog.reset(new CNetBVSLogStream(log_path, 25 * 1024 * 1024));
        // All errors redirected to rotated log
        // from this moment on the server is silent...
        SetDiagStream(m_ErrLog.get());

        // start threaded server

        int port = 
            reg.GetInt("server", "port", 9000, 0, CNcbiRegistry::eReturn);

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

        bool is_log =
            reg.GetBool("server", "log", false, 0, CNcbiRegistry::eReturn);
        unsigned tls_size =
            reg.GetInt("server", "tls_size", 64 * 1024, 0, CNcbiRegistry::eReturn);

        auto_ptr<CNetBVSServer> thr_srv(
            new CNetBVSServer(port, 
                              max_threads, 
                              init_threads,
                              network_timeout,
                              reg));

        if (args["bvsname"]) {
            string bvsname = args["bvsname"].AsString();
            string spath, sname;
            bool split = NStr::SplitInTwo(bvsname, "=", spath, sname);
            if (split) {
                thr_srv->MountBVStore(spath, sname, 10 * 1024 * 1024);
            } else {
                thr_srv->MountBVStore("", bvsname, 10 * 1024 * 1024);
            }
        }

        NcbiCerr << "Running server on port " << port << NcbiEndl;
        LOG_POST(Info << "Running server on port " << port);
        thr_srv->Run();
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
    return CNetBVStoreDApp().AppMain(argc, argv, 0, eDS_Default);
}

/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2006/11/30 14:23:50  dicuccio
 * Update to use buffer typedefs from CBDB_RawFile
 *
 * Revision 1.3  2006/06/07 15:48:09  kuznets
 * commented out socket send delay
 *
 * Revision 1.2  2006/06/06 16:12:56  kuznets
 * minor fix
 *
 * Revision 1.1  2006/06/02 12:44:55  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
