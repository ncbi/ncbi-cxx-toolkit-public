#ifndef PSGS_RESOLVEBASE__HPP
#define PSGS_RESOLVEBASE__HPP

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

#include <corelib/request_status.hpp>
#include <corelib/ncbidiag.hpp>

#include "cass_fetch.hpp"
#include "psgs_request.hpp"
#include "psgs_reply.hpp"
#include "cass_processor_base.hpp"
#include "async_resolve_base.hpp"
#include "async_bioseq_info_base.hpp"

#include <objects/seqloc/Seq_id.hpp>
USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_IDBLOB_SCOPE;

struct SResolveInputSeqIdError;


// Deriving from CPSGS_CassProcessorBase is not required because
// CPSGS_AsyncResolveBase and CPSGS_AsyncBioseqInfoBase derive from it
// (virtually) as well
class CPSGS_ResolveBase : public CPSGS_AsyncResolveBase,
                          public CPSGS_AsyncBioseqInfoBase
{
public:
    CPSGS_ResolveBase();
    CPSGS_ResolveBase(shared_ptr<CPSGS_Request> request,
                      shared_ptr<CPSGS_Reply> reply,
                      TSeqIdResolutionFinishedCB finished_cb,
                      TSeqIdResolutionErrorCB error_cb,
                      TSeqIdResolutionStartProcessingCB resolution_start_processing_cb);
    virtual ~CPSGS_ResolveBase();

private:
    SPSGS_RequestBase::EPSGS_CacheAndDbUse x_GetRequestUseCache(void);
    bool x_GetEffectiveSeqIdType(const CSeq_id &  parsed_seq_id,
                                 int16_t &  eff_seq_id_type,
                                 bool  need_trace);
    EPSGS_SeqIdParsingResult x_ParseInputSeqId(CSeq_id &  seq_id,
                                               string &  err_msg);
    bool x_ComposeOSLT(CSeq_id &  parsed_seq_id,
                       int16_t &  effective_seq_id_type,
                       list<string> &  secondary_id_list,
                       string &  primary_id);

private:
    EPSGS_CacheLookupResult x_ResolvePrimaryOSLTInCache(
                                const string &  primary_id,
                                int16_t  effective_version,
                                int16_t  effective_seq_id_type,
                                SBioseqResolution &  bioseq_resolution);
    EPSGS_CacheLookupResult x_ResolveSecondaryOSLTInCache(
                                const string &  secondary_id,
                                int16_t  effective_seq_id_type,
                                SBioseqResolution &  bioseq_resolution);
    EPSGS_CacheLookupResult x_ResolveAsIsInCache(
                                SBioseqResolution &  bioseq_resolution,
                                bool  need_as_is=true);
    void x_ResolveViaComposeOSLTInCache(
                                CSeq_id &  parsed_seq_id,
                                int16_t  effective_seq_id_type,
                                const list<string> &  secondary_id_list,
                                const string &  primary_id,
                                SBioseqResolution &  bioseq_resolution);

private:
    void x_OnResolutionGoodData(void);
    void x_OnSeqIdResolveError(
                        CRequestStatus::ECode  status,
                        int  code,
                        EDiagSev  severity,
                        const string &  message);
    void x_OnSeqIdResolveFinished(
                        SBioseqResolution &&  bioseq_resolution);
    void x_RegisterSuccessTiming(const SBioseqResolution &  bioseq_resolution);

protected:
    void ResolveInputSeqId(void);

private:
    TSeqIdResolutionFinishedCB          m_FinalFinishedCB;
    TSeqIdResolutionErrorCB             m_FinalErrorCB;
    TSeqIdResolutionStartProcessingCB   m_FinalStartProcessingCB;

    bool                                m_AsyncStarted;
};

#endif  // PSGS_RESOLVEBASE__HPP

