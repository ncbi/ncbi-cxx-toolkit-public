#ifndef SRA__READER__SRA__IMPL__SNPPTIS__HPP
#define SRA__READER__SRA__IMPL__SNPPTIS__HPP
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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Access to primary SNP track service
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <objects/dbsnp/primary_track/snpptis.hpp>

#include <grpc++/grpc++.h>
#include <objects/dbsnp/primary_track/dbsnp.pb.h>
#include <objects/dbsnp/primary_track/dbsnp.grpc.pb.h>

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);

class CSnpPtisClient_Impl : public CSnpPtisClient
{
public:
    CSnpPtisClient_Impl();
    virtual ~CSnpPtisClient_Impl();

    virtual string GetPrimarySnpTrackForGi(TGi gi);
    virtual string GetPrimarySnpTrackForAccVer(const string& acc_ver);

private:

    typedef ncbi::grpcapi::dbsnp::primary_track::SeqIdRequestStringAccverUnion TRequest;

    string x_GetPrimarySnpTrack(const TRequest& request);

    int max_retries;
    float timeout;
    float timeout_mul;
    float timeout_inc;
    float timeout_max;
    float wait_time;
    float wait_time_mul;
    float wait_time_inc;
    float wait_time_max;
    shared_ptr<grpc::Channel> channel;
    unique_ptr<ncbi::grpcapi::dbsnp::primary_track::DbSnpPrimaryTrack::Stub> stub;
};

END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // SRA__READER__SRA__IMPL__SNPPTIS__HPP
