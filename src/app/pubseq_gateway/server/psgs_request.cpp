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
#include "pubseq_gateway_exception.hpp"

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


SPSGS_ResolveRequest &  CPSGS_Request::GetResolveRequest(void)
{
    if (m_RequestType != ePSGS_ResolveRequest)
        NCBI_THROW(CPubseqGatewayException, eInvalidUserRequestType,
                   "User request type mismatch. Requested: " +
                   x_RequestTypeToString(ePSGS_ResolveRequest) +
                   " Stored: " +
                   x_RequestTypeToString(m_RequestType));
    return m_ResolveRequest;
}


SPSGS_BlobBySeqIdRequest &  CPSGS_Request::GetBlobBySeqIdRequest(void)
{
    if (m_RequestType != ePSGS_BlobBySeqIdRequest)
        NCBI_THROW(CPubseqGatewayException, eInvalidUserRequestType,
                   "User request type mismatch. Requested: " +
                   x_RequestTypeToString(ePSGS_BlobBySeqIdRequest) +
                   " Stored: " +
                   x_RequestTypeToString(m_RequestType));
    return m_BlobBySeqIdRequest;
}


SPSGS_BlobBySatSatKeyRequest &  CPSGS_Request::GetBlobBySatSatKeyRequest(void)
{
    if (m_RequestType != ePSGS_BlobBySatSatKeyRequest)
        NCBI_THROW(CPubseqGatewayException, eInvalidUserRequestType,
                   "User request type mismatch. Requested: " +
                   x_RequestTypeToString(ePSGS_BlobBySatSatKeyRequest) +
                   " Stored: " +
                   x_RequestTypeToString(m_RequestType));
    return m_BlobBySatSatKeyRequest;
}


SPSGS_AnnotRequest &  CPSGS_Request::GetAnnotRequest(void)
{
    if (m_RequestType != ePSGS_AnnotationRequest)
        NCBI_THROW(CPubseqGatewayException, eInvalidUserRequestType,
                   "User request type mismatch. Requested: " +
                   x_RequestTypeToString(ePSGS_AnnotationRequest) +
                   " Stored: " +
                   x_RequestTypeToString(m_RequestType));
    return m_AnnotRequest;
}


SPSGS_TSEChunkRequest &  CPSGS_Request::GetTSEChunkRequest(void)
{
    if (m_RequestType != ePSGS_TSEChunkRequest)
        NCBI_THROW(CPubseqGatewayException, eInvalidUserRequestType,
                   "User request type mismatch. Requested: " +
                   x_RequestTypeToString(ePSGS_TSEChunkRequest) +
                   " Stored: " +
                   x_RequestTypeToString(m_RequestType));
    return m_TSEChunkRequest;
}

