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
*           Aleksey Grichenko
*           Michael Kimelman
*           Andrei Gourianov
*           Eugene Vasilchenko
*
* File Description:
*   Sequence map for the Object Manager. Describes sequence as a set of
*   segments of different types (data, reference, gap or end).
*
*/

#include <objects/objmgr/seq_map.hpp>
#include <objects/objmgr/seq_map_ext.hpp>
#include <objects/objmgr/seq_id_mapper.hpp>
#include "data_source.hpp"
#include <objects/objmgr/scope.hpp>
#include <objects/objmgr/bioseq_handle.hpp>

#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/Ref_ext.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Packed_seqint.hpp>
#include <objects/seqloc/Packed_seqpnt.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

////////////////////////////////////////////////////////////////////
//  CSeqMap::CSegment

inline
CSeqMap::CSegment::CSegment(ESegmentType seg_type,
                            TSeqPos length)
    : m_Position(kInvalidSeqPos),
      m_Length(length),
      m_SegType(seg_type),
      m_RefObjectType(eNull),
      m_RefMinusStrand(false),
      m_RefPosition(0)
{
}

////////////////////////////////////////////////////////////////////
//  CSeqMap


CSeqMap::CSeqMap(CDataSource* source)
    : //m_ParentSeqMap(0),
      //m_ParentIndex(0),
      m_Resolved(0),
      m_Source(source)
{
    //m_LockCounter.Set(0);
}


CSeqMap::CSeqMap(CSeqMap* parent, size_t index)
    : //m_ParentSeqMap(parent),
      //m_ParentIndex(index),
      m_Resolved(0),
      m_Source(parent->m_Source)
{
    //m_LockCounter.Set(0);
    //parent->x_Lock();
}


CSeqMap::CSeqMap(CSeq_loc& ref, CDataSource* source)
    : //m_ParentSeqMap(0),
      //m_ParentIndex(0),
      m_Resolved(0),
      m_Source(source)
{
    x_Add(ref);
    x_AddEnd();
}


CSeqMap::CSeqMap(CSeq_data& data, TSeqPos length, CDataSource* source)
    : //m_ParentSeqMap(0),
      //m_ParentIndex(0),
      m_Resolved(0),
      m_Source(source)
{
    x_Add(data, length);
    x_AddEnd();
}


CSeqMap::CSeqMap(TSeqPos length, CDataSource* source)
    : //m_ParentSeqMap(0),
      //m_ParentIndex(0),
      m_Resolved(0),
      m_Source(source)
{
    x_AddGap(length);
    x_AddEnd();
}


CSeqMap::~CSeqMap(void)
{
    m_Resolved = 0;
    m_Segments.clear();
    //_ASSERT(m_LockCounter.Get() == 0);
    /*
    if ( const CSeqMap* parent = m_ParentSeqMap ) {
        parent->x_Unlock();
    }
    */
}

/*
void CSeqMap::x_SetParent(CSeqMap* parent, size_t index)
{
    _ASSERT(!m_ParentSeqMap);
    m_ParentSeqMap = parent;
    m_ParentIndex = index;
    m_Source = parent->m_Source;
    parent->x_Lock();
}
*/
/*
void CSeqMap::x_Lock(void) const
{
    if ( m_LockCounter.Add(1) == 1 ) {
        m_LockCounter.Add(1);
    }
}


void CSeqMap::x_Unlock(void) const
{
    if ( m_LockCounter.Add(-1) == 1 ) {
        // m_LastUsed = CTime::Now(); // update time field
        m_LockCounter.Add(-1);
    }
}
*/

void CSeqMap::x_GetSegmentException(size_t /*index*/) const
{
    THROW1_TRACE(runtime_error,
                 "CSeqMap::x_GetSegment: invalid segment index");
}


CSeqMap::CSegment& CSeqMap::x_SetSegment(size_t index)
{
    if ( index >= x_GetSegmentsCount() ) {
        THROW1_TRACE(runtime_error,
                     "CSeqMap::x_SetSegment: invalid segment index");
    }
    return m_Segments[index];
}


CBioseq_Handle CSeqMap::x_GetBioseqHandle(const CSegment& seg, CScope* scope) const
{
    if ( !scope ) {
        THROW1_TRACE(runtime_error,
                     "CSeqMap::x)GetBioseqHandle: "
                     "scope is null");
    }
    CBioseq_Handle bh = scope->GetBioseqHandle(x_GetRefSeqid(seg));
    if ( !bh ) {
        THROW1_TRACE(runtime_error,
                     "CSeqMap::x_GetBioseqHandle: "
                     "cannot resolve reference");
    }
    return bh;
}


TSeqPos CSeqMap::x_ResolveSegmentLength(size_t index, CScope* scope) const
{
    const CSegment& seg = x_GetSegment(index);
    TSeqPos length = seg.m_Length;
    if ( length == kInvalidSeqPos ) {
        if ( seg.m_SegType == eSeqSubMap ) {
            length = x_GetSubSeqMap(seg, scope)->GetLength(scope);
        }
        else if ( seg.m_SegType == eSeqRef ) {
            CBioseq_Handle bh = x_GetBioseqHandle(seg, scope);
            CBioseq_Handle::TBioseqCore seq = bh.GetBioseqCore();
            if ( !seq->GetInst().IsSetLength() ) {
                THROW1_TRACE(runtime_error,
                             "CSeqMap::x_ResolveSegmentLength: "
                             "length of sequence is not set");
            }
            length = seq->GetInst().GetLength();
        }
        _ASSERT(length != kInvalidSeqPos);
        seg.m_Length = length;
    }
    return length;
}


TSeqPos CSeqMap::x_ResolveSegmentPosition(size_t index, CScope* scope) const
{
    if ( index > x_GetSegmentsCount() ) {
        THROW1_TRACE(runtime_error,
                     "CSeqMap::x_GetSegmentPosition: invalid segment index");
    }
    size_t resolved = m_Resolved;
    if ( index <= resolved )
        return x_GetSegment(index).m_Position;
    TSeqPos resolved_pos = x_GetSegment(resolved).m_Position;
    do {
        resolved_pos += x_GetSegmentLength(resolved, scope);
        m_Segments[++resolved].m_Position = resolved_pos;
    } while ( resolved < index );
    {{
        CFastMutexGuard guard(m_SeqMap_Mtx);
        if ( m_Resolved < resolved )
            m_Resolved = resolved;
    }}
    return resolved_pos;
}


size_t CSeqMap::x_FindSegment(TSeqPos pos, CScope* scope) const
{
    size_t resolved = m_Resolved;
    TSeqPos resolved_pos = x_GetSegment(resolved).m_Position;
    if ( resolved_pos <= pos ) {
        do {
            if ( resolved >= x_GetSegmentsCount() ) {
                // end of segments
                m_Resolved = resolved;
                return size_t(-1);
            }
            resolved_pos += x_GetSegmentLength(resolved, scope);
            m_Segments[++resolved].m_Position = resolved_pos;
        } while ( resolved_pos <= pos );
        {{
            CFastMutexGuard guard(m_SeqMap_Mtx);
            if ( m_Resolved < resolved )
                m_Resolved = resolved;
        }}
        return resolved - 1;
    }
    else {
        TSegments::const_iterator end = m_Segments.begin()+resolved;
        TSegments::const_iterator it = 
            upper_bound(m_Segments.begin(), end,
                        pos, SPosLessSegment());
        if ( it == end ) {
            return size_t(-1);
        }
        return it - m_Segments.begin();
    }
}


void CSeqMap::x_LoadObject(const CSegment& seg) const
{
    _ASSERT(seg.m_Position != kInvalidSeqPos);
    _ASSERT(m_Source);
    if ( !seg.m_RefObject ) {
        THROW1_TRACE(runtime_error,
                     "CSeqMap::x_LoadObject: cannot load part of seqmap");
    }
}

/*
const CObject* CSeqMap::x_GetObject(const CSegment& seg) const
{
    if ( !seg.m_RefObject ) {
        x_LoadObject(seg);
    }
    return seg.m_RefObject.GetPointer();
}


CObject* CSeqMap::x_GetObject(CSegment& seg)
{
    if ( !seg.m_Object ) {
        x_LoadObject(seg);
    }
    return seg.m_Object.GetPointer();
}
*/

CConstRef<CSeqMap> CSeqMap::x_GetSubSeqMap(const CSegment& seg, CScope* scope,
                                           bool resolveExternal) const
{
    CConstRef<CSeqMap> ret;
    if ( seg.m_SegType == eSeqSubMap ) {
        if ( !seg.m_RefObject ) {
            x_LoadObject(seg);
        }
        if ( !seg.m_RefObject ) {
            THROW1_TRACE(runtime_error,
                         "CSeqMap::x_GetSubSeqMap: null CSeqMap pointer");
        }
        if ( seg.m_RefObjectType != eSeqMap ) {
            THROW1_TRACE(runtime_error,
                         "CSeqMap::x_GetSubSeqMap: incompatible object type");
        }
        ret.Reset(static_cast<const CSeqMap*>(seg.m_RefObject.GetPointer()));
    }
    else if ( resolveExternal && seg.m_SegType == eSeqRef ) {
        ret.Reset(&x_GetBioseqHandle(seg, scope).GetSeqMap());
    }
    return ret;
}

/*
CSeqMap* CSeqMap::x_GetSubSeqMap(CSegment& seg)
{
    if ( seg.m_SegType != eSeqSubMap )
        return 0;
    return static_cast<CSeqMap*>(x_GetObject(seg));
}
*/

void CSeqMap::x_SetSubSeqMap(size_t /*index*/, CSeqMap_Delta_seqs* /*subMap*/)
{
    // not valid in generic seq map -> incompatible objects
    THROW1_TRACE(runtime_error,
                 "CSeqMap::x_SetSeq_data: invalid parent map");
}


const CSeq_data& CSeqMap::x_GetSeq_data(const CSegment& seg) const
{
    if ( seg.m_SegType == eSeqData ) {
        if ( !seg.m_RefObject ) {
            x_LoadObject(seg);
        }
        if ( !seg.m_RefObject ) {
            THROW1_TRACE(runtime_error,
                         "CSeqMap::x_GetSeq_data: null CSeq_data pointer");
        }
        if ( seg.m_RefObjectType != eSeq_data ) {
            THROW1_TRACE(runtime_error,
                         "CSeqMap::x_GetSeq_data: incompatible object type");
        }
        return *static_cast<const CSeq_data*>(seg.m_RefObject.GetPointer());
    }
    THROW1_TRACE(runtime_error,
                 "CSeqMap::x_GetSeq_data: invalid segment type");
}


void CSeqMap::x_SetSeq_data(size_t index, CSeq_data& data)
{
    // check segment type
    CSegment& seg = x_SetSegment(index);
    if ( seg.m_SegType != eSeqData ) {
        THROW1_TRACE(runtime_error,
                     "CSeqMap::x_SetSeq_data: invalid segment type");
    }
    // lock for object modification
    CFastMutexGuard guard(m_SeqMap_Mtx);
    // check for object
    if ( seg.m_RefObject ) {
        THROW1_TRACE(runtime_error,
                     "CSeqMap::x_SetSeq_data: CSeq_data already set");
    }
    // set object
    seg.m_RefObject.Reset(&data);
    seg.m_RefObjectType = eSeq_data;
}


const CSeq_id& CSeqMap::x_GetRefSeqid(const CSegment& seg) const
{
    if ( seg.m_SegType == eSeqRef ) {
        return static_cast<const CSeq_id&>(*seg.m_RefObject);
    }
    THROW1_TRACE(runtime_error,
                 "CSeqMap::x_GetRefSeqid: invalid segment type");
}


TSeqPos CSeqMap::x_GetRefPosition(const CSegment& seg) const
{
    return seg.m_RefPosition;
}


bool CSeqMap::x_GetRefMinusStrand(const CSegment& seg) const
{
    return seg.m_RefMinusStrand;
}


CSeqMap::TSegment_CI CSeqMap::Begin(CScope* scope) const
{
    return TSegment_CI(CConstRef<CSeqMap>(this), scope, TSegment_CI::eBegin);
}


CSeqMap::TSegment_CI CSeqMap::End(CScope* scope) const
{
    return TSegment_CI(CConstRef<CSeqMap>(this), scope, TSegment_CI::eEnd);
}


CSeqMap::TSegment_CI CSeqMap::FindSegment(TSeqPos pos, CScope* scope) const
{
    return TSegment_CI(CConstRef<CSeqMap>(this), scope,
                       TSegment_CI::ePosition, pos);
}


CSeqMap::const_iterator CSeqMap::begin(CScope* scope) const
{
    return Begin(scope);
}


CSeqMap::const_iterator CSeqMap::end(CScope* scope) const
{
    return End(scope);
}


CSeqMap::const_iterator CSeqMap::find(TSeqPos pos, CScope* scope) const
{
    return FindSegment(pos, scope);
}


CSeqMap::TSegment_CI CSeqMap::BeginResolved(CScope* scope, size_t maxResolveCount) const
{
    return begin_resolved(scope, maxResolveCount);
}


CSeqMap::TSegment_CI CSeqMap::EndResolved(CScope* scope, size_t maxResolveCount) const
{
    return end_resolved(scope, maxResolveCount);
}


CSeqMap::TSegment_CI CSeqMap::FindResolved(TSeqPos pos, CScope* scope,
                                           size_t maxResolveCount) const
{
    return find_resolved(pos, scope, maxResolveCount);
}


CSeqMap::const_iterator CSeqMap::begin_resolved(CScope* scope,
                                                size_t maxResolveCount) const
{
    return TSegment_CI(CConstRef<CSeqMap>(this), scope,
                       TSegment_CI::eBegin,
                       maxResolveCount);
}


CSeqMap::const_iterator CSeqMap::end_resolved(CScope* scope,
                                              size_t maxResolveCount) const
{
    return TSegment_CI(CConstRef<CSeqMap>(this), scope,
                       TSegment_CI::eEnd,
                       maxResolveCount);
}


CSeqMap::const_iterator CSeqMap::find_resolved(TSeqPos pos, CScope* scope,
                                               size_t maxResolveCount) const
{
    return TSegment_CI(CConstRef<CSeqMap>(this), scope,
                       TSegment_CI::ePosition, pos,
                       maxResolveCount);
}


CSeqMap::TSegment_CI
CSeqMap::ResolvedRangeIterator(CScope* scope,
                               ENa_strand strand,
                               TSeqPos from,
                               TSeqPos length,
                               size_t maxResolveCount) const
{
    if ( strand == eNa_strand_minus ) {
        from = GetLength(scope) - from - length;
    }
    return TSegment_CI(CConstRef<CSeqMap>(this), scope,
                       from, length, strand,
                       TSegment_CI::eBegin,
                       maxResolveCount);
}


CSeqMap::TSegment_CI
CSeqMap::ResolvedRangeIterator(CScope* scope,
                               TSeqPos from,
                               TSeqPos length,
                               ENa_strand strand,
                               size_t maxResolveCount) const
{
    return TSegment_CI(CConstRef<CSeqMap>(this), scope,
                       from, length, strand,
                       TSegment_CI::eBegin,
                       maxResolveCount);
}


void CSeqMap::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
#if 0
    ddc.SetFrame("CSeqMap");
    CObject::DebugDump( ddc, depth);

    DebugDumpValue(ddc, "m_FirstUnresolvedPos", m_FirstUnresolvedPos);
    if (depth == 0) {
        DebugDumpValue(ddc, "m_Data.size()", m_Data.size());
    } else {
        DebugDumpValue(ddc, "m_Data.type",
            "vector<CRef<CSegment>>");
        DebugDumpRangeCRef(ddc, "m_Data",
            m_Data.begin(), m_Data.end(), depth);
    }
#endif
}


CConstRef<CSeqMap> CSeqMap::CreateSeqMapForBioseq(CBioseq& seq,
                                                  CDataSource* source)
{
    CConstRef<CSeqMap> ret;
    CSeq_inst& inst = seq.SetInst();
    if ( inst.IsSetSeq_data() ) {
        ret.Reset(new CSeqMap(inst.SetSeq_data(), inst.GetLength(), source));
    }
    else if ( inst.IsSetExt() ) {
        CSeq_ext& ext = inst.SetExt();
        switch (ext.Which()) {
        case CSeq_ext::e_Seg:
            ret.Reset(new CSeqMap_Seq_locs(ext.SetSeg(), ext.SetSeg().Set(),
                                           source));
            break;
        case CSeq_ext::e_Ref:
            ret = CreateSeqMapForSeq_loc(ext.SetRef().Set(), source);
            break;
        case CSeq_ext::e_Delta:
            ret.Reset(new CSeqMap_Delta_seqs(ext.SetDelta(), source));
            break;
        case CSeq_ext::e_Map:
            //### Not implemented
            THROW1_TRACE(runtime_error, "CSeq_ext::e_Map -- not implemented");
        default:
            //### Not implemented
            THROW1_TRACE(runtime_error, "CSeq_ext::??? -- not implemented");
        }
    }
    else {
        // Virtual sequence -- no data, no segments
        _ASSERT(inst.GetRepr() == CSeq_inst::eRepr_virtual);
        // The total sequence is gap
        ret.Reset(new CSeqMap(inst.GetLength()));
    }
    return ret;
}


CConstRef<CSeqMap> CSeqMap::CreateSeqMapForSeq_loc(const CSeq_loc& loc,
                                                   CDataSource* source)
{
    return CConstRef<CSeqMap>(new CSeqMap(const_cast<CSeq_loc&>(loc), source));
}


CConstRef<CSeqMap> CSeqMap::CreateSeqMapForStrand(CConstRef<CSeqMap> seqMap,
                                                  ENa_strand strand)
{
    if ( strand == eNa_strand_minus ) {
        CRef<CSeqMap> ret(new CSeqMap());
        ret->x_AddSegment(eSeqSubMap,
                          const_cast<CSeqMap*>(seqMap.GetPointer()),
                          0, kInvalidSeqPos, strand).m_RefObjectType = eSeqMap;
        ret->x_AddEnd();
        seqMap = ret;
    }
    return seqMap;
}


CSeqMap::CSegment& CSeqMap::x_AddSegment(ESegmentType type, TSeqPos len)
{
    m_Segments.push_back(CSegment(type, len));
    if ( m_Segments.size() == 1 )
        m_Segments.front().m_Position = 0;
    return m_Segments.back();
}


CSeqMap::CSegment& CSeqMap::x_AddSegment(ESegmentType type, TSeqPos len,
                                         CObject* object)
{
    CSegment& ret = x_AddSegment(type, len);
    ret.m_RefObject.Reset(object);
    return ret;
}


CSeqMap::CSegment& CSeqMap::x_AddSegment(ESegmentType type,
                                         CObject* object,
                                         TSeqPos refPos,
                                         TSeqPos len,
                                         ENa_strand strand)
{
    CSegment& ret = x_AddSegment(type, len, object);
    ret.m_RefPosition = refPos;
    ret.m_RefMinusStrand = strand == eNa_strand_minus;
    return ret;
}


void CSeqMap::x_AddEnd(void)
{
    x_AddSegment(eSeqEnd, 0);
}


CSeqMap::CSegment& CSeqMap::x_AddGap(TSeqPos len)
{
    return x_AddSegment(eSeqGap, len);
}


CSeqMap::CSegment& CSeqMap::x_AddUnloadedSubMap(TSeqPos len)
{
    return x_AddSegment(eSeqSubMap, len);
}


CSeqMap::CSegment& CSeqMap::x_AddUnloadedSeq_data(TSeqPos len)
{
    return x_AddSegment(eSeqData, len);
}


CSeqMap::CSegment& CSeqMap::x_Add(CSeq_data& data, TSeqPos len)
{
    CSegment& seg = x_AddSegment(eSeqData, len, &data);
    seg.m_RefObjectType = eSeq_data;
    return seg;
}


CSeqMap::CSegment& CSeqMap::x_Add(CSeq_point& ref)
{
    CSegment& seg = x_AddSegment(eSeqRef, &ref.SetId(),
                                 ref.GetPoint(), 1, ref.GetStrand());
    seg.m_RefObjectType = eSeq_id;
    return seg;
}


CSeqMap::CSegment& CSeqMap::x_Add(CSeq_interval& ref)
{
    CSegment& seg = x_AddSegment(eSeqRef, &ref.SetId(),
                        ref.GetFrom(), ref.GetLength(), ref.GetStrand());
    seg.m_RefObjectType = eSeq_id;
    return seg;
}


CSeqMap::CSegment& CSeqMap::x_Add(CSeq_id& ref)
{
    CSegment& seg = x_AddSegment(eSeqRef, &ref, 0, kInvalidSeqPos);
    seg.m_RefObjectType = eSeq_id;
    return seg;
}


CSeqMap::CSegment& CSeqMap::x_Add(CSeqMap* submap)
{
    CSegment& seg = x_AddSegment(eSeqSubMap, kInvalidSeqPos, submap);
    seg.m_RefObjectType = eSeqMap;
    return seg;
}


CSeqMap::CSegment& CSeqMap::x_Add(CPacked_seqint& seq)
{
    return x_Add(new CSeqMap_Seq_intervals(seq));
}


CSeqMap::CSegment& CSeqMap::x_Add(CPacked_seqpnt& seq)
{
    return x_Add(new CSeqMap_SeqPoss(seq));
}


CSeqMap::CSegment& CSeqMap::x_Add(CSeq_loc_mix& seq)
{
    return x_Add(new CSeqMap_Seq_locs(seq, seq.Set()));
}


CSeqMap::CSegment& CSeqMap::x_Add(CSeq_loc_equiv& seq)
{
    return x_Add(new CSeqMap_Seq_locs(seq, seq.Set()));
}


CSeqMap::CSegment& CSeqMap::x_Add(CSeq_literal& seq)
{
    if ( seq.IsSetSeq_data() ) {
        return x_Add(seq.SetSeq_data(), seq.GetLength());
    }
    else {
        // No data exist - treat it like a gap
        return x_AddGap(seq.GetLength()); //???
    }
}


CSeqMap::CSegment& CSeqMap::x_Add(CSeq_loc& loc)
{
    switch ( loc.Which() ) {
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Null:
    case CSeq_loc::e_Empty:
        return x_AddGap(0); // Add gap ???
    case CSeq_loc::e_Whole:
        return x_Add(loc.SetWhole());
    case CSeq_loc::e_Int:
        return x_Add(loc.SetInt());
    case CSeq_loc::e_Pnt:
        return x_Add(loc.SetPnt());
    case CSeq_loc::e_Packed_int:
        return x_Add(loc.SetPacked_int());
    case CSeq_loc::e_Packed_pnt:
        return x_Add(loc.SetPacked_pnt());
    case CSeq_loc::e_Mix:
        return x_Add(loc.SetMix());
    case CSeq_loc::e_Equiv:
        return x_Add(loc.SetEquiv());
    case CSeq_loc::e_Bond:
        THROW1_TRACE(runtime_error,
                     "CSeqMap::x_Add() -- "
                     "e_Bond is not allowed as a reference type");
    case CSeq_loc::e_Feat:
        THROW1_TRACE(runtime_error,
                     "CSeqMap::x_Add() -- "
                     "e_Feat is not allowed as a reference type");
    default:
        THROW1_TRACE(runtime_error,
                     "CSeqMap::x_Add() -- "
                     "invalid reference type");
    }
}


CSeqMap::CSegment& CSeqMap::x_Add(CDelta_seq& seq)
{
    switch ( seq.Which() ) {
    case CDelta_seq::e_Loc:
        return x_Add(seq.SetLoc());
    case CDelta_seq::e_Literal:
        return x_Add(seq.SetLiteral());
    default:
        THROW1_TRACE(runtime_error, "CSeqMap::x_Add: empty Delta-seq");
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.29  2003/01/28 17:16:06  vasilche
* Added CSeqMap::ResolvedRangeIterator with strand coordinate translation.
*
* Revision 1.28  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.27  2002/12/26 20:55:18  dicuccio
* Moved seq_id_mapper.hpp, tse_info.hpp, and bioseq_info.hpp -> include/ tree
*
* Revision 1.26  2002/12/26 20:35:14  ucko
* #include <algorithm> for upper_bound<>
*
* Revision 1.25  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.24  2002/11/04 21:29:12  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.23  2002/10/18 19:12:40  grichenk
* Removed mutex pools, converted most static mutexes to non-static.
* Protected CSeqMap::x_Resolve() with mutex. Modified code to prevent
* dead-locks.
*
* Revision 1.22  2002/07/08 20:51:02  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.21  2002/05/29 21:21:13  gouriano
* added debug dump
*
* Revision 1.20  2002/05/06 17:43:06  ivanov
* ssize_t changed to long
*
* Revision 1.19  2002/05/06 17:03:49  ivanov
* Sorry. Rollback to R1.17
*
* Revision 1.18  2002/05/06 16:56:23  ivanov
* Fixed typo ssize_t -> size_t
*
* Revision 1.17  2002/05/06 03:28:47  vakatov
* OM/OM1 renaming
*
* Revision 1.16  2002/05/03 21:28:10  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.15  2002/05/02 20:42:38  grichenk
* throw -> THROW1_TRACE
*
* Revision 1.14  2002/04/30 18:55:41  gouriano
* added GetRefSeqid function
*
* Revision 1.13  2002/04/11 12:07:30  grichenk
* Redesigned CAnnotTypes_CI to resolve segmented sequences correctly.
*
* Revision 1.12  2002/04/04 21:33:55  grichenk
* Fixed x_FindSegment() for sequences with unresolved segments
*
* Revision 1.11  2002/04/03 18:06:48  grichenk
* Fixed segmented sequence bugs (invalid positioning of literals
* and gaps). Improved CSeqVector performance.
*
* Revision 1.9  2002/03/08 21:36:21  gouriano
* *** empty log message ***
*
* Revision 1.8  2002/03/08 21:24:35  gouriano
* fixed errors with unresolvable references
*
* Revision 1.7  2002/02/25 21:05:29  grichenk
* Removed seq-data references caching. Increased MT-safety. Fixed typos.
*
* Revision 1.6  2002/02/21 19:27:06  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.5  2002/02/01 21:49:38  gouriano
* minor changes to make it compilable and run on Solaris Workshop
*
* Revision 1.4  2002/01/30 22:09:28  gouriano
* changed CSeqMap interface
*
* Revision 1.3  2002/01/23 21:59:32  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.2  2002/01/16 16:25:56  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:24  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
