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
* Authors:  Kamen Todorov, NCBI
*
* File Description:
*   Alignment converters
*
* ===========================================================================
*/


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_seg.hpp>

#include <objtools/alnmgr/pairwise_aln.hpp>


BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CDense_seg;
END_SCOPE(objects)


NCBI_XALNMGR_EXPORT
void
ConvertSeqAlignToPairwiseAln(CPairwiseAln& pairwise_aln,  ///< output
                             const objects::CSeq_align& sa,        ///< input Seq-align
                             objects::CSeq_align::TDim row_1,      ///< which pair of rows
                             objects::CSeq_align::TDim row_2);


NCBI_XALNMGR_EXPORT
void
ConvertDensegToPairwiseAln(CPairwiseAln& pairwise_aln,  ///< output
                           const objects::CDense_seg& ds,        ///< input Dense-seg
                           objects::CDense_seg::TDim row_1,      ///< which pair of rows
                           objects::CDense_seg::TDim row_2);


NCBI_XALNMGR_EXPORT
void
ConvertStdsegToPairwiseAln(CPairwiseAln& pairwise_aln,          ///< output
                           const objects::CSeq_align::TSegs::TStd& stds, ///< input Stds
                           objects::CSeq_align::TDim row_1,              ///< which pair of rows 
                           objects::CSeq_align::TDim row_2);


NCBI_XALNMGR_EXPORT
void
ConvertDendiagToPairwiseAln(CPairwiseAln& pairwise_aln,                  ///< output
                            const objects::CSeq_align::TSegs::TDendiag& dendiags, ///< input Dendiags
                            objects::CSeq_align::TDim row_1,                      ///< which pair of rows 
                            objects::CSeq_align::TDim row_2);

/// Create an anchored alignment from Seq-align using hints
template <class TAlnStats>
CRef<CAnchoredAln> 
CreateAnchoredAlnFromAln(const TAlnStats& aln_stats,
                         size_t aln_idx)
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
             row);
        anchored_aln->SetPairwiseAlns()[target_row].Reset(pairwise_aln);
    }
    anchored_aln->SetAnchorRow(target_anchor_row);
    return anchored_aln;
}


template <class TAlnStats>
void 
CreateAnchoredAlnVec(TAlnStats& aln_stats, ///< Input
                     vector<CRef<CAnchoredAln> >& out_vec) ///< Output
{
    _ASSERT(out_vec.empty());
    out_vec.resize(aln_stats.GetAlnCount());
    for (size_t aln_idx = 0;  
         aln_idx < aln_stats.GetAlnCount();
         ++aln_idx) {

        out_vec[aln_idx] = 
            CreateAnchoredAlnFromAln(aln_stats, aln_idx);

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


/*
* ===========================================================================
*
* $Log$
* Revision 1.18  2007/01/10 18:08:31  todorov
* Vector->Vec
*
* Revision 1.17  2007/01/09 19:53:31  yazhuk
* Replaced USING_SCOPE() with explicit declarations, included Dense_seg.hpp
*
* Revision 1.16  2007/01/05 18:32:23  todorov
* Added support for Dense_diag.
*
* Revision 1.15  2007/01/04 21:10:45  todorov
* + fAllowAbutting
*
* Revision 1.14  2006/12/13 18:57:37  todorov
* + NCBI_XALNMGR_EXPORT
*
* Revision 1.13  2006/12/13 18:45:17  todorov
* Moved definitions to .cpp
*
* Revision 1.12  2006/12/12 20:48:27  todorov
* Deduce the base widths in case of std-seg.
*
* Revision 1.11  2006/12/01 21:22:34  todorov
* - NCBI_XALNMGR_EXPORT
*
* Revision 1.10  2006/12/01 17:53:22  todorov
* + NCBI_XALNMGR_EXPORT
*
* Revision 1.9  2006/11/22 00:45:48  todorov
* Fixed the anchor collection flags.
*
* Revision 1.8  2006/11/20 18:39:48  todorov
* + CreateAnchoredAlnVector
*
* Revision 1.7  2006/11/17 05:36:13  todorov
* hints -> stats
*
* Revision 1.6  2006/11/16 18:07:24  todorov
* Anchor row is set to dim - 1.  This would allow to use CAnchoredAln
* for non-query-anchored alignments (via a fake anchor).
*
* Revision 1.5  2006/11/16 13:40:29  todorov
* Minor refactoring.
*
* Revision 1.4  2006/11/14 20:39:26  todorov
* Initial revision.
*
* ===========================================================================
*/

#endif  // OBJTOOLS_ALNMGR___ALN_CONVERTERS__HPP
