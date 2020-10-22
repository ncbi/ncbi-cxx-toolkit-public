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

#include "osg_processor.hpp"
#include "osg_resolve.hpp"
#include "osg_getblob.hpp"
#include "osg_annot.hpp"
#include "osg_connection.hpp"
#include "pubseq_gateway.hpp"
#include <objects/id2/ID2_Blob_Id.hpp>


BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(osg);


CPSGS_OSGProcessor::CPSGS_OSGProcessor()
{
}


CPSGS_OSGProcessor::~CPSGS_OSGProcessor()
{
}


IPSGS_Processor*
CPSGS_OSGProcessor::CreateProcessor(shared_ptr<CPSGS_Request> request,
                                    shared_ptr<CPSGS_Reply> reply,
                                    TProcessorPriority priority) const
{
    auto app = CPubseqGatewayApp::GetInstance();
    if ( !app->GetOSGEnabled() ) {
        return nullptr;
    }
    
    switch ( request->GetRequestType() ) {
    case CPSGS_Request::ePSGS_ResolveRequest:
        // VDB WGS sequences
        if ( CPSGS_OSGResolve::CanProcess(request->GetRequest<SPSGS_ResolveRequest>()) ) {
            return new CPSGS_OSGResolve(app->GetOSGConnectionPool(), request, reply, priority);
        }
        return nullptr;

    case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
        // VDB WGS sequences
        if ( CPSGS_OSGGetBlobBySeqId::CanProcess(request->GetRequest<SPSGS_BlobBySeqIdRequest>()) ) {
            return new CPSGS_OSGGetBlobBySeqId(app->GetOSGConnectionPool(), request, reply, priority);
        }
        return nullptr;

    case CPSGS_Request::ePSGS_BlobBySatSatKeyRequest:
        if ( CPSGS_OSGGetBlob::CanProcess(request->GetRequest<SPSGS_BlobBySatSatKeyRequest>()) ) {
            return new CPSGS_OSGGetBlob(app->GetOSGConnectionPool(), request, reply, priority);
        }
        return nullptr;

    case CPSGS_Request::ePSGS_TSEChunkRequest:
        if ( CPSGS_OSGGetChunks::CanProcess(request->GetRequest<SPSGS_TSEChunkRequest>()) ) {
            return new CPSGS_OSGGetChunks(app->GetOSGConnectionPool(), request, reply, priority);
        }
        return nullptr;

    case CPSGS_Request::ePSGS_AnnotationRequest:
        if ( CPSGS_OSGAnnot::CanProcess(request->GetRequest<SPSGS_AnnotRequest>(), priority) ) {
            return new CPSGS_OSGAnnot(app->GetOSGConnectionPool(), request, reply, priority);
        }
        return nullptr;

    default:
        return nullptr;
    }
}


string CPSGS_OSGProcessor::GetName() const
{
    return "OSG";
}


void CPSGS_OSGProcessor::Process()
{
}


void CPSGS_OSGProcessor::Cancel()
{
}


IPSGS_Processor::EPSGS_Status CPSGS_OSGProcessor::GetStatus()
{
    return ePSGS_Error; // not supposed to process any requests
}


END_NAMESPACE(osg);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;
