#ifndef IPG_RESOLVE_CALLBACK__HPP
#define IPG_RESOLVE_CALLBACK__HPP

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

#include "http_daemon.hpp"
#include "pending_operation.hpp"
#include "cass_fetch.hpp"

#include <functional>

using TIPGResolveCB = function<
                                bool(vector<CIpgStorageReportEntry> &&  page,
                                     bool  is_last,
                                     CCassIPGFetch *  fetch_details)>;
using TIPGResolveErrorCB = function<
                                void(CCassIPGFetch *  fetch_details,
                                     CRequestStatus::ECode  status,
                                     int  code,
                                     EDiagSev  severity,
                                     const string &  message)>;

class IPSGS_Processor;


class CIPGResolveCallback
{
    public:
        CIPGResolveCallback(
                IPSGS_Processor *  processor,
                TIPGResolveCB  ipg_resolve_cb,
                CCassIPGFetch *  fetch_details) :
            m_Processor(processor),
            m_IPGResolveCB(ipg_resolve_cb),
            m_FetchDetails(fetch_details),
            m_Count(0),
            m_RetrieveTiming(psg_clock_t::now())
        {}

        bool operator()(vector<CIpgStorageReportEntry> &&  page, bool  is_last)
        {
            if (is_last) {
                auto    app = CPubseqGatewayApp::GetInstance();
                if (m_Count == 0) {
                    app->GetTiming().Register(m_Processor, eIPGResolveRetrieve,
                                              eOpStatusNotFound,
                                              m_RetrieveTiming);
                    app->GetCounters().Increment(
                                    m_Processor,
                                    CPSGSCounters::ePSGS_IPGResolveNotFound);
                } else {
                    app->GetTiming().Register(m_Processor, eIPGResolveRetrieve,
                                              eOpStatusFound,
                                              m_RetrieveTiming);
                }
            }

            ++m_Count;
            return m_IPGResolveCB(std::move(page), is_last, m_FetchDetails);
        }

    private:
        IPSGS_Processor *       m_Processor;
        TIPGResolveCB           m_IPGResolveCB;
        CCassIPGFetch *         m_FetchDetails;

        size_t                  m_Count;
        psg_time_point_t        m_RetrieveTiming;
};


class CIPGResolveErrorCallback
{
    public:
        CIPGResolveErrorCallback(
                TIPGResolveErrorCB  error_cb,
                CCassIPGFetch *  fetch_details) :
            m_ErrorCB(error_cb),
            m_FetchDetails(fetch_details)
        {}

        void operator()(CRequestStatus::ECode  status,
                        int  code,
                        EDiagSev  severity,
                        const string &  message)
        {
            m_ErrorCB(m_FetchDetails, status, code, severity, message);
        }

    private:
        TIPGResolveErrorCB          m_ErrorCB;
        CCassIPGFetch *             m_FetchDetails;
};


#endif

