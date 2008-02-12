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
* Author:  Eugene Vasilchenko
*
* File Description:
*   Utility class for collecting ranges of sequences
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objmgr/split/id_range.hpp>
#include <objmgr/error_codes.hpp>

#include <objects/seqloc/seqloc__.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Packed_seg.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Sparse_seg.hpp>
#include <objects/seqalign/Sparse_align.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Prot_pos.hpp>
#include <objects/seqres/Seq_graph.hpp>


#define NCBI_USE_ERRCODE_X   ObjMgr_IdRange

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


void COneSeqRange::Add(const COneSeqRange& range)
{
    Add(range.GetTotalRange());
}


void COneSeqRange::Add(const TRange& range)
{
    m_TotalRange += range;
}


void COneSeqRange::Add(TSeqPos start, TSeqPos stop_exclusive)
{
    Add(COpenRange<TSeqPos>(start, stop_exclusive));
}


CSeqsRange::CSeqsRange(void)
{
}


CSeqsRange::~CSeqsRange(void)
{
}


CNcbiOstream& CSeqsRange::Print(CNcbiOstream& out) const
{
    ITERATE ( TRanges, it, m_Ranges ) {
        if ( it != m_Ranges.begin() ) {
            out << ',';
        }
        TRange range = it->second.GetTotalRange();
        out << it->first.AsString();
        if ( range != TRange::GetWhole() ) {
            out << "(" << range.GetFrom() << "-" << range.GetTo() << ")";
        }
    }
    return out;
}


CSeq_id_Handle CSeqsRange::GetSingleId(void) const
{
    CSeq_id_Handle ret;
    if ( m_Ranges.size() == 1 ) {
        ret = m_Ranges.begin()->first;
    }
    return ret;
}


void CSeqsRange::Add(const CSeq_id_Handle& id, const COneSeqRange& loc)
{
    m_Ranges[id].Add(loc);
}


void CSeqsRange::Add(const CSeq_id_Handle& id, const TRange& range)
{
    m_Ranges[id].Add(range);
}


void CSeqsRange::Add(const CSeqsRange& range)
{
    ITERATE ( CSeqsRange, it, range ) {
        m_Ranges[it->first].Add(it->second);
    }
}


void CSeqsRange::Add(const CSeq_loc& loc)
{
    switch ( loc.Which() ) {
    case CSeq_loc::e_Whole:
        Add(loc.GetWhole());
        break;
    case CSeq_loc::e_Int:
        Add(loc.GetInt());
        break;
    case CSeq_loc::e_Pnt:
        Add(loc.GetPnt());
        break;
    case CSeq_loc::e_Packed_int:
        ITERATE( CPacked_seqint::Tdata, ii, loc.GetPacked_int().Get() ) {
            Add(**ii);
        }
        break;
    case CSeq_loc::e_Packed_pnt:
        Add(loc.GetPacked_pnt());
        break;
    case CSeq_loc::e_Mix:
        // extract sub-locations
        ITERATE ( CSeq_loc_mix::Tdata, li, loc.GetMix().Get() ) {
            Add(**li);
        }
        break;
    case CSeq_loc::e_Equiv:
        // extract sub-locations
        ITERATE ( CSeq_loc_equiv::Tdata, li, loc.GetEquiv().Get() ) {
            Add(**li);
        }
        break;
    case CSeq_loc::e_Bond:
        Add(loc.GetBond().GetA());
        if ( loc.GetBond().IsSetB() ) {
            Add(loc.GetBond().GetB());
        }
        break;
    default:
        break;
    }
}


void CSeqsRange::Add(const CSeq_id& id)
{
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(id);
    m_Ranges[idh].Add(TRange::GetWhole());
}


void CSeqsRange::Add(const CSeq_point& p)
{
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(p.GetId());
    m_Ranges[idh].Add(p.GetPoint(), p.GetPoint()+1);
}


void CSeqsRange::Add(const CSeq_interval& i)
{
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(i.GetId());
    m_Ranges[idh].Add(i.GetFrom(), i.GetTo()+1);
}


void CSeqsRange::Add(const CPacked_seqpnt& pp)
{
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(pp.GetId());
    COneSeqRange& range = m_Ranges[idh];
    ITERATE ( CPacked_seqpnt::TPoints, pi, pp.GetPoints() ) {
        range.Add(*pi, *pi+1);
    }
}


void CSeqsRange::Add(const CSeq_feat& obj)
{
    Add(obj.GetLocation());
    if ( obj.IsSetProduct() ) {
        Add(obj.GetProduct());
    }
}


void CSeqsRange::Add(const CSeq_align& obj)
{
    const CSeq_align::C_Segs& segs = obj.GetSegs();
    switch ( segs.Which() ) {
    case CSeq_align::C_Segs::e_Dendiag:
        ITERATE ( CSeq_align::C_Segs::TDendiag, it, segs.GetDendiag() ) {
            Add(**it);
        }
        break;
    case CSeq_align::C_Segs::e_Denseg:
        Add(segs.GetDenseg());
        break;
    case CSeq_align::C_Segs::e_Std:
        ITERATE ( CSeq_align::C_Segs::TStd, it, segs.GetStd() ) {
            ITERATE ( CStd_seg::TLoc, it_loc, (*it)->GetLoc() ) {
                Add(**it_loc);
            }
        }
        break;
    case CSeq_align::C_Segs::e_Packed:
        Add(segs.GetPacked());
        break;
    case CSeq_align::C_Segs::e_Disc:
        ITERATE ( CSeq_align_set::Tdata, it, segs.GetDisc().Get() ) {
            Add(**it);
        }
        break;
    case CSeq_align::C_Segs::e_Spliced:
        Add(segs.GetSpliced());
        break;
    default:
        break;
    }
}


void CSeqsRange::Add(const CDense_seg& denseg)
{
    size_t dim    = denseg.GetDim();
    size_t numseg = denseg.GetNumseg();
    // claimed dimension may not be accurate :-/
    if ( numseg != denseg.GetLens().size() ) {
        ERR_POST_X(1, Warning << "Invalid 'lens' size in denseg");
        numseg = min(numseg, denseg.GetLens().size());
    }
    if ( dim != denseg.GetIds().size() ) {
        ERR_POST_X(2, Warning << "Invalid 'ids' size in denseg");
        dim = min(dim, denseg.GetIds().size());
    }
    if ( dim*numseg != denseg.GetStarts().size() ) {
        ERR_POST_X(3, Warning << "Invalid 'starts' size in denseg");
        dim = min(dim*numseg, denseg.GetStarts().size()) / numseg;
    }
    CDense_seg::TStarts::const_iterator it_start = denseg.GetStarts().begin();
    CDense_seg::TLens::const_iterator it_len = denseg.GetLens().begin();
    for ( size_t seg = 0;  seg < numseg;  seg++, ++it_len) {
        CDense_seg::TIds::const_iterator it_id = denseg.GetIds().begin();
        for ( size_t seq = 0;  seq < dim;  seq++, ++it_start, ++it_id) {
            if ( *it_start >= 0 ) {
                CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(**it_id);
                m_Ranges[idh].Add(*it_start, *it_start + *it_len);
            }
        }
    }
}


void CSeqsRange::Add(const CDense_diag& diag)
{
    size_t dim = diag.GetDim();
    if ( dim != diag.GetIds().size() ) {
        ERR_POST_X(4, Warning << "Invalid 'ids' size in dendiag");
        dim = min(dim, diag.GetIds().size());
    }
    if ( dim != diag.GetStarts().size() ) {
        ERR_POST_X(5, Warning << "Invalid 'starts' size in dendiag");
        dim = min(dim, diag.GetStarts().size());
    }
    TSeqPos len = diag.GetLen();
    CDense_diag::TStarts::const_iterator it_start = diag.GetStarts().begin();
    ITERATE ( CDense_diag::TIds, it_id, diag.GetIds() ) {
        CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(**it_id);
        m_Ranges[idh].Add(*it_start, *it_start + len);
        ++it_start;
    }
}


void CSeqsRange::Add(const CPacked_seg& packed)
{
    size_t dim    = packed.GetDim();
    size_t numseg = packed.GetNumseg();
    // claimed dimension may not be accurate :-/
    if ( dim * numseg > packed.GetStarts().size() ) {
        dim = packed.GetStarts().size() / numseg;
    }
    if ( dim * numseg > packed.GetPresent().size() ) {
        dim = packed.GetPresent().size() / numseg;
    }
    if ( dim > packed.GetLens().size() ) {
        dim = packed.GetLens().size();
    }
    CPacked_seg::TStarts::const_iterator it_start = packed.GetStarts().begin();
    CPacked_seg::TLens::const_iterator it_len = packed.GetLens().begin();
    CPacked_seg::TPresent::const_iterator it_pres= packed.GetPresent().begin();
    for ( size_t seg = 0;  seg < numseg;  seg++, ++it_len ) {
        CPacked_seg::TIds::const_iterator it_id = packed.GetIds().begin();
        for ( size_t seq = 0;  seq < dim;  seq++, ++it_pres) {
            if ( *it_pres ) {
                CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(**it_id);
                m_Ranges[idh].Add(*it_start, *it_start + *it_len);
                ++it_id;
                ++it_start;
            }
        }
    }
}


void CSeqsRange::Add(const CSpliced_seg& spliced)
{
    const CSeq_id* gen_id = spliced.IsSetGenomic_id() ?
        &spliced.GetGenomic_id() : 0;
    const CSeq_id* prod_id = spliced.IsSetProduct_id() ?
        &spliced.GetProduct_id() : 0;
    ITERATE ( CSpliced_seg::TExons, it, spliced.GetExons() ) {
        const CSpliced_exon& ex = **it;
        const CSeq_id* ex_gen_id = ex.IsSetGenomic_id() ?
            &ex.GetGenomic_id() : gen_id;
        if ( ex_gen_id ) {
            CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*ex_gen_id);
            m_Ranges[idh].Add(ex.GetGenomic_start(), ex.GetGenomic_end());
        }
        const CSeq_id* ex_prod_id = ex.IsSetProduct_id() ?
            &ex.GetProduct_id() : prod_id;
        if ( ex_prod_id ) {
            CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*ex_prod_id);
            m_Ranges[idh].Add(ex.GetProduct_start().IsNucpos() ?
                ex.GetProduct_start().GetNucpos()
                : ex.GetProduct_start().GetProtpos().GetAmin(),
                ex.GetProduct_end().IsNucpos() ?
                ex.GetProduct_end().GetNucpos()
                : ex.GetProduct_end().GetProtpos().GetAmin());
        }
    }
}


void CSeqsRange::Add(const CSparse_seg& sparse)
{
    size_t dim = sparse.GetRows().size();
    size_t row = 0;
    ITERATE ( CSparse_seg::TRows, it, sparse.GetRows() ) {
        const CSparse_align& aln_row = **it;
        size_t numseg = aln_row.GetNumseg();
        if (numseg != aln_row.GetFirst_starts().size()) {
            ERR_POST_X(6, Warning <<
                "Invalid size of 'first-starts' in sparse-align");
            numseg = min(numseg, aln_row.GetFirst_starts().size());
        }
        if (numseg != aln_row.GetSecond_starts().size()) {
            ERR_POST_X(7, Warning <<
                "Invalid size of 'second-starts' in sparse-align");
            numseg = min(numseg, aln_row.GetSecond_starts().size());
        }
        if (numseg != aln_row.GetLens().size()) {
            ERR_POST_X(8, Warning <<
                "Invalid size of 'lens' in sparse-align");
            numseg = min(numseg, aln_row.GetLens().size());
        }
        if (aln_row.IsSetSecond_strands()  &&
            numseg != aln_row.GetSecond_strands().size()) {
            ERR_POST_X(9, Warning <<
                "Invalid size of 'second-strands' in sparse-align");
            numseg = min(numseg, aln_row.GetSecond_strands().size());
        }

        for (int seg = 0; seg < seg; ++seg) {
            TSeqPos len = aln_row.GetLens()[seg];
            CSeq_id_Handle idh =
                CSeq_id_Handle::GetHandle(aln_row.GetFirst_id());
            m_Ranges[idh].Add(aln_row.GetFirst_starts()[seg],
                aln_row.GetFirst_starts()[seg] + len - 1);
            idh = CSeq_id_Handle::GetHandle(aln_row.GetSecond_id());
            m_Ranges[idh].Add(aln_row.GetSecond_starts()[seg],
                aln_row.GetSecond_starts()[seg] + len - 1);
        }
        row++;
    }
}


void CSeqsRange::Add(const CSeq_graph& obj)
{
    Add(obj.GetLoc());
}


END_SCOPE(objects)
END_NCBI_SCOPE
