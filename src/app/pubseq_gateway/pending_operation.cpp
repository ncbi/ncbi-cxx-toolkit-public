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
        ERR_POST(Trace << "CPendingOperation::CPendingOperation: blob "
                 "requested by seq_id/seq_id_type: " <<
                 m_BlobRequest.m_SeqId << "." << m_BlobRequest.m_SeqIdType <<
                 ", this: " << this);
    } else {
        ERR_POST(Trace << "CPendingOperation::CPendingOperation: blob "
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
    _TRACE("CPendingOperation::CPendingOperation: resolution "
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
            ERR_POST(Trace << "CPendingOperation::~CPendingOperation: blob "
                     "requested by seq_id/seq_id_type: " <<
                     m_BlobRequest.m_SeqId << "." <<
                     m_BlobRequest.m_SeqIdType <<
                     ", this: " << this);
        } else {
            ERR_POST(Trace << "CPendingOperation::~CPendingOperation: blob "
                     "requested by sat/sat_key: " <<
                     m_BlobRequest.m_BlobId.m_Sat << "." <<
                     m_BlobRequest.m_BlobId.m_SatKey <<
                     ", this: " << this);
        }
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

    for (auto &  details: m_Id2ChunkFetchDetails) {
        if (details) {
            ERR_POST(Trace << "CPendingOperation::~CPendingOperation: "
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
            ERR_POST(Trace << "CPendingOperation::Clear: blob "
                     "requested by seq_id/seq_id_type: " <<
                     m_BlobRequest.m_SeqId << "." <<
                     m_BlobRequest.m_SeqIdType <<
                     ", this: " << this);
        } else {
            ERR_POST(Trace << "CPendingOperation::Clear: blob "
                     "requested by sat/sat_key: " <<
                     m_BlobRequest.m_BlobId.m_Sat << "." <<
                     m_BlobRequest.m_BlobId.m_SatKey <<
                     ", this: " << this);
        }
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

    for (auto &  details: m_Id2ChunkFetchDetails) {
        if (details) {
            ERR_POST(Trace << "CPendingOperation::Clear: "
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

    // There are two options here:
    // - this is a resolution request
    // - the blobs were requested by sat/sat_key
    // - the blob was requested by a seq_id/seq_id_type
    if (m_IsResolveRequest) {
        x_ProcessResolveRequest();
        return;
    }

    if (m_BlobRequest.GetBlobIdentificationType() == eBySeqId) {
        // By seq_id -> first part is synchronous

        SBioseqInfo     bioseq_info;
        string          si2csi_cache_data;

        // retrieve from SI2CSI
        CRequestStatus::ECode   status = x_ResolveToCanonicalSeqId(
                                                            bioseq_info,
                                                            si2csi_cache_data);
        if (status != CRequestStatus::e200_Ok) {
            if (status != CRequestStatus::e400_BadRequest)
                UpdateOverallStatus(status);
            x_SendReplyCompletion(true);
            resp.Send(m_Chunks, true);
            m_Chunks.clear();
            return;
        }

        // retrieve from BIOSEQ_INFO
        string      bioseq_info_cache_data;
        status = x_FetchBioseqInfo(bioseq_info, si2csi_cache_data,
                                   bioseq_info_cache_data);
        if (status != CRequestStatus::e200_Ok) {
            UpdateOverallStatus(status);
            x_SendReplyCompletion(true);
            resp.Send(m_Chunks, true);
            m_Chunks.clear();
            return;
        }

        if (!bioseq_info_cache_data.empty())
            ConvertBioseqProtobufToBioseqInfo(bioseq_info_cache_data,
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
    SBioseqInfo     bioseq_info;
    string          si2csi_cache_data;

    // retrieve from SI2CSI
    CRequestStatus::ECode   status = x_ResolveToCanonicalSeqId(bioseq_info,
                                                               si2csi_cache_data);
    if (status != CRequestStatus::e200_Ok) {
        if (status != CRequestStatus::e400_BadRequest)
            UpdateOverallStatus(status);
        if (x_UsePsgProtocol()) {
            x_SendReplyCompletion(true);
            m_Reply->Send(m_Chunks, true);
            m_Chunks.clear();
        } else {
            x_PrintRequestStop(m_OverallStatus);
        }
        return;
    }

    if ((m_ResolveRequest.m_IncludeDataFlags &
            EServIncludeData::fServBioseqFields) ==
                                    EServIncludeData::fServCanonicalId) {
        // Only canonical ID was requested and it is already here, so send it
        // away
        x_SendCSIAsBioseqInfo(si2csi_cache_data, bioseq_info,
                              m_ResolveRequest.m_OutputFormat,
                              m_ResolveRequest.m_UsePsgProtocol);
        if (x_UsePsgProtocol()) {
            x_SendReplyCompletion(true);
            m_Reply->Send(m_Chunks, true);
            m_Chunks.clear();
        } else {
            x_PrintRequestStop(m_OverallStatus);
        }
        return;
    }


    // retrieve from BIOSEQ_INFO
    string      bioseq_info_cache_data;
    status = x_FetchBioseqInfo(bioseq_info, si2csi_cache_data,
                               bioseq_info_cache_data);
    if (status != CRequestStatus::e200_Ok) {
        UpdateOverallStatus(status);
        if (x_UsePsgProtocol()) {
            x_SendReplyCompletion(true);
            m_Reply->Send(m_Chunks, true);
            m_Chunks.clear();
        } else {
            x_PrintRequestStop(m_OverallStatus);
        }
        return;
    }

    // Send BIOSEQ_INFO
    x_SendBioseqInfo(bioseq_info_cache_data,
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

        ERR_POST(error);

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


CRequestStatus::ECode
CPendingOperation::x_ResolveToCanonicalSeqId(SBioseqInfo &  bioseq_info,
                                             string &  cache_data)
{
    CTempString seq_id = m_BlobRequest.m_SeqId;
    int         seq_id_type = m_BlobRequest.m_SeqIdType;
    bool        seq_id_type_provided = m_BlobRequest.m_SeqIdTypeProvided;
    if (m_IsResolveRequest) {
        seq_id = m_ResolveRequest.m_SeqId;
        seq_id_type = m_ResolveRequest.m_SeqIdType;
        seq_id_type_provided = m_ResolveRequest.m_SeqIdTypeProvided;
    }

    // First, try to avoid going to the DB
    try {
        CSeq_id                 parsed_seq_id(seq_id);
        const CTextseq_id *     text_seq_id = parsed_seq_id.GetTextseq_Id();

        if (text_seq_id != NULL) {
            bioseq_info.m_Accession = text_seq_id->GetAccession();

            CSeq_id_Base::E_Choice      parsed_seq_id_type = parsed_seq_id.Which();
            if (parsed_seq_id_type == CSeq_id_Base::e_not_set) {
                if (seq_id_type_provided)
                    bioseq_info.m_SeqIdType = seq_id_type;
                else
                    // Signal that seq_id_type is unknown
                    bioseq_info.m_SeqIdType = -1;
            } else {
                if (seq_id_type_provided) {
                    // Check that the seq_id_type matches with the URL provided
                    if (seq_id_type != parsed_seq_id_type) {
                        string  msg =
                            "Non matching 'seq_id_type' values. "
                            "The CSeq_id class parses it as " +
                            NStr::NumericToString(int(parsed_seq_id_type)) +
                            " while the URL request has it set to " +
                            NStr::NumericToString(seq_id_type) + ".";
                        if (x_UsePsgProtocol())
                            PrepareReplyMessage(msg,
                                                CRequestStatus::e400_BadRequest,
                                                eMalformedParameter,
                                                eDiag_Error);
                        else
                            m_Reply->Send400("Bad Request", msg.c_str());
                        return CRequestStatus::e400_BadRequest;
                    }
                }

                bioseq_info.m_SeqIdType = parsed_seq_id_type;
            }

            if (text_seq_id->CanGetVersion()) {
                bioseq_info.m_Version = text_seq_id->GetVersion();
            } else {
                // Signal that the version is unknown
                bioseq_info.m_Version = -1;
            }

            return CRequestStatus::e200_Ok;
        }
    } catch (const exception &  exc) {
        // Choke the exceptions and fallback to the cache or DB
    } catch (...) {
        // Choke the exceptions and fallback to the cache or DB
    }

    // Could not resolve using the CSeq_id class, so try cache and DB
    CRequestStatus::ECode status = x_CSICacheOrDB(seq_id, seq_id_type,
                                                  seq_id_type_provided,
                                                  cache_data,
                                                  bioseq_info);
    if (status == CRequestStatus::e200_Ok && !cache_data.empty()) {
        // Unpack cache data: accession, version, seq_id_type
        psg::retrieval::si2csi_value    cache_values;

        CPubseqGatewayData::UnpackSiInfo(cache_data, cache_values);
        bioseq_info.m_Accession = cache_values.accession();
        bioseq_info.m_Version = cache_values.version();
        bioseq_info.m_SeqIdType = cache_values.seq_id_type();
    }

    return status;
}


CRequestStatus::ECode
CPendingOperation::x_FetchCanonicalSeqId(SBioseqInfo &  bioseq_info)
{
    CRequestStatus::ECode   status = CRequestStatus::e200_Ok;
    string                  msg;

    CTempString seq_id = m_BlobRequest.m_SeqId;
    int         seq_id_type = m_BlobRequest.m_SeqIdType;
    bool        seq_id_type_provided = m_BlobRequest.m_SeqIdTypeProvided;
    if (m_IsResolveRequest) {
        seq_id = m_ResolveRequest.m_SeqId;
        seq_id_type = m_ResolveRequest.m_SeqIdType;
        seq_id_type_provided = m_ResolveRequest.m_SeqIdTypeProvided;
    }

    try {
        status = FetchCanonicalSeqId(
                        m_Conn,
                        CPubseqGatewayApp::GetInstance()->GetBioseqKeyspace(),
                        seq_id, seq_id_type, seq_id_type_provided,
                        bioseq_info.m_Accession,
                        bioseq_info.m_Version,
                        bioseq_info.m_SeqIdType);
        if (status == CRequestStatus::e404_NotFound) {
            msg = "No translation of the seq_id '" +
                  m_BlobRequest.m_SeqId + "' to canonical seq_id found";
        } else if (status == CRequestStatus::e300_MultipleChoices) {
            msg = "More than one translation of the seq_id '" +
                  m_BlobRequest.m_SeqId + "' to canonical seq_id found";
        }
    } catch (const exception &  exc) {
        msg = "Error translating seq_id '" + seq_id +
              "'  to canonical seq_id: " + string(exc.what());
        status = CRequestStatus::e500_InternalServerError;
    } catch (...) {
        msg = "Unknown error translating seq_id '" + seq_id +
              "' to canonical seq_id";
        status = CRequestStatus::e500_InternalServerError;
    }

    if (status != CRequestStatus::e200_Ok) {
        if (status == CRequestStatus::e404_NotFound) {
            // It is a client error, no data for the request
            ERR_POST(Warning << msg);
        } else if (status == CRequestStatus::e300_MultipleChoices) {
            // It is a client error, too broad request
            ERR_POST(Warning << msg);
            status = CRequestStatus::e404_NotFound; // CXX-10046
        } else {
            // It is a server error, data inconsistency or DB connectivity
            ERR_POST(msg);
            CPubseqGatewayApp::GetInstance()->GetErrorCounters().
                                                     IncCanonicalSeqIdError();
        }

        if (x_UsePsgProtocol()) {
            size_t      item_id = GetItemId();
            PrepareBioseqMessage(item_id, msg, status,
                                 eNoCanonicalTranslation, eDiag_Error);
            PrepareBioseqCompletion(item_id, 2);
        } else {
            // Send it as an HTTP reply (not PSG protocol)
            switch (status) {
                case CRequestStatus::e300_MultipleChoices:
                case CRequestStatus::e404_NotFound:
                    m_Reply->Send404("Not Found", msg.c_str());
                    break;
                case CRequestStatus::e500_InternalServerError:
                    m_Reply->Send500("Internal Server Error", msg.c_str());
                    break;
                default:
                    m_Reply->Send404("Not Found", msg.c_str());
            }
        }
    }

    return status;
}


CRequestStatus::ECode
CPendingOperation::x_FetchBioseqInfo(SBioseqInfo &  bioseq_info,
                                     const string &  si2csi_cache_data,
                                     string &  bioseq_info_cache_data)
{
    // Try cache first
    CPubseqGatewayCache *   cache = CPubseqGatewayApp::GetInstance()->
                                                        GetLookupCache();
    if (!si2csi_cache_data.empty()) {
        psg::retrieval::si2csi_value        protobuf_si2csi_value;
        CPubseqGatewayData::UnpackSiInfo(si2csi_cache_data,
                                         protobuf_si2csi_value);
        bioseq_info.m_Accession = protobuf_si2csi_value.accession();
        bioseq_info.m_Version = protobuf_si2csi_value.version();
        bioseq_info.m_SeqIdType = protobuf_si2csi_value.seq_id_type();
    }


    bool        cache_hit = false;
    if (bioseq_info.m_Version >= 0) {
        if (bioseq_info.m_SeqIdType >= 0)
            cache_hit = cache->LookupBioseqInfoByAccessionVersionSeqIdType(
                                        bioseq_info.m_Accession,
                                        bioseq_info.m_Version,
                                        bioseq_info.m_SeqIdType,
                                        bioseq_info_cache_data);
        else
            cache_hit = cache->LookupBioseqInfoByAccessionVersion(
                                        bioseq_info.m_Accession,
                                        bioseq_info.m_Version,
                                        bioseq_info.m_SeqIdType,
                                        bioseq_info_cache_data);
    } else {
        if (bioseq_info.m_SeqIdType >= 0)
            cache_hit = cache->LookupBioseqInfoByAccessionVersionSeqIdType(
                                        bioseq_info.m_Accession,
                                        0,
                                        bioseq_info.m_SeqIdType,
                                        bioseq_info_cache_data);
        else
            cache_hit = cache->LookupBioseqInfoByAccession(
                                        bioseq_info.m_Accession,
                                        bioseq_info.m_Version,
                                        bioseq_info.m_SeqIdType,
                                        bioseq_info_cache_data);
    }

    if (cache_hit) {
        CPubseqGatewayApp::GetInstance()->GetCacheCounters().
                                                IncBioseqInfoCacheHit();
        return CRequestStatus::e200_Ok;
    }

    CPubseqGatewayApp::GetInstance()->GetCacheCounters().
                                                IncBioseqInfoCacheMiss();

    // Fallback to Cassandra
    CRequestStatus::ECode   status = CRequestStatus::e200_Ok;
    string                  msg;
    try {
        // -1 means not provided (or not retrieved from si2csi)
        status = FetchBioseqInfo(
                        m_Conn,
                        CPubseqGatewayApp::GetInstance()->GetBioseqKeyspace(),
                        bioseq_info.m_Version != -1,
                        bioseq_info.m_SeqIdType != -1,
                        bioseq_info);
        if (status == CRequestStatus::e404_NotFound) {
            string      seq_id = m_BlobRequest.m_SeqId;
            if (m_IsResolveRequest)
                seq_id = m_ResolveRequest.m_SeqId;
            msg = "No bioseq info found for seq_id '" + seq_id + "'";
            status = CRequestStatus::e500_InternalServerError;
        } else if (status == CRequestStatus::e300_MultipleChoices) {
            string      seq_id = m_BlobRequest.m_SeqId;
            if (m_IsResolveRequest)
                seq_id = m_ResolveRequest.m_SeqId;
            msg = "More than one bioseq info found for seq_id '" + seq_id + "'";
            status = CRequestStatus::e500_InternalServerError;
        }
    } catch (const exception &  exc) {
        string      seq_id = m_BlobRequest.m_SeqId;
        if (m_IsResolveRequest)
            seq_id = m_ResolveRequest.m_SeqId;
        msg = "Error retrieving bioseq info for seq_id '" + seq_id +
              "': " + string(exc.what());
        status = CRequestStatus::e500_InternalServerError;
    } catch (...) {
        string      seq_id = m_BlobRequest.m_SeqId;
        if (m_IsResolveRequest)
            seq_id = m_ResolveRequest.m_SeqId;
        msg = "Unknown error retrieving bioseq info for seq_id '" + seq_id + "'";
        status = CRequestStatus::e500_InternalServerError;
    }

    if (status != CRequestStatus::e200_Ok) {

        CPubseqGatewayApp::GetInstance()->GetErrorCounters().
                                                IncBioseqInfoError();
        ERR_POST(msg);  // It is always a server error: data inconsistency

        if (x_UsePsgProtocol()) {
            size_t      item_id = GetItemId();
            PrepareBioseqMessage(item_id, msg, status,
                                 eNoBioseqInfo, eDiag_Error);
            PrepareBioseqCompletion(item_id, 2);
        } else {
            // Send it as an HTTP reply (not PSG protocol)
            switch (status) {
                case CRequestStatus::e400_BadRequest:
                    m_Reply->Send400("Bad Request", msg.c_str());
                    break;
                case CRequestStatus::e404_NotFound:
                    m_Reply->Send404("Not Found", msg.c_str());
                    break;
                case CRequestStatus::e500_InternalServerError:
                    m_Reply->Send500("Internal Server Error", msg.c_str());
                    break;
                default:
                    m_Reply->Send404("Not Found", msg.c_str());
            }
        }
    }

    // No problems, biseq info has been retrieved
    return status;
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
        PrepareBioseqData(item_id, data_to_send);
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


void CPendingOperation::x_SendCSIAsBioseqInfo(string &  si2csi_cache_data,
                                              SBioseqInfo &  bioseq_info,
                                              EOutputFormat  output_format,
                                              bool  use_psg_protocol)
{
    EOutputFormat       effective_output_format = output_format;

    if (output_format == eNativeFormat || output_format == eUnknownFormat) {
        if (si2csi_cache_data.empty())
            effective_output_format = eJsonFormat;
        else
            effective_output_format = eProtobufFormat;
    }

    string              data_to_send;

    if (effective_output_format == eJsonFormat) {
        if (!si2csi_cache_data.empty())
            ConvertSi2csiProtobufToBioseqJson(si2csi_cache_data, data_to_send);
        else
            ConvertSi2csiToBioseqJson(bioseq_info, data_to_send);
    } else {
        if (!si2csi_cache_data.empty())
            ConvertSi2csiProtobufToBioseqProtobuf(si2csi_cache_data,
                                                  data_to_send);
        else
            ConvertSi2sciToBioseqProtobuf(bioseq_info, data_to_send);
    }

    if (use_psg_protocol) {
        size_t              item_id = GetItemId();
        PrepareBioseqData(item_id, data_to_send);
        PrepareBioseqCompletion(item_id, 2);
    } else {
        // Send as HTTP data
        if (effective_output_format == eJsonFormat)
            m_Reply->SetJsonContentType();
        else
            m_Reply->SetProtobufContentType();
        m_Reply->SetContentLength(data_to_send.length());
        m_Reply->SendOk(data_to_send.data(), data_to_send.length(), false);
    }
}


CRequestStatus::ECode
CPendingOperation::x_CSICacheOrDB(const CTempString &  sec_seq_id,
                                  int  sec_seq_id_type,
                                  bool  sec_seq_id_type_provided,
                                  string &  csi_cache_data,
                                  SBioseqInfo &  bioseq_info)
{
    // Try cache first
    CPubseqGatewayCache *   cache = CPubseqGatewayApp::GetInstance()->
                                                        GetLookupCache();
    bool                    cache_hit = false;
    if (sec_seq_id_type_provided) {
        cache_hit = cache->LookupCsiBySeqIdSeqIdType(sec_seq_id,
                                                     sec_seq_id_type,
                                                     csi_cache_data);
    } else {
        int     cache_sec_seq_id_type = -1;     // Not used
        cache_hit = cache->LookupCsiBySeqId(sec_seq_id, cache_sec_seq_id_type,
                                            csi_cache_data);
    }

    if (cache_hit) {
        CPubseqGatewayApp::GetInstance()->GetCacheCounters().
                                                IncSi2csiCacheHit();
        return CRequestStatus::e200_Ok;
    }

    // Fallback to DB
    CPubseqGatewayApp::GetInstance()->GetCacheCounters().
                                                IncSi2csiCacheMiss();
    return x_FetchCanonicalSeqId(bioseq_info);
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
    ERR_POST(msg);

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
                                          const string &  content)
{
    string      header = GetBioseqInfoHeader(item_id, content.size());
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
        ERR_POST("Unexpected data received "
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
            ERR_POST(Warning << message);
            m_PendingOp->UpdateOverallStatus(CRequestStatus::e404_NotFound);
            m_PendingOp->PrepareBlobPropMessage(
                                    m_FetchDetails, message,
                                    CRequestStatus::e404_NotFound,
                                    eBlobPropsNotFound, eDiag_Error);
        } else {
            // Server error, data inconsistency
            ERR_POST(message);
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
        ERR_POST(message);
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
        ERR_POST(message);
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
        ERR_POST(message);
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
        ERR_POST(validate_message);
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
    ERR_POST(message);
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
