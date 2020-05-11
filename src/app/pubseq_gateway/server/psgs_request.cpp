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
 * Authors: Sergey Satskiy
 *
 * File Description:
 *
 */
#include <ncbi_pch.hpp>

#include <corelib/ncbistr.hpp>

#include "psgs_request.hpp"

USING_NCBI_SCOPE;


SPSGS_BlobId::SPSGS_BlobId(const string &  blob_id) :
    m_Sat(-1), m_SatKey(-1)
{
    list<string>    parts;
    NStr::Split(blob_id, ".", parts);

    if (parts.size() == 2) {
        try {
            m_Sat = NStr::StringToNumeric<int>(parts.front());
            m_SatKey = NStr::StringToNumeric<int>(parts.back());
        } catch (...) {
        }
    }
}


CPSGS_Request::CPSGS_Request() :
    m_OverallStatus(CRequestStatus::e200_Ok)
{}


CPSGS_Request::CPSGS_Request(unique_ptr<SPSGS_RequestBase> req,
                             CRef<CRequestContext>  request_context) :
    m_Request(move(req)),
    m_RequestContext(request_context),
    m_OverallStatus(CRequestStatus::e200_Ok)
{}


CPSGS_Request::EPSGS_Type  CPSGS_Request::GetRequestType(void) const
{
    if (m_Request)
        return m_Request->GetRequestType();
    return ePSGS_UnknownRequest;
}


CRef<CRequestContext>  CPSGS_Request::GetRequestContext(void)
{
    return m_RequestContext;
}


CRequestStatus::ECode  CPSGS_Request::GetOverallStatus(void) const
{
    return m_OverallStatus;
}


void CPSGS_Request::UpdateOverallStatus(CRequestStatus::ECode  status)
{
    m_OverallStatus = max(status, m_OverallStatus);
}


TPSGS_HighResolutionTimePoint CPSGS_Request::GetStartTimestamp(void) const
{
    if (m_Request)
        return m_Request->GetStartTimestamp();

    NCBI_THROW(CPubseqGatewayException, eLogic,
               "User request is not initialized");
}


bool CPSGS_Request::NeedTrace(void)
{
    if (m_Request) {
        auto    trace = m_Request->GetTrace();
        if (GetRequestType() != ePSGS_ResolveRequest)
            return trace == SPSGS_RequestBase::ePSGS_WithTracing;

        // Tracing is compatible only with PSG protocol.
        // The PSG protocol may be not used in case of resolve request.
        return GetRequest<SPSGS_ResolveRequest>().m_UsePsgProtocol &&
               trace == SPSGS_RequestBase::ePSGS_WithTracing;
    }

    NCBI_THROW(CPubseqGatewayException, eLogic,
               "User request is not initialized");
}


bool CPSGS_Request::UsePsgProtocol(void)
{
    // The only resolve request can send data without PSG protocol
    if (GetRequestType() == ePSGS_ResolveRequest)
        return GetRequest<SPSGS_ResolveRequest>().m_UsePsgProtocol;

    return true;
}


string CPSGS_Request::x_RequestTypeToString(EPSGS_Type  type) const
{
    switch (type) {
        case ePSGS_ResolveRequest:
            return "ResolveRequest";
        case ePSGS_BlobBySeqIdRequest:
            return "BlobBySeqIdRequest";
        case ePSGS_BlobBySatSatKeyRequest:
            return "BlobBySatSatKeyRequest";
        case ePSGS_AnnotationRequest:
            return "AnnotationRequest";
        case ePSGS_TSEChunkRequest:
            return "TSEChunkRequest";
        case ePSGS_UnknownRequest:
            return "UnknownRequest";
        default:
            break;
    }
    return "???";
}

