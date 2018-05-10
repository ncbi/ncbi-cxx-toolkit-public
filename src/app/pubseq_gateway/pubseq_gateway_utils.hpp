#ifndef PUBSEQ_GATEWAY_UTILS__HPP
#define PUBSEQ_GATEWAY_UTILS__HPP

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

#include <corelib/request_status.hpp>
#include <corelib/ncbidiag.hpp>
USING_NCBI_SCOPE;

#include <string>
using namespace std;

// The request URLs and the reply content parameters need the blob ID as a
// string while internally they are represented as a pair of integers. So
// a few conversion utilities need to be created.

struct SBlobId
{
    int     m_Sat;
    int     m_SatKey;

    // Resolved sat; may be ommitted
    string  m_SatName;

    SBlobId();
    SBlobId(const string &  blob_id);
    SBlobId(int  sat, int  sat_key);

    void SetSatName(const string &  name)
    {
        m_SatName = name;
    }

    bool IsValid(void) const;
    bool operator < (const SBlobId &  other) const;
};


string  GetBlobId(const SBlobId &  blob_id);
SBlobId ParseBlobId(const string &  blob_id);


// A few protocol utils
enum EPubseqGatewayErrorCode {
    eUnknownSatellite = 300,
    eUnknownResolvedSatellite,
    eResolutionNotFound,
    eUnpackingError
};


string  GetResolutionHeader(size_t  resolution_size);
string  GetBlobChunkHeader(size_t  chunk_size, const SBlobId &  blob_id,
                           size_t  chunk_number);
string  GetBlobCompletionHeader(const SBlobId &  blob_id,
                                size_t  total_blob_data_chunks,
                                size_t  total_cnunks);
string  GetReplyCompletionHeader(size_t  chunk_count);
string  GetResolutionErrorHeader(const string &  accession, size_t  msg_size,
                                 CRequestStatus::ECode  status, int  code,
                                 EDiagSev  severity);
string  GetBlobErrorHeader(const SBlobId &  blob_id, size_t  msg_size,
                           CRequestStatus::ECode  status, int  code,
                           EDiagSev  severity);
string  GetBlobMessageHeader(const SBlobId &  blob_id, size_t  msg_size,
                             CRequestStatus::ECode  status, int  code,
                             EDiagSev  severity);
string  GetReplyErrorHeader(size_t  msg_size,
                            CRequestStatus::ECode  status, int  code,
                            EDiagSev  severity);


// Reset the request context if necessary
class CRequestContextResetter
{
public:
    CRequestContextResetter();
    ~CRequestContextResetter();
};


#endif
