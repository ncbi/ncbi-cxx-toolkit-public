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
*   Interface for examining alignments (of type Dense-seg)
*
* ===========================================================================
*/


#include <objects/alnmgr/alnmap.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

void CAlnMap::x_CreateAlnStarts(void)
{
    m_AlnStarts.clear();
    m_AlnStarts.reserve(GetNumSegs());
    
    int start = 0, len = 0;
    for (int i = 0;  i < GetNumSegs();  ++i) {
        start += len;
        m_AlnStarts.push_back(start);
        len = m_DS->GetLens()[i];
    }
}


void CAlnMap::UnsetAnchor(void)
{
    m_AlnSegIdx.clear();
    m_NumSegWithOffsets.clear();
    if (m_RawSegTypes) {
        delete m_RawSegTypes;
        m_RawSegTypes = 0;
    }
    m_Anchor = -1;

    // we must call this last, as it uses some internal shenanigans that
    // are affected by the reset above
    x_CreateAlnStarts();
}


void CAlnMap::SetAnchor(TNumrow anchor)
{
    if (anchor == -1) {
        UnsetAnchor();
        return;
    }
    if (anchor < 0  ||  anchor >= m_DS->GetDim()) {
        NCBI_THROW(CAlnException, eInvalidRow,
                   "CAlnVec::SetAnchor(): "
                   "Invalid row");
    }
    m_AlnSegIdx.clear();
    m_AlnStarts.clear();
    m_NumSegWithOffsets.clear();
    if (m_RawSegTypes) {
        delete m_RawSegTypes;
        m_RawSegTypes = 0;
    }

    int start = 0, len = 0, aln_seg = -1, offset = 0;
    
    m_Anchor = anchor;
    for (int i = 0, pos = m_Anchor;  i < m_DS->GetNumseg();
         ++i, pos += m_DS->GetDim()) {
        if (m_DS->GetStarts()[pos] != -1) {
            ++aln_seg;
            offset = 0;
            m_AlnSegIdx.push_back(i);
            m_NumSegWithOffsets.push_back(CNumSegWithOffset(aln_seg));
            start += len;
            m_AlnStarts.push_back(start);
            len = m_DS->GetLens()[i];
        } else {
            ++offset;
            m_NumSegWithOffsets.push_back(CNumSegWithOffset(aln_seg, offset));
        }
    }
    if (!m_AlnSegIdx.size()) {
        NCBI_THROW(CAlnException, eInvalidDenseg,
                   "CAlnVec::SetAnchor(): "
                   "Invalid Dense-seg: No sequence on the anchor row");
    }
}


CAlnMap::TSegTypeFlags 
CAlnMap::x_SetRawSegType(TNumrow row, TNumseg seg) const
{
    TSegTypeFlags flags = 0;
    TNumseg       l_seg, r_seg, l_index, r_index, index;
    TSeqPos       cont_next_start, cont_prev_stop;

    l_seg = r_seg = seg;
    l_index = r_index = index = seg * m_DS->GetDim() + row;

    TSignedSeqPos start = m_DS->GetStarts()[index];

    // is it seq or gap?
    if (start >= 0) {
        flags |= fSeq;
        cont_next_start = start + m_DS->GetLens()[seg];
        cont_prev_stop  = start;
    }

    // is it aligned to sequence on the anchor?
    if (IsSetAnchor()) {
        flags |= fNotAlignedToSeqOnAnchor;
        if (m_DS->GetStarts()[seg * m_DS->GetDim() + m_Anchor] >= 0) {
            flags &= ~(flags & fNotAlignedToSeqOnAnchor);
        }
    }

    // what's on the right?
    flags |= fEndOnRight | fNoSeqOnRight;
    while (++r_seg < m_DS->GetNumseg()) {
        flags &= ~(flags & fEndOnRight);
        r_index += m_DS->GetDim();
        if ((start = m_DS->GetStarts()[r_index]) >= 0) {
            if ((flags & fSeq) && 
                (IsPositiveStrand(row) ?
                 start != (TSignedSeqPos)cont_next_start :
                 start + m_DS->GetLens()[r_seg] != cont_prev_stop)) {
                flags |= fUnalignedOnRight;
            }
            flags &= ~(flags & fNoSeqOnRight);
            break;
        }
    }

    // what's on the left?
    flags |= fEndOnLeft | fNoSeqOnLeft;
    while (--l_seg >= 0) {
        flags &= ~(flags & fEndOnLeft);
        l_index -= m_DS->GetDim();
        if ((start = m_DS->GetStarts()[l_index]) >= 0) {
            if ((flags & fSeq) && 
                (IsPositiveStrand(row) ?
                 start + m_DS->GetLens()[l_seg] != cont_prev_stop :
                 start != (TSignedSeqPos)cont_next_start)) {
                flags |= fUnalignedOnLeft;
            }
            flags &= ~(flags & fNoSeqOnLeft);
            break;
        }
    }
        
    // add to cache
    if ( !m_RawSegTypes ) {
        // Using kZero for 0 works around a bug in Compaq's C++ compiler.
        static const TSegTypeFlags kZero = 0;
        m_RawSegTypes = new vector<TSegTypeFlags>
            (m_DS->GetDim() * m_DS->GetNumseg(), kZero);
    }
    (*m_RawSegTypes)[row + m_DS->GetDim() * seg] = flags | fTypeIsSet;

    return flags;
}


CAlnMap::TNumseg CAlnMap::GetSeg(TSeqPos aln_pos) const
{
    TNumseg btm, top, mid;

    btm = 0;
    top = m_AlnStarts.size() - 1;

    if (aln_pos > m_AlnStarts[top] +
        m_DS->GetLens()[x_GetRawSegFromSeg(top)] - 1) 
        return -1; // out of range

    while (btm < top) {
        mid = (top + btm) / 2;
        if (m_AlnStarts[mid] == (TSignedSeqPos)aln_pos) {
            return mid;
        }
        if (m_AlnStarts[mid + 1] <= (TSignedSeqPos)aln_pos) {
            btm = mid + 1; 
        } else {
            top = mid;
        }
    } 
    return top;
}


CAlnMap::TNumseg
CAlnMap::GetRawSeg(TNumrow row, TSeqPos seq_pos) const
{
    TSignedSeqPos start = -1, sseq_pos = seq_pos;
    TNumseg       btm, top, mid, cur;
    TNumrow       dim;
    btm = 0; cur = top = m_DS->GetNumseg() - 1;
    dim = m_DS->GetDim();

    if (sseq_pos < GetSeqStart(row)  ||  sseq_pos > GetSeqStop(row)) {
        return -1; // out of range
    }

    while (btm <= top) {
        cur = mid = (top + btm) / 2;

        while (cur <= top
               &&  (start = m_DS->GetStarts()[cur * dim + row]) < 0) {
            ++cur;
        }
        if (cur <= top && start >= 0) {
            if (sseq_pos >= start &&
                seq_pos < start + m_DS->GetLens()[cur]) {
                return cur; // found
            }
            if (sseq_pos > start) {
                btm = cur + 1; 
            } else {
                top = mid - 1;
            }
            continue;
        }

        cur = mid-1;
        while (cur >= btm &&
               (start = m_DS->GetStarts()[cur * dim + row]) < 0) {
            --cur;
        }
        if (cur >= btm && start >= 0) {
            if (sseq_pos >= start
                &&  seq_pos < start + m_DS->GetLens()[cur]) {
                return cur; // found
            }
            if (sseq_pos > start) {
                btm = mid + 1;
            } else {
                top = cur - 1;
            }
            continue;
        }
    }        
    return -1; /* No match found */
}
    

TSignedSeqPos CAlnMap::GetAlnPosFromSeqPos(TNumrow row, TSeqPos seq_pos) const
{
    TNumseg raw_seg = GetRawSeg(row, seq_pos);
    CNumSegWithOffset seg = x_GetSegFromRawSeg(raw_seg);
    if (seg.GetOffset()) {
        return -1; // it is offset
    } else {
        TSeqPos delta
            = seq_pos - m_DS->GetStarts()[raw_seg * m_DS->GetDim() + row];
        return (m_AlnStarts[seg.GetAlnSeg()]
                + (IsPositiveStrand(row) ? delta
                   : (m_DS->GetLens()[raw_seg] - 1 - delta)));
    }
}


TSignedSeqPos CAlnMap::GetSeqPosFromAlnPos(TNumrow for_row,
                                           TSeqPos aln_pos) const
{
    TNumseg seg = GetSeg(aln_pos);
    if (seg < 0) {
        return -1;
    } else {
        TSignedSeqPos pos = GetStart(for_row, seg);
        if (pos == -1) {
            // our alignment position lies on a gap, so we return
            // the best available offset
            if (IsPositiveStrand(for_row)) {
                for ( ; seg > 0 && pos == -1; --seg)
                    pos = GetStop(for_row, seg);
            } else {
                for ( ; seg < GetNumSegs() && pos == -1; ++seg) {
                    pos = GetStop(for_row, seg);
                }
            }
        } else {
            TSeqPos delta = aln_pos - GetAlnStart(seg);
            if (IsPositiveStrand(for_row)) {
                pos += delta;
            } else {
                pos += GetLen(seg) - 1 - delta;
            }
        }

        return pos;
    }
}


TSignedSeqPos CAlnMap::GetSeqPosFromSeqPos(TNumrow for_row,
                                           TNumrow row, TSeqPos seq_pos) const
{
    TNumseg raw_seg = GetRawSeg(row, seq_pos);
    TSeqPos delta
        = seq_pos - m_DS->GetStarts()[raw_seg * m_DS->GetDim() + row];

    return (GetStart(for_row, raw_seg)
            + (StrandSign(row) == StrandSign(for_row) ? delta
               : m_DS->GetLens()[raw_seg] - 1 - delta));
}


TSignedSeqPos CAlnMap::GetSeqStart(TNumrow row) const
{
    int seg, dim, start;
    dim = m_DS->GetDim();
    
    if (IsPositiveStrand(row)) {
        seg = -1;
        while (++seg < m_DS->GetNumseg()) {
            if ((start = m_DS->GetStarts()[seg * dim + row]) >= 0) {
                return start;
            }
        }
    } else {
        seg = m_DS->GetNumseg();
        while (seg--) {
            if ((start = m_DS->GetStarts()[seg * dim + row]) >= 0) {
                return start;
            }
        }
    }
    return -1;
}


TSignedSeqPos CAlnMap::GetSeqStop(TNumrow row) const
{
    int seg, dim, start;
    dim = m_DS->GetDim();
    
    if (IsPositiveStrand(row)) {
        seg = m_DS->GetNumseg();
        while (seg--) {
            if ((start = m_DS->GetStarts()[seg * dim + row]) >= 0) {
                return start + m_DS->GetLens()[seg] - 1;
            }
        }
    } else {
        seg = -1;
        while (++seg < m_DS->GetNumseg()) {
            if ((start = m_DS->GetStarts()[seg * dim + row]) >= 0) {
                return start + m_DS->GetLens()[seg] - 1;
            }
        }
    }
    return -1;
}


TSignedSeqPos CAlnMap::GetSeqAlnStart(TNumrow row) const
{
    int seg = -1;
    
    while (++seg < GetNumSegs()) {
        if (GetStart(row, seg) >= 0
            &&  ( !IsSetAnchor()  ||  GetStart(m_Anchor, seg) >= 0)) {
            return GetAlnStart(seg);
        }
    }
    return -1;
}


TSignedSeqPos CAlnMap::GetSeqAlnStop(TNumrow row) const
{
    int seg = GetNumSegs();
    
    while (seg--) {
        if (GetStart(row, seg) >= 0
            &&  ( !IsSetAnchor()  ||  GetStart(m_Anchor, seg) >= 0)) {
            return GetAlnStop(seg);
        }
    }
    return -1;
}


CRef<CAlnMap::CAlnChunkVec>
CAlnMap::GetAlnChunks(TNumrow row, const TSignedRange& range,
                      TGetChunkFlags flags) const
{
    CRef<CAlnChunkVec> vec = new CAlnChunkVec(*this, row);

    if (range.GetTo() < 0
        ||  (TSeqPos)range.GetFrom() > GetAlnStop(GetNumSegs() - 1)) {
        return vec;
    }

    TNumseg first_seg, last_seg, aln_seg;

    if (range.GetFrom() < 0) {
        first_seg = 0;
    } else {        
        first_seg = x_GetRawSegFromSeg(aln_seg = GetSeg(range.GetFrom()));
        if (GetAlnStart(aln_seg) != (TSeqPos)range.GetFrom()) {
            vec->m_LeftDelta = range.GetFrom() - GetAlnStart(aln_seg);
        }
    }
    if ((TSeqPos)range.GetTo() > GetAlnStop(GetNumSegs()-1)) {
        last_seg = m_DS->GetNumseg()-1;
    } else {
        last_seg = x_GetRawSegFromSeg(aln_seg = GetSeg(range.GetTo()));
        if (GetAlnStop(aln_seg) != (TSeqPos)range.GetTo()) {
            vec->m_RightDelta = GetAlnStop(aln_seg) - range.GetTo();
        }
    }
    
    vec->m_Idx.push_back(first_seg);
    for (int seg = first_seg;  seg < last_seg;  seg++) {
        if (x_RawSegTypeDiffNextSegType(row, seg, flags)) {
            vec->m_Idx.push_back(seg+1);
        }
    }
    vec->m_Idx.push_back(last_seg+1);
    vec->m_Size = vec->m_Idx.size()-1;
    return vec;
}


CConstRef<CAlnMap::CAlnChunk>
CAlnMap::CAlnChunkVec::operator[](CAlnMap::TNumchunk i) const
{
    CAlnMap::TNumseg seg_from = m_Idx[i];
    CAlnMap::TNumseg seg_to   = m_Idx[i+1];

    CRef<CAlnChunk>  chunk    = new CAlnChunk();

    chunk->SetRange().SetFrom(m_AlnMap.m_DS->GetStarts()
                              [seg_from * m_AlnMap.m_DS->GetDim()
                              + m_Row]);
    if ( !chunk->IsGap() ) {
        chunk->SetRange().SetTo(chunk->GetRange().GetFrom()
                                + m_AlnMap.m_DS->GetLens()[seg_from] - 1);
    } else {
        chunk->SetRange().SetFrom(-1);
        chunk->SetRange().SetTo(-1);
    }

    chunk->SetType(m_AlnMap.x_GetRawSegType(m_Row, seg_from));

    for (CAlnMap::TNumseg seg = seg_from + 1;  seg < seg_to;  seg++) {
        if ( !chunk->IsGap() ) {
            if (m_AlnMap.IsPositiveStrand(m_Row)) {
                chunk->SetRange().SetTo(chunk->GetRange().GetTo()
                                        + m_AlnMap.m_DS->GetLens()[seg]);
            } else {
                chunk->SetRange().SetFrom(chunk->GetRange().GetFrom()
                                          - m_AlnMap.m_DS->GetLens()[seg]);
            }
        }
        chunk->SetType(chunk->GetType()
                       | m_AlnMap.x_GetRawSegType(m_Row, seg));
    }

    // fix if extreme end
    if (i == 0 && m_LeftDelta) {
        if (!chunk->IsGap()) {
            if (m_AlnMap.IsPositiveStrand(m_Row)) {
                chunk->SetRange().SetFrom(chunk->GetRange().GetFrom()
                                          + m_LeftDelta);
            } else {
                chunk->SetRange().SetTo(chunk->GetRange().GetTo()
                                        - m_LeftDelta);
            }
        }            
        chunk->SetType(chunk->GetType() &
                    ~(fNoSeqOnLeft | fUnalignedOnLeft | fEndOnLeft));
    }
    if (i == m_Size - 1 && m_RightDelta) {
        if (!chunk->IsGap()) {
            if (m_AlnMap.IsPositiveStrand(m_Row)) {
                chunk->SetRange().SetTo(chunk->GetRange().GetTo()
                                        - m_RightDelta);
            } else {
                chunk->SetRange().SetFrom(chunk->GetRange().GetFrom()
                                          + m_RightDelta);
            }
        }
        chunk->SetType(chunk->GetType() &
                       ~(fNoSeqOnRight | fUnalignedOnRight | fEndOnRight));
    }

    return chunk;
}


bool
CAlnMap::x_RawSegTypeDiffNextSegType(TNumrow row, TNumseg seg,
                                     TGetChunkFlags flags) const
{
    TSegTypeFlags type      = x_GetRawSegType(row, seg);
    TSegTypeFlags next_type = x_GetRawSegType(row, seg+1);
    
    if ((type & fSeq) != (next_type & fSeq)) {
        return true;
    }
    if ((type & fUnalignedOnRight) && !(flags & fIgnoreUnaligned)) {
        return true;
    }
    if ((type & fNotAlignedToSeqOnAnchor) ==
        (next_type & fNotAlignedToSeqOnAnchor)) {
        return false;
    }
    if (type & fSeq) {
        if (!(flags & fInsertSameAsSeq)) {
            return true;
        }
    } else {
        if (!(flags & fDeleteSameAsGap)) {
            return true;
        }
    }
    return false;
}

END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.11  2002/09/27 16:57:46  todorov
* changed order of params for GetSeqPosFrom{Seq,Aln}Pos
*
* Revision 1.10  2002/09/26 18:24:50  todorov
* fixed a just introduced bug
*
* Revision 1.9  2002/09/26 17:43:17  todorov
* 1) Changed flag fAlignedToSeqOnAnchor to fNotAlignedToSeqOnAnchor. This proved
* more convenient.
* 2) Introduced some exceptions
* 3) Fixed a strand bug in CAlnMap::CAlnChunkVec::operator[]
*
* Revision 1.8  2002/09/25 18:16:29  dicuccio
* Reworked computation of consensus sequence - this is now stored directly
* in the underlying CDense_seg
* Added exception class; currently used only on access of non-existent
* consensus.
*
* Revision 1.7  2002/09/19 22:16:48  todorov
* fix the range on the extreme end only if not a gap
*
* Revision 1.6  2002/09/19 22:09:07  todorov
* fixed a problem due to switching of lines during code cleanup
*
* Revision 1.5  2002/09/18 19:24:54  todorov
* fixing the flags on the extreme end only if delta
*
* Revision 1.4  2002/09/05 19:30:39  dicuccio
* - added ability to reference a consensus sequence for a given alignment
* - added caching for CSeqVector objects (big performance gain)
* - many small bugs fixed
*
* Revision 1.3  2002/08/23 20:34:17  ucko
* Work around a Compaq C++ compiler bug.
*
* Revision 1.2  2002/08/23 20:31:17  todorov
* fixed neg strand deltas
*
* Revision 1.1  2002/08/23 14:43:52  ucko
* Add the new C++ alignment manager to the public tree (thanks, Kamen!)
*
*
* ===========================================================================
*/
