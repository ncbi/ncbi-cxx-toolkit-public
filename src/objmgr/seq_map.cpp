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
                            TSeqPos      length,
                            bool         unknown_len)
    : m_Position(kInvalidSeqPos),
      m_Length(length),
      m_UnknownLength(unknown_len),
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

/*
CSeqMap::CSeqMap(const CSeq_data& data, TSeqPos length)
    : m_Resolved(0),
      m_Mol(CSeq_inst::eMol_not_set),
      m_SeqLength(kInvalidSeqPos)
{
    x_AddEnd();
    x_Add(data, length);
    x_AddEnd();
}
*/

CSeqMap::CSeqMap(TSeqPos length)
    : m_Resolved(0),
      m_Mol(CSeq_inst::eMol_not_set),
      m_SeqLength(length)
{
    x_AddEnd();
    x_AddGap(length, false);
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


CBioseq_Handle CSeqMap::x_GetBioseqHandle(const CSegment& seg,
                                          CScope* scope) const
{
    const CSeq_id& seq_id = x_GetRefSeqid(seg);
    if ( !scope ) {
        NCBI_THROW(CSeqMapException, eNullPointer,
                   "Cannot resolve "+
                   seq_id.AsFastaString()+": null scope pointer");
    }
    CBioseq_Handle bh = scope->GetBioseqHandle(seq_id);
    if ( !bh ) {
        NCBI_THROW(CSeqMapException, eFail,
                   "Cannot resolve "+
                   seq_id.AsFastaString()+": unknown");
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
        if (length == kInvalidSeqPos) {
            NCBI_THROW(CSeqMapException, eDataError,
                    "Invalid sequence length");
        }
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
        TSeqPos seg_pos = resolved_pos;
        resolved_pos += x_GetSegmentLength(resolved, scope);
        if (resolved_pos < seg_pos  ||  resolved_pos == kInvalidSeqPos) {
            NCBI_THROW(CSeqMapException, eDataError,
                    "Sequence position overflow");
        }
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
            TSeqPos seg_pos = resolved_pos;
            resolved_pos += x_GetSegmentLength(resolved, scope);
            if (resolved_pos < seg_pos  ||  resolved_pos == kInvalidSeqPos) {
                NCBI_THROW(CSeqMapException, eDataError,
                        "Sequence position overflow");
            }
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
        const CObject* obj = seg.m_RefObject.GetPointer();
        if ( obj && seg.m_ObjType == eSeqChunk ) {
            const CTSE_Chunk_Info* chunk =
                dynamic_cast<const CTSE_Chunk_Info*>(obj);
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
    if ( seg.m_RefObject && seg.m_SegType == seg.m_ObjType ) {
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
         seg.m_RefObject && seg.m_SegType == seg.m_ObjType ) {
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
    size_t index = x_FindSegment(pos, 0);
    const CSegment& seg = x_GetSegment(index);
    if ( seg.m_Position != pos || seg.m_Length != len ) {
        NCBI_THROW(CSeqMapException, eDataError,
                   "Invalid segment size");
    }
    x_SetSeq_data(index, const_cast<CSeq_data&>(data));
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


CSeqMap_CI CSeqMap::Begin(CScope* scope) const
{
    return CSeqMap_CI(CConstRef<CSeqMap>(this), scope, SSeqMapSelector());
}


CSeqMap_CI CSeqMap::End(CScope* scope) const
{
    return CSeqMap_CI(CConstRef<CSeqMap>(this), scope, SSeqMapSelector(),
                      kMax_UInt);
}


CSeqMap_CI CSeqMap::FindSegment(TSeqPos pos, CScope* scope) const
{
    return CSeqMap_CI(CConstRef<CSeqMap>(this), scope, SSeqMapSelector(), pos);
}


CSeqMap::const_iterator CSeqMap::begin(CScope* scope) const
{
    return Begin(scope);
}


CSeqMap::const_iterator CSeqMap::end(CScope* scope) const
{
    return End(scope);
}


CSeqMap_CI CSeqMap::BeginResolved(CScope*                scope,
                                  const SSeqMapSelector& sel) const
{
    return CSeqMap_CI(CConstRef<CSeqMap>(this), scope, sel);
}


CSeqMap_CI CSeqMap::BeginResolved(CScope* scope) const
{
    SSeqMapSelector sel;
    sel.SetResolveCount(kMax_UInt);
    return CSeqMap_CI(CConstRef<CSeqMap>(this), scope, sel);
}


CSeqMap_CI CSeqMap::EndResolved(CScope* scope) const
{
    SSeqMapSelector sel;
    sel.SetResolveCount(kMax_UInt);
    return CSeqMap_CI(CConstRef<CSeqMap>(this), scope, sel, kMax_UInt);
}


CSeqMap_CI CSeqMap::EndResolved(CScope*                scope,
                                const SSeqMapSelector& sel) const
{
    return CSeqMap_CI(CConstRef<CSeqMap>(this), scope, sel, kMax_UInt);
}


CSeqMap_CI CSeqMap::FindResolved(CScope*                scope,
                                 TSeqPos                pos,
                                 const SSeqMapSelector& selector) const
{
    return CSeqMap_CI(CConstRef<CSeqMap>(this), scope, selector, pos);
}


CSeqMap_CI CSeqMap::ResolvedRangeIterator(CScope* scope,
                                          TSeqPos from,
                                          TSeqPos length,
                                          ENa_strand strand,
                                          size_t maxResolveCount,
                                          TFlags flags) const
{
    SSeqMapSelector sel;
    sel.SetFlags(flags).SetResolveCount(maxResolveCount);
    sel.SetRange(from, length).SetStrand(strand);
    return CSeqMap_CI(CConstRef<CSeqMap>(this), scope, sel);
}


bool CSeqMap::CanResolveRange(CScope* scope,
                              TSeqPos from,
                              TSeqPos length,
                              ENa_strand strand,
                              size_t depth,
                              TFlags flags) const
{
    SSeqMapSelector sel;
    sel.SetFlags(flags).SetResolveCount(depth);
    sel.SetRange(from, length).SetStrand(strand);
    return CanResolveRange(scope, sel);
}


bool CSeqMap::CanResolveRange(CScope* scope, const SSeqMapSelector& sel) const
{
    try {
        for(CSeqMap_CI seg(CConstRef<CSeqMap>(this), scope, sel); seg; ++seg) {
            // do nothing, just scan
        }
    }
    catch (exception) {
        return false;
    }
    return true;
}


CConstRef<CSeqMap> CSeqMap::CreateSeqMapForBioseq(const CBioseq& seq)
{
    CConstRef<CSeqMap> ret;
    const CSeq_inst& inst = seq.GetInst();
    if ( inst.IsSetSeq_data() ) {
        ret.Reset(new CSeqMap_Seq_data(inst));
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
        ret.Reset(new CSeqMap_Seq_data(inst));
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
        CSeqMap_CI i(ret, scope, SSeqMapSelector(fFindData, kMax_UInt));
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


CSeqMap::CSegment& CSeqMap::x_AddSegment(ESegmentType type,
                                         TSeqPos len,
                                         bool unknown_len)
{
    m_Segments.push_back(CSegment(type, len, unknown_len));
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


CSeqMap::CSegment& CSeqMap::x_AddGap(TSeqPos len, bool unknown_len)
{
    return x_AddSegment(eSeqGap, len, unknown_len);
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
        return x_AddGap(seq.GetLength(), seq.CanGetFuzz()); //???
    }
}


CSeqMap::CSegment& CSeqMap::x_Add(const CSeq_loc& loc)
{
    switch ( loc.Which() ) {
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Null:
    case CSeq_loc::e_Empty:
        return x_AddGap(0, false); // Add gap ???
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

        // update segment position if not set yet
        if ( index > m_Resolved ) {
            _ASSERT(index == m_Resolved + 1);
            _ASSERT(seg.m_Position == kInvalidSeqPos || seg.m_Position == pos);
            seg.m_Position = pos;
            m_Resolved = index;
        }
        // check segment
        if ( seg.m_Position != pos || seg.m_Length > length ) {
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
