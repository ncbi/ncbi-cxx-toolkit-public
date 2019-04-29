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

#include "pending_operation.hpp"
#include "protocol_utils.hpp"
#include "pubseq_gateway_utils.hpp"
#include "cass_fetch.hpp"


void CProtocolUtils::SetReply(HST::CHttpReply<CPendingOperation> *  reply)
{
    m_Reply = reply;
}


HST::CHttpReply<CPendingOperation> *  CProtocolUtils::GetReply(void)
{
    return m_Reply;
}


bool CProtocolUtils::IsReplyFinished(void) const
{
    return m_Reply->IsFinished();
}


size_t CProtocolUtils::GetItemId(void)
{
    return ++m_NextItemId;
}


void CProtocolUtils::Flush(void)
{
    m_Reply->Send(m_Chunks, true);
    m_Chunks.clear();
}


void CProtocolUtils::Flush(bool  is_last)
{
    m_Reply->Send(m_Chunks, is_last);
    m_Chunks.clear();
}


void CProtocolUtils::Clear(void)
{
    m_Chunks.clear();
    m_Reply = nullptr;
    m_TotalSentReplyChunks = 0;
}


void CProtocolUtils::PrepareBioseqMessage(size_t  item_id,
                                          const string &  msg,
                                          CRequestStatus::ECode  status,
                                          int  err_code,
                                          EDiagSev  severity)
{
    string  header = GetBioseqMessageHeader(item_id, msg.size(), status,
                                            err_code, severity);
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(msg.data()), msg.size()));
    ++m_TotalSentReplyChunks;
}



void CProtocolUtils::PrepareBioseqData(size_t  item_id,
                                       const string &  content,
                                       EOutputFormat  output_format)
{
    string      header = GetBioseqInfoHeader(item_id, content.size(),
                                             output_format);
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(content.data()), content.size()));
    ++m_TotalSentReplyChunks;
}


void CProtocolUtils::PrepareBioseqCompletion(size_t  item_id,
                                             size_t  chunk_count)
{
    string      bioseq_meta = GetBioseqCompletionHeader(item_id, chunk_count);
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(bioseq_meta.data()),
                bioseq_meta.size()));
    ++m_TotalSentReplyChunks;
}


void CProtocolUtils::PrepareBlobPropMessage(size_t                 item_id,
                                            const string &         msg,
                                            CRequestStatus::ECode  status,
                                            int                    err_code,
                                            EDiagSev               severity)
{
    string      header = GetBlobPropMessageHeader(item_id, msg.size(),
                                                  status, err_code, severity);
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(msg.data()), msg.size()));
    ++m_TotalSentReplyChunks;
}


void CProtocolUtils::PrepareBlobPropMessage(CCassBlobFetch *       fetch_details,
                                            const string &         msg,
                                            CRequestStatus::ECode  status,
                                            int                    err_code,
                                            EDiagSev               severity)
{
    PrepareBlobPropMessage(fetch_details->GetBlobPropItemId(this),
                           msg, status, err_code, severity);
    fetch_details->IncrementTotalSentBlobChunks();
}


void CProtocolUtils::PrepareBlobPropData(CCassBlobFetch *  fetch_details,
                                         const string &    content)
{
    string  header = GetBlobPropHeader(fetch_details->GetBlobPropItemId(this),
                                       fetch_details->GetBlobId(),
                                       content.size());
    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(content.data()),
                    content.size()));
    fetch_details->IncrementTotalSentBlobChunks();
    ++m_TotalSentReplyChunks;
}


void CProtocolUtils::PrepareBlobData(CCassBlobFetch *       fetch_details,
                                     const unsigned char *  chunk_data,
                                     unsigned int           data_size,
                                     int                    chunk_no)
{
    fetch_details->IncrementTotalSentBlobChunks();
    ++m_TotalSentReplyChunks;

    string  header = GetBlobChunkHeader(
                            fetch_details->GetBlobChunkItemId(this),
                            fetch_details->GetBlobId(), data_size, chunk_no);
    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(header.data()),
                    header.size()));

    if (data_size > 0 && chunk_data != nullptr)
        m_Chunks.push_back(m_Reply->PrepareChunk(chunk_data, data_size));
}


void CProtocolUtils::PrepareBlobPropCompletion(size_t  item_id,
                                               size_t  chunk_count)
{
    string      blob_prop_meta = GetBlobPropCompletionHeader(item_id,
                                                             chunk_count);
    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(blob_prop_meta.data()),
                    blob_prop_meta.size()));
    ++m_TotalSentReplyChunks;
}


void CProtocolUtils::PrepareBlobPropCompletion(CCassBlobFetch *  fetch_details)
{
    // +1 is for the completion itself
    PrepareBlobPropCompletion(fetch_details->GetBlobPropItemId(this),
                              fetch_details->GetTotalSentBlobChunks() + 1);

    // From now the counter will count chunks for the blob data
    fetch_details->ResetTotalSentBlobChunks();
    fetch_details->SetBlobPropSent();
}


void CProtocolUtils::PrepareBlobMessage(size_t                 item_id,
                                        const SBlobId &        blob_id,
                                        const string &         msg,
                                        CRequestStatus::ECode  status,
                                        int                    err_code,
                                        EDiagSev               severity)
{
    string      header = GetBlobMessageHeader(item_id, blob_id, msg.size(),
                                              status, err_code, severity);
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(msg.data()), msg.size()));
    ++m_TotalSentReplyChunks;
}


void CProtocolUtils::PrepareBlobMessage(CCassBlobFetch *       fetch_details,
                                        const string &         msg,
                                        CRequestStatus::ECode  status,
                                        int                    err_code,
                                        EDiagSev               severity)
{
    PrepareBlobMessage(fetch_details->GetBlobChunkItemId(this),
                       fetch_details->GetBlobId(),
                       msg, status, err_code, severity);
    fetch_details->IncrementTotalSentBlobChunks();
}


void CProtocolUtils::PrepareBlobCompletion(size_t           item_id,
                                           const SBlobId &  blob_id,
                                           size_t           chunk_count)
{
    string completion = GetBlobCompletionHeader(item_id, blob_id, chunk_count);
    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(completion.data()),
                    completion.size()));
    ++m_TotalSentReplyChunks;
}


void CProtocolUtils::PrepareBlobExcluded(size_t  item_id,
                                         const SBlobId &  blob_id,
                                         EBlobSkipReason  skip_reason)
{
    string  exclude = GetBlobExcludeHeader(item_id, blob_id, skip_reason);
    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(exclude.data()),
                    exclude.size()));
    ++m_TotalSentReplyChunks;
}


void CProtocolUtils::PrepareBlobCompletion(CCassBlobFetch *  fetch_details)
{
    // +1 is for the completion itself
    PrepareBlobCompletion(fetch_details->GetBlobChunkItemId(this),
                          fetch_details->GetBlobId(),
                          fetch_details->GetTotalSentBlobChunks() + 1);
    fetch_details->IncrementTotalSentBlobChunks();
}


void CProtocolUtils::PrepareReplyMessage(const string &         msg,
                                         CRequestStatus::ECode  status,
                                         int                    err_code,
                                         EDiagSev               severity)
{
    string      header = GetReplyMessageHeader(msg.size(),
                                               status, err_code, severity);
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(msg.data()), msg.size()));
    ++m_TotalSentReplyChunks;
}



void CProtocolUtils::PrepareNamedAnnotationData(const string &  accession,
                                                int16_t         version,
                                                int16_t         seq_id_type,
                                                const string &  annot_name,
                                                const string &  content)
{
    size_t      item_id = GetItemId();
    string      header = GetNamedAnnotationHeader(item_id, accession,
                                                  version, seq_id_type,
                                                  annot_name, content.size());
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(content.data()), content.size()));
    ++m_TotalSentReplyChunks;

    // There are always 2 chunks
    string      bioseq_na_meta = GetNamedAnnotationCompletionHeader(item_id, 2);
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(bioseq_na_meta.data()),
                bioseq_na_meta.size()));
    ++m_TotalSentReplyChunks;
}


void CProtocolUtils::PrepareReplyCompletion(void)
{
    ++m_TotalSentReplyChunks;

    string  reply_completion = GetReplyCompletionHeader(m_TotalSentReplyChunks);
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(reply_completion.data()),
                reply_completion.size()));
}


void CProtocolUtils::SendData(const string *  data_ptr,
                              EReplyMimeType  mime_type)
{
    m_Reply->SetContentType(mime_type);
    m_Reply->SetContentLength(data_ptr->length());
    m_Reply->SendOk(data_ptr->data(), data_ptr->length(), false);
}


void CProtocolUtils::Send400(const char *  msg)
{
    m_Reply->SetContentType(ePlainTextMime);
    m_Reply->Send400("Bad Request", msg);
}


void CProtocolUtils::Send404(const char *  msg)
{
    m_Reply->SetContentType(ePlainTextMime);
    m_Reply->Send404("Not Found", msg);
}


void CProtocolUtils::Send500(const char *  msg)
{
    m_Reply->SetContentType(ePlainTextMime);
    m_Reply->Send500("Internal Server Error", msg);
}


void CProtocolUtils::Send502(const char *  msg)
{
    m_Reply->SetContentType(ePlainTextMime);
    m_Reply->Send502("Bad Gateway", msg);
}


void CProtocolUtils::Send503(const char *  msg)
{
    m_Reply->SetContentType(ePlainTextMime);
    m_Reply->Send503("Service Unavailable", msg);
}

