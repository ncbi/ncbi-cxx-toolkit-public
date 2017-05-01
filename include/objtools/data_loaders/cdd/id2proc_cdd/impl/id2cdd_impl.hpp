#ifndef OBJTOOLS_DATA_LOADERS_CDD_ID2PROC_CDD_IMPL__ID2CDD_IMPL__HPP
#define OBJTOOLS_DATA_LOADERS_CDD_ID2PROC_CDD_IMPL__ID2CDD_IMPL__HPP
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
 * Authors:  Eugene Vasilchenko, Aleksey Grichenko
 *
 * File Description:
 *   Processor of ID2 requests for CDD annotations
 *
 */

#include <corelib/ncbistd.hpp>
#include <objtools/data_loaders/cdd/id2proc_cdd/id2cdd.hpp>
#include <objtools/data_loaders/cdd/cdd_access/cdd_access__.hpp>
#include <objtools/data_loaders/cdd/cdd_access/cdd_client.hpp>
#include <objects/id2/id2__.hpp>
#include <objects/id2/id2processor.hpp>

BEGIN_NCBI_NAMESPACE;

BEGIN_NAMESPACE(objects);


class NCBI_ID2PROC_CDD_EXPORT CID2CDDProcessorPacketContext : public CID2ProcessorPacketContext
{
public:
    typedef CRef<CID2_Reply> TID2ReplyPtr;
    typedef map<int, TID2ReplyPtr> TReplies;

    TReplies m_Replies;
};


class NCBI_ID2PROC_CDD_EXPORT CID2CDDProcessor_Impl : public CObject
{
public:
    CID2CDDProcessor_Impl(const CConfig::TParamTree* params = 0,
                          const string& driver_name = kEmptyStr);
    ~CID2CDDProcessor_Impl(void);

    typedef CID2CDDProcessor::TReplies TReplies;

    CRef<CID2ProcessorPacketContext> ProcessPacket(CID2_Request_Packet& packet,
                                                   CID2CDDProcessor::TReplies& replies);

    void ProcessReply(CID2_Reply& reply,
                      TReplies& replies,
                      CID2ProcessorPacketContext* packet_context);

private:
    CRef<CID2_Reply> x_GetBlobId(int serial_number, const CID2_Seq_id& req_id);
    CRef<CID2_Reply> x_GetBlob(int serial_number, const CID2_Blob_Id& blob_id);
    CRef<CID2_Reply> x_CreateID2_Reply(int serial_number,
                                       CCDD_Reply& cdd_reply);
    void x_CreateBlobIdReply(const CCDD_Request::TRequest& cdd_request,
                             CCDD_Reply::TReply& cdd_reply,
                             CID2_Reply::TReply& id2_reply);
    void x_CreateBlobReply(const CCDD_Request::TRequest& cdd_request,
                           CCDD_Reply::TReply& cdd_reply,
                           CID2_Reply::TReply& id2_reply);

    bool m_Compress;
    CRef<CCDDClient> m_Client;
};


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // OBJTOOLS_DATA_LOADERS_CDD_ID2PROC_CDD_IMPL__ID2CDD_IMPL__HPP
