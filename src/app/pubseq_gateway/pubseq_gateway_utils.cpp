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
static string   s_Fmt = "fmt=";

// Fixed values
static string   s_BioseqInfo = "bioseq_info";
static string   s_BlobProp = "blob_prop";
static string   s_Data = "data";
static string   s_Reply = "reply";
static string   s_Blob = "blob";
static string   s_Meta = "meta";
static string   s_Message = "message";
static string   s_Protobuf = "protobuf";
static string   s_Json = "json";

// Combinations
static string   s_BioseqInfoItem = s_ItemType + s_BioseqInfo;
static string   s_BlobPropItem = s_ItemType + s_BlobProp;
static string   s_BlobItem = s_ItemType + s_Blob;
static string   s_ReplyItem = s_ItemType + s_Reply;

static string   s_DataChunk = s_ChunkType + s_Data;
static string   s_MetaChunk = s_ChunkType + s_Meta;
static string   s_MessageChunk = s_ChunkType + s_Message;
static string   s_FmtJson = s_Fmt + s_Json;
static string   s_FmtProtobuf = s_Fmt + s_Protobuf;

static string   s_ReplyBegin = s_ProtocolPrefix + s_ItemId;



static string SeverityToLowerString(EDiagSev  severity)
{
    string  severity_as_string = CNcbiDiag::SeverityName(severity);
    NStr::ToLower(severity_as_string);
    return severity_as_string;
}


string  GetBioseqInfoHeader(size_t  item_id, size_t  bioseq_info_size,
                            EOutputFormat  output_format)
{
    // E.g. PSG-Reply-Chunk: item_id=1&item_type=bioseq_info&chunk_type=data&size=450&fmt=protobuf
    string      reply;
    reply.reserve(1024);

    reply.append(s_ReplyBegin)
         .append(NStr::NumericToString(item_id))
         .append(1, '&')
         .append(s_BioseqInfoItem)
         .append(1, '&')
         .append(s_DataChunk)
         .append(1, '&')
         .append(s_Size)
         .append(NStr::NumericToString(bioseq_info_size))
         .append(1, '&');
    if (output_format == eJsonFormat)
        reply.append(s_FmtJson);
    else
        reply.append(s_FmtProtobuf);
    return reply.append(1, '\n');
}


string  GetBioseqMessageHeader(size_t  item_id, size_t  msg_size,
                               CRequestStatus::ECode  status, int  code,
                               EDiagSev  severity)
{
    string      reply;
    reply.reserve(1024);

    return reply.append(s_ReplyBegin)
                .append(NStr::NumericToString(item_id))
                .append(1, '&')
                .append(s_BioseqInfoItem)
                .append(1, '&')
                .append(s_MessageChunk)
                .append(1, '&')
                .append(s_Size)
                .append(NStr::NumericToString(msg_size))
                .append(1, '&')
                .append(s_Status)
                .append(NStr::NumericToString(static_cast<int>(status)))
                .append(1, '&')
                .append(s_Code)
                .append(NStr::NumericToString(code))
                .append(1, '&')
                .append(s_Severity)
                .append(SeverityToLowerString(severity))
                .append(1, '\n');
}


string  GetBioseqCompletionHeader(size_t  item_id, size_t  chunk_count)
{
    // E.g. PSG-Reply-Chunk: item_id=1&item_type=bioseq_info&chunk_type=meta&n_chunks=1
    string      reply;
    reply.reserve(1024);

    return reply.append(s_ReplyBegin)
                .append(NStr::NumericToString(item_id))
                .append(1, '&')
                .append(s_BioseqInfoItem)
                .append(1, '&')
                .append(s_MetaChunk)
                .append(1, '&')
                .append(s_NChunks)
                .append(NStr::NumericToString(chunk_count))
                .append(1, '\n');
}


string  GetBlobPropHeader(size_t  item_id,
                          const SBlobId &  blob_id,
                          size_t  blob_prop_size)
{
    // E.g. PSG-Reply-Chunk: item_id=2&item_type=blob_prop&chunk_type=data&size=550&sat=111
    string      reply;
    reply.reserve(1024);

    return reply.append(s_ReplyBegin)
                .append(NStr::NumericToString(item_id))
                .append(1, '&')
                .append(s_BlobPropItem)
                .append(1, '&')
                .append(s_DataChunk)
                .append(1, '&')
                .append(s_Size)
                .append(NStr::NumericToString(blob_prop_size))
                .append(1, '&')
                .append(s_BlobId)
                .append(GetBlobId(blob_id))
                .append(1, '\n');
}


string  GetBlobPropMessageHeader(size_t  item_id, size_t  msg_size,
                                 CRequestStatus::ECode  status, int  code,
                                 EDiagSev  severity)
{
    string      reply;
    reply.reserve(1024);

    return reply.append(s_ReplyBegin)
                .append(NStr::NumericToString(item_id))
                .append(1, '&')
                .append(s_BlobPropItem)
                .append(1, '&')
                .append(s_MessageChunk)
                .append(1, '&')
                .append(s_Size)
                .append(NStr::NumericToString(msg_size))
                .append(1, '&')
                .append(s_Status)
                .append(NStr::NumericToString(static_cast<int>(status)))
                .append(1, '&')
                .append(s_Code)
                .append(NStr::NumericToString(code))
                .append(1, '&')
                .append(s_Severity)
                .append(SeverityToLowerString(severity))
                .append(1, '\n');
}


string  GetBlobPropCompletionHeader(size_t  item_id, size_t  chunk_count)
{
    string      reply;
    reply.reserve(1024);

    return reply.append(s_ReplyBegin)
                .append(NStr::NumericToString(item_id))
                .append(1, '&')
                .append(s_BlobPropItem)
                .append(1, '&')
                .append(s_MetaChunk)
                .append(1, '&')
                .append(s_NChunks)
                .append(NStr::NumericToString(chunk_count))
                .append(1, '\n');
}


string  GetBlobChunkHeader(size_t  item_id, const SBlobId &  blob_id,
                           size_t  chunk_size,
                           size_t  chunk_number)
{
    // E.g. PSG-Reply-Chunk: item_id=3&item_type=blob&chunk_type=data&size=2345&blob_id=333.444&blob_chunk=37
    string      reply;
    reply.reserve(1024);

    return reply.append(s_ReplyBegin)
                .append(NStr::NumericToString(item_id))
                .append(1, '&')
                .append(s_BlobItem)
                .append(1, '&')
                .append(s_DataChunk)
                .append(1, '&')
                .append(s_Size)
                .append(NStr::NumericToString(chunk_size))
                .append(1, '&')
                .append(s_BlobId)
                .append(GetBlobId(blob_id))
                .append(1, '&')
                .append(s_BlobChunk)
                .append(NStr::NumericToString(chunk_number))
                .append(1, '\n');
}


string  GetBlobCompletionHeader(size_t  item_id, const SBlobId &  blob_id,
                                size_t  chunk_count)
{
    // E.g. PSG-Reply-Chunk: item_id=4&item_type=blob&chunk_type=meta&blob_id=333.444&n_chunks=100
    string      reply;
    reply.reserve(1024);

    return reply.append(s_ReplyBegin)
                .append(NStr::NumericToString(item_id))
                .append(1, '&')
                .append(s_BlobItem)
                .append(1, '&')
                .append(s_MetaChunk)
                .append(1, '&')
                .append(s_BlobId)
                .append(GetBlobId(blob_id))
                .append(1, '&')
                .append(s_NChunks)
                .append(NStr::NumericToString(chunk_count))
                .append(1, '\n');
}


string  GetBlobMessageHeader(size_t  item_id, const SBlobId &  blob_id,
                             size_t  msg_size,
                             CRequestStatus::ECode  status, int  code,
                             EDiagSev  severity)
{
    // E.g. PSG-Reply-Chunk: item_id=3&item_type=blob&chunk_type=message&size=22&blob_id=333.444&status=404&code=5&severity=critical
    string      reply;
    reply.reserve(1024);

    return reply.append(s_ReplyBegin)
                .append(NStr::NumericToString(item_id))
                .append(1, '&')
                .append(s_BlobItem)
                .append(1, '&')
                .append(s_MessageChunk)
                .append(1, '&')
                .append(s_Size)
                .append(NStr::NumericToString(msg_size))
                .append(1, '&')
                .append(s_BlobId)
                .append(GetBlobId(blob_id))
                .append(1, '&')
                .append(s_Status)
                .append(NStr::NumericToString(static_cast<int>(status)))
                .append(1, '&')
                .append(s_Code)
                .append(NStr::NumericToString(code))
                .append(1, '&')
                .append(s_Severity)
                .append(SeverityToLowerString(severity))
                .append(1, '\n');
}


string  GetReplyCompletionHeader(size_t  chunk_count)
{
    // E.g. PSG-Reply-Chunk: item_id=0&item_type=reply&chunk_type=meta&n_chunks=153
    string      reply;
    reply.reserve(1024);

    return reply.append(s_ReplyBegin)
                .append(1, '0')
                .append(1, '&')
                .append(s_ReplyItem)
                .append(1, '&')
                .append(s_MetaChunk)
                .append(1, '&')
                .append(s_NChunks)
                .append(NStr::NumericToString(chunk_count))
                .append(1, '\n');
}


string  GetReplyMessageHeader(size_t  msg_size,
                              CRequestStatus::ECode  status, int  code,
                              EDiagSev  severity)
{
    // E.g. PSG-Reply-Chunk: item_id=0&item_type=reply&chunk_type=message&size=22&status=404&code=5&severity=critical
    string      reply;
    reply.reserve(1024);

    return reply.append(s_ReplyBegin)
                .append(1, '0')
                .append(1, '&')
                .append(s_ReplyItem)
                .append(1, '&')
                .append(s_MessageChunk)
                .append(1, '&')
                .append(s_Size)
                .append(NStr::NumericToString(msg_size))
                .append(1, '&')
                .append(s_Status)
                .append(NStr::NumericToString(static_cast<int>(status)))
                .append(1, '&')
                .append(s_Code)
                .append(NStr::NumericToString(code))
                .append(1, '&')
                .append(s_Severity)
                .append(SeverityToLowerString(severity))
                .append(1, '\n');
}



CRequestContextResetter::CRequestContextResetter()
{}


extern bool  g_Log;
CRequestContextResetter::~CRequestContextResetter()
{
    if (g_Log)
        CDiagContext::SetRequestContext(NULL);
}
