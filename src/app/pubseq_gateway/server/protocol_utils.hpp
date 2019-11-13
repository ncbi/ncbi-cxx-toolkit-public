#ifndef PROTOCOL_UTILS__HPP
#define PROTOCOL_UTILS__HPP

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
 * Authors: Sergey Satskiy
 *
 * File Description:
 *
 */


#include "http_server_transport.hpp"
#include <corelib/request_status.hpp>


class CPendingOperation;
class CCassBlobFetch;
struct SBlobId;


// Keeps track of the protocol replies
class CProtocolUtils
{
public:
    CProtocolUtils(size_t  initial_reply_chunks) :
        m_Reply(nullptr),
        m_NextItemId(0),
        m_TotalSentReplyChunks(initial_reply_chunks)
    {}

public:
    void Flush(void);
    void Flush(bool  is_last);
    void Clear(void);

    void SetReply(HST::CHttpReply<CPendingOperation> *  reply)
    {
        m_Reply = reply;
    }

    HST::CHttpReply<CPendingOperation> *  GetReply(void)
    {
        return m_Reply;
    }

    bool IsReplyFinished(void) const
    {
        return m_Reply->IsFinished();
    }

    size_t  GetItemId(void)
    {
        return ++m_NextItemId;
    }

public:
    // PSG protocol facilities
    void PrepareBioseqMessage(size_t  item_id, const string &  msg,
                              CRequestStatus::ECode  status,
                              int  err_code, EDiagSev  severity);
    void PrepareBioseqData(size_t  item_id, const string &  content,
                           EOutputFormat  output_format);
    void PrepareBioseqCompletion(size_t  item_id, size_t  chunk_count);
    void PrepareBlobPropMessage(size_t  item_id, const string &  msg,
                                CRequestStatus::ECode  status,
                                int  err_code, EDiagSev  severity);
    void PrepareBlobPropMessage(CCassBlobFetch *  fetch_details,
                                const string &  msg,
                                CRequestStatus::ECode  status, int  err_code,
                                EDiagSev  severity);
    void PrepareBlobPropData(CCassBlobFetch *  fetch_details,
                             const string &  content);
    void PrepareBlobData(CCassBlobFetch *  fetch_details,
                         const unsigned char *  chunk_data,
                         unsigned int  data_size, int  chunk_no);
    void PrepareBlobPropCompletion(size_t  item_id, size_t  chunk_count);
    void PrepareBlobPropCompletion(CCassBlobFetch *  fetch_details);
    void PrepareBlobMessage(size_t  item_id, const SBlobId &  blob_id,
                            const string &  msg,
                            CRequestStatus::ECode  status, int  err_code,
                            EDiagSev  severity);
    void PrepareBlobMessage(CCassBlobFetch *  fetch_details,
                            const string &  msg,
                            CRequestStatus::ECode  status, int  err_code,
                            EDiagSev  severity);
    void PrepareBlobCompletion(size_t  item_id, const SBlobId &  blob_id,
                               size_t  chunk_count);
    void PrepareBlobExcluded(const SBlobId &  blob_id,
                             EBlobSkipReason  skip_reason);
    void PrepareBlobExcluded(size_t  item_id, const SBlobId &  blob_id,
                             EBlobSkipReason  skip_reason);
    void PrepareBlobCompletion(CCassBlobFetch *  fetch_details);
    void PrepareReplyMessage(const string &  msg,
                             CRequestStatus::ECode  status, int  err_code,
                             EDiagSev  severity);
    void PrepareNamedAnnotationData(const string &  annot_name,
                                    const string &  content);
    void PrepareReplyCompletion(void);

public:
    // HTTP facilities
    void SendData(const string &  data_ptr, EReplyMimeType  mime_type);
    void Send400(const char *  msg);
    void Send404(const char *  msg);
    void Send500(const char *  msg);
    void Send502(const char *  msg);
    void Send503(const char *  msg);

private:
    HST::CHttpReply<CPendingOperation> *    m_Reply;
    size_t                                  m_NextItemId;
    int32_t                                 m_TotalSentReplyChunks;
    vector<h2o_iovec_t>                     m_Chunks;
};


#endif
