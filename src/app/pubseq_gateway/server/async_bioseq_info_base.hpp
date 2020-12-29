#ifndef PSGS_ASYNCBIOSEQINFOBASE__HPP
#define PSGS_ASYNCBIOSEQINFOBASE__HPP

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
 * File Description: base class for processors which need to retrieve bioseq
 *                   info asynchronously
 *
 */

#include <corelib/request_status.hpp>
#include <corelib/ncbidiag.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info/record.hpp>

#include "cass_fetch.hpp"
#include "cass_processor_base.hpp"
#include "async_resolve_base.hpp"

#include <objects/seqloc/Seq_id.hpp>
USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_IDBLOB_SCOPE;


class CPSGS_AsyncBioseqInfoBase : virtual public CPSGS_CassProcessorBase
{
public:
    CPSGS_AsyncBioseqInfoBase();
    CPSGS_AsyncBioseqInfoBase(shared_ptr<CPSGS_Request> request,
                              shared_ptr<CPSGS_Reply> reply,
                              TSeqIdResolutionFinishedCB finished_cb,
                              TSeqIdResolutionErrorCB error_cb);
    virtual ~CPSGS_AsyncBioseqInfoBase();

protected:
    void MakeRequest(SBioseqResolution &&  bioseq_resolution);

private:
    void x_MakeRequest(void);
    void x_OnBioseqInfo(vector<CBioseqInfoRecord>&&  records);
    void x_OnBioseqInfoWithoutSeqIdType(vector<CBioseqInfoRecord>&&  records);
    void x_OnBioseqInfoError(CRequestStatus::ECode  status, int  code,
                             EDiagSev  severity, const string &  message);

private:
    SBioseqResolution                   m_BioseqResolution;

    TSeqIdResolutionFinishedCB          m_FinishedCB;
    TSeqIdResolutionErrorCB             m_ErrorCB;

    bool                                m_NeedTrace;

    CCassFetch *                        m_Fetch;
    CCassFetch *                        m_NoSeqIdTypeFetch;
    TPSGS_HighResolutionTimePoint       m_BioseqRequestStart;

    bool                                m_WithSeqIdType;
};

#endif  // PSGS_ASYNCBIOSEQINFOBASE__HPP

