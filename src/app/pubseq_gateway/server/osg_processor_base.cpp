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


BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(osg);


CPSGS_OSGProcessorBase::CPSGS_OSGProcessorBase(const CRef<COSGConnectionPool>& pool,
                                               const shared_ptr<CPSGS_Request>& request,
                                               const shared_ptr<CPSGS_Reply>& reply,
                                               TProcessorPriority priority)
    : m_Context(request->GetRequestContext()),
      m_ConnectionPool(pool),
      m_Status(IPSGS_Processor::ePSGS_InProgress),
      m_Canceled(false)
{
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
}


void CPSGS_OSGProcessorBase::Process()
{
    if ( m_Canceled ) {
        return;
    }
    if ( m_Fetches.empty() ) {
        CreateRequests();
    }
    _ASSERT(!m_Fetches.empty());

    for ( double retry_count = m_ConnectionPool->GetRetryCount(); retry_count > 0; ) {
        if ( m_Canceled ) {
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
        
        if ( m_Canceled ) {
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

    if ( m_Canceled ) {
        return;
    }
    ProcessReplies();
}


void CPSGS_OSGProcessorBase::Cancel()
{
    m_Canceled = true;
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
    return m_Status;
}


void CPSGS_OSGProcessorBase::SetFinalStatus(EPSGS_Status status)
{
    _ASSERT(m_Status == ePSGS_InProgress || status == m_Status);
    m_Status = status;
}


void CPSGS_OSGProcessorBase::FinalizeResult()
{
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
