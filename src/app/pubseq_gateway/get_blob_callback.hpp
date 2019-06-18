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

#include "http_server_transport.hpp"
#include "cass_fetch.hpp"


class CPendingOperation;
class CProtocolUtils;

class CBlobChunkCallback
{
    public:
        CBlobChunkCallback(
                CPendingOperation *  pending_op,
                CCassBlobFetch *  fetch_details) :
            m_PendingOp(pending_op),
            m_FetchDetails(fetch_details),
            m_BlobSize(0),
            m_BlobRetrieveTiming(chrono::high_resolution_clock::now())
        {}

        void operator()(CBlobRecord const & blob, const unsigned char *  data,
                        unsigned int  size, int  chunk_no)
        {
            if (chunk_no >= 0)
                m_BlobSize += size;
            else
                CPubseqGatewayApp::GetInstance()->GetTiming().
                    Register(eBlobRetrieve, eOpStatusFound,
                             m_BlobRetrieveTiming, m_BlobSize);

            m_PendingOp->OnGetBlobChunk(m_FetchDetails, blob, data, size, chunk_no);
        }

    private:
        CPendingOperation *     m_PendingOp;
        CCassBlobFetch *        m_FetchDetails;

        unsigned long                                   m_BlobSize;
        chrono::high_resolution_clock::time_point       m_BlobRetrieveTiming;
};


class CBlobPropCallback
{
    public:
        CBlobPropCallback(
                CPendingOperation *  pending_op,
                CCassBlobFetch *  fetch_details,
                bool  need_timing) :
            m_PendingOp(pending_op),
            m_FetchDetails(fetch_details),
            m_InProcess(false),
            m_NeedTiming(need_timing)
        {
            if (need_timing)
                m_BlobPropTiming = chrono::high_resolution_clock::now();
        }

        void operator()(CBlobRecord const &  blob, bool is_found)
        {
            if (!m_InProcess) {
                if (m_NeedTiming) {
                    CPubseqGatewayApp *  app = CPubseqGatewayApp::GetInstance();
                    if (is_found)
                        app->GetTiming().Register(eLookupCassBlobProp,
                                                  eOpStatusFound,
                                                  m_BlobPropTiming);
                    else
                        app->GetTiming().Register(eLookupCassBlobProp,
                                                  eOpStatusNotFound,
                                                  m_BlobPropTiming);
                }

                m_InProcess = true;
                m_PendingOp->OnGetBlobProp(m_FetchDetails, blob, is_found);
                m_InProcess = false;
            }
        }

    private:
        CPendingOperation *                     m_PendingOp;
        CCassBlobFetch *                        m_FetchDetails;

        // Avoid an infinite loop
        bool            m_InProcess;

        // Timing
        bool                                            m_NeedTiming;
        chrono::high_resolution_clock::time_point       m_BlobPropTiming;
};


class CGetBlobErrorCallback
{
    public:
        CGetBlobErrorCallback(
                CPendingOperation *  pending_op,
                CCassBlobFetch *  fetch_details) :
            m_PendingOp(pending_op),
            m_FetchDetails(fetch_details),
            m_BlobRetrieveTiming(chrono::high_resolution_clock::now())
        {}

        void operator()(CRequestStatus::ECode  status,
                        int  code,
                        EDiagSev  severity,
                        const string &  message)
        {
            if (status == CRequestStatus::e404_NotFound)
                CPubseqGatewayApp::GetInstance()->GetTiming().
                    Register(eBlobRetrieve, eOpStatusNotFound,
                             m_BlobRetrieveTiming);
            m_PendingOp->OnGetBlobError(m_FetchDetails, status, code,
                                        severity, message);
        }

    private:
        CPendingOperation *                     m_PendingOp;
        CCassBlobFetch *                        m_FetchDetails;

        chrono::high_resolution_clock::time_point       m_BlobRetrieveTiming;
};


#endif
