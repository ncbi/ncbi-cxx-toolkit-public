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
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info.hpp>
#include <objtools/pubseq_gateway/protobuf/psg_protobuf.pb.h>
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
    if (m_IsResolveRequest)
        x_ProcessResolveRequest();
    else
        x_ProcessGetRequest();
}


void CPendingOperation::x_ProcessGetRequest(void)
{
    // Get request could be one of the following:
    // - the blob was requested by a sat/sat_key pair
    // - the blob was requested by a seq_id/seq_id_type pair
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
            m_Reply->Send(m_Chunks, true);
            m_Chunks.clear();
            return;
        }

        // retrieve BLOB_PROP & CHUNKS (if needed)
        m_BlobRequest.m_BlobId = blob_id;
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
                            false, nullptr));
    } else if (m_BlobRequest.m_LastModified == INT64_MIN) {
        m_MainBlobFetchDetails->m_Loader.reset(
            new CCassBlobTaskLoadBlob(
                            m_Timeout, m_MaxRetries, m_Conn,
                            m_BlobRequest.m_BlobId.m_SatName,
                            m_BlobRequest.m_BlobId.m_SatKey,
                            false, nullptr));
    } else {
        m_MainBlobFetchDetails->m_Loader.reset(
            new CCassBlobTaskLoadBlob(
                            m_Timeout, m_MaxRetries, m_Conn,
                            m_BlobRequest.m_BlobId.m_SatName,
                            m_BlobRequest.m_BlobId.m_SatKey,
                            m_BlobRequest.m_LastModified,
                            false, nullptr));
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


bool CPendingOperation::x_ParseUrlSeqIdAsIs(CSeq_id &  seq_id)
{
    try {
        seq_id.Set(m_UrlSeqId);
        return true;
    } catch (...) {
    }
    return false;
}


bool CPendingOperation::x_ParseUrlSeqIdAsFastaTypeAndContent(CSeq_id &  seq_id)
{
    if (m_UrlSeqIdType >= 0) {
        try {
            seq_id.Set(CSeq_id::eFasta_AsTypeAndContent,
                       (CSeq_id_Base::E_Choice)(m_UrlSeqIdType),
                       m_UrlSeqId);
            return true;
        } catch (...) {
        }
    }
    return false;
}


SBioseqKey CPendingOperation::x_ResolveInputSeqId(string &  err_msg)
{
    m_UrlSeqId = m_BlobRequest.m_SeqId;
    m_UrlSeqIdType = m_BlobRequest.m_SeqIdType;
    if (m_IsResolveRequest) {
        m_UrlSeqId = m_ResolveRequest.m_SeqId;
        m_UrlSeqIdType = m_ResolveRequest.m_SeqIdType;
    }

    SBioseqKey      bioseq_key;
    CSeq_id         parsed_seq_id;
    bool            parsed_as_is_ok = false;
    bool            parsed_as_fasta_ok = false;

    parsed_as_is_ok = x_ParseUrlSeqIdAsIs(parsed_seq_id);
    if (!parsed_as_is_ok)
        parsed_as_fasta_ok = x_ParseUrlSeqIdAsFastaTypeAndContent(parsed_seq_id);

    bool        resolve_attepmt = false;
    if (parsed_as_is_ok || parsed_as_fasta_ok) {
        const CTextseq_id *     text_seq_id = parsed_seq_id.GetTextseq_Id();

        // CXX-10335
        x_ResolveViaComposeOSLT(parsed_seq_id, text_seq_id, bioseq_key);
        if (bioseq_key.IsValid())
            return bioseq_key;

        int         eff_seq_id_type;
        bool        eff_seq_id_ok = x_GetEffectiveSeqIdType(parsed_seq_id,
                                                            eff_seq_id_type);
        if (text_seq_id != NULL ||
            (eff_seq_id_ok && eff_seq_id_type == CSeq_id_Base::e_General)) {
            try {
                x_ResolveInputSeqId(parsed_seq_id, parsed_as_is_ok,
                                    text_seq_id, bioseq_key);
            } catch (const exception &  ex) {
                err_msg = ex.what();
            } catch (...) {
                err_msg = "Unknown error while resolving input seq_id";
            }

            resolve_attepmt = true;
        }
    }

    if (!resolve_attepmt) {
        // false: not tried as fasta content
        x_ResolveInputSeqIdAsIs(parsed_seq_id, parsed_as_is_ok,
                                false, bioseq_key);
    }

    if (bioseq_key.IsValid())
        CPubseqGatewayApp::GetInstance()->GetRequestCounters().
                                                IncResolvedAsFallback();
    else
        CPubseqGatewayApp::GetInstance()->GetRequestCounters().
                                                IncNotResolved();
    return bioseq_key;
}


void CPendingOperation::x_ResolveViaComposeOSLT(CSeq_id &  parsed_seq_id,
                                                const CTextseq_id *  text_seq_id,
                                                SBioseqKey &  bioseq_key)
{
    try {
        list<string>    secondary_id_list;
        string          primary_id = parsed_seq_id.ComposeOSLT(&secondary_id_list);
        int             effective_seq_id_type;

        if (!x_GetEffectiveSeqIdType(parsed_seq_id, effective_seq_id_type))
            return;     // inconsistency

        if (!primary_id.empty()) {
            // Try BIOSEQ_INFO
            bioseq_key.m_Accession = std::move(primary_id);
            bioseq_key.m_Version = x_GetEffectiveVersion(text_seq_id);
            bioseq_key.m_SeqIdType = effective_seq_id_type;
            if (x_LookupCachedBioseqInfo(bioseq_key.m_Accession,
                                         bioseq_key.m_Version,
                                         bioseq_key.m_SeqIdType,
                                         bioseq_key.m_BioseqInfo)) {
                CPubseqGatewayApp::GetInstance()->GetRequestCounters().
                                                IncResolvedAsPrimaryOSLT();
                return;
            }
        }

        // Try SI2CSI
        for (const auto &  secondary_id : secondary_id_list) {
            string      csi_cache_data;

            bioseq_key.Reset();
            bioseq_key.m_Accession = std::move(secondary_id);
            bioseq_key.m_SeqIdType = effective_seq_id_type;
            if (x_LookupCachedCsi(bioseq_key.m_Accession,
                                  bioseq_key.m_SeqIdType, csi_cache_data)) {
                ConvertSi2csiToBioseqKey(csi_cache_data, bioseq_key);
                CPubseqGatewayApp::GetInstance()->GetRequestCounters().
                                                IncResolvedAsSecondaryOSLT();
                return;
            }
        }
    } catch (...) {
    }

    bioseq_key.Reset();
}


void CPendingOperation::x_ResolveInputSeqId(CSeq_id &  parsed_seq_id,
                                            bool  seq_id_parsed_as_is,
                                            const CTextseq_id *  text_seq_id,
                                            SBioseqKey &  bioseq_key)
{
    // There are two paths:
    // - Path 1: using accession and [version] and [seq_id_type]
    // - Path 2: using name
    // Then the paths need to be compared
    SBioseqKey  bioseq_info_path1 = x_ResolveInputSeqIdPath1(parsed_seq_id,
                                                             seq_id_parsed_as_is,
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
CPendingOperation::x_ResolveInputSeqIdPath1(CSeq_id &  parsed_seq_id,
                                            bool  seq_id_parsed_as_is,
                                            const CTextseq_id *  text_seq_id)
{
    SBioseqKey          bioseq_key;
    int                 eff_seq_id_type;
    bool                tried_as_fasta = false;

    if (x_GetEffectiveSeqIdType(parsed_seq_id, eff_seq_id_type)) {
        if (eff_seq_id_type == CSeq_id_Base::e_General) {
            // Try it as fasta content
            string      csi_cache_data;
            bool        cache_hit = false;

            try {
                tried_as_fasta = true;

                string      seq_id_content;
                parsed_seq_id.GetLabel(&seq_id_content, CSeq_id::eFastaContent,
                                       CSeq_id::fLabel_Trimmed);

                cache_hit = x_LookupCachedCsi(seq_id_content, eff_seq_id_type,
                                              csi_cache_data);
            } catch (...) {
            }

            // Try it as Tag
            if (!cache_hit) {
                const CObject_id &  obj_id = parsed_seq_id.GetGeneral().GetTag();
                if (obj_id.Which() == CObject_id::e_Str)
                    cache_hit = x_LookupCachedCsi(obj_id.GetStr(),
                                                  eff_seq_id_type, csi_cache_data);
                else
                    cache_hit = x_LookupCachedCsi(NStr::NumericToString(obj_id.GetId()),
                                                  eff_seq_id_type, csi_cache_data);
            }

            if (cache_hit) {
                ConvertSi2csiToBioseqKey(csi_cache_data, bioseq_key);
                return bioseq_key;
            }
        }

        if (text_seq_id && text_seq_id->CanGetAccession()) {
            bioseq_key.m_SeqIdType = eff_seq_id_type;
            bioseq_key.m_Accession = text_seq_id->GetAccession();
            bioseq_key.m_Version = x_GetEffectiveVersion(text_seq_id);

            if (x_LookupCachedBioseqInfo(bioseq_key.m_Accession,
                                         bioseq_key.m_Version,
                                         bioseq_key.m_SeqIdType,
                                         bioseq_key.m_BioseqInfo))
                return bioseq_key;

            // Another try with what has come from url
            bioseq_key.Reset();
        }
    }

    x_ResolveInputSeqIdAsIs(parsed_seq_id, seq_id_parsed_as_is,
                            tried_as_fasta, bioseq_key);
    return bioseq_key;
}


bool
CPendingOperation::x_LookupCachedBioseqInfo(const string &  accession,
                                            int &  version,
                                            int &  seq_id_type,
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
                                        accession, version,
                                        bioseq_info_cache_data,
                                        seq_id_type);
    } else {
        if (seq_id_type >= 0)
            cache_hit = cache->LookupBioseqInfoByAccessionVersionSeqIdType(
                                        accession, -1, seq_id_type,
                                        bioseq_info_cache_data,
                                        version, seq_id_type);
        else
            cache_hit = cache->LookupBioseqInfoByAccession(
                                        accession,
                                        bioseq_info_cache_data,
                                        version, seq_id_type);
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
                                     string &  csi_cache_data)
{
    bool                    cache_hit = false;
    CPubseqGatewayCache *   cache = CPubseqGatewayApp::GetInstance()->
                                                            GetLookupCache();
    if (seq_id_type < 0)
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


void CPendingOperation::x_ResolveInputSeqIdAsIs(CSeq_id &  parsed_seq_id,
                                                bool  seq_id_parsed_as_is,
                                                bool  tried_as_fasta_content,
                                                SBioseqKey &  bioseq_key)
{
    string      csi_cache_data;
    bool        cache_hit = false;

    bool        fasta_tried = seq_id_parsed_as_is && tried_as_fasta_content;
    if (!fasta_tried) {
        // Try as fasta content
        try {
            if (!seq_id_parsed_as_is)
                parsed_seq_id.Set(m_UrlSeqId);

            int         eff_seq_id_type;
            if (x_GetEffectiveSeqIdType(parsed_seq_id, eff_seq_id_type)) {
                string      seq_id_content;
                parsed_seq_id.GetLabel(&seq_id_content, CSeq_id::eFastaContent,
                                       CSeq_id::fLabel_Trimmed);

                cache_hit = x_LookupCachedCsi(seq_id_content, eff_seq_id_type,
                                              csi_cache_data);
            }
        } catch (...) {
        }
    }

    // Try as it came from the URL
    if (!cache_hit) {
        // Strip the trailing '|' from the url_seq_id if there are any
        while (m_UrlSeqId[m_UrlSeqId.size() - 1] == '|')
            m_UrlSeqId.erase(m_UrlSeqId.size() - 1);

        cache_hit = x_LookupCachedCsi(m_UrlSeqId, m_UrlSeqIdType,
                                      csi_cache_data);
    }

    if (!cache_hit) {
        // Try bioseq_info cache as it came from URL
        bioseq_key.m_Accession = m_UrlSeqId;
        bioseq_key.m_Version = -1;
        bioseq_key.m_SeqIdType = m_UrlSeqIdType;

        if (x_LookupCachedBioseqInfo(bioseq_key.m_Accession,
                                     bioseq_key.m_Version,
                                     bioseq_key.m_SeqIdType,
                                     bioseq_key.m_BioseqInfo))
            return; // found

        // Not found, so bioseq_key must be cleared
        bioseq_key.Reset();
    }

    if (cache_hit)
        ConvertSi2csiToBioseqKey(csi_cache_data, bioseq_key);
}


SBioseqKey
CPendingOperation::x_ResolveInputSeqIdPath2(const CSeq_id &  parsed_seq_id,
                                            const CTextseq_id *  text_seq_id)
{
    SBioseqKey      bioseq_key;

    if (text_seq_id) {
        if (text_seq_id->CanGetName()) {
            string      name = text_seq_id->GetName();
            int         eff_seq_id_type;

            if (x_GetEffectiveSeqIdType(parsed_seq_id, eff_seq_id_type)) {
                string      csi_cache_data;
                if (x_LookupCachedCsi(name, eff_seq_id_type, csi_cache_data))
                    ConvertSi2csiToBioseqKey(csi_cache_data, bioseq_key);
            }
        }
    }

    return bioseq_key;
}


bool CPendingOperation::x_GetEffectiveSeqIdType(const CSeq_id &  parsed_seq_id,
                                                int &  eff_seq_id_type)
{
    CSeq_id_Base::E_Choice  parsed_seq_id_type = parsed_seq_id.Which();
    bool                    parsed_seq_id_type_found = (parsed_seq_id_type !=
                                                        CSeq_id_Base::e_not_set);

    if (!parsed_seq_id_type_found && m_UrlSeqIdType < 0) {
        eff_seq_id_type = -1;
        return true;
    }

    if (!parsed_seq_id_type_found) {
        eff_seq_id_type = m_UrlSeqIdType;
        return true;
    }

    if (m_UrlSeqIdType < 0) {
        eff_seq_id_type = parsed_seq_id_type;
        return true;
    }

    // Both found
    if (parsed_seq_id_type == m_UrlSeqIdType) {
        eff_seq_id_type = m_UrlSeqIdType;
        return true;
    }

    // The parsed and url explicit seq_id_type do not match
    return false;
}


int CPendingOperation::x_GetEffectiveVersion(const CTextseq_id *  text_seq_id)
{
    try {
        if (text_seq_id == nullptr)
            return -1;
        if (text_seq_id->CanGetVersion())
            return text_seq_id->GetVersion();
    } catch (...) {
    }
    return -1;
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
                                     fServAllBioseqFields,
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

    if (is_found) {
        // Found, send back as JSON
        CJsonNode   json = ConvertBlobPropToJson(blob);
        m_PendingOp->PrepareBlobPropData(m_FetchDetails, json.Repr());

        // Note: initially only blob_props are requested and at that moment the
        //       TSE option is 'known'. So the initial request should be
        //       finished, see m_FinishedRead = true
        //       In the further requests of the blob data regardless with blob
        //       props or not, the TSE option is set to unknown so the request
        //       will be finished at the moment when blob chunks are handled.
        switch (m_FetchDetails->m_TSEOption) {
            case eNoneTSE:
                m_FetchDetails->m_FinishedRead = true;
                // Nothing else to be sent;
                m_PendingOp->PrepareBlobPropCompletion(m_FetchDetails);
                m_PendingOp->x_SendReplyCompletion();
                break;
            case eSlimTSE:
                m_FetchDetails->m_FinishedRead = true;
                if (blob.GetId2Info().empty()) {
                    // Nothing else to be sent
                    m_PendingOp->PrepareBlobPropCompletion(m_FetchDetails);
                    m_PendingOp->x_SendReplyCompletion();
                } else {
                    // Request the split INFO blob only
                    x_RequestID2BlobChunks(blob, true);

                    // It is important to send completion after: there could be
                    // an error of converting/translating ID2 info
                    m_PendingOp->PrepareBlobPropCompletion(m_FetchDetails);
                }
                break;
            case eSmartTSE:
                m_FetchDetails->m_FinishedRead = true;
                if (blob.GetId2Info().empty()) {
                    // Request original blob chunks
                    m_PendingOp->PrepareBlobPropCompletion(m_FetchDetails);
                    x_RequestOriginalBlobChunks(blob);
                } else {
                    // Request the split INFO blob only
                    x_RequestID2BlobChunks(blob, true);

                    // It is important to send completion after: there could be
                    // an error of converting/translating ID2 info
                    m_PendingOp->PrepareBlobPropCompletion(m_FetchDetails);
                }
                break;
            case eWholeTSE:
                m_FetchDetails->m_FinishedRead = true;
                if (blob.GetId2Info().empty()) {
                    // Request original blob chunks
                    m_PendingOp->PrepareBlobPropCompletion(m_FetchDetails);
                    x_RequestOriginalBlobChunks(blob);
                } else {
                    // Request the split INFO blob and all split chunks
                    x_RequestID2BlobChunks(blob, false);

                    // It is important to send completion after: there could be
                    // an error of converting/translating ID2 info
                    m_PendingOp->PrepareBlobPropCompletion(m_FetchDetails);
                }
                break;
            case eOrigTSE:
                m_FetchDetails->m_FinishedRead = true;
                // Request original blob chunks
                m_PendingOp->PrepareBlobPropCompletion(m_FetchDetails);
                x_RequestOriginalBlobChunks(blob);
                break;
            case eUnknownTSE:
                // Used when INFO blobs are asked; i.e. chunks have been
                // requeted as well, so only the prop completion message needs
                // to be sent.
                m_PendingOp->PrepareBlobPropCompletion(m_FetchDetails);
                break;
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

    // eUnknownTSE is safe here; no blob prop call will happen anyway
    SBlobRequest    orig_blob_request(m_FetchDetails->m_BlobId,
                                      blob.GetModified(), eUnknownTSE);

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


void CBlobPropCallback::x_RequestID2BlobChunks(CBlobRecord const &  blob,
                                               bool  info_blob_only)
{
    x_ParseId2Info(blob);
    if (!m_Id2InfoValid)
        return;

    CPubseqGatewayApp *     app = CPubseqGatewayApp::GetInstance();

    // Translate sat to keyspace
    SBlobId     info_blob_id(m_Id2InfoSat, m_Id2InfoInfo);    // mandatory

    if (!app->SatToSatName(m_Id2InfoSat, info_blob_id.m_SatName)) {
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

    // Create the Id2Info requests.
    // eUnknownTSE is treated in the blob prop handler as to do nothing (no
    // sending completion message, no requesting other blobs)
    SBlobRequest    info_blob_request(info_blob_id, INT64_MIN, eUnknownTSE);

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
                    true, nullptr));
    } else {
        m_PendingOp->m_Id2InfoFetchDetails->m_Loader.reset(
                new CCassBlobTaskLoadBlob(
                    m_PendingOp->m_Timeout, m_PendingOp->m_MaxRetries,
                    m_PendingOp->m_Conn,
                    info_blob_request.m_BlobId.m_SatName,
                    info_blob_request.m_BlobId.m_SatKey,
                    true, nullptr));
    }
    m_PendingOp->m_Id2InfoFetchDetails->m_Loader->SetDataReadyCB(
            HST::CHttpReply<CPendingOperation>::s_DataReady, m_Reply);
    m_PendingOp->m_Id2InfoFetchDetails->m_Loader->SetErrorCB(
            CGetBlobErrorCallback(m_PendingOp, m_Reply,
                                  m_PendingOp->m_Id2InfoFetchDetails.get()));
    m_PendingOp->m_Id2InfoFetchDetails->m_Loader->SetPropsCallback(
            CBlobPropCallback(m_PendingOp, m_Reply,
                              m_PendingOp->m_Id2InfoFetchDetails.get()));
    m_PendingOp->m_Id2InfoFetchDetails->m_Loader->SetChunkCallback(
            CBlobChunkCallback(m_PendingOp, m_Reply,
                               m_PendingOp->m_Id2InfoFetchDetails.get()));

    // We may need to request ID2 chunks
    if (!info_blob_only) {
        // Sat name is the same
        x_RequestId2SplitBlobs(info_blob_id.m_SatName);
    }

    // initiate retrieval
    m_PendingOp->m_Id2InfoFetchDetails->m_Loader->Wait();
    for (auto &  fetch_details: m_PendingOp->m_Id2ChunkFetchDetails) {
        if (fetch_details)
            fetch_details->m_Loader->Wait();
    }
}


void CBlobPropCallback::x_RequestId2SplitBlobs(const string &  sat_name)
{
    for (int  chunk_no = 1; chunk_no <= m_Id2InfoChunks; ++chunk_no) {
        SBlobId     chunks_blob_id(m_Id2InfoSat,
                                   m_Id2InfoInfo - m_Id2InfoChunks - 1 + chunk_no);
        chunks_blob_id.m_SatName = sat_name;

        // eUnknownTSE is treated in the blob prop handler as to do nothing (no
        // sending completion message, no requesting other blobs)
        SBlobRequest    chunk_request(chunks_blob_id, INT64_MIN, eUnknownTSE);

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
                    true, nullptr));
        } else {
            details->m_Loader.reset(
                new CCassBlobTaskLoadBlob(
                    m_PendingOp->m_Timeout, m_PendingOp->m_MaxRetries,
                    m_PendingOp->m_Conn,
                    chunk_request.m_BlobId.m_SatName,
                    chunk_request.m_BlobId.m_SatKey,
                    true, nullptr));
        }
        details->m_Loader->SetDataReadyCB(
            HST::CHttpReply<CPendingOperation>::s_DataReady, m_Reply);
        details->m_Loader->SetErrorCB(
            CGetBlobErrorCallback(m_PendingOp, m_Reply, details.get()));
        details->m_Loader->SetPropsCallback(
            CBlobPropCallback(m_PendingOp, m_Reply, details.get()));
        details->m_Loader->SetChunkCallback(
            CBlobChunkCallback(m_PendingOp, m_Reply, details.get()));

        m_PendingOp->m_Id2ChunkFetchDetails.push_back(std::move(details));
    }
}


void CBlobPropCallback::x_ParseId2Info(CBlobRecord const &  blob)
{
    // id2_info: "sat.info[.chunks]"

    if (m_Id2InfoParsed)
        return;

    m_Id2InfoParsed = true;

    CPubseqGatewayApp *     app = CPubseqGatewayApp::GetInstance();
    string                  id2_info = blob.GetId2Info();
    vector<string>          parts;
    NStr::Split(id2_info, ".", parts);

    if (parts.size() < 3) {
        // Error: send it in the context of the blob props
        string      message = "Error extracting id2 info for the blob " +
                              GetBlobId(m_FetchDetails->m_BlobId) +
                              ". Expected 'sat.info.nchunks', found " +
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
        m_Id2InfoInfo = NStr::StringToInt(parts[1]);
        m_Id2InfoChunks = NStr::StringToInt(parts[2]);
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

    CPubseqGatewayApp *      app = CPubseqGatewayApp::GetInstance();
    PSG_ERROR(message);

// TEMPORARY!!!
//if (message.find("invalid sequence of operations") != string::npos) {
//    abort();
//}

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
