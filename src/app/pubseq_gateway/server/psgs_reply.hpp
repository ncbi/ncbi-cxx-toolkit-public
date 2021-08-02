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


class CPendingOperation;
class CCassBlobFetch;
template<typename P> class CHttpReply;
namespace idblob { class CCassDataCallbackReceiver; }

// Keeps track of the protocol replies
class CPSGS_Reply
{
public:
    CPSGS_Reply(unique_ptr<CHttpReply<CPendingOperation>>  low_level_reply) :
        m_Reply(low_level_reply.release()),
        m_ReplyOwned(true),
        m_NextItemIdLock(false),
        m_NextItemId(0),
        m_TotalSentReplyChunks(0),
        m_ChunksLock(false)
    {
        SetContentType(ePSGS_PSGMime);
    }

    // This constructor is to reuse the infrastructure (PSG chunks, counting
    // them etc) in the low level error reports
    CPSGS_Reply(CHttpReply<CPendingOperation> *  low_level_reply) :
        m_Reply(low_level_reply),
        m_ReplyOwned(false),
        m_NextItemIdLock(false),
        m_NextItemId(0),
        m_TotalSentReplyChunks(0),
        m_ChunksLock(false)
    {
        SetContentType(ePSGS_PSGMime);
    }

    ~CPSGS_Reply();

public:
    void Flush(void);
    void Flush(bool  is_last);
    void Clear(void);
    shared_ptr<idblob::CCassDataCallbackReceiver> GetDataReadyCB(void);
    bool IsFinished(void) const;
    bool IsOutputReady(void) const;
    void SetContentType(EPSGS_ReplyMimeType  mime_type);

    CHttpReply<CPendingOperation> *  GetHttpReply(void)
    {
        return m_Reply;
    }

    size_t  GetItemId(void)
    {
        while (m_NextItemIdLock.exchange(true)) {}
        auto    ret = ++m_NextItemId;
        m_NextItemIdLock = false;
        return ret;
    }

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
                             EDiagSev  severity);
    void PrepareNamedAnnotationData(const string &  annot_name,
                                    const string &  processor_id,
                                    const string &  content);
    void PrepareAccVerHistoryData(const string &  processor_id,
                                  const string &  content);
    void PrepareReplyCompletion(void);
    void SendTrace(const string &  msg,
                   const TPSGS_HighResolutionTimePoint &  create_timestamp);

public:
    // HTTP facilities
    void SendData(const string &  data_ptr, EPSGS_ReplyMimeType  mime_type);

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

private:
    CHttpReply<CPendingOperation> *     m_Reply;
    bool                                m_ReplyOwned;
    atomic<bool>                        m_NextItemIdLock;
    size_t                              m_NextItemId;
    int32_t                             m_TotalSentReplyChunks;
    atomic<bool>                        m_ChunksLock;
    vector<h2o_iovec_t>                 m_Chunks;
};


#endif
