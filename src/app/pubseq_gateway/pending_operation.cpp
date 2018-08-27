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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 */
#include <ncbi_pch.hpp>

#include "pubseq_gateway.hpp"
#include "pending_operation.hpp"
#include "pubseq_gateway_exception.hpp"
#include "pubseq_gateway_convert_utils.hpp"
#include "pubseq_gateway_logging.hpp"

#include <objects/seqloc/Seq_id.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info.hpp>
#include <objtools/pubseq_gateway/protobuf/psg_protobuf_data.hpp>
USING_IDBLOB_SCOPE;
USING_SCOPE(objects);


CPendingOperation::CPendingOperation(const SBlobRequest &  blob_request,
                                     size_t  initial_reply_chunks,
                                     shared_ptr<CCassConnection>  conn,
                                     unsigned int  timeout,
                                     unsigned int  max_retries,
                                     CRef<CRequestContext>  request_context) :
    m_Reply(nullptr),
    m_Conn(conn),
    m_Timeout(timeout),
    m_MaxRetries(max_retries),
    m_OverallStatus(CRequestStatus::e200_Ok),
    m_IsResolveRequest(false),
    m_BlobRequest(blob_request),
    m_TotalSentReplyChunks(initial_reply_chunks),
    m_Cancelled(false),
    m_RequestContext(request_context),
    m_NextItemId(0)
{
    if (m_BlobRequest.GetBlobIdentificationType() == eBySeqId) {
        PSG_TRACE("CPendingOperation::CPendingOperation: blob "
                 "requested by seq_id/seq_id_type: " <<
                 m_BlobRequest.m_SeqId << "." << m_BlobRequest.m_SeqIdType <<
                 ", this: " << this);
    } else {
        PSG_TRACE("CPendingOperation::CPendingOperation: blob "
                 "requested by sat/sat_key: " <<
                 m_BlobRequest.m_BlobId.m_Sat << "." <<
                 m_BlobRequest.m_BlobId.m_SatKey <<
                 ", this: " << this);
    }
}


CPendingOperation::CPendingOperation(const SResolveRequest &  resolve_request,
                                     size_t  initial_reply_chunks,
                                     shared_ptr<CCassConnection>  conn,
                                     unsigned int  timeout,
                                     unsigned int  max_retries,
                                     CRef<CRequestContext>  request_context) :
    m_Reply(nullptr),
    m_Conn(conn),
    m_Timeout(timeout),
    m_MaxRetries(max_retries),
    m_OverallStatus(CRequestStatus::e200_Ok),
    m_IsResolveRequest(true),
    m_ResolveRequest(resolve_request),
    m_TotalSentReplyChunks(initial_reply_chunks),
    m_Cancelled(false),
    m_RequestContext(request_context),
    m_NextItemId(0)
{
    PSG_TRACE("CPendingOperation::CPendingOperation: resolution "
              "request by seq_id/seq_id_type: " <<
              m_ResolveRequest.m_SeqId << "." << m_ResolveRequest.m_SeqIdType <<
              ", this: " << this);
}


CPendingOperation::~CPendingOperation()
{
    CRequestContextResetter     context_resetter;
    x_SetRequestContext();

    if (!m_IsResolveRequest) {
        if (m_BlobRequest.GetBlobIdentificationType() == eBySeqId) {
            PSG_TRACE("CPendingOperation::~CPendingOperation: blob "
                      "requested by seq_id/seq_id_type: " <<
                      m_BlobRequest.m_SeqId << "." <<
                      m_BlobRequest.m_SeqIdType <<
                      ", this: " << this);
        } else {
            PSG_TRACE("CPendingOperation::~CPendingOperation: blob "
                      "requested by sat/sat_key: " <<
                      m_BlobRequest.m_BlobId.m_Sat << "." <<
                      m_BlobRequest.m_BlobId.m_SatKey <<
                      ", this: " << this);
        }
    }

    if (m_MainBlobFetchDetails)
        PSG_TRACE("CPendingOperation::~CPendingOperation: "
                  "main blob loader: " <<
                  m_MainBlobFetchDetails->m_Loader.get());
    if (m_Id2ShellFetchDetails)
        PSG_TRACE("CPendingOperation::~CPendingOperation: "
                  "Id2Shell blob loader: " <<
                  m_Id2ShellFetchDetails->m_Loader.get());
    if (m_Id2InfoFetchDetails)
        PSG_TRACE("CPendingOperation::~CPendingOperation: "
                  "Id2Info blob loader: " <<
                  m_Id2InfoFetchDetails->m_Loader.get());

    for (auto &  details: m_Id2ChunkFetchDetails) {
        if (details) {
            PSG_TRACE("CPendingOperation::~CPendingOperation: "
                      "Id2SplibChunks blob loader: " <<
                      details->m_Loader.get());
        }
    }

    // Just in case if a request ended without a normal request stop,
    // finish it here as the last resort. (is it Cancel() case?)
    x_PrintRequestStop(m_OverallStatus);
}


void CPendingOperation::Clear()
{
    CRequestContextResetter     context_resetter;
    x_SetRequestContext();

    if (!m_IsResolveRequest) {
        if (m_BlobRequest.GetBlobIdentificationType() == eBySeqId) {
            PSG_TRACE("CPendingOperation::Clear: blob "
                      "requested by seq_id/seq_id_type: " <<
                      m_BlobRequest.m_SeqId << "." <<
                      m_BlobRequest.m_SeqIdType <<
                      ", this: " << this);
        } else {
            PSG_TRACE("CPendingOperation::Clear: blob "
                      "requested by sat/sat_key: " <<
                      m_BlobRequest.m_BlobId.m_Sat << "." <<
                      m_BlobRequest.m_BlobId.m_SatKey <<
                      ", this: " << this);
        }
    }

    if (m_MainBlobFetchDetails) {
        PSG_TRACE("CPendingOperation::Clear: "
                  "main blob loader: " <<
                  m_MainBlobFetchDetails->m_Loader.get());
        m_MainBlobFetchDetails->m_Loader->SetDataReadyCB(nullptr, nullptr);
        m_MainBlobFetchDetails->m_Loader->SetErrorCB(nullptr);
        m_MainBlobFetchDetails->m_Loader->SetChunkCallback(nullptr);
        m_MainBlobFetchDetails = nullptr;
    }
    if (m_Id2ShellFetchDetails) {
        PSG_TRACE("CPendingOperation::Clear: "
                  "Id2Shell blob loader: " <<
                  m_Id2ShellFetchDetails->m_Loader.get());
        m_Id2ShellFetchDetails->m_Loader->SetDataReadyCB(nullptr, nullptr);
        m_Id2ShellFetchDetails->m_Loader->SetErrorCB(nullptr);
        m_Id2ShellFetchDetails->m_Loader->SetChunkCallback(nullptr);
        m_Id2ShellFetchDetails = nullptr;
    }
    if (m_Id2InfoFetchDetails) {
        PSG_TRACE("CPendingOperation::Clear: "
                  "Id2Info blob loader: " <<
                  m_Id2InfoFetchDetails->m_Loader.get());
        m_Id2InfoFetchDetails->m_Loader->SetDataReadyCB(nullptr, nullptr);
        m_Id2InfoFetchDetails->m_Loader->SetErrorCB(nullptr);
        m_Id2InfoFetchDetails->m_Loader->SetChunkCallback(nullptr);
        m_Id2InfoFetchDetails = nullptr;
    }

    for (auto &  details: m_Id2ChunkFetchDetails) {
        if (details) {
            PSG_TRACE("CPendingOperation::Clear: "
                      "Id2SplibChunks blob loader: " <<
                      details->m_Loader.get());
            details->m_Loader->SetDataReadyCB(nullptr, nullptr);
            details->m_Loader->SetErrorCB(nullptr);
            details->m_Loader->SetChunkCallback(nullptr);
            details = nullptr;
        }
    }

    m_Chunks.clear();
    m_Reply = nullptr;
    m_Cancelled = false;

    m_TotalSentReplyChunks = 0;
}


void CPendingOperation::Start(HST::CHttpReply<CPendingOperation>& resp)
{
    m_Reply = &resp;

    // There are a few options here:
    // - this is a resolution request
    // - the blobs were requested by sat/sat_key
    // - the blob was requested by a seq_id/seq_id_type
    if (m_IsResolveRequest) {
        x_ProcessResolveRequest();
        return;
    }

    if (m_BlobRequest.GetBlobIdentificationType() == eBySeqId) {
        // By seq_id -> search for the bioseq key info in the cache only
        string          err_msg;
        SBioseqKey      bioseq_key = x_ResolveInputSeqId(err_msg);

        if (!err_msg.empty()) {
            // Bad request error, 400
            PSG_WARNING(err_msg);
            PrepareReplyMessage(err_msg, CRequestStatus::e400_BadRequest,
                                eUnresolvedSeqId, eDiag_Error);
            x_SendReplyCompletion(true);
            m_Reply->Send(m_Chunks, true);
            m_Chunks.clear();
            return;
        }

        if (!bioseq_key.IsValid()) {
            // Could not resolve, send 404
            size_t      item_id = GetItemId();
            err_msg = "Could not resolve seq_id " + m_BlobRequest.m_SeqId;
            PrepareBioseqMessage(item_id, err_msg,
                                 CRequestStatus::e404_NotFound,
                                 eNoBioseqInfo, eDiag_Error);
            PrepareBioseqCompletion(item_id, 2);
            x_SendReplyCompletion(true);
            m_Reply->Send(m_Chunks, true);
            m_Chunks.clear();
            return;
        }


        // Resolved; may be bioseq info is already at hand
        if (bioseq_key.m_BioseqInfo.empty()) {
            // Try to get bioseq info from the cache
            x_LookupCachedBioseqInfo(bioseq_key.m_Accession,
                                     bioseq_key.m_Version,
                                     bioseq_key.m_SeqIdType,
                                     bioseq_key.m_BioseqInfo);
        }

        if (bioseq_key.m_BioseqInfo.empty()) {
            // If there is no bioseq info found then it is a server error 500
            UpdateOverallStatus(CRequestStatus::e500_InternalServerError);

            err_msg = "Data inconsistency: the bioseq key info was resolved "
                      "for seq_id " + m_BlobRequest.m_SeqId + " but the bioseq "
                      "info is not in cache";

            size_t      item_id = GetItemId();
            PrepareBioseqMessage(item_id, err_msg,
                                 CRequestStatus::e500_InternalServerError,
                                 eNoBioseqInfo, eDiag_Error);
            PrepareBioseqCompletion(item_id, 2);
            x_SendReplyCompletion(true);
            m_Reply->Send(m_Chunks, true);
            m_Chunks.clear();
            return;
        }

        SBioseqInfo     bioseq_info;
        bioseq_info.m_Accession = bioseq_key.m_Accession;
        bioseq_info.m_Version = bioseq_key.m_Version;
        bioseq_info.m_SeqIdType = bioseq_key.m_SeqIdType;
        ConvertBioseqProtobufToBioseqInfo(bioseq_key.m_BioseqInfo,
                                          bioseq_info);

        // Send BIOSEQ_INFO
        x_SendBioseqInfo("", bioseq_info, eJsonFormat);

        // Translate sat to keyspace
        SBlobId     blob_id(bioseq_info.m_Sat, bioseq_info.m_SatKey);

        if (!x_SatToSatName(m_BlobRequest, blob_id)) {
            // This is server data inconsistency error, so 500 (not 404)
            UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
            x_SendReplyCompletion(true);
            resp.Send(m_Chunks, true);
            m_Chunks.clear();
            return;
        }

        // retrieve BLOB_PROP & CHUNKS (if needed)
        m_BlobRequest.m_BlobId = blob_id;

        // Calculate effective no_tse flag
        if (m_BlobRequest.m_IncludeDataFlags & fServOrigTSE ||
            m_BlobRequest.m_IncludeDataFlags & fServWholeTSE)
            m_BlobRequest.m_IncludeDataFlags &= ~fServNoTSE;

        if (m_BlobRequest.m_IncludeDataFlags & fServNoTSE) {
            m_BlobRequest.m_NeedBlobProp = true;
            m_BlobRequest.m_NeedChunks = false;
            m_BlobRequest.m_Optional = false;
        } else {
            if (m_BlobRequest.m_IncludeDataFlags & fServOrigTSE) {
                m_BlobRequest.m_NeedBlobProp = true;
                m_BlobRequest.m_NeedChunks = true;
                m_BlobRequest.m_Optional = false;
            } else {
                m_BlobRequest.m_NeedBlobProp = true;
                m_BlobRequest.m_NeedChunks = false;
                m_BlobRequest.m_Optional = false;
            }
        }
    }

    x_StartMainBlobRequest();
}


void CPendingOperation::x_ProcessResolveRequest(void)
{
    string          err_msg;
    SBioseqKey      bioseq_key = x_ResolveInputSeqId(err_msg);

    if (!err_msg.empty()) {
        // Bad request error, 400
        PSG_WARNING(err_msg);
        if (x_UsePsgProtocol()) {
            PrepareReplyMessage(err_msg, CRequestStatus::e400_BadRequest,
                                eUnresolvedSeqId, eDiag_Error);
            x_SendReplyCompletion(true);
            m_Reply->Send(m_Chunks, true);
            m_Chunks.clear();
        } else {
            m_Reply->Send400("Bad Request", err_msg.c_str());
            x_PrintRequestStop(m_OverallStatus);
        }
        return;
    }

    if (!bioseq_key.IsValid()) {
        // Could not resolve, send 404
        err_msg = "Could not resolve seq_id " + m_ResolveRequest.m_SeqId;

        if (x_UsePsgProtocol()) {
            size_t      item_id = GetItemId();
            PrepareBioseqMessage(item_id, err_msg,
                                 CRequestStatus::e404_NotFound,
                                 eNoBioseqInfo, eDiag_Error);
            PrepareBioseqCompletion(item_id, 2);
            x_SendReplyCompletion(true);
            m_Reply->Send(m_Chunks, true);
            m_Chunks.clear();
        } else {
            m_Reply->Send404("Not Found", err_msg.c_str());
            x_PrintRequestStop(m_OverallStatus);
        }
        return;
    }

    // Resolved; may be bioseq info is already at hand
    if (bioseq_key.m_BioseqInfo.empty()) {
        // Try to get bioseq info from the cache
        x_LookupCachedBioseqInfo(bioseq_key.m_Accession,
                                 bioseq_key.m_Version,
                                 bioseq_key.m_SeqIdType,
                                 bioseq_key.m_BioseqInfo);
    }

    if (bioseq_key.m_BioseqInfo.empty()) {
        // If there is no bioseq info found then it is a server error 500
        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);

        err_msg = "Data inconsistency: the bioseq key info was resolved "
                  "for seq_id " + m_ResolveRequest.m_SeqId + 
                  " but the bioseq info is not in cache";

        if (x_UsePsgProtocol()) {
            size_t      item_id = GetItemId();
            PrepareBioseqMessage(item_id, err_msg,
                                 CRequestStatus::e500_InternalServerError,
                                 eNoBioseqInfo, eDiag_Error);
            PrepareBioseqCompletion(item_id, 2);
            x_SendReplyCompletion(true);
            m_Reply->Send(m_Chunks, true);
            m_Chunks.clear();
        } else {
            m_Reply->Send500("Internal Server Error", err_msg.c_str());
            x_PrintRequestStop(m_OverallStatus);
        }
        return;
    }

    // Bioseq info is found, send it to the client
    SBioseqInfo     bioseq_info;
    bioseq_info.m_Accession = bioseq_key.m_Accession;
    bioseq_info.m_Version = bioseq_key.m_Version;
    bioseq_info.m_SeqIdType = bioseq_key.m_SeqIdType;
    x_SendBioseqInfo(bioseq_key.m_BioseqInfo,
                     bioseq_info, m_ResolveRequest.m_OutputFormat);
    if (x_UsePsgProtocol()) {
        x_SendReplyCompletion(true);
        m_Reply->Send(m_Chunks, true);
        m_Chunks.clear();
    } else {
        x_PrintRequestStop(m_OverallStatus);
    }
}


void CPendingOperation::x_StartMainBlobRequest(void)
{
    // Here: m_BlobRequest has the resolved sat and a sat_key regardless
    //       how the blob was requested
    m_MainBlobFetchDetails.reset(new SBlobFetchDetails(m_BlobRequest));

    unique_ptr<CBlobRecord>     blob_record(new CBlobRecord);
    bool            cache_hit = x_LookupBlobPropCache(
                                    m_BlobRequest.m_BlobId.m_Sat,
                                    m_BlobRequest.m_BlobId.m_SatKey,
                                    m_BlobRequest.m_LastModified,
                                    *blob_record.get());
    if (cache_hit) {
        m_MainBlobFetchDetails->m_Loader.reset(
            new CCassBlobTaskLoadBlob(
                            m_Timeout, m_MaxRetries, m_Conn,
                            m_BlobRequest.m_BlobId.m_SatName,
                            std::move(blob_record),
                            m_BlobRequest.m_NeedChunks,
                            nullptr));
    } else if (m_BlobRequest.m_LastModified == INT64_MIN) {
        m_MainBlobFetchDetails->m_Loader.reset(
            new CCassBlobTaskLoadBlob(
                            m_Timeout, m_MaxRetries, m_Conn,
                            m_BlobRequest.m_BlobId.m_SatName,
                            m_BlobRequest.m_BlobId.m_SatKey,
                            m_BlobRequest.m_NeedChunks,
                            nullptr));
    } else {
        m_MainBlobFetchDetails->m_Loader.reset(
            new CCassBlobTaskLoadBlob(
                            m_Timeout, m_MaxRetries, m_Conn,
                            m_BlobRequest.m_BlobId.m_SatName,
                            m_BlobRequest.m_BlobId.m_SatKey,
                            m_BlobRequest.m_LastModified,
                            m_BlobRequest.m_NeedChunks,
                            nullptr));
    }

    m_MainBlobFetchDetails->m_Loader->SetDataReadyCB(
                            HST::CHttpReply<CPendingOperation>::s_DataReady,
                            m_Reply);

    m_MainBlobFetchDetails->m_Loader->SetErrorCB(
                CGetBlobErrorCallback(this, m_Reply,
                                      m_MainBlobFetchDetails.get()));
    m_MainBlobFetchDetails->m_Loader->SetPropsCallback(
                CBlobPropCallback(this, m_Reply,
                                  m_MainBlobFetchDetails.get()));

    if (m_BlobRequest.m_NeedChunks)
        m_MainBlobFetchDetails->m_Loader->SetChunkCallback(
                    CBlobChunkCallback(this, m_Reply,
                                       m_MainBlobFetchDetails.get()));

    m_MainBlobFetchDetails->m_Loader->Wait();
}


void CPendingOperation::Peek(HST::CHttpReply<CPendingOperation>& resp,
                             bool  need_wait)
{
    if (m_Cancelled) {
        if (resp.IsOutputReady() && !resp.IsFinished()) {
            resp.Send(nullptr, 0, true, true);
        }
        return;
    }
    // 1 -> call m_Loader->Wait1 to pick data
    // 2 -> check if we have ready-to-send buffers
    // 3 -> call resp->Send()  to send what we have if it is ready
    if (m_MainBlobFetchDetails)
        x_Peek(resp, need_wait, m_MainBlobFetchDetails);
    if (m_Id2ShellFetchDetails)
        x_Peek(resp, need_wait, m_Id2ShellFetchDetails);
    if (m_Id2InfoFetchDetails)
        x_Peek(resp, need_wait, m_Id2InfoFetchDetails);
    if (m_OriginalBlobChunkFetch)
        x_Peek(resp, need_wait, m_OriginalBlobChunkFetch);

    for (auto &  details: m_Id2ChunkFetchDetails) {
        if (details)
            x_Peek(resp, need_wait, details);
    }

    bool    all_finished_read = x_AllFinishedRead();
    if (resp.IsOutputReady() && (!m_Chunks.empty() || all_finished_read)) {
        resp.Send(m_Chunks, all_finished_read);
        m_Chunks.clear();
    }
}


void CPendingOperation::x_Peek(HST::CHttpReply<CPendingOperation>& resp,
                               bool  need_wait,
                               unique_ptr<SBlobFetchDetails> &  fetch_details)
{
    if (!fetch_details->m_Loader)
        return;

    if (need_wait)
        if (!fetch_details->m_FinishedRead)
            fetch_details->m_Loader->Wait();

    if (fetch_details->m_Loader->HasError() &&
        resp.IsOutputReady() && !resp.IsFinished()) {
        // Send an error
        string               error = fetch_details->m_Loader->LastError();
        CPubseqGatewayApp *  app = CPubseqGatewayApp::GetInstance();
        app->GetErrorCounters().IncUnknownError();

        PSG_ERROR(error);

        if (fetch_details->IsBlobPropStage()) {
            PrepareBlobPropMessage(fetch_details.get(), error,
                                   CRequestStatus::e500_InternalServerError,
                                   eUnknownError, eDiag_Error);
            PrepareBlobPropCompletion(fetch_details.get());
        } else {
            PrepareBlobMessage(fetch_details.get(), error,
                               CRequestStatus::e500_InternalServerError,
                               eUnknownError, eDiag_Error);
            PrepareBlobCompletion(fetch_details.get());
        }

        // Mark finished
        fetch_details->m_FinishedRead = true;

        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        x_SendReplyCompletion();
    }
}


bool CPendingOperation::x_AllFinishedRead(void) const
{
    size_t  started_count = 0;

    if (m_MainBlobFetchDetails) {
        ++started_count;
        if (!m_MainBlobFetchDetails->m_FinishedRead)
            return false;
    }

    if (m_Id2ShellFetchDetails) {
        ++started_count;
        if (!m_Id2ShellFetchDetails->m_FinishedRead)
            return false;
    }

    if (m_Id2InfoFetchDetails) {
        ++started_count;
        if (!m_Id2InfoFetchDetails->m_FinishedRead)
            return false;
    }

    if (m_OriginalBlobChunkFetch) {
        ++started_count;
        if (!m_OriginalBlobChunkFetch->m_FinishedRead)
            return false;
    }

    for (auto &  details: m_Id2ChunkFetchDetails) {
        if (details) {
            ++started_count;
            if (!details->m_FinishedRead)
                return false;
        }
    }

    return started_count != 0;
}


void CPendingOperation::x_SendReplyCompletion(bool  forced)
{
    // Send the reply completion only if needed
    if (x_AllFinishedRead() || forced) {
        // +1 for the reply completion itself
        PrepareReplyCompletion(m_TotalSentReplyChunks + 1);

        // No need to set the context/engage context resetter: they set outside
        x_PrintRequestStop(m_OverallStatus);
    }
}


void CPendingOperation::x_SetRequestContext(void)
{
    if (m_RequestContext.NotNull())
        CDiagContext::SetRequestContext(m_RequestContext);
}


void CPendingOperation::x_PrintRequestStop(int  status)
{
    if (m_RequestContext.NotNull()) {
        CDiagContext::SetRequestContext(m_RequestContext);
        m_RequestContext->SetReadOnly(false);
        m_RequestContext->SetRequestStatus(status);
        GetDiagContext().PrintRequestStop();
        m_RequestContext.Reset();
        CDiagContext::SetRequestContext(NULL);
    }
}


SBioseqKey CPendingOperation::x_ResolveInputSeqId(string &  err_msg)
{
    SBioseqKey      bioseq_key;
    CTempString     url_seq_id = m_BlobRequest.m_SeqId;

    if (m_IsResolveRequest) {
        url_seq_id = m_ResolveRequest.m_SeqId;
    }

    try {
        CSeq_id                 parsed_seq_id(url_seq_id);
        const CTextseq_id *     text_seq_id = parsed_seq_id.GetTextseq_Id();

        if (text_seq_id != NULL) {
            try {
                // The only possible problem is a mismatch between
                // acc/ver/seq_id_type
                x_ResolveInputSeqId(parsed_seq_id, text_seq_id, bioseq_key);
                if (bioseq_key.IsValid())
                    return bioseq_key;
            } catch (const exception &  ex) {
                err_msg = ex.what();
                return bioseq_key;
            } catch (...) {
                err_msg = "Unknown error while resolving input seq_id";
                return bioseq_key;
            }
        }
    } catch (...) {
        // Choke and resolve it as is below
    }

    return x_ResolveInputSeqIdAsIs();
}


void CPendingOperation::x_ResolveInputSeqId(const CSeq_id &  parsed_seq_id,
                                            const CTextseq_id *  text_seq_id,
                                            SBioseqKey &  bioseq_key)
{
    // There are two paths:
    // - Path 1: using accession and [version] and [seq_id_type]
    // - Path 2: using name
    // Then the paths need to be compared
    SBioseqKey  bioseq_info_path1 = x_ResolveInputSeqIdPath1(parsed_seq_id,
                                                             text_seq_id);
    SBioseqKey  bioseq_info_path2 = x_ResolveInputSeqIdPath2(parsed_seq_id,
                                                             text_seq_id);

    if (!bioseq_info_path1.IsValid() && !bioseq_info_path2.IsValid())
        return;     // No resolution

    if (!bioseq_info_path1.IsValid()) {
        bioseq_key = std::move(bioseq_info_path2);
        return;
    }

    if (!bioseq_info_path2.IsValid()) {
        bioseq_key = std::move(bioseq_info_path1);
        return;
    }

    // Both found
    if (bioseq_info_path1.m_Accession != bioseq_info_path2.m_Accession)
        NCBI_THROW(CPubseqGatewayException, eAccessionMismatch,
                   "Resolving input seq_id failure: accession mismatch. "
                   "Path via accession produced '" +
                   bioseq_info_path1.m_Accession + "' while path via name "
                   "produced '" + bioseq_info_path2.m_Accession + "'");
    if (bioseq_info_path1.m_Version != bioseq_info_path2.m_Version)
        NCBI_THROW(CPubseqGatewayException, eVersionMismatch,
                   "Resolving input seq_id failure: version mismatch. "
                   "Path via accession produced " +
                   NStr::NumericToString(bioseq_info_path1.m_Version) +
                   " while path via name produced " +
                   NStr::NumericToString(bioseq_info_path2.m_Version));
    if (bioseq_info_path1.m_SeqIdType != bioseq_info_path2.m_SeqIdType)
        NCBI_THROW(CPubseqGatewayException, eSeqIdMismatch,
                   "Resolving input seq_id failure: seq_id_type mismatch. "
                   "Path via accession produced " +
                   NStr::NumericToString(bioseq_info_path1.m_SeqIdType) +
                   " while path via name produced " +
                   NStr::NumericToString(bioseq_info_path2.m_SeqIdType));

    // Important to take #1 because there could be cached bioseq_info
    bioseq_key = std::move(bioseq_info_path1);
}


SBioseqKey
CPendingOperation::x_ResolveInputSeqIdPath1(const CSeq_id &  parsed_seq_id,
                                            const CTextseq_id *  text_seq_id)
{
    SBioseqKey              bioseq_key;

    bioseq_key.m_Accession = text_seq_id->GetAccession();
    bioseq_key.m_SeqIdType = x_GetEffectiveSeqIdType(parsed_seq_id);

    if (text_seq_id->CanGetVersion())
        bioseq_key.m_Version = text_seq_id->GetVersion();

    if (x_LookupCachedBioseqInfo(bioseq_key.m_Accession,
                                 bioseq_key.m_Version,
                                 bioseq_key.m_SeqIdType,
                                 bioseq_key.m_BioseqInfo))
        return bioseq_key;


    // Another try via Fasta
    string      csi_cache_data;
    string      seq_id_content;
    parsed_seq_id.GetLabel(&seq_id_content, CSeq_id::eFastaContent);

    if (x_LookupCachedCsi(seq_id_content, bioseq_key.m_SeqIdType,
                          bioseq_key.m_SeqIdType >= 0, csi_cache_data)) {
        ConvertSi2csiToBioseqKey(csi_cache_data, bioseq_key);
        return bioseq_key;
    }

    // Another try with what has come from url
    return x_ResolveInputSeqIdAsIs();
}


bool
CPendingOperation::x_LookupCachedBioseqInfo(const string &  accession,
                                            int &  version,
                                            int  seq_id_type,
                                            string &  bioseq_info_cache_data)
{
    bool                    cache_hit = false;
    CPubseqGatewayCache *   cache = CPubseqGatewayApp::GetInstance()->
                                                            GetLookupCache();
    if (version >= 0) {
        if (seq_id_type >= 0)
            cache_hit = cache->LookupBioseqInfoByAccessionVersionSeqIdType(
                                        accession, version, seq_id_type,
                                        bioseq_info_cache_data);
        else
            cache_hit = cache->LookupBioseqInfoByAccessionVersion(
                                        accession, version, seq_id_type,
                                        bioseq_info_cache_data);
    } else {
        if (seq_id_type >= 0) {
            version = 0;
            cache_hit = cache->LookupBioseqInfoByAccessionVersionSeqIdType(
                                        accession, version, seq_id_type,
                                        bioseq_info_cache_data);
        } else
            cache_hit = cache->LookupBioseqInfoByAccession(
                                        accession, version, seq_id_type,
                                        bioseq_info_cache_data);
    }

    if (cache_hit)
        CPubseqGatewayApp::GetInstance()->GetCacheCounters().
                                                IncBioseqInfoCacheHit();
    else
        CPubseqGatewayApp::GetInstance()->GetCacheCounters().
                                                IncBioseqInfoCacheMiss();
    return cache_hit;
}


bool
CPendingOperation::x_LookupCachedCsi(const string &  seq_id,
                                     int &  seq_id_type,
                                     bool  seq_id_type_provided,
                                     string &  csi_cache_data)
{
    bool                    cache_hit = false;
    CPubseqGatewayCache *   cache = CPubseqGatewayApp::GetInstance()->
                                                            GetLookupCache();
    if (!seq_id_type_provided)
        cache_hit = cache->LookupCsiBySeqId(seq_id, seq_id_type,
                                            csi_cache_data);
    else
        cache_hit = cache->LookupCsiBySeqIdSeqIdType(seq_id, seq_id_type,
                                                     csi_cache_data);

    if (cache_hit)
        CPubseqGatewayApp::GetInstance()->GetCacheCounters().
                                                IncSi2csiCacheHit();
    else
        CPubseqGatewayApp::GetInstance()->GetCacheCounters().
                                                IncSi2csiCacheMiss();
    return cache_hit;
}


SBioseqKey CPendingOperation::x_ResolveInputSeqIdAsIs(void)
{
    SBioseqKey  bioseq_key;

    string      url_seq_id = m_BlobRequest.m_SeqId;
    int         url_seq_id_type = m_BlobRequest.m_SeqIdType;
    bool        url_seq_id_type_provided = m_BlobRequest.m_SeqIdTypeProvided;
    if (m_IsResolveRequest) {
        url_seq_id = m_ResolveRequest.m_SeqId;
        url_seq_id_type = m_ResolveRequest.m_SeqIdType;
        url_seq_id_type_provided = m_ResolveRequest.m_SeqIdTypeProvided;
    }

    string      csi_cache_data;
    if (x_LookupCachedCsi(url_seq_id, url_seq_id_type,
                          url_seq_id_type_provided, csi_cache_data))
        ConvertSi2csiToBioseqKey(csi_cache_data, bioseq_key);

    return bioseq_key;
}


SBioseqKey
CPendingOperation::x_ResolveInputSeqIdPath2(const CSeq_id &  parsed_seq_id,
                                            const CTextseq_id *  text_seq_id)
{
    SBioseqKey      bioseq_key;

    if (text_seq_id->CanGetName()) {
        string      name = text_seq_id->GetName();
        int         eff_seq_id_type = x_GetEffectiveSeqIdType(parsed_seq_id);

        string      csi_cache_data;
        if (x_LookupCachedCsi(name, eff_seq_id_type, eff_seq_id_type >= 0,
                              csi_cache_data))
            ConvertSi2csiToBioseqKey(csi_cache_data, bioseq_key);
    }
    return bioseq_key;
}


int CPendingOperation::x_GetEffectiveSeqIdType(const CSeq_id &  parsed_seq_id)
{
    CSeq_id_Base::E_Choice  parsed_seq_id_type = parsed_seq_id.Which();
    bool                    parsed_seq_id_type_found = (parsed_seq_id_type !=
                                                        CSeq_id_Base::e_not_set);

    int         seq_id_type = m_BlobRequest.m_SeqIdType;
    bool        seq_id_type_provided = m_BlobRequest.m_SeqIdTypeProvided;
    if (m_IsResolveRequest) {
        seq_id_type = m_ResolveRequest.m_SeqIdType;
        seq_id_type_provided = m_ResolveRequest.m_SeqIdTypeProvided;
    }

    if (!parsed_seq_id_type_found && !seq_id_type_provided)
        return -1;
    if (!parsed_seq_id_type_found)
        return seq_id_type;
    if (!seq_id_type_provided)
        return parsed_seq_id_type;

    // Both found
    if (parsed_seq_id_type != seq_id_type)
        NCBI_THROW(CPubseqGatewayException, eSeqIdMismatch,
                   "Resolving input seq_id failure: seq_id_type mismatch. "
                   "URL provided is " +
                   NStr::NumericToString(seq_id_type) +
                   " while the parsed one is " +
                   NStr::NumericToString(int(parsed_seq_id_type)));

    return seq_id_type;
}


void CPendingOperation::x_SendBioseqInfo(const string &  protobuf_bioseq_info,
                                         SBioseqInfo &  bioseq_info,
                                         EOutputFormat  output_format)
{
    EOutputFormat       effective_output_format = output_format;

    if (output_format == eNativeFormat || output_format == eUnknownFormat) {
        if (protobuf_bioseq_info.empty())
            effective_output_format = eJsonFormat;
        else
            effective_output_format = eProtobufFormat;
    }

    string              data_to_send;

    if (effective_output_format == eJsonFormat) {
        if (!protobuf_bioseq_info.empty())
            ConvertBioseqProtobufToBioseqInfo(protobuf_bioseq_info,
                                              bioseq_info);

        ConvertBioseqInfoToJson(
                bioseq_info,
                m_IsResolveRequest ? m_ResolveRequest.m_IncludeDataFlags :
                                     m_BlobRequest.m_IncludeDataFlags,
                data_to_send);
    } else {
        if (protobuf_bioseq_info.empty())
            ConvertBioseqInfoToBioseqProtobuf(bioseq_info, data_to_send);
        else
            data_to_send = protobuf_bioseq_info;
    }

    if (x_UsePsgProtocol()) {
        // Send it as the PSG protocol
        size_t              item_id = GetItemId();
        PrepareBioseqData(item_id, data_to_send, effective_output_format);
        PrepareBioseqCompletion(item_id, 2);
    } else {
        // Send it as the HTTP data
        if (effective_output_format == eJsonFormat)
            m_Reply->SetJsonContentType();
        else
            m_Reply->SetProtobufContentType();
        m_Reply->SetContentLength(data_to_send.length());
        m_Reply->SendOk(data_to_send.data(), data_to_send.length(), false);
    }
}


bool CPendingOperation::x_UsePsgProtocol(void) const
{
    if (!m_IsResolveRequest)
        return true;
    return m_ResolveRequest.m_UsePsgProtocol;
}


bool CPendingOperation::x_LookupBlobPropCache(int  sat, int  sat_key,
                                              int64_t &  last_modified,
                                              CBlobRecord &  blob_record)
{
    CPubseqGatewayCache *   cache = CPubseqGatewayApp::GetInstance()->
                                                        GetLookupCache();
    bool                    cache_hit = false;
    string                  blob_prop_cache_data;

    if (last_modified == INT64_MIN) {
        cache_hit = cache->LookupBlobPropBySatKey(
                                    sat, sat_key, last_modified,
                                    blob_prop_cache_data);
    } else {
        cache_hit = cache->LookupBlobPropBySatKeyLastModified(
                                    sat, sat_key, last_modified,
                                    blob_prop_cache_data);
    }

    if (cache_hit) {
        CPubseqGatewayApp::GetInstance()->GetCacheCounters().
                                                IncBlobPropCacheHit();
        ConvertBlobPropProtobufToBlobRecord(sat_key, last_modified,
                                            blob_prop_cache_data, blob_record);
    }

    CPubseqGatewayApp::GetInstance()->GetCacheCounters().
                                                IncBlobPropCacheMiss();
    return cache_hit;
}


bool CPendingOperation::x_SatToSatName(const SBlobRequest &  blob_request,
                                       SBlobId &  blob_id)
{
    CPubseqGatewayApp *      app = CPubseqGatewayApp::GetInstance();

    if (app->SatToSatName(blob_id.m_Sat, blob_id.m_SatName))
        return true;

    // Cannot resolve
    string      msg = string("Unknown satellite number ") +
                      NStr::NumericToString(blob_id.m_Sat);
    if (blob_request.GetBlobIdentificationType() == eBySeqId)
        msg += " for bioseq info with seq_id '" +
               blob_request.m_SeqId + "'";
    else
        msg += " for the blob " + GetBlobId(blob_id);

    // It is a server error - data inconsistency
    PSG_ERROR(msg);

    x_SendUnknownServerSatelliteError(GetItemId(), msg,
                                      eUnknownResolvedSatellite);

    app->GetErrorCounters().IncServerSatToSatName();
    return false;
}

void CPendingOperation::PrepareBioseqMessage(
                                size_t  item_id, const string &  msg,
                                CRequestStatus::ECode  status,
                                int  err_code, EDiagSev  severity)
{
    string  header = GetBioseqMessageHeader(item_id, msg.size(), status,
                                            err_code, severity);
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(msg.data()), msg.size()));
    ++m_TotalSentReplyChunks;
}


void CPendingOperation::PrepareBioseqData(size_t  item_id,
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


void CPendingOperation::PrepareBioseqCompletion(size_t  item_id,
                                                size_t  chunk_count)
{
    string      bioseq_meta = GetBioseqCompletionHeader(item_id, chunk_count);
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(bioseq_meta.data()),
                bioseq_meta.size()));
    ++m_TotalSentReplyChunks;
}


void CPendingOperation::PrepareBlobPropMessage(
                                size_t  item_id, const string &  msg,
                                CRequestStatus::ECode  status,
                                int  err_code, EDiagSev  severity)
{
    string      header = GetBlobPropMessageHeader(item_id, msg.size(),
                                                  status, err_code, severity);
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(msg.data()), msg.size()));
    ++m_TotalSentReplyChunks;
}

void CPendingOperation::PrepareBlobPropMessage(
                            SBlobFetchDetails *  fetch_details,
                            const string &  msg,
                            CRequestStatus::ECode  status, int  err_code,
                            EDiagSev  severity)
{
    PrepareBlobPropMessage(fetch_details->GetBlobPropItemId(this),
                           msg, status, err_code, severity);
    ++fetch_details->m_TotalSentBlobChunks;
}


void CPendingOperation::PrepareBlobPropData(
                            SBlobFetchDetails *  fetch_details,
                            const string &  content)
{
    string  header = GetBlobPropHeader(fetch_details->GetBlobPropItemId(this),
                                       fetch_details->m_BlobId,
                                       content.size());
    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(content.data()),
                    content.size()));
    ++fetch_details->m_TotalSentBlobChunks;
    ++m_TotalSentReplyChunks;
}


void CPendingOperation::PrepareBlobPropCompletion(size_t  item_id,
                                                  size_t  chunk_count)
{
    string      blob_prop_meta = GetBlobPropCompletionHeader(item_id,
                                                             chunk_count);
    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(blob_prop_meta.data()),
                    blob_prop_meta.size()));
    ++m_TotalSentReplyChunks;
}


void CPendingOperation::PrepareBlobPropCompletion(
                            SBlobFetchDetails *  fetch_details)
{
    // +1 is for the completion itself
    PrepareBlobPropCompletion(fetch_details->GetBlobPropItemId(this),
                              fetch_details->m_TotalSentBlobChunks + 1);

    // From now the counter will count chunks for the blob data
    fetch_details->m_TotalSentBlobChunks = 0;
    fetch_details->m_BlobPropSent = true;
}


void CPendingOperation::PrepareBlobMessage(
                                size_t  item_id, const SBlobId &  blob_id,
                                const string &  msg,
                                CRequestStatus::ECode  status, int  err_code,
                                EDiagSev  severity)
{
    string      header = GetBlobMessageHeader(item_id, blob_id, msg.size(),
                                              status, err_code, severity);
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(msg.data()), msg.size()));
    ++m_TotalSentReplyChunks;
}


void CPendingOperation::PrepareBlobMessage(
                            SBlobFetchDetails *  fetch_details,
                            const string &  msg,
                            CRequestStatus::ECode  status, int  err_code,
                            EDiagSev  severity)
{
    PrepareBlobMessage(fetch_details->GetBlobChunkItemId(this),
                       fetch_details->m_BlobId,
                       msg, status, err_code, severity);
    ++fetch_details->m_TotalSentBlobChunks;
}


void CPendingOperation::PrepareBlobData(
                            SBlobFetchDetails *  fetch_details,
                            const unsigned char *  chunk_data,
                            unsigned int  data_size, int  chunk_no)
{
    ++fetch_details->m_TotalSentBlobChunks;
    ++m_TotalSentReplyChunks;

    string  header = GetBlobChunkHeader(
                            fetch_details->GetBlobChunkItemId(this),
                            fetch_details->m_BlobId, data_size, chunk_no);
    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(header.data()),
                    header.size()));

    if (data_size > 0 && chunk_data != nullptr)
        m_Chunks.push_back(m_Reply->PrepareChunk(chunk_data, data_size));
}


void CPendingOperation::PrepareBlobCompletion(size_t  item_id,
                                              const SBlobId &  blob_id,
                                              size_t  chunk_count)
{
    string completion = GetBlobCompletionHeader(item_id, blob_id, chunk_count);
    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(completion.data()),
                    completion.size()));
    ++m_TotalSentReplyChunks;
}


void CPendingOperation::PrepareBlobCompletion(
                                    SBlobFetchDetails *  fetch_details)
{
    // +1 is for the completion itself
    PrepareBlobCompletion(fetch_details->GetBlobChunkItemId(this),
                          fetch_details->m_BlobId,
                          fetch_details->m_TotalSentBlobChunks + 1);
    ++fetch_details->m_TotalSentBlobChunks;
}


void CPendingOperation::PrepareReplyMessage(
                            const string &  msg,
                            CRequestStatus::ECode  status, int  err_code,
                            EDiagSev  severity)
{
    string      header = GetReplyMessageHeader(msg.size(),
                                               status, err_code, severity);
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(msg.data()), msg.size()));
    ++m_TotalSentReplyChunks;
}


void CPendingOperation::PrepareReplyCompletion(size_t  chunk_count)
{
    string  reply_completion = GetReplyCompletionHeader(chunk_count);
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(reply_completion.data()),
                reply_completion.size()));
    ++m_TotalSentReplyChunks;
}


void CPendingOperation::x_SendUnknownServerSatelliteError(
                                size_t  item_id,
                                const string &  msg,
                                int  err_code)
{
    PrepareBlobPropMessage(item_id, msg,
                           CRequestStatus::e500_InternalServerError,
                           err_code, eDiag_Error);
    PrepareBlobPropCompletion(item_id, 2);
}



// Blob operations callbacks: blob props, blob chunks, errors

void CBlobChunkCallback::operator()(const unsigned char *  chunk_data,
                                    unsigned int  data_size,
                                    int  chunk_no)
{
    CRequestContextResetter     context_resetter;
    m_PendingOp->x_SetRequestContext();

    assert(!m_FetchDetails->m_FinishedRead);
    if (m_PendingOp->m_Cancelled) {
        m_FetchDetails->m_Loader->Cancel();
        m_FetchDetails->m_FinishedRead = true;
        return;
    }
    if (m_Reply->IsFinished()) {
        CPubseqGatewayApp::GetInstance()->GetErrorCounters().
                                                     IncUnknownError();
        PSG_ERROR("Unexpected data received "
                  "while the output has finished, ignoring");
        return;
    }

    if (chunk_no >= 0) {
        // A blob chunk; 0-length chunks are allowed too
        m_PendingOp->PrepareBlobData(m_FetchDetails,
                                     chunk_data, data_size, chunk_no);
    } else {
        // End of the blob
        m_PendingOp->PrepareBlobCompletion(m_FetchDetails);
        m_FetchDetails->m_FinishedRead = true;

        m_PendingOp->x_SendReplyCompletion();
    }

    if (m_Reply->IsOutputReady())
        m_PendingOp->Peek(*m_Reply, false);
}


void CBlobPropCallback::operator()(CBlobRecord const &  blob, bool is_found)
{
    CRequestContextResetter     context_resetter;
    m_PendingOp->x_SetRequestContext();

    // Need to skip properties in two cases:
    // - no props asked
    // - not found and optional
    if (m_FetchDetails->m_NeedBlobProp == false)
        return;
    if (m_FetchDetails->m_Optional && is_found == false) {
        m_FetchDetails->m_FinishedRead = true;
        m_PendingOp->x_SendReplyCompletion();
        return;
    }

    if (is_found) {
        // Found, send back as JSON
        CJsonNode   json = ConvertBlobPropToJson(blob);
        m_PendingOp->PrepareBlobPropData(m_FetchDetails, json.Repr());

        if (m_FetchDetails->m_NeedChunks == false) {
            m_FetchDetails->m_FinishedRead = true;
            if (m_FetchDetails->m_IncludeDataFlags & fServNoTSE) {
                // Reply is finished, the only blob prop were needed
                m_PendingOp->PrepareBlobPropCompletion(m_FetchDetails);
                m_PendingOp->x_SendReplyCompletion();
            } else {
                // Not the end; need to decide about the other two blobs or the
                // original one
                if (blob.GetId2Info().empty()) {
                    // Request original blob chunks

                    // It is important to send completion before: the new
                    // request overwrites the main one
                    m_PendingOp->PrepareBlobPropCompletion(m_FetchDetails);
                    x_RequestOriginalBlobChunks(blob);
                } else {
                    // Request chunks of two other ID2 blobs.
                    x_RequestID2BlobChunks(blob);

                    // It is important to send completion after: there could be
                    // an error of converting/translating ID2 info
                    m_PendingOp->PrepareBlobPropCompletion(m_FetchDetails);
                }
            }
        } else {
            m_PendingOp->PrepareBlobPropCompletion(m_FetchDetails);
        }
    } else {
        // Not found, report 500 because it is data inconsistency
        // or 404 if it was requested via sat.sat_key
        CPubseqGatewayApp::GetInstance()->GetErrorCounters().
                                                IncBlobPropsNotFoundError();

        string      message = "Blob properties are not found";
        string      blob_prop_msg;
        if (m_FetchDetails->m_BlobIdType == eBySatAndSatKey) {
            // User requested wrong sat_key, so it is a client error
            PSG_WARNING(message);
            m_PendingOp->UpdateOverallStatus(CRequestStatus::e404_NotFound);
            m_PendingOp->PrepareBlobPropMessage(
                                    m_FetchDetails, message,
                                    CRequestStatus::e404_NotFound,
                                    eBlobPropsNotFound, eDiag_Error);
        } else {
            // Server error, data inconsistency
            PSG_ERROR(message);
            m_PendingOp->UpdateOverallStatus(
                                    CRequestStatus::e500_InternalServerError);
            m_PendingOp->PrepareBlobPropMessage(
                                    m_FetchDetails, message,
                                    CRequestStatus::e500_InternalServerError,
                                    eBlobPropsNotFound, eDiag_Error);
        }

        m_PendingOp->PrepareBlobPropCompletion(m_FetchDetails);

        m_FetchDetails->m_FinishedRead = true;
        m_PendingOp->x_SendReplyCompletion();
    }

    if (m_Reply->IsOutputReady())
        m_PendingOp->Peek(*m_Reply, false);
}


void CBlobPropCallback::x_RequestOriginalBlobChunks(CBlobRecord const &  blob)
{
    // Cannot reuse the fetch details, it leads to a core dump

    SBlobRequest    orig_blob_request(m_FetchDetails->m_BlobId,
                                      blob.GetModified());

    orig_blob_request.m_NeedBlobProp = false;
    orig_blob_request.m_NeedChunks = true;
    orig_blob_request.m_Optional = false;

    m_PendingOp->m_OriginalBlobChunkFetch.reset(
            new CPendingOperation::SBlobFetchDetails(orig_blob_request));

    unique_ptr<CBlobRecord>     blob_record(new CBlobRecord(blob));
    m_PendingOp->m_OriginalBlobChunkFetch->m_Loader.reset(
            new CCassBlobTaskLoadBlob(
                    m_PendingOp->m_Timeout,
                    m_PendingOp->m_MaxRetries,
                    m_PendingOp->m_Conn,
                    orig_blob_request.m_BlobId.m_SatName,
                    std::move(blob_record),
                    true, nullptr));

    m_PendingOp->m_OriginalBlobChunkFetch->m_Loader->SetDataReadyCB(
            HST::CHttpReply<CPendingOperation>::s_DataReady,
            m_Reply);
    m_PendingOp->m_OriginalBlobChunkFetch->m_Loader->SetErrorCB(
            CGetBlobErrorCallback(m_PendingOp, m_Reply,
                                  m_PendingOp->m_OriginalBlobChunkFetch.get()));
    m_PendingOp->m_OriginalBlobChunkFetch->m_Loader->SetPropsCallback(nullptr);
    m_PendingOp->m_OriginalBlobChunkFetch->m_Loader->SetChunkCallback(
            CBlobChunkCallback(m_PendingOp, m_Reply,
                               m_PendingOp->m_OriginalBlobChunkFetch.get()));
    m_PendingOp->m_OriginalBlobChunkFetch->m_Loader->Wait();
}


void CBlobPropCallback::x_RequestID2BlobChunks(CBlobRecord const &  blob)
{
    x_ParseId2Info(blob);
    if (!m_Id2InfoValid)
        return;

    CPubseqGatewayApp *     app = CPubseqGatewayApp::GetInstance();

    // Translate sat to keyspace
    SBlobId     shell_blob_id(m_Id2InfoSat, m_Id2InfoShell);  // optional
    SBlobId     info_blob_id(m_Id2InfoSat, m_Id2InfoInfo);    // mandatory

    if (!app->SatToSatName(m_Id2InfoSat, shell_blob_id.m_SatName)) {
        // Error: send it in the context of the blob props
        string      message = "Error mapping id2 info sat (" +
                              NStr::NumericToString(m_Id2InfoSat) +
                              ") to a cassandra keyspace for the blob " +
                              GetBlobId(m_FetchDetails->m_BlobId);
        m_PendingOp->PrepareBlobPropMessage(
                                m_FetchDetails, message,
                                CRequestStatus::e500_InternalServerError,
                                eBadID2Info, eDiag_Error);
        app->GetErrorCounters().IncServerSatToSatName();
        m_PendingOp->UpdateOverallStatus(
                                CRequestStatus::e500_InternalServerError);
        PSG_ERROR(message);
        return;
    }

    // The keyspace match for both blobs
    info_blob_id.m_SatName = shell_blob_id.m_SatName;

    // Create the Id2Shell and Id2Info requests
    SBlobRequest    shell_blob_request(shell_blob_id, INT64_MIN);
    SBlobRequest    info_blob_request(info_blob_id, INT64_MIN);

    shell_blob_request.m_NeedBlobProp = false;
    shell_blob_request.m_NeedChunks = true;
    shell_blob_request.m_Optional = true;

    info_blob_request.m_NeedBlobProp = false;
    info_blob_request.m_NeedChunks = true;
    info_blob_request.m_Optional = false;

    // Prepare Id2Shell retrieval
    if (m_Id2InfoShell > 0) {
        m_PendingOp->m_Id2ShellFetchDetails.reset(
                new CPendingOperation::SBlobFetchDetails(shell_blob_request));

        unique_ptr<CBlobRecord>     blob_record(new CBlobRecord);
        bool            cache_hit = m_PendingOp->x_LookupBlobPropCache(
                                        shell_blob_request.m_BlobId.m_Sat,
                                        shell_blob_request.m_BlobId.m_SatKey,
                                        shell_blob_request.m_LastModified,
                                        *blob_record.get());

        if (cache_hit) {
            m_PendingOp->m_Id2ShellFetchDetails->m_Loader.reset(
                    new CCassBlobTaskLoadBlob(
                        m_PendingOp->m_Timeout, m_PendingOp->m_MaxRetries,
                        m_PendingOp->m_Conn,
                        shell_blob_request.m_BlobId.m_SatName,
                        std::move(blob_record),
                        shell_blob_request.m_NeedChunks,
                        nullptr));
        } else {
            m_PendingOp->m_Id2ShellFetchDetails->m_Loader.reset(
                    new CCassBlobTaskLoadBlob(
                        m_PendingOp->m_Timeout, m_PendingOp->m_MaxRetries,
                        m_PendingOp->m_Conn,
                        shell_blob_request.m_BlobId.m_SatName,
                        shell_blob_request.m_BlobId.m_SatKey,
                        shell_blob_request.m_NeedChunks,
                        nullptr));
        }
        m_PendingOp->m_Id2ShellFetchDetails->m_Loader->SetDataReadyCB(
                HST::CHttpReply<CPendingOperation>::s_DataReady, m_Reply);
        m_PendingOp->m_Id2ShellFetchDetails->m_Loader->SetErrorCB(
                CGetBlobErrorCallback(m_PendingOp, m_Reply,
                                      m_PendingOp->m_Id2ShellFetchDetails.get()));
        m_PendingOp->m_Id2ShellFetchDetails->m_Loader->SetPropsCallback(nullptr);
        m_PendingOp->m_Id2ShellFetchDetails->m_Loader->SetChunkCallback(
                CBlobChunkCallback(m_PendingOp, m_Reply,
                                   m_PendingOp->m_Id2ShellFetchDetails.get()));
    }

    // Prepare Id2Info retrieval
    m_PendingOp->m_Id2InfoFetchDetails.reset(
            new CPendingOperation::SBlobFetchDetails(info_blob_request));

    unique_ptr<CBlobRecord>     blob_record(new CBlobRecord);
    bool            cache_hit = m_PendingOp->x_LookupBlobPropCache(
                                    info_blob_request.m_BlobId.m_Sat,
                                    info_blob_request.m_BlobId.m_SatKey,
                                    info_blob_request.m_LastModified,
                                    *blob_record.get());
    if (cache_hit) {
        m_PendingOp->m_Id2InfoFetchDetails->m_Loader.reset(
                new CCassBlobTaskLoadBlob(
                    m_PendingOp->m_Timeout, m_PendingOp->m_MaxRetries,
                    m_PendingOp->m_Conn,
                    info_blob_request.m_BlobId.m_SatName,
                    std::move(blob_record),
                    info_blob_request.m_NeedChunks,
                    nullptr));
    } else {
        m_PendingOp->m_Id2InfoFetchDetails->m_Loader.reset(
                new CCassBlobTaskLoadBlob(
                    m_PendingOp->m_Timeout, m_PendingOp->m_MaxRetries,
                    m_PendingOp->m_Conn,
                    info_blob_request.m_BlobId.m_SatName,
                    info_blob_request.m_BlobId.m_SatKey,
                    info_blob_request.m_NeedChunks,
                    nullptr));
    }
    m_PendingOp->m_Id2InfoFetchDetails->m_Loader->SetDataReadyCB(
            HST::CHttpReply<CPendingOperation>::s_DataReady, m_Reply);
    m_PendingOp->m_Id2InfoFetchDetails->m_Loader->SetErrorCB(
            CGetBlobErrorCallback(m_PendingOp, m_Reply,
                                  m_PendingOp->m_Id2InfoFetchDetails.get()));
    m_PendingOp->m_Id2InfoFetchDetails->m_Loader->SetPropsCallback(nullptr);
    m_PendingOp->m_Id2InfoFetchDetails->m_Loader->SetChunkCallback(
            CBlobChunkCallback(m_PendingOp, m_Reply,
                               m_PendingOp->m_Id2InfoFetchDetails.get()));

    // We may need to request ID2 chunks
    if (m_FetchDetails->m_IncludeDataFlags & fServWholeTSE) {
        // Sat name is the same
        x_RequestId2SplitBlobs(shell_blob_id.m_SatName);
    }

    // initiate retrieval
    if (m_PendingOp->m_Id2ShellFetchDetails)
        m_PendingOp->m_Id2ShellFetchDetails->m_Loader->Wait();
    m_PendingOp->m_Id2InfoFetchDetails->m_Loader->Wait();
    for (auto &  fetch_details: m_PendingOp->m_Id2ChunkFetchDetails) {
        if (fetch_details)
            fetch_details->m_Loader->Wait();
    }
}


void CBlobPropCallback::x_RequestId2SplitBlobs(const string &  sat_name)
{
    for (int  chunk_no = 1; chunk_no <= m_Id2InfoChunks; ++chunk_no) {
        SBlobId     chunks_blob_id(m_Id2InfoSat, m_Id2InfoInfo + chunk_no);
        chunks_blob_id.m_SatName = sat_name;

        SBlobRequest    chunk_request(chunks_blob_id, INT64_MIN);
        chunk_request.m_NeedBlobProp = false;
        chunk_request.m_NeedChunks = true;
        chunk_request.m_Optional = false;

        unique_ptr<CPendingOperation::SBlobFetchDetails>   details;
        details.reset(new CPendingOperation::SBlobFetchDetails(chunk_request));

        unique_ptr<CBlobRecord>     blob_record(new CBlobRecord);
        bool            cache_hit = m_PendingOp->x_LookupBlobPropCache(
                                        chunk_request.m_BlobId.m_Sat,
                                        chunk_request.m_BlobId.m_SatKey,
                                        chunk_request.m_LastModified,
                                        *blob_record.get());

        if (cache_hit) {
            details->m_Loader.reset(
                new CCassBlobTaskLoadBlob(
                    m_PendingOp->m_Timeout, m_PendingOp->m_MaxRetries,
                    m_PendingOp->m_Conn,
                    chunk_request.m_BlobId.m_SatName,
                    std::move(blob_record),
                    chunk_request.m_NeedChunks,
                    nullptr));
        } else {
            details->m_Loader.reset(
                new CCassBlobTaskLoadBlob(
                    m_PendingOp->m_Timeout, m_PendingOp->m_MaxRetries,
                    m_PendingOp->m_Conn,
                    chunk_request.m_BlobId.m_SatName,
                    chunk_request.m_BlobId.m_SatKey,
                    chunk_request.m_NeedChunks,
                    nullptr));
        }
        details->m_Loader->SetDataReadyCB(
            HST::CHttpReply<CPendingOperation>::s_DataReady, m_Reply);
        details->m_Loader->SetErrorCB(
            CGetBlobErrorCallback(m_PendingOp, m_Reply, details.get()));
        details->m_Loader->SetPropsCallback(nullptr);
        details->m_Loader->SetChunkCallback(
            CBlobChunkCallback(m_PendingOp, m_Reply, details.get()));

        m_PendingOp->m_Id2ChunkFetchDetails.push_back(std::move(details));
    }
}


void CBlobPropCallback::x_ParseId2Info(CBlobRecord const &  blob)
{
    // id2_info: "sat.shell.info[.chunks]"

    if (m_Id2InfoParsed)
        return;

    m_Id2InfoParsed = true;

    CPubseqGatewayApp *     app = CPubseqGatewayApp::GetInstance();
    string                  id2_info = blob.GetId2Info();
    vector<string>          parts;
    NStr::Split(id2_info, ".", parts);

    if (parts.size() != 4) {
        // Error: send it in the context of the blob props
        string      message = "Error extracting id2 info for the blob " +
                              GetBlobId(m_FetchDetails->m_BlobId) +
                              ". Expected 'sat.shell.info.nchunks', found " +
                              NStr::NumericToString(parts.size()) + " parts.";
        m_PendingOp->PrepareBlobPropMessage(
                            m_FetchDetails, message,
                            CRequestStatus::e500_InternalServerError,
                            eBadID2Info, eDiag_Error);
        app->GetErrorCounters().IncBioseqID2Info();
        m_PendingOp->UpdateOverallStatus(
                                CRequestStatus::e500_InternalServerError);
        PSG_ERROR(message);
        return;
    }

    try {
        m_Id2InfoSat = NStr::StringToInt(parts[0]);
        m_Id2InfoShell = NStr::StringToInt(parts[1]);
        m_Id2InfoInfo = NStr::StringToInt(parts[2]);
        m_Id2InfoChunks = NStr::StringToInt(parts[3]);
    } catch (...) {
        // Error: send it in the context of the blob props
        string      message = "Error converting id2 info into integers for "
                              "the blob " + GetBlobId(m_FetchDetails->m_BlobId);
        m_PendingOp->PrepareBlobPropMessage(
                            m_FetchDetails, message,
                            CRequestStatus::e500_InternalServerError,
                            eBadID2Info, eDiag_Error);
        app->GetErrorCounters().IncBioseqID2Info();
        m_PendingOp->UpdateOverallStatus(
                                CRequestStatus::e500_InternalServerError);
        PSG_ERROR(message);
        return;
    }

    // Validate the values
    string      validate_message;
    if (m_Id2InfoSat <= 0)
        validate_message = "Invalid id2_info SAT value. Expected to be > 0. Received: " +
            NStr::NumericToString(m_Id2InfoSat) + ".";
    if (m_Id2InfoShell < 0) {
        if (!validate_message.empty())
            validate_message += " ";
        validate_message += "Invalid id2_info SHELL value. Expected to be >= 0. Received: " +
            NStr::NumericToString(m_Id2InfoShell) + ".";
    }
    if (m_Id2InfoInfo <= 0) {
        if (!validate_message.empty())
            validate_message += " ";
        validate_message += "Invalid id2_info INFO value. Expected to be > 0. Received: " +
            NStr::NumericToString(m_Id2InfoInfo) + ".";
    }
    if (m_Id2InfoChunks <= 0) {
        if (!validate_message.empty())
            validate_message += " ";
        validate_message += "Invalid id2_info NCHUNKS value. Expected to be > 0. Received: " +
            NStr::NumericToString(m_Id2InfoChunks) + ".";
    }


    if (!validate_message.empty()) {
        // Error: send it in the context of the blob props
        validate_message += " Blob: " + GetBlobId(m_FetchDetails->m_BlobId);
        m_PendingOp->PrepareBlobPropMessage(
                            m_FetchDetails, validate_message,
                            CRequestStatus::e500_InternalServerError,
                            eBadID2Info, eDiag_Error);
        app->GetErrorCounters().IncBioseqID2Info();
        m_PendingOp->UpdateOverallStatus(
                                CRequestStatus::e500_InternalServerError);
        PSG_ERROR(validate_message);
        return;
    }

    m_Id2InfoValid = true;
}


void CGetBlobErrorCallback::operator()(CRequestStatus::ECode  status,
                                       int  code,
                                       EDiagSev  severity,
                                       const string &  message)
{
    CRequestContextResetter     context_resetter;
    m_PendingOp->x_SetRequestContext();

    // To avoid sending an error in Peek()
    m_FetchDetails->m_Loader->ClearError();

    // It could be a message or an error
    bool    is_error = (severity == eDiag_Error ||
                        severity == eDiag_Critical ||
                        severity == eDiag_Fatal);

    // If it was an optional blob then the retrieving errors are to be ignored
    if (m_FetchDetails->m_Optional) {
        if (is_error)
            m_FetchDetails->m_FinishedRead = true;
        return;
    }

    CPubseqGatewayApp *      app = CPubseqGatewayApp::GetInstance();
    PSG_ERROR(message);
    if (status == CRequestStatus::e404_NotFound) {
        app->GetErrorCounters().IncGetBlobNotFound();
    } else {
        if (is_error)
            app->GetErrorCounters().IncUnknownError();
    }

    if (is_error) {
        m_PendingOp->UpdateOverallStatus(
                                    CRequestStatus::e500_InternalServerError);

        if (m_FetchDetails->IsBlobPropStage()) {
            m_PendingOp->PrepareBlobPropMessage(
                                m_FetchDetails, message,
                                CRequestStatus::e500_InternalServerError,
                                code, severity);
            m_PendingOp->PrepareBlobPropCompletion(m_FetchDetails);
        } else {
            m_PendingOp->PrepareBlobMessage(
                                m_FetchDetails, message,
                                CRequestStatus::e500_InternalServerError,
                                code, severity);
            m_PendingOp->PrepareBlobCompletion(m_FetchDetails);
        }

        // If it is an error then regardless what stage it was, props or
        // chunks, there will be no more activity
        m_FetchDetails->m_FinishedRead = true;
    } else {
        if (m_FetchDetails->IsBlobPropStage())
            m_PendingOp->PrepareBlobPropMessage(m_FetchDetails, message,
                                                status, code, severity);
        else
            m_PendingOp->PrepareBlobMessage(m_FetchDetails, message,
                                            status, code, severity);
    }

    m_PendingOp->x_SendReplyCompletion();

    if (m_Reply->IsOutputReady())
        m_PendingOp->Peek(*m_Reply, false);
}
