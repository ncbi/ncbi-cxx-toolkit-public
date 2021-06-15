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

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;


class CPSGS_CassBlobBase : virtual public CPSGS_CassProcessorBase
{
public:
    CPSGS_CassBlobBase();
    CPSGS_CassBlobBase(shared_ptr<CPSGS_Request>  request,
                       shared_ptr<CPSGS_Reply>  reply,
                       const string &  processor_id);
    virtual ~CPSGS_CassBlobBase();

protected:
    void OnGetBlobProp(TBlobPropsCB  blob_props_cb,
                       TBlobChunkCB  blob_chunk_cb,
                       TBlobErrorCB  blob_error_cb,
                       CCassBlobFetch *  fetch_details,
                       CBlobRecord const &  blob, bool is_found);
    void OnGetBlobError(CCassBlobFetch *  fetch_details,
                        CRequestStatus::ECode  status, int  code,
                        EDiagSev  severity, const string &  message);
    void OnGetBlobChunk(bool  cancelled,
                        CCassBlobFetch *  fetch_details,
                        const unsigned char *  chunk_data,
                        unsigned int  data_size, int  chunk_no);
    void SetFinished(CCassBlobFetch *  fetch_details);
    bool CountError(CRequestStatus::ECode  status,
                    int  code,
                    EDiagSev  severity,
                    const string &  message);
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
    void x_OnBlobPropSlimTSE(TBlobPropsCB  blob_props_cb,
                             TBlobChunkCB  blob_chunk_cb,
                             TBlobErrorCB  blob_error_cb,
                             CCassBlobFetch *  fetch_details,
                             CBlobRecord const &  blob);
    void x_OnBlobPropSmartTSE(TBlobPropsCB  blob_props_cb,
                              TBlobChunkCB  blob_chunk_cb,
                              TBlobErrorCB  blob_error_cb,
                              CCassBlobFetch *  fetch_details,
                              CBlobRecord const &  blob);
    void x_OnBlobPropWholeTSE(TBlobPropsCB  blob_props_cb,
                              TBlobChunkCB  blob_chunk_cb,
                              TBlobErrorCB  blob_error_cb,
                              CCassBlobFetch *  fetch_details,
                              CBlobRecord const &  blob);
    void x_OnBlobPropOrigTSE(TBlobChunkCB  blob_chunk_cb,
                             TBlobErrorCB  blob_error_cb,
                             CCassBlobFetch *  fetch_details,
                             CBlobRecord const &  blob);

private:
    void x_RequestOriginalBlobChunks(TBlobChunkCB  blob_chunk_cb,
                                     TBlobErrorCB  blob_error_cb,
                                     CCassBlobFetch *  fetch_details,
                                     CBlobRecord const &  blob);
    void x_RequestID2BlobChunks(TBlobPropsCB  blob_props_cb,
                                TBlobChunkCB  blob_chunk_cb,
                                TBlobErrorCB  blob_error_cb,
                                CCassBlobFetch *  fetch_details,
                                CBlobRecord const &  blob,
                                bool  info_blob_only);
    void x_RequestId2SplitBlobs(TBlobPropsCB  blob_props_cb,
                                TBlobChunkCB  blob_chunk_cb,
                                TBlobErrorCB  blob_error_cb,
                                CCassBlobFetch *  fetch_details,
                                const string &  sat_name);


private:
    enum EPSGS_BlobCacheCheckResult {
        ePSGS_InCache,
        ePSGS_NotInCache
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

protected:
    // Used for get blob by sat/sat key request
    SCass_BlobId                                        m_BlobId;

private:
    bool                                                m_NeedToParseId2Info;
    unique_ptr<CPSGS_SatInfoChunksVerFlavorId2Info>     m_Id2Info;
    string                                              m_ProcessorId;

    // last_modified from the original blob props
    CBlobRecord::TTimestamp                             m_LastModified;
};

#endif  // PSGS_CASSBLOBBASE__HPP

