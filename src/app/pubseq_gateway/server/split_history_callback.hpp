#ifndef SPLIT_HISTORY_CALLBACK__HPP
#define SPLIT_HISTORY_CALLBACK__HPP

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


using TSplitHistoryConsumeCB =
                function<void(CCassSplitHistoryFetch *  fetch_details,
                              vector<SSplitHistoryRecord> && result)>;
using TSplitHistoryErrorCB =
                function<void(CCassSplitHistoryFetch *  fetch_details,
                              CRequestStatus::ECode  status,
                              int  code,
                              EDiagSev  severity,
                              const string &  message)>;


class CSplitHistoryConsumeCallback
{
    public:
        CSplitHistoryConsumeCallback(
                TSplitHistoryConsumeCB  consume_cb,
                CCassSplitHistoryFetch *  fetch_details) :
            m_ConsumeCB(consume_cb),
            m_FetchDetails(fetch_details),
            m_SplitHistoryRetrieveTiming(chrono::high_resolution_clock::now())
        {}

        void operator()(vector<SSplitHistoryRecord> && result)
        {
            EPSGOperationStatus     op_status = eOpStatusFound;
            if (result.empty())
                op_status = eOpStatusNotFound;

            CPubseqGatewayApp::GetInstance()->GetTiming().
                Register(eSplitHistoryRetrieve, op_status,
                         m_SplitHistoryRetrieveTiming);

            m_ConsumeCB(m_FetchDetails, std::move(result));
        }

    private:
        TSplitHistoryConsumeCB                          m_ConsumeCB;
        CCassSplitHistoryFetch *                        m_FetchDetails;
        chrono::high_resolution_clock::time_point       m_SplitHistoryRetrieveTiming;
};


class CSplitHistoryErrorCallback
{
    public:
        CSplitHistoryErrorCallback(
                TSplitHistoryErrorCB  error_cb,
                CCassSplitHistoryFetch *  fetch_details) :
            m_ErrorCB(error_cb),
            m_FetchDetails(fetch_details),
            m_SplitHistoryRetrieveTiming(chrono::high_resolution_clock::now())
        {}

        void operator()(CRequestStatus::ECode  status,
                        int  code,
                        EDiagSev  severity,
                        const string &  message)
        {
            if (status == CRequestStatus::e404_NotFound)
                CPubseqGatewayApp::GetInstance()->GetTiming().
                    Register(eSplitHistoryRetrieve, eOpStatusNotFound,
                             m_SplitHistoryRetrieveTiming);
            m_ErrorCB(m_FetchDetails, status, code, severity, message);
        }

    private:
        TSplitHistoryErrorCB                            m_ErrorCB;
        CCassSplitHistoryFetch *                        m_FetchDetails;
        chrono::high_resolution_clock::time_point       m_SplitHistoryRetrieveTiming;
};


#endif

