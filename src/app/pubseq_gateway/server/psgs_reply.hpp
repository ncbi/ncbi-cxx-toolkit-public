#ifndef PSGS_REPLY__HPP
#define PSGS_REPLY__HPP

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

#include <corelib/request_status.hpp>
#include <h2o.h>

#include "pubseq_gateway_types.hpp"
#include "psgs_request.hpp"
#include "pubseq_gateway_utils.hpp"


class CPendingOperation;
class CCassBlobFetch;
class CHttpReply;
namespace idblob { class CCassDataCallbackReceiver; }

// Keeps track of the protocol replies
class CPSGS_Reply
{
public:
    enum EPSGS_ReplyFlush
    {
        ePSGS_SendAccumulated,    // Only flushes the accumulated chunks.
        ePSGS_SendAndFinish       // Flushes the accumulated chunks and closes
                                  // the stream.
    };

public:
    CPSGS_Reply(unique_ptr<CHttpReply>  low_level_reply) :
        m_Reply(low_level_reply.release()),
        m_ReplyOwned(true),
        m_NextItemIdLock(false),
        m_NextItemId(0),
        m_TotalSentReplyChunks(0),
        m_ConnectionCanceled(false),
        m_RequestId(0),
        m_LastActivityTimestamp(psg_clock_t::now())
    {
        SetContentType(ePSGS_PSGMime);
        m_Chunks.reserve(64);
    }

    // This constructor is to reuse the infrastructure (PSG chunks, counting
    // them etc) in the low level error reports
    CPSGS_Reply(CHttpReply *  low_level_reply) :
        m_Reply(low_level_reply),
        m_ReplyOwned(false),
        m_NextItemIdLock(false),
        m_NextItemId(0),
        m_TotalSentReplyChunks(0),
        m_ConnectionCanceled(false),
        m_RequestId(0),
        m_LastActivityTimestamp(psg_clock_t::now())
    {
        SetContentType(ePSGS_PSGMime);
        m_Chunks.reserve(64);
    }

    ~CPSGS_Reply();

public:
    // Flush can close the stream
    void Flush(EPSGS_ReplyFlush  how);

    // Tells the lower level that the pending op can be deleted
    void SetCompleted(void);

    // Tells if the stream is closed and the pending op can be deleted
    bool IsCompleted(void) const;

    // Tells if the stream is closed
    bool IsFinished(void) const;

    // Tells if the output is ready
    bool IsOutputReady(void) const;

    bool IsClosed(void) const;

    void Clear(void);
    void SetContentType(EPSGS_ReplyMimeType  mime_type);
    void SetContentLength(uint64_t  content_length);
    size_t GetBytesSent(void) const;

    void SendOk(const char *  payload, size_t  payload_len, bool  is_persist);
    void Send202(const char *  payload, size_t  payload_len);
    void Send400(const char *  payload);
    void Send401(const char *  payload);
    void Send404(const char *  payload);
    void Send409(const char *  payload);
    void Send500(const char *  payload);
    void Send502(const char *  payload);
    void Send503(const char *  payload);

    void ConnectionCancel(void);
    shared_ptr<idblob::CCassDataCallbackReceiver> GetDataReadyCB(void);

    CHttpReply *  GetHttpReply(void)
    {
        return m_Reply;
    }

    size_t  GetItemId(void)
    {
        CSpinlockGuard      guard(&m_NextItemIdLock);
        auto    ret = ++m_NextItemId;
        return ret;
    }

    void SetRequestId(size_t  request_id);

    size_t GetRequestId(void) const
    {
        return m_RequestId;
    }

    psg_time_point_t  GetLastActivityTimestamp(void) const
    {
        return m_LastActivityTimestamp;
    }

    unsigned long  GetTimespanFromLastActivityToNowMks(void) const
    {
        return GetTimespanToNowMks(m_LastActivityTimestamp);
    }

    bool GetExceedSoftLimitFlag(void) const;
    void ResetExceedSoftLimitFlag(void);
    void IncrementRejectedDueToSoftLimit(void);
    uint16_t GetConnCntAtOpen(void) const;
    int64_t GetConnectionId(void) const;
    void UpdatePeerIdAndUserAgent(const string &  peer_id,
                                  const string &  peer_user_agent);
    void UpdatePeerUserAgent(const string &  peer_user_agent);
    void UpdatePeerId(const string &  peer_id);

public:
    // PSG protocol facilities
    void PrepareBioseqMessage(size_t  item_id,
                              const string &  processor_id,
                              const string &  msg,
                              CRequestStatus::ECode  status,
                              int  err_code, EDiagSev  severity);
    void PrepareBioseqData(size_t  item_id,
                           const string &  processor_id,
                           const string &  content,
                           SPSGS_ResolveRequest::EPSGS_OutputFormat  output_format);
    void PrepareBioseqCompletion(size_t  item_id,
                                 const string &  processor_id,
                                 size_t  chunk_count);
    void PrepareBioseqDataAndCompletion(
                    size_t  item_id,
                    const string &  processor_id,
                    const string &  content,
                    SPSGS_ResolveRequest::EPSGS_OutputFormat  output_format,
                    size_t  chunk_count);
    void PrepareBlobPropMessage(size_t  item_id,
                                const string &  processor_id,
                                const string &  msg,
                                CRequestStatus::ECode  status,
                                int  err_code,
                                EDiagSev  severity);
    void PrepareBlobPropMessage(CCassBlobFetch *  fetch_details,
                                const string &  processor_id,
                                const string &  msg,
                                CRequestStatus::ECode  status,
                                int  err_code,
                                EDiagSev  severity);
    void PrepareTSEBlobPropMessage(CCassBlobFetch *       fetch_details,
                                   const string &         processor_id,
                                   int64_t                id2_chunk,
                                   const string &         id2_info,
                                   const string &         msg,
                                   CRequestStatus::ECode  status,
                                   int                    err_code,
                                   EDiagSev               severity);
    void PrepareBlobPropData(size_t                   item_id,
                             const string &           processor_id,
                             const string &           blob_id,
                             const string &           content,
                             CBlobRecord::TTimestamp  last_modified=-1);
    void PrepareBlobPropData(CCassBlobFetch *         fetch_details,
                             const string &           processor_id,
                             const string &           content,
                             CBlobRecord::TTimestamp  last_modified=-1);
    void PrepareTSEBlobPropData(size_t            item_id,
                                const string &    processor_id,
                                int64_t           id2_chunk,
                                const string &    id2_info,
                                const string &    content);
    void PrepareTSEBlobPropData(CCassBlobFetch *  fetch_details,
                                const string &    processor_id,
                                int64_t           id2_chunk,
                                const string &    id2_info,
                                const string &    content);
    void PrepareBlobData(size_t                   item_id,
                         const string &           processor_id,
                         const string &           blob_id,
                         const unsigned char *    chunk_data,
                         unsigned int             data_size,
                         int                      chunk_no,
                         CBlobRecord::TTimestamp  last_modified=-1);
    void PrepareBlobData(CCassBlobFetch *         fetch_details,
                         const string &           processor_id,
                         const unsigned char *    chunk_data,
                         unsigned int             data_size,
                         int                      chunk_no,
                         CBlobRecord::TTimestamp  last_modified=-1);
    void PrepareTSEBlobData(size_t                 item_id,
                            const string &         processor_id,
                            const unsigned char *  chunk_data,
                            unsigned int           data_size,
                            int                    chunk_no,
                            int64_t                id2_chunk,
                            const string &         id2_info);
    void PrepareTSEBlobData(CCassBlobFetch *  fetch_details,
                            const string &  processor_id,
                            const unsigned char *  chunk_data,
                            unsigned int  data_size,
                            int  chunk_no,
                            int64_t  id2_chunk,
                            const string &  id2_info);
    void PrepareBlobPropCompletion(size_t  item_id,
                                   const string &  processor_id,
                                   size_t  chunk_count);
    void PrepareBlobPropCompletion(CCassBlobFetch *  fetch_details,
                                   const string &  processor_id);
    void PrepareTSEBlobPropCompletion(CCassBlobFetch *  fetch_details,
                                      const string &  processor_id);
    void PrepareBlobMessage(size_t  item_id,
                            const string &  processor_id,
                            const string &  blob_id,
                            const string &  msg,
                            CRequestStatus::ECode  status,
                            int  err_code,
                            EDiagSev  severity,
                            CBlobRecord::TTimestamp  last_modified=-1);
    void PrepareBlobMessage(CCassBlobFetch *  fetch_details,
                            const string &  processor_id,
                            const string &  msg,
                            CRequestStatus::ECode  status, int  err_code,
                            EDiagSev  severity,
                            CBlobRecord::TTimestamp  last_modified=-1);
    void PrepareTSEBlobMessage(CCassBlobFetch *  fetch_details,
                               const string &  processor_id,
                               int64_t  id2_chunk,
                               const string &  id2_info,
                               const string &  msg,
                               CRequestStatus::ECode  status, int  err_code,
                               EDiagSev  severity);
    void PrepareBlobCompletion(size_t  item_id,
                               const string &  processor_id,
                               size_t  chunk_count);
    void PrepareTSEBlobCompletion(size_t item_id,
                                  const string &  processor_id,
                                  size_t  chunk_count);
    void PrepareTSEBlobCompletion(CCassBlobFetch *  fetch_details,
                                  const string &  processor_id);
    void PrepareBlobExcluded(const string &  blob_id,
                             const string &  processor_id,
                             EPSGS_BlobSkipReason  skip_reason,
                             CBlobRecord::TTimestamp  last_modified=-1);
    void PrepareBlobExcluded(size_t  item_id,
                             const string &  processor_id,
                             const string &  blob_id,
                             EPSGS_BlobSkipReason  skip_reason);
    void PrepareBlobExcluded(const string &  blob_id,
                             const string &  processor_id,
                             unsigned long  sent_mks_ago,
                             unsigned long  until_resend_mks,
                             CBlobRecord::TTimestamp  last_modified=-1);
    // NOTE: the blob id argument is temporary to satisfy the older clients
    void PrepareTSEBlobExcluded(const string &  processor_id,
                                EPSGS_BlobSkipReason  skip_reason,
                                const string &  blob_id,
                                int64_t  id2_chunk,
                                const string &  id2_info);
    // NOTE: the blob id argument is temporary to satisfy the older clients
    void PrepareTSEBlobExcluded(const string &  blob_id,
                                int64_t  id2_chunk,
                                const string &  id2_info,
                                const string &  processor_id,
                                unsigned long  sent_mks_ago,
                                unsigned long  until_resend_mks);
    void PrepareBlobCompletion(CCassBlobFetch *  fetch_details,
                               const string &  processor_id);
    void PrepareProcessorMessage(size_t  item_id, const string &  processor_id,
                                 const string &  msg,
                                 CRequestStatus::ECode  status, int  err_code,
                                 EDiagSev  severity);
    void PreparePublicComment(const string &  processor_id,
                              const string &  public_comment,
                              const string &  blob_id,
                              CBlobRecord::TTimestamp  last_modified);
    void PreparePublicComment(const string &  processor_id,
                              const string &  public_comment,
                              int64_t  id2_chunk,
                              const string &  id2_info);
    void PrepareReplyMessage(const string &  msg,
                             CRequestStatus::ECode  status, int  err_code,
                             EDiagSev  severity,
                             bool  need_update_last_activity=true);
    void PrepareNamedAnnotationData(const string &  annot_name,
                                    const string &  processor_id,
                                    const string &  content);
    void SendPerNamedAnnotationResults(const string &  content);
    void PrepareAccVerHistoryData(const string &  processor_id,
                                  const string &  content);
    void PrepareIPGResolveData(const string &  processor_id,
                               const string &  content);
    void PrepareIPGInfoMessageAndMeta(const string &  processor_id,
                                      const string &  msg,
                                      CRequestStatus::ECode  status,
                                      int  err_code,
                                      EDiagSev  severity);
    void PrepareRequestTimeoutMessage(const string &  msg);
    void PrepareProcessorProgressMessage(const string &  processor_id,
                                         const string &  progress_status);

    void PrepareReplyCompletion(CRequestStatus::ECode  status,
                                const psg_time_point_t &  create_timestamp);

    // The last activity timestamp needs to be updated if it was a processor
    // initiated activity with the reply. If it was a trace from the processor
    // dispatcher then the activity timestamp does not need to be updated
    void SendTrace(const string &  msg,
                   const psg_time_point_t &  create_timestamp,
                   bool  need_update_last_activity=true);

private:
    void x_PrepareTSEBlobPropCompletion(size_t          item_id,
                                        const string &  processor_id,
                                        size_t          chunk_count);
    void x_PrepareTSEBlobPropMessage(size_t  item_id,
                                     const string &  processor_id,
                                     int64_t  id2_chunk,
                                     const string &  id2_info,
                                     const string &  msg,
                                     CRequestStatus::ECode  status,
                                     int  err_code,
                                     EDiagSev  severity);
    void x_PrepareTSEBlobMessage(size_t  item_id,
                                 const string &  processor_id,
                                 int64_t  id2_chunk,
                                 const string &  id2_info,
                                 const string &  msg,
                                 CRequestStatus::ECode  status,
                                 int  err_code,
                                 EDiagSev  severity);

    void x_UpdateLastActivity(void)
    {
        m_LastActivityTimestamp = psg_clock_t::now();
    }

private:
    CHttpReply *            m_Reply;
    bool                    m_ReplyOwned;
    atomic<bool>            m_NextItemIdLock;
    size_t                  m_NextItemId;
    int32_t                 m_TotalSentReplyChunks;
    mutex                   m_ChunksLock;
    vector<h2o_iovec_t>     m_Chunks;
    volatile bool           m_ConnectionCanceled;
    size_t                  m_RequestId;
    psg_time_point_t        m_LastActivityTimestamp;
};


#endif
