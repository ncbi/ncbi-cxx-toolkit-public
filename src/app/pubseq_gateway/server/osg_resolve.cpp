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

#include "osg_resolve.hpp"
#include "osg_getblob_base.hpp"
#include "osg_fetch.hpp"
#include "osg_connection.hpp"

#include <objects/seqloc/Seq_id.hpp>
#include <objects/id2/id2__.hpp>
//#include <objects/seqsplit/seqsplit__.hpp>
//#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info/record.hpp>
//#include "pubseq_gateway_convert_utils.hpp"

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(osg);


static const bool kAlwaysAskForBlobId = true;


CPSGS_OSGResolve::CPSGS_OSGResolve(TEnabledFlags enabled_flags,
                                   const CRef<COSGConnectionPool>& pool,
                                   const shared_ptr<CPSGS_Request>& request,
                                   const shared_ptr<CPSGS_Reply>& reply,
                                   TProcessorPriority priority)
    : CPSGS_OSGProcessorBase(enabled_flags, pool, request, reply, priority)
{
}


CPSGS_OSGResolve::~CPSGS_OSGResolve()
{
}


string CPSGS_OSGResolve::GetName() const
{
    return "OSG-resolve";
}


bool CPSGS_OSGResolve::CanProcess(TEnabledFlags enabled_flags,
                                  shared_ptr<CPSGS_Request>& request)
{
    if ( !(enabled_flags & fEnabledWGS) ) {
        return false;
    }
    auto& psg_req = request->GetRequest<SPSGS_ResolveRequest>();
    return CanResolve(psg_req.m_SeqIdType, psg_req.m_SeqId);
}


void CPSGS_OSGResolve::CreateRequests()
{
    auto& psg_req = GetRequest()->GetRequest<SPSGS_ResolveRequest>();
    if ( psg_req.m_IncludeDataFlags &
         (psg_req.fPSGS_SeqIds |
          psg_req.fPSGS_CanonicalId |
          psg_req.fPSGS_Gi) ) {
        // actual sequence ids
        CRef<CID2_Request> osg_req(new CID2_Request);
        auto& req = osg_req->SetRequest().SetGet_seq_id();
        SetSeqId(req.SetSeq_id().SetSeq_id(), psg_req.m_SeqIdType, psg_req.m_SeqId);
        req.SetSeq_id_type(CID2_Request_Get_Seq_id::eSeq_id_type_all);
        AddRequest(osg_req);
    }
    if ( psg_req.m_IncludeDataFlags &
         (psg_req.fPSGS_MoleculeType |
          psg_req.fPSGS_Length |
          psg_req.fPSGS_TaxId |
          psg_req.fPSGS_Hash) ) {
        // artificial ids for special info requests
        CRef<CID2_Request> osg_req(new CID2_Request);
        auto& req = osg_req->SetRequest().SetGet_seq_id();
        SetSeqId(req.SetSeq_id().SetSeq_id(), psg_req.m_SeqIdType, psg_req.m_SeqId);
        req.SetSeq_id_type(0);
        if ( psg_req.m_IncludeDataFlags & psg_req.fPSGS_MoleculeType ) {
            req.SetSeq_id_type() |= CID2_Request_Get_Seq_id::eSeq_id_type_seq_mol;
        }
        if ( psg_req.m_IncludeDataFlags & psg_req.fPSGS_Length ) {
            req.SetSeq_id_type() |= CID2_Request_Get_Seq_id::eSeq_id_type_seq_length;
        }
        if ( psg_req.m_IncludeDataFlags & psg_req.fPSGS_TaxId ) {
            req.SetSeq_id_type() |= CID2_Request_Get_Seq_id::eSeq_id_type_taxid;
        }
        if ( psg_req.m_IncludeDataFlags & psg_req.fPSGS_Hash ) {
            req.SetSeq_id_type() |= CID2_Request_Get_Seq_id::eSeq_id_type_hash;
        }
        AddRequest(osg_req);
    }
    /*
      TODO
      fPSGS_SeqState = (1 << 12),
    */
    if ( kAlwaysAskForBlobId ||
         (psg_req.m_IncludeDataFlags &
          (psg_req.fPSGS_BlobId |
           psg_req.fPSGS_State |
           psg_req.fPSGS_DateChanged)) ) {
        // blob id or state
        CRef<CID2_Request> osg_req(new CID2_Request);
        auto& req = osg_req->SetRequest().SetGet_blob_id();
        SetSeqId(req.SetSeq_id().SetSeq_id().SetSeq_id(), psg_req.m_SeqIdType, psg_req.m_SeqId);
        AddRequest(osg_req);
    }
    if ( GetFetches().empty() ) {
        // resolve only, no any other information to return
        CRef<CID2_Request> osg_req(new CID2_Request);
        auto& req = osg_req->SetRequest().SetGet_seq_id();
        SetSeqId(req.SetSeq_id().SetSeq_id(), psg_req.m_SeqIdType, psg_req.m_SeqId);
        req.SetSeq_id_type(CID2_Request_Get_Seq_id::eSeq_id_type_any);
        AddRequest(osg_req);
    }
}


void CPSGS_OSGResolve::ProcessReplies()
{
    bool got_blob_id = false;
    for ( auto& f : GetFetches() ) {
        for ( auto& r : f->GetReplies() ) {
            switch ( r->GetReply().Which() ) {
            case CID2_Reply::TReply::e_Init:
            case CID2_Reply::TReply::e_Empty:
                // do nothing
                break;
            case CID2_Reply::TReply::e_Get_seq_id:
                ProcessResolveReply(*r);
                break;
            case CID2_Reply::TReply::e_Get_blob_id:
                got_blob_id = true;
                ProcessResolveReply(*r);
                if ( m_BioseqInfoFlags & SPSGS_ResolveRequest::fPSGS_BlobId ) {
                    // resolved to OSG sequence
                    SignalStartProcessing();
                }
                break;
            default:
                ERR_POST(GetName()<<": "
                         "Unknown reply to "<<MSerial_AsnText<<*f->GetRequest()<<"\n"<<*r);
                break;
            }
        }
    }
    if ( m_BioseqInfoFlags == 0 ) {
        FinalizeResult(ePSGS_NotFound);
    }
    else if ( got_blob_id && !(m_BioseqInfoFlags & SPSGS_ResolveRequest::fPSGS_BlobId) ) {
        // got non-OSG blob-id -> non-OSG sequence
        FinalizeResult(ePSGS_NotFound);
    }
    else {
        auto& psg_req = GetRequest()->GetRequest<SPSGS_ResolveRequest>();
        if ( !psg_req.m_IncludeDataFlags ) {
            // only resolution is requested
            m_BioseqInfoFlags = 0;
        }
        SendBioseqInfo(GetRequest()->GetRequest<SPSGS_ResolveRequest>().m_OutputFormat);
        FinalizeResult(ePSGS_Found);
    }
}


CPSGS_OSGGetBlobBySeqId::CPSGS_OSGGetBlobBySeqId(TEnabledFlags enabled_flags,
                                                 const CRef<COSGConnectionPool>& pool,
                                                 const shared_ptr<CPSGS_Request>& request,
                                                 const shared_ptr<CPSGS_Reply>& reply,
                                                 TProcessorPriority priority)
    : CPSGS_OSGProcessorBase(enabled_flags, pool, request, reply, priority)
{
}


CPSGS_OSGGetBlobBySeqId::~CPSGS_OSGGetBlobBySeqId()
{
}


string CPSGS_OSGGetBlobBySeqId::GetName() const
{
    return "OSG-get";
}


bool CPSGS_OSGGetBlobBySeqId::CanProcess(TEnabledFlags enabled_flags,
                                         shared_ptr<CPSGS_Request>& request)
{
    if ( !(enabled_flags & fEnabledWGS) ) {
        return false;
    }
    auto& psg_req = request->GetRequest<SPSGS_BlobBySeqIdRequest>();
    return CanResolve(psg_req.m_SeqIdType, psg_req.m_SeqId);
}


void CPSGS_OSGGetBlobBySeqId::CreateRequests()
{
    auto& psg_req = GetRequest()->GetRequest<SPSGS_BlobBySeqIdRequest>();
    CRef<CID2_Request> osg_req(new CID2_Request);
    auto& get_req = osg_req->SetRequest().SetGet_blob_info();
    if ( psg_req.m_TSEOption != psg_req.ePSGS_NoneTSE ) {
        get_req.SetGet_data();
    }
    auto& req = get_req.SetBlob_id().SetResolve();
    SetSeqId(req.SetRequest().SetSeq_id().SetSeq_id().SetSeq_id(), psg_req.m_SeqIdType, psg_req.m_SeqId);
    for ( auto& excl_id : psg_req.m_ExcludeBlobs ) {
        if ( auto excl_blob_id = CPSGS_OSGGetBlobBase::ParsePSGBlobId(excl_id) ) {
            req.SetExclude_blobs().push_back(excl_blob_id);
        }
    }
    AddRequest(osg_req);
}


bool CPSGS_OSGGetBlobBySeqId::BlobIsExcluded(const string& psg_blob_id)
{
    auto& psg_req = GetRequest()->GetRequest<SPSGS_BlobBySeqIdRequest>();
    return find(psg_req.m_ExcludeBlobs.begin(), psg_req.m_ExcludeBlobs.end(), psg_blob_id) !=
        psg_req.m_ExcludeBlobs.end();
}


void CPSGS_OSGGetBlobBySeqId::ProcessReplies()
{
    for ( auto& f : GetFetches() ) {
        for ( auto& r : f->GetReplies() ) {
            switch ( r->GetReply().Which() ) {
            case CID2_Reply::TReply::e_Init:
            case CID2_Reply::TReply::e_Empty:
                // do nothing
                break;
            case CID2_Reply::TReply::e_Get_seq_id:
                ProcessResolveReply(*r);
                break;
            case CID2_Reply::TReply::e_Get_blob_id:
                ProcessResolveReply(*r);
                if ( m_BioseqInfoFlags & SPSGS_ResolveRequest::fPSGS_BlobId ) {
                    // resolved to OSG sequence
                    SignalStartProcessing();
                }
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
    if ( !(m_BioseqInfoFlags & SPSGS_ResolveRequest::fPSGS_BlobId) ) {
        FinalizeResult(ePSGS_NotFound);
    }
    else if ( HasBlob() ) {
        SendBioseqInfo(SPSGS_ResolveRequest::ePSGS_UnknownFormat);
        SendBlob();
        FinalizeResult(ePSGS_Found);
    }
    else if ( GetRequest()->GetRequest<SPSGS_BlobBySeqIdRequest>().m_TSEOption == SPSGS_BlobBySeqIdRequest::ePSGS_NoneTSE ) {
        SendBioseqInfo(SPSGS_ResolveRequest::ePSGS_UnknownFormat);
        FinalizeResult(ePSGS_Found);
    }
    else if ( BlobIsExcluded(m_BlobId) ) {
        SendBioseqInfo(SPSGS_ResolveRequest::ePSGS_UnknownFormat);
        SendExcludedBlob(m_BlobId);
        FinalizeResult(ePSGS_Found);
    }
    else {
        ERR_POST(GetName()<<": Unexpected missing blob");
        FinalizeResult(ePSGS_NotFound);
    }
}


END_NAMESPACE(osg);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;
