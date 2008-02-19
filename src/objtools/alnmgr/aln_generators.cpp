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
*   Alignment generators
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
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Prot_pos.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objtools/alnmgr/aln_generators.hpp>
#include <objtools/alnmgr/alnexception.hpp>

#include <util/range_coll.hpp>

#include <serial/typeinfo.hpp> // for SerialAssign

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


CRef<CSeq_align>
CreateSeqAlignFromAnchoredAln(const CAnchoredAln& anchored_aln,   ///< input
                              CSeq_align::TSegs::E_Choice choice) ///< choice of alignment 'segs'
{
    CRef<CSeq_align> sa(new CSeq_align);
    sa->SetType(CSeq_align::eType_not_set);
    sa->SetDim(anchored_aln.GetDim());

    switch(choice)    {
    case CSeq_align::TSegs::e_Dendiag:
        break;
    case CSeq_align::TSegs::e_Denseg: {
        sa->SetSegs().SetDenseg(*CreateDensegFromAnchoredAln(anchored_aln));
        break;
    }
    case CSeq_align::TSegs::e_Std:
        break;
    case CSeq_align::TSegs::e_Packed:
        break;
    case CSeq_align::TSegs::e_Disc:
        break;
    case CSeq_align::TSegs::e_Spliced:
        break;
    case CSeq_align::TSegs::e_Sparse:
        break;
    case CSeq_align::TSegs::e_not_set:
        NCBI_THROW(CAlnException, eInvalidRequest,
                   "Invalid CSeq_align::TSegs type.");
        break;
    }
    return sa;
}


class CSegmentedRangeCollection : public CRangeCollection<CPairwiseAln::TPos>
{
public:
    typedef ncbi::CRangeCollection<CPairwiseAln::TPos> TParent;

    const_iterator CutAtPosition(position_type pos) {
        iterator ret_it = TParent::m_vRanges.end();
        iterator it = find_nc(pos);
        if (it != TParent::end()  &&  it->GetFrom() < pos) {
            TRange left_clip_r(it->GetFrom(), pos-1);
            TRange right_clip_r(pos, it->GetTo());
            ret_it = TParent::m_vRanges.insert(TParent::m_vRanges.erase(it),
                                               right_clip_r);
            TParent::m_vRanges.insert(ret_it, left_clip_r);
        }
        return ret_it;
    }

    void insert(const TRange& r) {
        // Cut
        CutAtPosition(r.GetFrom());
        CutAtPosition(r.GetToOpen());
        
        // Find the diff if any
        TParent addition;
        addition.CombineWith(r);
        addition.Subtract(*this);
        
        // Insert the diff
        iterator it = find_nc(addition.begin()->GetToOpen());
        ITERATE(TParent, add_it, addition) {
            TRange rr(add_it->GetFrom(), add_it->GetTo());
            it = TParent::m_vRanges.insert(it, rr);
            ++it;
        }
    }
};


CRef<CDense_seg>
CreateDensegFromAnchoredAln(const CAnchoredAln& anchored_aln) 
{
    const CAnchoredAln::TPairwiseAlnVector& pairwises = anchored_aln.GetPairwiseAlns();

    //typedef CRangeCollection<CPairwiseAln::TPos> TAnchorSegments;
    typedef CSegmentedRangeCollection TAnchorSegments;
    TAnchorSegments anchor_segments;
    ITERATE(CAnchoredAln::TPairwiseAlnVector, pairwise_aln_i, pairwises) {
        ITERATE (CPairwiseAln::TAlnRngColl, rng_i, **pairwise_aln_i) {
            anchor_segments.insert(CPairwiseAln::TRng(rng_i->GetFirstFrom(), rng_i->GetFirstTo()));
        }
    }

    /// Create a dense-seg
    CRef<CDense_seg> ds(new CDense_seg);

    /// Determine dimensions
    CDense_seg::TNumseg& numseg = ds->SetNumseg();
    numseg = anchor_segments.size();
    CDense_seg::TDim& dim = ds->SetDim();
    dim = anchored_aln.GetDim();

    /// Tmp vars
    CDense_seg::TDim row;
    CDense_seg::TNumseg seg;

    /// Ids
    CDense_seg::TIds& ids = ds->SetIds();
    ids.resize(dim);
    for (row = 0;  row < dim;  ++row) {
        ids[row].Reset(new CSeq_id);
        SerialAssign<CSeq_id>(*ids[row], anchored_aln.GetId(row)->GetSeqId());
    }

    /// Lens
    CDense_seg::TLens& lens = ds->SetLens();
    lens.resize(numseg);
    TAnchorSegments::const_iterator seg_i = anchor_segments.begin();
    for (seg = 0; seg < numseg; ++seg, ++seg_i) {
        lens[seg] = seg_i->GetLength();
    }

    int matrix_size = dim * numseg;

    /// Strands (just resize, will set while setting starts)
    CDense_seg::TStrands& strands = ds->SetStrands();
    strands.resize(matrix_size, eNa_strand_unknown);

    /// Starts and strands
    CDense_seg::TStarts& starts = ds->SetStarts();
    starts.resize(matrix_size, -1);
    for (row = 0;  row < dim;  ++row) {
        seg = 0;
        int matrix_row_pos = row;  // optimization to eliminate multiplication
        seg_i = anchor_segments.begin();
        CPairwiseAln::TAlnRngColl::const_iterator aln_rng_i = pairwises[row]->begin();
        TSignedSeqPos left_delta = 0;
        TSignedSeqPos right_delta = aln_rng_i->GetLength();
        bool direct = aln_rng_i->IsDirect();
        while (seg_i != anchor_segments.end()) {
            _ASSERT(seg < numseg);
            _ASSERT(matrix_row_pos == row + dim * seg);
            if (aln_rng_i != pairwises[row]->end()  &&
                seg_i->GetFrom() >= aln_rng_i->GetFirstFrom()) {
                starts[matrix_row_pos] = 
                    (direct ?
                     aln_rng_i->GetSecondFrom() + left_delta :
                     aln_rng_i->GetSecondToOpen() - right_delta);
                left_delta += seg_i->GetLength();
                _ASSERT(right_delta >= seg_i->GetLength());
                right_delta -= seg_i->GetLength();
                if (right_delta == 0) {
                    ++aln_rng_i;
                    if (aln_rng_i != pairwises[row]->end()) {
                        left_delta = 0;
                        right_delta = aln_rng_i->GetLength();
                        direct = aln_rng_i->IsDirect();
                    }
                }
            }
            strands[matrix_row_pos] = (direct ? eNa_strand_plus : eNa_strand_minus);
            ++seg_i;
            ++seg;
            matrix_row_pos += dim;
        }
    }
#if _DEBUG
    ds->Validate(true);
#endif    
    return ds;
}


CRef<CDense_seg>
CreateDensegFromPairwiseAln(const CPairwiseAln& pairwise_aln)
{
    /// Create a dense-seg
    CRef<CDense_seg> ds(new CDense_seg);


    /// Determine dimensions
    CDense_seg::TNumseg& numseg = ds->SetNumseg();
    numseg = pairwise_aln.size();
    ds->SetDim(2);
    int matrix_size = 2 * numseg;

    CDense_seg::TLens& lens = ds->SetLens();
    lens.resize(numseg);

    CDense_seg::TStarts& starts = ds->SetStarts();
    starts.resize(matrix_size, -1);

    CDense_seg::TIds& ids = ds->SetIds();
    ids.resize(2);


    /// Ids
    ids[0].Reset(new CSeq_id);
    SerialAssign<CSeq_id>(*ids[0], pairwise_aln.GetFirstId()->GetSeqId());
    ids[1].Reset(new CSeq_id);
    SerialAssign<CSeq_id>(*ids[1], pairwise_aln.GetSecondId()->GetSeqId());


    /// Tmp vars
    CDense_seg::TNumseg seg = 0;
    int matrix_pos = 0;


    /// Main loop to set all values
    ITERATE(CPairwiseAln::TAlnRngColl, aln_rng_i, pairwise_aln) {
        starts[matrix_pos++] = aln_rng_i->GetFirstFrom();
        if ( !aln_rng_i->IsDirect() ) {
            if ( !ds->IsSetStrands() ) {
                ds->SetStrands().resize(matrix_size, eNa_strand_plus);
            }
            ds->SetStrands()[matrix_pos] = eNa_strand_minus;
        }
        starts[matrix_pos++] = aln_rng_i->GetSecondFrom();
        lens[seg++] = aln_rng_i->GetLength();
    }
    _ASSERT(matrix_pos == matrix_size);
    _ASSERT(seg == numseg);


#ifdef _DEBUG
    ds->Validate(true);
#endif
    return ds;
}


CRef<CSpliced_seg>
CreateSplicedsegFromAnchoredAln(const CAnchoredAln& anchored_aln)
{
    _ASSERT(anchored_aln.GetDim() == 2);

    /// Create a spliced_seg
    CRef<CSpliced_seg> spliced_seg(new CSpliced_seg);

    const CPairwiseAln& pairwise = *anchored_aln.GetPairwiseAlns()[0];
    _ASSERT(pairwise.GetSecondBaseWidth() == 1  ||
            pairwise.GetSecondBaseWidth() == 3);
    _ASSERT(pairwise.GetSecondBaseWidth() == 1);
    bool prot = pairwise.GetFirstBaseWidth() == 3;

    /// Ids
    CRef<CSeq_id> product_id(new CSeq_id); 
    SerialAssign<CSeq_id>(*product_id, pairwise.GetFirstId()->GetSeqId());
    spliced_seg->SetProduct_id(*product_id);
    CRef<CSeq_id> genomic_id(new CSeq_id); 
    SerialAssign<CSeq_id>(*genomic_id, pairwise.GetFirstId()->GetSeqId());
    spliced_seg->SetGenomic_id(*genomic_id);


    /// Product type
    spliced_seg->SetProduct_type(prot ?
                                 CSpliced_seg::eProduct_type_protein :
                                 CSpliced_seg::eProduct_type_transcript);


    /// Exons
    CSpliced_seg::TExons& exons = spliced_seg->SetExons();

    typedef TSignedSeqPos                  TPos;
    typedef CRange<TPos>                   TRng; 
    typedef CAlignRange<TPos>              TAlnRng;
    typedef CAlignRangeCollection<TAlnRng> TAlnRngColl;

    ITERATE (CPairwiseAln::TAlnRngColl, rng_it, pairwise) {
        const CPairwiseAln::TAlnRng& rng = *rng_it;
        CRef<CSpliced_exon> exon(new CSpliced_exon);
        if (prot) {
            exon->SetProduct_start().SetProtpos().SetAmin(rng.GetFirstFrom() / 3);
            exon->SetProduct_start().SetProtpos().SetFrame(rng.GetFirstFrom() % 3 + 1);
            exon->SetProduct_end().SetProtpos().SetAmin(rng.GetFirstTo() / 3);
            exon->SetProduct_end().SetProtpos().SetFrame(rng.GetFirstTo() % 3 + 1);
        } else {
            exon->SetProduct_start().SetNucpos(rng.GetFirstFrom());
            exon->SetProduct_end().SetNucpos(rng.GetFirstTo());
        }
        exon->SetGenomic_start(rng.GetSecondFrom());
        exon->SetGenomic_end(rng.GetSecondTo());

        exon->SetProduct_strand(eNa_strand_plus);
        exon->SetGenomic_strand(rng.IsDirect() ?
                                eNa_strand_plus :
                                eNa_strand_minus);
        exons.push_back(exon);
    }
    

#ifdef _DEBUG
    spliced_seg->Validate(true);
#endif
    return spliced_seg;
}


END_NCBI_SCOPE
