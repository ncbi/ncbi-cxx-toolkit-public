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
#include <objects/seqloc/Seq_id.hpp>

#include "pubseq_gateway_utils.hpp"
#include "psgs_request.hpp"
#include "psgs_reply.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


// see CXX-10728
// Need to replace the found accession with the seq_ids found accession
EPSGS_AccessionAdjustmentResult
SBioseqResolution::AdjustAccession(shared_ptr<CPSGS_Request>  request,
                                   shared_ptr<CPSGS_Reply>  reply)
{
    if (m_AdjustmentTried)
        return m_AccessionAdjustmentResult;
    m_AdjustmentTried = true;

    if (m_ResolutionResult != ePSGS_BioseqDB && m_ResolutionResult != ePSGS_BioseqCache) {
        m_AdjustmentError = "BIOSEQ_INFO accession adjustment logic error. The "
                            "data are not ready for adjustments.";
        m_AccessionAdjustmentResult = ePSGS_LogicError;
        return m_AccessionAdjustmentResult;
    }

    auto    seq_id_type = m_BioseqInfo.GetSeqIdType();
    if (m_BioseqInfo.GetVersion() > 0 && seq_id_type != CSeq_id::e_Gi) {
        if (request->NeedTrace())
            reply->SendTrace("No need to adjust accession",
                             request->GetStartTimestamp());

        m_AccessionAdjustmentResult = ePSGS_NotRequired;
        return m_AccessionAdjustmentResult;
    }

    auto &    seq_ids = m_BioseqInfo.GetSeqIds();
    for (const auto &  seq_id : seq_ids) {
        if (get<0>(seq_id) == CSeq_id::e_Gi) {
            string  orig_accession = m_BioseqInfo.GetAccession();
            auto    orig_seq_id_type = m_BioseqInfo.GetSeqIdType();

            m_BioseqInfo.SetAccession(get<1>(seq_id));
            m_BioseqInfo.SetSeqIdType(CSeq_id::e_Gi);

            seq_ids.erase(seq_id);
            if (orig_seq_id_type != CSeq_id::e_Gi)
                seq_ids.insert(make_tuple(orig_seq_id_type, orig_accession));

            if (request->NeedTrace())
                reply->SendTrace("Accession adjusted with Gi",
                                 request->GetStartTimestamp());

            m_AccessionAdjustmentResult = ePSGS_AdjustedWithGi;
            return m_AccessionAdjustmentResult;
        }
    }

    if (seq_ids.empty()) {
        m_AdjustmentError = "BIOSEQ_INFO data inconsistency. Accession " +
                            m_BioseqInfo.GetAccession() + " needs to be "
                            "adjusted but the seq_ids list is empty.";
        m_AccessionAdjustmentResult = ePSGS_SeqIdsEmpty;
        return m_AccessionAdjustmentResult;
    }

    // Adjusted with any
    string  orig_accession = m_BioseqInfo.GetAccession();
    auto    orig_seq_id_type = m_BioseqInfo.GetSeqIdType();

    auto    first_seq_id = seq_ids.begin();
    m_BioseqInfo.SetAccession(get<1>(*first_seq_id));
    m_BioseqInfo.SetSeqIdType(get<0>(*first_seq_id));

    seq_ids.erase(*first_seq_id);
    if (orig_seq_id_type != CSeq_id::e_Gi)
        seq_ids.insert(make_tuple(orig_seq_id_type, orig_accession));

    if (request->NeedTrace())
        reply->SendTrace("Accession adjusted with type " +
                         to_string(m_BioseqInfo.GetSeqIdType()) +
                         " (first from the seq_ids list)",
                         request->GetStartTimestamp());

    m_AccessionAdjustmentResult = ePSGS_AdjustedWithAny;
    return m_AccessionAdjustmentResult;
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
static string   s_NA = "na=";
static string   s_Reason = "reason=";
static string   s_NChunksOne = "n_chunks=1";
static string   s_ProcessorId = "processor_id=";
static string   s_Id2Info = "id2_info=";
static string   s_TSEId = "tse_id=";
static string   s_TSELastModified = "tse_last_modified=";

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
static string   s_BioseqNA = "bioseq_na";
static string   s_Excluded = "excluded";
static string   s_InProgress = "inprogress";
static string   s_Sent = "sent";
static string   s_Processor = "processor";

// Combinations
static string   s_AndSize = "&" + s_Size;
static string   s_AndStatus = "&" + s_Status;
static string   s_AndCode = "&" + s_Code;
static string   s_AndSeverity = "&" + s_Severity;
static string   s_AndNChunks = "&" + s_NChunks;
static string   s_AndBlobId = "&" + s_BlobId;
static string   s_AndBlobChunk = "&" + s_BlobChunk;
static string   s_AndNChunksOne = "&" + s_NChunksOne;
static string   s_AndReason = "&" + s_Reason;
static string   s_AndNA = "&" + s_NA;
static string   s_BioseqInfoItem = s_ItemType + s_BioseqInfo;
static string   s_AndBioseqInfoItem = "&" + s_BioseqInfoItem;
static string   s_BlobPropItem = s_ItemType + s_BlobProp;
static string   s_AndBlobPropItem = "&" + s_BlobPropItem;
static string   s_BioseqNAItem = s_ItemType + s_BioseqNA;
static string   s_AndBioseqNAItem = "&" + s_BioseqNAItem;
static string   s_BlobItem = s_ItemType + s_Blob;
static string   s_AndBlobItem = "&" + s_BlobItem;
static string   s_ReplyItem = s_ItemType + s_Reply;
static string   s_AndReplyItem = "&" + s_ReplyItem;
static string   s_ProcessorItem = s_ItemType + s_Processor;
static string   s_AndProcessorItem = "&" + s_ProcessorItem;
static string   s_AndProcessorId = "&" + s_ProcessorId;
static string   s_AndId2Info = "&" + s_Id2Info;
static string   s_AndTSEId = "&" + s_TSEId;
static string   s_AndTSELastModified = "&" + s_TSELastModified;

static string   s_DataChunk = s_ChunkType + s_Data;
static string   s_AndDataChunk = "&" + s_DataChunk;
static string   s_MetaChunk = s_ChunkType + s_Meta;
static string   s_AndMetaChunk = "&" + s_MetaChunk;
static string   s_MessageChunk = s_ChunkType + s_Message;
static string   s_AndMessageChunk = "&" + s_MessageChunk;
static string   s_FmtJson = s_Fmt + s_Json;
static string   s_AndFmtJson = "&" + s_FmtJson;
static string   s_FmtProtobuf = s_Fmt + s_Protobuf;
static string   s_AndFmtProtobuf = "&" + s_FmtProtobuf;

static string   s_ReplyBegin = s_ProtocolPrefix + s_ItemId;
static string   s_ReplyCompletionFixedPart = s_ReplyBegin + "0&" +
                                             s_ReplyItem + "&" +
                                             s_MetaChunk + "&" + s_NChunks;


static string SeverityToLowerString(EDiagSev  severity)
{
    string  severity_as_string = CNcbiDiag::SeverityName(severity);
    NStr::ToLower(severity_as_string);
    return severity_as_string;
}


string  GetBioseqInfoHeader(size_t  item_id,
                            const string &  processor_id,
                            size_t  bioseq_info_size,
                            SPSGS_ResolveRequest::EPSGS_OutputFormat  output_format)
{
    // E.g. PSG-Reply-Chunk: item_id=1&processor_id=get+blob+proc&item_type=bioseq_info&chunk_type=data&size=450&fmt=protobuf
    string      reply(s_ReplyBegin);

    reply.append(to_string(item_id))
         .append(s_AndProcessorId)
         .append(NStr::URLEncode(processor_id))
         .append(s_AndBioseqInfoItem)
         .append(s_AndDataChunk)
         .append(s_AndSize)
         .append(to_string(bioseq_info_size));
    if (output_format == SPSGS_ResolveRequest::ePSGS_JsonFormat)
        reply.append(s_AndFmtJson);
    else
        reply.append(s_AndFmtProtobuf);
    return reply.append(1, '\n');
}


string  GetBioseqMessageHeader(size_t  item_id,
                               const string &  processor_id,
                               size_t  msg_size,
                               CRequestStatus::ECode  status,
                               int  code,
                               EDiagSev  severity)
{
    string      reply(s_ReplyBegin);

    return reply.append(to_string(item_id))
                .append(s_AndProcessorId)
                .append(NStr::URLEncode(processor_id))
                .append(s_AndBioseqInfoItem)
                .append(s_AndMessageChunk)
                .append(s_AndSize)
                .append(to_string(msg_size))
                .append(s_AndStatus)
                .append(to_string(static_cast<int>(status)))
                .append(s_AndCode)
                .append(to_string(code))
                .append(s_AndSeverity)
                .append(SeverityToLowerString(severity))
                .append(1, '\n');
}


string  GetBioseqCompletionHeader(size_t  item_id,
                                  const string &  processor_id,
                                  size_t  chunk_count)
{
    // E.g. PSG-Reply-Chunk: item_id=1&processor_id=get+blob+proc&item_type=bioseq_info&chunk_type=meta&n_chunks=1
    string      reply(s_ReplyBegin);

    return reply.append(to_string(item_id))
                .append(s_AndProcessorId)
                .append(NStr::URLEncode(processor_id))
                .append(s_AndBioseqInfoItem)
                .append(s_AndMetaChunk)
                .append(s_AndNChunks)
                .append(to_string(chunk_count))
                .append(1, '\n');
}


string  GetBlobPropHeader(size_t  item_id,
                          const string &  processor_id,
                          const string &  blob_id,
                          size_t  blob_prop_size)
{
    // E.g. PSG-Reply-Chunk: item_id=2&processor_id=get+blob+proc&item_type=blob_prop&chunk_type=data&size=550&sat=111
    string      reply(s_ReplyBegin);

    return reply.append(to_string(item_id))
                .append(s_AndProcessorId)
                .append(NStr::URLEncode(processor_id))
                .append(s_AndBlobPropItem)
                .append(s_AndDataChunk)
                .append(s_AndSize)
                .append(to_string(blob_prop_size))
                .append(s_AndBlobId)
                .append(blob_id)
                .append(1, '\n');
}


string  GetBlobPropMessageHeader(size_t  item_id,
                                 const string &  processor_id,
                                 size_t  msg_size,
                                 CRequestStatus::ECode  status,
                                 int  code,
                                 EDiagSev  severity)
{
    string      reply(s_ReplyBegin);

    return reply.append(to_string(item_id))
                .append(s_AndProcessorId)
                .append(NStr::URLEncode(processor_id))
                .append(s_AndBlobPropItem)
                .append(s_AndMessageChunk)
                .append(s_AndSize)
                .append(to_string(msg_size))
                .append(s_AndStatus)
                .append(to_string(static_cast<int>(status)))
                .append(s_AndCode)
                .append(to_string(code))
                .append(s_AndSeverity)
                .append(SeverityToLowerString(severity))
                .append(1, '\n');
}


string  GetBlobPropCompletionHeader(size_t  item_id,
                                    const string &  processor_id,
                                    size_t  chunk_count)
{
    string      reply(s_ReplyBegin);

    return reply.append(to_string(item_id))
                .append(s_AndProcessorId)
                .append(NStr::URLEncode(processor_id))
                .append(s_AndBlobPropItem)
                .append(s_AndMetaChunk)
                .append(s_AndNChunks)
                .append(to_string(chunk_count))
                .append(1, '\n');
}


string  GetBlobChunkHeader(size_t  item_id,
                           const string &  processor_id,
                           const string &  blob_id,
                           size_t  chunk_size,
                           size_t  chunk_number)
{
    // E.g. PSG-Reply-Chunk: item_id=3&processor_id=get+blob+proc&item_type=blob&chunk_type=data&size=2345&blob_id=333.444&blob_chunk=37
    string      reply(s_ReplyBegin);

    return reply.append(to_string(item_id))
                .append(s_AndProcessorId)
                .append(NStr::URLEncode(processor_id))
                .append(s_AndBlobItem)
                .append(s_AndDataChunk)
                .append(s_AndSize)
                .append(to_string(chunk_size))
                .append(s_AndBlobId)
                .append(blob_id)
                .append(s_AndBlobChunk)
                .append(to_string(chunk_number))
                .append(1, '\n');
}


string  GetTSEBlobChunkHeader(size_t  item_id,
                              const string &  processor_id,
                              const string &  blob_id,
                              size_t  chunk_size,
                              size_t  chunk_number,
                              const string &  id2_info,
                              const string &  tse_id,
                              int64_t  last_modified)
{
    // E.g. PSG-Reply-Chunk: item_id=3&processor_id=get+blob+proc&item_type=blob&chunk_type=data&size=2345&blob_id=333.444&blob_chunk=37
    string      reply(s_ReplyBegin);

    return reply.append(to_string(item_id))
                .append(s_AndProcessorId)
                .append(NStr::URLEncode(processor_id))
                .append(s_AndBlobItem)
                .append(s_AndDataChunk)
                .append(s_AndSize)
                .append(to_string(chunk_size))
                .append(s_AndBlobId)
                .append(blob_id)
                .append(s_AndBlobChunk)
                .append(to_string(chunk_number))
                .append(s_AndId2Info)
                .append(id2_info)
                .append(s_AndTSEId)
                .append(tse_id)
                .append(s_AndTSELastModified)
                .append(to_string(last_modified))
                .append(1, '\n');
}


string  GetBlobExcludeHeader(size_t  item_id,
                             const string &  processor_id,
                             const string &  blob_id,
                             EPSGS_BlobSkipReason  skip_reason)
{
    // E.g. PSG-Reply-Chunk: item_id=5&processor_id=get+blob+proc&item_type=blob&chunk_type=meta&blob_id=555.666&n_chunks=1&reason={excluded,inprogress,sent}
    string      reason;
    switch (skip_reason) {
        case ePSGS_BlobExcluded:
            reason = s_Excluded;
            break;
        case ePSGS_BlobInProgress:
            reason = s_InProgress;
            break;
        case ePSGS_BlobSent:
            reason = s_Sent;
            break;
    }

    string      reply(s_ReplyBegin);

    return reply.append(to_string(item_id))
                .append(s_AndProcessorId)
                .append(NStr::URLEncode(processor_id))
                .append(s_AndBlobItem)
                .append(s_AndMetaChunk)
                .append(s_AndBlobId)
                .append(blob_id)
                .append(s_AndNChunksOne)
                .append(s_AndReason)
                .append(reason)
                .append(1, '\n');
}

string  GetBlobCompletionHeader(size_t  item_id,
                                const string &  processor_id,
                                const string &  blob_id,
                                size_t  chunk_count)
{
    // E.g. PSG-Reply-Chunk: item_id=4&processor_id=get+blob+proc&item_type=blob&chunk_type=meta&blob_id=333.444&n_chunks=100
    string      reply(s_ReplyBegin);

    return reply.append(to_string(item_id))
                .append(s_AndProcessorId)
                .append(NStr::URLEncode(processor_id))
                .append(s_AndBlobItem)
                .append(s_AndMetaChunk)
                .append(s_AndBlobId)
                .append(blob_id)
                .append(s_AndNChunks)
                .append(to_string(chunk_count))
                .append(1, '\n');
}


string  GetBlobMessageHeader(size_t  item_id,
                             const string &  processor_id,
                             const string &  blob_id,
                             size_t  msg_size,
                             CRequestStatus::ECode  status,
                             int  code,
                             EDiagSev  severity)
{
    // E.g. PSG-Reply-Chunk: item_id=3&processor_id=get+blob+proc&item_type=blob&chunk_type=message&size=22&blob_id=333.444&status=404&code=5&severity=critical
    string      reply(s_ReplyBegin);

    return reply.append(to_string(item_id))
                .append(s_AndProcessorId)
                .append(NStr::URLEncode(processor_id))
                .append(s_AndBlobItem)
                .append(s_AndMessageChunk)
                .append(s_AndSize)
                .append(to_string(msg_size))
                .append(s_AndBlobId)
                .append(blob_id)
                .append(s_AndStatus)
                .append(to_string(static_cast<int>(status)))
                .append(s_AndCode)
                .append(to_string(code))
                .append(s_AndSeverity)
                .append(SeverityToLowerString(severity))
                .append(1, '\n');
}


string  GetReplyCompletionHeader(size_t  chunk_count)
{
    // E.g. PSG-Reply-Chunk: item_id=0&item_type=reply&chunk_type=meta&n_chunks=153
    string      reply(s_ReplyCompletionFixedPart);

    return reply.append(to_string(chunk_count))
                .append(1, '\n');
}


string  GetReplyMessageHeader(size_t  msg_size,
                              CRequestStatus::ECode  status,
                              int  code,
                              EDiagSev  severity)
{
    // E.g. PSG-Reply-Chunk: item_id=0&item_type=reply&chunk_type=message&size=22&status=404&code=5&severity=critical
    string      reply(s_ReplyBegin);

    return reply.append(1, '0')
                .append(s_AndReplyItem)
                .append(s_AndMessageChunk)
                .append(s_AndSize)
                .append(to_string(msg_size))
                .append(s_AndStatus)
                .append(to_string(static_cast<int>(status)))
                .append(s_AndCode)
                .append(to_string(code))
                .append(s_AndSeverity)
                .append(SeverityToLowerString(severity))
                .append(1, '\n');
}


string GetNamedAnnotationHeader(size_t  item_id,
                                const string &  processor_id,
                                const string &  annot_name,
                                size_t  annotation_size)
{
    // E.g. PSG-Reply-Chunk: item_id=1&processor_id=get+blob+proc&item_type=bioseq_na&chunk_type=data&size=150&na=NA000111.1
    string      reply(s_ReplyBegin);

    return reply.append(to_string(item_id))
                .append(s_AndProcessorId)
                .append(NStr::URLEncode(processor_id))
                .append(s_AndBioseqNAItem)
                .append(s_AndDataChunk)
                .append(s_AndSize)
                .append(to_string(annotation_size))
                .append(s_AndNA)
                .append(annot_name)
                .append(1, '\n');
}


string GetNamedAnnotationMessageHeader(size_t  item_id,
                                       const string &  processor_id,
                                       size_t  msg_size,
                                       CRequestStatus::ECode  status,
                                       int  code,
                                       EDiagSev  severity)
{
    // E.g. PSG-Reply-Chunk: item_id=5&processor_id=get+blob+proc&item_type=reply&chunk_type=message&size=22&status=404&code=5&severity=critical

    string      reply(s_ReplyBegin);

    return reply.append(to_string(item_id))
                .append(s_AndProcessorId)
                .append(NStr::URLEncode(processor_id))
                .append(s_AndBioseqNAItem)
                .append(s_AndMessageChunk)
                .append(s_AndSize)
                .append(to_string(msg_size))
                .append(s_AndStatus)
                .append(to_string(static_cast<int>(status)))
                .append(s_AndCode)
                .append(to_string(code))
                .append(s_AndSeverity)
                .append(SeverityToLowerString(severity))
                .append(1, '\n');
}


string GetNamedAnnotationMessageCompletionHeader(size_t  item_id,
                                                 const string &  processor_id,
                                                 size_t  chunk_count)
{
    string      reply(s_ReplyBegin);

    return reply.append(to_string(item_id))
                .append(s_AndProcessorId)
                .append(NStr::URLEncode(processor_id))
                .append(s_AndBioseqNAItem)
                .append(s_AndMetaChunk)
                .append(s_AndNChunks)
                .append(to_string(chunk_count))
                .append(1, '\n');
}


string GetNamedAnnotationCompletionHeader(size_t  item_id,
                                          const string &  processor_id,
                                          size_t  chunk_count)
{
    string      reply(s_ReplyBegin);

    return reply.append(to_string(item_id))
                .append(s_AndProcessorId)
                .append(NStr::URLEncode(processor_id))
                .append(s_AndBioseqNAItem)
                .append(s_AndMetaChunk)
                .append(s_AndNChunks)
                .append(to_string(chunk_count))
                .append(1, '\n');
}


string GetProcessorMessageHeader(size_t  item_id,
                                 const string &  processor_id,
                                 size_t  msg_size,
                                 CRequestStatus::ECode  status,
                                 int  code,
                                 EDiagSev  severity)
{
    string      reply(s_ReplyBegin);

    return reply.append(to_string(item_id))
                .append(s_AndProcessorId)
                .append(NStr::URLEncode(processor_id))
                .append(s_AndProcessorItem)
                .append(s_AndMessageChunk)
                .append(s_AndSize)
                .append(to_string(msg_size))
                .append(s_AndStatus)
                .append(to_string(static_cast<int>(status)))
                .append(s_AndCode)
                .append(to_string(code))
                .append(s_AndSeverity)
                .append(SeverityToLowerString(severity))
                .append(1, '\n');
}


string GetProcessorMessageCompletionHeader(size_t  item_id,
                                           const string &  processor_id,
                                           size_t  chunk_count)
{
    string      reply(s_ReplyBegin);

    return reply.append(to_string(item_id))
                .append(s_AndProcessorId)
                .append(NStr::URLEncode(processor_id))
                .append(s_AndProcessorItem)
                .append(s_AndMetaChunk)
                .append(s_AndNChunks)
                .append(to_string(chunk_count))
                .append(1, '\n');
}



extern bool  g_Log;
CRequestContextResetter::~CRequestContextResetter()
{
    if (g_Log)
        CDiagContext::SetRequestContext(NULL);
}


string FormatPreciseTime(const chrono::system_clock::time_point &  t_point)
{
    std::time_t             t = chrono::system_clock::to_time_t(t_point);
    chrono::milliseconds    t_ms = chrono::duration_cast<chrono::milliseconds>
                                                    (t_point.time_since_epoch());

    struct tm               local_time;
    localtime_r(&t, &local_time);

    char                    buffer[64];
    size_t                  char_count = strftime(buffer, 64,
                                                  "%Y-%m-%d %H:%M:%S",
                                                  &local_time);
    sprintf(&buffer[char_count], ".%03ld", t_ms.count() % 1000);
    return buffer;
}



static map<EPSGS_StartupDataState, string> s_CassStartupDataStateMsg =
    { {ePSGS_NoCassConnection, "Cassandra DB connection is not established"},
      {ePSGS_NoValidCassMapping, "Cassanda DB mapping configuration is invalid"},
      {ePSGS_NoCassCache, "LMDB cache is not initialized"},
      {ePSGS_StartupDataOK, "Cassandra DB mapping data are OK"} };
string GetCassStartupDataStateMessage(EPSGS_StartupDataState  state)
{
    return s_CassStartupDataStateMsg[state];
}

