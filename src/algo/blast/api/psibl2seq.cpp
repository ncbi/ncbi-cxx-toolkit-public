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
#include <algo/blast/api/traceback_stage.hpp>
#include "blast_seqalign.hpp"
#include "prelim_search_runner.hpp"
#include "seqinfosrc_bioseq.hpp"
#include "psiblast_aux_priv.hpp"

// Object includes
#include <objects/scoremat/Pssm.hpp>
#include <objects/scoremat/PssmFinalData.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>
#include <objects/scoremat/PssmIntermediateData.hpp>
#include <objects/seqset/Seq_entry.hpp>

// Utility headers
#include <util/format_guess.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CPsiBl2Seq::CPsiBl2Seq(CRef<objects::CPssmWithParameters> pssm,
                       CRef<IQueryFactory> subject,
                       CConstRef<CPSIBlastOptionsHandle> options)
: m_Pssm(pssm), m_Query(0), m_Subject(subject), m_OptsHandle(options)
{
    x_Validate();
    x_ExtractQueryFromPssm();
    x_CreatePssmScoresFromFrequencyRatios();
}

CPsiBl2Seq::CPsiBl2Seq(CRef<IQueryFactory> query,
                       CRef<IQueryFactory> subject,
                       CConstRef<CBlastProteinOptionsHandle> options)
: m_Pssm(0), m_Query(query), m_Subject(subject), m_OptsHandle(options)
{
    x_Validate();
}

/// Enumeration to specify the different uses of the query factory
enum EQueryFactoryType { eQuery, eSubject };

/// Function to perform sanity checks on the query factory
void
s_QueryFactorySanityCheck(CRef<IQueryFactory> query_factory, 
                          CConstRef<CBlastOptionsHandle> opts_handle,
                          EQueryFactoryType query_factory_type)
{
    CRef<ILocalQueryData> query_data =
        query_factory->MakeLocalQueryData(&opts_handle->GetOptions());

    // Compose the exception error message
    string excpt_msg("CPsiBl2Seq only accepts ");
    if (query_factory_type == eQuery) {
        excpt_msg += "one protein sequence as query";
    } else if (query_factory_type == eSubject) {
        excpt_msg += "protein sequences as subjects";
    } else {
        abort();
    }

    if (query_factory_type == eQuery) {
        if (query_data->GetNumQueries() != 1) {
            NCBI_THROW(CBlastException, eInvalidArgument, excpt_msg);
        }
    }

    BLAST_SequenceBlk* sblk = NULL;
    try { sblk = query_data->GetSequenceBlk(); }
    catch (const CBlastException& e) {
        if (e.GetMsg().find("Incompatible sequence codings") != ncbi::NPOS) {
            NCBI_THROW(CBlastException, eInvalidArgument, excpt_msg);
        }
    }
    ASSERT(sblk);
    ASSERT(sblk->length > 0);

    CFormatGuess::ESequenceType sequence_type =
        CFormatGuess::SequenceType((const char*)sblk->sequence_start,
                                   static_cast<unsigned>(sblk->length));
    if (sequence_type == CFormatGuess::eNucleotide) {
        NCBI_THROW(CBlastException, eInvalidArgument, excpt_msg);
    }
}

void
CPsiBl2Seq::x_Validate()
{
    // Either PSSM or a protein query must be provided
    if (m_Pssm.Empty() && m_Query.Empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument, "Missing PSSM or Query");
    }

    // Options validation
    if (m_OptsHandle.Empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument, "Missing options");
    }
    m_OptsHandle->Validate();

    // PSSM/Query sanity checks
    if (m_Pssm.NotEmpty()) {
        ValidatePssm(*m_Pssm);
    } else if (m_Query.NotEmpty()) {
        s_QueryFactorySanityCheck(m_Query, m_OptsHandle, eQuery);
    } else {
        abort();
    }

    // Subject sequence(s) sanity checks
    if (m_Subject.Empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
                   "Missing subject sequence data source");
    } else {
        s_QueryFactorySanityCheck(m_Subject, m_OptsHandle, eSubject);
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
    m_Query.Reset(new CObjMgrFree_QueryFactory(query_bioseq));
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
    BlastSeqSrc* seqsrc(QueryFactoryBlastSeqSrcInit(subject_factory, kProgram));
    retval->m_SeqSrc.Reset(new TBlastSeqSrc(seqsrc, BlastSeqSrcFree));

    return retval;
}

CRef<CSearchResults>
CPsiBl2Seq::Run()
{
    int status(0);
    const CBlastOptions& opts = m_OptsHandle->GetOptions();
    CRef<SInternalData> core_data(s_SetUpInternalDataStructures(m_Query, 
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

    bool is_prot = BlastSeqSrcGetIsProt
        (core_data->m_SeqSrc->GetPointer()) ? true : false;
    CRef<IRemoteQueryData> subject_data(m_Subject->MakeRemoteQueryData());
    CRef<CBioseq_set> subject_bioseqs(subject_data->GetBioseqSet());
    CBioseqSeqInfoSrc seqinfo_src(*subject_bioseqs, is_prot);

    CBlastTracebackSearch tback(m_Query, core_data, opts, seqinfo_src);
    m_Results = tback.Run();
    return CRef<CSearchResults>(&m_Results[0]);
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
