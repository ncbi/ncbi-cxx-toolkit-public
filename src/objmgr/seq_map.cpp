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
* Authors: Aleksey Grichenko, Michael Kimelman, Eugene Vasilchenko,
*          Andrei Gourianov
*
* File Description:
*   Sequence map for the Object Manager. Describes sequence as a set of
*   segments of different types (data, reference, gap or end).
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/seq_map.hpp>
#include <objmgr/seq_map_ci.hpp>
#include <objmgr/seq_map_ext.hpp>
#include <objmgr/seq_id_mapper.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_data.hpp>
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
      m_ObjType(seg_type),
      m_RefMinusStrand(false),
      m_RefPosition(0)
{
}

////////////////////////////////////////////////////////////////////
//  CSeqMap


CSeqMap::CSeqMap(void)
    : m_Resolved(0),
      m_Mol(CSeq_inst::eMol_not_set),
      m_SeqLength(kInvalidSeqPos)
{
}


CSeqMap::CSeqMap(CSeqMap* /*parent*/, size_t /*index*/)
    : m_Resolved(0),
      m_Mol(CSeq_inst::eMol_not_set),
      m_SeqLength(kInvalidSeqPos)
{
}


CSeqMap::CSeqMap(const CSeq_loc& ref)
    : m_Resolved(0),
      m_Mol(CSeq_inst::eMol_not_set),
      m_SeqLength(kInvalidSeqPos)
{
    x_AddEnd();
    x_Add(ref);
    x_AddEnd();
}


CSeqMap::CSeqMap(const CSeq_data& data, TSeqPos length)
    : m_Resolved(0),
      m_Mol(CSeq_inst::eMol_not_set),
      m_SeqLength(kInvalidSeqPos)
{
    x_AddEnd();
    x_Add(data, length);
    x_AddEnd();
}


CSeqMap::CSeqMap(TSeqPos length)
    : m_Resolved(0),
      m_Mol(CSeq_inst::eMol_not_set),
      m_SeqLength(length)
{
    x_AddEnd();
    x_AddGap(length);
    x_AddEnd();
}


CSeqMap::~CSeqMap(void)
{
    m_Resolved = 0;
    m_Segments.clear();
}


void CSeqMap::x_GetSegmentException(size_t /*index*/) const
{
    NCBI_THROW(CSeqMapException, eInvalidIndex,
               "Invalid segment index");
}


CSeqMap::CSegment& CSeqMap::x_SetSegment(size_t index)
{
    if ( index >= x_GetSegmentsCount() ) {
        NCBI_THROW(CSeqMapException, eInvalidIndex,
                   "Invalid segment index");
    }
    return m_Segments[index];
}


CBioseq_Handle CSeqMap::x_GetBioseqHandle(const CSegment& seg, CScope* scope) const
{
    if ( !scope ) {
        NCBI_THROW(CSeqMapException, eNullPointer,
                   "Null scope pointer");
    }
    CBioseq_Handle bh = scope->GetBioseqHandle(x_GetRefSeqid(seg));
    if ( !bh ) {
        NCBI_THROW(CSeqMapException, eFail,
                   "Cannot resolve reference");
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
            length = x_GetBioseqHandle(seg, scope).GetBioseqLength();
        }
        _ASSERT(length != kInvalidSeqPos);
        seg.m_Length = length;
    }
    return length;
}


TSeqPos CSeqMap::x_ResolveSegmentPosition(size_t index, CScope* scope) const
{
    if ( index > x_GetSegmentsCount() ) {
        NCBI_THROW(CSeqMapException, eInvalidIndex,
                   "Invalid segment index");
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
    if ( !seg.m_RefObject || seg.m_SegType != seg.m_ObjType ) {
        CObject* obj = const_cast<CObject*>(seg.m_RefObject.GetPointer());
        if ( obj && seg.m_ObjType == eSeqChunk ) {
            CTSE_Chunk_Info* chunk = dynamic_cast<CTSE_Chunk_Info*>(obj);
            if ( chunk ) {
                chunk->Load();
            }
        }
    }
}


const CObject* CSeqMap::x_GetObject(const CSegment& seg) const
{
    if ( !seg.m_RefObject || seg.m_SegType != seg.m_ObjType ) {
        x_LoadObject(seg);
    }
    if ( !seg.m_RefObject || seg.m_SegType != seg.m_ObjType ) {
        NCBI_THROW(CSeqMapException, eNullPointer, "null object pointer");
    }
    return seg.m_RefObject.GetPointer();
}


void CSeqMap::x_SetObject(const CSegment& seg, const CObject& obj)
{
    // lock for object modification
    CFastMutexGuard guard(m_SeqMap_Mtx);
    // check for object
    if ( bool(seg.m_RefObject) && seg.m_SegType == seg.m_ObjType ) {
        NCBI_THROW(CSeqMapException, eDataError, "object already set");
    }
    // set object
    const_cast<CSegment&>(seg).m_ObjType = seg.m_SegType;
    const_cast<CSegment&>(seg).m_RefObject.Reset(&obj);
}


void CSeqMap::x_SetChunk(const CSegment& seg, CTSE_Chunk_Info& chunk)
{
    // lock for object modification
    //CFastMutexGuard guard(m_SeqMap_Mtx);
    // check for object
    if ( seg.m_ObjType == eSeqChunk ||
         bool(seg.m_RefObject) && seg.m_SegType == seg.m_ObjType ) {
        NCBI_THROW(CSeqMapException, eDataError, "object already set");
    }
    // set object
    const_cast<CSegment&>(seg).m_RefObject.Reset(&chunk);
    const_cast<CSegment&>(seg).m_ObjType = eSeqChunk;
}


CConstRef<CSeqMap> CSeqMap::x_GetSubSeqMap(const CSegment& seg, CScope* scope,
                                           bool resolveExternal) const
{
    CConstRef<CSeqMap> ret;
    if ( seg.m_SegType == eSeqSubMap ) {
        ret.Reset(static_cast<const CSeqMap*>(x_GetObject(seg)));
    }
    else if ( resolveExternal && seg.m_SegType == eSeqRef ) {
        ret.Reset(&x_GetBioseqHandle(seg, scope).GetSeqMap());
    }
    return ret;
}


void CSeqMap::x_SetSubSeqMap(size_t /*index*/, CSeqMap_Delta_seqs* /*subMap*/)
{
    // not valid in generic seq map -> incompatible objects
    NCBI_THROW(CSeqMapException, eDataError, "Invalid parent map");
}


const CSeq_data& CSeqMap::x_GetSeq_data(const CSegment& seg) const
{
    if ( seg.m_SegType == eSeqData ) {
        return *static_cast<const CSeq_data*>(x_GetObject(seg));
    }
    NCBI_THROW(CSeqMapException, eSegmentTypeError,
               "Invalid segment type");
}


void CSeqMap::x_SetSeq_data(size_t index, CSeq_data& data)
{
    // check segment type
    CSegment& seg = x_SetSegment(index);
    if ( seg.m_SegType != eSeqData ) {
        NCBI_THROW(CSeqMapException, eSegmentTypeError,
                   "Invalid segment type");
    }
    x_SetObject(seg, data);
}


void CSeqMap::LoadSeq_data(TSeqPos pos, TSeqPos len,
                           const CSeq_data& data)
{
    const CSegment& seg = x_GetSegment(x_FindSegment(pos, 0));
    if ( seg.m_Position != pos || seg.m_Length != len ) {
        NCBI_THROW(CSeqMapException, eDataError,
                   "Invalid segment size");
    }
    if ( seg.m_SegType != eSeqData ) {
        NCBI_THROW(CSeqMapException, eSegmentTypeError,
                   "Invalid segment type");
    }
    x_SetObject(seg, data);
}


const CSeq_id& CSeqMap::x_GetRefSeqid(const CSegment& seg) const
{
    if ( seg.m_SegType == eSeqRef ) {
        return static_cast<const CSeq_id&>(*x_GetObject(seg));
    }
    NCBI_THROW(CSeqMapException, eSegmentTypeError,
               "Invalid segment type");
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
    return TSegment_CI(CConstRef<CSeqMap>(this), scope, 0);
}


CSeqMap::TSegment_CI CSeqMap::End(CScope* scope) const
{
    return TSegment_CI(CConstRef<CSeqMap>(this), scope, GetLength(scope));
}


CSeqMap::TSegment_CI CSeqMap::FindSegment(TSeqPos pos, CScope* scope) const
{
    return TSegment_CI(CConstRef<CSeqMap>(this), scope, pos);
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


CSeqMap::TSegment_CI CSeqMap::BeginResolved(CScope* scope, size_t maxResolveCount,
                                            TFlags flags) const
{
    return begin_resolved(scope, maxResolveCount, flags);
}


CSeqMap::TSegment_CI CSeqMap::EndResolved(CScope* scope, size_t maxResolveCount,
                                          TFlags flags) const
{
    return end_resolved(scope, maxResolveCount, flags);
}


CSeqMap::TSegment_CI CSeqMap::FindResolved(TSeqPos pos, CScope* scope,
                                           size_t maxResolveCount,
                                           TFlags flags) const
{
    return find_resolved(pos, scope, maxResolveCount, flags);
}


CSeqMap::TSegment_CI CSeqMap::FindResolved(TSeqPos pos, CScope* scope,
                                           ENa_strand strand,
                                           size_t maxResolveCount,
                                           TFlags flags) const
{
    return find_resolved(pos, scope, strand, maxResolveCount, flags);
}


CSeqMap::const_iterator CSeqMap::begin_resolved(CScope* scope,
                                                size_t maxResolveCount,
                                                TFlags flags) const
{
    return TSegment_CI(CConstRef<CSeqMap>(this), scope, 0,
                       maxResolveCount, flags);
}


CSeqMap::const_iterator CSeqMap::end_resolved(CScope* scope,
                                              size_t maxResolveCount,
                                              TFlags flags) const
{
    return TSegment_CI(CConstRef<CSeqMap>(this), scope,
                       GetLength(scope),
                       maxResolveCount, flags);
}


CSeqMap::const_iterator CSeqMap::find_resolved(TSeqPos pos, CScope* scope,
                                               size_t maxResolveCount,
                                               TFlags flags) const
{
    return TSegment_CI(CConstRef<CSeqMap>(this), scope, pos,
                       maxResolveCount, flags);
}


CSeqMap::const_iterator CSeqMap::find_resolved(TSeqPos pos, CScope* scope,
                                               ENa_strand strand,
                                               size_t maxResolveCount,
                                               TFlags flags) const
{
    return TSegment_CI(CConstRef<CSeqMap>(this), scope,
                       pos, strand,
                       maxResolveCount, flags);
}


CSeqMap::TSegment_CI
CSeqMap::ResolvedRangeIterator(CScope* scope,
                               ENa_strand strand,
                               TSeqPos from,
                               TSeqPos length,
                               size_t maxResolveCount,
                               TFlags flags) const
{
    if ( IsReverse(strand) ) {
        from = GetLength(scope) - from - length;
    }
    return TSegment_CI(CConstRef<CSeqMap>(this), scope,
                       SSeqMapSelector()
                       .SetRange(from, length)
                       .SetResolveCount(maxResolveCount)
                       .SetFlags(flags),
                       strand);
}


CSeqMap::TSegment_CI
CSeqMap::ResolvedRangeIterator(CScope* scope,
                               TSeqPos from,
                               TSeqPos length,
                               ENa_strand strand,
                               size_t maxResolveCount,
                               TFlags flags) const
{
    return TSegment_CI(CConstRef<CSeqMap>(this), scope,
                       SSeqMapSelector()
                       .SetRange(from, length)
                       .SetResolveCount(maxResolveCount)
                       .SetFlags(flags),
                       strand);
}


bool CSeqMap::CanResolveRange(CScope* scope,
                              TSeqPos from,
                              TSeqPos length,
                              ENa_strand strand) const
{
    try {
        TSegment_CI seg(CConstRef<CSeqMap>(this), scope,
                        SSeqMapSelector()
                        .SetRange(from, length)
                        .SetResolveCount(size_t(-1))
                        .SetFlags(fDefaultFlags),
                        strand);
        for ( ; seg; ++seg);
    }
    catch (exception) {
        return false;
    }
    return true;
}


void CSeqMap::DebugDump(CDebugDumpContext /*ddc*/,
                        unsigned int /*depth*/) const
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


CConstRef<CSeqMap> CSeqMap::CreateSeqMapForBioseq(const CBioseq& seq)
{
    CConstRef<CSeqMap> ret;
    const CSeq_inst& inst = seq.GetInst();
    if ( inst.IsSetSeq_data() ) {
        ret.Reset(new CSeqMap(inst.GetSeq_data(),
                              inst.GetLength()));
    }
    else if ( inst.IsSetExt() ) {
        const CSeq_ext& ext = inst.GetExt();
        switch (ext.Which()) {
        case CSeq_ext::e_Seg:
            ret.Reset(new CSeqMap_Seq_locs(ext.GetSeg(),
                                           ext.GetSeg().Get()));
            break;
        case CSeq_ext::e_Ref:
            ret.Reset(new CSeqMap(ext.GetRef()));
            break;
        case CSeq_ext::e_Delta:
            ret.Reset(new CSeqMap_Delta_seqs(ext.GetDelta()));
            break;
        case CSeq_ext::e_Map:
            //### Not implemented
            NCBI_THROW(CSeqMapException, eUnimplemented,
                       "CSeq_ext::e_Map -- not implemented");
        default:
            //### Not implemented
            NCBI_THROW(CSeqMapException, eUnimplemented,
                       "CSeq_ext::??? -- not implemented");
        }
    }
    else if ( inst.GetRepr() == CSeq_inst::eRepr_virtual ) {
        // Virtual sequence -- no data, no segments
        // The total sequence is gap
        ret.Reset(new CSeqMap(inst.GetLength()));
    }
    else if ( inst.GetRepr() != CSeq_inst::eRepr_not_set && 
              inst.IsSetLength() && inst.GetLength() != 0 ) {
        // split seq-data
        ret.Reset(new CSeqMap(inst.GetLength()));
    }
    else {
        if ( inst.GetRepr() != CSeq_inst::eRepr_not_set ) {
            NCBI_THROW(CSeqMapException, eDataError,
                       "CSeq_inst.repr of sequence without data "
                       "should be not_set");
        }
        if ( inst.IsSetLength() && inst.GetLength() != 0 ) {
            NCBI_THROW(CSeqMapException, eDataError,
                       "CSeq_inst.length of sequence without data "
                       "should be 0");
        }
        ret.Reset(new CSeqMap(TSeqPos(0)));
    }
    const_cast<CSeqMap&>(*ret).m_Mol = inst.GetMol();
    if ( inst.IsSetLength() ) {
        const_cast<CSeqMap&>(*ret).m_SeqLength = inst.GetLength();
    }
    return ret;
}


CConstRef<CSeqMap> CSeqMap::CreateSeqMapForSeq_loc(const CSeq_loc& loc,
                                                   CScope* scope)
{
    CConstRef<CSeqMap> ret(new CSeqMap(loc));
    if ( scope ) {
        CSeqMap::const_iterator i(
            ret->BeginResolved(scope, size_t(-1), fFindData));
        for ( ; i; ++i ) {
            _ASSERT(i.GetType() == eSeqData);
            switch ( i.GetRefData().Which() ) {
            case CSeq_data::e_Ncbi2na:
            case CSeq_data::e_Ncbi4na:
            case CSeq_data::e_Ncbi8na:
            case CSeq_data::e_Ncbipna:
            case CSeq_data::e_Iupacna:
                const_cast<CSeqMap&>(*ret).m_Mol = CSeq_inst::eMol_na;
                break;
            case CSeq_data::e_Ncbi8aa:
            case CSeq_data::e_Ncbieaa:
            case CSeq_data::e_Ncbipaa:
            case CSeq_data::e_Ncbistdaa:
            case CSeq_data::e_Iupacaa:
                const_cast<CSeqMap&>(*ret).m_Mol = CSeq_inst::eMol_aa;
                break;
            default:
                const_cast<CSeqMap&>(*ret).m_Mol = CSeq_inst::eMol_not_set;
            }
        }
    }
    return ret;
}


CConstRef<CSeqMap> CSeqMap::CreateSeqMapForStrand(CConstRef<CSeqMap> seqMap,
                                                  ENa_strand strand)
{
    if ( IsReverse(strand) ) {
        CRef<CSeqMap> ret(new CSeqMap());
        ret->x_AddEnd();
        ret->x_AddSegment(eSeqSubMap,
                          const_cast<CSeqMap*>(seqMap.GetPointer()),
                          0, kInvalidSeqPos, strand);
        ret->x_AddEnd();
        ret->m_Mol = seqMap->m_Mol;
        ret->m_SeqLength = seqMap->m_SeqLength;
        seqMap = ret;
    }
    return seqMap;
}


CSeqMap::CSegment& CSeqMap::x_AddSegment(ESegmentType type, TSeqPos len)
{
    m_Segments.push_back(CSegment(type, len));
    return m_Segments.back();
}


CSeqMap::CSegment& CSeqMap::x_AddSegment(ESegmentType type, TSeqPos len,
                                         const CObject* object)
{
    CSegment& ret = x_AddSegment(type, len);
    ret.m_RefObject.Reset(object);
    return ret;
}


CSeqMap::CSegment& CSeqMap::x_AddSegment(ESegmentType type,
                                         const CObject* object,
                                         TSeqPos refPos,
                                         TSeqPos len,
                                         ENa_strand strand)
{
    CSegment& ret = x_AddSegment(type, len, object);
    ret.m_RefPosition = refPos;
    ret.m_RefMinusStrand = IsReverse(strand);
    return ret;
}


void CSeqMap::x_AddEnd(void)
{
    TSeqPos pos = m_Segments.empty()? 0: kInvalidSeqPos;
    x_AddSegment(eSeqEnd, 0).m_Position = pos;
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


CSeqMap::CSegment& CSeqMap::x_Add(const CSeq_data& data, TSeqPos len)
{
    return x_AddSegment(eSeqData, len, &data);
}


CSeqMap::CSegment& CSeqMap::x_Add(const CSeq_point& ref)
{
    return x_AddSegment(eSeqRef, &ref.GetId(),
                        ref.GetPoint(), 1,
                        ref.IsSetStrand()? ref.GetStrand(): eNa_strand_unknown);
}


CSeqMap::CSegment& CSeqMap::x_Add(const CSeq_interval& ref)
{
    return x_AddSegment(eSeqRef, &ref.GetId(),
                        ref.GetFrom(), ref.GetLength(),
                        ref.IsSetStrand()? ref.GetStrand(): eNa_strand_unknown);
}


CSeqMap::CSegment& CSeqMap::x_Add(const CSeq_id& ref)
{
    return x_AddSegment(eSeqRef, &ref, 0, kInvalidSeqPos);
}


CSeqMap::CSegment& CSeqMap::x_Add(CSeqMap* submap)
{
    return x_AddSegment(eSeqSubMap, kInvalidSeqPos, submap);
}


CSeqMap::CSegment& CSeqMap::x_Add(const CPacked_seqint& seq)
{
    return x_Add(new CSeqMap_Seq_intervals(seq));
}


CSeqMap::CSegment& CSeqMap::x_Add(const CPacked_seqpnt& seq)
{
    return x_Add(new CSeqMap_SeqPoss(seq));
}


CSeqMap::CSegment& CSeqMap::x_Add(const CSeq_loc_mix& seq)
{
    return x_Add(new CSeqMap_Seq_locs(seq, seq.Get()));
}


CSeqMap::CSegment& CSeqMap::x_Add(const CSeq_loc_equiv& seq)
{
    return x_Add(new CSeqMap_Seq_locs(seq, seq.Get()));
}


CSeqMap::CSegment& CSeqMap::x_Add(const CSeq_literal& seq)
{
    if ( seq.IsSetSeq_data() ) {
        return x_Add(seq.GetSeq_data(), seq.GetLength());
    }
    else {
        // No data exist - treat it like a gap
        return x_AddGap(seq.GetLength()); //???
    }
}


CSeqMap::CSegment& CSeqMap::x_Add(const CSeq_loc& loc)
{
    switch ( loc.Which() ) {
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Null:
    case CSeq_loc::e_Empty:
        return x_AddGap(0); // Add gap ???
    case CSeq_loc::e_Whole:
        return x_Add(loc.GetWhole());
    case CSeq_loc::e_Int:
        return x_Add(loc.GetInt());
    case CSeq_loc::e_Pnt:
        return x_Add(loc.GetPnt());
    case CSeq_loc::e_Packed_int:
        return x_Add(loc.GetPacked_int());
    case CSeq_loc::e_Packed_pnt:
        return x_Add(loc.GetPacked_pnt());
    case CSeq_loc::e_Mix:
        return x_Add(loc.GetMix());
    case CSeq_loc::e_Equiv:
        return x_Add(loc.GetEquiv());
    case CSeq_loc::e_Bond:
        NCBI_THROW(CSeqMapException, eDataError,
                   "e_Bond is not allowed as a reference type");
    case CSeq_loc::e_Feat:
        NCBI_THROW(CSeqMapException, eDataError,
                   "e_Feat is not allowed as a reference type");
    default:
        NCBI_THROW(CSeqMapException, eDataError,
                   "invalid reference type");
    }
}


CSeqMap::CSegment& CSeqMap::x_Add(const CDelta_seq& seq)
{
    switch ( seq.Which() ) {
    case CDelta_seq::e_Loc:
        return x_Add(seq.GetLoc());
    case CDelta_seq::e_Literal:
        return x_Add(seq.GetLiteral());
    default:
        NCBI_THROW(CSeqMapException, eDataError,
                   "Can not add empty Delta-seq");
    }
}


void CSeqMap::SetRegionInChunk(CTSE_Chunk_Info& chunk,
                               TSeqPos pos, TSeqPos length)
{
    if ( length == kInvalidSeqPos ) {
        _ASSERT(pos == 0);
        _ASSERT(m_SeqLength != kInvalidSeqPos);
        length = m_SeqLength;
    }
    size_t index = x_FindSegment(pos, 0);
    CFastMutexGuard guard(m_SeqMap_Mtx);
    while ( length ) {
        // get segment
        if ( index > x_GetSegmentsCount() ) {
            NCBI_THROW(CSeqMapException, eDataError,
                       "split chunk beyond SeqMap bounds");
        }
        const CSegment& seg = x_GetSegment(index);

        // check segment
        if ( seg.m_Position != pos || seg.m_Length < length ) {
            NCBI_THROW(CSeqMapException, eDataError,
                       "SeqMap segment crosses split chunk boundary");
        }
        if ( seg.m_SegType != eSeqGap ) {
            NCBI_THROW(CSeqMapException, eDataError,
                       "split chunk covers bad SeqMap segment");
        }
        _ASSERT(!seg.m_RefObject);

        // update segment
        const_cast<CSegment&>(seg).m_SegType = eSeqData;
        x_SetChunk(seg, chunk);

        // next
        pos += seg.m_Length;
        length -= seg.m_Length;
        ++index;
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.55  2004/06/15 14:51:27  grichenk
* Fixed CRef to bool conversion
*
* Revision 1.54  2004/06/15 14:06:49  vasilche
* Added support to load split sequences.
*
* Revision 1.53  2004/05/21 21:42:13  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.52  2004/03/16 15:47:28  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.51  2004/02/19 17:19:10  vasilche
* Removed 'unused argument' warning.
*
* Revision 1.50  2003/11/19 22:18:04  grichenk
* All exceptions are now CException-derived. Catch "exception" rather
* than "runtime_error".
*
* Revision 1.49  2003/11/12 16:53:17  grichenk
* Modified CSeqMap to work with const objects (CBioseq, CSeq_loc etc.)
*
* Revision 1.48  2003/09/30 16:22:04  vasilche
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
* Revision 1.47  2003/09/05 17:29:40  grichenk
* Structurized Object Manager exceptions
*
* Revision 1.46  2003/08/27 14:27:19  vasilche
* Use Reverse(ENa_strand) function.
*
* Revision 1.45  2003/07/17 19:10:28  grichenk
* Added methods for seq-map and seq-vector validation,
* updated demo.
*
* Revision 1.44  2003/07/14 21:13:26  grichenk
* Added possibility to resolve seq-map iterator withing a single TSE
* and to skip intermediate references during this resolving.
*
* Revision 1.43  2003/06/30 18:39:18  vasilche
* Fixed access to uninitialized member.
*
* Revision 1.42  2003/06/27 19:09:02  grichenk
* Fixed problem with unset sequence length.
*
* Revision 1.41  2003/06/26 19:47:27  grichenk
* Added sequence length cache
*
* Revision 1.40  2003/06/24 14:22:46  vasilche
* Fixed CSeqMap constructor from CSeq_loc.
*
* Revision 1.39  2003/06/12 17:04:55  vasilche
* Fixed creation of CSeqMap for sequences with repr == not_set.
*
* Revision 1.38  2003/06/11 19:32:55  grichenk
* Added molecule type caching to CSeqMap, simplified
* coding and sequence type calculations in CSeqVector.
*
* Revision 1.37  2003/06/02 16:06:38  dicuccio
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
* Revision 1.36  2003/05/21 16:03:08  vasilche
* Fixed access to uninitialized optional members.
* Added initialization of mandatory members.
*
* Revision 1.35  2003/05/20 20:36:14  vasilche
* Added FindResolved() with strand argument.
*
* Revision 1.34  2003/05/20 15:44:38  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.33  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.32  2003/02/24 18:57:22  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.31  2003/02/05 17:59:17  dicuccio
* Moved formerly private headers into include/objects/objmgr/impl
*
* Revision 1.30  2003/02/05 15:55:26  vasilche
* Added eSeqEnd segment at the beginning of seq map.
* Added flags to CSeqMap_CI to stop on data, gap, or references.
*
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
