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

/** @file psibl2seq.cpp
 * Implementation of CPsiBl2Seq.
 */

#include <ncbi_pch.hpp>
#include <algo/blast/api/psibl2seq.hpp>
#include <algo/blast/api/setup_factory.hpp>
#include <algo/blast/api/seqsrc_query_factory.hpp>
#include <algo/blast/api/objmgrfree_query_data.hpp>
#include "blast_seqalign.hpp"
#include "prelim_search_runner.hpp"
#include "seqinfosrc_bioseq.hpp"
#include "psiblast_aux_priv.hpp"
#include "/home/camacho/work/debug_lib/new_blast_debug.h"
#include "/home/camacho/work/debug_lib/current_function.hpp"

// Object includes
#include <objects/scoremat/Pssm.hpp>
#include <objects/scoremat/PssmFinalData.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>
#include <objects/scoremat/PssmIntermediateData.hpp>
#include <objects/seqset/Seq_entry.hpp>

// Core BLAST includes
#include <algo/blast/core/blast_traceback.h>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CPsiBl2Seq::CPsiBl2Seq(CRef<CPssmWithParameters> pssm,
                       CRef<IQueryFactory> subject,
                       CConstRef<CPSIBlastOptionsHandle> options)
: m_Pssm(pssm), m_Subject(subject), m_OptsHandle(options), m_QueryFactory(0)
{
    x_Validate();
    x_ExtractQueryFromPssm();
    x_CreatePssmScoresFromFrequencyRatios();
}

void
CPsiBl2Seq::x_Validate() const
{
    // PSSM/Query sequence sanity checks
    if (m_Pssm.Empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument, "Missing PSSM");
    }
    if ( !m_Pssm->CanGetPssm() ) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
                   "Missing PSSM data");
    }

    bool missing_scores(false);
    if ( !m_Pssm->GetPssm().CanGetFinalData() || 
         !m_Pssm->GetPssm().GetFinalData().CanGetScores() || 
         m_Pssm->GetPssm().GetFinalData().GetScores().empty() ) {
        missing_scores = true;
    }

    bool missing_freq_ratios(false);
    if ( !m_Pssm->GetPssm().CanGetIntermediateData() || 
         !m_Pssm->GetPssm().GetIntermediateData().CanGetFreqRatios() || 
         m_Pssm->GetPssm().GetIntermediateData().GetFreqRatios().empty() ) {
        missing_freq_ratios = true;
    }

    if (missing_freq_ratios && missing_scores) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
                   "PSSM data must contain either scores or frequency ratios");
    }

    if ( !m_Pssm->GetPssm().CanGetQuery() ) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
                   "Missing query sequence in PSSM");
    }
    if ( !m_Pssm->GetPssm().GetQuery().IsSeq() ) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
                   "Query sequence in ASN.1 PSSM is not a single Bioseq");
    }

    // Options validation
    if (m_OptsHandle.Empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument, "Missing options");
    }

    m_OptsHandle->Validate();

    // Subject sequence(s) sanity checks
    if (m_Subject.Empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
                   "Missing subject sequence data source");
    }
}

void
CPsiBl2Seq::x_CreatePssmScoresFromFrequencyRatios()
{
    if ( !m_Pssm->GetPssm().CanGetFinalData() ||
         !m_Pssm->GetPssm().GetFinalData().CanGetScores() ||
         m_Pssm->GetPssm().GetFinalData().GetScores().empty() ) {
        PsiBlastComputePssmScores(m_Pssm, m_OptsHandle->GetOptions());
    }
}

void
CPsiBl2Seq::x_ExtractQueryFromPssm()
{
    CConstRef<CBioseq> query_bioseq(&m_Pssm->GetPssm().GetQuery().GetSeq());
    m_QueryFactory.Reset(new CObjMgrFree_QueryFactory(query_bioseq));
}

static CRef<SInternalData> 
s_SetUpInternalDataStructures(CRef<IQueryFactory> query_factory,
                              CRef<IQueryFactory> subject_factory, 
                              CConstRef<CPssmWithParameters> pssm,
                              const CBlastOptions& options)
{
    CRef<SInternalData> retval(new SInternalData);
    const EBlastProgramType kProgram(options.GetProgramType());

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
                                        &lookup_segments);
    retval->m_ScoreBlk.Reset(new TBlastScoreBlk(sbp, BlastScoreBlkFree));
    PsiBlastSetupScoreBlock(sbp, pssm);

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
    BlastSeqSrc* seqsrc(QueryFactoryBlastSeqSrcInit(subject_factory, kProgram));
    retval->m_SeqSrc.Reset(new TBlastSeqSrc(seqsrc, BlastSeqSrcFree));

    return retval;
}

CRef<CSearchResults>
CPsiBl2Seq::Run()
{
    int status(0);
    const CBlastOptions& opts = m_OptsHandle->GetOptions();
    CRef<SInternalData> core_data(s_SetUpInternalDataStructures(m_QueryFactory, 
                                                                m_Subject, 
                                                                m_Pssm,
                                                                opts));

    CConstRef<CBlastOptionsMemento> opts_memento(opts.CreateSnapshot());
    status = CPrelimSearchRunner(*core_data, opts_memento)();
    if (status) {
        string msg("Preliminary search failed with status ");
        msg += NStr::IntToString(status);
        NCBI_THROW(CBlastException, eCoreBlastError, msg);
    }

    CBlastHSPResults hsp_results;
    status = Blast_RunTracebackSearch(opts_memento->m_ProgramType, 
                                      core_data->m_Queries, 
                                      core_data->m_QueryInfo, 
                                      core_data->m_SeqSrc->GetPointer(), 
                                      opts_memento->m_ScoringOpts, 
                                      opts_memento->m_ExtnOpts, 
                                      opts_memento->m_HitSaveOpts, 
                                      opts_memento->m_EffLenOpts, 
                                      opts_memento->m_DbOpts, 
                                      opts_memento->m_PSIBlastOpts, 
                                      core_data->m_ScoreBlk->GetPointer(), 
                                      core_data->m_HspStream->GetPointer(), 
                                      0, /* RPS-BLAST data */ 
                                      0, /* PHI-BLAST data */
                                      &hsp_results);
    if (status) {
        string msg("Traceback search failed with status ");
        msg += NStr::IntToString(status);
        NCBI_THROW(CBlastException, eCoreBlastError, msg);
    }
    
    bool is_prot = BlastSeqSrcGetIsProt
        (core_data->m_SeqSrc->GetPointer()) ? true : false;

    CRef<IRemoteQueryData> subject_data(m_Subject->MakeRemoteQueryData());
    CRef<CBioseq_set> subject_bioseqs(subject_data->GetBioseqSet());
    
    CBioseqSeqInfoSrc seqinfo_src(*subject_bioseqs, is_prot);
    TSeqAlignVector aligns =
        LocalBlastResults2SeqAlign(hsp_results,
           *m_QueryFactory->MakeLocalQueryData(&opts),
           seqinfo_src,
           opts_memento->m_ProgramType,
           opts.GetGappedMode(),
           opts.GetOutOfFrameMode());
    
    // The code should probably capture and return errors here; the
    // traceback stage does not seem to produce messages, but the
    // preliminary stage does; they should be saved and returned here
    // if they have not been returned or reported yet.
    
    ASSERT(aligns.size() == 1);
    CSearchResults::TErrors no_errors(aligns.size());
    return CRef<CSearchResults>(new CSearchResults(aligns[0], no_errors));
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
