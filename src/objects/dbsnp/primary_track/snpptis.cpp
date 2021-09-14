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
 */


#include <ncbi_pch.hpp>
#include <ncbiconf.h>

#include <objects/dbsnp/primary_track/snpptis.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <serial/grpc_integration/grpc_integration.hpp>
#ifdef HAVE_LIBGRPC
# include <objects/dbsnp/primary_track/impl/snpptis_impl.hpp>
# include <corelib/ncbi_param.hpp>
# include <corelib/ncbi_system.hpp>
#endif

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);


CSnpPtisClient::CSnpPtisClient()
{
}


CSnpPtisClient::~CSnpPtisClient()
{
}


#ifdef HAVE_LIBGRPC
const char* const kSection = "ID2SNP";
const char* const kParam_PTISName = "PTIS_NAME";
const char* const kParam_Retry = "RETRY";
const char* const kParam_Timeout    = "TIMEOUT";
const char* const kParam_TimeoutMul = "TIMEOUT_MULTIPLIER";
const char* const kParam_TimeoutInc = "TIMEOUT_INCREMENT";
const char* const kParam_TimeoutMax = "TIMEOUT_MAX";
const char* const kParam_WaitTime    = "WAIT_TIME";
const char* const kParam_WaitTimeMul = "WAIT_TIME_MULTIPLIER";
const char* const kParam_WaitTimeInc = "WAIT_TIME_INCREMENT";
const char* const kParam_WaitTimeMax = "WAIT_TIME_MAX";
const char* const kParam_Debug = "PTIS_DEBUG";
const int kDefault_Retry = 5;
const float kDefault_Timeout    = 1;
const float kDefault_TimeoutMul = 1.5;
const float kDefault_TimeoutInc = 0;
const float kDefault_TimeoutMax = 10;
const float kDefault_WaitTime    = 0.5;
const float kDefault_WaitTimeMul = 1.2f;
const float kDefault_WaitTimeInc = 0.2f;
const float kDefault_WaitTimeMax = 5;
const int kDefault_Debug = 1; // errors only

NCBI_PARAM_DECL(int, ID2SNP, PTIS_DEBUG);
NCBI_PARAM_DEF(int, ID2SNP, PTIS_DEBUG, kDefault_Debug);
#endif


bool CSnpPtisClient::IsEnabled()
{
#ifdef HAVE_LIBGRPC
    if ( !CGRPCClientContext::IsImplemented() ) {
        return false;
    }
    // check if there's valid address
    CParamBase::EParamSource source;
    auto addr = g_NCBI_GRPC_GetAddress(kSection, kParam_PTISName, nullptr, &source);
#ifndef NCBI_OS_LINUX
    if ( source == CParamBase::eSource_Default ) {
        // default grpc link to linkerd daemon works on Linux only
        return false;
    }
#endif
    return !addr.empty();
#else
    return false;
#endif
}


static int GetDebugLevel()
{
#ifdef HAVE_LIBGRPC
    return NCBI_PARAM_TYPE(ID2SNP, PTIS_DEBUG)::GetDefault();
#else
    return 0;
#endif
}


CRef<CSnpPtisClient> CSnpPtisClient::CreateClient()
{
#ifdef HAVE_LIBGRPC
    CRef<CSnpPtisClient> client;
    client = new CSnpPtisClient_Impl();
    return client;
#else
    ERR_POST_ONCE("CSnpPtisClient is disabled due to lack of GRPC support");
    NCBI_THROW(CException, eUnknown, "CSnpPtisClient is disabled due to lack of GRPC support");
#endif
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


#ifdef HAVE_LIBGRPC
CSnpPtisClient_Impl::CSnpPtisClient_Impl()
{
    grpc::ChannelArguments args;
    string address = g_NCBI_GRPC_GetAddress(kSection, kParam_PTISName);
    //LOG_POST(Trace<<"CSnpPtisClient: connecting to "<<address);
    channel = grpc::CreateCustomChannel(address, grpc::InsecureChannelCredentials(), args);
    max_retries = g_GetConfigInt(kSection, kParam_Retry, nullptr, kDefault_Retry);
    timeout     = g_GetConfigDouble(kSection, kParam_Timeout   , nullptr, kDefault_Timeout   );
    timeout_mul = g_GetConfigDouble(kSection, kParam_TimeoutMul, nullptr, kDefault_TimeoutMul);
    timeout_inc = g_GetConfigDouble(kSection, kParam_TimeoutInc, nullptr, kDefault_TimeoutInc);
    timeout_max = g_GetConfigDouble(kSection, kParam_TimeoutMax, nullptr, kDefault_TimeoutMax);
    wait_time     = g_GetConfigDouble(kSection, kParam_WaitTime   , nullptr, kDefault_WaitTime   );
    wait_time_mul = g_GetConfigDouble(kSection, kParam_WaitTimeMul, nullptr, kDefault_WaitTimeMul);
    wait_time_inc = g_GetConfigDouble(kSection, kParam_WaitTimeInc, nullptr, kDefault_WaitTimeInc);
    wait_time_max = g_GetConfigDouble(kSection, kParam_WaitTimeMax, nullptr, kDefault_WaitTimeMax);
    
    stub = ncbi::grpcapi::dbsnp::primary_track::DbSnpPrimaryTrack::NewStub(channel);
}


CSnpPtisClient_Impl::~CSnpPtisClient_Impl()
{
}


string CSnpPtisClient_Impl::GetPrimarySnpTrackForGi(TGi gi)
{
    ncbi::grpcapi::dbsnp::primary_track::SeqIdRequestStringAccverUnion request;
    if ( GetDebugLevel() >= 5 ) {
        LOG_POST(Info << "CSnpPtisClient: asking for gi "<<gi);
    }
    request.set_gi(GI_TO(TIntId, gi));
    return x_GetPrimarySnpTrack(request);
}

string CSnpPtisClient_Impl::GetPrimarySnpTrackForAccVer(const string& acc_ver)
{
    ncbi::grpcapi::dbsnp::primary_track::SeqIdRequestStringAccverUnion request;
    if ( GetDebugLevel() >= 5 ) {
        LOG_POST(Info << "CSnpPtisClient: asking for acc_ver "<<acc_ver);
    }
    request.set_accver(acc_ver);
    return x_GetPrimarySnpTrack(request);
}


string CSnpPtisClient_Impl::x_GetPrimarySnpTrack(const TRequest& request)
{
    int cur_retry = 0;
    float cur_timeout = timeout;
    float cur_wait_time = wait_time;
    for ( ;; ) {
        CGRPCClientContext context;
        std::chrono::system_clock::time_point deadline =
            std::chrono::system_clock::now() + std::chrono::microseconds(Int8(cur_timeout*1e6));
        context.set_deadline(deadline);
    
        ncbi::grpcapi::dbsnp::primary_track::PrimaryTrackReply reply;
        
        auto status = stub->ForSeqId(&context, request, &reply);
        
        if ( status.ok() ) {
            if ( GetDebugLevel() >= 5 ) {
                LOG_POST(Info << "CSnpPtisClient: received "<<reply.na_track_acc_with_filter());
            }
            return reply.na_track_acc_with_filter();
        }
        
        if ( status.error_code() == grpc::StatusCode::NOT_FOUND ) {
            if ( GetDebugLevel() >= 5 ) {
                LOG_POST(Info << "CSnpPtisClient: received NOT_FOUND");
            }
            return string();
        }
        if ( ++cur_retry >= max_retries ) {
            if ( GetDebugLevel() >= 5 ) {
                LOG_POST(Info << "CSnpPtisClient: failed: "<<status.error_message());
            }
            NCBI_THROW(CException, eUnknown, status.error_message());
        }
        ERR_POST(Trace<<
                 "CSnpPtisClient: failed : "<<status.error_message()<<". "
                 "Waiting "<<cur_wait_time<<" seconds before retry...");
        SleepMicroSec(Int8(cur_wait_time*1e6));
        cur_timeout = min(cur_timeout*timeout_mul + timeout_inc, timeout_max);
        cur_wait_time = min(cur_wait_time*wait_time_mul + wait_time_inc, wait_time_max);
    }
}
#endif

END_NAMESPACE(objects);
END_NCBI_NAMESPACE;
