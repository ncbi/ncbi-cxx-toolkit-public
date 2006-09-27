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

#include <connect/ncbi_core_cxx.hpp>
#include <connect/threaded_server.hpp>
#include <connect/server.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/services/netschedule_client.hpp>
#include <connect/ncbi_conn_stream.hpp>

#include <bdb/bdb_expt.hpp>
#include <bdb/bdb_cursor.hpp>

#include "bdb_queue.hpp"
#include <db.h>

#if defined(NCBI_OS_UNIX)
# include <corelib/ncbi_os_unix.hpp>
# include <signal.h>
#endif

#include "job_time_line.hpp"
#include "access_list.hpp"


USING_NCBI_SCOPE;


#define NETSCHEDULED_VERSION \
    "NCBI NetSchedule server version=1.13.0  build " __DATE__ " " __TIME__

class CNetScheduleServer;
static CNetScheduleServer* s_netschedule_server = 0;

/// JS request types
///
/// @internal
///
typedef enum {
    eRegisterClient,
    eUnRegisterClient,
    eSubmitJob,
    eSubmitBatch,
    eForceReschedule,
    eCancelJob,
    eStatusJob,
    eGetJob,
    eWaitGetJob,
    ePutJobResult,
    ePutJobFailure,
    eReturnJob,
    eJobRunTimeout,
    eJobDelayExpiration,
    eDropQueue,
    eDropJob,
    eGetProgressMsg,
    ePutProgressMsg,
    eJobExchange,
    eShutdown,
    eVersion,
    eLogging,
    eStatistics,
    eQuitSession,
    eError,
    eMonitor,
    eDumpQueue,
    ePrintQueue,
    eReloadConfig,
    eQList,
    eStatusSnapshot
} EJS_RequestType;

/// Request context
///
/// @internal
struct SJS_Request
{
    EJS_RequestType    req_type;
    char               input[kNetScheduleMaxDBDataSize];
    char               output[kNetScheduleMaxDBDataSize];
    char               progress_msg[kNetScheduleMaxDBDataSize];
    char               cout[kNetScheduleMaxDBDataSize];
    char               cerr[kNetScheduleMaxDBDataSize];
    char               err[kNetScheduleMaxDBErrSize];
    char               affinity_token[kNetScheduleMaxDBDataSize];
    string             job_key_str;
    unsigned int       jcount;
    unsigned int       job_id;
    unsigned int       job_return_code;
    unsigned int       port;
    unsigned int       timeout;
    unsigned int       job_mask;

    string             err_msg;

    void Init()
    {
        input[0] = output[0] = progress_msg[0] = affinity_token[0] 
                                               = cout[0] = cerr[0] = 0;
        job_key_str.erase(); err_msg.erase();
        jcount = job_id = job_return_code = port = timeout = job_mask = 0;
    }
};


const unsigned kMaxMessageSize = kNetScheduleMaxDBErrSize * 4;

/// Thread specific data for threaded server
///
/// @internal
///
struct SThreadData
{
    char*      request;

    string            auth;
    string            auth_prog;  // prog='...' from auth string
    CQueueClientInfo  auth_prog_info;    

    
    string      queue;
    string      answer;

    char        msg_buf[kMaxMessageSize];

    string      lmsg;       ///< LOG message
    unsigned    peer_addr;  ///< Peer address

    vector<SNS_BatchSubmitRec>  batch_subm_vec;

private:
    unsigned int   x_request_buf[kNetScheduleMaxDBErrSize];
public:
    SThreadData() { request = (char*) x_request_buf; }  
    size_t GetBufSize() const { return sizeof(x_request_buf); }
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



/// Log stream without creating backup names
///
/// @internal
class CNetScheduleLogStream : public CRotatingLogStream
{
public:
    typedef CRotatingLogStream TParent;
public:
    CNetScheduleLogStream(const string&    filename, 
                          CNcbiStreamoff   limit)
    : TParent(filename, limit)
    {}
protected:
    virtual string x_BackupName(string& name)
    {
        return kEmptyStr;
    }

};

// Helper
inline void JS_CHECK_IO_STATUS(EIO_Status x)
{
    switch (x)  {
    case eIO_Success:
        break;
    case eIO_Timeout:
        NCBI_THROW(CNetServiceException,
                    eTimeout, "Communication timeout error");
    case eIO_Closed:
        NCBI_THROW(CNetServiceException,
            eCommunicationError, "Communication error(socket closed)");
    default:
        NCBI_THROW(CNetServiceException,
            eCommunicationError, "Communication error");
    }
}

// Helper
bool CheckVersion(SThreadData* tdata, CQueueDataBase::CQueue& queue)
{
    string::size_type pos; 
    pos = tdata->auth.find("prog='");
    if (pos == string::npos) {
        return false;
    }
    string& prog = tdata->auth_prog;
    prog.erase();
    const char* prog_str = tdata->auth.c_str();
    prog_str += pos + 6;
    for(; *prog_str && *prog_str != '\''; ++prog_str) {
        char ch = *prog_str;
        prog.push_back(ch);
    }
    if (prog.empty()) {
        return false;
    }
    try {
        CQueueClientInfo& prog_info = tdata->auth_prog_info;
        ParseVersionString(prog,
                           &prog_info.client_name, 
                           &prog_info.version_info);
        bool match = queue.IsMatchingClient(prog_info);
        if (!match)
            return false;
    }
    catch (exception&)
    {
        return false;
    }
    return true;
}


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
        NCBI_THROW(CNetServiceException, eTimeout, "Socket Timeout expired");
    default:
        NCBI_THROW(CNetServiceException, 
                   eCommunicationError, "Socket Wait error.");
        return false;
    }
    return false;
}        


//////////////////////////////////////////////////////////////////////////
/// ConnectionHandler for NetScheduler

class CNetScheduleServer;
class CNetScheduleHandler : public IServer_LineMessageHandler
{
public:
    CNetScheduleHandler(CNetScheduleServer* server);
    // MessageHandler protocol
    virtual void OnOpen(void) { }
    virtual void OnWrite(void) { }
    virtual void OnClose(void);
    virtual void OnTimeout(void);
    virtual void OnMessage(BUF buffer); 

    // Transitional
    // TODO: Move this method to OnOpen handler
    void Init(CSocket& socket, unsigned host_addr, unsigned local_addr);
    SThreadData* GetThreadData() { return &m_ThreadData; }
    CNetScheduleMonitor* GetMonitor(void) { return m_Monitor; }
    bool IsDone(void) { return m_Done; }

    void WriteMsg(CSocket&      sock, 
                  const char*   prefix, 
                  const char*   msg,
                  bool          comm_control = false,
                  bool          msg_size_control = true);

private:
    // Message processing phases
    void ProcessMsgAuth(BUF buffer);
    void ProcessMsgQueue(BUF buffer);
    void ProcessMsgRequest(BUF buffer);

    // Protocol processing methods
    void ParseRequest(const char* reqstr, SJS_Request* req);

    // Moved from CNetScheduleServer
    void ProcessSubmit(CSocket& socket);
    void ProcessSubmitBatch(CSocket& socket);
    void ProcessCancel(CSocket& socket);
    void ProcessStatus(CSocket& socket);
    void ProcessGet(CSocket& socket);
    void ProcessWaitGet(CSocket& socket);
    void ProcessPut(CSocket& socket);
    void ProcessJobExchange(CSocket& socket);
    void ProcessMPut(CSocket& socket);
    void ProcessMGet(CSocket& socket);
    void ProcessPutFailure(CSocket& socket);
    void ProcessDropQueue(CSocket& socket);
    void ProcessReturn(CSocket& socket);
    void ProcessJobRunTimeout(CSocket& socket);
    void ProcessJobDelayExpiration(CSocket& socket);
    void ProcessDropJob(CSocket& socket);
    void ProcessForceReschedule(CSocket& socket);
    void ProcessStatistics(CSocket& socket);
    void ProcessStatusSnapshot(CSocket& socket);
    void ProcessMonitor(CSocket& socket);
    void ProcessReloadConfig(CSocket& socket);
    void ProcessDump(CSocket& socket);
    void ProcessPrintQueue(CSocket& socket);
    void ProcessShutdown(CSocket& socket);
    void ProcessVersion(CSocket& socket);
    void ProcessRegisterClient(CSocket& socket);
    void ProcessUnRegisterClient(CSocket& socket);
    void ProcessQList(CSocket &socket);
    void ProcessError(CSocket& socket);
    void ProcessLog(CSocket& socket);

private:
    // Moved from CNetScheduleServer
    void x_MakeLogMessage(CSocket& socket);
    void x_MakeGetAnswer(const char* key_buf, 
                         const char*  jout,
                         const char*  jerr,
                         unsigned     job_mask);

    // Transitional (with GetThreadData). Because every instance of ConnectionHandler
    // is responsible for handling specific connection and executes in
    // thread pooled thread, there is no need for TLS here. It MUST be
    // recoded as direct access to fields of SThreadData.
    SThreadData m_ThreadData;
    bool m_Done;


    // Real data
    CNetScheduleServer* m_Server;
    auto_ptr<CQueueDataBase::CQueue> m_Queue;
    CNetScheduleMonitor* m_Monitor;
    SJS_Request m_JobReq;
    bool m_VersionControl;
    bool m_AdminAccess;
    void (CNetScheduleHandler::*m_ProcessMessage)(BUF buffer);

    /// Quick local timer
    CFastLocalTime              m_LocalTimer;

};

//////////////////////////////////////////////////////////////////////////
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
    virtual bool ShutdownRequested(void) 
    {
        if (m_Shutdown) {
            LOG_POST(Info << "Shutdown flag set! Signal=" << m_SigNum); 
        } 
        return m_Shutdown; 
    }
        
    void SetShutdownFlag(int signum = 0) 
    { 
        if (!m_Shutdown) {
            m_Shutdown = true; 
            m_SigNum = signum;
        }
    }
    
    /// Override some parent parameters
    virtual void SetParams()
    {
        m_ThrdSrvAcceptTimeout.sec = 1;
        m_ThrdSrvAcceptTimeout.usec = 0;
    }

    void SetAdminHosts(const string& host_names);

    //////////////////////////////////////////////////////////////////
    /// For Handlers
    CNetScheduleLogStream& GetLog(void) { return m_AccessLog; }
    /// TRUE if logging is ON
    bool IsLog() const { return m_LogFlag.Get() != 0; }
    void SetLogging(bool flag) {
        if (flag) {
            bool is_log = IsLog();
            m_LogFlag.Set(1);
            if (!is_log) {
                m_LogFlag.Set(1);
                m_AccessLog.Rotate();
            }
        } else {
            m_LogFlag.Set(0);
        }
    }
    CQueueDataBase* GetQueueDB(void) { return m_QueueDB; }
    unsigned GetInactivityTimeout(void) { return m_InactivityTimeout; }
    string& GetHost() { return m_Host; }
    // GetPort inherited from CThreadedServer
    const CTime& GetStartTime(void) const { return m_StartTime; }
    const CNetSchedule_AccessList& GetAccessList(void) const { return m_AdminHosts; }

protected:
    virtual void ProcessOverflow(SOCK sock) 
    { 
        SOCK_Close(sock); 
        ERR_POST("ProcessOverflow!");
    }

private:
    void x_WriteBuf(CSocket& sock,
                    char*    buf,
                    size_t   bytes);

    /// Check if we have active thread data for this thread.
    /// Setup thread data if we don't.
    SThreadData* x_GetThreadData(); 

    void x_SetSocketParams(CSocket* sock);

    void x_CreateLog();

private:
    /// Host name where server runs
    string             m_Host;
    unsigned           m_HostNetAddr;
    unsigned           m_LocalNetAddr;
    bool               m_Shutdown; 
    int                m_SigNum;  ///< Shutdown signal number
    /// Time to wait for the client (seconds)
    unsigned           m_InactivityTimeout;
    
    /// Accept timeout for threaded server
    STimeout           m_ThrdSrvAcceptTimeout;

    CQueueDataBase*    m_QueueDB;

    CTime              m_StartTime;

    CAtomicCounter         m_LogFlag;
    CNetScheduleLogStream  m_AccessLog;
    /// Quick local timer
    CFastLocalTime              m_LocalTimer;

    /// List of admin stations allowed to enter
    CNetSchedule_AccessList    m_AdminHosts;
};

static void s_ReadBufToString(BUF buffer, string& str)
{
    char buf[1024];
    size_t size;
    str.erase();
    for (;;) {
        size = BUF_Read(buffer, buf, sizeof(buf));
        if (!size) break;
        str.append(buf, size);
    }
}

//////////////////////////////////////////////////////////////////////////
// ConnectionHandler implementation

CNetScheduleHandler::CNetScheduleHandler(CNetScheduleServer* server)
    : m_Done(false), m_Server(server), m_Monitor(NULL),
      m_VersionControl(false), m_AdminAccess(false)
{
}

// Transitional
void CNetScheduleHandler::Init(CSocket& socket,
                               unsigned host_addr, unsigned local_addr)
{
    socket.DisableOSSendDelay();
    STimeout to = {m_Server->GetInactivityTimeout(), 0};
    socket.SetTimeout(eIO_ReadWrite, &to);

    socket.GetPeerAddress(&m_ThreadData.peer_addr, 0, eNH_NetworkByteOrder);
    // always use localhost(127.0*) address for clients coming from 
    // the same net address (sometimes it can be 127.* or full address)
    if (m_ThreadData.peer_addr == host_addr) {
        m_ThreadData.peer_addr = local_addr;
    }

    m_ThreadData.auth.erase();

    m_ProcessMessage = &CNetScheduleHandler::ProcessMsgAuth;
}


void CNetScheduleHandler::OnClose()
{
    m_Done = true;
}


void CNetScheduleHandler::OnTimeout()
{
    m_Done = true;
}


void CNetScheduleHandler::OnMessage(BUF buffer)
{
    (this->*m_ProcessMessage)(buffer);
}


void CNetScheduleHandler::ProcessMsgAuth(BUF buffer)
{
    s_ReadBufToString(buffer, m_ThreadData.auth);

    if (m_ThreadData.auth == "netschedule_control" ||
        m_ThreadData.auth == "netschedule_admin") {
        m_AdminAccess = true;
    }
    m_ProcessMessage = &CNetScheduleHandler::ProcessMsgQueue;
}


void CNetScheduleHandler::ProcessMsgQueue(BUF buffer)
{
    s_ReadBufToString(buffer, m_ThreadData.queue);

    m_Queue.reset(new CQueueDataBase::CQueue(*(m_Server->GetQueueDB()), 
                                             m_ThreadData.queue,
                                             m_ThreadData.peer_addr));
    m_Monitor = m_Queue->GetMonitor();

    if (!m_Queue->IsVersionControl()) {
        m_VersionControl = true;
    }
    m_ProcessMessage = &CNetScheduleHandler::ProcessMsgRequest;
}


// Workhorse method
void CNetScheduleHandler::ProcessMsgRequest(BUF buffer)
{
    SThreadData* tdata = GetThreadData(); // Transitional
    bool is_log = m_Server->IsLog();

    size_t msg_size = BUF_Read(buffer, tdata->request, tdata->GetBufSize());
    if (msg_size < tdata->GetBufSize()) tdata->request[msg_size] = '\0';

    CSocket& socket = GetSocket();
    // Logging
    if (is_log) {
        x_MakeLogMessage(socket);
        m_Server->GetLog() << tdata->lmsg;
    }
    if (m_Monitor) {
        if (!m_Monitor->IsMonitorActive()) {
            m_Monitor = 0;
        } else {
            if (!is_log) {
                x_MakeLogMessage(socket);
            }
            m_Monitor->SendString(tdata->lmsg);
        }
    }

    m_JobReq.Init();
    ParseRequest(tdata->request, &m_JobReq);

    if (m_JobReq.req_type == eQuitSession) {
        m_Done = true;
        // read trailing input (see netcached.cpp for more comments on "why?")
        STimeout to; to.sec = to.usec = 0;
        socket.SetTimeout(eIO_Read, &to);
        socket.Read(NULL, 1024);
        socket.Close();
        return;
    }

    // program version control
    if (!m_VersionControl &&
        // we want status request to be fast, skip version control 
        (m_JobReq.req_type != eStatusJob) &&
        // bypass for admin tools
        !m_AdminAccess) {
        if (!CheckVersion(tdata, *m_Queue)) {
            m_Done = true;
            WriteMsg(socket, "ERR:", "CLIENT_VERSION");
            return;
        }
    }

    switch (m_JobReq.req_type) {
    case eSubmitJob:
        ProcessSubmit(socket);
        break;
    case eSubmitBatch:
        ProcessSubmitBatch(socket);
        break;
    case eCancelJob:
        ProcessCancel(socket);
        break;
    case eStatusJob:
        ProcessStatus(socket);
        break;
    case eStatusSnapshot:
        ProcessStatusSnapshot(socket);
        break;
    case eGetJob:
        ProcessGet(socket);
        break;
    case eJobExchange:
        ProcessJobExchange(socket);
        break;
    case eWaitGetJob:
        ProcessWaitGet(socket);
        break;
    case ePutJobResult:
        ProcessPut(socket);
        break;
    case ePutJobFailure:
        ProcessPutFailure(socket);
        break;
    case eReturnJob:
        ProcessReturn(socket);
        break;
    case eJobRunTimeout:
        ProcessJobRunTimeout(socket);
        break;
    case eJobDelayExpiration:
        ProcessJobDelayExpiration(socket);
        break;
    case eDropJob:
        ProcessDropJob(socket);
        break;
    case eForceReschedule:
        ProcessForceReschedule(socket);
        break;
    case eGetProgressMsg:
        ProcessMGet(socket);
        break;
    case ePutProgressMsg:
        ProcessMPut(socket);
        break;
    case eShutdown:
        ProcessShutdown(socket);
        break;
    case eVersion:
        ProcessVersion(socket);
        break;
    case eDropQueue:
        ProcessDropQueue(socket);
        break;
    case eLogging:
        ProcessLog(socket);
        break;
    case eMonitor:
        ProcessMonitor(socket);
        break;
    case eDumpQueue:
        ProcessDump(socket);
        break;
    case ePrintQueue:
        ProcessPrintQueue(socket);
        break;
    case eStatistics:
        ProcessStatistics(socket);
        break;
    case eReloadConfig:
        ProcessReloadConfig(socket);
        break;
    case eRegisterClient:
        ProcessRegisterClient(socket);
        break;
    case eUnRegisterClient:
        ProcessUnRegisterClient(socket);
        break;
    case eQList:
        ProcessQList(socket);
        break;
    case eError:
        ProcessError(socket);
        break;
    default:
        _ASSERT(0);
    } // switch
}

// TODO: ProcessSubmitBatch parses commands itself. Refactor to avoid it.
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

void CNetScheduleHandler::ProcessSubmit(CSocket& socket)
{
    if (!m_Queue->IsSubmitAllowed()) {
        WriteMsg(socket, "ERR:", "OPERATION_ACCESS_DENIED");
        return;
    }
    unsigned job_id =
        m_Queue->Submit(m_JobReq.input, 
                        m_ThreadData.peer_addr, 
                        m_JobReq.port, m_JobReq.timeout, 
                        m_JobReq.progress_msg,
                        m_JobReq.affinity_token,
                        m_JobReq.cout,
                        m_JobReq.cerr,
                        m_JobReq.job_mask);

    char buf[1024];
    sprintf(buf, NETSCHEDULE_JOBMASK, 
                 job_id, m_Server->GetHost().c_str(), unsigned(m_Server->GetPort()));

    WriteMsg(socket, "OK:", buf);

    if (m_Monitor && m_Monitor->IsMonitorActive()) {
        string msg = "::ProcessSubmit ";
        msg += m_LocalTimer.GetLocalTime().AsString();
        msg += buf;
        msg += " ==> ";
        msg += socket.GetPeerAddress();
        msg += " ";
        msg += m_ThreadData.auth;

        m_Monitor->SendString(msg);
    }

}

void CNetScheduleHandler::ProcessSubmitBatch(CSocket& socket)
{
    if (!m_Queue->IsSubmitAllowed()) {
        WriteMsg(socket, "ERR:", "OPERATION_ACCESS_DENIED");
        return;
    }

    char    buf[kNetScheduleMaxDBDataSize * 6 + 1];
    size_t  n_read;

    WriteMsg(socket, "OK:", "Batch submit ready");
    s_WaitForReadSocket(socket, m_Server->GetInactivityTimeout());
    EIO_Status io_st;

    while (1) {
        unsigned batch_size = 0;

        // BTCH batch_size
        //
        {{
            io_st = socket.ReadLine(buf, sizeof(buf), &n_read);
            JS_CHECK_IO_STATUS(io_st);

            const char* s = buf;
            if (strncmp(s, "BTCH", 4) != 0) {
                if (strncmp(s, "ENDS", 4) == 0) {
                    break;
                }
                WriteMsg(socket, "ERR:", 
                        "Batch submit error: BATCH expected");
                return;
            }
            s+=5;
            NS_SKIPSPACE(s)

            batch_size = atoi(s);
        }}

        // read the batch body
        //

        CStopWatch sw1(CStopWatch::eStart);

        m_ThreadData.batch_subm_vec.resize(batch_size);

        for (unsigned i = 0; i < batch_size; ++i) {
            s_WaitForReadSocket(socket, m_Server->GetInactivityTimeout());

            io_st = socket.ReadLine(buf, sizeof(buf)-1, &n_read);
            JS_CHECK_IO_STATUS(io_st);

            if (*buf == 0) {  // something is wrong
                WriteMsg(socket, "ERR:", 
                        "Batch submit error: empty string");
                return;
            }

            const char* s = buf;
            if (*s !='"') {
                WriteMsg(socket, "ERR:", 
                        "Invalid batch submission, syntax error");
                return;
            }

            SNS_BatchSubmitRec& rec = m_ThreadData.batch_subm_vec[i];
            char *ptr = rec.input;
            for (++s; *s != '"'; ++s) {
                if (*s == 0) {
                    WriteMsg(socket, "ERR:",
                             "Batch submit error: unexpected end of string");
                    return;
                }
                *ptr++ = *s;
            } // for
            
            *ptr = 0;

            ++s;
            NS_SKIPSPACE(s)

            // *s == "aff'
            if (s[0] == 'a' && s[1] == 'f' && s[2] == 'f') {
                s += 3;
                if (*s == '=') {
                    ++s;
                    // affinity token
                    if (*s != '"') {
                        WriteMsg(socket, "ERR:",
                                "Batch submit error: error in affinity token");
                    }
                    char *ptr = rec.affinity_token;
                    for (++s; *s != '"'; ++s) {
                        if (*s == 0) {
                            WriteMsg(socket, "ERR:",
                            "Batch submit error: unexpected end of affinity str");
                            return;
                        }
                        *ptr++ = *s;
                    } // for
                    *ptr = 0;
                } 
                else
                if (*s == 'p') {
                    ++s;
                    // "affp" - take previous affinity
                    rec.affinity_id = kMax_I4;
                }
                else {
                    WriteMsg(socket, "ERR:",
                    "Batch submit error: unrecognised affinity clause");
                    return;
                }

            }

        } // for batch_size

        s_WaitForReadSocket(socket, m_Server->GetInactivityTimeout());

        io_st = socket.ReadLine(buf, sizeof(buf), &n_read);
        JS_CHECK_IO_STATUS(io_st);


        const char* s = buf;
        if (strncmp(s, "ENDB", 4) != 0) {
            WriteMsg(socket, "ERR:",
                             "Batch submit error: unexpected end of batch");
        }

        double comm_elapsed = sw1.Elapsed();

        // we have our batch now

        CStopWatch sw2(CStopWatch::eStart);

        unsigned job_id =
            m_Queue->SubmitBatch(m_ThreadData.batch_subm_vec, 
                                 m_ThreadData.peer_addr,
                                 0, 0, 0);

        double db_elapsed = sw2.Elapsed();

        if (m_Monitor && m_Monitor->IsMonitorActive()) {
            string msg = "::ProcessSubmitBatch ";
            msg += m_LocalTimer.GetLocalTime().AsString();
            msg += buf;
            msg += " ==> ";
            msg += socket.GetPeerAddress();
            msg += " ";
            msg += m_ThreadData.auth;
            msg += " batch block size=";
            msg += NStr::UIntToString(batch_size);
            msg += " comm.time=";
            msg += NStr::DoubleToString(comm_elapsed, 4, NStr::fDoubleFixed);
            msg += " trans.time=";
            msg += NStr::DoubleToString(db_elapsed, 4, NStr::fDoubleFixed);

            m_Monitor->SendString(msg);
        }

        {{
        char buf[1024];
        sprintf(buf, "%u %s %u", 
                    job_id, m_Server->GetHost().c_str(), unsigned(m_Server->GetPort()));

        WriteMsg(socket, "OK:", buf);
        }}

    } // while

    m_Server->GetQueueDB()->TransactionCheckPoint();
}

void CNetScheduleHandler::ProcessCancel(CSocket& socket)
{
    if (!m_Queue->IsSubmitAllowed()) {
        WriteMsg(socket, "ERR:", "OPERATION_ACCESS_DENIED");
        return;
    }

    unsigned job_id = CNetSchedule_GetJobId(m_JobReq.job_key_str);
    m_Queue->Cancel(job_id);
    WriteMsg(socket, "OK:", "");
}


void CNetScheduleHandler::ProcessForceReschedule(
                                       CSocket& socket)
{
    if (!m_Queue->IsSubmitAllowed()) {
        WriteMsg(socket, "ERR:", "OPERATION_ACCESS_DENIED");
        return;
    }

    unsigned job_id = CNetSchedule_GetJobId(m_JobReq.job_key_str);
    m_Queue->ForceReschedule(job_id);
    WriteMsg(socket, "OK:", "");
}


void CNetScheduleHandler::ProcessDropJob(CSocket& socket)
{
    if (!m_Queue->IsSubmitAllowed()) {
        WriteMsg(socket, "ERR:", "OPERATION_ACCESS_DENIED");
        return;
    }

    unsigned job_id = CNetSchedule_GetJobId(m_JobReq.job_key_str);
    m_Queue->DropJob(job_id);
    WriteMsg(socket, "OK:", "");
}


void CNetScheduleHandler::ProcessJobRunTimeout(CSocket& socket)
{
    unsigned job_id = CNetSchedule_GetJobId(m_JobReq.job_key_str);
    m_Queue->SetJobRunTimeout(job_id, m_JobReq.timeout);
    WriteMsg(socket, "OK:", "");
}

void CNetScheduleHandler::ProcessJobDelayExpiration(CSocket& socket)
{
    unsigned job_id = CNetSchedule_GetJobId(m_JobReq.job_key_str);
    m_Queue->JobDelayExpiration(job_id, m_JobReq.timeout);
    WriteMsg(socket, "OK:", "");
}


void CNetScheduleHandler::ProcessStatusSnapshot(CSocket& socket)
{
    const char* aff_token = m_JobReq.affinity_token;
    CNetScheduler_JobStatusTracker::TStatusSummaryMap st_map;

    bool aff_exists = m_Queue->CountStatus(&st_map, aff_token);
    if (!aff_exists) {
        WriteMsg(socket, "ERR:", "Unknown affinity token.");
        return;
    }
    ITERATE(CNetScheduler_JobStatusTracker::TStatusSummaryMap,
            it,
            st_map) {
        string st_str = CNetScheduleClient::StatusToString(it->first);
        st_str.push_back(' ');
        st_str.append(NStr::UIntToString(it->second));
        WriteMsg(socket, "OK:", st_str.c_str());
    } // ITERATE
    WriteMsg(socket, "OK:", "END");
}


void CNetScheduleHandler::ProcessStatus(CSocket& socket)
{
    unsigned job_id = CNetSchedule_GetJobId(m_JobReq.job_key_str);

    CNetScheduleClient::EJobStatus status = m_Queue->GetStatus(job_id);
    char szBuf[kNetScheduleMaxDBDataSize * 2];
    int st = (int) status;
    int ret_code = 0;

    switch (status) {
    case CNetScheduleClient::eDone:
        {
        if (m_Queue->GetJobDescr(job_id, &ret_code, 
                                 m_JobReq.input,
                                 m_JobReq.output,
                                 0, 0,
                                 0, 0, 
                                 status)) {
        send_reply:
            sprintf(szBuf, 
                    "%i %i \"%s\" \"\" \"%s\"", 
                    st, ret_code, 
                    m_JobReq.output,
                    m_JobReq.input);
            WriteMsg(socket, "OK:", szBuf);
            return;
        } else {
            st = (int)CNetScheduleClient::eJobNotFound;
        }
        }
    break;

    case CNetScheduleClient::eRunning:
    case CNetScheduleClient::eReturned:
    case CNetScheduleClient::ePending:
        {
        bool b = m_Queue->GetJobDescr(job_id, 0, 
                                      m_JobReq.input,
                                      0,
                                      0, 0,
                                      0, 0, 
                                      status);

        if (b) {
            m_JobReq.output[0] = 0;
            goto send_reply;
        } else {
            st = (int)CNetScheduleClient::eJobNotFound;
        }
        }
        break;

    case CNetScheduleClient::eFailed:
        {
        bool b = m_Queue->GetJobDescr(job_id, &ret_code, 
                                      m_JobReq.input,
                                      m_JobReq.output,
                                      m_JobReq.err,
                                      0,
                                      0, 0, 
                                      status);

        if (b) {
            sprintf(szBuf, 
                    "%i %i \"%s\" \"%s\" \"%s\"", 
                    st, ret_code, 
                    m_JobReq.output,
                    m_JobReq.err,
                    m_JobReq.input);
            WriteMsg(socket, "OK:", szBuf);
            return;
        } else {
            st = (int)CNetScheduleClient::eJobNotFound;
        }
        }
        break;
    default:
        break;

    } // switch


    sprintf(szBuf, "%i", st);
    WriteMsg(socket, "OK:", szBuf);
}

void CNetScheduleHandler::ProcessMGet(CSocket& socket)
{
    unsigned job_id = CNetSchedule_GetJobId(m_JobReq.job_key_str);
    int ret_code;
    bool b = m_Queue->GetJobDescr(job_id, &ret_code, 
                                  0, 0, 0, m_JobReq.progress_msg, 0, 0);

    if (b) {
        WriteMsg(socket, "OK:", m_JobReq.progress_msg);
    } else {
        WriteMsg(socket, "ERR:", "Job not found");
    }
}

void CNetScheduleHandler::ProcessMPut(CSocket& socket)
{
    unsigned job_id = CNetSchedule_GetJobId(m_JobReq.job_key_str);

    m_Queue->PutProgressMessage(job_id, m_JobReq.progress_msg);
    WriteMsg(socket, "OK:", "");
}

void CNetScheduleHandler::ProcessGet(CSocket& socket)
{
    if (!m_Queue->IsWorkerAllowed()) {
        WriteMsg(socket, "ERR:", "OPERATION_ACCESS_DENIED");
        return;
    }
    unsigned job_id;
    char key_buf[1024];
    m_Queue->GetJob(m_ThreadData.peer_addr, &job_id,
                    m_JobReq.input, m_JobReq.cout, m_JobReq.cerr,
                    m_ThreadData.auth, 
                    &m_JobReq.job_mask);

    m_Queue->GetJobKey(key_buf, job_id,
        m_Server->GetHost(), m_Server->GetPort());

    if (job_id) {
        x_MakeGetAnswer(key_buf, m_JobReq.cout, m_JobReq.cerr,
                        m_JobReq.job_mask);
        WriteMsg(socket, "OK:", m_ThreadData.answer.c_str());
    } else {
        WriteMsg(socket, "OK:", "");
    }

    if (m_Monitor && m_Monitor->IsMonitorActive() && job_id) {
        string msg = "::ProcessGet ";
        msg += m_LocalTimer.GetLocalTime().AsString();
        msg += m_ThreadData.answer;
        msg += " ==> ";
        msg += socket.GetPeerAddress();
        msg += " ";
        msg += m_ThreadData.auth;

        m_Monitor->SendString(msg);
    }

    if (m_JobReq.port) {  // unregister notification
        m_Queue->RegisterNotificationListener(
            m_ThreadData.peer_addr, m_JobReq.port, 0, m_ThreadData.auth);
   }
}

void 
CNetScheduleHandler::ProcessJobExchange(CSocket& socket)
{
    if (!m_Queue->IsWorkerAllowed()) {
        WriteMsg(socket, "ERR:", "OPERATION_ACCESS_DENIED");
        return;
    }
    unsigned job_id;
    char key_buf[1024];

    unsigned done_job_id;
    if (!m_JobReq.job_key_str.empty()) {
        done_job_id = CNetSchedule_GetJobId(m_JobReq.job_key_str);
    } else {
        done_job_id = 0;
    }
    m_Queue->PutResultGetJob(done_job_id, m_JobReq.job_return_code,
                             m_JobReq.output, false /*don't change the timeline*/,
                             m_ThreadData.peer_addr, &job_id,
                             m_JobReq.input, m_JobReq.cout, m_JobReq.cerr,
                             m_ThreadData.auth,
                             &m_JobReq.job_mask);

    m_Queue->GetJobKey(key_buf, job_id,
        m_Server->GetHost(), m_Server->GetPort());

    if (job_id) {
        x_MakeGetAnswer(key_buf, m_JobReq.cout, m_JobReq.cerr,
                        m_JobReq.job_mask);
        WriteMsg(socket, "OK:", m_ThreadData.answer.c_str());
    } else {
        WriteMsg(socket, "OK:", "");
    }
    // postpone removal from the timeline, so we remove while 
    // request response is network transferred
    m_Queue->TimeLineExchange(done_job_id, job_id, time(0));

    if (m_Monitor && m_Monitor->IsMonitorActive()) {
        string msg = "::ProcessJobExchange ";
        msg += m_LocalTimer.GetLocalTime().AsString();
        msg += m_ThreadData.answer;
        msg += " ==> ";
        msg += socket.GetPeerAddress();
        msg += " ";
        msg += m_ThreadData.auth;

        m_Monitor->SendString(msg);
    }
}

void CNetScheduleHandler::ProcessWaitGet(CSocket& socket)
{
    if (!m_Queue->IsWorkerAllowed()) {
        WriteMsg(socket, "ERR:", "OPERATION_ACCESS_DENIED");
        return;
    }
    unsigned job_id;
    char key_buf[1024];
    m_Queue->GetJob(m_ThreadData.peer_addr, &job_id, 
                    m_JobReq.input, m_JobReq.cout, m_JobReq.cerr,
                    m_ThreadData.auth, &m_JobReq.job_mask);

    m_Queue->GetJobKey(key_buf, job_id,
        m_Server->GetHost(), m_Server->GetPort());

    if (job_id) {
        x_MakeGetAnswer(key_buf, m_JobReq.cout, m_JobReq.cerr,
                        m_JobReq.job_mask);
        WriteMsg(socket, "OK:", m_ThreadData.answer.c_str());
        return;
    }

    // job not found, initiate waiting mode

    WriteMsg(socket, "OK:", kEmptyStr.c_str());

    m_Queue->RegisterNotificationListener(
        m_ThreadData.peer_addr, m_JobReq.port, m_JobReq.timeout,
        m_ThreadData.auth);
}

void CNetScheduleHandler::ProcessRegisterClient(CSocket& socket)
{
    m_Queue->RegisterNotificationListener(
        m_ThreadData.peer_addr, m_JobReq.port, 1, m_ThreadData.auth);

    WriteMsg(socket, "OK:", kEmptyStr.c_str());
}

void CNetScheduleHandler::ProcessUnRegisterClient(CSocket& socket)
{
    m_Queue->UnRegisterNotificationListener(m_ThreadData.peer_addr,
                                            m_JobReq.port);
    m_Queue->ClearAffinity(m_ThreadData.peer_addr, m_ThreadData.auth);

    WriteMsg(socket, "OK:", kEmptyStr.c_str());
}

void CNetScheduleHandler::ProcessQList(CSocket &socket)
{
    string qnames;
    ITERATE(CQueueCollection::TQueueMap, it, 
            m_Server->GetQueueDB()->GetQueueCollection().GetMap()) {
        qnames += it->first; qnames += ";";
    }
    WriteMsg(socket, "OK:", qnames.c_str());
}

void CNetScheduleHandler::ProcessError(CSocket& socket) {
    WriteMsg(socket, "ERR:", m_JobReq.err_msg.c_str());
}


void CNetScheduleHandler::ProcessPutFailure(CSocket& socket)
{
    unsigned job_id = CNetSchedule_GetJobId(m_JobReq.job_key_str);
    m_Queue->JobFailed(job_id, m_JobReq.err_msg, m_JobReq.output,
                       m_JobReq.job_return_code,
                       m_ThreadData.peer_addr, m_ThreadData.auth);
    WriteMsg(socket, "OK:", kEmptyStr.c_str());
}

void CNetScheduleHandler::ProcessPut(CSocket& socket)
{
    unsigned job_id = CNetSchedule_GetJobId(m_JobReq.job_key_str);
    m_Queue->PutResult(job_id, 
                       m_JobReq.job_return_code, m_JobReq.output, 
                       false // don't remove from tl
                       );
    WriteMsg(socket, "OK:", kEmptyStr.c_str());
    m_Queue->RemoveFromTimeLine(job_id);
}


void CNetScheduleHandler::ProcessReturn(CSocket& socket)
{
    unsigned job_id = CNetSchedule_GetJobId(m_JobReq.job_key_str);
    m_Queue->ReturnJob(job_id);
    WriteMsg(socket, "OK:", kEmptyStr.c_str());
}


void CNetScheduleHandler::ProcessDropQueue(CSocket& socket)
{
    m_Queue->Truncate();
    WriteMsg(socket, "OK:", kEmptyStr.c_str());
}

void CNetScheduleHandler::ProcessMonitor(CSocket& socket)
{
    socket.DisableOSSendDelay(false);
    WriteMsg(socket, "OK:", NETSCHEDULED_VERSION);
    string started = "Started: " + m_Server->GetStartTime().AsString();
    WriteMsg(socket, "OK:", started.c_str());
    socket.SetOwnership(eNoOwnership);
    m_Queue->SetMonitorSocket(socket.GetSOCK());
}

void CNetScheduleHandler::ProcessPrintQueue(CSocket& socket)
{
    CNetScheduleClient::EJobStatus 
        job_status = CNetScheduleClient::StringToStatus(m_JobReq.job_key_str);

    if (job_status == CNetScheduleClient::eJobNotFound) {
        string err_msg = "Status unknown: " + m_JobReq.job_key_str;
        WriteMsg(socket, "ERR:", err_msg.c_str());
        return;
    }

    SOCK sk = socket.GetSOCK();
    socket.SetOwnership(eNoOwnership);
    socket.Reset(0, eTakeOwnership, eCopyTimeoutsToSOCK);

    CConn_SocketStream ios(sk);  // sock is being passed and used exclusively

    m_Queue->PrintQueue(ios, 
                        job_status,
                        m_Server->GetHost(),
                        m_Server->GetPort());

    ios << "OK:END";
}


void CNetScheduleHandler::ProcessDump(CSocket& socket)
{
    WriteMsg(socket, "OK:", NETSCHEDULED_VERSION);
    if (m_JobReq.job_key_str.empty()) {
        WriteMsg(socket, "OK:", "[Job status matrix]:");

        SOCK sk = socket.GetSOCK();
        socket.SetOwnership(eNoOwnership);
        socket.Reset(0, eTakeOwnership, eCopyTimeoutsToSOCK);

        CConn_SocketStream ios(sk);  // sock is being passed and used exclusively

        m_Queue->PrintJobStatusMatrix(ios);

        ios << "OK:[Job DB]:\n";
        m_Queue->PrintAllJobDbStat(ios);
        ios << "OK:END";
    } else {
        try {
            unsigned job_id = CNetSchedule_GetJobId(m_JobReq.job_key_str);

            CNetScheduleClient::EJobStatus status = m_Queue->GetStatus(job_id);
            
            string st_str = CNetScheduleClient::StatusToString(status);
            st_str = "[Job status matrix]:" + st_str;
            WriteMsg(socket, "OK:", st_str.c_str());

            SOCK sk = socket.GetSOCK();
            socket.SetOwnership(eNoOwnership);
            socket.Reset(0, eTakeOwnership, eCopyTimeoutsToSOCK);

            CConn_SocketStream ios(sk);

            ios << "OK:[Job DB]:\n";
            m_Queue->PrintJobDbStat(job_id, ios);
            ios << "OK:END";
        } 
        catch (CException& ex)
        {
            // dump by status
            CNetScheduleClient::EJobStatus 
                job_status = CNetScheduleClient::StringToStatus(
                    m_JobReq.job_key_str);

            if (job_status == CNetScheduleClient::eJobNotFound) {
                string err_msg = "Status unknown: " + m_JobReq.job_key_str;
                WriteMsg(socket, "ERR:", err_msg.c_str());
                return;
            }

            SOCK sk = socket.GetSOCK();
            socket.SetOwnership(eNoOwnership);
            socket.Reset(0, eTakeOwnership, eCopyTimeoutsToSOCK);

            CConn_SocketStream ios(sk);  // sock is being passed and used exclusively
/*
            m_Queue->PrintQueue(ios, 
                                job_status,
                                m_Server->GetHost(),
                                m_Server->GetPort());
*/
            ios << "OK:END";

        }
    }
}


void CNetScheduleHandler::ProcessReloadConfig(CSocket& socket)
{
    CNcbiApplication* app = CNcbiApplication::Instance();

    bool reloaded = app->ReloadConfig(CMetaRegistry::fReloadIfChanged);
    if (reloaded) {
        unsigned tmp;
        m_Server->GetQueueDB()->ReadConfig(app->GetConfig(), &tmp);
    }
    WriteMsg(socket, "OK:", "");
}


void CNetScheduleHandler::ProcessStatistics(CSocket& socket)
{
    socket.DisableOSSendDelay(false);

    WriteMsg(socket, "OK:", NETSCHEDULED_VERSION);
    string started = "Started: " + m_Server->GetStartTime().AsString();
    WriteMsg(socket, "OK:", started.c_str());

    for (int i = CNetScheduleClient::ePending; 
         i < CNetScheduleClient::eLastStatus; ++i) {
             CNetScheduleClient::EJobStatus st = 
                                (CNetScheduleClient::EJobStatus) i;
             unsigned count = m_Queue->CountStatus(st);


             string st_str = CNetScheduleClient::StatusToString(st);
             st_str += ": ";
             st_str += NStr::UIntToString(count);

             WriteMsg(socket, "OK:", st_str.c_str(), true, false);

             CNetScheduler_JobStatusTracker::TBVector::statistics 
                 bv_stat;
             m_Queue->StatusStatistics(st, &bv_stat);
             st_str = "   bit_blk="; 
                st_str.append(NStr::UIntToString(bv_stat.bit_blocks));
             st_str += "; gap_blk=";
                st_str.append(NStr::UIntToString(bv_stat.gap_blocks));
             st_str += "; mem_used=";
                st_str.append(NStr::UIntToString(bv_stat.memory_used));
             WriteMsg(socket, "OK:", st_str.c_str());
             
    } // for

    if (m_JobReq.job_key_str == "ALL") {

        unsigned db_recs = m_Queue->CountRecs();
        string recs = "Records:";
        recs += NStr::UIntToString(db_recs);
        WriteMsg(socket, "OK:", recs.c_str());
        WriteMsg(socket, "OK:", "[Database statistics]:");

        {{
            CNcbiOstrstream ostr;
            m_Queue->PrintStat(ostr);
            ostr << ends;

            char* stat_str = ostr.str();
    //        size_t os_str_len = ostr.pcount()-1;
    //        stat_str[os_str_len] = 0;
            try {
                WriteMsg(socket, "OK:", stat_str, true, false);
            } catch (...) {
                ostr.freeze(false);
                throw;
            }
            ostr.freeze(false);
        }}

    }

    WriteMsg(socket, "OK:", "[Worker node statistics]:");

    {{
        CNcbiOstrstream ostr;
        m_Queue->PrintNodeStat(ostr);
        ostr << ends;

        char* stat_str = ostr.str();
        try {
            WriteMsg(socket, "OK:", stat_str, true, false);
        } catch (...) {
            ostr.freeze(false);
            throw;
        }
        ostr.freeze(false);
    }}

    {{
        WriteMsg(socket, "OK:", "[Configured job submitters]:");
        CNcbiOstrstream ostr;
        m_Queue->PrintSubmHosts(ostr);
        ostr << ends;

        char* stat_str = ostr.str();
        try {
            WriteMsg(socket, "OK:", stat_str, true, false);
        } catch (...) {
            ostr.freeze(false);
            throw;
        }
        ostr.freeze(false);
    }}

    {{
        WriteMsg(socket, "OK:", "[Configured workers]:");
        CNcbiOstrstream ostr;
        m_Queue->PrintWNodeHosts(ostr);
        ostr << ends;

        char* stat_str = ostr.str();
        try {
            WriteMsg(socket, "OK:", stat_str, true, false);
        } catch (...) {
            ostr.freeze(false);
            throw;
        }
        ostr.freeze(false);
    }}

    WriteMsg(socket, "OK:", "END");
}

void CNetScheduleHandler::ProcessLog(CSocket& socket)
{
    const char* str = m_JobReq.job_key_str.c_str();
    if (NStr::strcasecmp(str, "ON") == 0) {
        m_Server->SetLogging(true);
        LOG_POST("Logging turned ON");
    } else if (NStr::strcasecmp(str, "OFF") == 0) {
        m_Server->SetLogging(false);
        LOG_POST("Logging turned OFF");
    } else {
        WriteMsg(socket, "ERR:", "Incorrect LOG syntax");
    }
    WriteMsg(socket, "OK:", "");
}

void CNetScheduleHandler::ProcessShutdown(CSocket& socket)
{
    string admin_host = socket.GetPeerAddress();
    size_t pos = admin_host.find_first_of(':');
    if (pos != string::npos) {
        admin_host = admin_host.substr(0, pos);
    }
    unsigned ha = CSocketAPI::gethostbyname(admin_host);
    if (!m_Server->GetAccessList().IsRestrictionSet() || // no control
        m_Server->GetAccessList().IsAllowed(ha)) {
        string msg = "Shutdown request... ";
        msg += admin_host;
        msg += " ";
        msg += CTime(CTime::eCurrent).AsString();
        LOG_POST(Info << msg);
        m_Server->SetShutdownFlag();
        WriteMsg(socket, "OK:", "");
    } else {
        WriteMsg(socket, "ERR:", "Shutdown access denied.");
        LOG_POST(Warning << "Shutdown request denied: " << admin_host);
    }
}

void CNetScheduleHandler::ProcessVersion(CSocket& socket) {
    WriteMsg(socket, "OK:", NETSCHEDULED_VERSION);
}

/// NetSchedule command codes
/// Here I use 4 byte strings packed into integer for int comparison (faster)
///
/// @internal
///
struct SNS_Commands
{
    unsigned  JXCG;
    unsigned  VERS;
    unsigned  JRTO;
    unsigned  PUT_;
    unsigned  SUBM;
    unsigned  STAT;
    unsigned  SHUT;
    unsigned  GET_;
    unsigned  GET0;
    unsigned  MPUT;
    unsigned  MGET;
    unsigned  MONI;
    unsigned  WGET;
    unsigned  CANC;
    unsigned  DROJ;
    unsigned  DUMP;
    unsigned  FPUT;
    unsigned  RECO;
    unsigned  RETU;
    unsigned  QUIT;
    unsigned  QLST;
    unsigned  QPRT;
    unsigned  BSUB;
    unsigned  REGC;
    unsigned  URGC;
    unsigned  FRES;
    unsigned  JDEX;
    unsigned  STSN;


    SNS_Commands()
    {
        ::memcpy(&JXCG, "JXCG", 4);
        ::memcpy(&VERS, "VERS", 4);
        ::memcpy(&JRTO, "JRTO", 4);
        ::memcpy(&PUT_, "PUT ", 4);
        ::memcpy(&SUBM, "SUBM", 4);
        ::memcpy(&STAT, "STAT", 4);
        ::memcpy(&SHUT, "SHUT", 4);
        ::memcpy(&GET_, "GET ", 4);
        ::strcpy((char*)&GET0, "GET");
        ::memcpy(&MPUT, "MPUT", 4);
        ::memcpy(&MGET, "MGET", 4);
        ::memcpy(&MONI, "MONI", 4);
        ::memcpy(&WGET, "WGET", 4);
        ::memcpy(&CANC, "CANC", 4);
        ::memcpy(&DROJ, "DROJ", 4);
        ::memcpy(&DUMP, "DUMP", 4);
        ::memcpy(&FPUT, "FPUT", 4);
        ::memcpy(&RECO, "RECO", 4);
        ::memcpy(&RETU, "RETU", 4);
        ::memcpy(&QUIT, "QUIT", 4);
        ::memcpy(&QLST, "QLST", 4);
        ::memcpy(&QPRT, "QPRT", 4);
        ::memcpy(&BSUB, "BSUB", 4);
        ::memcpy(&REGC, "REGC", 4);
        ::memcpy(&URGC, "URGC", 4);
        ::memcpy(&FRES, "FRES", 4);
        ::memcpy(&JDEX, "JDEX", 4);
        ::memcpy(&STSN, "STSN", 4);
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
static SNS_Commands  s_NS_cmd_codes;

void CNetScheduleHandler::ParseRequest(const char* reqstr, SJS_Request* req)
{
    // Request formats and types:
    //
    // 1. SUBMIT "NCID_01_1..." ["Progress msg"] [udp_port notif_wait] 
    //           [aff="Affinity token"] 
    //           [out="out_file_name"] [err="err_file_name"] [msk=value]
    // 2. CANCEL JSID_01_1
    // 3. STATUS JSID_01_1
    // 4. GET udp_port
    // 5. PUT JSID_01_1 EndStatus "NCID_01_2..."
    // 6. RETURN JSID_01_1
    // 7. SHUTDOWN
    // 8. VERSION
    // 9. LOG [ON/OFF]
    // 10.STAT [ALL]
    // 11.QUIT 
    // 12.DROPQ
    // 13.WGET udp_port_number timeout
    // 14.JRTO JSID_01_1 timeout
    // 15.DROJ JSID_01_1
    // 16.FPUT JSID_01_1 "error message" "output" ret_code
    // 17.MPUT JSID_01_1 "progress message"
    // 18.MGET JSID_01_1
    // 19.MONI
    // 20.DUMP [JSID_01_1]
    // 21.RECO
    // 22.QLST
    // 23.BSUB
    // 24.JXCG [JSID_01_1 EndStatus "NCID_01_2..."]
    // 25.REGC udp_port
    // 26.URGC udp_port
    // 27.QPRT Status
    // 28.FRES JSID_01_1
    // 29.JDEX JSID_01_1 timeout
    // 30.STSN [aff="Affinity token"]


    const char* s = reqstr;

    switch (*s) {

    case 'S':
        if (SNS_Commands::IsCmd(s_NS_cmd_codes.STAT, s)) {
            s += 4;
            // STAT - statictics or STATUS ?
            if (*s == 'U') {
                req->req_type = eStatusJob;
                s += 2;
                NS_SKIPSPACE(s)
                NS_GETSTRING(s, req->job_key_str)
            
                if (req->job_key_str.empty()) {
                    NS_RETURN_ERROR("Misformed STATUS request")
                }
            } else {
                req->req_type = eStatistics;
                NS_SKIPSPACE(s)
                NS_RETEND(s)
                NS_GETSTRING(s, req->job_key_str)
                return;
            }
            return;
        }

        if (SNS_Commands::IsCmd(s_NS_cmd_codes.SUBM, s)) {
            req->req_type = eSubmitJob;
            s += 6;
            NS_SKIPSPACE(s)
            if (*s !='"') {
                NS_RETURN_ERROR("Misformed SUBMIT request")
            }
            char *ptr = req->input;
            for (++s; true; ++s) {
                if (*s == '"' && *(s-1) != '\\') break;
                NS_CHECKEND(s, "Misformed SUBMIT request")
                NS_CHECKSIZE(ptr-req->input, kNetScheduleMaxDBDataSize);
                *ptr++ = *s;
            }
            ++s;
            *ptr = 0;

            NS_SKIPSPACE(s)

            // optional progress message

            if (*s == '"') {
                ptr = req->progress_msg;

                for (++s; true; ++s) {
                    if (*s == '"' && *(s-1) != '\\') break;
                    NS_CHECKEND(s, "Misformed SUBMIT request")
                    NS_CHECKSIZE(ptr-req->progress_msg, 
                                 kNetScheduleMaxDBDataSize);
                    *ptr++ = *s;
                }
                ++s;
                *ptr = 0;

                NS_SKIPSPACE(s)
            }

            // optional UDP notification parameters

            NS_RETEND(s)

            if (isdigit((unsigned char)(*s))) {
                req->port = (unsigned)atoi(s);
                NS_SKIPNUM(s)
                NS_SKIPSPACE(s)
                NS_RETEND(s)
            }

            if (isdigit((unsigned char)(*s))) {
                req->timeout = atoi(s);
                NS_SKIPNUM(s)
                NS_SKIPSPACE(s)
                NS_RETEND(s)
            }

            // optional affinity token
            if (strncmp(s, "aff=", 4) == 0) {
                s += 4;
                if (*s != '"') {
                    NS_RETURN_ERROR("Misformed SUBMIT request")
                }
                char *ptr = req->affinity_token;
                for (++s; *s != '"'; ++s) {
                    NS_CHECKEND(s, "Misformed SUBMIT request")
                    NS_CHECKSIZE(ptr-req->affinity_token, 
                                 kNetScheduleMaxDBDataSize);
                    *ptr++ = *s;
                }
                *ptr = 0;
                ++s;

                NS_SKIPSPACE(s)
            }

            // optional job output parameter
            if (strncmp(s, "out=", 4) == 0) {
                s += 4;
                if (*s != '"') {
                    NS_RETURN_ERROR("Misformed SUBMIT request")
                }
                char *ptr = req->cout;
                for (++s; *s != '"'; ++s) {
                    NS_CHECKEND(s, "Misformed SUBMIT request")
                    NS_CHECKSIZE(ptr-req->cout, 
                                 kNetScheduleMaxDBDataSize);
                    *ptr++ = *s;
                }
                *ptr = 0;
                ++s;
                NS_SKIPSPACE(s)
            }

            // optional job cerr parameter
            if (strncmp(s, "err=", 4) == 0) {
                s += 4;
                if (*s != '"') {
                    NS_RETURN_ERROR("Misformed SUBMIT request")
                }
                char *ptr = req->cerr;
                for (++s; *s != '"'; ++s) {
                    NS_CHECKEND(s, "Misformed SUBMIT request")
                    NS_CHECKSIZE(ptr-req->cerr, 
                                 kNetScheduleMaxDBDataSize);
                    *ptr++ = *s;
                }
                *ptr = 0;
            }

            // optional job msk parameter
            if (strncmp(s, "msk=", 4) == 0) {
                s += 4;
                if (!isdigit(*s)) {
                    NS_RETURN_ERROR("Misformed SUBMIT request")
                }
                req->job_mask = atoi(s);
            }

            return;
        }

        if (SNS_Commands::IsCmd(s_NS_cmd_codes.STSN, s)) {
            req->req_type = eStatusSnapshot;
            s += 4;

            NS_SKIPSPACE(s)
            
            if (!*s) {
                req->affinity_token[0] = 0;
                return;
            }

            if (strncmp(s, "aff=", 4) == 0) {
                s += 4;
                if (*s != '"') {
                    NS_RETURN_ERROR("Misformed Status Snapshot request")
                }
                char *ptr = req->affinity_token;
                for (++s; *s != '"'; ++s) {
                    NS_CHECKEND(s, "Misformed Status Snapshot request")
                    NS_CHECKSIZE(ptr - req->affinity_token, 
                                 kNetScheduleMaxDBDataSize);
                    *ptr++ = *s;
                }
                *ptr = 0;
                ++s;
                NS_SKIPSPACE(s)
            }

            return;
        }


        if (SNS_Commands::IsCmd(s_NS_cmd_codes.SHUT, s)) {
            req->req_type = eShutdown;
            return;
        }

        break;

    case 'G':
        if (SNS_Commands::IsCmd(s_NS_cmd_codes.GET_, s)
            ||
            SNS_Commands::IsCmd(s_NS_cmd_codes.GET0, s)
            ||
            (strncmp(s, "GET", 3) == 0)
           ) {
            req->req_type = eGetJob;            
            s += 3;
            NS_SKIPSPACE(s)

            if (*s) {
                int port = atoi(s);
                if (port > 0) {
                    req->port = (unsigned)port;
                }
            }
            return;
        }

        break;
    case 'M':
        if (SNS_Commands::IsCmd(s_NS_cmd_codes.MPUT, s)) {
            req->req_type = ePutProgressMsg;
            s += 4;

            NS_SKIPSPACE(s)
            NS_GETSTRING(s, req->job_key_str)
            NS_SKIPSPACE(s)

            if (*s !='"') {
                NS_RETURN_ERROR("Misformed MPUT request")
            }
            char *ptr = req->progress_msg;
            for (++s; true; ++s) {
                if (*s == '"' && *(s-1) != '\\') break;
                NS_CHECKEND(s, "Misformed MPUT request")
                NS_CHECKSIZE(ptr-req->progress_msg, kNetScheduleMaxDBDataSize);
                *ptr++ = *s;            
            }
            *ptr = 0;

            return;
        }
        if (SNS_Commands::IsCmd(s_NS_cmd_codes.MGET, s)) {
            req->req_type = eGetProgressMsg;
            s += 4;
            NS_SKIPSPACE(s)
            if (*s != 0) {
                NS_GETSTRING(s, req->job_key_str)
            }
            return;
        }
        if (SNS_Commands::IsCmd(s_NS_cmd_codes.MONI, s)) {
            req->req_type = eMonitor;
            return;
        }

        break;

    case 'J':
        if (SNS_Commands::IsCmd(s_NS_cmd_codes.JXCG, s)) {
            req->req_type = eJobExchange;
            s += 4;
            NS_SKIPSPACE(s)
            if (*s == 0) {
                req->job_key_str.erase();
                return;
            }
            goto parse_put_params;
        }
        if (SNS_Commands::IsCmd(s_NS_cmd_codes.JRTO, s)) {
            req->req_type = eJobRunTimeout;
            
            s += 4;
            NS_SKIPSPACE(s)
            NS_CHECKEND(s, "Misformed job run timeout request")
            NS_GETSTRING(s, req->job_key_str)
            
            if (req->job_key_str.empty()) {
                NS_RETURN_ERROR("Misformed job run timeout request")
            }
            NS_SKIPSPACE(s)
            NS_CHECKEND(s, "Misformed job run timeout request")

            int timeout = atoi(s);
            if (timeout < 0) {
                NS_RETURN_ERROR(
                    "Invalid job run timeout request: incorrect timeout value")
            }
            req->timeout = timeout;
            return;
        }
        if (SNS_Commands::IsCmd(s_NS_cmd_codes.JDEX, s)) {
            req->req_type = eJobDelayExpiration;
            s += 4;
            NS_SKIPSPACE(s)
            NS_CHECKEND(s, "Misformed job delay expiration request")
            NS_GETSTRING(s, req->job_key_str)
            
            if (req->job_key_str.empty()) {
                NS_RETURN_ERROR("Misformed job delay expiration request")
            }
            NS_SKIPSPACE(s)
            NS_CHECKEND(s, "Misformed job delay expiration request")

            int timeout = atoi(s);
            if (timeout < 0) {
                NS_RETURN_ERROR(
                    "Invalid job run timeout request: incorrect timeout value")
            }
            req->timeout = timeout;
            return;
        }

        break;

    case 'P':
        if (SNS_Commands::IsCmd(s_NS_cmd_codes.PUT_, s)) {
            req->req_type = ePutJobResult;
            s += 3;
            NS_SKIPSPACE(s)
        parse_put_params:
            NS_GETSTRING(s, req->job_key_str)
            NS_SKIPSPACE(s)

            // return code
            if (*s) {
                req->job_return_code = atoi(s);
                // skip digits
                for (; isdigit((unsigned char)(*s)); ++s) {}
                if (!isspace((unsigned char)(*s))) {
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
            char *ptr = req->output;
            for (++s; true; ++s) {
                if (*s == '"' && *(s-1) != '\\') break;
                NS_CHECKEND(s, "Misformed PUT request")
                NS_CHECKSIZE(ptr-req->output, kNetScheduleMaxDBDataSize);
                *ptr++ = *s;            
            }
            *ptr = 0;

            return;
        }

        break;
    case 'W':
        if (SNS_Commands::IsCmd(s_NS_cmd_codes.WGET, s)) {
            req->req_type = eWaitGetJob;
            
            s += 4;
            NS_SKIPSPACE(s)
            NS_CHECKEND(s, "Misformed WGET request")

            int port = atoi(s);
            if (port > 0) {
                req->port = (unsigned)port;
            }

            for (; *s && isdigit((unsigned char)(*s)); ++s) {}

            NS_CHECKEND(s, "Misformed WGET request")
            NS_SKIPSPACE(s)
            NS_CHECKEND(s, "Misformed WGET request")

            int timeout = atoi(s);
            if (timeout <= 0) {
                timeout = 60;
            }
            req->timeout = timeout;
            return;
        }
        break;
    case 'C':
        if (SNS_Commands::IsCmd(s_NS_cmd_codes.CANC, s)) {
            req->req_type = eCancelJob;
            s += 6;
            NS_SKIPSPACE(s)
            NS_GETSTRING(s, req->job_key_str)
            
            if (req->job_key_str.empty()) {
                NS_RETURN_ERROR("Misformed CANCEL request")
            }
            return;
        }

        break;
    case 'D':
        if (SNS_Commands::IsCmd(s_NS_cmd_codes.DROJ, s)) {
            req->req_type = eDropJob;
            s += 4;
            NS_SKIPSPACE(s)
            NS_GETSTRING(s, req->job_key_str)
            
            if (req->job_key_str.empty()) {
                NS_RETURN_ERROR("Misformed drop job request")
            }
            return;
        }

        if (strncmp(s, "DROPQ", 4) == 0) {
            req->req_type = eDropQueue;
            return;
        }

        if (SNS_Commands::IsCmd(s_NS_cmd_codes.DUMP, s)) {
            req->req_type = eDumpQueue;
            s += 4;

            NS_SKIPSPACE(s)
            NS_GETSTRING(s, req->job_key_str)
            return;
        }

        break;
    case 'F':
        if (SNS_Commands::IsCmd(s_NS_cmd_codes.FPUT, s)) {
            req->req_type = ePutJobFailure;
            s += 4;
            NS_SKIPSPACE(s)
            NS_GETSTRING(s, req->job_key_str)

            if (req->job_key_str.empty()) {
                NS_RETURN_ERROR("Misformed put error request")
            }

            if (!*s) return;
            NS_SKIPSPACE(s)
            if (!*s) return;

            if (*s !='"') {
                NS_RETURN_ERROR("Misformed PUT error request")
            }
            for (++s; true; ++s) {
                if (*s == '"' && *(s-1) != '\\') break;
                NS_CHECKEND(s, "Misformed PUT error request")
                req->err_msg.push_back(*s);
            }
            ++s;

            if (!*s) return;
            NS_SKIPSPACE(s)
            if (!*s) return;

            if (*s !='"') {
                NS_RETURN_ERROR("Misformed PUT error request")
            }

            char *ptr = req->output;
            for (++s; true; ++s) {
                if (*s == '"' && *(s-1) != '\\') break;
                NS_CHECKEND(s, "Misformed PUT error request")
                NS_CHECKSIZE(ptr-req->output, kNetScheduleMaxDBDataSize);
                *ptr++ = *s;            
            }
            *ptr = 0;

            ++s;
            if (!*s) return;
            NS_SKIPSPACE(s)
            if (!*s) return;

            req->job_return_code = atoi(s);
            return;
        }

        if (SNS_Commands::IsCmd(s_NS_cmd_codes.FRES, s)) {
            req->req_type = ePutJobFailure;
            s += 4;
            NS_SKIPSPACE(s)
            NS_GETSTRING(s, req->job_key_str)
            
            if (req->job_key_str.empty()) {
                NS_RETURN_ERROR("Misformed force reschedule request")
            }
            return;
        }

        break;

    case 'R':
        if (SNS_Commands::IsCmd(s_NS_cmd_codes.RETU, s)) {
            req->req_type = eReturnJob;
            s += 6;
            NS_SKIPSPACE(s)
            NS_CHECKEND(s, "Misformed job return request")
            NS_GETSTRING(s, req->job_key_str)

            return;
        }
        if (SNS_Commands::IsCmd(s_NS_cmd_codes.RECO, s)) {
            req->req_type = eReloadConfig;
            return;
        }
        if (SNS_Commands::IsCmd(s_NS_cmd_codes.REGC, s)) {
            req->req_type = eRegisterClient;
            s += 4;
            NS_SKIPSPACE(s)
            NS_CHECKEND(s, "Misformed Register request")

            int port = atoi(s);
            if (port > 0) {
                req->port = (unsigned)port;
            }
            return;
        }
        break;
    case 'Q':
        if (SNS_Commands::IsCmd(s_NS_cmd_codes.QUIT, s)) {
            req->req_type = eQuitSession;
            return;
        }
        if (SNS_Commands::IsCmd(s_NS_cmd_codes.QLST, s)) {
            req->req_type = eQList;
            return;
        }
        if (SNS_Commands::IsCmd(s_NS_cmd_codes.QPRT, s)) {
            req->req_type = ePrintQueue;
            s += 4;
            NS_SKIPSPACE(s)
            NS_CHECKEND(s, "Misformed QPRT request")
            NS_GETSTRING(s, req->job_key_str)
            return;
        }
        break;
    case 'V':
        if (SNS_Commands::IsCmd(s_NS_cmd_codes.VERS, s)) {
            req->req_type = eVersion;
            return;
        }
        break;
    case 'L':
        if (strncmp(s, "LOG", 3) == 0) {
            req->req_type = eLogging;
            s += 3;
            NS_SKIPSPACE(s)
            NS_CHECKEND(s, "Misformed LOG request")
            NS_GETSTRING(s, req->job_key_str)
            return;
        } // LOG
        break;
    case 'B':
        if (SNS_Commands::IsCmd(s_NS_cmd_codes.BSUB, s)) {
            req->req_type = eSubmitBatch;
            s += 4;
            return;
        }
        break;
    case 'U':
        if (SNS_Commands::IsCmd(s_NS_cmd_codes.URGC, s)) {
            req->req_type = eUnRegisterClient;
            s += 4;
            NS_SKIPSPACE(s)
            NS_CHECKEND(s, "Misformed UnRegister request")

            int port = atoi(s);
            if (port > 0) {
                req->port = (unsigned)port;
            }
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



void CNetScheduleHandler::WriteMsg(CSocket&       sock, 
                                   const char*    prefix, 
                                   const char*    msg,
                                   bool           comm_control,
                                   bool           msg_size_control)
{
    SThreadData* tdata = GetThreadData();
    if (tdata == 0) {
        LOG_POST(Error << "Cannot deliver message (no TLS buffer):"
                       << prefix << msg);
        return;
    }
    size_t msg_length = 0;
    char* ptr = tdata->msg_buf;

    for (; *prefix; ++prefix) {
        *ptr++ = *prefix;
        ++msg_length;
    }
    for (; *msg; ++msg) {
        *ptr++ = *msg;
        ++msg_length;
        if (msg_size_control && msg_length >= kMaxMessageSize) {
            LOG_POST(Error << "Message too large:" << msg);
            _ASSERT(0);
            return;
        }
    }
    *ptr++ = '\r';
    *ptr++ = '\n';
    msg_length = ptr - tdata->msg_buf;

    size_t n_written;
    EIO_Status io_st = 
        sock.Write(tdata->msg_buf, msg_length, &n_written);
    if (comm_control && io_st) {
        NCBI_THROW(CNetServiceException, 
                   eCommunicationError, "Socket write error.");
    }

}

void CNetScheduleHandler::x_MakeLogMessage(CSocket& socket)
{
    string peer = socket.GetPeerAddress();
    // get only host name
    string::size_type offs = peer.find_first_of(":");
    if (offs != string::npos) {
        peer.erase(offs, peer.length());
    }

    string& lmsg = m_ThreadData.lmsg;
    lmsg.erase();

    lmsg += peer;
    lmsg += ';';
    lmsg += m_LocalTimer.GetLocalTime().AsString();
    lmsg += ';';
    lmsg += m_ThreadData.auth;
    lmsg += ';';
    lmsg += m_ThreadData.queue;
    lmsg += ';';
    lmsg += m_ThreadData.request;
    lmsg += "\n";
}

void CNetScheduleHandler::x_MakeGetAnswer(const char*   key_buf,
                                          const char*   jout,
                                          const char*   jerr,
                                          unsigned      job_mask)
{
    _ASSERT(jout);
    _ASSERT(jerr);

    m_ThreadData.answer = key_buf;
    m_ThreadData.answer.append(" \"");
    m_ThreadData.answer.append(m_JobReq.input);
    m_ThreadData.answer.append("\"");
    m_ThreadData.answer.append(" \"");
    m_ThreadData.answer.append(jout);
    m_ThreadData.answer.append("\"");
    m_ThreadData.answer.append(" \"");
    m_ThreadData.answer.append(jerr);
    m_ThreadData.answer.append("\"");
    m_ThreadData.answer.append(" ");
    m_ThreadData.answer.append(NStr::UIntToString(job_mask));
}


//////////////////////////////////////////////////////////////////////////
// CNetScheduleServer implementation
CNetScheduleServer::CNetScheduleServer(unsigned int    port,
                                       CQueueDataBase* qdb,
                                       unsigned        max_threads,
                                       unsigned        init_threads,
                                       unsigned        network_timeout,
                                       bool            is_log)
: CThreadedServer(port),
    m_Shutdown(false),
    m_SigNum(0),
    m_InactivityTimeout(network_timeout),
    m_StartTime(CTime::eCurrent),
    m_AccessLog("ns_access.log", 100 * 1024 * 1024)
{
    m_QueueDB = qdb;
/*
    char hostname[256];
    int status = SOCK_gethostname(hostname, sizeof(hostname));
    if (status == 0) {
        m_Host = hostname;
    }
*/
    m_Host = CSocketAPI::gethostname();
    m_HostNetAddr = CSocketAPI::gethostbyname(kEmptyStr);
    m_LocalNetAddr = CSocketAPI::GetLoopbackAddress();


    m_MaxThreads = max_threads ? max_threads : 25;
    m_InitThreads = init_threads ? 
        (init_threads < m_MaxThreads ? init_threads : 2) : 10;
    // TODO: the following does not make sense for threadpool implementation
    m_QueueSize = m_MaxThreads * 2;

    s_netschedule_server = this;

    m_AcceptTimeout = &m_ThrdSrvAcceptTimeout;

    if (is_log) {
        m_LogFlag.Set(1);
    } else {
        m_LogFlag.Set(0);
    }
}

CNetScheduleServer::~CNetScheduleServer()
{
    delete m_QueueDB;
}

void CNetScheduleServer::Process(SOCK sock)
{
    CSocket socket;
    socket.Reset(sock, eTakeOwnership, eCopyTimeoutsFromSOCK);

    // Transitional
    auto_ptr<CNetScheduleHandler> handler(new CNetScheduleHandler(this));
    handler->SetSocket(&socket);
    SThreadData* tdata = handler->GetThreadData();

    CNetScheduleMonitor* monitor = 0;

    try {
        // Process requests
        handler->Init(socket, m_HostNetAddr, m_LocalNetAddr);
        for (unsigned cmd = 0; true; (cmd>=100?cmd=0:++cmd)) {
            handler->OnRead();
            if (handler->IsDone()) break;
            // abort permanent connection session if shutdown requested
            //
            // the cmd counter condition here is used to make sure that
            // client is not interrupted immediately but had a chance 
            // to return job results to the queue (if it is a worker)
            // (rather useless attempt to shutdown gracefully)
            if ((cmd > 10) && ShutdownRequested()) {
                handler->WriteMsg(socket, "ERR:", 
                    "NetSchedule server is shutting down. Session aborted.");
                break;
            }

        } // for 
    } 
    catch (CNetScheduleException &ex)
    {
        if (ex.GetErrCode() == CNetScheduleException::eUnknownQueue) {
            handler->WriteMsg(socket, "ERR:", "QUEUE_NOT_FOUND", false /*no control*/);
        } else {
            string err = NStr::PrintableString(ex.what());
            handler->WriteMsg(socket, "ERR:", err.c_str(), false /*no control*/);
        }
        string msg = "Server error: ";
        msg += ex.what();
        msg += " Peer=";
        msg += socket.GetPeerAddress();
        msg += " ";
        msg += tdata->auth;
        msg += tdata->request;

        ERR_POST(msg);

        msg = m_LocalTimer.GetLocalTime().AsString() + msg;
        if (monitor = handler->GetMonitor()) {
            monitor->SendString(msg);
        }
    }
    catch (CNetServiceException &ex)
    {
        handler->WriteMsg(socket, "ERR:", ex.what(), false /*no control*/);
        string msg = "Server error: ";
        msg += ex.what();
        msg += " Peer=";
        msg += socket.GetPeerAddress();
        msg += " ";
        msg += tdata->auth;
        msg += tdata->request;

        ERR_POST(msg);

        msg = m_LocalTimer.GetLocalTime().AsString() + msg;
        if (monitor = handler->GetMonitor()) {
            monitor->SendString(msg);
        }
    }
    catch (CBDB_ErrnoException& ex)
    {
        int err_no = ex.BDB_GetErrno();
        if (err_no == DB_RUNRECOVERY) {
            string msg = "Fatal Berkeley DB error: DB_RUNRECOVERY. ";
            msg += "Emergency shutdown initiated!";
            msg += ex.what();
            msg += " Peer=";
            msg += socket.GetPeerAddress();
            msg += " ";
            msg += tdata->auth;
            msg += tdata->request;
        
            ERR_POST(msg);

            msg = m_LocalTimer.GetLocalTime().AsString() + msg;
            if (monitor = handler->GetMonitor()) {
                monitor->SendString(msg);
            }

            SetShutdownFlag();
        } else {
            string msg = "BDB error:";
            msg += ex.what();
            msg += " Peer=";
            msg += socket.GetPeerAddress();
            msg += " ";
            msg += tdata->auth;
            msg += tdata->request;

            ERR_POST(msg);

            msg = m_LocalTimer.GetLocalTime().AsString() + msg;
            if (monitor = handler->GetMonitor()) {
                monitor->SendString(msg);
            }

            string err = "Internal database error:";
            err += ex.what();
            err = NStr::PrintableString(err);
            handler->WriteMsg(socket, "ERR:", err.c_str(), false /*no control*/);
        }
    }
    catch (CBDB_Exception &ex)
    {
        string err = "Internal database (BDB) error:";
        err += ex.what();
        err = NStr::PrintableString(err);
        handler->WriteMsg(socket, "ERR:", err.c_str(), false /*no control*/);

        string msg = "BDB error:";
        msg += ex.what();
        msg += " Peer=";
        msg += socket.GetPeerAddress();
        msg += " ";
        msg += tdata->auth;
        msg += tdata->request;

        ERR_POST(msg);

        msg = m_LocalTimer.GetLocalTime().AsString() + msg;
        if (monitor = handler->GetMonitor()) {
            monitor->SendString(msg);
        }
    }
    catch (exception& ex)
    {
        string msg = "Command execution error ";
        if (tdata) msg += tdata->request;
        msg += " ";
        msg += ex.what();
        string err = NStr::PrintableString(ex.what());
        handler->WriteMsg(socket, "ERR:", err.c_str(), false /*no control*/);

        msg = "STL exception:";
        msg += ex.what();
        msg += " Peer=";
        msg += socket.GetPeerAddress();
        msg += " ";
        msg += tdata->auth;
        msg += tdata->request;

        ERR_POST(msg);

        msg = m_LocalTimer.GetLocalTime().AsString() + msg;
        if (monitor = handler->GetMonitor()) {
            monitor->SendString(msg);
        }
    }
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

void CNetScheduleServer::SetAdminHosts(const string& host_names)
{
    m_AdminHosts.SetHosts(host_names);
}


///////////////////////////////////////////////////////////////////////


/// NetSchedule daemon application
///
/// @internal
///
class CNetScheduleDApp : public CNcbiApplication
{
public:
    CNetScheduleDApp() 
        : CNcbiApplication()
    {}
    void Init(void);
    int Run(void);
private:
    auto_ptr<CRotatingLogStream> m_ErrLog;
};

void CNetScheduleDApp::Init(void)
{
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostFlag(eDPF_DateTime);

    // Setup command line arguments and parameters
        
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "netscheduled");

    arg_desc->AddFlag("reinit", "Recreate the storage directory.");

    SetupArgDescriptions(arg_desc.release());

    CONNECT_Init(&GetConfig());
}


/// @internal
extern "C" void Threaded_Server_SignalHandler(int signum)
{
    if (s_netschedule_server && 
        (!s_netschedule_server->ShutdownRequested()) ) {
        s_netschedule_server->SetShutdownFlag(signum);
    }
}




int CNetScheduleDApp::Run(void)
{
    LOG_POST(NETSCHEDULED_VERSION);
    
    CArgs args = GetArgs();

    try {
        const CNcbiRegistry& reg = GetConfig();

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
                   
        m_ErrLog.reset(new CNetScheduleLogStream("ns_err.log", 25 * 1024 * 1024));     
        // All errors redirected to rotated log
        // from this moment on the server is silent...
        SetDiagStream(m_ErrLog.get());
        

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
            bdb_conf.GetString("netschedule", "path", CConfig::eErr_Throw);
        bool reg_reinit =
            reg.GetBool("server", "reinit", false, 0, IRegistry::eReturn);

        if (args["reinit"] || reg_reinit) {  // Drop the database directory
            CDir dir(db_path);
            dir.Remove();
            LOG_POST(Info << "Reinintialization. " << db_path 
                          << " removed. \n");
        }

        unsigned mem_size = (unsigned)
            bdb_conf.GetDataSize("netschedule", "mem_size", 
                                 CConfig::eErr_NoThrow, 0);
        int max_locks = 
            bdb_conf.GetInt("netschedule", "max_locks", 
                                 CConfig::eErr_NoThrow, 0);

        unsigned log_mem_size = (unsigned)
            bdb_conf.GetDataSize("netschedule", "log_mem_size", 
                                 CConfig::eErr_NoThrow, 0);
        LOG_POST(Info << "Mounting database at: " << db_path);

        unsigned max_threads =
            reg.GetInt("server", "max_threads", 25, 0, CNcbiRegistry::eReturn);

        // two transactions per thread should be enough
        unsigned max_trans = max_threads * 2;

        auto_ptr<CQueueDataBase> qdb(new CQueueDataBase());

        qdb->Open(db_path, mem_size, max_locks, log_mem_size, max_trans);


        int port = 
            reg.GetInt("server", "port", 9100, 0, CNcbiRegistry::eReturn);

        int udp_port = 
            reg.GetInt("server", "udp_port", 0, 0, CNcbiRegistry::eReturn);
        if (udp_port == 0) {
            udp_port = port + 1;
            LOG_POST(Info << "UDP notification port: " << udp_port);
        }
        if (udp_port < 1024 || udp_port > 65535) {
            LOG_POST(Error << "Invalid UDP port value: " << udp_port 
                           << ". Notification will be disabled.");
            udp_port = -1;
        }
        if (udp_port < 0) {
            LOG_POST(Info << "UDP notification disabled. ");
        }
        if (udp_port > 0) {
            qdb->SetUdpPort((unsigned short) udp_port);
        }



        // mount default queue
        string qname = "noname";
        LOG_POST(Info << "Mounting queue: " << qname);
        qdb->MountQueue(qname, 3600, 7, 3600, 1800, 
                        kEmptyStr, false/*do not delete done*/);


        // Scan and mount queues
        unsigned min_run_timeout = 3600;

        qdb->ReadConfig(reg, &min_run_timeout);

        LOG_POST(Info << "Running execution control every " 
                      << min_run_timeout << " seconds. ");
        min_run_timeout = min_run_timeout >= 0 ? min_run_timeout : 2;
        
        
        qdb->RunExecutionWatcherThread(min_run_timeout);
        qdb->RunPurgeThread();
        qdb->RunNotifThread();


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

        string admin_hosts =
            reg.GetString("server", "admin_host", kEmptyStr);
        if (!admin_hosts.empty()) {
            thr_srv->SetAdminHosts(admin_hosts);
        }

        NcbiCout << "Running server on port " << port << NcbiEndl;
        LOG_POST(Info << "Running server on port " << port);

        thr_srv->Run();

        LOG_POST("NetSchedule server stoped.");

    }
    catch (CBDB_ErrnoException& ex)
    {
        LOG_POST(Error << "Error: DBD errno exception:" << ex.what());
        return 1;
    }
    catch (CBDB_LibException& ex)
    {
        LOG_POST(Error << "Error: DBD library exception:" << ex.what());
        return 1;
    }
    catch (exception& ex)
    {
        LOG_POST(Error << "Error: STD exception:" << ex.what());
        return 1;
    }

    return 0;
}




int main(int argc, const char* argv[])
{

     return 
        CNetScheduleDApp().AppMain(argc, argv, 0, 
                           eDS_Default, "netscheduled.ini");
}

/*
 * ===========================================================================
 * $Log$
 * Revision 1.99  2006/09/27 21:26:06  joukovv
 * Thread-per-request is finally implemented. Interface changed to enable
 * streams, line-based message handler added, netscedule adapted.
 *
 * Revision 1.98  2006/09/21 21:28:59  joukovv
 * Consistency of memory state and database strengthened, ability to retry failed
 * jobs on different nodes (and corresponding queue parameter, failed_retries)
 * added, overall code regularization performed.
 *
 * Revision 1.97  2006/09/13 18:32:21  joukovv
 * Added (non-functional yet) framework for thread-per-request thread pooling model,
 * netscheduled.cpp refactored for this model; bug in bdb_cursor.cpp fixed.
 *
 * Revision 1.96  2006/08/28 19:14:45  didenko
 * Fixed a bug in GetJobDescr logic
 *
 * Revision 1.95  2006/07/19 15:53:34  kuznets
 * Extended database size to accomodate escaped strings
 *
 * Revision 1.94  2006/06/29 21:24:45  kuznets
 * cosmetics
 *
 * Revision 1.93  2006/06/29 21:09:33  kuznets
 * Added queue dump by status(pending, running, etc)
 *
 * Revision 1.92  2006/06/27 15:39:42  kuznets
 * Added int mask to jobs to carry flags(like exclusive)
 *
 * Revision 1.91  2006/06/26 13:46:01  kuznets
 * Fixed job expiration and restart mechanism
 *
 * Revision 1.90  2006/06/19 16:15:49  kuznets
 * fixed crash when working with affinity
 *
 * Revision 1.89  2006/06/19 14:32:21  kuznets
 * fixed bug in affinity status
 *
 * Revision 1.88  2006/06/07 13:00:01  kuznets
 * Implemented command to get status summary based on affinity token
 *
 * Revision 1.87  2006/05/22 18:01:06  didenko
 * Added sending ret_code and output from GetStatus method for jobs with failed status
 *
 * Revision 1.86  2006/05/22 16:51:04  kuznets
 * Fixed bug in reporting worker nodes
 *
 * Revision 1.85  2006/05/22 15:19:40  kuznets
 * Added return code to failure reporting
 *
 * Revision 1.84  2006/05/22 12:36:33  kuznets
 * Added output argument to PutFailure
 *
 * Revision 1.83  2006/05/17 17:04:55  ucko
 * Tweak ProcessStatus to fix compilation with GCC, which does not permit
 * goto statements to skip over variable initializations.
 *
 * Revision 1.82  2006/05/17 16:09:02  kuznets
 * Fixed retrieval of job input for Pending, Running, Returned jobs
 *
 * Revision 1.81  2006/05/11 14:31:51  kuznets
 * Fixed bug in job prolongation
 *
 * Revision 1.80  2006/05/10 15:59:06  kuznets
 * Implemented NS call to delay job expiration
 *
 * Revision 1.79  2006/05/08 15:54:35  ucko
 * Tweak settings-retrieval APIs to account for the fact that the
 * supplied default string value may be a reference to a temporary, and
 * therefore unsafe to return by reference.
 *
 * Revision 1.78  2006/05/08 11:24:52  kuznets
 * Implemented file redirection cout/cerr for worker nodes
 *
 * Revision 1.77  2006/05/04 15:36:35  kuznets
 * patch number inc
 *
 * Revision 1.76  2006/05/03 15:18:32  kuznets
 * Fixed deletion of done jobs
 *
 * Revision 1.75  2006/04/17 15:46:54  kuznets
 * Added option to remove job when it is done (similar to LSF)
 *
 * Revision 1.74  2006/04/14 12:43:28  kuznets
 * Fixed crash when deleting affinity records
 *
 * Revision 1.73  2006/04/10 15:16:24  kuznets
 * Collect detailed info on shutdown by signal
 *
 * Revision 1.72  2006/03/30 17:38:55  kuznets
 * Set max. transactions according to number of active threads
 *
 * Revision 1.71  2006/03/28 21:20:39  kuznets
 * Permanent connection interruption on shutdown
 *
 * Revision 1.70  2006/03/28 20:51:35  didenko
 * Added printing of the request content into the log file when an error is acquired.
 *
 * Revision 1.69  2006/03/27 15:26:07  didenko
 * Fixed request's input parsing
 *
 * Revision 1.68  2006/03/17 14:25:29  kuznets
 * Force reschedule (to re-try failed jobs)
 *
 * Revision 1.67  2006/03/16 19:39:02  kuznets
 * version increment
 *
 * Revision 1.66  2006/03/16 19:37:28  kuznets
 * Fixed possible race condition between client and worker
 *
 * Revision 1.65  2006/03/13 16:01:36  kuznets
 * Fixed queue truncation (transaction log overflow). Added commands to print queue selectively
 *
 * Revision 1.64  2006/02/23 20:05:10  kuznets
 * Added grid client registration-unregistration
 *
 * Revision 1.63  2006/02/21 14:44:57  kuznets
 * Bug fixes, improvements in statistics
 *
 * Revision 1.62  2006/02/08 15:17:33  kuznets
 * Tuning and bug fixing of job affinity
 *
 * Revision 1.61  2006/02/06 14:10:29  kuznets
 * Added job affinity
 *
 * Revision 1.60  2006/01/09 12:41:24  kuznets
 * Reflected changes in CStopWatch
 *
 * Revision 1.59  2005/11/21 14:26:28  kuznets
 * Better handling of authentication errors (induced by LBSM and port scanners
 *
 * Revision 1.58  2005/08/30 14:20:02  kuznets
 * Server version increment
 *
 * Revision 1.57  2005/08/26 12:36:10  kuznets
 * Performance optimization
 *
 * Revision 1.56  2005/08/25 15:00:35  kuznets
 * Optimized command parsing, eliminated many string comparisons
 *
 * Revision 1.55  2005/08/24 18:17:59  kuznets
 * Incremented program version
 *
 * Revision 1.54  2005/08/24 16:57:56  kuznets
 * Optimization of command parsing
 *
 * Revision 1.53  2005/08/22 14:01:58  kuznets
 * Added JobExchange command
 *
 * Revision 1.52  2005/08/17 14:24:37  kuznets
 * Improved err checking
 *
 *
 * ===========================================================================
 */
