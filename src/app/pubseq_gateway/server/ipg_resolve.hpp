#ifndef PSGS_IPGRESOLVE__HPP
#define PSGS_IPGRESOLVE__HPP

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
 * File Description: ipg resolve processor
 *
 */

#include "cass_processor_base.hpp"
#include "resolve_base.hpp"
#include <objtools/pubseq_gateway/impl/ipg/fetch_ipg_report.hpp>

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;
using namespace ipg;

// Forward declaration
class CCassFetch;
class CCassIPGResolveFetch;

class CPSGS_IPGResolveProcessor : public CPSGS_ResolveBase
{
public:
    virtual bool CanProcess(shared_ptr<CPSGS_Request> request,
                            shared_ptr<CPSGS_Reply> reply) const;
    virtual IPSGS_Processor* CreateProcessor(shared_ptr<CPSGS_Request> request,
                                             shared_ptr<CPSGS_Reply> reply,
                                             TProcessorPriority  priority) const;
    virtual void Process(void);
    virtual EPSGS_Status GetStatus(void);
    virtual string GetName(void) const;
    virtual string GetGroupName(void) const;
    virtual void ProcessEvent(void);

private:
    enum EPSGS_IPGStage {
        ePSGS_NoResolution,
        ePSGS_ResolveGIProtein,
        ePSGS_ResolveGINucleotide,
        ePSGS_ResolveNonGIProtein,
        ePSGS_ResolveNonGINucleotide,
        ePSGS_Finished
    };

public:
    CPSGS_IPGResolveProcessor();
    CPSGS_IPGResolveProcessor(shared_ptr<CPSGS_Request> request,
                              shared_ptr<CPSGS_Reply> reply,
                              TProcessorPriority  priority);
    virtual ~CPSGS_IPGResolveProcessor();

private:
    CPubseqGatewayFetchIpgReportRequest x_PrepareRequestOnOriginalValues(void);
    CPubseqGatewayFetchIpgReportRequest x_PrepareRequestOnResolvedlValues(void);
    string x_FormSeqId(SBioseqResolution &  bioseq_info);
    void x_InitiateIPGFetch(const CPubseqGatewayFetchIpgReportRequest &  request);
    void x_ProcessWithResolve(void);
    void x_DetectSeqIdTypes(void);
    bool x_AnyGIs(void);
    bool x_InitiateResolve(void);
    string x_GetNotFoundMessage(void);

private:
    bool x_OnIPGResolveData(vector<CIpgStorageReportEntry> &&  page,
                            bool  is_last,
                            CCassIPGFetch *  fetch_details);
    void x_OnIPGResolveError(CCassIPGFetch *  fetch_details,
                             CRequestStatus::ECode  status,
                             int  code,
                             EDiagSev  severity,
                             const string &  message);

private:
    void x_OnSeqIdResolveFinished(
                        SBioseqResolution &&  bioseq_resolution);
    void x_OnSeqIdResolveError(
                        CRequestStatus::ECode  status,
                        int  code,
                        EDiagSev  severity,
                        const string &  message);
    void x_OnResolutionGoodData(void);

private:
    void x_Peek(bool  need_wait);
    bool x_Peek(unique_ptr<CCassFetch> &  fetch_details,
                bool  need_wait);

private:
    SPSGS_IPGResolveRequest *   m_IPGResolveRequest;
    size_t                      m_RecordCount;
    EPSGS_IPGStage              m_IPGStage;

private:
    // Seq IDs which were detected as GIs and non GIs.
    // They are populated only for the case when seq_id_resolve is set to true
    optional<CSeq_id_Base::E_Choice>    m_ProteinType;
    optional<CSeq_id_Base::E_Choice>    m_NucleotideType;

    SBioseqResolution           m_ResolvedProtein;
    SBioseqResolution           m_ResolvedNucleotide;

    vector<string>              m_NotFoundCriterias;
};

#endif  // PSGS_IPGRESOLVE__HPP

