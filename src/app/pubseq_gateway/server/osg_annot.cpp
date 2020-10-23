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

#include <objects/seqloc/Seq_id.hpp>
#include <objects/id2/id2__.hpp>
#include <objects/seqsplit/seqsplit__.hpp>

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
    if ( !CanResolve(request.m_SeqIdType, request.m_SeqId) ) {
        return false;
    }
    return !GetNamesToProcess(request, priority).empty();
}


vector<string> CPSGS_OSGAnnot::GetNamesToProcess(SPSGS_AnnotRequest& request,
                                                 TProcessorPriority priority)
{
    vector<string> ret;
    for ( auto& name : request.GetNotProcessedName(priority) ) {
        if ( CanProcessAnnotName(name) ) {
            ret.push_back(name);
        }
    }
    return ret;
}


bool CPSGS_OSGAnnot::CanProcessAnnotName(const string& name)
{
    if ( name == "CDD" ) {
        // CDD annots
        return true;
    }
    if ( name == "SNP" ) {
        // default SNP track
        return true;
    }
    if ( NStr::StartsWith(name, "NA") && name.find("#") != NPOS ) {
        // explicitly names SNP track
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
    for ( auto& name : GetNamesToProcess(psg_req, GetPriority()) ) {
        m_NamesToProcess.push_back(name);
        req.SetSources().push_back(name);
    }
    AddRequest(osg_req);
}


void CPSGS_OSGAnnot::ProcessReplies()
{
    for ( auto& f : GetFetches() ) {
        for ( auto& r : f->GetReplies() ) {
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
        string psg_blob_id = CPSGS_OSGGetBlobBase::GetPSGBlobId(r->GetBlob_id());
        auto& annot_info = r->GetAnnot_info();
        string annot_name = annot_info.front()->GetName();
        CJsonNode       json(CJsonNode::NewObjectNode());
        json.SetString("blob_id", psg_blob_id);
        if ( r->GetBlob_id().IsSetVersion() ) {
            json.SetInteger("last_modified", r->GetBlob_id().GetVersion()*60000);
        }
        GetReply()->PrepareNamedAnnotationData(annot_name, GetName(),
                                               json.Repr(CJsonNode::fStandardJson));
        //GetReply()->PrepareReplyCompletion();
    }
    // register processed names
    for ( auto& name : m_NamesToProcess ) {
        psg_req.RegisterProcessedName(GetPriority(), name);
    }
}


END_NAMESPACE(osg);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;
