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
 * File Description: Network scheduler daemon
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
#include <util/bitset/ncbi_bitset.hpp>

#include <util/logrotate.hpp>

#include <connect/threaded_server.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/netschedule_client.hpp>

#include <bdb/bdb_expt.hpp>

#include "bdb_queue.hpp"

#if defined(NCBI_OS_UNIX)
# include <signal.h>
#endif



USING_NCBI_SCOPE;

//const unsigned int kObjectTimeout = 60 * 60; ///< Default timeout in seconds

/// General purpose NetSchedule Mutex
DEFINE_STATIC_FAST_MUTEX(x_NetScheduleMutex);


class CNetScheduleServer;
static CNetScheduleServer* s_netschedule_server = 0;

/// JS request types
/// @internal
typedef enum {
    eSubmitJob,
    eCancelJob,
    eStatusJob,
    eGetJob,
    ePutJobResult,
    eReturnJob,

    eShutdown,
    eVersion,
    eLogging,
    eStatistics,
    eQuitSession,
    eError
} EJS_RequestType;

/// Request context
///
/// @internal
struct SJS_Request
{
    EJS_RequestType    req_type;
    string             input;
    string             output;
    string             job_key_str;
    unsigned int       jcount;
    unsigned int       job_id;
    unsigned int       job_return_code;

    string             err_msg;

    void Init()
    {
        input.erase(); output.erase(); job_key_str.erase(); err_msg.erase();
        jcount = job_id = job_return_code = 0;
    }
};


/// Thread specific data for threaded server
///
/// @internal
///
struct SThreadData
{
    string      request;
    string      auth;
    string      queue;

    SJS_Request req;
};

///@internal
static
void TlsCleanup(SThreadData* p_value, void* /* data */ )
{
    delete p_value;
}

/// @internal
static
CRef< CTls<SThreadData> > s_tls(new CTls<SThreadData>);



/// NetScheduler threaded server
///
/// @internal
class CNetScheduleServer : public CThreadedServer
{
public:
    CNetScheduleServer(unsigned int    port,
                       CQueueDataBase* qdb,
                       unsigned        max_threads,
                       unsigned        init_threads,
                       unsigned        network_timeout,
                       bool            is_log); 

    virtual ~CNetScheduleServer();

    /// Take request code from the socket and process the request
    virtual void  Process(SOCK sock);
    virtual bool ShutdownRequested(void) { return m_Shutdown; }
        
    void SetShutdownFlag() { if (!m_Shutdown) m_Shutdown = true; }
    
    /// Override some parent parameters
    virtual void SetParams()
    {
        m_ThrdSrvAcceptTimeout.sec = 0;
        m_ThrdSrvAcceptTimeout.usec = 500;
        m_AcceptTimeout = &m_ThrdSrvAcceptTimeout;
    }

    void ProcessSubmit(CSocket& sock, SThreadData& tdata);

private:

    /// TRUE return means we have EOF in the socket (no more data is coming)
    bool ReadBuffer(CSocket& sock, 
                    char*    buf, 
                    size_t   buf_size,
                    size_t*  read_length);

    void ParseRequest(const string& reqstr, SJS_Request* req);

    /// Reply to the client
    void WriteMsg(CSocket&      sock, 
                  const string& prefix, 
                  const string& msg,
                  bool          comm_control = false);



private:
    /// Check if JOB id has the correct format
    bool x_CheckJobId(CSocket&          sock,
                       CNetSchedule_Key* job_id, 
                       const string&     job_key);

    void x_WriteBuf(CSocket& sock,
                    char*    buf,
                    size_t   bytes);

    /// Check if we have active thread data for this thread.
    /// Setup thread data if we don't.
    SThreadData* x_GetThreadData(); 

    void x_SetSocketParams(CSocket* sock);


//    void x_CreateLog();
private:
    /// Host name where server runs
    string             m_Host;
    /// ID counter
    //unsigned           m_MaxId;
//    bm::bvector<>      m_UsedIds;
    bool               m_Shutdown; 
    /// Time to wait for the client (seconds)
    unsigned           m_InactivityTimeout;
    
    /// Accept timeout for threaded server
    STimeout                        m_ThrdSrvAcceptTimeout;

    /// Job scheduler status tracker
    //CNetScheduler_JobStatusTracker  m_StatusTracker;

    CQueueDataBase*    m_QueueDB;
};

/// @internal
static
bool s_WaitForReadSocket(CSocket& sock, unsigned time_to_wait)
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



CNetScheduleServer::CNetScheduleServer(unsigned int    port,
                                       CQueueDataBase* qdb,
                                       unsigned        max_threads,
                                       unsigned        init_threads,
                                       unsigned        network_timeout,
                                       bool            /*is_log*/)
: CThreadedServer(port),
    m_Shutdown(false),
    m_InactivityTimeout(network_timeout)
{
    m_QueueDB = qdb;
    char hostname[256];
    int status = SOCK_gethostname(hostname, sizeof(hostname));
    if (status == 0) {
        m_Host = hostname;
    }

    m_MaxThreads = max_threads ? max_threads : 25;
    m_InitThreads = init_threads ? 
        (init_threads < m_MaxThreads ? init_threads : 2)  : 10;
    m_QueueSize = m_MaxThreads * 2;

    s_netschedule_server = this;
}

CNetScheduleServer::~CNetScheduleServer()
{
    delete m_QueueDB;
}

#define JS_CHECK_IO_STATUS(x) \
        switch (x)  { \
        case eIO_Success: \
            break; \
        default: \
            return; \
        }; 


void CNetScheduleServer::Process(SOCK sock)
{
    SThreadData* tdata = 0;
    EIO_Status io_st;

    try {
        CSocket socket;
        socket.Reset(sock, eTakeOwnership, eCopyTimeoutsFromSOCK);

        x_SetSocketParams(&socket);

        tdata = x_GetThreadData();

        // Process requests

        while (1) {
            s_WaitForReadSocket(socket, m_InactivityTimeout);
            io_st = socket.ReadLine(tdata->auth);
            JS_CHECK_IO_STATUS(io_st)

            io_st = socket.ReadLine(tdata->queue);
            JS_CHECK_IO_STATUS(io_st)

            if (tdata->queue.empty()) {  // TODO: check if queue exists.
                WriteMsg(socket, "ERR:", "Empty queue name ");
                return;
            }

            io_st = socket.ReadLine(tdata->request);
            JS_CHECK_IO_STATUS(io_st)

            SJS_Request& req = tdata->req;
            req.Init();
            ParseRequest(tdata->request, &req);

            if (req.req_type == eQuitSession) {
                break;
            }

            CNetSchedule_Key jkey;
            if (req.req_type != eSubmitJob ||
                req.req_type != eError) {

                CNetSchedule_ParseJobKey(&jkey, req.job_key_str);
            }

            
            switch (req.req_type) {
            case eSubmitJob:
                ProcessSubmit(socket, *tdata);
                break;
            case eCancelJob:
            case eStatusJob:
            case eGetJob:
            case ePutJobResult:
            case eReturnJob:
            case eShutdown:
            case eVersion:
            case eLogging:
            case eStatistics:
                WriteMsg(socket, "ERR:", "Not implemented.");
                break;
            case eError:
                WriteMsg(socket, "ERR:", tdata->req.err_msg);
                break;
            default:
                _ASSERT(0);
            } // switch
        } // while

        // read trailing input (see netcached.cpp for more comments on "why?")
        STimeout to; to.sec = to.usec = 0;
        char buf[1024];
        socket.SetTimeout(eIO_Read, &to);
        size_t n_read;
        socket.Read(buf, sizeof(buf), &n_read);

    } 
    catch (CNetScheduleException &ex)
    {
        ERR_POST("Server error: " << ex.what());
    }
    catch (CNetServiceException &ex)
    {
        ERR_POST("Server error: " << ex.what());
    }
    catch (CBDB_Exception &ex)
    {
        ERR_POST("BDB Server error: " << ex.what());
    }
    catch (exception& ex)
    {
        string msg = "Command execution error ";
        if (tdata) msg += tdata->request;
        msg += " ";
        msg += ex.what();
        ERR_POST(msg);
    }
}

void CNetScheduleServer::ProcessSubmit(CSocket& sock, SThreadData& tdata)
{

    SJS_Request& req = tdata.req;
//    GenerateJobId(&req.job_key_str, &req.job_id);

    _ASSERT(req.job_id != 0);
/*
    CNetSchedule_JS_Guard js_guard(m_StatusTracker, 
                                   req.job_id,
                                   CNetScheduler_JobStatusTracker::ePending);
*/

    WriteMsg(sock, "OK:", req.job_key_str);
}


void CNetScheduleServer::x_WriteBuf(CSocket& sock,
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
            NCBI_THROW(CNetServiceException, 
                       eTimeout, "Communication timeout error");
            break;
        default:
            NCBI_THROW(CNetServiceException, 
              eCommunicationError, "Communication error (cannot send data)");
            break;
        } // switch
        bytes -= n_written;
        buf += n_written;
    } while (bytes > 0);
}





#define NS_RETURN_ERROR(err) { req->req_type = eError; req->err_msg = err; return; }
#define NS_SKIPSPACE(x)  while (*x && isspace(*x)) { ++x; }
#define NS_CHECKEND(x, msg) if (!*s) { req->req_type = eError; req->err_msg = msg; }
#define NS_GETSTRING(x, str) for (;*x && !isspace(*x); ++x) { str.push_back(*x); }


void CNetScheduleServer::ParseRequest(const string& reqstr, SJS_Request* req)
{
    // Request formats and types:
    //
    // 1. SUBMIT "NCID_01_1..." 
    // 2. CANCEL JSID_01_1
    // 3. STATUS JSID_01_1
    // 4. GET Number_of_jobs
    // 5. PUT JSID_01_1 EndStatus "NCID_01_2..."
    // 6. RETURN JSID_01_1
    // 7. SHUTDOWN
    // 8. VERSION
    // 9. LOG [ON/OFF]
    // 10.STAT
    // 11.QUIT 

    const char* s = reqstr.c_str();

    if (strncmp(s, "SUBMIT", 6) == 0) {
        req->req_type = eSubmitJob;
        s += 6;
        NS_SKIPSPACE(s)
        if (*s !='"') {
            NS_RETURN_ERROR("Misformed SUBMIT request")
        }
        while (*s != '"') {
            NS_CHECKEND(s, "Misformed SUBMIT request")
            req->job_key_str += *s;
        }
        return;
    }
    if (strncmp(s, "CANCEL", 6) == 0) {
        req->req_type = eCancelJob;
        s += 6;
        NS_SKIPSPACE(s)
        NS_GETSTRING(s, req->job_key_str)
        
        if (req->job_key_str.empty()) {
            NS_RETURN_ERROR("Misformed CANCEL request")
        }
        return;
    }
    if (strncmp(s, "STATUS", 6) == 0) {
        req->req_type = eStatusJob;
        s += 6;
        NS_SKIPSPACE(s)
        NS_GETSTRING(s, req->job_key_str)
        
        if (req->job_key_str.empty()) {
            NS_RETURN_ERROR("Misformed STATUS request")
        }
        return;
    }

    if (strncmp(s, "GET", 3) == 0) {
        req->req_type = eGetJob;
        s += 3;
        NS_SKIPSPACE(s)

        if (*s) {
            int jobs = atoi(s);
            if (jobs > 0) {
                req->jcount = jobs;
            }
        }
        return;
    }

    if (strncmp(s, "PUT", 3) == 0) {
        req->req_type = ePutJobResult;
        s += 3;
        NS_SKIPSPACE(s)
        NS_GETSTRING(s, req->job_key_str)

        NS_SKIPSPACE(s)

        // return code
        if (*s) {
            req->job_return_code = atoi(s);
            // skip digits
            for (;isdigit(*s); ++s) {}            
            if (!isspace(*s)) {
                goto put_format_error;
            }
        } else {
        put_format_error:
            NS_RETURN_ERROR("Misformed PUT request. Missing job return code.")
        }

        // output information
        NS_SKIPSPACE(s)
        
        if (*s !='"') {
            NS_RETURN_ERROR("Misformed PUT request")
        }
        while (*s != '"') {
            NS_CHECKEND(s, "Misformed PUT request")
            req->output.push_back(*s);
        }

        return;
    }

    if (strncmp(s, "RETURN", 6) == 0) {
        req->req_type = eReturnJob;
        s += 6;
        NS_SKIPSPACE(s)

        NS_GETSTRING(s, req->job_key_str)

        return;
    }

    if (strncmp(s, "QUIT", 4) == 0) {
        req->req_type = eQuitSession;
        return;
    }

    req->req_type = eError;
    req->err_msg = "Unknown request";
}

bool CNetScheduleServer::ReadBuffer(CSocket& sock, 
                                    char*    buf, 
                                    size_t   buf_size,
                                    size_t*  read_length)
{
    *read_length = 0;
    size_t nn_read = 0;

    bool ret_flag = true;

    while (ret_flag) {
        if (!s_WaitForReadSocket(sock, m_InactivityTimeout)) {
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

void CNetScheduleServer::WriteMsg(CSocket&       sock, 
                                  const string&  prefix, 
                                  const string&  msg,
                                  bool           comm_control)
{
    string err_msg(prefix);
    err_msg.append(msg);
    err_msg.append("\r\n");

    size_t n_written;
    EIO_Status io_st = 
        sock.Write(err_msg.c_str(), err_msg.length(), &n_written);
    if (comm_control && io_st) {
        NCBI_THROW(CNetServiceException, 
                   eCommunicationError, "Socket write error.");
    }
}

/*
void CNetScheduleServer::GenerateJobId(string*            id_str,
                                       unsigned int*      transaction_id)
{

    unsigned int id;
    {{
    CFastMutexGuard guard(x_NetScheduleMutex);
    if (++m_MaxId == 0) {
        ++m_MaxId;
    }
    id = m_MaxId;
    }}
    *transaction_id = id;

    CNetSchedule_GenerateJobKey(id_str, id, m_Host, GetPort());
}
*/

bool CNetScheduleServer::x_CheckJobId(CSocket&          sock,
                                      CNetSchedule_Key* job_id, 
                                      const string&     job_key)
{
    try {
        CNetSchedule_ParseJobKey(job_id, job_key);
        if (job_id->version != 1     ||
            job_id->hostname.empty() ||
            job_id->id == 0          ||
            job_id->port == 0) 
        {
            NCBI_THROW(CNetScheduleException, eKeyFormatError, kEmptyStr);
        }
    } 
    catch (CNetScheduleException&)
    {
        string err = "JOB id format error. (";
        err += job_key;
        err += ")";
        WriteMsg(sock, "ERR:", err);
        return false;
    }
    return true;
}

SThreadData* CNetScheduleServer::x_GetThreadData()
{
    SThreadData* tdata = s_tls->GetValue();
    if (tdata) {
    } else {
        tdata = new SThreadData();
        s_tls->SetValue(tdata, TlsCleanup);
    }
    return tdata;
}

void CNetScheduleServer::x_SetSocketParams(CSocket* sock)
{
    sock->DisableOSSendDelay();
    STimeout to = {m_InactivityTimeout, 0};
    sock->SetTimeout(eIO_ReadWrite, &to);
}

///////////////////////////////////////////////////////////////////////


/// NetSchedule daemon application
///
/// @internal
///
class CNetScheduleDApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};

void CNetScheduleDApp::Init(void)
{
    SetDiagPostLevel(eDiag_Info);

    // Setup command line arguments and parameters
        
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "netscheduled");

    arg_desc->AddFlag("reinit", "Recreate the storage directory.");

    SetupArgDescriptions(arg_desc.release());
}



int CNetScheduleDApp::Run(void)
{
    CArgs args = GetArgs();

    try {
        const CNcbiRegistry& reg = GetConfig();

        CConfig conf(reg);

        const CConfig::TParamTree* param_tree = conf.GetTree();
        const TPluginManagerParamTree* bdb_tree = 
            param_tree->FindSubNode("bdb");

        if (!bdb_tree) {
            ERR_POST("BDB initialization section not found");
            return 1;
        }

        // Storage initialization

        
        CConfig bdb_conf((CConfig::TParamTree*)bdb_tree, eNoOwnership);
        const string& db_path = 
            bdb_conf.GetString("netschedule", "path", 
                                CConfig::eErr_Throw, kEmptyStr);

        if (args["reinit"]) {  // Drop the database directory
            CDir dir(db_path);
            dir.Remove();
        }

        unsigned mem_size = (unsigned)
            bdb_conf.GetDataSize("netschedule", "mem_size", 
                                 CConfig::eErr_NoThrow, 0);

        LOG_POST(Info << "Mounting database at: " << db_path);

        auto_ptr<CQueueDataBase> qdb(new CQueueDataBase());
        qdb->Open(db_path, mem_size);

        string qname = "noname";
        LOG_POST(Info << "Mounting queue: " << qname);
        qdb->MountQueue(qname); // default queue


        // Scan and mount queues

        list<string> sections;
        reg.EnumerateSections(&sections);

        string tmp;
        ITERATE(list<string>, it, sections) {
            const string& sname = *it;
            NStr::SplitInTwo(sname, "_", tmp, qname);
            if (NStr::CompareNocase(tmp, "queue") == 0) {
                LOG_POST(Info << "Mounting queue: " << qname);
                qdb->MountQueue(qname);
            }
        }

        // Init threaded server
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
        }

        bool is_log =
            reg.GetBool("server", "log", false, 0, CNcbiRegistry::eReturn);

        auto_ptr<CNetScheduleServer> thr_srv(
            new CNetScheduleServer(port, 
                                   qdb.release(),
                                   max_threads, 
                                   init_threads,
                                   network_timeout,
                                   is_log));


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



/// @internal
extern "C" void Threaded_Server_SignalHandler( int )
{
    if (s_netschedule_server && 
        (!s_netschedule_server->ShutdownRequested()) ) {
        s_netschedule_server->SetShutdownFlag();
        LOG_POST("Interrupt signal. Shutdown requested.");
    }
}



int main(int argc, const char* argv[])
{

#if defined(NCBI_OS_UNIX)
    // attempt to get server gracefully shutdown on signal
     signal( SIGINT, Threaded_Server_SignalHandler);
     signal( SIGTERM, Threaded_Server_SignalHandler);    
#endif

     return 
        CNetScheduleDApp().AppMain(argc, argv, 0, 
                           eDS_Default, "netscheduled.ini");
}

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/02/08 16:42:55  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
