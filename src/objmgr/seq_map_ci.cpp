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

#include <objects/objmgr/seq_map_ci.hpp>
#include <objects/objmgr/seq_map.hpp>
#include "data_source.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

////////////////////////////////////////////////////////////////////
// CSeqMap_CI_SegmentInfo


bool CSeqMap_CI_SegmentInfo::x_Move(bool minusStrand, CScope* scope)
{
    const CSeqMap& seqMap = *m_SeqMap;
    const CSeqMap::CSegment& seg = seqMap.x_GetSegment(m_Index);
    if ( !minusStrand ) {
        TSeqPos end = seg.m_Position + seg.m_Length;
        if ( end >= m_LevelRangeEnd ) {
            if ( end > m_LevelRangeEnd || // out of range
                 m_LevelRangeEnd != seqMap.GetLength() || // not whole
                 m_Index + 1 >= seqMap.x_GetSegmentsCount() ) { // last
                return false;
            }
        }
        ++m_Index;
        seqMap.x_GetSegmentLength(m_Index, scope); // Update length of segment
    }
    else {
        TSeqPos pos = seg.m_Position;
        if ( pos <= m_LevelRangePos ) {
            if ( pos < m_LevelRangePos || // out of range
                 m_LevelRangePos != 0 || // not whole
                 m_Index == 0 ) { // last
                return false;
            }
        }
        --m_Index;
    }
    return true;
}




////////////////////////////////////////////////////////////////////
// CSeqMap_CI


CSeqMap_CI::CSeqMap_CI(void)
    : m_Position(kInvalidSeqPos),
      m_Scope(0),
      m_MaxResolveCount(0)
{
}

CSeqMap_CI::CSeqMap_CI(CConstRef<CSeqMap> seqMap, CScope* scope,
                       EPosition /*byPos*/, TSeqPos pos,
                       size_t maxResolveCount)
    : m_Position(0),
      m_Scope(scope),
      m_MaxResolveCount(maxResolveCount)
{
    x_Push(seqMap, 0, seqMap->GetLength(scope), false, pos);
    x_Resolve(pos);
}


CSeqMap_CI::CSeqMap_CI(CConstRef<CSeqMap> seqMap, CScope* scope,
                       EBegin /*toBegin*/,
                       size_t maxResolveCount)
    : m_Position(0),
      m_Scope(scope),
      m_MaxResolveCount(maxResolveCount)
{
    x_Push(seqMap, 0, seqMap->GetLength(scope), false, 0);
    x_Resolve(0);
}


CSeqMap_CI::CSeqMap_CI(CConstRef<CSeqMap> seqMap, CScope* scope,
                       EEnd /*toEnd*/,
                       size_t maxResolveCount)
    : m_Position(seqMap->GetLength(scope)),
      m_Length(0),
      m_Scope(scope),
      m_MaxResolveCount(maxResolveCount)
{
    TSegmentInfo push;
    push.m_SeqMap = seqMap;
    push.m_LevelRangePos = 0;
    push.m_LevelRangeEnd = m_Position;
    push.m_MinusStrand = false;
    push.m_Index = seqMap->x_GetSegmentsCount();
    m_Stack.push_back(push);
}


CSeqMap_CI::CSeqMap_CI(CConstRef<CSeqMap> seqMap, CScope* scope,
                       TSeqPos from, TSeqPos length, ENa_strand strand,
                       EBegin /*toBegin*/,
                       size_t maxResolveCount)
    : m_Position(0),
      m_Scope(scope),
      m_MaxResolveCount(maxResolveCount)
{
    x_Push(seqMap, from, length, strand == eNa_strand_minus, 0);
    x_Resolve(0);
}


CSeqMap_CI::~CSeqMap_CI(void)
{
}


CSeqMap::ESegmentType CSeqMap_CI::GetType(void) const
{
    return CSeqMap::ESegmentType(x_GetSegment().m_SegType);
}


const CSeq_data& CSeqMap_CI::GetData(void) const
{
    if ( GetRefPosition() != 0 || GetRefMinusStrand() ) {
        THROW1_TRACE(runtime_error,
                     "CSeqMap_CI::GetData: non standard Seq_data: "
                     "use methods GetRefData/GetRefPosition/GetRefMinusStrand");
    }
    return GetRefData();
}


const CSeq_data& CSeqMap_CI::GetRefData(void) const
{
    return x_GetSeqMap().x_GetSeq_data(x_GetSegment());
}


CSeq_id_Handle CSeqMap_CI::GetRefSeqid(void) const
{
    _ASSERT(x_GetSeqMap().m_Source);
    return x_GetSeqMap().m_Source->GetIdMapper().
        GetHandle(x_GetSeqMap().x_GetRefSeqid(x_GetSegment()));
}


TSeqPos CSeqMap_CI_SegmentInfo::GetRefPosition(void) const
{
    const CSeqMap::CSegment& seg = x_GetSegment();
    TSignedSeqPos skip;
    if ( !seg.m_RefMinusStrand ) {
        skip = m_LevelRangePos - seg.m_Position;
    }
    else {
        skip = (seg.m_Position + seg.m_Length) - m_LevelRangeEnd;
    }
    if ( skip < 0 )
        skip = 0;
    return seg.m_RefPosition + skip;
}


TSeqPos CSeqMap_CI_SegmentInfo::x_GetTopOffset(void) const
{
    TSignedSeqPos offset;
    if ( !m_MinusStrand ) {
        offset = x_GetLevelRealPos() - m_LevelRangePos;
    }
    else {
        offset = m_LevelRangeEnd - x_GetLevelRealEnd();
    }
    if ( offset < 0 )
        offset = 0;
    return offset;
}


TSeqPos CSeqMap_CI::x_GetTopOffset(void) const
{
    return x_GetSegmentInfo().x_GetTopOffset();
}


bool CSeqMap_CI::x_Push(TSeqPos pos)
{
    const TSegmentInfo& info = x_GetSegmentInfo();
    const CSeqMap::CSegment& seg = info.x_GetSegment();
    CConstRef<CSeqMap> seqMap;
    if ( seg.m_RefObjectType == CSeqMap::eSeqMap ) {
        // internal SeqMap -> no resolution
        seqMap.Reset(reinterpret_cast<const CSeqMap*>
                     (seg.m_RefObject.GetPointer()));
    }
    else if ( seg.m_RefObjectType == CSeqMap::eSeq_id &&
              m_MaxResolveCount > 0 ) {
        // external reference -> resolution
        seqMap.Reset(&info.m_SeqMap->x_GetBioseqHandle(seg, GetScope())
                     .GetSeqMap());
        --m_MaxResolveCount;
    }
    else {
        return false;
    }

    x_Push(seqMap, GetRefPosition(), GetLength(), GetRefMinusStrand(), pos);
    return true;
}


void CSeqMap_CI::x_Push(CConstRef<CSeqMap> seqMap,
                        TSeqPos from, TSeqPos length,
                        bool minusStrand,
                        TSeqPos pos)
{
    TSegmentInfo push;
    push.m_SeqMap = seqMap;
    push.m_LevelRangePos = from;
    push.m_LevelRangeEnd = from + length;
    push.m_MinusStrand = minusStrand;
    TSeqPos findOffset = !minusStrand? pos: length - 1 - pos;
    push.m_Index = seqMap->x_FindSegment(from + findOffset, GetScope());
    if ( push.m_Index == size_t(-1) ) {
	_ASSERT(length == 0);
	push.m_Index = !minusStrand? seqMap->x_GetSegmentsCount(): 0;
    }
    else {
	_ASSERT(push.m_Index < seqMap->x_GetSegmentsCount());
    }
    // update length of current segment
    seqMap->x_GetSegmentLength(push.m_Index, GetScope());
    m_Stack.push_back(push);
    // update position
    m_Position += x_GetTopOffset();
    // update length
    m_Length = push.x_CalcLength();
}


bool CSeqMap_CI::x_Pop(void)
{
    if ( m_Stack.size() <= 1 ) {
        return false;
    }

    m_Position -= x_GetTopOffset();
    m_Stack.pop_back();
    if ( x_GetSegment().m_RefObjectType == CSeqMap::eSeq_id ) {
        ++m_MaxResolveCount;
    }
    m_Length = x_GetSegmentInfo().x_CalcLength();
    return true;
}


bool CSeqMap_CI::x_TopNext(void)
{
    TSegmentInfo& top = x_GetSegmentInfo();
    if ( !top.x_Move(top.m_MinusStrand, GetScope()) )
        return false;
    m_Position += m_Length;
    m_Length = x_GetSegmentInfo().x_CalcLength();
    return true;
}


bool CSeqMap_CI::x_TopPrev(void)
{
    TSegmentInfo& top = x_GetSegmentInfo();
    if ( !top.x_Move(!top.m_MinusStrand, GetScope()) )
        return false;
    m_Length = x_GetSegmentInfo().x_CalcLength();
    m_Position -= m_Length;
    return true;
}


bool CSeqMap_CI::x_StopOnIntermediateSegments(void) const
{
    return false;
}


void CSeqMap_CI::x_Resolve(TSeqPos pos)
{
    while ( x_Push(pos - m_Position) && !x_StopOnIntermediateSegments() ) {
    }
}


bool CSeqMap_CI::Next(void)
{
    if ( x_StopOnIntermediateSegments() && x_Push(0) ) {
        return true;
    }
    for ( ;; ) {
        if ( x_TopNext() ) {
            if ( !x_StopOnIntermediateSegments() ) {
                x_Resolve(m_Position);
            }
            return true;
        }
        else if ( !x_Pop() ) {
            if ( x_GetSegmentInfo().m_Index <
                 x_GetSegmentInfo().x_GetSeqMap().x_GetSegmentsCount() ) {
                ++x_GetSegmentInfo().m_Index;
                m_Position += m_Length;
                m_Length = 0;
            }
            return false;
        }
    }
}


bool CSeqMap_CI::Prev(void)
{
    if ( x_StopOnIntermediateSegments() && x_Push(m_Length-1) ) {
        return true;
    }
    for ( ;; ) {
        if ( x_TopPrev() ) {
            if ( !x_StopOnIntermediateSegments() ) {
                x_Resolve(m_Position+m_Length-1);
            }
            return true;
        }
        else if ( !x_Pop() ) {
            return false;
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2003/01/24 20:14:08  vasilche
* Fixed processing zero length references.
*
* Revision 1.2  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.1  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
*
* ===========================================================================
*/
