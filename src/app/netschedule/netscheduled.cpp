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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
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

#include "job_status.hpp"
#include "job_time_line.hpp"
#include "access_list.hpp"

#if defined(NCBI_OS_UNIX)
# include <corelib/ncbi_os_unix.hpp>
# include <signal.h>
#endif


USING_NCBI_SCOPE;


#define NETSCHEDULED_VERSION \
    "NCBI NetSchedule server Version 2.5.0  build " __DATE__ " " __TIME__

class CNetScheduleServer;
static CNetScheduleServer* s_netschedule_server = 0;

/// Request context
///
/// @internal
enum ENSRequestField {
    eNSRF_Input = 0,
    eNSRF_Output,
    eNSRF_ProgressMsg,
    eNSRF_COut,
    eNSRF_CErr,
    eNSRF_AffinityToken,
    eNSRF_JobKey,
    eNSRF_JobReturnCode,
    eNSRF_Port,
    eNSRF_Timeout,
    eNSRF_JobMask,
    eNSRF_ErrMsg,
    eNSRF_Option,
    eNSRF_Status,
    eNSRF_QueueName,
    eNSRF_QueueClass
};
struct SJS_Request
{
    char               input[kNetScheduleMaxDBDataSize];
    char               output[kNetScheduleMaxDBDataSize];
    char               progress_msg[kNetScheduleMaxDBDataSize];
    char               cout[kNetScheduleMaxDBDataSize];
    char               cerr[kNetScheduleMaxDBDataSize];
    char               affinity_token[kNetScheduleMaxDBDataSize];
    string             job_key;
    unsigned int       job_return_code;
    unsigned int       port;
    unsigned int       timeout;
    unsigned int       job_mask;
    string             err_msg;
    string             param1;
    string             param2;

    void Init()
    {
        input[0] = output[0] = progress_msg[0] = affinity_token[0] 
                                               = cout[0] = cerr[0] = 0;
        job_key.erase(); err_msg.erase();
        param1.erase(); param2.erase();
        job_return_code = port = timeout = job_mask = 0;
    }
    void SetField(ENSRequestField fld,
                  const char* val, int size,
                  const char* dflt = "")
    {
        if (!val) val = dflt;
        if (size < 0) size = strlen(val);
        unsigned eff_size = (unsigned) size > kNetScheduleMaxDBDataSize - 1 ?
            kNetScheduleMaxDBDataSize - 1 :
            (unsigned) size;
        switch (fld) {
        case eNSRF_Input:
            strncpy(input, val, eff_size);
            input[eff_size] = 0;
            break;
        case eNSRF_Output:
            strncpy(output, val, eff_size);
            output[eff_size] = 0;
            break;
        case eNSRF_ProgressMsg:
            strncpy(progress_msg, val, eff_size);
            progress_msg[eff_size] = 0;
            break;
        case eNSRF_COut:
            strncpy(cout, val, eff_size);
            cout[eff_size] = 0;
            break;
        case eNSRF_CErr:
            strncpy(cerr, val, eff_size);
            cerr[eff_size] = 0;
            break;
        case eNSRF_AffinityToken:
            strncpy(affinity_token, val, eff_size);
            affinity_token[eff_size] = 0;
            break;
        case eNSRF_JobKey:
            job_key.erase();
            job_key.append(val, eff_size);
            break;
        case eNSRF_JobReturnCode:
            job_return_code = atoi(val);
            break;
        case eNSRF_Port:
            port = atoi(val);
            break;
        case eNSRF_Timeout:
            timeout = atoi(val);
            break;
        case eNSRF_JobMask:
            job_mask = atoi(val);
            break;
        case eNSRF_ErrMsg:
            err_msg.erase();
            err_msg.append(val, eff_size);
            break;
        case eNSRF_Option:
        case eNSRF_Status:
        case eNSRF_QueueName:
            param1.erase();
            param1.append(val, eff_size);
            break;
        case eNSRF_QueueClass:
            param2.erase();
            param2.append(val, eff_size);
            break;
        default:
            break;
        }
    }
};


const unsigned kMaxMessageSize = kNetScheduleMaxDBErrSize * 4;

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


//////////////////////////////////////////////////////////////////////////
/// ConnectionHandler for NetScheduler

class CNetScheduleServer;
class CNetScheduleHandler : public IServer_LineMessageHandler
{
public:
    CNetScheduleHandler(CNetScheduleServer* server);
    // MessageHandler protocol
    virtual void OnOpen(void);
    virtual void OnWrite(void) { }
    virtual void OnClose(void) { }
    virtual void OnTimeout(void);
    virtual void OnOverflow(void);
    virtual void OnMessage(BUF buffer);

    void WriteMsg(const char*   prefix, 
                  const char*   msg,
                  bool          comm_control = false,
                  bool          msg_size_control = true);

private:
    // Message processing phases
    void ProcessMsgAuth(BUF buffer);
    void ProcessMsgQueue(BUF buffer);
    void ProcessMsgRequest(BUF buffer);
    // Message processing for ProcessSubmitBatch phases
    void ProcessMsgBatchHeader(BUF buffer);
    void ProcessMsgBatchItem(BUF buffer);
    void ProcessMsgBatchEnd(BUF buffer);

public:
    // Protocol processing methods
    enum ENSTokenType {
        eNST_Error = -2,    // Error in the middle of the token, e.g. string
        eNST_None = -1,     // No more tokens
        eNST_Id = 0,
        eNST_Str,
        eNST_Int,
        eNST_KeyNone,       // key with empty value
        eNST_KeyId,
        eNST_KeyStr,
        eNST_KeyInt
    };
    enum ENSArgType {
        eNSA_None     = -1, // end of arg list
        eNSA_Required = 0,  // argument must be present
        eNSA_Optional,      // the argument may be omited
        eNSA_Optchain       // if the argument is absent, whole
                            // chain is ignored
    };
    enum ENSAccess {
        eNSAC_Queue     = 1,
        eNSAC_Worker    = 2,
        eNSAC_Submitter = 4,
        eNSAC_Admin     = 8
    };
    enum ENSClientRole {
        eNSCR_Any        = 0,
        eNSCR_Queue      = eNSAC_Queue,
        eNSCR_Worker     = eNSAC_Worker + eNSAC_Queue,
        eNSCR_Submitter  = eNSAC_Submitter + eNSAC_Queue,
        eNSCR_Admin      = eNSAC_Admin,
        eNSCR_QueueAdmin = eNSAC_Admin + eNSAC_Queue 
    };
    typedef void (CNetScheduleHandler::*FProcessor)(void);
    struct SArgument {
        ENSArgType      atype;
        ENSTokenType    ttype;
        ENSRequestField dest;
        const char*     dflt;
        const char*     key; // for eNST_KeyId and eNST_KeyInt
    };
    enum { kMaxArgs = 8 };
    struct SCommandMap {
        const char*   cmd;
        FProcessor    processor;
        ENSClientRole role;
        SArgument     args[kMaxArgs+1]; // + end of record
    };
private:
    static SArgument sm_End;
    static SCommandMap sm_CommandMap[];

    FProcessor ParseRequest(const char* reqstr, SJS_Request* req);

    // Command processors
    void ProcessSubmit();
    void ProcessSubmitBatch();
    void ProcessCancel();
    void ProcessStatus();
    void ProcessGet();
    void ProcessWaitGet();
    void ProcessPut();
    void ProcessJobExchange();
    void ProcessPutMessage();
    void ProcessGetMessage();
    void ProcessForceReschedule();
    void ProcessPutFailure();
    void ProcessDropQueue();
    void ProcessReturn();
    void ProcessJobRunTimeout();
    void ProcessJobDelayExpiration();
    void ProcessDropJob();
    void ProcessStatistics();
    void ProcessStatusSnapshot();
    void ProcessMonitor();
    void ProcessReloadConfig();
    void ProcessDump();
    void ProcessPrintQueue();
    void ProcessShutdown();
    void ProcessVersion();
    void ProcessRegisterClient();
    void ProcessUnRegisterClient();
    void ProcessQList();
    void ProcessError();
    void ProcessLog();
    void ProcessQuitSession();
    void ProcessCreateQueue();
    void ProcessDeleteQueue();

private:
    static
    bool x_TypeMatch(SArgument *argsDescr, ENSTokenType ttype,
                     const char* token, unsigned tsize);
    static
    ENSTokenType x_GetToken(const char*& s, const char*& tok, int& size);
    static
    void x_GetValue(ENSTokenType ttype,
                    const char* token, int tsize,
                    const char*&val, int& vsize);

    bool x_CheckVersion(void);
    void x_CheckAccess(ENSClientRole role);
    string x_FormatErrorMessage(string header, string what);
    void x_WriteErrorToMonitor(string msg);
    // Moved from CNetScheduleServer
    void x_MakeLogMessage(BUF buffer, string& lmsg);
    void x_MakeGetAnswer(const char* key_buf, 
                         const char*  jout,
                         const char*  jerr,
                         unsigned     job_mask);

    // Data
    char m_Request[kMaxMessageSize];
    size_t x_GetRequestBufSize() const { return sizeof(m_Request); }
    string m_Answer;
    char   m_MsgBuffer[kMaxMessageSize];

    unsigned                    m_PeerAddr;
    string                      m_AuthString;
    CNetScheduleServer*         m_Server;
    string                      m_QueueName;
    auto_ptr<CQueue>            m_Queue;
    CNetScheduleMonitor*        m_Monitor;
    SJS_Request                 m_JobReq;
    bool                        m_VersionControl;
    bool                        m_AdminAccess;
    void (CNetScheduleHandler::*m_ProcessMessage)(BUF buffer);

    // Batch submit data
    unsigned                    m_BatchSize;
    unsigned                    m_BatchPos;
    CStopWatch                  m_BatchStopWatch;
    vector<SNS_BatchSubmitRec>  m_BatchSubmitVector;

    /// Quick local timer
    CFastLocalTime              m_LocalTimer;

}; // CNetScheduleHandler

//////////////////////////////////////////////////////////////////////////
/// CNetScheduleConnectionFactory::
class CNetScheduleConnectionFactory : public IServer_ConnectionFactory
{
public:
    CNetScheduleConnectionFactory(CNetScheduleServer* server) :
        m_Server(server)
    {
    }
    IServer_ConnectionHandler* Create(void) {
        return new CNetScheduleHandler(m_Server);
    }
private:
    CNetScheduleServer* m_Server;
};


//////////////////////////////////////////////////////////////////////////
/// NetScheduler threaded server
///
/// @internal
class CNetScheduleServer : public CServer
{
public:
    CNetScheduleServer(unsigned        port,
                       bool            use_hostname,
                       CQueueDataBase* qdb,
                       unsigned        network_timeout,
                       bool            is_log); 

    virtual ~CNetScheduleServer();

    virtual bool ShutdownRequested(void) 
    {
        if (m_Shutdown) {
        //    LOG_POST(Info << "Shutdown flag set! Signal=" << m_SigNum); 
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
    unsigned GetPort() { return m_Port; }
    unsigned GetHostNetAddr() { return m_HostNetAddr; }
    // GetPort inherited from CThreadedServer
    const CTime& GetStartTime(void) const { return m_StartTime; }
    const CNetSchedule_AccessList& GetAccessList(void) const { return m_AdminHosts; }

private:
    void x_CreateLog();

private:
    /// Host name where server runs
    string             m_Host;
    unsigned           m_Port;
    unsigned           m_HostNetAddr;
    bool               m_Shutdown; 
    int                m_SigNum;  ///< Shutdown signal number
    /// Time to wait for the client (seconds)
    unsigned           m_InactivityTimeout;
    
    CQueueDataBase*    m_QueueDB;

    CTime              m_StartTime;

    CAtomicCounter         m_LogFlag;
    CNetScheduleLogStream  m_AccessLog;
    /// Quick local timer
    CFastLocalTime              m_LocalTimer;

    /// List of admin stations
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
    : m_Server(server), m_Monitor(NULL),
      m_VersionControl(false), m_AdminAccess(false)
{
}


void CNetScheduleHandler::OnOpen(void)
{
    CSocket& socket = GetSocket();
    socket.DisableOSSendDelay();
    STimeout to = {m_Server->GetInactivityTimeout(), 0};
    socket.SetTimeout(eIO_ReadWrite, &to);

    socket.GetPeerAddress(&m_PeerAddr, 0, eNH_NetworkByteOrder);
    // always use localhost(127.0*) address for clients coming from 
    // the same net address (sometimes it can be 127.* or full address)
    if (m_PeerAddr == m_Server->GetHostNetAddr()) {
        m_PeerAddr = CSocketAPI::GetLoopbackAddress();
    }

    m_AuthString.erase();

    m_ProcessMessage = &CNetScheduleHandler::ProcessMsgAuth;
}


void CNetScheduleHandler::OnTimeout()
{
    ERR_POST("OnTimeout!");
}


void CNetScheduleHandler::OnOverflow() 
{ 
    ERR_POST("OnOverflow!");
}


void CNetScheduleHandler::OnMessage(BUF buffer)
{
    if (m_Server->ShutdownRequested()) {
        WriteMsg("ERR:", 
            "NetSchedule server is shutting down. Session aborted.");
        return;
    }

    try {
        (this->*m_ProcessMessage)(buffer);
    }
    catch (CNetScheduleException &ex)
    {
        switch(ex.GetErrCode()) {
        case CNetScheduleException::eUnknownQueue:
            WriteMsg("ERR:", "QUEUE_NOT_FOUND");
            break;
        case CNetScheduleException::eOperationAccessDenied:
            WriteMsg("ERR:", "OPERATION_ACCESS_DENIED");
            break;
        default:
            string err = NStr::PrintableString(ex.what());
            WriteMsg("ERR:", err.c_str());
            break;
        }
        string msg = x_FormatErrorMessage("Server error", ex.what());
        ERR_POST(msg);
        x_WriteErrorToMonitor(msg);
        throw;
    }
    catch (CNetServiceException &ex)
    {
        WriteMsg("ERR:", ex.what());
        string msg = x_FormatErrorMessage("Server error", ex.what());
        ERR_POST(msg);
        x_WriteErrorToMonitor(msg);
        throw;
    }
    catch (CBDB_ErrnoException& ex)
    {
        int err_no = ex.BDB_GetErrno();
        if (err_no == DB_RUNRECOVERY) {
            string msg = x_FormatErrorMessage("Fatal Berkeley DB error: DB_RUNRECOVERY. " 
                                              "Emergency shutdown initiated!", ex.what());
            ERR_POST(msg);
            x_WriteErrorToMonitor(msg);
            m_Server->SetShutdownFlag();
        } else {
            string msg = x_FormatErrorMessage("BDB error", ex.what());
            ERR_POST(msg);
            x_WriteErrorToMonitor(msg);

            string err = "Internal database error:";
            err += ex.what();
            err = NStr::PrintableString(err);
            WriteMsg("ERR:", err.c_str());
        }
        throw;
    }
    catch (CBDB_Exception &ex)
    {
        string err = "Internal database (BDB) error:";
        err += ex.what();
        err = NStr::PrintableString(err);
        WriteMsg("ERR:", err.c_str());

        string msg = x_FormatErrorMessage("BDB error", ex.what());
        ERR_POST(msg);
        x_WriteErrorToMonitor(msg);
        throw;
    }
    catch (exception& ex)
    {
        string err = NStr::PrintableString(ex.what());
        WriteMsg("ERR:", err.c_str());

        string msg = x_FormatErrorMessage("STL exception", ex.what());
        ERR_POST(msg);
        x_WriteErrorToMonitor(msg);
        throw;
    }
}


string CNetScheduleHandler::x_FormatErrorMessage(string header, string what)
{
    string msg(header);
    CSocket& socket = GetSocket();
    msg += ": ";
    msg += what;
    msg += " Peer=";
    msg += socket.GetPeerAddress();
    msg += " ";
    msg += m_AuthString;
    msg += m_Request;
    return msg;
}

void CNetScheduleHandler::x_WriteErrorToMonitor(string msg)
{
    if (m_Monitor) {
        m_Monitor->SendString(m_LocalTimer.GetLocalTime().AsString() + msg);
    }
}


void CNetScheduleHandler::ProcessMsgAuth(BUF buffer)
{
    s_ReadBufToString(buffer, m_AuthString);

    if ((!m_Server->GetAccessList().IsRestrictionSet() || // no host control
         m_Server->GetAccessList().IsAllowed(m_PeerAddr)) &&
        (m_AuthString == "netschedule_control" ||
         m_AuthString == "netschedule_admin"))
    {
        m_AdminAccess = true;
    }
    m_ProcessMessage = &CNetScheduleHandler::ProcessMsgQueue;
}


void CNetScheduleHandler::ProcessMsgQueue(BUF buffer)
{
    s_ReadBufToString(buffer, m_QueueName);

    if (m_QueueName != "noname" && !m_QueueName.empty()) {
        m_Queue.reset(new CQueue(*(m_Server->GetQueueDB()),
                                 m_QueueName, m_PeerAddr));
        m_Monitor = m_Queue->GetMonitor();

        if (!m_Queue->IsVersionControl()) {
            m_VersionControl = true;
        }
    }

    m_ProcessMessage = &CNetScheduleHandler::ProcessMsgRequest;
}


// Workhorse method
void CNetScheduleHandler::ProcessMsgRequest(BUF buffer)
{
    bool is_log = m_Server->IsLog();

    size_t msg_size = BUF_Read(buffer, m_Request, x_GetRequestBufSize());
    if (msg_size < x_GetRequestBufSize()) m_Request[msg_size] = '\0';

    // Logging
    if (is_log || (m_Monitor && m_Monitor->IsMonitorActive())) {
        string lmsg;
        x_MakeLogMessage(buffer, lmsg);
        if (is_log)
            m_Server->GetLog() << lmsg;
        if (m_Monitor && m_Monitor->IsMonitorActive())
            m_Monitor->SendString(lmsg);
    }

    m_JobReq.Init();
    FProcessor requestProcessor = ParseRequest(m_Request, &m_JobReq);

    if (requestProcessor == &CNetScheduleHandler::ProcessQuitSession) {
        ProcessQuitSession();
        return;
    }

    // program version control
    if (m_VersionControl &&
        // we want status request to be fast, skip version control 
        (requestProcessor != &CNetScheduleHandler::ProcessStatus) &&
        // bypass for admin tools
        !m_AdminAccess) {
        if (!x_CheckVersion()) {
            WriteMsg("ERR:", "CLIENT_VERSION");
            CSocket& socket = GetSocket();
            socket.Close();
            return;
        }
        // One check per session is enough
        m_VersionControl = false;
    }
    (this->*requestProcessor)();
}

bool CNetScheduleHandler::x_CheckVersion()
{
    string::size_type pos; 
    pos = m_AuthString.find("prog='");
    if (pos == string::npos) {
        return false;
    }
    string auth_prog;
    const char* prog_str = m_AuthString.c_str();
    prog_str += pos + 6;
    for(; *prog_str && *prog_str != '\''; ++prog_str) {
        char ch = *prog_str;
        auth_prog.push_back(ch);
    }
    if (auth_prog.empty()) {
        return false;
    }
    try {
        CQueueClientInfo auth_prog_info;
        ParseVersionString(auth_prog,
                           &auth_prog_info.client_name, 
                           &auth_prog_info.version_info);
        if (!m_Queue->IsMatchingClient(auth_prog_info))
            return false;
    }
    catch (exception&)
    {
        return false;
    }
    return true;
}

void CNetScheduleHandler::x_CheckAccess(ENSClientRole role)
{
    if (((role & eNSAC_Queue) && m_Queue.get() == 0) ||
        ((role & eNSAC_Worker) && !m_Queue->IsWorkerAllowed()) ||
        ((role & eNSAC_Submitter) && !m_Queue->IsSubmitAllowed()) ||
        ((role & eNSAC_Admin) && !m_AdminAccess))
    {
        NCBI_THROW(CNetScheduleException, eOperationAccessDenied, "");
    }
}

//////////////////////////////////////////////////////////////////////////
// Process* methods for processing commands

void CNetScheduleHandler::ProcessSubmit()
{
    unsigned job_id =
        m_Queue->Submit(m_JobReq.input, 
                        m_PeerAddr, 
                        m_JobReq.port, m_JobReq.timeout, 
                        m_JobReq.progress_msg,
                        m_JobReq.affinity_token,
                        m_JobReq.cout,
                        m_JobReq.cerr,
                        m_JobReq.job_mask);

    char buf[1024];
    sprintf(buf, NETSCHEDULE_JOBMASK, 
                 job_id, m_Server->GetHost().c_str(), unsigned(m_Server->GetPort()));

    WriteMsg("OK:", buf);

    if (m_Monitor && m_Monitor->IsMonitorActive()) {
        CSocket& socket = GetSocket();
        string msg = "::ProcessSubmit ";
        msg += m_LocalTimer.GetLocalTime().AsString();
        msg += buf;
        msg += " ==> ";
        msg += socket.GetPeerAddress();
        msg += " ";
        msg += m_AuthString;

        m_Monitor->SendString(msg);
    }

}


void CNetScheduleHandler::ProcessSubmitBatch()
{
    WriteMsg("OK:", "Batch submit ready");
    m_ProcessMessage = &CNetScheduleHandler::ProcessMsgBatchHeader;
}


// Message processors for ProcessSubmitBatch
void CNetScheduleHandler::ProcessMsgBatchHeader(BUF buffer)
{
    // Expecting BTCH size | ENDS
    char cmd[256];
    size_t msg_size = BUF_Read(buffer, cmd, sizeof(cmd)-1);
    cmd[msg_size] = '\0';
    const char* s = cmd;
    const char* token;
    int tsize;
    ENSTokenType ttype = x_GetToken(s, token, tsize);
    if (ttype == eNST_Id && tsize == 4 && strncmp(token, "BTCH", tsize) == 0) {
        ttype = x_GetToken(s, token, tsize);
        if (ttype == eNST_Int) {
            m_BatchPos = 0;
            m_BatchSize = atoi(token);
            m_BatchStopWatch.Restart();
            m_BatchSubmitVector.resize(m_BatchSize);
            m_ProcessMessage = &CNetScheduleHandler::ProcessMsgBatchItem;
            return;
        } // else error???
    } else {
        if (ttype != eNST_Id || tsize != 4 || strncmp(token, "ENDS", 4) != 0) {
            BUF_Read(buffer, 0, BUF_Size(buffer));
            WriteMsg("ERR:", "Batch submit error: BATCH expected");
        }
    }
    m_ProcessMessage = &CNetScheduleHandler::ProcessMsgRequest;
    m_Server->GetQueueDB()->TransactionCheckPoint();
}


void CNetScheduleHandler::ProcessMsgBatchItem(BUF buffer)
{
    // Expecting "input" [affp|aff="affinity_token"]
    char data[kNetScheduleMaxDBDataSize * 6 + 1];
    size_t msg_size = BUF_Read(buffer, data, sizeof(data)-1);
    data[msg_size] = '\0';
    SNS_BatchSubmitRec& rec = m_BatchSubmitVector[m_BatchPos];
    const char* s = data;
    const char* token;
    int tsize;
    ENSTokenType ttype = x_GetToken(s, token, tsize);
    if (ttype != eNST_Str) {
        WriteMsg("ERR:", "Invalid batch submission, syntax error");
        m_ProcessMessage = &CNetScheduleHandler::ProcessMsgRequest;
        return;
    }
    strncpy(rec.input, token, tsize);
    rec.input[tsize] = '\0';
    ttype = x_GetToken(s, token, tsize);
    if (ttype == eNST_Id && strncmp(token, "affp", 4) == 0) {
        rec.affinity_id = kMax_I4;
    } else if (ttype == eNST_KeyStr && strncmp(token, "aff=", 4) == 0) {
        const char* val;
        int vsize;
        x_GetValue(ttype, token, tsize, val, vsize);
        strncpy(rec.affinity_token, val, vsize);
        rec.affinity_token[vsize] = '\0';
    } else if (ttype == eNST_None) {
        // Do nothing - legal absense of affinity token
    } else {
        WriteMsg("ERR:", "Batch submit error: unrecognized affinity clause");
        m_ProcessMessage = &CNetScheduleHandler::ProcessMsgRequest;
        return;
    }

    if (++m_BatchPos >= m_BatchSize)
        m_ProcessMessage = &CNetScheduleHandler::ProcessMsgBatchEnd;
}


void CNetScheduleHandler::ProcessMsgBatchEnd(BUF buffer)
{
    // Expecting ENDB
    char cmd[256];
    size_t msg_size = BUF_Read(buffer, cmd, sizeof(cmd)-1);
    cmd[msg_size] = '\0';
    const char* s = cmd;
    const char* token;
    int tsize;
    ENSTokenType ttype = x_GetToken(s, token, tsize);
    if (ttype != eNST_Id || tsize != 4 || strncmp(token, "ENDB", tsize) != 0) {
        BUF_Read(buffer, 0, BUF_Size(buffer));
        WriteMsg("ERR:", "Batch submit error: unexpected end of batch");
    }

    double comm_elapsed = m_BatchStopWatch.Elapsed();

    // we have our batch now

    CStopWatch sw(CStopWatch::eStart);
    unsigned job_id =
        m_Queue->SubmitBatch(m_BatchSubmitVector, 
                             m_PeerAddr,
                             0, 0, 0);
    double db_elapsed = sw.Elapsed();

    if (m_Monitor && m_Monitor->IsMonitorActive()) {
        CSocket& socket = GetSocket();
        string msg = "::ProcessSubmitBatch ";
        msg += m_LocalTimer.GetLocalTime().AsString();
        msg += " ==> ";
        msg += socket.GetPeerAddress();
        msg += " ";
        msg += m_AuthString;
        msg += " batch block size=";
        msg += NStr::UIntToString(m_BatchSize);
        msg += " comm.time=";
        msg += NStr::DoubleToString(comm_elapsed, 4, NStr::fDoubleFixed);
        msg += " trans.time=";
        msg += NStr::DoubleToString(db_elapsed, 4, NStr::fDoubleFixed);

        m_Monitor->SendString(msg);
    }

    {{
        char buf[1024];
        sprintf(buf, "%u %s %u", 
                job_id, m_Server->GetHost().c_str(),
                unsigned(m_Server->GetPort()));

        WriteMsg("OK:", buf);
    }}

    m_ProcessMessage = &CNetScheduleHandler::ProcessMsgBatchHeader;
}


void CNetScheduleHandler::ProcessCancel()
{
    unsigned job_id = CNetSchedule_GetJobId(m_JobReq.job_key);
    m_Queue->Cancel(job_id);
    WriteMsg("OK:", "");
}


void CNetScheduleHandler::ProcessForceReschedule()
{
    unsigned job_id = CNetSchedule_GetJobId(m_JobReq.job_key);
    m_Queue->ForceReschedule(job_id);
    WriteMsg("OK:", "");
}


void CNetScheduleHandler::ProcessDropJob()
{
    unsigned job_id = CNetSchedule_GetJobId(m_JobReq.job_key);
    m_Queue->DropJob(job_id);
    WriteMsg("OK:", "");
}


void CNetScheduleHandler::ProcessJobRunTimeout()
{
    unsigned job_id = CNetSchedule_GetJobId(m_JobReq.job_key);
    m_Queue->SetJobRunTimeout(job_id, m_JobReq.timeout);
    WriteMsg("OK:", "");
}


void CNetScheduleHandler::ProcessJobDelayExpiration()
{
    unsigned job_id = CNetSchedule_GetJobId(m_JobReq.job_key);
    m_Queue->JobDelayExpiration(job_id, m_JobReq.timeout);
    WriteMsg("OK:", "");
}


void CNetScheduleHandler::ProcessStatusSnapshot()
{
    const char* aff_token = m_JobReq.affinity_token;
    CJobStatusTracker::TStatusSummaryMap st_map;

    bool aff_exists = m_Queue->CountStatus(&st_map, aff_token);
    if (!aff_exists) {
        WriteMsg("ERR:", "Unknown affinity token.");
        return;
    }
    ITERATE(CJobStatusTracker::TStatusSummaryMap,
            it,
            st_map) {
        string st_str = CNetScheduleClient::StatusToString(it->first);
        st_str.push_back(' ');
        st_str.append(NStr::UIntToString(it->second));
        WriteMsg("OK:", st_str.c_str());
    } // ITERATE
    WriteMsg("OK:", "END");
}


void CNetScheduleHandler::ProcessStatus()
{
    unsigned job_id = CNetSchedule_GetJobId(m_JobReq.job_key);

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
                sprintf(szBuf, 
                        "%i %i \"%s\" \"\" \"%s\"", 
                        st, ret_code, 
                        m_JobReq.output,
                        m_JobReq.input);
                WriteMsg("OK:", szBuf);
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
                sprintf(szBuf, 
                        "%i %i \"%s\" \"\" \"%s\"", 
                        st, ret_code, 
                        m_JobReq.output,
                        m_JobReq.input);
                WriteMsg("OK:", szBuf);
                return;
            } else {
                st = (int)CNetScheduleClient::eJobNotFound;
            }
        }
        break;

    case CNetScheduleClient::eFailed:
        {
            char err[kNetScheduleMaxDBErrSize];
            bool b = m_Queue->GetJobDescr(job_id, &ret_code, 
                                          m_JobReq.input,
                                          m_JobReq.output,
                                          err,
                                          0,
                                          0, 0, 
                                          status);

            if (b) {
                sprintf(szBuf, 
                        "%i %i \"%s\" \"%s\" \"%s\"", 
                        st, ret_code, 
                        m_JobReq.output,
                        err,
                        m_JobReq.input);
                WriteMsg("OK:", szBuf);
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
    WriteMsg("OK:", szBuf);
}


void CNetScheduleHandler::ProcessGetMessage()
{
    unsigned job_id = CNetSchedule_GetJobId(m_JobReq.job_key);
    int ret_code;
    bool b = m_Queue->GetJobDescr(job_id, &ret_code, 
                                  0, 0, 0, m_JobReq.progress_msg, 0, 0);

    if (b) {
        WriteMsg("OK:", m_JobReq.progress_msg);
    } else {
        WriteMsg("ERR:", "Job not found");
    }
}


void CNetScheduleHandler::ProcessPutMessage()
{
    unsigned job_id = CNetSchedule_GetJobId(m_JobReq.job_key);

    m_Queue->PutProgressMessage(job_id, m_JobReq.progress_msg);
    WriteMsg("OK:", "");
}


void CNetScheduleHandler::ProcessGet()
{
    unsigned job_id;
    char key_buf[1024];
    m_Queue->GetJob(m_PeerAddr, &job_id,
                    m_JobReq.input, m_JobReq.cout, m_JobReq.cerr,
                    m_AuthString, 
                    &m_JobReq.job_mask);

    m_Queue->GetJobKey(key_buf, job_id,
        m_Server->GetHost(), m_Server->GetPort());

    if (job_id) {
        x_MakeGetAnswer(key_buf, m_JobReq.cout, m_JobReq.cerr,
                        m_JobReq.job_mask);
        WriteMsg("OK:", m_Answer.c_str());
    } else {
        WriteMsg("OK:", "");
    }

    if (m_Monitor && m_Monitor->IsMonitorActive() && job_id) {
        CSocket& socket = GetSocket();
        string msg = "::ProcessGet ";
        msg += m_LocalTimer.GetLocalTime().AsString();
        msg += m_Answer;
        msg += " ==> ";
        msg += socket.GetPeerAddress();
        msg += " ";
        msg += m_AuthString;

        m_Monitor->SendString(msg);
    }

    if (m_JobReq.port) {  // unregister notification
        m_Queue->RegisterNotificationListener(
            m_PeerAddr, m_JobReq.port, 0, m_AuthString);
    }
}


void 
CNetScheduleHandler::ProcessJobExchange()
{
    unsigned job_id;
    char key_buf[1024];

    unsigned done_job_id;
    if (!m_JobReq.job_key.empty()) {
        done_job_id = CNetSchedule_GetJobId(m_JobReq.job_key);
    } else {
        done_job_id = 0;
    }
    m_Queue->PutResultGetJob(done_job_id, m_JobReq.job_return_code,
                             m_JobReq.output, false /*don't change the timeline*/,
                             m_PeerAddr, &job_id,
                             m_JobReq.input, m_JobReq.cout, m_JobReq.cerr,
                             m_AuthString,
                             &m_JobReq.job_mask);

    m_Queue->GetJobKey(key_buf, job_id,
        m_Server->GetHost(), m_Server->GetPort());

    if (job_id) {
        x_MakeGetAnswer(key_buf, m_JobReq.cout, m_JobReq.cerr,
                        m_JobReq.job_mask);
        WriteMsg("OK:", m_Answer.c_str());
    } else {
        WriteMsg("OK:", "");
    }
    // postpone removal from the timeline, so we remove while 
    // request response is network transferred
    m_Queue->TimeLineExchange(done_job_id, job_id, time(0));

    if (m_Monitor && m_Monitor->IsMonitorActive()) {
        CSocket& socket = GetSocket();
        string msg = "::ProcessJobExchange ";
        msg += m_LocalTimer.GetLocalTime().AsString();
        msg += " ";
        msg += m_Answer;
        msg += " ==> ";
        msg += socket.GetPeerAddress();
        msg += " ";
        msg += m_AuthString;

        m_Monitor->SendString(msg);
    }
}


void CNetScheduleHandler::ProcessWaitGet()
{
    unsigned job_id;
    m_Queue->GetJob(m_PeerAddr, &job_id, 
                    m_JobReq.input, m_JobReq.cout, m_JobReq.cerr,
                    m_AuthString, &m_JobReq.job_mask);

    if (job_id) {
        char key_buf[1024];
        m_Queue->GetJobKey(key_buf, job_id,
            m_Server->GetHost(), m_Server->GetPort());
        x_MakeGetAnswer(key_buf, m_JobReq.cout, m_JobReq.cerr,
                        m_JobReq.job_mask);
        WriteMsg("OK:", m_Answer.c_str());
        return;
    }

    // job not found, initiate waiting mode

    WriteMsg("OK:", kEmptyStr.c_str());

    m_Queue->RegisterNotificationListener(
        m_PeerAddr, m_JobReq.port, m_JobReq.timeout,
        m_AuthString);
}


void CNetScheduleHandler::ProcessRegisterClient()
{
    m_Queue->RegisterNotificationListener(
        m_PeerAddr, m_JobReq.port, 1, m_AuthString);

    WriteMsg("OK:", kEmptyStr.c_str());
}


void CNetScheduleHandler::ProcessUnRegisterClient()
{
    m_Queue->UnRegisterNotificationListener(m_PeerAddr,
                                            m_JobReq.port);
    m_Queue->ClearAffinity(m_PeerAddr, m_AuthString);

    WriteMsg("OK:", kEmptyStr.c_str());
}


void CNetScheduleHandler::ProcessQList()
{
    string qnames;
    ITERATE(CQueueCollection, it, 
            m_Server->GetQueueDB()->GetQueueCollection()) {
        qnames += it.GetName(); qnames += ";";
    }
    WriteMsg("OK:", qnames.c_str());
}


void CNetScheduleHandler::ProcessError() {
    WriteMsg("ERR:", m_JobReq.err_msg.c_str());
}


void CNetScheduleHandler::ProcessQuitSession(void)
{
    // read trailing input (see netcached.cpp for more comments on "why?")
    CSocket& socket = GetSocket();
    STimeout to; to.sec = to.usec = 0;
    socket.SetTimeout(eIO_Read, &to);
    socket.Read(NULL, 1024);
    socket.Close();
}


void CNetScheduleHandler::ProcessPutFailure()
{
    unsigned job_id = CNetSchedule_GetJobId(m_JobReq.job_key);
    m_Queue->JobFailed(job_id, m_JobReq.err_msg, m_JobReq.output,
                       m_JobReq.job_return_code,
                       m_PeerAddr, m_AuthString);
    WriteMsg("OK:", kEmptyStr.c_str());
}


void CNetScheduleHandler::ProcessPut()
{
    unsigned job_id = CNetSchedule_GetJobId(m_JobReq.job_key);
    m_Queue->PutResult(job_id, 
                       m_JobReq.job_return_code, m_JobReq.output, 
                       false // don't remove from tl
                       );
    WriteMsg("OK:", kEmptyStr.c_str());
    m_Queue->RemoveFromTimeLine(job_id);
}


void CNetScheduleHandler::ProcessReturn()
{
    unsigned job_id = CNetSchedule_GetJobId(m_JobReq.job_key);
    m_Queue->ReturnJob(job_id);
    WriteMsg("OK:", kEmptyStr.c_str());
}


void CNetScheduleHandler::ProcessDropQueue()
{
    m_Queue->Truncate();
    WriteMsg("OK:", kEmptyStr.c_str());
}


void CNetScheduleHandler::ProcessMonitor()
{
    CSocket& socket = GetSocket();
    socket.DisableOSSendDelay(false);
    WriteMsg("OK:", NETSCHEDULED_VERSION);
    string started = "Started: " + m_Server->GetStartTime().AsString();
    WriteMsg("OK:", started.c_str());
    socket.SetOwnership(eNoOwnership);
    m_Queue->SetMonitorSocket(socket.GetSOCK());
    // TODO: somehow release socket from regular scheduling here - it is
    // write only since this moment
}


void CNetScheduleHandler::ProcessPrintQueue()
{
    // TODO this method can not support session, because socket is closed at
    // the end.
    CNetScheduleClient::EJobStatus 
        job_status = CNetScheduleClient::StringToStatus(m_JobReq.param1);

    if (job_status == CNetScheduleClient::eJobNotFound) {
        string err_msg = "Status unknown: " + m_JobReq.param1;
        WriteMsg("ERR:", err_msg.c_str());
        return;
    }

    CSocket& socket = GetSocket();
    SOCK sock = socket.GetSOCK();
    socket.SetOwnership(eNoOwnership);
    socket.Reset(0, eTakeOwnership, eCopyTimeoutsToSOCK);

    CConn_SocketStream ios(sock);  // sock is being passed and used exclusively

    m_Queue->PrintQueue(ios,
                        job_status,
                        m_Server->GetHost(),
                        m_Server->GetPort());

    ios << "OK:END" << endl;
}


void CNetScheduleHandler::ProcessDump()
{
    // TODO this method can not support session, because socket is closed at
    // the end.
    CSocket& socket = GetSocket();
    SOCK sock = socket.GetSOCK();
    socket.SetOwnership(eNoOwnership);
    socket.Reset(0, eTakeOwnership, eCopyTimeoutsToSOCK);

    CConn_SocketStream ios(sock);

    ios << "OK:" << NETSCHEDULED_VERSION << endl;

    if (m_JobReq.job_key.empty()) {
        ios << "OK:" << "[Job status matrix]:";

        m_Queue->PrintJobStatusMatrix(ios);

        ios << "OK:[Job DB]:" << endl;
        m_Queue->PrintAllJobDbStat(ios);
    } else {
        try {
            unsigned job_id = CNetSchedule_GetJobId(m_JobReq.job_key);

            CNetScheduleClient::EJobStatus status = m_Queue->GetStatus(job_id);
            
            string st_str = CNetScheduleClient::StatusToString(status);
            ios << "OK:" << "[Job status matrix]:" << st_str;
            ios << "OK:[Job DB]:" << endl;
            m_Queue->PrintJobDbStat(job_id, ios);
        } 
        catch (CException& ex)
        {
            // dump by status
            CNetScheduleClient::EJobStatus 
                job_status = CNetScheduleClient::StringToStatus(
                    m_JobReq.job_key);

            if (job_status == CNetScheduleClient::eJobNotFound) {
                ios << "ERR:" << "Status unknown: " << m_JobReq.job_key;
                return;
            }
/*
            m_Queue->PrintQueue(ios, 
                                job_status,
                                m_Server->GetHost(),
                                m_Server->GetPort());
*/
        }
    }
    ios << "OK:END" << endl;
}


void CNetScheduleHandler::ProcessReloadConfig()
{
    CNcbiApplication* app = CNcbiApplication::Instance();

    bool reloaded = app->ReloadConfig(CMetaRegistry::fReloadIfChanged);
    if (reloaded) {
        unsigned tmp;
        m_Server->GetQueueDB()->Configure(app->GetConfig(), &tmp);
    }
    WriteMsg("OK:", "");
}


void CNetScheduleHandler::ProcessStatistics()
{
    CSocket& socket = GetSocket();
    socket.DisableOSSendDelay(false);

    WriteMsg("OK:", NETSCHEDULED_VERSION);
    string started = "Started: " + m_Server->GetStartTime().AsString();
    WriteMsg("OK:", started.c_str());

    string load_str = "Load: jobs dispatched: ";
    load_str.append(NStr::DoubleToString(m_Queue->GetGetAverage()));
    load_str += "/sec, jobs complete: ";
    load_str.append(NStr::DoubleToString(m_Queue->GetPutAverage()));
    load_str += "/sec";
    WriteMsg("OK:", load_str.c_str());

    for (int i = CNetScheduleClient::ePending; 
         i < CNetScheduleClient::eLastStatus; ++i) {
        CNetScheduleClient::EJobStatus st = 
                        (CNetScheduleClient::EJobStatus) i;
        unsigned count = m_Queue->CountStatus(st);


        string st_str = CNetScheduleClient::StatusToString(st);
        st_str += ": ";
        st_str += NStr::UIntToString(count);

        WriteMsg("OK:", st_str.c_str(), true, false);

        CJobStatusTracker::TBVector::statistics bv_stat;
        m_Queue->StatusStatistics(st, &bv_stat);
        st_str = "   bit_blk="; 
        st_str.append(NStr::UIntToString(bv_stat.bit_blocks));
        st_str += "; gap_blk=";
        st_str.append(NStr::UIntToString(bv_stat.gap_blocks));
        st_str += "; mem_used=";
        st_str.append(NStr::UIntToString(bv_stat.memory_used));
        WriteMsg("OK:", st_str.c_str());
    } // for

    if (m_JobReq.param1 == "ALL") {

        unsigned db_recs = m_Queue->CountRecs();
        string recs = "Records:";
        recs += NStr::UIntToString(db_recs);
        WriteMsg("OK:", recs.c_str());
        WriteMsg("OK:", "[Database statistics]:");

        {{
            CNcbiOstrstream ostr;
            m_Queue->PrintStat(ostr);
            ostr << ends;

            char* stat_str = ostr.str();
    //        size_t os_str_len = ostr.pcount()-1;
    //        stat_str[os_str_len] = 0;
            try {
                WriteMsg("OK:", stat_str, true, false);
            } catch (...) {
                ostr.freeze(false);
                throw;
            }
            ostr.freeze(false);
        }}

    }

    WriteMsg("OK:", "[Worker node statistics]:");

    {{
        CNcbiOstrstream ostr;
        m_Queue->PrintNodeStat(ostr);
        ostr << ends;

        char* stat_str = ostr.str();
        try {
            WriteMsg("OK:", stat_str, true, false);
        } catch (...) {
            ostr.freeze(false);
            throw;
        }
        ostr.freeze(false);
    }}

    {{
        WriteMsg("OK:", "[Configured job submitters]:");
        CNcbiOstrstream ostr;
        m_Queue->PrintSubmHosts(ostr);
        ostr << ends;

        char* stat_str = ostr.str();
        try {
            WriteMsg("OK:", stat_str, true, false);
        } catch (...) {
            ostr.freeze(false);
            throw;
        }
        ostr.freeze(false);
    }}

    {{
        WriteMsg("OK:", "[Configured workers]:");
        CNcbiOstrstream ostr;
        m_Queue->PrintWNodeHosts(ostr);
        ostr << ends;

        char* stat_str = ostr.str();
        try {
            WriteMsg("OK:", stat_str, true, false);
        } catch (...) {
            ostr.freeze(false);
            throw;
        }
        ostr.freeze(false);
    }}

    WriteMsg("OK:", "END");
}


void CNetScheduleHandler::ProcessLog()
{
    const char* str = m_JobReq.param1.c_str();
    if (NStr::strcasecmp(str, "ON") == 0) {
        m_Server->SetLogging(true);
        LOG_POST("Logging turned ON");
    } else if (NStr::strcasecmp(str, "OFF") == 0) {
        m_Server->SetLogging(false);
        LOG_POST("Logging turned OFF");
    } else {
        WriteMsg("ERR:", "Incorrect LOG syntax");
    }
    WriteMsg("OK:", "");
}


void CNetScheduleHandler::ProcessShutdown()
{
    CSocket& socket = GetSocket();
    string admin_host = socket.GetPeerAddress();
    string msg = "Shutdown request... ";
    msg += admin_host;
    msg += " ";
    msg += CTime(CTime::eCurrent).AsString();
    LOG_POST(Info << msg);
    m_Server->SetShutdownFlag();
    WriteMsg("OK:", "");
}


void CNetScheduleHandler::ProcessVersion()
{
    WriteMsg("OK:", NETSCHEDULED_VERSION);
}

void CNetScheduleHandler::ProcessCreateQueue()
{
    const string& qname  = m_JobReq.param1;
    const string& qclass = m_JobReq.param2;
    if (m_Server->GetQueueDB()->CreateQueue(qname, qclass)) {
        WriteMsg("OK:", "");
    } else {
        WriteMsg("ERR:", "Can not create.");
        LOG_POST(Warning << "Can not create queue: " << qname <<
                 " of class " << qclass);
    }
}

void CNetScheduleHandler::ProcessDeleteQueue()
{
    const string& qname = m_JobReq.param1;
    if (m_Server->GetQueueDB()->DeleteQueue(qname)) {
        WriteMsg("OK:", "");
    } else {
        WriteMsg("ERR:", "Can not delete.");
        LOG_POST(Warning << "Can not delete queue: " << qname);
    }
}

//////////////////////////////////////////////////////////////////////////

/// NetSchedule command parser
///
/// @internal
///

CNetScheduleHandler::SArgument CNetScheduleHandler::sm_End = { eNSA_None };
#define NO_ARGS {sm_End}
CNetScheduleHandler::SCommandMap CNetScheduleHandler::sm_CommandMap[] = {
    // SUBMIT input : str [ progress_msg : str ] [ port : uint [ timeout : uint ]]
    //        [ affinity_token : keystr(aff) ] [ cout : keystr(out) ] 
    //        [ cerr : keystr(err) ] [ job_mask : keyint(msk) ]
    { "SUBMIT",   &CNetScheduleHandler::ProcessSubmit, eNSCR_Submitter,
        { { eNSA_Required, eNST_Str,     eNSRF_Input },
          { eNSA_Optional, eNST_Str,     eNSRF_ProgressMsg },
          { eNSA_Optional, eNST_Int,     eNSRF_Port },
          { eNSA_Optional, eNST_Int,     eNSRF_Timeout },
          { eNSA_Optional, eNST_KeyStr,  eNSRF_AffinityToken, "", "aff" },
          { eNSA_Optional, eNST_KeyStr,  eNSRF_COut, "", "out" },
          { eNSA_Optional, eNST_KeyStr,  eNSRF_CErr, "", "err" },
          { eNSA_Optional, eNST_KeyInt,  eNSRF_JobMask, "0", "msk" }, sm_End } },
    // CANCEL job_key : id
    { "CANCEL",   &CNetScheduleHandler::ProcessCancel, eNSCR_Submitter, 
        { { eNSA_Required, eNST_Id, eNSRF_JobKey }, sm_End } },
    // STATUS job_key : id
    { "STATUS",   &CNetScheduleHandler::ProcessStatus, eNSCR_Queue,
        { { eNSA_Required, eNST_Id, eNSRF_JobKey }, sm_End } },
    // GET [ port : int ]
    { "GET",      &CNetScheduleHandler::ProcessGet, eNSCR_Worker,
        { { eNSA_Optional, eNST_Int, eNSRF_Port }, sm_End } },
    // PUT job_key : id  job_return_code : int  output : str
    { "PUT",      &CNetScheduleHandler::ProcessPut, eNSCR_Worker,
        { { eNSA_Required, eNST_Id,  eNSRF_JobKey },
          { eNSA_Required, eNST_Int, eNSRF_JobReturnCode },
          { eNSA_Required, eNST_Str, eNSRF_Output }, sm_End } },
    // RETURN job_key : id
    { "RETURN",   &CNetScheduleHandler::ProcessReturn, eNSCR_Worker,
        { { eNSA_Required, eNST_Id, eNSRF_JobKey }, sm_End } },
    { "SHUTDOWN", &CNetScheduleHandler::ProcessShutdown, eNSCR_Admin,
        NO_ARGS },
    { "VERSION",  &CNetScheduleHandler::ProcessVersion, eNSCR_Any, NO_ARGS },
    // LOG option : id -- "ON" or "OFF"
    { "LOG",      &CNetScheduleHandler::ProcessLog, eNSCR_Any, // ?? Admin
        { { eNSA_Required, eNST_Id, eNSRF_Option }, sm_End } },
    // STAT [ option : id ] -- "ALL"
    { "STAT",     &CNetScheduleHandler::ProcessStatistics, eNSCR_Queue,
        { { eNSA_Optional, eNST_Id, eNSRF_Option }, sm_End } },
    { "QUIT",     &CNetScheduleHandler::ProcessQuitSession, eNSCR_Any,
        NO_ARGS },
    { "DROPQ",    &CNetScheduleHandler::ProcessDropQueue, eNSCR_QueueAdmin,
        NO_ARGS },
    // WGET port : uint  timeout : uint
    { "WGET",     &CNetScheduleHandler::ProcessWaitGet, eNSCR_Worker,
        { { eNSA_Required, eNST_Int, eNSRF_Port },
          { eNSA_Required, eNST_Int, eNSRF_Timeout }, sm_End} },
    // JRTO job_key : id  timeout : uint
    { "JRTO",     &CNetScheduleHandler::ProcessJobRunTimeout, eNSCR_Queue, // ?? Worker/Submitter
        { { eNSA_Required, eNST_Id,  eNSRF_JobKey },
          { eNSA_Required, eNST_Int, eNSRF_Timeout }, sm_End} },
    // DROJ job_key : id
    { "DROJ",     &CNetScheduleHandler::ProcessDropJob, eNSCR_Submitter,
        { { eNSA_Required, eNST_Id, eNSRF_JobKey }, sm_End } },
    // FPUT job_key : id  err_msg : str  output : str  job_return_code : int
    { "FPUT",     &CNetScheduleHandler::ProcessPutFailure, eNSCR_Worker,
        { { eNSA_Required, eNST_Id,  eNSRF_JobKey },
          { eNSA_Required, eNST_Str, eNSRF_ErrMsg },
          { eNSA_Required, eNST_Str, eNSRF_Output }, 
          { eNSA_Required, eNST_Int, eNSRF_JobReturnCode }, sm_End} },
    // MPUT job_key : id  progress_msg : str
    { "MPUT",     &CNetScheduleHandler::ProcessPutMessage, eNSCR_Queue,
        { { eNSA_Required, eNST_Id,  eNSRF_JobKey },
          { eNSA_Required, eNST_Str, eNSRF_ProgressMsg }, sm_End} },
    // MGET job_key : id
    { "MGET",     &CNetScheduleHandler::ProcessGetMessage, eNSCR_Queue,
        { { eNSA_Required, eNST_Id, eNSRF_JobKey }, sm_End } },
    { "MONI",     &CNetScheduleHandler::ProcessMonitor, eNSCR_Queue, NO_ARGS },
    // DUMP [ job_key : id ]
    { "DUMP",     &CNetScheduleHandler::ProcessDump, eNSCR_Queue,
        { { eNSA_Optional, eNST_Id, eNSRF_JobKey }, sm_End } },
    { "RECO",     &CNetScheduleHandler::ProcessReloadConfig, eNSCR_Any, // ?? Admin
        NO_ARGS },
    { "QLST",     &CNetScheduleHandler::ProcessQList, eNSCR_Any, NO_ARGS },
    { "BSUB",     &CNetScheduleHandler::ProcessSubmitBatch, eNSCR_Submitter,
        NO_ARGS },
    // JXCG [ job_key : id job_return_code : int output : str ]
    { "JXCG",     &CNetScheduleHandler::ProcessJobExchange, eNSCR_Worker,
        { { eNSA_Optchain, eNST_Id,  eNSRF_JobKey },
          { eNSA_Optchain, eNST_Int, eNSRF_JobReturnCode },
          { eNSA_Optchain, eNST_Str, eNSRF_Output }, sm_End } },
    // REGC port : uint
    { "REGC",     &CNetScheduleHandler::ProcessRegisterClient, eNSCR_Worker,
        { { eNSA_Required, eNST_Int, eNSRF_Port }, sm_End} },
    // URGC port : uint
    { "URGC",     &CNetScheduleHandler::ProcessUnRegisterClient, eNSCR_Worker,
        { { eNSA_Required, eNST_Int, eNSRF_Port }, sm_End} },
    // QPRT status : id
    { "QPRT",     &CNetScheduleHandler::ProcessPrintQueue, eNSCR_Queue,
        { { eNSA_Required, eNST_Id, eNSRF_Status }, sm_End } },
    // FRES job_key : id
    { "FRES",     &CNetScheduleHandler::ProcessForceReschedule, eNSCR_Submitter,
        { { eNSA_Required, eNST_Id, eNSRF_JobKey }, sm_End } },
    // JDEX job_key : id timeout : uint
    { "JDEX",     &CNetScheduleHandler::ProcessJobDelayExpiration, eNSCR_Worker,
        { { eNSA_Required, eNST_Id,  eNSRF_JobKey },
          { eNSA_Required, eNST_Int, eNSRF_Timeout }, sm_End} },
    // STSN [ affinity_token : keystr(aff) ]
    { "STSN",     &CNetScheduleHandler::ProcessStatusSnapshot, eNSCR_Queue,
        { { eNSA_Optional, eNST_KeyStr, eNSRF_AffinityToken, "", "aff" }, sm_End} },
    // QCRE qname : id  qclass : id
    { "QCRE",     &CNetScheduleHandler::ProcessCreateQueue, eNSCR_Admin,
        { { eNSA_Required, eNST_Id, eNSRF_QueueName },
          { eNSA_Required, eNST_Id, eNSRF_QueueClass }, sm_End} },
    // QDEL qname : id
    { "QDEL",     &CNetScheduleHandler::ProcessDeleteQueue, eNSCR_Admin,
        { { eNSA_Required, eNST_Id, eNSRF_QueueName }, sm_End } },
    { 0,          &CNetScheduleHandler::ProcessError },
};


CNetScheduleHandler::ENSTokenType
CNetScheduleHandler::x_GetToken(const char*& s,
                                const char*& tok,
                                int &size)
{
    // Skip space
    while (*s && (*s == ' ' || *s == '\t')) ++s;
    if (!*s) return eNST_None;
    ENSTokenType ttype = eNST_None;
    if (*s == '"') {
        s = tok = s + 1;
        ttype = eNST_Str;
    } else {
        tok = s;
        if (isdigit(*s) || *s == '-') ttype = eNST_Int;
        else ttype = eNST_Id;
    }
    for ( ; *s && ((ttype == eNST_Str || ttype == eNST_KeyStr) ?
                        !(*s == '"' && *(s-1) != '\\') :
                        !(*s == ' ' || *s == '\t'));
            ++s) {
        switch (ttype) {
        case eNST_Int:
            if (!isdigit(*s)) ttype = eNST_Id;
            break;
        case eNST_KeyNone:
            if (isdigit(*s) || *s == '-') ttype = eNST_KeyInt;
            else if (*s == '"') ttype = eNST_KeyStr;
            else ttype = eNST_KeyId;
            break;
        case eNST_KeyInt:
            if (!isdigit(*s)) ttype = eNST_KeyId;
            break;
        case eNST_Id:
            if (*s == '=') ttype = eNST_KeyNone;
            break;
        case eNST_Str:
            if (unsigned(s-tok) > kNetScheduleMaxDBDataSize) {
                return eNST_Error; // ?? different error type
            }
        default:
            break;
        }
    }
    size = s - tok;
    if (ttype == eNST_Str || ttype == eNST_KeyStr) {
        if (*s != '"') return eNST_Error;
        if (ttype == eNST_KeyStr) {
            // we do not parse internal structure of key-value here
            // for KeyStr, so we report size including closing "
            ++size;
        }
        ++s;
    }
    return ttype;
}


// Compare actual token type with argument type. 
// If the argument is of key type, compare the key in the token with
// the key in argument description as well.
bool CNetScheduleHandler::x_TypeMatch(SArgument *argsDescr, ENSTokenType ttype,
                                      const char* token, unsigned tsize)
{
    if (argsDescr->ttype != ttype) return false;
    if (argsDescr->ttype != eNST_KeyNone && argsDescr->ttype != eNST_KeyId &&
        argsDescr->ttype != eNST_KeyInt && argsDescr->ttype != eNST_KeyStr)
        return true;
    const char* eq_pos = strchr(token, '=');
    if (!eq_pos) return false;
    return strncmp(argsDescr->key, token, eq_pos - token) == 0;
}


void CNetScheduleHandler::x_GetValue(ENSTokenType ttype,
                                     const char* token, int tsize,
                                     const char*&val, int& vsize)
{
    if (ttype != eNST_KeyNone && ttype != eNST_KeyId &&
        ttype != eNST_KeyInt && ttype != eNST_KeyStr) {
        val = token;
        vsize = tsize;
        return;
    }
    const char* eq_pos = strchr(token, '=');
    if (!eq_pos) {
        val = 0;
        vsize = 0;
        return;
    }
    if (ttype == eNST_KeyStr) ++eq_pos;
    val = eq_pos + 1;
    vsize = tsize - (val-token);
    if (ttype == eNST_KeyStr) --vsize;
}


CNetScheduleHandler::FProcessor CNetScheduleHandler::ParseRequest(
    const char* reqstr, SJS_Request* req)
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

    FProcessor processor = &CNetScheduleHandler::ProcessError;
    const char* s = reqstr;
    const char* token;
    int tsize;
    ENSTokenType ttype = x_GetToken(s, token, tsize);
    if (ttype != eNST_Id) {
        req->err_msg = "Command absent";
        return &CNetScheduleHandler::ProcessError;
    }
    SArgument *argsDescr = 0;
    // Look up command
    int n_cmd;
    for (n_cmd = 0; sm_CommandMap[n_cmd].cmd; ++n_cmd) {
        if (strncmp(token, sm_CommandMap[n_cmd].cmd, tsize) == 0 &&
            strlen(sm_CommandMap[n_cmd].cmd) == (unsigned) tsize) {
            processor = sm_CommandMap[n_cmd].processor;
            argsDescr = sm_CommandMap[n_cmd].args;
            break;
        }
    }
    if (!argsDescr) {
        req->err_msg = "Unknown request";
        return &CNetScheduleHandler::ProcessError;
    }
    x_CheckAccess(sm_CommandMap[n_cmd].role);
    // Parse arguments
    while (argsDescr->atype != eNSA_None && // extra arguments are just ignored
           (ttype = x_GetToken(s, token, tsize)) >= 0) // end or error
    {
        // tok processing here
        bool matched = false;
        while (argsDescr->atype != eNSA_None &&
               !(matched = x_TypeMatch(argsDescr, ttype, token, tsize))) {
            // Can skip optional arguments
            if (argsDescr->atype == eNSA_Required) break;
            if (argsDescr->atype == eNSA_Optional) {
                req->SetField(argsDescr->dest, argsDescr->dflt, -1);
                ++argsDescr;
                continue;
            }
            while (argsDescr->atype == eNSA_Optchain) {
                req->SetField(argsDescr->dest, argsDescr->dflt, -1);
                ++argsDescr;
            }
        }
        if (argsDescr->atype == eNSA_None || !matched) break;
        // accept argument
        const char* val;
        int vsize;
        x_GetValue(ttype, token, tsize, val, vsize);
        req->SetField(argsDescr->dest, val, vsize, argsDescr->dflt);
        ++argsDescr;
    }
    while (argsDescr->atype != eNSA_None && argsDescr->atype != eNSA_Required)
        ++argsDescr;
    if (ttype == eNST_Error || argsDescr->atype != eNSA_None) {
        req->err_msg = "Malformed ";
        req->err_msg.append(sm_CommandMap[n_cmd].cmd);
        req->err_msg.append(" request");
        return &CNetScheduleHandler::ProcessError;
    }

    return processor;
}


void CNetScheduleHandler::WriteMsg(const char*    prefix, 
                                   const char*    msg,
                                   bool           comm_control,
                                   bool           msg_size_control)
{
    CSocket& socket = GetSocket();
    size_t msg_length = 0;
    const char* buf_ptr;
    string buffer;
    if (msg_size_control) {
        char* ptr = m_MsgBuffer;

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
        msg_length = ptr - m_MsgBuffer;
        buf_ptr = m_MsgBuffer;
    } else {
        buffer = prefix;
        buffer += msg;
        buffer += "\r\n";
        msg_length = buffer.size();
        buf_ptr = buffer.c_str();
    }

    size_t n_written;
    EIO_Status io_st = 
        socket.Write(buf_ptr, msg_length, &n_written);
    if (comm_control && io_st) {
        NCBI_THROW(CNetServiceException, 
                   eCommunicationError, "Socket write error.");
    }
}


void CNetScheduleHandler::x_MakeLogMessage(BUF buffer, string& lmsg)
{
    CSocket& socket = GetSocket();
    string peer = socket.GetPeerAddress();
    // get only host name
    string::size_type offs = peer.find_first_of(":");
    if (offs != string::npos) {
        peer.erase(offs, peer.length());
    }

    lmsg = peer;
    lmsg += ';';
    lmsg += m_LocalTimer.GetLocalTime().AsString();
    lmsg += ';';
    lmsg += m_AuthString;
    lmsg += ';';
    lmsg += m_QueueName;
    lmsg += ';';
    lmsg += m_Request;
    lmsg += "\n";
}


void CNetScheduleHandler::x_MakeGetAnswer(const char*   key_buf,
                                          const char*   jout,
                                          const char*   jerr,
                                          unsigned      job_mask)
{
    _ASSERT(jout);
    _ASSERT(jerr);

    string& answer = m_Answer;
    answer = key_buf;
    answer.append(" \"");
    answer.append(m_JobReq.input);
    answer.append("\"");

    answer.append(" \"");
    answer.append(jout);
    answer.append("\"");

    answer.append(" \"");
    answer.append(jerr);
    answer.append("\"");

    answer.append(" ");
    answer.append(NStr::UIntToString(job_mask));
}


//////////////////////////////////////////////////////////////////////////
// CNetScheduleServer implementation
CNetScheduleServer::CNetScheduleServer(unsigned int    port,
                                       bool            use_hostname,
                                       CQueueDataBase* qdb,
                                       unsigned        network_timeout,
                                       bool            is_log)
:   m_Port(port),
    m_Shutdown(false),
    m_SigNum(0),
    m_InactivityTimeout(network_timeout),
    m_StartTime(CTime::eCurrent),
    m_AccessLog("ns_access.log", 100 * 1024 * 1024)
{
    m_QueueDB = qdb;

    m_HostNetAddr = CSocketAPI::gethostbyname(kEmptyStr);
    if (use_hostname) {
        m_Host = CSocketAPI::gethostname();
    } else {
        unsigned int hostaddr = SOCK_HostToNetLong(m_HostNetAddr);
        char ipaddr[32];
        sprintf(ipaddr, "%u.%u.%u.%u", (hostaddr >> 24) & 0xff,
                                       (hostaddr >> 16) & 0xff,
                                       (hostaddr >> 8)  & 0xff,
                                        hostaddr        & 0xff);
        m_Host = ipaddr;
    }

    s_netschedule_server = this;


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
    STimeout m_ServerAcceptTimeout;
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


        // mount default queue - it is needed for authentication of admin operations
        // No longer needed for code which does not access m_Queue and m_Monitor
        // string qname = "noname";
        // LOG_POST(Info << "Mounting queue: " << qname);
        // SQueueParameters qparams;
        // qdb->MountQueue(qname, qparams);


        // Scan and mount queues
        unsigned min_run_timeout = 3600;

        qdb->Configure(reg, &min_run_timeout);

        LOG_POST(Info << "Running execution control every " 
                      << min_run_timeout << " seconds. ");
        min_run_timeout = min_run_timeout >= 0 ? min_run_timeout : 2;
        
        
        qdb->RunExecutionWatcherThread(min_run_timeout);
        qdb->RunPurgeThread();
        qdb->RunNotifThread();


        SServer_Parameters params;

        unsigned max_connections =
            reg.GetInt("server", "max_connections", 100, 0, CNcbiRegistry::eReturn);

        unsigned init_threads =
            reg.GetInt("server", "init_threads", 10, 0, CNcbiRegistry::eReturn);
        params.init_threads = init_threads < max_threads ?
                              init_threads : max_threads;

        unsigned network_timeout =
            reg.GetInt("server", "network_timeout", 10, 0, CNcbiRegistry::eReturn);
        if (network_timeout == 0) {
            LOG_POST(Warning << 
                "INI file sets 0 sec. network timeout. Assume 10 seconds.");
            network_timeout =  10;
        }

        bool is_log =
            reg.GetBool("server", "log", false, 0, CNcbiRegistry::eReturn);

        bool use_hostname =
            reg.GetBool("server", "use_hostname", false, 0, CNcbiRegistry::eReturn);

        params.max_threads     = max_threads;
        params.max_connections = max_connections;
        params.queue_size      = max_connections;
        m_ServerAcceptTimeout.sec = 1;
        m_ServerAcceptTimeout.usec = 0;
        params.accept_timeout  = &m_ServerAcceptTimeout;

        auto_ptr<CNetScheduleServer> server(
            new CNetScheduleServer(port,
                                   use_hostname,
                                   qdb.release(),
                                   network_timeout,
                                   is_log));

        string admin_hosts =
            reg.GetString("server", "admin_host", kEmptyStr);
        if (!admin_hosts.empty()) {
            server->SetAdminHosts(admin_hosts);
        }
        
        server->SetParameters(params);

        server->AddListener(
            new CNetScheduleConnectionFactory(&*server),
            port);

        NcbiCout << "Running server on port " << port << NcbiEndl;
        LOG_POST(Info << "Running server on port " << port);

        server->Run();

        LOG_POST("NetSchedule server stopped.");

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
 * Revision 1.111  2006/12/04 23:31:30  joukovv
 * Access control/version control checks corrected.
 *
 * Revision 1.110  2006/12/04 22:07:39  joukovv
 * Access checks corrected, version bumped
 *
 * Revision 1.109  2006/12/04 21:58:32  joukovv
 * netschedule_control commands for dynamic queue creation, access control
 * centralized
 *
 * Revision 1.108  2006/12/01 00:10:58  joukovv
 * Dynamic queue creation implemented.
 *
 * Revision 1.107  2006/11/29 17:25:16  joukovv
 * New option and behavior use_hostname introduced, version bumped.
 *
 * Revision 1.106  2006/11/27 16:46:21  joukovv
 * Iterator to CQueueCollection introduced to decouple it with CQueueDataBase;
 * un-nested CQueue from CQueueDataBase; instrumented code to count job
 * throughput average.
 *
 * Revision 1.105  2006/11/14 01:57:56  ucko
 * Make CNetScheduleHandler's internal types public to fix WorkShop compilation.
 *
 * Revision 1.104  2006/11/13 19:15:35  joukovv
 * Protocol parser re-implemented. Remnants of ThreadData removed.
 *
 * Revision 1.103  2006/10/31 19:35:26  joukovv
 * Queue creation and reading of its parameters decoupled. Code reorganized to
 * reduce coupling in general. Preparing for queue-on-demand.
 *
 * Revision 1.102  2006/10/19 20:38:20  joukovv
 * Works in thread-per-request mode. Errors in BDB layer fixed.
 *
 * Revision 1.101  2006/10/10 15:22:13  didenko
 * Changed version number
 *
 * Revision 1.100  2006/10/03 14:56:57  joukovv
 * Delayed job deletion implemented, code restructured preparing to move to
 * thread-per-request model.
 *
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
