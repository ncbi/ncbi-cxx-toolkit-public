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
 * File Description: factories, hosts etc for the NS server
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/request_ctx.hpp>

#include "ns_server_misc.hpp"
#include "ns_handler.hpp"
#include "ns_server.hpp"


USING_NCBI_SCOPE;


//////////////////////////////////////////////////////////////////////////
/// CNetScheduleConnectionFactory implemetation
IServer_ConnectionHandler* CNetScheduleConnectionFactory::Create(void)
{
    return new CNetScheduleHandler(m_Server);
}


//////////////////////////////////////////////////////////////////////////
/// CNetScheduleBackgroundHost implementation
void
CNetScheduleBackgroundHost::ReportError(ESeverity          severity,
                                        const std::string& what)
{
    if (severity == CBackgroundHost::eFatal)
        m_Server->SetShutdownFlag();
}

bool
CNetScheduleBackgroundHost::ShouldRun()
{
    return true;
}


//////////////////////////////////////////////////////////////////////////
/// CNetScheduleRequestExecutor implementation
void
CNetScheduleRequestExecutor::SubmitRequest(const CRef<CStdRequest>& request)
{
    m_Server->SubmitRequest(request);
}


//////////////////////////////////////////////////////////////////////////
/// CRequestContextPoolFactory implementation
CRequestContext* CRequestContextPoolFactory::Create()
{
    CRequestContext* rc = new CRequestContext;
    rc->AddReference();
    return rc;
}

void CRequestContextPoolFactory::Delete(CRequestContext* rc)
{
    if (rc) rc->RemoveReference();
}


//////////////////////////////////////////////////////////////////////////
// CNSRequestContextFactory implementation

CRequestContext* CNSRequestContextFactory::Get()
{
    CRequestContext* req_ctx = m_Server->GetRequestContextFromPool();
    req_ctx->SetRequestID(CRequestContext::GetNextRequestID());
    return req_ctx;
}

void CNSRequestContextFactory::Return(CRequestContext* req_ctx)
{
    m_Server->ReturnRequestContextToPool(req_ctx);
}


//////////////////////////////////////////////////////////////////////////
// CDiagnosticsGuard implementation

CDiagnosticsGuard::CDiagnosticsGuard(CNetScheduleHandler*  handler)
    : m_Handler(handler)
{
    m_Handler->InitDiagnostics();
}


CDiagnosticsGuard::~CDiagnosticsGuard(void)
{
    m_Handler->ResetDiagnostics();
}


