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

#include "pubseq_gateway_utils.hpp"

USING_NCBI_SCOPE;


SBlobId::SBlobId() :
    m_Sat(-1), m_SatKey(-1)
{}


SBlobId::SBlobId(const string &  blob_id) :
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


SBlobId::SBlobId(int  sat, int  sat_key) :
    m_Sat(sat), m_SatKey(sat_key)
{}


bool SBlobId::IsValid(void) const
{
    return m_Sat >= 0 && m_SatKey >= 0;
}


bool SBlobId::operator < (const SBlobId &  other) const
{
    if (m_Sat < other.m_Sat)
        return true;
    return m_SatKey < other.m_SatKey;
}


string  GetBlobId(const SBlobId &  blob_id)
{
    return NStr::NumericToString(blob_id.m_Sat) + "." +
           NStr::NumericToString(blob_id.m_SatKey);
}


SBlobId ParseBlobId(const string &  blob_id)
{
    return SBlobId(blob_id);
}


CJsonNode  BlobPropToJSON(const CBlobRecord &  blob_prop)
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetInteger("Key", blob_prop.GetKey());
    json.SetInteger("LastModified", blob_prop.GetModified());
    json.SetInteger("Flags", blob_prop.GetFlags());
    json.SetInteger("Size", blob_prop.GetSize());
    json.SetInteger("SizeUnpacked", blob_prop.GetSizeUnpacked());
    json.SetInteger("Class", blob_prop.GetClass());
    json.SetInteger("DateAsn1", blob_prop.GetDateAsn1());
    json.SetInteger("HupDate", blob_prop.GetHupDate());
    json.SetString("Div", blob_prop.GetDiv());
    json.SetString("Id2Info", blob_prop.GetId2Info());
    json.SetInteger("Owner", blob_prop.GetOwner());
    json.SetString("UserName", blob_prop.GetUserName());
    json.SetInteger("NChunks", blob_prop.GetNChunks());

    return json;
}


static string   s_ProtocolPrefix = "\n\nPSG-Reply-Chunk: ";
static string   s_ItemType = "item_type=";
static string   s_ChunkType = "chunk_type=";
static string   s_Size = "size=";
static string   s_BlobId = "blob_id=";
static string   s_Accession = "accession=";
static string   s_BlobChunk = "blob_chunk=";
static string   s_ReplyNChunks = "reply_n_chunks=";
static string   s_BlobNChunks = "blob_n_chunks=";
static string   s_BlobNDataChunks = "blob_n_data_chunks=";
static string   s_Status = "status=";
static string   s_Code = "code=";
static string   s_Severity = "severity=";
static string   s_ReplyItem = "reply";
static string   s_BlobItem = "blob";
static string   s_ResolutionItem = "resolution";
static string   s_BlobPropItem = "blob_prop";
static string   s_BioseqInfoItem = "bioseq_info";
static string   s_Data = "data";
static string   s_Meta = "meta";
static string   s_Error = "error";
static string   s_Message = "message";

static string   s_ReplyBegin = s_ProtocolPrefix + s_ItemType;



static string SeverityToLowerString(EDiagSev  severity)
{
    string  severity_as_string = CNcbiDiag::SeverityName(severity);
    NStr::ToLower(severity_as_string);
    return severity_as_string;
}


string  GetResolutionHeader(size_t  resolution_size)
{
    return s_ReplyBegin + s_ResolutionItem +
           "&" + s_ChunkType + s_Data +
           "&" + s_Size + NStr::NumericToString(resolution_size) +
           "\n";
}


string  GetBioseqInfoHeader(size_t  bioseq_info_size, const SBlobId &  blob_id)
{
    return s_ReplyBegin + s_BioseqInfoItem +
           "&" + s_ChunkType + s_Data +
           "&" + s_Size + NStr::NumericToString(bioseq_info_size) +
           "&" + s_BlobId + GetBlobId(blob_id) +
           "\n";
}


string  GetBlobPropHeader(size_t  blob_prop_size, const SBlobId &  blob_id)
{
    return s_ReplyBegin + s_BlobPropItem +
           "&" + s_ChunkType + s_Data +
           "&" + s_Size + NStr::NumericToString(blob_prop_size) +
           "&" + s_BlobId + GetBlobId(blob_id) +
           "\n";
}


string  GetBlobChunkHeader(size_t  chunk_size, const SBlobId &  blob_id,
                           size_t  chunk_number)
{
    return s_ReplyBegin + s_BlobItem +
           "&" + s_ChunkType + s_Data +
           "&" + s_Size + NStr::NumericToString(chunk_size) +
           "&" + s_BlobId + GetBlobId(blob_id) +
           "&" + s_BlobChunk + NStr::NumericToString(chunk_number) +
           "\n";
}


string  GetBlobCompletionHeader(const SBlobId &  blob_id,
                                size_t  total_blob_data_chunks,
                                size_t  total_cnunks)
{
    return s_ReplyBegin + s_BlobItem +
           "&" + s_ChunkType + s_Meta +
           "&" + s_BlobId + GetBlobId(blob_id) +
           "&" + s_BlobNDataChunks + NStr::NumericToString(total_blob_data_chunks) +
           "&" + s_BlobNChunks + NStr::NumericToString(total_cnunks) +
           "\n";
}


string  GetBlobErrorHeader(const SBlobId &  blob_id, size_t  msg_size,
                           CRequestStatus::ECode  status, int  code,
                           EDiagSev  severity)
{
    return s_ReplyBegin + s_BlobItem +
           "&" + s_ChunkType + s_Error +
           "&" + s_Size + NStr::NumericToString(msg_size) +
           "&" + s_BlobId + GetBlobId(blob_id) +
           "&" + s_Status + NStr::NumericToString(static_cast<int>(status)) +
           "&" + s_Code + NStr::NumericToString(code) +
           "&" + s_Severity + SeverityToLowerString(severity) +
           "\n";
}


string  GetBlobMessageHeader(const SBlobId &  blob_id, size_t  msg_size,
                             CRequestStatus::ECode  status, int  code,
                             EDiagSev  severity)
{
    return s_ReplyBegin + s_BlobItem +
           "&" + s_ChunkType + s_Message +
           "&" + s_Size + NStr::NumericToString(msg_size) +
           "&" + s_BlobId + GetBlobId(blob_id) +
           "&" + s_Status + NStr::NumericToString(static_cast<int>(status)) +
           "&" + s_Code + NStr::NumericToString(code) +
           "&" + s_Severity + SeverityToLowerString(severity) +
           "\n";
}


string  GetResolutionErrorHeader(const string &  accession, size_t  msg_size,
                                 CRequestStatus::ECode  status, int  code,
                                 EDiagSev  severity)
{
    return s_ReplyBegin + s_ResolutionItem +
           "&" + s_ChunkType + s_Error +
           "&" + s_Size + NStr::NumericToString(msg_size) +
           "&" + s_Accession + accession +
           "&" + s_Status + NStr::NumericToString(static_cast<int>(status)) +
           "&" + s_Code + NStr::NumericToString(code) +
           "&" + s_Severity + SeverityToLowerString(severity) +
           "\n";
}


string  GetReplyErrorHeader(size_t  msg_size,
                            CRequestStatus::ECode  status, int  code,
                            EDiagSev  severity)
{
    return s_ReplyBegin + s_ReplyItem +
           "&" + s_ChunkType + s_Error +
           "&" + s_Size + NStr::NumericToString(msg_size) +
           "&" + s_Status + NStr::NumericToString(static_cast<int>(status)) +
           "&" + s_Code + NStr::NumericToString(code) +
           "&" + s_Severity + SeverityToLowerString(severity) +
           "\n";
}


string  GetReplyCompletionHeader(size_t  chunk_count)
{
    return s_ReplyBegin + s_ReplyItem +
           "&" + s_ChunkType + s_Meta +
           "&" + s_ReplyNChunks + NStr::NumericToString(chunk_count) +
           "\n";
}


CRequestContextResetter::CRequestContextResetter()
{}


CRequestContextResetter::~CRequestContextResetter()
{
    CDiagContext::SetRequestContext(NULL);
}
