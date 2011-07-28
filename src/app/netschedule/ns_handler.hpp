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
#include <connect/server.hpp>
#include <connect/services/netservice_protocol_parser.hpp>

#include "ns_queue.hpp"
#include "ns_js_request.hpp"
#include "ns_server_misc.hpp"
#include "netschedule_version.hpp"



BEGIN_NCBI_SCOPE

// Forward declarations
class CNetScheduleServer;
class CNSRequestContextFactory;

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
    virtual EIO_Event GetEventsToPollFor(const CTime** alarm_time) const;
    virtual void      OnOpen(void);
    virtual void      OnWrite(void);
    virtual void      OnClose(IServer_ConnectionHandler::EClosePeer peer);
    virtual void      OnTimeout(void);
    virtual void      OnOverflow(EOverflowReason reason);
    virtual void      OnMessage(BUF buffer);

    /// Init diagnostics Client IP and Session ID for proper logging
    void InitDiagnostics(void);
    /// Reset diagnostics Client IP and Session ID to avoid logging
    /// not related to the request
    void ResetDiagnostics(void);

    /// Writes a message to the socket
    void WriteMessage(CTempString prefix, CTempString msg);
    void WriteMessage(CTempString msg);

    /// Statuses of commands to be set in diagnostics' request context
    /// Additional statuses can be taken from
    /// http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
    enum EHTTPStatus {
        eStatus_OK                  = 200, ///< Command is ok and execution is good
        eStatus_Inactive            = 204, ///< Connection was closed due to inactivity
                                           ///< timeout
        eStatus_BadCmd              = 400, ///< Command is incorrect
        eStatus_BadAuth             = 401, ///< Operation is not permitted
        eStatus_HTTPProbe           = 402, ///< Routine test from systems
        eStatus_NotFound            = 404, ///< Job is not found
        eStatus_CmdTimeout          = 408, ///< Command timeout is exceeded
        eStatus_ServerError         = 500, ///< Internal server error
        eStatus_NoImpl              = 501, ///< Command is not implemented
        eStatus_JobNotifierError    = 502, ///< Job noticication thread failed
        eStatus_JobExecWatcherError = 503, ///< Job execution watcher thread failed
        eStatus_QueueCleanerError   = 504  ///< Queue cleaner thread failed
    };


private:
    // Message processing phases
    void x_ProcessMsgAuth(BUF buffer);
    void x_ProcessMsgQueue(BUF buffer);
    void x_ProcessMsgRequest(BUF buffer);
    // Message processing for ProcessSubmitBatch phases
    void x_ProcessMsgBatchHeader(BUF buffer);
    void x_ProcessMsgBatchJob(BUF buffer);
    void x_ProcessMsgBatchSubmit(BUF buffer);

    void x_SetQuickAcknowledge(void);
    void x_WriteMessageNoThrow(CTempString  prefix, CTempString msg);

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
        eNSCR_Any           = 0,
        eNSCR_Queue         = eNSAC_Queue,
        eNSCR_Worker        = eNSAC_Worker + eNSAC_Queue,
        eNSCR_Submitter     = eNSAC_Submitter + eNSAC_Queue,
        eNSCR_Admin         = eNSAC_Admin,
        eNSCR_QueueAdmin    = eNSAC_QueueAdmin + eNSAC_Queue
    };
    typedef unsigned TNSClientRole;

    typedef void (CNetScheduleHandler::*FProcessor)(CQueue*);
    struct SCommandExtra {
        FProcessor          processor;
        TNSClientRole       role;
    };
    typedef SNSProtoCmdDef<SCommandExtra>      SCommandMap;
    typedef SNSProtoParsedCmd<SCommandExtra>   SParsedCmd;
    typedef CNetServProtoParser<SCommandExtra> TProtoParser;

private:

    // Command processors
    void x_ProcessFastStatusS(CQueue*);
    void x_ProcessFastStatusW(CQueue*);
    void x_ProcessSubmit(CQueue*);
    void x_ProcessSubmitBatch(CQueue*);
    void x_ProcessBatchStart(CQueue*);
    void x_ProcessBatchSequenceEnd(CQueue*);
    void x_ProcessCancel(CQueue*);
    void x_ProcessStatus(CQueue*);
    void x_ProcessGetJob(CQueue*);
    void x_ProcessWaitGet(CQueue*);
    void x_ProcessPut(CQueue*);
    void x_ProcessJobExchange(CQueue*);
    void x_ProcessPutMessage(CQueue*);
    void x_ProcessGetMessage(CQueue*);
    void x_ProcessPutFailure(CQueue*);
    void x_ProcessDropQueue(CQueue*);
    void x_ProcessReturn(CQueue*);
    void x_ProcessJobDelayExpiration(CQueue*);
    void x_ProcessDropJob(CQueue*);
    void x_ProcessStatistics(CQueue*);
    void x_ProcessStatusSnapshot(CQueue*);
    void x_ProcessReloadConfig(CQueue*);
    void x_ProcessActiveCount(CQueue*);
    void x_ProcessDump(CQueue*);
    void x_ProcessPrintQueue(CQueue*);
    void x_ProcessShutdown(CQueue*);
    void x_ProcessVersion(CQueue*);
    void x_ProcessRegisterClient(CQueue*);
    void x_ProcessUnRegisterClient(CQueue*);
    void x_ProcessQList(CQueue*);
    void x_ProcessQuitSession(CQueue*);
    void x_ProcessCreateQueue(CQueue*);
    void x_ProcessDeleteQueue(CQueue*);
    void x_ProcessQueueInfo(CQueue*);
    void x_ProcessQuery(CQueue*);
    void x_ProcessSelectQuery(CQueue*);
    void x_ProcessGetParam(CQueue*);
    void x_ProcessGetConfiguration(CQueue*);
    void x_ProcessReading(CQueue*);
    void x_ProcessConfirm(CQueue*);
    void x_ProcessReadFailed(CQueue*);
    void x_ProcessReadRollback(CQueue*);
    void x_ProcessGetAffinityList(CQueue*);
    void x_ProcessInitWorkerNode(CQueue*);
    void x_ProcessClearWorkerNode(CQueue*);
    void x_CmdNotImplemented(CQueue*);

    // Delayed output handlers
    void x_WriteProjection();

private:
    CRef<CQueue> GetQueue(void) {
        CRef<CQueue> ref(m_QueueRef.Lock());
        if (ref != NULL)
            return ref;
        NCBI_THROW(CNetScheduleException, eUnknownQueue, "Job queue deleted");
    }

    static void x_ParseTags(const std::string& strtags, TNSTagList& tags);
    static std::string x_SerializeBitVector(TNSBitVector &  bv);
    static TNSBitVector x_DeserializeBitVector(const std::string& s);

    bool x_CheckVersion(void);
    void x_CheckAccess(TNSClientRole role);
    void x_AccessViolationMessage(unsigned deficit, std::string& msg);
    // Moved from CNetScheduleServer
    void x_StatisticsNew(CQueue* q, const std::string& what, time_t curr);

    void x_PrintRequestStart(const SParsedCmd& cmd);
    void x_PrintRequestStart(CTempString  msg);
    void x_PrintRequestStop(EHTTPStatus  status);

    std::string  x_FormGetJobResponse(const CQueue* q, const CJob& job) const;

    // Data
    size_t                          m_MsgBufferSize;
    char*                           m_MsgBuffer;

    unsigned                        m_PeerAddr;
    // CWorkerNode contains duplicates of m_AuthString and m_PeerAddr
    TWorkerNodeRef                  m_WorkerNode;
    std::string                     m_RawAuthString;
    std::string                     m_AuthClientName;
    std::string                     m_AuthProgName;

    CNetScheduleServer*             m_Server;
    std::string                     m_QueueName;
    CWeakRef<CQueue>                m_QueueRef;
    SJS_Request                     m_JobReq;

    // Incapabilities - that is combination of ENSAccess
    // rights, which can NOT be performed by this connection
    unsigned                        m_Incaps;
    unsigned                        m_Unreported;
    bool                            m_VersionControl;

    // Phase of connection - login, queue, command, batch submit etc.
    void (CNetScheduleHandler::*m_ProcessMessage)(BUF buffer);
    // Delayed output processor
    void (CNetScheduleHandler::*m_DelayedOutput)();

    // Batch submit data
    unsigned                        m_BatchSize;
    unsigned                        m_BatchPos;
    CStopWatch                      m_BatchStopWatch;
    vector<CJob>                    m_BatchJobs;
    unsigned                        m_BatchSubmPort;
    unsigned                        m_BatchSubmTimeout;
    std::string                     m_BatchClientIP;
    std::string                     m_BatchClientSID;

    // For projection writer
    auto_ptr<TNSBitVector>          m_SelectedIds;
    SFieldsDescription              m_FieldDescr;

    /// Quick local timer
    CFastLocalTime                  m_LocalTimer;

    /// Parsers for incoming commands and their parser tables
    TProtoParser                    m_SingleCmdParser;
    static SCommandMap              sm_CommandMap[];
    TProtoParser                    m_BatchHeaderParser;
    static SCommandMap              sm_BatchHeaderMap[];
    TProtoParser                    m_BatchEndParser;
    static SCommandMap              sm_BatchEndMap[];


    /// Diagnostics context for the current connection
    CRef<CRequestContext>           m_ConnContext;
    string                          m_ConnReqId;
    /// Diagnostics context for the currently executed command
    CRef<CRequestContext>           m_DiagContext;

}; // CNetScheduleHandler


END_NCBI_SCOPE

#endif

