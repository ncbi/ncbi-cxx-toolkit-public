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
            m_FetchDetails(fetch_details)
        {}

        void operator()(CBlobRecord const & blob, const unsigned char *  data,
                        unsigned int  size, int  chunk_no)
        {
            m_PendingOp->OnGetBlobChunk(m_FetchDetails, blob, data, size, chunk_no);
        }

    private:
        CPendingOperation *     m_PendingOp;
        CCassBlobFetch *        m_FetchDetails;
};


class CBlobPropCallback
{
    public:
        CBlobPropCallback(
                CPendingOperation *  pending_op,
                CCassBlobFetch *  fetch_details) :
            m_PendingOp(pending_op),
            m_FetchDetails(fetch_details),
            m_InProcess(false)
        {}

        void operator()(CBlobRecord const &  blob, bool is_found)
        {
            if (!m_InProcess) {
                m_InProcess = true;
                m_PendingOp->OnGetBlobProp(m_FetchDetails, blob, is_found);
                m_InProcess = false;
            }
        }

    private:
        void x_RequestOriginalBlobChunks(CBlobRecord const &  blob);
        void x_RequestID2BlobChunks(CBlobRecord const &  blob,
                                    bool  info_blob_only);
        void x_ParseId2Info(CBlobRecord const &  blob);
        void x_RequestId2SplitBlobs(const string &  sat_name);

    private:
        CPendingOperation *                     m_PendingOp;
        CCassBlobFetch *                        m_FetchDetails;

        // Avoid an infinite loop
        bool            m_InProcess;
};


class CGetBlobErrorCallback
{
    public:
        CGetBlobErrorCallback(
                CPendingOperation *  pending_op,
                CCassBlobFetch *  fetch_details) :
            m_PendingOp(pending_op),
            m_FetchDetails(fetch_details)
        {}

        void operator()(CRequestStatus::ECode  status,
                        int  code,
                        EDiagSev  severity,
                        const string &  message)
        {
            m_PendingOp->OnGetBlobError(m_FetchDetails, status, code,
                                        severity, message);
        }

    private:
        CPendingOperation *                     m_PendingOp;
        CCassBlobFetch *                        m_FetchDetails;
};


#endif
