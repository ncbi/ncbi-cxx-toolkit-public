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


static string   s_ProtocolPrefix = "\n\nPSG-Reply-Chunk: ";

// Names
static string   s_ItemId = "item_id=";
static string   s_ItemType = "item_type=";
static string   s_ChunkType = "chunk_type=";
static string   s_Size = "size=";
static string   s_BlobChunk = "blob_chunk=";
static string   s_NChunks = "n_chunks=";
static string   s_Status = "status=";
static string   s_Code = "code=";
static string   s_Severity = "severity=";
static string   s_BlobId = "blob_id=";

// Fixed values
static string   s_BioseqInfo = "bioseq_info";
static string   s_BlobProp = "blob_prop";
static string   s_Data = "data";
static string   s_Reply = "reply";
static string   s_Blob = "blob";
static string   s_Meta = "meta";
static string   s_Message = "message";

// Combinations
static string   s_BioseqInfoItem = s_ItemType + s_BioseqInfo;
static string   s_BlobPropItem = s_ItemType + s_BlobProp;
static string   s_BlobItem = s_ItemType + s_Blob;
static string   s_ReplyItem = s_ItemType + s_Reply;

static string   s_DataChunk = s_ChunkType + s_Data;
static string   s_MetaChunk = s_ChunkType + s_Meta;
static string   s_MessageChunk = s_ChunkType + s_Message;

static string   s_ReplyBegin = s_ProtocolPrefix + s_ItemId;



static string SeverityToLowerString(EDiagSev  severity)
{
    string  severity_as_string = CNcbiDiag::SeverityName(severity);
    NStr::ToLower(severity_as_string);
    return severity_as_string;
}


string  GetBioseqInfoHeader(size_t  item_id, size_t  bioseq_info_size)
{
    // E.g. PSG-Reply-Chunk: item_id=1&item_type=bioseq_info&chunk_type=data&size=450
    return s_ReplyBegin + NStr::NumericToString(item_id) +
           "&" + s_BioseqInfoItem +
           "&" + s_DataChunk +
           "&" + s_Size + NStr::NumericToString(bioseq_info_size) +
           "\n";
}


string  GetBioseqMessageHeader(size_t  item_id, size_t  msg_size,
                               CRequestStatus::ECode  status, int  code,
                               EDiagSev  severity)
{
    return s_ReplyBegin + NStr::NumericToString(item_id) +
           "&" + s_BioseqInfoItem +
           "&" + s_MessageChunk +
           "&" + s_Size + NStr::NumericToString(msg_size) +
           "&" + s_Status + NStr::NumericToString(static_cast<int>(status)) +
           "&" + s_Code + NStr::NumericToString(code) +
           "&" + s_Severity + SeverityToLowerString(severity) +
           "\n";
}


string  GetBioseqCompletionHeader(size_t  item_id, size_t  chunk_count)
{
    // E.g. PSG-Reply-Chunk: item_id=1&item_type=bioseq_info&chunk_type=meta&n_chunks=1
    return s_ReplyBegin + NStr::NumericToString(item_id) +
           "&" + s_BioseqInfoItem +
           "&" + s_MetaChunk +
           "&" + s_NChunks + NStr::NumericToString(chunk_count) +
           "\n";
}


string  GetBlobPropHeader(size_t  item_id,
                          const SBlobId &  blob_id,
                          size_t  blob_prop_size)
{
    // E.g. PSG-Reply-Chunk:
    // item_id=2&item_type=blob_prop&chunk_type=data&size=550&sat=111
    return s_ReplyBegin + NStr::NumericToString(item_id) +
           "&" + s_BlobPropItem +
           "&" + s_DataChunk +
           "&" + s_Size + NStr::NumericToString(blob_prop_size) +
           "&" + s_BlobId + GetBlobId(blob_id) +
           "\n";
}


string  GetBlobPropMessageHeader(size_t  item_id, size_t  msg_size,
                                 CRequestStatus::ECode  status, int  code,
                                 EDiagSev  severity)
{
    return s_ReplyBegin + NStr::NumericToString(item_id) +
           "&" + s_BlobPropItem +
           "&" + s_MessageChunk +
           "&" + s_Size + NStr::NumericToString(msg_size) +
           "&" + s_Status + NStr::NumericToString(static_cast<int>(status)) +
           "&" + s_Code + NStr::NumericToString(code) +
           "&" + s_Severity + SeverityToLowerString(severity) +
           "\n";
}


string  GetBlobPropCompletionHeader(size_t  item_id, size_t  chunk_count)
{
    return s_ReplyBegin + NStr::NumericToString(item_id) +
           "&" + s_BlobPropItem +
           "&" + s_MetaChunk +
           "&" + s_NChunks + NStr::NumericToString(chunk_count) +
           "\n";
}


string  GetBlobChunkHeader(size_t  item_id, const SBlobId &  blob_id,
                           size_t  chunk_size,
                           size_t  chunk_number)
{
    // E.g. PSG-Reply-Chunk: item_id=3&item_type=blob&chunk_type=data&size=2345&blob_id=333.444&blob_chunk=37
    return s_ReplyBegin + NStr::NumericToString(item_id) +
           "&" + s_BlobItem +
           "&" + s_DataChunk +
           "&" + s_Size + NStr::NumericToString(chunk_size) +
           "&" + s_BlobId + GetBlobId(blob_id) +
           "&" + s_BlobChunk + NStr::NumericToString(chunk_number) +
           "\n";
}


string  GetBlobCompletionHeader(size_t  item_id, const SBlobId &  blob_id,
                                size_t  chunk_count)
{
    // E.g. PSG-Reply-Chunk: item_id=4&item_type=blob&chunk_type=meta&blob_id=333.444&n_chunks=100
    return s_ReplyBegin + NStr::NumericToString(item_id) +
           "&" + s_BlobItem +
           "&" + s_MetaChunk +
           "&" + s_BlobId + GetBlobId(blob_id) +
           "&" + s_NChunks + NStr::NumericToString(chunk_count) +
           "\n";
}


string  GetBlobMessageHeader(size_t  item_id, const SBlobId &  blob_id,
                             size_t  msg_size,
                             CRequestStatus::ECode  status, int  code,
                             EDiagSev  severity)
{
    // E.g. PSG-Reply-Chunk: item_id=3&item_type=blob&chunk_type=message&size=22&blob_id=333.444&status=404&code=5&severity=critical
    return s_ReplyBegin + NStr::NumericToString(item_id) +
           "&" + s_BlobItem +
           "&" + s_MessageChunk +
           "&" + s_Size + NStr::NumericToString(msg_size) +
           "&" + s_BlobId + GetBlobId(blob_id) +
           "&" + s_Status + NStr::NumericToString(static_cast<int>(status)) +
           "&" + s_Code + NStr::NumericToString(code) +
           "&" + s_Severity + SeverityToLowerString(severity) +
           "\n";
}


string  GetReplyCompletionHeader(size_t  chunk_count)
{
    // E.g. PSG-Reply-Chunk: item_id=0&item_type=reply&chunk_type=meta&n_chunks=153
    return s_ReplyBegin + "0"
           "&" + s_ReplyItem +
           "&" + s_MetaChunk +
           "&" + s_NChunks + NStr::NumericToString(chunk_count) +
           "\n";
}


string  GetReplyMessageHeader(size_t  msg_size,
                              CRequestStatus::ECode  status, int  code,
                              EDiagSev  severity)
{
    // E.g. PSG-Reply-Chunk: item_id=0&item_type=reply&chunk_type=message&size=22&status=404&code=5&severity=critical
    return s_ReplyBegin + "0"
           "&" + s_ReplyItem +
           "&" + s_MessageChunk +
           "&" + s_Size + NStr::NumericToString(msg_size) +
           "&" + s_Status + NStr::NumericToString(static_cast<int>(status)) +
           "&" + s_Code + NStr::NumericToString(code) +
           "&" + s_Severity + SeverityToLowerString(severity) +
           "\n";
}



CRequestContextResetter::CRequestContextResetter()
{}


extern bool  g_Log;
CRequestContextResetter::~CRequestContextResetter()
{
    if (g_Log)
        CDiagContext::SetRequestContext(NULL);
}
