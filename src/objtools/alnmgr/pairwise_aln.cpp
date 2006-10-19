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
*   Collection of diagonal alignment ranges
*
* ===========================================================================
*/


#include <ncbi_pch.hpp>

#include <objtools/alnmgr/pairwise_aln.hpp>

#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

#include <objects/seqloc/Seq_id.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(ncbi::objects);


CPairwiseAln::CPairwiseAln(const CSeq_align& sa,
                           TDim row_1,
                           TDim row_2) :
    CDiagRngColl(),
    m_SeqAlign(&sa),
    m_Row1(row_1),
    m_Row2(row_2)
{
    typedef CSeq_align::TSegs TSegs;
    const TSegs& segs = sa.GetSegs();

    switch(segs.Which())    {
    case CSeq_align::TSegs::e_Dendiag:
        break;
    case CSeq_align::TSegs::e_Denseg: {
        x_BuildFromDenseg();
        break;
    }
    case CSeq_align::TSegs::e_Std:
        x_BuildFromStdseg();
        break;
    case CSeq_align::TSegs::e_Packed:
        break;
    case CSeq_align::TSegs::e_Disc:
        ITERATE(CSeq_align_set::Tdata, sa_it, segs.GetDisc().Get()) {
            CPairwiseAln(**sa_it, row_1, row_2);
        }
        break;
//     case CSeq_align::TSegs::e_Spliced:
//     case CSeq_align::TSegs::e_Sparse:
    case CSeq_align::TSegs::e_not_set:
        break;
    }
}


void 
CPairwiseAln::x_BuildFromDenseg()
{
    const CDense_seg& ds = m_SeqAlign->GetSegs().GetDenseg();

    _ASSERT(m_Row1 >=0  &&  m_Row1 < ds.GetDim());
    _ASSERT(m_Row2 >=0  &&  m_Row2 < ds.GetDim());

    const CDense_seg::TNumseg numseg = ds.GetNumseg();
    const CDense_seg::TDim dim = ds.GetDim();
    const CDense_seg::TStarts& starts = ds.GetStarts();
    const CDense_seg::TLens& lens = ds.GetLens();
    const CDense_seg::TStrands* strands = 
        ds.IsSetStrands() ? &ds.GetStrands() : NULL;

    CDense_seg::TNumseg seg;
    int pos_1, pos_2;
    for (seg = 0, pos_1 = m_Row1, pos_2 = m_Row2;
         seg < numseg;
         ++seg, pos_1 += dim, pos_2 += dim) {
        const TSeqPos& from_1 = starts[pos_1];
        const TSeqPos& from_2 = starts[pos_2];

        /// if not a gap
        if (from_1 >= 0  &&  from_2 >= 0)  {
            bool direct = true;
            if (strands) {
                bool minus_1 = (*strands)[pos_1] == eNa_strand_minus;
                bool minus_2 = (*strands)[pos_2] == eNa_strand_minus;
                direct = minus_1 == minus_2;
            }
            insert(TAlnRng(from_1, from_2, lens[seg], direct));

            /// update range
            if (empty()) {
                m_Rng.SetFrom(from_1);
                m_Rng.SetLength(lens[seg]);
            } else {
                m_Rng.SetFrom(min(m_Rng.GetFrom(), from_1));
                m_Rng.SetToOpen(max(m_Rng.GetToOpen(), from_1 + lens[seg]));
            }
        }
    }

    _ASSERT( !(GetFlags() & fInvalid) );
}


void
CPairwiseAln::x_BuildFromStdseg() 
{
    ITERATE (CSeq_align::TSegs::TStd, 
             std_i,
             m_SeqAlign->GetSegs().GetStd()) {
        *std_i;
    }
}


END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2006/10/19 20:19:51  todorov
* Initial revision.
*
* ===========================================================================
*/
