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

#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info.hpp>
USING_IDBLOB_SCOPE;


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
    m_BlobRequest(blob_request),
    m_TotalSentReplyChunks(initial_reply_chunks),
    m_Cancelled(false),
    m_RequestContext(request_context),
    m_NextItemId(0)
{
    m_BlobRequest.m_ItemId = GetItemId();

    if (m_BlobRequest.GetBlobIdentificationType() == eBySeqId) {
        ERR_POST(Trace << "CPendingOperation::CPendingOperation: blob "
                 "requested by seq_id/id_type: " <<
                 m_BlobRequest.m_SeqId << "." << m_BlobRequest.m_IdType <<
                 ", this: " << this);
    } else {
        ERR_POST(Trace << "CPendingOperation::CPendingOperation: blob "
                 "requested by sat/sat_key: " <<
                 m_BlobRequest.m_BlobId.m_Sat << "." <<
                 m_BlobRequest.m_BlobId.m_SatKey <<
                 ", this: " << this);
    }
}


CPendingOperation::~CPendingOperation()
{
    CRequestContextResetter     context_resetter;
    x_SetRequestContext();

    if (m_BlobRequest.GetBlobIdentificationType() == eBySeqId) {
        ERR_POST(Trace << "CPendingOperation::~CPendingOperation: blob "
                 "requested by seq_id/id_type: " <<
                 m_BlobRequest.m_SeqId << "." << m_BlobRequest.m_IdType <<
                 ", this: " << this);
    } else {
        ERR_POST(Trace << "CPendingOperation::~CPendingOperation: blob "
                 "requested by sat/sat_key: " <<
                 m_BlobRequest.m_BlobId.m_Sat << "." <<
                 m_BlobRequest.m_BlobId.m_SatKey <<
                 ", this: " << this);
    }

    if (m_MainBlobFetchDetails)
        ERR_POST(Trace << "CPendingOperation::~CPendingOperation: "
                 "main blob loader: " <<
                 m_MainBlobFetchDetails->m_Loader.get());
    if (m_Id2ShellFetchDetails)
        ERR_POST(Trace << "CPendingOperation::~CPendingOperation: "
                 "Id2Shell blob loader: " <<
                 m_Id2ShellFetchDetails->m_Loader.get());
    if (m_Id2InfoFetchDetails)
        ERR_POST(Trace << "CPendingOperation::~CPendingOperation: "
                 "Id2Info blob loader: " <<
                 m_Id2InfoFetchDetails->m_Loader.get());

    // Just in case if a request ended without a normal request stop,
    // finish it here as a last resort. (is it Cancel() case?)
    // e410_Gone is chosen for easier identification in the logs
    x_PrintRequestStop(CRequestStatus::e410_Gone);
}


void CPendingOperation::Clear()
{
    CRequestContextResetter     context_resetter;
    x_SetRequestContext();

    if (m_BlobRequest.GetBlobIdentificationType() == eBySeqId) {
        ERR_POST(Trace << "CPendingOperation::Clear: blob "
                 "requested by seq_id/id_type: " <<
                 m_BlobRequest.m_SeqId << "." << m_BlobRequest.m_IdType <<
                 ", this: " << this);
    } else {
        ERR_POST(Trace << "CPendingOperation::Clear: blob "
                 "requested by sat/sat_key: " <<
                 m_BlobRequest.m_BlobId.m_Sat << "." <<
                 m_BlobRequest.m_BlobId.m_SatKey <<
                 ", this: " << this);
    }

    if (m_MainBlobFetchDetails) {
        ERR_POST(Trace << "CPendingOperation::Clear: "
                 "main blob loader: " <<
                 m_MainBlobFetchDetails->m_Loader.get());
        m_MainBlobFetchDetails->m_Loader->SetDataReadyCB(nullptr, nullptr);
        m_MainBlobFetchDetails->m_Loader->SetErrorCB(nullptr);
        m_MainBlobFetchDetails->m_Loader->SetChunkCallback(nullptr);
        m_MainBlobFetchDetails = nullptr;
    }
    if (m_Id2ShellFetchDetails) {
        ERR_POST(Trace << "CPendingOperation::Clear: "
                 "Id2Shell blob loader: " <<
                 m_Id2ShellFetchDetails->m_Loader.get());
        m_Id2ShellFetchDetails->m_Loader->SetDataReadyCB(nullptr, nullptr);
        m_Id2ShellFetchDetails->m_Loader->SetErrorCB(nullptr);
        m_Id2ShellFetchDetails->m_Loader->SetChunkCallback(nullptr);
        m_Id2ShellFetchDetails = nullptr;
    }
    if (m_Id2InfoFetchDetails) {
        ERR_POST(Trace << "CPendingOperation::Clear: "
                 "Id2Info blob loader: " <<
                 m_Id2InfoFetchDetails->m_Loader.get());
        m_Id2InfoFetchDetails->m_Loader->SetDataReadyCB(nullptr, nullptr);
        m_Id2InfoFetchDetails->m_Loader->SetErrorCB(nullptr);
        m_Id2InfoFetchDetails->m_Loader->SetChunkCallback(nullptr);
        m_Id2InfoFetchDetails = nullptr;
    }

    m_Chunks.clear();
    m_Reply = nullptr;
    m_Cancelled = false;

    m_TotalSentReplyChunks = 0;
}


void CPendingOperation::Start(HST::CHttpReply<CPendingOperation>& resp)
{
    m_Reply = &resp;

    // There are two options here:
    // - the blobs were requested by sat/sat_key, i.e. the requests are ready
    // - the blob was requested by a seq_id, i.e. there is no request
    if (m_BlobRequest.GetBlobIdentificationType() == eBySeqId) {
        // By seq_id -> first part is synchronous

        SBioseqInfo     bioseq_info;

        // retrieve from SI2CSI
        if (!x_FetchCanonicalSeqId(bioseq_info)) {
            x_SendReplyCompletion();
            return;
        }

        // retrieve from BIOSEQ_INFO
        if (!x_FetchBioseqInfo(bioseq_info)) {
            x_SendReplyCompletion();
            return;
        }

        // Send BIOSEQ_INFO
        x_SendBioseqInfo(m_BlobRequest.m_ItemId, bioseq_info);

        // The next item here is a blob prop and it should use its own id
        m_BlobRequest.m_ItemId = GetItemId();

        // Translate sat to keyspace
        SBlobId     blob_id(bioseq_info.m_Sat, bioseq_info.m_SatKey);

        if (!x_SatToSatName(m_BlobRequest, blob_id)) {
            x_SendReplyCompletion();
            return;
        }

        // retrieve BLOB_PROP & CHUNKS (if needed)
        m_BlobRequest.m_BlobId = blob_id;

        // Calculate effective no_tse flag
        if (m_BlobRequest.m_IncludeDataFlags & CPendingOperation::fServOrigTSE ||
            m_BlobRequest.m_IncludeDataFlags & CPendingOperation::fServWholeTSE)
            m_BlobRequest.m_IncludeDataFlags &= ~CPendingOperation::fServNoTSE;

        if (m_BlobRequest.m_IncludeDataFlags & CPendingOperation::fServNoTSE) {
            m_BlobRequest.m_NeedBlobProp = true;
            m_BlobRequest.m_NeedChunks = false;
            m_BlobRequest.m_Optional = false;
        } else {
            if (m_BlobRequest.m_IncludeDataFlags & CPendingOperation::fServOrigTSE ||
                m_BlobRequest.m_IncludeDataFlags & CPendingOperation::fServWholeTSE) {
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


void CPendingOperation::x_StartMainBlobRequest(void)
{
    // Here: m_BlobRequest has the resolved sat and a sat_key regardless
    //       how the blob was requested
    m_MainBlobFetchDetails.reset(new SBlobFetchDetails(m_BlobRequest));

    if (m_BlobRequest.m_LastModified.empty()) {
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
                            NStr::StringToLong(m_BlobRequest.m_LastModified),
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

        ERR_POST(error);
        string  blob_reply = GetBlobMessageHeader(
                                        fetch_details->m_ItemId,
                                        fetch_details->m_BlobId,
                                        error.size(),
                                        CRequestStatus::e503_ServiceUnavailable,
                                        CRequestStatus::e503_ServiceUnavailable,
                                        eDiag_Error);

        m_Chunks.push_back(resp.PrepareChunk(
                (const unsigned char *)(blob_reply.data()),
                blob_reply.size()));
        m_Chunks.push_back(resp.PrepareChunk(
                (const unsigned char *)(error.data()), error.size()));

        // It is a chunk about the blob, so increment the counter
        ++fetch_details->m_TotalSentBlobChunks;
        ++m_TotalSentReplyChunks;

        // Send the completion after error as well: +1 for the completion
        string  completion = GetBlobCompletionHeader(
                                    fetch_details->m_ItemId,
                                    fetch_details->m_BlobId,
                                    fetch_details->m_TotalSentBlobChunks + 1);
        m_Chunks.push_back(resp.PrepareChunk(
                (const unsigned char *)(completion.data()),
                completion.size()));
        ++m_TotalSentReplyChunks;

        // Mark finished
        fetch_details->m_FinishedRead = true;

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

    return started_count != 0;
}


void CPendingOperation::x_SendReplyCompletion(void)
{
    // Send the reply completion only if needed
    if (x_AllFinishedRead()) {
        ++m_TotalSentReplyChunks;   // +1 for the reply completion
        string  reply_completion = GetReplyCompletionHeader(
                                        m_TotalSentReplyChunks);
        m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(reply_completion.data()),
                reply_completion.size()));

        // No need to set the context/engage context resetter: they set outside
        x_PrintRequestStop(CRequestStatus::e200_Ok);
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


bool CPendingOperation::x_FetchCanonicalSeqId(SBioseqInfo &  bioseq_info)
{
    string                  msg;
    CRequestStatus::ECode   status;
    try {
        if (!FetchCanonicalSeqId(m_Conn, m_BlobRequest.m_SeqId,
                                 m_BlobRequest.m_IdType,
                                 bioseq_info.m_Accession,
                                 bioseq_info.m_Version,
                                 bioseq_info.m_IdType)) {
            // Failure: no translation to canonical
            msg = "No translation seq_id '" +
                  m_BlobRequest.m_SeqId + "' of type " +
                  NStr::NumericToString(m_BlobRequest.m_IdType) +
                  " to canonical seq_id found";
            status = CRequestStatus::e404_NotFound;
        }
    } catch (const exception &  exc) {
        msg = "Error translating seq_id '" +
              m_BlobRequest.m_SeqId + "' of type " +
              NStr::NumericToString(m_BlobRequest.m_IdType) +
              " to canonical seq_id: " + string(exc.what());
        status = CRequestStatus::e500_InternalServerError;
    } catch (...) {
        msg = "Unknown error translating seq_id '" +
              m_BlobRequest.m_SeqId + "' of type " +
              NStr::NumericToString(m_BlobRequest.m_IdType) +
              " to canonical seq_id";
        status = CRequestStatus::e500_InternalServerError;
    }

    if (!msg.empty()) {
        size_t      item_id = m_BlobRequest.m_ItemId;

        CPubseqGatewayApp::GetInstance()->GetErrorCounters().
                                                     IncCanonicalSeqIdError();
        ERR_POST(msg);
        string  header = GetBioseqMessageHeader(item_id, msg.size(),
                                                status, status,
                                                eDiag_Error);
        m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(header.data()),
                    header.size()));
        m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(msg.data()),
                    msg.size()));
        ++m_TotalSentReplyChunks;

        string      bioseq_meta = GetBioseqCompletionHeader(item_id, 2);
        m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(bioseq_meta.data()),
                    bioseq_meta.size()));
        ++m_TotalSentReplyChunks;
        return false;
    }

    // No problems, resolved
    return true;
}


bool CPendingOperation::x_FetchBioseqInfo(SBioseqInfo &  bioseq_info)
{
    string                  msg;
    CRequestStatus::ECode   status;
    try {
        if (!FetchBioseqInfo(m_Conn, bioseq_info)) {
            // Failure: no bioseq info
            msg = "No bioseq info found for seq_id '" +
                  m_BlobRequest.m_SeqId + "' of type " +
                  NStr::NumericToString(m_BlobRequest.m_IdType);
            status = CRequestStatus::e404_NotFound;
        }
    } catch (const exception &  exc) {
        msg = "Error retrieving bioseq info for seq_id '" +
              m_BlobRequest.m_SeqId + "' of type " +
              NStr::NumericToString(m_BlobRequest.m_IdType) +
              ": " + string(exc.what());
        status = CRequestStatus::e500_InternalServerError;
    } catch (...) {
        msg = "Unknown error retrieving bioseq info for seq_id '" +
              m_BlobRequest.m_SeqId + "' of type " +
              NStr::NumericToString(m_BlobRequest.m_IdType);
        status = CRequestStatus::e500_InternalServerError;
    }

    if (!msg.empty()) {
        size_t      item_id = m_BlobRequest.m_ItemId;

        ERR_POST(msg);
        string  header = GetBioseqMessageHeader(item_id, msg.size(),
                                                status, status,
                                                eDiag_Error);
        m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(header.data()),
                    header.size()));
        m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(msg.data()),
                    msg.size()));
        ++m_TotalSentReplyChunks;

        string      bioseq_meta = GetBioseqCompletionHeader(item_id, 2);
        m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(bioseq_meta.data()),
                    bioseq_meta.size()));
        ++m_TotalSentReplyChunks;
        return false;
    }

    // No problems, biseq info has been retrieved
    return true;
}


void CPendingOperation::x_SendBioseqInfo(size_t  item_id,
                                         const SBioseqInfo &  bioseq_info)
{
    CJsonNode   json = BioseqInfoToJSON(bioseq_info);
    string      content = json.Repr();
    string      header = GetBioseqInfoHeader(item_id, content.size());

    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()),
                header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(content.data()),
                content.size()));
    ++m_TotalSentReplyChunks;

    string      bioseq_meta = GetBioseqCompletionHeader(item_id, 2);
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(bioseq_meta.data()),
                bioseq_meta.size()));
    ++m_TotalSentReplyChunks;
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
               blob_request.m_SeqId + "' of type " +
               NStr::NumericToString(blob_request.m_IdType);
    else
        msg += " for the blob " + GetBlobId(blob_id);
    x_SendUnknownSatelliteError(blob_request.m_ItemId, msg,
                                eUnknownResolvedSatellite);

    app->GetErrorCounters().IncSatToSatName();
    return false;
}


void CPendingOperation::x_SendUnknownSatelliteError(
                                size_t  item_id,
                                const string &  msg,
                                int  err_code)
{
    ERR_POST(msg);

    string      header = GetBlobPropMessageHeader(item_id, msg.size(),
                                                  CRequestStatus::e404_NotFound,
                                                  err_code, eDiag_Error);
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(msg.data()), msg.size()));
    ++m_TotalSentReplyChunks;

    string      blob_prop_meta = GetBlobPropCompletionHeader(item_id, 2);
    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(blob_prop_meta.data()),
                    blob_prop_meta.size()));
    ++m_TotalSentReplyChunks;
}


CBlobChunkCallback::CBlobChunkCallback(
        CPendingOperation *  pending_op,
        HST::CHttpReply<CPendingOperation> *  reply,
        CPendingOperation::SBlobFetchDetails *  fetch_details) :
    m_PendingOp(pending_op), m_Reply(reply), m_FetchDetails(fetch_details)
{}


void CBlobChunkCallback::operator()(const unsigned char *  data,
                                    unsigned int  size,
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
        ERR_POST("Unexpected data received "
                 "while the output has finished, ignoring");
        return;
    }

    if (chunk_no >= 0) {
        // A blob chunk; 0-length chunks are allowed too
        ++m_FetchDetails->m_TotalSentBlobChunks;
        ++m_PendingOp->m_TotalSentReplyChunks;

        string      header = GetBlobChunkHeader(
                                m_FetchDetails->m_ItemId,
                                m_FetchDetails->m_BlobId,
                                size, chunk_no);
        m_PendingOp->m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()),
                header.size()));

        if (size > 0 && data != nullptr)
            m_PendingOp->m_Chunks.push_back(m_Reply->PrepareChunk(data, size));
    } else {
        // End of the blob; +1 because of this very blob meta chunk
        string  blob_completion = GetBlobCompletionHeader(
                    m_FetchDetails->m_ItemId,
                    m_FetchDetails->m_BlobId,
                    m_FetchDetails->m_TotalSentBlobChunks + 1);
        m_PendingOp->m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(blob_completion.data()),
                blob_completion.size()));

        // +1 is for the blob completion message
        ++m_PendingOp->m_TotalSentReplyChunks;

        m_FetchDetails->m_FinishedRead = true;

        if (m_FetchDetails->m_BlobIdType == eBySeqId)
            CPubseqGatewayApp::GetInstance()->GetRequestCounters().
                                              IncGetBlobBySeqId();
        else
            CPubseqGatewayApp::GetInstance()->GetRequestCounters().
                                              IncGetBlobBySatSatKey();

        m_PendingOp->x_SendReplyCompletion();
    }

    if (m_Reply->IsOutputReady())
        m_PendingOp->Peek(*m_Reply, false);
}


CBlobPropCallback::CBlobPropCallback(
        CPendingOperation *  pending_op,
        HST::CHttpReply<CPendingOperation> *  reply,
        CPendingOperation::SBlobFetchDetails *  fetch_details) :
    m_PendingOp(pending_op), m_Reply(reply), m_FetchDetails(fetch_details)
{}


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
        CJsonNode   json = BlobPropToJSON(blob);
        string      content = json.Repr();
        string      header = GetBlobPropHeader(m_FetchDetails->m_ItemId,
                                               content.size());

        m_PendingOp->m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()),
                header.size()));

        m_PendingOp->m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(content.data()),
                    content.size()));

        ++m_FetchDetails->m_TotalSentBlobChunks;
        ++m_PendingOp->m_TotalSentReplyChunks;

        if (m_FetchDetails->m_NeedChunks == false) {
            m_FetchDetails->m_FinishedRead = true;
            if (m_FetchDetails->m_IncludeDataFlags & CPendingOperation::fServNoTSE) {
                // Reply is finished, the only blob prop were needed
                x_SendBlobPropCompletion();
                m_PendingOp->x_SendReplyCompletion();
            } else {
                // Not the end; need to decide about the other two blobs or the
                // original one
                if (blob.GetId2Info().empty()) {
                    // Request original blob chunks

                    // It is important to send completion before: the new
                    // request overwrites the main one
                    x_SendBlobPropCompletion();
                    x_RequestOriginalBlobChunks(blob);
                } else {
                    // Request chunks of two other ID2 blobs.
                    x_RequestID2BlobChunks(blob);

                    // It is important to send completion after: there could be
                    // an error of converting/translating ID2 info
                    x_SendBlobPropCompletion();
                }
            }
        }
    } else {
        // Not found, i.e. report 404
        CPubseqGatewayApp::GetInstance()->GetErrorCounters().
                                                IncGetBlobNotFound();

        string  message = "Blob properties are not found";
        ERR_POST(message);
        string  blob_prop_msg = GetBlobPropMessageHeader(
                                    m_FetchDetails->m_ItemId,
                                    message.size(),
                                    CRequestStatus::e404_NotFound,
                                    CRequestStatus::e404_NotFound,
                                    eDiag_Error);

        m_PendingOp->m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(blob_prop_msg.data()),
                blob_prop_msg.size()));
        m_PendingOp->m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(message.data()),
                message.size()));

        ++m_FetchDetails->m_TotalSentBlobChunks;
        ++m_PendingOp->m_TotalSentReplyChunks;

        x_SendBlobPropCompletion();

        m_FetchDetails->m_FinishedRead = true;
        m_PendingOp->x_SendReplyCompletion();
    }

    if (m_Reply->IsOutputReady())
        m_PendingOp->Peek(*m_Reply, false);
}


void CBlobPropCallback::x_SendBlobPropCompletion(void)
{
    string      completion = GetBlobPropCompletionHeader(
                                m_FetchDetails->m_ItemId,
                                m_FetchDetails->m_TotalSentBlobChunks + 1);

    m_PendingOp->m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(completion.data()),
                completion.size()));
    ++m_PendingOp->m_TotalSentReplyChunks;
}


void CBlobPropCallback::x_RequestOriginalBlobChunks(CBlobRecord const &  blob)
{
    // Cannot reuse the fetch details, it leads to a core dump

    SBlobRequest    orig_blob_request(m_FetchDetails->m_BlobId,
                                      NStr::NumericToString(blob.GetModified()));

    orig_blob_request.m_NeedBlobProp = false;
    orig_blob_request.m_NeedChunks = true;
    orig_blob_request.m_Optional = false;
    orig_blob_request.m_ItemId = m_PendingOp->GetItemId();

    m_PendingOp->m_OriginalBlobChunkFetch.reset(
            new CPendingOperation::SBlobFetchDetails(orig_blob_request));
    m_PendingOp->m_OriginalBlobChunkFetch->m_Loader.reset(
            new CCassBlobTaskLoadBlob(
                    m_PendingOp->m_Timeout,
                    m_PendingOp->m_MaxRetries,
                    m_PendingOp->m_Conn,
                    orig_blob_request.m_BlobId.m_SatName,
                    orig_blob_request.m_BlobId.m_SatKey,
                    blob.GetModified(),
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
    // id2_info: "sat.shell.info"
    string              id2_info = blob.GetId2Info();
    vector<string>      parts;
    NStr::Split(id2_info, ".", parts);

    if (parts.size() < 3) {
        // Error: send it in the context of the blob props
        string      message = "Error extracting id2 info for the blob " +
                              GetBlobId(m_FetchDetails->m_BlobId) +
                              ". Expected 'sat.shell.info', found " +
                              NStr::NumericToString(parts.size()) + " parts.";
        string      header = GetBlobPropMessageHeader(
                                    m_FetchDetails->m_ItemId,
                                    message.size(),
                                    CRequestStatus::e503_ServiceUnavailable,
                                    eBadID2Info, eDiag_Error);
        m_PendingOp->m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()),
                header.size()));
        m_PendingOp->m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(message.data()),
                message.size()));
        ++m_FetchDetails->m_TotalSentBlobChunks;
        ++m_PendingOp->m_TotalSentReplyChunks;
        ERR_POST(message);
        return;
    }

    int         sat;
    int         shell;
    int         info;

    try {
        sat = NStr::StringToInt(parts[0]);
        shell = NStr::StringToInt(parts[1]);
        info = NStr::StringToInt(parts[2]);
    } catch (...) {
        // Error: send it in the context of the blob props
        string      message = "Error converting id2 info into integers for "
                              "the blob " + GetBlobId(m_FetchDetails->m_BlobId);
        string      header = GetBlobPropMessageHeader(
                                    m_FetchDetails->m_ItemId,
                                    message.size(),
                                    CRequestStatus::e503_ServiceUnavailable,
                                    eBadID2Info, eDiag_Error);
        m_PendingOp->m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()),
                header.size()));
        m_PendingOp->m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(message.data()),
                message.size()));
        ++m_FetchDetails->m_TotalSentBlobChunks;
        ++m_PendingOp->m_TotalSentReplyChunks;
        ERR_POST(message);
        return;
    }

    // Translate sat to keyspace
    SBlobId     shell_blob_id(sat, shell);  // mandatory
    SBlobId     info_blob_id(sat, info);    // optional

    CPubseqGatewayApp *      app = CPubseqGatewayApp::GetInstance();
    if (!app->SatToSatName(sat, shell_blob_id.m_SatName)) {
        // Error: send it in the context of the blob props
        string      message = "Error mapping id2 info sat (" +
                              NStr::NumericToString(sat) +
                              ") to a cassandra keyspace for the blob " +
                              GetBlobId(m_FetchDetails->m_BlobId);
        string      header = GetBlobPropMessageHeader(
                                    m_FetchDetails->m_ItemId,
                                    message.size(),
                                    CRequestStatus::e503_ServiceUnavailable,
                                    eBadID2Info, eDiag_Error);
        m_PendingOp->m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()),
                header.size()));
        m_PendingOp->m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(message.data()),
                message.size()));
        ++m_FetchDetails->m_TotalSentBlobChunks;
        ++m_PendingOp->m_TotalSentReplyChunks;

        app->GetErrorCounters().IncSatToSatName();
        ERR_POST(message);
        return;
    }

    // The keyspace match for both blobs
    info_blob_id.m_SatName = shell_blob_id.m_SatName;

    // Create the Id2Shell and Id2Info requests
    SBlobRequest    shell_blob_request(shell_blob_id, "");
    SBlobRequest    info_blob_request(info_blob_id, "");

    shell_blob_request.m_NeedBlobProp = false;
    shell_blob_request.m_NeedChunks = true;
    shell_blob_request.m_Optional = false;
    shell_blob_request.m_ItemId = m_PendingOp->GetItemId();

    info_blob_request.m_NeedBlobProp = false;
    info_blob_request.m_NeedChunks = true;
    info_blob_request.m_Optional = true;
    info_blob_request.m_ItemId = m_PendingOp->GetItemId();

    // Deploy Id2Shell retrieval
    m_PendingOp->m_Id2ShellFetchDetails.reset(
            new CPendingOperation::SBlobFetchDetails(shell_blob_request));
    m_PendingOp->m_Id2ShellFetchDetails->m_Loader.reset(
            new CCassBlobTaskLoadBlob(
                m_PendingOp->m_Timeout, m_PendingOp->m_MaxRetries,
                m_PendingOp->m_Conn,
                shell_blob_request.m_BlobId.m_SatName,
                shell_blob_request.m_BlobId.m_SatKey,
                shell_blob_request.m_NeedChunks,
                nullptr));
    m_PendingOp->m_Id2ShellFetchDetails->m_Loader->SetDataReadyCB(
            HST::CHttpReply<CPendingOperation>::s_DataReady, m_Reply);
    m_PendingOp->m_Id2ShellFetchDetails->m_Loader->SetErrorCB(
            CGetBlobErrorCallback(m_PendingOp, m_Reply,
                                  m_PendingOp->m_Id2ShellFetchDetails.get()));
    m_PendingOp->m_Id2ShellFetchDetails->m_Loader->SetPropsCallback(nullptr);
    m_PendingOp->m_Id2ShellFetchDetails->m_Loader->SetChunkCallback(
            CBlobChunkCallback(m_PendingOp, m_Reply,
                               m_PendingOp->m_Id2ShellFetchDetails.get()));
    m_PendingOp->m_Id2ShellFetchDetails->m_Loader->Wait();

    // Deploy Id2Info retrieval
    m_PendingOp->m_Id2InfoFetchDetails.reset(
            new CPendingOperation::SBlobFetchDetails(info_blob_request));
    m_PendingOp->m_Id2InfoFetchDetails->m_Loader.reset(
            new CCassBlobTaskLoadBlob(
                m_PendingOp->m_Timeout, m_PendingOp->m_MaxRetries,
                m_PendingOp->m_Conn,
                info_blob_request.m_BlobId.m_SatName,
                info_blob_request.m_BlobId.m_SatKey,
                info_blob_request.m_NeedChunks,
                nullptr));
    m_PendingOp->m_Id2InfoFetchDetails->m_Loader->SetDataReadyCB(
            HST::CHttpReply<CPendingOperation>::s_DataReady, m_Reply);
    m_PendingOp->m_Id2InfoFetchDetails->m_Loader->SetErrorCB(
            CGetBlobErrorCallback(m_PendingOp, m_Reply,
                                  m_PendingOp->m_Id2InfoFetchDetails.get()));
    m_PendingOp->m_Id2InfoFetchDetails->m_Loader->SetPropsCallback(nullptr);
    m_PendingOp->m_Id2InfoFetchDetails->m_Loader->SetChunkCallback(
            CBlobChunkCallback(m_PendingOp, m_Reply,
                               m_PendingOp->m_Id2InfoFetchDetails.get()));
    m_PendingOp->m_Id2InfoFetchDetails->m_Loader->Wait();
}


CGetBlobErrorCallback::CGetBlobErrorCallback(
        CPendingOperation *  pending_op,
        HST::CHttpReply<CPendingOperation> *  reply,
        CPendingOperation::SBlobFetchDetails *  fetch_details) :
    m_PendingOp(pending_op), m_Reply(reply), m_FetchDetails(fetch_details)
{}


void CGetBlobErrorCallback::operator()(CRequestStatus::ECode  status,
                                       int  code,
                                       EDiagSev  severity,
                                       const string &  message)
{
    CRequestContextResetter     context_resetter;
    m_PendingOp->x_SetRequestContext();

    // If it was an optional blob then the retrieving errors are to be ignored
    if (m_FetchDetails->m_Optional)
        return;


    // It could be a message or an error
    bool    is_error = (severity == eDiag_Error ||
                        severity == eDiag_Critical ||
                        severity == eDiag_Fatal);

    // To avoid sending an error in Peek()
    m_FetchDetails->m_Loader->ClearError();

    CPubseqGatewayApp *      app = CPubseqGatewayApp::GetInstance();
    ERR_POST(message);
    if (status == CRequestStatus::e404_NotFound) {
        app->GetErrorCounters().IncGetBlobNotFound();
    } else {
        if (is_error)
            app->GetErrorCounters().IncUnknownError();
    }

    string  blob_reply = GetBlobMessageHeader(
                                m_FetchDetails->m_ItemId,
                                m_FetchDetails->m_BlobId,
                                message.size(), status, code, severity);

    m_PendingOp->m_Chunks.push_back(m_Reply->PrepareChunk(
            (const unsigned char *)(blob_reply.data()),
            blob_reply.size()));
    m_PendingOp->m_Chunks.push_back(m_Reply->PrepareChunk(
            (const unsigned char *)(message.data()), message.size()));

    // It is a chunk about the blob, so increment the counter
    ++m_FetchDetails->m_TotalSentBlobChunks;
    ++m_PendingOp->m_TotalSentReplyChunks;

    if (is_error) {
        string  completion = GetBlobCompletionHeader(
                            m_FetchDetails->m_ItemId,
                            m_FetchDetails->m_BlobId,
                            m_FetchDetails->m_TotalSentBlobChunks + 1);
        m_PendingOp->m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(completion.data()),
                blob_reply.size()));
        ++m_PendingOp->m_TotalSentReplyChunks;
        m_FetchDetails->m_FinishedRead = true;
    }

    m_PendingOp->x_SendReplyCompletion();

    if (m_Reply->IsOutputReady())
        m_PendingOp->Peek(*m_Reply, false);
}
