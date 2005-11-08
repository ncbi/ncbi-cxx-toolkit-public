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
#include "prelim_search_runner.hpp"
#include "psiblast_aux_priv.hpp"
#include <algo/blast/api/psiblast_options.hpp>
#include <algo/blast/api/traceback_stage.hpp>
#include <algo/blast/api/setup_factory.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/api/psiblast_options.hpp>
#include <algo/blast/api/objmgrfree_query_data.hpp>

// Object includes
#include <objects/scoremat/Pssm.hpp>
#include <objects/scoremat/PssmFinalData.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>
#include <objects/seqset/Seq_entry.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CPsiBlastImpl::CPsiBlastImpl(CRef<objects::CPssmWithParameters> pssm,
                             IPsiBlastSubject* subject,
                             CConstRef<CPSIBlastOptionsHandle> options)
: m_Pssm(pssm), m_Query(0), m_Subject(subject), m_OptsHandle(options)
{
    x_Validate();
    x_ExtractQueryFromPssm();
    x_CreatePssmScoresFromFrequencyRatios();
}

CPsiBlastImpl::CPsiBlastImpl(CRef<IQueryFactory> query,
                             IPsiBlastSubject* subject,
                             CConstRef<CBlastProteinOptionsHandle> options)
: m_Pssm(0), m_Query(query), m_Subject(subject), m_OptsHandle(options)
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
    if ( !m_Subject ) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
                   "Missing database or subject sequences");
    }
    m_Subject->Validate();
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
    m_Query.Reset(new CObjMgrFree_QueryFactory(query_bioseq));
}

static CRef<SInternalData> 
s_SetUpInternalDataStructures(CRef<IQueryFactory> query_factory,
                              IPsiBlastSubject* subject,
                              CConstRef<CPssmWithParameters> pssm,
                              const CBlastOptions& options)
{
    CRef<SInternalData> retval(new SInternalData);

    // 1. Initialize the query data (borrow it from the factory)
    CRef<ILocalQueryData> query_data = 
        query_factory->MakeLocalQueryData(&options);
    retval->m_Queries = query_data->GetSequenceBlk();
    retval->m_QueryInfo = query_data->GetQueryInfo();

    // 2. Create the options memento
    CConstRef<CBlastOptionsMemento> opts_memento(options.CreateSnapshot());

    // 3. Create the BlastScoreBlk
    BlastSeqLoc* lookup_segments(0);
    BlastScoreBlk* sbp =
        CSetupFactory::CreateScoreBlock(opts_memento, query_data,
                                        &lookup_segments,
                                        /* FIXME: masked locations */ 0);
    retval->m_ScoreBlk.Reset(new TBlastScoreBlk(sbp, BlastScoreBlkFree));
    if (pssm.NotEmpty()) {
        PsiBlastSetupScoreBlock(sbp, pssm);
    }

    // 4. Create lookup table
    LookupTableWrap* lut =
        CSetupFactory::CreateLookupTable(query_data, opts_memento, 
                                         retval->m_ScoreBlk->GetPointer(), 
                                         lookup_segments);
    retval->m_LookupTable.Reset(new TLookupTableWrap(lut, LookupTableWrapFree));
    lookup_segments = BlastSeqLocFree(lookup_segments);
    ASSERT(lookup_segments == NULL);

    // 5. Create diagnostics
    BlastDiagnostics* diags = CSetupFactory::CreateDiagnosticsStructure();
    retval->m_Diagnostics.Reset(new TBlastDiagnostics(diags, 
                                                      Blast_DiagnosticsFree));

    // 6. Create HSP stream
    BlastHSPStream* hsp_stream = 
        CSetupFactory::CreateHspStream(opts_memento, 
                                       query_data->GetNumQueries());
    retval->m_HspStream.Reset(new TBlastHSPStream(hsp_stream, 
                                                  BlastHSPStreamFree));

    // 7. Set up the BlastSeqSrc
    retval->m_SeqSrc.Reset(new TBlastSeqSrc(subject->MakeSeqSrc(), 0));

    return retval;
}

CRef<CSearchResults>
CPsiBlastImpl::Run()
{
    int status(0);
    const CBlastOptions& opts = m_OptsHandle->GetOptions();
    CRef<SInternalData> core_data(s_SetUpInternalDataStructures(m_Query, 
                                                                m_Subject, 
                                                                m_Pssm,
                                                                opts));
    CConstRef<CBlastOptionsMemento> opts_memento(opts.CreateSnapshot());

    // Run the preliminary stage
    status = CPrelimSearchRunner(*core_data, opts_memento)();
    if (status) {
        string msg("Preliminary search failed with status ");
        msg += NStr::IntToString(status);
        NCBI_THROW(CBlastException, eCoreBlastError, msg);
    }

    // Run the traceback stage
    IBlastSeqInfoSrc* seqinfo_src(m_Subject->MakeSeqInfoSrc());
    ASSERT(seqinfo_src);
    CBlastTracebackSearch tback(m_Query, core_data, opts, *seqinfo_src);
    m_Results = tback.Run();
    return CRef<CSearchResults>(&m_Results[0]);
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

CConstRef<CPssmWithParameters>
CPsiBlastImpl::GetPssm() const 
{
    return m_Pssm;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
