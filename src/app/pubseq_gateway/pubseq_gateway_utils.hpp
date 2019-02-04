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
#include <connect/services/json_over_uttp.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/blob_record.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info.hpp>

#include "pubseq_gateway_types.hpp"

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

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



struct SBioseqResolution
{
    SBioseqResolution() :
        m_ResolutionResult(eNotResolved)
    {
        m_BioseqInfo.m_Version = -1;
        m_BioseqInfo.m_SeqIdType = -1;
    }

    EResolutionResult   m_ResolutionResult;
    SBioseqInfo         m_BioseqInfo;
    string              m_CacheInfo;    // Bioseq or si2csi cache

    bool IsValid(void) const
    {
        return m_ResolutionResult != eNotResolved;
    }

    void Reset(void)
    {
        m_ResolutionResult = eNotResolved;
        m_CacheInfo.clear();
        m_BioseqInfo.m_Accession.clear();
        m_BioseqInfo.m_Version = -1;
        m_BioseqInfo.m_SeqIdType = -1;
    }
};



// Bioseq messages
string  GetBioseqInfoHeader(size_t  item_id, size_t  bioseq_info_size,
                            EOutputFormat  output_format);
string  GetBioseqMessageHeader(size_t  item_id, size_t  msg_size,
                               CRequestStatus::ECode  status, int  code,
                               EDiagSev  severity);
string  GetBioseqCompletionHeader(size_t  item_id, size_t  chunk_count);

// Blob prop messages
string  GetBlobPropHeader(size_t  item_id, const SBlobId &  blob_id,
                          size_t  blob_prop_size);
string  GetBlobPropMessageHeader(size_t  item_id, size_t  msg_size,
                                 CRequestStatus::ECode  status, int  code,
                                 EDiagSev  severity);
string  GetBlobPropCompletionHeader(size_t  item_id, size_t  chunk_count);

// Blob chunk messages
string  GetBlobChunkHeader(size_t  item_id, const SBlobId &  blob_id,
                           size_t  chunk_size, size_t  chunk_number);
string  GetBlobCompletionHeader(size_t  item_id, const SBlobId &  blob_id,
                                size_t  chunk_count);
string  GetBlobMessageHeader(size_t  item_id, const SBlobId &  blob_id,
                             size_t  msg_size,
                             CRequestStatus::ECode  status, int  code,
                             EDiagSev  severity);

// Named annotation messages
string GetNamedAnnotationHeader(size_t  item_id, const string &  accession,
                                int16_t  version, int16_t  seq_id_type,
                                const string &  annot_name,
                                size_t  annotation_size);
string GetNamedAnnotationMessageHeader(size_t  item_id, size_t  msg_size,
                                       CRequestStatus::ECode  status, int  code,
                                       EDiagSev  severity);
string GetNamedAnnotationCompletionHeader(size_t  item_id, size_t  chunk_count);


// Reply messages
string  GetReplyCompletionHeader(size_t  chunk_count);
string  GetReplyMessageHeader(size_t  msg_size,
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
