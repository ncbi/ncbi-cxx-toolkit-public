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
#include <connect/services/netschedule_api.hpp>
#include <connect/ncbi_conn_stream.hpp>

#include <bdb/bdb_expt.hpp>
#include <bdb/bdb_cursor.hpp>

#include "bdb_queue.hpp"

#include "ns_types.hpp"
#include "job_status.hpp"
#include "access_list.hpp"

#include "netschedule_version.hpp"

#define NETSCHEDULED_FEATURES \
    "fast_status=1;dyn_queues=1;tags=1;version=" NETSCHEDULED_VERSION

#if defined(NCBI_OS_UNIX)
# include <corelib/ncbi_os_unix.hpp>
# include <signal.h>
#endif


USING_NCBI_SCOPE;

#define NETSCHEDULED_FULL_VERSION \
    "NCBI NetSchedule server Version " NETSCHEDULED_VERSION \
    " build " __DATE__ " " __TIME__

static int s_TokenToInt(const char*tok, int size)
{
    bool neg = size > 0 && tok[0] == '-';
    if (neg) {
        tok++; size--;
    }
    int res = 0;
    while (size-- > 0) {
        res = res*10 + *tok++ - '0';
    }
    if (neg) res = -res;
    return res;
}

class CNetScheduleServer;
static CNetScheduleServer* s_netschedule_server = 0;

/// Request context
///
/// @internal
enum ENSRequestField {
    eNSRF_Input = 0,
    eNSRF_Output,
    eNSRF_ProgressMsg,
    eNSRF_AffinityToken,
    eNSRF_JobKey,
    eNSRF_JobReturnCode,
    eNSRF_Port,
    eNSRF_Timeout,
    eNSRF_JobMask,
    eNSRF_Flags,
    eNSRF_ErrMsg,
    eNSRF_Option,
    eNSRF_Status,
    eNSRF_QueueName,
    eNSRF_QueueClass,
    eNSRF_QueueComment,
    eNSRF_Tags,
    eNSRF_AffinityPrev,
    // for Query
    eNSRF_Select,
    eNSRF_Where,
    eNSRF_Action,
    eNSRF_Fields
};
struct SJS_Request
{
    string    input;
    string    output;
    char      progress_msg[kNetScheduleMaxDBDataSize];
    char      affinity_token[kNetScheduleMaxDBDataSize];
    string    job_key;
    int       job_return_code;
    unsigned  port;
    unsigned  timeout;
    unsigned  job_mask;
    unsigned  flags;   
    string    err_msg;
    string    param1;
    string    param2;
    string    param3;
    string    tags;

    void Init()
    {
        input[0] = output[0] = progress_msg[0] = affinity_token[0] = 0;
        job_key.erase(); err_msg.erase();
        param1.erase(); param2.erase(); param3.erase();
        job_return_code = port = timeout = job_mask = 0;
    }
    void SetField(ENSRequestField fld,
                  const char* val, int size)
    {
        if (!val) {
            val = "";
            size = 0;
        }
        if (size < 0) size = strlen(val);
        unsigned eff_size = (unsigned) size > kNetScheduleMaxDBDataSize - 1 ?
            kNetScheduleMaxDBDataSize - 1 :
            (unsigned) size;
        switch (fld) {
        case eNSRF_Input:
            input.erase(); input.append(val, size);
            break;
        case eNSRF_Output:
            output.erase(); output.append(val, size);
            break;
        case eNSRF_ProgressMsg:
            strncpy(progress_msg, val, eff_size);
            progress_msg[eff_size] = 0;
            break;
        case eNSRF_AffinityToken:
            strncpy(affinity_token, val, eff_size);
            affinity_token[eff_size] = 0;
            break;
        case eNSRF_JobKey:
            job_key.erase(); job_key.append(val, eff_size);
            break;
        case eNSRF_JobReturnCode:
            job_return_code = s_TokenToInt(val, size);
            break;
        case eNSRF_Port:
            port = s_TokenToInt(val, size);
            break;
        case eNSRF_Timeout:
            timeout = s_TokenToInt(val, size);
            break;
        case eNSRF_JobMask:
            job_mask = s_TokenToInt(val, size);
            break;
        case eNSRF_Flags:
	    flags = s_TokenToInt(val, size);   
            break;
        case eNSRF_ErrMsg:
            err_msg.erase(); err_msg.append(val, eff_size);
            break;
        case eNSRF_Option:
        case eNSRF_Status:
        case eNSRF_QueueName:
        case eNSRF_AffinityPrev:
            param1.erase(); param1.append(val, eff_size);
            break;
        case eNSRF_QueueClass:
        case eNSRF_Action:
            param2.erase(); param2.append(val, eff_size);
            break;
        case eNSRF_QueueComment:
            param3.erase(); param3.append(val, eff_size);
            break;
        case eNSRF_Tags:
            tags.erase(); tags.append(val, size);
            break;
        case eNSRF_Select:
        case eNSRF_Where:
            param1.erase(); param1.append(val, size);
            break;
        case eNSRF_Fields:
            param3.erase(); param3.append(val, size);
            break;
        default:
            break;
        }
    }
};


const unsigned kMaxMessageSize = kNetScheduleMaxDBErrSize * 4;

//////////////////////////////////////////////////////////////////////////
/// ConnectionHandler for NetScheduler

class CNetScheduleServer;
class CNetScheduleHandler : public IServer_LineMessageHandler
{
public:
    CNetScheduleHandler(CNetScheduleServer* server);
    // MessageHandler protocol
    virtual EIO_Event GetEventsToPollFor(const CTime** alarm_time) const;
    virtual void OnOpen(void);
    virtual void OnWrite(void);
    virtual void OnClose(void) { }
    virtual void OnTimeout(void);
    virtual void OnOverflow(void);
    virtual void OnMessage(BUF buffer);

    void WriteMsg(const char*   prefix,
                  const string& msg = kEmptyStr,
                  bool          comm_control = false);

private:
    // Message processing phases
    void ProcessMsgAuth(BUF buffer);
    void ProcessMsgQueue(BUF buffer);
    void ProcessMsgRequest(BUF buffer);
    // Message processing for ProcessSubmitBatch phases
    void ProcessMsgBatchHeader(BUF buffer);
    void ProcessMsgBatchItem(BUF buffer);
    void ProcessMsgBatchEnd(BUF buffer);

    // Monitoring
    /// Are we monitoring?
    bool IsMonitoring();
    /// Send string to monitor
    void MonitorPost(const string& msg);


public:
    enum ENSAccess {
        eNSAC_Queue         = 1 << 0,
        eNSAC_Worker        = 1 << 1,
        eNSAC_Submitter     = 1 << 2,
        eNSAC_Admin         = 1 << 3,
        eNSAC_QueueAdmin    = 1 << 4,
        eNSAC_Test          = 1 << 5,
        eNSAC_DynQueueAdmin = 1 << 6,
        eNSAC_DynClassAdmin = 1 << 7,
        eNSAC_AnyAdminMask  = eNSAC_Admin | eNSAC_QueueAdmin |
                              eNSAC_DynQueueAdmin | eNSAC_DynClassAdmin,
        // Combination of flags for client roles
        eNSCR_Any        = 0,
        eNSCR_Queue      = eNSAC_Queue,
        eNSCR_Worker     = eNSAC_Worker + eNSAC_Queue,
        eNSCR_Submitter  = eNSAC_Submitter + eNSAC_Queue,
        eNSCR_Admin      = eNSAC_Admin,
        eNSCR_QueueAdmin = eNSAC_QueueAdmin + eNSAC_Queue 
    };
    typedef unsigned TNSClientRole;

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
        fNSA_Optional = 1 << 0, // the argument may be omited
        fNSA_Chain    = 1 << 1, // if the argument is absent, whole
                                // chain is ignored
        fNSA_Or       = 1 << 2, // This argument is ORed to next a|b
                                // as opposed to default sequence a b
        fNSA_Match    = 1 << 3, // This argument is exact match with "key" field
                                // and uses "dflt" field as value
                                // as opposed to default using the arg as value
        // Typical values
        eNSA_None     = -1, // end of arg list
        eNSA_Required = 0,  // argument must be present
        eNSA_Optional = fNSA_Optional,
        eNSA_Optchain = fNSA_Optional | fNSA_Chain
    };
    typedef int TNSArgType;
    typedef void (CNetScheduleHandler::*FProcessor)(void);
    struct SArgument {
        TNSArgType      atype;
        ENSTokenType    ttype;
        ENSRequestField dest;
        const char*     dflt;
        const char*     key; // for eNST_KeyId and eNST_KeyInt
    };
    enum { kMaxArgs = 8 };
    struct SCommandMap {
        const char*   cmd;
        FProcessor    processor;
        TNSClientRole role;
        SArgument     args[kMaxArgs+1]; // + end of record
    };
private:
    static SArgument sm_End;
    static SCommandMap sm_CommandMap[];
    static SArgument sm_BatchArgs[];

    FProcessor ParseRequest(const string& reqstr);

    // Command processors
    void ProcessFastStatusS();
    void ProcessFastStatusW();
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
    void ProcessQueueInfo();
    void ProcessQuery();
    void ProcessSelectQuery();
    void ProcessGetParam();

    // Delayed output handlers
    void WriteProjection();

private:
    static
    bool x_TokenMatch(const SArgument *arg_descr, ENSTokenType ttype,
                      const char* token, int tsize);
    static
    ENSTokenType x_GetToken(const char*& s, const char* end,
                            const char*& tok, int& size);
    static
    bool x_GetValue(ENSTokenType ttype,
                    const char* token, int tsize,
                    const char*&val, int& vsize);
    static
    void x_ParseTags(const string& strtags, TNSTagList& tags);
    void x_MonitorRec(const SNS_SubmitRecord& rec);

    bool x_ParseArguments(const char* s, const char* end,
                          const SArgument* arg_descr);

    bool x_CheckVersion(void);
    void x_CheckAccess(TNSClientRole role);
    void x_AccessViolationMessage(unsigned deficit, string& msg);
    string x_FormatErrorMessage(string header, string what);
    void x_WriteErrorToMonitor(string msg);
    // Moved from CNetScheduleServer
    void x_MakeLogMessage(string& lmsg);
    void x_MakeGetAnswer(const char* key_buf,
                         unsigned job_mask,
                         const string& aff_token);

    // Data
    string m_Request;
    string m_Answer;
    char   m_MsgBuffer[kMaxMessageSize];

    unsigned                    m_PeerAddr;
    string                      m_AuthString;
    CNetScheduleServer*         m_Server;
    string                      m_QueueName;
    auto_ptr<CQueue>            m_Queue;
    SJS_Request                 m_JobReq;
    // Uncapabilities - that is combination of ENSAccess
    // rights, which can NOT be performed by this connection
    unsigned                    m_Uncaps;
    unsigned                    m_Unreported;
    bool                        m_VersionControl;
    // Unique command number for relating command and reply
    unsigned                    m_CommandNumber;
    // Phase of connection - login, queue, command, batch submit etc.
    void (CNetScheduleHandler::*m_ProcessMessage)(BUF buffer);
    // Delayed output processor
    void (CNetScheduleHandler::*m_DelayedOutput)();

    // Batch submit data
    unsigned                    m_BatchSize;
    unsigned                    m_BatchPos;
    CStopWatch                  m_BatchStopWatch;
    vector<SNS_SubmitRecord>    m_BatchSubmitVector;

    // For projection writer
    auto_ptr<TNSBitVector>      m_SelectedIds;
    SFieldsDescription          m_FieldDescr;

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
    IServer_ConnectionHandler* Create(void)
    {
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
                       unsigned        network_timeout,
                       bool            is_log); 

    virtual ~CNetScheduleServer();

    virtual bool ShutdownRequested(void)
    {
        return m_Shutdown;
    }

    void SetQueueDB(CQueueDataBase* qdb)
    {
        delete m_QueueDB;
        m_QueueDB = qdb;
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
    /// TRUE if logging is ON
    bool IsLog() const { return m_LogFlag.Get() != 0; }
    void SetLogging(bool flag) {
        m_LogFlag.Set(flag);
    }
    unsigned GetCommandNumber() { return m_AtomicCommandNumber.Add(1); }
    CQueueDataBase* GetQueueDB(void) { return m_QueueDB; }
    unsigned GetInactivityTimeout(void) { return m_InactivityTimeout; }
    string& GetHost() { return m_Host; }
    unsigned GetPort() { return m_Port; }
    unsigned GetHostNetAddr() { return m_HostNetAddr; }
    const CTime& GetStartTime(void) const { return m_StartTime; }
    const CNetSchedule_AccessList& GetAccessList(void) const { return m_AdminHosts; }

protected:
    virtual void Exit();

private:
    void x_CreateLog();

private:
    /// Host name where server runs
    string                  m_Host;
    unsigned                m_Port;
    unsigned                m_HostNetAddr;
    bool                    m_Shutdown;
    int                     m_SigNum;  ///< Shutdown signal number
    /// Time to wait for the client (seconds)
    unsigned                m_InactivityTimeout;
    CQueueDataBase*         m_QueueDB;
    CTime                   m_StartTime;
    CAtomicCounter          m_LogFlag;
    /// Quick local timer
    CFastLocalTime          m_LocalTimer;

    /// List of admin stations
    CNetSchedule_AccessList m_AdminHosts;

    CAtomicCounter          m_AtomicCommandNumber;
};


static void s_BufReadHelper(void* data, void* ptr, size_t size)
{
    ((string*) data)->append((const char *) ptr, size);
}


static void s_ReadBufToString(BUF buf, string& str)
{
    str.erase();
    size_t size = BUF_Size(buf);
    str.reserve(size);
    BUF_PeekAtCB(buf, 0, s_BufReadHelper, &str, size);
    BUF_Read(buf, NULL, size);
}


//////////////////////////////////////////////////////////////////////////
// ConnectionHandler implementation

CNetScheduleHandler::CNetScheduleHandler(CNetScheduleServer* server)
    : m_Server(server), m_Uncaps(~0L), m_Unreported(~0L),
      m_VersionControl(false)
{
}


EIO_Event CNetScheduleHandler::GetEventsToPollFor(const CTime** /*alarm_time*/) const
{
    if (m_DelayedOutput) return eIO_Write;
    return eIO_Read;
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
    m_DelayedOutput = NULL;
}


void CNetScheduleHandler::OnWrite()
{
    if (m_DelayedOutput)
        (this->*m_DelayedOutput)();
}


void CNetScheduleHandler::OnTimeout()
{
//    ERR_POST("OnTimeout!");
}


void CNetScheduleHandler::OnOverflow() 
{ 
//    ERR_POST("OnOverflow!");
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
        WriteMsg("ERR:", string(ex.GetErrCodeString()) + ":" + ex.GetMsg());
        string msg = x_FormatErrorMessage("Server error", ex.what());
        ERR_POST(msg);
        x_WriteErrorToMonitor(msg);
        throw;
    }
    catch (CNetServiceException &ex)
    {
        WriteMsg("ERR:", string(ex.GetErrCodeString()) + ":" + ex.GetMsg());
        string msg = x_FormatErrorMessage("Server error", ex.what());
        ERR_POST(msg);
        x_WriteErrorToMonitor(msg);
        throw;
    }
    catch (CBDB_ErrnoException& ex)
    {
        if (ex.IsRecovery()) {
            string msg = x_FormatErrorMessage("Fatal Berkeley DB error: DB_RUNRECOVERY. " 
                                              "Emergency shutdown initiated!", ex.what());
            ERR_POST(msg);
            x_WriteErrorToMonitor(msg);
            m_Server->SetShutdownFlag();
        } else {
            string msg = x_FormatErrorMessage("BDB error", ex.what());
            ERR_POST(msg);
            x_WriteErrorToMonitor(msg);

            string err = "eInternalError:Internal database error - ";
            err += ex.what();
            err = NStr::PrintableString(err);
            WriteMsg("ERR:", err);
        }
        throw;
    }
    catch (CBDB_Exception &ex)
    {
        string err = "eInternalError:Internal database (BDB) error - ";
        err += ex.what();
        WriteMsg("ERR:", NStr::PrintableString(err));

        string msg = x_FormatErrorMessage("BDB error", ex.what());
        ERR_POST(msg);
        x_WriteErrorToMonitor(msg);
        throw;
    }
    catch (exception& ex)
    {
        string err = "eInternalError:Internal error - ";
        err += ex.what();
        WriteMsg("ERR:", NStr::PrintableString(err));

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
    if (IsMonitoring()) {
        MonitorPost(m_LocalTimer.GetLocalTime().AsString() + msg);
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
        m_Uncaps &= ~(eNSAC_Admin | eNSAC_QueueAdmin);
    }
    m_ProcessMessage = &CNetScheduleHandler::ProcessMsgQueue;
}


void CNetScheduleHandler::ProcessMsgQueue(BUF buffer)
{
    s_ReadBufToString(buffer, m_QueueName);

    if (m_QueueName != "noname" && !m_QueueName.empty()) {
        m_Queue.reset(new CQueue(*(m_Server->GetQueueDB()),
                                 m_QueueName, m_PeerAddr));
        m_Uncaps &= ~eNSAC_Queue;
        if (m_Queue->IsWorkerAllowed())
            m_Uncaps &= ~eNSAC_Worker;
        if (m_Queue->IsSubmitAllowed())
            m_Uncaps &= ~eNSAC_Submitter;
        if (m_Queue->IsVersionControl())
            m_VersionControl = true;
    }

    m_ProcessMessage = &CNetScheduleHandler::ProcessMsgRequest;
}


// Workhorse method
void CNetScheduleHandler::ProcessMsgRequest(BUF buffer)
{
    bool is_log = m_Server->IsLog();

    s_ReadBufToString(buffer, m_Request);

    m_CommandNumber = m_Server->GetCommandNumber();

    // Logging
    if (is_log || IsMonitoring()) {
        string lmsg;
        x_MakeLogMessage(lmsg);
        if (is_log) {
            NCBI_NS_NCBI::CNcbiDiag(eDiag_Info, eDPF_Log).GetRef()
                << lmsg
                << NCBI_NS_NCBI::Endm;
        }
        if (IsMonitoring()) {
            lmsg += "\n";
            MonitorPost(lmsg);
        }
    }

    m_JobReq.Init();
    FProcessor requestProcessor = ParseRequest(m_Request);

    if (requestProcessor == &CNetScheduleHandler::ProcessQuitSession) {
        ProcessQuitSession();
        return;
    }

    // program version control
    if (m_VersionControl &&
        // we want status request to be fast, skip version control 
        (requestProcessor != &CNetScheduleHandler::ProcessStatus) &&
        // bypass for admin tools
        (m_Uncaps & eNSAC_Admin)) {
        if (!x_CheckVersion()) {
            WriteMsg("ERR:", "eInvalidClientOrVersion:");
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
    for (; *prog_str && *prog_str != '\''; ++prog_str) {
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

void CNetScheduleHandler::x_CheckAccess(TNSClientRole role)
{
    unsigned deficit = role & m_Uncaps;
    if (!deficit) return;
    if (deficit & eNSAC_DynQueueAdmin) {
        // check that we have admin rights on queue m_JobReq.param1
        deficit &= ~eNSAC_DynQueueAdmin;
    }
    if (deficit & eNSAC_DynClassAdmin) {
        // check that we have admin rights on class m_JobReq.param2
        deficit &= ~eNSAC_DynClassAdmin;
    }
    if (!deficit) return;
    bool deny =
        (deficit & eNSAC_AnyAdminMask)  ||  // for any admin access
        (m_Uncaps & eNSAC_Queue)        ||  // or if no queue
        m_Queue->IsDenyAccessViolations();  // or if queue configured so
    bool report =
        !(m_Uncaps & eNSAC_Queue)         &&  // only if there is a queue
        m_Queue->IsLogAccessViolations()  &&  // so configured
        (m_Unreported & deficit);             // and we did not report it yet
    if (report) {
        m_Unreported &= ~deficit;
        CSocket& socket = GetSocket();
        string msg = "Unauthorized access from: ";
        msg += socket.GetPeerAddress();
        msg += " ";
        msg += m_AuthString;
        x_AccessViolationMessage(deficit, msg);
        LOG_POST(Warning << msg);
    } else {
        m_Unreported = ~0L;
    }
    if (deny) {
        string msg = "Access denied:";
        x_AccessViolationMessage(deficit, msg);
        NCBI_THROW(CNetScheduleException, eAccessDenied, msg);
    }
}


void CNetScheduleHandler::x_AccessViolationMessage(unsigned deficit,
                                                   string& msg)
{
    if (deficit & eNSAC_Queue)
        msg += " queue required";
    if (deficit & eNSAC_Worker)
        msg += " worker node privileges required";
    if (deficit & eNSAC_Submitter)
        msg += " submitter privileges required";
    if (deficit & eNSAC_Admin)
        msg += " admin privileges required";
    if (deficit & eNSAC_QueueAdmin)
        msg += " queue admin privileges required";
}


void CNetScheduleHandler::x_ParseTags(const string& strtags, TNSTagList& tags)
{
    list<string> tokens;
    NStr::Split(NStr::ParseEscapes(strtags), "\t",
        tokens, NStr::eNoMergeDelims);
    tags.clear();
    for (list<string>::iterator it = tokens.begin(); it != tokens.end(); ++it) {
        string key(*it); ++it;
        if (it != tokens.end()){
            tags.push_back(TNSTag(key, *it));
        } else {
            tags.push_back(TNSTag(key, kEmptyStr));
            break;
        }
    }
}


void CNetScheduleHandler::x_MonitorRec(const SNS_SubmitRecord& rec)
{
    string msg;

    msg += string("id: ") + NStr::IntToString(rec.job_id);
    msg += string(" input: ") + rec.input;
    msg += string(" aff: ") + rec.affinity_token;
    msg += string(" aff_id: ") + NStr::IntToString(rec.affinity_id);
    msg += string(" mask: ") + NStr::IntToString(rec.mask);
    msg += string(" tags: ");
    ITERATE(TNSTagList, it, rec.tags) {
        msg += it->first + "=" + it->second + "; ";
    }
    MonitorPost(msg);
}


//////////////////////////////////////////////////////////////////////////
// Process* methods for processing commands

void CNetScheduleHandler::ProcessFastStatusS()
{
    unsigned job_id = CNetScheduleKey(m_JobReq.job_key).id;

    CNetScheduleAPI::EJobStatus status = m_Queue->GetStatus(job_id);
    // TODO: update timestamp
    WriteMsg("OK:", NStr::IntToString((int) status));
}


void CNetScheduleHandler::ProcessFastStatusW()
{
    unsigned job_id = CNetScheduleKey(m_JobReq.job_key).id;

    CNetScheduleAPI::EJobStatus status = m_Queue->GetStatus(job_id);
    WriteMsg("OK:", NStr::IntToString((int) status));
}


void CNetScheduleHandler::ProcessSubmit()
{
    SNS_SubmitRecord rec;
    rec.input = m_JobReq.input;
    strcpy(rec.affinity_token, m_JobReq.affinity_token);
    x_ParseTags(m_JobReq.tags, rec.tags);
    rec.mask = m_JobReq.job_mask;
    unsigned job_id =
        m_Queue->Submit(&rec, 
                        m_PeerAddr, 
                        m_JobReq.port, m_JobReq.timeout, 
                        m_JobReq.progress_msg);

    char buf[1024];
    sprintf(buf, NETSCHEDULE_JOBMASK, 
                 job_id, m_Server->GetHost().c_str(), unsigned(m_Server->GetPort()));

    WriteMsg("OK:", buf);

    if (IsMonitoring()) {
        CSocket& socket = GetSocket();
        string msg = "::ProcessSubmit ";
        msg += m_LocalTimer.GetLocalTime().AsString();
        msg += buf;
        msg += " ==> ";
        msg += socket.GetPeerAddress();
        msg += " ";
        msg += m_AuthString;

        MonitorPost(msg);
        x_MonitorRec(rec);
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
    const char* s = cmd;
    const char* end = cmd + msg_size;
    const char* token;
    int tsize;
    ENSTokenType ttype = x_GetToken(s, end, token, tsize);
    if (ttype == eNST_Id && tsize == 4 && strncmp(token, "BTCH", tsize) == 0) {
        ttype = x_GetToken(s, end, token, tsize);
        if (ttype == eNST_Int) {
            m_BatchPos = 0;
            m_BatchSize = s_TokenToInt(token, tsize);
            m_BatchStopWatch.Restart();
            m_BatchSubmitVector.resize(m_BatchSize);
            m_ProcessMessage = &CNetScheduleHandler::ProcessMsgBatchItem;
            if (IsMonitoring()) {
                CSocket& socket = GetSocket();
                string msg = "::ProcessMsgBatchHeader ";
                msg += m_LocalTimer.GetLocalTime().AsString();
                msg += " ";
                msg += socket.GetPeerAddress();
                msg += " ";
                msg += m_AuthString;
            }
            return;
        } // else error???
    } else {
        if (ttype != eNST_Id || tsize != 4 || strncmp(token, "ENDS", 4) != 0) {
            BUF_Read(buffer, 0, BUF_Size(buffer));
            WriteMsg("ERR:", "eProtocolSyntaxError:"
                             "Batch submit error - BATCH expected");
        }
    }
    m_BatchSubmitVector.clear();
    m_ProcessMessage = &CNetScheduleHandler::ProcessMsgRequest;
    WriteMsg("OK:");
}

CNetScheduleHandler::SArgument CNetScheduleHandler::sm_BatchArgs[] = {
    { eNSA_Required, eNST_Str,     eNSRF_Input },
    { eNSA_Optional | fNSA_Or | fNSA_Match, eNST_Id, eNSRF_AffinityPrev, "", "affp" },
    { eNSA_Optional, eNST_KeyStr,  eNSRF_AffinityToken, "", "aff" },
    { eNSA_Optional, eNST_KeyInt,  eNSRF_JobMask, "0", "msk" },
    { eNSA_Optional, eNST_KeyStr,  eNSRF_Tags, "", "tags" },
    { eNSA_None }
};

bool CNetScheduleHandler::x_ParseArguments(const char* s, const char* end,
                                           const SArgument* arg_descr)
{
    const char* token;
    int tsize;
    ENSTokenType ttype = eNST_None; // if arglist is empty, it should be successful

    while (arg_descr->atype != eNSA_None && // extra arguments are just ignored
           (ttype = x_GetToken(s, end, token, tsize)) >= 0) // end or error
    {
        // token processing here
        bool matched = false;
        while (arg_descr->atype != eNSA_None &&
               !(matched = x_TokenMatch(arg_descr, ttype, token, tsize))) {
            // Can skip optional arguments
            if (!(arg_descr->atype & fNSA_Optional)) break;
            if (!(arg_descr->atype & fNSA_Chain)) {
                m_JobReq.SetField(arg_descr->dest, arg_descr->dflt, -1);
                ++arg_descr;
                continue;
            }
            while (arg_descr->atype != eNSA_None  &&
                   arg_descr->atype & fNSA_Chain) {
                m_JobReq.SetField(arg_descr->dest, arg_descr->dflt, -1);
                ++arg_descr;
            }
        }
        if (arg_descr->atype == eNSA_None || !matched) break;
        // accept argument
        const char* val;
        int vsize;
        bool arg_set = x_GetValue(ttype, token, tsize, val, vsize);
        if (arg_set)
            m_JobReq.SetField(arg_descr->dest, val, vsize);
        // Process OR by skipping argument descriptions until OR flags is set
        while (arg_descr->atype != eNSA_None  &&  arg_descr->atype & fNSA_Or) {
            if (!arg_set)
                m_JobReq.SetField(arg_descr->dest, arg_descr->dflt, -1);
            ++arg_descr;
            arg_set = false;
        }
        if (arg_descr->atype != eNSA_None) {
            if (!arg_set)
                m_JobReq.SetField(arg_descr->dest, arg_descr->dflt, -1);
            ++arg_descr;
        }
    }
    // Check that remaining arg descriptors are optional
    while (arg_descr->atype != eNSA_None && (arg_descr->atype & fNSA_Optional)) {
        m_JobReq.SetField(arg_descr->dest, arg_descr->dflt, -1);
        ++arg_descr;
    }
    return ttype != eNST_Error && arg_descr->atype == eNSA_None;
}


void CNetScheduleHandler::ProcessMsgBatchItem(BUF buffer)
{
    // Expecting:
    // "input" [affp|aff="affinity_token"] [msk=1]
    //         [tags="key1\tval1\tkey2\t\tkey3\tval3"]
    size_t size = BUF_Size(buffer);
    string msg;
    s_ReadBufToString(buffer, msg);
    const char* data = msg.data();

    SNS_SubmitRecord& rec = m_BatchSubmitVector[m_BatchPos];

    if (!x_ParseArguments(data, data+size, sm_BatchArgs)) {
        WriteMsg("ERR:", "eProtocolSyntaxError:"
                         "Invalid batch submission, syntax error");
        m_ProcessMessage = &CNetScheduleHandler::ProcessMsgRequest;
        return;
    }
    rec.input = m_JobReq.input;
    if (m_JobReq.param1 == "affp") {
        rec.affinity_id = kMax_I4;
    } else {
        strcpy(rec.affinity_token, m_JobReq.affinity_token);
    }
    x_ParseTags(m_JobReq.tags, rec.tags);

    if (++m_BatchPos >= m_BatchSize)
        m_ProcessMessage = &CNetScheduleHandler::ProcessMsgBatchEnd;
}


void CNetScheduleHandler::ProcessMsgBatchEnd(BUF buffer)
{
    // Expecting ENDB
    char cmd[256];
    size_t msg_size = BUF_Read(buffer, cmd, sizeof(cmd)-1);
    const char* s = cmd;
    const char* token;
    int tsize;
    ENSTokenType ttype = x_GetToken(s, cmd+msg_size, token, tsize);
    if (ttype != eNST_Id || tsize != 4 || strncmp(token, "ENDB", tsize) != 0) {
        BUF_Read(buffer, 0, BUF_Size(buffer));
        WriteMsg("ERR:", "eProtocolSyntaxError:"
                         "Batch submit error - unexpected end of batch");
    }

    double comm_elapsed = m_BatchStopWatch.Elapsed();

    // we have our batch now

    CStopWatch sw(CStopWatch::eStart);
    unsigned job_id =
        m_Queue->SubmitBatch(m_BatchSubmitVector);
    double db_elapsed = sw.Elapsed();

    if (IsMonitoring()) {
        ITERATE(vector<SNS_SubmitRecord>, it, m_BatchSubmitVector) {
            x_MonitorRec(*it);
        }
        CSocket& socket = GetSocket();
        string msg = "::ProcessMsgSubmitBatch ";
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

        MonitorPost(msg);
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


bool CNetScheduleHandler::IsMonitoring()
{
    return (m_Uncaps & eNSAC_Queue) || m_Queue->IsMonitoring();
}


void CNetScheduleHandler::MonitorPost(const string& msg)
{
    if (m_Uncaps & eNSAC_Queue)
        return;
    m_Queue->MonitorPost(msg);
}


void CNetScheduleHandler::ProcessCancel()
{
    unsigned job_id = CNetScheduleKey(m_JobReq.job_key).id;
    m_Queue->Cancel(job_id);
    WriteMsg("OK:");
}


void CNetScheduleHandler::ProcessForceReschedule()
{
    unsigned job_id = CNetScheduleKey(m_JobReq.job_key).id;
    m_Queue->ForceReschedule(job_id);
    WriteMsg("OK:");
}


void CNetScheduleHandler::ProcessDropJob()
{
    unsigned job_id = CNetScheduleKey(m_JobReq.job_key).id;
    m_Queue->DropJob(job_id);
    WriteMsg("OK:");
}


void CNetScheduleHandler::ProcessJobRunTimeout()
{
    unsigned job_id = CNetScheduleKey(m_JobReq.job_key).id;
    m_Queue->SetJobRunTimeout(job_id, m_JobReq.timeout);
    WriteMsg("OK:");
}


void CNetScheduleHandler::ProcessJobDelayExpiration()
{
    unsigned job_id = CNetScheduleKey(m_JobReq.job_key).id;
    m_Queue->JobDelayExpiration(job_id, m_JobReq.timeout);
    WriteMsg("OK:");
}


void CNetScheduleHandler::ProcessStatusSnapshot()
{
    const char* aff_token = m_JobReq.affinity_token;
    CJobStatusTracker::TStatusSummaryMap st_map;

    bool aff_exists = m_Queue->CountStatus(&st_map, aff_token);
    if (!aff_exists) {
        WriteMsg("ERR:", string("eProtocolSyntaxError:Unknown affinity token \"")
                         + aff_token + "\"");
        return;
    }
    ITERATE(CJobStatusTracker::TStatusSummaryMap, it, st_map) {
        string st_str = CNetScheduleAPI::StatusToString(it->first);
        st_str.push_back(' ');
        st_str.append(NStr::UIntToString(it->second));
        WriteMsg("OK:", st_str);
    }
    WriteMsg("OK:", "END");
}


void CNetScheduleHandler::ProcessStatus()
{
    unsigned job_id = CNetScheduleKey(m_JobReq.job_key).id;

    CNetScheduleAPI::EJobStatus status = m_Queue->GetStatus(job_id);
    int ret_code = 0;
    string input, output, error;

    bool res = m_Queue->GetJobDescr(job_id, &ret_code, 
                                    &input, &output,
                                    &error, 0, status);
    if (!res) status = CNetScheduleAPI::eJobNotFound;
    string buf = NStr::IntToString((int) status);
    if (status != CNetScheduleAPI::eJobNotFound) {
            buf += " ";
            buf += NStr::IntToString(ret_code);
    }
    switch (status) {
    case CNetScheduleAPI::eDone:
        buf += string(" \"") + output + "\" \"\" \"" + input + "\"";
        break;

    case CNetScheduleAPI::eRunning:
    case CNetScheduleAPI::eReturned:
    case CNetScheduleAPI::ePending:
        buf += string(" \"\" \"\" \"") + input + "\"";
        break;

    case CNetScheduleAPI::eFailed:
        buf += string(" \"") + output + "\" \"" + error + "\" \"" + input + "\"";
        break;
    default:
        break;
    }

    WriteMsg("OK:", buf);
}


void CNetScheduleHandler::ProcessGetMessage()
{
    unsigned job_id = CNetScheduleKey(m_JobReq.job_key).id;
    string progress_msg;

    if (m_Queue->GetJobDescr(job_id, 0, 0, 0, 0,
                             &progress_msg)) {
        WriteMsg("OK:", progress_msg);
    } else {
        WriteMsg("ERR:", "Job not found");
    }
}


void CNetScheduleHandler::ProcessPutMessage()
{
    unsigned job_id = CNetScheduleKey(m_JobReq.job_key).id;

    m_Queue->PutProgressMessage(job_id, m_JobReq.progress_msg);
    WriteMsg("OK:");
}


void CNetScheduleHandler::ProcessGet()
{
    unsigned job_id;
    char key_buf[1024];
    string aff_token;
    list<string> aff_list;
    NStr::Split(m_JobReq.affinity_token, "\t,",
                aff_list, NStr::eNoMergeDelims);
    m_Queue->GetJob(m_PeerAddr,
                    &m_AuthString,
                    &aff_list,
                    &job_id,
                    &m_JobReq.input,
                    &m_JobReq.job_mask,
                    &aff_token);

    m_Queue->GetJobKey(key_buf, job_id,
        m_Server->GetHost(), m_Server->GetPort());

    if (job_id) {
        x_MakeGetAnswer(key_buf, m_JobReq.job_mask, aff_token);
        WriteMsg("OK:", m_Answer);
    } else {
        WriteMsg("OK:");
    }

    if (job_id && IsMonitoring()) {
        CSocket& socket = GetSocket();
        string msg = "::ProcessGet ";
        msg += m_LocalTimer.GetLocalTime().AsString();
        msg += m_Answer;
        msg += " ==> ";
        msg += socket.GetPeerAddress();
        msg += " ";
        msg += m_AuthString;

        MonitorPost(msg);
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
    string aff_token;
    list<string> aff_list;
    NStr::Split(m_JobReq.affinity_token, "\t,",
                aff_list, NStr::eNoMergeDelims);

    unsigned done_job_id;
    if (!m_JobReq.job_key.empty()) {
        done_job_id = CNetScheduleKey(m_JobReq.job_key).id;
    } else {
        done_job_id = 0;
    }
    m_Queue->PutResultGetJob(done_job_id,
                             m_JobReq.job_return_code,
                             &m_JobReq.output,
                             // GetJob params
                             m_PeerAddr,
                             &m_AuthString,
                             &aff_list,
                             &job_id,
                             &m_JobReq.input,
                             &m_JobReq.job_mask,
                             &aff_token);

    m_Queue->GetJobKey(key_buf, job_id,
        m_Server->GetHost(), m_Server->GetPort());

    if (job_id) {
        x_MakeGetAnswer(key_buf, m_JobReq.job_mask, aff_token);
        WriteMsg("OK:", m_Answer);
    } else {
        WriteMsg("OK:");
    }

    if (IsMonitoring()) {
        CSocket& socket = GetSocket();
        string msg = "::ProcessJobExchange ";
        msg += m_LocalTimer.GetLocalTime().AsString();
        msg += " ";
        msg += m_Answer;
        msg += " ==> ";
        msg += socket.GetPeerAddress();
        msg += " ";
        msg += m_AuthString;

        MonitorPost(msg);
    }
}


void CNetScheduleHandler::ProcessWaitGet()
{
    unsigned job_id;
    string aff_token;
    list<string> aff_list;
    NStr::Split(m_JobReq.affinity_token, "\t,",
                aff_list, NStr::eNoMergeDelims);
    m_Queue->GetJob(m_PeerAddr,
                    &m_AuthString,
                    &aff_list,
                    &job_id, 
                    &m_JobReq.input,
                    &m_JobReq.job_mask,
                    &aff_token);

    if (job_id) {
        char key_buf[1024];
        m_Queue->GetJobKey(key_buf, job_id,
            m_Server->GetHost(), m_Server->GetPort());
        x_MakeGetAnswer(key_buf, m_JobReq.job_mask, aff_token);
        WriteMsg("OK:", m_Answer);
        return;
    }

    // job not found, initiate waiting mode

    WriteMsg("OK:");

    m_Queue->RegisterNotificationListener(
        m_PeerAddr, m_JobReq.port, m_JobReq.timeout,
        m_AuthString);
}


void CNetScheduleHandler::ProcessRegisterClient()
{
    m_Queue->RegisterNotificationListener(
        m_PeerAddr, m_JobReq.port, 1, m_AuthString);

    WriteMsg("OK:");
}


void CNetScheduleHandler::ProcessUnRegisterClient()
{
    m_Queue->UnRegisterNotificationListener(m_PeerAddr,
                                            m_JobReq.port);
    m_Queue->ClearAffinity(m_PeerAddr, m_AuthString);

    WriteMsg("OK:");
}


void CNetScheduleHandler::ProcessQList()
{
    string qnames;
    m_Server->GetQueueDB()->GetQueueNames(&qnames, ";");
    WriteMsg("OK:", qnames);
}


void CNetScheduleHandler::ProcessError() {
    WriteMsg("ERR:", m_JobReq.err_msg);
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
    unsigned job_id = CNetScheduleKey(m_JobReq.job_key).id;
    m_Queue->JobFailed(job_id, m_JobReq.err_msg, m_JobReq.output,
                       m_JobReq.job_return_code,
                       m_PeerAddr, m_AuthString);
    WriteMsg("OK:");
}


void CNetScheduleHandler::ProcessPut()
{
    unsigned job_id = CNetScheduleKey(m_JobReq.job_key).id;
    m_Queue->PutResult(job_id, m_JobReq.job_return_code, &m_JobReq.output);
    WriteMsg("OK:");
}


void CNetScheduleHandler::ProcessReturn()
{
    unsigned job_id = CNetScheduleKey(m_JobReq.job_key).id;
    m_Queue->ReturnJob(job_id);
    WriteMsg("OK:");
}


void CNetScheduleHandler::ProcessDropQueue()
{
    m_Queue->Truncate();
    WriteMsg("OK:");
}


void CNetScheduleHandler::ProcessMonitor()
{
    CSocket& socket = GetSocket();
    socket.DisableOSSendDelay(false);
    WriteMsg("OK:", NETSCHEDULED_FULL_VERSION);
    string started = "Started: " + m_Server->GetStartTime().AsString();
    WriteMsg("OK:", started);
    // Socket is released from regular scheduling here - it is
    // write only for monitor since this moment.
    m_Queue->SetMonitorSocket(socket);
}


void CNetScheduleHandler::ProcessPrintQueue()
{
    // TODO this method can not support session, because socket is closed at
    // the end.
    CNetScheduleAPI::EJobStatus 
        job_status = CNetScheduleAPI::StringToStatus(m_JobReq.param1);

    if (job_status == CNetScheduleAPI::eJobNotFound) {
        string err_msg = "Status unknown: " + m_JobReq.param1;
        WriteMsg("ERR:", err_msg);
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

    ios << "OK:" << NETSCHEDULED_FULL_VERSION << endl;

    if (m_JobReq.job_key.empty()) {
        ios << "OK:" << "[Job status matrix]:";

        m_Queue->PrintJobStatusMatrix(ios);

        ios << "OK:[Job DB]:" << endl;
        m_Queue->PrintAllJobDbStat(ios);
    } else {
        try {
            unsigned job_id = CNetScheduleKey(m_JobReq.job_key).id;

            CNetScheduleAPI::EJobStatus status = m_Queue->GetStatus(job_id);
            
            string st_str = CNetScheduleAPI::StatusToString(status);
            ios << "OK:" << "[Job status matrix]:" << st_str;
            ios << "OK:[Job DB]:" << endl;
            m_Queue->PrintJobDbStat(job_id, ios);
        } 
        catch (CException&)
        {
            // dump by status
            CNetScheduleAPI::EJobStatus 
                job_status = CNetScheduleAPI::StringToStatus(
                    m_JobReq.job_key);

            if (job_status == CNetScheduleAPI::eJobNotFound) {
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
    WriteMsg("OK:");
}


void CNetScheduleHandler::ProcessStatistics()
{
    CSocket& socket = GetSocket();
    socket.DisableOSSendDelay(false);

    WriteMsg("OK:", NETSCHEDULED_FULL_VERSION);
    string started = "Started: " + m_Server->GetStartTime().AsString();
    WriteMsg("OK:", started);

    string load_str = "Load: jobs dispatched: ";
    load_str +=
        NStr::DoubleToString(m_Queue->GetAverage(SLockedQueue::eStatGetEvent));
    load_str += "/sec, jobs complete: ";
    load_str +=
        NStr::DoubleToString(m_Queue->GetAverage(SLockedQueue::eStatPutEvent));
    load_str += "/sec, DB locks: ";
    load_str +=
        NStr::DoubleToString(m_Queue->GetAverage(SLockedQueue::eStatDBLockEvent));
    load_str += "/sec, Job DB writes: ";
    load_str +=
        NStr::DoubleToString(m_Queue->GetAverage(SLockedQueue::eStatDBWriteEvent));
    load_str += "/sec";
    WriteMsg("OK:", load_str);

    for (int i = CNetScheduleAPI::ePending; 
         i < CNetScheduleAPI::eLastStatus; ++i) {
        CNetScheduleAPI::EJobStatus st = 
                        (CNetScheduleAPI::EJobStatus) i;
        unsigned count = m_Queue->CountStatus(st);


        string st_str = CNetScheduleAPI::StatusToString(st);
        st_str += ": ";
        st_str += NStr::UIntToString(count);

        WriteMsg("OK:", st_str, true);

        if (m_JobReq.param1 == "ALL") {
            TNSBitVector::statistics bv_stat;
            m_Queue->StatusStatistics(st, &bv_stat);
            st_str = "   bit_blk="; 
            st_str.append(NStr::UIntToString(bv_stat.bit_blocks));
            st_str += "; gap_blk=";
            st_str.append(NStr::UIntToString(bv_stat.gap_blocks));
            st_str += "; mem_used=";
            st_str.append(NStr::UIntToString(bv_stat.memory_used));
            WriteMsg("OK:", st_str);
        }
    } // for

    /*
    if (m_JobReq.param1 == "ALL") {

        unsigned db_recs = m_Queue->CountRecs();
        string recs = "Records:";
        recs += NStr::UIntToString(db_recs);
        WriteMsg("OK:", recs);
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
    */

    if (m_JobReq.param1 == "ALL") {
        WriteMsg("OK:", "[Berkeley DB Mutexes]:");
        {{
            CNcbiOstrstream ostr;
            m_Queue->GetBDBEnv().PrintMutexStat(ostr);
            ostr << ends;

            char* stat_str = ostr.str();
            try {
                WriteMsg("OK:", stat_str, true);
            } catch (...) {
                ostr.freeze(false);
                throw;
            }
            ostr.freeze(false);

        }}

        WriteMsg("OK:", "[Berkeley DB Locks]:");
        {{
            CNcbiOstrstream ostr;
            m_Queue->GetBDBEnv().PrintLockStat(ostr);
            ostr << ends;

            char* stat_str = ostr.str();
            try {
                WriteMsg("OK:", stat_str, true);
            } catch (...) {
                ostr.freeze(false);
                throw;
            }
            ostr.freeze(false);

        }}

        WriteMsg("OK:", "[Berkeley DB Memory Usage]:");
        {{
            CNcbiOstrstream ostr;
            m_Queue->GetBDBEnv().PrintMemStat(ostr);
            ostr << ends;

            char* stat_str = ostr.str();
            try {
                WriteMsg("OK:", stat_str, true);
            } catch (...) {
                ostr.freeze(false);
                throw;
            }
            ostr.freeze(false);

        }}

        WriteMsg("OK:", "[BitVector block pool]:");

        {{
            const TBlockAlloc::TBucketPool::TBucketVector& bv =
                TBlockAlloc::GetPoolVector();
            size_t pool_vec_size = bv.size();
            string tmp_str = "Pool vector size: ";
            tmp_str.append(NStr::UIntToString(pool_vec_size));
            WriteMsg("OK:", tmp_str);  
            for (size_t i = 0; i < pool_vec_size; ++i) {
                const TBlockAlloc::TBucketPool::TResourcePool* rp =
                    TBlockAlloc::GetPool(i);
                if (rp) {
                    size_t pool_size = rp->GetSize();
                    if (pool_size) {
                        tmp_str = "Pool [ ";
                        tmp_str.append(NStr::UIntToString(i));
                        tmp_str.append("] = ");
                        tmp_str.append(NStr::UIntToString(pool_size));

                        WriteMsg("OK:", tmp_str);  
                    }
                }
            }
        }}
    }

    WriteMsg("OK:", "[Worker node statistics]:");

    {{
        CNcbiOstrstream ostr;
        m_Queue->PrintNodeStat(ostr);
        ostr << ends;

        char* stat_str = ostr.str();
        try {
            WriteMsg("OK:", stat_str, true);
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
            WriteMsg("OK:", stat_str, true);
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
            WriteMsg("OK:", stat_str, true);
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
        WriteMsg("ERR:", "eProtocolSyntaxError:Incorrect LOG syntax");
    }
    WriteMsg("OK:");
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
    WriteMsg("OK:");
}


void CNetScheduleHandler::ProcessVersion()
{
    WriteMsg("OK:", NETSCHEDULED_FULL_VERSION);
}

void CNetScheduleHandler::ProcessCreateQueue()
{
    const string& qname  = m_JobReq.param1;
    const string& qclass = m_JobReq.param2;
    const string& comment = NStr::ParseEscapes(m_JobReq.param3);
    m_Server->GetQueueDB()->CreateQueue(qname, qclass, comment);
    WriteMsg("OK:");
}

void CNetScheduleHandler::ProcessDeleteQueue()
{
    const string& qname = m_JobReq.param1;
    m_Server->GetQueueDB()->DeleteQueue(qname);
    WriteMsg("OK:");
}


void CNetScheduleHandler::ProcessQueueInfo()
{
    const string& qname = m_JobReq.param1;
    int kind;
    string qclass;
    string comment;
    m_Server->GetQueueDB()->QueueInfo(qname, kind, &qclass, &comment);
    WriteMsg("OK:", NStr::IntToString(kind) + "\t" + qclass + "\t\"" +
                    NStr::PrintableString(comment) + "\"");
}


void CNetScheduleHandler::ProcessQuery()
{
    string result_str;

    string query = NStr::ParseEscapes(m_JobReq.param1);

    list<string> fields;
    m_SelectedIds.reset(m_Queue->ExecSelect(query, fields));

    string& action = m_JobReq.param2;

    if (!fields.empty()) {
        WriteMsg("ERR:", string("eProtocolSyntaxError:") +
                 "SELECT is not allowed");
        m_SelectedIds.release();
        return;
    } else if (action == "SLCT") {
        // Execute 'projection' phase
        string str_fields(NStr::ParseEscapes(m_JobReq.param3));
        // Split fields
        NStr::Split(str_fields, "\t,", fields, NStr::eNoMergeDelims);
        m_Queue->PrepareFields(m_FieldDescr, fields);
        m_DelayedOutput = &CNetScheduleHandler::WriteProjection;
        return;
    } else if (action == "COUNT") {
        result_str = NStr::IntToString(m_SelectedIds->count());
    } else if (action == "IDS") {
        TNSBitVector::statistics st;
        m_SelectedIds->optimize(0, TNSBitVector::opt_compress, &st);
        size_t dst_size = st.max_serialize_mem * 4 / 3 + 1;
        size_t src_size = st.max_serialize_mem;
        unsigned char* buf = new unsigned char[src_size + dst_size];
        size_t size = bm::serialize(*m_SelectedIds, buf);
        size_t src_read, dst_written, line_len;
        line_len = 0; // no line feeds in encoded value
        unsigned char* encode_buf = buf+src_size;
        BASE64_Encode(buf, size, &src_read, encode_buf, dst_size, &dst_written, &line_len);
        result_str = string((const char*) encode_buf, dst_written);
        delete [] buf;
    } else if (action == "DROP") {
    } else if (action == "FRES") {
    } else if (action == "CNCL") {
    } else {
        WriteMsg("ERR:", string("eProtocolSyntaxError:") +
                 "Unknown action " + action);
        m_SelectedIds.release();
        return;
    }
    WriteMsg("OK:", result_str);
    m_SelectedIds.release();
}


void CNetScheduleHandler::ProcessSelectQuery()
{
    string result_str;

    string query = NStr::ParseEscapes(m_JobReq.param1);

    list<string> fields;
    m_SelectedIds.reset(m_Queue->ExecSelect(query, fields));

    if (fields.empty()) {
        WriteMsg("ERR:", string("eProtocolSyntaxError:") +
                 "SQL-like select is expected");
        m_SelectedIds.release();
    } else {
        m_Queue->PrepareFields(m_FieldDescr, fields);
        m_DelayedOutput = &CNetScheduleHandler::WriteProjection;
    }
}


/*
static bool Less(const CQueue::TRecord& elem1, const CQueue::TRecord& elem2)
{
    int size = min(elem1.size(), elem2.size());
    for (int i = 0; i < size; ++i) {
        // try to convert both elements to integer
        const string& p1 = elem1[i];
        const string& p2 = elem2[i];
        try {
            int i1 = NStr::StringToInt(p1);
            int i2 = NStr::StringToInt(p2);
            if (i1 < i2) return true;
            if (i1 > i2) return false;
        } catch (CStringException&) {
            if (p1 < p2) return true;
            if (p1 > p2) return false;
        }
    }
    if (elem1.size() < elem2.size()) return true;
    return false;
}
*/


void CNetScheduleHandler::WriteProjection()
{
    unsigned chunk_size = 10000;

    unsigned n;
    TNSBitVector chunk;
    unsigned job_id = 0;
    for (n = 0; n < chunk_size &&
                (job_id = m_SelectedIds->extract_next(job_id));
         ++n)
    {
        chunk.set(job_id);
    }
    if (n > 0) {
        SQueueDescription qdesc;
        qdesc.host = m_Server->GetHost().c_str();
        qdesc.port = m_Server->GetPort();
        CQueue::TRecordSet record_set;
        m_Queue->ExecProject(record_set, chunk, m_FieldDescr);
        /*
        sort(record_set.begin(), record_set.end(), Less);

        list<string> string_set;
        ITERATE(TRecordSet, it, record_set) {
            string_set.push_back(NStr::Join(*it, "\t"));
        }
        list<string> out_set;
        unique_copy(string_set.begin(), string_set.end(),
            back_insert_iterator<list<string> > (out_set));
        out_set.push_front(NStr::IntToString(out_set.size()));
        result_str = NStr::Join(out_set, "\n");
        */
        ITERATE(CQueue::TRecordSet, it, record_set) {
            const CQueue::TRecord& rec = *it;
            string str_rec;
            for (unsigned i = 0; i < rec.size(); ++i) {
                if (i) str_rec += '\t';
                const string& field = rec[i];
                if (m_FieldDescr.formatters[i])
                    str_rec += m_FieldDescr.formatters[i](field, &qdesc);
                else
                    str_rec += field;
            }
            WriteMsg("OK:", str_rec);
        }
    }
    if (!m_SelectedIds->any()) {
        WriteMsg("OK:", "END");
        m_SelectedIds.release();
        m_DelayedOutput = NULL;
    }
}


void CNetScheduleHandler::ProcessGetParam()
{
    string res("max_input_size=");
    res += NStr::IntToString(m_Queue->GetMaxInputSize());
    res += ";max_output_size=";
    res += NStr::IntToString(m_Queue->GetMaxOutputSize());
    res += ";";
    res += NETSCHEDULED_FEATURES;
    WriteMsg("OK:", res);
}


//////////////////////////////////////////////////////////////////////////

/// NetSchedule command parser
///
/// @internal
///

CNetScheduleHandler::SArgument CNetScheduleHandler::sm_End = { eNSA_None };
#define NO_ARGS {sm_End}
CNetScheduleHandler::SCommandMap CNetScheduleHandler::sm_CommandMap[] = {
    // SST job_key : id -- submitter fast status, changes timestamp
    { "SST",      &CNetScheduleHandler::ProcessFastStatusS, eNSCR_Submitter,
        { { eNSA_Required, eNST_Id, eNSRF_JobKey }, sm_End } },
    // WST job_key : id -- worker node fast status, does not change timestamp
    { "WST",      &CNetScheduleHandler::ProcessFastStatusW, eNSCR_Worker,
        { { eNSA_Required, eNST_Id, eNSRF_JobKey }, sm_End } },
    // SUBMIT input : str [ progress_msg : str ] [ port : uint [ timeout : uint ]]
    //        [ affinity_token : keystr(aff) ] [ job_mask : keyint(msk) ]
    //        [ tags : keystr(tags) ]
    { "SUBMIT",   &CNetScheduleHandler::ProcessSubmit, eNSCR_Submitter,
        { { eNSA_Required, eNST_Str,     eNSRF_Input },
          { eNSA_Optional, eNST_Str,     eNSRF_ProgressMsg },
          { eNSA_Optional, eNST_Int,     eNSRF_Port },
          { eNSA_Optional, eNST_Int,     eNSRF_Timeout },
          { eNSA_Optional, eNST_KeyStr,  eNSRF_AffinityToken, "", "aff" },
          { eNSA_Optional, eNST_KeyInt,  eNSRF_JobMask, "0", "msk" },
          { eNSA_Optional, eNST_KeyStr,  eNSRF_Tags, "", "tags" }, sm_End } },
    // CANCEL job_key : id
    { "CANCEL",   &CNetScheduleHandler::ProcessCancel, eNSCR_Submitter, 
        { { eNSA_Required, eNST_Id, eNSRF_JobKey }, sm_End } },
    // STATUS job_key : id
    { "STATUS",   &CNetScheduleHandler::ProcessStatus, eNSCR_Queue,
        { { eNSA_Required, eNST_Id, eNSRF_JobKey }, sm_End } },
    // GET [ port : int ] [affinity_list : keystr(aff) ]
    { "GET",      &CNetScheduleHandler::ProcessGet, eNSCR_Worker,
        { { eNSA_Optional, eNST_Int, eNSRF_Port },
          { eNSA_Optional, eNST_KeyStr, eNSRF_AffinityToken, "", "aff" },
          sm_End } },
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
    //      [affinity_list : keystr(aff) ]
    { "WGET",     &CNetScheduleHandler::ProcessWaitGet, eNSCR_Worker,
        { { eNSA_Required, eNST_Int, eNSRF_Port },
          { eNSA_Required, eNST_Int, eNSRF_Timeout },
          { eNSA_Optional, eNST_KeyStr, eNSRF_AffinityToken, "", "aff" },
          sm_End} },
    // JRTO job_key : id  timeout : uint
    { "JRTO",     &CNetScheduleHandler::ProcessJobRunTimeout, eNSCR_Worker,
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
    //      [affinity_list : keystr(aff) ]
    { "JXCG",     &CNetScheduleHandler::ProcessJobExchange, eNSCR_Worker,
        { { eNSA_Optchain, eNST_Id,  eNSRF_JobKey },
          { eNSA_Optchain, eNST_Int, eNSRF_JobReturnCode },
          { eNSA_Optchain, eNST_Str, eNSRF_Output },
          { eNSA_Optional, eNST_KeyStr, eNSRF_AffinityToken, "", "aff" },
          sm_End } },
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
    // QCRE qname : id  qclass : id [ comment : str ]
    { "QCRE",     &CNetScheduleHandler::ProcessCreateQueue, eNSAC_DynClassAdmin,
        { { eNSA_Required, eNST_Id, eNSRF_QueueName },
          { eNSA_Required, eNST_Id, eNSRF_QueueClass },
          { eNSA_Optional, eNST_Str, eNSRF_QueueComment }, sm_End} },
    // QDEL qname : id
    { "QDEL",     &CNetScheduleHandler::ProcessDeleteQueue, eNSAC_DynQueueAdmin,
        { { eNSA_Required, eNST_Id, eNSRF_QueueName }, sm_End } },
    { "QINF",     &CNetScheduleHandler::ProcessQueueInfo, eNSCR_Any,
        { { eNSA_Required, eNST_Id, eNSRF_QueueName }, sm_End } },
    { "QERY",     &CNetScheduleHandler::ProcessQuery, eNSCR_Queue,
        { { eNSA_Required, eNST_Str,     eNSRF_Where },
          { eNSA_Optional, eNST_Id,      eNSRF_Action, "COUNT" },
          { eNSA_Optional, eNST_Str,     eNSRF_Fields }, sm_End} },
    { "QSEL",     &CNetScheduleHandler::ProcessSelectQuery, eNSCR_Queue,
        { { eNSA_Required, eNST_Str,     eNSRF_Select }, sm_End} },
    { "GETP",     &CNetScheduleHandler::ProcessGetParam, eNSCR_Queue,
        NO_ARGS },
    { 0,          &CNetScheduleHandler::ProcessError },
};


CNetScheduleHandler::ENSTokenType
CNetScheduleHandler::x_GetToken(const char*& s,
                                const char* end,
                                const char*& tok,
                                int &size)
{
    // Skip space
    while (s < end && (*s == ' ' || *s == '\t')) ++s;
    if (!(s < end)) return eNST_None;
    ENSTokenType ttype = eNST_None;
    bool has_digit = false;
    if (*s == '"') {
        s = tok = s + 1;
        ttype = eNST_Str;
    } else {
        tok = s;
        if ((has_digit = isdigit(*s)) || (*s == '-' && s++))
	    ttype = eNST_Int;
        else
	    ttype = eNST_Id;
    }
    for ( ; (s < end) && ((ttype == eNST_Str || ttype == eNST_KeyStr) ?
                        !(*s == '"' && *(s-1) != '\\') :
                        !(*s == ' ' || *s == '\t'));
            ++s) {
        switch (ttype) {
        case eNST_Int:
            if (!isdigit(*s)) ttype = eNST_Id;
	    else has_digit = true;
            break;
        case eNST_KeyNone:
            if ((has_digit = isdigit(*s)) || (*s == '-' && s++))
	        ttype = eNST_KeyInt;
            else if (*s == '"')
	        ttype = eNST_KeyStr;
            else
	        ttype = eNST_KeyId;
            break;
        case eNST_KeyInt:
            if (!isdigit(*s)) ttype = eNST_KeyId;
	    else has_digit = true;
            break;
        case eNST_Id:
            if (*s == '=') ttype = eNST_KeyNone;
            break;
        case eNST_Str:
            if (unsigned(s-tok) > kNetScheduleMaxOverflowSize-1) {
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
    if (ttype == eNST_Int && !has_digit) ttype = eNST_Id;
    if (ttype == eNST_KeyInt && !has_digit) ttype = eNST_KeyId;
    return ttype;
}


// Compare actual token type with argument type. 
// If the argument is of key type, compare the key in the token with
// the key in argument description as well.
bool CNetScheduleHandler::x_TokenMatch(const SArgument *arg_descr,
                                       ENSTokenType ttype,
                                       const char* token,
                                       int tsize)
{
    if (arg_descr->ttype != ttype) return false;
    if (arg_descr->atype & fNSA_Match)
        return strncmp(arg_descr->key, token, tsize) == 0;
    if (arg_descr->ttype != eNST_KeyNone && arg_descr->ttype != eNST_KeyId &&
        arg_descr->ttype != eNST_KeyInt && arg_descr->ttype != eNST_KeyStr)
        return true;
    const char* eq_pos = strchr(token, '=');
    if (!eq_pos || (eq_pos-token) > tsize) return false;
    return strncmp(arg_descr->key, token, eq_pos - token) == 0;
}


bool CNetScheduleHandler::x_GetValue(ENSTokenType ttype,
                                     const char* token, int tsize,
                                     const char*&val, int& vsize)
{
    if (ttype != eNST_KeyNone && ttype != eNST_KeyId &&
        ttype != eNST_KeyInt && ttype != eNST_KeyStr) {
        val = token;
        vsize = tsize;
        return true;
    }
    const char* eq_pos = strchr(token, '=');
    if (!eq_pos || (eq_pos-token) > tsize) {
        return false;
    }
    if (ttype == eNST_KeyStr) ++eq_pos;
    val = eq_pos + 1;
    vsize = tsize - (val-token);
    if (ttype == eNST_KeyStr) --vsize;
    return true;
}


CNetScheduleHandler::FProcessor CNetScheduleHandler::ParseRequest(
    const string& reqstr)
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
    const char* s = reqstr.data();
    const char* end = s + reqstr.size();
    const char* token;
    int tsize;
    ENSTokenType ttype;

    ttype = x_GetToken(s, end, token, tsize);
    if (ttype != eNST_Id) {
        m_JobReq.err_msg = "eProtocolSyntaxError:Command absent";
        return &CNetScheduleHandler::ProcessError;
    }
    const SArgument *argsDescr = 0;
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
        m_JobReq.err_msg = "eProtocolSyntaxError:Unknown request";
        return &CNetScheduleHandler::ProcessError;
    }
    x_CheckAccess(sm_CommandMap[n_cmd].role);

    if (!x_ParseArguments(s, end, argsDescr)) {
        m_JobReq.err_msg = "eProtocolSyntaxError:Malformed ";
        m_JobReq.err_msg.append(sm_CommandMap[n_cmd].cmd);
        m_JobReq.err_msg.append(" request");
        return &CNetScheduleHandler::ProcessError;
    }

    return processor;
}


void CNetScheduleHandler::WriteMsg(const char*   prefix,
                                   const string& msg,
                                   bool          comm_control)
{
    CSocket& socket = GetSocket();
    size_t msg_length = 0;
    const char* buf_ptr;
    string buffer;
    size_t prefix_size = strlen(prefix);

    if (prefix_size + msg.size() + 2 > kMaxMessageSize) {
        buffer = prefix;
        buffer += msg;
        buffer += "\r\n";
        msg_length = buffer.size();
        buf_ptr = buffer.data();
    } else {
        char* ptr = m_MsgBuffer;
        strcpy(ptr, prefix);
        ptr += prefix_size;
        size_t msg_size = msg.size();
        memcpy(ptr, msg.data(), msg_size);
        ptr += msg_size;
        *ptr++ = '\r';
        *ptr++ = '\n';
        msg_length = ptr - m_MsgBuffer;
        buf_ptr = m_MsgBuffer;
    }

    if (m_Server->IsLog()) {
        string lmsg;
        lmsg = string("ANS:") + NStr::IntToString(m_CommandNumber);
        lmsg += ';';
        lmsg += m_AuthString;
        lmsg += ';';
        lmsg += m_QueueName;
        lmsg += ';';
        size_t log_length = min(msg_length, (size_t) 1024);
        bool shortened = log_length < msg_length;
        while (log_length > 0 && (buf_ptr[log_length-1] == '\n' ||
                                  buf_ptr[log_length-1] == '\r')) {
                log_length--;
        }
        lmsg += string(buf_ptr, log_length);
        if (shortened) lmsg += "...";
        NCBI_NS_NCBI::CNcbiDiag(eDiag_Info, eDPF_Log).GetRef()
            << lmsg
            << NCBI_NS_NCBI::Endm;
    }

    size_t n_written;
    EIO_Status io_st = 
        socket.Write(buf_ptr, msg_length, &n_written);
    if (comm_control && io_st) {
        NCBI_THROW(CNetServiceException, 
                   eCommunicationError, "Socket write error.");
    }
}


void CNetScheduleHandler::x_MakeLogMessage(string& lmsg)
{
    CSocket& socket = GetSocket();
    string peer = socket.GetPeerAddress();
    // get only host name
    string::size_type offs = peer.find_first_of(":");
    if (offs != string::npos) {
        peer.erase(offs, peer.length());
    }

    lmsg = string("REQ:") + NStr::IntToString(m_CommandNumber) + ';' + peer + ';';
    lmsg += m_AuthString;
    lmsg += ';';
    lmsg += m_QueueName;
    lmsg += ';';
    lmsg += m_Request;
}


void CNetScheduleHandler::x_MakeGetAnswer(const char*   key_buf,
                                          unsigned      job_mask,
                                          const string& aff_token)
{
    string& answer = m_Answer;
    answer = key_buf;
    answer.append(" \"");
    answer.append(m_JobReq.input);
    answer.append("\"");

    answer.append(" \"");
    answer.append(aff_token);
    answer.append("\" \"\""); // to keep compat. with jout & jerr

    answer.append(" ");
    answer.append(NStr::UIntToString(job_mask));
}


//////////////////////////////////////////////////////////////////////////
// CNetScheduleServer implementation
CNetScheduleServer::CNetScheduleServer(unsigned int    port,
                                       bool            use_hostname,
                                       unsigned        network_timeout,
                                       bool            is_log)
:   m_Port(port),
    m_Shutdown(false),
    m_SigNum(0),
    m_InactivityTimeout(network_timeout),
    m_QueueDB(0),
    m_StartTime(CTime::eCurrent)
{
    m_AtomicCommandNumber.Set(1);

//    m_QueueDB = qdb;

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


void CNetScheduleServer::Exit()
{
    m_QueueDB->Close();
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
    LOG_POST(NETSCHEDULED_FULL_VERSION);
    
    const CArgs& args = GetArgs();

    const CNcbiRegistry& reg = GetConfig();
    bool is_daemon =
        reg.GetBool("server", "daemon", false, 0, CNcbiRegistry::eReturn);

    try {
#if defined(NCBI_OS_UNIX)
        // attempt to get server gracefully shutdown on signal
        signal( SIGINT, Threaded_Server_SignalHandler);
        signal( SIGTERM, Threaded_Server_SignalHandler);    
#endif
                   
        CConfig conf(reg);

        SNSDBEnvironmentParams bdb_params;
        
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
        const string& db_log_path = ""; // doesn't work yet
            // bdb_conf.GetString("netschedule", "transaction_log_path",
            // CConfig::eErr_NoThrow, "");
        bool reg_reinit =
            reg.GetBool("server", "reinit", false, 0, IRegistry::eReturn);

        if (args["reinit"] || reg_reinit) {  // Drop the database directory
            CDir dir(db_path);
            dir.Remove();
            LOG_POST(Info << "Reinintialization. " << db_path 
                          << " removed. \n");
        }

        bdb_params.cache_ram_size = (unsigned)
            bdb_conf.GetDataSize("netschedule", "mem_size", 
                                 CConfig::eErr_NoThrow, 0);
        bdb_params.mutex_max = (unsigned)
            bdb_conf.GetInt("netschedule", "mutex_max", 
                            CConfig::eErr_NoThrow, 0);
        bdb_params.max_locks = (unsigned)
            bdb_conf.GetInt("netschedule", "max_locks", 
                            CConfig::eErr_NoThrow, 0);
        bdb_params.max_lockers = (unsigned)
            bdb_conf.GetInt("netschedule", "max_lockers", 
                            CConfig::eErr_NoThrow, 0);
        bdb_params.max_lockobjects = (unsigned)
            bdb_conf.GetInt("netschedule", "max_lockobjects", 
                            CConfig::eErr_NoThrow, 0);

        bdb_params.log_mem_size = (unsigned)
            bdb_conf.GetDataSize("netschedule", "log_mem_size", 
                                 CConfig::eErr_NoThrow, 0);

        bdb_params.checkpoint_kb = (unsigned)
            bdb_conf.GetInt("netschedule", "checkpoint_kb", 
                            CConfig::eErr_NoThrow, 5000);
        bdb_params.checkpoint_min = (unsigned)
            bdb_conf.GetInt("netschedule", "checkpoint_min", 
                            CConfig::eErr_NoThrow, 5);

        bdb_params.sync_transactions =
            bdb_conf.GetBool("netschedule", "sync_transactions",
                             CConfig::eErr_NoThrow, false);

        bdb_params.direct_db =
            bdb_conf.GetBool("netschedule", "direct_db",
                             CConfig::eErr_NoThrow, false);
        bdb_params.direct_log =
            bdb_conf.GetBool("netschedule", "direct_log",
                             CConfig::eErr_NoThrow, false);
        bdb_params.private_env =
            bdb_conf.GetBool("netschedule", "private_env",
                             CConfig::eErr_NoThrow, false);

        SServer_Parameters params;

        params.max_connections =
            reg.GetInt("server", "max_connections", 100, 0, CNcbiRegistry::eReturn);

        params.max_threads =
            reg.GetInt("server", "max_threads", 25, 0, CNcbiRegistry::eReturn);

        params.init_threads =
            reg.GetInt("server", "init_threads", 10, 0, CNcbiRegistry::eReturn);
        if (params.init_threads > params.max_threads)
            params.init_threads = params.max_threads;

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

        m_ServerAcceptTimeout.sec = 1;
        m_ServerAcceptTimeout.usec = 0;
        params.accept_timeout  = &m_ServerAcceptTimeout;

        int port = 
            reg.GetInt("server", "port", 9100, 0, CNcbiRegistry::eReturn);

        auto_ptr<CNetScheduleServer> server(
            new CNetScheduleServer(port,
                                   use_hostname,
                                   network_timeout,
                                   is_log));

        string admin_hosts =
            reg.GetString("server", "admin_host", kEmptyStr);
        if (!admin_hosts.empty())
            server->SetAdminHosts(admin_hosts);

        server->SetParameters(params);

        server->AddListener(
            new CNetScheduleConnectionFactory(&*server),
            port);

        server->StartListening();

        NcbiCout << "Server listening on port " << port << NcbiEndl;
        LOG_POST(Info << "Server listening on port " << port);

        // two transactions per thread should be enough
        bdb_params.max_trans = params.max_threads * 2;

        auto_ptr<CQueueDataBase> qdb(new CQueueDataBase());

        NcbiCout << "Mounting database at " << db_path << NcbiEndl;
        LOG_POST(Info << "Mounting database at " << db_path);

        qdb->Open(db_path, db_log_path, bdb_params);

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
            LOG_POST(Info << "UDP notification disabled.");
        }
        if (udp_port > 0) {
            qdb->SetUdpPort((unsigned short) udp_port);
        }

#if defined(NCBI_OS_UNIX)
        if (is_daemon) {
            LOG_POST("Entering UNIX daemon mode...");
            bool daemon = Daemonize(0, fDaemon_DontChroot);
            if (!daemon) {
                return 0;
            }

        }

#else
        is_daemon = false;
#endif

        // Scan and mount queues
        unsigned min_run_timeout = 3600;
        qdb->Configure(reg, &min_run_timeout);

        LOG_POST(Info << "Running execution control every " 
                      << min_run_timeout << " seconds");
        min_run_timeout = min_run_timeout >= 0 ? min_run_timeout : 2;
        
        
        qdb->RunExecutionWatcherThread(min_run_timeout);
        qdb->RunPurgeThread();
        qdb->RunNotifThread();

        server->SetQueueDB(qdb.release());

        if (!is_daemon) {
            NcbiCout << "Server started" << NcbiEndl;
        }
        LOG_POST(Info << "Server started");

        server->Run();

        if (!is_daemon) {
            NcbiCout << "Server stopped" << NcbiEndl;
        }
        LOG_POST("Server stopped");

    }
    catch (CBDB_ErrnoException& ex)
    {
        LOG_POST(Error << "Error: DBD errno exception:" << ex.what());
        return ex.BDB_GetErrno();
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
     GetDiagContext().SetOldPostFormat(false);
     return CNetScheduleDApp().AppMain(argc, argv, NULL, eDS_ToStdlog);
}
