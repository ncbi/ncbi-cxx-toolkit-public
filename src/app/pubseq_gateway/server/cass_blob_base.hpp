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

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;


class CPSGS_CassBlobBase : virtual public CPSGS_CassProcessorBase
{
public:
    CPSGS_CassBlobBase();
    virtual ~CPSGS_CassBlobBase();

protected:
    void OnGetBlobProp(shared_ptr<CPSGS_Request>  request,
                       shared_ptr<CPSGS_Reply>  reply,
                       TBlobPropsCB  blob_props_cb,
                       TBlobChunkCB  blob_chunk_cb,
                       TBlobErrorCB  blob_error_cb,
                       CCassBlobFetch *  fetch_details,
                       CBlobRecord const &  blob, bool is_found);
    void OnGetBlobError(shared_ptr<CPSGS_Request>  request,
                        shared_ptr<CPSGS_Reply>  reply,
                        CCassBlobFetch *  fetch_details,
                        CRequestStatus::ECode  status, int  code,
                        EDiagSev  severity, const string &  message);
    void OnGetBlobChunk(shared_ptr<CPSGS_Request>  request,
                        shared_ptr<CPSGS_Reply>  reply,
                        bool  cancelled,
                        CCassBlobFetch *  fetch_details,
                        const unsigned char *  chunk_data,
                        unsigned int  data_size, int  chunk_no);

private:
    void x_OnBlobPropNoneTSE(shared_ptr<CPSGS_Reply>  reply,
                             CCassBlobFetch *  fetch_details);
    void x_OnBlobPropSlimTSE(shared_ptr<CPSGS_Request>  request,
                             shared_ptr<CPSGS_Reply>  reply,
                             TBlobPropsCB  blob_props_cb,
                             TBlobChunkCB  blob_chunk_cb,
                             TBlobErrorCB  blob_error_cb,
                             CCassBlobFetch *  fetch_details,
                             CBlobRecord const &  blob);
    void x_OnBlobPropSmartTSE(shared_ptr<CPSGS_Request>  request,
                              shared_ptr<CPSGS_Reply>  reply,
                              TBlobPropsCB  blob_props_cb,
                              TBlobChunkCB  blob_chunk_cb,
                              TBlobErrorCB  blob_error_cb,
                              CCassBlobFetch *  fetch_details,
                              CBlobRecord const &  blob);
    void x_OnBlobPropWholeTSE(shared_ptr<CPSGS_Request>  request,
                              shared_ptr<CPSGS_Reply>  reply,
                              TBlobPropsCB  blob_props_cb,
                              TBlobChunkCB  blob_chunk_cb,
                              TBlobErrorCB  blob_error_cb,
                              CCassBlobFetch *  fetch_details,
                              CBlobRecord const &  blob);
    void x_OnBlobPropOrigTSE(shared_ptr<CPSGS_Request>  request,
                             shared_ptr<CPSGS_Reply>  reply,
                             TBlobChunkCB  blob_chunk_cb,
                             TBlobErrorCB  blob_error_cb,
                             CCassBlobFetch *  fetch_details,
                             CBlobRecord const &  blob);

private:
    void x_RequestOriginalBlobChunks(shared_ptr<CPSGS_Request>  request,
                                     shared_ptr<CPSGS_Reply>  reply,
                                     TBlobChunkCB  blob_chunk_cb,
                                     TBlobErrorCB  blob_error_cb,
                                     CCassBlobFetch *  fetch_details,
                                     CBlobRecord const &  blob);
    void x_RequestID2BlobChunks(shared_ptr<CPSGS_Request>  request,
                                shared_ptr<CPSGS_Reply>  reply,
                                TBlobPropsCB  blob_props_cb,
                                TBlobChunkCB  blob_chunk_cb,
                                TBlobErrorCB  blob_error_cb,
                                CCassBlobFetch *  fetch_details,
                                CBlobRecord const &  blob,
                                bool  info_blob_only);
    void x_RequestId2SplitBlobs(shared_ptr<CPSGS_Request>  request,
                                shared_ptr<CPSGS_Reply>  reply,
                                TBlobPropsCB  blob_props_cb,
                                TBlobChunkCB  blob_chunk_cb,
                                TBlobErrorCB  blob_error_cb,
                                CCassBlobFetch *  fetch_details,
                                const string &  sat_name);


private:
    enum EPSGS_BlobCacheCheckResult {
        ePSGS_InCache,
        ePSGS_NotInCache
    };

    EPSGS_BlobCacheCheckResult
    x_CheckExcludeBlobCache(shared_ptr<CPSGS_Request>  request,
                            shared_ptr<CPSGS_Reply>  reply,
                            CCassBlobFetch *  fetch_details,
                            SPSGS_BlobRequestBase &  blob_request);
    void x_OnBlobPropNotFound(shared_ptr<CPSGS_Request>  request,
                              shared_ptr<CPSGS_Reply>  reply,
                              CCassBlobFetch *  fetch_details);
    bool x_ParseId2Info(shared_ptr<CPSGS_Request>  request,
                        shared_ptr<CPSGS_Reply>  reply,
                        CCassBlobFetch *  fetch_details,
                        CBlobRecord const &  blob);

private:
    void x_SetFinished(shared_ptr<CPSGS_Reply>  reply,
                       CCassBlobFetch *  fetch_details);

private:
    unique_ptr<CPSGId2Info>     m_Id2Info;
};

#endif  // PSGS_CASSBLOBBASE__HPP

