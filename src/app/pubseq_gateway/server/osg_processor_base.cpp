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


/////////////////////////////////////////////////////////////////////////////
// COSGProcessorRef
/////////////////////////////////////////////////////////////////////////////

COSGProcessorRef::COSGProcessorRef(CPSGS_OSGProcessorBase* ptr)
    : m_ProcessorPtr(ptr),
      m_FinalStatus(IPSGS_Processor::ePSGS_InProgress),
      m_Context(ptr->m_Context),
      m_ConnectionPool(ptr->m_ConnectionPool)
{
    _ASSERT(m_Context);
    _ASSERT(m_ConnectionPool);
    if ( ptr->GetFetches().empty() ) {
        ptr->CreateRequests();
    }
    _ASSERT(!ptr->GetFetches().empty());
}


COSGProcessorRef::~COSGProcessorRef()
{
}


COSGProcessorRef::TFetches COSGProcessorRef::GetFetches()
{
    CFastMutexGuard guard(m_ProcessorPtrMutex);
    if ( m_ProcessorPtr ) {
        return m_ProcessorPtr->GetFetches();
    }
    else {
        return TFetches();
    }
}


void COSGProcessorRef::NotifyOSGCallStart()
{
    CFastMutexGuard guard(m_ProcessorPtrMutex);
    if ( m_ProcessorPtr ) {
        m_ProcessorPtr->NotifyOSGCallStart();
    }
}


void COSGProcessorRef::NotifyOSGCallReply(const CID2_Reply& reply)
{
    CFastMutexGuard guard(m_ProcessorPtrMutex);
    if ( m_ProcessorPtr ) {
        m_ProcessorPtr->NotifyOSGCallReply(reply);
    }
}


void COSGProcessorRef::NotifyOSGCallEnd()
{
    CFastMutexGuard guard(m_ProcessorPtrMutex);
    if ( m_ProcessorPtr ) {
        m_ProcessorPtr->NotifyOSGCallEnd();
    }
}


void COSGProcessorRef::FinalizeResult(IPSGS_Processor::EPSGS_Status status)
{
    CFastMutexGuard guard(m_ProcessorPtrMutex);
    if ( m_ProcessorPtr ) {
        m_ProcessorPtr->FinalizeResult(status);
    }
}


IPSGS_Processor::EPSGS_Status COSGProcessorRef::GetStatus() const
{
    CFastMutexGuard guard(m_ProcessorPtrMutex);
    return m_ProcessorPtr? m_ProcessorPtr->GetStatus(): m_FinalStatus;
}


void COSGProcessorRef::Cancel()
{
    CFastMutexGuard guard(m_ProcessorPtrMutex);
    if ( m_ProcessorPtr ) {
        m_ProcessorPtr->Cancel();
    }
    else {
        m_FinalStatus = IPSGS_Processor::ePSGS_Cancelled;
    }
}


void COSGProcessorRef::Detach()
{
    CFastMutexGuard guard(m_ProcessorPtrMutex);
    if ( m_ProcessorPtr ) {
        m_FinalStatus = m_ProcessorPtr->GetStatus();
        m_ProcessorPtr = 0;
    }
}


void COSGProcessorRef::ProcessReplies()
{
    CFastMutexGuard guard(m_ProcessorPtrMutex);
    if ( m_ProcessorPtr ) {
        m_ProcessorPtr->ProcessReplies();
    }
}


void COSGProcessorRef::s_ProcessReplies(void *data)
{
    shared_ptr<COSGProcessorRef>& ref = *static_cast<shared_ptr<COSGProcessorRef>*>(data);
    {{
        CFastMutexGuard guard(ref->m_ProcessorPtrMutex);
        if ( ref->m_ProcessorPtr ) {
            ref->m_ProcessorPtr->ProcessReplies();
        }
    }}
    delete &ref;
}


void COSGProcessorRef::ProcessRepliesInUvLoop()
{
    CFastMutexGuard guard(m_ProcessorPtrMutex);
    if ( m_ProcessorPtr ) {
        _ASSERT(m_ProcessorPtr->m_ProcessorRef);
        CPubseqGatewayApp::GetInstance()->GetUvLoopBinder().PostponeInvoke(
            s_ProcessReplies,
            new shared_ptr<COSGProcessorRef>(m_ProcessorPtr->m_ProcessorRef));
    }
}


void COSGProcessorRef::s_Process(shared_ptr<COSGProcessorRef> processor)
{
    processor->Process();
}


void COSGProcessorRef::Process()
{
    try {
        tLOG_POST("COSGProcessorRef("<<m_ProcessorPtr<<")::Process() start: "<<GetStatus());
        for ( double retry_count = m_ConnectionPool->GetRetryCount(); retry_count > 0; ) {
            if ( IsCanceled() ) {
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
                caller.AllocateConnection(m_ConnectionPool, m_Context);
            }
            catch ( exception& exc ) {
                if ( last_attempt ) {
                    ERR_POST("OSG: failed opening connection: "<<exc.what());
                    throw;
                }
                else {
                    // failed new connection - consume full retry
                    ERR_POST("OSG: retrying after failure opening connection: "<<exc.what());
                    retry_count -= 1;
                    continue;
                }
            }
        
            if ( IsCanceled() ) {
                return;
            }
        
            try {
                caller.SendRequest(*this);
                caller.WaitForReplies(*this);
            }
            catch ( exception& exc ) {
                if ( last_attempt ) {
                    ERR_POST("OSG: failed receiving replies: "<<exc.what());
                    throw;
                }
                else {
                    // this may be failure of old connection
                    ERR_POST("OSG: retrying after failure receiving replies: "<<exc.what());
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
            return;
        }
        tLOG_POST("COSGProcessorRef("<<m_ProcessorPtr<<")::Process() got replies: "<<GetStatus());
        if ( 1 ) {
            ProcessRepliesInUvLoop();
            tLOG_POST("COSGProcessorRef("<<m_ProcessorPtr<<")::Process() return with async post: "<<GetStatus());
        }
        else {
            ProcessReplies();
            tLOG_POST("COSGProcessorRef("<<m_ProcessorPtr<<")::Process() finished: "<<GetStatus());
            _ASSERT(GetStatus() != IPSGS_Processor::ePSGS_InProgress);
            tLOG_POST("COSGProcessorRef("<<m_ProcessorPtr<<")::Process() return: "<<GetStatus());
        }
        tLOG_POST("COSGProcessorRef("<<m_ProcessorPtr<<")::Process() done: "<<GetStatus());
    }
    catch ( exception& exc ) {
        ERR_POST("OSG: DoProcess() failed: "<<exc.what());
        FinalizeResult(IPSGS_Processor::ePSGS_Error);
    }
}


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
      m_Status(IPSGS_Processor::ePSGS_InProgress)
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
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::~CPSGS_OSGProcessorBase() status: "<<m_Status);
    _ASSERT(m_Status != IPSGS_Processor::ePSGS_InProgress);
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::~CPSGS_OSGProcessorBase() return: "<<m_Status);
    if ( m_ProcessorRef ) {
        m_ProcessorRef->Detach();
    }
}


void CPSGS_OSGProcessorBase::StopAsyncThread()
{
    if ( m_ProcessorRef ) {
        m_ProcessorRef->Detach();
    }
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
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::Process(): "<<m_Status);
    if ( IsCanceled() ) {
        tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::Process() canceled: "<<m_Status);
        return;
    }
    m_ProcessorRef = make_shared<COSGProcessorRef>(this);
    if ( m_ConnectionPool->GetAsyncProcessing() ) {
        ProcessAsync();
    }
    else {
        ProcessSync();
    }
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::Process() return: "<<m_Status);
}


void CPSGS_OSGProcessorBase::ProcessSync()
{
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::ProcessSync(): "<<m_Status);
    m_ProcessorRef->Process();
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::ProcessSync() return: "<<m_Status);
    _ASSERT(m_Status != ePSGS_InProgress);
}


void CPSGS_OSGProcessorBase::ProcessAsync()
{
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::ProcessAsync(): "<<m_Status);
    thread(bind(&COSGProcessorRef::s_Process, m_ProcessorRef)).detach();
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::ProcessAsync() started: "<<m_Status);
}


void CPSGS_OSGProcessorBase::s_ProcessReplies(void* proc)
{
    auto processor = static_cast<CPSGS_OSGProcessorBase*>(proc);
    tLOG_POST("CPSGS_OSGProcessorBase("<<proc<<")::ProcessReplies() start: "<<processor->m_Status);
    processor->ProcessReplies();
    tLOG_POST("CPSGS_OSGProcessorBase("<<proc<<")::ProcessReplies() done: "<<processor->m_Status);
}


void CPSGS_OSGProcessorBase::Cancel()
{
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::Cancel()");
    SetFinalStatus(ePSGS_Cancelled);
    FinalizeResult();
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
    m_Fetches.push_back(Ref(new COSGFetch(req, m_Context)));
}


IPSGS_Processor::EPSGS_Status CPSGS_OSGProcessorBase::GetStatus()
{
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::GetStatus(): "<<m_Status);
    return m_Status;
}


void CPSGS_OSGProcessorBase::SetFinalStatus(EPSGS_Status status)
{
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::SetFinalStatus(): "<<m_Status<<" -> "<<status);
    _ASSERT(m_Status == ePSGS_InProgress || status == m_Status ||
            m_Status == ePSGS_Cancelled || status == ePSGS_Cancelled);
    m_Status = status;
}


void CPSGS_OSGProcessorBase::FinalizeResult()
{
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::FinalizeResult(): "<<m_Status);
    _ASSERT(m_Status != ePSGS_InProgress);
    SignalFinishProcessing();
}


void CPSGS_OSGProcessorBase::FinalizeResult(EPSGS_Status status)
{
    SetFinalStatus(status);
    FinalizeResult();
}


END_NAMESPACE(osg);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;
