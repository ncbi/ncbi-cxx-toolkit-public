#ifndef GET_BLOB_CALLBACK__HPP
#define GET_BLOB_CALLBACK__HPP

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

#include "cass_fetch.hpp"
#include "pubseq_gateway.hpp"
#include "pubseq_gateway_convert_utils.hpp"

#include <functional>

using TBlobChunkCB = function<void(CCassBlobFetch *  fetch_details,
                                   CBlobRecord const &  blob,
                                   const unsigned char *  chunk_data,
                                   unsigned int  data_size,
                                   int  chunk_no)>;
using TBlobPropsCB = function<void(CCassBlobFetch *  fetch_details,
                                   CBlobRecord const &  blob,
                                   bool  is_found)>;
using TBlobErrorCB = function<void(CCassBlobFetch *  fetch_details,
                                   CRequestStatus::ECode  status,
                                   int  code,
                                   EDiagSev  severity,
                                   const string &  message)>;

class IPSGS_Processor;


class CBlobChunkCallback
{
    public:
        CBlobChunkCallback(IPSGS_Processor *  processor,
                           TBlobChunkCB  blob_chunk_cb,
                           CCassBlobFetch *  fetch_details,
                           EPSGOperation  retrieve_statistic=eBlobRetrieve) :
            m_Processor(processor),
            m_BlobChunkCB(blob_chunk_cb),
            m_FetchDetails(fetch_details),
            m_RetrieveStatistic(retrieve_statistic),
            m_BlobSize(0),
            m_BlobRetrieveTiming(psg_clock_t::now())
        {}

        void operator()(CBlobRecord const & blob,
                        const unsigned char *  chunk_data,
                        unsigned int  data_size, int  chunk_no)
        {
            if (chunk_no >= 0)
                m_BlobSize += data_size;
            else
                CPubseqGatewayApp::GetInstance()->GetTiming().
                    Register(m_Processor, m_RetrieveStatistic, eOpStatusFound,
                             m_BlobRetrieveTiming, m_BlobSize);

            m_BlobChunkCB(m_FetchDetails, blob, chunk_data, data_size, chunk_no);
        }

    private:
        IPSGS_Processor *       m_Processor;
        TBlobChunkCB            m_BlobChunkCB;
        CCassBlobFetch *        m_FetchDetails;
        EPSGOperation           m_RetrieveStatistic;

        unsigned long           m_BlobSize;
        psg_time_point_t        m_BlobRetrieveTiming;
};


class CBlobPropCallback
{
    public:
        CBlobPropCallback(
                IPSGS_Processor *  processor,
                TBlobPropsCB  blob_prop_cb,
                shared_ptr<CPSGS_Request>  request,
                shared_ptr<CPSGS_Reply>  reply,
                CCassBlobFetch *  fetch_details,
                bool  need_timing) :
            m_Processor(processor),
            m_BlobPropCB(blob_prop_cb),
            m_Request(request),
            m_Reply(reply),
            m_FetchDetails(fetch_details),
            m_InProcess(false),
            m_NeedTiming(need_timing)
        {
            if (need_timing)
                m_BlobPropTiming = psg_clock_t::now();
        }

        void operator()(CBlobRecord const &  blob, bool is_found)
        {
            if (!m_InProcess) {
                if (m_Request->NeedTrace()) {
                    if (is_found) {
                        m_Reply->SendTrace("Cassandra blob props: " +
                            ToJsonString(blob),
                            m_Request->GetStartTimestamp());
                    } else {
                        m_Reply->SendTrace("Cassandra blob props not found",
                                           m_Request->GetStartTimestamp());
                    }
                }

                if (m_NeedTiming) {
                    CPubseqGatewayApp *  app = CPubseqGatewayApp::GetInstance();
                    if (is_found)
                        app->GetTiming().Register(m_Processor,
                                                  eLookupCassBlobProp,
                                                  eOpStatusFound,
                                                  m_BlobPropTiming);
                    else
                        app->GetTiming().Register(m_Processor,
                                                  eLookupCassBlobProp,
                                                  eOpStatusNotFound,
                                                  m_BlobPropTiming);
                }

                m_InProcess = true;
                m_BlobPropCB(m_FetchDetails, blob, is_found);
                m_InProcess = false;
            }
        }

    private:
        IPSGS_Processor *               m_Processor;
        TBlobPropsCB                    m_BlobPropCB;
        shared_ptr<CPSGS_Request>       m_Request;
        shared_ptr<CPSGS_Reply>         m_Reply;
        CCassBlobFetch *                m_FetchDetails;

        // Avoid an infinite loop
        bool                            m_InProcess;

        // Timing
        bool                            m_NeedTiming;
        psg_time_point_t                m_BlobPropTiming;
};


class CGetBlobErrorCallback
{
    public:
        CGetBlobErrorCallback(IPSGS_Processor *  processor,
                              TBlobErrorCB  blob_error_cb,
                              CCassBlobFetch *  fetch_details,
                              EPSGOperation  retrieve_statistic=eBlobRetrieve) :
            m_Processor(processor),
            m_BlobErrorCB(blob_error_cb),
            m_FetchDetails(fetch_details),
            m_RetrieveStatistic(retrieve_statistic),
            m_BlobRetrieveTiming(psg_clock_t::now())
        {}

        void operator()(CRequestStatus::ECode  status,
                        int  code,
                        EDiagSev  severity,
                        const string &  message)
        {
            if (status == CRequestStatus::e404_NotFound)
                CPubseqGatewayApp::GetInstance()->GetTiming().
                    Register(m_Processor, m_RetrieveStatistic, eOpStatusNotFound,
                             m_BlobRetrieveTiming);
            m_BlobErrorCB(m_FetchDetails, status, code, severity, message);
        }

    private:
        IPSGS_Processor *   m_Processor;
        TBlobErrorCB        m_BlobErrorCB;
        CCassBlobFetch *    m_FetchDetails;
        EPSGOperation       m_RetrieveStatistic;

        psg_time_point_t    m_BlobRetrieveTiming;
};

#endif

