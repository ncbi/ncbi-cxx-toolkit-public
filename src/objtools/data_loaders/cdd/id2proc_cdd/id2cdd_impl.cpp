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
#include <objtools/error_codes.hpp>
#include <objtools/data_loaders/cdd/id2proc_cdd/impl/id2cdd_impl.hpp>
#include <objtools/data_loaders/cdd/id2proc_cdd/id2cdd.hpp>
#include <objtools/data_loaders/cdd/id2proc_cdd/id2cdd_params.h>
#include <objtools/data_loaders/cdd/cdd_access/cdd_client.hpp>
#include <objtools/data_loaders/cdd/cdd_access/cdd_access__.hpp>

#define NCBI_USE_ERRCODE_X Objtools_CDD_ID2Proc

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


CID2CDDProcessorPacketContext::~CID2CDDProcessorPacketContext(void)
{
    if (m_Processor.NotEmpty()) {
        if (m_Replies.empty()  &&  !m_SeqIDs.empty()) {
            CCDD_Reply last_reply;
            try {
                m_Client->second->JustFetch(last_reply);
            } STD_CATCH_ALL("~CID2CDDProcessorPacketContext");
        }
        m_Processor->ReturnClient(m_Client);
    }
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

    m_ServiceName = conf.GetString(driver_name,
        NCBI_ID2PROC_CDD_PARAM_SERVICE_NAME,
        CConfig::eErr_NoThrow,
        DEFAULT_CDD_SERVICE_NAME);

    m_InitialContext.m_Compress = conf.GetBool(driver_name,
        NCBI_ID2PROC_CDD_PARAM_COMPRESS_DATA,
        CConfig::eErr_NoThrow,
        false);

    m_PoolSoftLimit = conf.GetInt(driver_name,
                                  NCBI_ID2PROC_CDD_PARAM_POOL_SOFT_LIMIT,
                                  CConfig::eErr_NoThrow, 10);

    m_PoolAgeLimit = conf.GetInt(driver_name,
                                 NCBI_ID2PROC_CDD_PARAM_POOL_AGE_LIMIT,
                                 CConfig::eErr_NoThrow, 15 * 60);

    string authoritative_satellites
        = conf.GetString(driver_name,
                         NCBI_ID2PROC_CDD_PARAM_AUTHORITATIVE_SATELLITES,
                         CConfig::eErr_NoThrow, NStr::IntToString(kCDD_Sat));
    vector<CTempString> v;
    NStr::Split(authoritative_satellites, ", ", v, NStr::fSplit_Tokenize);
    for (const auto& x : v) {
        m_AuthoritativeSatellites.insert(NStr::StringToNumeric<int>(x));
    }

    m_ExcludeNucleotides = conf.GetBool
        (driver_name, NCBI_ID2PROC_CDD_PARAM_EXCLUDE_NUCLEOTIDES,
         CConfig::eErr_NoThrow, true);
}


CID2CDDProcessor_Impl::~CID2CDDProcessor_Impl(void)
{
}


CRef<CID2CDDProcessorContext>
CID2CDDProcessor_Impl::CreateContext(void)
{
    CRef<CID2CDDProcessorContext> context(new CID2CDDProcessorContext);
    context->m_Context = m_InitialContext;
    return context;
}


void CID2CDDProcessor_Impl::InitContext(
    CID2CDDContext& context,
    const CID2_Request& request)
{
    context = GetInitialContext();
    if (request.IsSetParams()) {
        ITERATE(CID2_Request::TParams::Tdata, it, request.GetParams().Get()) {
            const CID2_Param& param = **it;
            if (param.GetName() == "id2:allow" && param.IsSetValue()) {
                ITERATE(CID2_Param::TValue, it2, param.GetValue()) {
                    if (*it2 == "vdb-cdd") {
                        context.m_AllowVDB = true;
                    }
                }
            }
        }
    }
}


CRef<CID2ProcessorPacketContext> CID2CDDProcessor_Impl::ProcessPacket(
    CID2CDDProcessorContext* context,
    CID2_Request_Packet& packet,
    TReplies& replies)
{
    CRef<CID2CDDProcessorPacketContext> packet_context;
    CRef<CCDD_Request_Packet>           cdd_packet;
    map<int, CID2_Request_Packet::Tdata::iterator> blob_requests;
    unique_ptr<CDiagContext_Extra>      extra;
    ERASE_ITERATE(CID2_Request_Packet::Tdata, it, packet.Set()) {
        const CID2_Request& req = **it;

        // init request can come without serial number
        if ((*it)->GetRequest().IsInit()) {
            InitContext(context->m_Context, **it);
            continue;
        }
        if (!context->m_Context.m_AllowVDB) {
            continue;
        }

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
                break;
            }
        default:
            break;
        }

        if (req_id.Empty()  &&  blob_id.Empty()) {
            continue;
        }

        CRef<CCDD_Request> cdd_request(new CCDD_Request);
        int serial_number = req.IsSetSerial_number() ? req.GetSerial_number() : -1;
        cdd_request->SetSerial_number(serial_number);

        if ( !packet_context ) {
            packet_context.Reset(new CID2CDDProcessorPacketContext);
            packet_context->m_Client = m_InUse.end();
            cdd_packet.Reset(new CCDD_Request_Packet);
        }
        if ( req_id ) {
            CConstRef<CSeq_id> id = ID2_id_To_Seq_id(*req_id);
            if (m_ExcludeNucleotides
                &&  (id->IdentifyAccession() & CSeq_id::fAcc_nuc) != 0) {
                if (extra.get() == NULL) {
                    extra.reset(new CDiagContext_Extra
                                (GetDiagContext().Extra()));
                }
                extra->Print(FORMAT(serial_number << ".cdd-filtered-out"),
                             id->AsFastaString());
                continue;
            }
            packet_context->m_SeqIDs[serial_number] = id;
            cdd_request->SetRequest().SetGet_blob_id().Assign(*id);
            _TRACE("Queuing request (#" << serial_number
                   << ") for CDD blob ID for " << id->AsFastaString());
        }
        if ( blob_id ) {
            packet_context->m_BlobIDs[serial_number] = blob_id;
            cdd_request->SetRequest().SetGet_blob().Assign(*blob_id);
            blob_requests[serial_number] = it;
            _TRACE("Queuing request (#" << serial_number
                   << ") for CDD blob for " << MSerial_FlatAsnText
                   << *blob_id);
        }
        cdd_packet->Set().push_back(cdd_request);
    }
    if (cdd_packet.NotEmpty()  &&  !cdd_packet->Get().empty()) {
        _TRACE("Sending queued CDD requests.");
        {{
            time_t now;
            CTime::GetCurrentTimeT(&now);
            time_t cutoff = now - m_PoolAgeLimit;
            CFastMutexGuard GUARD(m_PoolLock);
            TClientPool::iterator it = m_NotInUse.lower_bound(cutoff);
            if (it == m_NotInUse.end()) {
                CRef<CCDDClient> client(new CCDDClient(m_ServiceName));
                packet_context->m_Client = m_InUse.emplace(now, client);
            } else {
                packet_context->m_Client = m_InUse.insert(*it);
                ++it;
            }
            m_NotInUse.erase(m_NotInUse.begin(), it);
        }}
        if (packet_context->m_BlobIDs.empty()) {
            try {
                packet_context->m_Client->second->JustAsk(*cdd_packet);
            } catch (...) {
                x_DiscardClient(packet_context->m_Client);
                throw;
            }
        } else {
            CCDD_Reply last_reply;
            try {
                packet_context->m_Client->second->Ask(*cdd_packet, last_reply);
                x_TranslateReplies(context->m_Context, *packet_context);
                ReturnClient(packet_context->m_Client);
            } catch (exception& e) {
                x_DiscardClient(packet_context->m_Client);
                ERR_POST_X(4, Warning << e.what());
            } catch (...) {
                x_DiscardClient(packet_context->m_Client);
                throw;
            }
            for (auto & id : packet_context->m_BlobIDs) {
                const auto & reply = packet_context->m_Replies.find(id.first);
                if (reply != packet_context->m_Replies.end()
                    &&  reply->second.NotEmpty()) {
                    replies.push_back(reply->second);
                    packet_context->m_Replies.erase(reply);
                    packet.Set().erase(blob_requests[id.first]);
                } else if (m_AuthoritativeSatellites.count(id.second->GetSat())
                           > 0) {
                    string msg = FORMAT("No CDD annotation found for "
                                        << MSerial_FlatAsnText << *id.second);
                    NStr::TrimSuffixInPlace(msg, "\n");
                    ERR_POST_X(5, Warning << msg);
                    CRef<CID2_Error> id2_error(new CID2_Error);
                    id2_error->SetSeverity(CID2_Error::eSeverity_no_data);
                    id2_error->SetMessage(msg);
                    CRef<CID2_Reply> error_reply(new CID2_Reply);
                    error_reply->SetSerial_number(id.first);
                    error_reply->SetError().push_back(id2_error);
                    error_reply->SetEnd_of_reply();
                    error_reply->SetReply().SetEmpty();
                    replies.push_back(error_reply);
                    packet.Set().erase(blob_requests[id.first]);
                }
            }
            if (packet.Get().empty()) {
                packet_context.Reset();
            } else {
                packet_context->m_BlobIDs.clear();
            }
        }
    }
    return CRef<CID2ProcessorPacketContext>(packet_context.GetPointer());
}


void CID2CDDProcessor_Impl::ProcessReply(
    CID2ProcessorContext* context,
    CID2ProcessorPacketContext* packet_context,
    CID2_Reply& reply,
    TReplies& replies)
{
    CID2CDDProcessorContext* cdd_context = dynamic_cast<CID2CDDProcessorContext*>(context);
    if (cdd_context && cdd_context->m_Context.m_AllowVDB) {
        if (reply.IsSetEnd_of_reply()) {
            CID2CDDProcessorPacketContext* cdd_packet_context =
                dynamic_cast<CID2CDDProcessorPacketContext*>(packet_context);
            CRef<CID2_Reply> cdd_reply;
            if (cdd_packet_context) {
                int serial_num = reply.GetSerial_number();
                CID2CDDProcessorPacketContext::TSeqIDs::iterator id_it
                    = cdd_packet_context->m_SeqIDs.find(serial_num);
                if (id_it != cdd_packet_context->m_SeqIDs.end()) {
                    if (cdd_packet_context->m_Replies.empty()) {
                        CCDD_Reply last_reply;
                        try {
                            cdd_packet_context->m_Client->second
                                ->JustFetch(last_reply);
                            x_TranslateReplies(cdd_context->m_Context,
                                               *cdd_packet_context);
                            ReturnClient(cdd_packet_context->m_Client);
                        } catch (...) {
                            x_DiscardClient(cdd_packet_context->m_Client);
                        }
                    }
                    auto it = cdd_packet_context->m_Replies.find(serial_num);
                    if (it != cdd_packet_context->m_Replies.end()) {
                        cdd_reply = it->second;
                        cdd_packet_context->m_Replies.erase(it);
                        replies.push_back(cdd_reply);
                    }
                    cdd_packet_context->m_SeqIDs.erase(id_it);
                }
            }
        }

        if (reply.GetReply().IsGet_blob_id()) {
            const CID2_Blob_Id& bid = reply.GetReply().GetGet_blob_id().GetBlob_id();
            if (bid.GetSat() == kPubseqCDD_Sat) {
                if (!reply.IsSetEnd_of_reply()) return;
                // If end-of-reply is set, don't discard the reply, but rather make it empty.
                reply.SetReply().SetEmpty();
            }
        }
    }

    replies.push_back(Ref(&reply));
}


void CID2CDDProcessor_Impl::x_TranslateReplies
(const CID2CDDContext& context, CID2CDDProcessorPacketContext& packet_context)
{
    for (auto& it : packet_context.m_Client->second->GetReplies()) {
        if (it->GetReply().IsEmpty()  &&  !it->IsSetError()) {
            continue;
        }
        if (it->GetReply().IsEmpty()  &&  !it->IsSetError()) {
            continue;
        }
        if ( !it->IsSetSerial_number() ) {
            CNcbiOstrstream oss;
            oss << "Discarding non-empty CDD-Reply with no serial number";
            if (it->IsSetError()) {
                const CCDD_Error& e = it->GetError();
                oss << " and error message " << e.GetMessage() << " (code "
                    << e.GetCode() << ", severity " << (int)e.GetSeverity()
                    << ").";
            } else {
                oss << " and no error message.";
            }
            ERR_POST_X(1, Warning << CNcbiOstrstreamToString(oss));
            continue;
        }
        int serial_number = it->GetSerial_number();
        _TRACE("Received CDD reply with serial number " << serial_number);
        CRef<CID2_Reply> id2_reply = x_CreateID2_Reply(serial_number, *it);
        switch (it->GetReply().Which()) {
        case CCDD_Reply::TReply::e_Get_blob_id:
        {
            const auto& id_it = packet_context.m_SeqIDs.find(serial_number);
            if (id_it == packet_context.m_SeqIDs.end()) {
                ERR_POST_X(2, Warning <<
                           "Discarding blob-id CDD-Reply with unexpected"
                           " serial number " << serial_number);
                continue;
            }
            x_TranslateBlobIdReply(id2_reply, id_it->second, it);
            break;
        }
        case CCDD_Reply::TReply::e_Get_blob:
        {
            const auto& id_it = packet_context.m_BlobIDs.find(serial_number);
            if (id_it == packet_context.m_BlobIDs.end()) {
                ERR_POST_X(3, Warning <<
                           "Discarding blob CDD-Reply with unexpected"
                           " serial number " << serial_number);
                continue;
            }
            x_TranslateBlobReply
                (id2_reply, context, *id_it->second,
                 CRef<CCDD_Reply>(&const_cast<CCDD_Reply&>(*it)));
            break;
        }
        default:
            break;
        }
        if (id2_reply.NotEmpty()) {
            packet_context.m_Replies[serial_number] = id2_reply;
        }
    }
}


void
CID2CDDProcessor_Impl::x_TranslateBlobIdReply(CRef<CID2_Reply> id2_reply,
                                              CConstRef<CSeq_id> id,
                                              CConstRef<CCDD_Reply> cdd_reply)
{
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
}


void CID2CDDProcessor_Impl::x_TranslateBlobReply(CRef<CID2_Reply> id2_reply,
                                                 const CID2CDDContext& context,
                                                 const CID2_Blob_Id& blob_id,
                                                 CRef<CCDD_Reply> cdd_reply)
{
    CID2_Reply::TReply& reply = id2_reply->SetReply();
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
    if (context.m_Compress) {
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
}


CRef<CID2_Reply> CID2CDDProcessor_Impl::x_CreateID2_Reply(
    CID2_Request::TSerial_number serial_number,
    const CCDD_Reply& cdd_reply)
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


void CID2CDDProcessor_Impl::ReturnClient(TClientPool::iterator& client)
{
    time_t now;
    CTime::GetCurrentTimeT(&now);
    time_t cutoff = now - m_PoolAgeLimit;
    CFastMutexGuard GUARD(m_PoolLock);
    // Clean up unconditionally (may as well)
    m_NotInUse.erase(m_NotInUse.begin(), m_NotInUse.lower_bound(cutoff));
    if (client != m_InUse.end()) {
        if (client->first >= cutoff
            &&  m_InUse.size() + m_NotInUse.size() <= m_PoolSoftLimit) {
            m_NotInUse.insert(*client);
        }
        m_InUse.erase(client);
        client = m_InUse.end();
    }
}

void CID2CDDProcessor_Impl::GetClientCounts(size_t& in_use, size_t& not_in_use)
    const
{
    CFastMutexGuard GUARD(m_PoolLock);
    in_use     = m_InUse.size();
    not_in_use = m_NotInUse.size();
}

void CID2CDDProcessor_Impl::x_DiscardClient(TClientPool::iterator& client)
{
    CFastMutexGuard GUARD(m_PoolLock);
    m_InUse.erase(client);
    client = m_InUse.end();
}


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;
