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
 * File Description: base class for processors which need to resolve seq_id
 *
 */

#include <ncbi_pch.hpp>


#include <corelib/request_status.hpp>
#include <corelib/ncbidiag.hpp>

#include "pubseq_gateway.hpp"
#include "pubseq_gateway_utils.hpp"
#include "pubseq_gateway_cache_utils.hpp"
#include "cass_fetch.hpp"
#include "psgs_request.hpp"
#include "psgs_reply.hpp"
#include "insdc_utils.hpp"
#include "resolve_base.hpp"

#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
USING_IDBLOB_SCOPE;
USING_SCOPE(objects);

using namespace std::placeholders;


CPSGS_ResolveBase::CPSGS_ResolveBase()
{}


CPSGS_ResolveBase::CPSGS_ResolveBase(shared_ptr<CPSGS_Request> request,
                                     shared_ptr<CPSGS_Reply> reply,
                                     TSeqIdResolutionFinishedCB finished_cb,
                                     TSeqIdResolutionErrorCB error_cb,
                                     TSeqIdResolutionStartProcessingCB resolution_start_processing_cb) :
    CPSGS_AsyncResolveBase(request, reply,
                           bind(&CPSGS_ResolveBase::x_OnSeqIdResolveFinished,
                                this, _1),
                           bind(&CPSGS_ResolveBase::x_OnSeqIdResolveError,
                                this, _1, _2, _3, _4),
                           bind(&CPSGS_ResolveBase::x_OnResolutionGoodData,
                                this)),
    CPSGS_AsyncBioseqInfoBase(request, reply,
                              bind(&CPSGS_ResolveBase::x_OnSeqIdResolveFinished,
                                   this, _1),
                              bind(&CPSGS_ResolveBase::x_OnSeqIdResolveError,
                                   this, _1, _2, _3, _4)),
    m_FinalFinishedCB(finished_cb),
    m_FinalErrorCB(error_cb),
    m_FinalStartProcessingCB(resolution_start_processing_cb),
    m_AsyncStarted(false)
{}


CPSGS_ResolveBase::~CPSGS_ResolveBase()
{}


SPSGS_RequestBase::EPSGS_CacheAndDbUse
CPSGS_ResolveBase::x_GetRequestUseCache(void)
{
    switch (m_Request->GetRequestType()) {
        case CPSGS_Request::ePSGS_ResolveRequest:
            return m_Request->GetRequest<SPSGS_ResolveRequest>().m_UseCache;
        case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
            return m_Request->GetRequest<SPSGS_BlobBySeqIdRequest>().m_UseCache;
        case CPSGS_Request::ePSGS_AnnotationRequest:
            return m_Request->GetRequest<SPSGS_AnnotRequest>().m_UseCache;
        default:
            break;
    }
    NCBI_THROW(CPubseqGatewayException, eLogic,
               "Not handled request type " +
               to_string(static_cast<int>(m_Request->GetRequestType())));
}


bool
CPSGS_ResolveBase::x_GetEffectiveSeqIdType(
                                const CSeq_id &  parsed_seq_id,
                                int16_t &  eff_seq_id_type,
                                bool  need_trace)
{
    auto    parsed_seq_id_type = parsed_seq_id.Which();
    bool    parsed_seq_id_type_found = (parsed_seq_id_type !=
                                        CSeq_id_Base::e_not_set);
    auto    request_seq_id_type = GetRequestSeqIdType();

    if (!parsed_seq_id_type_found && request_seq_id_type < 0) {
        eff_seq_id_type = -1;
        return true;
    }

    if (!parsed_seq_id_type_found) {
        eff_seq_id_type = request_seq_id_type;
        return true;
    }

    if (request_seq_id_type < 0) {
        eff_seq_id_type = parsed_seq_id_type;
        return true;
    }

    // Both found
    if (parsed_seq_id_type == request_seq_id_type) {
        eff_seq_id_type = request_seq_id_type;
        return true;
    }

    // The parsed and url explicit seq_id_type do not match
    if (IsINSDCSeqIdType(parsed_seq_id_type) &&
        IsINSDCSeqIdType(request_seq_id_type)) {
        if (need_trace) {
            m_Reply->SendTrace(
                "Seq id type mismatch. Parsed CSeq_id reports seq_id_type as " +
                to_string(parsed_seq_id_type) + " while the URL reports " +
                to_string(request_seq_id_type) + ". They both belong to INSDC types so "
                "CSeq_id provided type " + to_string(parsed_seq_id_type) +
                " is taken as an effective one",
                m_Request->GetStartTimestamp());
        }
        eff_seq_id_type = parsed_seq_id_type;
        return true;
    }

    return false;
}


EPSGS_SeqIdParsingResult
CPSGS_ResolveBase::x_ParseInputSeqId(CSeq_id &  seq_id,
                                     string &  err_msg)
{
    bool    need_trace = m_Request->NeedTrace();
    auto    request_seq_id = GetRequestSeqId();
    auto    request_seq_id_type = GetRequestSeqIdType();

    try {
        seq_id.Set(request_seq_id);
        if (need_trace)
            m_Reply->SendTrace("Parsing CSeq_id('" + request_seq_id +
                             "') succeeded", m_Request->GetStartTimestamp());

        if (request_seq_id_type <= 0) {
            if (need_trace)
                m_Reply->SendTrace("Parsing CSeq_id finished OK (#1)",
                                   m_Request->GetStartTimestamp());
            return ePSGS_ParsedOK;
        }

        // Check the parsed type with the given
        int16_t     eff_seq_id_type;
        if (x_GetEffectiveSeqIdType(seq_id, eff_seq_id_type, false)) {
            if (need_trace)
                m_Reply->SendTrace("Parsing CSeq_id finished OK (#2)",
                                   m_Request->GetStartTimestamp());
            return ePSGS_ParsedOK;
        }

        // seq_id_type from URL and from CSeq_id differ
        CSeq_id_Base::E_Choice  seq_id_type = seq_id.Which();

        if (need_trace)
            m_Reply->SendTrace("CSeq_id provided type " + to_string(seq_id_type) +
                               " and URL provided seq_id_type " +
                               to_string(request_seq_id_type) + " mismatch",
                               m_Request->GetStartTimestamp());

        if (IsINSDCSeqIdType(request_seq_id_type) &&
            IsINSDCSeqIdType(seq_id_type)) {
            // Both seq_id_types belong to INSDC
            if (need_trace) {
                m_Reply->SendTrace("Both types belong to INSDC types.\n"
                                   "Parsing CSeq_id finished OK (#3)",
                                   m_Request->GetStartTimestamp());
            }
            return ePSGS_ParsedOK;
        }

        // Type mismatch: form the error message in case of resolution problems
        err_msg = "Seq_id '" + request_seq_id +
                  "' possible type mismatch: the URL provides " +
                  to_string(request_seq_id_type) +
                  " while the CSeq_Id detects it as " +
                  to_string(static_cast<int>(seq_id_type));
    } catch (...) {
        if (need_trace)
            m_Reply->SendTrace("Parsing CSeq_id('" + request_seq_id +
                               "') failed (exception)",
                               m_Request->GetStartTimestamp());
    }

    // Second variation of Set()
    if (request_seq_id_type > 0) {
        try {
            seq_id.Set(CSeq_id::eFasta_AsTypeAndContent,
                       (CSeq_id_Base::E_Choice)(request_seq_id_type),
                       request_seq_id);
            if (need_trace) {
                m_Reply->SendTrace("Parsing CSeq_id(eFasta_AsTypeAndContent, " +
                                   to_string(request_seq_id_type) +
                                   ", '" + request_seq_id + "') succeeded.\n"
                                   "Parsing CSeq_id finished OK (#4)",
                                   m_Request->GetStartTimestamp());
            }
            return ePSGS_ParsedOK;
        } catch (...) {
            if (need_trace)
                m_Reply->SendTrace("Parsing CSeq_id(eFasta_AsTypeAndContent, " +
                                   to_string(request_seq_id_type) +
                                   ", '" + request_seq_id + "') failed (exception)",
                                   m_Request->GetStartTimestamp());
        }
    }

    if (need_trace) {
        m_Reply->SendTrace("Parsing CSeq_id finished FAILED",
                           m_Request->GetStartTimestamp());
    }

    return ePSGS_ParseFailed;
}


bool
CPSGS_ResolveBase::x_ComposeOSLT(CSeq_id &  parsed_seq_id,
                                 int16_t &  effective_seq_id_type,
                                 list<string> &  secondary_id_list,
                                 string &  primary_id)
{
    bool    need_trace = m_Request->NeedTrace();

    if (!x_GetEffectiveSeqIdType(parsed_seq_id,
                                 effective_seq_id_type, need_trace)) {
        if (need_trace) {
            m_Reply->SendTrace("OSLT has not been tried due to mismatch "
                             "between the  parsed CSeq_id seq_id_type and "
                             "the URL provided one",
                             m_Request->GetStartTimestamp());
        }
        return false;
    }

    try {
        primary_id = parsed_seq_id.ComposeOSLT(&secondary_id_list,
                                               CSeq_id::fGpipeAddSecondary);
    } catch (...) {
        if (need_trace) {
            m_Reply->SendTrace("OSLT call failure (exception)",
                             m_Request->GetStartTimestamp());
        }
        return false;
    }

    if (need_trace) {
        string  trace_msg("OSLT succeeded");
        trace_msg += "\nOSLT primary id: " + primary_id;

        if (secondary_id_list.empty()) {
            trace_msg += "\nOSLT secondary id list is empty";
        } else {
            for (const auto &  item : secondary_id_list) {
                trace_msg += "\nOSLT secondary id: " + item;
            }
        }
        m_Reply->SendTrace(trace_msg, m_Request->GetStartTimestamp());
    }

    return true;
}


EPSGS_CacheLookupResult
CPSGS_ResolveBase::x_ResolvePrimaryOSLTInCache(
                                const string &  primary_id,
                                int16_t  effective_version,
                                int16_t  effective_seq_id_type,
                                SBioseqResolution &  bioseq_resolution)
{
    EPSGS_CacheLookupResult     bioseq_cache_lookup_result = ePSGS_CacheNotHit;

    if (!primary_id.empty()) {
        CPSGCache           psg_cache(true, m_Request, m_Reply);

        // Try BIOSEQ_INFO
        bioseq_resolution.m_BioseqInfo.SetAccession(primary_id);
        bioseq_resolution.m_BioseqInfo.SetVersion(effective_version);
        bioseq_resolution.m_BioseqInfo.SetSeqIdType(effective_seq_id_type);

        bioseq_cache_lookup_result = psg_cache.LookupBioseqInfo(
                                        bioseq_resolution);
        if (bioseq_cache_lookup_result == ePSGS_CacheHit) {
            bioseq_resolution.m_ResolutionResult = ePSGS_BioseqCache;
            return ePSGS_CacheHit;
        }

        bioseq_resolution.Reset();
    }
    return bioseq_cache_lookup_result;
}


EPSGS_CacheLookupResult
CPSGS_ResolveBase::x_ResolveSecondaryOSLTInCache(
                                const string &  secondary_id,
                                int16_t  effective_seq_id_type,
                                SBioseqResolution &  bioseq_resolution)
{
    bioseq_resolution.m_BioseqInfo.SetAccession(secondary_id);
    bioseq_resolution.m_BioseqInfo.SetSeqIdType(effective_seq_id_type);

    CPSGCache   psg_cache(true, m_Request, m_Reply);
    auto        si2csi_cache_lookup_result =
                        psg_cache.LookupSi2csi(bioseq_resolution);
    if (si2csi_cache_lookup_result == ePSGS_CacheHit) {
        bioseq_resolution.m_ResolutionResult = ePSGS_Si2csiCache;
        return ePSGS_CacheHit;
    }

    bioseq_resolution.Reset();

    if (si2csi_cache_lookup_result == ePSGS_CacheFailure)
        return ePSGS_CacheFailure;
    return ePSGS_CacheNotHit;
}


EPSGS_CacheLookupResult
CPSGS_ResolveBase::x_ResolveAsIsInCache(
                                SBioseqResolution &  bioseq_resolution,
                                bool  need_as_is)
{
    EPSGS_CacheLookupResult     cache_lookup_result = ePSGS_CacheNotHit;

    // Capitalize seq_id
    string      upper_seq_id = GetRequestSeqId();
    NStr::ToUpper(upper_seq_id);

    auto        seq_id_type = GetRequestSeqIdType();

    // 1. As is
    if (need_as_is == true) {
        cache_lookup_result = x_ResolveSecondaryOSLTInCache(
                                    upper_seq_id, seq_id_type,
                                    bioseq_resolution);
    }

    if (cache_lookup_result == ePSGS_CacheNotHit) {
        // 2. if there are | at the end => strip all trailing bars
        //    else => add one | at the end
        if (upper_seq_id[upper_seq_id.size() - 1] == '|') {
            string  strip_bar_seq_id(upper_seq_id);
            while (strip_bar_seq_id[strip_bar_seq_id.size() - 1] == '|')
                strip_bar_seq_id.erase(strip_bar_seq_id.size() - 1, 1);
            cache_lookup_result = x_ResolveSecondaryOSLTInCache(
                                        strip_bar_seq_id, seq_id_type,
                                        bioseq_resolution);
        } else {
            string      seq_id_added_bar(upper_seq_id);
            seq_id_added_bar.append(1, '|');
            cache_lookup_result = x_ResolveSecondaryOSLTInCache(
                                        seq_id_added_bar, seq_id_type,
                                        bioseq_resolution);
        }
    }

    if (cache_lookup_result == ePSGS_CacheFailure) {
        bioseq_resolution.Reset();
        bioseq_resolution.m_Error.m_ErrorMessage = "Cache lookup failure";
        bioseq_resolution.m_Error.m_ErrorCode = CRequestStatus::e500_InternalServerError;
    }

    return cache_lookup_result;
}


void
CPSGS_ResolveBase::x_ResolveViaComposeOSLTInCache(
                                CSeq_id &  parsed_seq_id,
                                int16_t  effective_seq_id_type,
                                const list<string> &  secondary_id_list,
                                const string &  primary_id,
                                SBioseqResolution &  bioseq_resolution)
{
    const CTextseq_id *  text_seq_id = parsed_seq_id.GetTextseq_Id();
    int16_t              effective_version = GetEffectiveVersion(text_seq_id);
    bool                 cache_failure = false;

    if (!primary_id.empty()) {
        auto    cache_lookup_result =
                    x_ResolvePrimaryOSLTInCache(primary_id, effective_version,
                                                effective_seq_id_type,
                                                bioseq_resolution);
        if (cache_lookup_result == ePSGS_CacheHit)
            return;
        if (cache_lookup_result == ePSGS_CacheFailure)
            cache_failure = true;
    }

    for (const auto &  secondary_id : secondary_id_list) {
        auto    cache_lookup_result =
                    x_ResolveSecondaryOSLTInCache(secondary_id,
                                                  effective_seq_id_type,
                                                  bioseq_resolution);
        if (cache_lookup_result == ePSGS_CacheHit)
            return;
        if (cache_lookup_result == ePSGS_CacheFailure) {
            cache_failure = true;
            break;
        }
    }

    // Try cache as it came from URL
    // The primary id may match the URL given seq_id so it makes sense to
    // exclude trying the very same string in x_ResolveAsIsInCache(). The
    // x_ResolveAsIsInCache() capitalizes the url seq id so the capitalized
    // versions need to be compared
    string      upper_seq_id = GetRequestSeqId();
    NStr::ToUpper(upper_seq_id);
    bool        need_as_is = primary_id != upper_seq_id;
    auto        cache_lookup_result =
                    x_ResolveAsIsInCache(bioseq_resolution, need_as_is);
    if (cache_lookup_result == ePSGS_CacheHit)
        return;
    if (cache_lookup_result == ePSGS_CacheFailure)
        cache_failure = true;

    bioseq_resolution.Reset();

    if (cache_failure) {
        bioseq_resolution.m_Error.m_ErrorMessage = "Cache lookup failure";
        bioseq_resolution.m_Error.m_ErrorCode = CRequestStatus::e500_InternalServerError;
    }
}


void
CPSGS_ResolveBase::ResolveInputSeqId(void)
{
    SBioseqResolution   bioseq_resolution;
    auto                app = CPubseqGatewayApp::GetInstance();
    string              parse_err_msg;
    CSeq_id             oslt_seq_id;
    auto                parsing_result = x_ParseInputSeqId(oslt_seq_id,
                                                           parse_err_msg);

    // The results of the ComposeOSLT are used in both cache and DB
    int16_t         effective_seq_id_type;
    list<string>    secondary_id_list;
    string          primary_id;
    bool            composed_ok = false;
    if (parsing_result == ePSGS_ParsedOK) {
        composed_ok = x_ComposeOSLT(oslt_seq_id, effective_seq_id_type,
                                    secondary_id_list, primary_id);
    }

    auto    request_use_cache = x_GetRequestUseCache();
    if (request_use_cache != SPSGS_RequestBase::ePSGS_DbOnly) {
        // Try cache
        if (composed_ok)
            x_ResolveViaComposeOSLTInCache(oslt_seq_id, effective_seq_id_type,
                                           secondary_id_list, primary_id,
                                           bioseq_resolution);
        else
            x_ResolveAsIsInCache(bioseq_resolution);

        if (bioseq_resolution.IsValid()) {
            // Special case for the seq_id like gi|156232
            bool    continue_with_cassandra = false;
            if (bioseq_resolution.m_ResolutionResult == ePSGS_Si2csiCache) {
                if (!CanSkipBioseqInfoRetrieval(
                            bioseq_resolution.m_BioseqInfo)) {
                    // This is an optimization. Try to find the record in the
                    // BIOSEQ_INFO only if needed.
                    CPSGCache   psg_cache(true, m_Request, m_Reply);
                    auto        bioseq_cache_lookup_result =
                                    psg_cache.LookupBioseqInfo(bioseq_resolution);

                    if (bioseq_cache_lookup_result != ePSGS_CacheHit) {
                        // Not found or error
                        continue_with_cassandra = true;
                        bioseq_resolution.Reset();
                    } else {
                        bioseq_resolution.m_ResolutionResult = ePSGS_BioseqCache;

                        auto    adj_result = AdjustBioseqAccession(
                                                            bioseq_resolution);
                        if (adj_result == ePSGS_LogicError ||
                            adj_result == ePSGS_SeqIdsEmpty) {
                            continue_with_cassandra = true;
                            bioseq_resolution.Reset();
                        }
                    }
                }
            } else {
                // The result is coming from the BIOSEQ_INFO cache. Need to try
                // the adjustment
                auto    adj_result = AdjustBioseqAccession(bioseq_resolution);
                if (adj_result == ePSGS_LogicError ||
                    adj_result == ePSGS_SeqIdsEmpty) {
                    continue_with_cassandra = true;
                    bioseq_resolution.Reset();
                }
            }

            if (!continue_with_cassandra) {
                x_OnSeqIdResolveFinished(move(bioseq_resolution));
                return;
            }
        }
    }

    if (request_use_cache != SPSGS_RequestBase::ePSGS_CacheOnly) {
        // Need to initiate async DB resolution

        // Memorize an error if there was one
        if (! parse_err_msg.empty() &&
            ! bioseq_resolution.m_Error.HasError()) {
            bioseq_resolution.m_Error.m_ErrorMessage = parse_err_msg;
            bioseq_resolution.m_Error.m_ErrorCode = CRequestStatus::e404_NotFound;
        }

        // Async request
        m_AsyncStarted = true;
        CPSGS_AsyncResolveBase::Process(
                GetEffectiveVersion(oslt_seq_id.GetTextseq_Id()),
                effective_seq_id_type,
                move(secondary_id_list),
                move(primary_id),
                composed_ok,
                move(bioseq_resolution));

        // Async resolver will call a callback
        return;
    }

    // Finished with resolution:
    // - not found
    // - parsing error
    // - LMDB error
    app->GetCounters().Increment(CPSGSCounters::ePSGS_InputSeqIdNotResolved);

    if (bioseq_resolution.m_Error.HasError()) {
        x_OnSeqIdResolveError(bioseq_resolution.m_Error.m_ErrorCode,
                              ePSGS_UnresolvedSeqId,
                              eDiag_Error,
                              bioseq_resolution.m_Error.m_ErrorMessage);
        return;
    }

    if (!parse_err_msg.empty()) {
        x_OnSeqIdResolveError(CRequestStatus::e404_NotFound,
                              ePSGS_UnresolvedSeqId, eDiag_Error,
                              parse_err_msg);
        return;
    }

    x_OnSeqIdResolveError(CRequestStatus::e404_NotFound, ePSGS_UnresolvedSeqId,
                          eDiag_Error,
                          "Could not resolve seq_id " + GetRequestSeqId());
}


SBioseqResolution
CPSGS_ResolveBase::ResolveTestInputSeqId(void)
{
    // The method is to support the 'health' and 'deep-health' URLs.
    // The only cache needs to be tried and no writing to the reply is allowed
    SBioseqResolution   bioseq_resolution;
    string              parse_err_msg;
    CSeq_id             oslt_seq_id;
    auto                parsing_result = x_ParseInputSeqId(oslt_seq_id,
                                                           parse_err_msg);

    // The results of the ComposeOSLT are used in both cache and DB
    int16_t         effective_seq_id_type;
    list<string>    secondary_id_list;
    string          primary_id;
    bool            composed_ok = false;
    if (parsing_result == ePSGS_ParsedOK) {
        composed_ok = x_ComposeOSLT(oslt_seq_id, effective_seq_id_type,
                                    secondary_id_list, primary_id);
    }

    // Try cache unconditionally
    if (composed_ok)
        x_ResolveViaComposeOSLTInCache(oslt_seq_id, effective_seq_id_type,
                                       secondary_id_list, primary_id,
                                       bioseq_resolution);
    else
        x_ResolveAsIsInCache(bioseq_resolution);


    if (!bioseq_resolution.IsValid()) {
        if (!bioseq_resolution.m_Error.HasError()) {
            if (!parse_err_msg.empty()) {
                bioseq_resolution.m_Error.m_ErrorMessage = parse_err_msg;
            }
        }
    }

    return bioseq_resolution;
}


void
CPSGS_ResolveBase::x_OnSeqIdResolveError(
                        CRequestStatus::ECode  status,
                        int  code,
                        EDiagSev  severity,
                        const string &  message)
{
    auto    app = CPubseqGatewayApp::GetInstance();
    if (status == CRequestStatus::e404_NotFound) {
        app->GetTiming().Register(eResolutionNotFound, eOpStatusNotFound,
                                  m_Request->GetStartTimestamp());
        if (m_AsyncStarted)
            app->GetTiming().Register(eResolutionCass, eOpStatusNotFound,
                                      GetAsyncResolutionStartTimestamp());
        else
            app->GetTiming().Register(eResolutionLmdb, eOpStatusNotFound,
                                      m_Request->GetStartTimestamp());
    }
    else {
        app->GetTiming().Register(eResolutionError, eOpStatusNotFound,
                                  m_Request->GetStartTimestamp());
    }

    m_FinalErrorCB(status, code, severity, message);
}


// Called only in case of a success
void CPSGS_ResolveBase::x_OnSeqIdResolveFinished(
                                    SBioseqResolution &&  bioseq_resolution)
{
    // A few cases here: comes from cache or DB
    // ePSGS_Si2csiCache, ePSGS_Si2csiDB, ePSGS_BioseqCache, ePSGS_BioseqDB
    if (bioseq_resolution.m_ResolutionResult == ePSGS_Si2csiDB ||
        bioseq_resolution.m_ResolutionResult == ePSGS_Si2csiCache) {
        // We have the following fields at hand:
        // - accession, version, seq_id_type, gi
        // May be it is what the user asked for
        if (!CanSkipBioseqInfoRetrieval(bioseq_resolution.m_BioseqInfo)) {
            // Need to pull the full bioseq info
            CPSGCache   psg_cache(m_Request, m_Reply);
            auto        cache_lookup_result =
                                psg_cache.LookupBioseqInfo(bioseq_resolution);
            if (cache_lookup_result != ePSGS_CacheHit) {
                // No cache hit (or not allowed); need to get to DB if allowed
                if (x_GetRequestUseCache() != SPSGS_RequestBase::ePSGS_CacheOnly) {
                    // Async DB query

                    // To have the proper timing registered in the errors
                    // handler
                    m_AsyncStarted = true;

                    if (bioseq_resolution.m_CassQueryCount == 0) {
                        // It now became cassandra based so need to memorize
                        // the start timestamp
                        SetAsyncResolutionStartTimestamp(
                                        chrono::high_resolution_clock::now());
                    }

                    CPSGS_AsyncBioseqInfoBase::MakeRequest(
                                                    move(bioseq_resolution));
                    return;
                }

                // It is a bioseq inconsistency case
                x_OnSeqIdResolveError(
                                CRequestStatus::e500_InternalServerError,
                                ePSGS_NoBioseqInfo, eDiag_Error,
                                "Data inconsistency: the bioseq key info was "
                                "resolved for seq_id " + GetRequestSeqId() +
                                " but the bioseq info is not found");
                return;
            } else {
                bioseq_resolution.m_ResolutionResult = ePSGS_BioseqCache;
            }
        }
    }

    // All good
    x_OnResolutionGoodData();
    x_RegisterSuccessTiming(bioseq_resolution);
    m_FinalFinishedCB(move(bioseq_resolution));
}


void
CPSGS_ResolveBase::x_RegisterSuccessTiming(
                                const SBioseqResolution &  bioseq_resolution)
{
    auto    app = CPubseqGatewayApp::GetInstance();

    // Overall timing, regardless how it was done
    app->GetTiming().Register(eResolutionFound, eOpStatusFound,
                              m_Request->GetStartTimestamp());

    if (bioseq_resolution.m_CassQueryCount > 0) {
        // Regardless how many requests
        app->GetTiming().Register(eResolutionCass, eOpStatusFound,
                                  GetAsyncResolutionStartTimestamp());

        // Separated by the number of requests
        app->GetTiming().Register(eResolutionFoundInCassandra,
                                  eOpStatusFound,
                                  GetAsyncResolutionStartTimestamp(),
                                  bioseq_resolution.m_CassQueryCount);
    } else {
        app->GetTiming().Register(eResolutionLmdb, eOpStatusFound,
                                  m_Request->GetStartTimestamp());
    }
}


void CPSGS_ResolveBase::x_OnResolutionGoodData(void)
{
    m_FinalStartProcessingCB();
}

