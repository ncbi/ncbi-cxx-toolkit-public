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



CPendingOperation::CPendingOperation(const SBlobRequest &  blob_request,
                                     size_t  initial_reply_chunks,
                                     shared_ptr<CCassConnection>  conn,
                                     unsigned int  timeout,
                                     unsigned int  max_retries,
                                     CRef<CRequestContext>  request_context) :
    m_Reply(nullptr),
    m_TotalSentReplyChunks(initial_reply_chunks),
    m_Cancelled(false),
    m_RequestContext(request_context)
{
    unique_ptr<SBlobRequestDetails>     details(
                                    new SBlobRequestDetails(blob_request));

    details->m_Context.reset(new SOperationContext(blob_request.m_BlobId));
    details->m_Loader.reset(new CCassBlobTaskLoadBlob(timeout,
                                                      max_retries,
                                                      conn,
                                                      blob_request.m_BlobId.m_SatName,
                                                      blob_request.m_BlobId.m_SatKey,
                                                      true,
                                                      nullptr));

    m_Requests.insert(make_pair(blob_request.m_BlobId, std::move(details)));
}


CPendingOperation::CPendingOperation(
        const vector<SBlobRequest> &  blob_requests,
        size_t  initial_reply_chunks,
        shared_ptr<CCassConnection>  conn,
        unsigned int  timeout,
        unsigned int  max_retries,
        CRef<CRequestContext>  request_context) :
    m_Reply(nullptr),
    m_TotalSentReplyChunks(initial_reply_chunks),
    m_Cancelled(false),
    m_RequestContext(request_context)
{
#if 0
    for (const auto &  blob_request: blob_requests) {
        unique_ptr<SBlobRequestDetails>     details(
                                    new SBlobRequestDetails(blob_request));

        details->m_Context.reset(new SOperationContext(blob_request.m_BlobId));
        details->m_Loader.reset(new CCassBlobLoader(
                                            timeout, conn,
                                            blob_request.m_BlobId.m_SatName,
                                            blob_request.m_BlobId.m_SatKey,
                                            true, max_retries,
                                            details->m_Context.get(),
                                            nullptr,  nullptr));
        m_Requests.insert(make_pair(blob_request.m_BlobId, std::move(details)));
    }
#endif
}


CPendingOperation::~CPendingOperation()
{
    CRequestContextResetter     context_resetter;
    x_SetRequestContext();

    for (const auto &  request: m_Requests) {
        ERR_POST(Trace << "CPendingOperation::CPendingOperation: blob " <<
                 request.first.m_Sat << "." << request.first.m_SatKey <<
                 ", this: " << this <<
                 " m_Loader: " << request.second->m_Loader.get());
    }

    // Just in case if a request ended without a normal request stop,
    // finish it here as a last resort. (is it Cancel() case?)
    // e410_Gone is chosen for easier identification in the logs
    x_PrintRequestStop(CRequestStatus::e410_Gone);
}


void CPendingOperation::Clear()
{
    CRequestContextResetter     context_resetter;
    x_SetRequestContext();

    for (const auto &  request: m_Requests) {
        ERR_POST(Trace << "CPendingOperation::Clear: blob " <<
                 request.first.m_Sat << "." << request.first.m_SatKey <<
                 " this: " << this <<
                 " m_Loader: " << request.second->m_Loader.get());
        request.second->m_Loader->SetDataReadyCB(nullptr, nullptr);
        request.second->m_Loader->SetErrorCB(nullptr);
        request.second->m_Loader->SetChunkCallback(nullptr);
    }
    m_Requests.clear();

    m_Chunks.clear();
    m_Reply = nullptr;
    m_Cancelled = false;

    m_TotalSentReplyChunks = 0;
}


void CPendingOperation::Start(HST::CHttpReply<CPendingOperation>& resp)
{
    m_Reply = &resp;

    for (auto &  request: m_Requests) {
        request.second->m_Loader->SetDataReadyCB(
                            HST::CHttpReply<CPendingOperation>::s_DataReady,
                            &resp);

        auto    op_context = request.second->m_Context.get();

        request.second->m_Loader->SetErrorCB(
        [this, &resp, op_context](CRequestStatus::ECode  status,
                                  int  code,
                                  EDiagSev  severity,
                                  const string &  message)
        {
            CRequestContextResetter     context_resetter;
            x_SetRequestContext();

            auto    request_details = m_Requests.find(op_context->m_BlobId);
            if (request_details == m_Requests.end()) {
                // Logic error, a blob which which was not ordered
                ERR_POST("Logic error. An error callback is called "
                         "for the blob which was not requested: " <<
                         op_context->m_BlobId.m_Sat << "." <<
                         op_context->m_BlobId.m_SatKey);
                return;
            }

            // It could be a message or an error
            bool    is_error = (severity == eDiag_Error ||
                                severity == eDiag_Critical ||
                                severity == eDiag_Fatal);

            // To avoid sending an error in Peek()
            request_details->second->m_Loader->ClearError();

            CPubseqGatewayApp *      app = CPubseqGatewayApp::GetInstance();
            ERR_POST(message);
            if (status == CRequestStatus::e404_NotFound) {
                app->GetErrorCounters().IncGetBlobNotFound();
            } else {
                if (is_error)
                    app->GetErrorCounters().IncUnknownError();
            }

            string  blob_reply;
            if (is_error)
                blob_reply = GetBlobErrorHeader(op_context->m_BlobId,
                                                message.size(),
                                                status, code, severity);
            else
                blob_reply = GetBlobMessageHeader(op_context->m_BlobId,
                                                  message.size(),
                                                  status, code, severity);

            m_Chunks.push_back(resp.PrepareChunk(
                    (const unsigned char *)(blob_reply.data()),
                    blob_reply.size()));
            m_Chunks.push_back(resp.PrepareChunk(
                    (const unsigned char *)(message.data()), message.size()));

            // It is a chunk about the blob, so increment the counter
            ++request_details->second->m_TotalSentBlobChunks;
            ++m_TotalSentReplyChunks;

            if (is_error)
                request_details->second->m_FinishedRead = true;

            x_SendReplyCompletion();

            if (resp.IsOutputReady())
                Peek(resp, false);
        }
        );

        request.second->m_Loader->SetPropsCallback(
        [this, &resp, op_context](CBlobRecord const & blob, bool isFound)
        {
            ERR_POST("BLOB Props callback: " << isFound);
        }
        );

        request.second->m_Loader->SetChunkCallback(
        [this, &resp, op_context](const unsigned char *  data,
                                  unsigned int  size,
                                  int  chunk_no)
        {
            CRequestContextResetter     context_resetter;
            x_SetRequestContext();

            auto    request_details = m_Requests.find(op_context->m_BlobId);
            if (request_details == m_Requests.end()) {
                // Logic error, a blob which which was not ordered
                ERR_POST("Logic error. A data chunk callback is called "
                         "for the blob which was not requested: " <<
                         op_context->m_BlobId.m_Sat << "." <<
                         op_context->m_BlobId.m_SatKey);
                return;
            }

            ERR_POST(Info << "Chunk: [" << chunk_no << "]: " << size);
            assert(!request_details->second->m_FinishedRead);
            if (m_Cancelled) {
                request_details->second->m_Loader->Cancel();
                request_details->second->m_FinishedRead = true;
                return;
            }
            if (resp.IsFinished()) {
                CPubseqGatewayApp::GetInstance()->GetErrorCounters().
                                                             IncUnknownError();
                ERR_POST("Unexpected data received "
                         "while the output has finished, ignoring");
                return;
            }

            if (chunk_no >= 0) {
                // A blob chunk; 0-length chunks are allowed too
                ++request_details->second->m_TotalSentBlobChunks;
                ++m_TotalSentReplyChunks;

                string      header = GetBlobChunkHeader(size,
                                                        op_context->m_BlobId,
                                                        chunk_no);
                m_Chunks.push_back(resp.PrepareChunk(
                            (const unsigned char *)(header.data()),
                            header.size()));

                if (size > 0 && data != nullptr)
                    m_Chunks.push_back(resp.PrepareChunk(data, size));
            } else {
                // End of the blob; +1 because of this very blob meta chunk
                string  blob_completion = GetBlobCompletionHeader(
                            op_context->m_BlobId,
                            request_details->second->m_TotalSentBlobChunks,
                            request_details->second->m_TotalSentBlobChunks + 1);
                m_Chunks.push_back(resp.PrepareChunk(
                        (const unsigned char *)(blob_completion.data()),
                        blob_completion.size()));

                // +1 is for the blob completion message
                ++m_TotalSentReplyChunks;

                request_details->second->m_FinishedRead = true;

                if (request_details->second->m_BlobIdType == eByAccession)
                    CPubseqGatewayApp::GetInstance()->GetRequestCounters().
                                                      IncGetBlobByAccession();
                else
                    CPubseqGatewayApp::GetInstance()->GetRequestCounters().
                                                      IncGetBlobBySatSatKey();

                x_SendReplyCompletion();
            }

            if (resp.IsOutputReady())
                Peek(resp, false);
        }
        );

        request.second->m_Loader->Wait();
    }
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
    // 3 -> call resp->  to send what we have if it is ready
    for (auto &  request: m_Requests) {
        if (need_wait) {
            if (!request.second->m_FinishedRead) {
                request.second->m_Loader->Wait();
            }
        }

        if (request.second->m_Loader->HasError() &&
            resp.IsOutputReady() && !resp.IsFinished()) {
            // Send an error
            string               error = request.second->m_Loader->LastError();
            CPubseqGatewayApp *  app = CPubseqGatewayApp::GetInstance();
            app->GetErrorCounters().IncUnknownError();

            string  blob_reply = GetBlobErrorHeader(
                                        request.first, error.size(),
                                        CRequestStatus::e503_ServiceUnavailable,
                                        CRequestStatus::e503_ServiceUnavailable,
                                        eDiag_Error);

            m_Chunks.push_back(resp.PrepareChunk(
                    (const unsigned char *)(blob_reply.data()),
                    blob_reply.size()));
            m_Chunks.push_back(resp.PrepareChunk(
                    (const unsigned char *)(error.data()), error.size()));

            // It is a chunk about the blob, so increment the counter
            ++request.second->m_TotalSentBlobChunks;
            ++m_TotalSentReplyChunks;

            // Mark finished
            request.second->m_FinishedRead = true;

            x_SendReplyCompletion();
        }
    }

    bool    all_finished_read = x_AllFinishedRead();
    if (resp.IsOutputReady() && (!m_Chunks.empty() || all_finished_read)) {
        resp.Send(m_Chunks, all_finished_read);
        m_Chunks.clear();
    }
}


bool CPendingOperation::x_AllFinishedRead(void) const
{
    for (const auto &  request: m_Requests) {
        if (!request.second->m_FinishedRead)
            return false;
    }
    return true;
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
