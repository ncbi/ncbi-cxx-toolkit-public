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
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Prot_pos.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objtools/alnmgr/aln_generators.hpp>
#include <objtools/alnmgr/alnexception.hpp>
#include <objtools/alnmgr/aln_serial.hpp>
#include <objtools/alnmgr/aln_converters.hpp>

#include <util/range_coll.hpp>

#include <serial/typeinfo.hpp> // for SerialAssign

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


CRef<CSeq_align>
CreateSeqAlignFromAnchoredAln(const CAnchoredAln& anchored_aln,   ///< input
                              CSeq_align::TSegs::E_Choice choice, ///< choice of alignment 'segs'
                              CScope* scope)
{
    CRef<CSeq_align> sa(new CSeq_align);
    sa->SetType(CSeq_align::eType_not_set);
    sa->SetDim(anchored_aln.GetDim());

    switch(choice) {
    case CSeq_align::TSegs::e_Dendiag:
        CreateDense_diagFromAnchoredAln(sa->SetSegs().SetDendiag(), anchored_aln, scope);
        break;
    case CSeq_align::TSegs::e_Denseg:
        sa->SetSegs().SetDenseg(*CreateDensegFromAnchoredAln(anchored_aln, scope));
        break;
    case CSeq_align::TSegs::e_Std:
        break;
    case CSeq_align::TSegs::e_Packed:
        sa->SetSegs().SetPacked(*CreatePackedsegFromAnchoredAln(anchored_aln, scope));
        break;
    case CSeq_align::TSegs::e_Disc:
        sa->SetSegs().SetDisc(*CreateAlignSetFromAnchoredAln(anchored_aln, scope));
        break;
    case CSeq_align::TSegs::e_Spliced:
        sa->SetSegs().SetSpliced(*CreateSplicedsegFromAnchoredAln(anchored_aln, scope));
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


CRef<CSeq_align>
CreateSeqAlignFromPairwiseAln(const CPairwiseAln& pairwise_aln,   ///< input
                              CSeq_align::TSegs::E_Choice choice, ///< choice of alignment 'segs'
                              CScope* scope)
{
    CRef<CSeq_align> sa(new CSeq_align);
    sa->SetType(CSeq_align::eType_not_set);
    sa->SetDim(2);

    switch(choice) {
    case CSeq_align::TSegs::e_Denseg:
        sa->SetSegs().SetDenseg(*CreateDensegFromPairwiseAln(pairwise_aln, scope));
        break;
    case CSeq_align::TSegs::e_Disc:
        sa->SetSegs().SetDisc(*CreateAlignSetFromPairwiseAln(pairwise_aln, scope));
        break;
    case CSeq_align::TSegs::e_Packed:
        sa->SetSegs().SetPacked(*CreatePackedsegFromPairwiseAln(pairwise_aln, scope));
        break;
    case CSeq_align::TSegs::e_Spliced:
        sa->SetSegs().SetSpliced(*CreateSplicedsegFromPairwiseAln(pairwise_aln, scope));
        break;
    case CSeq_align::TSegs::e_Std:
    case CSeq_align::TSegs::e_Dendiag:
    case CSeq_align::TSegs::e_Sparse:
    case CSeq_align::TSegs::e_not_set:
        NCBI_THROW(CAlnException, eInvalidRequest,
                   "Unsupported CSeq_align::TSegs type.");
        break;
    }
    return sa;
}


//#define _TRACE_CSegmentedRangeCollection
#ifdef _TRACE_CSegmentedRangeCollection
ostream& operator<<(ostream& out, const CRangeCollection<CPairwiseAln::TPos>& segmetned_range_coll)
{
    out << "CRangeCollection<CPairwiseAln::TPos>" << endl;

    ITERATE (CRangeCollection<CPairwiseAln::TPos>, rng_it, segmetned_range_coll) {
        out << (CPairwiseAln::TRng)*rng_it << endl;
    }
    return out << endl;
}
#endif

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
#ifdef _TRACE_CSegmentedRangeCollection
        cerr << "=====================" << endl;
        cerr << "Original:" << *this;
        cerr << "Inserting: " <<  endl << (CPairwiseAln::TRng)r << endl << endl;
#endif
        // Cut
        CutAtPosition(r.GetFrom());
        CutAtPosition(r.GetToOpen());
#ifdef _TRACE_CSegmentedRangeCollection
        cerr << "After the cut:" << *this << endl;
#endif

        // Find the diff if any
        TParent addition;
        addition.CombineWith(r);
        addition.Subtract(*this);

        if ( !addition.empty() ) {
#ifdef _TRACE_CSegmentedRangeCollection
            cerr << "Addition: " << addition << endl;
#endif
            // Insert the diff
            iterator it = find_nc(addition.begin()->GetToOpen());
            ITERATE(TParent, add_it, addition) {
                TRange rr(add_it->GetFrom(), add_it->GetTo());
                while (it != TParent::m_vRanges.end()  &&
                       rr.GetFrom() >= it->GetFrom()) {
                    ++it;
                }
                it = TParent::m_vRanges.insert(it, rr);
                ++it;
            }
        }
#ifdef _TRACE_CSegmentedRangeCollection
        else {
            cerr << "No addition." << endl << endl;
        }
#endif
#ifdef _TRACE_CSegmentedRangeCollection
        cerr << "Result: " << *this;
        cerr << "=====================" << endl << endl;
#endif
    }
};


void
CreateDense_diagFromAnchoredAln(CSeq_align::TSegs::TDendiag& dd,
                                const CAnchoredAln& anchored_aln,
                                CScope* scope) 
{
    const CAnchoredAln::TPairwiseAlnVector& pairwises = anchored_aln.GetPairwiseAlns();

    typedef CSegmentedRangeCollection TAnchorSegments;
    TAnchorSegments anchor_segments;
    ITERATE(CAnchoredAln::TPairwiseAlnVector, pairwise_aln_i, pairwises) {
        ITERATE (CPairwiseAln::TAlnRngColl, rng_i, **pairwise_aln_i) {
            anchor_segments.insert(CPairwiseAln::TRng(rng_i->GetFirstFrom(), rng_i->GetFirstTo()));
        }
    }

    CSeq_align::TSegs::TDendiag diags;
    CDense_diag::TDim dim = anchored_aln.GetDim();
    size_t numseg = anchor_segments.size();
    for (size_t seg = 0; seg < numseg; ++seg) {
        CRef<CDense_diag> diag(new CDense_diag);
        diag->SetDim(dim);
        diag->SetIds().resize(dim);
        for (int row = 0;  row < dim;  ++row) {
            CRef<CSeq_id> id(new CSeq_id);
            id->Assign(anchored_aln.GetId(dim - row - 1)->GetSeqId());
            diag->SetIds()[row] = id;
        }
        diag->SetStarts().resize(dim, kInvalidSeqPos);
        diag->SetStrands().resize(dim);
        diag->SetLen(anchor_segments[seg].GetLength());
        diags.push_back(diag);
    }

    for (int row = 0;  row < dim;  ++row) {
        size_t seg = 0;
        CSeq_align::TSegs::TDendiag::iterator diag_it = diags.begin();

        TAnchorSegments::const_iterator seg_i = anchor_segments.begin();
        CPairwiseAln::TAlnRngColl::const_iterator aln_rng_i =
            pairwises[dim - row - 1]->begin();
        bool direct = aln_rng_i->IsDirect();
        TSignedSeqPos left_delta = 0;
        TSignedSeqPos right_delta = aln_rng_i->GetLength();
        while (seg_i != anchor_segments.end()) {
            _ASSERT(seg < numseg);
            if (aln_rng_i != pairwises[dim - row - 1]->end()  &&
                seg_i->GetFrom() >= aln_rng_i->GetFirstFrom()) {
                _ASSERT(seg_i->GetToOpen() <= aln_rng_i->GetFirstToOpen());
                if (seg_i->GetToOpen() > aln_rng_i->GetFirstToOpen()) {
                    NCBI_THROW(CAlnException, eInternalFailure,
                               "seg_i->GetToOpen() > aln_rng_i->GetFirstToOpen()");
                }

                // dec right_delta
                _ASSERT(right_delta >= seg_i->GetLength());
                if (right_delta < seg_i->GetLength()) {
                    NCBI_THROW(CAlnException, eInternalFailure,
                               "right_delta < seg_i->GetLength()");
                }
                right_delta -= seg_i->GetLength();

                (*diag_it)->SetStarts()[row] = 
                    (direct ?
                     aln_rng_i->GetSecondFrom() + left_delta :
                     aln_rng_i->GetSecondFrom() + right_delta);

                // inc left_delta
                left_delta += seg_i->GetLength();

                if (right_delta == 0) {
                    _ASSERT(left_delta == aln_rng_i->GetLength());
                    ++aln_rng_i;
                    if (aln_rng_i != pairwises[dim - row - 1]->end()) {
                        direct = aln_rng_i->IsDirect();
                        left_delta = 0;
                        right_delta = aln_rng_i->GetLength();
                    }
                }
            }
            (*diag_it)->SetStrands()[row] =
                (direct ? eNa_strand_plus : eNa_strand_minus);
            ++seg_i;
            ++seg;
            ++diag_it;
        }
    }
    // Cleanup: remove rows with gaps, remove one-row diags.
    NON_CONST_ITERATE(CSeq_align::TSegs::TDendiag, diag_it, diags) {
        size_t row = 0;
        CDense_diag& diag = **diag_it;
        CDense_diag::TStarts& starts = diag.SetStarts();
        while (row < starts.size()) {
            if (starts[row] == kInvalidSeqPos) {
                starts.erase(starts.begin() + row);
                CDense_diag::TIds& ids = diag.SetIds();
                ids.erase(ids.begin() + row);
                CDense_diag::TStrands& strands = diag.SetStrands();
                strands.erase(strands.begin() + row);
                continue;
            }
            ++row;
        }
        if (diag.GetStarts().size() < 2) continue;
        diag.SetDim(starts.size());
#if _DEBUG
        diag.Validate();
#endif
        dd.push_back(*diag_it);
    }
}


CRef<CDense_seg>
CreateDensegFromAnchoredAln(const CAnchoredAln& anchored_aln,
                            CScope* scope) 
{
    const CAnchoredAln::TPairwiseAlnVector& pairwises = anchored_aln.GetPairwiseAlns();

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
        SerialAssign<CSeq_id>(*ids[row], anchored_aln.GetId(dim - row - 1)->GetSeqId());
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
        CPairwiseAln::TAlnRngColl::const_iterator aln_rng_i = pairwises[dim - row - 1]->begin();
        bool direct = aln_rng_i->IsDirect();
        TSignedSeqPos left_delta = 0;
        TSignedSeqPos right_delta = aln_rng_i->GetLength();
        while (seg_i != anchor_segments.end()) {
            _ASSERT(seg < numseg);
            _ASSERT(matrix_row_pos == row + dim * seg);
            if (aln_rng_i != pairwises[dim - row - 1]->end()  &&
                seg_i->GetFrom() >= aln_rng_i->GetFirstFrom()) {
                _ASSERT(seg_i->GetToOpen() <= aln_rng_i->GetFirstToOpen());
                if (seg_i->GetToOpen() > aln_rng_i->GetFirstToOpen()) {
                    NCBI_THROW(CAlnException, eInternalFailure,
                               "seg_i->GetToOpen() > aln_rng_i->GetFirstToOpen()");
                }

                // dec right_delta
                _ASSERT(right_delta >= seg_i->GetLength());
                if (right_delta < seg_i->GetLength()) {
                    NCBI_THROW(CAlnException, eInternalFailure,
                               "right_delta < seg_i->GetLength()");
                }
                right_delta -= seg_i->GetLength();

                starts[matrix_row_pos] = 
                    (direct ?
                     aln_rng_i->GetSecondFrom() + left_delta :
                     aln_rng_i->GetSecondFrom() + right_delta);

                // inc left_delta
                left_delta += seg_i->GetLength();

                if (right_delta == 0) {
                    _ASSERT(left_delta == aln_rng_i->GetLength());
                    ++aln_rng_i;
                    if (aln_rng_i != pairwises[dim - row - 1]->end()) {
                        direct = aln_rng_i->IsDirect();
                        left_delta = 0;
                        right_delta = aln_rng_i->GetLength();
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
CreateDensegFromPairwiseAln(const CPairwiseAln& pairwise_aln,
                            CScope* scope)
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


static const TSignedSeqPos kMaxSplicedExonIndelLength = 15;

void InitSplicedsegFromPairwiseAln(CSpliced_seg& spliced_seg,
                                   const CPairwiseAln& pairwise_aln,
                                   CScope* scope)
{
    // Sort by product positions (if minus strand, then reverse).
    // Make sure genomic positions are sorted in the corresponding direction too, no overlaps etc.
    // Small one-row gap -> indel.
    // Large genomic gap -> intron (start new exon).
    // Other gaps -> intron, set 'partial' on both sides.
    // If the product does not start/end at the sequence extreme, set 'partial' for the exon.

    // Check strands - one per row.
    _ASSERT((pairwise_aln.GetFlags() & CPairwiseAln::fMixedDir) != CPairwiseAln::fMixedDir);
    // Product is nuc or prot.
    _ASSERT(pairwise_aln.GetFirstBaseWidth() == 1  ||
            pairwise_aln.GetFirstBaseWidth() == 3);
    bool prot = pairwise_aln.GetFirstBaseWidth() == 3;
    // The other row is genomic.
    _ASSERT(pairwise_aln.GetSecondBaseWidth() == 1);

    /// Ids
    CRef<CSeq_id> product_id(new CSeq_id);
    product_id->Assign(pairwise_aln.GetFirstId()->GetSeqId());
    spliced_seg.SetProduct_id(*product_id);
    CRef<CSeq_id> genomic_id(new CSeq_id);
    genomic_id->Assign(pairwise_aln.GetSecondId()->GetSeqId());
    spliced_seg.SetGenomic_id(*genomic_id);

    // Product type
    spliced_seg.SetProduct_type(prot ?
        CSpliced_seg::eProduct_type_protein
        : CSpliced_seg::eProduct_type_transcript);

    // Exons
    CSpliced_seg::TExons& exons = spliced_seg.SetExons();

    typedef TSignedSeqPos                  TPos;
    typedef CRange<TPos>                   TRng; 
    typedef CAlignRange<TPos>              TAlnRng;
    typedef CAlignRangeCollection<TAlnRng> TAlnRngColl;

    TPos last_prod_end = 0;
    TPos last_gen_end = 0;
    CRef<CSpliced_exon> exon;
    CPairwiseAln::TAlnRngColl::const_iterator rg_it = pairwise_aln.begin();
    bool gen_direct = rg_it == pairwise_aln.end() || rg_it->IsDirect();
    bool prod_direct = prot ||
        rg_it == pairwise_aln.end() || rg_it->IsFirstDirect();
    // Adjust genomic strand - in CPairwiseAln it's relative to the first seq.
    if ( !prod_direct ) {
        gen_direct = !gen_direct;
    }

    TRng ex_prod_rg;
    TRng ex_gen_rg;

    /// Main loop to set all values
    ITERATE(CPairwiseAln::TAlnRngColl, rg_it, pairwise_aln) {
        const CPairwiseAln::TAlnRng& rg = *rg_it;

        // Unaligned ranges.
        TPos prod_skip, gen_skip;
        if (rg_it == pairwise_aln.begin()) {
            prod_skip = 0;
            gen_skip = 0;
        }
        else {
            prod_skip = rg.GetFirstFrom() - last_prod_end;
            gen_skip = gen_direct == prod_direct ?
                rg.GetSecondFrom() - last_gen_end
                : last_gen_end - rg.GetSecondToOpen();
        }

        // Break exon, ignore long gaps between exons.
        if (!exon  ||  prod_skip > kMaxSplicedExonIndelLength  ||
            gen_skip > kMaxSplicedExonIndelLength) {
            if ( exon ) {
                _ASSERT(exon->IsSetProduct_start());
                _ASSERT(exon->IsSetGenomic_start());
                _ASSERT(exon->IsSetProduct_end());
                _ASSERT(exon->IsSetGenomic_end());
                if ( prod_direct ) {
                    exons.push_back(exon);
                }
                else {
                    exons.push_front(exon);
                }
                exon.Reset();
                ex_prod_rg = TRng::GetEmpty();
                ex_gen_rg = TRng::GetEmpty();
            }
            prod_skip = 0;
            gen_skip = 0;
        }

        if (prod_skip > 0  ||  gen_skip > 0) {
            _ASSERT(exon);
            TSeqPos mismatch = min(prod_skip, gen_skip);
            if (mismatch > 0) {
                CRef<CSpliced_exon_chunk> chunk(new CSpliced_exon_chunk);
                chunk->SetMismatch(mismatch);
                if ( prod_direct ) {
                    exon->SetParts().push_back(chunk);
                }
                else {
                    exon->SetParts().push_front(chunk);
                }
                prod_skip -= mismatch;
                gen_skip -= mismatch;
            }
            if (prod_skip > 0) {
                CRef<CSpliced_exon_chunk> chunk(new CSpliced_exon_chunk);
                chunk->SetProduct_ins(prod_skip);
                if ( prod_direct ) {
                    exon->SetParts().push_back(chunk);
                }
                else {
                    exon->SetParts().push_front(chunk);
                }
            }
            if (gen_skip > 0) {
                CRef<CSpliced_exon_chunk> chunk(new CSpliced_exon_chunk);
                chunk->SetGenomic_ins(gen_skip);
                if ( prod_direct ) {
                    exon->SetParts().push_back(chunk);
                }
                else {
                    exon->SetParts().push_front(chunk);
                }
            }
            prod_skip = 0;
            gen_skip = 0;
        }

        if ( !exon ) {
            // Start new exon
            exon.Reset(new CSpliced_exon);
            // The first exon must start at 0 or be partial.
            if (exons.empty()  &&  rg.GetFirstFrom() > 0) {
                exon->SetPartial(true);
            }
            if ( !prot ) {
                exon->SetProduct_strand(prod_direct
                    ? eNa_strand_plus : eNa_strand_minus);
            }
            exon->SetGenomic_strand(gen_direct
                ? eNa_strand_plus : eNa_strand_minus);
        }
        // Aligned chunk
        CRef<CSpliced_exon_chunk> chunk(new CSpliced_exon_chunk);
        chunk->SetMatch(rg.GetLength());
        if ( prod_direct ) {
            exon->SetParts().push_back(chunk);
        }
        else {
            exon->SetParts().push_front(chunk);
        }

        // Update exon extremes
        ex_prod_rg.CombineWith(TRng(rg.GetFirstFrom(), rg.GetFirstTo()));
        ex_gen_rg.CombineWith(TRng(rg.GetSecondFrom(), rg.GetSecondTo()));
        if ( exon ) {
            if (prot) {
                exon->SetProduct_start().SetProtpos().SetAmin(ex_prod_rg.GetFrom() / 3);
                exon->SetProduct_start().SetProtpos().SetFrame(ex_prod_rg.GetFrom() % 3 + 1);
                exon->SetProduct_end().SetProtpos().SetAmin(ex_prod_rg.GetTo() / 3);
                exon->SetProduct_end().SetProtpos().SetFrame(ex_prod_rg.GetTo() % 3 + 1);
            } else {
                exon->SetProduct_start().SetNucpos(ex_prod_rg.GetFrom());
                exon->SetProduct_end().SetNucpos(ex_prod_rg.GetTo());
            }
            exon->SetGenomic_start(ex_gen_rg.GetFrom());
            exon->SetGenomic_end(ex_gen_rg.GetTo());
        }

        last_prod_end = rg.GetFirstToOpen();
        last_gen_end = gen_direct == prod_direct ?
            rg.GetSecondToOpen() : rg.GetSecondFrom();
    }
    if ( exon ) {
        _ASSERT(exon->IsSetProduct_start());
        _ASSERT(exon->IsSetGenomic_start());
        _ASSERT(exon->IsSetProduct_end());
        _ASSERT(exon->IsSetGenomic_end());
        if ( prod_direct ) {
            exons.push_back(exon);
        }
        else {
            exons.push_front(exon);
        }
    }
    else if ( !exons.empty() ) {
        // Get the last added exon.
        exon = prod_direct ? exons.front() : exons.back();
    }

    // Check if the last exon ends at product end.
    if (exon  &&  scope) {
        TSeqPos prod_end = 0;
        if ( exon->GetProduct_end().IsNucpos() ) {
            prod_end = exon->GetProduct_end().GetNucpos();
        }
        else {
            prod_end = exon->GetProduct_end().GetProtpos().GetAmin();
        }
        CBioseq_Handle h = scope->GetBioseqHandle(*product_id);
        if ( h ) {
            TSeqPos prod_len = h.GetBioseqLength();
            if (prod_len != kInvalidSeqPos  &&  prod_len != prod_end + 1) {
                exon->SetPartial(true);
            }
        }
    }

#ifdef _DEBUG
    spliced_seg.Validate(true);
#endif
}


CRef<CSpliced_seg>
CreateSplicedsegFromAnchoredAln(const CAnchoredAln& anchored_aln,
                                CScope* scope)
{
    _ASSERT(anchored_aln.GetDim() == 2);

    // Sort by product positions (if minus strand, then reverse).
    // Make sure genomic positions are sorted in the corresponding direction too, no overlaps etc.
    // Small one-row gap -> indel.
    // Large genomic gap -> intron (start new exon).
    // Other gaps -> intron, set 'partial' on both sides.
    // If the product does not start/end at the sequence extreme, set 'partial' for the exon.

    // Create a spliced_seg
    CRef<CSpliced_seg> spliced_seg(new CSpliced_seg);
    CAnchoredAln::TDim anchor_row = anchored_aln.GetAnchorRow();
    const CPairwiseAln& pairwise = *anchored_aln.GetPairwiseAlns()[1 - anchor_row];
    InitSplicedsegFromPairwiseAln(*spliced_seg, pairwise, scope);
    return spliced_seg;
}


CRef<CSpliced_seg>
CreateSplicedsegFromPairwiseAln(const CPairwiseAln& pairwise_aln,
                                CScope* scope)
{
    /// Create a dense-seg
    CRef<CSpliced_seg> ss(new CSpliced_seg);
    InitSplicedsegFromPairwiseAln(*ss, pairwise_aln, scope);
    return ss;
}


void
s_TranslatePairwise(CPairwiseAln& out_pw,   ///< output pairwise (needs to be empty)
                    const CPairwiseAln& pw, ///< input pairwise to translate from
                    const CPairwiseAln& tr) ///< translating pairwise
{
    ITERATE (CPairwiseAln, it, pw) {
        CPairwiseAln::TAlnRng ar = *it;
        ar.SetFirstFrom(tr.GetSecondPosByFirstPos(ar.GetFirstFrom()));
        out_pw.insert(ar);
    }
}


typedef CAnchoredAln::TDim TDim;


void
CreateSeqAlignFromEachPairwiseAln
(const CAnchoredAln::TPairwiseAlnVector pairwises, ///< input
 TDim anchor,                                      ///< choice of anchor
 vector<CRef<CSeq_align> >& out_seqaligns,         ///< output
 CSeq_align::TSegs::E_Choice choice,      ///< choice of alignment 'segs'
 CScope* scope)
{
    out_seqaligns.resize(pairwises.size() - 1);
    for (TDim row = 0, sa_idx = 0;
         row < (TDim) pairwises.size();
         ++row, ++sa_idx) {
        if (row != anchor) {
            CRef<CSeq_align> sa(new CSeq_align);
            sa->SetType(CSeq_align::eType_partial);
            sa->SetDim(2);

            const CPairwiseAln& pw = *pairwises[row];
            CRef<CPairwiseAln> p(new CPairwiseAln(pairwises[anchor]->GetSecondId(),
                                                  pw.GetSecondId(),
                                                  pw.GetFlags()));
            s_TranslatePairwise(*p, pw, *pairwises[anchor]);

            switch(choice)    {
            case CSeq_align::TSegs::e_Denseg: {
                CRef<CDense_seg> ds = CreateDensegFromPairwiseAln(*p, scope);
                sa->SetSegs().SetDenseg(*ds);
                break;
            }
            case CSeq_align::TSegs::e_Disc: {
                CRef<CSeq_align_set> disc = CreateAlignSetFromPairwiseAln(*p, scope);
                sa->SetSegs().SetDisc(*disc);
                break;
            }
            case CSeq_align::TSegs::e_Packed: {
                CRef<CPacked_seg> ps = CreatePackedsegFromPairwiseAln(*p, scope);
                sa->SetSegs().SetPacked(*ps);
                break;
            }
            case CSeq_align::TSegs::e_Spliced: {
                CRef<CSpliced_seg> ss = CreateSplicedsegFromPairwiseAln(*p, scope);
                sa->SetSegs().SetSpliced(*ss);
                break;
            }
            case CSeq_align::TSegs::e_Dendiag:
            case CSeq_align::TSegs::e_Std:
            case CSeq_align::TSegs::e_Sparse:
                NCBI_THROW(CAlnException, eInvalidRequest,
                           "Unsupported CSeq_align::TSegs type.");
            case CSeq_align::TSegs::e_not_set:
            default:
                NCBI_THROW(CAlnException, eInvalidRequest,
                           "Invalid CSeq_align::TSegs type.");
            }
            out_seqaligns[sa_idx].Reset(sa);
        }
    }
}


CRef<CSeq_align_set>
CreateAlignSetFromAnchoredAln(const CAnchoredAln& anchored_aln,
                              CScope* scope)
{
    CRef<CSeq_align_set> disc(new CSeq_align_set);

    const CAnchoredAln::TPairwiseAlnVector& pairwises = anchored_aln.GetPairwiseAlns();

    typedef CSegmentedRangeCollection TAnchorSegments;
    TAnchorSegments anchor_segments;
    ITERATE(CAnchoredAln::TPairwiseAlnVector, pairwise_aln_i, pairwises) {
        ITERATE (CPairwiseAln::TAlnRngColl, rng_i, **pairwise_aln_i) {
            anchor_segments.insert(CPairwiseAln::TRng(rng_i->GetFirstFrom(), rng_i->GetFirstTo()));
        }
    }

    CDense_seg::TDim row;
    CDense_seg::TNumseg seg;

    // Determine dimensions
    CDense_seg::TNumseg numseg = anchor_segments.size();
    CDense_seg::TDim dim = anchored_aln.GetDim();

    vector< CRef<CDense_seg> > dsegs;
    dsegs.resize(numseg);
    for (size_t i = 0; i < dsegs.size(); ++i) {
        dsegs[i].Reset(new CDense_seg);
        CDense_seg& dseg = *dsegs[i];
        dseg.SetDim(dim);
        dseg.SetNumseg(1);
        // Ids
        CDense_seg::TIds& ids = dseg.SetIds();
        ids.resize(dim);
        for (row = 0;  row < dim;  ++row) {
            ids[row].Reset(new CSeq_id);
            SerialAssign<CSeq_id>(*ids[row], anchored_aln.GetId(dim - row - 1)->GetSeqId());
        }
        // Lens, strands, starts - just prepare the storage
        dseg.SetLens().resize(1);
        dseg.SetStrands().resize(dim, eNa_strand_unknown);
        dseg.SetStarts().resize(dim, -1);
    }

    for (row = 0;  row < dim;  ++row) {
        seg = 0;
        TAnchorSegments::const_iterator seg_i = anchor_segments.begin();
        CPairwiseAln::TAlnRngColl::const_iterator aln_rng_i = pairwises[dim - row - 1]->begin();
        bool direct = aln_rng_i->IsDirect();
        TSignedSeqPos left_delta = 0;
        TSignedSeqPos right_delta = aln_rng_i->GetLength();
        while (seg_i != anchor_segments.end()) {
            _ASSERT(seg < numseg);
            CDense_seg& dseg = *dsegs[seg];
            dseg.SetLens()[0] = seg_i->GetLength();
            CDense_seg::TStarts& starts = dseg.SetStarts();

            dseg.SetStrands()[row] = (direct ? eNa_strand_plus : eNa_strand_minus);
            if (aln_rng_i != pairwises[dim - row - 1]->end()  &&
                seg_i->GetFrom() >= aln_rng_i->GetFirstFrom()) {
                _ASSERT(seg_i->GetToOpen() <= aln_rng_i->GetFirstToOpen());
                if (seg_i->GetToOpen() > aln_rng_i->GetFirstToOpen()) {
                    NCBI_THROW(CAlnException, eInternalFailure,
                               "seg_i->GetToOpen() > aln_rng_i->GetFirstToOpen()");
                }

                // dec right_delta
                _ASSERT(right_delta >= seg_i->GetLength());
                if (right_delta < seg_i->GetLength()) {
                    NCBI_THROW(CAlnException, eInternalFailure,
                               "right_delta < seg_i->GetLength()");
                }
                right_delta -= seg_i->GetLength();

                starts[row] = 
                    (direct ?
                     aln_rng_i->GetSecondFrom() + left_delta :
                     aln_rng_i->GetSecondFrom() + right_delta);

                // inc left_delta
                left_delta += seg_i->GetLength();

                if (right_delta == 0) {
                    _ASSERT(left_delta == aln_rng_i->GetLength());
                    ++aln_rng_i;
                    if (aln_rng_i != pairwises[dim - row - 1]->end()) {
                        direct = aln_rng_i->IsDirect();
                        left_delta = 0;
                        right_delta = aln_rng_i->GetLength();
                    }
                }
            }
            // Add only densegs with both rows non-empty
            if (starts[0] >= 0  &&  starts[1] >= 0) {
                CRef<CSeq_align> seg_aln(new CSeq_align);
                seg_aln->SetType(CSeq_align::eType_not_set);
                seg_aln->SetDim(dim);
                disc->Set().push_back(seg_aln);
                seg_aln->SetSegs().SetDenseg(dseg);
            }
            ++seg_i;
            ++seg;
        }
    }

    return disc;
}


CRef<CSeq_align_set>
CreateAlignSetFromPairwiseAln(const CPairwiseAln& pairwise_aln,
                              CScope* scope)
{
    CRef<CSeq_align_set> disc(new CSeq_align_set);

    CDense_seg::TNumseg numseg = pairwise_aln.size();

    vector< CRef<CDense_seg> > dsegs;
    dsegs.resize(numseg);
    for (size_t i = 0; i < dsegs.size(); ++i) {
        CRef<CSeq_align> seg_aln(new CSeq_align);
        seg_aln->SetType(CSeq_align::eType_not_set);
        seg_aln->SetDim(2);
        disc->Set().push_back(seg_aln);
        CDense_seg& dseg = seg_aln->SetSegs().SetDenseg();
        dsegs[i].Reset(&dseg);
        dseg.SetDim(2);
        dseg.SetNumseg(1);
        // Ids
        CDense_seg::TIds& ids = dseg.SetIds();
        ids.resize(2);
        ids[0].Reset(new CSeq_id);
        SerialAssign<CSeq_id>(*ids[0], pairwise_aln.GetFirstId()->GetSeqId());
        ids[1].Reset(new CSeq_id);
        SerialAssign<CSeq_id>(*ids[1], pairwise_aln.GetSecondId()->GetSeqId());
        // Lens, strands, starts - just prepare the storage
        dseg.SetLens().resize(1);
        dseg.SetStrands().resize(2, eNa_strand_unknown);
        dseg.SetStarts().resize(2, -1);
    }

    // Main loop to set all values
    CDense_seg::TNumseg seg = 0;
    ITERATE(CPairwiseAln::TAlnRngColl, aln_rng_i, pairwise_aln) {
        CDense_seg& dseg = *dsegs[seg];
        dseg.SetStarts()[0] = aln_rng_i->GetFirstFrom();
        if ( !aln_rng_i->IsDirect() ) {
            if ( !dseg.IsSetStrands() ) {
                dseg.SetStrands().resize(2, eNa_strand_plus);
            }
            dseg.SetStrands()[1] = eNa_strand_minus;
        }
        dseg.SetStarts()[1] = aln_rng_i->GetSecondFrom();
        dseg.SetLens()[0] = aln_rng_i->GetLength();
        seg++;
    }

    return disc;
}


CRef<CPacked_seg>
CreatePackedsegFromAnchoredAln(const CAnchoredAln& anchored_aln,
                               CScope* scope) 
{
    const CAnchoredAln::TPairwiseAlnVector& pairwises = anchored_aln.GetPairwiseAlns();

    typedef CSegmentedRangeCollection TAnchorSegments;
    TAnchorSegments anchor_segments;
    ITERATE(CAnchoredAln::TPairwiseAlnVector, pairwise_aln_i, pairwises) {
        ITERATE (CPairwiseAln::TAlnRngColl, rng_i, **pairwise_aln_i) {
            anchor_segments.insert(CPairwiseAln::TRng(rng_i->GetFirstFrom(), rng_i->GetFirstTo()));
        }
    }

    // Create a packed-seg
    CRef<CPacked_seg> ps(new CPacked_seg);

    // Determine dimensions
    CPacked_seg::TNumseg& numseg = ps->SetNumseg();
    numseg = anchor_segments.size();
    CPacked_seg::TDim& dim = ps->SetDim();
    dim = anchored_aln.GetDim();

    // Tmp vars
    CPacked_seg::TDim row;
    CPacked_seg::TNumseg seg;

    // Ids
    CPacked_seg::TIds& ids = ps->SetIds();
    ids.resize(dim);
    for (row = 0;  row < dim;  ++row) {
        ids[row].Reset(new CSeq_id);
        SerialAssign<CSeq_id>(*ids[row], anchored_aln.GetId(dim - row - 1)->GetSeqId());
    }

    // Lens
    CPacked_seg::TLens& lens = ps->SetLens();
    lens.resize(numseg);
    TAnchorSegments::const_iterator seg_i = anchor_segments.begin();
    for (seg = 0; seg < numseg; ++seg, ++seg_i) {
        lens[seg] = seg_i->GetLength();
    }

    int matrix_size = dim * numseg;

    // Present
    CPacked_seg::TPresent& present = ps->SetPresent();
    present.resize(matrix_size);

    // Strands (just resize, will set while setting starts)
    CPacked_seg::TStrands& strands = ps->SetStrands();
    strands.resize(matrix_size, eNa_strand_unknown);

    // Starts and strands
    CPacked_seg::TStarts& starts = ps->SetStarts();
    starts.resize(matrix_size, 0);
    for (row = 0;  row < dim;  ++row) {
        seg = 0;
        int matrix_row_pos = row;  // optimization to eliminate multiplication
        seg_i = anchor_segments.begin();
        CPairwiseAln::TAlnRngColl::const_iterator aln_rng_i = pairwises[dim - row - 1]->begin();
        bool direct = aln_rng_i->IsDirect();
        TSignedSeqPos left_delta = 0;
        TSignedSeqPos right_delta = aln_rng_i->GetLength();
        while (seg_i != anchor_segments.end()) {
            _ASSERT(seg < numseg);
            _ASSERT(matrix_row_pos == row + dim * seg);
            strands[matrix_row_pos] = (direct ? eNa_strand_plus : eNa_strand_minus);
            if (aln_rng_i != pairwises[dim - row - 1]->end()  &&
                seg_i->GetFrom() >= aln_rng_i->GetFirstFrom()) {
                _ASSERT(seg_i->GetToOpen() <= aln_rng_i->GetFirstToOpen());
                if (seg_i->GetToOpen() > aln_rng_i->GetFirstToOpen()) {
                    NCBI_THROW(CAlnException, eInternalFailure,
                               "seg_i->GetToOpen() > aln_rng_i->GetFirstToOpen()");
                }

                // dec right_delta
                _ASSERT(right_delta >= seg_i->GetLength());
                if (right_delta < seg_i->GetLength()) {
                    NCBI_THROW(CAlnException, eInternalFailure,
                               "right_delta < seg_i->GetLength()");
                }
                right_delta -= seg_i->GetLength();

                CPacked_seg::TStarts::value_type start = (direct ?
                    aln_rng_i->GetSecondFrom() + left_delta
                    : aln_rng_i->GetSecondFrom() + right_delta);
                starts[matrix_row_pos] = start;

                present[matrix_row_pos] = (start != kInvalidSeqPos);

                // inc left_delta
                left_delta += seg_i->GetLength();

                if (right_delta == 0) {
                    _ASSERT(left_delta == aln_rng_i->GetLength());
                    ++aln_rng_i;
                    if (aln_rng_i != pairwises[dim - row - 1]->end()) {
                        direct = aln_rng_i->IsDirect();
                        left_delta = 0;
                        right_delta = aln_rng_i->GetLength();
                    }
                }
            }
            ++seg_i;
            ++seg;
            matrix_row_pos += dim;
        }
    }
    return ps;
}


CRef<CPacked_seg>
CreatePackedsegFromPairwiseAln(const CPairwiseAln& pairwise_aln,
                               CScope* scope)
{
    // Create a packed-seg
    CRef<CPacked_seg> ps(new CPacked_seg);


    // Determine dimensions
    CPacked_seg::TNumseg& numseg = ps->SetNumseg();
    numseg = pairwise_aln.size();
    ps->SetDim(2);
    int matrix_size = 2 * numseg;

    CPacked_seg::TLens& lens = ps->SetLens();
    lens.resize(numseg);

    CPacked_seg::TStarts& starts = ps->SetStarts();
    starts.resize(matrix_size, 0);

    CPacked_seg::TPresent& present = ps->SetPresent();
    present.resize(matrix_size, 0);

    CPacked_seg::TIds& ids = ps->SetIds();
    ids.resize(2);

    // Ids
    ids[0].Reset(new CSeq_id);
    SerialAssign<CSeq_id>(*ids[0], pairwise_aln.GetFirstId()->GetSeqId());
    ids[1].Reset(new CSeq_id);
    SerialAssign<CSeq_id>(*ids[1], pairwise_aln.GetSecondId()->GetSeqId());


    // Tmp vars
    CPacked_seg::TNumseg seg = 0;
    int matrix_pos = 0;

    // Main loop to set all values
    ITERATE(CPairwiseAln::TAlnRngColl, aln_rng_i, pairwise_aln) {
        CPacked_seg::TStarts::value_type start = aln_rng_i->GetFirstFrom();
        present[matrix_pos] = (start != kInvalidSeqPos);
        starts[matrix_pos++] = start;
        if ( !aln_rng_i->IsDirect() ) {
            if ( !ps->IsSetStrands() ) {
                ps->SetStrands().resize(matrix_size, eNa_strand_plus);
            }
            ps->SetStrands()[matrix_pos] = eNa_strand_minus;
        }
        start = aln_rng_i->GetSecondFrom();
        present[matrix_pos] = (start != kInvalidSeqPos);
        starts[matrix_pos++] = start;
        lens[seg++] = aln_rng_i->GetLength();
    }
    _ASSERT(matrix_pos == matrix_size);
    _ASSERT(seg == numseg);

    return ps;
}


CRef<CSeq_align>
ConvertSeq_align(const CSeq_align& src,
                 CSeq_align::TSegs::E_Choice dst_choice,
                 CSeq_align::TDim anchor_row,
                 CScope* scope)
{
    TScopeAlnSeqIdConverter id_conv(scope);
    TScopeIdExtract id_extract(id_conv);
    TScopeAlnIdMap aln_id_map(id_extract, 1);
    TAlnSeqIdVec ids;
    id_extract(src, ids);
    aln_id_map.push_back(src);

    TScopeAlnStats aln_stats(aln_id_map);
    CAlnUserOptions aln_user_options;
    CRef<CAnchoredAln> anchored_aln =
        CreateAnchoredAlnFromAln(aln_stats, 0, aln_user_options, anchor_row);

    return CreateSeqAlignFromAnchoredAln(*anchored_aln, dst_choice, scope);
}


END_NCBI_SCOPE
