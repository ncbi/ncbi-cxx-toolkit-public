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

/// @file psiblast_aux_priv.cpp
/// 

#include <ncbi_pch.hpp>
#include "psiblast_aux_priv.hpp"
#include <algo/blast/core/blast_stat.h>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_encoding.h>

#include <algo/blast/api/pssm_engine.hpp>
#include <algo/blast/api/psi_pssm_input.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/objmgrfree_query_data.hpp>
#include "blast_aux_priv.hpp"

// Utility headers
#include <util/format_guess.hpp>
#include <util/math/matrix.hpp>

// Object includes
#include <objects/seqloc/Seq_id.hpp>
#include <objects/scoremat/Pssm.hpp>
#include <objects/scoremat/PssmFinalData.hpp>
#include <objects/scoremat/PssmParameters.hpp>
#include <objects/scoremat/FormatRpsDbParameters.hpp>
#include <objects/scoremat/PssmIntermediateData.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqalign/Dense_seg.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

void PsiBlastSetupScoreBlock(BlastScoreBlk* score_blk,
                             CConstRef<CPssmWithParameters> pssm)
{
    ASSERT(score_blk);
    ASSERT(pssm.NotEmpty());

    if ( !score_blk->protein_alphabet ) {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "BlastScoreBlk is not configured for a protein alphabet");
    }

    // Assign the gapped Karlin-Altschul block
    score_blk->kbp_gap_psi[0]->Lambda =
        pssm->GetPssm().GetFinalData().GetLambda();
    score_blk->kbp_gap_psi[0]->K = pssm->GetPssm().GetFinalData().GetKappa();
    score_blk->kbp_gap_psi[0]->logK = log(score_blk->kbp_gap_psi[0]->K);
    score_blk->kbp_gap_psi[0]->H = pssm->GetPssm().GetFinalData().GetH();

    // Assign the matrix scores/frequency ratios
    const size_t kQueryLength = pssm->GetPssm().GetNumColumns();
    score_blk->psi_matrix = SPsiBlastScoreMatrixNew(kQueryLength);
    ASSERT((int)score_blk->alphabet_size == (int)pssm->GetPssm().GetNumRows());

    // Get the scores
    bool missing_scores = false;
    try {
        auto_ptr< CNcbiMatrix<int> > scores
            (CScorematPssmConverter::GetScores(pssm));
        ASSERT(score_blk->psi_matrix->pssm->ncols == scores->GetCols());
        ASSERT(score_blk->psi_matrix->pssm->nrows == scores->GetRows());

        for (TSeqPos c = 0; c < scores->GetCols(); c++) {
            for (TSeqPos r = 0; r < scores->GetRows(); r++) {
                score_blk->psi_matrix->pssm->data[c][r] = (*scores)(r, c);
            }
        }
    } catch (const std::runtime_error&) {
        missing_scores = true;
    }

    // Get the frequency ratios
    bool missing_freq_ratios = false;
    try {
        auto_ptr< CNcbiMatrix<double> > freq_ratios
            (CScorematPssmConverter::GetFreqRatios(pssm));
        ASSERT(score_blk->psi_matrix->pssm->ncols == 
               freq_ratios->GetCols());
        ASSERT(score_blk->psi_matrix->pssm->nrows == 
               freq_ratios->GetRows());

        for (TSeqPos c = 0; c < freq_ratios->GetCols(); c++) {
            for (TSeqPos r = 0; r < freq_ratios->GetRows(); r++) {
                score_blk->psi_matrix->freq_ratios[c][r] = 
                    (*freq_ratios)(r, c);
            }
        }
    } catch (const std::runtime_error&) {
        missing_freq_ratios = true;
    }

    if (missing_scores && missing_freq_ratios) {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "Missing scores and frequency ratios in PSSM");
    }
}

CNcbiMatrix<int>*
CScorematPssmConverter::GetScores(CConstRef<objects::CPssmWithParameters> 
                                  pssm_asn)
{
    if (pssm_asn.Empty()) {
        throw runtime_error("Cannot use NULL ASN.1 PSSM");
    }

    if ( !pssm_asn->GetPssm().CanGetFinalData() ||
         !pssm_asn->GetPssm().GetFinalData().CanGetScores() ) {
        throw runtime_error("Cannot obtain scores from ASN.1 PSSM");
    }

    const CPssm& pssm = pssm_asn->GetPssm();
    ASSERT((size_t)pssm.GetFinalData().GetScores().size() ==
           (size_t)pssm.GetNumRows()*pssm_asn->GetPssm().GetNumColumns());

    auto_ptr< CNcbiMatrix<int> > retval
        (new CNcbiMatrix<int>(pssm.GetNumRows(), 
                              pssm.GetNumColumns(), 
                              BLAST_SCORE_MIN));
    ASSERT(retval->GetRows() == (size_t)BLASTAA_SIZE);

    CPssmFinalData::TScores::const_iterator itr =
        pssm.GetFinalData().GetScores().begin();
    if (pssm.GetByRow() == true) {
        for (TSeqPos r = 0; r < retval->GetRows(); r++) {
            for (TSeqPos c = 0; c < retval->GetCols(); c++) {
                (*retval)(r, c) = *itr++;
            }
        }
    } else {
        for (TSeqPos c = 0; c < retval->GetCols(); c++) {
            for (TSeqPos r = 0; r < retval->GetRows(); r++) {
                (*retval)(r, c) = *itr++;
            }
        }
    }
    ASSERT(itr == pssm.GetFinalData().GetScores().end());
    return retval.release();
}

CNcbiMatrix<double>*
CScorematPssmConverter::GetFreqRatios(CConstRef<objects::CPssmWithParameters> 
                                      pssm_asn)
{
    if (pssm_asn.Empty()) {
        throw runtime_error("Cannot use NULL PSSM");
    }

    if ( !pssm_asn->GetPssm().CanGetIntermediateData() ||
         !pssm_asn->GetPssm().GetIntermediateData().CanGetFreqRatios() ) {
        throw runtime_error("Cannot obtain frequency ratios from ASN.1 PSSM");
    }

    const CPssm& pssm = pssm_asn->GetPssm();
    ASSERT((size_t)pssm.GetIntermediateData().GetFreqRatios().size() ==
           (size_t)pssm.GetNumRows()*pssm_asn->GetPssm().GetNumColumns());

    auto_ptr< CNcbiMatrix<double> > retval
        (new CNcbiMatrix<double>(pssm.GetNumRows(),
                                 pssm.GetNumColumns()));
    ASSERT(retval->GetRows() == (size_t)BLASTAA_SIZE);

    CPssmIntermediateData::TFreqRatios::const_iterator itr =
        pssm.GetIntermediateData().GetFreqRatios().begin();
    if (pssm.GetByRow() == true) {
        for (TSeqPos r = 0; r < retval->GetRows(); r++) {
            for (TSeqPos c = 0; c < retval->GetCols(); c++) {
                (*retval)(r, c) = *itr++;
            }
        }
    } else {
        for (TSeqPos c = 0; c < retval->GetCols(); c++) {
            for (TSeqPos r = 0; r < retval->GetRows(); r++) {
                (*retval)(r, c) = *itr++;
            }
        }
    }
    ASSERT(itr == pssm.GetIntermediateData().GetFreqRatios().end());
    return retval.release();
}

/** Even though the query sequence and the matrix gap costs are not a
 * product of the PSSM engine, set them as they are required for the
 * PSI-BLAST (query sequence) and RPS-BLAST/formatrpsdb (gap costs)
 * @param pssm PSSM to modify [in|out]
 * @param query Query sequence which corresponds to the PSSM [in]
 * @param gap_open Gap opening cost associated with the matrix used to build
 * the PSSM [in]
 * @param gap_extend Gap extension cost associated with the matrix used to 
 * build the PSSM [in]
 */
void
PsiBlastAddAncilliaryPssmData(CPssmWithParameters& pssm, 
                              const CBioseq& query, 
                              int gap_open, 
                              int gap_extend)
{
    pssm.SetPssm().SetQuery().SetSeq(const_cast<CBioseq&>(query));
    ASSERT(pssm.GetParams().GetRpsdbparams().IsSetMatrixName());
    pssm.SetParams().SetRpsdbparams().SetGapOpen(gap_open);
    pssm.SetParams().SetRpsdbparams().SetGapExtend(gap_extend);
}

void PsiBlastComputePssmScores(CRef<CPssmWithParameters> pssm,
                               const CBlastOptions& opts)
{
    CConstRef<CBioseq> query(&pssm->GetPssm().GetQuery().GetSeq());
    CRef<IQueryFactory> seq_fetcher(new CObjMgrFree_QueryFactory(query));

    CRef<ILocalQueryData> query_data(seq_fetcher->MakeLocalQueryData(&opts));
    BLAST_SequenceBlk* seqblk = query_data->GetSequenceBlk();
    ASSERT(query_data->GetSeqLength(0) == seqblk->length);
    ASSERT(query_data->GetSeqLength(0) == pssm->GetPssm().GetNumColumns());
    auto_ptr< CNcbiMatrix<double> > freq_ratios
        (CScorematPssmConverter::GetFreqRatios(pssm));

    CPsiBlastInputFreqRatios pssm_engine_input(seqblk->sequence, 
                                               seqblk->length, 
                                               *freq_ratios, 
                                               opts.GetMatrixName());
    CPssmEngine pssm_engine(&pssm_engine_input);
    CRef<CPssmWithParameters> pssm_with_scores(pssm_engine.Run());

    pssm->SetPssm().SetFinalData().SetScores() =
        pssm_with_scores->GetPssm().GetFinalData().GetScores();
    pssm->SetPssm().SetFinalData().SetLambda() =
        pssm_with_scores->GetPssm().GetFinalData().GetLambda();
    pssm->SetPssm().SetFinalData().SetKappa() =
        pssm_with_scores->GetPssm().GetFinalData().GetKappa();
    pssm->SetPssm().SetFinalData().SetH() =
        pssm_with_scores->GetPssm().GetFinalData().GetH();

    PsiBlastAddAncilliaryPssmData(*pssm, *query, 
                                  opts.GetGapOpeningCost(), 
                                  opts.GetGapExtensionCost());
}

/// Returns the evalue from this score object
/// @param score ASN.1 score object [in]
static double 
s_GetEvalue(const CScore& score)
{
    string score_type = score.GetId().GetStr();
    if (score.GetValue().IsReal() && 
       (score_type == "e_value" || score_type == "sum_e")) {
        return score.GetValue().GetReal();
    }
    return numeric_limits<double>::max();
}

double GetLowestEvalue(const CDense_seg::TScores& scores)
{
    double retval = numeric_limits<double>::max();
    double tmp;

    ITERATE(CDense_seg::TScores, i, scores) {
        if ( (tmp = s_GetEvalue(**i)) < retval) {
            retval = tmp;
        }
    }
    return retval;
}

void
CPsiBlastAlignmentProcessor::operator()
    (const objects::CSeq_align_set& alignments, 
     double evalue_inclusion_threshold, 
     THitIdentifiers& output)
{
    output.clear();
    unsigned int seq_index = 0;

    // For each discontinuous Seq-align corresponding to each query-subj pair
    // (i.e.: for each hit)
    ITERATE(CSeq_align_set::Tdata, hit, alignments.Get()) {
        ASSERT((*hit)->GetSegs().IsDisc());

        // For each HSP of this query-subj pair
        ITERATE(CSeq_align::C_Segs::TDisc::Tdata, hsp,
                (*hit)->GetSegs().GetDisc().Get()) {
            
            // Look for HSP with score less than inclusion_ethresh
            double e = GetLowestEvalue((*hsp)->GetScore());
            if (e < evalue_inclusion_threshold) {
                CRef<CSeq_id> id(const_cast<CSeq_id*>(&(*hsp)->GetSeq_id(1)));
                output.push_back(make_pair(seq_index, id));
                break;
            }
        }
        seq_index++;
    }

    _ASSERT(seq_index == alignments.Get().size());
}

void 
CPsiBlastValidate::Pssm(const objects::CPssmWithParameters& pssm,
                        bool require_scores)
{
    if ( !pssm.CanGetPssm() ) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
                   "Missing PSSM data");
    }

    bool missing_scores(false);
    if ( !pssm.GetPssm().CanGetFinalData() || 
         !pssm.GetPssm().GetFinalData().CanGetScores() || 
         pssm.GetPssm().GetFinalData().GetScores().empty() ) {
        missing_scores = true;
    }

    bool missing_freq_ratios(false);
    if ( !pssm.GetPssm().CanGetIntermediateData() || 
         !pssm.GetPssm().GetIntermediateData().CanGetFreqRatios() || 
         pssm.GetPssm().GetIntermediateData().GetFreqRatios().empty() ) {
        missing_freq_ratios = true;
    }

    if (missing_freq_ratios && missing_scores) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
                   "PSSM data must contain either scores or frequency ratios");
    }
    if (missing_scores && require_scores) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
               "PSSM data must contain scores (did you run the PSSM engine?)");
    }

    if ( !pssm.GetPssm().CanGetQuery() ) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
                   "Missing query sequence in PSSM");
    }
    if ( !pssm.GetPssm().GetQuery().IsSeq() ) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
                   "Query sequence in ASN.1 PSSM is not a single Bioseq");
    }

    if ( !pssm.GetPssm().GetIsProtein() ) {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "PSSM does not represent protein scoring matrix");
    }
}

void
CPsiBlastValidate::QueryFactory(CRef<IQueryFactory> query_factory, 
                                const CBlastOptionsHandle& opts_handle, 
                                EQueryFactoryType qf_type)
{
    CRef<ILocalQueryData> query_data =
        query_factory->MakeLocalQueryData(&opts_handle.GetOptions());

    // Compose the exception error message
    string excpt_msg("PSI-BLAST only accepts ");
    if (qf_type == eQFT_Query) {
        excpt_msg += "one protein sequence as query";
    } else if (qf_type == eQFT_Subject) {
        excpt_msg += "protein sequences as subjects";
    } else {
        abort();
    }

    if (qf_type == eQFT_Query) {
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

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
