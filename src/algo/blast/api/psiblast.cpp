#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */
/* ===========================================================================
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
 * Author:  Christiam Camacho
 *
 */

/** @file psiblast.cpp
 * Implementation of CPsiBlast.
 */

#include <ncbi_pch.hpp>
#include <algo/blast/api/psiblast.hpp>
#include "psiblast_impl.hpp"
#include "psiblast_aux_priv.hpp"    // for PsiBlastAddAncillaryPssmData

// PSSM Engine includes
#include <algo/blast/api/psiblast_options.hpp>
#include <algo/blast/api/psi_pssm_input.hpp>
#include <algo/blast/api/pssm_engine.hpp>
#include "bioseq_extract_data_priv.hpp"     // for CBlastQuerySourceBioseqSet
#include <objects/scoremat/PssmWithParameters.hpp>

#include <objects/scoremat/PssmWithParameters.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CPsiBlast::CPsiBlast(CRef<IQueryFactory> query_factory,
                     CRef<CLocalDbAdapter> blastdb,
                     CConstRef<CPSIBlastOptionsHandle> options)
: m_Subject(blastdb), m_Impl(0)
{
    m_Impl = new CPsiBlastImpl(query_factory, m_Subject, 
         CConstRef<CBlastProteinOptionsHandle>(options.GetPointer()));
}

CPsiBlast::CPsiBlast(CRef<objects::CPssmWithParameters> pssm,
                     CRef<CLocalDbAdapter> blastdb,
                     CConstRef<CPSIBlastOptionsHandle> options)
: m_Subject(blastdb), m_Impl(0)
{
    m_Impl = new CPsiBlastImpl(pssm, m_Subject, options);
}

CPsiBlast::~CPsiBlast()
{
    if (m_Impl) {
        delete m_Impl;
    }
}

void
CPsiBlast::SetPssm(CConstRef<objects::CPssmWithParameters> pssm)
{
    m_Impl->SetPssm(pssm);
}

CConstRef<objects::CPssmWithParameters>
CPsiBlast::GetPssm() const
{
    return m_Impl->GetPssm();
}

CRef<CSearchResultSet>
CPsiBlast::Run()
{
    m_Impl->SetNumberOfThreads(GetNumberOfThreads());
    return m_Impl->Run();
}

CRef<objects::CPssmWithParameters> 
PsiBlastComputePssmFromAlignment(const objects::CBioseq& query,
                                 CConstRef<objects::CSeq_align_set> alignment,
                                 CRef<objects::CScope> database_scope,
                                 const CPSIBlastOptionsHandle& opts_handle,
                                 CConstRef<CBlastAncillaryData> ancillary_data,
                                 PSIDiagnosticsRequest* diagnostics_request)
{
    // Extract PSSM engine options from options handle
    CPSIBlastOptions opts;
    PSIBlastOptionsNew(&opts);
    opts->pseudo_count = opts_handle.GetPseudoCount();
    opts->inclusion_ethresh = opts_handle.GetInclusionThreshold();

    CBlastQuerySourceBioseqSet query_source(query, true);
    string warnings;
    const SBlastSequence query_seq = 
        query_source.GetBlastSequence(0, eBlastEncodingProtein,
                                      eNa_strand_unknown,
                                      eSentinels, &warnings);
    _ASSERT(warnings.empty());

    CPsiBlastInputData input(query_seq.data.get()+1,    // skip sentinel
                             query_seq.length-2,        // don't count sentinels
                             alignment, database_scope, 
                             *opts.Get(), 
                             opts_handle.GetMatrixName(),
                             opts_handle.GetGapOpeningCost(),
                             opts_handle.GetGapExtensionCost(),
                             diagnostics_request);

    CPssmEngine engine(&input);
    engine.SetUngappedStatisticalParams(ancillary_data);
    CRef<CPssmWithParameters> retval(engine.Run());

    PsiBlastAddAncillaryPssmData(*retval,
                                  opts_handle.GetGapOpeningCost(), 
                                  opts_handle.GetGapExtensionCost());
    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
