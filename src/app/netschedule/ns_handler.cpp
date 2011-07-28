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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>


USING_NCBI_SCOPE;



/// NetSchedule command parser
///
/// @internal
///

CNetScheduleHandler::SCommandMap CNetScheduleHandler::sm_BatchHeaderMap[] = {
    { "BTCH", { &CNetScheduleHandler::x_ProcessBatchStart, 0 },
        { { "size", eNSPT_Int, eNSPA_Required } } },
    { "ENDS", { &CNetScheduleHandler::x_ProcessBatchSequenceEnd, 0 } },
    { NULL }
};

CNetScheduleHandler::SCommandMap CNetScheduleHandler::sm_BatchEndMap[] = {
    { "ENDB" },
    { NULL }
};

SNSProtoArgument s_BatchArgs[] = {
    { "input", eNSPT_Str, eNSPA_Required },
    { "affp",  eNSPT_Id,  eNSPA_Optional | fNSPA_Or | fNSPA_Match, "" },
    { "aff",   eNSPT_Str, eNSPA_Optional, "" },
    { "msk",   eNSPT_Int, eNSPA_Optional, "0" },
    { "tags",  eNSPT_Str, eNSPA_Optional, "" },
    { NULL }
};

CNetScheduleHandler::SCommandMap CNetScheduleHandler::sm_CommandMap[] = {

    /*** Admin role ***/
    { "SHUTDOWN", { &CNetScheduleHandler::x_ProcessShutdown,
                    eNSCR_Admin } },

    /*** Any role ***/
    { "VERSION",  { &CNetScheduleHandler::x_ProcessVersion,
                    eNSCR_Any } },
    { "LOG",      { &CNetScheduleHandler::x_CmdNotImplemented,
                    eNSCR_Any } },
    { "QUIT",     { &CNetScheduleHandler::x_ProcessQuitSession,
                    eNSCR_Any } },
    { "RECO",     { &CNetScheduleHandler::x_ProcessReloadConfig,
                    eNSCR_Any } }, // ?? Admin
    { "ACNT",     { &CNetScheduleHandler::x_ProcessActiveCount,
                    eNSCR_Any } },
    { "QLST",     { &CNetScheduleHandler::x_ProcessQList,
                    eNSCR_Any } },
    { "QINF",     { &CNetScheduleHandler::x_ProcessQueueInfo,
                    eNSCR_Any },
        { { "qname", eNSPT_Id, eNSPA_Required } } },

    /*** QueueAdmin role ***/
    { "DROPQ",    { &CNetScheduleHandler::x_ProcessDropQueue,
                    eNSCR_QueueAdmin } },

    /*** DynClassAdmin role ***/
    // QCRE qname : id  qclass : id [ comment : str ]
    { "QCRE",     { &CNetScheduleHandler::x_ProcessCreateQueue,
                    eNSAC_DynClassAdmin },
        { { "qname",   eNSPT_Id,  eNSPA_Required },
          { "qclass",  eNSPT_Id,  eNSPA_Required },
          { "comment", eNSPT_Str, eNSPA_Optional } } },
    // QDEL qname : id
    { "QDEL",     { &CNetScheduleHandler::x_ProcessDeleteQueue,
                    eNSAC_DynQueueAdmin },
        { { "qname", eNSPT_Id, eNSPA_Required } } },

    /*** Queue role ***/
    // STATUS job_key : id
    { "STATUS",   { &CNetScheduleHandler::x_ProcessStatus,
                    eNSCR_Queue },
        { { "job_key", eNSPT_Id, eNSPA_Required } } },
    // STAT [ option : id ] -- "ALL"
    { "STAT",     { &CNetScheduleHandler::x_ProcessStatistics,
                    eNSCR_Queue },
        { { "option", eNSPT_Id, eNSPA_Optional } } },
    // MPUT job_key : id  progress_msg : str
    { "MPUT",     { &CNetScheduleHandler::x_ProcessPutMessage,
                    eNSCR_Queue },
        { { "job_key",      eNSPT_Id, eNSPA_Required },
          { "progress_msg", eNSPT_Str, eNSPA_Required } } },
    // MGET job_key : id
    { "MGET",     { &CNetScheduleHandler::x_ProcessGetMessage,
                    eNSCR_Queue },
        { { "job_key", eNSPT_Id, eNSPA_Required } } },
    { "MONI",     { &CNetScheduleHandler::x_CmdNotImplemented,
                    eNSCR_Queue } },
    // DUMP [ job_key : id ]
    { "DUMP",     { &CNetScheduleHandler::x_ProcessDump,
                    eNSCR_Queue },
        { { "job_key", eNSPT_Id, eNSPA_Optional } } },
    // QPRT status : id
    { "QPRT",     { &CNetScheduleHandler::x_ProcessPrintQueue,
                    eNSCR_Queue },
        { { "status", eNSPT_Id, eNSPA_Required } } },
    // STSN [ affinity_token : keystr(aff) ]
    { "STSN",     { &CNetScheduleHandler::x_ProcessStatusSnapshot,
                    eNSCR_Queue },
        { { "aff", eNSPT_Str, eNSPA_Optional, "" } } },
    { "QERY",     { &CNetScheduleHandler::x_ProcessQuery,
                    eNSCR_Queue },
        { { "where",  eNSPT_Str, eNSPA_Required },
          { "action", eNSPT_Id,  eNSPA_Optional, "COUNT" },
          { "fields", eNSPT_Str, eNSPA_Optional } } },
    { "QSEL",     { &CNetScheduleHandler::x_ProcessSelectQuery,
                    eNSCR_Queue },
        { { "select", eNSPT_Str, eNSPA_Required } } },
    // GETP [ client_info : id ]
    { "GETP",     { &CNetScheduleHandler::x_ProcessGetParam,
                    eNSCR_Queue },
        { { "info", eNSPT_Str, eNSPA_Optional } } },
    { "GETC",     { &CNetScheduleHandler::x_ProcessGetConfiguration,
                    eNSCR_Queue } },
    // CLRN id : string
    { "CLRN",     { &CNetScheduleHandler::x_ProcessClearWorkerNode,
                    eNSCR_Queue },
        { { "guid", eNSPT_Str, eNSPA_Required } } },

    /*** Submitter role ***/
    // SST job_key : id -- submitter fast status, changes timestamp
    { "SST",      { &CNetScheduleHandler::x_ProcessFastStatusS,
                    eNSCR_Submitter },
        { { "job_key", eNSPT_Id, eNSPA_Required } } },
    // SUBMIT input : str [ progress_msg : str ] [ port : uint [ timeout : uint ]]
    //        [ affinity_token : keystr(aff) ] [ job_mask : keyint(msk) ]
    //        [ tags : keystr(tags) ]
    { "SUBMIT",   { &CNetScheduleHandler::x_ProcessSubmit,
                    eNSCR_Submitter },
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
    { "CANCEL",   { &CNetScheduleHandler::x_ProcessCancel,
                    eNSCR_Submitter },
        { { "job_key", eNSPT_Id, eNSPA_Required } } },
    // DROJ job_key : id
    { "DROJ",     { &CNetScheduleHandler::x_ProcessDropJob,
                    eNSCR_Submitter },
        { { "job_key", eNSPT_Id, eNSPA_Required } } },
    { "BSUB",     { &CNetScheduleHandler::x_ProcessSubmitBatch,
                    eNSCR_Submitter },
        { { "port",         eNSPT_Int, eNSPA_Optional },
          { "timeout",      eNSPT_Int, eNSPA_Optional },
          { "ip",           eNSPT_Str, eNSPA_Optional, "" },
          { "sid",          eNSPT_Str, eNSPA_Optional, "" } } },
    // FRES job_key : id
    { "FRES",     { &CNetScheduleHandler::x_CmdNotImplemented,
                    eNSCR_Submitter } },
    // READ limit : int [ timeout : int ] -> group : int jobs : str (encoded_vec)
    { "READ",     { &CNetScheduleHandler::x_ProcessReading,
                    eNSCR_Submitter },
        { { "count",   eNSPT_Int, eNSPA_Required },
          { "timeout", eNSPT_Int, eNSPA_Optional, "0" } } },
    // CFRM group : int jobs : str
    { "CFRM",     { &CNetScheduleHandler::x_ProcessConfirm,
                    eNSCR_Submitter },
        { { "count",  eNSPT_Int, eNSPA_Required },
          { "output", eNSPT_Str, eNSPA_Required } } },
    // FRED group : int jobs : str [ message : str ]
    { "FRED",     { &CNetScheduleHandler::x_ProcessReadFailed,
                    eNSCR_Submitter },
        { { "count",   eNSPT_Int, eNSPA_Required },
          { "output",  eNSPT_Str, eNSPA_Required },
          { "err_msg", eNSPT_Str, eNSPA_Optional } } },
    // RDRB group : int jobs : str
    { "RDRB",     { &CNetScheduleHandler::x_ProcessReadRollback,
                    eNSCR_Submitter },
        { { "count",   eNSPT_Int, eNSPA_Required },
          { "output",  eNSPT_Str, eNSPA_Required } } },

    /*** Worker role ***/
    // WST job_key : id -- worker node fast status, does not change timestamp
    { "WST",      { &CNetScheduleHandler::x_ProcessFastStatusW,
                    eNSCR_Worker },
        { { "job_key", eNSPT_Id, eNSPA_Required } } },
    // GET [ port : int ] [affinity_list : keystr(aff) ]
    { "GET",      { &CNetScheduleHandler::x_ProcessGetJob,
                    eNSCR_Worker },
        { { "port", eNSPT_Id,  eNSPA_Optional },
          { "aff",  eNSPT_Str, eNSPA_Optional, "" } } },
    // PUT job_key : id  job_return_code : int  output : str
    { "PUT",      { &CNetScheduleHandler::x_ProcessPut,
                    eNSCR_Worker },
        { { "job_key",         eNSPT_Id,  eNSPA_Required },
          { "job_return_code", eNSPT_Id,  eNSPA_Required },
          { "output",          eNSPT_Str, eNSPA_Required } } },
    // RETURN job_key : id
    { "RETURN",   { &CNetScheduleHandler::x_ProcessReturn,
                    eNSCR_Worker },
        { { "job_key", eNSPT_Id, eNSPA_Required } } },
    // WGET port : uint  timeout : uint
    //      [affinity_list : keystr(aff) ]
    { "WGET",     { &CNetScheduleHandler::x_ProcessWaitGet,
                    eNSCR_Worker },
        { { "port",    eNSPT_Int, eNSPA_Required },
          { "timeout", eNSPT_Int, eNSPA_Required },
          { "aff",     eNSPT_Str, eNSPA_Optional, "" } } },
    // JRTO job_key : id  timeout : uint
    { "JRTO",     { &CNetScheduleHandler::x_CmdNotImplemented,
                    eNSCR_Worker } },
    // FPUT job_key : id  err_msg : str  output : str  job_return_code : int
    { "FPUT",     { &CNetScheduleHandler::x_ProcessPutFailure,
                    eNSCR_Worker },
        { { "job_key",         eNSPT_Id,  eNSPA_Required },
          { "err_msg",         eNSPT_Str, eNSPA_Required },
          { "output",          eNSPT_Str, eNSPA_Required },
          { "job_return_code", eNSPT_Int, eNSPA_Required } } },
    // JXCG [ job_key : id [ job_return_code : int [ output : str ] ] ]
    //      [affinity_list : keystr(aff) ]
    { "JXCG",     { &CNetScheduleHandler::x_ProcessJobExchange,
                    eNSCR_Worker },
        { { "job_key",         eNSPT_Id,  eNSPA_Optchain },
          { "job_return_code", eNSPT_Int, eNSPA_Optchain },
          { "output",          eNSPT_Str, eNSPA_Optional },
          { "aff",             eNSPT_Str, eNSPA_Optional, "" } } },
    // REGC port : uint
    { "REGC",     { &CNetScheduleHandler::x_ProcessRegisterClient,
                    eNSCR_Worker },
        { { "port", eNSPT_Int, eNSPA_Required } } },
    // URGC port : uint
    { "URGC",     { &CNetScheduleHandler::x_ProcessUnRegisterClient,
                    eNSCR_Worker },
        { { "port", eNSPT_Int, eNSPA_Required } } },
    // JDEX job_key : id timeout : uint
    { "JDEX",     { &CNetScheduleHandler::x_ProcessJobDelayExpiration,
                    eNSCR_Worker },
        { { "job_key", eNSPT_Id,  eNSPA_Required },
          { "timeout", eNSPT_Int, eNSPA_Required } } },
    // AFLS
    { "AFLS",     { &CNetScheduleHandler::x_ProcessGetAffinityList,
                    eNSCR_Worker } },
    // INIT port : uint id : string
    { "INIT",     { &CNetScheduleHandler::x_ProcessInitWorkerNode,
                    eNSCR_Worker },
        { { "port", eNSPT_Int, eNSPA_Required },
          { "guid", eNSPT_Str, eNSPA_Required } } },

    { NULL },
};


static SNSProtoArgument s_AuthArgs[] = {
    { "client", eNSPT_Str, eNSPA_Optional, "Unknown client" },
    { "params", eNSPT_Str, eNSPA_Ellipsis },
    { NULL }
};



static void s_BufReadHelper(void* data, void* ptr, size_t size)
{
    ((string*) data)->append((const char *) ptr, size);
}


static void s_ReadBufToString(BUF buf, string& str)
{
    size_t      size = BUF_Size(buf);

    str.erase();
    str.reserve(size);

    BUF_PeekAtCB(buf, 0, s_BufReadHelper, &str, size);
    BUF_Read(buf, NULL, size);
}


CNetScheduleHandler::CNetScheduleHandler(CNetScheduleServer* server)
    : m_MsgBufferSize(kInitialMessageBufferSize),
      m_MsgBuffer(new char[kInitialMessageBufferSize]),
      m_PeerAddr(0), m_Server(server),
      m_Incaps(~0L), m_Unreported(~0L),
      m_VersionControl(false),
      m_SingleCmdParser(sm_CommandMap),
      m_BatchHeaderParser(sm_BatchHeaderMap),
      m_BatchEndParser(sm_BatchEndMap)
{}


CNetScheduleHandler::~CNetScheduleHandler()
{
    delete [] m_MsgBuffer;
}


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
    x_SetQuickAcknowledge();

    socket.GetPeerAddress(&m_PeerAddr, 0, eNH_NetworkByteOrder);
    // always use localhost(127.0*) address for clients coming from
    // the same net address (sometimes it can be 127.* or full address)
    if (m_PeerAddr == m_Server->GetHostNetAddr()) {
        m_PeerAddr = CSocketAPI::GetLoopbackAddress();
    }

    m_WorkerNode = new CWorkerNode_Real(m_PeerAddr);

    m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgAuth;
    m_DelayedOutput = NULL;

    // Log the fact of opened connection
    m_ConnContext.Reset(new CRequestContext());
    m_ConnReqId = NStr::UInt8ToString(m_ConnContext->SetRequestID());

    CDiagnosticsGuard   guard(this);
    if (m_Server->IsLog()) {
        CDiagContext_Extra diag_extra = GetDiagContext().PrintRequestStart();
        diag_extra.Print("_type", "conn");
        diag_extra.Print("peer", socket.GetPeerAddress(eSAF_IP));
        diag_extra.Print("pport", socket.GetPeerAddress(eSAF_Port));
        diag_extra.Print("port", socket.GetLocalPort(eNH_HostByteOrder));
        diag_extra.Flush();
    }
    m_ConnContext->SetRequestStatus(eStatus_OK);
}


void CNetScheduleHandler::OnWrite()
{
    CDiagnosticsGuard   guard(this);

    if (m_DelayedOutput)
        (this->*m_DelayedOutput)();
}


void CNetScheduleHandler::OnClose(IServer_ConnectionHandler::EClosePeer peer)
{
    // It's possible that this method will be called before OnOpen - when
    // connection is just created and server is shutting down. In this case
    // OnOpen will never be called.
    if (m_ConnContext.IsNull())
        return;


    CDiagnosticsGuard   guard(this);

    switch (peer)
    {
    case IServer_ConnectionHandler::eOurClose:
        if (m_DiagContext.NotNull()) {
            m_ConnContext->SetRequestStatus(m_DiagContext->GetRequestStatus());
        } else {
            if (m_ConnContext->GetRequestStatus() != eStatus_HTTPProbe)
                m_ConnContext->SetRequestStatus(eStatus_Inactive);
        }
        break;
    case IServer_ConnectionHandler::eClientClose:
        if (m_DiagContext.NotNull()) {
            m_DiagContext->SetRequestStatus(eStatus_BadCmd);
            m_ConnContext->SetRequestStatus(eStatus_BadCmd);
        }
        break;
    }


    if (m_Server->IsLog()) {
        CSocket&        socket = GetSocket();
        CDiagContext::SetRequestContext(m_ConnContext);
        m_ConnContext->SetBytesRd(socket.GetCount(eIO_Read));
        m_ConnContext->SetBytesWr(socket.GetCount(eIO_Write));
        GetDiagContext().PrintRequestStop();
    }

    m_ConnContext.Reset();
}


void CNetScheduleHandler::OnTimeout()
{
    CDiagnosticsGuard       guard(this);

    if (m_Server->IsLog()) {
        LOG_POST(Info << "Inactivity timeout expired, closing connection");
        if (m_DiagContext)
            m_ConnContext->SetRequestStatus(eStatus_CmdTimeout);
        else
            m_ConnContext->SetRequestStatus(eStatus_Inactive);
    }
}


void CNetScheduleHandler::OnOverflow(EOverflowReason reason)
{
    switch (reason) {
    case eOR_ConnectionPoolFull:
        ERR_POST("eCommunicationError:Connection pool full");
        break;
    case eOR_UnpollableSocket:
        ERR_POST("eCommunicationError:Unpollable connection");
        break;
    case eOR_RequestQueueFull:
        ERR_POST("eCommunicationError:Request queue full");
        break;
    default:
        ERR_POST("eCommunicationError:Unknown overflow error");
        break;
    }
}


void CNetScheduleHandler::OnMessage(BUF buffer)
{
    CDiagnosticsGuard   guard(this);

    if (m_Server->ShutdownRequested()) {
        x_WriteMessageNoThrow("ERR:", "NetSchedule server is shutting down. "
                                      "Session aborted.");
        return;
    }

    try {
        // Single line user input processor
        (this->*m_ProcessMessage)(buffer);
    }
    catch (CNetScheduleException &  ex) {
        x_WriteMessageNoThrow("ERR:", string(ex.GetErrCodeString()) +
                                      ":" + ex.GetMsg());
        ERR_POST("Server error: " << ex);
        x_PrintRequestStop(eStatus_ServerError);
    }
    catch (CNSProtoParserException &  ex) {
        x_WriteMessageNoThrow("ERR:", string(ex.GetErrCodeString()) +
                                      ":" + ex.GetMsg());
        ERR_POST("Server error: " << ex);
        x_PrintRequestStop(eStatus_ServerError);
    }
    catch (CNetServiceException &  ex) {
        x_WriteMessageNoThrow("ERR:", string(ex.GetErrCodeString()) +
                                      ":" + ex.GetMsg());
        ERR_POST("Server error: " << ex);
        x_PrintRequestStop(eStatus_ServerError);
    }
    catch (CBDB_ErrnoException &  ex) {
        string  error;
        if (ex.IsRecovery())
            error = NStr::PrintableString("Fatal Berkeley DB error "
                                          "(DB_RUNRECOVERY). Emergency "
                                          "shutdown initiated. " +
                                          string(ex.what()));
        else
            error = NStr::PrintableString("Internal database error - " +
                                          string(ex.what()));

        x_WriteMessageNoThrow("ERR:", "eInternalError:" + error);
        ERR_POST("BDB error: " << ex);
        x_PrintRequestStop(eStatus_ServerError);

        if (ex.IsRecovery())
            m_Server->SetShutdownFlag();
    }
    catch (CBDB_Exception &  ex) {
        string  error = NStr::PrintableString("eInternalError:Internal "
                                              "database (BDB) error - " +
                                              string(ex.what()));
        x_WriteMessageNoThrow("ERR:", error);
        ERR_POST("BDB error: " << ex);
        x_PrintRequestStop(eStatus_ServerError);
    }
    catch (exception &  ex) {
        string  error = NStr::PrintableString("eInternalError:Internal "
                                              "error - " + string(ex.what()));
        x_WriteMessageNoThrow("ERR:", error);
        ERR_POST("STL exception: " << ex.what());
        x_PrintRequestStop(eStatus_ServerError);
    }
}


void CNetScheduleHandler::x_SetQuickAcknowledge(void)
{
    int     fd = 0;
    int     val = 1;

    GetSocket().GetOSHandle(&fd, sizeof(fd));
    setsockopt(fd, IPPROTO_TCP, TCP_QUICKACK, &val, sizeof(val));
}


void CNetScheduleHandler::WriteMessage(CTempString  prefix,
                                       CTempString  msg)
{
    size_t  prefix_size = prefix.size();
    size_t  msg_size = msg.size();
    size_t  required_size = prefix_size + msg_size + 1;

    if (required_size > m_MsgBufferSize) {
        delete [] m_MsgBuffer;
        while (required_size > m_MsgBufferSize)
            m_MsgBufferSize += kMessageBufferIncrement;
        m_MsgBuffer = new char[m_MsgBufferSize];
    }

    memcpy(m_MsgBuffer, prefix.data(), prefix_size);
    memcpy(m_MsgBuffer + prefix_size, msg.data(), msg_size);
    m_MsgBuffer[required_size-1] = '\n';

    // Write to the socket as a single transaction
    size_t      n_written;
    if (GetSocket().Write(m_MsgBuffer, required_size,
                          &n_written) != eIO_Success) {
        NCBI_THROW(CNetServiceException,
                   eCommunicationError, "Socket write error.");
    }
}


void CNetScheduleHandler::WriteMessage(CTempString msg)
{
    size_t  msg_size = msg.size();
    size_t  required_size = msg_size + 1;

    if (required_size > m_MsgBufferSize) {
        delete [] m_MsgBuffer;
        while (required_size > m_MsgBufferSize)
            m_MsgBufferSize += kMessageBufferIncrement;
        m_MsgBuffer = new char[m_MsgBufferSize];
    }

    memcpy(m_MsgBuffer, msg.data(), msg_size);
    m_MsgBuffer[required_size-1] = '\n';

    // Write to the socket as a single transaction
    size_t      n_written;
    if (GetSocket().Write(m_MsgBuffer, required_size,
                          &n_written) != eIO_Success) {
        NCBI_THROW(CNetServiceException,
                   eCommunicationError, "Socket write error.");
    }
}


void CNetScheduleHandler::x_WriteMessageNoThrow(CTempString  prefix,
                                                CTempString  msg)
{
    try {
        WriteMessage(prefix, msg);
    }
    catch (CNetServiceException&  ex) {
        ERR_POST(ex);
    }
}


void CNetScheduleHandler::InitDiagnostics(void)
{
    if (m_DiagContext.NotNull()) {
        CDiagContext::SetRequestContext(m_DiagContext);
    }
    else if (m_ConnContext.NotNull()) {
        CDiagContext::SetRequestContext(m_ConnContext);
    }
}


void CNetScheduleHandler::ResetDiagnostics(void)
{
    CDiagContext::SetRequestContext(NULL);
}


void CNetScheduleHandler::x_ProcessMsgAuth(BUF buffer)
{
    // This should only memorize the received string.
    // The x_ProcessMsgQueue(...)  will parse it.
    // This is done to avoid copying parsed parameters and to have exactly one
    // diagnostics extra with both auth parameters and queue name.
    s_ReadBufToString(buffer, m_RawAuthString);

    // Check if it was systems probe...
    if (strncmp(m_RawAuthString.c_str(), "GET / HTTP/1.", 13) == 0) {
        // That was systems probing ports

        if (m_ConnContext.NotNull()) {
            m_ConnContext->SetRequestStatus(eStatus_HTTPProbe);
        }

        // read trailing input and close the port
        CSocket& socket = GetSocket();
        STimeout to; to.sec = to.usec = 0;
        socket.SetTimeout(eIO_Read, &to);
        socket.Read(NULL, 1024);
        m_Server->CloseConnection(&socket);
        return;
    }

    m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgQueue;
    x_SetQuickAcknowledge();
}


void CNetScheduleHandler::x_ProcessMsgQueue(BUF buffer)
{
    s_ReadBufToString(buffer, m_QueueName);

    // Parse saved raw authorization string and produce log output
    TNSProtoParams      params;
    try {
        m_AuthProgName = "";
        m_SingleCmdParser.ParseArguments(m_RawAuthString, s_AuthArgs, &params);
        if (params.find("prog") != params.end())
            m_AuthProgName = params["prog"];
        m_AuthClientName = params["client"];
    }
    catch (CNSProtoParserException &  ex) {
        ERR_POST("Error authenticating client: '" <<
                 m_RawAuthString << "': " << ex);
        params["client"] = m_RawAuthString;
        m_AuthClientName = m_RawAuthString;
    }

    if (m_Server->AdminHostValid(m_PeerAddr) &&
        (m_AuthClientName == "netschedule_control" ||
         m_AuthClientName == "netschedule_admin"))
    {
        // TODO: queue admin should be checked in ProcessMsgQueue,
        // when queue info is available
        m_Incaps &= ~(eNSAC_Admin | eNSAC_QueueAdmin);
    }
    m_WorkerNode->SetAuth(m_AuthClientName);


    // Produce the log output if required
    if (m_Server->IsLog()) {
        CDiagContext_Extra diag_extra = GetDiagContext().Extra();
        diag_extra.Print("queue", m_QueueName);
        ITERATE(TNSProtoParams, it, params) {
            diag_extra.Print(it->first, it->second);
        }
    }


    // Handle the queue name
    try {
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
    }
    catch (CNetScheduleException &  ex) {
        // There is no such a queue, do not produce stop request
        x_WriteMessageNoThrow("ERR:", string(ex.GetErrCodeString()) +
                                      ":" + ex.GetMsg());
        ERR_POST(ex);
        return;
    }

    // TODO: When all worker nodes will learn to send the INIT command,
    // the following line has to be changed to something like
    // m_ProcessMessage = &CNetScheduleHandler::x_ProcessInitWorkerNode.
    m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
    x_SetQuickAcknowledge();
}


// Workhorse method
void CNetScheduleHandler::x_ProcessMsgRequest(BUF buffer)
{
    m_DiagContext.Reset(new CRequestContext());
    m_DiagContext->SetRequestID();
    InitDiagnostics();

    SParsedCmd      cmd;
    string          msg;
    try {
        s_ReadBufToString(buffer, msg);
        cmd = m_SingleCmdParser.ParseCommand(msg);
        x_PrintRequestStart(cmd);
    }
    catch (const CNSProtoParserException& ex) {
        // Rewrite error in usual terms
        // To prevent stall due to possible very long message (which
        // quotes original, arbitrarily long user input) we truncate it here
        // to the value long enough for diagnostics.

        // Parsing is done before PrintRequestStart(...) so a diag context is
        // not created here - create it just to provide an error output
        x_PrintRequestStart("Invalid command");
        ERR_POST("Error parsing command: " << ex);
        x_PrintRequestStop(eStatus_BadCmd);
        x_WriteMessageNoThrow("ERR:", "eProtocolSyntaxError:" +
                                      ex.GetMsg().substr(0, 128));
        return;
    }

    const SCommandExtra&    extra = cmd.command->extra;

    x_CheckAccess(extra.role);

    m_JobReq.Init();
    if (m_JobReq.SetParamFields(cmd.params) == SJS_Request::eStatus_TooLong) {
        ERR_POST("User input exceeds the limit. Command rejected.");
        x_PrintRequestStop(eStatus_BadCmd);
        x_WriteMessageNoThrow("ERR:", "eDataTooLong:"
                                      "User input exceeds the limit.");
        return;
    }

    if (extra.processor == &CNetScheduleHandler::x_ProcessQuitSession) {
        x_ProcessQuitSession(0);
        return;
    }

    // program version control
    if (m_VersionControl &&
        // we want status request to be fast, skip version control
        // TODO: How about two other status calls - SST and WST?
        (extra.processor != &CNetScheduleHandler::x_ProcessStatus) &&
        // bypass for admin tools
        (m_Incaps & eNSAC_Admin)) {
        if (!x_CheckVersion()) {
            WriteMessage("ERR:eInvalidClientOrVersion:");
            m_Server->CloseConnection(&GetSocket());
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


// Message processors for x_ProcessSubmitBatch
void CNetScheduleHandler::x_ProcessMsgBatchHeader(BUF buffer)
{
    // Expecting BTCH size | ENDS
    try {
        string          msg;
        s_ReadBufToString(buffer, msg);

        SParsedCmd      cmd      = m_BatchHeaderParser.ParseCommand(msg);
        const string &  size_str = cmd.params["size"];

        if (!size_str.empty())
            m_BatchSize = NStr::StringToInt(size_str);
        else
            m_BatchSize = 0;
        (this->*cmd.command->extra.processor)(0);
    }
    catch (const CNSProtoParserException &  ex) {
        // Rewrite error in usual terms
        // To prevent stall due to possible very long message (which
        // quotes original, arbitrarily long user input) we truncate it here
        // to the value long enough for diagnostics.
        x_WriteMessageNoThrow("ERR:", "eProtocolSyntaxError:" +
                                      ex.GetMsg().substr(0, 128) +
                                      ", BTCH or ENDS expected");
        ERR_POST("Error parsing command: " << ex);
        x_PrintRequestStop(eStatus_BadCmd);

        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
    }
    catch (const CException &  ex) {
        x_WriteMessageNoThrow("ERR:", "eProtocolSyntaxError:"
                                      "Error processing BTCH or ENDS.");
        ERR_POST("Error processing command: " << ex);
        x_PrintRequestStop(eStatus_BadCmd);

        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
    }
}


void CNetScheduleHandler::x_ProcessMsgBatchJob(BUF buffer)
{
    // Expecting:
    // "input" [affp|aff="affinity_token"] [msk=1]
    //         [tags="key1\tval1\tkey2\t\tkey3\tval3"]
    string          msg;
    s_ReadBufToString(buffer, msg);

    CJob&           job = m_BatchJobs[m_BatchPos];
    TNSProtoParams  params;
    try {
        m_BatchEndParser.ParseArguments(msg, s_BatchArgs, &params);
    }
    catch (const CNSProtoParserException &  ex) {
        // Rewrite error in usual terms
        // To prevent stall due to possible very long message (which
        // quotes original, arbitrarily long user input) we truncate it here
        // to the value long enough for diagnostics.
        x_WriteMessageNoThrow("ERR:", "eProtocolSyntaxError:" +
                                      ex.GetMsg().substr(0, 128));
        ERR_POST("Error parsing command: " << ex);
        x_PrintRequestStop(eStatus_BadCmd);

        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
        return;
    }
    catch (const CException &  ex) {
        x_WriteMessageNoThrow("ERR:", "eProtocolSyntaxError:"
                                      "Invalid batch submission, syntax error");
        ERR_POST("Error processing command: " << ex);
        x_PrintRequestStop(eStatus_BadCmd);

        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
        return;
    }

    if (m_JobReq.SetParamFields(params) == SJS_Request::eStatus_TooLong) {
        ERR_POST("User input exceeds the limit. Command rejected.");
        x_PrintRequestStop(eStatus_BadCmd);
        x_WriteMessageNoThrow("ERR:", "eDataTooLong:"
                                      "User input exceeds the limit.");
        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
        return;
    }

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
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgBatchSubmit;
}


void CNetScheduleHandler::x_ProcessMsgBatchSubmit(BUF buffer)
{
    // Expecting ENDB
    try {
        string      msg;
        s_ReadBufToString(buffer, msg);
        m_BatchEndParser.ParseCommand(msg);
    }
    catch (const CNSProtoParserException &  ex) {
        BUF_Read(buffer, 0, BUF_Size(buffer));
        x_WriteMessageNoThrow("ERR:", "eProtocolSyntaxError:" +
                                      ex.GetMsg().substr(0, 128));
        ERR_POST("Error parsing command: " << ex);
        x_PrintRequestStop(eStatus_BadCmd);

        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
        return;
    }
    catch (const CException &  ex) {
        BUF_Read(buffer, 0, BUF_Size(buffer));
        x_WriteMessageNoThrow("ERR:", "eProtocolSyntaxError:"
                              "Batch submit error - unexpected end of batch");
        ERR_POST("Error processing command: " << ex);
        x_PrintRequestStop(eStatus_BadCmd);

        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
        return;
    }

    double      comm_elapsed = m_BatchStopWatch.Elapsed();

    // BTCH logging is in a separate context
    CRef<CRequestContext>   current_context(& CDiagContext::GetRequestContext());
    try {
        if (m_Server->IsLog()) {
            CRequestContext *   ctx(new CRequestContext());
            ctx->SetRequestID();
            GetDiagContext().SetRequestContext(ctx);
            GetDiagContext().PrintRequestStart()
                            .Print("_type", "cmd")
                            .Print("cmd", "BTCH")
                            .Print("size", m_BatchJobs.size());
            ctx->SetRequestStatus(CNetScheduleHandler::eStatus_OK);
        }

        // we have our batch now
        CStopWatch  sw(CStopWatch::eStart);
        unsigned    job_id = GetQueue()->SubmitBatch(m_BatchJobs);
        double      db_elapsed = sw.Elapsed();

        if (m_Server->IsLog())
            GetDiagContext().Extra()
                .Print("commit_time",
                       NStr::DoubleToString(comm_elapsed, 4, NStr::fDoubleFixed))
                .Print("transaction_time",
                       NStr::DoubleToString(db_elapsed, 4, NStr::fDoubleFixed));

        WriteMessage("OK:", NStr::UIntToString(job_id) + " " +
                            m_Server->GetHost().c_str() + " " +
                            NStr::UIntToString(unsigned(m_Server->GetPort())));

    } catch (...) {
        if (m_Server->IsLog()) {
            CDiagContext::GetRequestContext().SetRequestStatus(eStatus_ServerError);
            GetDiagContext().PrintRequestStop();
            GetDiagContext().SetRequestContext(current_context);
        }
        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
        throw;
    }

    if (m_Server->IsLog()) {
        GetDiagContext().PrintRequestStop();
        GetDiagContext().SetRequestContext(current_context);
    }

    m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgBatchHeader;
}


//////////////////////////////////////////////////////////////////////////
// Process* methods for processing commands

void CNetScheduleHandler::x_ProcessFastStatusS(CQueue* q)
{
    TJobStatus      status = q->GetJobStatus(m_JobReq.job_id);
    // TODO: update timestamp

    WriteMessage("OK:", NStr::IntToString((int) status));
    if (m_Server->IsLog())
        GetDiagContext().Extra().Print("job_status",
                                       CNetScheduleAPI::StatusToString(status));
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessFastStatusW(CQueue* q)
{
    TJobStatus      status = q->GetJobStatus(m_JobReq.job_id);

    WriteMessage("OK:", NStr::IntToString((int) status));
    if (m_Server->IsLog())
        GetDiagContext().Extra().Print("job_status",
                                       CNetScheduleAPI::StatusToString(status));
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessSubmit(CQueue* q)
{
    CJob        job(m_JobReq, m_PeerAddr);

    // Never leave Client IP empty, if we're not provided with a real one,
    // use peer address as a last resort. See also x_ProcessSubmitBatch.
    if (!m_JobReq.param3.empty())
        job.SetClientIP(m_JobReq.param3);
    else {
        string  s_ip;
        NS_FormatIPAddress(m_PeerAddr, s_ip);
        job.SetClientIP(s_ip);
    }

    WriteMessage("OK:", q->MakeKey(q->Submit(job)));

    // There is no need to log the job key, it is logged at lower level
    // together with all the submitted job parameters
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessSubmitBatch(CQueue* q)
{
    m_BatchSubmPort    = m_JobReq.port;
    m_BatchSubmTimeout = m_JobReq.timeout;
    if (!m_JobReq.param3.empty())
        m_BatchClientIP = m_JobReq.param3;
    else
        NS_FormatIPAddress(m_PeerAddr, m_BatchClientIP);
    m_BatchClientSID   = m_JobReq.param2;
    WriteMessage("OK:Batch submit ready");
    m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgBatchHeader;
}


void CNetScheduleHandler::x_ProcessBatchStart(CQueue*)
{
    m_BatchPos = 0;
    m_BatchStopWatch.Restart();
    m_BatchJobs.resize(m_BatchSize);
    if (m_BatchSize)
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgBatchJob;
    else
        // Unfortunately, because batches can be generated by
        // client program, we better honor zero size ones.
        // Skip right to waiting for 'ENDB'.
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgBatchSubmit;
}


void CNetScheduleHandler::x_ProcessBatchSequenceEnd(CQueue*)
{
    m_BatchJobs.clear();
    m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
    WriteMessage("OK:");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessCancel(CQueue* q)
{
    switch (q->Cancel(m_JobReq.job_id, m_PeerAddr)) {
        case CNetScheduleAPI::eJobNotFound:
            WriteMessage("OK:WARNING:Job not found;");
            break;
        case CNetScheduleAPI::eCanceled:
            WriteMessage("OK:WARNING:Already canceled;");
            break;
        default:
            WriteMessage("OK:");
    }

    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessStatus(CQueue* q)
{
    TJobStatus  status = q->GetJobStatus(m_JobReq.job_id);
    int         ret_code = 0;
    string      input, output, error;

    bool        res = q->GetJobDescr(m_JobReq.job_id, &ret_code,
                                     &input, &output,
                                     &error, 0, status);
    if (!res) {
        // Here: there is no such a job
        WriteMessage("OK:",
                     NStr::IntToString((int) CNetScheduleAPI::eJobNotFound));
        if (m_Server->IsLog())
            GetDiagContext().Extra()
                .Print("job_status",
                       CNetScheduleAPI::StatusToString(
                                            CNetScheduleAPI::eJobNotFound));
        x_PrintRequestStop(eStatus_OK);
        return;
    }

    // Here: the job was found
    WriteMessage("OK:", NStr::IntToString((int) status) +
                        " " + NStr::IntToString(ret_code) +
                        " \""   + NStr::PrintableString(output) +
                        "\" \"" + NStr::PrintableString(error) +
                        "\" \"" + NStr::PrintableString(input) +
                        "\"");

    if (m_Server->IsLog())
        GetDiagContext().Extra()
                .Print("job_status", CNetScheduleAPI::StatusToString(status))
                .Print("job_ret_code", ret_code)
                .Print("job_output", NStr::PrintableString(output))
                .Print("job_error", NStr::PrintableString(error))
                .Print("job_input", NStr::PrintableString(input));
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessGetJob(CQueue* q)
{
    list<string>    aff_list;
    NStr::Split(NStr::ParseEscapes(m_JobReq.affinity_token),
                "\t,", aff_list, NStr::eNoMergeDelims);

    CJob            job;
    q->PutResultGetJob(m_WorkerNode, m_PeerAddr, 0, 0, 0, &aff_list, &job);

    if (job.GetId())
        WriteMessage("OK:", x_FormGetJobResponse(q, job));
    else
        WriteMessage("OK:");

    // We don't call UnRegisterNotificationListener here
    // because for old worker nodes it finishes the session
    // and cleans up all the information, affinity association
    // included.
    if (m_JobReq.port)  // unregister notification
        q->GetWorkerNodeList().RegisterNotificationListener(m_WorkerNode, 0);
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessWaitGet(CQueue* q)
{
    list<string>        aff_list;
    NStr::Split(NStr::ParseEscapes(m_JobReq.affinity_token),
                "\t,", aff_list, NStr::eNoMergeDelims);

    CJob                job;
    q->PutResultGetJob(m_WorkerNode, m_PeerAddr, 0,0,0, &aff_list, &job);

    if (job.GetId()) {
        WriteMessage("OK:", x_FormGetJobResponse(q, job));
        x_PrintRequestStop(eStatus_OK);
        return;
    }

    // job not found, initiate waiting mode
    WriteMessage("OK:");
    x_PrintRequestStop(eStatus_OK);

    q->GetWorkerNodeList().
        RegisterNotificationListener(m_WorkerNode, m_JobReq.timeout);
}


void CNetScheduleHandler::x_ProcessPut(CQueue* q)
{
    string      output = NStr::ParseEscapes(m_JobReq.output);
    q->PutResultGetJob(m_WorkerNode, m_PeerAddr, m_JobReq.job_id,
                       m_JobReq.job_return_code, &output, 0, 0);
    WriteMessage("OK:");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessJobExchange(CQueue* q)
{
    list<string>        aff_list;
    NStr::Split(NStr::ParseEscapes(m_JobReq.affinity_token),
                "\t,", aff_list, NStr::eNoMergeDelims);

    CJob                job;
    string output = NStr::ParseEscapes(m_JobReq.output);
    q->PutResultGetJob(m_WorkerNode, m_PeerAddr, m_JobReq.job_id,
                       m_JobReq.job_return_code, &output,
                       // GetJob params
                       &aff_list, &job);

    if (job.GetId())
        WriteMessage("OK:", x_FormGetJobResponse(q, job));
    else
        WriteMessage("OK:");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessPutMessage(CQueue* q)
{
    if (q->PutProgressMessage(m_JobReq.job_id, m_JobReq.param1)) {
        WriteMessage("OK:");
        x_PrintRequestStop(eStatus_OK);
    }
    else {
        WriteMessage("ERR:Job not found");
        x_PrintRequestStop(eStatus_NotFound);
    }
}


void CNetScheduleHandler::x_ProcessGetMessage(CQueue* q)
{
    string      progress_msg;
    if (q->GetJobDescr(m_JobReq.job_id, 0, 0, 0, 0, &progress_msg)) {
        WriteMessage("OK:", progress_msg);
        x_PrintRequestStop(eStatus_OK);
    }
    else {
        WriteMessage("ERR:Job not found");
        x_PrintRequestStop(eStatus_NotFound);
    }
}


void CNetScheduleHandler::x_ProcessPutFailure(CQueue* q)
{
    q->FailJob(m_WorkerNode, m_PeerAddr, m_JobReq.job_id,
               NStr::ParseEscapes(m_JobReq.err_msg),
               NStr::ParseEscapes(m_JobReq.output),
               m_JobReq.job_return_code);
    WriteMessage("OK:");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessDropQueue(CQueue* q)
{
    q->Truncate();
    WriteMessage("OK:");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessReturn(CQueue* q)
{
    if (q->ReturnJob(m_JobReq.job_id, m_PeerAddr) == STemporaryEventCodes::eReturned)
        WriteMessage("OK:");
    else
        WriteMessage("ERR:Job not found");

    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessJobDelayExpiration(CQueue* q)
{
    q->JobDelayExpiration(m_WorkerNode, m_JobReq.job_id, m_JobReq.timeout);
    WriteMessage("OK:");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessDropJob(CQueue* q)
{
    q->EraseJob(m_JobReq.job_id);
    WriteMessage("OK:");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessStatistics(CQueue* q)
{
    CSocket &           socket = GetSocket();
    const string &      what   = m_JobReq.param1;
    time_t              curr   = time(0);

    socket.DisableOSSendDelay(false);
    if (!what.empty() && what != "ALL") {
        x_StatisticsNew(q, what, curr);
        return;
    }

    WriteMessage("OK:Started: ", m_Server->GetStartTime().AsString());

    WriteMessage("OK:",
                 "Load: jobs dispatched: " +
                 NStr::DoubleToString(q->GetAverage(CQueue::eStatGetEvent)) +
                 "/sec, jobs complete: " +
                 NStr::DoubleToString(q->GetAverage(CQueue::eStatPutEvent)) +
                 "/sec, DB locks: " +
                 NStr::DoubleToString(q->GetAverage(CQueue::eStatDBLockEvent)) +
                 "/sec, Job DB writes: " +
                 NStr::DoubleToString(q->GetAverage(CQueue::eStatDBWriteEvent)) +
                 "/sec");

    for (int i = CNetScheduleAPI::ePending;
         i < CNetScheduleAPI::eLastStatus; ++i) {
        TJobStatus      st = TJobStatus(i);
        unsigned        count = q->CountStatus(st);

        WriteMessage("OK:", CNetScheduleAPI::StatusToString(st) +
                            ": " + NStr::UIntToString(count));

        if (m_JobReq.param1 == "ALL") {
            TNSBitVector::statistics bv_stat;
            q->StatusStatistics(st, &bv_stat);
            WriteMessage("OK:",
                         "  bit_blk=" + NStr::UIntToString(bv_stat.bit_blocks) +
                         "; gap_blk=" + NStr::UIntToString(bv_stat.gap_blocks) +
                         "; mem_used=" + NStr::UIntToString(bv_stat.memory_used));
        }
    } // for


    if (m_JobReq.param1 == "ALL") {
        WriteMessage("OK:[Berkeley DB Mutexes]:");
        {{
            CNcbiOstrstream ostr;

            try {
                m_Server->PrintMutexStat(ostr);
                ostr << ends;
                WriteMessage("OK:", ostr.str());
            } catch (...) {
                ostr.freeze(false);
                throw;
            }
            ostr.freeze(false);
        }}

        WriteMessage("OK:[Berkeley DB Locks]:");
        {{
            CNcbiOstrstream ostr;

            try {
                m_Server->PrintLockStat(ostr);
                ostr << ends;
                WriteMessage("OK:", ostr.str());
            } catch (...) {
                ostr.freeze(false);
                throw;
            }
            ostr.freeze(false);
        }}

        WriteMessage("OK:[Berkeley DB Memory Usage]:");
        {{
            CNcbiOstrstream ostr;

            try {
                m_Server->PrintMemStat(ostr);
                ostr << ends;
                WriteMessage("OK:", ostr.str());
            } catch (...) {
                ostr.freeze(false);
                throw;
            }
            ostr.freeze(false);
        }}

        WriteMessage("OK:[BitVector block pool]:");
        {{
            const TBlockAlloc::TBucketPool::TBucketVector& bv =
                TBlockAlloc::GetPoolVector();
            size_t      pool_vec_size = bv.size();

            WriteMessage("OK:Pool vector size: ",
                         NStr::UIntToString(pool_vec_size));

            for (size_t  i = 0; i < pool_vec_size; ++i) {
                const TBlockAlloc::TBucketPool::TResourcePool* rp =
                    TBlockAlloc::GetPool(i);
                if (rp) {
                    size_t pool_size = rp->GetSize();
                    if (pool_size) {
                        WriteMessage("OK:",
                                     "Pool [ " + NStr::UIntToString(i) +
                                     "] = " + NStr::UIntToString(pool_size));
                    }
                }
            }
        }}
    }

    WriteMessage("OK:[Worker node statistics]:");
    q->PrintWorkerNodeStat(*this, curr);

    WriteMessage("OK:[Configured job submitters]:");
    q->PrintSubmHosts(*this);

    WriteMessage("OK:[Configured workers]:");
    q->PrintWNodeHosts(*this);

    WriteMessage("OK:END");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessStatusSnapshot(CQueue* q)
{
    CJobStatusTracker::TStatusSummaryMap st_map;
    bool    aff_exists = q->CountStatus(&st_map,
                                NStr::ParseEscapes(m_JobReq.affinity_token));

    if (!aff_exists) {
        WriteMessage("ERR:", "eProtocolSyntaxError:"
                             "Unknown affinity token \"" +
                             m_JobReq.affinity_token + "\"");
        x_PrintRequestStop(eStatus_NotFound);
        return;
    }
    ITERATE(CJobStatusTracker::TStatusSummaryMap, it, st_map) {
        WriteMessage("OK:", CNetScheduleAPI::StatusToString(it->first) +
                            " " + NStr::UIntToString(it->second));
    }
    WriteMessage("OK:END");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessReloadConfig(CQueue* q)
{
    CNcbiApplication* app = CNcbiApplication::Instance();

    bool                    reloaded = app->ReloadConfig(CMetaRegistry::fReloadIfChanged);
    const CNcbiRegistry &   reg = app->GetConfig();
    if (reloaded)
        m_Server->Configure(reg);

    // Logging from the [server] section
    SNS_Parameters          params;

    params.Read(reg, "server");
    m_Server->SetNSParameters(params, true);

    WriteMessage("OK:");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessActiveCount(CQueue* q)
{
    string      active_jobs = NStr::UIntToString(m_Server->CountActiveJobs());

    WriteMessage("OK:", active_jobs);
    if (m_Server->IsLog())
        GetDiagContext().Extra().Print("active_jobs", active_jobs);
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessDump(CQueue* q)
{
    if (m_JobReq.job_id == 0) {
        // The whole queue dump
        q->PrintAllJobDbStat(*this);
        WriteMessage("OK:END");
        x_PrintRequestStop(eStatus_OK);
        return;
    }


    // Certain job dump
    if (q->PrintJobDbStat(*this, m_JobReq.job_id) == 0) {
        // Nothing was printed because there is no such a job
        WriteMessage("ERR:Job not found");
        x_PrintRequestStop(eStatus_NotFound);
        return;
    }

    WriteMessage("OK:END");
    x_PrintRequestStop(eStatus_OK);
    return;
}


void CNetScheduleHandler::x_ProcessPrintQueue(CQueue* q)
{
    TJobStatus job_status = CNetScheduleAPI::StringToStatus(m_JobReq.param1);

    if (job_status == CNetScheduleAPI::eJobNotFound) {
        WriteMessage("ERR:Status unknown: ", m_JobReq.param1);
        x_PrintRequestStop(eStatus_BadCmd);
        return;
    }

    q->PrintQueue(*this, job_status);
    WriteMessage("OK:END");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessShutdown(CQueue*)
{
    x_PrintRequestStop(eStatus_OK);
    WriteMessage("OK:");
    m_Server->SetShutdownFlag();
}


void CNetScheduleHandler::x_ProcessVersion(CQueue*)
{
    WriteMessage("OK:" NETSCHEDULED_FULL_VERSION);
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessRegisterClient(CQueue* q)
{
    q->GetWorkerNodeList().
        RegisterNotificationListener(m_WorkerNode, 0);
    WriteMessage("OK:");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessUnRegisterClient(CQueue* q)
{
    q->UnRegisterNotificationListener(m_WorkerNode, m_PeerAddr);
    WriteMessage("OK:");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessQList(CQueue*)
{
    WriteMessage("OK:", m_Server->GetQueueNames(";"));
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessQuitSession(CQueue*)
{
    // read trailing input
    CSocket& socket = GetSocket();
    STimeout to; to.sec = to.usec = 0;
    socket.SetTimeout(eIO_Read, &to);
    socket.Read(NULL, 1024);
    m_Server->CloseConnection(&socket);

    // Log the close request
    CDiagContext::SetRequestContext(m_DiagContext);
    x_PrintRequestStop(eStatus_OK);
    CDiagContext::SetRequestContext(NULL);
}


void CNetScheduleHandler::x_ProcessCreateQueue(CQueue*)
{
    m_Server->CreateQueue(m_JobReq.param1,                          // qname
                          m_JobReq.param2,                          // qclass
                          NStr::ParseEscapes(m_JobReq.param3));     // comment
    WriteMessage("OK:");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessDeleteQueue(CQueue*)
{
    m_Server->DeleteQueue(m_JobReq.param1);     // qname
    WriteMessage("OK:");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessQueueInfo(CQueue*)
{
    const string&   qname = m_JobReq.param1;
    int             kind;
    string          qclass;
    string          comment;

    m_Server->QueueInfo(qname, kind, &qclass, &comment);
    WriteMessage("OK:", NStr::IntToString(kind) + "\t" + qclass + "\t\"" +
                        NStr::PrintableString(comment) + "\"");
    if (m_Server->IsLog())
        GetDiagContext().Extra()
                .Print("queue_kind", kind)
                .Print("queue_class", qclass)
                .Print("queue_comment", NStr::PrintableString(comment));
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessQuery(CQueue* q)
{
    string          result_str;
    string          query = NStr::ParseEscapes(m_JobReq.param1);
    string &        action = m_JobReq.param2;
    list<string>    fields;

    m_SelectedIds.reset(q->ExecSelect(query, fields));

    if (!fields.empty()) {
        WriteMessage("ERR:eProtocolSyntaxError:SELECT is not allowed");
        m_SelectedIds.release();
        x_PrintRequestStop(eStatus_BadCmd);
        return;
    }

    if (action == "SLCT") {
        // Execute 'projection' phase
        string          str_fields(NStr::ParseEscapes(m_JobReq.param3));
        // Split fields
        NStr::Split(str_fields, "\t,", fields, NStr::eNoMergeDelims);
        q->PrepareFields(m_FieldDescr, fields);
        m_DelayedOutput = &CNetScheduleHandler::x_WriteProjection;
        x_PrintRequestStop(eStatus_OK);
        return;
    }

    if (action == "COUNT") {
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
        WriteMessage("ERR:eProtocolSyntaxError:Unknown action ", action);
        m_SelectedIds.release();
        x_PrintRequestStop(eStatus_BadCmd);
        return;
    }
    WriteMessage("OK:", result_str);
    m_SelectedIds.release();
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessSelectQuery(CQueue* q)
{
    string          result_str;
    string          query = NStr::ParseEscapes(m_JobReq.param1);
    list<string>    fields;

    m_SelectedIds.reset(q->ExecSelect(query, fields));

    if (fields.empty()) {
        WriteMessage("ERR:eProtocolSyntaxError:"
                     "SQL-like select is expected");
        m_SelectedIds.release();
        x_PrintRequestStop(eStatus_BadCmd);
    } else {
        q->PrepareFields(m_FieldDescr, fields);
        m_DelayedOutput = &CNetScheduleHandler::x_WriteProjection;
        x_PrintRequestStop(eStatus_OK);
    }
}


void CNetScheduleHandler::x_ProcessGetParam(CQueue* q)
{
    string  max_input_size = NStr::IntToString(q->GetMaxInputSize());
    string  max_output_size = NStr::IntToString(q->GetMaxOutputSize());

    string res = "max_input_size=" + max_input_size + ";"
                 "max_output_size=" + max_output_size + ";" +
                 NETSCHEDULED_FEATURES;
    if (m_Server->IsLog())
        GetDiagContext().Extra().Print("max_input_size", max_input_size)
                                .Print("max_output_size", max_output_size)
                                .Print("ns_features", NETSCHEDULED_FEATURES);

    if (!m_JobReq.param1.empty())
        res += ";" + m_JobReq.param1;

    WriteMessage("OK:", res);
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessGetConfiguration(CQueue* q)
{
    CQueue::TParameterList      parameters = q->GetParameters();

    ITERATE(CQueue::TParameterList, it, parameters) {
        WriteMessage("OK:", it->first + '=' + it->second);
        if (m_Server->IsLog())
            GetDiagContext().Extra().Print(it->first, it->second);
    }
    WriteMessage("OK:END");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessReading(CQueue* q)
{
    unsigned        read_id;
    TNSBitVector    jobs;

    q->ReadJobs(m_PeerAddr, m_JobReq.count, m_JobReq.timeout, read_id, jobs);
    WriteMessage("OK:", NStr::UIntToString(read_id) +
                          " " + NS_EncodeBitVector(jobs));

    if (m_Server->IsLog()) {
        GetDiagContext().Extra().Print("group_id", read_id);

        list<string>                    job_keys = BitVectorToJobKeys(q, jobs);
        list<string>::const_iterator    j = job_keys.begin();
        for (; j != job_keys.end(); ++j)
            GetDiagContext().Extra().Print("job_key", *j);
    }
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessConfirm(CQueue* q)
{
    TNSBitVector    jobs = NS_DecodeBitVector(m_JobReq.output);
    q->ConfirmJobs(m_JobReq.count, jobs, m_PeerAddr);
    WriteMessage("OK:");

    if (m_Server->IsLog()) {
        GetDiagContext().Extra().Print("group_id", m_JobReq.count);

        list<string>                    job_keys = BitVectorToJobKeys(q, jobs);
        list<string>::const_iterator    j = job_keys.begin();
        for (; j != job_keys.end(); ++j)
            GetDiagContext().Extra().Print("job_key", *j);
    }
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessReadFailed(CQueue* q)
{
    TNSBitVector        jobs = NS_DecodeBitVector(m_JobReq.output);
//    m_JobReq.err_msg; we still don't (and probably, won't) use this
    q->FailReadingJobs(m_JobReq.count, jobs, m_PeerAddr);
    WriteMessage("OK:");

    if (m_Server->IsLog()) {
        GetDiagContext().Extra().Print("group_id", m_JobReq.count);

        list<string>                    job_keys = BitVectorToJobKeys(q, jobs);
        list<string>::const_iterator    j = job_keys.begin();
        for (; j != job_keys.end(); ++j)
            GetDiagContext().Extra().Print("job_key", *j);
    }
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessReadRollback(CQueue* q)
{
    TNSBitVector        jobs = NS_DecodeBitVector(m_JobReq.output);
    q->ReturnReadingJobs(m_JobReq.count, jobs, m_PeerAddr);
    WriteMessage("OK:");

    if (m_Server->IsLog()) {
        GetDiagContext().Extra().Print("group_id", m_JobReq.count);

        list<string>                    job_keys = BitVectorToJobKeys(q, jobs);
        list<string>::const_iterator    j = job_keys.begin();
        for (; j != job_keys.end(); ++j)
            GetDiagContext().Extra().Print("job_key", *j);
    }
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessGetAffinityList(CQueue* q)
{
    WriteMessage("OK:", q->GetAffinityList());
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessInitWorkerNode(CQueue* q)
{
    string  old_id = m_WorkerNode->GetId();
    string  new_id = m_JobReq.param1.substr(0, kMaxWorkerNodeIdSize);

    if (old_id != new_id) {
        if (!old_id.empty())
            q->ClearWorkerNode(m_WorkerNode, m_PeerAddr, "replaced by new node");

        q->GetWorkerNodeList().SetId(m_WorkerNode, new_id);
    }

    WriteMessage("OK:");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessClearWorkerNode(CQueue* q)
{
    q->ClearWorkerNode(m_JobReq.param1.substr(0, kMaxWorkerNodeIdSize),
                       m_PeerAddr, "cleared");

    WriteMessage("OK:");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_CmdNotImplemented(CQueue*)
{
    WriteMessage("ERR:Not implemented");
    x_PrintRequestStop(eStatus_NoImpl);
}


void CNetScheduleHandler::x_WriteProjection()
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
        SQueueDescription       qdesc;
        CQueue::TRecordSet      record_set;

        qdesc.host = m_Server->GetHost().c_str();
        qdesc.port = m_Server->GetPort();
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
            const CQueue::TRecord&      rec = *it;
            string                      str_rec;
            for (unsigned i = 0; i < rec.size(); ++i) {
                if (i)
                    str_rec += '\t';

                const string&   field = rec[i];
                if (m_FieldDescr.formatters[i])
                    str_rec += m_FieldDescr.formatters[i](field, &qdesc);
                else
                    str_rec += field;
            }
            WriteMessage("OK:", str_rec);
        }
    }
    if (!m_SelectedIds->any()) {
        WriteMessage("OK:END");
        m_SelectedIds.release();
        m_DelayedOutput = NULL;
    }
}


void CNetScheduleHandler::x_ParseTags(const string &  strtags,
                                      TNSTagList &    tags)
{
    list<string>        tokens;

    NStr::Split(NStr::ParseEscapes(strtags), "\t", tokens,
                NStr::eNoMergeDelims);
    tags.clear();
    for (list<string>::const_iterator  it = tokens.begin();
         it != tokens.end(); ++it) {
        string      key(*it);
        ++it;
        if (it == tokens.end()) {
            tags.push_back(TNSTag(key, kEmptyStr));
            break;
        }
        else
            tags.push_back(TNSTag(key, *it));
    }
}


string CNetScheduleHandler::x_SerializeBitVector(TNSBitVector &  bv)
{
    TNSBitVector::statistics    st;
    bv.optimize(0, TNSBitVector::opt_compress, &st);
    size_t dst_size = st.max_serialize_mem * 4 / 3 + 1;
    size_t src_size = st.max_serialize_mem;
    unsigned char* buf = new unsigned char[src_size + dst_size];
    size_t size = bm::serialize(bv, buf);
    size_t src_read, dst_written, line_len;
    line_len = 0; // no line feeds in encoded value
    unsigned char* encode_buf = buf+src_size;
    BASE64_Encode(buf, size, &src_read, encode_buf,
                  dst_size, &dst_written, &line_len);
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


bool CNetScheduleHandler::x_CheckVersion()
{
    if (m_AuthProgName.empty())
        return false;

    try {
        CQueueClientInfo    auth_prog_info;
        ParseVersionString(m_AuthProgName,
                           &auth_prog_info.client_name,
                           &auth_prog_info.version_info);
        return GetQueue()->IsMatchingClient(auth_prog_info);
    }
    catch (exception&) {
        return false;
    }
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
                     GetSocket().GetPeerAddress() + " " + m_AuthClientName;
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


void CNetScheduleHandler::x_PrintRequestStart(const SParsedCmd& cmd)
{
    if (m_Server->IsLog()) {
        CDiagContext_Extra    ctxt_extra = GetDiagContext().PrintRequestStart();
        ctxt_extra.Print("_type", "cmd");
        ctxt_extra.Print("cmd", cmd.command->cmd);
        ctxt_extra.Print("peer", GetSocket().GetPeerAddress(eSAF_IP));
        ctxt_extra.Print("conn", m_ConnReqId);

        // SUMBIT parameters should not be logged. The new job attributes will
        // be logged when a new job is actually submitted.
        if (strcmp(cmd.command->cmd, "SUBMIT") != 0 ) {
            ITERATE(TNSProtoParams, it, cmd.params) {
                ctxt_extra.Print(it->first, it->second);
            }
        }
    }
    m_DiagContext->SetRequestStatus(eStatus_OK);
}


void CNetScheduleHandler::x_PrintRequestStart(CTempString  msg)
{
    if (m_Server->IsLog())
        GetDiagContext().PrintRequestStart()
                .Print("_type", "cmd")
                .Print("info", msg);
}


void CNetScheduleHandler::x_PrintRequestStop(EHTTPStatus  status)
{
    if (m_Server->IsLog()) {
        CDiagContext::GetRequestContext().SetRequestStatus(status);
        GetDiagContext().PrintRequestStop();
        CDiagContext::SetRequestContext(m_ConnContext);
        m_DiagContext.Reset();
    }
}


// The function forms a responce for various 'get job' commands and prints
// extra to the log if required
string  CNetScheduleHandler::x_FormGetJobResponse(const CQueue *   q,
                                                  const CJob &     job) const
{
    string      job_key = q->MakeKey(job.GetId());

    if (m_Server->IsLog()) {
        // The only piece required for logging is the job key
        GetDiagContext().Extra().Print("job_key", job_key);
    }

    // We can re-use old jout and jerr job parameters for affinity and
    // session id/client ip respectively.
    return job_key +
           " \"" + NStr::PrintableString(job.GetInput()) + "\""
           " \"" + NStr::PrintableString(job.GetAffinityToken()) + "\""
           " \"" + job.GetClientIP() + " " + job.GetClientSID() + "\""
           " " + NStr::UIntToString(job.GetMask());
}


void CNetScheduleHandler::x_StatisticsNew(CQueue*       q,
                                          const string& what,
                                          time_t        curr)
{
    if (what == "WNODE") {
        q->PrintWorkerNodeStat(*this, curr, eWNF_WithNodeId);
    }
    if (what == "JOBS") {
        unsigned total = 0;
        for (int i = CNetScheduleAPI::ePending;
            i < CNetScheduleAPI::eLastStatus; ++i) {
                TJobStatus      st = TJobStatus(i);
                unsigned        count = q->CountStatus(st);

                total += count;
                WriteMessage("OK:", CNetScheduleAPI::StatusToString(st) +
                                    ": " + NStr::UIntToString(count));
        } // for
        WriteMessage("OK:Total: ", NStr::UIntToString(total));
    }
    WriteMessage("OK:END");
    x_PrintRequestStop(eStatus_OK);
}

