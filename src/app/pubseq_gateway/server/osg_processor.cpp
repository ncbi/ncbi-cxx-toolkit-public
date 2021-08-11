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


static pair<bool, CPSGS_OSGProcessorBase::TEnabledFlags> s_ParseEnabledFlags(const vector<string>& names)
{
    pair<bool, CPSGS_OSGProcessorBase::TEnabledFlags> ret(false, 0);
    for (const auto& name : names ) {
        if ( NStr::EqualNocase(name, "osg") ) {
            ret.first = true;
            continue;
        }
        if ( NStr::EqualNocase(name, "osg-wgs") ) {
            ret.second |= CPSGS_OSGProcessorBase::fEnabledWGS;
            continue;
        }
        if ( NStr::EqualNocase(name, "osg-snp") ) {
            ret.second |= CPSGS_OSGProcessorBase::fEnabledSNP;
            continue;
        }
        if ( NStr::EqualNocase(name, "osg-cdd") ) {
            ret.second |= CPSGS_OSGProcessorBase::fEnabledCDD;
            continue;
        }
    }
    return ret;
}


IPSGS_Processor*
CPSGS_OSGProcessor::CreateProcessor(shared_ptr<CPSGS_Request> request,
                                    shared_ptr<CPSGS_Reply> reply,
                                    TProcessorPriority priority) const
{
    auto& req_base = request->GetRequest<SPSGS_RequestBase>();
    if ( req_base.m_Hops > 0 ) {
        return nullptr;
    }
    auto enabled_explicitly = s_ParseEnabledFlags(req_base.m_EnabledProcessors);
    auto disabled_explicitly = s_ParseEnabledFlags(req_base.m_DisabledProcessors);
    auto app = CPubseqGatewayApp::GetInstance();
    auto conn_pool = app->GetOSGConnectionPool();
    bool enabled_main = app->GetOSGProcessorsEnabled();
    enabled_main |= enabled_explicitly.first;
    enabled_main &= !disabled_explicitly.first;
    CPSGS_OSGProcessorBase::TEnabledFlags enabled_flags = enabled_main? conn_pool->GetDefaultEnabledFlags(): 0;
    enabled_flags |= enabled_explicitly.second;
    enabled_flags &= ~disabled_explicitly.second;
    if ( !enabled_flags ) {
        return nullptr;
    }
    
    switch ( request->GetRequestType() ) {
    case CPSGS_Request::ePSGS_ResolveRequest:
        // VDB WGS sequences
        if ( CPSGS_OSGResolve::CanProcess(enabled_flags, request) ) {
            return new CPSGS_OSGResolve(enabled_flags, conn_pool, request, reply, priority);
        }
        return nullptr;

    case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
        // VDB WGS sequences
        if ( CPSGS_OSGGetBlobBySeqId::CanProcess(enabled_flags, request) ) {
            return new CPSGS_OSGGetBlobBySeqId(enabled_flags, conn_pool, request, reply, priority);
        }
        return nullptr;

    case CPSGS_Request::ePSGS_BlobBySatSatKeyRequest:
        if ( CPSGS_OSGGetBlob::CanProcess(enabled_flags, request) ) {
            return new CPSGS_OSGGetBlob(enabled_flags, conn_pool, request, reply, priority);
        }
        return nullptr;

    case CPSGS_Request::ePSGS_TSEChunkRequest:
        if ( CPSGS_OSGGetChunks::CanProcess(enabled_flags, request) ) {
            return new CPSGS_OSGGetChunks(enabled_flags, conn_pool, request, reply, priority);
        }
        return nullptr;

    case CPSGS_Request::ePSGS_AnnotationRequest:
        if ( CPSGS_OSGAnnot::CanProcess(enabled_flags, request, priority) ) {
            return new CPSGS_OSGAnnot(enabled_flags, conn_pool, request, reply, priority);
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
