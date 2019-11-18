/* ===========================================================================
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
 */


#include <ncbi_pch.hpp>

#include <objects/dbsnp/primary_track/snpptis.hpp>
#include <objects/dbsnp/primary_track/impl/snpptis_impl.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <corelib/ncbi_param.hpp>

#include <misc/grpc_integration/grpc_integration.hpp>

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);


CSnpPtisClient::CSnpPtisClient()
{
}


CSnpPtisClient::~CSnpPtisClient()
{
}


CRef<CSnpPtisClient> CSnpPtisClient::CreateClient()
{
    CRef<CSnpPtisClient> client;
    client = new CSnpPtisClient_Impl();
    return client;
}


CSnpPtisClient_Impl::CSnpPtisClient_Impl()
{
    channel = grpc::CreateChannel(g_NCBI_GRPC_GetAddress("ID2SNP", "PTIS_NAME"),
                                  grpc::InsecureChannelCredentials());

    stub = ncbi::grpcapi::dbsnp::primary_track::DbSnpPrimaryTrack::NewStub(channel);
}


CSnpPtisClient_Impl::~CSnpPtisClient_Impl()
{
}


string CSnpPtisClient_Impl::GetPrimarySnpTrackForGi(TGi gi)
{
    ncbi::grpcapi::dbsnp::primary_track::SeqIdRequestStringAccverUnion request;
    request.set_gi(TIntId(gi));
    return x_GetPrimarySnpTrack(request);
}

string CSnpPtisClient_Impl::GetPrimarySnpTrackForAccVer(const string& acc_ver)
{
    ncbi::grpcapi::dbsnp::primary_track::SeqIdRequestStringAccverUnion request;
    request.set_accver(acc_ver);
    return x_GetPrimarySnpTrack(request);
}


string CSnpPtisClient::GetPrimarySnpTrackForId(const string& id)
{
    return GetPrimarySnpTrackForId(CSeq_id(id));
}


string CSnpPtisClient::GetPrimarySnpTrackForId(const CSeq_id& id)
{
    if ( id.IsGi() ) {
        return GetPrimarySnpTrackForGi(id.GetGi());
    }
    else if ( const CTextseq_id* text_id = id.GetTextseq_Id() ) {
        return GetPrimarySnpTrackForAccVer(text_id->GetAccession()+'.'+NStr::NumericToString(text_id->GetVersion()));
    }
    else {
        NCBI_THROW_FMT(CException, eUnknown, "Invalid Seq-id type: "<<id.AsFastaString());
    }
}


string CSnpPtisClient_Impl::x_GetPrimarySnpTrack(const TRequest& request)
{
    CGRPCClientContext context;
    
    ncbi::grpcapi::dbsnp::primary_track::PrimaryTrackReply reply;

    auto status = stub->ForSeqId(&context, request, &reply);
    
    if ( !status.ok() ) {
        if ( status.error_code() == grpc::StatusCode::NOT_FOUND ) {
            return string();
        }
        NCBI_THROW(CException, eUnknown, status.error_message());
    }

    // cout << reply.na_track_acc_with_filter() << "\t" << reply.tms_track_id() << endl;

    return reply.na_track_acc_with_filter();
}


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;
