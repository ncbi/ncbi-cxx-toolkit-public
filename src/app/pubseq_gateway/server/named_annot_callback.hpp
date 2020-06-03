#ifndef NAMED_ANNOT_CALLBACK__HPP
#define NAMED_ANNOT_CALLBACK__HPP

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

#include "http_server_transport.hpp"
#include "pending_operation.hpp"
#include "cass_fetch.hpp"

#include <functional>

using TNamedAnnotationCB = function<
                                bool(CNAnnotRecord &&  annot_record,
                                     bool  last,
                                     CCassNamedAnnotFetch *  fetch_details,
                                     int32_t  sat)>;
using TNamedAnnotationErrorCB = function<
                                void(CCassNamedAnnotFetch *  fetch_details,
                                     CRequestStatus::ECode  status,
                                     int  code,
                                     EDiagSev  severity,
                                     const string &  message)>;


class CNamedAnnotationCallback
{
    public:
        CNamedAnnotationCallback(
                TNamedAnnotationCB  named_annotation_cb,
                CCassNamedAnnotFetch *  fetch_details,
                int32_t  sat) :
            m_NamedAnnotationCB(named_annotation_cb),
            m_FetchDetails(fetch_details),
            m_Sat(sat),
            m_AnnotCount(0),
            m_RetrieveTiming(chrono::high_resolution_clock::now())
        {}

        bool operator()(CNAnnotRecord &&  annot_record, bool  last)
        {
            if (last) {
                auto    app = CPubseqGatewayApp::GetInstance();
                if (m_AnnotCount == 0)
                    app->GetTiming().Register(eNARetrieve, eOpStatusNotFound,
                                              m_RetrieveTiming);
                else
                    app->GetTiming().Register(eNARetrieve, eOpStatusFound,
                                              m_RetrieveTiming);
            }

            ++m_AnnotCount;
            return m_NamedAnnotationCB(move(annot_record),
                                       last, m_FetchDetails, m_Sat);
        }

    private:
        TNamedAnnotationCB                      m_NamedAnnotationCB;
        CCassNamedAnnotFetch *                  m_FetchDetails;
        int32_t                                 m_Sat;

        size_t                                      m_AnnotCount;
        chrono::high_resolution_clock::time_point   m_RetrieveTiming;
};


class CNamedAnnotationErrorCallback
{
    public:
        CNamedAnnotationErrorCallback(
                TNamedAnnotationErrorCB  error_cb,
                CCassNamedAnnotFetch *  fetch_details) :
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
        TNamedAnnotationErrorCB     m_ErrorCB;
        CCassNamedAnnotFetch *      m_FetchDetails;
};


#endif
