/* $Id$
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
 * Author:  Aleksey Grichenko
 *
 * File Description:
 *   RPC client for CDD annotations
 *
 */


#ifndef OBJTOOLS_DATA_LOADERS_CDD_CDD_ACCESS__CDD_CLIENT__HPP
#define OBJTOOLS_DATA_LOADERS_CDD_CDD_ACCESS__CDD_CLIENT__HPP


#include <objtools/data_loaders/cdd/cdd_access/CDD_Request_Packet.hpp>
#include <objtools/data_loaders/cdd/cdd_access/CDD_Reply.hpp>
#include <serial/rpcbase.hpp>


BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

#define DEFAULT_CDD_SERVICE_NAME "getCddSeqAnnot"

class CSeq_id;
class CID2_Blob_Id;

class NCBI_CDD_ACCESS_EXPORT CCDDClient : public CRPCClient<CCDD_Request_Packet, CCDD_Reply>
{
    typedef CRPCClient<CCDD_Request_Packet, CCDD_Reply> Tparent;
public:
    // constructor
    CCDDClient(const string& service_name = kEmptyStr);
    // destructor
    ~CCDDClient(void);

    typedef CCDD_Request_Packet TRequests;
    typedef vector< CConstRef<CCDD_Reply> > TReplies;

    virtual void Ask(const CCDD_Request_Packet& request, CCDD_Reply& reply);

    virtual void ReadReply(CObjectIStream& in, CCDD_Reply& reply);

    const TReplies& GetReplies(void) { return m_Replies; }

    // Shortcut method for fetching a single blob-id.
    CRef<CCDD_Reply> AskBlobId(int serial_number, const CSeq_id& seq_id);

    // Shortcut method for fetching a single blob.
    CRef<CCDD_Reply> AskBlob(int serial_number, const CID2_Blob_Id& blob_id);

private:
    // Prohibit copy constructor and assignment operator
    CCDDClient(const CCDDClient& value);
    CCDDClient& operator=(const CCDDClient& value);

    TReplies m_Replies;
};


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJTOOLS_DATA_LOADERS_CDD_CDD_ACCESS__CDD_CLIENT__HPP
