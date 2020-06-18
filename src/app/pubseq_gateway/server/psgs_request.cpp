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


CPSGS_Request::CPSGS_Request()
{}


CPSGS_Request::CPSGS_Request(unique_ptr<SPSGS_RequestBase> req,
                             CRef<CRequestContext>  request_context) :
    m_Request(move(req)),
    m_RequestContext(request_context)
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


void CPSGS_Request::SetRequestContext(void)
{
    if (m_RequestContext.NotNull())
        CDiagContext::SetRequestContext(m_RequestContext);
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
    if (m_Request)
        return m_Request->GetTrace() == SPSGS_RequestBase::ePSGS_WithTracing;

    NCBI_THROW(CPubseqGatewayException, eLogic,
               "User request is not initialized");
}


string CPSGS_Request::GetName(void) const
{
    if (m_Request)
        return m_Request->GetName();
    return "unknown (request is not initialized)";
}


CJsonNode CPSGS_Request::Serialize(void) const
{
    if (m_Request)
        return m_Request->Serialize();

    CJsonNode       json(CJsonNode::NewObjectNode());
    json.SetString("name", GetName());
    return json;
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


CJsonNode SPSGS_ResolveRequest::Serialize(void) const
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetString("name", GetName());
    json.SetString("seq id", m_SeqId);
    json.SetInteger("seq id type", m_SeqIdType);
    json.SetBoolean("trace", m_Trace);
    json.SetInteger("include data flags", m_IncludeDataFlags);
    json.SetInteger("output format", m_OutputFormat);
    json.SetBoolean("use cache", m_UseCache);
    json.SetInteger("subst option", m_AccSubstOption);
    return json;
}


CJsonNode SPSGS_BlobBySeqIdRequest::Serialize(void) const
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetString("name", GetName());
    json.SetString("seq id", m_SeqId);
    json.SetInteger("seq id type", m_SeqIdType);
    json.SetInteger("tse option", m_TSEOption);
    json.SetBoolean("use cache", m_UseCache);
    json.SetString("client id", m_ClientId);
    json.SetBoolean("trace", m_Trace);
    json.SetInteger("subst option", m_AccSubstOption);

    CJsonNode   exclude_blobs(CJsonNode::NewArrayNode());
    for (const auto &  blob_id : m_ExcludeBlobs) {
        exclude_blobs.AppendString(blob_id.ToString());
    }
    json.SetByKey("exclude blobs", exclude_blobs);

    return json;
}


CJsonNode SPSGS_BlobBySatSatKeyRequest::Serialize(void) const
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetString("name", GetName());
    json.SetString("blob id", m_BlobId.ToString());
    json.SetInteger("tse option", m_TSEOption);
    json.SetBoolean("use cache", m_UseCache);
    json.SetString("client id", m_ClientId);
    json.SetBoolean("trace", m_Trace);
    json.SetInteger("last modified", m_LastModified);
    return json;
}


CJsonNode SPSGS_AnnotRequest::Serialize(void) const
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetString("name", GetName());
    json.SetString("seq id", m_SeqId);
    json.SetInteger("seq id type", m_SeqIdType);
    json.SetBoolean("trace", m_Trace);
    json.SetBoolean("use cache", m_UseCache);

    CJsonNode   names(CJsonNode::NewArrayNode());
    for (const auto &  name : m_Names) {
        names.AppendString(name);
    }
    json.SetByKey("names", names);

    return json;
}


CJsonNode SPSGS_TSEChunkRequest::Serialize(void) const
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetString("name", GetName());
    json.SetString("tse id", m_TSEId.ToString());
    json.SetInteger("chunk", m_Chunk);
    json.SetInteger("split version", m_SplitVersion);
    json.SetBoolean("use cache", m_UseCache);
    json.SetBoolean("trace", m_Trace);
    return json;
}

