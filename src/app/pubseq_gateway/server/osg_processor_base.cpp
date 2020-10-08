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
      m_Status(IPSGS_Processor::ePSGS_InProgress)
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
    Start();
    WaitForFinish();
}


void CPSGS_OSGProcessorBase::Cancel()
{
    // TODO
}


void CPSGS_OSGProcessorBase::CreateFetches()
{
    if ( m_Fetches.empty() ) {
        CreateRequests();
        _ASSERT(!m_Fetches.empty());
    }
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


void CPSGS_OSGProcessorBase::Start()
{
    CreateFetches();
    AllocateOSGCaller();
}


void CPSGS_OSGProcessorBase::AllocateOSGCaller()
{
    if ( !m_OSGCaller ) {
        m_Connection = m_ConnectionPool->AllocateConnection();
        m_OSGCaller = new COSGCaller(m_Connection, m_Context);
        m_OSGCaller->Process(m_Fetches);
    }
}


void CPSGS_OSGProcessorBase::WaitForFinish()
{
    _ASSERT(m_OSGCaller);
    m_OSGCaller->WaitForFinish();
    m_ConnectionPool->ReleaseConnection(m_Connection);
    ProcessReplies();
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
    GetReply()->SignalProcessorFinished();
}


void CPSGS_OSGProcessorBase::FinalizeResult(EPSGS_Status status)
{
    SetFinalStatus(status);
    FinalizeResult();
}


END_NAMESPACE(osg);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;
