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
 * Authors: Eugene Vasilchenko
 *
 * File Description: base class for processors which may generate os_gateway
 *                   fetches
 *
 */

#include <ncbi_pch.hpp>

#include "osg_processor_base.hpp"

#include "osg_fetch.hpp"
#include "osg_caller.hpp"
#include "osg_connection.hpp"
#include "cass_processor_base.hpp"

#include <objects/id2/ID2_Request_Packet.hpp>
#include <objects/id2/ID2_Request.hpp>
#include <objects/id2/ID2_Params.hpp>
#include <objects/id2/ID2_Param.hpp>
#include <objects/id2/ID2_Reply.hpp>
#include "pubseq_gateway.hpp"
#include <thread>


BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(osg);


#if 0
# define tLOG_POST(m) LOG_POST(m)
#else
# define tLOG_POST(m) ((void)0)
#endif

#define SEND_TRACE(str) SendTrace(str)
#define SEND_TRACE_FMT(m)                                               \
    do {                                                                \
        if ( NeedTrace() ) {                                            \
            ostringstream str;                                          \
            str << m;                                                   \
            SendTrace(str.str());                                       \
        }                                                               \
    } while(0)


/////////////////////////////////////////////////////////////////////////////
// CPSGS_OSGProcessorBase
/////////////////////////////////////////////////////////////////////////////


CPSGS_OSGProcessorBase::CPSGS_OSGProcessorBase(TEnabledFlags enabled_flags,
                                               const CRef<COSGConnectionPool>& pool,
                                               const shared_ptr<CPSGS_Request>& request,
                                               const shared_ptr<CPSGS_Reply>& reply,
                                               TProcessorPriority priority)
    : m_Context(request->GetRequestContext()),
      m_ConnectionPool(pool),
      m_EnabledFlags(enabled_flags),
      m_Status(IPSGS_Processor::ePSGS_InProgress),
      m_BackgroundProcesing(0),
      m_NeedTrace(request->NeedTrace())
{
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::CPSGS_OSGProcessorBase()");
    m_Request = request;
    m_Reply = reply;
    m_Priority = priority;
    _ASSERT(m_ConnectionPool);
    _ASSERT(m_Request);
    _ASSERT(m_Reply);
}


static int s_DebugLevel = eDebugLevel_default;
static EDiagSev s_DiagSeverity = eDiag_Trace;


int GetDebugLevel()
{
    return s_DebugLevel;
}


void SetDebugLevel(int level)
{
    s_DebugLevel = level;
}


Severity GetDiagSeverity()
{
    return Severity(s_DiagSeverity);
}


void SetDiagSeverity(EDiagSev severity)
{
    s_DiagSeverity = severity;
}


IPSGS_Processor* CPSGS_OSGProcessorBase::CreateProcessor(shared_ptr<CPSGS_Request> request,
                                                         shared_ptr<CPSGS_Reply> reply,
                                                         TProcessorPriority priority) const
{
    return nullptr;
}


CPSGS_OSGProcessorBase::~CPSGS_OSGProcessorBase()
{
    StopAsyncThread();
    CMutexGuard guard(m_StatusMutex);
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::~CPSGS_OSGProcessorBase() status: "<<State());
    _ASSERT(m_Status != IPSGS_Processor::ePSGS_InProgress);
    _ASSERT(!m_BackgroundProcesing);
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::~CPSGS_OSGProcessorBase() return: "<<State());
}


void CPSGS_OSGProcessorBase::WaitForOtherProcessors()
{
}


void CPSGS_OSGProcessorBase::WaitForCassandra()
{
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::WaitForCassandra() waiting: "<<State());
    SendTrace("OSG: waiting for Cassandra results");
    GetRequest()->WaitFor(kCassandraProcessorEvent);
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::WaitForCassandra() waited: "<<State());
}


void CPSGS_OSGProcessorBase::SendTrace(const string& str)
{
    if ( NeedTrace() ) {
        CMutexGuard guard(m_StatusMutex);
        if ( !IsCanceled() ) {
            m_Reply->SendTrace(str, m_Request->GetStartTimestamp());
        }
    }
}


void CPSGS_OSGProcessorBase::StopAsyncThread()
{
    /*
    if ( m_Thread ) {
        tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::~CPSGS_OSGProcessorBase() joining status: "<<m_Status);
        m_Thread->join();
        m_Thread.reset();
        tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::~CPSGS_OSGProcessorBase() joined status: "<<m_Status);
    }
    */
}


void CPSGS_OSGProcessorBase::Process()
{
    SEND_TRACE("OSG: Process() called");
    CRequestContextResetter     context_resetter;
    GetRequest()->SetRequestContext();

    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::Process(): "<<State());
    if ( IsCanceled() ) {
        tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::Process() canceled: "<<State());
        return;
    }
    if ( GetFetches().empty() ) {
        CreateRequests();
        if ( m_Context ) {
            for ( auto& f : m_Fetches ) {
                f->SetContext(*m_Context);
            }
        }
    }
    _ASSERT(!GetFetches().empty());
    CallDoProcess();
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::Process() return: "<<State());
}


void CPSGS_OSGProcessorBase::CallDoProcess()
{
    if ( m_ConnectionPool->GetAsyncProcessing() ) {
        CallDoProcessAsync();
    }
    else {
        CallDoProcessSync();
    }
}


void CPSGS_OSGProcessorBase::CallDoProcessSync()
{
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::CallDoProcessSync() start: "<<State());
    DoProcess();
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::CallDoProcessSync() return: "<<State());
}


void CPSGS_OSGProcessorBase::CallDoProcessCallback(const CBackgroundProcessingGuard& guard_in)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    CBackgroundProcessingGuard guard(guard_in);
    if ( !guard.GetGuardedProcessor() ) {
        tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::CallDoProcessCallback() canceled: "<<State());
        return;
    }
    _ASSERT(guard.GetGuardedProcessor() == this);
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::CallDoProcessCallback() start: "<<State());
    DoProcess();
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::CallDoProcessCallback() return: "<<State());
}


void CPSGS_OSGProcessorBase::CallDoProcessAsync()
{
    SEND_TRACE("OSG: switching Process() to background thread");
    try {
        CBackgroundProcessingGuard guard(this);
        if ( !guard.GetGuardedProcessor() ) {
            tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::CallDoProcessAsync(): canceled: "<<State());
            return;
        }
        tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::CallDoProcessAsync(): starting: "<<State());
        thread(bind(&CPSGS_OSGProcessorBase::CallDoProcessCallback, this, guard)).detach();
        tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::CallDoProcessAsync() started: "<<State());
    }
    catch (exception& exc) {
        tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::CallDoProcessAsync() failed: "<<exc.what()<<": "<<State());
        PSG_ERROR("OSG: DoProcessAsync: failed to create thread: "<<exc.what());
        FinalizeResult(IPSGS_Processor::ePSGS_Error);
    }
}


void CPSGS_OSGProcessorBase::DoProcess()
{
    SEND_TRACE("OSG: DoProcess() started");
    try {
        tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::DoProcess() start: "<<State());
        for ( double retry_count = m_ConnectionPool->GetRetryCount(); retry_count > 0; ) {
            if ( IsCanceled() ) {
                SEND_TRACE("OSG: DoProcess() canceled 1");
                return;
            }
        
            // We need to distinguish different kinds of communication failures with different
            //   effect on retry logic.
            // 1. stale/disconnected connection failure - there maybe multiple in active connection pool
            // 2. multiple simultaneous failures from concurrent incoming requests
            // 3. repeated failure of specific request at OSG server
            // In the first case we shouldn't account all such failures in the same retry counter -
            //   it will overflow easily, and quite unnecessary.
            // In the first case we shouldn't increase wait time too much -
            //   the failures should be treated as single failure for the sake of waiting before
            //   next connection attempt.
            // In the third case we should make sure we abandon the failing request when retry limit
            //   is reached. It should be detected no matter of concurrent successful requests.
        
            bool last_attempt = retry_count <= 1;
            COSGCaller caller;
            try {
                SEND_TRACE("OSG: DoProcess() allocating connection");
                caller.AllocateConnection(m_ConnectionPool);
                SEND_TRACE_FMT("OSG: DoProcess() allocated connection: "<<caller.GetConnectionID());
            }
            catch ( exception& exc ) {
                if ( last_attempt ) {
                    PSG_ERROR("OSG: DoProcess() failed opening connection: "<<exc.what());
                    throw;
                }
                else {
                    // failed new connection - consume full retry
                    PSG_ERROR("OSG: DoProcess() retrying after failure opening connection: "<<exc.what());
                    retry_count -= 1;
                    continue;
                }
            }
        
            if ( IsCanceled() ) {
                SEND_TRACE_FMT("OSG: DoProcess() canceled 2, releasing connection: "<<caller.GetConnectionID());
                caller.ReleaseConnection();
                return;
            }
        
            try {
                caller.SendRequest(*this);
                caller.WaitForReplies(*this);
            }
            catch ( exception& exc ) {
                if ( last_attempt ) {
                    PSG_ERROR("OSG: DoProcess() failed receiving replies: "<<exc.what());
                    throw;
                }
                else {
                    // this may be failure of old connection
                    PSG_ERROR("OSG: retrying after failure receiving replies: "<<exc.what());
                    if ( caller.GetRequestPacket().Get().front()->GetSerial_number() <= 1 ) {
                        // new connection - consume full retry
                        retry_count -= 1;
                    }
                    else {
                        // old connection from pool - consume part of retry
                        retry_count -= 1./m_ConnectionPool->GetMaxConnectionCount();
                    }
                    continue;
                }
            }
        
            // successful
            break;
        }

        if ( IsCanceled() ) {
            SEND_TRACE("OSG: DoProcess() canceled 3");
            return;
        }
        tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::DoProcess() got replies: "<<State());

        if ( m_ConnectionPool->GetAsyncProcessing() ) {
            WaitForOtherProcessors();
            if ( IsCanceled() ) {
                tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::CallDoProcessRepliesCallback() canceled: "<<State());
                return;
            }
        }

        CallDoProcessReplies();
        tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::Process() done: "<<State());
    }
    catch ( exception& exc ) {
        PSG_ERROR("OSG: DoProcess() failed: "<<exc.what());
        FinalizeResult(IPSGS_Processor::ePSGS_Error);
    }
}


void CPSGS_OSGProcessorBase::DoProcessReplies()
{
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::DoProcessReplies() start: "<<State());
    SEND_TRACE("OSG: processing replies");
    ProcessReplies();
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::DoProcessReplies() return: "<<State());
    _ASSERT(m_Status != IPSGS_Processor::ePSGS_InProgress);
}


void CPSGS_OSGProcessorBase::CallDoProcessReplies()
{
    if ( 1 ) {
        CallDoProcessRepliesAsync();
    }
    else {
        CallDoProcessRepliesSync();
    }
}


void CPSGS_OSGProcessorBase::CallDoProcessRepliesSync()
{
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::CallDoProcessRepliesSync() start: "<<State());
    DoProcessReplies();
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::CallDoProcessRepliesSync() return: "<<State());
}


void CPSGS_OSGProcessorBase::CallDoProcessRepliesCallback(const CBackgroundProcessingGuard& guard_in)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    CBackgroundProcessingGuard guard(guard_in);
    if ( !guard.GetGuardedProcessor() ) {
        tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::CallDoProcessRepliesCallback() canceled: "<<State());
        return;
    }
    _ASSERT(guard.GetGuardedProcessor() == this);
    try {
        tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::CallDoProcessRepliesCallback() signal start: "<<State());
        tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::CallDoProcessRepliesCallback() start: "<<State());
        DoProcessReplies();
        tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::CallDoProcessRepliesCallback() return: "<<State());
    }
    catch ( exception& exc ) {
        tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::CallDoProcessRepliesCallback() exc: "<<exc.what()<<": "<<State());
        PSG_ERROR("OSG: ProcessReplies() failed: "<<exc.what());
        FinalizeResult(IPSGS_Processor::ePSGS_Error);
    }
}


void CPSGS_OSGProcessorBase::s_CallDoProcessRepliesUvCallback(void *data)
{
    unique_ptr<CBackgroundProcessingGuard> guard(static_cast<CBackgroundProcessingGuard*>(data));
    guard->GetGuardedProcessor()->CallDoProcessRepliesCallback(*guard);
}


void CPSGS_OSGProcessorBase::CallDoProcessRepliesAsync()
{
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::CallDoProcessRepliesAsync() start: "<<State());
    SEND_TRACE("OSG: scheduling ProcessReplies() to UV loop");
    unique_ptr<CBackgroundProcessingGuard> guard = make_unique<CBackgroundProcessingGuard>(this);
    if ( !guard->GetGuardedProcessor() ) {
        tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::CallDoProcessRepliesAsync() canceled: "<<State());
        return;
    }
    GetUvLoopBinder().PostponeInvoke(s_CallDoProcessRepliesUvCallback, guard.release());
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::CallDoProcessRepliesAsync() return: "<<State());
}


void CPSGS_OSGProcessorBase::Cancel()
{
    CMutexGuard guard(m_StatusMutex);
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::Cancel(): before: "<<State());
    SetFinalStatus(ePSGS_Canceled);
    FinalizeResult();
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::Cancel(): after: "<<State());
}


CNcbiOstream& COSGStateReporter::Print(CNcbiOstream& out) const
{
    return out << m_ProcessorPtr->m_Status << ' '
               << m_ProcessorPtr->m_BackgroundProcesing << ' '
               << m_ProcessorPtr->m_FinishSignalled;
}


void CPSGS_OSGProcessorBase::ResetReplies()
{
}


void CPSGS_OSGProcessorBase::NotifyOSGCallStart()
{
}


void CPSGS_OSGProcessorBase::NotifyOSGCallReply(const CID2_Reply& /*reply*/)
{
}


void CPSGS_OSGProcessorBase::NotifyOSGCallEnd()
{
}


void CPSGS_OSGProcessorBase::AddRequest(const CRef<CID2_Request>& req0)
{
    CRef<CID2_Request> req = req0;
    if ( 1 ) {
        // set hops
        auto hops = GetRequest()->GetRequest<SPSGS_RequestBase>().m_Hops + 1;
        CRef<CID2_Param> param(new CID2_Param);
        param->SetName("hops");
        param->SetValue().push_back(to_string(hops));
        req->SetParams().Set().push_back(param);
    }
    m_Fetches.push_back(Ref(new COSGFetch(req)));
}


IPSGS_Processor::EPSGS_Status CPSGS_OSGProcessorBase::GetStatus()
{
    //tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::GetStatus(): "<<State());
    return m_Status;
}


void CPSGS_OSGProcessorBase::SetFinalStatus(EPSGS_Status status)
{
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::SetFinalStatus(): status: "<<State());
    _ASSERT(m_Status == ePSGS_InProgress || status == m_Status ||
            m_Status == ePSGS_Canceled || status == ePSGS_Canceled);
    m_Status = status;
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::SetFinalStatus(): return: "<<State());
}


void CPSGS_OSGProcessorBase::FinalizeResult()
{
    CMutexGuard guard(m_StatusMutex);
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::FinalizeResult(): state: "<<State());
    _ASSERT(m_Status != ePSGS_InProgress);
    if ( !m_BackgroundProcesing ) {
        tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::FinalizeResult(): signal: "<<State());
        SignalFinishProcessing();
    }
}


void CPSGS_OSGProcessorBase::FinalizeResult(EPSGS_Status status)
{
    SetFinalStatus(status);
    FinalizeResult();
}


bool CPSGS_OSGProcessorBase::SignalStartOfBackgroundProcessing()
{
    CMutexGuard guard(m_StatusMutex);
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::SignalStartOfBackgroundProcessing(): "<<State());
    if ( m_Status == IPSGS_Processor::ePSGS_Canceled ) {
        tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::SignalStartOfBackgroundProcessing(): return cancel: "<<State());
        return false;
    }
    ++m_BackgroundProcesing;
    return true;
}


void CPSGS_OSGProcessorBase::SignalEndOfBackgroundProcessing()
{
    CMutexGuard guard(m_StatusMutex);
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::SignalEndOfBackgroundProcessing(): "<<State());
    _ASSERT(m_BackgroundProcesing > 0);
    if ( --m_BackgroundProcesing == 0 ) {
        tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::SignalEndOfBackgroundProcessing(): signal: "<<State());
        SignalFinishProcessing();
        tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::SignalEndOfBackgroundProcessing(): return: "<<State());
    }
}


END_NAMESPACE(osg);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;
