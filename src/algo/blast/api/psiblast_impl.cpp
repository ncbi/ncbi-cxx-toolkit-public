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

/// @file psiblast_impl.cpp
/// Implements implementation class for PSI-BLAST and PSI-BLAST 2 Sequences

#include <ncbi_pch.hpp>
#include "psiblast_impl.hpp"
#include "psiblast_aux_priv.hpp"
#include <algo/blast/api/psiblast_options.hpp>
#include <algo/blast/api/prelim_stage.hpp>
#include <algo/blast/api/traceback_stage.hpp>
#include <algo/blast/api/setup_factory.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/api/objmgrfree_query_data.hpp>
#include <algo/blast/api/blast_seqinfosrc.hpp>

// Object includes
#include <objects/seqset/Seq_entry.hpp>
#include <objects/scoremat/Pssm.hpp>
#include <objects/scoremat/PssmFinalData.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CPsiBlastImpl::CPsiBlastImpl(CRef<objects::CPssmWithParameters> pssm,
                             CRef<CLocalDbAdapter> subject,
                             CConstRef<CPSIBlastOptionsHandle> options)
: m_Pssm(pssm), m_Query(0), m_Subject(subject), m_OptsHandle(options),
    m_ResultType(eDatabaseSearch)
{
    x_Validate();
    x_ExtractQueryFromPssm();
    x_CreatePssmScoresFromFrequencyRatios();
}

CPsiBlastImpl::CPsiBlastImpl(CRef<IQueryFactory> query,
                             CRef<CLocalDbAdapter> subject,
                             CConstRef<CBlastProteinOptionsHandle> options)
: m_Pssm(0), m_Query(query), m_Subject(subject), m_OptsHandle(options),
    m_ResultType(eDatabaseSearch)
{
    x_Validate();
}

void
CPsiBlastImpl::x_Validate()
{
    // Validate the options
    if (m_OptsHandle.Empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument, "Missing options");
    }
    m_OptsHandle->Validate();

    // Either PSSM or a protein query must be provided
    if (m_Pssm.NotEmpty()) {
        CPsiBlastValidate::Pssm(*m_Pssm);
    } else if (m_Query.NotEmpty()) {
        CPsiBlastValidate::QueryFactory(m_Query, *m_OptsHandle);
    } else {
        NCBI_THROW(CBlastException, eInvalidArgument, "Missing query or pssm");
    }

    // Validate the subject
    if (m_Subject.Empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
                   "Missing database or subject sequences");
    }
}

void
CPsiBlastImpl::x_CreatePssmScoresFromFrequencyRatios()
{
    if ( !m_Pssm->GetPssm().CanGetFinalData() ||
         !m_Pssm->GetPssm().GetFinalData().CanGetScores() ||
         m_Pssm->GetPssm().GetFinalData().GetScores().empty() ) {
        PsiBlastComputePssmScores(m_Pssm, m_OptsHandle->GetOptions());
    }
}

void
CPsiBlastImpl::x_ExtractQueryFromPssm()
{
    CConstRef<CBioseq> query_bioseq(&m_Pssm->GetPssm().GetQuery().GetSeq());
    m_Query.Reset(new CObjMgrFree_QueryFactory(query_bioseq)); /* NCBI_FAKE_WARNING */
}

CRef<CSearchResultSet>
CPsiBlastImpl::Run()
{
    CRef<CBlastOptions>
        opts(const_cast<CBlastOptions*>(&m_OptsHandle->GetOptions()));

    // FIXME: Move the following line and initialization of all
    // BlastSeqSrc/subjects to CBlastPrelimSearch::x_Init
    m_Subject->ResetBlastSeqSrcIteration();

    // Run the preliminary stage
    CBlastPrelimSearch prelim_search(m_Query, 
                                     opts, 
                                     m_Subject->MakeSeqSrc(), 
                                     m_Pssm);
    prelim_search.SetNumberOfThreads(GetNumberOfThreads());
    CRef<SInternalData> core_data = prelim_search.Run();

    // Run the traceback stage
    CRef<IBlastSeqInfoSrc> seqinfo_src(m_Subject->MakeSeqInfoSrc());
    _ASSERT(seqinfo_src.NotEmpty());
    TSearchMessages search_messages = prelim_search.GetSearchMessages();
    CBlastTracebackSearch tback(m_Query, 
                                core_data, 
                                opts, 
                                seqinfo_src,
                                search_messages);
    tback.SetResultType(m_ResultType);
    m_Results = tback.Run();

    // Save the K&A values be as they might have been modified in the 
    // composition adjustment library
    if (m_Pssm.NotEmpty()) {
        CPssm& pssm = m_Pssm->SetPssm();
        pssm.SetLambda
            (core_data->m_ScoreBlk->GetPointer()->kbp_gap_psi[0]->Lambda);
        pssm.SetKappa
            (core_data->m_ScoreBlk->GetPointer()->kbp_gap_psi[0]->K);
        pssm.SetH
            (core_data->m_ScoreBlk->GetPointer()->kbp_gap_psi[0]->H);
        pssm.SetLambdaUngapped
            (core_data->m_ScoreBlk->GetPointer()->kbp_psi[0]->Lambda);
        pssm.SetKappaUngapped
            (core_data->m_ScoreBlk->GetPointer()->kbp_psi[0]->K);
        pssm.SetHUngapped
            (core_data->m_ScoreBlk->GetPointer()->kbp_psi[0]->H);
    }
    return m_Results;
}

void
CPsiBlastImpl::SetPssm(CConstRef<objects::CPssmWithParameters> pssm)
{
    if (pssm.Empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
                   "Setting empty reference for pssm");
    }
    CPsiBlastValidate::Pssm(*pssm, true);
    m_Pssm.Reset(const_cast<CPssmWithParameters*>(&*pssm));
}

void
CPsiBlastImpl::SetResultType(EResultType type)
{
    m_ResultType = type;
}

CConstRef<CPssmWithParameters>
CPsiBlastImpl::GetPssm() const 
{
    return m_Pssm;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
