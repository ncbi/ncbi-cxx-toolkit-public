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
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/impl/scope_impl.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
// SSeqMapSelector
/////////////////////////////////////////////////////////////////////////////

SSeqMapSelector& SSeqMapSelector::SetLimitTSE(const CSeq_entry* tse)
{
    m_TSE.Reset(tse);
    m_TSE_Info.Reset();
    return *this;
}

SSeqMapSelector& SSeqMapSelector::SetLimitTSE(const CSeq_entry_Handle& tse)
{
    if ( tse ) {
        m_TSE_Info = tse.GetTSE_Info();
        m_TSE = tse.GetSeq_entryCore();
    }
    else {
        m_TSE_Info.Reset();
        m_TSE.Reset();
    }
    return *this;
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
             index == seqMap.x_GetSegmentsCount() )
            return false;
        m_Index = ++index;
        seqMap.x_GetSegmentLength(index, scope); // Update length of segment
        return seqMap.x_GetSegmentPosition(index, scope) < m_LevelRangeEnd;
    }
    else {
        if ( old_seg.m_Position + old_seg.m_Length < m_LevelRangePos ||
             index == 0 )
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
{
    m_Selector.SetPosition(kInvalidSeqPos)
        .SetResolveCount(0)
        .SetFlags(CSeqMap::fDefaultFlags);
}

CSeqMap_CI::CSeqMap_CI(const CConstRef<CSeqMap>& seqMap,
                       CScope* scope,
                       TSeqPos pos,
                       size_t maxResolveCount,
                       TFlags flags)
    : m_Scope(scope)
{
    m_Selector
        .SetResolveCount(maxResolveCount)
        .SetFlags(flags);
    if (pos >= seqMap->GetLength(scope)) {
        // Special case - end of seq map
        m_Selector.SetRange(seqMap->GetLength(scope), 0);
        TSegmentInfo push;
        push.m_SeqMap = seqMap;
        push.m_LevelRangePos = 0;
        push.m_LevelRangeEnd = GetPosition();
        push.m_MinusStrand = false;
        push.m_Index = seqMap->x_GetSegmentsCount();
        m_Stack.push_back(push);
        return;
    }
    x_Push(seqMap, 0, seqMap->GetLength(scope), false, pos);
    while ( !x_Found() ) {
        if ( !x_Push(pos - GetPosition()) ) {
            x_SettleNext();
            break;
        }
    }
}


CSeqMap_CI::CSeqMap_CI(const CConstRef<CSeqMap>& seqMap,
                       CScope* scope,
                       TSeqPos pos,
                       ENa_strand strand,
                       size_t maxResolveCount,
                       TFlags flags)
    : m_Scope(scope)
{
    m_Selector
        .SetResolveCount(maxResolveCount)
        .SetFlags(flags);
    if (pos >= seqMap->GetLength(scope)) {
        // Special case - end of seq map
        m_Selector.SetRange(seqMap->GetLength(scope), 0);
        TSegmentInfo push;
        push.m_SeqMap = seqMap;
        push.m_LevelRangePos = 0;
        push.m_LevelRangeEnd = GetPosition();
        push.m_MinusStrand = false;
        push.m_Index = seqMap->x_GetSegmentsCount();
        m_Stack.push_back(push);
        return;
    }
    x_Push(seqMap, 0, seqMap->GetLength(scope), IsReverse(strand), pos);
    while ( !x_Found() ) {
        if ( !x_Push(pos - GetPosition()) ) {
            x_SettleNext();
            break;
        }
    }
}


CSeqMap_CI::CSeqMap_CI(const CConstRef<CSeqMap>& seqMap,
                       CScope* scope,
                       SSeqMapSelector& selector,
                       ENa_strand strand)
    : m_Scope(scope),
      m_Selector(selector)
{
    TSeqPos pos = GetPosition();
    x_Push(seqMap,
           m_Selector.m_Position,
           m_Selector.m_Length,
           IsReverse(strand), 0);
    while ( !x_Found() ) {
        if ( !x_Push(pos - GetPosition()) ) {
            x_SettleNext();
            break;
        }
    }
}


CSeqMap_CI::CSeqMap_CI(const CConstRef<CSeqMap>& seqMap,
                       CScope* scope,
                       SSeqMapSelector& selector)
    : m_Scope(scope),
      m_Selector(selector)
{
    TSeqPos pos = GetPosition();
    if (m_Selector.m_Length == kInvalidSeqPos) {
        m_Selector.SetRange(pos, seqMap->GetLength(scope));
    }
    x_Push(seqMap,
           m_Selector.m_Position,
           m_Selector.m_Length,
           false, 0);
    while ( !x_Found() ) {
        if ( !x_Push(pos - GetPosition()) ) {
            x_SettleNext();
            break;
        }
    }
}


CSeqMap_CI::~CSeqMap_CI(void)
{
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


CSeq_id_Handle CSeqMap_CI::GetRefSeqid(void) const
{
    if ( !*this ) {
        NCBI_THROW(CSeqMapException, eOutOfRange,
                   "Iterator out of range");
    }
    return CSeq_id_Mapper::GetInstance()->
        GetHandle(x_GetSeqMap().x_GetRefSeqid(x_GetSegment()));
}


TSeqPos CSeqMap_CI_SegmentInfo::GetRefPosition(void) const
{
    if ( !InRange() ) {
        NCBI_THROW(CSeqMapException, eOutOfRange,
                   "Iterator out of range");
    }
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
        offset = min(x_GetLevelRealPos(), m_LevelRangeEnd) - m_LevelRangePos;
    }
    else {
        offset = m_LevelRangeEnd - max(x_GetLevelRealEnd(), m_LevelRangePos);
    }
    if ( offset < 0 )
        offset = 0;
    return offset;
}


TSeqPos CSeqMap_CI::x_GetTopOffset(void) const
{
    return x_GetSegmentInfo().x_GetTopOffset();
}


bool CSeqMap_CI::x_RefTSEMatch(const CSeqMap::CSegment& seg) const
{
    _ASSERT(m_Selector.m_TSE);
    _ASSERT(CSeqMap::ESegmentType(seg.m_SegType) == CSeqMap::eSeqRef);
    CSeq_id_Handle id = CSeq_id_Mapper::GetInstance()->
        GetHandle(x_GetSeqMap().x_GetRefSeqid(seg));
    CSeq_entry_Handle tse_info;
    CScope& scope = *m_Scope;
    if ( m_Selector.m_TSE_Info ) {
        tse_info =
            CSeq_entry_Handle(scope,
                              static_cast<const CTSE_Info&>
                              (*m_Selector.m_TSE_Info));
    }
    else {
        tse_info = scope.GetSeq_entryHandle(*m_Selector.m_TSE);
        const_cast<SSeqMapSelector&>(m_Selector).m_TSE_Info =
            tse_info.GetTSE_Info();
    }
    return scope.GetBioseqHandleFromTSE(id, tse_info);
}


inline
bool CSeqMap_CI::x_CanResolve(const CSeqMap::CSegment& seg) const
{
    return m_Selector.CanResolve() &&
        (!m_Selector.m_TSE || x_RefTSEMatch(seg));
}


bool CSeqMap_CI::x_Push(TSeqPos pos, bool resolveExternal)
{
    const TSegmentInfo& info = x_GetSegmentInfo();
    const CSeqMap::CSegment& seg = info.x_GetSegment();
    CSeqMap::ESegmentType type = CSeqMap::ESegmentType(seg.m_SegType);
    if ( !(type == CSeqMap::eSeqSubMap  ||
           type == CSeqMap::eSeqRef  &&  resolveExternal) ) {
        return false;
    }
    // Check TSE limit
    if ( bool(m_Selector.m_TSE)  &&  !x_RefTSEMatch(seg) ) {
        return false;
    }
    x_Push(info.m_SeqMap->x_GetSubSeqMap(seg, GetScope(), resolveExternal),
           GetRefPosition(), GetLength(), GetRefMinusStrand(), pos);
    if ( type == CSeqMap::eSeqRef ) {
        m_Selector.PushResolve();
    }
    return true;
}


void CSeqMap_CI::x_Push(const CConstRef<CSeqMap>& seqMap,
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
    if ( x_Push(0, resolveExternal) ) {
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
    while ( x_Push(m_Selector.m_Length-1) ) {
    }
    return true;
}


bool CSeqMap_CI::x_Found(void) const
{
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
    while ( !x_Found() ) {
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


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.26  2004/07/12 15:05:32  grichenk
* Moved seq-id mapper from xobjmgr to seq library
*
* Revision 1.25  2004/06/10 16:21:27  grichenk
* Changed CSeq_id_Mapper singleton type to pointer, GetSeq_id_Mapper
* returns CRef<> which is locked by CObjectManager.
*
* Revision 1.24  2004/06/08 13:33:28  grichenk
* Fixed segment type checking in CSeqMap_CI
*
* Revision 1.23  2004/05/21 21:42:13  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.22  2004/04/12 16:49:16  vasilche
* Allow null scope in CSeqMap_CI and CSeqVector.
*
* Revision 1.21  2004/03/16 15:47:28  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.20  2003/11/10 18:12:09  grichenk
* Removed extra EFlags declaration from seq_map_ci.hpp
*
* Revision 1.19  2003/09/30 16:22:04  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* Revision 1.18  2003/09/05 17:29:40  grichenk
* Structurized Object Manager exceptions
*
* Revision 1.17  2003/08/27 14:27:19  vasilche
* Use Reverse(ENa_strand) function.
*
* Revision 1.16  2003/07/17 22:51:31  vasilche
* Fixed unused variables warnings.
*
* Revision 1.15  2003/07/14 21:13:26  grichenk
* Added possibility to resolve seq-map iterator withing a single TSE
* and to skip intermediate references during this resolving.
*
* Revision 1.14  2003/06/02 16:06:38  dicuccio
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
* Revision 1.13  2003/05/23 16:32:21  vasilche
* Fixed backward traversal of CSeqMap_CI.
*
* Revision 1.12  2003/05/20 20:36:14  vasilche
* Added FindResolved() with strand argument.
*
* Revision 1.11  2003/05/20 15:44:38  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.10  2003/02/27 16:29:19  vasilche
* Fixed lost features from first segment.
*
* Revision 1.9  2003/02/25 14:48:29  vasilche
* Removed performance warning on Windows.
*
* Revision 1.8  2003/02/24 18:57:22  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.7  2003/02/11 19:26:18  vasilche
* Fixed CSeqMap_CI with ending NULL segment.
*
* Revision 1.6  2003/02/05 17:59:17  dicuccio
* Moved formerly private headers into include/objects/objmgr/impl
*
* Revision 1.5  2003/02/05 15:55:26  vasilche
* Added eSeqEnd segment at the beginning of seq map.
* Added flags to CSeqMap_CI to stop on data, gap, or references.
*
* Revision 1.4  2003/01/29 22:03:46  grichenk
* Use single static CSeq_id_Mapper instead of per-OM model.
*
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
