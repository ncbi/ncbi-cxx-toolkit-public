#ifndef NETSCHEDULE_HANDLER__HPP
#define NETSCHEDULE_HANDLER__HPP

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

#include <string>
#include <connect/services/netschedule_api.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/request_ctx.hpp>
#include <connect/server.hpp>
#include <connect/services/netservice_protocol_parser.hpp>

#include "ns_queue.hpp"
#include "ns_command_arguments.hpp"
#include "ns_server_misc.hpp"
#include "netschedule_version.hpp"
#include "ns_clients.hpp"
#include "ns_access.hpp"
#include "ns_precise_time.hpp"



BEGIN_NCBI_SCOPE

// Forward declarations
class CNetScheduleServer;
class CNSRequestContextFactory;
class CNSRollbackInterface;

//
const size_t    kInitialMessageBufferSize = 16 * 1024;
const size_t    kMessageBufferIncrement = 2 * 1024;



//////////////////////////////////////////////////////////////////////////
/// ConnectionHandler for NetScheduler

class CNetScheduleHandler : public IServer_LineMessageHandler
{
public:
    CNetScheduleHandler(CNetScheduleServer* server);
    ~CNetScheduleHandler();

    // MessageHandler protocol
    virtual void      OnOpen(void);
    virtual void      OnWrite(void);
    virtual void      OnClose(IServer_ConnectionHandler::EClosePeer peer);
    virtual void      OnTimeout(void);
    virtual void      OnOverflow(EOverflowReason reason);
    virtual void      OnMessage(BUF buffer);

    /// Statuses of commands to be set in diagnostics' request context
    /// Additional statuses can be taken from
    /// http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
    enum EHTTPStatus {
        eStatus_OK                  = 200, ///< Command is ok and execution is good

        eStatus_BadRequest          = 400, ///< Command is incorrect
        eStatus_NotFound            = 404, ///< Job is not found
        eStatus_Inactive            = 408, ///< Connection was closed due to inactivity
                                           ///< timeout
        eStatus_InvalidJobStatus    = 409, ///< Invalid job status
        eStatus_HTTPProbe           = 444, ///< Routine test from systems
        eStatus_SocketIOError       = 499, ///< Error writing to socket

        eStatus_ServerError         = 500, ///< Internal server error
        eStatus_NotImplemented      = 501, ///< Command is not implemented
        eStatus_SubmitRefused       = 503, ///< In refuse submits mode and received SUBMIT
        eStatus_ShuttingDown        = 503  ///< Server is shutting down
    };


private:
    // Message processing phases
    unsigned int x_GetPeerAddress(void);
    void x_ProcessMsgAuth(BUF buffer);
    void x_ProcessMsgQueue(BUF buffer);
    void x_ProcessMsgRequest(BUF buffer);
    void x_UpdateClientPassedChecks(CQueue * q);
    // Message processing for ProcessSubmitBatch phases
    void x_ProcessMsgBatchHeader(BUF buffer);
    void x_ProcessMsgBatchJob(BUF buffer);
    void x_ProcessMsgBatchSubmit(BUF buffer);

    void x_SetQuickAcknowledge(void);
    void x_SetCmdRequestStatus(unsigned int  status)
    { if (m_CmdContext.NotNull())
        m_CmdContext->SetRequestStatus(status); }
    void x_SetConnRequestStatus(unsigned int  status)
    { if (m_ConnContext.NotNull())
        m_ConnContext->SetRequestStatus(status); }
    // Writes a message to the socket
    // It closes the connection if there were socket writing errors
    EIO_Status x_WriteMessage(CTempString msg);


public:

    typedef void (CNetScheduleHandler::*FProcessor)(CQueue*);
    struct SCommandExtra {
        FProcessor          processor;
        TNSCommandChecks    checks;
    };
    typedef SNSProtoCmdDef<SCommandExtra>      SCommandMap;
    typedef SNSProtoParsedCmd<SCommandExtra>   SParsedCmd;
    typedef CNetServProtoParser<SCommandExtra> TProtoParser;

private:

    // Command processors
    void x_ProcessFastStatusS(CQueue*);
    void x_ProcessFastStatusW(CQueue*);
    void x_ProcessChangeAffinity(CQueue*);
    void x_ProcessSetAffinity(CQueue*);
    void x_ProcessSubmit(CQueue*);
    void x_ProcessSubmitBatch(CQueue*);
    void x_ProcessBatchStart(CQueue*);
    void x_ProcessBatchSequenceEnd(CQueue*);
    void x_ProcessCancel(CQueue*);
    void x_ProcessStatus(CQueue*);
    void x_ProcessGetJob(CQueue*);
    void x_ProcessCancelWaitGet(CQueue*);
    void x_ProcessPut(CQueue*);
    void x_ProcessJobExchange(CQueue*);
    void x_ProcessPutMessage(CQueue*);
    void x_ProcessGetMessage(CQueue*);
    void x_ProcessPutFailure(CQueue*);
    void x_ProcessDropQueue(CQueue*);
    void x_ProcessReturn(CQueue*);
    void x_ProcessJobDelayExpiration(CQueue*);
    void x_ProcessListenJob(CQueue*);
    void x_ProcessStatistics(CQueue*);
    void x_ProcessStatusSnapshot(CQueue*);
    void x_ProcessReloadConfig(CQueue*);
    void x_ProcessActiveCount(CQueue*);
    void x_ProcessDump(CQueue*);
    void x_ProcessShutdown(CQueue*);
    void x_ProcessGetConf(CQueue*);
    void x_ProcessVersion(CQueue*);
    void x_ProcessHealth(CQueue*);
    void x_ProcessAckAlert(CQueue*);
    void x_ProcessQList(CQueue*);
    void x_ProcessQuitSession(CQueue*);
    void x_ProcessCreateDynamicQueue(CQueue*);
    void x_ProcessDeleteDynamicQueue(CQueue*);
    void x_ProcessQueueInfo(CQueue*);
    void x_ProcessSetQueue(CQueue*);
    void x_ProcessGetParam(CQueue*);
    void x_ProcessGetConfiguration(CQueue*);
    void x_ProcessReading(CQueue*);
    void x_ProcessConfirm(CQueue*);
    void x_ProcessReadFailed(CQueue*);
    void x_ProcessReadRollback(CQueue*);
    void x_FinalizeReadCommand(const string &  command,
                               TJobStatus      status);
    void x_ProcessGetAffinityList(CQueue*);
    void x_ProcessSetClientData(CQueue*);
    void x_ProcessClearWorkerNode(CQueue*);
    void x_ProcessCancelQueue(CQueue*);
    void x_ProcessRefuseSubmits(CQueue*);
    void x_ProcessPause(CQueue*);
    void x_ProcessResume(CQueue*);
    void x_CmdNotImplemented(CQueue*);
    void x_CmdObsolete(CQueue*);
    void x_CheckNonAnonymousClient(const string &  message);
    void x_CheckPortAndTimeout(void);
    void x_CheckAuthorizationToken(void);
    void x_CheckGetParameters(void);
    void x_CheckQInf2Parameters(void);

private:
    CRef<CQueue> GetQueue(void) {
        CRef<CQueue> ref(m_QueueRef.Lock());
        if (ref != NULL)
            return ref;
        NCBI_THROW(CNetScheduleException, eUnknownQueue, "Job queue unknown");
    }

    void x_CreateConnContext(void);

    // Write into socket, logs the message and closes the connection
    void x_OnCmdParserError(bool  need_request_start,
                            const string &  msg, const string &  suffix);

    // Moved from CNetScheduleServer
    void x_StatisticsNew(CQueue *  q, const string &  what,
                         const CNSPreciseTime &  curr);

    void x_PrintCmdRequestStart(const SParsedCmd& cmd);
    void x_PrintCmdRequestStart(CTempString  msg);
    void x_PrintCmdRequestStop(void);

    void x_PrintGetJobResponse(const CQueue * q,
                               const CJob &   job,
                               bool           add_security_token);
    bool x_CanBeWithoutQueue(FProcessor  processor) const;

    void x_ClearRollbackAction(void);
    void x_ExecuteRollbackAction(CQueue * q);

    string x_GetServerSection(void) const;
    string x_GetStoredSectionValues(const string &  section_name,
                                    const map<string, string> & values) const;
    string x_GetLogSection(void) const;
    string x_GetDiagSection(void) const;
    string x_GetBdbSection(void) const;

    // Data
    size_t                          m_MsgBufferSize;
    char *                          m_MsgBuffer;

    string                          m_RawAuthString;
    CNSClientId                     m_ClientId;

    CNetScheduleServer*             m_Server;
    string                          m_QueueName;
    CWeakRef<CQueue>                m_QueueRef;
    SNSCommandArguments             m_CommandArguments;

    // Phase of connection - login, queue, command, batch submit etc.
    void (CNetScheduleHandler::*m_ProcessMessage)(BUF buffer);

    // Batch submit data
    unsigned                        m_BatchSize;
    unsigned                        m_BatchPos;
    CStopWatch                      m_BatchStopWatch;
    vector< pair<CJob, string> >    m_BatchJobs;    // Job + aff_token
    string                          m_BatchGroup;
    unsigned                        m_BatchSubmPort;
    CNSPreciseTime                  m_BatchSubmTimeout;
    string                          m_BatchClientIP;
    string                          m_BatchClientSID;
    bool                            m_WithinBatchSubmit;

    // Parsers for incoming commands and their parser tables
    TProtoParser                    m_SingleCmdParser;
    static SCommandMap              sm_CommandMap[];
    TProtoParser                    m_BatchHeaderParser;
    static SCommandMap              sm_BatchHeaderMap[];
    TProtoParser                    m_BatchEndParser;
    static SCommandMap              sm_BatchEndMap[];


    // Diagnostics context for the current connection
    CRef<CRequestContext>           m_ConnContext;
    // Diagnostics context for the currently executed command
    CRef<CRequestContext>           m_CmdContext;

    // Rollback support
    CNSRollbackInterface *          m_RollbackAction;

}; // CNetScheduleHandler


END_NCBI_SCOPE

#endif

