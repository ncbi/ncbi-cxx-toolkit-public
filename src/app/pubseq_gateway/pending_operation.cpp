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


CPendingOperation::CPendingOperation(EBlobIdentificationType  blob_id_type,
                                     const SBlobId &  blob_id,
                                     string &&  sat_name,
                                     shared_ptr<CCassConnection>  conn,
                                     unsigned int  timeout,
                                     unsigned int  max_retries) :
    m_BlobIdType(blob_id_type),
    m_TotalSentBlobChunks(0),
    m_TotalSentReplyChunks(0),
    m_Reply(nullptr),
    m_Cancelled(false),
    m_FinishedRead(false),
    m_BlobId(blob_id)
{
    // In case of an accession identification a resolution has already been
    // sent as one reply chunk, so reflect it here
    if (blob_id_type == eByAccession)
        m_TotalSentReplyChunks = 1;

    m_Context.reset(new SOperationContext(blob_id));
    m_Loader.reset(new CCassBlobLoader(&m_Op, timeout, conn,
                                       std::move(sat_name),
                                       m_BlobId.sat_key,
                                       true, max_retries, m_Context.get(),
                                       nullptr,  nullptr));
}


CPendingOperation::~CPendingOperation()
{
    LOG5(("CPendingOperation::CPendingOperation: this: %p, m_Loader: %p",
          this, m_Loader.get()));
}


void CPendingOperation::Clear()
{
    LOG5(("CPendingOperation::Clear: this: %p, m_Loader: %p",
          this, m_Loader.get()));
    if (m_Loader) {
        m_Loader->SetDataReadyCB(nullptr, nullptr);
        m_Loader->SetErrorCB(nullptr);
        m_Loader->SetDataChunkCB(nullptr);
        m_Loader = nullptr;
    }
    m_Chunks.clear();
    m_Reply = nullptr;
    m_Cancelled = false;
    m_FinishedRead = false;
    m_TotalSentBlobChunks = 0;
    m_TotalSentReplyChunks = 0;
}


void CPendingOperation::Start(HST::CHttpReply<CPendingOperation>& resp)
{
    m_Reply = &resp;

    m_Loader->SetDataReadyCB(HST::CHttpReply<CPendingOperation>::s_DataReady, &resp);
    m_Loader->SetErrorCB(
        [this, &resp](void *  context,
                      CRequestStatus::ECode  status,
                      int  code,
                      EDiagSev  severity,
                      const string &  message)
        {
            SOperationContext *     op_context =
                            reinterpret_cast<SOperationContext *>(context);

            // It could be a message or an error
            bool    is_error = (severity == eDiag_Error ||
                                severity == eDiag_Critical ||
                                severity == eDiag_Fatal);

            // To avoid sending 503 in Peek()
            m_Loader->ClearError();

            CPubseqGatewayApp *      app = CPubseqGatewayApp::GetInstance();
            ERRLOG1(("%s", message.c_str()));
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
            ++m_TotalSentBlobChunks;
            ++m_TotalSentReplyChunks;

            if (is_error) {
                ++m_TotalSentReplyChunks;   // +1 for the reply completion
                string  reply_completion = GetReplyCompletionHeader(
                                                m_TotalSentReplyChunks);
                m_Chunks.push_back(resp.PrepareChunk(
                        (const unsigned char *)(reply_completion.data()),
                        reply_completion.size()));

                m_FinishedRead = true;
            }

            if (resp.IsOutputReady())
                Peek(resp, false);
        }
    );
    m_Loader->SetDataChunkCB(
        [this, &resp](void *  context,
                      const unsigned char *  data,
                      unsigned int  size,
                      int  chunk_no)
        {
            SOperationContext *     op_context =
                            reinterpret_cast<SOperationContext *>(context);

            LOG3(("Chunk: [%d]: %u", chunk_no, size));
            assert(!m_FinishedRead);
            if (m_Cancelled) {
                m_Loader->Cancel();
                m_FinishedRead = true;
                return;
            }
            if (resp.IsFinished()) {
                CPubseqGatewayApp::GetInstance()->GetErrorCounters().
                                                             IncUnknownError();
                ERRLOG1(("Unexpected data received "
                         "while the output has finished, ignoring"));
                return;
            }

            if (chunk_no >= 0) {
                // A blob chunk; 0-length chunks are allowed too
                ++m_TotalSentBlobChunks;
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
                                            m_TotalSentBlobChunks,
                                            m_TotalSentBlobChunks + 1);
                m_Chunks.push_back(resp.PrepareChunk(
                        (const unsigned char *)(blob_completion.data()),
                        blob_completion.size()));

                // +1 is for the blob completion message
                // +1 is for the reply completion message
                m_TotalSentReplyChunks += 2;
                string  reply_completion = GetReplyCompletionHeader(
                                                m_TotalSentReplyChunks);
                m_Chunks.push_back(resp.PrepareChunk(
                        (const unsigned char *)(reply_completion.data()),
                        reply_completion.size()));

                m_FinishedRead = true;

                if (m_BlobIdType == eByAccession)
                    CPubseqGatewayApp::GetInstance()->GetRequestCounters().
                                                      IncGetBlobByAccession();
                else
                    CPubseqGatewayApp::GetInstance()->GetRequestCounters().
                                                      IncGetBlobBySatSatKey();
            }

            if (resp.IsOutputReady())
                Peek(resp, false);
        }
    );

    m_Loader->Wait();
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
    if (m_Loader) {
       if (need_wait) {
            m_Loader->Wait();
       }
       if (m_Loader->HasError() && resp.IsOutputReady() && !resp.IsFinished()) {
           resp.Send503("error", m_Loader->LastError().c_str());
           return;
       }
    }

    if (resp.IsOutputReady() && (!m_Chunks.empty() || m_FinishedRead)) {
        resp.Send(m_Chunks, m_FinishedRead);
        m_Chunks.clear();
    }

/*
            void *dest = m_buf.Reserve(Size);
            if (resp.IsReady()) {

                size_t sz = m_len;
                if (sz > sizeof(m_buf))
                    sz = sizeof(m_buf);
                bool is_final = (m_len == sz);
                resp.SetOnReady(nullptr);
                resp.Send(m_buf, sz, true, is_final);
                m_len -= sz;
                if (!is_final) {
                    if (!m_wake)
                        NCBI_THROW(CPubseqGatewayException, eNoWakeCallback,
                                   "WakeCB is not assigned");
                    m_wake();
                }
            }
            else {
                if (!m_wake)
                    NCBI_THROW(CPubseqGatewayException, eNoWakeCallback,
                               "WakeCB is not assigned");
                resp.SetOnReady(m_wake);
            }
*/

}
