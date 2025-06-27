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
#include "http_reply.hpp"
#include "psgs_reply.hpp"
#include "pubseq_gateway_utils.hpp"
#include "cass_fetch.hpp"


CPSGS_Reply::~CPSGS_Reply()
{
    if (m_ReplyOwned) {
        delete m_Reply;
    }
}


void CPSGS_Reply::ConnectionCancel(void)
{
    m_ConnectionCanceled = true;
    m_Reply->NotifyClientConnectionDrop();
}


void CPSGS_Reply::Flush(EPSGS_ReplyFlush  how)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();

    if (!m_Reply->IsOutputReady()) {
        return;
    }

    if (how == ePSGS_SendAccumulated) {
        // Only chunks
        if (m_Chunks.empty()) {
            // There is nothing to send
            return;
        }

        // false => not last; do not finish the stream
        m_Reply->Send(m_Chunks, false);
    } else {
        // Chunks (if any) + set end of stream
        // true => the last; finish the stream
        m_Reply->Send(m_Chunks, true);
    }

    m_Chunks.clear();
}

void CPSGS_Reply::SetCompleted(void)
{
    x_UpdateLastActivity();
    m_Reply->SetCompleted();
}


bool CPSGS_Reply::IsCompleted(void) const
{
    return m_Reply->IsCompleted();
}


bool CPSGS_Reply::IsFinished(void) const
{
    return m_Reply->IsFinished();
}


bool CPSGS_Reply::IsOutputReady(void) const
{
    return m_Reply->IsOutputReady();
}


bool CPSGS_Reply::IsClosed(void) const
{
    return m_Reply->IsClosed();
}


void CPSGS_Reply:: SetRequestId(size_t  request_id)
{
    m_RequestId = request_id;
    m_Reply->SetRequestId(request_id);
}


bool CPSGS_Reply::GetExceedSoftLimitFlag(void) const
{
    if (m_Reply)
        return m_Reply->GetExceedSoftLimitFlag();
    return false;
}


void CPSGS_Reply::ResetExceedSoftLimitFlag(void)
{
    if (m_Reply)
        return m_Reply->ResetExceedSoftLimitFlag();
}


void CPSGS_Reply::IncrementRejectedDueToSoftLimit(void)
{
    if (m_Reply)
        m_Reply->IncrementRejectedDueToSoftLimit();
}


uint16_t CPSGS_Reply::GetConnCntAtOpen(void) const
{
    if (m_Reply)
        return m_Reply->GetConnCntAtOpen();
    return 0;
}


int64_t CPSGS_Reply::GetConnectionId(void) const
{
    if (m_Reply)
        return m_Reply->GetConnectionId();
    return 0;
}


void CPSGS_Reply::UpdatePeerIdAndUserAgent(const string &  peer_id,
                                           const string &  peer_user_agent)
{
    if (m_Reply)
        m_Reply->UpdatePeerIdAndUserAgent(peer_id, peer_user_agent);
}


void CPSGS_Reply::UpdatePeerUserAgent(const string &  peer_user_agent)
{
    if (m_Reply)
        m_Reply->UpdatePeerUserAgent(peer_user_agent);
}


void CPSGS_Reply::UpdatePeerId(const string &  peer_id)
{
    if (m_Reply)
        m_Reply->UpdatePeerId(peer_id);
}


void CPSGS_Reply::Clear(void)
{
    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();

    m_Chunks.clear();
    m_Reply = nullptr;
    m_TotalSentReplyChunks = 0;
}


shared_ptr<idblob::CCassDataCallbackReceiver> CPSGS_Reply::GetDataReadyCB(void)
{
    return m_Reply->GetDataReadyCB();
}


void CPSGS_Reply::SetContentType(EPSGS_ReplyMimeType  mime_type)
{
    // This call does not access the lower level structures
    // so it is safe to set the content type without checking that the
    // connection is canceled or closed
    m_Reply->SetContentType(mime_type);
}


void CPSGS_Reply::SetContentLength(uint64_t  content_length)
{
    if (m_ConnectionCanceled || IsFinished())
        return;
    m_Reply->SetContentLength(content_length);
}


size_t CPSGS_Reply::GetBytesSent(void) const
{
    if (m_Reply)
        return m_Reply->GetBytesSent();
    return 0;
}


void CPSGS_Reply::SendOk(const char *  payload, size_t  payload_len, bool  is_persist)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    x_UpdateLastActivity();

    m_Reply->SendOk(payload, payload_len, is_persist);

    // OK is always the last so the reply is not needed anymore.
    // SetCompleted() will let the framework to know that the pending op can be
    // discarded.
    // Typically this method would be used without the pending op so
    // SetCompleted() would not be needed but it does not harm in those cases.
    m_Reply->SetCompleted();
}


void CPSGS_Reply::Send202(const char *  payload, size_t  payload_len)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    x_UpdateLastActivity();

    m_Reply->Send202(payload, payload_len);

    // 202 is always the last so the reply is not needed anymore.
    // SetCompleted() will let the framework to know that the pending op can be
    // discarded.
    // Typically this method would be used without the pending op so
    // SetCompleted() would not be needed but it does not harm in those cases.
    m_Reply->SetCompleted();
}


void CPSGS_Reply::Send400(const char *  payload)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    x_UpdateLastActivity();

    m_Reply->Send400(payload);

    // 400 is always the last so the reply is not needed anymore.
    // SetCompleted() will let the framework to know that the pending op can be
    // discarded.
    // Typically this method would be used without the pending op so
    // SetCompleted() would not be needed but it does not harm in those cases.
    m_Reply->SetCompleted();
}


void CPSGS_Reply::Send401(const char *  payload)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    x_UpdateLastActivity();

    m_Reply->Send401(payload);

    // 401 is always the last so the reply is not needed anymore.
    // SetCompleted() will let the framework to know that the pending op can be
    // discarded.
    // Typically this method would be used without the pending op so
    // SetCompleted() would not be needed but it does not harm in those cases.
    m_Reply->SetCompleted();
}


void CPSGS_Reply::Send404(const char *  payload)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    x_UpdateLastActivity();

    m_Reply->Send404(payload);

    // 404 is always the last so the reply is not needed anymore.
    // SetCompleted() will let the framework to know that the pending op can be
    // discarded.
    // Typically this method would be used without the pending op so
    // SetCompleted() would not be needed but it does not harm in those cases.
    m_Reply->SetCompleted();
}


void CPSGS_Reply::Send409(const char *  payload)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    x_UpdateLastActivity();

    m_Reply->Send409(payload);

    // 409 is always the last so the reply is not needed anymore.
    // SetCompleted() will let the framework to know that the pending op can be
    // discarded.
    // Typically this method would be used without the pending op so
    // SetCompleted() would not be needed but it does not harm in those cases.
    m_Reply->SetCompleted();
}


void CPSGS_Reply::Send500(const char *  payload)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    x_UpdateLastActivity();

    m_Reply->Send500(payload);

    // 500 is always the last so the reply is not needed anymore.
    // SetCompleted() will let the framework to know that the pending op can be
    // discarded.
    // Typically this method would be used without the pending op so
    // SetCompleted() would not be needed but it does not harm in those cases.
    m_Reply->SetCompleted();
}


void CPSGS_Reply::Send502(const char *  payload)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    x_UpdateLastActivity();

    m_Reply->Send502(payload);

    // 502 is always the last so the reply is not needed anymore.
    // SetCompleted() will let the framework to know that the pending op can be
    // discarded.
    // Typically this method would be used without the pending op so
    // SetCompleted() would not be needed but it does not harm in those cases.
    m_Reply->SetCompleted();
}


void CPSGS_Reply::Send503(const char *  payload)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    x_UpdateLastActivity();

    m_Reply->Send503(payload);

    // 503 is always the last so the reply is not needed anymore.
    // SetCompleted() will let the framework to know that the pending op can be
    // discarded.
    // Typically this method would be used without the pending op so
    // SetCompleted() would not be needed but it does not harm in those cases.
    m_Reply->SetCompleted();
}


void CPSGS_Reply::PrepareBioseqMessage(size_t  item_id,
                                       const string &  processor_id,
                                       const string &  msg,
                                       CRequestStatus::ECode  status,
                                       int  err_code,
                                       EDiagSev  severity)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    string  header = GetBioseqMessageHeader(item_id, processor_id,
                                            msg.size(), status,
                                            err_code, severity);

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();

    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(msg.data()), msg.size()));
    ++m_TotalSentReplyChunks;
}



void CPSGS_Reply::PrepareBioseqData(
                    size_t  item_id,
                    const string &  processor_id,
                    const string &  content,
                    SPSGS_ResolveRequest::EPSGS_OutputFormat  output_format)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    string      header = GetBioseqInfoHeader(item_id, processor_id,
                                             content.size(), output_format);

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();

    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(content.data()), content.size()));
    ++m_TotalSentReplyChunks;
}


void CPSGS_Reply::PrepareBioseqCompletion(size_t  item_id,
                                          const string &  processor_id,
                                          size_t  chunk_count)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    string      bioseq_meta = GetBioseqCompletionHeader(item_id,
                                                        processor_id,
                                                        chunk_count);

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();

    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(bioseq_meta.data()),
                bioseq_meta.size()));
    ++m_TotalSentReplyChunks;
}


void CPSGS_Reply::PrepareBlobPropMessage(size_t                 item_id,
                                         const string &         processor_id,
                                         const string &         msg,
                                         CRequestStatus::ECode  status,
                                         int                    err_code,
                                         EDiagSev               severity)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    string      header = GetBlobPropMessageHeader(item_id, processor_id,
                                                  msg.size(), status, err_code,
                                                  severity);

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();

    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(msg.data()), msg.size()));
    ++m_TotalSentReplyChunks;
}


void CPSGS_Reply::x_PrepareTSEBlobPropMessage(size_t                 item_id,
                                              const string &         processor_id,
                                              int64_t                id2_chunk,
                                              const string &         id2_info,
                                              const string &         msg,
                                              CRequestStatus::ECode  status,
                                              int                    err_code,
                                              EDiagSev               severity)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    string      header = GetTSEBlobPropMessageHeader(
                                item_id, processor_id, id2_chunk, id2_info,
                                msg.size(), status, err_code, severity);

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();

    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(msg.data()), msg.size()));
    ++m_TotalSentReplyChunks;
}


void CPSGS_Reply::PrepareBlobPropMessage(CCassBlobFetch *       fetch_details,
                                         const string &         processor_id,
                                         const string &         msg,
                                         CRequestStatus::ECode  status,
                                         int                    err_code,
                                         EDiagSev               severity)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    PrepareBlobPropMessage(fetch_details->GetBlobPropItemId(this),
                           processor_id, msg, status, err_code, severity);
    fetch_details->IncrementTotalSentBlobChunks();
}


void CPSGS_Reply::PrepareTSEBlobPropMessage(CCassBlobFetch *       fetch_details,
                                            const string &         processor_id,
                                            int64_t                id2_chunk,
                                            const string &         id2_info,
                                            const string &         msg,
                                            CRequestStatus::ECode  status,
                                            int                    err_code,
                                            EDiagSev               severity)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    x_PrepareTSEBlobPropMessage(fetch_details->GetBlobPropItemId(this),
                                processor_id, id2_chunk, id2_info, msg,
                                status, err_code, severity);
    fetch_details->IncrementTotalSentBlobChunks();
}


void CPSGS_Reply::PrepareBlobPropData(size_t                   item_id,
                                      const string &           processor_id,
                                      const string &           blob_id,
                                      const string &           content,
                                      CBlobRecord::TTimestamp  last_modified)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    string  header = GetBlobPropHeader(item_id, processor_id, blob_id,
                                       content.size(), last_modified);

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();

    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(content.data()),
                    content.size()));
    ++m_TotalSentReplyChunks;
}


void CPSGS_Reply::PrepareBlobPropData(CCassBlobFetch *         fetch_details,
                                      const string &           processor_id,
                                      const string &           content,
                                      CBlobRecord::TTimestamp  last_modified)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    PrepareBlobPropData(fetch_details->GetBlobPropItemId(this),
                        processor_id,
                        fetch_details->GetBlobId().ToString(),
                        content,
                        last_modified);
    fetch_details->IncrementTotalSentBlobChunks();
}


void CPSGS_Reply::PrepareTSEBlobPropData(CCassBlobFetch *  fetch_details,
                                         const string &    processor_id,
                                         int64_t           id2_chunk,
                                         const string &    id2_info,
                                         const string &    content)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    PrepareTSEBlobPropData(fetch_details->GetBlobPropItemId(this),
                           processor_id, id2_chunk, id2_info, content);
    fetch_details->IncrementTotalSentBlobChunks();
}


void CPSGS_Reply::PrepareTSEBlobPropData(size_t  item_id,
                                         const string &    processor_id,
                                         int64_t           id2_chunk,
                                         const string &    id2_info,
                                         const string &    content)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    string  header = GetTSEBlobPropHeader(item_id,
                                          processor_id,
                                          id2_chunk, id2_info,
                                          content.size());

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();

    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(content.data()),
                    content.size()));
    ++m_TotalSentReplyChunks;
}


void CPSGS_Reply::PrepareBlobData(size_t                   item_id,
                                  const string &           processor_id,
                                  const string &           blob_id,
                                  const unsigned char *    chunk_data,
                                  unsigned int             data_size,
                                  int                      chunk_no,
                                  CBlobRecord::TTimestamp  last_modified)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    ++m_TotalSentReplyChunks;

    string  header = GetBlobChunkHeader(item_id, processor_id, blob_id,
                                        data_size, chunk_no, last_modified);

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();

    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(header.data()),
                    header.size()));

    if (data_size > 0 && chunk_data != nullptr)
        m_Chunks.push_back(m_Reply->PrepareChunk(chunk_data, data_size));
}


void CPSGS_Reply::PrepareBlobData(CCassBlobFetch *         fetch_details,
                                  const string &           processor_id,
                                  const unsigned char *    chunk_data,
                                  unsigned int             data_size,
                                  int                      chunk_no,
                                  CBlobRecord::TTimestamp  last_modified)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    fetch_details->IncrementTotalSentBlobChunks();
    PrepareBlobData(fetch_details->GetBlobChunkItemId(this),
                    processor_id,
                    fetch_details->GetBlobId().ToString(),
                    chunk_data, data_size, chunk_no,
                    last_modified);
}


void CPSGS_Reply::PrepareTSEBlobData(size_t                 item_id,
                                     const string &         processor_id,
                                     const unsigned char *  chunk_data,
                                     unsigned int           data_size,
                                     int                    chunk_no,
                                     int64_t                id2_chunk,
                                     const string &         id2_info)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    ++m_TotalSentReplyChunks;

    string  header = GetTSEBlobChunkHeader(item_id, processor_id, data_size,
                                           chunk_no, id2_chunk, id2_info);

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();

    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(header.data()),
                    header.size()));

    if (data_size > 0 && chunk_data != nullptr)
        m_Chunks.push_back(m_Reply->PrepareChunk(chunk_data, data_size));
}


void CPSGS_Reply::PrepareTSEBlobData(CCassBlobFetch *  fetch_details,
                                     const string &  processor_id,
                                     const unsigned char *  chunk_data,
                                     unsigned int  data_size,
                                     int  chunk_no,
                                     int64_t  id2_chunk,
                                     const string &  id2_info)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    fetch_details->IncrementTotalSentBlobChunks();
    PrepareTSEBlobData(fetch_details->GetBlobChunkItemId(this),
                       processor_id,
                       chunk_data, data_size, chunk_no,
                       id2_chunk, id2_info);
}


void CPSGS_Reply::PrepareBlobPropCompletion(size_t  item_id,
                                            const string &  processor_id,
                                            size_t  chunk_count)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    string      blob_prop_meta = GetBlobPropCompletionHeader(item_id,
                                                             processor_id,
                                                             chunk_count);

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();

    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(blob_prop_meta.data()),
                    blob_prop_meta.size()));
    ++m_TotalSentReplyChunks;
}


void CPSGS_Reply::x_PrepareTSEBlobPropCompletion(size_t          item_id,
                                                 const string &  processor_id,
                                                 size_t          chunk_count)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    string      blob_prop_meta = GetTSEBlobPropCompletionHeader(item_id,
                                                                processor_id,
                                                                chunk_count);

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();

    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(blob_prop_meta.data()),
                    blob_prop_meta.size()));
    ++m_TotalSentReplyChunks;
}


void CPSGS_Reply::PrepareBlobPropCompletion(CCassBlobFetch *  fetch_details,
                                            const string &  processor_id)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    // +1 is for the completion itself
    PrepareBlobPropCompletion(fetch_details->GetBlobPropItemId(this),
                              processor_id,
                              fetch_details->GetTotalSentBlobChunks() + 1);

    // From now the counter will count chunks for the blob data
    fetch_details->ResetTotalSentBlobChunks();
    fetch_details->SetBlobPropSent();
}


void CPSGS_Reply::PrepareTSEBlobPropCompletion(CCassBlobFetch *  fetch_details,
                                               const string &  processor_id)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    // +1 is for the completion itself
    x_PrepareTSEBlobPropCompletion(fetch_details->GetBlobPropItemId(this),
                                   processor_id,
                                   fetch_details->GetTotalSentBlobChunks() + 1);

    // From now the counter will count chunks for the blob data
    fetch_details->ResetTotalSentBlobChunks();
    fetch_details->SetBlobPropSent();
}


void CPSGS_Reply::PrepareBlobMessage(size_t                   item_id,
                                     const string &           processor_id,
                                     const string &           blob_id,
                                     const string &           msg,
                                     CRequestStatus::ECode    status,
                                     int                      err_code,
                                     EDiagSev                 severity,
                                     CBlobRecord::TTimestamp  last_modified)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    string      header = GetBlobMessageHeader(item_id, processor_id,
                                              blob_id, msg.size(),
                                              status, err_code, severity,
                                              last_modified);

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();

    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(msg.data()), msg.size()));
    ++m_TotalSentReplyChunks;
}


void CPSGS_Reply::PrepareBlobMessage(CCassBlobFetch *         fetch_details,
                                     const string &           processor_id,
                                     const string &           msg,
                                     CRequestStatus::ECode    status,
                                     int                      err_code,
                                     EDiagSev                 severity,
                                     CBlobRecord::TTimestamp  last_modified)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    PrepareBlobMessage(fetch_details->GetBlobChunkItemId(this),
                       processor_id,
                       fetch_details->GetBlobId().ToString(),
                       msg, status, err_code, severity, last_modified);
    fetch_details->IncrementTotalSentBlobChunks();
}


void CPSGS_Reply::x_PrepareTSEBlobMessage(size_t  item_id,
                                          const string &  processor_id,
                                          int64_t  id2_chunk,
                                          const string &  id2_info,
                                          const string &  msg,
                                          CRequestStatus::ECode  status,
                                          int  err_code,
                                          EDiagSev  severity)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    string      header = GetTSEBlobMessageHeader(item_id, processor_id,
                                                 id2_chunk, id2_info, msg.size(),
                                                 status, err_code, severity);

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();

    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(msg.data()), msg.size()));
    ++m_TotalSentReplyChunks;
}


void CPSGS_Reply::PrepareTSEBlobMessage(CCassBlobFetch *  fetch_details,
                                        const string &  processor_id,
                                        int64_t  id2_chunk,
                                        const string &  id2_info,
                                        const string &  msg,
                                        CRequestStatus::ECode  status, int  err_code,
                                        EDiagSev  severity)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    x_PrepareTSEBlobMessage(fetch_details->GetBlobChunkItemId(this),
                            processor_id, id2_chunk, id2_info,
                            msg, status, err_code, severity);
    fetch_details->IncrementTotalSentBlobChunks();
}


void CPSGS_Reply::PrepareBlobCompletion(size_t                   item_id,
                                        const string &           processor_id,
                                        size_t                   chunk_count)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    string completion = GetBlobCompletionHeader(item_id, processor_id,
                                                chunk_count);

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();

    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(completion.data()),
                    completion.size()));
    ++m_TotalSentReplyChunks;
}


void CPSGS_Reply::PrepareTSEBlobCompletion(CCassBlobFetch *  fetch_details,
                                           const string &  processor_id)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    // +1 is for the completion itself
    PrepareTSEBlobCompletion(fetch_details->GetBlobChunkItemId(this),
                             processor_id,
                             fetch_details->GetTotalSentBlobChunks() + 1);
    fetch_details->IncrementTotalSentBlobChunks();
}


void CPSGS_Reply::PrepareTSEBlobCompletion(size_t  item_id,
                                           const string &  processor_id,
                                           size_t  chunk_count)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    string completion = GetTSEBlobCompletionHeader(item_id, processor_id,
                                                   chunk_count);

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();

    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(completion.data()),
                    completion.size()));
    ++m_TotalSentReplyChunks;
}


void CPSGS_Reply::PrepareBlobExcluded(const string &           blob_id,
                                      const string &           processor_id,
                                      EPSGS_BlobSkipReason     skip_reason,
                                      CBlobRecord::TTimestamp  last_modified)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    string  exclude = GetBlobExcludeHeader(GetItemId(), processor_id,
                                           blob_id, skip_reason, last_modified);

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();

    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(exclude.data()),
                    exclude.size()));
    ++m_TotalSentReplyChunks;
}


void CPSGS_Reply::PrepareBlobExcluded(const string &  blob_id,
                                      const string &  processor_id,
                                      unsigned long  sent_mks_ago,
                                      unsigned long  until_resend_mks,
                                      CBlobRecord::TTimestamp  last_modified)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    string  exclude = GetBlobExcludeHeader(GetItemId(), processor_id, blob_id,
                                           sent_mks_ago, until_resend_mks,
                                           last_modified);

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();

    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(exclude.data()),
                    exclude.size()));
    ++m_TotalSentReplyChunks;
}


void CPSGS_Reply::PrepareBlobExcluded(size_t                item_id,
                                      const string &        processor_id,
                                      const string &        blob_id,
                                      EPSGS_BlobSkipReason  skip_reason)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    string  exclude = GetBlobExcludeHeader(item_id, processor_id,
                                           blob_id, skip_reason);

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();

    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(exclude.data()),
                    exclude.size()));
    ++m_TotalSentReplyChunks;
}


// NOTE: the blob id argument is temporary to satisfy the older clients
void CPSGS_Reply::PrepareTSEBlobExcluded(const string &        processor_id,
                                         EPSGS_BlobSkipReason  skip_reason,
                                         const string &        blob_id,
                                         int64_t               id2_chunk,
                                         const string &        id2_info)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    // NOTE: the blob id argument is temporary to satisfy the older clients
    string  exclude = GetTSEBlobExcludeHeader(GetItemId(), processor_id, blob_id,
                                              skip_reason, id2_chunk, id2_info);

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();

    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(exclude.data()),
                    exclude.size()));
    ++m_TotalSentReplyChunks;
}


// NOTE: the blob id argument is temporary to satisfy the older clients
void CPSGS_Reply::PrepareTSEBlobExcluded(const string &  blob_id,
                                         int64_t  id2_chunk,
                                         const string &  id2_info,
                                         const string &  processor_id,
                                         unsigned long  sent_mks_ago,
                                         unsigned long  until_resend_mks)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    // NOTE: the blob id argument is temporary to satisfy the older clients
    string  exclude = GetTSEBlobExcludeHeader(GetItemId(), processor_id,
                                              blob_id, id2_chunk, id2_info,
                                              sent_mks_ago, until_resend_mks);

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();

    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(exclude.data()),
                    exclude.size()));
    ++m_TotalSentReplyChunks;
}


void CPSGS_Reply::PrepareBlobCompletion(CCassBlobFetch *  fetch_details,
                                        const string &    processor_id)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    // +1 is for the completion itself
    PrepareBlobCompletion(fetch_details->GetBlobChunkItemId(this),
                          processor_id,
                          fetch_details->GetTotalSentBlobChunks() + 1);
    fetch_details->IncrementTotalSentBlobChunks();
}


void CPSGS_Reply::PrepareReplyMessage(const string &         msg,
                                      CRequestStatus::ECode  status,
                                      int                    err_code,
                                      EDiagSev               severity,
                                      bool                   need_update_last_activity)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    string      header = GetReplyMessageHeader(msg.size(),
                                               status, err_code, severity);

    lock_guard<mutex>       guard(m_ChunksLock);

    if (need_update_last_activity) {
        x_UpdateLastActivity();
    }

    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(msg.data()), msg.size()));
    ++m_TotalSentReplyChunks;
}


void CPSGS_Reply::PrepareProcessorMessage(size_t                 item_id,
                                          const string &         processor_id,
                                          const string &         msg,
                                          CRequestStatus::ECode  status,
                                          int                    err_code,
                                          EDiagSev               severity)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    string      header = GetProcessorMessageHeader(item_id, processor_id,
                                                   msg.size(),
                                                   status, err_code, severity);
    string      completion = GetProcessorMessageCompletionHeader(item_id,
                                                                 processor_id,
                                                                 2);

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();

    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(msg.data()), msg.size()));
    ++m_TotalSentReplyChunks;

    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(completion.data()), completion.size()));
    ++m_TotalSentReplyChunks;
}


void CPSGS_Reply::PreparePublicComment(const string &  processor_id,
                                       const string &  public_comment,
                                       const string &  blob_id,
                                       CBlobRecord::TTimestamp  last_modified)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    auto        item_id = GetItemId();
    string      header = GetPublicCommentHeader(item_id, processor_id, blob_id,
                                                last_modified, public_comment.size());
    string      completion = GetPublicCommentCompletionHeader(item_id,
                                                              processor_id,
                                                              2);

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();

    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(public_comment.data()), public_comment.size()));
    ++m_TotalSentReplyChunks;

    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(completion.data()), completion.size()));
    ++m_TotalSentReplyChunks;
}


void CPSGS_Reply::PreparePublicComment(const string &  processor_id,
                                       const string &  public_comment,
                                       int64_t  id2_chunk,
                                       const string &  id2_info)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    auto        item_id = GetItemId();
    string      header = GetPublicCommentHeader(item_id, processor_id, id2_chunk,
                                                id2_info, public_comment.size());
    string      completion = GetPublicCommentCompletionHeader(item_id,
                                                              processor_id,
                                                              2);

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();

    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(public_comment.data()), public_comment.size()));
    ++m_TotalSentReplyChunks;

    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(completion.data()), completion.size()));
    ++m_TotalSentReplyChunks;
}


void CPSGS_Reply::PrepareNamedAnnotationData(const string &  annot_name,
                                             const string &  processor_id,
                                             const string &  content)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    size_t      item_id = GetItemId();
    string      header = GetNamedAnnotationHeader(item_id, processor_id,
                                                  annot_name, content.size());
    // There are always 2 chunks
    string      bioseq_na_meta = GetNamedAnnotationCompletionHeader(item_id,
                                                                    processor_id,
                                                                    2);

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();

    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(content.data()), content.size()));
    ++m_TotalSentReplyChunks;

    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(bioseq_na_meta.data()),
                bioseq_na_meta.size()));
    ++m_TotalSentReplyChunks;
}


void CPSGS_Reply::SendPerNamedAnnotationResults(const string &  content)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    size_t      item_id = GetItemId();
    string      header = GetPerNamedAnnotationResultsHeader(item_id,
                                                            content.size());
    // There are always 2 chunks
    string      na_results_meta = GetPerNAResultsCompletionHeader(item_id, 2);

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(content.data()), content.size()));
    ++m_TotalSentReplyChunks;

    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(na_results_meta.data()),
                na_results_meta.size()));
    ++m_TotalSentReplyChunks;
}


void CPSGS_Reply::PrepareAccVerHistoryData(const string &  processor_id,
                                           const string &  content)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    size_t      item_id = GetItemId();
    string      header = GetAccVerHistoryHeader(item_id, processor_id,
                                                content.size());

    // There are always 2 chunks
    string      acc_ver_hist_meta = GetAccVerHistCompletionHeader(item_id,
                                                                  processor_id,
                                                                  2);

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();

    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(content.data()), content.size()));
    ++m_TotalSentReplyChunks;

    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(acc_ver_hist_meta.data()),
                acc_ver_hist_meta.size()));
    ++m_TotalSentReplyChunks;
}


void CPSGS_Reply::PrepareIPGResolveData(const string &  processor_id,
                                        const string &  content)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    size_t      item_id = GetItemId();
    string      data_and_meta = GetIPGResolveHeader(item_id, processor_id,
                                                    content.size());

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(data_and_meta.data()), data_and_meta.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(content.data()), content.size()));
    ++m_TotalSentReplyChunks;
}


void CPSGS_Reply::PrepareIPGInfoMessageAndMeta(const string &  processor_id,
                                               const string &  msg,
                                               CRequestStatus::ECode  status,
                                               int  err_code,
                                               EDiagSev  severity)
{
    if (m_ConnectionCanceled || IsFinished())
        return;

    size_t      item_id = GetItemId();
    string      data_and_meta = GetIPGMessageHeader(item_id, processor_id,
                                                    status, err_code, severity,
                                                    msg.size());

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(data_and_meta.data()), data_and_meta.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(msg.data()), msg.size()));
    ++m_TotalSentReplyChunks;
}


void CPSGS_Reply::PrepareRequestTimeoutMessage(const string &  msg)
{
    // NOTE: no need to update the last activity
    //       - it is used by the dispatcher
    //       - it is when the request timer is over

    if (m_ConnectionCanceled || IsFinished())
        return;

    string      header = GetReplyMessageHeader(msg.size(),
                                               CRequestStatus::e504_GatewayTimeout,
                                               ePSGS_RequestTimeout, eDiag_Error);

    lock_guard<mutex>       guard(m_ChunksLock);
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(msg.data()), msg.size()));
    ++m_TotalSentReplyChunks;
}


void CPSGS_Reply::PrepareProcessorProgressMessage(const string &  processor_id,
                                                  const string &  progress_status)
{
    // NOTE: no need to update the last activity because it is used by the dispatcher

    if (m_ConnectionCanceled || IsFinished())
        return;

    size_t      item_id = GetItemId();
    string      header = GetProcessorProgressMessageHeader(item_id,
                                                           processor_id,
                                                           progress_status);

    lock_guard<mutex>       guard(m_ChunksLock);
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    ++m_TotalSentReplyChunks;
}


void CPSGS_Reply::PrepareReplyCompletion(CRequestStatus::ECode  status,
                                         const psg_time_point_t &  create_timestamp)
{
    if (m_ConnectionCanceled || IsFinished())
        return;
    if (m_Reply->IsClosed())
        return;

    lock_guard<mutex>       guard(m_ChunksLock);
    x_UpdateLastActivity();

    ++m_TotalSentReplyChunks;

    string  reply_completion = GetReplyCompletionHeader(m_TotalSentReplyChunks,
                                                        status,
                                                        create_timestamp);
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(reply_completion.data()),
                reply_completion.size()));
}


// The last activity timestamp needs to be updated if it was a processor
// initiated activity with the reply. If it was a trace from the processor
// dispatcher then the activity timestamp does not need to be updated
void CPSGS_Reply::SendTrace(const string &  msg,
                            const psg_time_point_t &  create_timestamp,
                            bool  need_update_last_activity)
{
    if (need_update_last_activity) {
        x_UpdateLastActivity();
    }

    if (m_ConnectionCanceled || IsFinished())
        return;

    auto            now = psg_clock_t::now();
    uint64_t        mks = chrono::duration_cast<chrono::microseconds>
                                            (now - create_timestamp).count();
    string          timestamp = "Timestamp (mks): " + to_string(mks) + "\n";

    PrepareReplyMessage(timestamp + msg,
                        CRequestStatus::e200_Ok, 0, eDiag_Trace,
                        need_update_last_activity);
}

