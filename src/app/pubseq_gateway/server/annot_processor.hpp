#ifndef PSGS_ANNOTPROCESSOR__HPP
#define PSGS_ANNOTPROCESSOR__HPP

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
 * File Description: named annotation processor
 *
 */

#include "resolve_base.hpp"

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

// Forward declaration
class CCassFetch;


class CPSGS_AnnotProcessor : public CPSGS_ResolveBase
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
    CPSGS_AnnotProcessor();
    CPSGS_AnnotProcessor(shared_ptr<CPSGS_Request> request,
                         shared_ptr<CPSGS_Reply> reply,
                         TProcessorPriority  priority);
    virtual ~CPSGS_AnnotProcessor();

private:
    static vector<string> x_FilterNames(const vector<string> &  names);
    static bool x_IsNameValid(const string &  name);

    void x_OnResolutionGoodData(void);
    void x_OnSeqIdResolveError(
                        CRequestStatus::ECode  status,
                        int  code,
                        EDiagSev  severity,
                        const string &  message);
    void x_OnSeqIdResolveFinished(
                        SBioseqResolution &&  bioseq_resolution);
    void x_SendBioseqInfo(SBioseqResolution &  bioseq_resolution);

private:
    bool x_OnNamedAnnotData(CNAnnotRecord &&  annot_record,
                            bool  last,
                            CCassNamedAnnotFetch *  fetch_details,
                            int32_t  sat);
    void x_OnNamedAnnotError(CCassNamedAnnotFetch *  fetch_details,
                             CRequestStatus::ECode  status,
                             int  code,
                             EDiagSev  severity,
                             const string &  message);

private:
    void x_Peek(bool  need_wait);
    bool x_Peek(unique_ptr<CCassFetch> &  fetch_details,
                bool  need_wait);

private:
    // The processor filters out some of the requested named annotations
    // This vector holds those which can be processed
    vector<string>              m_ValidNames;

    SPSGS_AnnotRequest *        m_AnnotRequest;
    bool                        m_Cancelled;
    bool                        m_InPeek;
};

#endif  // PSGS_RESOLVEPROCESSOR__HPP

