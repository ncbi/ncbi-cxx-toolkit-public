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
* Authors:  Kamen Todorov, Andrey Yazhuk, NCBI
*
* File Description:
*   Seq-align converters
*
* ===========================================================================
*/


#include <ncbi_pch.hpp>

#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Sparse_seg.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Prot_pos.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objtools/alnmgr/aln_converters.hpp>
#include <objtools/alnmgr/aln_rng_coll_oper.hpp>

#include <objtools/error_codes.hpp>

#define NCBI_USE_ERRCODE_X   Objtools_Aln_Conv

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


void
ConvertSeqAlignToPairwiseAln(CPairwiseAln& pairwise_aln,
                             const CSeq_align& sa,
                             CSeq_align::TDim row_1,
                             CSeq_align::TDim row_2,
                             CAlnUserOptions::EDirection direction,
                             const TAlnSeqIdVec* ids)
{
    _ALNMGR_ASSERT(row_1 >=0  &&  row_2 >= 0);
    _ALNMGR_ASSERT(sa.CheckNumRows() > max(row_1, row_2));

    typedef CSeq_align::TSegs TSegs;
    const TSegs& segs = sa.GetSegs();

    switch(segs.Which())    {
    case CSeq_align::TSegs::e_Dendiag:
        ConvertDendiagToPairwiseAln(pairwise_aln, segs.GetDendiag(),
                                    row_1, row_2, direction, ids);
        break;
    case CSeq_align::TSegs::e_Denseg: {
        ConvertDensegToPairwiseAln(pairwise_aln, segs.GetDenseg(),
                                   row_1, row_2, direction, ids);
        break;
    }
    case CSeq_align::TSegs::e_Std:
        ConvertStdsegToPairwiseAln(pairwise_aln, segs.GetStd(),
                                   row_1, row_2, direction, ids);
        break;
    case CSeq_align::TSegs::e_Packed:
        ConvertPackedsegToPairwiseAln(pairwise_aln, segs.GetPacked(),
                                      row_1, row_2, direction, ids);
        break;
    case CSeq_align::TSegs::e_Disc:
        ITERATE(CSeq_align_set::Tdata, sa_it, segs.GetDisc().Get()) {
            ConvertSeqAlignToPairwiseAln(pairwise_aln, **sa_it,
                                         row_1, row_2, direction, ids);
        }
        break;
    case CSeq_align::TSegs::e_Spliced:
        ConvertSplicedToPairwiseAln(pairwise_aln, segs.GetSpliced(),
                                    row_1, row_2, direction, ids);
        break;
    case CSeq_align::TSegs::e_Sparse:
        ConvertSparseToPairwiseAln(pairwise_aln, segs.GetSparse(),
                                   row_1, row_2, direction, ids);
        break;
    case CSeq_align::TSegs::e_not_set:
        NCBI_THROW(CAlnException, eInvalidRequest,
                   "Invalid CSeq_align::TSegs type.");
        break;
    }
}


// Check if the set of alignment seq-ids contains mixed sequence types.
static bool IsMixedAlignment(const TAlnSeqIdVec* ids)
{
    if ( !ids ) return false;
    bool have_nuc = false;
    bool have_prot = false;
    ITERATE(TAlnSeqIdVec, id, *ids) {
        switch ((*id)->GetBaseWidth()) {
        case 1:
            have_nuc = true;
            break;
        case 3:
            have_prot = true;
            break;
        }
        if (have_nuc  &&  have_prot) return true;
    }
    return false;
}


void
ConvertDensegToPairwiseAln(CPairwiseAln& pairwise_aln,
                           const CDense_seg& ds,
                           CSeq_align::TDim row_1,
                           CSeq_align::TDim row_2,
                           CAlnUserOptions::EDirection direction,
                           const TAlnSeqIdVec* ids)
{
    _ALNMGR_ASSERT(row_1 >=0  &&  row_1 < ds.GetDim());
    _ALNMGR_ASSERT(row_2 >=0  &&  row_2 < ds.GetDim());

    const CDense_seg::TNumseg& numseg = ds.GetNumseg();
    const CDense_seg::TDim& dim = ds.GetDim();
    const CDense_seg::TStarts& starts = ds.GetStarts();
    const CDense_seg::TLens& lens = ds.GetLens();
    const CDense_seg::TStrands* strands = 
        ds.IsSetStrands() ? &ds.GetStrands() : NULL;

    bool mixed = IsMixedAlignment(ids);

    CDense_seg::TNumseg seg;
    int pos_1, pos_2;
    TSignedSeqPos last_to_1 = 0;

    for (seg = 0, pos_1 = row_1, pos_2 = row_2;
         seg < numseg;
         ++seg, pos_1 += dim, pos_2 += dim) {
        TSignedSeqPos from_1 = starts[pos_1];
        TSignedSeqPos from_2 = starts[pos_2];
        TSeqPos len = lens[seg];

        // determinte the direction
        bool direct = true;
        bool first_direct = true;
        if (strands) {
            first_direct = !IsReverse((*strands)[pos_1]);
            bool minus_2 = IsReverse((*strands)[pos_2]);
            direct = first_direct != minus_2;
        }

        if (direction == CAlnUserOptions::eBothDirections  ||
            (direct ?
             direction == CAlnUserOptions::eDirect :
             direction == CAlnUserOptions::eReverse)) {

            // base-width adjustments
            const int& base_width_1 = pairwise_aln.GetFirstBaseWidth();
            const int& base_width_2 = pairwise_aln.GetSecondBaseWidth();
            if (mixed  ||  base_width_1 > 1) {
                // Tell the pairwise alignment we are already using genomic
                // coordinates.
                pairwise_aln.SetUsingGenomic();

                if (base_width_1 > 1) {
                    from_1 *= base_width_1;
                }
                if (base_width_2 > 1) {
                    from_2 *= base_width_2;
                }
                len *= 3;
            }

            // if not a gap, insert it to the collection
            if (from_1 >= 0  &&  from_2 >= 0)  {
                // insert the range
                pairwise_aln.insert(CPairwiseAln::TAlnRng(
                    from_1, from_2, len, direct, first_direct));
                last_to_1 = first_direct ? from_1 + len : from_1;
            }
            else if (from_1 < 0  &&  from_2 >= 0) {
                // Store gaps
                TSignedSeqPos ins_pos = last_to_1;
                if (!first_direct  &&  ins_pos == 0) {
                    // Special case: first is on minus strand and starts with a gap.
                    // Instead of using '0' for insertion position, place it after
                    // the first non-gap segment.
                    CDense_seg::TNumseg skip_seg = 1;
                    while (seg + skip_seg < numseg) {
                        int p = starts[pos_1 + skip_seg*dim];
                        if (p >= 0) {
                            ins_pos = p + lens[seg + skip_seg];
                            break;
                        }
                        skip_seg++;
                    }
                }
                pairwise_aln.AddInsertion(CPairwiseAln::TAlnRng(
                    ins_pos, from_2, len, direct, first_direct));
            }
            else if (from_1 >= 0) {
                // Adjust next possible gap start
                last_to_1 = first_direct ? from_1 + len : from_1;
            }
        }
    }
}


void
ConvertPackedsegToPairwiseAln(CPairwiseAln& pairwise_aln,
                              const CPacked_seg& ps,
                              CSeq_align::TDim row_1,
                              CSeq_align::TDim row_2,
                              CAlnUserOptions::EDirection direction,
                              const TAlnSeqIdVec* ids)
{
    _ALNMGR_ASSERT(row_1 >=0  &&  row_1 < ps.GetDim());
    _ALNMGR_ASSERT(row_2 >=0  &&  row_2 < ps.GetDim());

    const CPacked_seg::TNumseg& numseg = ps.GetNumseg();
    const CPacked_seg::TDim& dim = ps.GetDim();
    const CPacked_seg::TStarts& starts = ps.GetStarts();
    const CPacked_seg::TPresent presents = ps.GetPresent();
    const CPacked_seg::TLens& lens = ps.GetLens();
    const CPacked_seg::TStrands* strands = 
        ps.IsSetStrands() ? &ps.GetStrands() : NULL;

    bool mixed = IsMixedAlignment(ids);

    CPacked_seg::TNumseg seg;
    int pos_1, pos_2;
    TSeqPos last_to_1 = 0;
    for (seg = 0, pos_1 = row_1, pos_2 = row_2;
         seg < numseg;
         ++seg, pos_1 += dim, pos_2 += dim) {
        TSeqPos from_1 = starts[pos_1];
        TSeqPos from_2 = starts[pos_2];
        bool present_1 = presents[pos_1];
        bool present_2 = presents[pos_2];
        TSeqPos len = lens[seg];

        // determinte the direction
        bool direct = true;
        bool first_direct = true;
        if (strands) {
            first_direct = !IsReverse((*strands)[pos_1]);
            bool minus_2 = IsReverse((*strands)[pos_2]);
            direct = first_direct != minus_2;
        }

        if (direction == CAlnUserOptions::eBothDirections  ||
            (direct ?
             direction == CAlnUserOptions::eDirect :
             direction == CAlnUserOptions::eReverse)) {

            // base-width adjustments
            const int& base_width_1 = pairwise_aln.GetFirstBaseWidth();
            const int& base_width_2 = pairwise_aln.GetSecondBaseWidth();
            if (mixed  ||  base_width_1 > 1) {
                // Tell the pairwise alignment we are already using genomic
                // coordinates.
                pairwise_aln.SetUsingGenomic();

                if (base_width_1 > 1) {
                    from_1 *= base_width_1;
                }
                if (base_width_2 > 1) {
                    from_2 *= base_width_2;
                }
                len *= 3;
            }
                
            // if not a gap, insert it to the collection
            if (present_1  &&  present_2)  {
                // insert the range
                pairwise_aln.insert(CPairwiseAln::TAlnRng(
                    from_1, from_2, len, direct, first_direct));
                last_to_1 = first_direct ? from_1 + len : from_1;
            }
            else if (!present_1  &&  present_2) {
                // Store gaps
                pairwise_aln.AddInsertion(CPairwiseAln::TAlnRng(
                    last_to_1, from_2, len, direct, first_direct));
            }
            else if (present_1) {
                // Adjust next possible gap start
                last_to_1 = first_direct ? from_1 + len : from_1;
            }
        }
    }
}


void
ConvertStdsegToPairwiseAln(CPairwiseAln& pairwise_aln,
                           const CSeq_align::TSegs::TStd& stds,
                           CSeq_align::TDim row_1,
                           CSeq_align::TDim row_2,
                           CAlnUserOptions::EDirection direction,
                           const TAlnSeqIdVec* ids)
{
    _ALNMGR_ASSERT(row_1 >=0  &&  row_2 >= 0);

    TSeqPos last_to_1 = 0;

    int guessed_width_1 = 0;
    int guessed_width_2 = 0;

    // Check global strand of the first row - we'll need it for gaps.
    bool global_first_direct = true;
    ITERATE (CSeq_align::TSegs::TStd, std_it, stds) {
        const CStd_seg::TLoc& loc = (*std_it)->GetLoc();
        if ( !loc[row_1]->GetTotalRange().Empty() ) {
            global_first_direct = !loc[row_1]->IsReverseStrand();
            break;
        }
    }

    ITERATE (CSeq_align::TSegs::TStd, std_it, stds) {

        const CStd_seg::TLoc& loc = (*std_it)->GetLoc();
        
        _ALNMGR_ASSERT((CSeq_align::TDim) loc.size() > max(row_1, row_2));

        CSeq_loc::TRange rng_1 = loc[row_1]->GetTotalRange();
        CSeq_loc::TRange rng_2 = loc[row_2]->GetTotalRange();

        TSeqPos len_1 = rng_1.GetLength();
        TSeqPos len_2 = rng_2.GetLength();
        bool first_direct = !loc[row_1]->IsReverseStrand();
        bool direct = first_direct != loc[row_2]->IsReverseStrand();

        if (len_1 > 0  &&  len_2 > 0) {

            last_to_1 = first_direct ? rng_1.GetFrom() + len_1 : rng_1.GetFrom();
            if (direction == CAlnUserOptions::eBothDirections  ||
                (direct ?
                 direction == CAlnUserOptions::eDirect :
                 direction == CAlnUserOptions::eReverse)) {

                TSeqPos nuc_to_nuc_diff =
                    abs((TSignedSeqPos)len_1 - (TSignedSeqPos)len_2);
                TSeqPos prot_to_nuc_diff =
                    abs((TSignedSeqPos)len_1*3 - (TSignedSeqPos)len_2);
                TSeqPos nuc_to_prot_diff =
                    abs((TSignedSeqPos)len_1 - (TSignedSeqPos)len_2*3);
                bool row_1_is_protein = prot_to_nuc_diff < nuc_to_nuc_diff;
                bool row_2_is_protein = nuc_to_prot_diff < nuc_to_nuc_diff;

                if (nuc_to_nuc_diff != 0) {
                    // If the alignment is mixed, using genomic coordinates.
                    pairwise_aln.SetUsingGenomic();
                }

                guessed_width_1 = row_1_is_protein ? 3 : 1;
                guessed_width_2 = row_2_is_protein ? 3 : 1;

                CPairwiseAln::TAlnRng aln_rng;
                aln_rng.SetDirect(direct);
                aln_rng.SetFirstDirect(first_direct);
                if (!row_1_is_protein && !row_2_is_protein) {
                    aln_rng.SetFirstFrom(rng_1.GetFrom());
                    aln_rng.SetSecondFrom(rng_2.GetFrom());
                    if (len_1 != len_2) {
                        TSeqPos remainder =
                            abs((TSignedSeqPos)len_1 - (TSignedSeqPos)len_2);
                        aln_rng.SetLength(min(len_1, len_2));
                        pairwise_aln.insert(aln_rng);
                        pairwise_aln.insert
                            (CPairwiseAln::TAlnRng
                             (aln_rng.GetFirstToOpen(),
                              aln_rng.GetSecondToOpen(),
                              remainder,
                              direct, first_direct));
                    } else {
                        aln_rng.SetLength(len_1);
                        pairwise_aln.insert(aln_rng);
                    }
                } else if (row_2_is_protein) {
                    aln_rng.SetFirstFrom(rng_1.GetFrom());
                    aln_rng.SetSecondFrom(rng_2.GetFrom() * 3);
                    if (len_1 / 3 < len_2) {
                        _ALNMGR_ASSERT(len_1 / 3 == len_2 - 1);
                        TSeqPos remainder = len_1 % 3;
                        aln_rng.SetLength(len_1 - remainder);
                        pairwise_aln.insert(aln_rng);
                        pairwise_aln.insert
                            (CPairwiseAln::TAlnRng
                             (aln_rng.GetFirstToOpen(),
                              aln_rng.GetSecondToOpen(),
                              remainder,
                              direct, first_direct));
                    } else {
                        aln_rng.SetLength(len_1);
                        pairwise_aln.insert(aln_rng);
                    }
                } else { // row 1 is protein
                    aln_rng.SetFirstFrom(rng_1.GetFrom() * 3);
                    aln_rng.SetSecondFrom(rng_2.GetFrom());
                    if (len_2 / 3 < len_1) {
                        _ALNMGR_ASSERT(len_2 / 3 == len_1 - 1);
                        TSeqPos remainder = len_2 % 3;
                        aln_rng.SetLength(len_2 - remainder);
                        pairwise_aln.insert(aln_rng);
                        pairwise_aln.insert
                            (CPairwiseAln::TAlnRng
                             (aln_rng.GetFirstToOpen(),
                              aln_rng.GetSecondToOpen(),
                              remainder,
                              direct, first_direct));
                    } else {
                        aln_rng.SetLength(len_2);
                        pairwise_aln.insert(aln_rng);
                    }
                }
            }
        }
        else if (len_2 > 0) {
            // Gap in the second row - add insertion
            TSignedSeqPos from_1 = last_to_1;
            TSignedSeqPos from_2 = rng_2.GetFrom();
            TSignedSeqPos len = len_2;
            // If we've seen a normal segment, widths should be set (but may be
            // different from base widths from the pairwise_aln). Otherwise we
            // have to use pairwise alignment.
            if (guessed_width_1 == 0) {
                guessed_width_1 = pairwise_aln.GetFirstBaseWidth();
            }
            if (guessed_width_2 == 0) {
                guessed_width_2 = pairwise_aln.GetSecondBaseWidth();
            }
            from_1 *= guessed_width_1;
            from_2 *= guessed_width_2;
            len *= guessed_width_2;
            bool gap_direct = global_first_direct != loc[row_2]->IsReverseStrand();
            pairwise_aln.AddInsertion(CPairwiseAln::TAlnRng(
                from_1, from_2, len, gap_direct, global_first_direct));
        }
        else if (len_1 > 0) {
            last_to_1 = first_direct ? rng_1.GetFrom() + len_1 : rng_1.GetFrom();
        }
    }
}



void
ConvertDendiagToPairwiseAln(CPairwiseAln& pairwise_aln,
                            const CSeq_align::TSegs::TDendiag& dendiags,
                            CSeq_align::TDim row_1,
                            CSeq_align::TDim row_2,
                            CAlnUserOptions::EDirection direction,
                            const TAlnSeqIdVec* ids)
{
    _ALNMGR_ASSERT(row_1 >=0  &&  row_2 >= 0);

    bool mixed = IsMixedAlignment(ids);

    ITERATE (CSeq_align::TSegs::TDendiag, dendiag_it, dendiags) {

        const CDense_diag& dd = **dendiag_it;

        _ASSERT(max(row_1, row_2) < dd.GetDim());

        TSeqPos from_1 = dd.GetStarts()[row_1];
        TSeqPos from_2 = dd.GetStarts()[row_2];
        TSeqPos len = dd.GetLen();

        // determinte the strands
        bool direct = true;
        bool first_direct = true;
        if (dd.IsSetStrands()) {
            first_direct = !IsReverse(dd.GetStrands()[row_1]);
            bool minus_2 = IsReverse(dd.GetStrands()[row_2]);
            direct = first_direct != minus_2;
        }

        if (direction == CAlnUserOptions::eBothDirections  ||
            (direct ?
             direction == CAlnUserOptions::eDirect :
             direction == CAlnUserOptions::eReverse)) {

            // base-width adjustments
            const int& base_width_1 = pairwise_aln.GetFirstBaseWidth();
            const int& base_width_2 = pairwise_aln.GetSecondBaseWidth();
            if (mixed  ||  base_width_1 > 1) {
                // Tell the pairwise alignment we are already using genomic
                // coordinates.
                pairwise_aln.SetUsingGenomic();

                if (base_width_1 > 1) {
                    from_1 *= base_width_1;
                }
                if (base_width_2 > 1) {
                    from_2 *= base_width_2;
                }
                len *= 3;
            }

            // insert the range
            pairwise_aln.insert(CPairwiseAln::TAlnRng(
                from_1, from_2, len, direct, first_direct));

        }
    }
}


void
ConvertSparseToPairwiseAln(CPairwiseAln& pairwise_aln,
                           const CSparse_seg& sparse_seg,
                           CSeq_align::TDim row_1,
                           CSeq_align::TDim row_2,
                           CAlnUserOptions::EDirection direction,
                           const TAlnSeqIdVec* ids)
{
    typedef CPairwiseAln::TAlnRngColl TAlnRngColl;

    // Sparse-segs are not intended to store mixed alignments.
    _ALNMGR_ASSERT(pairwise_aln.GetFirstId()->GetBaseWidth() ==
        pairwise_aln.GetSecondId()->GetBaseWidth());

    _ALNMGR_ASSERT(row_1 == 0); // TODO: Hanlde case when the anchor is not sparse_aln's anchor.
    if (row_1 == 0) {
        if (row_2 == 0) { // Anchor aligned to itself
            bool first_row = true;
            ITERATE(CSparse_seg::TRows, aln_it, sparse_seg.GetRows()) {
                TAlnRngColl curr_row;
                const CSparse_align& sa = **aln_it;
                const CSparse_align::TFirst_starts& starts_1 = sa.GetFirst_starts();
                const CSparse_align::TLens& lens = sa.GetLens();
                for (CSparse_align::TNumseg seg = 0;
                     seg < sa.GetNumseg();  seg++) {
                    CPairwiseAln::TAlnRng aln_rng(
                        starts_1[seg], starts_1[seg], lens[seg]);
                    if (first_row) {
                        pairwise_aln.insert(aln_rng);
                    } else {
                        curr_row.insert(aln_rng);
                    }
                }
                if (first_row) {
                    first_row = false;
                } else {
                    TAlnRngColl diff;
                    SubtractAlnRngCollections(curr_row, pairwise_aln, diff);
                    ITERATE(TAlnRngColl, aln_rng_it, diff) {
                        pairwise_aln.insert(*aln_rng_it);
                    }
                }                    
            }
        } else { // Regular row
            _ALNMGR_ASSERT(row_2 > 0  &&  row_2 <= sparse_seg.CheckNumRows());

            const CSparse_align& sa = *sparse_seg.GetRows()[row_2 - 1];

            const CSparse_align::TFirst_starts& starts_1 = sa.GetFirst_starts();
            const CSparse_align::TSecond_starts& starts_2 = sa.GetSecond_starts();
            const CSparse_align::TLens& lens = sa.GetLens();
            const CSparse_align::TSecond_strands* strands =
                sa.IsSetSecond_strands() ? &sa.GetSecond_strands() : 0;

            CSparse_align::TNumseg seg;
            for (seg = 0;  seg < sa.GetNumseg();  seg++) {
                pairwise_aln.insert(CPairwiseAln::TAlnRng(
                    starts_1[seg], starts_2[seg], lens[seg],
                    strands ? !IsReverse((*strands)[seg]) : true));
            }
        }
    }
}


void
ConvertSplicedToPairwiseAln(CPairwiseAln& pairwise_aln,
                            const CSpliced_seg& spliced_seg,
                            CSeq_align::TDim row_1,
                            CSeq_align::TDim row_2,
                            CAlnUserOptions::EDirection direction,
                            const TAlnSeqIdVec* ids)
{
    _ALNMGR_ASSERT((row_1 == 0  ||  row_1 == 1)  &&  (row_2 == 0  ||  row_2 == 1));

    bool prot = spliced_seg.GetProduct_type() == CSpliced_seg::eProduct_type_protein;
    // With spliced-seg we always know how to translate coordinates and use
    // genomic ones.
    pairwise_aln.SetUsingGenomic();

    ITERATE (CSpliced_seg::TExons, exon_it, spliced_seg.GetExons()) {

        const CSpliced_exon& exon = **exon_it;
            
        // Determine strands
        if (spliced_seg.CanGetProduct_strand()  &&  exon.CanGetProduct_strand()  &&
            spliced_seg.GetProduct_strand() != exon.GetProduct_strand()) {
            NCBI_THROW(CSeqalignException, eInvalidAlignment,
                       "Product strands are not consistent.");
        }
        bool product_plus = true;
        if (exon.CanGetProduct_strand()) {
            product_plus = !IsReverse(exon.GetProduct_strand());
        } else if (spliced_seg.CanGetProduct_strand()) {
            product_plus = !IsReverse(spliced_seg.GetProduct_strand());
        }
        _ALNMGR_ASSERT(prot ? product_plus : true);

        if (spliced_seg.CanGetGenomic_strand()  &&  exon.CanGetGenomic_strand()  &&
            spliced_seg.GetGenomic_strand() != exon.GetGenomic_strand()) {
            NCBI_THROW(CSeqalignException, eInvalidAlignment,
                       "Genomic strands are not consistent.");
        }
        bool genomic_plus = true;
        if (exon.CanGetGenomic_strand()) {
            genomic_plus = !IsReverse(exon.GetGenomic_strand());
        } else if (spliced_seg.CanGetGenomic_strand()) {
            genomic_plus = !IsReverse(spliced_seg.GetGenomic_strand());
        }
        bool direct = product_plus == genomic_plus;
        bool first_direct = row_1 == 0 ? product_plus : genomic_plus;

        // Determine positions
        TSeqPos product_start = exon.GetProduct_start().AsSeqPos();
        TSeqPos product_end = exon.GetProduct_end().AsSeqPos();

        TSeqPos product_pos = product_plus ? product_start : product_end;

        TSeqPos genomic_start = exon.GetGenomic_start();
        TSeqPos genomic_end = exon.GetGenomic_end();
        TSeqPos genomic_pos = (genomic_plus ? 
                               exon.GetGenomic_start() :
                               exon.GetGenomic_end());

        if (exon.GetParts().empty()) {
            TSeqPos product_len = product_end - product_start + 1;
            TSeqPos genomic_len = genomic_end - genomic_start + 1;

            _ALNMGR_ASSERT(product_len == genomic_len);
            _ALNMGR_ASSERT(genomic_len != 0);
            
            TSeqPos starts[] = { product_start, genomic_start };
            if (row_1 == row_2  ||
                direction == CAlnUserOptions::eBothDirections  ||
                (direct ?
                 direction == CAlnUserOptions::eDirect :
                 direction == CAlnUserOptions::eReverse)) {
                pairwise_aln.insert(CPairwiseAln::TAlnRng(
                    starts[row_1], starts[row_2], genomic_len,
                    row_1 == row_2 ? true : direct,
                    first_direct));
            }
        } else {
            // Iterate trhough exon chunks
            TSeqPos last_product_to = 0;
            TSeqPos last_genomic_to = 0;
            ITERATE (CSpliced_exon::TParts, chunk_it, exon.GetParts()) {
                const CSpliced_exon_chunk& chunk = **chunk_it;
                
                TSeqPos product_len = 0;
                TSeqPos genomic_len = 0;
            
                switch (chunk.Which()) {
                case CSpliced_exon_chunk::e_Match: 
                    product_len = genomic_len = chunk.GetMatch();
                    break;
                case CSpliced_exon_chunk::e_Diag: 
                    product_len = genomic_len = chunk.GetDiag();
                    break;
                case CSpliced_exon_chunk::e_Mismatch:
                    product_len = genomic_len = chunk.GetMismatch();
                    break;
                case CSpliced_exon_chunk::e_Product_ins:
                    product_len = chunk.GetProduct_ins();
                    break;
                case CSpliced_exon_chunk::e_Genomic_ins:
                    genomic_len = chunk.GetGenomic_ins();
                    break;
                default:
                    break;
                }
                TSeqPos ppos = product_plus ? product_pos : product_pos - product_len + 1;
                TSeqPos gpos = genomic_plus ? genomic_pos : genomic_pos - genomic_len + 1;
                if (row_1 == 0  &&  row_2 == 0) {
                    if (product_len != 0) {
                        // insert the range
                        pairwise_aln.insert(CPairwiseAln::TAlnRng(
                            ppos, ppos, product_len, true, first_direct));
                        last_product_to = first_direct ? ppos + product_len : ppos;
                    }
                } else if (row_1 == 1  &&  row_2 == 1) {
                    if (genomic_len != 0) {
                        // insert the range
                        pairwise_aln.insert(CPairwiseAln::TAlnRng(
                            gpos, gpos, genomic_len, true, first_direct));
                        last_genomic_to = first_direct ? gpos + genomic_len : gpos;
                    }
                } else {
                    if (product_len != 0  &&  product_len == genomic_len  &&
                        (direction == CAlnUserOptions::eBothDirections  ||
                         (direct ?
                          direction == CAlnUserOptions::eDirect :
                          direction == CAlnUserOptions::eReverse))) {
                        // insert the range
                        if (row_1 == 0) {
                            pairwise_aln.insert(CPairwiseAln::TAlnRng(
                                ppos, gpos, genomic_len, direct, first_direct));
                            last_product_to = first_direct ? ppos + product_len : ppos;
                            last_genomic_to = direct ? gpos + genomic_len : gpos;
                        } else {
                            pairwise_aln.insert(CPairwiseAln::TAlnRng(
                                gpos, ppos, genomic_len, direct, first_direct));
                            last_product_to = direct ? ppos + product_len : ppos;
                            last_genomic_to = first_direct ? gpos + genomic_len : gpos;
                        }
                    }
                    else {
                        // Gap on the first row?
                        if (row_1 == 0) {
                            if (product_len == 0  &&  genomic_len != 0) {
                                pairwise_aln.AddInsertion(CPairwiseAln::TAlnRng(
                                    last_product_to, gpos, genomic_len,
                                    direct, first_direct));
                            }
                            else if (product_len != 0  &&  genomic_len == 0) {
                                if (product_plus) {
                                    last_product_to += product_len;
                                }
                                else {
                                    last_product_to -= product_len;
                                }
                            }
                        }
                        else if (row_1 == 1) {
                            if (genomic_len == 0  &&  product_len != 0) {
                                pairwise_aln.AddInsertion(CPairwiseAln::TAlnRng(
                                    last_genomic_to, ppos, product_len,
                                    direct, first_direct));
                            }
                            else if (genomic_len != 0  &&  product_len == 0) {
                                if (genomic_plus) {
                                    last_genomic_to += genomic_len;
                                }
                                else {
                                    last_genomic_to -= genomic_len;
                                }
                            }
                        }
                    }
                }
                if (product_plus) {
                    product_pos += product_len;
                } else {
                    product_pos -= product_len;
                }
                if (genomic_plus) {
                    genomic_pos += genomic_len;
                } else {
                    genomic_pos -= genomic_len;
                }
            }
        }
    }
}


void
ConvertSeqLocsToPairwiseAln(CPairwiseAln& aln,
                            const objects::CSeq_loc& loc_1,
                            const objects::CSeq_loc& loc_2,
                            CAlnUserOptions::EDirection direction)
{
    // Make sure each seq-loc contains just one seq-id
    _ASSERT(loc_1.GetId());
    _ASSERT(loc_2.GetId());

    // Rough check if strands are the same (may be false-positive if
    // there are multiple strands).
    bool direct = 
        loc_1.IsReverseStrand() == loc_2.IsReverseStrand();

    if (direction != CAlnUserOptions::eBothDirections  &&
        (direct ?
            direction != CAlnUserOptions::eDirect :
            direction != CAlnUserOptions::eReverse)) {
        return;
    }

    TSeqPos wid1 = aln.GetFirstBaseWidth();
    if ( !wid1 ) {
        wid1 = 1;
    }
    TSeqPos wid2 = aln.GetSecondBaseWidth();
    if ( !wid2 ) {
        wid2 = 1;
    }
    if (wid1 == 3  ||  wid2 == 3) {
        // We know we are translating everything to genomic coordinates.
        aln.SetUsingGenomic();
    }

    CSeq_loc_CI it1(loc_1);
    CSeq_loc_CI it2(loc_2);
    TSeqPos lshift1 = 0;
    TSeqPos lshift2 = 0;
    TSeqPos rshift1 = 0;
    TSeqPos rshift2 = 0;
    while (it1  &&  it2) {
        if (it1.IsEmpty()) {
            ++it1;
            continue;
        }
        if (it2.IsEmpty()) {
            ++it2;
            continue;
        }
        bool rev1 = IsReverse(it1.GetStrand());
        bool rev2 = IsReverse(it2.GetStrand());
        TSeqPos len1 = it1.GetRange().GetLength()*wid1 - lshift1 - rshift1;
        TSeqPos len2 = it2.GetRange().GetLength()*wid2 - lshift2 - rshift2;
        TSeqPos len = len1 > len2 ? len2 : len1;
        TSeqPos start1 = it1.GetRange().GetFrom()*wid1 + lshift1;
        if ( rev1 ) {
            start1 += len1 - len;
        }
        TSeqPos start2 = it2.GetRange().GetFrom()*wid2 + lshift2;
        if ( rev2 ) {
            start2 += len2 - len;
        }
        aln.insert(CPairwiseAln::TAlnRng(start1, start2, len, rev1 == rev2, rev1));
        if ( rev1 ) {
            rshift1 += len;
        }
        else {
            lshift1 += len;
        }
        if ( rev2 ) {
            rshift2 += len;
        }
        else {
            lshift2 += len;
        }
        if (len1 == len) {
            ++it1;
            lshift1 = rshift1 = 0;
        }
        if (len2 == len) {
            ++it2;
            lshift2 = rshift2 = 0;
        }
    }
}


typedef map<CSeq_id_Handle, CRef<CPairwiseAln> > TAlnMap;
typedef map<CSeq_id_Handle, CSeq_id_Handle> TSynonymsMap;

CSeq_id_Handle s_GetBestSynonym(const CSeq_id_Handle& idh,
                                TSynonymsMap& syn_map,
                                const CSeq_loc_Mapper_Base& mapper)
{
    TSynonymsMap::const_iterator best_it = syn_map.find(idh);
    if (best_it != syn_map.end()) {
        return best_it->second;
    }
    // Add all synonyms for the new id handle
    CSeq_loc_Mapper_Base::TSynonyms syn_set;
    mapper.CollectSynonyms(idh, syn_set);
    CSeq_id_Handle best_id = idh;
    int best_score = idh.GetSeqId()->BestRankScore();
    ITERATE(CSeq_loc_Mapper_Base::TSynonyms, it, syn_set) {
        int score = it->GetSeqId()->BestRankScore();
        if (score < best_score) {
            best_id = *it;
            best_score = score;
        }
    }
    ITERATE(CSeq_loc_Mapper_Base::TSynonyms, it, syn_set) {
        syn_map[*it] = best_id;
    }
    return best_id;
}


void SeqLocMapperToPairwiseAligns(const objects::CSeq_loc_Mapper_Base& mapper,
                                  TPairwiseAlnList&                    aligns)
{
    aligns.clear();
    TSynonymsMap synonyms;

    const CMappingRanges& mappings = mapper.GetMappingRanges();
    ITERATE(CMappingRanges::TIdMap, id_it, mappings.GetIdMap()) {
        CSeq_id_Handle src_idh =
            s_GetBestSynonym(id_it->first, synonyms, mapper);
        if (src_idh != id_it->first) {
            continue; // skip synonyms
        }
        TAlnSeqIdIRef src_id(Ref(new CAlnSeqId(*src_idh.GetSeqId())));
        src_id->SetBaseWidth(mapper.GetWidthById(src_idh));
        TAlnMap aln_map;
        ITERATE(CMappingRanges::TRangeMap, rg_it, id_it->second) {
            const CMappingRange& mrg = *rg_it->second;
            CSeq_id_Handle dst_idh =
                s_GetBestSynonym(mrg.GetDstIdHandle(), synonyms, mapper);
            if (dst_idh == src_idh) {
                continue; // skip self-mappings
            }
            TAlnMap::iterator aln_it = aln_map.find(dst_idh);
            CRef<CPairwiseAln> aln;
            if (aln_it == aln_map.end()) {
                TAlnSeqIdIRef dst_id(Ref(new CAlnSeqId(*dst_idh.GetSeqId())));
                dst_id->SetBaseWidth(mapper.GetWidthById(dst_idh));
                aln = new CPairwiseAln(src_id, dst_id);
                aln->SetUsingGenomic(); // All coordinates are already genomic.
                aln_map[dst_idh] = aln;
                aligns.push_back(aln);
            }
            else {
                aln = aln_it->second;
            }
            aln->insert(CPairwiseAln::TAlnRng(mrg.GetSrc_from(),
                mrg.GetDst_from(), mrg.GetLength(), mrg.GetReverse()));
        }
    }
}


CRef<CPairwiseAln>
CreatePairwiseAlnFromSeqAlign(const CSeq_align& sa)
{
    _ALNMGR_ASSERT(sa.CheckNumRows() == 2);

    TAlnSeqIdIRef id1(new CAlnSeqId(sa.GetSeq_id(0)));
    TAlnSeqIdIRef id2(new CAlnSeqId(sa.GetSeq_id(1)));
    CRef<CPairwiseAln> pairwise(new CPairwiseAln(id1, id2));
    ConvertSeqAlignToPairwiseAln(*pairwise, sa, 0, 1);
    return pairwise;
}


END_NCBI_SCOPE
