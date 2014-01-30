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
    TNumseg       l_anchor_index, r_anchor_index, anchor_index;
    TSeqPos       cont_next_start = 0, cont_prev_stop = 0;
    TSeqPos       anchor_cont_next_start = 0, anchor_cont_prev_stop = 0;
    TSignedSeqPos anchor_start;

    l_seg = r_seg = seg;
    l_index = r_index = index = seg * m_NumRows + row;
    if (IsSetAnchor()) {
        l_anchor_index = r_anchor_index = anchor_index = seg * m_NumRows + m_Anchor;
    }

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
        anchor_start = m_Starts[anchor_index];
        if (anchor_start >= 0) {
            flags &= ~(flags & fNotAlignedToSeqOnAnchor);
            anchor_cont_next_start = anchor_start + x_GetLen(m_Anchor, seg);
            anchor_cont_prev_stop  = anchor_start;
        }
    }

    // what's on the right?
    if (r_seg <= m_NumSegs - 1) {
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
    if (IsSetAnchor()  && 
        !(flags & fNotAlignedToSeqOnAnchor)) {
        r_seg = seg;
        while (++r_seg < m_NumSegs) {
            r_anchor_index += m_NumRows;
            if ((anchor_start = m_Starts[r_anchor_index]) >= 0) {
                if (IsPositiveStrand(m_Anchor) ?
                    anchor_start != (TSignedSeqPos)anchor_cont_next_start :
                    anchor_start + x_GetLen(m_Anchor, r_seg) != anchor_cont_prev_stop) {
                    flags |= fUnalignedOnRightOnAnchor;
                }
                break;
            }
        }
    }

    // what's on the left?
    if (l_seg >= 0) {
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
    if (IsSetAnchor()  && 
        !(flags & fNotAlignedToSeqOnAnchor)) {
        l_seg = seg;
        while (--l_seg >= 0) {
            l_anchor_index -= m_NumRows;
            if ((anchor_start = m_Starts[l_anchor_index]) >= 0) {
                if (IsPositiveStrand(m_Anchor) ?
                    anchor_start + x_GetLen(m_Anchor, l_seg) != anchor_cont_prev_stop :
                    anchor_start != (TSignedSeqPos)anchor_cont_next_start) {
                    flags |= fUnalignedOnLeftOnAnchor;
                }
                break;
            }
        }
    }

    return flags;
}


void
CAlnMap::x_SetRawSegTypes(TNumrow row) const
{
    TRawSegTypes& types = x_GetRawSegTypes();

    /// Check if already done (enough to check the first seg only)
    if (types[row] & fTypeIsSet) {
        return;
    }

    /// Strand
    bool plus = IsPositiveStrand(row);

    /// Variables
    TNumseg seg;
    int idx;
    int l_idx = row;
    int r_idx = (m_NumSegs - 1) * m_NumRows + row;
    int anchor_idx = -1;

    /// Determine types for the anchor first
    bool anchored = IsSetAnchor();
    if (anchored) {
        if (row != m_Anchor) {
            /// Prevent infinite loop
            x_SetRawSegTypes(m_Anchor);
        }
        anchor_idx = m_Anchor;
    }


    /// Ends are trivial
    types[l_idx] |= fEndOnLeft;
    types[r_idx] |= fEndOnRight;


    /// Left-to-right pass
    int left_seq_pos = -1;
    for (idx = l_idx, seg = 0;
         idx <= r_idx;
         idx += m_NumRows, anchor_idx += m_NumRows, ++seg) {

        _ASSERT(idx == seg * m_NumRows + row);
        _ASSERT(anchor_idx == seg * m_NumRows + m_Anchor);
        _ASSERT(seg >= 0);
        _ASSERT(seg < m_NumSegs);

        TSegTypeFlags& flags = types[idx];

        /// Sequence on left
        if (left_seq_pos < 0) {
            flags |= fNoSeqOnLeft;
        }

        /// Sequence or Gap?
        TSignedSeqPos start = m_Starts[idx];
        if (start >= 0) {

            /// Sequence
            flags |= fSeq;

            /// Unaligned on left?
            if (left_seq_pos > 0) {
                if (plus ? 
                    start > left_seq_pos :
                    start + x_GetLen(row, seg) < (TSeqPos) left_seq_pos) {
                    flags |= fUnalignedOnLeft;
                }
            }
            left_seq_pos = plus ? start + x_GetLen(row, seg) : start;

        } else {  /// Gap

            if (anchored  &&  row == m_Anchor) {
                flags |= fNotAlignedToSeqOnAnchor;
            }

        }
    }


    /// Right-to-left pass
    int right_seq_pos = -1;
    anchor_idx -= m_NumRows; // this relies on value from previous loop
    _ASSERT(anchor_idx == (m_NumSegs - 1) * m_NumRows + m_Anchor);
    for (idx = r_idx, seg = m_NumSegs - 1;
         idx >= l_idx;
         idx -= m_NumRows, anchor_idx -= m_NumRows, --seg) {

        _ASSERT(idx == seg * m_NumRows + row);
        _ASSERT(anchor_idx == seg * m_NumRows + m_Anchor);
        _ASSERT(seg >= 0);
        _ASSERT(seg < m_NumSegs);

        TSegTypeFlags& flags = types[idx];

        /// Sequence on right
        if (right_seq_pos < 0) {
            flags |= fNoSeqOnRight;
        }


        TSignedSeqPos start = m_Starts[idx];
        if (start >= 0) {

            /// Sequence
            _ASSERT(flags | fSeq);

            /// Unaligned on right?
            if (right_seq_pos > 0) {
                if (plus ? 
                    start + x_GetLen(row, seg) < (TSeqPos) right_seq_pos :
                    start > right_seq_pos) {
                    flags |= fUnalignedOnRight;
                }
            }
            right_seq_pos = plus ? start : start + x_GetLen(row, seg);
        }


         /// What's on the anchor?
        if (anchored) {
            if ( ! (types[anchor_idx] & fSeq) ) {
                flags |= fNotAlignedToSeqOnAnchor;
            }
            if (types[anchor_idx] & fUnalignedOnRight) {
                flags |= fUnalignedOnRightOnAnchor;
            }
            if (types[anchor_idx] & fUnalignedOnLeft) {
                flags |= fUnalignedOnLeftOnAnchor;
            }
        }
        
        /// Done with this segment
        flags |= fTypeIsSet;
    }
}


CAlnMap::TNumseg CAlnMap::GetSeg(TSeqPos aln_pos) const
{
    TNumseg btm, top, mid;

    btm = 0;
    top = TNumseg(m_AlnStarts.size()) - 1;

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
    if (sseq_pos < (TSignedSeqPos)GetSeqStart(row)) {
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
    } else if (sseq_pos > (TSignedSeqPos)GetSeqStop(row)) {
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

    // return either -1 or the closest segment in dir direction
    if (dir == eNone) {
        return -1;
    } else if (dir == eBackwards  ||  dir == (plus ? eLeft : eRight)) {
        return (plus ? cur_btm : last - cur_btm);
    } else if (dir == eForward  ||  dir == (plus ? eRight : eLeft)) {
        return (plus ? cur_top : last - cur_top);
    }

    return -1;
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

TSignedSeqPos CAlnMap::x_FindClosestSeqPos(TNumrow row,
                                           TNumseg seg,
                                           ESearchDirection dir,
                                           bool try_reverse_dir) const
{
    _ASSERT(x_GetRawStart(row, seg) == -1);
    TNumseg orig_seg = seg;
    TSignedSeqPos pos = -1;
    bool reverse_pass = false;

    while (true) {
        if (IsPositiveStrand(row)) {
            if (dir == eBackwards  ||  dir == eLeft) {
                while (--seg >=0  &&  pos == -1) {
                    pos = x_GetRawStop(row, seg);
                }
            } else {
                while (++seg < m_NumSegs  &&  pos == -1) {
                    pos = x_GetRawStart(row, seg);
                }
            }
        } else {
            if (dir == eForward  ||  dir == eLeft) {
                while (--seg >=0  &&  pos == -1) {
                    pos = x_GetRawStart(row, seg);
                }
            } else {
                while (++seg < m_NumSegs  &&  pos == -1) {
                    pos = x_GetRawStop(row, seg);
                } 
            }
        }
        if (!try_reverse_dir) {
            break;
        }
        if (pos >= 0) {
            break; // found
        } else if (reverse_pass) {
            string msg = "Invalid Dense-seg: Row " +
                NStr::IntToString(row) +
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
        default:
            break;
        }
    }
    return pos;
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
        // found a gap, search in the neighbouring segments
        // according to search direction (dir) and strand
        pos = x_FindClosestSeqPos(for_row, x_GetRawSegFromSeg(seg), dir, try_reverse_dir);
    }
    return pos;
}

TSignedSeqPos CAlnMap::GetSeqPosFromSeqPos(TNumrow for_row,
                                           TNumrow row, TSeqPos seq_pos,
                                           ESearchDirection dir,
                                           bool try_reverse_dir)  const
{
    TNumseg raw_seg = GetRawSeg(row, seq_pos);
    if (raw_seg < 0) {
        return -1;
    }
    unsigned offset = raw_seg * m_NumRows;
    TSignedSeqPos pos = m_Starts[offset + for_row];
    if (pos >= 0) {
        TSeqPos delta = seq_pos - m_Starts[offset + row];
        if (GetWidth(for_row) != GetWidth(row)) {
            delta = delta / GetWidth(row) * GetWidth(for_row);
        }
        if (StrandSign(row) == StrandSign(for_row)) {
            pos += delta;
        } else {
            pos += x_GetLen(for_row, raw_seg) - 1 - delta;
        }
    } else {
        pos = x_FindClosestSeqPos(for_row, raw_seg, dir, try_reverse_dir);
    }
    return pos;
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
    TSeqPos aln_pos = 0, next_aln_pos, l_len = 0, r_len = 0, l_delta, r_delta;
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
    _ASSERT(aln_start >= aln_pos);
    l_delta = aln_start - aln_pos;
    l_len -= l_delta;
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
    _ASSERT(aln_pos >= aln_stop);
    r_delta = aln_pos - aln_stop;
    r_len -= r_delta;
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
    TSignedSeqPos delta;
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
        while (++seg < (TNumseg) m_AlnSegIdx.size()) {
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
        TNumseg seg = TNumseg(m_AlnSegIdx.size());
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
    TNumseg left_seg, right_seg, aln_seg;

    if (range.GetFrom() < 0) {
        left_seg = 0;
    } else {        
        left_seg = x_GetRawSegFromSeg(aln_seg = GetSeg(range.GetFrom()));
        if ( !(flags & fDoNotTruncateSegs) ) {
            vec->m_LeftDelta = range.GetFrom() - GetAlnStart(aln_seg);
        }
    }
    if ((TSeqPos)range.GetTo() > GetAlnStop(GetNumSegs()-1)) {
        right_seg = m_NumSegs-1;
    } else {
        right_seg = x_GetRawSegFromSeg(aln_seg = GetSeg(range.GetTo()));
        if ( !(flags & fDoNotTruncateSegs) ) {
            vec->m_RightDelta = GetAlnStop(aln_seg) - range.GetTo();
        }
    }
    
    x_GetChunks(vec, row, left_seg, right_seg, flags);
    return vec;
}


CRef<CAlnMap::CAlnChunkVec>
CAlnMap::GetSeqChunks(TNumrow row, const TSignedRange& range,
                      TGetChunkFlags flags) const
{
    CRef<CAlnChunkVec> vec(new CAlnChunkVec(*this, row));

    // boundaries check
    if (range.GetTo() < (TSignedSeqPos)GetSeqStart(row)  ||
        range.GetFrom() > (TSignedSeqPos)GetSeqStop(row)) {
        return vec;
    }

    // determine the participating segments range
    TNumseg left_seg = 0, right_seg = m_NumSegs - 1;

    if (range.GetFrom() >= (TSignedSeqPos)GetSeqStart(row)) {
        if (IsPositiveStrand(row)) {
            left_seg = GetRawSeg(row, range.GetFrom());
            vec->m_LeftDelta = range.GetFrom() - x_GetRawStart(row, left_seg);
        } else {
            right_seg = GetRawSeg(row, range.GetFrom());
            vec->m_RightDelta = range.GetFrom() - x_GetRawStart(row, right_seg);
        }
    }
    if (range.GetTo() <= (TSignedSeqPos)GetSeqStop(row)) {
        if (IsPositiveStrand(row)) {
            right_seg = GetRawSeg(row, range.GetTo());
            if ( !(flags & fDoNotTruncateSegs) ) {
                vec->m_RightDelta = x_GetRawStop(row, right_seg) - range.GetTo();
            }
        } else {
            left_seg = GetRawSeg(row, range.GetTo());
            if ( !(flags & fDoNotTruncateSegs) ) {
                vec->m_LeftDelta = x_GetRawStop(row, right_seg) - range.GetTo();
            }
        }
    }

    x_GetChunks(vec, row, left_seg, right_seg, flags);
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
        (left_type & fUnalignedOnRight || right_type & fUnalignedOnLeft ||
         left_type & fUnalignedOnRightOnAnchor || right_type & fUnalignedOnLeftOnAnchor)) {
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
                          TNumseg left_seg, TNumseg right_seg,
                          TGetChunkFlags flags) const
{
    TSegTypeFlags type, test_type;

    _ASSERT(left_seg <= right_seg);

    size_t hint_idx = m_NumRows * left_seg + row;

    // add the participating segments to the vector
    for (TNumseg seg = left_seg;  seg <= right_seg;  seg++, hint_idx += m_NumRows) {
        type = x_GetRawSegType(row, seg, int(hint_idx));
    
        // see if the segment needs to be skipped
        if (x_SkipType(type, flags)) {
            if (seg == left_seg) {
                vec->m_LeftDelta = 0;
            } else if (seg == right_seg) {
                vec->m_RightDelta = 0;
            }
            continue;
        }

        vec->m_StartSegs.push_back(seg); // start seg

        // find the stop seg
        TNumseg test_seg = seg;
        int test_hint_idx = int(hint_idx);
        while (test_seg < right_seg) {
            test_seg++;
            test_hint_idx += m_NumRows;
            test_type = x_GetRawSegType(row, test_seg, test_hint_idx);
            if (x_CompareAdjacentSegTypes(type, test_type, flags)) {
                seg = test_seg;
                hint_idx = test_hint_idx;
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

        // add unaligned chunk if needed
        if (flags & fAddUnalignedChunks  &&
            type & fUnalignedOnRight) {
            vec->m_StartSegs.push_back(seg+1);
            vec->m_StopSegs.push_back(seg);
        }
    }
}


CConstRef<CAlnMap::CAlnChunk>
CAlnMap::CAlnChunkVec::operator[](CAlnMap::TNumchunk i) const
{
    CAlnMap::TNumseg start_seg = m_StartSegs[i];
    CAlnMap::TNumseg stop_seg  = m_StopSegs[i];

    CRef<CAlnChunk>  chunk(new CAlnChunk());

    if (start_seg > stop_seg) {
        // flipped segs means this is unaligned region;
        // deal with it specially

        // type
        chunk->SetType(fUnaligned | fInsert);
        
        TSignedSeqPos 
            l_from   = -1, 
            l_to     = -1,
            r_from   = -1,
            r_to     = -1,
            aln_from = -1, 
            aln_to   = -1;
        TSegTypeFlags type;

        // explore on the left
        for (TNumseg l_seg = start_seg - 1, idx = l_seg * m_AlnMap.m_NumRows + m_Row;
             l_seg >= 0;
             --l_seg, idx -= m_AlnMap.m_NumRows) {
            
            type = m_AlnMap.x_GetRawSegType(m_Row, l_seg, idx);
            if (type & fSeq  &&  l_from == -1) {
                l_from = m_AlnMap.m_Starts[idx];
                l_to = l_from + m_AlnMap.x_GetLen(m_Row, l_seg) - 1;
                if (aln_to != -1) {
                    break;
                }
            }
            if ( !(type & fNotAlignedToSeqOnAnchor)  &&  type & fSeq  &&
                 aln_to == -1) {
                aln_to = m_AlnMap.GetAlnStop
                    (m_AlnMap.x_GetSegFromRawSeg(l_seg).GetAlnSeg());
                if (l_from != - 1) {
                    break;
                }
            }
        }

        // explore on the right
        for (TNumseg r_seg = stop_seg + 1, idx = r_seg * m_AlnMap.m_NumRows + m_Row;
             r_seg < m_AlnMap.m_NumSegs;
             ++r_seg, idx += m_AlnMap.m_NumRows) {

            type = m_AlnMap.x_GetRawSegType(m_Row, r_seg, idx);
            if (type & fSeq  &&  r_from == -1) {
                r_from = m_AlnMap.m_Starts[idx];
                r_to = r_from + m_AlnMap.x_GetLen(m_Row, r_seg) - 1;
                if (aln_from != -1) {
                    break;
                }
            }
            if ( !(type & fNotAlignedToSeqOnAnchor)  &&  type & fSeq  &&
                 aln_from == -1) {
                aln_from = m_AlnMap.GetAlnStart
                    (m_AlnMap.x_GetSegFromRawSeg(r_seg).GetAlnSeg());
                if (r_from != - 1) {
                    break;
                }
            }
        }

        if (m_AlnMap.IsPositiveStrand(m_Row)) {
            chunk->SetRange().Set(l_to + 1, r_from - 1);
        } else {
            chunk->SetRange().Set(r_to + 1, l_from - 1);
        }
        chunk->SetAlnRange().Set(aln_from, aln_to);
         
        return chunk;
    }

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

    int idx = start_seg * m_AlnMap.m_NumRows + m_Row;
    chunk->SetType(m_AlnMap.x_GetRawSegType(m_Row, start_seg, idx));
    idx += m_AlnMap.m_NumRows;

    TSegTypeFlags type;
    for (CAlnMap::TNumseg seg = start_seg + 1;
         seg <= stop_seg;
         ++seg, idx += m_AlnMap.m_NumRows) {

        type = m_AlnMap.x_GetRawSegType(m_Row, seg, idx);
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
            from = 0;
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
            to = -1;
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


CRef<CSeq_align> CAlnMap::CreateAlignFromRange(
    const vector<TNumrow>& selected_rows,
    TSignedSeqPos          aln_from,
    TSignedSeqPos          aln_to,
    ESegmentTrimFlag       seg_flag)
{
    CRef<CSeq_align> ret(new CSeq_align);
    ret->SetType(CSeq_align::eType_partial);
    CDense_seg& ds = ret->SetSegs().SetDenseg();

    bool have_strands = !m_Strands.empty();
    bool have_widths = !m_Widths.empty();

    // Initialize selected rows
    size_t dim = selected_rows.size();
    ret->SetDim(CSeq_align::TDim(dim));
    ds.SetDim(CDense_seg::TDim(dim));
    ds.SetIds().resize(dim);
    if ( have_widths ) {
        ds.SetWidths().resize(dim);
    }
    for (size_t i = 0; i < dim; i++) {
        TNumrow r = selected_rows[i];
        _ASSERT(r < m_NumRows);
        ds.SetIds()[i] = m_Ids[r];
        if ( have_widths ) {
            ds.SetWidths()[i] = m_Widths[r];
        }
    }
    TNumseg from_seg = GetSeg(aln_from);
    TNumseg to_seg = GetSeg(aln_to);
    if (from_seg < 0) {
        from_seg = 0;
        aln_from = 0;
    }
    if (to_seg < 0) {
        to_seg = m_NumSegs - 1;
        aln_to = GetAlnStop();
    }

    TNumseg num_seg = 0;
    CDense_seg::TStarts& starts = ds.SetStarts();
    for (TNumseg seg = from_seg; seg <= to_seg; seg++) {
        TSeqPos len = GetLen(seg);

        // Check trimming of the first segment
        TSeqPos aln_seg_from = GetAlnStart(seg);
        TSeqPos from_trim = 0;
        if (seg == from_seg  &&  TSeqPos(aln_from) > aln_seg_from) {
            if (seg_flag == eSegment_Remove) {
                continue; // ignore incomplete segments
            }
            if (seg_flag == eSegment_Trim) {
                from_trim = aln_from - aln_seg_from;
                len -= from_trim;
                aln_seg_from = aln_from;
            }
        }
        // Check trimming of the last segment
        if (seg == to_seg) {
            TSeqPos aln_seg_to = GetAlnStop(seg);
            if (aln_seg_to > TSeqPos(aln_to)) {
                if (seg_flag == eSegment_Remove) {
                    continue; // ignore incomplete segments
                }
                if (seg_flag == eSegment_Trim) {
                    len -= aln_seg_to - aln_to;
                    aln_seg_to = aln_to;
                }
            }
        }
        ds.SetLens().push_back(len);
        // Copy rows to the destination
        for (TNumrow row = 0; row < selected_rows.size(); row++) {
            TSignedSeqPos row_start = GetStart(selected_rows[row], seg);
            if (row_start >= 0) {
                row_start += from_trim;
            }
            starts.push_back(row_start);
            if ( have_strands ) {
                ds.SetStrands().push_back(
                    m_Strands[seg*m_NumRows + selected_rows[row]]);
            }
        }
        num_seg++;
    }

    // Ignore scores - if anythign was trimmed (and it probably was),
    // all scores are now useless.

    if (num_seg > 0) {
        ds.SetNumseg(num_seg);
    }
    else {
        ret.Reset();
    }
    return ret;
}


END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE
