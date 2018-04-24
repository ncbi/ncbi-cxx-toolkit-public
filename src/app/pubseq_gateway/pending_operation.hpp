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

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>
USING_IDBLOB_SCOPE;

#include "http_server_transport.hpp"
#include "pubseq_gateway_utils.hpp"

#include <objtools/pubseq_gateway/impl/diag/AppLog.hpp>
#include <objtools/pubseq_gateway/impl/diag/AppPerf.hpp>
using namespace IdLogUtil;


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


class CPendingOperation
{
public:
    CPendingOperation(EBlobIdentificationType  blob_id_type,
                      const SBlobId &  blob_id,
                      string &&  sat_name,
                      shared_ptr<CCassConnection>  conn,
                      unsigned int  timeout,
                      unsigned int  max_retries);
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
    EBlobIdentificationType                 m_BlobIdType;
    int32_t                                 m_TotalSentBlobChunks;
    int32_t                                 m_TotalSentReplyChunks;

    HST::CHttpReply<CPendingOperation>*     m_Reply;
    bool                                    m_Cancelled;
    bool                                    m_FinishedRead;
    const SBlobId                           m_BlobId;
    CAppOp                                  m_Op;
    unique_ptr<CCassBlobLoader>             m_Loader;
    vector<h2o_iovec_t>                     m_Chunks;
    unique_ptr<SOperationContext>           m_Context;
};


#endif
