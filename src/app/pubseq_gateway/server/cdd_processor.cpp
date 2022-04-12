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
 * Authors: Aleksey Grichenko
 *
 * File Description: processor for data from CDD
 *
 */

#include <ncbi_pch.hpp>

#include "cdd_processor.hpp"
#include "pubseq_gateway.hpp"
#include "pubseq_gateway_convert_utils.hpp"
#include "pubseq_gateway_logging.hpp"
#include <objtools/data_loaders/cdd/cdd_access/cdd_client.hpp>
#include <objtools/data_loaders/cdd/cdd_access/CDD_Rep_Get_Blob_By_Seq_Id.hpp>
#include <objtools/data_loaders/cdd/cdd_access/CDD_Reply_Get_Blob_Id.hpp>
#include <objects/id2/ID2_Blob_Id.hpp>
#include <objects/seqsplit/ID2S_Seq_annot_Info.hpp>
#include <objects/seqsplit/ID2S_Feat_type_Info.hpp>
#include <objects/seqsplit/ID2S_Seq_loc.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqset/seqset__.hpp>

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(cdd);

USING_SCOPE(objects);

static const string kCDDAnnotName = "CDD";
static const string kCDDProcessorGroupName = "CDD";
static const string kCDDProcessorName = "CDD";
const CID2_Blob_Id::TSat kCDDSat = 8087;


CPSGS_CDDProcessor::CPSGS_CDDProcessor(void)
    : m_ClientPool(new CCDDClientPool()),
      m_Status(ePSGS_NotFound),
      m_Canceled(false),
      m_Unlocked(true)
{
}

CPSGS_CDDProcessor::CPSGS_CDDProcessor(
    shared_ptr<CCDDClientPool> client_pool,
    shared_ptr<CPSGS_Request> request,
    shared_ptr<CPSGS_Reply> reply,
    TProcessorPriority priority)
    : m_ClientPool(client_pool),
      m_Status(ePSGS_InProgress),
      m_Canceled(false),
      m_Unlocked(true)
{
    m_Request = request;
    m_Reply = reply;
    m_Priority = priority;
}

CPSGS_CDDProcessor::~CPSGS_CDDProcessor(void)
{
    _ASSERT(m_Status != ePSGS_InProgress);
    x_UnlockRequest();
}


IPSGS_Processor*
CPSGS_CDDProcessor::CreateProcessor(shared_ptr<CPSGS_Request> request,
                                    shared_ptr<CPSGS_Reply> reply,
                                    TProcessorPriority priority) const
{
    auto req_type = request->GetRequestType();
    if (req_type != CPSGS_Request::ePSGS_AnnotationRequest &&
        req_type != CPSGS_Request::ePSGS_BlobBySatSatKeyRequest) return nullptr;

    auto app = CPubseqGatewayApp::GetInstance();
    bool enabled = app->GetCDDProcessorsEnabled();
    if ( enabled ) {
        for (const auto& name : request->GetRequest<SPSGS_RequestBase>().m_DisabledProcessors ) {
            if ( NStr::EqualNocase(name, kCDDProcessorName) ) {
                enabled = false;
                break;
            }
        }
    }
    else {
        for (const auto& name : request->GetRequest<SPSGS_RequestBase>().m_EnabledProcessors ) {
            if ( NStr::EqualNocase(name, kCDDProcessorName) ) {
                enabled = true;
                break;
            }
        }
    }
    if ( !enabled ) return nullptr;

    if (req_type == CPSGS_Request::ePSGS_AnnotationRequest &&
        !x_CanProcessAnnotRequest(request->GetRequest<SPSGS_AnnotRequest>(), priority))
        return nullptr;
    if (req_type == CPSGS_Request::ePSGS_BlobBySatSatKeyRequest &&
        !x_CanProcessBlobRequest(request->GetRequest<SPSGS_BlobBySatSatKeyRequest>()))
        return nullptr;
        
    return new CPSGS_CDDProcessor(m_ClientPool, request, reply, priority);
}


string CPSGS_CDDProcessor::GetName() const
{
    return kCDDProcessorName;
}


string CPSGS_CDDProcessor::GetGroupName() const
{
    return kCDDProcessorGroupName;
}


void CPSGS_CDDProcessor::Process()
{
    CRequestContextResetter     context_resetter;
    GetRequest()->SetRequestContext();

    try {
        m_Unlocked = false;
        if (m_Request) m_Request->Lock(kCDDProcessorEvent);
        auto req_type = GetRequest()->GetRequestType();
        switch (req_type) {
        case CPSGS_Request::ePSGS_AnnotationRequest:
            x_ProcessResolveRequest();
            break;
        case CPSGS_Request::ePSGS_BlobBySatSatKeyRequest:
            x_ProcessGetBlobRequest();
            break;
        default:
            x_Finish(ePSGS_Error);
            break;
        }
    }
    catch (...) {
        x_Finish(ePSGS_Error);
    }
}


static void s_GetBlobId(CPSGS_CDDProcessor* processor)
{
    processor->GetBlobId();
}


static void s_OnGotBlobId(void* data)
{
    static_cast<CPSGS_CDDProcessor*>(data)->OnGotBlobId();
}


static void s_GetBlobBySeqId(CPSGS_CDDProcessor* processor)
{
    processor->GetBlobBySeqId();
}


static void s_OnGotBlobBySeqId(void* data)
{
    static_cast<CPSGS_CDDProcessor*>(data)->OnGotBlobBySeqId();
}


void CPSGS_CDDProcessor::x_ProcessResolveRequest(void)
{
    SPSGS_AnnotRequest& annot_request = GetRequest()->GetRequest<SPSGS_AnnotRequest>();
    if ( !x_NameIncluded(annot_request.GetNotProcessedName(m_Priority)) ) {
        x_Finish(ePSGS_NotFound);
        return;
    }
    try {
        CSeq_id id(annot_request.m_SeqId);
        m_SeqId = CSeq_id_Handle::GetHandle(id);
    }
    catch (exception& e) {
        PSG_WARNING("Bad seq-id: " << annot_request.m_SeqId);
        x_Finish(ePSGS_Error);
        return;
    }

    if (annot_request.m_TSEOption == SPSGS_BlobRequestBase::EPSGS_TSEOption::ePSGS_SmartTSE ||
        annot_request.m_TSEOption == SPSGS_BlobRequestBase::EPSGS_TSEOption::ePSGS_WholeTSE ||
        annot_request.m_TSEOption == SPSGS_BlobRequestBase::EPSGS_TSEOption::ePSGS_OrigTSE) {
        // Send whole TSE.
        thread(bind(&s_GetBlobBySeqId, this)).detach();
    }
    else {
        // Send annot info only.
        thread(bind(&s_GetBlobId, this)).detach();
    }
}


static void s_GetBlobByBlobId(CPSGS_CDDProcessor* processor)
{
    processor->GetBlobByBlobId();
}


static void s_OnGotBlobByBlobId(void* data)
{
    static_cast<CPSGS_CDDProcessor*>(data)->OnGotBlobByBlobId();
}


void CPSGS_CDDProcessor::x_ProcessGetBlobRequest(void)
{
    SPSGS_BlobBySatSatKeyRequest blob_request =
        GetRequest()->GetRequest<SPSGS_BlobBySatSatKeyRequest>();
    m_BlobId = CCDDClientPool::StringToBlobId(blob_request.m_BlobId.GetId());
    if ( !m_BlobId ) {
        x_Finish(ePSGS_NotFound);
        return;
    }
    thread(bind(&s_GetBlobByBlobId, this)).detach();
}


void CPSGS_CDDProcessor::GetBlobId(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    try {
        m_CDDBlob.info = m_ClientPool->GetBlobIdBySeq_id(m_SeqId);
    }
    catch (...) {
        m_CDDBlob.info.Reset();
        m_CDDBlob.data.Reset();
    }
    GetUvLoopBinder().PostponeInvoke(s_OnGotBlobId, this);
}


void CPSGS_CDDProcessor::GetBlobBySeqId(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    try {
        m_CDDBlob = m_ClientPool->GetBlobBySeq_id(m_SeqId);
    }
    catch (...) {
        m_CDDBlob.info.Reset();
        m_CDDBlob.data.Reset();
    }
    GetUvLoopBinder().PostponeInvoke(s_OnGotBlobBySeqId, this);
}


void CPSGS_CDDProcessor::GetBlobByBlobId(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    try {
        m_CDDBlob.data = m_ClientPool->GetBlobByBlobId(*m_BlobId);
    }
    catch (...) {
        m_CDDBlob.info.Reset();
        m_CDDBlob.data.Reset();
    }
    GetUvLoopBinder().PostponeInvoke(s_OnGotBlobByBlobId, this);
}


void CPSGS_CDDProcessor::OnGotBlobId(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    if ( x_IsCanceled() ) {
        return;
    }
    if ( !m_CDDBlob.info ) {
        x_Finish(ePSGS_NotFound);
        return;
    }
    if ( !x_SignalStartProcessing() ) {
        return;
    }
    try {
        x_SendAnnotInfo(*m_CDDBlob.info);
    }
    catch (...) {
        x_Finish(ePSGS_Error);
        return;
    }
    x_Finish(ePSGS_Done);
}


void CPSGS_CDDProcessor::OnGotBlobBySeqId(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    if ( x_IsCanceled() ) {
        return;
    }
    if ( !m_CDDBlob.info  ||  !m_CDDBlob.data ) {
        x_Finish(ePSGS_NotFound);
        return;
    }
    if ( !x_SignalStartProcessing() ) {
        return;
    }
    try {
        x_SendAnnotInfo(*m_CDDBlob.info);
        x_SendAnnot(m_CDDBlob.info->GetBlob_id(), m_CDDBlob.data);
    }
    catch (...) {
        x_Finish(ePSGS_Error);
        return;
    }
    x_Finish(ePSGS_Done);
}


void CPSGS_CDDProcessor::OnGotBlobByBlobId(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    if ( x_IsCanceled() ) {
        return;
    }
    if ( !m_CDDBlob.data ) {
        x_Finish(ePSGS_NotFound);
        return;
    }
    if ( !x_SignalStartProcessing() ) {
        return;
    }
    try {
        x_SendAnnot(*m_BlobId, m_CDDBlob.data);
    }
    catch (...) {
        x_Finish(ePSGS_Error);
        return;
    }
    x_Finish(ePSGS_Done);
}


void CPSGS_CDDProcessor::x_SendAnnotInfo(const CCDD_Reply_Get_Blob_Id& blob_info)
{
    SPSGS_AnnotRequest& annot_request = GetRequest()->GetRequest<SPSGS_AnnotRequest>();
    const CID2_Blob_Id& blob_id = blob_info.GetBlob_id();
    CJsonNode       json(CJsonNode::NewObjectNode());
    json.SetString("blob_id", CCDDClientPool::BlobIdToString(blob_id));
    if ( blob_id.IsSetVersion() ) {
        json.SetInteger("last_modified", blob_id.GetVersion()*60000);
    }

    CRef<CID2S_Seq_annot_Info> annot_info(new CID2S_Seq_annot_Info);
    annot_info->SetName(kCDDAnnotName);
    CRef<CID2S_Feat_type_Info> feat_info(new CID2S_Feat_type_Info);
    feat_info->SetType(CSeqFeatData::e_Region);
    feat_info->SetSubtypes().push_back(CSeqFeatData::eSubtype_region);
    annot_info->SetFeat().push_back(feat_info);
    feat_info.Reset(new CID2S_Feat_type_Info);
    feat_info->SetType(CSeqFeatData::e_Site);
    feat_info->SetSubtypes().push_back(CSeqFeatData::eSubtype_site);
    annot_info->SetFeat().push_back(feat_info);

    const CSeq_id& annot_id = blob_info.GetSeq_id();
    if ( annot_id.IsGi() ) {
        annot_info->SetSeq_loc().SetWhole_gi(annot_id.GetGi());
    }
    else {
        annot_info->SetSeq_loc().SetWhole_seq_id().Assign(annot_id);
    }

    ostringstream annot_str;
    annot_str << MSerial_AsnBinary << *annot_info;
    json.SetString("seq_annot_info", NStr::Base64Encode(annot_str.str(), 0));

    GetReply()->PrepareNamedAnnotationData(kCDDAnnotName, kCDDProcessorName,
        json.Repr(CJsonNode::fStandardJson));
    annot_request.RegisterProcessedName(GetPriority(), kCDDAnnotName);
}


void CPSGS_CDDProcessor::x_SendAnnot(const CID2_Blob_Id& id2_blob_id, CRef<CSeq_annot>& annot)
{
    string psg_blob_id = CCDDClientPool::BlobIdToString(id2_blob_id);
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSet().SetSeq_set();
    entry->SetAnnot().push_back(annot);
    ostringstream blob_str;
    blob_str << MSerial_AsnBinary << *entry;
    string blob_data = blob_str.str();

    CBlobRecord blob_props;
    if (id2_blob_id.IsSetVersion()) {
        blob_props.SetModified(int64_t(id2_blob_id.GetVersion()*60000));
    }
    blob_props.SetNChunks(1);
    size_t item_id = GetReply()->GetItemId();
    GetReply()->PrepareBlobPropData(
        item_id,
        kCDDProcessorName,
        psg_blob_id,
        ToJsonString(blob_props));
    GetReply()->PrepareBlobPropCompletion(item_id, kCDDProcessorName, 2);

    item_id = GetReply()->GetItemId();
    GetReply()->PrepareBlobData(
        item_id,
        kCDDProcessorName,
        psg_blob_id,
        (const unsigned char*)blob_data.data(), blob_data.size(), 0);
    GetReply()->PrepareBlobCompletion(item_id, kCDDProcessorName, 2);
}


void CPSGS_CDDProcessor::Cancel()
{
    m_Canceled = true;
    x_UnlockRequest();
}


IPSGS_Processor::EPSGS_Status CPSGS_CDDProcessor::GetStatus()
{
    return m_Status;
}


void CPSGS_CDDProcessor::x_UnlockRequest(void)
{
    if (m_Unlocked) return;
    m_Unlocked = true;
    if (m_Request) m_Request->Unlock(kCDDProcessorEvent);
}


bool CPSGS_CDDProcessor::x_IsCanceled()
{
    if ( m_Canceled ) {
        x_Finish(ePSGS_Canceled);
        return true;
    }
    return false;
}


bool CPSGS_CDDProcessor::x_SignalStartProcessing()
{
    if ( SignalStartProcessing() == ePSGS_Cancel ) {
        x_Finish(ePSGS_Canceled);
        return false;
    }
    return true;
}


bool CPSGS_CDDProcessor::x_CanProcessAnnotRequest(SPSGS_AnnotRequest& annot_request,
                                                  TProcessorPriority priority) const
{
    try {
        CSeq_id id(annot_request.m_SeqId);
        if (!id.IsGi() && !id.GetTextseq_Id()) return false;
        if (!m_ClientPool->IsValidId(id)) return false;
    }
    catch (exception& e) {
        PSG_WARNING("Bad seq-id: " << annot_request.m_SeqId);
        return false;
    }
    return x_NameIncluded(annot_request.GetNotProcessedName(priority));
}


bool CPSGS_CDDProcessor::x_CanProcessBlobRequest(SPSGS_BlobBySatSatKeyRequest& blob_request) const
{
    CRef<CCDDClientPool::TBlobId> blob_id =
        CCDDClientPool::StringToBlobId(blob_request.m_BlobId.GetId());
    return blob_id && blob_id->GetSat() == kCDDSat;
}


bool CPSGS_CDDProcessor::x_NameIncluded(const vector<string>& names) const
{
    for ( auto& name : names ) {
        if ( NStr::EqualNocase(name, kCDDAnnotName) ) return true;
    }
    return false;
}


void CPSGS_CDDProcessor::x_Finish(EPSGS_Status status)
{
    _ASSERT(status != ePSGS_InProgress);
    m_Status = status;
    x_UnlockRequest();
    SignalFinishProcessing();
}


END_NAMESPACE(cdd);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;
