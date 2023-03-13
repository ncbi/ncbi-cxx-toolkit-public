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
#include <corelib/ncbi_param.hpp>

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(osg);


CPSGS_OSGAnnot::CPSGS_OSGAnnot(TEnabledFlags enabled_flags,
                               const CRef<COSGConnectionPool>& pool,
                               const shared_ptr<CPSGS_Request>& request,
                               const shared_ptr<CPSGS_Reply>& reply,
                               TProcessorPriority priority)
    : CPSGS_OSGProcessorBase(enabled_flags, pool, request, reply, priority)
{
}


CPSGS_OSGAnnot::~CPSGS_OSGAnnot()
{
    StopAsyncThread();
}


string CPSGS_OSGAnnot::GetName() const
{
    return "OSG-annot";
}


string CPSGS_OSGAnnot::GetGroupName() const
{
    return kOSGProcessorGroupName;
}


vector<string> CPSGS_OSGAnnot::WhatCanProcess(TEnabledFlags enabled_flags,
                                              shared_ptr<CPSGS_Request>& request)
{
    if ( !(enabled_flags&fEnabledAllAnnot) ) {
        return vector<string>();
    }
    auto& psg_req = request->GetRequest<SPSGS_AnnotRequest>();
    // check if id is good enough
    CSeq_id id;
    try {
        SetSeqId(id, psg_req.m_SeqIdType, psg_req.m_SeqId);
    }
    catch ( exception& /*ignore*/ ) {
        return vector<string>();
    }
    if ( !id.IsGi() && !id.GetTextseq_Id() ) {
        return vector<string>();
    }
    //if ( !CanResolve(request.m_SeqIdType, request.m_SeqId) ) {
    //    return false;
    //}
    return GetNamesToProcess(enabled_flags, psg_req, 0);
}


bool CPSGS_OSGAnnot::CanProcess(TEnabledFlags enabled_flags,
                                shared_ptr<CPSGS_Request>& request,
                                TProcessorPriority priority)
{
    if ( !(enabled_flags&fEnabledAllAnnot) ) {
        return false;
    }
    auto& psg_req = request->GetRequest<SPSGS_AnnotRequest>();
    // check if id is good enough
    CSeq_id id;
    try {
        SetSeqId(id, psg_req.m_SeqIdType, psg_req.m_SeqId);
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
    return !GetNamesToProcess(enabled_flags, psg_req, priority).empty();
}


vector<string> CPSGS_OSGAnnot::GetNamesToProcess(TEnabledFlags enabled_flags,
                                                 SPSGS_AnnotRequest& request,
                                                 TProcessorPriority priority)
{
    vector<string> ret;
    for ( auto& name : request.GetNotProcessedName(priority) ) {
        if ( CanProcessAnnotName(enabled_flags, name) ) {
            ret.push_back(name);
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


bool CPSGS_OSGAnnot::CanProcessAnnotName(TEnabledFlags enabled_flags,
                                         const string& name)
{
    if ( (enabled_flags & fEnabledSNP) && IsSNPName(name) ) {
        return true;
    }
    if ( (enabled_flags & fEnabledCDD) && IsCDDName(name) ) {
        return true;
    }
    return false;
}


void CPSGS_OSGAnnot::CreateRequests()
{
    auto& psg_req = GetRequest()->GetRequest<SPSGS_AnnotRequest>();
    CRef<CID2_Request> osg_req(new CID2_Request);
    auto& req = osg_req->SetRequest().SetGet_blob_id();
    SetSeqId(req.SetSeq_id().SetSeq_id().SetSeq_id(), psg_req.m_SeqIdType, psg_req.m_SeqId);
    m_NamesToProcess.clear();
    m_ApplyCDDFix = false;
    for ( auto& name : GetNamesToProcess(GetEnabledFlags(), psg_req, GetPriority()) ) {
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


bool CPSGS_OSGAnnot::RegisterProcessedName(const string& annot_name)
{
    auto& psg_req = GetRequest()->GetRequest<SPSGS_AnnotRequest>();
    auto prev_priority = psg_req.RegisterProcessedName(GetPriority(), annot_name);
    if ( prev_priority > GetPriority() ) {
        if ( NeedTrace() ) {
            ostringstream str;
            str << "OSG-annot: skip sending NA "<<annot_name
                << " that has already been processed by processor with priority "
                << prev_priority << " > " << GetPriority();
            SendTrace(str.str());
        }
        return false;
    }
    if ( prev_priority != kUnknownPriority && prev_priority < GetPriority() ) {
        if ( NeedTrace() ) {
            ostringstream str;
            str << "OSG-annot: resending NA "<<annot_name
                << " that has already been processed by processor with priority "
                << prev_priority << " < " << GetPriority();
            SendTrace(str.str());
        }
    }
    return true;
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
                PSG_ERROR(GetName()<<": "
                          "Unknown reply to "<<MSerial_AsnText<<*f->GetRequest()<<"\n"<<*r);
                break;
            }
        }
    }
    if ( IsCanceled() ) {
        return;
    }
    if ( s_SimulateError() ) {
        SPSGS_AnnotRequest& annot_request = GetRequest()->GetRequest<SPSGS_AnnotRequest>();
        for ( auto& name : m_NamesToProcess ) {
            annot_request.ReportResultStatus(name, SPSGS_AnnotRequest::ePSGS_RS_Error);
        }
        FinalizeResult(ePSGS_Error);
        return;
    }
    SendReplies();
}


string CPSGS_OSGAnnot::GetAnnotName(const CID2_Reply_Get_Blob_Id& blob_id)
{
    string name;
    for ( auto& ai : blob_id.GetAnnot_info() ) {
        // find annot name
        string ai_name = ai->GetName();
        SIZE_TYPE zoom_pos = ai_name.find("@@");
        if ( zoom_pos != NPOS ) {
            ai_name.resize(zoom_pos);
        }
        if ( name.empty() ) {
            name = ai_name;
        }
        else if ( name != ai_name ) {
            return 0;
        }
    }
    return name;
}


const string* CPSGS_OSGAnnot::AddBlobId(const CID2_Reply_Get_Blob_Id& blob_id)
{
    if ( !blob_id.IsSetBlob_id() ||
         !CPSGS_OSGGetBlobBase::IsEnabledAnnotBlob(GetEnabledFlags(), blob_id.GetBlob_id()) ||
         !blob_id.IsSetAnnot_info() ) {
        return 0;
    }
    string name = GetAnnotName(blob_id);
    if ( !m_NamesToProcess.count(name) ) {
        return 0;
    }
    auto iter = m_BlobIds.insert(make_pair(name, TBlobIdList())).first;
    iter->second.push_back(Ref(&blob_id));
    return &iter->first;
}


void CPSGS_OSGAnnot::SendReplies()
{
    if ( GetDebugLevel() >= eDebug_exchange ) {
        for ( auto& name : m_NamesToProcess ) {
            LOG_POST(GetDiagSeverity() << "OSG: "
                     "Asked for annot "<<name);
        }
        for ( auto& r_name : m_BlobIds ) {
            for ( auto& r : r_name.second ) {
                LOG_POST(GetDiagSeverity() << "OSG: "
                         "Received annot reply "<<MSerial_AsnText<<*r);
            }
        }
    }
    set<string> has_data;
    set<string> processed_by_other;
    for ( auto& r_name : m_BlobIds ) {
        auto& annot_name = r_name.first;
        if ( !RegisterProcessedName(annot_name) ) {
            processed_by_other.insert(annot_name);
            continue;
        }
        has_data.insert(annot_name);
        for ( auto& r : r_name.second ) {
            string psg_blob_id = CPSGS_OSGGetBlobBase::GetPSGBlobId(r->GetBlob_id());
            x_RegisterTiming(eNAResolve, eOpStatusFound, 0);
            CJsonNode       json(CJsonNode::NewObjectNode());
            json.SetString("blob_id", psg_blob_id);
            if ( r->GetBlob_id().IsSetVersion() ) {
                json.SetInteger("last_modified", r->GetBlob_id().GetVersion()*60000);
            }
            if ( r->IsSetAnnot_info() ) {
                // set ASN.1 annot info
                ostringstream str;
                for ( auto& info : r->GetAnnot_info() ) {
                    str << MSerial_AsnBinary << *info;
                }
                json.SetString("seq_annot_info", NStr::Base64Encode(str.str(), 0));
            }
            GetReply()->PrepareNamedAnnotationData(annot_name, GetName(),
                                                   json.Repr(CJsonNode::fStandardJson));
        }
    }
    SPSGS_AnnotRequest& annot_request = GetRequest()->GetRequest<SPSGS_AnnotRequest>();
    for ( auto& name : m_NamesToProcess ) {
        if ( processed_by_other.count(name) ) {
            continue;
        }
        if ( !has_data.count(name) ) {
            annot_request.ReportResultStatus(name, SPSGS_AnnotRequest::ePSGS_RS_NotFound);
        }
    }
    FinalizeResult(!has_data.empty()? ePSGS_Done: ePSGS_NotFound);
}


bool CPSGS_OSGAnnot::IsCDDReply(const CID2_Reply& reply) const
{
    if ( !reply.GetReply().IsGet_blob_id() ) {
        return false;
    }
    
    const CID2_Reply_Get_Blob_Id& blob_id = reply.GetReply().GetGet_blob_id();
    if ( !blob_id.IsSetBlob_id() ||
         !CPSGS_OSGGetBlobBase::IsEnabledCDDBlob(GetEnabledFlags(), blob_id.GetBlob_id()) ||
         !blob_id.IsSetAnnot_info() ) {
        return false;
    }
    return IsCDDName(GetAnnotName(blob_id));
}


END_NAMESPACE(osg);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;
