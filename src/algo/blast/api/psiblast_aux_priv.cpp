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
/// Definitions of auxiliary functions/classes for PSI-BLAST

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
#include "../core/blast_psi_priv.h"

// Utility headers
#include <util/format_guess.hpp>
#include <util/math/matrix.hpp>

// Object includes
#include <objects/seqset/Seq_entry.hpp>
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
#include <sstream>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

void PsiBlastSetupScoreBlock(BlastScoreBlk* score_blk,
                             CConstRef<objects::CPssmWithParameters> pssm,
                             TSearchMessages& messages,
                             CConstRef<CBlastOptions> options)
{
    _ASSERT(score_blk);
    _ASSERT(pssm.NotEmpty());

    if ( !score_blk->protein_alphabet ) {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "BlastScoreBlk is not configured for a protein alphabet");
    }

    // Assign the ungapped Karlin-Altschul block
    if (pssm->GetPssm().GetLambdaUngapped() != CPssm::kInvalidStat) {
        score_blk->kbp_psi[0]->Lambda = pssm->GetPssm().GetLambdaUngapped();
    } else if (score_blk->kbp_std[0]->Lambda > 0.0) {
        score_blk->kbp_psi[0]->Lambda = score_blk->kbp_std[0]->Lambda;
    }

    if (pssm->GetPssm().GetKappaUngapped() != CPssm::kInvalidStat) {
        score_blk->kbp_psi[0]->K = pssm->GetPssm().GetKappaUngapped();
    } else if (score_blk->kbp_std[0]->K > 0.0) {
        score_blk->kbp_psi[0]->K = score_blk->kbp_std[0]->K;
    }
    score_blk->kbp_psi[0]->logK = log(score_blk->kbp_psi[0]->K);

    if (pssm->GetPssm().GetHUngapped() != CPssm::kInvalidStat) {
        score_blk->kbp_psi[0]->H = pssm->GetPssm().GetHUngapped();
    } else if (score_blk->kbp_std[0]->K > 0.0) {
        score_blk->kbp_psi[0]->H = score_blk->kbp_std[0]->H;
    }

    // Assign the gapped Karlin-Altschul block
    if (pssm->GetPssm().GetLambda() != CPssm::kInvalidStat) {
        score_blk->kbp_gap_psi[0]->Lambda = pssm->GetPssm().GetLambda();
    } else if (score_blk->kbp_gap_std[0]->Lambda > 0.0) {
        score_blk->kbp_gap_psi[0]->Lambda = score_blk->kbp_gap_std[0]->Lambda;
    }

    if (pssm->GetPssm().GetKappa() != CPssm::kInvalidStat) {
        score_blk->kbp_gap_psi[0]->K = pssm->GetPssm().GetKappa();
    } else if (score_blk->kbp_gap_std[0]->K > 0.0) {
        score_blk->kbp_gap_psi[0]->K = score_blk->kbp_gap_std[0]->K;
    }
    score_blk->kbp_gap_psi[0]->logK = log(score_blk->kbp_gap_psi[0]->K);

    if (pssm->GetPssm().GetH() != CPssm::kInvalidStat) {
        score_blk->kbp_gap_psi[0]->H = pssm->GetPssm().GetH();
    } else if (score_blk->kbp_gap_std[0]->H > 0.0) {
        score_blk->kbp_gap_psi[0]->H = score_blk->kbp_gap_std[0]->H;
    }

    // Assign the matrix scores/frequency ratios
    const size_t kQueryLength = pssm->GetPssm().GetNumColumns();
    score_blk->psi_matrix = SPsiBlastScoreMatrixNew(kQueryLength);

    // Get the scores
    bool missing_scores = false;
    try {
        auto_ptr< CNcbiMatrix<int> > scores
            (CScorematPssmConverter::GetScores(*pssm));
        _ASSERT(score_blk->psi_matrix->pssm->ncols == scores->GetCols());
        _ASSERT(score_blk->psi_matrix->pssm->nrows == scores->GetRows());

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
    // are all of the frequency ratios zeros? if so, issue a warning
    bool freq_ratios_all_zeros = true;

    try {
        auto_ptr< CNcbiMatrix<double> > freq_ratios
            (CScorematPssmConverter::GetFreqRatios(*pssm));
        _ASSERT(score_blk->psi_matrix->pssm->ncols == 
               freq_ratios->GetCols());
        _ASSERT(score_blk->psi_matrix->pssm->nrows == 
               freq_ratios->GetRows());

        for (TSeqPos c = 0; c < freq_ratios->GetCols(); c++) {
            for (TSeqPos r = 0; r < freq_ratios->GetRows(); r++) {
                score_blk->psi_matrix->freq_ratios[c][r] = 
                    (*freq_ratios)(r, c);
                if ((*freq_ratios)(r,c) > kEpsilon) {
                    freq_ratios_all_zeros = false;
                }
            }
        }
    } catch (const std::runtime_error&) {
        missing_freq_ratios = true;
    }

    if (missing_scores && missing_freq_ratios) {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "Missing scores and frequency ratios in PSSM");
    }

    _ASSERT(options->GetCompositionBasedStats() < eNumCompoAdjustModes);
    if ((options->GetCompositionBasedStats() != eNoCompositionBasedStats) &&
        freq_ratios_all_zeros) {
        ostringstream os;
        os << "Frequency ratios for PSSM are all zeros, frequency ratios for ";
        os << options->GetMatrixName() << " will be used during traceback ";
        os << "in composition based statistics";
        CRef<CSearchMessage> sm(new CSearchMessage(eBlastSevWarning, 0,
                                                   os.str()));
        _ASSERT(messages.size() == 1); // PSI-BLAST only works with one query
        messages.front().push_back(sm);
    }

    if (options->GetCompositionBasedStats() > eCompositionBasedStats) {
        // ugly, but necessary
        const_cast<CBlastOptions*>(&*options)
            ->SetCompositionBasedStats(eCompositionBasedStats);
        ostringstream os;
        os << "Composition-based score adjustment conditioned on "
           << "sequence properties and unconditional composition-based score "
           << "adjustment is not supported with PSSMs, resetting to default "
           << "value of standard composition-based statistics";
        CRef<CSearchMessage> sm(new CSearchMessage(eBlastSevWarning, 0,
                                                   os.str()));
        _ASSERT(messages.size() == 1); // PSI-BLAST only works with one query
        messages.front().push_back(sm);
    }
}

/// Convert a list of values into a CNcbiMatrix
/// @param source source of data [in]
/// @param dest destination of data [out]
/// @param by_row is the matrix data stored by row? [in]
/// @param num_rows number of rows [in]
/// @param num_cols number of columns [in]
template <class T>
void Convert2Matrix(const list<T>& source, CNcbiMatrix<T>& dest, 
                    bool by_row, SIZE_TYPE num_rows, SIZE_TYPE num_columns)
{
    typename list<T>::const_iterator itr = source.begin();
    if (by_row == true) {
        for (SIZE_TYPE r = 0; r < num_rows; r++) {
            for (SIZE_TYPE c = 0; c < num_columns; c++) {
                dest(r, c) = *itr++;
            }
        }
    } else {
        for (SIZE_TYPE c = 0; c < num_columns; c++) {
            for (SIZE_TYPE r = 0; r < num_rows; r++) {
                dest(r, c) = *itr++;
            }
        }
    }
    _ASSERT(itr == source.end());
}

CNcbiMatrix<int>*
CScorematPssmConverter::GetScores(const objects::CPssmWithParameters& pssm_asn)
{
    if ( !pssm_asn.GetPssm().CanGetFinalData() ||
         !pssm_asn.GetPssm().GetFinalData().CanGetScores() ||
         pssm_asn.GetPssm().GetFinalData().GetScores().empty() ) {
        throw runtime_error("Cannot obtain scores from ASN.1 PSSM");
    }

    const CPssm& pssm = pssm_asn.GetPssm();
    _ASSERT((size_t)pssm.GetFinalData().GetScores().size() ==
           (size_t)pssm.GetNumRows()*pssm_asn.GetPssm().GetNumColumns());

    auto_ptr< CNcbiMatrix<int> > retval
        (new CNcbiMatrix<int>(BLASTAA_SIZE,
                              pssm.GetNumColumns(), 
                              BLAST_SCORE_MIN));

    Convert2Matrix(pssm.GetFinalData().GetScores(),
                   *retval, pssm.GetByRow(), pssm.GetNumRows(),
                   pssm.GetNumColumns());
    return retval.release();
}

CNcbiMatrix<double>*
CScorematPssmConverter::GetFreqRatios(const objects::CPssmWithParameters& 
                                      pssm_asn)
{
    if ( !pssm_asn.GetPssm().CanGetIntermediateData() ||
         !pssm_asn.GetPssm().GetIntermediateData().CanGetFreqRatios() ||
         pssm_asn.GetPssm().GetIntermediateData().GetFreqRatios().empty() ) {
        throw runtime_error("Cannot obtain frequency ratios from ASN.1 PSSM");
    }

    const CPssm& pssm = pssm_asn.GetPssm();
    _ASSERT((size_t)pssm.GetIntermediateData().GetFreqRatios().size() ==
           (size_t)pssm.GetNumRows()*pssm_asn.GetPssm().GetNumColumns());

    auto_ptr< CNcbiMatrix<double> > retval
        (new CNcbiMatrix<double>(BLASTAA_SIZE, pssm.GetNumColumns(), 0.0));

    Convert2Matrix(pssm.GetIntermediateData().GetFreqRatios(),
                   *retval, pssm.GetByRow(), pssm.GetNumRows(),
                   pssm.GetNumColumns());
    return retval.release();
}

CNcbiMatrix<int>*
CScorematPssmConverter::GetResidueFrequencies
    (const objects::CPssmWithParameters& pssm_asn)
{
    if ( !pssm_asn.GetPssm().CanGetIntermediateData() ||
         !pssm_asn.GetPssm().GetIntermediateData().CanGetResFreqsPerPos() ||
         pssm_asn.GetPssm().GetIntermediateData().GetResFreqsPerPos().empty() )
    {
        return NULL;
    }

    const CPssm& pssm = pssm_asn.GetPssm();
    _ASSERT((size_t)pssm.GetIntermediateData().GetResFreqsPerPos().size() ==
           (size_t)pssm.GetNumRows()*pssm_asn.GetPssm().GetNumColumns());

    auto_ptr< CNcbiMatrix<int> > retval
        (new CNcbiMatrix<int>(BLASTAA_SIZE, pssm.GetNumColumns(), 0));

    Convert2Matrix(pssm.GetIntermediateData().GetResFreqsPerPos(),
                   *retval, pssm.GetByRow(), pssm.GetNumRows(),
                   pssm.GetNumColumns());
    return retval.release();
}

CNcbiMatrix<double>*
CScorematPssmConverter::GetWeightedResidueFrequencies
    (const objects::CPssmWithParameters& pssm_asn)
{
    if ( !pssm_asn.GetPssm().CanGetIntermediateData() ||
         !pssm_asn.GetPssm().GetIntermediateData().
            CanGetWeightedResFreqsPerPos() ||
         pssm_asn.GetPssm().GetIntermediateData().
            GetWeightedResFreqsPerPos().empty() ) {
        return NULL;
    }

    const CPssm& pssm = pssm_asn.GetPssm();
    _ASSERT((size_t)pssm.GetIntermediateData().
                GetWeightedResFreqsPerPos().size() ==
           (size_t)pssm.GetNumRows()*pssm_asn.GetPssm().GetNumColumns());

    auto_ptr< CNcbiMatrix<double> > retval
        (new CNcbiMatrix<double>(BLASTAA_SIZE, pssm.GetNumColumns(), 0.0));

    Convert2Matrix(pssm.GetIntermediateData().GetWeightedResFreqsPerPos(),
                   *retval, pssm.GetByRow(), pssm.GetNumRows(),
                   pssm.GetNumColumns());
    return retval.release();
}

void
CScorematPssmConverter::GetInformationContent
    (const objects::CPssmWithParameters& pssm_asn, 
     vector<double>& retval)
{
    retval.clear();
    if ( !pssm_asn.GetPssm().CanGetIntermediateData() ||
         !pssm_asn.GetPssm().GetIntermediateData().CanGetInformationContent() ||
         pssm_asn.GetPssm().
            GetIntermediateData().GetInformationContent().empty() ) {
        return;
    }
    const CPssm& pssm = pssm_asn.GetPssm();
    copy(pssm.GetIntermediateData().GetInformationContent().begin(),
         pssm.GetIntermediateData().GetInformationContent().end(),
         back_inserter(retval));
}

void
CScorematPssmConverter::GetGaplessColumnWeights
    (const objects::CPssmWithParameters& pssm_asn, 
     vector<double>& retval)
{
    retval.clear();
    if ( !pssm_asn.GetPssm().CanGetIntermediateData() ||
         !pssm_asn.GetPssm().
            GetIntermediateData().CanGetGaplessColumnWeights() ||
         pssm_asn.GetPssm().
            GetIntermediateData().GetGaplessColumnWeights().empty() ) {
        return;
    }
    const CPssm& pssm = pssm_asn.GetPssm();
    copy(pssm.GetIntermediateData().GetGaplessColumnWeights().begin(),
         pssm.GetIntermediateData().GetGaplessColumnWeights().end(),
         back_inserter(retval));
}

void
CScorematPssmConverter::GetSigma(const objects::CPssmWithParameters& pssm_asn, 
                                 vector<double>& retval)
{
    retval.clear();
    if ( !pssm_asn.GetPssm().CanGetIntermediateData() ||
         !pssm_asn.GetPssm().GetIntermediateData().CanGetSigma() ||
         pssm_asn.GetPssm().GetIntermediateData().GetSigma().empty() ) {
        return;
    }
    const CPssm& pssm = pssm_asn.GetPssm();
    copy(pssm.GetIntermediateData().GetSigma().begin(),
         pssm.GetIntermediateData().GetSigma().end(),
         back_inserter(retval));
}

void
CScorematPssmConverter::GetIntervalSizes
    (const objects::CPssmWithParameters& pssm_asn, vector<int>& retval)
{
    retval.clear();
    if ( !pssm_asn.GetPssm().CanGetIntermediateData() ||
         !pssm_asn.GetPssm().
            GetIntermediateData().CanGetIntervalSizes() ||
         pssm_asn.GetPssm().
            GetIntermediateData().GetIntervalSizes().empty() ) {
        return;
    }
    const CPssm& pssm = pssm_asn.GetPssm();
    copy(pssm.GetIntermediateData().GetIntervalSizes().begin(),
         pssm.GetIntermediateData().GetIntervalSizes().end(),
         back_inserter(retval));
}

void
CScorematPssmConverter::GetNumMatchingSeqs
    (const objects::CPssmWithParameters& pssm_asn, vector<int>& retval)
{
    retval.clear();
    if ( !pssm_asn.GetPssm().CanGetIntermediateData() ||
         !pssm_asn.GetPssm().
            GetIntermediateData().CanGetNumMatchingSeqs() ||
         pssm_asn.GetPssm().
            GetIntermediateData().GetNumMatchingSeqs().empty() ) {
        return;
    }
    const CPssm& pssm = pssm_asn.GetPssm();
    copy(pssm.GetIntermediateData().GetNumMatchingSeqs().begin(),
         pssm.GetIntermediateData().GetNumMatchingSeqs().end(),
         back_inserter(retval));
}

void
PsiBlastAddAncillaryPssmData(objects::CPssmWithParameters& pssm, 
                         int gap_open, 
                         int gap_extend)
{
    _ASSERT(pssm.GetParams().GetRpsdbparams().IsSetMatrixName());
    pssm.SetParams().SetRpsdbparams().SetGapOpen(gap_open);
    pssm.SetParams().SetRpsdbparams().SetGapExtend(gap_extend);
}

/** After creating the PSSM from frequency ratios, adjust the frequency ratios
 * matrix to match the dimensions of the score matrix 
 * @param pssm matrix to adjust [in|out]
 */
static void
s_AdjustFrequencyRatiosMatrixToMatchScoreMatrix(objects::CPssmWithParameters&
                                                pssm)
{
    _ASSERT(pssm.GetPssm().GetNumRows() < BLASTAA_SIZE);
    if (pssm.GetPssm().CanGetFinalData()) {
        _ASSERT(pssm.GetPssm().GetFinalData().GetScores().size() ==
                (size_t)BLASTAA_SIZE*pssm.GetPssm().GetNumColumns());
    }

    const size_t diff = (size_t)BLASTAA_SIZE - pssm.GetPssm().GetNumRows();
    CPssmIntermediateData::TFreqRatios& freq_ratios =
        pssm.SetPssm().SetIntermediateData().SetFreqRatios();

    if (pssm.GetPssm().GetByRow() == true) {
        freq_ratios.resize(pssm.GetPssm().GetNumColumns() * BLASTAA_SIZE, 0.0);
    } else {
        CPssmIntermediateData::TFreqRatios::iterator itr = freq_ratios.begin();
        for (int c = 0; c < pssm.GetPssm().GetNumColumns(); c++) {
            advance(itr, pssm.GetPssm().GetNumRows());
            freq_ratios.insert(itr, diff, 0.0);
        }
    }

    pssm.SetPssm().SetNumRows() = BLASTAA_SIZE;
}

void PsiBlastComputePssmScores(CRef<objects::CPssmWithParameters> pssm,
                               const CBlastOptions& opts)
{
    CConstRef<CBioseq> query(&pssm->GetQuery().GetSeq());
    CRef<IQueryFactory> seq_fetcher(new CObjMgrFree_QueryFactory(query));

    CRef<ILocalQueryData> query_data(seq_fetcher->MakeLocalQueryData(&opts));
    BLAST_SequenceBlk* seqblk = query_data->GetSequenceBlk();
    _ASSERT(query_data->GetSeqLength(0) == (size_t)seqblk->length);
    _ASSERT(query_data->GetSeqLength(0) == 
            (size_t)pssm->GetPssm().GetNumColumns());
    auto_ptr< CNcbiMatrix<double> > freq_ratios
        (CScorematPssmConverter::GetFreqRatios(*pssm));

    CPsiBlastInputFreqRatios pssm_engine_input(seqblk->sequence, 
                                               seqblk->length, 
                                               *freq_ratios, 
                                               opts.GetMatrixName());
    CPssmEngine pssm_engine(&pssm_engine_input);
    CRef<CPssmWithParameters> pssm_with_scores(pssm_engine.Run());

    if (pssm->GetPssm().GetNumRows() !=
        pssm_with_scores->GetPssm().GetNumRows()) {
        _ASSERT(pssm_with_scores->GetPssm().GetNumRows() == BLASTAA_SIZE);
        s_AdjustFrequencyRatiosMatrixToMatchScoreMatrix(*pssm);
    }
    pssm->SetPssm().SetFinalData().SetScores() =
        pssm_with_scores->GetPssm().GetFinalData().GetScores();
    pssm->SetPssm().SetFinalData().SetLambda() =
        pssm_with_scores->GetPssm().GetFinalData().GetLambda();
    pssm->SetPssm().SetFinalData().SetKappa() =
        pssm_with_scores->GetPssm().GetFinalData().GetKappa();
    pssm->SetPssm().SetFinalData().SetH() =
        pssm_with_scores->GetPssm().GetFinalData().GetH();

    PsiBlastAddAncillaryPssmData(*pssm,
                                  opts.GetGapOpeningCost(), 
                                  opts.GetGapExtensionCost());
}

/// Returns the evalue from this score object
/// @param score ASN.1 score object [in]
static double s_GetEvalue(const CScore& score)
{
    string score_type = score.GetId().GetStr();
    if (score.GetValue().IsReal() && 
       (score_type == "e_value" || score_type == "sum_e")) {
        return score.GetValue().GetReal();
    }
    return numeric_limits<double>::max();
}

/// Returns the bit_score from this score object
/// @param score ASN.1 score object [in]
static double s_GetBitScore(const CScore& score)
{
    string score_type = score.GetId().GetStr();
    if (score.GetValue().IsReal() && score_type == "bit_score") {
        return score.GetValue().GetReal();
    }
    return BLAST_EXPECT_VALUE;
}

double GetLowestEvalue(const objects::CDense_seg::TScores& scores,
                       double* bit_score /* = NULL */)
{
    double retval = BLAST_EXPECT_VALUE;
    double tmp;
    if (bit_score) {
        *bit_score = retval;
    }

    ITERATE(CDense_seg::TScores, i, scores) {
        if ( (tmp = s_GetEvalue(**i)) < retval) {
            retval = tmp;
        }
        if (bit_score && ((tmp = s_GetBitScore(**i)) > *bit_score)) {
            *bit_score = tmp;
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

    ITERATE(CSeq_align_set::Tdata, hsp, alignments.Get()) {
        // Look for HSP with score less than inclusion_ethresh
        double e = GetLowestEvalue((*hsp)->GetScore());
        if (e < evalue_inclusion_threshold) {
            CSeq_id_Handle sid =
                CSeq_id_Handle::GetHandle((*hsp)->GetSeq_id(1));
            output.insert(sid);
        }
    }
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

    // Only unscaled PSSMs are supported
    if (!missing_scores && 
        pssm.GetPssm().GetFinalData().CanGetScalingFactor() &&
        pssm.GetPssm().GetFinalData().GetScalingFactor() != 1) {
        string msg("PSSM has a scaling factor of ");
        msg += NStr::IntToString(pssm.GetPssm()
                                 .GetFinalData()
                                 .GetScalingFactor());
        msg += ". PSI-BLAST does not accept scaled PSSMs";
        NCBI_THROW(CBlastException, eInvalidArgument, msg);
    }

    if ( !pssm.HasQuery() ) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
                   "Missing query sequence in PSSM");
    }
    if ( !pssm.GetQuery().IsSeq() ) {
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
    _ASSERT(sblk);
    _ASSERT(sblk->length > 0);

    CFormatGuess::ESequenceType sequence_type =
        CFormatGuess::SequenceType((const char*)sblk->sequence_start,
                                   static_cast<unsigned>(sblk->length));
    if (sequence_type == CFormatGuess::eNucleotide) {
        excpt_msg.assign("PSI-BLAST cannot accept nucleotide ");
        excpt_msg += (qf_type == eQFT_Query ? "queries" : "subjects");
        NCBI_THROW(CBlastException, eInvalidArgument, excpt_msg);
    }
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
