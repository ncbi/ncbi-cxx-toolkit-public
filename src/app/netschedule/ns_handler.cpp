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
    { "aff",   eNSPT_Str, eNSPA_Optional, "" },
    { "msk",   eNSPT_Int, eNSPA_Optional, "0" },
    { NULL }
};

CNetScheduleHandler::SCommandMap CNetScheduleHandler::sm_CommandMap[] = {

    /*** Admin role ***/
    { "SHUTDOWN", { &CNetScheduleHandler::x_ProcessShutdown,
                    eNSCR_Admin } },
    { "GETCONF",  { &CNetScheduleHandler::x_ProcessGetConf,
                    eNSCR_Admin } },

    /*** Any role ***/
    { "VERSION",  { &CNetScheduleHandler::x_ProcessVersion,
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
        { { "option",  eNSPT_Id, eNSPA_Optional },
          { "comment", eNSPT_Id, eNSPA_Optional } } },
    // MPUT job_key : id  progress_msg : str
    { "MPUT",     { &CNetScheduleHandler::x_ProcessPutMessage,
                    eNSCR_Queue },
        { { "job_key",      eNSPT_Id, eNSPA_Required },
          { "progress_msg", eNSPT_Str, eNSPA_Required } } },
    // MGET job_key : id
    { "MGET",     { &CNetScheduleHandler::x_ProcessGetMessage,
                    eNSCR_Queue },
        { { "job_key", eNSPT_Id, eNSPA_Required } } },
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
    // GETP [ client_info : id ]
    { "GETP",     { &CNetScheduleHandler::x_ProcessGetParam,
                    eNSCR_Queue } },
    { "GETC",     { &CNetScheduleHandler::x_ProcessGetConfiguration,
                    eNSCR_Queue } },
    // CLRN (obsolete)
    { "CLRN",     { &CNetScheduleHandler::x_ProcessClearWorkerNode,
                    eNSCR_Queue } },
    { "CANCELQ",  { &CNetScheduleHandler::x_ProcessCancelQueue,
                    eNSCR_Queue } },

    /*** Submitter role ***/
    // SST job_key : id -- submitter fast status, changes timestamp
    { "SST",      { &CNetScheduleHandler::x_ProcessFastStatusS,
                    eNSCR_Submitter },
        { { "job_key", eNSPT_Id, eNSPA_Required } } },
    // SUBMIT input : str [ progress_msg : str ] [ port : uint [ timeout : uint ]]
    //        [ affinity_token : keystr(aff) ] [ job_mask : keyint(msk) ]
    { "SUBMIT",   { &CNetScheduleHandler::x_ProcessSubmit,
                    eNSCR_Submitter },
        { { "input",        eNSPT_Str, eNSPA_Required },        // input
          { "progress_msg", eNSPT_Str, eNSPA_Optional },
          { "port",         eNSPT_Int, eNSPA_Optional },
          { "timeout",      eNSPT_Int, eNSPA_Optional },
          { "aff",          eNSPT_Str, eNSPA_Optional, "" },    // affinity_token
          { "msk",          eNSPT_Int, eNSPA_Optional, "0" },
          { "ip",           eNSPT_Str, eNSPA_Optional, "" },
          { "sid",          eNSPT_Str, eNSPA_Optional, "" } } },
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
    // READ [ timeout : int ] -> job key
    { "READ",     { &CNetScheduleHandler::x_ProcessReading,
                    eNSCR_Submitter },
        { { "timeout", eNSPT_Int, eNSPA_Optional, "0" } } },
    // CFRM group : int jobs : str
    { "CFRM",     { &CNetScheduleHandler::x_ProcessConfirm,
                    eNSCR_Submitter },
        { { "job_key",    eNSPT_Id, eNSPA_Required },
          { "auth_token", eNSPT_Id, eNSPA_Required } } },
    // FRED group : int jobs : str [ message : str ]
    { "FRED",     { &CNetScheduleHandler::x_ProcessReadFailed,
                    eNSCR_Submitter },
        { { "job_key",    eNSPT_Id, eNSPA_Required },
          { "auth_token", eNSPT_Id, eNSPA_Required },
          { "err_msg",      eNSPT_Str, eNSPA_Optional } } },
    // RDRB group : int jobs : str
    { "RDRB",     { &CNetScheduleHandler::x_ProcessReadRollback,
                    eNSCR_Submitter },
        { { "job_key",    eNSPT_Id, eNSPA_Required },
          { "auth_token", eNSPT_Str, eNSPA_Required } } },

    /*** Worker node role ***/
    // WST job_key : id -- worker node fast status, does not change timestamp
    { "WST",      { &CNetScheduleHandler::x_ProcessFastStatusW,
                    eNSCR_Worker },
        { { "job_key", eNSPT_Id, eNSPA_Required } } },
    // CHAFF [add: string] [del: string] -- change affinity
    { "CHAFF",    { &CNetScheduleHandler::x_ProcessChangeAffinity,
                    eNSCR_Worker },
        { { "add", eNSPT_Str, eNSPA_Optional, "" },
          { "del", eNSPT_Str, eNSPA_Optional, "" } } },
    // GET [ port : int ] [affinity_list : keystr(aff) ]
    { "GET",      { &CNetScheduleHandler::x_ProcessGetJob,
                    eNSCR_Worker },
        { { "port",      eNSPT_Id,  eNSPA_Optional },
          { "aff",       eNSPT_Str, eNSPA_Optional, "" },
          { "any_aff",   eNSPT_Int, eNSPA_Optional, 0 },
          { "wnode_aff", eNSPT_Int, eNSPA_Optional, 0 } } },
    { "GET2",     { &CNetScheduleHandler::x_ProcessGetJob,
                    eNSCR_Worker },
        { { "port",      eNSPT_Id,  eNSPA_Optional },
          { "aff",       eNSPT_Str, eNSPA_Optional, "" },
          { "any_aff",   eNSPT_Int, eNSPA_Optional, 0 },
          { "wnode_aff", eNSPT_Int, eNSPA_Optional, 0 } } },
    // PUT job_key : id  job_return_code : int  output : str
    { "PUT",      { &CNetScheduleHandler::x_ProcessPut,
                    eNSCR_Worker },
        { { "job_key",         eNSPT_Id,  eNSPA_Required },
          { "job_return_code", eNSPT_Id,  eNSPA_Required },
          { "output",          eNSPT_Str, eNSPA_Required } } },
    { "PUT2",     { &CNetScheduleHandler::x_ProcessPut,
                    eNSCR_Worker },
        { { "job_key",         eNSPT_Id,  eNSPA_Required },
          { "auth_token",      eNSPT_Id,  eNSPA_Required },
          { "job_return_code", eNSPT_Id,  eNSPA_Required },
          { "output",          eNSPT_Str, eNSPA_Required } } },
    // RETURN job_key : id
    { "RETURN",   { &CNetScheduleHandler::x_ProcessReturn,
                    eNSCR_Worker },
        { { "job_key", eNSPT_Id, eNSPA_Required } } },
    { "RETURN2",  { &CNetScheduleHandler::x_ProcessReturn,
                    eNSCR_Worker },
        { { "job_key",         eNSPT_Id,  eNSPA_Required },
          { "auth_token",      eNSPT_Id,  eNSPA_Required } } },
    // WGET port : uint  timeout : uint
    //      [affinity_list : keystr(aff) ]
    { "WGET",     { &CNetScheduleHandler::x_ProcessWaitGet,
                    eNSCR_Worker },
        { { "port",      eNSPT_Int, eNSPA_Required },
          { "timeout",   eNSPT_Int, eNSPA_Required },
          { "aff",       eNSPT_Str, eNSPA_Optional, "" },
          { "any_aff",   eNSPT_Int, eNSPA_Optional, 0 },
          { "wnode_aff", eNSPT_Int, eNSPA_Optional, 0 } } },
    { "WGET2",    { &CNetScheduleHandler::x_ProcessWaitGet,
                    eNSCR_Worker },
        { { "port",      eNSPT_Int, eNSPA_Required },
          { "timeout",   eNSPT_Int, eNSPA_Required },
          { "aff",       eNSPT_Str, eNSPA_Optional, "" },
          { "any_aff",   eNSPT_Int, eNSPA_Optional, 0 },
          { "wnode_aff", eNSPT_Int, eNSPA_Optional, 0 } } },
    { "CWGET",    { &CNetScheduleHandler::x_ProcessCancelWaitGet,
                    eNSCR_Worker } },
    // FPUT job_key : id  err_msg : str  output : str  job_return_code : int
    { "FPUT",     { &CNetScheduleHandler::x_ProcessPutFailure,
                    eNSCR_Worker },
        { { "job_key",         eNSPT_Id,  eNSPA_Required },
          { "err_msg",         eNSPT_Str, eNSPA_Required },
          { "output",          eNSPT_Str, eNSPA_Required },
          { "job_return_code", eNSPT_Int, eNSPA_Required } } },
    { "FPUT2",    { &CNetScheduleHandler::x_ProcessPutFailure,
                    eNSCR_Worker },
        { { "job_key",         eNSPT_Id,  eNSPA_Required },
          { "auth_token",      eNSPT_Id,  eNSPA_Required },
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
          { "aff",             eNSPT_Str, eNSPA_Optional, "" },
          { "any_aff",         eNSPT_Int, eNSPA_Optional, 0 },
          { "wnode_aff",       eNSPT_Int, eNSPA_Optional, 0 } } },
    { "JXCG2",    { &CNetScheduleHandler::x_ProcessJobExchange,
                    eNSCR_Worker },
        { { "job_key",         eNSPT_Id,  eNSPA_Required },
          { "auth_token",      eNSPT_Id,  eNSPA_Required },
          { "job_return_code", eNSPT_Int, eNSPA_Required },
          { "output",          eNSPT_Str, eNSPA_Required },
          { "aff",             eNSPT_Str, eNSPA_Optional, "" },
          { "any_aff",         eNSPT_Int, eNSPA_Optional, 0 },
          { "wnode_aff",       eNSPT_Int, eNSPA_Optional, 0 } } },
    // JDEX job_key : id timeout : uint
    { "JDEX",     { &CNetScheduleHandler::x_ProcessJobDelayExpiration,
                    eNSCR_Worker },
        { { "job_key", eNSPT_Id,  eNSPA_Required },
          { "timeout", eNSPT_Int, eNSPA_Required } } },
    // AFLS
    { "AFLS",     { &CNetScheduleHandler::x_ProcessGetAffinityList,
                    eNSCR_Worker } },

    // Obsolete commands
    { "REGC",     { &CNetScheduleHandler::x_ProcessRegisterClient,
                    eNSCR_Worker },
        { { "port", eNSPT_Int, eNSPA_Optional } } },
    { "URGC",     { &CNetScheduleHandler::x_ProcessUnRegisterClient,
                    eNSCR_Worker },
        { { "port", eNSPT_Int, eNSPA_Optional } } },
    { "INIT",     { &CNetScheduleHandler::x_ProcessInitWorkerNode,
                    eNSCR_Worker } },
    { "JRTO",     { &CNetScheduleHandler::x_CmdNotImplemented,
                    eNSCR_Worker } },
    { "FRES",     { &CNetScheduleHandler::x_CmdNotImplemented,
                    eNSCR_Submitter } },
    { "QERY",     { &CNetScheduleHandler::x_CmdNotImplemented,
                    eNSCR_Queue } },
    { "QSEL",     { &CNetScheduleHandler::x_CmdNotImplemented,
                    eNSCR_Queue } },
    { "MONI",     { &CNetScheduleHandler::x_CmdNotImplemented,
                    eNSCR_Queue } },
    { "LOG",      { &CNetScheduleHandler::x_CmdNotImplemented,
                    eNSCR_Any } },

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
      m_Server(server),
      m_SingleCmdParser(sm_CommandMap),
      m_BatchHeaderParser(sm_BatchHeaderMap),
      m_BatchEndParser(sm_BatchEndMap)
{}


CNetScheduleHandler::~CNetScheduleHandler()
{
    delete [] m_MsgBuffer;
}


void CNetScheduleHandler::OnOpen(void)
{
    CSocket &       socket = GetSocket();
    STimeout        to = {m_Server->GetInactivityTimeout(), 0};

    socket.DisableOSSendDelay();
    socket.SetTimeout(eIO_ReadWrite, &to);
    x_SetQuickAcknowledge();

    m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgAuth;

    // Log the fact of opened connection
    m_ConnContext.Reset(new CRequestContext());
    m_ConnReqId = NStr::UInt8ToString(m_ConnContext->SetRequestID());

    CDiagnosticsGuard   guard(this);
    if (m_Server->IsLog()) {
        GetDiagContext().PrintRequestStart()
                        .Print("_type", "conn")
                        .Print("peer", socket.GetPeerAddress(eSAF_IP))
                        .Print("pport", socket.GetPeerAddress(eSAF_Port))
                        .Print("port", socket.GetLocalPort(eNH_HostByteOrder))
                        .Flush();
    }
    m_ConnContext->SetRequestStatus(eStatus_OK);
}


void CNetScheduleHandler::OnWrite()
{}


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
    catch (const CNetScheduleException &  ex) {
        x_WriteMessageNoThrow("ERR:", string(ex.GetErrCodeString()) +
                                      ":" + ex.GetMsg());
        ERR_POST("Server error: " << ex);
        x_PrintRequestStop(eStatus_ServerError);
    }
    catch (const CNSProtoParserException &  ex) {
        x_WriteMessageNoThrow("ERR:", string(ex.GetErrCodeString()) +
                                      ":" + ex.GetMsg());
        ERR_POST("Command parser error: " << ex);
        x_PrintRequestStop(eStatus_ServerError);
    }
    catch (const CNetServiceException &  ex) {
        x_WriteMessageNoThrow("ERR:", string(ex.GetErrCodeString()) +
                                      ":" + ex.GetMsg());
        ERR_POST("Server error: " << ex);
        x_PrintRequestStop(eStatus_ServerError);
    }
    catch (const CBDB_ErrnoException &  ex) {
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
    catch (const CBDB_Exception &  ex) {
        string  error = NStr::PrintableString("eInternalError:Internal "
                                              "database (BDB) error - " +
                                              string(ex.what()));
        x_WriteMessageNoThrow("ERR:", error);
        ERR_POST("BDB error: " << ex);
        x_PrintRequestStop(eStatus_ServerError);
    }
    catch (const exception &  ex) {
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
                   eCommunicationError, "Socket write error; peer: " +
                                        GetSocket().GetPeerAddress());
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
                   eCommunicationError, "Socket write error; peer: " +
                                         GetSocket().GetPeerAddress());
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


unsigned int CNetScheduleHandler::x_GetPeerAddress(void)
{
    unsigned int        peer_addr;

    GetSocket().GetPeerAddress(&peer_addr, 0, eNH_NetworkByteOrder);

    // always use localhost(127.0*) address for clients coming from
    // the same net address (sometimes it can be 127.* or full address)
    if (peer_addr == m_Server->GetHostNetAddr())
        return CSocketAPI::GetLoopbackAddress();
    return peer_addr;
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

        x_CloseConnection();
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
    bool                parse_failed = false;

    try {
        m_SingleCmdParser.ParseArguments(m_RawAuthString, s_AuthArgs, &params);
    }
    catch (CNSProtoParserException &  ex) {
        ERR_POST("Error authenticating client: '" <<
                 m_RawAuthString << "': " << ex);
        parse_failed = true;
    }

    // Memorize what we know about the client
    try {
        m_ClientId.Update(this->x_GetPeerAddress(), params);
        if (parse_failed)
            m_ClientId.SetClientName(m_RawAuthString);
    }
    catch (const CNetScheduleException &  ex) {
        if (ex.GetErrCode() == CNetScheduleException::eAuthenticationError) {
            x_WriteMessageNoThrow("ERR:", string(ex.GetErrCodeString()) +
                                          ":" + ex.GetMsg());
            ERR_POST("Server error: " << ex);
            if (m_Server->IsLog()) {
                CDiagContext::GetRequestContext().SetRequestStatus(eStatus_BadAuth);
                GetDiagContext().PrintRequestStop();
                m_ConnContext.Reset();
            }
            x_CloseConnection();
            return;
        }
        throw;
    }


    if (m_Server->AdminHostValid(m_ClientId.GetAddress()) &&
        m_ClientId.IsAdminClientName())
    {
        // TODO: queue admin should be checked in ProcessMsgQueue,
        // when queue info is available
        m_ClientId.AddCapability(eNSAC_Admin | eNSAC_QueueAdmin);
    }

    // Produce the log output if required
    if (m_Server->IsLog()) {
        CDiagContext_Extra diag_extra = GetDiagContext().Extra();
        diag_extra.Print("queue", m_QueueName);
        ITERATE(TNSProtoParams, it, params) {
            diag_extra.Print(it->first, it->second);
        }
    }

    if (m_QueueName != "noname" && !m_QueueName.empty()) {
        try {
            m_QueueRef.Reset(m_Server->OpenQueue(m_QueueName));
        }
        catch (const CNetScheduleException &  ex) {
            if (ex.GetErrCode() == CNetScheduleException::eUnknownQueue) {
                x_WriteMessageNoThrow("ERR:", string(ex.GetErrCodeString()) +
                                              ":" + ex.GetMsg());
                ERR_POST("Server error: " << ex);
                if (m_Server->IsLog()) {
                    CDiagContext::GetRequestContext().SetRequestStatus(eStatus_BadQueue);
                    GetDiagContext().PrintRequestStop();
                    m_ConnContext.Reset();
                }
                x_CloseConnection();
                return;
            }
            throw;
        }

        m_ClientId.AddCapability(eNSAC_Queue);

        CRef<CQueue>    q(GetQueue());
        if (q->IsWorkerAllowed(m_ClientId.GetAddress()))
            m_ClientId.AddCapability(eNSAC_Worker);
        if (q->IsSubmitAllowed(m_ClientId.GetAddress()))
            m_ClientId.AddCapability(eNSAC_Submitter);
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
    catch (const CNSProtoParserException &  ex) {
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

    const SCommandExtra &   extra = cmd.command->extra;


    // It throws an exception if the input is not valid
    m_CommandArguments.AssignValues(cmd.params, cmd.command->cmd);


    if (extra.processor == &CNetScheduleHandler::x_ProcessQuitSession) {
        m_ClientId.CheckAccess(extra.role, NULL);
        x_ProcessQuitSession(0);
        return;
    }


    // If the command requires queue, hold a hard reference to this
    // queue from a weak one (m_Queue) in queue_ref, and take C pointer
    // into queue_ptr. Otherwise queue_ptr is NULL, which is OK for
    // commands which does not require a queue.
    CRef<CQueue>        queue_ref;
    CQueue *            queue_ptr = NULL;
    if (extra.role & eNSAC_Queue) {
        queue_ref.Reset(GetQueue());
        queue_ptr = queue_ref.GetPointer();
    }

    m_ClientId.CheckAccess(extra.role, queue_ptr);

    // we want status request to be fast, skip version control
    if ((extra.processor != &CNetScheduleHandler::x_ProcessStatus) &&
        (extra.processor != &CNetScheduleHandler::x_ProcessFastStatusS) &&
        (extra.processor != &CNetScheduleHandler::x_ProcessFastStatusW))
        if (!m_ClientId.CheckVersion(queue_ptr)) {
            WriteMessage("ERR:eInvalidClientOrVersion:");
            x_CloseConnection();
            return;
        }

    if (queue_ptr)
        // The cient has a queue, so memorize the client
        queue_ptr->TouchClientsRegistry(m_ClientId);

    // Execute the command
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
    // "input" [aff="affinity_token"] [msk=1]
    string          msg;
    s_ReadBufToString(buffer, msg);

    CJob &          job = m_BatchJobs[m_BatchPos].first;
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


    try {
        m_CommandArguments.AssignValues(params);
    }
    catch (const CNetScheduleException &  ex) {
        ERR_POST(ex.GetMsg());
        x_PrintRequestStop(eStatus_BadCmd);
        x_WriteMessageNoThrow("ERR:", string(ex.GetErrCodeString()) +
                                      ":" + ex.GetMsg());
        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
        return;
    }
    catch (...) {
        ERR_POST("Arguments parsing unknown exception. Batch submit is rejected.");
        x_PrintRequestStop(eStatus_BadCmd);
        x_WriteMessageNoThrow("ERR:", "eInternalError:"
                                      "Arguments parsing unknown exception");
        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
        return;
    }

    job.SetInput(NStr::ParseEscapes(m_CommandArguments.input));

    // Memorize the job affinity if given
    if ( !m_CommandArguments.affinity_token.empty() )
        m_BatchJobs[m_BatchPos].second =
                    NStr::ParseEscapes(m_CommandArguments.affinity_token);

    job.SetMask(m_CommandArguments.job_mask);
    job.SetSubmNotifPort(m_BatchSubmPort);
    job.SetSubmNotifTimeout(m_BatchSubmTimeout);
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
        unsigned    job_id = GetQueue()->SubmitBatch(m_ClientId, m_BatchJobs);
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
    TJobStatus      status = q->GetJobStatus(m_CommandArguments.job_id);
    // TODO: update timestamp

    WriteMessage("OK:", NStr::IntToString((int) status));
    if (status == CNetScheduleAPI::eJobNotFound) {
        ERR_POST(Warning << "SST for unknown job: "
                         << m_CommandArguments.job_key);
        x_PrintRequestStop(eStatus_NotFound);
        return;
    }
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessFastStatusW(CQueue* q)
{
    TJobStatus      status = q->GetJobStatus(m_CommandArguments.job_id);

    WriteMessage("OK:", NStr::IntToString((int) status));
    if (status == CNetScheduleAPI::eJobNotFound) {
        ERR_POST(Warning << "WST for unknown job: "
                         << m_CommandArguments.job_key);
        x_PrintRequestStop(eStatus_NotFound);
        return;
    }
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessChangeAffinity(CQueue* q)
{
    // This functionality requires client name and the session
    if (!m_ClientId.IsComplete()) {
        ERR_POST(Warning << "The client did not provide the name and the "
                            "session required by CHAFF");
        WriteMessage("ERR:eInvalidParameter");
        x_PrintRequestStop(eStatus_BadCmd);
        return;
    }

    if (m_CommandArguments.aff_to_add.empty() &&
        m_CommandArguments.aff_to_del.empty()) {
        ERR_POST(Warning << "CHAFF with neither add list nor del list");
        WriteMessage("ERR:eInvalidParameter");
        x_PrintRequestStop(eStatus_BadCmd);
        return;
    }

    list<string>    aff_to_add_list;
    list<string>    aff_to_del_list;

    NStr::Split(NStr::ParseEscapes(m_CommandArguments.aff_to_add),
                "\t,", aff_to_add_list, NStr::eNoMergeDelims);
    NStr::Split(NStr::ParseEscapes(m_CommandArguments.aff_to_del),
                "\t,", aff_to_del_list, NStr::eNoMergeDelims);

    string  msg = q->ChangeAffinity(m_ClientId, aff_to_add_list,
                                                aff_to_del_list);
    if (msg.empty())
        WriteMessage("OK:");
    else
        WriteMessage("OK:WARNING:", msg + ";");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessSubmit(CQueue* q)
{
    CJob        job(m_CommandArguments);

    // Never leave Client IP empty, if we're not provided with a real one,
    // use peer address as a last resort. See also x_ProcessSubmitBatch.
    if (m_CommandArguments.ip.empty()) {
        string  s_ip;
        NS_FormatIPAddress(m_ClientId.GetAddress(), s_ip);
        job.SetClientIP(s_ip);
    }

    WriteMessage("OK:", q->MakeKey(q->Submit(m_ClientId, job,
                                             m_CommandArguments.affinity_token)));

    // There is no need to log the job key, it is logged at lower level
    // together with all the submitted job parameters
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessSubmitBatch(CQueue* q)
{
    m_BatchSubmPort    = m_CommandArguments.port;
    m_BatchSubmTimeout = m_CommandArguments.timeout;
    if (!m_CommandArguments.ip.empty())
        m_BatchClientIP = m_CommandArguments.ip;
    else
        NS_FormatIPAddress(m_ClientId.GetAddress(), m_BatchClientIP);
    m_BatchClientSID   = m_CommandArguments.sid;
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
    switch (q->Cancel(m_ClientId, m_CommandArguments.job_id)) {
        case CNetScheduleAPI::eJobNotFound:
            WriteMessage("OK:WARNING:Job not found;");
            LOG_POST(Warning << "CANCEL for unknown job: "
                             << m_CommandArguments.job_key);
            x_PrintRequestStop(eStatus_NotFound);
            break;
        case CNetScheduleAPI::eCanceled:
            WriteMessage("OK:WARNING:Already canceled;");
            x_PrintRequestStop(eStatus_OK);
            break;
        default:
            WriteMessage("OK:");
            x_PrintRequestStop(eStatus_OK);
    }
}


void CNetScheduleHandler::x_ProcessStatus(CQueue* q)
{
    CJob            job;
    unsigned int    job_id = q->ReadJobFromDB(m_CommandArguments.job_id, job);

    if (job_id == 0) {
        // Here: there is no such a job
        WriteMessage("OK:",
                     NStr::IntToString((int) CNetScheduleAPI::eJobNotFound));
        ERR_POST(Warning << "STATUS for unknown job: "
                         << m_CommandArguments.job_key);
        x_PrintRequestStop(eStatus_NotFound);
        return;
    }

    // Here: the job was found
    WriteMessage("OK:", NStr::IntToString((int) job.GetStatus()) +
                        " " + NStr::IntToString(job.GetRetCode()) +
                        " \""   + NStr::PrintableString(job.GetOutput()) +
                        "\" \"" + NStr::PrintableString(job.GetErrorMsg()) +
                        "\" \"" + NStr::PrintableString(job.GetInput()) +
                        "\"");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessGetJob(CQueue* q)
{
    x_CheckGetJobPrerequisites(m_CommandArguments.wnode_affinity);

    list<string>    aff_list;
    NStr::Split(NStr::ParseEscapes(m_CommandArguments.affinity_token),
                "\t,", aff_list, NStr::eNoMergeDelims);

    CJob            job;
    q->GetJobOrWait(m_ClientId,
                    m_CommandArguments.port,
                    0,  // This is GET, not WGET
                    time(0), &aff_list,
                    m_CommandArguments.wnode_affinity,
                    m_CommandArguments.any_affinity,
                    &job);

    x_PrintGetJobResponse(q, job, m_CommandArguments.cmd == "GET2");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessWaitGet(CQueue* q)
{
    x_CheckGetJobPrerequisites(m_CommandArguments.wnode_affinity);

    list<string>        aff_list;
    NStr::Split(NStr::ParseEscapes(m_CommandArguments.affinity_token),
                "\t,", aff_list, NStr::eNoMergeDelims);

    CJob                job;
    q->GetJobOrWait(m_ClientId, time(0),
                    m_CommandArguments.port,
                    m_CommandArguments.timeout,
                    &aff_list,
                    m_CommandArguments.wnode_affinity,
                    m_CommandArguments.any_affinity,
                    &job);

    x_PrintGetJobResponse(q, job, m_CommandArguments.cmd == "WGET2");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessCancelWaitGet(CQueue* q)
{
    if (!m_ClientId.IsComplete())
        NCBI_THROW(CNetScheduleException, eInvalidClient,
               "Anonymous client (no client_node and client_session"
               " at handshake) cannot cancel waiting after WGET.");

    q->CancelWaitGet(m_ClientId);
    WriteMessage("OK:");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessPut(CQueue* q)
{
    if (m_CommandArguments.cmd == "PUT2") {
        if (m_CommandArguments.auth_token.empty())
            NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                       "Invalid authorization token. It cannot be empty.");
    }

    string      output = NStr::ParseEscapes(m_CommandArguments.output);
    TJobStatus  old_status = q->PutResult(m_ClientId, time(0),
                                          m_CommandArguments.job_id,
                                          m_CommandArguments.auth_token,
                                          m_CommandArguments.job_return_code,
                                          &output);
    if (old_status == CNetScheduleAPI::ePending ||
        old_status == CNetScheduleAPI::eRunning) {
        WriteMessage("OK:");
        x_PrintRequestStop(eStatus_OK);
        return;
    }
    if (old_status == CNetScheduleAPI::eDone) {
        WriteMessage("OK:WARNING:Already done;");
        LOG_POST(Warning << "Cannot accept job "
                         << m_CommandArguments.job_key
                         << " results. The job has already been done.");
        x_PrintRequestStop(eStatus_OK);
        return;
    }
    if (old_status == CNetScheduleAPI::eJobNotFound) {
        WriteMessage("ERR:eJobNotFound");
        ERR_POST(Warning << "Cannot accept job "
                         << m_CommandArguments.job_key
                         << " results. The job is unknown");
        x_PrintRequestStop(eStatus_NotFound);
        return;
    }

    // Here: invalid job status, nothing will be done
    WriteMessage("ERR:eInvalidJobStatus:"
                 "Cannot accept job results; job status is " +
                 CNetScheduleAPI::StatusToString(old_status));
    ERR_POST(Warning << "Cannot accept job "
                     << m_CommandArguments.job_key
                     << " results. The job has already been done.");
    x_PrintRequestStop(eStatus_InvalidJobStatus);
}


void CNetScheduleHandler::x_ProcessJobExchange(CQueue* q)
{
    x_CheckGetJobPrerequisites(m_CommandArguments.wnode_affinity);
    if (m_CommandArguments.cmd == "JXCG2") {
        if (m_CommandArguments.auth_token.empty())
            NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                       "Invalid authorization token. It cannot be empty.");
    }

    list<string>        aff_list;
    NStr::Split(NStr::ParseEscapes(m_CommandArguments.affinity_token),
                "\t,", aff_list, NStr::eNoMergeDelims);

    CJob                job;
    string              output = NStr::ParseEscapes(m_CommandArguments.output);
    TJobStatus          old_status = q->PutResultGetJob(
                                            m_ClientId,
                                            m_CommandArguments.job_id,
                                            m_CommandArguments.auth_token,
                                            m_CommandArguments.job_return_code,
                                            &output,
                                            // GetJob params
                                            &aff_list,
                                            m_CommandArguments.wnode_affinity,
                                            m_CommandArguments.any_affinity,
                                            &job);

    // The PUT part output cannot be sent to the client to avoid breaking
    // backward compatibillity. So it is logged only.
    if (old_status == CNetScheduleAPI::eDone) {
        LOG_POST(Warning << "Cannot accept job "
                         << m_CommandArguments.job_key
                         << " results. The job has already been done.");
    }
    else if (old_status == CNetScheduleAPI::eJobNotFound) {
        ERR_POST(Warning << "Cannot accept job "
                         << m_CommandArguments.job_key
                         << " results. The job is unknown");
    }
    else if (old_status != CNetScheduleAPI::ePending &&
             old_status != CNetScheduleAPI::eRunning) {
        ERR_POST(Warning << "Cannot accept job "
                         << m_CommandArguments.job_key
                         << " results. The job has already been done.");
    }

    x_PrintGetJobResponse(q, job, m_CommandArguments.cmd == "JXCG2");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessPutMessage(CQueue* q)
{
    if (q->PutProgressMessage(m_CommandArguments.job_id,
                              m_CommandArguments.progress_msg)) {
        WriteMessage("OK:");
        x_PrintRequestStop(eStatus_OK);
    }
    else {
        WriteMessage("ERR:eJobNotFound");
        ERR_POST(Warning << "MPUT for unknown job "
                         << m_CommandArguments.job_key);
        x_PrintRequestStop(eStatus_NotFound);
    }
}


void CNetScheduleHandler::x_ProcessGetMessage(CQueue* q)
{
    CJob        job;

    if (q->ReadJobFromDB(m_CommandArguments.job_id, job) != 0) {
        WriteMessage("OK:", job.GetProgressMsg());
        x_PrintRequestStop(eStatus_OK);
    } else {
        WriteMessage("ERR:eJobNotFound");
        ERR_POST(Warning << "MGET for unknown job "
                         << m_CommandArguments.job_key);
        x_PrintRequestStop(eStatus_NotFound);
    }
}


void CNetScheduleHandler::x_ProcessPutFailure(CQueue* q)
{
    if (m_CommandArguments.cmd == "FPUT2") {
        if (m_CommandArguments.auth_token.empty())
            NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                       "Invalid authorization token. It cannot be empty.");
    }

    string      warning;
    TJobStatus  old_status = q->FailJob(
                                m_ClientId,
                                m_CommandArguments.job_id,
                                m_CommandArguments.auth_token,
                                NStr::ParseEscapes(m_CommandArguments.err_msg),
                                NStr::ParseEscapes(m_CommandArguments.output),
                                m_CommandArguments.job_return_code,
                                warning);

    if (old_status == CNetScheduleAPI::eJobNotFound) {
        WriteMessage("ERR:eJobNotFound");
        ERR_POST(Warning << "FPUT for unknown job "
                         << m_CommandArguments.job_key);
        x_PrintRequestStop(eStatus_NotFound);
        return;
    }

    if (old_status == CNetScheduleAPI::eFailed) {
        WriteMessage("OK:WARNING:Already failed;");
        LOG_POST(Warning << "FPUT for already failed job "
                         << m_CommandArguments.job_key);
        x_PrintRequestStop(eStatus_OK);
        return;
    }

    if (old_status != CNetScheduleAPI::eRunning) {
        WriteMessage("ERR:eInvalidJobStatus");
        ERR_POST(Warning << "Cannot fail job "
                         << m_CommandArguments.job_key
                         << " in status "
                         << CNetScheduleAPI::StatusToString(old_status));
        x_PrintRequestStop(eStatus_InvalidJobStatus);
        return;
    }

    // Here: all is fine
    if (warning.empty())
        WriteMessage("OK:");
    else
        WriteMessage("OK:", "WARNING: " + warning);
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
    if (m_CommandArguments.cmd == "RETURN2") {
        if (m_CommandArguments.auth_token.empty())
            NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                       "Invalid authorization token. It cannot be empty.");
    }

    string          warning;
    TJobStatus      old_status = q->ReturnJob(m_ClientId,
                                              m_CommandArguments.job_id,
                                              m_CommandArguments.auth_token,
                                              warning);

    if (old_status == CNetScheduleAPI::eRunning) {
        if (warning.empty())
            WriteMessage("OK:");
        else
            WriteMessage("OK:", "WARNING: " + warning);
        x_PrintRequestStop(eStatus_OK);
        return;
    }

    if (old_status == CNetScheduleAPI::eJobNotFound) {
        WriteMessage("ERR:eJobNotFound");
        ERR_POST(Warning << "RETURN for unknown job "
                         << m_CommandArguments.job_key);
        x_PrintRequestStop(eStatus_NotFound);
        return;
    }

    WriteMessage("ERR:eInvalidJobStatus:" +
                 CNetScheduleAPI::StatusToString(old_status));
    ERR_POST(Warning << "Cannot return job "
                     << m_CommandArguments.job_key
                     << " in status "
                     << CNetScheduleAPI::StatusToString(old_status));

    x_PrintRequestStop(eStatus_InvalidJobStatus);
}


void CNetScheduleHandler::x_ProcessJobDelayExpiration(CQueue* q)
{
    if (m_CommandArguments.timeout <= 0) {
        WriteMessage("ERR:eInvalidParameter");
        ERR_POST(Warning << "Invalid timeout "
                         << m_CommandArguments.timeout
                         << " in JDEX for job "
                         << m_CommandArguments.job_key);
        x_PrintRequestStop(eStatus_BadCmd);
        return;
    }

    TJobStatus      status = q->JobDelayExpiration(m_CommandArguments.job_id,
                                                   m_CommandArguments.timeout);

    if (status == CNetScheduleAPI::eJobNotFound) {
        WriteMessage("ERR:eJobNotFound");
        ERR_POST(Warning << "JDEX for unknown job "
                         << m_CommandArguments.job_key);
        x_PrintRequestStop(eStatus_NotFound);
        return;
    }
    if (status != CNetScheduleAPI::eRunning) {
        WriteMessage("ERR:eInvalidJobStatus:" +
                     CNetScheduleAPI::StatusToString(status));
        ERR_POST(Warning << "Cannot change expiration for job "
                         << m_CommandArguments.job_key
                         << " in status "
                         << CNetScheduleAPI::StatusToString(status));

        x_PrintRequestStop(eStatus_InvalidJobStatus);
        return;
    }

    // Here: the new timeout has been applied
    WriteMessage("OK:");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessDropJob(CQueue* q)
{
    x_ProcessCancel(q);
}


void CNetScheduleHandler::x_ProcessStatistics(CQueue* q)
{
    CSocket &           socket = GetSocket();
    const string &      what   = m_CommandArguments.option;
    time_t              curr   = time(0);

    socket.DisableOSSendDelay(false);
    if (!what.empty() && what != "ALL") {
        x_StatisticsNew(q, what, curr);
        return;
    }

    WriteMessage("OK:Started: ", m_Server->GetStartTime().AsString());

    for (size_t  k = 0; k < g_ValidJobStatusesSize; ++k) {
        TJobStatus      st = g_ValidJobStatuses[k];
        unsigned        count = q->CountStatus(st);

        WriteMessage("OK:", CNetScheduleAPI::StatusToString(st) +
                            ": " + NStr::UIntToString(count));

        if (what == "ALL") {
            TNSBitVector::statistics bv_stat;
            q->StatusStatistics(st, &bv_stat);
            WriteMessage("OK:",
                         "  bit_blk=" + NStr::UIntToString(bv_stat.bit_blocks) +
                         "; gap_blk=" + NStr::UIntToString(bv_stat.gap_blocks) +
                         "; mem_used=" + NStr::UIntToString(bv_stat.memory_used));
        }
    } // for


    if (what == "ALL") {
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
                         NStr::SizetToString(pool_vec_size));

            for (size_t  i = 0; i < pool_vec_size; ++i) {
                const TBlockAlloc::TBucketPool::TResourcePool* rp =
                    TBlockAlloc::GetPool(i);
                if (rp) {
                    size_t pool_size = rp->GetSize();
                    if (pool_size) {
                        WriteMessage("OK:",
                                     "Pool [ " + NStr::SizetToString(i) +
                                     "] = " + NStr::SizetToString(pool_size));
                    }
                }
            }
        }}
    }

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
                                NStr::ParseEscapes(m_CommandArguments.affinity_token));

    if (!aff_exists) {
        WriteMessage("ERR:", "eProtocolSyntaxError:"
                             "Unknown affinity token \"" +
                             m_CommandArguments.affinity_token + "\"");
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
    CNcbiApplication *      app = CNcbiApplication::Instance();
    bool                    reloaded = app->ReloadConfig(CMetaRegistry::fReloadIfChanged);

    if (reloaded) {
        const CNcbiRegistry &   reg = app->GetConfig();

        m_Server->Configure(reg);

        // Logging from the [server] section
        SNS_Parameters          params;

        params.Read(reg, "server");
        m_Server->SetNSParameters(params, true);

        WriteMessage("OK:");
    }
    else
        WriteMessage("OK:WARNING:Configuration file has not been changed. RECO ignored.");

    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessActiveCount(CQueue* q)
{
    string      active_jobs = NStr::UIntToString(m_Server->CountActiveJobs());

    WriteMessage("OK:", active_jobs);
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessDump(CQueue* q)
{
    if (m_CommandArguments.job_id == 0) {
        // The whole queue dump
        q->PrintAllJobDbStat(*this);
        WriteMessage("OK:END");
        x_PrintRequestStop(eStatus_OK);
        return;
    }


    // Certain job dump
    if (q->PrintJobDbStat(*this, m_CommandArguments.job_id) == 0) {
        // Nothing was printed because there is no such a job
        WriteMessage("ERR:eJobNotFound");
        x_PrintRequestStop(eStatus_NotFound);
        return;
    }

    WriteMessage("OK:END");
    x_PrintRequestStop(eStatus_OK);
    return;
}


void CNetScheduleHandler::x_ProcessPrintQueue(CQueue* q)
{
    if (m_CommandArguments.job_status == CNetScheduleAPI::eJobNotFound) {
        WriteMessage("ERR:Status unknown: ", m_CommandArguments.job_status_string);
        x_PrintRequestStop(eStatus_BadCmd);
        return;
    }

    q->PrintQueue(*this, m_CommandArguments.job_status);
    WriteMessage("OK:END");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessShutdown(CQueue*)
{
    x_PrintRequestStop(eStatus_OK);
    WriteMessage("OK:");
    m_Server->SetShutdownFlag();
}


void CNetScheduleHandler::x_ProcessGetConf(CQueue*)
{
    CConn_SocketStream  ss(GetSocket().GetSOCK(), eNoOwnership);

    CNcbiApplication::Instance()->GetConfig().Write(ss);
    ss.flush();
    WriteMessage("OK:END");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessVersion(CQueue*)
{
    WriteMessage("OK:",
                 "server_version=" NETSCHEDULED_VERSION
                 "&storage_version=" NETSCHEDULED_STORAGE_VERSION
                 "&protocol_version=" NETSCHEDULED_PROTOCOL_VERSION
                 "&build_date=" + NStr::URLEncode(NETSCHEDULED_BUILD_DATE) +
                 "&ns_node=" + m_Server->GetNodeID() +
                 "&ns_session=" + m_Server->GetSessionID());
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessRegisterClient(CQueue* q)
{
    WriteMessage("OK:WARNING:Obsolete");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessUnRegisterClient(CQueue* q)
{
    WriteMessage("OK:WARNING:Obsolete");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessQList(CQueue*)
{
    WriteMessage("OK:", m_Server->GetQueueNames(";"));
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessQuitSession(CQueue*)
{
    x_CloseConnection();

    // Log the close request
    CDiagContext::SetRequestContext(m_DiagContext);
    x_PrintRequestStop(eStatus_OK);
    CDiagContext::SetRequestContext(NULL);
}


void CNetScheduleHandler::x_ProcessCreateQueue(CQueue*)
{
    m_Server->CreateQueue(m_CommandArguments.qname,
                          m_CommandArguments.qclass,
                          NStr::ParseEscapes(m_CommandArguments.comment));
    WriteMessage("OK:");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessDeleteQueue(CQueue*)
{
    m_Server->DeleteQueue(m_CommandArguments.qname);
    WriteMessage("OK:");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessQueueInfo(CQueue*)
{
    const string&   qname = m_CommandArguments.qname;
    int             kind;
    string          qclass;
    string          comment;

    m_Server->QueueInfo(qname, kind, &qclass, &comment);
    WriteMessage("OK:", NStr::IntToString(kind) + "\t" + qclass + "\t\"" +
                        NStr::PrintableString(comment) + "\"");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessGetParam(CQueue* q)
{
    unsigned int    max_input_size;
    unsigned int    max_output_size;

    q->GetMaxIOSizes(max_input_size, max_output_size);
    WriteMessage("OK:", "max_input_size=" +
                        NStr::IntToString(max_input_size) + ";"
                        "max_output_size=" +
                        NStr::IntToString(max_output_size) + ";" +
                        NETSCHEDULED_FEATURES);
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessGetConfiguration(CQueue* q)
{
    CQueue::TParameterList      parameters = q->GetParameters();

    ITERATE(CQueue::TParameterList, it, parameters) {
        WriteMessage("OK:", it->first + '=' + it->second);
    }
    WriteMessage("OK:END");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessReading(CQueue* q)
{
    CJob            job;

    q->GetJobForReading(m_ClientId, m_CommandArguments.timeout, &job);

    unsigned int    job_id = job.GetId();
    string          job_key;

    if (job_id) {
        job_key = q->MakeKey(job_id);
        WriteMessage("OK:", job_key + " " + job.GetAuthToken());
    }
    else
        WriteMessage("OK:");

    if (m_Server->IsLog()) {
        if (job_id)
            GetDiagContext().Extra().Print("job_key", job_key)
                                    .Print("auth_token", job.GetAuthToken());
        else
            GetDiagContext().Extra().Print("job_key", "None");
    }
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessConfirm(CQueue* q)
{
    TJobStatus      old_status = q->ConfirmReadingJob(
                                            m_ClientId,
                                            m_CommandArguments.job_id,
                                            m_CommandArguments.auth_token);
    x_FinalizeReadCommand("CFRM", "confirm", old_status);
}


void CNetScheduleHandler::x_ProcessReadFailed(CQueue* q)
{
    TJobStatus      old_status = q->FailReadingJob(
                                            m_ClientId,
                                            m_CommandArguments.job_id,
                                            m_CommandArguments.auth_token);
    x_FinalizeReadCommand("FRED", "fail", old_status);
}


void CNetScheduleHandler::x_ProcessReadRollback(CQueue* q)
{
    TJobStatus      old_status = q->ReturnReadingJob(
                                            m_ClientId,
                                            m_CommandArguments.job_id,
                                            m_CommandArguments.auth_token);
    x_FinalizeReadCommand("RDRB", "rollback", old_status);
}


void CNetScheduleHandler::x_FinalizeReadCommand(const string &  command,
                                                const string &  description,
                                                TJobStatus      status)
{
    if (status == CNetScheduleAPI::eJobNotFound) {
        WriteMessage("ERR:eJobNotFound");
        ERR_POST(Warning << command << " for unknown job "
                         << m_CommandArguments.job_key);
        x_PrintRequestStop(eStatus_NotFound);
        return;
    }
    if (status != CNetScheduleAPI::eReading) {
        WriteMessage("ERR:eInvalidJobStatus:" +
                     CNetScheduleAPI::StatusToString(status));
        ERR_POST(Warning << "Cannot " << description << " read job "
                         << m_CommandArguments.job_key
                         << " in status "
                         << CNetScheduleAPI::StatusToString(status));

        x_PrintRequestStop(eStatus_InvalidJobStatus);
        return;
    }

    WriteMessage("OK:");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessGetAffinityList(CQueue* q)
{
    WriteMessage("OK:", q->GetAffinityList());
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessInitWorkerNode(CQueue *)
{
    WriteMessage("OK:WARNING:Obsolete");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessClearWorkerNode(CQueue* q)
{
    q->ClearWorkerNode(m_ClientId);
    WriteMessage("OK:");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_ProcessCancelQueue(CQueue* q)
{
    q->CancelAllJobs(m_ClientId);
    WriteMessage("OK:");
    x_PrintRequestStop(eStatus_OK);
}


void CNetScheduleHandler::x_CmdNotImplemented(CQueue *)
{
    WriteMessage("ERR:Not implemented");
    x_PrintRequestStop(eStatus_NoImpl);
}


void CNetScheduleHandler::x_CheckGetJobPrerequisites(bool  wnode_affinity)
{
    if (!wnode_affinity)
        return;

    if (m_ClientId.IsComplete())
        return;

    NCBI_THROW(CNetScheduleException, eInvalidClient,
               "Anonymous client (no client_node and client_session"
               " at handshake) cannot refer to preferred affinities.");
}


void CNetScheduleHandler::x_PrintRequestStart(const SParsedCmd& cmd)
{
    if (m_Server->IsLog()) {
        CDiagContext_Extra    ctxt_extra =
                GetDiagContext().PrintRequestStart()
                            .Print("_type", "cmd")
                            .Print("cmd", cmd.command->cmd)
                            .Print("peer", GetSocket().GetPeerAddress(eSAF_IP))
                            .Print("conn", m_ConnReqId);

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
                .Print("info", msg)
                .Print("peer",  GetSocket().GetPeerAddress(eSAF_IP))
                .Print("conn", m_ConnReqId);
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


void CNetScheduleHandler::x_CloseConnection(void)
{
    // read trailing input and close the port
    CSocket &       socket = GetSocket();
    STimeout        to     = {0, 0};

    socket.SetTimeout(eIO_Read, &to);
    socket.Read(NULL, 1024);
    m_Server->CloseConnection(&socket);
    return;
}


// The function forms a responce for various 'get job' commands and prints
// extra to the log if required
void
CNetScheduleHandler::x_PrintGetJobResponse(const CQueue *  q,
                                           const CJob &    job,
                                           bool            security_token)
{
    if (!job.GetId()) {
        // No suitable job found
        if (m_Server->IsLog())
            GetDiagContext().Extra().Print("job_key", "none");
        WriteMessage("OK:");
        return;
    }

    // Here: a suitable job found
    string      response = q->MakeKey(job.GetId());

    if (m_Server->IsLog()) {
        // The only piece required for logging is the job key
        GetDiagContext().Extra().Print("job_key", response);
    }

    // We can re-use old jout and jerr job parameters for affinity and
    // session id/client ip respectively.
    response += " \"" + NStr::PrintableString(job.GetInput()) + "\""
                " \"" + NStr::PrintableString(
                            q->GetAffinityTokenByID(job.GetAffinityId())) + "\""
                " \"" + job.GetClientIP() + " " + job.GetClientSID() + "\""
                " " + NStr::UIntToString(job.GetMask());
    if (security_token)
        response += " " + job.GetAuthToken();

    WriteMessage("OK:", response);
    return;
}


void CNetScheduleHandler::x_StatisticsNew(CQueue *        q,
                                          const string &  what,
                                          time_t          curr)
{
    if (what == "CLIENTS")
        q->PrintClientsList(*this,
                            m_CommandArguments.comment == "VERBOSE");
    else if (what == "NOTIFICATIONS")
        q->PrintNotificationsList(*this,
                                  m_CommandArguments.comment == "VERBOSE");
    else if (what == "AFFINITIES")
        q->PrintAffinitiesList(*this,
                               m_CommandArguments.comment == "VERBOSE");
    else if (what == "JOBS") {
        size_t      total = 0;
        for (size_t  k = 0; k < g_ValidJobStatusesSize; ++k) {
                TJobStatus      st = g_ValidJobStatuses[k];
                size_t          count = q->CountStatus(st);

                total += count;
                WriteMessage("OK:", CNetScheduleAPI::StatusToString(st) +
                                    ": " + NStr::SizetToString(count));
        } // for
        WriteMessage("OK:Total: ", NStr::SizetToString(total));
    }
    else if (what == "WNODE") {
        WriteMessage("OK:WARNING:Obsolete; Use STAT CLIENTS instead.");
    }
    WriteMessage("OK:END");
    x_PrintRequestStop(eStatus_OK);
}

