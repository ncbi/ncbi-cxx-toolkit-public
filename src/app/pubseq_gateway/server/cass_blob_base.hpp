#ifndef PSGS_CASSBLOBBASE__HPP
#define PSGS_CASSBLOBBASE__HPP

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
 * File Description: base class for processors which retrieve blobs
 *
 */

#include <corelib/request_status.hpp>
#include <corelib/ncbidiag.hpp>

#include "cass_fetch.hpp"
#include "psgs_request.hpp"
#include "psgs_reply.hpp"
#include "cass_processor_base.hpp"
#include "get_blob_callback.hpp"
#include "id2info.hpp"
#include "cass_blob_id.hpp"
#include "split_info_utils.hpp"


USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;


class CPSGS_CassBlobBase : virtual public CPSGS_CassProcessorBase
{
public:
    CPSGS_CassBlobBase();
    CPSGS_CassBlobBase(shared_ptr<CPSGS_Request>  request,
                       shared_ptr<CPSGS_Reply>  reply,
                       const string &  processor_id,
                       TBlobPropsCB  blob_props_cb,
                       TBlobChunkCB  blob_chunk_cb,
                       TBlobErrorCB  blob_error_cb);
    virtual ~CPSGS_CassBlobBase();

protected:
    void OnGetBlobProp(CCassBlobFetch *  fetch_details,
                       CBlobRecord const &  blob, bool is_found);
    void OnGetBlobError(CCassBlobFetch *  fetch_details,
                        CRequestStatus::ECode  status, int  code,
                        EDiagSev  severity, const string &  message);
    void OnGetBlobChunk(bool  cancelled,
                        CCassBlobFetch *  fetch_details,
                        const unsigned char *  chunk_data,
                        unsigned int  data_size, int  chunk_no);
    bool NeedToAddId2CunkId2Info(void) const;
    void PrepareServerErrorMessage(CCassBlobFetch *  fetch_details,
                                   int  code,
                                   EDiagSev  severity,
                                   const string &  message);
    void OnPublicCommentError(CCassPublicCommentFetch *  fetch_details,
                              CRequestStatus::ECode  status,
                              int  code,
                              EDiagSev  severity,
                              const string &  message);
    void OnPublicComment(CCassPublicCommentFetch *  fetch_details,
                         string  comment,
                         bool  is_found);

private:
    void x_OnBlobPropNoneTSE(CCassBlobFetch *  fetch_details);
    void x_OnBlobPropSlimTSE(CCassBlobFetch *  fetch_details,
                             CBlobRecord const &  blob);
    void x_OnBlobPropSmartTSE(CCassBlobFetch *  fetch_details,
                              CBlobRecord const &  blob);
    void x_OnBlobPropWholeTSE(CCassBlobFetch *  fetch_details,
                              CBlobRecord const &  blob);
    void x_OnBlobPropOrigTSE(CCassBlobFetch *  fetch_details,
                             CBlobRecord const &  blob);

private:
    void x_RequestOriginalBlobChunks(CCassBlobFetch *  fetch_details,
                                     CBlobRecord const &  blob);
    void x_RequestID2BlobChunks(CCassBlobFetch *  fetch_details,
                                CBlobRecord const &  blob,
                                bool  info_blob_only);
    void x_RequestId2SplitBlobs(CCassBlobFetch *  fetch_details);


private:
    enum EPSGS_BlobCacheCheckResult {
        ePSGS_SkipRetrieving,
        ePSGS_ProceedRetrieving
    };

    enum EPSGS_BlobOp {
        ePSGS_RetrieveBlobData
    };

    EPSGS_BlobCacheCheckResult
    x_CheckExcludeBlobCache(CCassBlobFetch *  fetch_details);
    void x_OnBlobPropNotFound(CCassBlobFetch *  fetch_details);
    bool x_ParseId2Info(CCassBlobFetch *  fetch_details,
                        CBlobRecord const &  blob);
    int64_t  x_GetId2ChunkNumber(CCassBlobFetch *  fetch_details);
    void x_PrepareBlobPropData(CCassBlobFetch *  fetch_details,
                               CBlobRecord const &  blob);
    bool x_IsAuthorized(EPSGS_BlobOp  blob_op,
                        const SCass_BlobId &  blob_id,
                        const CBlobRecord &  blob,
                        const TAuthToken &  auth_token);
    void x_PrepareBlobPropCompletion(CCassBlobFetch *  fetch_details);
    void x_PrepareBlobData(CCassBlobFetch *  fetch_details,
                           const unsigned char *  chunk_data,
                           unsigned int  data_size,
                           int  chunk_no);
    void x_PrepareBlobCompletion(CCassBlobFetch *  fetch_details);
    void x_PrepareBlobPropMessage(CCassBlobFetch *  fetch_details,
                                  const string &  message,
                                  CRequestStatus::ECode  status,
                                  int  err_code,
                                  EDiagSev  severity);
    void x_PrepareBlobMessage(CCassBlobFetch *  fetch_details,
                              const string &  message,
                              CRequestStatus::ECode  status,
                              int  err_code,
                              EDiagSev  severity);
    void x_PrepareBlobExcluded(CCassBlobFetch *  fetch_details,
                               EPSGS_BlobSkipReason  skip_reason);
    void x_PrepareBlobExcluded(CCassBlobFetch *  fetch_details,
                               unsigned long  sent_mks_ago,
                               unsigned long  until_resend_mks);

    void x_DecideToRequestMoreChunksForSmartTSE(
                                    CCassBlobFetch *  fetch_details,
                                    SCass_BlobId const &  info_blob_id);
    void x_DeserializeSplitInfo(CCassBlobFetch *  fetch_details);
    void x_RequestMoreChunksForSmartTSE(CCassBlobFetch *  fetch_details,
                                        const vector<int> &  extra_chunks,
                                        bool  need_wait);

protected:
    // Used for get blob by sat/sat key request
    SCass_BlobId                                        m_BlobId;

    // For the time being: only for the ID/get processor:
    // it sets the value after the resultion has succeeded and the accession
    // possibly adjusted
    CSeq_id                                             m_ResolvedSeqID;

private:
    bool                                                m_NeedToParseId2Info;
    unique_ptr<CPSGS_SatInfoChunksVerFlavorId2Info>     m_Id2Info;
    string                                              m_ProcessorId;

    // last_modified from the original blob props
    CBlobRecord::TTimestamp                             m_LastModified;

private:
    // Support for collecting split INFO for further deserialization
    bool                                                m_CollectSplitInfo;
    SCass_BlobId                                        m_InfoBlobId;
    bool                                                m_SplitInfoGzipFlag;
    psg::CDataChunkStream                               m_CollectedSplitInfo;

private:
    TBlobPropsCB                                        m_BlobPropsCB;
    TBlobChunkCB                                        m_BlobChunkCB;
    TBlobErrorCB                                        m_BlobErrorCB;

private:
    // Support of a fallback to the original blob if a split blob failed
    bool                                                m_NeedFallbackBlob;
    bool                                                m_FallbackBlobRequested;
    vector<string>                                      m_RequestedID2BlobChunks;
    CCassBlobFetch *                                    m_InitialBlobPropFetch;
    CBlobRecord                                         m_InitialBlobProps;

    void x_BlobChunkCallback(CCassBlobFetch *  fetch_details,
                             CBlobRecord const &  blob,
                             const unsigned char *  chunk_data,
                             unsigned int  data_size,
                             int  chunk_no);
    void x_BlobPropsCallback(CCassBlobFetch *  fetch_details,
                             CBlobRecord const &  blob,
                             bool  is_found);
    void x_BlobErrorCallback(CCassBlobFetch *  fetch_details,
                             CRequestStatus::ECode  status,
                             int  code,
                             EDiagSev  severity,
                             const string &  message);
};

#endif  // PSGS_CASSBLOBBASE__HPP

