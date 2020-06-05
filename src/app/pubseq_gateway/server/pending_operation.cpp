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
#include "pubseq_gateway_logging.hpp"
#include "getblob_processor.hpp"
#include "tse_chunk_processor.hpp"
#include "resolve_processor.hpp"
#include "annot_processor.hpp"
#include "get_processor.hpp"

#include <objtools/pubseq_gateway/protobuf/psg_protobuf.pb.h>
USING_IDBLOB_SCOPE;



CPendingOperation::CPendingOperation(unique_ptr<CPSGS_Request>  user_request,
                                     size_t  initial_reply_chunks) :
    m_UserRequest(move(user_request)),
    m_Reply(new CPSGS_Reply(initial_reply_chunks)),
    m_Cancelled(false)
{
    switch (m_UserRequest->GetRequestType()) {
        case CPSGS_Request::ePSGS_ResolveRequest:
            PSG_TRACE("CPendingOperation::CPendingOperation: resolution "
                      "request by seq_id/seq_id_type: " <<
                      m_UserRequest->GetRequest<SPSGS_ResolveRequest>().m_SeqId << "." <<
                      m_UserRequest->GetRequest<SPSGS_ResolveRequest>().m_SeqIdType <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
            PSG_TRACE("CPendingOperation::CPendingOperation: blob "
                      "requested by seq_id/seq_id_type: " <<
                      m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_SeqId << "." <<
                      m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_SeqIdType <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_BlobBySatSatKeyRequest:
            PSG_TRACE("CPendingOperation::CPendingOperation: blob "
                      "requested by sat/sat_key: " <<
                      m_UserRequest->GetRequest<SPSGS_BlobBySatSatKeyRequest>().m_BlobId.ToString() <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_AnnotationRequest:
            PSG_TRACE("CPendingOperation::CPendingOperation: annotation "
                      "request by seq_id/seq_id_type: " <<
                      m_UserRequest->GetRequest<SPSGS_AnnotRequest>().m_SeqId << "." <<
                      m_UserRequest->GetRequest<SPSGS_AnnotRequest>().m_SeqIdType <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_TSEChunkRequest:
            PSG_TRACE("CPendingOperation::CPendingOperation: TSE chunk "
                      "request by sat/sat_key: " <<
                      m_UserRequest->GetRequest<SPSGS_TSEChunkRequest>().m_TSEId.ToString() <<
                      ", this: " << this);
            break;
        default:
            ;
    }
}


CPendingOperation::~CPendingOperation()
{
    CRequestContextResetter     context_resetter;
    m_UserRequest->SetRequestContext();

    switch (m_UserRequest->GetRequestType()) {
        case CPSGS_Request::ePSGS_ResolveRequest:
            PSG_TRACE("CPendingOperation::~CPendingOperation: resolve "
                      "requested by seq_id/seq_id_type: " <<
                      m_UserRequest->GetRequest<SPSGS_ResolveRequest>().m_SeqId << "." <<
                      m_UserRequest->GetRequest<SPSGS_ResolveRequest>().m_SeqIdType <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
            PSG_TRACE("CPendingOperation::~CPendingOperation: blob "
                      "requested by seq_id/seq_id_type: " <<
                      m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_SeqId << "." <<
                      m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_SeqIdType <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_BlobBySatSatKeyRequest:
            PSG_TRACE("CPendingOperation::~CPendingOperation: blob "
                      "requested by sat/sat_key: " <<
                      m_UserRequest->GetRequest<SPSGS_BlobBySatSatKeyRequest>().m_BlobId.ToString() <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_AnnotationRequest:
            PSG_TRACE("CPendingOperation::~CPendingOperation: annotation "
                      "request by seq_id/seq_id_type: " <<
                      m_UserRequest->GetRequest<SPSGS_AnnotRequest>().m_SeqId << "." <<
                      m_UserRequest->GetRequest<SPSGS_AnnotRequest>().m_SeqIdType <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_TSEChunkRequest:
            PSG_TRACE("CPendingOperation::~CPendingOperation: TSE chunk "
                      "request by sat/sat_key: " <<
                      m_UserRequest->GetRequest<SPSGS_TSEChunkRequest>().m_TSEId.ToString() <<
                      ", this: " << this);
            break;
        default:
            ;
    }

    // Just in case if a request ended without a normal request stop,
    // finish it here as the last resort.
    x_PrintRequestStop();
}


void CPendingOperation::Clear()
{
    CRequestContextResetter     context_resetter;
    m_UserRequest->SetRequestContext();

    switch (m_UserRequest->GetRequestType()) {
        case CPSGS_Request::ePSGS_ResolveRequest:
            PSG_TRACE("CPendingOperation::Clear(): resolve "
                      "requested by seq_id/seq_id_type: " <<
                      m_UserRequest->GetRequest<SPSGS_ResolveRequest>().m_SeqId << "." <<
                      m_UserRequest->GetRequest<SPSGS_ResolveRequest>().m_SeqIdType <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
            PSG_TRACE("CPendingOperation::Clear(): blob "
                      "requested by seq_id/seq_id_type: " <<
                      m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_SeqId << "." <<
                      m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_SeqIdType <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_BlobBySatSatKeyRequest:
            PSG_TRACE("CPendingOperation::Clear(): blob "
                      "requested by sat/sat_key: " <<
                      m_UserRequest->GetRequest<SPSGS_BlobBySatSatKeyRequest>().m_BlobId.ToString() <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_AnnotationRequest:
            PSG_TRACE("CPendingOperation::Clear(): annotation "
                      "request by seq_id/seq_id_type: " <<
                      m_UserRequest->GetRequest<SPSGS_AnnotRequest>().m_SeqId << "." <<
                      m_UserRequest->GetRequest<SPSGS_AnnotRequest>().m_SeqIdType <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_TSEChunkRequest:
            PSG_TRACE("CPendingOperation::Clear(): TSE chunk "
                      "request by sat/sat_key: " <<
                      m_UserRequest->GetRequest<SPSGS_TSEChunkRequest>().m_TSEId.ToString() <<
                      ", this: " << this);
            break;
        default:
            ;
    }

    m_Reply->Clear();
    m_Cancelled = false;
}


void CPendingOperation::Start(HST::CHttpReply<CPendingOperation>& resp)
{
    m_Reply->SetReply(&resp);
    auto    request_type = m_UserRequest->GetRequestType();
    switch (request_type) {
        case CPSGS_Request::ePSGS_ResolveRequest:
            m_Processor.reset(new CPSGS_ResolveProcessor(m_UserRequest, m_Reply));
            m_Processor->Process();
            break;
        case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
            m_Processor.reset(new CPSGS_GetProcessor(m_UserRequest, m_Reply));
            m_Processor->Process();
            break;
        case CPSGS_Request::ePSGS_BlobBySatSatKeyRequest:
            m_Processor.reset(new CPSGS_GetBlobProcessor(m_UserRequest, m_Reply));
            m_Processor->Process();
            break;
        case CPSGS_Request::ePSGS_AnnotationRequest:
            m_Processor.reset(new CPSGS_AnnotProcessor(m_UserRequest, m_Reply));
            m_Processor->Process();
            break;
        case CPSGS_Request::ePSGS_TSEChunkRequest:
            m_Processor.reset(new CPSGS_TSEChunkProcessor(m_UserRequest, m_Reply));
            m_Processor->Process();
            break;
        default:
            NCBI_THROW(CPubseqGatewayException, eLogic,
                       "Unhandeled request type " +
                       to_string(static_cast<int>(request_type)));
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

    m_Processor->ProcessEvent();

    if (m_Processor->IsFinished() && !resp.IsFinished() && resp.IsOutputReady() ) {
        m_Reply->PrepareReplyCompletion();
        m_Reply->Flush(true);

        x_PrintRequestStop();
    }
}


void CPendingOperation::x_PrintRequestStop(void)
{
    if (m_UserRequest->GetRequestContext().NotNull()) {
        CDiagContext::SetRequestContext(m_UserRequest->GetRequestContext());
        m_UserRequest->GetRequestContext()->SetReadOnly(false);
        m_UserRequest->GetRequestContext()->SetRequestStatus(m_UserRequest->GetOverallStatus());
        GetDiagContext().PrintRequestStop();
        m_UserRequest->GetRequestContext().Reset();
        CDiagContext::SetRequestContext(NULL);
    }
}

