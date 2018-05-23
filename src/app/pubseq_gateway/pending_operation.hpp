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


// The operation context passed to the cassandra wrapper.
// For the time being it is just a blob identification but later it can be
// extended without changing the interface between the server and the cassandra
// wrappers.
struct SOperationContext
{
    SOperationContext(const SBlobId &  blob_id) :
        m_BlobId(blob_id)
    {}

    SBlobId     m_BlobId;
};


// The user may come with an accession or with a pair sat/sat key
// The requests are counted separately so the enumeration distinguish them.
enum EBlobIdentificationType {
    eByAccession,
    eBySatAndSatKey
};


// In case one many requested blobs a container of items is required.
// Each item describes one requested blob. The structure is used for that.
struct SBlobRequest
{
    SBlobRequest(const SBlobId &  blob_id,
                 const string &  last_modified,
                 EBlobIdentificationType  id_type) :
        m_BlobId(blob_id), m_LastModified(last_modified), m_IdType(id_type)
    {}

    SBlobId                     m_BlobId;
    string                      m_LastModified;
    EBlobIdentificationType     m_IdType;
};


class CPendingOperation
{
public:
    CPendingOperation(const SBlobRequest &  blob_request,
                      size_t  initial_reply_chunks,
                      shared_ptr<CCassConnection>  conn,
                      unsigned int  timeout,
                      unsigned int  max_retries,
                      CRef<CRequestContext>  request_context);
    CPendingOperation(const vector<SBlobRequest> &  blob_requests,
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

public:
    CPendingOperation(const CPendingOperation&) = delete;
    CPendingOperation& operator=(const CPendingOperation&) = delete;
    CPendingOperation(CPendingOperation&&) = default;
    CPendingOperation& operator=(CPendingOperation&&) = default;

private:
    // Internal structure to keep track of the blob retrieving
    struct SBlobRequestDetails
    {
        SBlobRequestDetails(const SBlobRequest &  blob_request) :
            m_BlobIdType(blob_request.m_IdType),
            m_TotalSentBlobChunks(0), m_FinishedRead(false)
        {}

        EBlobIdentificationType             m_BlobIdType;
        int32_t                             m_TotalSentBlobChunks;
        bool                                m_FinishedRead;

        unique_ptr<SOperationContext>       m_Context;
        unique_ptr<CCassBlobTaskLoadBlob>   m_Loader;
    };

    bool x_AllFinishedRead(void) const;
    void x_SendReplyCompletion(void);
    void x_SetRequestContext(void);
    void x_PrintRequestStop(int  status);

private:
    HST::CHttpReply<CPendingOperation> *    m_Reply;

    int32_t                                 m_TotalSentReplyChunks;
    bool                                    m_Cancelled;
    vector<h2o_iovec_t>                     m_Chunks;
    CRef<CRequestContext>                   m_RequestContext;

    // Storage for all the blob requests
    map<SBlobId,
        unique_ptr<SBlobRequestDetails>>    m_Requests;
};


#endif
