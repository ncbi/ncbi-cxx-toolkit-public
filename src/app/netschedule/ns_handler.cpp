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
 * File Description: netschedule commands handler
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/request_ctx.hpp>

#include "ns_handler.hpp"
#include "ns_server.hpp"
#include "ns_server_misc.hpp"


USING_NCBI_SCOPE;



/// NetSchedule command parser
///
/// @internal
///

CNetScheduleHandler::SCommandMap CNetScheduleHandler::sm_BatchHeaderMap[] = {
    { "BTCH", { &CNetScheduleHandler::ProcessBatchStart, 0 },
        { { "size", eNSPT_Int, eNSPA_Required } } },
    { "ENDS", { &CNetScheduleHandler::ProcessBatchSequenceEnd, 0 } },
    { NULL }
};

CNetScheduleHandler::SCommandMap CNetScheduleHandler::sm_BatchEndMap[] = {
    { "ENDB" },
    { NULL }
};

SNSProtoArgument CNetScheduleHandler::sm_BatchArgs[] = {
    { "input", eNSPT_Str, eNSPA_Required },
    { "affp",  eNSPT_Id,  eNSPA_Optional | fNSPA_Or | fNSPA_Match, "" },
    { "aff",   eNSPT_Str, eNSPA_Optional, "" },
    { "msk",   eNSPT_Int, eNSPA_Optional, "0" },
    { "tags",  eNSPT_Str, eNSPA_Optional, "" },
    { NULL }
};

CNetScheduleHandler::SCommandMap CNetScheduleHandler::sm_CommandMap[] = {

    /*** Admin role ***/
    { "SHUTDOWN", { &CNetScheduleHandler::ProcessShutdown,           eNSCR_Admin } },

    /*** Any role ***/
    { "VERSION",  { &CNetScheduleHandler::ProcessVersion,            eNSCR_Any } },
    // LOG option : id -- "ON" or "OFF"
    { "LOG",      { &CNetScheduleHandler::ProcessLog,                eNSCR_Any }, // ?? Admin
        { { "option", eNSPT_Id, eNSPA_Required } } },
    { "QUIT",     { &CNetScheduleHandler::ProcessQuitSession,        eNSCR_Any } },
    { "RECO",     { &CNetScheduleHandler::ProcessReloadConfig,       eNSCR_Any } }, // ?? Admin
    { "ACNT",     { &CNetScheduleHandler::ProcessActiveCount,        eNSCR_Any } },
    { "QLST",     { &CNetScheduleHandler::ProcessQList,              eNSCR_Any } },
    { "QINF",     { &CNetScheduleHandler::ProcessQueueInfo,          eNSCR_Any },
        { { "qname", eNSPT_Id, eNSPA_Required } } },

    /*** QueueAdmin role ***/
    { "DROPQ",    { &CNetScheduleHandler::ProcessDropQueue,          eNSCR_QueueAdmin } },

    /*** DynClassAdmin role ***/
    // QCRE qname : id  qclass : id [ comment : str ]
    { "QCRE",     { &CNetScheduleHandler::ProcessCreateQueue,        eNSAC_DynClassAdmin },
        { { "qname",   eNSPT_Id,  eNSPA_Required },
          { "qclass",  eNSPT_Id,  eNSPA_Required },
          { "comment", eNSPT_Str, eNSPA_Optional } } },
    // QDEL qname : id
    { "QDEL",     { &CNetScheduleHandler::ProcessDeleteQueue,        eNSAC_DynQueueAdmin },
        { { "qname", eNSPT_Id, eNSPA_Required } } },

    /*** Queue role ***/
    // STATUS job_key : id
    { "STATUS",   { &CNetScheduleHandler::ProcessStatus,             eNSCR_Queue },
        { { "job_key", eNSPT_Id, eNSPA_Required } } },
    // STAT [ option : id ] -- "ALL"
    { "STAT",     { &CNetScheduleHandler::ProcessStatistics,         eNSCR_Queue },
        { { "option", eNSPT_Id, eNSPA_Optional } } },
    // MPUT job_key : id  progress_msg : str
    { "MPUT",     { &CNetScheduleHandler::ProcessPutMessage,         eNSCR_Queue },
        { { "job_key",      eNSPT_Id, eNSPA_Required },
          { "progress_msg", eNSPT_Str, eNSPA_Required } } },
    // MGET job_key : id
    { "MGET",     { &CNetScheduleHandler::ProcessGetMessage,         eNSCR_Queue },
        { { "job_key", eNSPT_Id, eNSPA_Required } } },
    { "MONI",     { &CNetScheduleHandler::ProcessMonitor,            eNSCR_Queue } },
    // DUMP [ job_key : id ]
    { "DUMP",     { &CNetScheduleHandler::ProcessDump,               eNSCR_Queue },
        { { "job_key", eNSPT_Id, eNSPA_Optional } } },
    // QPRT status : id
    { "QPRT",     { &CNetScheduleHandler::ProcessPrintQueue,         eNSCR_Queue },
        { { "status", eNSPT_Id, eNSPA_Required } } },
    // STSN [ affinity_token : keystr(aff) ]
    { "STSN",     { &CNetScheduleHandler::ProcessStatusSnapshot,     eNSCR_Queue },
        { { "aff", eNSPT_Str, eNSPA_Optional, "" } } },
    { "QERY",     { &CNetScheduleHandler::ProcessQuery,              eNSCR_Queue },
        { { "where",  eNSPT_Str, eNSPA_Required },
          { "action", eNSPT_Id,  eNSPA_Optional, "COUNT" },
          { "fields", eNSPT_Str, eNSPA_Optional } } },
    { "QSEL",     { &CNetScheduleHandler::ProcessSelectQuery,        eNSCR_Queue },
        { { "select", eNSPT_Str, eNSPA_Required } } },
    // GETP [ client_info : id ]
    { "GETP",     { &CNetScheduleHandler::ProcessGetParam,           eNSCR_Queue },
        { { "info", eNSPT_Str, eNSPA_Optional } } },
    { "GETC",     { &CNetScheduleHandler::ProcessGetConfiguration,   eNSCR_Queue } },
    // CLRN id : string
    { "CLRN",     { &CNetScheduleHandler::ProcessClearWorkerNode,    eNSCR_Queue },
        { { "guid", eNSPT_Str, eNSPA_Required } } },

    /*** Submitter role ***/
    // SST job_key : id -- submitter fast status, changes timestamp
    { "SST",      { &CNetScheduleHandler::ProcessFastStatusS,        eNSCR_Submitter },
        { { "job_key", eNSPT_Id, eNSPA_Required } } },
    // SUBMIT input : str [ progress_msg : str ] [ port : uint [ timeout : uint ]]
    //        [ affinity_token : keystr(aff) ] [ job_mask : keyint(msk) ]
    //        [ tags : keystr(tags) ]
    { "SUBMIT",   { &CNetScheduleHandler::ProcessSubmit,             eNSCR_Submitter },
        { { "input",        eNSPT_Str, eNSPA_Required },        // input
          { "progress_msg", eNSPT_Str, eNSPA_Optional },        // param1
          { "port",         eNSPT_Int, eNSPA_Optional },
          { "timeout",      eNSPT_Int, eNSPA_Optional },
          { "aff",          eNSPT_Str, eNSPA_Optional, "" },    // affinity_token
          { "msk",          eNSPT_Int, eNSPA_Optional, "0" },
          { "tags",         eNSPT_Str, eNSPA_Optional, "" },    // tags
          { "ip",           eNSPT_Str, eNSPA_Optional, "" },    // param3
          { "sid",          eNSPT_Str, eNSPA_Optional, "" } } },// param2
    // CANCEL job_key : id
    { "CANCEL",   { &CNetScheduleHandler::ProcessCancel,             eNSCR_Submitter },
        { { "job_key", eNSPT_Id, eNSPA_Required } } },
    // DROJ job_key : id
    { "DROJ",     { &CNetScheduleHandler::ProcessDropJob,            eNSCR_Submitter },
        { { "job_key", eNSPT_Id, eNSPA_Required } } },
    { "BSUB",     { &CNetScheduleHandler::ProcessSubmitBatch,        eNSCR_Submitter },
        { { "port",         eNSPT_Int, eNSPA_Optional },
          { "timeout",      eNSPT_Int, eNSPA_Optional },
          { "ip",           eNSPT_Str, eNSPA_Optional, "" },
          { "sid",          eNSPT_Str, eNSPA_Optional, "" } } },
    // FRES job_key : id
    { "FRES",     { &CNetScheduleHandler::ProcessForceReschedule,    eNSCR_Submitter },
        { { "job_key", eNSPT_Id, eNSPA_Required } } },
    // READ limit : int [ timeout : int ] -> group : int jobs : str (encoded_vec)
    { "READ",     { &CNetScheduleHandler::ProcessReading,            eNSCR_Submitter },
        { { "count",   eNSPT_Int, eNSPA_Required },
          { "timeout", eNSPT_Int, eNSPA_Optional, "0" } } },
    // CFRM group : int jobs : str
    { "CFRM",     { &CNetScheduleHandler::ProcessConfirm,            eNSCR_Submitter },
        { { "count",  eNSPT_Int, eNSPA_Required },
          { "output", eNSPT_Str, eNSPA_Required } } },
    // FRED group : int jobs : str [ message : str ]
    { "FRED",     { &CNetScheduleHandler::ProcessReadFailed,         eNSCR_Submitter },
        { { "count",   eNSPT_Int, eNSPA_Required },
          { "output",  eNSPT_Str, eNSPA_Required },
          { "err_msg", eNSPT_Str, eNSPA_Optional } } },
    // RDRB group : int jobs : str
    { "RDRB",     { &CNetScheduleHandler::ProcessReadRollback,       eNSCR_Submitter },
        { { "count",   eNSPT_Int, eNSPA_Required },
          { "output",  eNSPT_Str, eNSPA_Required } } },

    /*** Worker role ***/
    // WST job_key : id -- worker node fast status, does not change timestamp
    { "WST",      { &CNetScheduleHandler::ProcessFastStatusW,        eNSCR_Worker },
        { { "job_key", eNSPT_Id, eNSPA_Required } } },
    // GET [ port : int ] [affinity_list : keystr(aff) ]
    { "GET",      { &CNetScheduleHandler::ProcessGetJob,             eNSCR_Worker },
        { { "port", eNSPT_Id,  eNSPA_Optional },
          { "aff",  eNSPT_Str, eNSPA_Optional, "" } } },
    // PUT job_key : id  job_return_code : int  output : str
    { "PUT",      { &CNetScheduleHandler::ProcessPut,                eNSCR_Worker },
        { { "job_key",         eNSPT_Id,  eNSPA_Required },
          { "job_return_code", eNSPT_Id,  eNSPA_Required },
          { "output",          eNSPT_Str, eNSPA_Required } } },
    // RETURN job_key : id
    { "RETURN",   { &CNetScheduleHandler::ProcessReturn,             eNSCR_Worker },
        { { "job_key", eNSPT_Id, eNSPA_Required } } },
    // WGET port : uint  timeout : uint
    //      [affinity_list : keystr(aff) ]
    { "WGET",     { &CNetScheduleHandler::ProcessWaitGet,            eNSCR_Worker },
        { { "port",    eNSPT_Int, eNSPA_Required },
          { "timeout", eNSPT_Int, eNSPA_Required },
          { "aff",     eNSPT_Str, eNSPA_Optional, "" } } },
    // JRTO job_key : id  timeout : uint     OBSOLETE, throws exception
    { "JRTO",     { &CNetScheduleHandler::ProcessJobRunTimeout,      eNSCR_Worker },
        { { "job_key", eNSPT_Id,  eNSPA_Required },
          { "timeout", eNSPT_Int, eNSPA_Required } } },
    // FPUT job_key : id  err_msg : str  output : str  job_return_code : int
    { "FPUT",     { &CNetScheduleHandler::ProcessPutFailure,         eNSCR_Worker },
        { { "job_key",         eNSPT_Id,  eNSPA_Required },
          { "err_msg",         eNSPT_Str, eNSPA_Required },
          { "output",          eNSPT_Str, eNSPA_Required },
          { "job_return_code", eNSPT_Int, eNSPA_Required } } },
    // JXCG [ job_key : id [ job_return_code : int [ output : str ] ] ]
    //      [affinity_list : keystr(aff) ]
    { "JXCG",     { &CNetScheduleHandler::ProcessJobExchange,        eNSCR_Worker },
        { { "job_key",         eNSPT_Id,  eNSPA_Optchain },
          { "job_return_code", eNSPT_Int, eNSPA_Optchain },
          { "output",          eNSPT_Str, eNSPA_Optional },
          { "aff",             eNSPT_Str, eNSPA_Optional, "" } } },
    // REGC port : uint
    { "REGC",     { &CNetScheduleHandler::ProcessRegisterClient,     eNSCR_Worker },
        { { "port", eNSPT_Int, eNSPA_Required } } },
    // URGC port : uint
    { "URGC",     { &CNetScheduleHandler::ProcessUnRegisterClient,   eNSCR_Worker },
        { { "port", eNSPT_Int, eNSPA_Required } } },
    // JDEX job_key : id timeout : uint
    { "JDEX",     { &CNetScheduleHandler::ProcessJobDelayExpiration, eNSCR_Worker },
        { { "job_key", eNSPT_Id,  eNSPA_Required },
          { "timeout", eNSPT_Int, eNSPA_Required } } },
    // AFLS
    { "AFLS",     { &CNetScheduleHandler::ProcessGetAffinityList,    eNSCR_Worker } },
    // INIT port : uint id : string
    { "INIT",     { &CNetScheduleHandler::ProcessInitWorkerNode,     eNSCR_Worker },
        { { "port", eNSPT_Int, eNSPA_Required },
          { "guid", eNSPT_Str, eNSPA_Required } } },

    { NULL },
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


CNetScheduleHandler::CNetScheduleHandler(CNetScheduleServer* server)
    : m_PeerAddr(0), m_Server(server),
      m_Incaps(~0L), m_Unreported(~0L),
      m_VersionControl(false),
      m_RequestContextFactory(new CNSRequestContextFactory(server)),
      m_ReqParser(sm_CommandMap)
{}


EIO_Event
CNetScheduleHandler::GetEventsToPollFor(const CTime** /*alarm_time*/) const
{
    if (m_DelayedOutput) return eIO_Write;
    return eIO_Read;
}

void CNetScheduleHandler::OnOpen(void)
{
    CSocket&        socket = GetSocket();
    STimeout        to = {m_Server->GetInactivityTimeout(), 0};

    socket.DisableOSSendDelay();
    socket.SetTimeout(eIO_ReadWrite, &to);

    socket.GetPeerAddress(&m_PeerAddr, 0, eNH_NetworkByteOrder);
    // always use localhost(127.0*) address for clients coming from
    // the same net address (sometimes it can be 127.* or full address)
    if (m_PeerAddr == m_Server->GetHostNetAddr()) {
        m_PeerAddr = CSocketAPI::GetLoopbackAddress();
    }

    m_WorkerNode = new CWorkerNode_Real(m_PeerAddr);

    m_ProcessMessage = &CNetScheduleHandler::ProcessMsgAuth;
    m_DelayedOutput = NULL;
}


void CNetScheduleHandler::OnWrite()
{
    if (m_DelayedOutput)
        (this->*m_DelayedOutput)();
}


void CNetScheduleHandler::OnClose(IServer_ConnectionHandler::EClosePeer peer)
{
//    LOG_POST(Warning << "Socket closed by " <<
//        (peer == IServer_ConnectionHandler::eOurClose ? "server" : "client"));
}


void CNetScheduleHandler::OnTimeout()
{
//    ERR_POST("OnTimeout!");
}


void CNetScheduleHandler::OnOverflow(EOverflowReason reason)
{
//    ERR_POST("OnOverflow!");
    switch (reason) {
    case eOR_ConnectionPoolFull:
        WriteErr("eCommunicationError:Connection pool full");
        break;
    case eOR_UnpollableSocket:
        WriteErr("eCommunicationError:Unpollable connection");
        break;
    default:
        break;
    }
}


void CNetScheduleHandler::OnMessage(BUF buffer)
{
    if (m_Server->ShutdownRequested()) {
        WriteErr("NetSchedule server is shutting down. Session aborted.");
        return;
    }
    CRequestContextGuard guard;
    try {
        (this->*m_ProcessMessage)(buffer);
    }
    catch (CNetScheduleException &ex)
    {
        WriteErr(string(ex.GetErrCodeString()) + ":" + ex.GetMsg());
        string msg = x_FormatErrorMessage("Server error", ex.what());
        ERR_POST(msg);
        x_WriteErrorToMonitor(msg);
        throw;
    }
    catch (CNSProtoParserException &ex)
    {
        WriteErr(string(ex.GetErrCodeString()) + ":" + ex.GetMsg());
        string msg = x_FormatErrorMessage("Server error", ex.what());
        ERR_POST(msg);
        x_WriteErrorToMonitor(msg);
        throw;
    }
    catch (CNetServiceException &ex)
    {
        WriteErr(string(ex.GetErrCodeString()) + ":" + ex.GetMsg());
        string msg = x_FormatErrorMessage("Server error", ex.what());
        ERR_POST(msg);
        x_WriteErrorToMonitor(msg);
        throw;
    }
    catch (CBDB_ErrnoException& ex)
    {
        if (ex.IsRecovery()) {
            string msg = x_FormatErrorMessage(
                "Fatal Berkeley DB error: DB_RUNRECOVERY. "
                "Emergency shutdown initiated!", ex.what());
            ERR_POST(msg);
            x_WriteErrorToMonitor(msg);
            m_Server->SetShutdownFlag();
        } else {
            string msg = x_FormatErrorMessage("BDB error", ex.what());
            ERR_POST(msg);
            x_WriteErrorToMonitor(msg);

            string err = "eInternalError:Internal database error - " + 
                         string(ex.what());
            WriteErr(NStr::PrintableString(err));
        }
        throw;
    }
    catch (CBDB_Exception &ex)
    {
        string err = "eInternalError:Internal database (BDB) error - " +
                     string(ex.what());
        WriteErr(NStr::PrintableString(err));

        string msg = x_FormatErrorMessage("BDB error", ex.what());
        ERR_POST(msg);
        x_WriteErrorToMonitor(msg);
        throw;
    }
    catch (exception& ex)
    {
        string err = "eInternalError:Internal error - " +
                     string(ex.what());
        WriteErr(NStr::PrintableString(err));

        string msg = x_FormatErrorMessage("STL exception", ex.what());
        ERR_POST(msg);
        x_WriteErrorToMonitor(msg);
        throw;
    }
}


void CNetScheduleHandler::WriteMsg(const char*   prefix,
                                   const string& msg,
                                   bool          interim)
{
    CSocket&        socket = GetSocket();
    size_t          msg_length = 0;
    const char*     buf_ptr;
    string          buffer;
    size_t          prefix_size = strlen(prefix);

    if (prefix_size + msg.size() + 2 > kMaxMessageSize) {
        buffer = prefix + msg + "\r\n";
        msg_length = buffer.size();
        buf_ptr = buffer.data();
    } else {
        char*   ptr = m_MsgBuffer;
        strcpy(ptr, prefix);
        ptr += prefix_size;
        size_t  msg_size = msg.size();
        memcpy(ptr, msg.data(), msg_size);
        ptr += msg_size;
        *ptr++ = '\r';
        *ptr++ = '\n';
        msg_length = ptr - m_MsgBuffer;
        buf_ptr = m_MsgBuffer;
    }

    bool    is_log = m_Server->IsLog();
    if (is_log || IsMonitoring()) {
        size_t  log_length = min(msg_length, (size_t) 1024);
        bool    shortened = log_length < msg_length;
        while (log_length > 0 && (buf_ptr[log_length-1] == '\n' ||
            buf_ptr[log_length-1] == '\r')) {
                log_length--;
        }

        if (is_log) {
            {{
            CDiagContext_Extra extra = GetDiagContext().Extra();
            extra.Print("answer", string(buf_ptr, log_length));
            }}
            if (!interim) {
                // TODO: remove prefix, replace with success/failure, reflect it
                // in request status.
                CDiagContext::GetRequestContext().SetRequestStatus(200);
                GetDiagContext().PrintRequestStop();
            }
        }
        if (IsMonitoring()) {
            string lmsg;
            x_MakeLogMessage(lmsg, "ANS", string(buf_ptr, log_length));
            if (shortened) lmsg += "...";
            MonitorPost(lmsg + "\n");
        }
    }

    size_t          n_written;
    EIO_Status      io_st = socket.Write(buf_ptr, msg_length, &n_written);
    if (interim && io_st) {
        NCBI_THROW(CNetServiceException,
                   eCommunicationError, "Socket write error.");
    }
}


void CNetScheduleHandler::WriteInterim(const string& msg)
{
    WriteMsg("OK:", msg, true);
}


void CNetScheduleHandler::WriteOK(const string& msg)
{
    WriteMsg("OK:", msg);
}


void CNetScheduleHandler::WriteErr(const string& msg)
{
    WriteMsg("ERR:", msg);
}


void CNetScheduleHandler::ProcessMsgAuth(BUF buffer)
{
    s_ReadBufToString(buffer, m_AuthString);

    if (m_Server->AdminHostValid(m_PeerAddr) &&
        (m_AuthString == "netschedule_control" ||
         m_AuthString == "netschedule_admin"))
    {
        // TODO: queue admin should be checked in ProcessMsgQueue,
        // when queue info is available
        m_Incaps &= ~(eNSAC_Admin | eNSAC_QueueAdmin);
    }
    m_WorkerNode->SetAuth(m_AuthString);
    m_ProcessMessage = &CNetScheduleHandler::ProcessMsgQueue;
}


void CNetScheduleHandler::ProcessMsgQueue(BUF buffer)
{
    s_ReadBufToString(buffer, m_QueueName);

    if (m_QueueName != "noname" && !m_QueueName.empty()) {
        m_QueueRef.Reset(m_Server->OpenQueue(m_QueueName));
        m_Incaps &= ~eNSAC_Queue;
        CRef<CQueue> q(GetQueue());
        if (q->IsWorkerAllowed(m_PeerAddr))
            m_Incaps &= ~eNSAC_Worker;
        if (q->IsSubmitAllowed(m_PeerAddr))
            m_Incaps &= ~eNSAC_Submitter;
        if (q->IsVersionControl())
            m_VersionControl = true;
    }

    // TODO: When all worker nodes will learn to send the INIT command,
    // the following line has to be changed to something like
    // m_ProcessMessage = &CNetScheduleHandler::ProcessInitWorkerNode.
    m_ProcessMessage = &CNetScheduleHandler::ProcessMsgRequest;
}


// Workhorse method
void CNetScheduleHandler::ProcessMsgRequest(BUF buffer)
{
    s_ReadBufToString(buffer, m_Request);

    m_CommandNumber = m_Server->GetCommandNumber();

    m_JobReq.Init();
    SParsedCmd cmd;
    try {
        cmd = x_ParseCommand(m_Request);
    }
    catch (const CNSProtoParserException& ex) {
        // Rewrite error in usual terms
        // To prevent stall due to possible very long message (which
        // quotes original, arbitrarily long user input) we truncate it here
        // to the value long enough for diagnostics.
        WriteErr("eProtocolSyntaxError:" + ex.GetMsg().substr(0, 128));
        return;
    }

    const SCommandExtra&    extra = cmd.command->extra;

    x_CheckAccess(extra.role);
    m_JobReq.SetParamFields(cmd.params);

    if (extra.processor == &CNetScheduleHandler::ProcessQuitSession) {
        ProcessQuitSession(0);
        return;
    }

    // program version control
    if (m_VersionControl &&
        // we want status request to be fast, skip version control
        // TODO: How about two other status calls - SST and WST?
        (extra.processor != &CNetScheduleHandler::ProcessStatus) &&
        // bypass for admin tools
        (m_Incaps & eNSAC_Admin)) {
        if (!x_CheckVersion()) {
            WriteErr("eInvalidClientOrVersion:");
            CSocket& socket = GetSocket();
            socket.Close();
            return;
        }
        // One check per session is enough
        m_VersionControl = false;
    }

    // If the command requires queue, hold a hard reference to this
    // queue from a weak one (m_Queue) in queue_ref, and take C pointer
    // into queue_ptr. Otherwise queue_ptr is NULL, which is OK for
    // commands which does not require a queue.
    CRef<CQueue>        queue_ref;
    CQueue*             queue_ptr = NULL;
    if (extra.role & eNSAC_Queue) {
        queue_ref.Reset(GetQueue());
        queue_ptr = queue_ref.GetPointer();
    }

    if (extra.role & eNSAC_Worker) {
        _ASSERT(extra.role & eNSAC_Queue);
        if (!m_WorkerNode->IsRegistered())
            queue_ptr->GetWorkerNodeList().
                RegisterNode(m_WorkerNode);

        if (!m_WorkerNode->IsIdentified()) {
            if (m_JobReq.port > 0)
                queue_ptr->GetWorkerNodeList().
                    IdentifyWorkerNodeByAddress(m_WorkerNode, m_JobReq.port);
            else if (m_JobReq.job_id > 0)
                queue_ptr->GetWorkerNodeList().
                    IdentifyWorkerNodeByJobId(m_WorkerNode, m_JobReq.job_id);
        } else if (m_JobReq.port > 0) {
            queue_ptr->GetWorkerNodeList().SetPort(
                m_WorkerNode, m_JobReq.port);
        }

        m_WorkerNode->SetNotificationTimeout(0);
    }

    (this->*extra.processor)(queue_ptr);
}


// Message processors for ProcessSubmitBatch
void CNetScheduleHandler::ProcessMsgBatchHeader(BUF buffer)
{
    // Expecting BTCH size | ENDS
    char        cmd[256];
    size_t      msg_size = BUF_Read(buffer, cmd, sizeof(cmd)-1);
    cmd[msg_size] = 0;
    m_ReqParser.SetCommandMap(sm_BatchHeaderMap);
    try {
        try {
            SParsedCmd      parsed = x_ParseCommand(cmd);
            const string&   size_str = parsed.params["size"];
            if (!size_str.empty()) {
                m_BatchSize = NStr::StringToInt(size_str);
            }
            (this->*parsed.command->extra.processor)(0);
        }
        catch (const CNSProtoParserException& ex) {
            // Rewrite error in usual terms
            // To prevent stall due to possible very long message (which
            // quotes original, arbitrarily long user input) we truncate it here
            // to the value long enough for diagnostics.
            WriteErr("eProtocolSyntaxError:" + ex.GetMsg().substr(0, 128) +
                    ", BTCH or ENDS expected");
            m_BatchJobs.clear();
            m_ProcessMessage = &CNetScheduleHandler::ProcessMsgRequest;
        }
        m_ReqParser.SetCommandMap(sm_CommandMap);
    }
    catch (...) {
        // ???
        // Should we care here about m_BatchJobs and m_ProcessMessage
        m_ReqParser.SetCommandMap(sm_CommandMap);
        throw;
    }
}


void CNetScheduleHandler::ProcessMsgBatchJob(BUF buffer)
{
    // Expecting:
    // "input" [affp|aff="affinity_token"] [msk=1]
    //         [tags="key1\tval1\tkey2\t\tkey3\tval3"]
    string      msg;
    s_ReadBufToString(buffer, msg);

    CJob&           job = m_BatchJobs[m_BatchPos];
    TNSProtoParams  params;
    try {
        m_ReqParser.ParseArguments(msg, sm_BatchArgs, &params);
    }
    catch (const CNSProtoParserException& ex) {
        // Rewrite error in usual terms
        // To prevent stall due to possible very long message (which
        // quotes original, arbitrarily long user input) we truncate it here
        // to the value long enough for diagnostics.
        WriteErr("eProtocolSyntaxError:" + ex.GetMsg().substr(0, 128));
        m_ProcessMessage = &CNetScheduleHandler::ProcessMsgRequest;
        return;
    }
    catch (const CException&) {
        WriteErr("eProtocolSyntaxError:"
                 "Invalid batch submission, syntax error");
        m_ProcessMessage = &CNetScheduleHandler::ProcessMsgRequest;
        // Shouldn't we return here? Why process further?
        return;
    }

    m_JobReq.SetParamFields(params);
    job.SetInput(NStr::ParseEscapes(m_JobReq.input));
    if (m_JobReq.param1 == "match") {
        // We reuse jobs from m_BatchJobs, so we always should reset this
        // field. We should call SetAffinityToken before SetAffinityId, because
        // it clears out id field of the job.
        job.SetAffinityToken("");
        job.SetAffinityId(kMax_I4);
    } else {
        job.SetAffinityToken(NStr::ParseEscapes(m_JobReq.affinity_token));
    }
    job.SetTags(m_JobReq.tags);

    job.SetMask(m_JobReq.job_mask);
    job.SetSubmAddr(m_PeerAddr);
    job.SetSubmPort(m_BatchSubmPort);
    job.SetSubmTimeout(m_BatchSubmTimeout);
    job.SetClientIP(m_BatchClientIP);
    job.SetClientSID(m_BatchClientSID);

    if (++m_BatchPos >= m_BatchSize)
        m_ProcessMessage = &CNetScheduleHandler::ProcessMsgBatchSubmit;
}


void CNetScheduleHandler::ProcessMsgBatchSubmit(BUF buffer)
{
    // Expecting ENDB
    char        cmd[256];
    size_t      msg_size = BUF_Read(buffer, cmd, sizeof(cmd)-1);
    cmd[msg_size] = 0;

    m_ReqParser.SetCommandMap(sm_BatchEndMap);
    try {
        try {
            x_ParseCommand(cmd);
        }
        catch (const CException&) {
            BUF_Read(buffer, 0, BUF_Size(buffer));
            WriteErr("eProtocolSyntaxError:"
                     "Batch submit error - unexpected end of batch");
        }
        m_ReqParser.SetCommandMap(sm_CommandMap);
    }
    catch (...) {
        m_ReqParser.SetCommandMap(sm_CommandMap);
        throw;
    }

    double comm_elapsed = m_BatchStopWatch.Elapsed();

    // we have our batch now
    CRequestContextSubmitGuard req_guard(m_Server,
                                         m_BatchClientIP, m_BatchClientSID);
    CStopWatch  sw(CStopWatch::eStart);
    unsigned    job_id = GetQueue()->SubmitBatch(m_BatchJobs);
    double      db_elapsed = sw.Elapsed();

    if (IsMonitoring()) {
        ITERATE(vector<CJob>, it, m_BatchJobs) {
            x_MonitorJob(*it);
        }
        MonitorPost("::ProcessMsgSubmitBatch " +
                    m_LocalTimer.GetLocalTime().AsString() + " ==> " +
                    GetSocket().GetPeerAddress() + " " + m_AuthString +
                    " batch block size=" + NStr::UIntToString(m_BatchSize) +
                    " comm.time=" +
                    NStr::DoubleToString(comm_elapsed, 4, NStr::fDoubleFixed) +
                    " trans.time=" +
                    NStr::DoubleToString(db_elapsed, 4, NStr::fDoubleFixed));
    }

    {{
        char    buf[1024];
        sprintf(buf, "%u %s %u",
                job_id, m_Server->GetHost().c_str(),
                unsigned(m_Server->GetPort()));

        WriteOK(buf);
    }}

    m_ProcessMessage = &CNetScheduleHandler::ProcessMsgBatchHeader;
}


bool CNetScheduleHandler::IsMonitoring() const
{
    if ((m_Incaps & eNSAC_Queue) == 0) {
        CRef<CQueue> queue_ref(m_QueueRef.Lock());
        if (queue_ref != NULL)
            return queue_ref->IsMonitoring();
    }
    return false;
}


void CNetScheduleHandler::MonitorPost(const string& msg)
{
    if (!(m_Incaps & eNSAC_Queue))
        GetQueue()->MonitorPost(msg);
}


// TODO: Move exception processing code here
void CNetScheduleHandler::ProcessError(const CException& ex)
{
    WriteErr(ex.GetMsg());
}



//////////////////////////////////////////////////////////////////////////
// Process* methods for processing commands

void CNetScheduleHandler::ProcessFastStatusS(CQueue* q)
{
    TJobStatus status = q->GetJobStatus(m_JobReq.job_id);
    // TODO: update timestamp
    WriteOK(NStr::IntToString((int) status));
}


void CNetScheduleHandler::ProcessFastStatusW(CQueue* q)
{
    TJobStatus status = q->GetJobStatus(m_JobReq.job_id);
    WriteOK(NStr::IntToString((int) status));
}


void CNetScheduleHandler::ProcessSubmit(CQueue* q)
{
    CJob    job;
    job.SetInput(NStr::ParseEscapes(m_JobReq.input));
    job.SetAffinityToken(NStr::ParseEscapes(m_JobReq.affinity_token));
    job.SetTags(m_JobReq.tags);
    job.SetMask(m_JobReq.job_mask);
    job.SetSubmAddr(m_PeerAddr);
    job.SetSubmPort(m_JobReq.port);
    job.SetSubmTimeout(m_JobReq.timeout);
    job.SetProgressMsg(m_JobReq.param1);
    // Never leave Client IP empty, if we're not provided with a real one,
    // use peer address as a last resort. See also ProcessSubmitBatch.
    if (!m_JobReq.param3.empty())
        job.SetClientIP(m_JobReq.param3);
    else {
        string  s_ip;
        NS_FormatIPAddress(m_PeerAddr, s_ip);
        job.SetClientIP(s_ip);
    }
    job.SetClientSID(m_JobReq.param2);

    unsigned    job_id = q->Submit(job);
    string      str_job_id(CNetScheduleKey(job_id,
                                           m_Server->GetHost(),
                                           m_Server->GetPort()));
    WriteOK(str_job_id);

    if (IsMonitoring()) {
        MonitorPost("::ProcessSubmit " +
                    m_LocalTimer.GetLocalTime().AsString() + str_job_id +
                    " ==> " + GetSocket().GetPeerAddress() + " " +
                    m_AuthString);
        x_MonitorJob(job);
    }
}


void CNetScheduleHandler::ProcessSubmitBatch(CQueue* q)
{
    m_BatchSubmPort    = m_JobReq.port;
    m_BatchSubmTimeout = m_JobReq.timeout;
    if (!m_JobReq.param3.empty())
        m_BatchClientIP = m_JobReq.param3;
    else
        NS_FormatIPAddress(m_PeerAddr, m_BatchClientIP);
    m_BatchClientSID   = m_JobReq.param2;
    WriteOK("Batch submit ready");
    m_ProcessMessage = &CNetScheduleHandler::ProcessMsgBatchHeader;
}


void CNetScheduleHandler::ProcessBatchStart(CQueue*)
{
    m_BatchPos = 0;
    m_BatchStopWatch.Restart();
    m_BatchJobs.resize(m_BatchSize);
    if (m_BatchSize)
        m_ProcessMessage = &CNetScheduleHandler::ProcessMsgBatchJob;
    else
        // Unfortunately, because batches can be generated by
        // client program, we better honor zero size ones.
        // Skip right to waiting for 'ENDB'.
        m_ProcessMessage = &CNetScheduleHandler::ProcessMsgBatchSubmit;
    if (IsMonitoring()) {
        MonitorPost("::ProcessMsgBatchHeader " +
                    m_LocalTimer.GetLocalTime().AsString() + " " +
                    GetSocket().GetPeerAddress() + " " + m_AuthString);
    }
}


void CNetScheduleHandler::ProcessBatchSequenceEnd(CQueue*)
{
    m_BatchJobs.clear();
    m_ProcessMessage = &CNetScheduleHandler::ProcessMsgRequest;
    WriteOK();
}


void CNetScheduleHandler::ProcessCancel(CQueue* q)
{
    q->Cancel(m_JobReq.job_id);
    WriteOK();
}


void CNetScheduleHandler::ProcessStatus(CQueue* q)
{
    TJobStatus  status = q->GetJobStatus(m_JobReq.job_id);
    int         ret_code = 0;
    string      input, output, error;

    bool res = q->GetJobDescr(m_JobReq.job_id, &ret_code,
                              &input, &output,
                              &error, 0, status);
    if (!res) status = CNetScheduleAPI::eJobNotFound;
    string buf = NStr::IntToString((int) status);
    if (status != CNetScheduleAPI::eJobNotFound) {
            buf += " " + NStr::IntToString(ret_code) +
                   " \""   + NStr::PrintableString(output) +
                   "\" \"" + NStr::PrintableString(error) +
                   "\" \"" + NStr::PrintableString(input) +
                   "\"";
    }
    WriteOK(buf);
}


void CNetScheduleHandler::ProcessGetJob(CQueue* q)
{
    list<string>    aff_list;
    NStr::Split(NStr::ParseEscapes(m_JobReq.affinity_token),
                "\t,", aff_list, NStr::eNoMergeDelims);
    CJob            job;
    q->PutResultGetJob(m_WorkerNode,
        0, 0, 0,
        m_RequestContextFactory.GetPointer(), &aff_list, &job);

    unsigned        job_id = job.GetId();
    if (job_id) {
        x_MakeGetAnswer(job);
        WriteOK(m_Answer);
    } else {
        WriteOK();
    }

    if (job_id && IsMonitoring()) {
        MonitorPost("::ProcessGetJob " +
                    m_LocalTimer.GetLocalTime().AsString() + m_Answer +
                    " ==> " + GetSocket().GetPeerAddress() +
                    " " + m_AuthString);
    }

    // We don't call UnRegisterNotificationListener here
    // because for old worker nodes it finishes the session
    // and cleans up all the information, affinity association
    // included.
    if (m_JobReq.port)  // unregister notification
        q->GetWorkerNodeList().RegisterNotificationListener(m_WorkerNode, 0);
}


void CNetScheduleHandler::ProcessWaitGet(CQueue* q)
{
    list<string> aff_list;
    NStr::Split(NStr::ParseEscapes(m_JobReq.affinity_token),
                "\t,", aff_list, NStr::eNoMergeDelims);

    CJob job;
    q->PutResultGetJob(m_WorkerNode,
                       0,0,0,
                       m_RequestContextFactory.GetPointer(),
                       &aff_list,
                       &job);

    unsigned job_id = job.GetId();
    if (job_id) {
        x_MakeGetAnswer(job);
        WriteOK(m_Answer);
        return;
    }

    // job not found, initiate waiting mode
    WriteOK();

    q->GetWorkerNodeList().
        RegisterNotificationListener(m_WorkerNode, m_JobReq.timeout);
}


void CNetScheduleHandler::ProcessPut(CQueue* q)
{
    string output = NStr::ParseEscapes(m_JobReq.output);
    q->PutResultGetJob(m_WorkerNode,
                       m_JobReq.job_id,
                       m_JobReq.job_return_code,
                       &output,
                       0, 0, 0);
    WriteOK();
}


void CNetScheduleHandler::ProcessJobExchange(CQueue* q)
{
    list<string> aff_list;
    NStr::Split(NStr::ParseEscapes(m_JobReq.affinity_token),
                "\t,", aff_list, NStr::eNoMergeDelims);

    CJob job;
    string output = NStr::ParseEscapes(m_JobReq.output);
    q->PutResultGetJob(m_WorkerNode,
                       m_JobReq.job_id,
                       m_JobReq.job_return_code,
                       &output,
                       // GetJob params
                       m_RequestContextFactory.GetPointer(),
                       &aff_list,
                       &job);

    unsigned job_id = job.GetId();
    if (job_id) {
        x_MakeGetAnswer(job);
        WriteOK(m_Answer);
    } else {
        WriteOK();
    }

    if (IsMonitoring()) {
        MonitorPost("::ProcessJobExchange " +
                    m_LocalTimer.GetLocalTime().AsString() + " " + m_Answer +
                    " ==> " + GetSocket().GetPeerAddress() + " " +
                    m_AuthString);
    }
}


void CNetScheduleHandler::ProcessPutMessage(CQueue* q)
{
    if (q->PutProgressMessage(m_JobReq.job_id, m_JobReq.param1))
        WriteOK();
    else
        WriteErr("Job not found");
}


void CNetScheduleHandler::ProcessGetMessage(CQueue* q)
{
    string progress_msg;

    if (q->GetJobDescr(m_JobReq.job_id, 0, 0, 0, 0,
                       &progress_msg)) {
        WriteOK(progress_msg);
    } else {
        WriteErr("Job not found");
    }
}


void CNetScheduleHandler::ProcessForceReschedule(CQueue* q)
{
    q->ForceReschedule(m_JobReq.job_id);
    WriteOK();
}


void CNetScheduleHandler::ProcessPutFailure(CQueue* q)
{
    q->FailJob(m_WorkerNode, m_JobReq.job_id,
               NStr::ParseEscapes(m_JobReq.err_msg),
               NStr::ParseEscapes(m_JobReq.output),
               m_JobReq.job_return_code);
    WriteOK();
}


void CNetScheduleHandler::ProcessDropQueue(CQueue* q)
{
    q->Truncate();
    WriteOK();
}


void CNetScheduleHandler::ProcessReturn(CQueue* q)
{
    q->ReturnJob(m_JobReq.job_id);
    WriteOK();
}


void CNetScheduleHandler::ProcessJobRunTimeout(CQueue*)
{
    if (IsMonitoring()) {
        MonitorPost(GetFastLocalTime().AsString() +
                    " OBSOLETE CQueue::SetJobRunTimeout: Job id=" +
                    NStr::IntToString(m_JobReq.job_id));
    }
    LOG_POST(Warning << "Obsolete API SetRunTimeout called");
    NCBI_THROW(CNetScheduleException,
        eObsoleteCommand, "Use API JobDelayExpiration (cmd JDEX) instead");
}


void CNetScheduleHandler::ProcessJobDelayExpiration(CQueue* q)
{
    q->JobDelayExpiration(m_WorkerNode,
        m_JobReq.job_id, m_JobReq.timeout);
    WriteOK();
}


void CNetScheduleHandler::ProcessDropJob(CQueue* q)
{
    q->EraseJob(m_JobReq.job_id);
    WriteOK();
}


void CNetScheduleHandler::ProcessStatistics(CQueue* q)
{
    CSocket&    socket = GetSocket();
    socket.DisableOSSendDelay(false);
    const       string& what = m_JobReq.param1;
    time_t      curr = time(0);

    if (!what.empty() && what != "ALL") {
        x_StatisticsNew(q, what, curr);
        return;
    }

    WriteInterim(NETSCHEDULED_FULL_VERSION);
    string started = "Started: " + m_Server->GetStartTime().AsString();
    WriteInterim(started);

    string load_str = "Load: jobs dispatched: ";
    load_str +=
        NStr::DoubleToString(q->GetAverage(CQueue::eStatGetEvent));
    load_str += "/sec, jobs complete: ";
    load_str +=
        NStr::DoubleToString(q->GetAverage(CQueue::eStatPutEvent));
    load_str += "/sec, DB locks: ";
    load_str +=
        NStr::DoubleToString(q->GetAverage(CQueue::eStatDBLockEvent));
    load_str += "/sec, Job DB writes: ";
    load_str +=
        NStr::DoubleToString(q->GetAverage(CQueue::eStatDBWriteEvent));
    load_str += "/sec";
    WriteInterim(load_str);

    for (int i = CNetScheduleAPI::ePending;
         i < CNetScheduleAPI::eLastStatus; ++i) {
        TJobStatus st = TJobStatus(i);
        unsigned count = q->CountStatus(st);


        string st_str = CNetScheduleAPI::StatusToString(st);
        st_str += ": ";
        st_str += NStr::UIntToString(count);

        WriteInterim(st_str);

        if (m_JobReq.param1 == "ALL") {
            TNSBitVector::statistics bv_stat;
            q->StatusStatistics(st, &bv_stat);
            st_str = "   bit_blk=";
            st_str.append(NStr::UIntToString(bv_stat.bit_blocks));
            st_str += "; gap_blk=";
            st_str.append(NStr::UIntToString(bv_stat.gap_blocks));
            st_str += "; mem_used=";
            st_str.append(NStr::UIntToString(bv_stat.memory_used));
            WriteInterim(st_str);
        }
    } // for

    /*
    if (m_JobReq.param1 == "ALL") {

        unsigned db_recs = m_Queue->GetQueue()->CountRecs();
        string recs = "Records:";
        recs += NStr::UIntToString(db_recs);
        WriteInterim(recs);
        WriteInterim("[Database statistics]:");

        {{
            CNcbiOstrstream ostr;
            m_Queue->GetQueue()->PrintStat(ostr);
            ostr << ends;

            char* stat_str = ostr.str();
    //        size_t os_str_len = ostr.pcount()-1;
    //        stat_str[os_str_len] = 0;
            try {
                WriteOK(stat_str);
            } catch (...) {
                ostr.freeze(false);
                throw;
            }
            ostr.freeze(false);
        }}
    }
    */

    if (m_JobReq.param1 == "ALL") {
        WriteInterim("[Berkeley DB Mutexes]:");
        {{
            CNcbiOstrstream ostr;
            m_Server->PrintMutexStat(ostr);
            ostr << ends;

            char* stat_str = ostr.str();
            try {
                WriteInterim(stat_str);
            } catch (...) {
                ostr.freeze(false);
                throw;
            }
            ostr.freeze(false);
        }}

        WriteInterim("[Berkeley DB Locks]:");
        {{
            CNcbiOstrstream ostr;
            m_Server->PrintLockStat(ostr);
            ostr << ends;

            char* stat_str = ostr.str();
            try {
                WriteInterim(stat_str);
            } catch (...) {
                ostr.freeze(false);
                throw;
            }
            ostr.freeze(false);
        }}

        WriteInterim("[Berkeley DB Memory Usage]:");
        {{
            CNcbiOstrstream ostr;
            m_Server->PrintMemStat(ostr);
            ostr << ends;

            char* stat_str = ostr.str();
            try {
                WriteInterim(stat_str);
            } catch (...) {
                ostr.freeze(false);
                throw;
            }
            ostr.freeze(false);
        }}

        WriteInterim("[BitVector block pool]:");

        {{
            const TBlockAlloc::TBucketPool::TBucketVector& bv =
                TBlockAlloc::GetPoolVector();
            size_t pool_vec_size = bv.size();
            string tmp_str = "Pool vector size: ";
            tmp_str.append(NStr::UIntToString(pool_vec_size));
            WriteInterim(tmp_str);
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

                        WriteInterim(tmp_str);
                    }
                }
            }
        }}
    }

    WriteInterim("[Worker node statistics]:");

    {{
        CNcbiOstrstream ostr;
        q->PrintWorkerNodeStat(ostr, curr);
        ostr << ends;

        char* stat_str = ostr.str();
        try {
            WriteInterim(stat_str);
        } catch (...) {
            ostr.freeze(false);
            throw;
        }
        ostr.freeze(false);
    }}

    {{
        WriteInterim("[Configured job submitters]:");
        CNcbiOstrstream ostr;
        q->PrintSubmHosts(ostr);
        ostr << ends;

        char* stat_str = ostr.str();
        try {
            WriteInterim(stat_str);
        } catch (...) {
            ostr.freeze(false);
            throw;
        }
        ostr.freeze(false);
    }}

    {{
        WriteInterim("[Configured workers]:");
        CNcbiOstrstream ostr;
        q->PrintWNodeHosts(ostr);
        ostr << ends;

        char* stat_str = ostr.str();
        try {
            WriteInterim(stat_str);
        } catch (...) {
            ostr.freeze(false);
            throw;
        }
        ostr.freeze(false);
    }}

    WriteOK("END");
}


void CNetScheduleHandler::ProcessStatusSnapshot(CQueue* q)
{
    CJobStatusTracker::TStatusSummaryMap st_map;

    bool aff_exists = q->CountStatus(&st_map,
        NStr::ParseEscapes(m_JobReq.affinity_token));
    if (!aff_exists) {
        WriteErr(string("eProtocolSyntaxError:Unknown affinity token \"")
                 + m_JobReq.affinity_token + "\"");
        return;
    }
    ITERATE(CJobStatusTracker::TStatusSummaryMap, it, st_map) {
        string st_str = CNetScheduleAPI::StatusToString(it->first);
        st_str.push_back(' ');
        st_str.append(NStr::UIntToString(it->second));
        WriteInterim(st_str);
    }
    WriteOK("END");
}


void CNetScheduleHandler::ProcessMonitor(CQueue* q)
{
    CSocket& socket = GetSocket();
    socket.DisableOSSendDelay(false);
    WriteOK(NETSCHEDULED_FULL_VERSION);
    WriteOK("Started: " + m_Server->GetStartTime().AsString());
    // Socket is released from regular scheduling here - it is
    // write only for monitor since this moment.
    q->SetMonitorSocket(socket);
}


void CNetScheduleHandler::ProcessReloadConfig(CQueue* q)
{
    CNcbiApplication* app = CNcbiApplication::Instance();

    bool reloaded = app->ReloadConfig(CMetaRegistry::fReloadIfChanged);
    if (reloaded)
        m_Server->Configure(app->GetConfig());
    WriteOK();
}


void CNetScheduleHandler::ProcessActiveCount(CQueue* q)
{
    WriteOK(NStr::UIntToString(m_Server->CountActiveJobs()));
}


void CNetScheduleHandler::ProcessDump(CQueue* q)
{
    // TODO this method can not support session, because socket is closed at
    // the end.
    CSocket&            socket = GetSocket();
    SOCK                sock = socket.GetSOCK();
    socket.SetOwnership(eNoOwnership);
    socket.Reset(0, eTakeOwnership, eCopyTimeoutsToSOCK);

    CConn_SocketStream  ios(sock, eTakeOwnership);

    ios << "OK:" << NETSCHEDULED_FULL_VERSION << endl;

    if (m_JobReq.job_id == 0) {
        ios << "OK:" << "[Job status matrix]:";

        q->PrintJobStatusMatrix(ios);

        ios << "OK:[Job DB]:" << endl;
        q->PrintAllJobDbStat(ios);
    } else {
        try {
            TJobStatus  status = q->GetJobStatus(m_JobReq.job_id);
            string      st_str = CNetScheduleAPI::StatusToString(status);

            ios << "OK:" << "[Job status matrix]:" << st_str;
            ios << "OK:[Job DB]:" << endl;
            q->PrintJobDbStat(m_JobReq.job_id, ios);
            ios << "OK:END" << endl;
        }
        catch (CException&)
        {
            ios << "ERR:Cannot get status for job " << m_JobReq.job_key;
        }
    }
}


void CNetScheduleHandler::ProcessPrintQueue(CQueue* q)
{
    // TODO this method can not support session, because socket is closed at
    // the end.
    TJobStatus job_status = CNetScheduleAPI::StringToStatus(m_JobReq.param1);

    if (job_status == CNetScheduleAPI::eJobNotFound) {
        string err_msg = "Status unknown: " + m_JobReq.param1;
        WriteErr(err_msg);
        return;
    }

    CSocket& socket = GetSocket();
    SOCK sock = socket.GetSOCK();
    socket.SetOwnership(eNoOwnership);
    socket.Reset(0, eTakeOwnership, eCopyTimeoutsToSOCK);

    CConn_SocketStream ios(sock, eTakeOwnership);  // sock is being passed and used exclusively

    q->PrintQueue(ios,
                  job_status,
                  m_Server->GetHost(),
                  m_Server->GetPort());

    ios << "OK:END" << endl;
}


void CNetScheduleHandler::ProcessShutdown(CQueue*)
{
    LOG_POST(Info << "Shutdown request... " + GetSocket().GetPeerAddress() +
                     " " + GetFastLocalTime().AsString());
    m_Server->SetShutdownFlag();
    WriteOK();
}


void CNetScheduleHandler::ProcessVersion(CQueue*)
{
    WriteOK(NETSCHEDULED_FULL_VERSION);
}


void CNetScheduleHandler::ProcessRegisterClient(CQueue* q)
{
    q->GetWorkerNodeList().
        RegisterNotificationListener(m_WorkerNode, 0);
    WriteOK();
}


void CNetScheduleHandler::ProcessUnRegisterClient(CQueue* q)
{
    q->UnRegisterNotificationListener(m_WorkerNode);
    WriteOK();
}


void CNetScheduleHandler::ProcessQList(CQueue*)
{
    WriteOK(m_Server->GetQueueNames(";"));
}


void CNetScheduleHandler::ProcessLog(CQueue*)
{
    const char* str = m_JobReq.param1.c_str();
    if (NStr::strcasecmp(str, "ON") == 0) {
        m_Server->SetLogging(true);
        LOG_POST("Logging turned ON");
    } else if (NStr::strcasecmp(str, "OFF") == 0) {
        m_Server->SetLogging(false);
        LOG_POST("Logging turned OFF");
    } else {
        WriteErr("eProtocolSyntaxError:Incorrect LOG syntax");
        return;
    }
    WriteOK();
}


void CNetScheduleHandler::ProcessQuitSession(CQueue*)
{
    // read trailing input
    CSocket& socket = GetSocket();
    STimeout to; to.sec = to.usec = 0;
    socket.SetTimeout(eIO_Read, &to);
    socket.Read(NULL, 1024);
    socket.Close();
}


void CNetScheduleHandler::ProcessCreateQueue(CQueue*)
{
    const string&   qname  = m_JobReq.param1;
    const string&   qclass = m_JobReq.param2;
    const string&   comment = NStr::ParseEscapes(m_JobReq.param3);
    m_Server->CreateQueue(qname, qclass, comment);
    WriteOK();
}


void CNetScheduleHandler::ProcessDeleteQueue(CQueue*)
{
    const string&   qname = m_JobReq.param1;
    m_Server->DeleteQueue(qname);
    WriteOK();
}


void CNetScheduleHandler::ProcessQueueInfo(CQueue*)
{
    const string&   qname = m_JobReq.param1;
    int             kind;
    string          qclass;
    string          comment;
    m_Server->QueueInfo(qname, kind, &qclass, &comment);
    WriteOK(NStr::IntToString(kind) + "\t" + qclass + "\t\"" +
            NStr::PrintableString(comment) + "\"");
}


void CNetScheduleHandler::ProcessQuery(CQueue* q)
{
    string result_str;

    string query = NStr::ParseEscapes(m_JobReq.param1);

    list<string> fields;
    m_SelectedIds.reset(q->ExecSelect(query, fields));

    string& action = m_JobReq.param2;

    if (!fields.empty()) {
        WriteErr(string("eProtocolSyntaxError:") +
                 "SELECT is not allowed");
        m_SelectedIds.release();
        return;
    } else if (action == "SLCT") {
        // Execute 'projection' phase
        string str_fields(NStr::ParseEscapes(m_JobReq.param3));
        // Split fields
        NStr::Split(str_fields, "\t,", fields, NStr::eNoMergeDelims);
        q->PrepareFields(m_FieldDescr, fields);
        m_DelayedOutput = &CNetScheduleHandler::WriteProjection;
        return;
    } else if (action == "COUNT") {
        result_str = NStr::IntToString(m_SelectedIds->count());
    } else if (action == "IDS") {
        result_str = x_SerializeBitVector(*m_SelectedIds);
    } else if (action == "DROP") {
        // NOT IMPLEMENTED
    } else if (action == "FRES") {
        // NOT IMPLEMENTED
    } else if (action == "CNCL") {
        // NOT IMPLEMENTED
    } else {
        WriteErr(string("eProtocolSyntaxError:") +
                 "Unknown action " + action);
        m_SelectedIds.release();
        return;
    }
    WriteOK(result_str);
    m_SelectedIds.release();
}


void CNetScheduleHandler::ProcessSelectQuery(CQueue* q)
{
    string          result_str;
    string          query = NStr::ParseEscapes(m_JobReq.param1);
    list<string>    fields;

    m_SelectedIds.reset(q->ExecSelect(query, fields));

    if (fields.empty()) {
        WriteErr(string("eProtocolSyntaxError:") +
                 "SQL-like select is expected");
        m_SelectedIds.release();
    } else {
        q->PrepareFields(m_FieldDescr, fields);
        m_DelayedOutput = &CNetScheduleHandler::WriteProjection;
    }
}


void CNetScheduleHandler::ProcessGetParam(CQueue* q)
{
    string res = "max_input_size=" + NStr::IntToString(q->GetMaxInputSize()) +
                 ";max_output_size=" + 
                 NStr::IntToString(q->GetMaxOutputSize()) + ";" +
                 NETSCHEDULED_FEATURES;
    if (!m_JobReq.param1.empty())
        res += ";" + m_JobReq.param1;
    WriteOK(res);
}


void CNetScheduleHandler::ProcessGetConfiguration(CQueue* q)
{
    CQueue::TParameterList parameters;
    parameters = q->GetParameters();
    ITERATE(CQueue::TParameterList, it, parameters) {
        WriteInterim(it->first + '=' + it->second);
    }
    WriteOK("END");
}


void CNetScheduleHandler::ProcessReading(CQueue* q)
{
    unsigned        read_id;
    TNSBitVector    jobs;
    q->ReadJobs(m_PeerAddr, m_JobReq.count,
                m_JobReq.timeout, read_id, jobs);
    WriteOK(NStr::UIntToString(read_id) + " " + NS_EncodeBitVector(jobs));
}


void CNetScheduleHandler::ProcessConfirm(CQueue* q)
{
    TNSBitVector jobs = NS_DecodeBitVector(m_JobReq.output);
    q->ConfirmJobs(m_JobReq.count, jobs);
    WriteOK();
}


void CNetScheduleHandler::ProcessReadFailed(CQueue* q)
{
    TNSBitVector jobs = NS_DecodeBitVector(m_JobReq.output);
//    m_JobReq.err_msg; we still don't (and probably, won't) use this
    q->FailReadingJobs(m_JobReq.count, jobs);
    WriteOK();
}


void CNetScheduleHandler::ProcessReadRollback(CQueue* q)
{
    TNSBitVector jobs = NS_DecodeBitVector(m_JobReq.output);
    q->ReturnReadingJobs(m_JobReq.count, jobs);
    WriteOK();
}


void CNetScheduleHandler::ProcessGetAffinityList(CQueue* q)
{
    WriteOK(q->GetAffinityList());
}


void CNetScheduleHandler::ProcessInitWorkerNode(CQueue* q)
{
    string old_id = m_WorkerNode->GetId();
    string new_id = m_JobReq.param1.substr(0, kMaxWorkerNodeIdSize);

    if (old_id != new_id) {
        if (!old_id.empty())
            q->ClearWorkerNode(m_WorkerNode, "replaced by new node");

        q->GetWorkerNodeList().SetId(m_WorkerNode, new_id);
    }

    WriteOK();
}


void CNetScheduleHandler::ProcessClearWorkerNode(CQueue* q)
{
    q->ClearWorkerNode(
        m_JobReq.param1.substr(0, kMaxWorkerNodeIdSize), "cleared");

    WriteOK();
}


void CNetScheduleHandler::WriteProjection()
{
    unsigned        chunk_size = 10000;
    unsigned        n;
    TNSBitVector    chunk;
    unsigned        job_id = 0;
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
        GetQueue()->ExecProject(record_set, chunk, m_FieldDescr);
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
            WriteInterim(str_rec);
        }
    }
    if (!m_SelectedIds->any()) {
        WriteOK("END");
        m_SelectedIds.release();
        m_DelayedOutput = NULL;
    }
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


string CNetScheduleHandler::x_SerializeBitVector(TNSBitVector& bv)
{
    TNSBitVector::statistics st;
    bv.optimize(0, TNSBitVector::opt_compress, &st);
    size_t dst_size = st.max_serialize_mem * 4 / 3 + 1;
    size_t src_size = st.max_serialize_mem;
    unsigned char* buf = new unsigned char[src_size + dst_size];
    size_t size = bm::serialize(bv, buf);
    size_t src_read, dst_written, line_len;
    line_len = 0; // no line feeds in encoded value
    unsigned char* encode_buf = buf+src_size;
    BASE64_Encode(buf, size, &src_read, encode_buf, dst_size, &dst_written, &line_len);
    delete [] buf;
    return string((const char*) encode_buf, dst_written);
}


TNSBitVector CNetScheduleHandler::x_DeserializeBitVector(const string& s)
{
    size_t src_read, dst_written;
    size_t dst_size =  s.size();
    vector<unsigned char> dst_buf(dst_size, 0);
    BASE64_Decode(s.data(),  s.size(), &src_read,
                  &dst_buf[0], dst_size, &dst_written);
    TNSBitVector bv;
    bm::deserialize(bv, &dst_buf[0]);
    return bv;
}


void CNetScheduleHandler::x_MonitorJob(const CJob& job)
{
    string msg = string("id: ") + NStr::IntToString(job.GetId()) +
                 string(" input: ") + job.GetInput() +
                 string(" aff: ") + job.GetAffinityToken() +
                 string(" aff_id: ") + NStr::IntToString(job.GetAffinityId()) +
                 string(" mask: ") + NStr::IntToString(job.GetMask()) +
                 string(" tags: ");
    ITERATE(TNSTagList, it, job.GetTags()) {
        msg += it->first + "=" + it->second + "; ";
    }
    MonitorPost(msg);
}


bool CNetScheduleHandler::x_CheckVersion()
{
    string::size_type   pos = m_AuthString.find("prog='");
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
        if (!GetQueue()->IsMatchingClient(auth_prog_info))
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
    unsigned deficit = role & m_Incaps;
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
        (m_Incaps & eNSAC_Queue)        ||  // or if no queue
        GetQueue()->GetDenyAccessViolations();  // or if queue configured so
    bool report =
        !(m_Incaps & eNSAC_Queue)       &&  // only if there is a queue
        GetQueue()->GetLogAccessViolations()  &&  // so configured
        (m_Unreported & deficit);           // and we did not report it yet
    if (report) {
        m_Unreported &= ~deficit;
        string msg = "Unauthorized access from: " +
                     GetSocket().GetPeerAddress() + " " + m_AuthString;
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


string CNetScheduleHandler::x_FormatErrorMessage(const string& header, const string& what)
{
    return header + ": " + what + " Peer=" + GetSocket().GetPeerAddress() +
           " " + m_AuthString + m_Request;
}


void CNetScheduleHandler::x_WriteErrorToMonitor(const string& msg)
{
    if (IsMonitoring()) {
        MonitorPost(m_LocalTimer.GetLocalTime().AsString() + msg);
    }
}


CNetScheduleHandler::SParsedCmd
CNetScheduleHandler::x_ParseCommand(CTempString command)
{
    bool is_log = m_Server->IsLog();
    if (is_log) {
        GetDiagContext().PrintRequestStart()
            .Print("cmd", command)
//                .Print("node", worker_node->GetId())
            .Print("queue", m_QueueName)
//                .Print("job_id", NStr::IntToString(job.GetId()))
        ;
    }
    if (IsMonitoring()) {
        string lmsg;
        x_MakeLogMessage(lmsg, "REQ", command);
        lmsg += "\n";
        MonitorPost(lmsg);
    }
    return m_ReqParser.ParseCommand(command);
}


void CNetScheduleHandler::x_MakeLogMessage(string&       lmsg,
                                           const string& op,
                                           const string& text)
{
    string              peer = GetSocket().GetPeerAddress();
    // get only host name
    string::size_type   offs = peer.find_first_of(":");
    if (offs != string::npos) {
        peer.erase(offs, peer.length());
    }

    lmsg = op + ':' + NStr::IntToString(m_CommandNumber) + ';' + peer + ';' +
           m_AuthString + ';' + m_QueueName + ';' + text;
}


void CNetScheduleHandler::x_MakeGetAnswer(const CJob& job)
{
    string& answer = m_Answer;
    answer = CNetScheduleKey(job.GetId(),
        m_Server->GetHost(), m_Server->GetPort());
    answer.append(" \"");
    answer.append(NStr::PrintableString(job.GetInput()));
    answer.append("\"");

    // We can re-use old jout and jerr job parameters for affinity and
    // session id/client ip respectively.
    answer.append(" \"");
    answer.append(NStr::PrintableString(job.GetAffinityToken()));
    answer.append("\" \"");
    answer.append(job.GetClientIP() + " " + job.GetClientSID());
    answer.append("\""); // to keep compat. with jout & jerr

    answer.append(" ");
    answer.append(NStr::UIntToString(job.GetMask()));
}


void CNetScheduleHandler::x_StatisticsNew(CQueue*       q,
                                          const string& what,
                                          time_t        curr)
{
    if (what == "WNODE") {
        CNcbiOstrstream ostr;
        q->PrintWorkerNodeStat(ostr, curr, eWNF_WithNodeId);
        ostr << ends;

        char* stat_str = ostr.str();
        try {
            WriteInterim(stat_str);
        } catch (...) {
            ostr.freeze(false);
            throw;
        }
        ostr.freeze(false);
    }
    if (what == "JOBS") {
        unsigned total = 0;
        for (int i = CNetScheduleAPI::ePending;
            i < CNetScheduleAPI::eLastStatus; ++i) {
                TJobStatus      st = TJobStatus(i);
                unsigned        count = q->CountStatus(st);

                total += count;
                WriteInterim(CNetScheduleAPI::StatusToString(st) +
                             ": " + NStr::UIntToString(count));
        } // for
        WriteInterim("Total: " + NStr::UIntToString(total));
    }
    WriteOK("END");
}

