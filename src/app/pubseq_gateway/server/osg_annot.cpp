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

#include "osg_annot.hpp"
#include "osg_getblob_base.hpp"
#include "osg_fetch.hpp"
#include "osg_connection.hpp"
#include "pubseq_gateway.hpp"

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/id2/id2__.hpp>
#include <objects/seqsplit/seqsplit__.hpp>
#include <util/range.hpp>

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(osg);


CPSGS_OSGAnnot::CPSGS_OSGAnnot(const CRef<COSGConnectionPool>& pool,
                               const shared_ptr<CPSGS_Request>& request,
                               const shared_ptr<CPSGS_Reply>& reply,
                               TProcessorPriority priority)
    : CPSGS_OSGProcessorBase(pool, request, reply, priority)
{
}


CPSGS_OSGAnnot::~CPSGS_OSGAnnot()
{
}


string CPSGS_OSGAnnot::GetName() const
{
    return "OSG-annot";
}


bool CPSGS_OSGAnnot::CanProcess(SPSGS_AnnotRequest& request,
                                TProcessorPriority priority)
{
    // check if id is good enough
    CSeq_id id;
    try {
        SetSeqId(id, request.m_SeqIdType, request.m_SeqId);
    }
    catch ( exception& /*ignore*/ ) {
        return false;
    }
    if ( !id.IsGi() && !id.GetTextseq_Id() ) {
        return false;
    }
    //if ( !CanResolve(request.m_SeqIdType, request.m_SeqId) ) {
    //    return false;
    //}
    return !GetNamesToProcess(request, priority).empty();
}


set<string> CPSGS_OSGAnnot::GetNamesToProcess(SPSGS_AnnotRequest& request,
                                              TProcessorPriority priority)
{
    set<string> ret;
    for ( auto& name : request.GetNotProcessedName(priority) ) {
        if ( CanProcessAnnotName(name) ) {
            ret.insert(name);
        }
    }
    return ret;
}


static bool IsCDDName(const string& name)
{
    return NStr::EqualNocase(name, "CDD");
}


// primary SNP track
static bool IsPrimarySNPName(const string& name)
{
    return NStr::EqualNocase(name, "SNP");
}


// explicit name for a SNP track
static bool IsExplicitSNPName(const string& name)
{
    return NStr::StartsWith(name, "NA", NStr::eNocase) && name.find("#") != NPOS;
}


static bool IsSNPName(const string& name)
{
    return IsPrimarySNPName(name) || IsExplicitSNPName(name);
}


bool CPSGS_OSGAnnot::CanProcessAnnotName(const string& name)
{
    auto app = CPubseqGatewayApp::GetInstance();
    auto& config = *app->GetOSGConnectionPool();
    return
        (config.GetEnabledCDD() && IsCDDName(name)) ||
        (config.GetEnabledSNP() && IsSNPName(name));
}


void CPSGS_OSGAnnot::CreateRequests()
{
    auto& psg_req = GetRequest()->GetRequest<SPSGS_AnnotRequest>();
    CRef<CID2_Request> osg_req(new CID2_Request);
    auto& req = osg_req->SetRequest().SetGet_blob_id();
    SetSeqId(req.SetSeq_id().SetSeq_id().SetSeq_id(), psg_req.m_SeqIdType, psg_req.m_SeqId);
    m_NamesToProcess.clear();
    m_ApplyCDDFix = false;
    for ( auto& name : GetNamesToProcess(psg_req, GetPriority()) ) {
        m_NamesToProcess.insert(name);
        if ( IsCDDName(name) ) {
            // CDD are external annotations in OSG
            req.SetExternal();
            m_ApplyCDDFix = GetConnectionPool().GetCDDRetryTimeout() > 0;
        }
        else {
            // others have named annot accession (source)
            req.SetSources().push_back(name);
        }
    }
    AddRequest(osg_req);
}


void CPSGS_OSGAnnot::NotifyOSGCallStart()
{
    if ( m_ApplyCDDFix ) {
        m_CDDReceived = false;
        m_RequestTime.Restart();
    }
}


void CPSGS_OSGAnnot::NotifyOSGCallReply(const CID2_Reply& reply)
{
    if ( m_ApplyCDDFix ) {
        if ( IsCDDReply(reply) ) {
            m_CDDReceived = true;
        }
    }
}


void CPSGS_OSGAnnot::NotifyOSGCallEnd()
{
    if ( m_ApplyCDDFix ) {
        if ( !m_CDDReceived &&
             m_RequestTime.Elapsed() > GetConnectionPool().GetCDDRetryTimeout() ) {
            NCBI_THROW(CPubseqGatewayException, eRequestCanceled, "no CDD due to OSG timeout");
        }
    }
}


void CPSGS_OSGAnnot::ProcessReplies()
{
    for ( auto& f : GetFetches() ) {
        if ( GetDebugLevel() >= eDebug_exchange ) {
            LOG_POST(GetDiagSeverity() << "OSG: "
                     "Processing fetch: "<<MSerial_AsnText<<f->GetRequest());
        }
        for ( auto& r : f->GetReplies() ) {
            if ( GetDebugLevel() >= eDebug_exchange ) {
                LOG_POST(GetDiagSeverity() << "OSG: "
                         "Processing reply: "<<MSerial_AsnText<<*r);
            }
            switch ( r->GetReply().Which() ) {
            case CID2_Reply::TReply::e_Init:
            case CID2_Reply::TReply::e_Empty:
                // do nothing
                break;
            case CID2_Reply::TReply::e_Get_seq_id:
                // do nothing
                break;
            case CID2_Reply::TReply::e_Get_blob_id:
                AddBlobId(r->GetReply().GetGet_blob_id());
                break;
            default:
                ERR_POST(GetName()<<": "
                         "Unknown reply to "<<MSerial_AsnText<<*f->GetRequest()<<"\n"<<*r);
                break;
            }
        }
    }
    SendReplies();
    FinalizeResult(ePSGS_Found);
}


void CPSGS_OSGAnnot::AddBlobId(const CID2_Reply_Get_Blob_Id& blob_id)
{
    if ( !blob_id.IsSetBlob_id() ) {
        return;
    }
    if ( !blob_id.IsSetAnnot_info() ) {
        return;
    }
    m_BlobIds.push_back(Ref(&blob_id));
}


namespace {
    struct SAnnotInfo {
        // NA accession
        string annot_name;
        // annotated location
        string accession;
        int version;
        int seq_id_type;
        CRange<TSeqPos> range;
        // annotation types
        CJsonNode json;

        void Add(const CID2S_Seq_loc& loc) {
            switch ( loc.Which() ) {
            case CID2S_Seq_loc::e_Whole_gi:
                Add(loc.GetWhole_gi());
                break;
            case CID2S_Seq_loc::e_Whole_seq_id:
                Add(loc.GetWhole_seq_id());
                break;
            case CID2S_Seq_loc::e_Whole_gi_range:
                Add(loc.GetWhole_gi_range());
                break;
            case CID2S_Seq_loc::e_Gi_interval:
                Add(loc.GetGi_interval());
                break;
            case CID2S_Seq_loc::e_Seq_id_interval:
                Add(loc.GetSeq_id_interval());
                break;
            case CID2S_Seq_loc::e_Gi_ints:
                Add(loc.GetGi_ints());
                break;
            case CID2S_Seq_loc::e_Seq_id_ints:
                Add(loc.GetSeq_id_ints());
                break;
            case CID2S_Seq_loc::e_Loc_set:
                for ( auto& l : loc.GetLoc_set() ) {
                    Add(*l);
                }
                break;
            default:
                break;
            }
        }
        void Add(TGi gi) {
            Add(gi, CRange<TSeqPos>::GetWhole());
        }
        void Add(const CSeq_id& id) {
            Add(id, CRange<TSeqPos>::GetWhole());
        }
        void Add(TGi gi, TSeqPos start, TSeqPos length) {
            Add(gi, COpenRange<TSeqPos>(start, start+length));
        }
        void Add(const CSeq_id& id, TSeqPos start, TSeqPos length) {
            Add(id, COpenRange<TSeqPos>(start, start+length));
        }
        void Add(TGi gi, CRange<TSeqPos> range) {
            Add(CSeq_id(CSeq_id::e_Gi, gi), range);
        }
        void SetSeqId(const CSeq_id& id) {
            string new_accession;
            int new_version = 0;
            int new_type = id.Which();
            if ( auto text_id = id.GetTextseq_Id() ) {
                if ( text_id->IsSetAccession() && text_id->IsSetVersion() ) {
                    new_accession = text_id->GetAccession();
                    new_version = text_id->GetVersion();
                }
            }
            else {
                id.GetLabel(&new_accession, CSeq_id::eFastaContent);
            }
            if ( accession.empty() ) {
                accession = new_accession;
                version = new_version;
                seq_id_type = new_type;
            }
            else if ( accession != new_accession ||
                      version != new_version ||
                      seq_id_type != new_type ) {
                ERR_POST("OSG-annot: multiple annotated Seq-ids");
                throw runtime_error("");
            }
        }
        void Add(const CSeq_id& id, CRange<TSeqPos> add_range) {
            SetSeqId(id);
            range.CombineWith(add_range);
        }
        void Add(const CID2S_Gi_Range& gi_range) {
            for ( TIntId i = 0; i < gi_range.GetCount(); ++i ) {
                Add(GI_FROM(TIntId, GI_TO(TIntId, gi_range.GetStart())+i));
            }
        }
        void Add(const CID2S_Gi_Interval& interval) {
            Add(interval.GetGi(), interval.GetStart(), interval.GetLength());
        }
        void Add(const CID2S_Seq_id_Interval& interval) {
            Add(interval.GetSeq_id(), interval.GetStart(), interval.GetLength());
        }
        void Add(const CID2S_Gi_Ints& ints) {
            for ( auto& i : ints.GetInts() ) {
                Add(ints.GetGi(), i->GetStart(), i->GetLength());
            }
        }
        void Add(const CID2S_Seq_id_Ints& ints) {
            for ( auto& i : ints.GetInts() ) {
                Add(ints.GetSeq_id(), i->GetStart(), i->GetLength());
            }
        }

        void SetAnnotName(const string& name)
            {
                if ( annot_name.empty() ) {
                    annot_name = name;
                }
                else if ( annot_name != name ) {
                    ERR_POST("OSG-annot: multiple annot accessions: "<<annot_name<<" <> "<<name);
                    throw runtime_error("");
                }
            }
        
        SAnnotInfo(const list<CRef<CID2S_Seq_annot_Info>>& annot_infos)
            : range(CRange<TSeqPos>::GetEmpty()),
              json(CJsonNode::NewObjectNode())
            {
                vector<int64_t> zooms;
                for ( auto& ai : annot_infos ) {
                    // collect location
                    if ( ai->IsSetSeq_loc() ) {
                        Add(ai->GetSeq_loc());
                    }

                    // collect name
                    auto& full_name = ai->GetName();
                    string acc;
                    SIZE_TYPE zoom_pos = full_name.find("@@");
                    if ( zoom_pos != NPOS ) {
                        SetAnnotName(full_name.substr(0, zoom_pos));
                        zooms.push_back(NStr::StringToInt(full_name.substr(zoom_pos+2)));
                    }
                    else {
                        SetAnnotName(full_name);
                    }

                    // collect types
                    if ( ai->IsSetAlign() ) {
                        CJsonNode type_json = CJsonNode::NewArrayNode();
                        type_json.AppendInteger(0);
                        json.SetByKey(to_string(CSeq_annot::C_Data::e_Align), type_json);
                    }
                    if ( ai->IsSetGraph() ) {
                        CJsonNode type_json = CJsonNode::NewArrayNode();
                        type_json.AppendInteger(0);
                        json.SetByKey(to_string(CSeq_annot::C_Data::e_Graph), type_json);
                    }
                    if ( ai->IsSetFeat() ) {
                        auto& types = ai->GetFeat();
                        if ( types.empty() ||
                             (types.size() == 1 &&
                              types.front()->GetType() == 0 &&
                              !types.front()->IsSetSubtypes()) ) {
                            CJsonNode type_json = CJsonNode::NewArrayNode();
                            type_json.AppendInteger(0);
                            json.SetByKey(to_string(CSeq_annot::C_Data::e_Seq_table), type_json);
                        }
                        else {
                            CJsonNode type_json = CJsonNode::NewObjectNode();
                            for ( auto& feat_type : types ) {
                                CJsonNode subtype_json = CJsonNode::NewArrayNode();
                                if ( feat_type->IsSetSubtypes() ) {
                                    for ( auto feat_subtype : feat_type->GetSubtypes() ) {
                                        subtype_json.AppendInteger(feat_subtype);
                                    }
                                }
                                type_json.SetByKey(to_string(feat_type->GetType()), subtype_json);
                            }
                            json.SetByKey(to_string(CSeq_annot::C_Data::e_Ftable), type_json);
                        }
                    }
                }
                
                // add collected zoom levels
                if ( !zooms.empty() ) {
                    CJsonNode zooms_json = CJsonNode::NewArrayNode();
                    for ( auto zoom : zooms ) {
                        zooms_json.AppendInteger(zoom);
                    }
                    json.SetByKey("2048", zooms_json);
                }
            }
    };
}


void CPSGS_OSGAnnot::SendReplies()
{
    if ( GetDebugLevel() >= eDebug_exchange ) {
        for ( auto& name : m_NamesToProcess ) {
            LOG_POST(GetDiagSeverity() << "OSG: "
                     "Asked for annot "<<name);
        }
        for ( auto& r : m_BlobIds ) {
            LOG_POST(GetDiagSeverity() << "OSG: "
                     "Received annot reply "<<MSerial_AsnText<<*r);
        }
    }
    auto& psg_req = GetRequest()->GetRequest<SPSGS_AnnotRequest>();
    for ( auto& r : m_BlobIds ) {
        if ( !CPSGS_OSGGetBlobBase::IsOSGBlob(r->GetBlob_id()) ) {
            continue;
        }
        string psg_blob_id = CPSGS_OSGGetBlobBase::GetPSGBlobId(r->GetBlob_id());
        CJsonNode       json(CJsonNode::NewObjectNode());
        json.SetString("blob_id", psg_blob_id);
        if ( r->GetBlob_id().IsSetVersion() ) {
            json.SetInteger("last_modified", r->GetBlob_id().GetVersion()*60000);
        }
        string annot_name;
        try {
            SAnnotInfo info(r->GetAnnot_info());
            annot_name = info.annot_name;
            json.SetString("accession", info.accession);
            json.SetInteger("version", info.version);
            json.SetInteger("seq_id_type", info.seq_id_type);
            if ( info.range != CRange<TSeqPos>::GetWhole() ) {
                json.SetInteger("start", info.range.GetFrom());
                json.SetInteger("stop", info.range.GetTo());
            }
            else {
                // whole sequence
                json.SetInteger("start", 0);
                json.SetInteger("stop", 0);
            }
            json.SetString("annot_info", info.json.Repr(CJsonNode::fStandardJson));
        }
        catch ( exception& ) {
            ERR_POST(GetName()<<": "
                     "Bad annot-info: "<<MSerial_AsnText<<*r);
            // find default annot_name
            for ( auto& ai : r->GetAnnot_info() ) {
                if ( m_NamesToProcess.count(ai->GetName()) ) {
                    annot_name = ai->GetName();
                    break;
                }
            }
        }
        if ( r->IsSetAnnot_info() ) {
            // set ASN.1 annot info
            ostringstream str;
            for ( auto& info : r->GetAnnot_info() ) {
                str << MSerial_AsnBinary << *info;
            }
            json.SetString("seq_annot_info", NStr::Base64Encode(str.str(), 0));
        }
        if ( m_NamesToProcess.count(annot_name) ) {
            GetReply()->PrepareNamedAnnotationData(annot_name, GetName(),
                                                   json.Repr(CJsonNode::fStandardJson));
        }
        //GetReply()->PrepareReplyCompletion();
    }
    // register processed names
    for ( auto& name : m_NamesToProcess ) {
        psg_req.RegisterProcessedName(GetPriority(), name);
    }
}


bool CPSGS_OSGAnnot::IsCDDReply(const CID2_Reply& reply) const
{
    if ( !reply.GetReply().IsGet_blob_id() ) {
        return false;
    }
    
    const CID2_Reply_Get_Blob_Id& blob_id = reply.GetReply().GetGet_blob_id();
    if ( !blob_id.IsSetBlob_id() || !CPSGS_OSGGetBlobBase::IsOSGBlob(blob_id.GetBlob_id()) ) {
        return false;
    }

    if ( !blob_id.IsSetAnnot_info() ) {
        return false;
    }
    try {
        SAnnotInfo info(blob_id.GetAnnot_info());
        return IsCDDName(info.annot_name);
    }
    catch ( exception& /*ignored*/ ) {
        return false;
    }
}


END_NAMESPACE(osg);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;
