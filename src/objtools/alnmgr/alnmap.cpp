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


#include <ncbi_pch.hpp>
#include <objtools/alnmgr/alnmap.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::


void CAlnMap::x_Init(void)
{
    m_SeqLeftSegs.resize(GetNumRows(), -1);
    m_SeqRightSegs.resize(GetNumRows(), -1);
}


void CAlnMap::x_CreateAlnStarts(void)
{
    m_AlnStarts.clear();
    m_AlnStarts.reserve(GetNumSegs());
    
    int start = 0, len = 0;
    for (int i = 0;  i < GetNumSegs();  ++i) {
        start += len;
        m_AlnStarts.push_back(start);
        len = m_Lens[i];
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
    if (anchor < 0  ||  anchor >= m_NumRows) {
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
    for (int i = 0, pos = m_Anchor;  i < m_NumSegs;
         ++i, pos += m_NumRows) {
        if (m_Starts[pos] != -1) {
            ++aln_seg;
            offset = 0;
            m_AlnSegIdx.push_back(i);
            m_NumSegWithOffsets.push_back(CNumSegWithOffset(aln_seg));
            start += len;
            m_AlnStarts.push_back(start);
            len = m_Lens[i];
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
    l_index = r_index = index = seg * m_NumRows + row;

    TSignedSeqPos start = m_Starts[index];

    // is it seq or gap?
    if (start >= 0) {
        flags |= fSeq;
        cont_next_start = start + x_GetLen(row, seg);
        cont_prev_stop  = start;
    }

    // is it aligned to sequence on the anchor?
    if (IsSetAnchor()) {
        flags |= fNotAlignedToSeqOnAnchor;
        if (m_Starts[seg * m_NumRows + m_Anchor] >= 0) {
            flags &= ~(flags & fNotAlignedToSeqOnAnchor);
        }
    }

    // what's on the right?
    if (r_seg < m_NumSegs - 1) {
        flags |= fEndOnRight;
    }
    flags |= fNoSeqOnRight;
    while (++r_seg < m_NumSegs) {
        flags &= ~(flags & fEndOnRight);
        r_index += m_NumRows;
        if ((start = m_Starts[r_index]) >= 0) {
            if ((flags & fSeq) && 
                (IsPositiveStrand(row) ?
                 start != (TSignedSeqPos)cont_next_start :
                 start + x_GetLen(row, r_seg) != cont_prev_stop)) {
                flags |= fUnalignedOnRight;
            }
            flags &= ~(flags & fNoSeqOnRight);
            break;
        }
    }

    // what's on the left?
    if (l_seg > 0) {
        flags |= fEndOnLeft;
    }
    flags |= fNoSeqOnLeft;
    while (--l_seg >= 0) {
        flags &= ~(flags & fEndOnLeft);
        l_index -= m_NumRows;
        if ((start = m_Starts[l_index]) >= 0) {
            if ((flags & fSeq) && 
                (IsPositiveStrand(row) ?
                 start + x_GetLen(row, l_seg) != cont_prev_stop :
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
            (m_NumRows * m_NumSegs, kZero);
    }
    (*m_RawSegTypes)[row + m_NumRows * seg] = flags | fTypeIsSet;

    return flags;
}


CAlnMap::TNumseg CAlnMap::GetSeg(TSeqPos aln_pos) const
{
    TNumseg btm, top, mid;

    btm = 0;
    top = m_AlnStarts.size() - 1;

    if (aln_pos > m_AlnStarts[top] +
        m_Lens[x_GetRawSegFromSeg(top)] - 1) 
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
CAlnMap::GetRawSeg(TNumrow row, TSeqPos seq_pos,
                   ESearchDirection dir, bool try_reverse_dir) const
{
    TSignedSeqPos start = -1, sseq_pos = seq_pos;
    TNumseg       btm, top, mid, cur, last, cur_top, cur_btm;
    btm = cur_btm = 0; cur = top = last = cur_top = m_NumSegs - 1;

    bool plus = IsPositiveStrand(row);

    // if out-of-range, return either -1 or the closest seg in dir direction
    if (sseq_pos < GetSeqStart(row)) {
        if (dir == eNone) {
            return -1;
        } else if (dir == eForward  || 
                   dir == (plus ? eRight : eLeft)  ||
                   try_reverse_dir) {
            TNumseg seg;
            if (plus) {
                seg = -1;
                while (++seg < m_NumSegs) {
                    if (m_Starts[seg * m_NumRows + row] >= 0) {
                        return seg;
                    }
                }
            } else {
                seg = m_NumSegs;
                while (seg--) {
                    if (m_Starts[seg * m_NumRows + row] >= 0) {
                        return seg;
                    }
                }
            }
        }
    } else if (sseq_pos > GetSeqStop(row)) {
        if (dir == eNone) {
            return -1;
        } else if (dir == eBackwards  ||
                   dir == (plus ? eLeft : eRight)  ||
                   try_reverse_dir) {
            TNumseg seg;
            if (plus) {
                seg = m_NumSegs;
                while (seg--) {
                    if (m_Starts[seg * m_NumRows + row] >= 0) {
                        return seg;
                    }
                }
            } else {
                seg = -1;
                while (++seg < m_NumSegs) {
                    if (m_Starts[seg * m_NumRows + row] >= 0) {
                        return seg;
                    }
                }
            }
        }
    }

    // main loop
    while (btm <= top) {
        cur = mid = (top + btm) / 2;

        while (cur <= top
               &&  (start = m_Starts[(plus ? cur : last - cur) 
                                             * m_NumRows + row]) < 0) {
            ++cur;
        }
        if (cur <= top && start >= 0) {
            if (sseq_pos >= start &&
                seq_pos < start + x_GetLen(row, plus ? cur : last - cur)) {
                return (plus ? cur : last - cur); // found
            }
            if (sseq_pos > start) {
                btm = cur + 1;
                cur_btm = cur;
            } else {
                top = mid - 1;
                cur_top = cur;
            }
            continue;
        }

        cur = mid-1;
        while (cur >= btm &&
               (start = m_Starts[(plus ? cur : last - cur)
                                         * m_NumRows + row]) < 0) {
            --cur;
        }
        if (cur >= btm && start >= 0) {
            if (sseq_pos >= start
                &&  seq_pos < start + x_GetLen(row, plus ? cur : last - cur)) {
                return (plus ? cur : last - cur); // found
            }
            if (sseq_pos > start) {
                btm = mid + 1;
                cur_btm = cur;
            } else {
                top = cur - 1;
                cur_top = cur;
            }
            continue;
        }

        // if we get here, seq_pos falls into an unaligned region
        // return either -1 or the closest segment in dir direction
        if (dir == eNone) {
            return -1;
        } else if (dir == eBackwards  ||  dir == (plus ? eLeft : eRight)) {
            return (plus ? cur_btm : last - cur_btm);
        } else if (dir == eForward  ||  dir == (plus ? eRight : eLeft)) {
            return (plus ? cur_top : last - cur_top);
        }
    }        
    return -1; /* No match found */
}
    

TSignedSeqPos CAlnMap::GetAlnPosFromSeqPos(TNumrow row, TSeqPos seq_pos,
                                           ESearchDirection dir,
                                           bool try_reverse_dir) const
{
    TNumseg raw_seg = GetRawSeg(row, seq_pos, dir, try_reverse_dir);
    if (raw_seg < 0) { // out of seq range
        return -1;
    }

    TSeqPos start = m_Starts[raw_seg * m_NumRows + row];
    TSeqPos len   = x_GetLen(row, raw_seg);
    TSeqPos stop  = start + len -1;
    bool    plus  = IsPositiveStrand(row);

    CNumSegWithOffset seg = x_GetSegFromRawSeg(raw_seg);

    if (dir == eNone) {
        if (seg.GetOffset()) {
            // seq_pos is within an insert
            return -1;
        } 
    } else {
        // check if within unaligned region
        // if seq_pos outside the segment returned by GetRawSeg
        // then return the edge alnpos
        if ((plus ? seq_pos < start : seq_pos > stop)) {
            return GetAlnStart(seg.GetAlnSeg());
        }
        if ((plus ? seq_pos > stop : seq_pos < start)) {
            return GetAlnStop(seg.GetAlnSeg());
        }


        // check if within an insert
        if (seg.GetOffset()  &&  
            (dir == eRight  ||
             dir == (plus ? eForward : eBackwards))) {

            // seek the nearest alnpos on the right
            if (seg.GetAlnSeg() < GetNumSegs() - 1) {
                return GetAlnStart(seg.GetAlnSeg() + 1);
            } else if (try_reverse_dir) {
                return GetAlnStop(seg.GetAlnSeg());
            } else {
                return -1;
            }

        }
        if (seg.GetOffset()  &&
            (dir == eLeft  ||
             dir == (plus ? eBackwards : eForward))) {
            
            // seek the nearest alnpos on left
            if (seg.GetAlnSeg() >= 0) {
                return GetAlnStop(seg.GetAlnSeg());
            } else if (try_reverse_dir) {
                return GetAlnStart(seg.GetAlnSeg() + 1);
            } else {
                return -1;
            }

        }
    } 

    // main case: seq_pos is within an alnseg
    //assert(seq_pos >= start  &&  seq_pos <= stop);
    TSeqPos delta = (seq_pos - start) / GetWidth(row);
    return m_AlnStarts[seg.GetAlnSeg()]
        + (plus ? delta : m_Lens[raw_seg] - 1 - delta);
}

TSignedSeqPos CAlnMap::GetSeqPosFromAlnPos(TNumrow for_row,
                                           TSeqPos aln_pos,
                                           ESearchDirection dir,
                                           bool try_reverse_dir) const
{
    if (aln_pos > GetAlnStop()) {
        aln_pos = GetAlnStop(); // out-of-range adjustment
    }
    TNumseg seg = GetSeg(aln_pos);
    TSignedSeqPos pos = GetStart(for_row, seg);
    if (pos >= 0) {
        TSeqPos delta = (aln_pos - GetAlnStart(seg)) * GetWidth(for_row);
        if (IsPositiveStrand(for_row)) {
            pos += delta;
        } else {
            pos += x_GetLen(for_row, x_GetRawSegFromSeg(seg)) - 1 - delta;
        }
    } else if (dir != eNone) {
        // it is a gap, search in the neighbouring segments
        // according to search direction (dir) and strand
        bool reverse_pass = false;
        TNumseg orig_seg = seg = x_GetRawSegFromSeg(seg);
            
        while (true) {
            if (IsPositiveStrand(for_row)) {
                if (dir == eBackwards  ||  dir == eLeft) {
                    while (--seg >=0  &&  pos == -1) {
                        pos = x_GetRawStop(for_row, seg);
                    }
                } else {
                    while (++seg < m_NumSegs  &&  pos == -1) {
                        pos = x_GetRawStart(for_row, seg);
                    }
                }
            } else {
                if (dir == eForward  ||  dir == eLeft) {
                    while (--seg >=0  &&  pos == -1) {
                        pos = x_GetRawStart(for_row, seg);
                    }
                } else {
                    while (++seg < m_NumSegs  &&  pos == -1) {
                        pos = x_GetRawStop(for_row, seg);
                    } 
                }
            }
            if (!try_reverse_dir) {
                break;
            }
            if (pos >= 0) {
                break; // found
            } else if (reverse_pass) {
                string msg = "CAlnVec::GetSeqPosFromAlnPos(): "
                    "Invalid Dense-seg: Row " +
                    NStr::IntToString(for_row) +
                    " contains gaps only.";
                NCBI_THROW(CAlnException, eInvalidDenseg, msg);
            }
            // not found, try reverse direction
            reverse_pass = true;
            seg = orig_seg;
            switch (dir) {
            case eLeft:
                dir = eRight; break;
            case eRight:
                dir = eLeft; break;
            case eForward:
                dir = eBackwards; break;
            case eBackwards:
                dir = eForward; break;
            }
        }
    }
    return pos;
}

TSignedSeqPos CAlnMap::GetSeqPosFromSeqPos(TNumrow for_row,
                                           TNumrow row, TSeqPos seq_pos) const
{
    TNumseg raw_seg = GetRawSeg(row, seq_pos);
    if (raw_seg < 0) {
        return -1;
    }
    unsigned offset = raw_seg * m_NumRows;
    TSignedSeqPos start = m_Starts[offset + for_row];
    if (start >= 0) {
        TSeqPos delta
            = seq_pos - m_Starts[offset + row];
        if (GetWidth(for_row) != GetWidth(row)) {
            delta = delta / GetWidth(row) * GetWidth(for_row);
        }
        return start
            + (StrandSign(row) == StrandSign(for_row) ? delta
               : x_GetLen(for_row, raw_seg) - 1 - delta);
    } else {
        return -1;
    }
}


const CAlnMap::TNumseg& CAlnMap::x_GetSeqLeftSeg(TNumrow row) const
{
    TNumseg& seg = m_SeqLeftSegs[row];
    if (seg < 0) {
        while (++seg < m_NumSegs) {
            if (m_Starts[seg * m_NumRows + row] >= 0) {
                return seg;
            }
        }
    } else {
        return seg;
    }
    seg = -1;
    string err_msg = "CAlnVec::x_GetSeqLeftSeg(): "
        "Invalid Dense-seg: Row " + NStr::IntToString(row) +
        " contains gaps only.";
    NCBI_THROW(CAlnException, eInvalidDenseg, err_msg);
}
    

const CAlnMap::TNumseg& CAlnMap::x_GetSeqRightSeg(TNumrow row) const
{
    TNumseg& seg = m_SeqRightSegs[row];
    if (seg < 0) {
        seg = m_NumSegs;
        while (seg--) {
            if (m_Starts[seg * m_NumRows + row] >= 0) {
                return seg;
            }
        }
    } else {
        return seg;
    }
    seg = -1;
    string err_msg = "CAlnVec::x_GetSeqRightSeg(): "
        "Invalid Dense-seg: Row " + NStr::IntToString(row) +
        " contains gaps only.";
    NCBI_THROW(CAlnException, eInvalidDenseg, err_msg);
}
    

void CAlnMap::GetResidueIndexMap(TNumrow row0,
                                 TNumrow row1,
                                 TRange aln_rng,
                                 vector<TSignedSeqPos>& result,
                                 TRange& rng0,
                                 TRange& rng1) const
{
    _ASSERT( ! IsSetAnchor() );
    TNumseg l_seg, r_seg;
    TSeqPos aln_start = aln_rng.GetFrom();
    TSeqPos aln_stop = aln_rng.GetTo();
    int l_idx0 = row0;
    int l_idx1 = row1;
    TSeqPos aln_pos = 0, next_aln_pos, l_len, r_len, l_delta, r_delta;
    bool plus0 = IsPositiveStrand(row0);
    bool plus1 = IsPositiveStrand(row1);
    TSeqPos l_pos0, r_pos0, l_pos1, r_pos1;

    l_seg = 0;
    while (l_seg < m_NumSegs) {
        l_len = m_Lens[l_seg];
        next_aln_pos = aln_pos + l_len;
        if (m_Starts[l_idx0] >= 0  &&  m_Starts[l_idx1] >= 0  &&
            aln_start >= aln_pos  &&  aln_start < next_aln_pos) {
            // found the left seg
            break;
        }
        aln_pos = next_aln_pos;
        l_idx0 += m_NumRows; l_idx1 += m_NumRows;
        l_seg++;
    }
    _ASSERT(l_seg < m_NumSegs);

    // determine left seq positions
    l_pos0 = m_Starts[l_idx0];
    l_pos1 = m_Starts[l_idx1];
    l_delta = aln_start - aln_pos;
    l_len -= l_delta;
    _ASSERT(l_delta >= 0);
    if (plus0) {
        l_pos0 += l_delta;
    } else {
        l_pos0 += l_len - 1;
    }
    if (plus1) {
        l_pos1 += l_delta;
    } else {
        l_pos1 += l_len - 1;
    }
        
    r_seg = m_NumSegs - 1;
    int r_idx0 = r_seg * m_NumRows + row0;
    int r_idx1 = r_seg * m_NumRows + row1;
    aln_pos = GetAlnStop();
    if (aln_stop > aln_pos) {
        aln_stop = aln_pos;
    }
    while (r_seg >= 0) {
        r_len = m_Lens[r_seg];
        next_aln_pos = aln_pos - r_len;
        if (m_Starts[l_idx0] >= 0  &&  m_Starts[l_idx1] >= 0  &&
            aln_stop > next_aln_pos  &&  aln_stop <= aln_pos) {
            // found the right seg
            break;
        }
        aln_pos = next_aln_pos;
        r_idx0 -= m_NumRows; r_idx1 -= m_NumRows;
        r_seg--;
    }
    
    // determine right seq positions
    r_pos0 = m_Starts[r_idx0];
    r_pos1 = m_Starts[r_idx1];
    r_delta = aln_pos - aln_stop;
    r_len -= r_delta;
    _ASSERT(r_delta >= 0);
    if (plus0) {
        r_pos0 += r_len - 1;
    } else {
        r_pos0 += r_delta;
    }
    if (plus1) {
        r_pos1 += r_len - 1;
    } else {
        r_pos1 += r_delta;
    }
        
    // We now know the size of the resulting vector
    TSeqPos size = (plus0 ? r_pos0 - l_pos0 : l_pos0 - r_pos0) + 1;
    result.resize(size, -1);

    // Initialize index positions (from left to right)
    TSeqPos pos0 = plus0 ? 0 : l_pos0 - r_pos0;
    TSeqPos pos1 = plus1 ? 0 : l_pos1 - r_pos1;

    // Initialize 'next' positions
    // -- to determine if there are unaligned pieces
    TSeqPos next_l_pos0 = plus0 ? l_pos0 + l_len : l_pos0 - l_len;
    TSeqPos next_l_pos1 = plus1 ? l_pos1 + l_len : l_pos1 - l_len;

    l_idx0 = row0;
    l_idx1 = row1;
    TNumseg seg = l_seg;
    TSeqPos delta;
    while (true) {
        if (m_Starts[l_idx0] >= 0) { // if row0 is not gapped

            if (seg > l_seg) {
                // check for unaligned region / validate
                if (plus0) {
                    delta = m_Starts[l_idx0] - next_l_pos0;
                    next_l_pos0 = m_Starts[l_idx0] + l_len;
                } else {
                    delta = next_l_pos0 - m_Starts[l_idx0] - l_len + 1;
                    next_l_pos0 = m_Starts[l_idx0] - 1;
                }
                if (delta > 0) {
                    // unaligned region
                    if (plus0) {
                        pos0 += delta;
                    } else {
                        pos0 -= delta;
                    }
                } else if (delta < 0) {
                    // invalid segment
                    string errstr = string("CAlnMap::GetResidueIndexMap():")
                        + " Starts are not consistent!"
                        + " Row=" + NStr::IntToString(row0) +
                        " Seg=" + NStr::IntToString(seg);
                    NCBI_THROW(CAlnException, eInvalidDenseg, errstr);
                }
            }

            if (m_Starts[l_idx1] >= 0) { // if row1 is not gapped
                
                if (seg > l_seg) {
                    // check for unaligned region / validate
                    if (plus1) {
                        delta = m_Starts[l_idx1] - next_l_pos1;
                        next_l_pos1 = m_Starts[l_idx1] + l_len;
                    } else {
                        delta = next_l_pos1 - m_Starts[l_idx1] - l_len + 1;
                        next_l_pos1 = m_Starts[l_idx1] - 1;
                    }
                    if (delta > 0) {
                        // unaligned region
                        if (plus1) {
                            pos1 += delta;
                        } else {
                            pos1 -= delta;
                        }
                    } else if (delta < 0) {
                        // invalid segment
                        string errstr = string("CAlnMap::GetResidueIndexMap():")
                            + " Starts are not consistent!"
                            + " Row=" + NStr::IntToString(row1) +
                            " Seg=" + NStr::IntToString(seg);
                        NCBI_THROW(CAlnException, eInvalidDenseg, errstr);
                    }
                }

                if (plus0) {
                    if (plus1) { // if row1 on +
                        while (l_len--) {
                            result[pos0++] = pos1++;
                        }
                    } else { // if row1 on -
                        while (l_len--) {
                            result[pos0++] = pos1--;
                        }
                    }
                } else { // if row0 on -
                    if (plus1) { // if row1 on +
                        while (l_len--) {
                            result[pos0--] = pos1++;
                        }
                    } else { // if row1 on -
                        while (l_len--) {
                            result[pos0--] = pos1--;
                        }
                    }
                }                    
            } else {
                if (plus0) {
                    pos0 += l_len;
                } else {
                    pos0 -= l_len;
                }
            }
        }

        // iterate to next segment
        seg++;
        l_idx0 += m_NumRows;
        l_idx1 += m_NumRows;
        if (seg < r_seg) {
            l_len = m_Lens[seg];
        } else if (seg == r_seg) {
            l_len = r_len;
        } else {
            break;
        }
    }

    // finally, set the ranges for the two sequences
    rng0.SetFrom(plus0 ? l_pos0 : r_pos0);
    rng0.SetTo(plus0 ? r_pos0 : l_pos0);
    rng1.SetFrom(plus1 ? l_pos1 : r_pos1);
    rng1.SetTo(plus1 ? r_pos1 : l_pos1);
}


TSignedSeqPos CAlnMap::GetSeqAlnStart(TNumrow row) const
{
    if (IsSetAnchor()) {
        TNumseg seg = -1;
        while (++seg < m_AlnSegIdx.size()) {
            if (m_Starts[m_AlnSegIdx[seg] * m_NumRows + row] >= 0) {
                return GetAlnStart(seg);
            }
        }
        return -1;
    } else {
        return GetAlnStart(x_GetSeqLeftSeg(row));
    }
}


TSignedSeqPos CAlnMap::GetSeqAlnStop(TNumrow row) const
{
    if (IsSetAnchor()) {
        TNumseg seg = m_AlnSegIdx.size();
        while (seg--) {
            if (m_Starts[m_AlnSegIdx[seg] * m_NumRows + row] >= 0) {
                return GetAlnStop(seg);
            }
        }
        return -1;
    } else {
        return GetAlnStop(x_GetSeqRightSeg(row));
    }
}


CRef<CAlnMap::CAlnChunkVec>
CAlnMap::GetAlnChunks(TNumrow row, const TSignedRange& range,
                      TGetChunkFlags flags) const
{
    CRef<CAlnChunkVec> vec(new CAlnChunkVec(*this, row));

    // boundaries check
    if (range.GetTo() < 0
        ||  range.GetFrom() > (TSignedSeqPos) GetAlnStop(GetNumSegs() - 1)) {
        return vec;
    }

    // determine the participating segments range
    TNumseg first_seg, last_seg, aln_seg;

    if (range.GetFrom() < 0) {
        first_seg = 0;
    } else {        
        first_seg = x_GetRawSegFromSeg(aln_seg = GetSeg(range.GetFrom()));
        if ( !(flags & fDoNotTruncateSegs) ) {
            vec->m_LeftDelta = range.GetFrom() - GetAlnStart(aln_seg);
        }
    }
    if ((TSeqPos)range.GetTo() > GetAlnStop(GetNumSegs()-1)) {
        last_seg = m_NumSegs-1;
    } else {
        last_seg = x_GetRawSegFromSeg(aln_seg = GetSeg(range.GetTo()));
        if ( !(flags & fDoNotTruncateSegs) ) {
            vec->m_RightDelta = GetAlnStop(aln_seg) - range.GetTo();
        }
    }
    
    x_GetChunks(vec, row, first_seg, last_seg, flags);
    return vec;
}


CRef<CAlnMap::CAlnChunkVec>
CAlnMap::GetSeqChunks(TNumrow row, const TSignedRange& range,
                      TGetChunkFlags flags) const
{
    CRef<CAlnChunkVec> vec(new CAlnChunkVec(*this, row));

    // boundaries check
    if (range.GetTo() < GetSeqStart(row)  ||
        range.GetFrom() > GetSeqStop(row)) {
        return vec;
    }

    // determine the participating segments range
    TNumseg first_seg, last_seg;

    if (range.GetFrom() < GetSeqStart(row)) {
        if (IsPositiveStrand(row)) {
            first_seg = 0;
        } else {
            last_seg = m_NumSegs - 1;
        }
    } else {        
        if (IsPositiveStrand(row)) {
            first_seg = GetRawSeg(row, range.GetFrom());
            vec->m_LeftDelta = range.GetFrom() - x_GetRawStart(row, first_seg);
        } else {
            last_seg = GetRawSeg(row, range.GetFrom());
            vec->m_RightDelta = range.GetFrom() - x_GetRawStart(row, last_seg);
        }
    }
    if (range.GetTo() > GetSeqStop(row)) {
        if (IsPositiveStrand(row)) {
            last_seg = m_NumSegs - 1;
        } else {
            first_seg = 0;
        }
    } else {
        if (IsPositiveStrand(row)) {
            last_seg = GetRawSeg(row, range.GetTo());
            if ( !(flags & fDoNotTruncateSegs) ) {
                vec->m_RightDelta = x_GetRawStop(row, last_seg) - range.GetTo();
            }
        } else {
            first_seg = GetRawSeg(row, range.GetTo());
            if ( !(flags & fDoNotTruncateSegs) ) {
                vec->m_LeftDelta = x_GetRawStop(row, last_seg) - range.GetTo();
            }
        }
    }

    x_GetChunks(vec, row, first_seg, last_seg, flags);
    return vec;
}


inline
bool CAlnMap::x_SkipType(TSegTypeFlags type, TGetChunkFlags flags) const
{
    bool skip = false;
    if (type & fSeq) {
        if (type & fNotAlignedToSeqOnAnchor) {
            if (flags & fSkipInserts) {
                skip = true;
            }
        } else {
            if (flags & fSkipAlnSeq) {
                skip = true;
            }
        }
    } else {
        if (type & fNotAlignedToSeqOnAnchor) {
            if (flags & fSkipUnalignedGaps) {
                skip = true;
            }
        } else {
            if (flags & fSkipDeletions) {
                skip = true;
            }
        }
    }        
    return skip;
}


inline
bool
CAlnMap::x_CompareAdjacentSegTypes(TSegTypeFlags left_type, 
                                   TSegTypeFlags right_type,
                                   TGetChunkFlags flags) const
    // returns true if types are the same (as specified by flags)
{
    if (flags & fChunkSameAsSeg) {
        return false;
    }
        
    if ((left_type & fSeq) != (right_type & fSeq)) {
        return false;
    }
    if (!(flags & fIgnoreUnaligned)  &&
        (left_type & fUnalignedOnRight || right_type & fUnalignedOnLeft)) {
        return false;
    }
    if ((left_type & fNotAlignedToSeqOnAnchor) ==
        (right_type & fNotAlignedToSeqOnAnchor)) {
        return true;
    }
    if (left_type & fSeq) {
        if (!(flags & fInsertSameAsSeq)) {
            return false;
        }
    } else {
        if (!(flags & fDeletionSameAsGap)) {
            return false;
        }
    }
    return true;
}

void CAlnMap::x_GetChunks(CAlnChunkVec * vec,
                          TNumrow row,
                          TNumseg first_seg, TNumseg last_seg,
                          TGetChunkFlags flags) const
{
    TSegTypeFlags type, test_type;

    // add the participating segments to the vector
    for (TNumseg seg = first_seg;  seg <= last_seg;  seg++) {
        type = x_GetRawSegType(row, seg);

        // see if the segment needs to be skipped
        if (x_SkipType(type, flags)) {
            if (seg == first_seg) {
                vec->m_LeftDelta = 0;
            } else if (seg == last_seg) {
                vec->m_RightDelta = 0;
            }
            continue;
        }

        vec->m_StartSegs.push_back(seg); // start seg

        // find the stop seg
        TNumseg test_seg = seg;
        while (test_seg < last_seg) {
            test_seg++;
            test_type = x_GetRawSegType(row, test_seg);
            if (x_CompareAdjacentSegTypes(type, test_type, flags)) {
                seg = test_seg;
                continue;
            }

            // include included gaps if desired
            if (flags & fIgnoreGaps  &&  !(test_type & fSeq)  &&
                x_CompareAdjacentSegTypes(type & ~fSeq, test_type, flags)) {
                continue;
            }
            break;
        }
        vec->m_StopSegs.push_back(seg);
    }
}


CConstRef<CAlnMap::CAlnChunk>
CAlnMap::CAlnChunkVec::operator[](CAlnMap::TNumchunk i) const
{
    CAlnMap::TNumseg start_seg = m_StartSegs[i];
    CAlnMap::TNumseg stop_seg  = m_StopSegs[i];

    CRef<CAlnChunk>  chunk(new CAlnChunk());
    TSignedSeqPos from, to;
    from = m_AlnMap.m_Starts[start_seg * m_AlnMap.m_NumRows
                                     + m_Row];
    if (from >= 0) {
        to = from + m_AlnMap.x_GetLen(m_Row, start_seg) - 1;
    } else {
        from = -1;
        to = -1;
    }
    chunk->SetRange().Set(from, to);
    chunk->SetType(m_AlnMap.x_GetRawSegType(m_Row, start_seg));

    TSegTypeFlags type;
    for (CAlnMap::TNumseg seg = start_seg + 1;  seg <= stop_seg;  seg++) {
        type = m_AlnMap.x_GetRawSegType(m_Row, seg);
        if (type & fSeq) {
            // extend the range
            if (m_AlnMap.IsPositiveStrand(m_Row)) {
                chunk->SetRange().Set(chunk->GetRange().GetFrom(),
                                      chunk->GetRange().GetTo()
                                      + m_AlnMap.x_GetLen(m_Row, seg));
            } else {
                chunk->SetRange().Set(chunk->GetRange().GetFrom()
                                      - m_AlnMap.x_GetLen(m_Row, seg),
                                      chunk->GetRange().GetTo());
            }
        }
        // extend the type
        chunk->SetType(chunk->GetType() | type);
    }

    //determine the aln range
    {{
        // from position
        CNumSegWithOffset seg = m_AlnMap.x_GetSegFromRawSeg(start_seg);
        if (seg.GetAlnSeg() < 0) {
            // before the aln start
            from = -1;
        } else {
            if (seg.GetOffset() > 0) {
                // between aln segs
                from = m_AlnMap.GetAlnStop(seg.GetAlnSeg()) + 1;
            } else {
                // at an aln seg
                from = m_AlnMap.GetAlnStart(seg.GetAlnSeg()) +
                    (i == 0  &&  m_LeftDelta ? m_LeftDelta : 0);
            }
        }

        // to position
        seg = m_AlnMap.x_GetSegFromRawSeg(stop_seg);
        if (seg.GetAlnSeg() < 0) {
            // before the aln start
            to = 0;
        } else {
            if (seg.GetOffset() > 0) {
                // between aln segs
                to = m_AlnMap.GetAlnStop(seg.GetAlnSeg());
            } else {
                // at an aln seg
                to = m_AlnMap.GetAlnStop(seg.GetAlnSeg()) -
                    (i == size() - 1  &&  m_RightDelta ? m_RightDelta : 0);
            }
        }
        chunk->SetAlnRange().Set(from, to);
    }}


    // fix if extreme end
    if (i == 0 && m_LeftDelta) {
        if (!chunk->IsGap()) {
            if (m_AlnMap.IsPositiveStrand(m_Row)) {
                chunk->SetRange().Set
                    (chunk->GetRange().GetFrom()
                     + m_LeftDelta * m_AlnMap.GetWidth(m_Row),
                     chunk->GetRange().GetTo());
            } else {
                chunk->SetRange().Set(chunk->GetRange().GetFrom(),
                                      chunk->GetRange().GetTo()
                                      - m_LeftDelta
                                      * m_AlnMap.GetWidth(m_Row));
            }
            chunk->SetType(chunk->GetType() & ~fNoSeqOnLeft);
        }            
        chunk->SetType(chunk->GetType() & ~(fUnalignedOnLeft | fEndOnLeft));
    }
    if (i == size() - 1 && m_RightDelta) {
        if (!chunk->IsGap()) {
            if (m_AlnMap.IsPositiveStrand(m_Row)) {
                chunk->SetRange().Set
                    (chunk->GetRange().GetFrom(),
                     chunk->GetRange().GetTo()
                     - m_RightDelta * m_AlnMap.GetWidth(m_Row));
            } else {
                chunk->SetRange().Set
                    (chunk->GetRange().GetFrom()
                     + m_RightDelta * m_AlnMap.GetWidth(m_Row),
                     chunk->GetRange().GetTo());
            }
            chunk->SetType(chunk->GetType() & ~fNoSeqOnRight);
        }
        chunk->SetType(chunk->GetType() & ~(fUnalignedOnRight | fEndOnRight));
    }

    return chunk;
}


END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.48  2004/05/21 21:42:51  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.47  2004/03/03 19:39:22  todorov
* +GetResidueIndexMap
*
* Revision 1.46  2004/01/21 21:15:59  ucko
* Fix typos in last revision.
*
* Revision 1.45  2004/01/21 20:59:42  todorov
* fDoNotTruncate -> fDoNotTruncateSegs; +comments
*
* Revision 1.44  2004/01/21 20:53:35  todorov
* EGetChunkFlags += fDoNotTruncate
*
* Revision 1.43  2004/01/14 22:32:16  todorov
* Check for out of range seq_pos in GetSeqPosFromSeqPos
*
* Revision 1.42  2004/01/14 21:32:40  todorov
* Swapped aln range coords for insert chunks to insure proper range arithmetics
*
* Revision 1.41  2003/11/04 19:37:38  todorov
* Fixed GetRawSeg and GetAlnPosFromSeqPos in case of unaligned region
*
* Revision 1.40  2003/09/18 23:05:18  todorov
* Optimized GetSeqAln{Start,Stop}
*
* Revision 1.39  2003/09/17 16:26:35  todorov
* Fixed GetSeqPosFromSeqPos
*
* Revision 1.38  2003/09/10 22:53:37  todorov
* Use raw seg in GetSeqPosFromAlnPos
*
* Revision 1.37  2003/08/25 16:34:59  todorov
* exposed GetWidth
*
* Revision 1.36  2003/08/20 14:34:58  todorov
* Support for NA2AA Densegs
*
* Revision 1.35  2003/07/17 22:47:13  todorov
* name change
*
* Revision 1.34  2003/07/08 20:26:34  todorov
* Created seq end segments cache
*
* Revision 1.33  2003/06/09 17:47:26  todorov
* local var start type fixed in GetSeqStart,GetSeqStop
*
* Revision 1.32  2003/06/05 21:14:15  todorov
* Type fixed
*
* Revision 1.31  2003/06/05 19:59:33  todorov
* Fixed a few inefficiencies
*
* Revision 1.30  2003/06/05 19:03:12  todorov
* Added const refs to Dense-seg members as a speed optimization
*
* Revision 1.29  2003/06/02 16:06:40  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.28  2003/05/23 18:10:47  todorov
* +fChunkSameAsSeg
*
* Revision 1.27  2003/03/20 16:38:21  todorov
* +fIgnoreGaps for GetXXXChunks
*
* Revision 1.26  2003/03/07 17:30:26  todorov
* + ESearchDirection dir, bool try_reverse_dir for GetAlnPosFromSeqPos
*
* Revision 1.25  2003/03/04 16:19:05  todorov
* Added advance search options for GetRawSeg
*
* Revision 1.24  2003/01/31 17:22:14  todorov
* Fixed a typo
*
* Revision 1.23  2003/01/15 18:48:30  todorov
* Added GetSeqChunks to be used with native seq range
*
* Revision 1.22  2003/01/08 23:04:47  todorov
* Fixed a bug in x_CompareAdjacentSegTypes
*
* Revision 1.21  2003/01/03 17:00:41  todorov
* Fixed negative strand handling in GetRawSeg
*
* Revision 1.20  2002/12/20 21:25:14  todorov
* ... and another small fix
*
* Revision 1.19  2002/12/20 20:30:42  todorov
* Fixed a bug introduced in the previous revision
*
* Revision 1.18  2002/12/19 20:24:53  grichenk
* Updated usage of CRange<>
*
* Revision 1.17  2002/11/13 16:40:56  todorov
* out of range check for GetAlnPosFromSeqPos
*
* Revision 1.16  2002/11/04 21:29:08  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.15  2002/10/24 21:27:29  todorov
* out-of-range adjustment instead of return -1 for the GetSeqPosFromAlnPos
*
* Revision 1.14  2002/10/21 19:14:27  todorov
* reworked aln chunks: now supporting more types; added chunk aln coords
*
* Revision 1.13  2002/10/10 17:23:43  todorov
* switched back to one (but this time enhanced) GetSeqPosFromAlnPos method
*
* Revision 1.12  2002/10/04 16:38:06  todorov
* new method GetBestSeqPosFromAlnPos
*
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
