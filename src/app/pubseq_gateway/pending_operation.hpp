#ifndef PENDING_OPERATION__HPP
#define PENDING_OPERATION__HPP

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

#include <string>
#include <memory>
using namespace std;

#include <corelib/request_ctx.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/blob_task/load_blob.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>
USING_IDBLOB_SCOPE;
USING_SCOPE(objects);

#include "http_server_transport.hpp"
#include "pubseq_gateway_utils.hpp"
#include "pubseq_gateway_types.hpp"
#include "pending_requests.hpp"


class CBlobChunkCallback;
class CBlobPropCallback;
class CGetBlobErrorCallback;


class CPendingOperation
{
public:
    CPendingOperation(const SBlobRequest &  blob_request,
                      size_t  initial_reply_chunks,
                      shared_ptr<CCassConnection>  conn,
                      unsigned int  timeout,
                      unsigned int  max_retries,
                      CRef<CRequestContext>  request_context);
    CPendingOperation(const SResolveRequest &  resolve_request,
                      size_t  initial_reply_chunks,
                      shared_ptr<CCassConnection>  conn,
                      unsigned int  timeout,
                      unsigned int  max_retries,
                      CRef<CRequestContext>  request_context);
    ~CPendingOperation();

    void Clear();
    void Start(HST::CHttpReply<CPendingOperation>& resp);
    void Peek(HST::CHttpReply<CPendingOperation>& resp, bool  need_wait);

    void Cancel(void)
    {
        m_Cancelled = true;
    }

    size_t  GetItemId(void)
    {
        return ++m_NextItemId;
    }

    void UpdateOverallStatus(CRequestStatus::ECode  status)
    {
        m_OverallStatus = max(status, m_OverallStatus);
    }

public:
    CPendingOperation(const CPendingOperation&) = delete;
    CPendingOperation& operator=(const CPendingOperation&) = delete;
    CPendingOperation(CPendingOperation&&) = default;
    CPendingOperation& operator=(CPendingOperation&&) = default;

private:
    // Internal structure to keep track of the blob retrieving
    struct SBlobFetchDetails
    {
        SBlobFetchDetails(const SBlobRequest &  blob_request) :
            m_BlobId(blob_request.m_BlobId),
            m_NeedBlobProp(blob_request.m_NeedBlobProp),
            m_NeedChunks(blob_request.m_NeedChunks),
            m_Optional(blob_request.m_Optional),
            m_BlobPropSent(false),
            m_BlobIdType(blob_request.GetBlobIdentificationType()),
            m_TotalSentBlobChunks(0), m_FinishedRead(false),
            m_IncludeDataFlags(blob_request.m_IncludeDataFlags),
            m_BlobPropItemId(0),
            m_BlobChunkItemId(0)
        {}

        SBlobFetchDetails() :
            m_NeedBlobProp(true),
            m_NeedChunks(true),
            m_Optional(false),
            m_BlobPropSent(false),
            m_BlobIdType(eBySatAndSatKey),
            m_TotalSentBlobChunks(0),
            m_FinishedRead(false),
            m_IncludeDataFlags(0),
            m_BlobPropItemId(0),
            m_BlobChunkItemId(0)
        {}

        bool IsBlobPropStage(void) const
        {
            // At the time of an error report it needs to be known to what the
            // error message is associated - to blob properties or to blob
            // chunks
            if (m_NeedChunks == false)
                return true;
            if (m_NeedBlobProp == false)
                return false;
            return !m_BlobPropSent;
        }

        size_t GetBlobPropItemId(CPendingOperation *  pending_op)
        {
            if (m_BlobPropItemId == 0)
                m_BlobPropItemId = pending_op->GetItemId();
            return m_BlobPropItemId;
        }

        size_t GetBlobChunkItemId(CPendingOperation *  pending_op)
        {
            if (m_BlobChunkItemId == 0)
                m_BlobChunkItemId = pending_op->GetItemId();
            return m_BlobChunkItemId;
        }

        SBlobId                             m_BlobId;

        bool                                m_NeedBlobProp;
        bool                                m_NeedChunks;
        bool                                m_Optional;
        bool                                m_BlobPropSent;

        EBlobIdentificationType             m_BlobIdType;
        int32_t                             m_TotalSentBlobChunks;
        bool                                m_FinishedRead;
        TServIncludeData                    m_IncludeDataFlags;

        unique_ptr<CCassBlobTaskLoadBlob>   m_Loader;

        private:
            size_t                          m_BlobPropItemId;
            size_t                          m_BlobChunkItemId;
    };

public:
    void PrepareBioseqMessage(size_t  item_id, const string &  msg,
                              CRequestStatus::ECode  status,
                              int  err_code, EDiagSev  severity);
    void PrepareBioseqData(size_t  item_id, const string &  content,
                           EOutputFormat  output_format);
    void PrepareBioseqCompletion(size_t  item_id, size_t  chunk_count);
    void PrepareBlobPropMessage(size_t  item_id, const string &  msg,
                                CRequestStatus::ECode  status,
                                int  err_code, EDiagSev  severity);
    void PrepareBlobPropMessage(
                            SBlobFetchDetails *  fetch_details,
                            const string &  msg,
                            CRequestStatus::ECode  status, int  err_code,
                            EDiagSev  severity);
    void PrepareBlobPropData(SBlobFetchDetails *  fetch_details,
                             const string &  content);
    void PrepareBlobData(SBlobFetchDetails *  fetch_details,
                         const unsigned char *  chunk_data,
                         unsigned int  data_size, int  chunk_no);
    void PrepareBlobPropCompletion(size_t  item_id, size_t  chunk_count);
    void PrepareBlobPropCompletion(
                            SBlobFetchDetails *  fetch_details);
    void PrepareBlobMessage(size_t  item_id, const SBlobId &  blob_id,
                            const string &  msg,
                            CRequestStatus::ECode  status, int  err_code,
                            EDiagSev  severity);
    void PrepareBlobMessage(SBlobFetchDetails *  fetch_details,
                            const string &  msg,
                            CRequestStatus::ECode  status, int  err_code,
                            EDiagSev  severity);
    void PrepareBlobCompletion(size_t  item_id, const SBlobId &  blob_id,
                               size_t  chunk_count);
    void PrepareBlobCompletion(SBlobFetchDetails *  fetch_details);
    void PrepareReplyMessage(const string &  msg,
                             CRequestStatus::ECode  status, int  err_code,
                             EDiagSev  severity);
    void PrepareReplyCompletion(size_t  chunk_count);

private:
    void x_ProcessResolveRequest(void);
    void x_StartMainBlobRequest(void);
    bool x_AllFinishedRead(void) const;
    void x_SendReplyCompletion(bool  forced = false);
    void x_SetRequestContext(void);
    void x_PrintRequestStop(int  status);
    bool x_SatToSatName(const SBlobRequest &  blob_request,
                        SBlobId &  blob_id);
    void x_SendUnknownServerSatelliteError(size_t  item_id,
                                           const string &  message,
                                           int  error_code);
    void x_SendBioseqInfo(const string &  protobuf_bioseq_info,
                          SBioseqInfo &  bioseq_info,
                          EOutputFormat  output_format);
    void x_Peek(HST::CHttpReply<CPendingOperation>& resp, bool  need_wait,
                unique_ptr<SBlobFetchDetails> &  fetch_details);

    bool x_UsePsgProtocol(void) const;

private:
    SBioseqKey x_ResolveInputSeqId(string &  err_msg);
    void x_ResolveInputSeqId(CSeq_id &  parsed_seq_id,
                             bool  seq_id_parsed_as_is,
                             const CTextseq_id *  text_seq_id,
                             SBioseqKey &  bioseq_key);
    SBioseqKey x_ResolveInputSeqIdPath1(CSeq_id &  parsed_seq_id,
                                        bool  seq_id_parsed_as_is,
                                        const CTextseq_id *  text_seq_id);
    SBioseqKey x_ResolveInputSeqIdPath2(const CSeq_id &  parsed_seq_id,
                                        const CTextseq_id *  text_seq_id);
    void x_ResolveInputSeqIdAsIs(CSeq_id &  parsed_seq_id,
                                 bool  seq_id_parsed_as_is,
                                 bool tried_as_fasta_content,
                                 SBioseqKey &  bioseq_key);
    bool x_GetEffectiveSeqIdType(const CSeq_id &  parsed_seq_id,
                                 int &  eff_seq_id_type);
    bool x_LookupCachedBioseqInfo(const string &  accession,
                                  int &  version,
                                  int &  seq_id_type,
                                  string &  bioseq_info_cache_data);
    bool x_LookupCachedCsi(const string &  seq_id,
                           int &  seq_id_type,
                           string &  csi_cache_data);
    bool x_LookupBlobPropCache(int  sat, int  sat_key,
                               int64_t &  last_modified,
                               CBlobRecord &  blob_record);
    bool x_ParseUrlSeqIdAsIs(CSeq_id &  seq_id);
    bool x_ParseUrlSeqIdAsFastaTypeAndContent(CSeq_id &  seq_id);

private:
    HST::CHttpReply<CPendingOperation> *    m_Reply;
    shared_ptr<CCassConnection>             m_Conn;
    unsigned int                            m_Timeout;
    unsigned int                            m_MaxRetries;
    CRequestStatus::ECode                   m_OverallStatus;

    // Incoming request. It could be by sat/sat_key or by seq_id/id_type
    bool                                    m_IsResolveRequest;
    SBlobRequest                            m_BlobRequest;
    SResolveRequest                         m_ResolveRequest;
    CTempString                             m_UrlSeqId;
    int                                     m_UrlSeqIdType;

    int32_t                                 m_TotalSentReplyChunks;
    bool                                    m_Cancelled;
    vector<h2o_iovec_t>                     m_Chunks;
    CRef<CRequestContext>                   m_RequestContext;

    // All blobs currently fetched
    unique_ptr<SBlobFetchDetails>           m_MainBlobFetchDetails;
    unique_ptr<SBlobFetchDetails>           m_Id2InfoFetchDetails;
    unique_ptr<SBlobFetchDetails>           m_OriginalBlobChunkFetch;
    vector<unique_ptr<SBlobFetchDetails>>   m_Id2ChunkFetchDetails;

    size_t                                  m_NextItemId;

    friend class CBlobChunkCallback;
    friend class CBlobPropCallback;
    friend class CGetBlobErrorCallback;
};



class CBlobChunkCallback
{
    public:
        CBlobChunkCallback(
                CPendingOperation *  pending_op,
                HST::CHttpReply<CPendingOperation> *  reply,
                CPendingOperation::SBlobFetchDetails *  fetch_details) :
            m_PendingOp(pending_op), m_Reply(reply),
            m_FetchDetails(fetch_details)
        {}

        void operator()(const unsigned char *  data,
                        unsigned int  size, int  chunk_no);

    private:
        CPendingOperation *                     m_PendingOp;
        HST::CHttpReply<CPendingOperation> *    m_Reply;
        CPendingOperation::SBlobFetchDetails *  m_FetchDetails;
};


class CBlobPropCallback
{
    public:
        CBlobPropCallback(
                CPendingOperation *  pending_op,
                HST::CHttpReply<CPendingOperation> *  reply,
                CPendingOperation::SBlobFetchDetails *  fetch_details) :
            m_PendingOp(pending_op), m_Reply(reply),
            m_FetchDetails(fetch_details),
            m_Id2InfoParsed(false), m_Id2InfoValid(false),
            m_Id2InfoSat(-1), m_Id2InfoShell(-1),
            m_Id2InfoInfo(-1), m_Id2InfoChunks(-1)
        {}

        void operator()(CBlobRecord const &  blob, bool is_found);

    private:
        void x_RequestOriginalBlobChunks(CBlobRecord const &  blob);
        void x_RequestID2BlobChunks(CBlobRecord const &  blob);
        void x_ParseId2Info(CBlobRecord const &  blob);
        void x_RequestId2SplitBlobs(const string &  sat_name);

    private:
        CPendingOperation *                     m_PendingOp;
        HST::CHttpReply<CPendingOperation> *    m_Reply;
        CPendingOperation::SBlobFetchDetails *  m_FetchDetails;

        // id2_info parsing support
        bool            m_Id2InfoParsed;
        bool            m_Id2InfoValid;
        int             m_Id2InfoSat;
        int             m_Id2InfoShell;
        int             m_Id2InfoInfo;
        int             m_Id2InfoChunks;
};


class CGetBlobErrorCallback
{
    public:
        CGetBlobErrorCallback(
                CPendingOperation *  pending_op,
                HST::CHttpReply<CPendingOperation> *  reply,
                CPendingOperation::SBlobFetchDetails *  fetch_details) :
            m_PendingOp(pending_op), m_Reply(reply),
            m_FetchDetails(fetch_details)
        {}

        void operator()(CRequestStatus::ECode  status,
                        int  code,
                        EDiagSev  severity,
                        const string &  message);

    private:
        CPendingOperation *                     m_PendingOp;
        HST::CHttpReply<CPendingOperation> *    m_Reply;
        CPendingOperation::SBlobFetchDetails *  m_FetchDetails;
};


#endif
