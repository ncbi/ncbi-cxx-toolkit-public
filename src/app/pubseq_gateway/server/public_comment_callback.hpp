#ifndef PUBLIC_COMMENT_CALLBACK__HPP
#define PUBLIC_COMMENT_CALLBACK__HPP

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
#include "cass_fetch.hpp"

#include <functional>


using TPublicCommentConsumeCB =
                function<void(CCassPublicCommentFetch *  fetch_details,
                              string  comment,
                              bool  is_found)>;
using TPublicCommentErrorCB =
                function<void(CCassPublicCommentFetch *  fetch_details,
                              CRequestStatus::ECode  status,
                              int  code,
                              EDiagSev  severity,
                              const string &  message)>;


class CPublicCommentConsumeCallback
{
    public:
        CPublicCommentConsumeCallback(
                TPublicCommentConsumeCB  consume_cb,
                CCassPublicCommentFetch *  fetch_details) :
            m_ConsumeCB(consume_cb),
            m_FetchDetails(fetch_details),
            m_PublicCommentRetrieveTiming(chrono::high_resolution_clock::now())
        {}

        void operator()(string  comment, bool  is_found)
        {
            EPSGOperationStatus     op_status = eOpStatusFound;
            if (!is_found)
                op_status = eOpStatusNotFound;

            CPubseqGatewayApp::GetInstance()->GetTiming().
                Register(ePublicCommentRetrieve, op_status,
                         m_PublicCommentRetrieveTiming);

            m_ConsumeCB(m_FetchDetails, comment, is_found);
        }

    private:
        TPublicCommentConsumeCB                         m_ConsumeCB;
        CCassPublicCommentFetch *                       m_FetchDetails;
        chrono::high_resolution_clock::time_point       m_PublicCommentRetrieveTiming;
};


class CPublicCommentErrorCallback
{
    public:
        CPublicCommentErrorCallback(
                TPublicCommentErrorCB  error_cb,
                CCassPublicCommentFetch *  fetch_details) :
            m_ErrorCB(error_cb),
            m_FetchDetails(fetch_details),
            m_PublicCommentRetrieveTiming(chrono::high_resolution_clock::now())
        {}

        void operator()(CRequestStatus::ECode  status,
                        int  code,
                        EDiagSev  severity,
                        const string &  message)
        {
            if (status == CRequestStatus::e404_NotFound)
                CPubseqGatewayApp::GetInstance()->GetTiming().
                    Register(ePublicCommentRetrieve, eOpStatusNotFound,
                             m_PublicCommentRetrieveTiming);
            m_ErrorCB(m_FetchDetails, status, code, severity, message);
        }

    private:
        TPublicCommentErrorCB                           m_ErrorCB;
        CCassPublicCommentFetch *                       m_FetchDetails;
        chrono::high_resolution_clock::time_point       m_PublicCommentRetrieveTiming;
};


#endif

