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
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objtools/alnmgr/pairwise_aln.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


void
ConvertDensegToPairwiseAln(CPairwiseAln& pairwise_aln,  //< output
                           const CDense_seg& ds,        //< input Dense-seg
                           CDense_seg::TDim row_1,      //< which pair of rows
                           CDense_seg::TDim row_2)
{
    _ASSERT(row_1 >=0  &&  row_1 < ds.GetDim());
    _ASSERT(row_2 >=0  &&  row_2 < ds.GetDim());

    const CDense_seg::TNumseg numseg = ds.GetNumseg();
    const CDense_seg::TDim dim = ds.GetDim();
    const CDense_seg::TStarts& starts = ds.GetStarts();
    const CDense_seg::TLens& lens = ds.GetLens();
    const CDense_seg::TStrands* strands = 
        ds.IsSetStrands() ? &ds.GetStrands() : NULL;

    CDense_seg::TNumseg seg;
    int pos_1, pos_2;
    for (seg = 0, pos_1 = row_1, pos_2 = row_2;
         seg < numseg;
         ++seg, pos_1 += dim, pos_2 += dim) {
        TSignedSeqPos from_1 = starts[pos_1];
        TSignedSeqPos from_2 = starts[pos_2];
        TSeqPos len = lens[seg];

        /// if not a gap, insert it to the collection
        if (from_1 >= 0  &&  from_2 >= 0)  {
            /// determinte the strands
            bool direct = true;
            if (strands) {
                bool minus_1 = (*strands)[pos_1] == eNa_strand_minus;
                bool minus_2 = (*strands)[pos_2] == eNa_strand_minus;
                direct = minus_1 == minus_2;
            }

            /// base-width adjustments
            const int& base_width_1 = pairwise_aln.GetFirstBaseWidth();
            const int& base_width_2 = pairwise_aln.GetSecondBaseWidth();
            if (base_width_1 > 1  ||  base_width_2 > 1) {
                if (base_width_1 > 1) {
                    from_1 *= base_width_1;
                }
                if (base_width_2 > 1) {
                    from_2 *= base_width_2;
                }
                if (base_width_1 == base_width_2) {
                    len *= base_width_1;
                }
            }

            /// insert the range
            pairwise_aln.insert(CPairwiseAln::TAlnRng(from_1, from_2, len, direct));
        }
    }
}


void
ConvertStdsegToPairwiseAln(CPairwiseAln& pairwise_aln,         //< output
                           const CSeq_align::TSegs::TStd& std, //< input Std
                           CSeq_align::TDim row_1,             //< which pair of rows 
                           CSeq_align::TDim row_2)
{
    ITERATE (CSeq_align::TSegs::TStd, 
             std_it,
             std) {

        const CStd_seg::TLoc& loc = (*std_it)->GetLoc();
        
        _ASSERT((CSeq_align::TDim) loc.size() > max(row_1, row_2));

        CSeq_loc::TRange rng_1 = loc[row_1]->GetTotalRange();
        CSeq_loc::TRange rng_2 = loc[row_2]->GetTotalRange();

        TSeqPos len_1 = rng_1.GetLength();
        TSeqPos len_2 = rng_2.GetLength();

        if (len_1 > 0  &&  len_2 > 0) {

            bool direct = 
                loc[row_1]->IsReverseStrand() == loc[row_2]->IsReverseStrand();

            const int& base_width_1 = pairwise_aln.GetFirstBaseWidth();
            const int& base_width_2 = pairwise_aln.GetSecondBaseWidth();
            _ASSERT(len_1 * base_width_1 == len_2 * base_width_2);

            pairwise_aln.insert
                (CPairwiseAln::TAlnRng(rng_1.GetFrom() * base_width_1,
                                       rng_2.GetFrom() * base_width_2,
                                       len_1 * base_width_1,
                                       direct));
        }
    }
}


void
ConvertSeqAlignToPairwiseAln(CPairwiseAln& pairwise_aln,  //< output
                             const CSeq_align& sa,        //< input Seq-align
                             CSeq_align::TDim row_1,      //< which pair of rows
                             CSeq_align::TDim row_2)
{
    _ASSERT(sa.GetDim() > max(row_1, row_2));

    typedef CSeq_align::TSegs TSegs;
    const TSegs& segs = sa.GetSegs();

    switch(segs.Which())    {
    case CSeq_align::TSegs::e_Dendiag:
        break;
    case CSeq_align::TSegs::e_Denseg: {
        ConvertDensegToPairwiseAln(pairwise_aln, segs.GetDenseg(),
                                   row_1, row_2);
        break;
    }
    case CSeq_align::TSegs::e_Std:
        ConvertStdsegToPairwiseAln(pairwise_aln, segs.GetStd(),
                                   row_1, row_2);
        break;
    case CSeq_align::TSegs::e_Packed:
        break;
    case CSeq_align::TSegs::e_Disc:
        ITERATE(CSeq_align_set::Tdata, sa_it, segs.GetDisc().Get()) {
            ConvertSeqAlignToPairwiseAln(pairwise_aln, **sa_it,
                                         row_1, row_2);
        }
        break;
    case CSeq_align::TSegs::e_Spliced:
        break;
    case CSeq_align::TSegs::e_Sparse:
        break;
    case CSeq_align::TSegs::e_not_set:
        break;
    }
}


/// Create an anchored alignment from Seq-align using hints
template <class TAlnHints>
CRef<CAnchoredAln> CreateAnchoredAlnFromAln(const TAlnHints& aln_hints,
                                            size_t aln_idx)
{
    _ASSERT(aln_hints.IsAnchored());

    typedef typename TAlnHints::TDim TDim;
    TDim dim = aln_hints.GetDimForAln(aln_idx);
    TDim anchor_row = aln_hints.GetAnchorRowForAln(aln_idx);
    TDim target_row;

    CRef<CAnchoredAln> anchored_aln(new CAnchoredAln);

    typedef typename TAlnHints::TSeqIdVector TSeqIdVector;
    TSeqIdVector ids = aln_hints.GetSeqIdsForAln(aln_idx);

    anchored_aln->SetSeqIds().resize(dim);
    anchored_aln->SetPairwiseAlns().resize(dim);

    int anchor_flags =
        CPairwiseAln::fKeepNormalized |
        CPairwiseAln::fAllowOverlap |
        CPairwiseAln::fAllowAbutting;

    int flags = 
        CPairwiseAln::fAllowMixedDir |
        CPairwiseAln::fAllowOverlap |
        CPairwiseAln::fAllowAbutting;

    for (TDim row = 0;  row < dim;  ++row) {
        /// Determine the where the row goes to (in the target
        /// anchored alignment)
        if (row < anchor_row) {
            target_row = row + 1;
        } else if (row == anchor_row) {
            target_row = 0;
        } else {
            target_row = row;
        }

        /// Create a pairwise
        CRef<CPairwiseAln> pairwise_aln
            (new CPairwiseAln(row == anchor_row ? anchor_flags : flags,
                              aln_hints.GetBaseWidthForAlnRow(aln_idx, anchor_row),
                              aln_hints.GetBaseWidthForAlnRow(aln_idx, row)));
        ConvertSeqAlignToPairwiseAln
            (*pairwise_aln,
             *aln_hints.GetAlnVector()[aln_idx],
             anchor_row,
             row);
        anchored_aln->SetPairwiseAlns()[target_row].Reset(pairwise_aln);

        /// Create an id;
        CConstRef<CSeq_id> id(ids[row]);
        anchored_aln->SetSeqIds()[target_row].Reset(id);
    }
    return anchored_aln;
}


END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.5  2006/11/16 13:40:29  todorov
* Minor refactoring.
*
* Revision 1.4  2006/11/14 20:39:26  todorov
* Initial revision.
*
* ===========================================================================
*/

#endif  // OBJTOOLS_ALNMGR___ALN_CONVERTERS__HPP
