#ifndef PSGS_ACCESSIONVERHISTORYPROC__HPP
#define PSGS_ACCESSIONVERHISTORYPROC__HPP

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
 * File Description: accession version history processor
 *
 */

#include "resolve_base.hpp"

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

// Forward declaration
class CCassFetch;


class CPSGS_AccessionVersionHistoryProcessor : public CPSGS_ResolveBase
{
public:
    virtual IPSGS_Processor* CreateProcessor(shared_ptr<CPSGS_Request> request,
                                             shared_ptr<CPSGS_Reply> reply,
                                             TProcessorPriority  priority) const;
    virtual void Process(void);
    virtual void Cancel(void);
    virtual EPSGS_Status GetStatus(void);
    virtual string GetName(void) const;
    virtual void ProcessEvent(void);

public:
    CPSGS_AccessionVersionHistoryProcessor();
    CPSGS_AccessionVersionHistoryProcessor(shared_ptr<CPSGS_Request> request,
                                           shared_ptr<CPSGS_Reply> reply,
                                           TProcessorPriority  priority);
    virtual ~CPSGS_AccessionVersionHistoryProcessor();

private:
    void x_OnResolutionGoodData(void);
    void x_OnSeqIdResolveError(
                        CRequestStatus::ECode  status,
                        int  code,
                        EDiagSev  severity,
                        const string &  message);
    void x_OnSeqIdResolveFinished(
                        SBioseqResolution &&  bioseq_resolution);
    void x_SendBioseqInfo(SBioseqResolution &  bioseq_resolution);
    bool x_OnAccVerHistData(SAccVerHistRec &&  acc_ver_hist_record,
                            bool  last,
                            CCassAccVerHistoryFetch *  fetch_details);
    void x_OnAccVerHistError(CCassAccVerHistoryFetch *  fetch_details,
                             CRequestStatus::ECode  status,
                             int  code,
                             EDiagSev  severity,
                             const string &  message);

private:
    void x_Peek(bool  need_wait);
    bool x_Peek(unique_ptr<CCassFetch> &  fetch_details,
                bool  need_wait);

private:
    size_t                                  m_RecordCount;
    SPSGS_AccessionVersionHistoryRequest *  m_AccVerHistoryRequest;
};

#endif  // PSGS_ACCESSIONVERHISTORYPROC__HPP

