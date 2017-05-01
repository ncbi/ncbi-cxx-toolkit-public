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

#include <ncbi_pch.hpp>
#include <corelib/rwstream.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/serial.hpp>
#include <util/compress/zlib.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/id2/id2__.hpp>
#include <objects/id2/id2processor_interface.hpp>
#include <objects/seqsplit/seqsplit__.hpp>
#include <objtools/data_loaders/cdd/id2proc_cdd/impl/id2cdd_impl.hpp>
#include <objtools/data_loaders/cdd/id2proc_cdd/id2cdd.hpp>
#include <objtools/data_loaders/cdd/id2proc_cdd/id2cdd_params.h>
#include <objtools/data_loaders/cdd/cdd_access/cdd_client.hpp>
#include <objtools/data_loaders/cdd/cdd_access/cdd_access__.hpp>


BEGIN_NCBI_NAMESPACE;

BEGIN_NAMESPACE(objects);


/////////////////////////////////////////////////////////////////////////////
// CID2CDDProcessor_Impl
/////////////////////////////////////////////////////////////////////////////


const int kPubseqCDD_Sat = 10;
const int kCDD_Sat = 8087;


inline
CConstRef<CSeq_id> ID2_id_To_Seq_id(const CID2_Seq_id& id2_id)
{
    return id2_id.IsSeq_id() ? ConstRef(&id2_id.GetSeq_id()) :
        ConstRef(new CSeq_id(id2_id.GetString()));
}


CID2CDDProcessor_Impl::CID2CDDProcessor_Impl(const CConfig::TParamTree* params,
                                             const string& driver_name)
{
    auto_ptr<CConfig::TParamTree> app_params;
    if ( !params ) {
        CMutexGuard guard(CNcbiApplication::GetInstanceMutex());
        if ( CNcbiApplication* app = CNcbiApplication::Instance() ) {
            app_params.reset(CConfig::ConvertRegToTree(app->GetConfig()));
            params = app_params.get();
        }
    }
    if ( params ) {
        params = params->FindSubNode(CInterfaceVersion<CID2Processor>::GetName());
    }
    if ( params ) {
        params = params->FindSubNode(driver_name);
    }
    CConfig conf(params);

    string service_name = conf.GetString(driver_name,
        NCBI_ID2PROC_CDD_PARAM_SERVICE_NAME,
        CConfig::eErr_NoThrow,
        DEFAULT_CDD_SERVICE_NAME);

    m_Compress = conf.GetBool(driver_name,
        NCBI_ID2PROC_CDD_PARAM_COMPRESS_DATA,
        CConfig::eErr_NoThrow,
        false);

    m_Client.Reset(new CCDDClient(service_name));
}


CID2CDDProcessor_Impl::~CID2CDDProcessor_Impl(void)
{
}


CRef<CID2ProcessorPacketContext> CID2CDDProcessor_Impl::ProcessPacket(
    CID2_Request_Packet& packet,
    CID2CDDProcessor::TReplies& replies)
{
    CRef<CID2CDDProcessorPacketContext> packet_context;
    ERASE_ITERATE(CID2_Request_Packet::Tdata, it, packet.Set()) {
        const CID2_Request& req = **it;
        CConstRef<CID2_Seq_id> req_id; // Can be in get-blob-id or get-blob-info
        CConstRef<CID2_Blob_Id> blob_id;
        switch (req.GetRequest().Which()) {
        case CID2_Request::TRequest::e_Get_blob_id:
            req_id.Reset(&req.GetRequest().GetGet_blob_id().GetSeq_id().GetSeq_id());
            break;
        case CID2_Request::TRequest::e_Get_blob_info:
            {
                const CID2_Request_Get_Blob_Info& req_get_info = req.GetRequest().GetGet_blob_info();
                if (req_get_info.GetBlob_id().IsResolve()) {
                    req_id.Reset(&req_get_info.GetBlob_id().GetResolve().GetRequest().GetSeq_id().GetSeq_id());
                }
                else if (req_get_info.GetBlob_id().IsBlob_id()  &&
                    req_get_info.GetBlob_id().GetBlob_id().GetSat() == kCDD_Sat) {
                    blob_id.Reset(&req_get_info.GetBlob_id().GetBlob_id());
                }
            }
        default:
            break;
        }

        int serial_number = req.IsSetSerial_number() ? req.GetSerial_number() : -1;
        if ( req_id ) {
            if ( !packet_context ) {
                packet_context.Reset(new CID2CDDProcessorPacketContext);
            }
            CRef<CID2_Reply> cdd_reply(x_GetBlobId(serial_number, *req_id));
            if (cdd_reply) {
                packet_context->m_Replies[serial_number] = cdd_reply;
            }
        }
        if ( blob_id ) {
            CRef<CID2_Reply> cdd_reply(x_GetBlob(serial_number, *blob_id));
            if (cdd_reply) {
                replies.push_back(cdd_reply);
                packet.Set().erase(it);
            }
        }
    }
    if (packet_context && packet_context->m_Replies.empty()) {
        packet_context.Reset();
    }
    return CRef<CID2ProcessorPacketContext>(packet_context.GetPointer());
}


void CID2CDDProcessor_Impl::ProcessReply(
    CID2_Reply& reply,
    TReplies& replies,
    CID2ProcessorPacketContext* packet_context)
{
    if ( reply.IsSetEnd_of_reply() ) {
        CID2CDDProcessorPacketContext* cdd_context =
            dynamic_cast<CID2CDDProcessorPacketContext*>(packet_context);
        CRef<CID2_Reply> cdd_reply;
        if ( cdd_context ) {
            CID2CDDProcessorPacketContext::TReplies::iterator it =
                cdd_context->m_Replies.find(reply.GetSerial_number());
            if (it != cdd_context->m_Replies.end()) {
                cdd_reply = it->second;
                cdd_context->m_Replies.erase(it);
                replies.push_back(cdd_reply);
            }
        }
    }

    if ( reply.GetReply().IsGet_blob_id() ) {
        const CID2_Blob_Id& bid = reply.GetReply().GetGet_blob_id().GetBlob_id();
        if (bid.GetSat() == kPubseqCDD_Sat) {
            if ( !reply.IsSetEnd_of_reply() ) return;
            // If end-of-reply is set, don't discard the reply, but rather make it empty.
            reply.SetReply().SetEmpty();
        }
    }

    replies.push_back(Ref(&reply));
}


CRef<CID2_Reply> CID2CDDProcessor_Impl::x_GetBlobId(int serial_number, const CID2_Seq_id& req_id)
{
    CConstRef<CSeq_id> id = ID2_id_To_Seq_id(req_id);
    CRef<CCDD_Reply> cdd_reply = m_Client->AskBlobId(serial_number, *id);
    CRef<CID2_Reply> id2_reply = x_CreateID2_Reply(serial_number, *cdd_reply);
    if (!id2_reply->IsSetReply() || !id2_reply->GetReply().IsGet_blob_id()) {
        // Not a blob-id reply.
        return id2_reply;
    }

    CID2_Reply_Get_Blob_Id& id2_gb = id2_reply->SetReply().SetGet_blob_id();
    id2_gb.SetEnd_of_reply();
    const CCDD_Reply_Get_Blob_Id& cdd_gb = cdd_reply->GetReply().GetGet_blob_id();
    id2_gb.SetSeq_id().Assign(*id);
    id2_gb.SetBlob_id().Assign(cdd_gb.GetBlob_id());
    CRef<CID2S_Seq_annot_Info> annot_info(new CID2S_Seq_annot_Info);
    annot_info->SetName("CDD");

    if (cdd_gb.IsSetFeat_types()) {
        ITERATE(CCDD_Reply_Get_Blob_Id::TFeat_types, it, cdd_gb.GetFeat_types()) {
            CRef<CID2S_Feat_type_Info> feat_info(new CID2S_Feat_type_Info);
            feat_info->SetType((*it)->GetType());
            if ( (*it)->IsSetSubtype() ) {
                feat_info->SetSubtypes().push_back((*it)->GetSubtype());
            }
            annot_info->SetFeat().push_back(feat_info);
        }
    }
    else {
        // Use default types
        CRef<CID2S_Feat_type_Info> feat_info(new CID2S_Feat_type_Info);
        feat_info->SetType(CSeqFeatData::e_Region);
        feat_info->SetSubtypes().push_back(CSeqFeatData::eSubtype_region);
        annot_info->SetFeat().push_back(feat_info);
        feat_info.Reset(new CID2S_Feat_type_Info);
        feat_info->SetType(CSeqFeatData::e_Site);
        feat_info->SetSubtypes().push_back(CSeqFeatData::eSubtype_site);
        annot_info->SetFeat().push_back(feat_info);
    }
    if ( cdd_gb.GetSeq_id().IsGi() ) {
        annot_info->SetSeq_loc().SetWhole_gi(cdd_gb.GetSeq_id().GetGi());
    }
    else {
        annot_info->SetSeq_loc().SetWhole_seq_id().Assign(cdd_gb.GetSeq_id());
    }
    id2_gb.SetAnnot_info().push_back(annot_info);
    return id2_reply;
}


CRef<CID2_Reply> CID2CDDProcessor_Impl::x_GetBlob(int serial_number, const CID2_Blob_Id& blob_id)
{
    CRef<CCDD_Reply> cdd_reply = m_Client->AskBlob(serial_number, blob_id);
    CRef<CID2_Reply> id2_reply = x_CreateID2_Reply(serial_number, *cdd_reply);
    CID2_Reply::TReply& reply = id2_reply->SetReply();
    if (!id2_reply->IsSetReply() || !id2_reply->GetReply().IsGet_blob()) {
        // Not a blob reply.
        return id2_reply;
    }

    CID2_Reply_Get_Blob& gb = reply.SetGet_blob();
    gb.SetBlob_id().Assign(blob_id);
    CID2_Reply_Data& data = gb.SetData();
    data.SetData_type(CID2_Reply_Data::eData_type_seq_entry);
    data.SetData_format(CID2_Reply_Data::eData_format_asn_binary);
    CSeq_entry entry;
    entry.SetSet().SetSeq_set();
    entry.SetAnnot().push_back(Ref(&cdd_reply->SetReply().SetGet_blob()));
    COctetStringSequenceWriter writer(data.SetData());
    CWStream writer_stream(&writer);
    AutoPtr<CNcbiOstream> str;
    if (m_Compress) {
        data.SetData_compression(CID2_Reply_Data::eData_compression_gzip);
        str.reset(new CCompressionOStream(writer_stream,
            new CZipStreamCompressor(ICompression::eLevel_Lowest),
            CCompressionIStream::fOwnProcessor));
    }
    else {
        data.SetData_compression(CID2_Reply_Data::eData_compression_none);
        str.reset(&writer_stream, eNoOwnership);
    }
    CObjectOStreamAsnBinary objstr(*str);
    objstr << entry;

    id2_reply->SetEnd_of_reply();
    return id2_reply;
}


CRef<CID2_Reply> CID2CDDProcessor_Impl::x_CreateID2_Reply(
    CID2_Request::TSerial_number serial_number,
    CCDD_Reply& cdd_reply)
{
    CRef<CID2_Reply> id2_reply(new CID2_Reply);
    id2_reply->SetSerial_number(serial_number);
    if ( cdd_reply.IsSetError() ) {
        CRef<CID2_Error> error(new CID2_Error);
        error->SetMessage(cdd_reply.GetError().GetMessage());
        // TODO: What to do with the CDD severity and code?
        error->SetSeverity(CID2_Error::eSeverity_failed_command);
        id2_reply->SetError().push_back(error);
    }

    switch (cdd_reply.GetReply().Which()) {
    case CCDD_Reply::TReply::e_Get_blob_id:
        id2_reply->SetReply().SetGet_blob_id();
        break;
    case CCDD_Reply::TReply::e_Get_blob:
        id2_reply->SetReply().SetGet_blob();
        break;
    default:
        // Unrecognized reply type.
        if ( !id2_reply->IsSetError() ) return null;
        // Error reply.
        id2_reply->SetReply().SetEmpty();
    }
    return id2_reply;
}


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;
