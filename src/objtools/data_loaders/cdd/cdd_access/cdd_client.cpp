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

#include <ncbi_pch.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/id2/ID2_Blob_Id.hpp>
#include <objtools/data_loaders/cdd/cdd_access/CDD_Request.hpp>
#include <objtools/data_loaders/cdd/cdd_access/cdd_client.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::


CCDDClient::CCDDClient(const string& service_name)
    : Tparent(service_name.empty() ? DEFAULT_CDD_SERVICE_NAME : service_name, eSerial_Json)
{
}


CCDDClient::~CCDDClient(void)
{
}


void CCDDClient::Ask(const CCDD_Request_Packet& request, CCDD_Reply& reply)
{
    m_Replies.clear();
    Tparent::Ask(request, reply);
}


void CCDDClient::ReadReply(CObjectIStream& in, CCDD_Reply& reply)
{
    m_Replies.clear();
    CRef<CCDD_Reply> next_reply;
    do {
        next_reply.Reset(new CCDD_Reply);
        in >> *next_reply;
        m_Replies.push_back(next_reply);
    } while (!next_reply->IsSetEnd_of_reply());

    if (!m_Replies.empty()) {
        reply.Assign(*m_Replies.back());
    }
}


CRef<CCDD_Reply> CCDDClient::AskBlobId(int serial_number, const CSeq_id& seq_id)
{
    CCDD_Request_Packet cdd_packet;
    CRef<CCDD_Request> cdd_request(new CCDD_Request);
    cdd_request->SetSerial_number(serial_number);
    cdd_request->SetRequest().SetGet_blob_id().Assign(seq_id);
    cdd_packet.Set().push_back(cdd_request);
    CRef<CCDD_Reply> cdd_reply(new CCDD_Reply);
    try {
        Ask(cdd_packet, *cdd_reply);
    }
    catch (exception& e) {
        ERR_POST(e.what());
        return null;
    }
    return cdd_reply;
}


CRef<CCDD_Reply> CCDDClient::AskBlob(int serial_number, const CID2_Blob_Id& blob_id)
{
    CCDD_Request_Packet cdd_packet;
    CRef<CCDD_Request> cdd_request(new CCDD_Request);
    cdd_request->SetSerial_number(serial_number);
    cdd_request->SetRequest().SetGet_blob().Assign(blob_id);
    cdd_packet.Set().push_back(cdd_request);
    CRef<CCDD_Reply> cdd_reply(new CCDD_Reply);
    try {
        Ask(cdd_packet, *cdd_reply);
    }
    catch (exception& e) {
        ERR_POST(e.what());
        return null;
    }
    return cdd_reply;
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 57, chars: 1749, CRC32: ab618a22 */
