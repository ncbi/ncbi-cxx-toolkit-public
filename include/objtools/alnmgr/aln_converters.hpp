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
/// Optionally, choose the anchor row explicitly (this overrides options.GetAnchorId())
/// NB: Potentially, this "shrinks" the alignment vertically in case some row was not aligned to the anchor.
//NCBI_XALNMGR_EXPORT
template<class _TAlnStats>
CRef<CAnchoredAln>
CreateAnchoredAlnFromAln(const _TAlnStats& aln_stats,      ///< input
                         size_t aln_idx,                  ///< which input alignment
                         const CAlnUserOptions& options,  ///< user options
                         objects::CSeq_align::TDim explicit_anchor_row = -1) ///< optional anchor row
{
    typedef typename _TAlnStats::TDim TDim;
    TDim dim = aln_stats.GetDimForAln(aln_idx);

    /// What anchor?
    TDim anchor_row;
    if (explicit_anchor_row >= 0) {
        if (explicit_anchor_row >= dim) {
            NCBI_THROW(CAlnException, eInvalidRequest,
                       "Invalid explicit_anchor_row");
        }
        anchor_row = explicit_anchor_row;
    } else {
        size_t anchor_id_idx = 0; // Prevent warning
        if (aln_stats.CanBeAnchored()) {
            if (options.GetAnchorId()) {
                // if anchor was chosen by the user
                typedef typename _TAlnStats::TIdMap TIdMap;
                typename TIdMap::const_iterator it = aln_stats.GetAnchorIdMap().find(options.GetAnchorId());
                if (it == aln_stats.GetAnchorIdMap().end()) {
                    NCBI_THROW(CAlnException, eInvalidRequest,
                               "Invalid options.GetAnchorId()");
                }
                anchor_id_idx = it->second[0];
            } else {
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
                    } else {
                        // perfect: the first anchor that is not aligned
                        // to itself
                        anchor_id_idx = aln_stats.GetAnchorIdxVec()[i];
                        break;
                    }
                }
            }
        } else {
            NCBI_THROW(CAlnException, eInvalidRequest,
                       "Alignments cannot be anchored.");
        }
        anchor_row = aln_stats.GetRowVecVec()[anchor_id_idx][aln_idx];
    }
    _ALNMGR_ASSERT(anchor_row >= 0  &&  anchor_row < dim);

    /// Flags
    int anchor_flags =
        CPairwiseAln::fKeepNormalized;

    int flags = 
        CPairwiseAln::fKeepNormalized | 
        CPairwiseAln::fAllowMixedDir;


    /// Create pairwises
    typedef typename _TAlnStats::TIdVec TIdVec;
    TIdVec ids = aln_stats.GetSeqIdsForAln(aln_idx);
    CAnchoredAln::TPairwiseAlnVector pairwises;
    pairwises.resize(dim);
    int empty_rows = 0;
    for (TDim row = 0;  row < dim;  ++row) {

        CRef<CPairwiseAln> pairwise_aln
            (new CPairwiseAln(ids[anchor_row],
                              ids[row],
                              row == anchor_row ? anchor_flags : flags));

        ConvertSeqAlignToPairwiseAln
            (*pairwise_aln,
             *aln_stats.GetAlnVec()[aln_idx],
             anchor_row,
             row,
             row == anchor_row ? CAlnUserOptions::eDirect : options.m_Direction);

        if (pairwise_aln->empty()) {
            ++empty_rows;
        }

        pairwises[row].Reset(pairwise_aln);
    }
    _ALNMGR_ASSERT(empty_rows >= 0  &&  empty_rows < dim);
    if (empty_rows == dim - 1) {
        _ALNMGR_ASSERT(options.m_Direction != CAlnUserOptions::eBothDirections);
        return CRef<CAnchoredAln>();
        /// Alternatively, perhaps we can continue processing here
        /// which would result in a CAnchoredAln that only contains
        /// the anchor.
    }
        
    /// Create the anchored aln (which may shrink vertically due to resulting empty rows)
    TDim new_dim = dim - empty_rows;
    _ALNMGR_ASSERT(new_dim > 0);

    TDim target_anchor_row = new_dim - 1; ///< anchor row goes at the last row (TODO: maybe a candidate for a user option?)

    CRef<CAnchoredAln> anchored_aln(new CAnchoredAln);
    anchored_aln->SetDim(new_dim);

    for (TDim row = 0, target_row = 0;  row < dim;  ++row) {
        if ( !pairwises[row]->empty() ) {
            anchored_aln->SetPairwiseAlns()[row == anchor_row ?
                                            target_anchor_row :
                                            target_row++].Reset(pairwises[row]);
        }
    }
    anchored_aln->SetAnchorRow(target_anchor_row);
    return anchored_aln;
}


//NCBI_XALNMGR_EXPORT
template<class _TAlnStats>
void
CreateAnchoredAlnVec(_TAlnStats& aln_stats,           ///< input
                     TAnchoredAlnVec& out_vec,        ///< output
                     const CAlnUserOptions& options) ///< user options
{
    _ASSERT(out_vec.empty());
    out_vec.reserve(aln_stats.GetAlnCount());
    for (size_t aln_idx = 0;  
         aln_idx < aln_stats.GetAlnCount();
         ++aln_idx) {

        CRef<CAnchoredAln> anchored_aln = 
            CreateAnchoredAlnFromAln(aln_stats, aln_idx, options);

        if (anchored_aln) {
            out_vec.push_back(anchored_aln);
            
            /// Calc scores
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
/// and you want to create a pairwise with the default policy
NCBI_XALNMGR_EXPORT
CRef<CPairwiseAln>
CreatePairwiseAlnFromSeqAlign(const objects::CSeq_align& seq_align);
                                 

END_NCBI_SCOPE

#endif  // OBJTOOLS_ALNMGR___ALN_CONVERTERS__HPP
