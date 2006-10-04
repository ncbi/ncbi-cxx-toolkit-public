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

namespace {

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

        struct SMatchInfo
        {
            SMatchInfo(void)
                : skip(true), m1(kInvalidSeqPos), m2(kInvalidSeqPos)
                {
                }
            bool skip;
            TSeqPos m1, m2;
            bool operator!(void) const
                {
                    return skip &&
                        (m1 == kInvalidSeqPos || m2 == kInvalidSeqPos);
                }
            bool operator<(const SMatchInfo& m) const
                {
                    if ( skip != m.skip )
                        return m.skip;
                    return m1+m2 < m.m1+m.m2;
                }
        };

        typedef SMatchInfo TMatch;

        // returns skip in first, match in second
        static CRange<int> GetMatchPos(const CRange<TSeqPos>& range,
                                                  const SSeqPos& pos)
            {
                CRange<int> ret;
                if ( pos.minus_strand ) {
                    ret.SetFrom(pos.pos - range.GetTo());
                }
                else {
                    ret.SetFrom(range.GetFrom() - pos.pos);
                }
                ret.SetLength(range.GetLength());
                return ret;
            }
        TMatch GetMatchOrdered(const SSeqPos& pos1, const SSeqPos& pos2) const
            {
                TMatch ret;
                if ( same_strand != (pos1.minus_strand==pos2.minus_strand) ) {
                     return ret;
                }
                CRange<int> m1 = GetMatchPos(range1, pos1);
                CRange<int> m2 = GetMatchPos(range2, pos2);
                //_TRACE("range1: "<<range1<<" pos1: "<<pos1<<" m1: "<<m1);
                //_TRACE("range2: "<<range2<<" pos2: "<<pos2<<" m2: "<<m2);
                if ( m1.GetTo() < 0 || m2.GetTo() < 0 ) {
                    return ret;
                }
                int l1 = m1.GetTo()-max(0, m1.GetFrom());
                int l2 = m2.GetTo()-max(0, m2.GetFrom());
                if ( l1 != l2 ) {
                    return ret;
                }
                if ( m1.GetFrom() <= 0 && m2.GetFrom() <= 0 ) {
                    ret.skip = false;
                    ret.m1 = ret.m2 = m1.GetTo()+1;
                }
                else {
                    ret.m1 = m1.GetFrom();
                    ret.m2 = m2.GetFrom();
                }
                return ret;
            }

        TMatch GetMatch(const SSeqPos& pos1, const SSeqPos& pos2) const
            {
                if ( pos1.id == id1 && pos2.id == id2 ) {
                    return GetMatchOrdered(pos1, pos2);
                }
                if ( pos2.id == id1 && pos1.id == id2 ) {
                    TMatch ret = GetMatchOrdered(pos2, pos1);
                    swap(ret.m1, ret.m2);
                    return ret;
                }
                return TMatch();
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

    typedef SSeqMapSwitchPoint::TDifferences TDifferences;
    pair<TSeqPos, TSeqPos> x_FindAlignMatch(SSeqPos pos1, // current segment
                                            SSeqPos pos2, // another segment
                                            TSeqPos limit, // on current
                                            TDifferences& diff) const;
};

pair<TSeqPos, TSeqPos>
SSeq_align_Info::x_FindAlignMatch(SSeqPos pos1,
                                  SSeqPos pos2,
                                  TSeqPos limit,
                                  TDifferences& diff) const
{
    pair<TSeqPos, TSeqPos> ret(0, 0);
    bool exact = true;
    TSeqPos skip1 = 0, skip2 = 0, offset = 0, skip_pos = kInvalidSeqPos;
    while ( limit ) {
        _TRACE("pos1="<<pos1<<" pos2="<<pos2);
        SMatch::TMatch add;
        TMatchMap::const_iterator miter =
            match_map.find(GetKey(pos1.id, pos2.id));
        if ( miter != match_map.end() ) {
            const TMatches& matches = miter->second;
            ITERATE ( TMatches, it, matches ) {
                SMatch::TMatch m = it->GetMatch(pos1, pos2);
                if ( m < add ) {
                    add = m;
                }
            }
        }
        _TRACE("pos1="<<pos1<<" pos2="<<pos2<<" add="<<add.m1<<','<<add.m2);
        if ( !add ) {
            break;
        }
        if ( add.skip ) {
            if ( skip1 == 0 ) {
                skip_pos = offset;
            }
            TSeqPos len = min(limit, add.m1);
            skip1 += add.m1;
            skip2 += add.m2;
            pos1 += add.m1;
            pos2 += add.m2;
            limit -= len;
            offset += len;
            exact = false;
        }
        else {
            if ( skip1 || skip2 ) {
                diff[skip_pos].second += skip1;
                diff[skip_pos].first += skip2;
                ret.first += skip1;
                skip1 = 0;
                skip2 = 0;
                skip_pos = kInvalidSeqPos;
            }
            _ASSERT(add.m1 == add.m2);
            TSeqPos len = min(limit, add.m1);
            ret.first += len;
            if ( exact ) {
                ret.second = ret.first;
            }
            pos1 += len;
            pos2 += len;
            limit -= len;
            offset += len;
        }
    }
    ITERATE ( TDifferences, i, diff ) {
        _TRACE("pos: "<<i->first<<" ins: "<<i->second.first<<" del: "<<i->second.second);
    }
    return ret;
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
    pair<TSeqPos, TSeqPos> ext2 =
        info.x_FindAlignMatch(pos2, pos1, iter2.GetLength(),
                              sp.m_RightDifferences);
    pos1.Reverse();
    pos2.Reverse();
    pair<TSeqPos, TSeqPos> ext1 =
        info.x_FindAlignMatch(pos1, pos2, iter1.GetLength(),
                              sp.m_LeftDifferences);
    sp.m_MasterRange.SetFrom(pos-ext1.first).SetTo(pos+ext2.first);
    sp.m_ExactMasterRange.SetFrom(pos-ext1.second).SetTo(pos+ext2.second);
    return sp;
}

SSeqMapSwitchPoint::TInsertDelete
x_GetDifferences(const SSeqMapSwitchPoint::TDifferences& diff,
                 TSeqPos offset, TSeqPos add)
{
    SSeqMapSwitchPoint::TInsertDelete ret;
    SSeqMapSwitchPoint::TDifferences::const_iterator iter = diff.begin();
    while ( iter != diff.end() && offset >= iter->first ) {
        if ( offset - iter->first <= iter->second.second ) {
            TSeqPos ins = min(add, iter->second.first);
            ret.first += ins;
            TSeqPos del = offset - iter->first;
            ret.second += del;
            break;
        }
        else {
            ret.first += iter->second.first;
            ret.second += iter->second.second;
        }
        ++iter;
    }
    return ret;
}

} // namespace

class CScope;

SSeqMapSwitchPoint::TInsertDelete
SSeqMapSwitchPoint::GetDifferences(TSeqPos new_pos, TSeqPos add) const
{
    if ( new_pos > m_MasterPos ) {
        return x_GetDifferences(m_RightDifferences, new_pos-m_MasterPos, add);
    }
    else if ( new_pos < m_MasterPos ) {
        return x_GetDifferences(m_LeftDifferences, m_MasterPos-new_pos, add);
    }
    else {
        return TInsertDelete();
    }
}

TSeqPos SSeqMapSwitchPoint::GetInsert(TSeqPos pos) const
{
    const TDifferences* diff;
    TSeqPos offset;
    if ( pos < m_MasterPos ) {
        diff = &m_LeftDifferences;
        offset = m_MasterPos - pos;
    }
    else {
        diff = &m_RightDifferences;
        offset = pos-m_MasterPos;
    }
    TDifferences::const_iterator iter = diff->find(offset);
    return iter == diff->end()? 0: iter->second.first;
}

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

    NCBI_THROW(CSeqMapException, eInvalidIndex,
               "Seq-align doesn't refer to segments");
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
* Revision 1.5  2006/10/04 20:03:35  ucko
* SMatch::GetMatchOrdered: comment out uncompilable _TRACE statements
* (CRange<> objects aren't directly printable at present).
*
* Revision 1.4  2006/10/04 19:31:08  vasilche
* Allow inexact match in segment switch.
*
* Revision 1.3  2006/09/27 22:41:27  vasilche
* Added exception in case of error.
*
* Revision 1.2  2006/09/27 22:37:40  vasilche
* GCC 2.95 does not understand BEGIN_SCOPE() without arguments.
*
* Revision 1.1  2006/09/27 21:28:58  vasilche
* Added functions to calculate switch points.
*
* ===========================================================================
*/
