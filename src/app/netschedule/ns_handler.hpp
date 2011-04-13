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
const unsigned kMaxMessageSize = kNetScheduleMaxDBErrSize * 4;



//////////////////////////////////////////////////////////////////////////
/// ConnectionHandler for NetScheduler

class CNetScheduleHandler : public IServer_LineMessageHandler
{
public:
    CNetScheduleHandler(CNetScheduleServer* server);
    // MessageHandler protocol
    virtual EIO_Event GetEventsToPollFor(const CTime** alarm_time) const;
    virtual void OnOpen(void);
    virtual void OnWrite(void);
    virtual void OnClose(IServer_ConnectionHandler::EClosePeer peer);
    virtual void OnTimeout(void);
    virtual void OnOverflow(EOverflowReason reason);
    virtual void OnMessage(BUF buffer);

    void WriteMsg(const char*           prefix,
                  const std::string&    msg = kEmptyStr,
                  bool                  interim = false);
    // Write "OK:message", differs in logging with WriteOK
    // it is logged as Extra not to end the request prematurely.
    void WriteInterim(const std::string& msg = kEmptyStr);
    void WriteOK(const std::string& msg = kEmptyStr);
    void WriteErr(const std::string& msg = kEmptyStr);

private:
    // Message processing phases
    void ProcessMsgAuth(BUF buffer);
    void ProcessMsgQueue(BUF buffer);
    void ProcessMsgRequest(BUF buffer);
    // Message processing for ProcessSubmitBatch phases
    void ProcessMsgBatchHeader(BUF buffer);
    void ProcessMsgBatchJob(BUF buffer);
    void ProcessMsgBatchSubmit(BUF buffer);

    // Monitoring
    bool IsMonitoring() const;
    /// Send string to monitor
    void MonitorPost(const std::string& msg);


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
        FProcessor    processor;
        TNSClientRole role;
    };
    typedef SNSProtoCmdDef<SCommandExtra>      SCommandMap;
    typedef SNSProtoParsedCmd<SCommandExtra>   SParsedCmd;
    typedef CNetServProtoParser<SCommandExtra> TProtoParser;

private:
    static SCommandMap          sm_CommandMap[];
    static SCommandMap          sm_BatchHeaderMap[];
    static SCommandMap          sm_BatchEndMap[];
    static SNSProtoArgument     sm_BatchArgs[];

    void ProcessError(const CException& ex);

    // Command processors
    void ProcessFastStatusS(CQueue*);
    void ProcessFastStatusW(CQueue*);
    void ProcessSubmit(CQueue*);
    void ProcessSubmitBatch(CQueue*);
    void ProcessBatchStart(CQueue*);
    void ProcessBatchSequenceEnd(CQueue*);
    void ProcessCancel(CQueue*);
    void ProcessStatus(CQueue*);
    void ProcessGetJob(CQueue*);
    void ProcessWaitGet(CQueue*);
    void ProcessPut(CQueue*);
    void ProcessJobExchange(CQueue*);
    void ProcessPutMessage(CQueue*);
    void ProcessGetMessage(CQueue*);
    void ProcessForceReschedule(CQueue*);
    void ProcessPutFailure(CQueue*);
    void ProcessDropQueue(CQueue*);
    void ProcessReturn(CQueue*);
    void ProcessJobRunTimeout(CQueue*);
    void ProcessJobDelayExpiration(CQueue*);
    void ProcessDropJob(CQueue*);
    void ProcessStatistics(CQueue*);
    void ProcessStatusSnapshot(CQueue*);
    void ProcessMonitor(CQueue*);
    void ProcessReloadConfig(CQueue*);
    void ProcessActiveCount(CQueue*);
    void ProcessDump(CQueue*);
    void ProcessPrintQueue(CQueue*);
    void ProcessShutdown(CQueue*);
    void ProcessVersion(CQueue*);
    void ProcessRegisterClient(CQueue*);
    void ProcessUnRegisterClient(CQueue*);
    void ProcessQList(CQueue*);
    void ProcessLog(CQueue*);
    void ProcessQuitSession(CQueue*);
    void ProcessCreateQueue(CQueue*);
    void ProcessDeleteQueue(CQueue*);
    void ProcessQueueInfo(CQueue*);
    void ProcessQuery(CQueue*);
    void ProcessSelectQuery(CQueue*);
    void ProcessGetParam(CQueue*);
    void ProcessGetConfiguration(CQueue*);
    void ProcessReading(CQueue*);
    void ProcessConfirm(CQueue*);
    void ProcessReadFailed(CQueue*);
    void ProcessReadRollback(CQueue*);
    void ProcessGetAffinityList(CQueue*);
    void ProcessInitWorkerNode(CQueue*);
    void ProcessClearWorkerNode(CQueue*);

    // Delayed output handlers
    void WriteProjection();

private:
    CRef<CQueue> GetQueue(void) {
        CRef<CQueue> ref(m_QueueRef.Lock());
        if (ref != NULL)
            return ref;
        NCBI_THROW(CNetScheduleException, eUnknownQueue, "Job queue deleted");
    }

    static
    void x_ParseTags(const std::string& strtags, TNSTagList& tags);
    static
    std::string x_SerializeBitVector(TNSBitVector& bv);
    static
    TNSBitVector x_DeserializeBitVector(const std::string& s);

    void x_MonitorJob(const CJob& job);


    bool x_CheckVersion(void);
    void x_CheckAccess(TNSClientRole role);
    void x_AccessViolationMessage(unsigned deficit, std::string& msg);
    std::string x_FormatErrorMessage(const std::string& header,
                                     const std::string& what);
    void x_WriteErrorToMonitor(const std::string& msg);
    SParsedCmd x_ParseCommand(CTempString command);
    // Moved from CNetScheduleServer
    void x_MakeLogMessage(std::string&       lmsg,
                          const std::string& op,
                          const std::string& text);
    void x_MakeGetAnswer(const CJob& job);
    void x_StatisticsNew(CQueue* q, const std::string& what, time_t curr);

    // Data
    std::string                     m_Request;
    std::string                     m_Answer;
    char                            m_MsgBuffer[kMaxMessageSize];

    unsigned                        m_PeerAddr;
    // CWorkerNode contains duplicates of m_AuthString and m_PeerAddr
    TWorkerNodeRef                  m_WorkerNode;
    std::string                     m_AuthString;
    CNetScheduleServer*             m_Server;
    std::string                     m_QueueName;
    CWeakRef<CQueue>                m_QueueRef;
    SJS_Request                     m_JobReq;
    // Incapabilities - that is combination of ENSAccess
    // rights, which can NOT be performed by this connection
    unsigned                        m_Incaps;
    unsigned                        m_Unreported;
    bool                            m_VersionControl;
    // Unique command number for relating command and reply
    unsigned                        m_CommandNumber;
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

    // Due to processing of one logical request can span
    // multiple threads, but goes on in a single handler,
    // we use manual request context switching, thus replacing
    // default per-thread mechanism.
    CRef<CNSRequestContextFactory>  m_RequestContextFactory;

    /// Parser for incoming commands
    TProtoParser                    m_ReqParser;

}; // CNetScheduleHandler


END_NCBI_SCOPE

#endif

