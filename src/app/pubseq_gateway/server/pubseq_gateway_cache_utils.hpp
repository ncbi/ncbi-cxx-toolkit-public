#ifndef PUBSEQ_GATEWAY_CACHE_UTILS__HPP
#define PUBSEQ_GATEWAY_CACHE_UTILS__HPP

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
 * Authors: Sergey Satskiy
 *
 * File Description:
 *
 */

#include <objtools/pubseq_gateway/impl/cassandra/blob_record.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info/record.hpp>
#include "pubseq_gateway_types.hpp"
#include "pubseq_gateway_utils.hpp"
#include "psgs_request.hpp"

#include <string>
using namespace std;

USING_IDBLOB_SCOPE;

class CPSGS_Reply;

class CPSGCache
{
public:
    CPSGCache(shared_ptr<CPSGS_Request>  request,
              shared_ptr<CPSGS_Reply>  reply) :
        m_Allowed(false),
        m_NeedTrace(request->NeedTrace()),
        m_Request(request),
        m_Reply(reply)
    {
        switch (request->GetRequestType()) {
            case CPSGS_Request::ePSGS_ResolveRequest:
                m_Allowed =
                    request->GetRequest<SPSGS_ResolveRequest>().m_UseCache !=
                        SPSGS_RequestBase::ePSGS_DbOnly;
                break;
            case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
                m_Allowed =
                    request->GetRequest<SPSGS_BlobBySeqIdRequest>().m_UseCache !=
                        SPSGS_RequestBase::ePSGS_DbOnly;
                break;
            case CPSGS_Request::ePSGS_BlobBySatSatKeyRequest:
                m_Allowed =
                    request->GetRequest<SPSGS_BlobBySatSatKeyRequest>().m_UseCache !=
                        SPSGS_RequestBase::ePSGS_DbOnly;
                break;
            case CPSGS_Request::ePSGS_AnnotationRequest:
                m_Allowed =
                    request->GetRequest<SPSGS_AnnotRequest>().m_UseCache !=
                        SPSGS_RequestBase::ePSGS_DbOnly;
                break;
            case CPSGS_Request::ePSGS_TSEChunkRequest:
                m_Allowed =
                    request->GetRequest<SPSGS_TSEChunkRequest>().m_UseCache !=
                        SPSGS_RequestBase::ePSGS_DbOnly;
                break;
            default:
                ;
        }
    }

    CPSGCache(bool  allowed,
              shared_ptr<CPSGS_Request>  request,
              shared_ptr<CPSGS_Reply>  reply) :
        m_Allowed(allowed),
        m_NeedTrace(request->NeedTrace()),
        m_Request(request),
        m_Reply(reply)
    {}

    bool IsAllowed(void) const
    {
        return m_Allowed;
    }

    EPSGS_CacheLookupResult  LookupBioseqInfo(SBioseqResolution &  bioseq_resolution)
    {
        if (m_Allowed)
            return x_LookupBioseqInfo(bioseq_resolution);
        return ePSGS_CacheNotHit;
    }

    EPSGS_CacheLookupResult  LookupSi2csi(SBioseqResolution &  bioseq_resolution)
    {
        if (m_Allowed)
            return x_LookupSi2csi(bioseq_resolution);
        return ePSGS_CacheNotHit;
    }

    EPSGS_CacheLookupResult  LookupBlobProp(int  sat,
                                            int  sat_key,
                                            int64_t &  last_modified,
                                            CBlobRecord &  blob_record)
    {
        if (m_Allowed)
            return x_LookupBlobProp(sat, sat_key, last_modified, blob_record);
        return ePSGS_CacheNotHit;
    }

private:
    EPSGS_CacheLookupResult  x_LookupBioseqInfo(
                                SBioseqResolution &  bioseq_resolution);
    EPSGS_CacheLookupResult  x_LookupINSDCBioseqInfo(
                                SBioseqResolution &  bioseq_resolution);

    EPSGS_CacheLookupResult  x_LookupSi2csi(
                                SBioseqResolution &  bioseq_resolution);

    EPSGS_CacheLookupResult  x_LookupBlobProp(int  sat,
                                              int  sat_key,
                                              int64_t &  last_modified,
                                              CBlobRecord &  blob_record);

private:
    bool                        m_Allowed;
    bool                        m_NeedTrace;
    shared_ptr<CPSGS_Request>   m_Request;
    shared_ptr<CPSGS_Reply>     m_Reply;
};

#endif
