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

#include <objtools/pubseq_gateway/impl/cassandra/blob_task/load_blob.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>
USING_IDBLOB_SCOPE;

#include "http_server_transport.hpp"
#include "pubseq_gateway_utils.hpp"


class CBlobChunkCallback;
class CBlobPropCallback;
class CGetBlobErrorCallback;


// Bit-set of CPendingOperation::EServIncludeData flags
typedef int TServIncludeData;


// The user may come with an accession or with a pair sat/sat key
// The requests are counted separately so the enumeration distinguish them.
enum EBlobIdentificationType {
    eBySeqId,
    eBySatAndSatKey
};


// In case one many requested blobs a container of items is required.
// Each item describes one requested blob. The structure is used for that.
struct SBlobRequest
{
    // Construct the request for the case of sat/sat_key request
    SBlobRequest(const SBlobId &  blob_id,
                 const string &  last_modified) :
        m_NeedBlobProp(true),
        m_NeedChunks(true),
        m_Optional(false),
        m_ItemId(0),
        m_BlobIdType(eBySatAndSatKey),
        m_BlobId(blob_id),
        m_LastModified(last_modified)
    {}

    // Construct the request for the case of seq_id/id_type request
    SBlobRequest(const string &  seq_id,
                 int  id_type,
                 TServIncludeData  include_data_flags) :
        m_NeedBlobProp(true),
        m_NeedChunks(false),
        m_Optional(false),
        m_ItemId(0),
        m_BlobIdType(eBySeqId),
        m_SeqId(seq_id), m_IdType(id_type),
        m_IncludeDataFlags(include_data_flags)
    {}

    EBlobIdentificationType  GetBlobIdentificationType(void) const
    {
        return m_BlobIdType;
    }

public:
    bool                        m_NeedBlobProp;
    bool                        m_NeedChunks;
    bool                        m_Optional;

    size_t                      m_ItemId;
    EBlobIdentificationType     m_BlobIdType;

    // Fields in case of request by sat/sat_key
    SBlobId                     m_BlobId;
    string                      m_LastModified;

    // Fields in case of request by seq_id/id_type
    string                      m_SeqId;
    int                         m_IdType;
    TServIncludeData            m_IncludeDataFlags;
};



class CPendingOperation
{
public:
    // Pretty much copied from the client; the justfication for copying is:
    // "it will be decoupled with the client type"
    enum EServIncludeData {
        fServNoTSE = (1 << 0),
        fServFastInfo = (1 << 1),
        fServWholeTSE = (1 << 2),
        fServOrigTSE = (1 << 3),
        fServCanonicalId = (1 << 4),
        fServOtherIds = (1 << 5),
        fServMoleculeType = (1 << 6),
        fServLength = (1 << 7),
        fServState = (1 << 8),
        fServBlobId = (1 << 9),
        fServTaxId = (1 << 10),
        fServHash = (1 << 11)
    };

public:
    CPendingOperation(const SBlobRequest &  blob_request,
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
            m_BlobIdType(blob_request.GetBlobIdentificationType()),
            m_TotalSentBlobChunks(0), m_FinishedRead(false),
            m_ItemId(blob_request.m_ItemId),
            m_IncludeDataFlags(blob_request.m_IncludeDataFlags)
        {}

        SBlobFetchDetails() :
            m_NeedBlobProp(true),
            m_NeedChunks(true),
            m_Optional(false),
            m_BlobIdType(eBySatAndSatKey),
            m_TotalSentBlobChunks(0),
            m_FinishedRead(false),
            m_ItemId(0),
            m_IncludeDataFlags(0)
        {}

        SBlobId                             m_BlobId;

        bool                                m_NeedBlobProp;
        bool                                m_NeedChunks;
        bool                                m_Optional;

        EBlobIdentificationType             m_BlobIdType;
        int32_t                             m_TotalSentBlobChunks;
        bool                                m_FinishedRead;
        size_t                              m_ItemId;
        TServIncludeData                    m_IncludeDataFlags;

        unique_ptr<CCassBlobTaskLoadBlob>   m_Loader;
    };

    void x_StartMainBlobRequest(void);
    bool x_AllFinishedRead(void) const;
    void x_SendReplyCompletion(void);
    void x_SetRequestContext(void);
    void x_PrintRequestStop(int  status);
    bool x_FetchCanonicalSeqId(SBioseqInfo &  bioseq_info);
    bool x_FetchBioseqInfo(SBioseqInfo &  bioseq_info);
    bool x_SatToSatName(const SBlobRequest &  blob_request,
                        SBlobId &  blob_id);
    void x_SendUnknownSatelliteError(size_t  item_id,
                                     const string &  message, int  error_code);
    void x_SendBioseqInfo(size_t  item_id, const SBioseqInfo &  bioseq_info);

    void x_Peek(HST::CHttpReply<CPendingOperation>& resp, bool  need_wait,
                unique_ptr<SBlobFetchDetails> &  fetch_details);
private:
    HST::CHttpReply<CPendingOperation> *    m_Reply;
    shared_ptr<CCassConnection>             m_Conn;
    unsigned int                            m_Timeout;
    unsigned int                            m_MaxRetries;

    // Incoming request. It could be by sat/sat_key or by seq_id/id_type
    SBlobRequest                            m_BlobRequest;

    int32_t                                 m_TotalSentReplyChunks;
    bool                                    m_Cancelled;
    vector<h2o_iovec_t>                     m_Chunks;
    CRef<CRequestContext>                   m_RequestContext;

    // All blobs currently fetched
    unique_ptr<SBlobFetchDetails>           m_MainBlobFetchDetails;
    unique_ptr<SBlobFetchDetails>           m_Id2ShellFetchDetails;
    unique_ptr<SBlobFetchDetails>           m_Id2InfoFetchDetails;

    size_t                                  m_NextItemId;

    friend class CBlobChunkCallback;
    friend class CBlobPropCallback;
    friend class CGetBlobErrorCallback;
};



class CBlobChunkCallback
{
    public:
        CBlobChunkCallback(CPendingOperation *  pending_op,
                           HST::CHttpReply<CPendingOperation> *  reply,
                           CPendingOperation::SBlobFetchDetails *  fetch_details);

        void operator()(const unsigned char *  data,
                        unsigned int  size,
                        int  chunk_no);

    private:
        CPendingOperation *                     m_PendingOp;
        HST::CHttpReply<CPendingOperation> *    m_Reply;
        CPendingOperation::SBlobFetchDetails *  m_FetchDetails;
};


class CBlobPropCallback
{
    public:
        CBlobPropCallback(CPendingOperation *  pending_op,
                          HST::CHttpReply<CPendingOperation> *  reply,
                          CPendingOperation::SBlobFetchDetails *  fetch_details);

        void operator()(CBlobRecord const &  blob, bool is_found);

    private:
        void x_RequestOriginalBlobChunks(CBlobRecord const &  blob);
        void x_RequestID2BlobChunks(CBlobRecord const &  blob);
        void x_SendBlobPropCompletion(void);

    private:
        CPendingOperation *                     m_PendingOp;
        HST::CHttpReply<CPendingOperation> *    m_Reply;
        CPendingOperation::SBlobFetchDetails *  m_FetchDetails;
};


class CGetBlobErrorCallback
{
    public:
        CGetBlobErrorCallback(CPendingOperation *  pending_op,
                              HST::CHttpReply<CPendingOperation> *  reply,
                              CPendingOperation::SBlobFetchDetails *  fetch_details);

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
