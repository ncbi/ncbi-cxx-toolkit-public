#if 0
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

#include <objects/objmgr/seq_map_rci.hpp>
#include <objects/objmgr/bioseq_handle.hpp>
#include <objects/objmgr/scope.hpp>
#include <objects/objmgr/seq_id_mapper.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_interval.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//  CSeqMapRange_CI

CSeqMapRange_CI::CSeqMapRange_CI(void)
    : m_RangePosition(0),
      m_RangeEndPosition(0),
      m_MinusStrand(false)
{
}


CSeqMapRange_CI::CSeqMapRange_CI(CSeqMap_CI::EPosition /*byPos*/, TSeqPos pos,
                                 const CSeqMap_CI& seg)
    : m_Seqid(seg.GetRefSeqid()),
      m_RangePosition(seg.GetRefPosition()),
      m_RangeEndPosition(seg.GetRefEndPosition()),
      m_MinusStrand(seg.GetRefMinusStrand())
{
    x_Init(seg.x_GetSubSeqMap(true), CSeqMap_CI::ePosition, pos, seg.GetScope());
}


CSeqMapRange_CI::CSeqMapRange_CI(CSeqMap_CI::EPosition /*byPos*/, TSeqPos pos,
                                 const CSeqMapRange_CI& seg)
    : m_Seqid(seg.GetRefSeqid()),
      m_RangePosition(seg.GetRefPosition()),
      m_RangeEndPosition(seg.GetRefEndPosition()),
      m_MinusStrand(seg.GetRefMinusStrand())
{
    x_Init(seg.m_Segment.x_GetSubSeqMap(true),
           CSeqMap_CI::ePosition, pos, seg.GetScope());
}

#if 0
CSeqMapRange_CI::CSeqMapRange_CI(CSeqMap_CI::EPosition /*byPos*/, TSeqPos pos,
                                 const CSeq_id_Handle& id,
                                 TSeqPos rangePosition,
                                 TSeqPos rangeEndPosition,
                                 bool minusStrand,
                                 CScope* scope)
    : m_Seqid(id),
      m_RangePosition(rangePosition),
      m_RangeEndPosition(rangeEndPosition),
      m_MinusStrand(minusStrand)
{
    x_Init(seg.x_GetSubSeqMap(true), CSeqMap_CI::ePosition, pos, scope);
}
#endif

CSeqMapRange_CI::CSeqMapRange_CI(CSeqMap_CI::EBegin /*toBegin*/,
                                 const CSeqMap_CI& seg)
    : m_Seqid(seg.GetRefSeqid()),
      m_RangePosition(seg.GetRefPosition()),
      m_RangeEndPosition(seg.GetRefEndPosition()),
      m_MinusStrand(seg.GetRefMinusStrand())
{
    x_Init(seg.x_GetSubSeqMap(true), CSeqMap_CI::eBegin, seg.GetScope());
}


CSeqMapRange_CI::CSeqMapRange_CI(CSeqMap_CI::EBegin /*toBegin*/,
                                 const CSeqMapRange_CI& seg)
    : m_Seqid(seg.GetRefSeqid()),
      m_RangePosition(seg.GetRefPosition()),
      m_RangeEndPosition(seg.GetRefEndPosition()),
      m_MinusStrand(seg.GetRefMinusStrand())
{
    x_Init(seg.m_Segment.x_GetSubSeqMap(true), CSeqMap_CI::eBegin, seg.GetScope());
}

#if 0
CSeqMapRange_CI::CSeqMapRange_CI(CSeqMap_CI::EBegin /*toBegin*/,
                                 const CSeq_id_Handle& id,
                                 TSeqPos rangePosition,
                                 TSeqPos rangeEndPosition,
                                 bool minusStrand,
                                 CScope* scope)
    : m_Seqid(id),
      m_RangePosition(rangePosition),
      m_RangeEndPosition(rangeEndPosition),
      m_MinusStrand(minusStrand)
{
    x_Init(seg.x_GetSubSeqMap(scope, true), CSeqMap_CI::eBegin, scope);
}
#endif

CSeqMapRange_CI::CSeqMapRange_CI(CSeqMap_CI::EEnd /*toEnd*/,
                                 const CSeqMap_CI& seg)
    : m_Seqid(seg.GetRefSeqid()),
      m_RangePosition(seg.GetRefPosition()),
      m_RangeEndPosition(seg.GetRefEndPosition()),
      m_MinusStrand(seg.GetRefMinusStrand())
{
    x_Init(seg.x_GetSubSeqMap(true), CSeqMap_CI::eEnd, seg.GetScope());
}


CSeqMapRange_CI::CSeqMapRange_CI(CSeqMap_CI::EEnd /*toEnd*/,
                                 const CSeqMapRange_CI& seg)
    : m_Seqid(seg.GetRefSeqid()),
      m_RangePosition(seg.GetRefPosition()),
      m_RangeEndPosition(seg.GetRefEndPosition()),
      m_MinusStrand(seg.GetRefMinusStrand())
{
    x_Init(seg.m_Segment.x_GetSubSeqMap(true), CSeqMap_CI::eEnd, seg.GetScope());
}

#if 0
CSeqMapRange_CI::CSeqMapRange_CI(CSeqMap_CI::EEnd /*toEnd*/,
                                 const CSeq_id_Handle& id,
                                 TSeqPos rangePosition,
                                 TSeqPos rangeEndPosition,
                                 bool minusStrand,
                                 CScope* scope)
    : m_Seqid(id),
      m_RangePosition(rangePosition),
      m_RangeEndPosition(rangeEndPosition),
      m_MinusStrand(minusStrand)
{
    x_Init(seg.x_GetSubSeqMap(scope, true), CSeqMap_CI::eEnd, scope);
}
#endif

CSeqMapRange_CI::CSeqMapRange_CI(EWhole /*whole*/, const CSeqMap_CI& current)
    : m_Segment(current),
      m_RangePosition(0),
      m_RangeEndPosition(kInvalidSeqPos),
      m_MinusStrand(false)
{
}


#if 0
CSeqMapRange_CI::CSeqMapRange_CI(const CSeq_id& whole,
                                 CScope* scope)
    : m_Seqid(scope->GetIdHandle(whole)),
      m_RangePosition(0),
      m_RangeEndPosition(kInvalidSeqPos),
      m_MinusStrand(false)
{
    x_Init(scope);
}


CSeqMapRange_CI::CSeqMapRange_CI(const CSeq_point& point,
                                 CScope* scope)
    : m_Seqid(scope->GetIdHandle(point.GetId())),
      m_RangePosition(point.GetPoint()),
      m_RangeEndPosition(point.GetPoint()+1),
      m_MinusStrand(false)
{
    x_Init(scope);
}


CSeqMapRange_CI::CSeqMapRange_CI(const CSeq_interval& interval,
                                 CScope* scope)
    : m_Seqid(scope->GetIdHandle(interval.GetId())),
      m_RangePosition(min(interval.GetFrom(), interval.GetTo())),
      m_RangeEndPosition(m_RangePosition + interval.GetLength()),
      m_MinusStrand(interval.GetStrand() == eNa_strand_minus)
{
    x_Init(scope);
}
#endif

CSeqMapRange_CI::~CSeqMapRange_CI(void)
{
}


void CSeqMapRange_CI::x_Init(CConstRef<CSeqMap> seqMap,
                             CSeqMap_CI::EBegin /*toBegin*/, CScope* scope)
{
    if ( m_RangeEndPosition == kInvalidSeqPos ) {
        m_RangeEndPosition = seqMap->GetLength(scope);
    }
    if ( !m_MinusStrand ) {
        m_Segment = seqMap->find(m_RangePosition, scope);
    }
    else {
        m_Segment = seqMap->find(m_RangeEndPosition-1, scope);
    }
}


void CSeqMapRange_CI::x_Init(CConstRef<CSeqMap> seqMap,
                             CSeqMap_CI::EEnd /*toEnd*/, CScope* scope)
{
    if ( m_RangeEndPosition == kInvalidSeqPos ) {
        m_RangeEndPosition = seqMap->GetLength(scope);
    }
    if ( !m_MinusStrand ) {
        m_Segment = seqMap->find(m_RangeEndPosition-1, scope);
    }
    else {
        m_Segment = seqMap->find(m_RangePosition, scope);
    }
}


void CSeqMapRange_CI::x_Init(CConstRef<CSeqMap> seqMap,
                             CSeqMap_CI::EPosition /*byPos*/, TSeqPos pos,
                             CScope* scope)
{
    if ( m_RangeEndPosition == kInvalidSeqPos ) {
        m_RangeEndPosition = seqMap->GetLength(scope);
    }
    if ( !m_MinusStrand ) {
        m_Segment = seqMap->find(m_RangePosition + pos, scope);
    }
    else {
        m_Segment = seqMap->find(m_RangeEndPosition - 1 - pos, scope);
    }
}


TSeqPos CSeqMapRange_CI::GetPosition(void) const
{
    if ( !m_MinusStrand ) {
        return GetSegPosition() - m_RangePosition;
    }
    else {
        return m_RangeEndPosition - GetSegEndPosition();
    }
}


TSeqPos CSeqMapRange_CI::GetEndPosition(void) const
{
    if ( !m_MinusStrand ) {
        return GetSegEndPosition() - m_RangePosition;
    }
    else {
        return m_RangeEndPosition - GetSegPosition();
    }
}


bool CSeqMapRange_CI::x_Move(bool minus)
{
    if ( !minus ) {
        return
            m_Segment.GetPosition() < m_RangeEndPosition && m_Segment.Next() &&
            m_Segment.GetPosition() < m_RangeEndPosition;
    }
    else {
        return
            m_Segment.GetEndPosition() > m_RangePosition && m_Segment.Prev() &&
            m_Segment.GetEndPosition() > m_RangePosition;
    }
}


CSeqMap::ESegmentType CSeqMapRange_CI::GetRealType(void) const
{
    return InRange()? m_Segment.GetType(): CSeqMap::eSeqEnd;
}


CSeqMap::ESegmentType CSeqMapRange_CI::GetType(void) const
{
    CSeqMap::ESegmentType type = GetRealType();
    if ( m_Seqid && type == CSeqMap::eSeqData )
        type = CSeqMap::eSeqRef;
    return type;
}


bool CSeqMapRange_CI::ReplacedData(void) const
{
    return m_Seqid && GetRealType() == CSeqMap::eSeqData;
}


const CSeq_data& CSeqMapRange_CI::GetData(void) const
{
    if ( ReplacedData() ) {
        THROW1_TRACE(runtime_error,
                     "CSeqMapRange_CI::GetData: invalid segment type");
    }
    return m_Segment.GetData();
}


CSeq_id_Handle CSeqMapRange_CI::GetRefSeqid(void) const
{
    return ReplacedData()? m_Seqid: m_Segment.GetRefSeqid();
}


TSeqPos CSeqMapRange_CI::GetRefPosition(void) const
{
    if ( ReplacedData() )
        return GetSegPosition();

    TSeqPos pos = m_Segment.GetRefPosition();
    if ( !m_Segment.GetRefMinusStrand() ) {
        pos += GetSegPosition() - m_Segment.GetPosition();
    }
    else {
        pos += m_Segment.GetEndPosition() - GetSegEndPosition();
    }
    return pos;
}


TSeqPos CSeqMapRange_CI::GetRefEndPosition(void) const
{
    if ( ReplacedData() )
        return GetSegEndPosition();

    TSeqPos pos = m_Segment.GetRefPosition();
    if ( !m_Segment.GetRefMinusStrand() ) {
        pos += GetSegEndPosition() - m_Segment.GetPosition();
    }
    else {
        pos += m_Segment.GetEndPosition() - GetSegPosition();
    }
    return pos;
}


bool CSeqMapRange_CI::GetRefMinusStrand(void) const
{
    bool minusStrand = m_MinusStrand;
    if ( !ReplacedData() )
        minusStrand ^= m_Segment.GetRefMinusStrand();
    return minusStrand;
}


bool CSeqMapRange_CI::IsRefData(void) const
{
    return GetRealType() == CSeqMap::eSeqData;
}


const CSeq_data& CSeqMapRange_CI::GetRefData(void) const
{
    return m_Segment.GetData();
}


////////////////////////////////////////////////////////////////////
//  CSeqMapResolved_CI

CSeqMapResolved_CI::CSeqMapResolved_CI(void)
{
}


CSeqMapResolved_CI::CSeqMapResolved_CI(const CSeqMap_CI& seg)
{
    x_Init(seg, seg.GetPosition());
}


CSeqMapResolved_CI::CSeqMapResolved_CI(const CSeqMap_CI& seg, TSeqPos pos)
{
    x_Init(seg, pos);
}


CSeqMapResolved_CI::CSeqMapResolved_CI(const pair<CSeqMap_CI, TSeqPos> pos)
{
    x_Init(pos.first, pos.second);
}


CSeqMapResolved_CI::~CSeqMapResolved_CI(void)
{
}


CSeq_id_Handle CSeqMapResolved_CI::GetRefSeqid(void) const
{
    return m_Stack.top().GetRefSeqid();
}


void CSeqMapResolved_CI::x_Push(TSeqPos pos)
{
    m_Stack.push(CSeqMapRange_CI(CSeqMap_CI::ePosition,
                                 pos-GetPosition(),
                                 m_Stack.top()));
    m_Position += x_GetTopOffset();
}


bool CSeqMapResolved_CI::x_Pop(void)
{
    if ( m_Stack.size() <= 1 )
        return false;
    m_Position -= x_GetTopOffset();
    m_Stack.pop();
    return true;
}


bool CSeqMapResolved_CI::x_TopNext(void)
{
    TSeqPos upPos = x_GetUpPosition();
    bool ret = m_Stack.top().Next();
    m_Position = upPos + x_GetTopOffset();
    return ret;
}


bool CSeqMapResolved_CI::x_TopPrev(void)
{
    TSeqPos upPos = x_GetUpPosition();
    bool ret = m_Stack.top().Prev();
    m_Position = upPos + x_GetTopOffset();
    return ret;
}


void CSeqMapResolved_CI::x_Resolve(TSeqPos position)
{
    while ( x_IsReference() ) {
        x_Push(position);
    }
}


void CSeqMapResolved_CI::x_Init(const CSeqMap_CI& seg, TSeqPos position)
{
    m_Stack.push(CSeqMapRange_CI(CSeqMapRange_CI::eWhole, seg));
    m_Position = seg.GetPosition();
    x_Resolve(position);
}


bool CSeqMapResolved_CI::Next(void)
{
    while ( !x_TopNext() ) {
        if ( !x_Pop() )
            return false;
    }
    x_Resolve(GetPosition());
    return true;
}


bool CSeqMapResolved_CI::Prev(void)
{
    while ( !x_TopPrev() ) {
        if ( !x_Pop() )
            return false;
    }
    x_Resolve(GetEndPosition()-1);
    return true;
}

END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2003/01/23 19:32:02  vasilche
* Commented out unused code. Prepared for removal.
*
* Revision 1.3  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.2  2002/12/26 20:55:18  dicuccio
* Moved seq_id_mapper.hpp, tse_info.hpp, and bioseq_info.hpp -> include/ tree
*
* Revision 1.1  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
*
* ===========================================================================
*/
#endif
