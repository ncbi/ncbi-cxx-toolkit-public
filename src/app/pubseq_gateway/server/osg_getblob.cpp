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
 * Authors: Eugene Vasilchenko
 *
 * File Description: processor for data from OSG
 *
 */

#include <ncbi_pch.hpp>

#include "osg_getblob.hpp"
#include "osg_fetch.hpp"
#include "osg_connection.hpp"

#include <objects/id2/id2__.hpp>
#include <objects/seqsplit/seqsplit__.hpp>


BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(osg);


CPSGS_OSGGetBlob::CPSGS_OSGGetBlob(const CRef<COSGConnectionPool>& pool,
                                   const shared_ptr<CPSGS_Request>& request,
                                   const shared_ptr<CPSGS_Reply>& reply,
                                   TProcessorPriority priority)
    : CPSGS_OSGProcessorBase(pool, request, reply, priority)
{
}


CPSGS_OSGGetBlob::~CPSGS_OSGGetBlob()
{
}


string CPSGS_OSGGetBlob::GetName() const
{
    return "OSG-getblob";
}


bool CPSGS_OSGGetBlob::CanProcess(SPSGS_BlobBySatSatKeyRequest& request)
{
    return ParsePSGBlobId(request.m_BlobId);
}


void CPSGS_OSGGetBlob::CreateRequests()
{
    auto& psg_req = GetRequest()->GetRequest<SPSGS_BlobBySatSatKeyRequest>();
    m_ApplyCDDFix = false;
    m_EmptyCDDReceived = false;
    if ( auto blob_id = ParsePSGBlobId(psg_req.m_BlobId) ) {
        if ( IsCDDBlob(*blob_id) ) {
            m_ApplyCDDFix = GetConnectionPool().GetCDDRetryTimeout() > 0;
        }
        CRef<CID2_Request> osg_req(new CID2_Request);
        auto& req = osg_req->SetRequest().SetGet_blob_info();
        req.SetBlob_id().SetBlob_id(*blob_id);
        // TODO: blob version?
        req.SetGet_data();
        AddRequest(osg_req);
    }
}


bool CPSGS_OSGGetBlob::IsEmptyCDDBlob(const CID2_Reply& reply)
{
    if ( !reply.GetReply().IsGet_blob() ) {
        return false;
    }
    const CID2_Reply_Get_Blob& reply_get_blob = reply.GetReply().GetGet_blob();
    if ( !reply_get_blob.IsSetData() ) {
        return false;
    }
    const CID2_Reply_Data& data = reply_get_blob.GetData();
    if ( data.GetData_type() != data.eData_type_seq_entry ||
         data.GetData_format() != data.eData_format_asn_binary ||
         data.GetData_compression() != data.eData_compression_none ) {
        return false;
    }
    // minimal Seq-entry with no data
    const size_t kEmptySeqEntrySize = 24;
    return data.GetData().size() == 1 && data.GetData().front()->size() == kEmptySeqEntrySize;
}


void CPSGS_OSGGetBlob::NotifyOSGCallStart()
{
    if ( m_ApplyCDDFix ) {
        m_EmptyCDDReceived = false;
        m_RequestTime.Restart();
    }
}


void CPSGS_OSGGetBlob::NotifyOSGCallReply(const CID2_Reply& reply)
{
    if ( m_ApplyCDDFix ) {
        if ( IsEmptyCDDBlob(reply) ) {
            m_EmptyCDDReceived = true;
        }
    }
}


void CPSGS_OSGGetBlob::NotifyOSGCallEnd()
{
    if ( m_ApplyCDDFix ) {
        if ( m_EmptyCDDReceived &&
             m_RequestTime.Elapsed() > GetConnectionPool().GetCDDRetryTimeout() ) {
            NCBI_THROW(CPubseqGatewayException, eRequestCancelled, "no CDD due to OSG timeout");
        }
    }
}


void CPSGS_OSGGetBlob::ProcessReplies()
{
    for ( auto& f : GetFetches() ) {
        for ( auto& r : f->GetReplies() ) {
            switch ( r->GetReply().Which() ) {
            case CID2_Reply::TReply::e_Init:
            case CID2_Reply::TReply::e_Empty:
                // do nothing
                break;
            case CID2_Reply::TReply::e_Get_blob:
                ProcessBlobReply(*r);
                break;
            case CID2_Reply::TReply::e_Get_split_info:
                ProcessBlobReply(*r);
                break;
            default:
                ERR_POST(GetName()<<": "
                         "Unknown reply to "<<MSerial_AsnText<<*f->GetRequest()<<"\n"<<*r);
                break;
            }
        }
    }
    SendBlob();
    FinalizeResult();
}


CPSGS_OSGGetChunks::CPSGS_OSGGetChunks(const CRef<COSGConnectionPool>& pool,
                                       const shared_ptr<CPSGS_Request>& request,
                                       const shared_ptr<CPSGS_Reply>& reply,
                                       TProcessorPriority priority)
    : CPSGS_OSGProcessorBase(pool, request, reply, priority)
{
}


CPSGS_OSGGetChunks::~CPSGS_OSGGetChunks()
{
}


string CPSGS_OSGGetChunks::GetName() const
{
    return "OSG-gettsechunk";
}


bool CPSGS_OSGGetChunks::CanProcess(SPSGS_TSEChunkRequest& request)
{
    return ParsePSGId2Info(request.m_Id2Info);
}


void CPSGS_OSGGetChunks::CreateRequests()
{
    auto& psg_req = GetRequest()->GetRequest<SPSGS_TSEChunkRequest>();
    if ( auto parsed = ParsePSGId2Info(psg_req.m_Id2Info) ) {
        x_SetSplitVersion(*parsed.tse_id, parsed.split_version);
        CRef<CID2_Request> osg_req(new CID2_Request);
        auto& req = osg_req->SetRequest().SetGet_chunks();
        req.SetBlob_id(*parsed.tse_id);
        req.SetSplit_version(parsed.split_version);
        req.SetChunks().push_back(CID2S_Chunk_Id(psg_req.m_Id2Chunk));
        AddRequest(osg_req);
    }
}


void CPSGS_OSGGetChunks::ProcessReplies()
{
    for ( auto& f : GetFetches() ) {
        for ( auto& r : f->GetReplies() ) {
            switch ( r->GetReply().Which() ) {
            case CID2_Reply::TReply::e_Init:
            case CID2_Reply::TReply::e_Empty:
                // do nothing
                break;
            case CID2_Reply::TReply::e_Get_chunk:
                ProcessBlobReply(*r);
                break;
            default:
                ERR_POST(GetName()<<": "
                         "Unknown reply to "<<MSerial_AsnText<<*f->GetRequest()<<"\n"<<*r);
                break;
            }
        }
    }
    SendBlob();
    FinalizeResult();
}


END_NAMESPACE(osg);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;
