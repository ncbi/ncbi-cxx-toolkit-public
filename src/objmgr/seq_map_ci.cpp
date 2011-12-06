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
*   Sequence map for the Object Manager. Describes sequence as a set of
*   segments of different types (data, reference, gap or end).
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/seq_map_ci.hpp>
#include <objmgr/seq_map.hpp>
#include <objmgr/tse_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/scope.hpp>
#include <objects/seq/seq_id_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
// SSeqMapSelector
/////////////////////////////////////////////////////////////////////////////


SSeqMapSelector::SSeqMapSelector(void)
    : m_Position(0),
      m_Length(kInvalidSeqPos),
      m_MinusStrand(false),
      m_LinkUsedTSE(true),
      m_MaxResolveCount(0),
      m_Flags(CSeqMap::fDefaultFlags),
      m_UsedTSEs(0)
{
}


SSeqMapSelector::SSeqMapSelector(TFlags flags, size_t resolve_count)
    : m_Position(0),
      m_Length(kInvalidSeqPos),
      m_MinusStrand(false),
      m_LinkUsedTSE(true),
      m_MaxResolveCount(resolve_count),
      m_Flags(flags),
      m_UsedTSEs(0)
{
}


SSeqMapSelector& SSeqMapSelector::SetLimitTSE(const CSeq_entry_Handle& tse)
{
    m_LimitTSE = tse.GetTSE_Handle();
    return *this;
}


const CTSE_Handle& SSeqMapSelector::x_GetLimitTSE(CScope* /* scope */) const
{
    _ASSERT(m_LimitTSE);
    return m_LimitTSE;
}


void SSeqMapSelector::AddUsedTSE(const CTSE_Handle& tse) const
{
    if ( m_UsedTSEs ) {
        m_UsedTSEs->push_back(tse);
    }
}


////////////////////////////////////////////////////////////////////
// CSeqMap_CI_SegmentInfo


bool CSeqMap_CI_SegmentInfo::x_Move(bool minusStrand, CScope* scope)
{
    const CSeqMap& seqMap = *m_SeqMap;
    size_t index = m_Index;
    const CSeqMap::CSegment& old_seg = seqMap.x_GetSegment(index);
    if ( !minusStrand ) {
        if ( old_seg.m_Position > m_LevelRangeEnd ||
             index >= seqMap.x_GetLastEndSegmentIndex() )
            return false;
        m_Index = ++index;
        seqMap.x_GetSegmentLength(index, scope); // Update length of segment
        return seqMap.x_GetSegmentPosition(index, scope) < m_LevelRangeEnd;
    }
    else {
        if ( old_seg.m_Position + old_seg.m_Length < m_LevelRangePos ||
             index <= seqMap.x_GetFirstEndSegmentIndex() )
            return false;
        m_Index = --index;
        return old_seg.m_Position > m_LevelRangePos;
    }
}



////////////////////////////////////////////////////////////////////
// CSeqMap_CI

inline
bool CSeqMap_CI::x_Push(TSeqPos pos)
{
    return x_Push(pos, m_Selector.CanResolve());
}


CSeqMap_CI::CSeqMap_CI(void)
    : m_SearchPos(0),
      m_SearchEnd(kInvalidSeqPos)
{
    m_Selector.SetPosition(kInvalidSeqPos);
}


CSeqMap_CI::CSeqMap_CI(const CConstRef<CSeqMap>& seqMap,
                       CScope* scope,
                       const SSeqMapSelector& sel,
                       TSeqPos pos)
    : m_Scope(scope),
      m_SearchPos(0),
      m_SearchEnd(kInvalidSeqPos)
{
    x_Select(seqMap, sel, pos);
}


CSeqMap_CI::CSeqMap_CI(const CConstRef<CSeqMap>& seqMap,
                       CScope* scope,
                       const SSeqMapSelector& sel,
                       const CRange<TSeqPos>& range)
    : m_Scope(scope),
      m_SearchPos(range.GetFrom()),
      m_SearchEnd(range.GetToOpen())
{
    x_Select(seqMap, sel, range.GetFrom());
}


CSeqMap_CI::CSeqMap_CI(const CBioseq_Handle& bioseq,
                       const SSeqMapSelector& sel,
                       TSeqPos pos)
    : m_Scope(&bioseq.GetScope()),
      m_SearchPos(0),
      m_SearchEnd(kInvalidSeqPos)
{
    SSeqMapSelector tse_sel(sel);
    tse_sel.SetLinkUsedTSE(bioseq.GetTSE_Handle());
    x_Select(ConstRef(&bioseq.GetSeqMap()), tse_sel, pos);
}


CSeqMap_CI::CSeqMap_CI(const CBioseq_Handle& bioseq,
                       const SSeqMapSelector& sel,
                       const CRange<TSeqPos>& range)
    : m_Scope(&bioseq.GetScope()),
      m_SearchPos(range.GetFrom()),
      m_SearchEnd(range.GetToOpen())
{
    SSeqMapSelector tse_sel(sel);
    tse_sel.SetLinkUsedTSE(bioseq.GetTSE_Handle());
    x_Select(ConstRef(&bioseq.GetSeqMap()), tse_sel, range.GetFrom());
}


CSeqMap_CI::CSeqMap_CI(const CSeqMap_CI& base,
                       const CSeqMap& seqmap,
                       size_t index,
                       TSeqPos pos)
    : m_Scope(base.m_Scope),
      m_Stack(1, base.m_Stack.back()),
      m_SearchPos(0),
      m_SearchEnd(kInvalidSeqPos)
{
    TSegmentInfo& info = x_GetSegmentInfo();
    if ( &info.x_GetSeqMap() != &seqmap ||
         info.x_GetIndex() != index ) {
        NCBI_THROW(CSeqMapException, eInvalidIndex,
                   "Invalid argument");
    }
    info.m_LevelRangePos = 0;
    info.m_LevelRangeEnd = kInvalidSeqPos;
    info.m_MinusStrand = 0;
    const CSeqMap::CSegment& seg = info.x_GetSegment();
    if ( seg.m_Position != pos ) {
        NCBI_THROW(CSeqMapException, eInvalidIndex,
                   "Invalid argument");
    }
    m_Selector.m_Position = pos;
    m_Selector.m_Length = info.x_CalcLength();
}


CSeqMap_CI::~CSeqMap_CI(void)
{
}


void CSeqMap_CI::x_Select(const CConstRef<CSeqMap>& seqMap,
                          const SSeqMapSelector& selector,
                          TSeqPos pos)
{
    m_Selector = selector;
    if ( m_Selector.m_Length == kInvalidSeqPos ) {
        TSeqPos len = seqMap->GetLength(GetScope());
        len -= min(len, m_Selector.m_Position);
        m_Selector.m_Length = len;
    }
    if ( pos < m_Selector.m_Position ) {
        pos = m_Selector.m_Position;
    }
    else if ( pos > m_Selector.m_Position + m_Selector.m_Length ) {
        pos = m_Selector.m_Position + m_Selector.m_Length;
    }
    x_Push(seqMap, m_Selector.m_TopTSE,
           m_Selector.m_Position,
           m_Selector.m_Length,
           m_Selector.m_MinusStrand,
           pos - m_Selector.m_Position);
    while ( !x_Found() && GetPosition() < m_SearchEnd ) {
        if ( !x_Push(pos - m_Selector.m_Position) ) {
            x_SettleNext();
            break;
        }
    }
}


const CSeq_data& CSeqMap_CI::GetData(void) const
{
    if ( !*this ) {
        NCBI_THROW(CSeqMapException, eOutOfRange,
                   "Iterator out of range");
    }
    if ( GetRefPosition() != 0 || GetRefMinusStrand() ) {
        NCBI_THROW(CSeqMapException, eDataError,
                   "Non standard Seq_data: use methods "
                   "GetRefData/GetRefPosition/GetRefMinusStrand");
    }
    return GetRefData();
}


const CSeq_data& CSeqMap_CI::GetRefData(void) const
{
    if ( !*this ) {
        NCBI_THROW(CSeqMapException, eOutOfRange,
                   "Iterator out of range");
    }
    return x_GetSeqMap().x_GetSeq_data(x_GetSegment());
}


bool CSeqMap_CI::IsUnknownLength(void) const
{
    if ( !*this ) {
        NCBI_THROW(CSeqMapException, eOutOfRange,
                   "Iterator out of range");
    }
    return x_GetSegment().m_UnknownLength;
}


CSeq_id_Handle CSeqMap_CI::GetRefSeqid(void) const
{
    if ( !*this ) {
        NCBI_THROW(CSeqMapException, eOutOfRange,
                   "Iterator out of range");
    }
    return CSeq_id_Handle::
        GetHandle(x_GetSeqMap().x_GetRefSeqid(x_GetSegment()));
}


TSeqPos CSeqMap_CI_SegmentInfo::GetRefPosition(void) const
{
    if ( !InRange() ) {
        NCBI_THROW(CSeqMapException, eOutOfRange,
                   "Iterator out of range");
    }
    const CSeqMap::CSegment& seg = x_GetSegment();
    TSeqPos skip;
    if ( !seg.m_RefMinusStrand ) {
        skip = m_LevelRangePos >= seg.m_Position ?
            m_LevelRangePos - seg.m_Position : 0;
    }
    else {
        TSeqPos seg_end = seg.m_Position + seg.m_Length;
        skip = seg_end > m_LevelRangeEnd ?
            seg_end - m_LevelRangeEnd : 0;
    }
    return seg.m_RefPosition + skip;
}


TSeqPos CSeqMap_CI_SegmentInfo::x_GetTopOffset(void) const
{
    if ( !m_MinusStrand ) {
        TSeqPos min_pos = min(x_GetLevelRealPos(), m_LevelRangeEnd);
        return min_pos > m_LevelRangePos ? min_pos - m_LevelRangePos : 0;
    }
    TSeqPos max_pos = max(x_GetLevelRealEnd(), m_LevelRangePos);
    return m_LevelRangeEnd > max_pos ? m_LevelRangeEnd - max_pos : 0;
}


TSeqPos CSeqMap_CI::x_GetTopOffset(void) const
{
    return x_GetSegmentInfo().x_GetTopOffset();
}


bool CSeqMap_CI::x_RefTSEMatch(const CSeqMap::CSegment& seg) const
{
    _ASSERT(m_Selector.x_HasLimitTSE());
    _ASSERT(CSeqMap::ESegmentType(seg.m_SegType) == CSeqMap::eSeqRef);
    CSeq_id_Handle id = CSeq_id_Handle::
        GetHandle(x_GetSeqMap().x_GetRefSeqid(seg));
    return m_Selector.x_GetLimitTSE(GetScope()).GetBioseqHandle(id);
}


inline
bool CSeqMap_CI::x_CanResolve(const CSeqMap::CSegment& seg) const
{
    return m_Selector.CanResolve() &&
        (!m_Selector.x_HasLimitTSE() || x_RefTSEMatch(seg));
}


bool CSeqMap_CI::x_Push(TSeqPos pos, bool resolveExternal)
{
    const TSegmentInfo& info = x_GetSegmentInfo();
    if ( !info.InRange() ) {
        return false;
    }
    const CSeqMap::CSegment& seg = info.x_GetSegment();
    CSeqMap::ESegmentType type = CSeqMap::ESegmentType(seg.m_SegType);

    switch ( type ) {
    case CSeqMap::eSeqSubMap:
    {{
        CConstRef<CSeqMap> push_map
            (static_cast<const CSeqMap*>(info.m_SeqMap->x_GetObject(seg)));
        // We have to copy the info.m_TSE into local variable push_tse because
        // of TSegmentInfo referenced by info can be moved inside x_Push() call.
        CTSE_Handle push_tse = info.m_TSE;
        x_Push(push_map, info.m_TSE,
               GetRefPosition(), GetLength(), GetRefMinusStrand(), pos);
        break;
    }}
    case CSeqMap::eSeqRef:
    {{
        if ( !resolveExternal ) {
            return false;
        }
        const CSeq_id& seq_id =
            static_cast<const CSeq_id&>(*info.m_SeqMap->x_GetObject(seg));
        CBioseq_Handle bh;
        if ( m_Selector.x_HasLimitTSE() ) {
            // Check TSE limit
            bh = m_Selector.x_GetLimitTSE().GetBioseqHandle(seq_id);
            if ( !bh ) {
                return false;
            }
        }
        else {
            if ( !GetScope() ) {
                NCBI_THROW(CSeqMapException, eNullPointer,
                           "Cannot resolve "+
                           seq_id.AsFastaString()+": null scope pointer");
            }
            bh = GetScope()->GetBioseqHandle(seq_id);
            if ( !bh ) {
                if ( GetFlags() & CSeqMap::fIgnoreUnresolved ) {
                    return false;
                }
                NCBI_THROW(CSeqMapException, eFail,
                           "Cannot resolve "+
                           seq_id.AsFastaString()+": unknown");
            }
        }
        if ( (GetFlags() & CSeqMap::fByFeaturePolicy) &&
            bh.GetFeatureFetchPolicy() == bh.eFeatureFetchPolicy_only_near ) {
            return false;
        }
        if ( info.m_TSE ) {
            if ( !info.m_TSE.AddUsedTSE(bh.GetTSE_Handle()) ) {
                m_Selector.AddUsedTSE(bh.GetTSE_Handle());
            }
        }
        size_t depth = m_Stack.size();
        x_Push(ConstRef(&bh.GetSeqMap()), bh.GetTSE_Handle(),
               GetRefPosition(), GetLength(), GetRefMinusStrand(), pos);
        if (m_Stack.size() == depth) {
            return false;
        }
        m_Selector.PushResolve();
        if ( (m_Stack.size() & 63) == 0 ) {
            // check for self-recursion every 64'th stack frame
            const CSeqMap* top_seq_map = &m_Stack.back().x_GetSeqMap();
            for ( int i = m_Stack.size()-2; i >= 0; --i ) {
                if ( &m_Stack[i].x_GetSeqMap() == top_seq_map ) {
                    NCBI_THROW(CSeqMapException, eSelfReference,
                               "Self-reference in CSeqMap");
                }
            }
        }
        break;
    }}
    default:
        return false;
    }
    return true;
}


void CSeqMap_CI::x_Push(const CConstRef<CSeqMap>& seqMap,
                        const CTSE_Handle& tse,
                        TSeqPos from, TSeqPos length,
                        bool minusStrand,
                        TSeqPos pos)
{
    TSegmentInfo push;
    push.m_SeqMap = seqMap;
    push.m_TSE = tse;
    push.m_LevelRangePos = from;
    push.m_LevelRangeEnd = from + length;
    if (push.m_LevelRangeEnd < push.m_LevelRangePos) {
        // Detect (from + length) overflow
        NCBI_THROW(CSeqMapException, eDataError,
                   "Sequence position overflow");
    }
    push.m_MinusStrand = minusStrand;
    TSeqPos findOffset = !minusStrand? pos: length - 1 - pos;
    push.m_Index = seqMap->x_FindSegment(from + findOffset, GetScope());
    if ( push.m_Index == size_t(-1) ) {
        if ( !m_Stack.empty() ) {
            return;
        }
        push.m_Index = !minusStrand?
            seqMap->x_GetLastEndSegmentIndex():
            seqMap->x_GetFirstEndSegmentIndex();
    }
    else {
        _ASSERT(push.m_Index > seqMap->x_GetFirstEndSegmentIndex() &&
                push.m_Index < seqMap->x_GetLastEndSegmentIndex());
        if ( pos >= length ) {
            if ( !minusStrand ) {
                if ( seqMap->x_GetSegmentPosition(push.m_Index, 0) <
                     push.m_LevelRangeEnd ) {
                    ++push.m_Index;
                    _ASSERT(seqMap->x_GetSegmentPosition(push.m_Index, 0) >=
                            push.m_LevelRangeEnd);
                }
            }
            else {
                if ( seqMap->x_GetSegmentEndPosition(push.m_Index, 0) >
                     push.m_LevelRangePos ) {
                    --push.m_Index;
                    _ASSERT(seqMap->x_GetSegmentEndPosition(push.m_Index, 0) <=
                            push.m_LevelRangePos);
                }
            }
        }
    }
    // update length of current segment
    seqMap->x_GetSegmentLength(push.m_Index, GetScope());
    m_Stack.push_back(push);
    // update position
    m_Selector.m_Position += x_GetTopOffset();
    // update length
    m_Selector.m_Length = push.x_CalcLength();
}


bool CSeqMap_CI::x_Pop(void)
{
    if ( m_Stack.size() <= 1 ) {
        return false;
    }

    m_Selector.m_Position -= x_GetTopOffset();
    m_Stack.pop_back();
    if ( x_GetSegment().m_SegType == CSeqMap::eSeqRef ) {
        m_Selector.PopResolve();
    }
    m_Selector.m_Length = x_GetSegmentInfo().x_CalcLength();
    return true;
}


bool CSeqMap_CI::x_TopNext(void)
{
    TSegmentInfo& top = x_GetSegmentInfo();
    m_Selector.m_Position += m_Selector.m_Length;
    if ( !top.x_Move(top.m_MinusStrand, GetScope()) ) {
        m_Selector.m_Length = 0;
        return false;
    }
    else {
        m_Selector.m_Length = x_GetSegmentInfo().x_CalcLength();
        return true;
    }
}


bool CSeqMap_CI::x_TopPrev(void)
{
    TSegmentInfo& top = x_GetSegmentInfo();
    if ( !top.x_Move(!top.m_MinusStrand, GetScope()) ) {
        m_Selector.m_Length = 0;
        return false;
    }
    else {
        m_Selector.m_Length = x_GetSegmentInfo().x_CalcLength();
        m_Selector.m_Position -= m_Selector.m_Length;
        return true;
    }
}


inline
bool CSeqMap_CI::x_Next(void)
{
    return x_Next(m_Selector.CanResolve());
}


bool CSeqMap_CI::x_Next(bool resolveExternal)
{
    TSeqPos search_pos = m_SearchPos;
    TSeqPos level_pos = GetPosition();
    TSeqPos offset = search_pos > level_pos? search_pos - level_pos: 0;
    if ( x_Push(offset, resolveExternal) ) {
        return true;
    }
    do {
        if ( x_TopNext() )
            return true;
    } while ( x_Pop() );
    return false;
}


bool CSeqMap_CI::x_Prev(void)
{
    if ( !x_TopPrev() )
        return x_Pop();
    for ( ;; ) {
        TSeqPos search_end = m_SearchEnd;
        TSeqPos level_end = GetEndPosition();
        TSeqPos end_offset = search_end < level_end? level_end - search_end: 0;
        if ( !x_Push(m_Selector.m_Length-end_offset-1) ) {
            break;
        }
    }
    return true;
}


bool CSeqMap_CI::x_Found(void) const
{
    if ( (GetFlags() & CSeqMap::fFindExactLevel) &&
         m_Selector.GetResolveCount() != 0 ) {
        return false;
    }
    switch ( x_GetSegment().m_SegType ) {
    case CSeqMap::eSeqRef:
        if ( (GetFlags() & CSeqMap::fFindLeafRef) != 0 ) {
            if ( (GetFlags() & CSeqMap::fFindInnerRef) != 0 ) {
                // both
                return true;
            }
            else {
                // leaf only
                return !x_CanResolve(x_GetSegment());
            }
        }
        else {
            if ( (GetFlags() & CSeqMap::fFindInnerRef) != 0 ) {
                // inner only
                return x_CanResolve(x_GetSegment());
            }
            else {
                // none
                return false;
            }
        }
    case CSeqMap::eSeqData:
        return (GetFlags() & CSeqMap::fFindData) != 0;
    case CSeqMap::eSeqGap:
        return (GetFlags() & CSeqMap::fFindGap) != 0;
    case CSeqMap::eSeqSubMap:
        return false; // always skip submaps
    default:
        return false;
    }
}


bool CSeqMap_CI::x_SettleNext(void)
{
    while ( !x_Found() && GetPosition() < m_SearchEnd ) {
        if ( !x_Next() )
            return false;
    }
    return true;
}


bool CSeqMap_CI::x_SettlePrev(void)
{
    while ( !x_Found() ) {
        if ( !x_Prev() )
            return false;
    }
    return true;
}


bool CSeqMap_CI::Next(bool resolveCurrentExternal)
{
    return x_Next(resolveCurrentExternal && m_Selector.CanResolve()) &&
        x_SettleNext();
}


bool CSeqMap_CI::Prev(void)
{
    return x_Prev() && x_SettlePrev();
}


void CSeqMap_CI::SetFlags(TFlags flags)
{
    m_Selector.SetFlags(flags);
}


bool CSeqMap_CI::IsValid(void) const
{
    return GetPosition() < m_SearchEnd &&
        !m_Stack.empty() &&
        m_Stack.front().InRange() &&
        m_Stack.front().GetType() != CSeqMap::eSeqEnd;
}


END_SCOPE(objects)
END_NCBI_SCOPE
