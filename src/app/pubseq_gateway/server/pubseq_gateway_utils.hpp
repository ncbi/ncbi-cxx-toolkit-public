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
#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info/record.hpp>

#include "pubseq_gateway_types.hpp"
#include "psgs_request.hpp"

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

#include <string>
#include <chrono>
using namespace std;

class CPSGS_Request;
class CPSGS_Reply;


// Used to report errors like bad request or LMDB cache error from a lower
// level code to an upper one.
struct SResolveInputSeqIdError
{
    SResolveInputSeqIdError() :
        m_ErrorCode(CRequestStatus::e200_Ok)
    {}

    SResolveInputSeqIdError(const SResolveInputSeqIdError &) = default;
    SResolveInputSeqIdError(SResolveInputSeqIdError &&) = default;
    SResolveInputSeqIdError &  operator=(const SResolveInputSeqIdError &) = default;
    SResolveInputSeqIdError &  operator=(SResolveInputSeqIdError &&) = default;


    bool HasError(void) const
    {
        return !m_ErrorMessage.empty();
    }

    void Reset(void)
    {
        m_ErrorMessage.clear();
        m_ErrorCode = CRequestStatus::e200_Ok;
    }

    string                  m_ErrorMessage;
    CRequestStatus::ECode   m_ErrorCode;
};




struct SBioseqResolution
{
    SBioseqResolution() :
        m_ResolutionResult(ePSGS_NotResolved),
        m_CassQueryCount(0),
        m_AdjustmentTried(false),
        m_AccessionAdjustmentResult(ePSGS_NotRequired)
    {}

    SBioseqResolution(const SBioseqResolution &) = default;
    SBioseqResolution(SBioseqResolution &&) = default;
    SBioseqResolution &  operator=(const SBioseqResolution &) = default;
    SBioseqResolution &  operator=(SBioseqResolution &&) = default;


    bool IsValid(void) const
    {
        return m_ResolutionResult != ePSGS_NotResolved;
    }

    void Reset(void)
    {
        m_ResolutionResult = ePSGS_NotResolved;
        m_BioseqInfo.Reset();

        m_AdjustmentTried = false;
        m_AccessionAdjustmentResult = ePSGS_NotRequired;
        m_AdjustmentError.clear();
    }

    EPSGS_AccessionAdjustmentResult
    AdjustAccession(shared_ptr<CPSGS_Request>  request,
                    shared_ptr<CPSGS_Reply>  reply);

    EPSGS_ResolutionResult          m_ResolutionResult;
    size_t                          m_CassQueryCount;

    // In case of the SI2CSI the only key fields are filled
    CBioseqInfoRecord               m_BioseqInfo;

    // It could be a parsing error, retrieval error etc.
    SResolveInputSeqIdError         m_Error;

    // The accession adjustment should happened exactly once so the results
    // of the call are saved for the use on the other stages
    bool                            m_AdjustmentTried;
    EPSGS_AccessionAdjustmentResult m_AccessionAdjustmentResult;
    string                          m_AdjustmentError;
};



// Bioseq messages
string  GetBioseqInfoHeader(size_t  item_id,
                            const string &  processor_id,
                            size_t  bioseq_info_size,
                            SPSGS_ResolveRequest::EPSGS_OutputFormat  output_format);
string  GetBioseqMessageHeader(size_t  item_id,
                               const string &  processor_id,
                               size_t  msg_size,
                               CRequestStatus::ECode  status,
                               int  code,
                               EDiagSev  severity);
string  GetBioseqCompletionHeader(size_t  item_id,
                                  const string &  processor_id,
                                  size_t  chunk_count);

// Blob prop messages
string  GetBlobPropHeader(size_t  item_id,
                          const string &  processor_id,
                          const string &  blob_id,
                          size_t  blob_prop_size);
string  GetBlobPropMessageHeader(size_t  item_id,
                                 const string &  processor_id,
                                 size_t  msg_size,
                                 CRequestStatus::ECode  status,
                                 int  code,
                                 EDiagSev  severity);
string  GetBlobPropCompletionHeader(size_t  item_id,
                                    const string &  processor_id,
                                    size_t  chunk_count);
string  GetTSEBlobPropHeader(size_t  item_id,
                             const string &  processor_id,
                             int64_t  id2_chunk,
                             const string &  id2_info,
                             size_t  blob_prop_size);
string  GetTSEBlobPropMessageHeader(size_t  item_id,
                                    const string &  processor_id,
                                    int64_t  id2_chunk,
                                    const string &  id2_info,
                                    size_t  msg_size,
                                    CRequestStatus::ECode  status,
                                    int  code,
                                    EDiagSev  severity);
string  GetTSEBlobPropCompletionHeader(size_t  item_id,
                                       const string &  processor_id,
                                       int64_t  id2_chunk,
                                       const string &  id2_info,
                                       size_t  chunk_count);

// Blob chunk messages
string  GetBlobChunkHeader(size_t  item_id,
                           const string &  processor_id,
                           const string &  blob_id,
                           size_t  chunk_size,
                           size_t  chunk_number);
string  GetBlobCompletionHeader(size_t  item_id,
                                const string &  processor_id,
                                const string &  blob_id,
                                size_t  chunk_count);
string  GetBlobMessageHeader(size_t  item_id,
                             const string &  processor_id,
                             const string &  blob_id,
                             size_t  msg_size,
                             CRequestStatus::ECode  status,
                             int  code,
                             EDiagSev  severity);
string  GetBlobExcludeHeader(size_t  item_id,
                             const string &  processor_id,
                             const string &  blob_id,
                             EPSGS_BlobSkipReason  skip_reason);
string  GetTSEBlobExcludeHeader(size_t  item_id,
                                const string &  processor_id,
                                int64_t  id2_chunk,
                                const string &  id2_info,
                                EPSGS_BlobSkipReason  skip_reason);
string  GetTSEBlobChunkHeader(size_t  item_id,
                              const string &  processor_id,
                              size_t  chunk_size,
                              size_t  chunk_number,
                              int64_t  id2_chunk,
                              const string &  id2_info);
string  GetTSEBlobMessageHeader(size_t  item_id,
                                const string &  processor_id,
                                int64_t  id2_chunk,
                                const string &  id2_info,
                                size_t  msg_size,
                                CRequestStatus::ECode  status,
                                int  code,
                                EDiagSev  severity);
string GetTSEBlobCompletionHeader(size_t  item_id,
                                  const string &  processor_id,
                                  int64_t  id2_chunk,
                                  const string &  id2_info,
                                  size_t  chunk_count);

// Named annotation messages
string GetNamedAnnotationHeader(size_t  item_id,
                                const string &  processor_id,
                                const string &  annot_name,
                                size_t  annotation_size);
string GetNamedAnnotationMessageHeader(size_t  item_id,
                                       const string &  processor_id,
                                       size_t  msg_size,
                                       CRequestStatus::ECode  status,
                                       int  code,
                                       EDiagSev  severity);
string GetNamedAnnotationMessageCompletionHeader(size_t  item_id,
                                                 const string &  processor_id,
                                                 size_t  chunk_count);
string GetNamedAnnotationCompletionHeader(size_t  item_id,
                                          const string &  processor_id,
                                          size_t  chunk_count);


// Reply messages
string  GetReplyCompletionHeader(size_t  chunk_count);
string  GetReplyMessageHeader(size_t  msg_size,
                              CRequestStatus::ECode  status, int  code,
                              EDiagSev  severity);

// Processor messages
string GetProcessorMessageHeader(size_t  item_id,
                                 const string &  processor_id,
                                 size_t  msg_size,
                                 CRequestStatus::ECode  status,
                                 int  code,
                                 EDiagSev  severity);
string GetProcessorMessageCompletionHeader(size_t  item_id,
                                           const string &  processor_id,
                                           size_t  chunk_count);


// Reset the request context if necessary
class CRequestContextResetter
{
public:
    CRequestContextResetter()
    {}

    ~CRequestContextResetter();
};


string FormatPreciseTime(const chrono::system_clock::time_point &  t_point);
string GetCassStartupDataStateMessage(EPSGS_StartupDataState  state);

#endif
