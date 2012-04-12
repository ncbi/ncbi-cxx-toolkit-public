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

#include <objtools/alnmgr/alnexception.hpp>
#include <objtools/alnmgr/pairwise_aln.hpp>
#include <objtools/alnmgr/aln_user_options.hpp>
#include <objtools/alnmgr/aln_stats.hpp>


BEGIN_NCBI_SCOPE


typedef vector<TAlnSeqIdIRef> TAlnSeqIdVec;

/// Build pairwise alignment from the selected rows of a seq-align.
/// @param pairwise_aln
///   Output pairwise alignment. Should be initialized with the correct ids
///   (the functions does not check if the ids in the pairwise alignment
///   correspond to the ids of the selected rows).
/// @param sa
///   Input seq-align object.
/// @param row_1
///   First row index.
/// @param row_2
///   Second row index.
/// @param direction
///   Flag indicating if the output pairwise alignment should include
///   direct, reverse, or any segments.
///   NOTE: segment direction in pariwise alignments is relative
///   (second vs first row).
/// @param ids
///   Optional vector of alignment seq-ids used only to check if the source alignment
///   contains mixed sequence types. All ids from the vector are compared, not just
///   the two selected rows.
NCBI_XALNMGR_EXPORT
void ConvertSeqAlignToPairwiseAln(
    CPairwiseAln&               pairwise_aln,
    const objects::CSeq_align&  sa,
    objects::CSeq_align::TDim   row_1,
    objects::CSeq_align::TDim   row_2,
    CAlnUserOptions::EDirection direction = CAlnUserOptions::eBothDirections,
    const TAlnSeqIdVec*         ids = 0);


/// Build pairwise alignment from the selected rows of a dense-seg.
/// @sa ConvertSeqAlignToPairwiseAln
NCBI_XALNMGR_EXPORT
void ConvertDensegToPairwiseAln(
    CPairwiseAln&               pairwise_aln,
    const objects::CDense_seg&  ds,
    objects::CSeq_align::TDim   row_1,
    objects::CSeq_align::TDim   row_2,
    CAlnUserOptions::EDirection direction = CAlnUserOptions::eBothDirections,
    const TAlnSeqIdVec*         ids = 0);


/// Build pairwise alignment from the selected rows of a packed-seg.
/// @sa ConvertSeqAlignToPairwiseAln
NCBI_XALNMGR_EXPORT
void ConvertPackedsegToPairwiseAln(
    CPairwiseAln&               pairwise_aln,
    const objects::CPacked_seg& ps,
    objects::CSeq_align::TDim   row_1,
    objects::CSeq_align::TDim   row_2,
    CAlnUserOptions::EDirection direction = CAlnUserOptions::eBothDirections,
    const TAlnSeqIdVec*         ids = 0);


/// Build pairwise alignment from the selected rows of an std-seg.
/// @sa ConvertSeqAlignToPairwiseAln
NCBI_XALNMGR_EXPORT
void ConvertStdsegToPairwiseAln(
    CPairwiseAln&                           pairwise_aln,
    const objects::CSeq_align::TSegs::TStd& stds,
    objects::CSeq_align::TDim               row_1,
    objects::CSeq_align::TDim               row_2,
    CAlnUserOptions::EDirection             direction
        = CAlnUserOptions::eBothDirections,
    const TAlnSeqIdVec*                     ids = 0);


/// Build pairwise alignment from the selected rows of a dendiag.
/// @sa ConvertSeqAlignToPairwiseAln
NCBI_XALNMGR_EXPORT
void ConvertDendiagToPairwiseAln(
    CPairwiseAln&                               pairwise_aln,
    const objects::CSeq_align::TSegs::TDendiag& dendiags,
    objects::CSeq_align::TDim                   row_1,
    objects::CSeq_align::TDim                   row_2,
    CAlnUserOptions::EDirection                 direction
        = CAlnUserOptions::eBothDirections,
    const TAlnSeqIdVec*                         ids = 0);


/// Build pairwise alignment from the selected rows of a sparse-seg.
/// @sa ConvertSeqAlignToPairwiseAln
NCBI_XALNMGR_EXPORT
void ConvertSparseToPairwiseAln(
    CPairwiseAln&               pairwise_aln,
    const objects::CSparse_seg& sparse_seg,
    objects::CSeq_align::TDim   row_1,
    objects::CSeq_align::TDim   row_2,
    CAlnUserOptions::EDirection direction = CAlnUserOptions::eBothDirections,
    const TAlnSeqIdVec*         ids = 0);


/// Build pairwise alignment from the selected rows of a spliced-seg.
/// @sa ConvertSeqAlignToPairwiseAln
NCBI_XALNMGR_EXPORT
void ConvertSplicedToPairwiseAln(
    CPairwiseAln&                 pairwise_aln,
    const objects::CSpliced_seg& spliced_seg,
    objects::CSeq_align::TDim    row_1,
    objects::CSeq_align::TDim    row_2,
    CAlnUserOptions::EDirection  direction = CAlnUserOptions::eBothDirections,
    const TAlnSeqIdVec*          ids = 0);


/// Build pairwise alignment from a pair of seq-locs. Each seq-loc must
/// reference a single sequence.
/// @param aln
///   Output pairwise alignment. Should be initialized with the correct ids
///   (the functions does not check if the ids in the pairwise alignment
///   correspond to the ids of the seq-locs).
/// @param loc_1
///   First seq-loc.
/// @param loc_2
///   Second seq-loc.
/// @param direction
///   Flag indicating if the output pairwise alignment should include
///   direct, reverse, or any segments.
///   NOTE: segment direction in pariwise alignments is relative
///   (second vs first row).
NCBI_XALNMGR_EXPORT
void ConvertSeqLocsToPairwiseAln(
    CPairwiseAln&               aln,
    const objects::CSeq_loc&    loc_1,
    const objects::CSeq_loc&    loc_2,
    CAlnUserOptions::EDirection direction = CAlnUserOptions::eBothDirections);


typedef list< CRef<CPairwiseAln> > TPairwiseAlnList;

/// Build a list of pairwise alignments from a seq-loc mapper's mappings.
NCBI_XALNMGR_EXPORT
void SeqLocMapperToPairwiseAligns(const objects::CSeq_loc_Mapper_Base& mapper,
                                  TPairwiseAlnList&                    aligns);


/// Create an anchored alignment from Seq-align using hints.
/// Optionally, choose the anchor row explicitly (this overrides
/// options.GetAnchorId()).
/// NOTE: Potentially, this "shrinks" the alignment vertically in case some
/// row was not aligned to the anchor.
/// @param aln_stats
///   Input alignment stats (see CAlnStats template).
/// @param aln_idx
///   Index of the input alignment in the stats.
/// @param options
///   Options for building the anchored alignment.
/// @param explicit_anchor_row
///   Explicit anchor row index (this overrides anchor id set in the options).
///   By default the anchor row is selected automatically.
/// @sa CAlnStats
template<class _TAlnStats>
CRef<CAnchoredAln> CreateAnchoredAlnFromAln(
    const _TAlnStats&         aln_stats,
    size_t                    aln_idx,
    const CAlnUserOptions&    options,
    objects::CSeq_align::TDim explicit_anchor_row = -1)
{
    typedef typename _TAlnStats::TDim TDim;
    TDim dim = aln_stats.GetDimForAln(aln_idx);

    // What anchor?
    TDim anchor_row;
    if (explicit_anchor_row >= 0) {
        if (explicit_anchor_row >= dim) {
            NCBI_THROW(CAlnException, eInvalidRequest,
                "Invalid explicit_anchor_row");
        }
        anchor_row = explicit_anchor_row;
    }
    else {
        size_t anchor_id_idx = 0; // Prevent warning
        if ( aln_stats.CanBeAnchored() ) {
            if ( options.GetAnchorId() ) {
                // if anchor was chosen by the user
                typedef typename _TAlnStats::TIdMap TIdMap;
                typename TIdMap::const_iterator it =
                    aln_stats.GetAnchorIdMap().find(options.GetAnchorId());
                if (it == aln_stats.GetAnchorIdMap().end()) {
                    NCBI_THROW(CAlnException, eInvalidRequest,
                        "Invalid options.GetAnchorId()");
                }
                anchor_id_idx = it->second[0];
            }
            else {
                // if not explicitly chosen, just choose the first potential
                // anchor that is preferably not aligned to itself
                for (size_t i = 0; i < aln_stats.GetAnchorIdVec().size(); ++i) {
                    const TAlnSeqIdIRef& anchor_id = aln_stats.GetAnchorIdVec()[i];
                    if (aln_stats.GetAnchorIdMap().find(anchor_id)->second.size() > 1) {
                        // this potential anchor is aligned to itself, not
                        // the best choice
                        if (i == 0) {
                            // but still, keep the first one in case all
                            // are bad
                            anchor_id_idx = aln_stats.GetAnchorIdxVec()[i];
                        }
                    }
                    else {
                        // perfect: the first anchor that is not aligned
                        // to itself
                        anchor_id_idx = aln_stats.GetAnchorIdxVec()[i];
                        break;
                    }
                }
            }
        }
        else {
            NCBI_THROW(CAlnException, eInvalidRequest,
                "Alignments cannot be anchored.");
        }
        anchor_row = aln_stats.GetRowVecVec()[anchor_id_idx][aln_idx];
    }
    _ALNMGR_ASSERT(anchor_row >= 0  &&  anchor_row < dim);

    // If there are different sequence types involved, force genomic coordinates.
    // No need to explicitly check this if the anchor is a nucleotide.
    bool force_widths = false;
    if ( aln_stats.GetIdVec()[anchor_row]->IsProtein() ) {
        for (size_t i = 0; i < aln_stats.GetIdVec().size(); ++i) {
            if ( aln_stats.GetIdVec()[i]->IsNucleotide() ) {
                force_widths = true;
                break;
            }
        }
    }

    // Flags
    int anchor_flags = CPairwiseAln::fKeepNormalized;
    int flags = CPairwiseAln::fKeepNormalized | CPairwiseAln::fAllowMixedDir;

    if ((options.m_MergeFlags & CAlnUserOptions::fIgnoreInsertions) != 0) {
        anchor_flags |= CPairwiseAln::fIgnoreInsertions;
        flags |= CPairwiseAln::fIgnoreInsertions;
    }

    // Create pairwises
    typedef typename _TAlnStats::TIdVec TIdVec;
    TIdVec ids = aln_stats.GetSeqIdsForAln(aln_idx);
    CAnchoredAln::TPairwiseAlnVector pairwises;
    pairwises.resize(dim);
    int empty_rows = 0;
    for (TDim row = 0;  row < dim;  ++row) {
        CRef<CPairwiseAln> pairwise_aln(new CPairwiseAln(ids[anchor_row],
            ids[row],
            row == anchor_row ? anchor_flags : flags));

        ConvertSeqAlignToPairwiseAln(
            *pairwise_aln,
            *aln_stats.GetAlnVec()[aln_idx],
            anchor_row,
            row,
            row == anchor_row ? CAlnUserOptions::eDirect : options.m_Direction,
            &aln_stats.GetIdVec());

        if ( force_widths ) {
            // Need to convert coordinates to genomic.
            pairwise_aln->ForceGenomicCoords();
        }

        if ( pairwise_aln->empty() ) {
            ++empty_rows;
        }

        pairwises[row].Reset(pairwise_aln);
    }
    _ALNMGR_ASSERT(empty_rows >= 0  &&  empty_rows < dim);
    if (empty_rows == dim - 1) {
        _ALNMGR_ASSERT(options.m_Direction != CAlnUserOptions::eBothDirections);
        return CRef<CAnchoredAln>();
        // Alternatively, perhaps we can continue processing here
        // which would result in a CAnchoredAln that only contains
        // the anchor.
    }

    // Create the anchored aln (which may shrink vertically due to resulting empty rows)
    TDim new_dim = dim - empty_rows;
    _ALNMGR_ASSERT(new_dim > 0);

    // Anchor row goes at the last row (TODO: maybe a candidate for a user option?)
    TDim target_anchor_row =
        (options.m_MergeFlags & CAlnUserOptions::fAnchorRowFirst) != 0 ?
        0 : new_dim - 1;

    CRef<CAnchoredAln> anchored_aln(new CAnchoredAln);
    anchored_aln->SetDim(new_dim);

    for (TDim row = 0, target_row = 0;  row < dim;  ++row) {
        if ( !pairwises[row]->empty() ) {
            if (target_row == target_anchor_row) {
                target_row++;
            }
            anchored_aln->SetPairwiseAlns()[row == anchor_row ?
                                            target_anchor_row :
                                            target_row++].Reset(pairwises[row]);
        }
    }
    anchored_aln->SetAnchorRow(target_anchor_row);
    return anchored_aln;
}


/// Create anchored alignment from each seq-align in the stats.
/// @sa CreateAnchoredAlnFromAln
template<class _TAlnStats>
void CreateAnchoredAlnVec(_TAlnStats&            aln_stats,
                          TAnchoredAlnVec&       out_vec,
                          const CAlnUserOptions& options)
{
    _ASSERT(out_vec.empty());
    out_vec.reserve(aln_stats.GetAlnCount());
    for (size_t aln_idx = 0; aln_idx < aln_stats.GetAlnCount(); ++aln_idx) {
        CRef<CAnchoredAln> anchored_aln =
            CreateAnchoredAlnFromAln(aln_stats, aln_idx, options);

        if ( anchored_aln ) {
            out_vec.push_back(anchored_aln);
            // Calc scores
            for (typename _TAlnStats::TDim row = 0; row < anchored_aln->GetDim(); ++row) {
                ITERATE(CPairwiseAln, rng_it, *anchored_aln->GetPairwiseAlns()[row]) {
                    anchored_aln->SetScore() += rng_it->GetLength();
                }
            }
            anchored_aln->SetScore() /= anchored_aln->GetDim();
        }
    }
}


/// A simple API that assumes that the seq_align has exactly two rows
/// and you want to create a pairwise with the default policy.
/// @sa ConvertSeqAlignToPairwiseAln
NCBI_XALNMGR_EXPORT
CRef<CPairwiseAln>
CreatePairwiseAlnFromSeqAlign(const objects::CSeq_align& seq_align);


END_NCBI_SCOPE

#endif  // OBJTOOLS_ALNMGR___ALN_CONVERTERS__HPP
