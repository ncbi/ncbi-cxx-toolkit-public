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

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objtools/alnmgr/aln_generators.hpp>
#include <objtools/alnmgr/alnexception.hpp>

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


CRef<CDense_seg>
CreateDensegFromAnchoredAln(const CAnchoredAln& anchored_aln) 
{
    const CAnchoredAln::TPairwiseAlnVector& pairwises = anchored_aln.GetPairwiseAlns();

    /// Extract all anchor starts
    set<CPairwiseAln::TPos> anchor_starts_set;
    ITERATE(CAnchoredAln::TPairwiseAlnVector, pairwise_aln_i, pairwises) {
        ITERATE (CPairwiseAln::TAlnRngColl, rng_i, **pairwise_aln_i) {
            anchor_starts_set.insert(rng_i->GetFirstFrom());
        }
    }
    vector<CPairwiseAln::TPos> anchor_starts(anchor_starts_set.size());
    copy(anchor_starts_set.begin(), anchor_starts_set.end(), anchor_starts.begin());
    anchor_starts_set.clear();

    /// Create a dense-seg
    CRef<CDense_seg> ds(new CDense_seg);

    /// Determine dimensions
    CDense_seg::TNumseg& numseg = ds->SetNumseg();
    numseg = anchor_starts.size();
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
    CPairwiseAln::TAlnRngColl::const_iterator 
        aln_rng_i = pairwises[anchored_aln.GetAnchorRow()]->begin();
    for (seg = 0;  seg < numseg;  ++seg) {
        if (seg == numseg - 1  || // last or...
            aln_rng_i->GetFirstToOpen() < anchor_starts[seg+1]) { // ...unaligned in-between

            lens[seg] = aln_rng_i->GetFirstToOpen() - anchor_starts[seg];
            ++aln_rng_i;
            _ASSERT(seg < numseg - 1  ||  aln_rng_i == pairwises[anchored_aln.GetAnchorRow()]->end());

        } else {
            lens[seg] = anchor_starts[seg+1] - anchor_starts[seg];
            if (aln_rng_i->GetFirstToOpen() == anchor_starts[seg+1]) {
                ++aln_rng_i;
            }
        }
    }

    int num = dim * numseg;

    /// Strands (just resize, will set while setting starts)
    CDense_seg::TStrands& strands = ds->SetStrands();
    strands.resize(num, eNa_strand_minus);

    /// Starts
    CDense_seg::TStarts& starts = ds->SetStarts();
    starts.resize(num, -1);
    for (row = 0;  row < dim;  ++row) {
        CDense_seg::TNumseg seg = 0;
        CPairwiseAln::TAlnRngColl::const_iterator aln_rng_i = pairwises[row]->begin();
        while (seg < numseg  &&  aln_rng_i != pairwises[row]->end()) {
            while (anchor_starts[seg] < aln_rng_i->GetFirstFrom()) {
                ++seg;
                _ASSERT(seg < numseg);
            }
            bool direct = aln_rng_i->IsDirect();
            CPairwiseAln::TPos start = 
                (direct ?
                 aln_rng_i->GetSecondFrom() :
                 aln_rng_i->GetSecondToOpen() - lens[seg]);
            while (seg < numseg  &&  anchor_starts[seg] < aln_rng_i->GetFirstToOpen()) {
                _ASSERT(row + seg * dim < num);
                starts[row + seg * dim] = start;
                if (direct) {
                    strands[row + seg * dim] = eNa_strand_plus;
                }
                if (direct) {
                    start += lens[seg];
                }
                ++seg;
                if ( !direct  &&  seg < numseg) {
                    start -= lens[seg];
                }
            }
            ++aln_rng_i;
        }
    }

    return ds;
}


END_NCBI_SCOPE
