#ifndef OBJTOOLS_ALNMGR___ALN_CONVERTERS__HPP
#define OBJTOOLS_ALNMGR___ALN_CONVERTERS__HPP
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
* Author:  Kamen Todorov, NCBI
*
* File Description:
*   Alignment converters
*
* ===========================================================================
*/


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seq/seq_loc_mapper_base.hpp>

#include <objtools/alnmgr/pairwise_aln.hpp>
#include <objtools/alnmgr/aln_user_options.hpp>


BEGIN_NCBI_SCOPE


NCBI_XALNMGR_EXPORT
void
ConvertSeqAlignToPairwiseAln(CPairwiseAln& pairwise_aln,      ///< output
                             const objects::CSeq_align& sa,   ///< input Seq-align
                             objects::CSeq_align::TDim row_1, ///< which pair of rows
                             objects::CSeq_align::TDim row_2,
                             CAlnUserOptions::EDirection direction = CAlnUserOptions::eBothDirections); ///< which direction


NCBI_XALNMGR_EXPORT
void
ConvertDensegToPairwiseAln(CPairwiseAln& pairwise_aln,       ///< output
                           const objects::CDense_seg& ds,    ///< input Dense-seg
                           objects::CSeq_align::TDim row_1,  ///< which pair of rows
                           objects::CSeq_align::TDim row_2,
                           CAlnUserOptions::EDirection direction = CAlnUserOptions::eBothDirections); ///< which direction


NCBI_XALNMGR_EXPORT
void
ConvertStdsegToPairwiseAln(CPairwiseAln& pairwise_aln,                   ///< output
                           const objects::CSeq_align::TSegs::TStd& stds, ///< input Stds
                           objects::CSeq_align::TDim row_1,              ///< which pair of rows 
                           objects::CSeq_align::TDim row_2,
                           CAlnUserOptions::EDirection direction = CAlnUserOptions::eBothDirections); ///< which direction


NCBI_XALNMGR_EXPORT
void
ConvertDendiagToPairwiseAln(CPairwiseAln& pairwise_aln,                           ///< output
                            const objects::CSeq_align::TSegs::TDendiag& dendiags, ///< input Dendiags
                            objects::CSeq_align::TDim row_1,                      ///< which pair of rows 
                            objects::CSeq_align::TDim row_2,
                            CAlnUserOptions::EDirection direction = CAlnUserOptions::eBothDirections); ///< which direction


NCBI_XALNMGR_EXPORT
void
ConvertSparseToPairwiseAln(CPairwiseAln& pairwise_aln,             ///< output
                           const objects::CSparse_seg& sparse_seg, ///< input Sparse-seg
                           objects::CSeq_align::TDim row_1,        ///< which pair of rows 
                           objects::CSeq_align::TDim row_2,
                           CAlnUserOptions::EDirection direction = CAlnUserOptions::eBothDirections); ///< which direction


NCBI_XALNMGR_EXPORT
void
ConvertSplicedToPairwiseAln(CPairwiseAln& pairwise_aln,               ///< output
                            const objects::CSpliced_seg& spliced_seg, ///< input Splice-seg
                            objects::CSeq_align::TDim row_1,          ///< which pair of rows 
                            objects::CSeq_align::TDim row_2,
                            CAlnUserOptions::EDirection direction = CAlnUserOptions::eBothDirections); ///< which direction


/// Build a pairwise alignment from a pair of seq-locs.
NCBI_XALNMGR_EXPORT
void
ConvertSeqLocsToPairwiseAln(CPairwiseAln&            aln,      ///< output
                            const objects::CSeq_loc& loc_1,    ///< first seq-loc
                            const objects::CSeq_loc& loc_2,    ///< second seq-loc
                            CAlnUserOptions::EDirection direction = CAlnUserOptions::eBothDirections); ///< which direction


typedef list< CRef<CPairwiseAln> > TPairwiseAlnList;

NCBI_XALNMGR_EXPORT
void SeqLocMapperToPairwiseAligns(const objects::CSeq_loc_Mapper_Base& mapper,
                                  TPairwiseAlnList&                    aligns);


/// Create an anchored alignment from Seq-align using hints
template <class TAlnStats>
CRef<CAnchoredAln> 
CreateAnchoredAlnFromAln(const TAlnStats& aln_stats,     ///< input
                         size_t aln_idx,                 ///< which input alignment
                         const CAlnUserOptions& options) ///< user options
{
    typedef typename TAlnStats::TDim TDim;
    TDim dim = aln_stats.GetDimForAln(aln_idx);
    TDim anchor_row = aln_stats.GetRowVecVec()[0][aln_idx];
    _ASSERT(anchor_row >= 0);
    TDim target_row;
    TDim target_anchor_row = dim - 1; ///< anchor row goes at the last row (TODO: maybe a candidate for a user option?)

    CRef<CAnchoredAln> anchored_aln(new CAnchoredAln);

    typedef typename TAlnStats::TIdVec TIdVec;
    TIdVec ids = aln_stats.GetSeqIdsForAln(aln_idx);

    anchored_aln->SetDim(dim);

    int anchor_flags =
        CPairwiseAln::fKeepNormalized |
        CPairwiseAln::fAllowAbutting;

    int flags = 
        CPairwiseAln::fAllowMixedDir |
        CPairwiseAln::fAllowOverlap |
        CPairwiseAln::fAllowAbutting;

    for (TDim row = 0;  row < dim;  ++row) {
        /// Determine where the row goes to (in the target anchored
        /// alignment)
        if (row < anchor_row) {
            target_row = row < target_anchor_row ? row : row + 1;
        } else if (row > anchor_row) {
            target_row = row > target_anchor_row ? row : row - 1;
        } else { // row == anchor_row
            target_row = target_anchor_row; 
        }

        /// Create a pairwise
        CRef<CPairwiseAln> pairwise_aln
            (new CPairwiseAln(ids[anchor_row],
                              ids[row],
                              row == anchor_row ? anchor_flags : flags));

        ConvertSeqAlignToPairwiseAln
            (*pairwise_aln,
             *aln_stats.GetAlnVec()[aln_idx],
             anchor_row,
             row,
             options.m_Direction);

        anchored_aln->SetPairwiseAlns()[target_row].Reset(pairwise_aln);
    }
    anchored_aln->SetAnchorRow(target_anchor_row);
    return anchored_aln;
}


template <class TAlnStats>
void 
CreateAnchoredAlnVec(TAlnStats& aln_stats,                 ///< input
                     vector<CRef<CAnchoredAln> >& out_vec, ///< output
                     const CAlnUserOptions& options)       ///< user options
{
    _ASSERT(out_vec.empty());
    out_vec.resize(aln_stats.GetAlnCount());
    for (size_t aln_idx = 0;  
         aln_idx < aln_stats.GetAlnCount();
         ++aln_idx) {

        out_vec[aln_idx] = 
            CreateAnchoredAlnFromAln(aln_stats, aln_idx, options);

        CAnchoredAln& anchored_aln = *out_vec[aln_idx];

        /// Calc scores
        for (typename TAlnStats::TDim row = 0; row < anchored_aln.GetDim(); ++row) {
            ITERATE(CPairwiseAln, rng_it, *anchored_aln.GetPairwiseAlns()[row]) {
                anchored_aln.SetScore() += rng_it->GetLength();
            }
        }
        anchored_aln.SetScore() /= anchored_aln.GetDim();
    }
}


END_NCBI_SCOPE

#endif  // OBJTOOLS_ALNMGR___ALN_CONVERTERS__HPP
