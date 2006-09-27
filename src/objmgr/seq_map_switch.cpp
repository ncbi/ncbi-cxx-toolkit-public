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
* Authors:
*           Eugene Vasilchenko
*
* File Description:
*   Working with seq-map switch points
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/seq_map_switch.hpp>
#include <objmgr/seq_map.hpp>
#include <objmgr/seq_map_ci.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/impl/seq_align_mapper.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScope;

BEGIN_SCOPE()

struct SSeqPos;

struct SSeqPos
{
    CSeq_id_Handle id;
    TSeqPos pos;
    bool minus_strand;

    enum ESeqMapSegmentEdge
    {
        eStart,
        eEnd
    };

    SSeqPos(const CSeqMap_CI& iter, ESeqMapSegmentEdge edge)
        : id(iter.GetRefSeqid()),
          minus_strand(iter.GetRefMinusStrand())
        {
            if ( edge == eStart ) {
                if ( !minus_strand ) {
                    pos = iter.GetRefPosition();
                }
                else {
                    pos = iter.GetRefEndPosition() - 1;
                }
            }
            else {
                if ( !minus_strand ) {
                    pos = iter.GetRefEndPosition();
                }
                else {
                    pos = iter.GetRefPosition() - 1;
                }
            }
        }

    SSeqPos& operator+=(int diff)
        {
            pos += minus_strand? -diff: diff;
            return *this;
        }
    SSeqPos& operator-=(int diff)
        {
            pos += minus_strand? diff: -diff;
            return *this;
        }

    void Reverse(void)
        {
            *this -= 1;
            minus_strand = !minus_strand;
        }
};
CNcbiOstream& operator<<(CNcbiOstream& out, const SSeqPos& pos)
{
    return out << pos.id.AsString() << " @ "
               << pos.pos << (pos.minus_strand? " minus": " plus");
}

struct SSeq_align_Info
{
    SSeq_align_Info()
        {
        }
    SSeq_align_Info(const CSeq_align& align, CScope& scope)
        {
            Add(align, scope);
        }
    void Add(const CSeq_align& align, CScope& scope)
        {
            CSeq_align_Mapper mapper(align, false, &scope);
            ITERATE ( CSeq_align_Mapper::TSegments, s, mapper.GetSegments() ) {
                ITERATE ( SAlignment_Segment::TRows, r1, s->m_Rows ) {
                    if ( r1->m_Start == kInvalidSeqPos ) {
                        continue;
                    }
                    ITERATE ( SAlignment_Segment::TRows, r2, s->m_Rows ) {
                        if ( r2 == r1 ) {
                            break;
                        }
                        if ( r2->m_Start == kInvalidSeqPos ) {
                            continue;
                        }
                        TMatches& matches = GetMatches(r1->m_Id, r2->m_Id);
                        SMatch match;
                        match.id1 = r1->m_Id;
                        match.range1.SetFrom(r1->m_Start);
                        match.range1.SetLength(s->m_Len);
                        match.id2 = r2->m_Id;
                        match.range2.SetFrom(r2->m_Start);
                        match.range2.SetLength(s->m_Len);
                        match.same_strand = r1->SameStrand(*r2);
                        matches.push_back(match);
                    }
                }
            }
        }

    struct SMatch
    {
        CSeq_id_Handle id1;
        CRange<TSeqPos> range1;
        CSeq_id_Handle id2;
        CRange<TSeqPos> range2;
        bool same_strand;

        static bool Contains(const CRange<TSeqPos>& range, TSeqPos pos)
            {
                return pos >= range.GetFrom() && pos <= range.GetTo();
            }
        static TSeqPos GetAdd(const CRange<TSeqPos>& range, const SSeqPos& pos)
            {
                _ASSERT(Contains(range, pos.pos));
                if ( pos.minus_strand ) {
                    return pos.pos - range.GetFrom() + 1;
                }
                else {
                    return range.GetTo() - pos.pos + 1;
                }
            }

        TSeqPos GetMatch2(const SSeqPos& pos1, const SSeqPos& pos2) const
            {
                if ( Contains(range1, pos1.pos) &&
                     Contains(range2, pos2.pos) &&
                     same_strand == (pos1.minus_strand==pos2.minus_strand )) {
                    return min(GetAdd(range1, pos1), GetAdd(range2, pos2));
                }
                return 0;
            }

        TSeqPos GetMatch(const SSeqPos& pos1, const SSeqPos& pos2) const
            {
                if ( id1 == pos1.id && id2 == pos2.id )
                    return GetMatch2(pos1, pos2);
                if ( id1 == pos2.id && id2 == pos1.id )
                    return GetMatch2(pos2, pos1);
                return 0;
            }
    };
    typedef vector<SMatch> TMatches;
    typedef pair<CSeq_id_Handle, CSeq_id_Handle> TKey;
    typedef map<TKey, TMatches> TMatchMap;

    static TKey GetKey(const CSeq_id_Handle& id1, const CSeq_id_Handle& id2)
        {
            TKey key;
            if ( id1 < id2 ) {
                key.first = id1;
                key.second = id2;
            }
            else {
                key.first = id2;
                key.second = id1;
            }
            return key;
        }
    TMatches& GetMatches(const CSeq_id_Handle& id1, const CSeq_id_Handle& id2)
        {
            return match_map[GetKey(id1, id2)];
        }

    TMatchMap match_map;
};

TSeqPos x_FindAlignMatch(SSeq_align_Info& info,
                         SSeqPos pos1, SSeqPos pos2,
                         TSeqPos limit)
{
    TSeqPos ret = 0;
    while ( limit ) {
        TSeqPos add = 0;

        const SSeq_align_Info::TMatches& matches =
            info.GetMatches(pos1.id, pos2.id);
        ITERATE ( SSeq_align_Info::TMatches, it, matches ) {
            add = it->GetMatch(pos1, pos2);
            if ( add ) {
                break;
            }
        }
        if ( add == 0 ) {
            break;
        }
        add = min(add, limit);
        ret += add;
        limit -= add;
        pos1 += add;
        pos2 += add;
    }
    return ret;
}

CSeqMap_CI x_FindSwitchPos(const CBioseq_Handle& seq,
                           CSeq_id_Handle& id1, CSeq_id_Handle& id2)
{
    CSeqMap_CI iter1 = seq.GetSeqMap().begin(&seq.GetScope());
    for ( ; iter1; ++iter1 ) {
        if ( iter1.GetType() == CSeqMap::eSeqRef ) {
            if ( iter1.GetRefSeqid() == id1 ) {
                break;
            }
            else if ( iter1.GetRefSeqid() == id2 ) {
                swap(id1, id2);
                break;
            }
        }
    }
    if ( !iter1 ) {
        NCBI_THROW(CSeqMapException, eInvalidIndex,
                   "Seq-align doesn't refer to segments");
    }
    CSeqMap_CI iter2 = iter1;
    if ( !++iter2 || iter2.GetType() != CSeqMap::eSeqRef ||
         iter2.GetRefSeqid() != id2 ) {
        NCBI_THROW(CSeqMapException, eInvalidIndex,
                   "Seq-align doesn't refer to segments");
    }
    return iter1;
}

SSeqMapSwitchPoint x_GetSwitchPoint(const CBioseq_Handle& seq,
                                    SSeq_align_Info& info,
                                    const CSeqMap_CI& iter1,
                                    const CSeqMap_CI& iter2)
{
    SSeqMapSwitchPoint sp;
    sp.m_Master = seq;
    sp.m_LeftId = iter1.GetRefSeqid();
    sp.m_RightId = iter2.GetRefSeqid();
    TSeqPos pos = iter2.GetPosition();
    _ASSERT(pos == iter1.GetEndPosition());
    sp.m_MasterPos = pos;
    SSeqPos pos1(iter1, SSeqPos::eEnd);
    SSeqPos pos2(iter2, SSeqPos::eStart);
    TSeqPos ext2 = x_FindAlignMatch(info, pos1, pos2, iter2.GetLength());
    pos1.Reverse();
    pos2.Reverse();
    TSeqPos ext1 = x_FindAlignMatch(info, pos1, pos2, iter1.GetLength());
    sp.m_MasterRange.SetFrom(pos-ext1).SetTo(pos+ext2);
    return sp;
}

END_SCOPE()

class CScope;

// calculate switch point for two segments specified by align
SSeqMapSwitchPoint GetSwitchPoint(const CBioseq_Handle& seq,
                                  const CSeq_align& align)
{
    SSeq_align_Info info(align, seq.GetScope());
    if ( info.match_map.size() != 1 ) {
        NCBI_THROW(CSeqMapException, eInvalidIndex,
                   "Seq-align dimension is not 2");
    }
    CSeq_id_Handle id1 = info.match_map.begin()->first.first;
    CSeq_id_Handle id2 = info.match_map.begin()->first.second;

    CSeqMap_CI iter1 = seq.GetSeqMap().begin(&seq.GetScope());
    if ( !iter1 ) {
        NCBI_THROW(CSeqMapException, eInvalidIndex,
                   "Sequence is not segmented");
    }
    CSeqMap_CI iter2 = iter1;
    ++iter2;

    for ( ; iter2; ++iter1, ++iter2 ) {
        if ( iter1.GetType() == CSeqMap::eSeqRef &&
             iter2.GetType() == CSeqMap::eSeqRef ) {
            if ( iter1.GetRefSeqid() == id1 && iter2.GetRefSeqid() == id2 ||
                 iter1.GetRefSeqid() == id2 && iter2.GetRefSeqid() == id1 ) {
                return x_GetSwitchPoint(seq, info, iter1, iter2);
            }
        }
    }
}

// calculate all sequence switch points using set of Seq-aligns
TSeqMapSwitchPoints GetAllSwitchPoints(const CBioseq_Handle& seq,
                                       const TSeqMapSwitchAligns& aligns)
{
    TSeqMapSwitchPoints pp;

    CSeqMap_CI iter1 = seq.GetSeqMap().begin(&seq.GetScope());
    if ( !iter1 ) {
        NCBI_THROW(CSeqMapException, eInvalidIndex,
                   "Sequence is not segmented");
    }
    CSeqMap_CI iter2 = iter1;
    ++iter2;

    SSeq_align_Info info;
    ITERATE ( TSeqMapSwitchAligns, it, aligns ) {
        info.Add(**it, seq.GetScope());
    }

    for ( ; iter2; ++iter1, ++iter2 ) {
        if ( iter1.GetType() == CSeqMap::eSeqRef &&
             iter2.GetType() == CSeqMap::eSeqRef ) {
            pp.push_back(x_GetSwitchPoint(seq, info, iter1, iter2));
        }
    }
    return pp;
}

// calculate all sequence switch points using set of Seq-aligns from assembly
vector<SSeqMapSwitchPoint> GetAllSwitchPoints(const CBioseq_Handle& seq)
{
    return GetAllSwitchPoints(seq, seq.GetInst_Hist().GetAssembly());
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2006/09/27 21:28:58  vasilche
* Added functions to calculate switch points.
*
* ===========================================================================
*/
