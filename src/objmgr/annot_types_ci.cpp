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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*   Object manager iterators
*
*/

#include <objmgr/annot_types_ci.hpp>
#include <objmgr/impl/annot_object.hpp>
#include <serial/typeinfo.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/impl/synonyms.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_map.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/general/Dbtag.hpp>

#include <serial/objostr.hpp>
#include <serial/objostrasn.hpp>
#include <serial/serial.hpp>

#include <algorithm>
#include <typeinfo>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class NCBI_XOBJMGR_EXPORT CFeat_Less
{
public:
    // Compare CRef-s: both must be features
    bool operator()(const CAnnotObject_Ref& x,
                    const CAnnotObject_Ref& y) const
        {
            {
                // smallest left extreme first
                TSeqPos x_from = x.GetTotalRange().GetFrom();
                TSeqPos y_from = y.GetTotalRange().GetFrom();
                if ( x_from != y_from ) {
                    return x_from < y_from;
                }
            }
            {
                // longest feature first
                TSeqPos x_to = x.GetTotalRange().GetToOpen();
                TSeqPos y_to = y.GetTotalRange().GetToOpen();
                if ( x_to != y_to ) {
                    return x_to > y_to;
                }
            }
            return x_less(x, y);
        }

private:
    bool x_less(const CAnnotObject_Ref& x, const CAnnotObject_Ref& y) const;
};


class NCBI_XOBJMGR_EXPORT CFeat_Reverse_Less
{
public:
    // Compare CRef-s: both must be features
    bool operator()(const CAnnotObject_Ref& x,
                    const CAnnotObject_Ref& y) const
        {
            {
                // largest right extreme first
                TSeqPos x_to = x.GetTotalRange().GetToOpen();
                TSeqPos y_to = y.GetTotalRange().GetToOpen();
                if ( x_to != y_to ) {
                    return x_to > y_to;
                }
            }
            {
                // longest feature first
                TSeqPos x_from = x.GetTotalRange().GetFrom();
                TSeqPos y_from = y.GetTotalRange().GetFrom();
                if ( x_from != y_from ) {
                    return x_from < y_from;
                }
            }
            return x_less(x, y);
        }
private:
    bool x_less(const CAnnotObject_Ref& x, const CAnnotObject_Ref& y) const;
};

class NCBI_XOBJMGR_EXPORT CAnnotObject_Less
{
public:
    // Compare CRef-s: if at least one is NULL, compare as pointers
    bool operator()(const CAnnotObject_Ref& x,
                    const CAnnotObject_Ref& y) const
        {
            {
                // smallest left extreme first
                TSeqPos x_from = x.GetTotalRange().GetFrom();
                TSeqPos y_from = y.GetTotalRange().GetFrom();
                if ( x_from != y_from ) {
                    return x_from < y_from;
                }
            }
            {
                // longest feature first
                TSeqPos x_to = x.GetTotalRange().GetToOpen();
                TSeqPos y_to = y.GetTotalRange().GetToOpen();
                if ( x_to != y_to ) {
                    return x_to > y_to;
                }
            }
            return x_less(x, y);
        }
private:
    bool x_less(const CAnnotObject_Ref& x, const CAnnotObject_Ref& y) const;
};


class NCBI_XOBJMGR_EXPORT CAnnotObject_Reverse_Less
{
public:
    // Compare CRef-s: if at least one is NULL, compare as pointers
    bool operator()(const CAnnotObject_Ref& x,
                    const CAnnotObject_Ref& y) const
        {
            {
                // largest right extreme first
                TSeqPos x_to = x.GetTotalRange().GetToOpen();
                TSeqPos y_to = y.GetTotalRange().GetToOpen();
                if ( x_to != y_to ) {
                    return x_to > y_to;
                }
            }
            {
                // longest feature first
                TSeqPos x_from = x.GetTotalRange().GetFrom();
                TSeqPos y_from = y.GetTotalRange().GetFrom();
                if ( x_from != y_from ) {
                    return x_from < y_from;
                }
            }
            return x_less(x, y);
        }
private:
    bool x_less(const CAnnotObject_Ref& x, const CAnnotObject_Ref& y) const;
};


bool CFeat_Less::x_less(const CAnnotObject_Ref& x,
                        const CAnnotObject_Ref& y) const
{
    const CAnnotObject_Info* x_info = x.m_Object.GetPointerOrNull();
    const CAnnotObject_Info* y_info = y.m_Object.GetPointerOrNull();
    _ASSERT(x_info && y_info);
    const CSeq_feat* x_feat = x_info->GetFeatFast();
    const CSeq_feat* y_feat = y_info->GetFeatFast();
    _ASSERT(x_feat && y_feat);
    const CSeq_loc* x_loc = x.m_MappedLoc.GetPointerOrNull();
    const CSeq_loc* y_loc = y.m_MappedLoc.GetPointerOrNull();
    if ( !x_loc )
        x_loc = &x_feat->GetLocation();
    if ( !y_loc )
        y_loc = &y_feat->GetLocation();
    int diff;
    try {
        diff = x_feat->CompareNonLocation(*y_feat, *x_loc, *y_loc);
    }
    catch ( exception& /*ignored*/ ) {
        // do not fail sort when compare function throws an exception
        diff = 0;
    }
    return diff? diff < 0: x_info < y_info;
}


bool CFeat_Reverse_Less::x_less(const CAnnotObject_Ref& x,
                                const CAnnotObject_Ref& y) const
{

    const CAnnotObject_Info* x_info = x.m_Object.GetPointerOrNull();
    const CAnnotObject_Info* y_info = y.m_Object.GetPointerOrNull();
    _ASSERT(x_info && y_info);
    const CSeq_feat* x_feat = x_info->GetFeatFast();
    const CSeq_feat* y_feat = y_info->GetFeatFast();
    _ASSERT(x_feat && y_feat);
    const CSeq_loc* x_loc = x.m_MappedLoc.GetPointerOrNull();
    const CSeq_loc* y_loc = y.m_MappedLoc.GetPointerOrNull();
    if ( !x_loc )
        x_loc = &x_feat->GetLocation();
    if ( !y_loc )
        y_loc = &y_feat->GetLocation();
    int diff;
    try {
        diff = x_feat->CompareNonLocation(*y_feat, *x_loc, *y_loc);
    }
    catch ( exception& /*ignored*/ ) {
        // do not fail sort when compare function throws an exception
        diff = 0;
    }
    return diff? diff < 0: x_info < y_info;
}


bool CAnnotObject_Less::x_less(const CAnnotObject_Ref& x,
                               const CAnnotObject_Ref& y) const
{
    const CAnnotObject_Info* x_info = x.m_Object.GetPointerOrNull();
    const CAnnotObject_Info* y_info = y.m_Object.GetPointerOrNull();
    _ASSERT(x_info && y_info);
    return x_info < y_info;
}


bool CAnnotObject_Reverse_Less::x_less(const CAnnotObject_Ref& x,
                                       const CAnnotObject_Ref& y) const
{
    const CAnnotObject_Info* x_info = x.m_Object.GetPointerOrNull();
    const CAnnotObject_Info* y_info = y.m_Object.GetPointerOrNull();
    _ASSERT(x_info && y_info);
    return x_info < y_info;
}


CAnnotTypes_CI::CAnnotTypes_CI(void)
{
}


CAnnotTypes_CI::CAnnotTypes_CI(CScope& scope,
                               const CSeq_loc& loc,
                               const SAnnotSelector& params)
    : SAnnotSelector(params),
      m_Scope(&scope)
{
    CHandleRangeMap master_loc;
    master_loc.AddLocation(loc);
    x_Initialize(master_loc);
}


CAnnotTypes_CI::CAnnotTypes_CI(const CBioseq_Handle& bioseq,
                               TSeqPos start, TSeqPos stop,
                               const SAnnotSelector& params)
    : SAnnotSelector(params),
      m_Scope(&bioseq.GetScope())
{
    x_Initialize(bioseq, start, stop);
}


CAnnotTypes_CI::CAnnotTypes_CI(CScope& scope,
                               const CSeq_loc& loc,
                               SAnnotSelector selector,
                               EOverlapType overlap_type,
                               EResolveMethod resolve_method,
                               const CSeq_entry* entry)
    : SAnnotSelector(selector
                     .SetOverlapType(overlap_type)
                     .SetResolveMethod(resolve_method)
                     .SetLimitSeqEntry(entry)),
      m_Scope(&scope)
{
    CHandleRangeMap master_loc;
    master_loc.AddLocation(loc);
    x_Initialize(master_loc);
}


CAnnotTypes_CI::CAnnotTypes_CI(const CBioseq_Handle& bioseq,
                               TSeqPos start, TSeqPos stop,
                               SAnnotSelector selector,
                               EOverlapType overlap_type,
                               EResolveMethod resolve_method,
                               const CSeq_entry* entry)
    : SAnnotSelector(selector
                     .SetOverlapType(overlap_type)
                     .SetResolveMethod(resolve_method)
                     .SetLimitSeqEntry(entry)),
      m_Scope(&bioseq.GetScope())
{
    x_Initialize(bioseq, start, stop);
}


void CAnnotTypes_CI::x_Initialize(const CBioseq_Handle& bioseq,
                                  TSeqPos start, TSeqPos stop)
{
    if ( start == 0 && stop == 0 ) {
        CBioseq_Handle::TBioseqCore core = bioseq.GetBioseqCore();
        if ( !core->GetInst().IsSetLength() ) {
            stop = kInvalidSeqPos-1;
        }
        else {
            stop = core->GetInst().GetLength()-1;
        }
    }
    CHandleRangeMap master_loc;
    master_loc.AddRange(*bioseq.GetSeqId(), start, stop);
    x_Initialize(master_loc);
}


CAnnotTypes_CI::CAnnotTypes_CI(const CAnnotTypes_CI& it)
{
    *this = it;
}


CAnnotTypes_CI::~CAnnotTypes_CI(void)
{
    x_Clear();
}


void CAnnotTypes_CI::x_Clear(void)
{
    m_AnnotSet.clear();
}


CAnnotTypes_CI& CAnnotTypes_CI::operator= (const CAnnotTypes_CI& it)
{
    if ( this != &it ) {
        x_Clear();
        // Copy TSE list, set TSE locks
        m_TSE_LockSet = it.m_TSE_LockSet;
        static_cast<SAnnotSelector&>(*this) = it;
        m_Scope = it.m_Scope;
        m_AnnotSet = it.m_AnnotSet;
        size_t index = it.m_CurAnnot - it.m_AnnotSet.begin();
        m_CurAnnot = m_AnnotSet.begin()+index;
    }
    return *this;
}


class CSeq_loc_Conversion
{
public:
    CSeq_loc_Conversion(const CSeq_id& master_id,
                        const CSeqMap_CI& seg,
                        const CSeq_id_Handle& src_id,
                        CScope* scope);

    TSeqPos ConvertPos(TSeqPos src_pos);

    bool GoodSrcId(const CSeq_id& id);

    bool ConvertPoint(TSeqPos src_pos);
    bool ConvertPoint(const CSeq_point& src);

    bool ConvertInterval(TSeqPos src_from, TSeqPos src_to);
    bool ConvertInterval(const CSeq_interval& src);

    bool Convert(const CSeq_loc& src, CRef<CSeq_loc>& dst,
                 bool always = false);

    void Convert(CAnnotObject_Ref& obj, int index);

    void Reset(void);

    bool IsPartial(void) const
        {
            return m_Partial;
        }

    const CSeq_id& GetDstId(void) const
        {
            return m_Dst_id;
        }
    void SetSrcId(const CSeq_id_Handle& src)
        {
            m_Src_id = src;
        }
    void SetConversion(const CSeqMap_CI& seg);

    typedef CRange<TSeqPos> TRange;
    const TRange& GetTotalRange(void) const
        {
            return m_TotalRange;
        }
    
private:
    CSeq_id_Handle m_Src_id;
    TSeqPos        m_Src_from;
    TSeqPos        m_Src_to;
    TSignedSeqPos  m_Shift;
    const CSeq_id& m_Dst_id;
    TRange         m_TotalRange;
    bool           m_Reverse;
    bool           m_Partial;
    CScope*        m_Scope;
    // temporaries for conversion results
    CRef<CSeq_interval> dst_int;
    CRef<CSeq_point>    dst_pnt;
};


inline
void CSeq_loc_Conversion::Reset(void)
{
    m_TotalRange = TRange::GetEmpty();
    m_Partial = false;
}


CSeq_loc_Conversion::CSeq_loc_Conversion(const CSeq_id& master_id,
                                         const CSeqMap_CI& seg,
                                         const CSeq_id_Handle& src_id,
                                         CScope* scope)
    : m_Src_id(src_id), m_Dst_id(master_id), m_Scope(scope)
{
    SetConversion(seg);
    Reset();
}


void CSeq_loc_Conversion::SetConversion(const CSeqMap_CI& seg)
{
    m_Src_from = seg.GetRefPosition();
    m_Src_to = m_Src_from + seg.GetLength() - 1;
    m_Reverse = seg.GetRefMinusStrand();
    if ( !m_Reverse ) {
        m_Shift = seg.GetPosition() - m_Src_from;
    }
    else {
        m_Shift = seg.GetPosition() + m_Src_to;
    }
}


inline
TSeqPos CSeq_loc_Conversion::ConvertPos(TSeqPos src_pos)
{
    if ( src_pos < m_Src_from || src_pos > m_Src_to ) {
        m_Partial = true;
        return kInvalidSeqPos;
    }
    TSeqPos dst_pos;
    if ( !m_Reverse ) {
        dst_pos = m_Shift + src_pos;
    }
    else {
        dst_pos = m_Shift - src_pos;
    }
    return dst_pos;
}


inline
bool CSeq_loc_Conversion::GoodSrcId(const CSeq_id& id)
{
    bool good = m_Scope->GetIdHandle(id) == m_Src_id;
    if ( !good ) {
        m_Partial = true;
    }
    return good;
}


bool CSeq_loc_Conversion::ConvertPoint(TSeqPos src_pos)
{
    if ( src_pos < m_Src_from || src_pos > m_Src_to ) {
        m_Partial = true;
        return false;
    }
    TSeqPos dst_pos;
    if ( !m_Reverse ) {
        dst_pos = m_Shift + src_pos;
    }
    else {
        dst_pos = m_Shift - src_pos;
    }
    CSeq_point* dst;
    dst_pnt.Reset(dst = new CSeq_point);
    dst->SetId().Assign(m_Dst_id);
    dst->SetPoint(dst_pos);
    m_TotalRange += TRange(dst_pos, dst_pos);
    return true;
}


inline
bool CSeq_loc_Conversion::ConvertPoint(const CSeq_point& src)
{
    return GoodSrcId(src.GetId()) && ConvertPoint(src.GetPoint());
}


bool CSeq_loc_Conversion::ConvertInterval(TSeqPos src_from, TSeqPos src_to)
{
    if ( src_from < m_Src_from ) {
        m_Partial = true;
        src_from = m_Src_from;
    }
    if ( src_to > m_Src_to ) {
        m_Partial = true;
        src_to = m_Src_to;
    }
    if ( src_from > src_to ) {
        return false;
    }
    TSeqPos dst_from, dst_to;
    if ( !m_Reverse ) {
        dst_from = m_Shift + src_from;
        dst_to = m_Shift + src_to;
    }
    else {
        dst_from = m_Shift - src_to;
        dst_to = m_Shift - src_from;
    }
    CSeq_interval* dst;
    dst_int.Reset(dst = new CSeq_interval);
    dst->SetId().Assign(m_Dst_id);
    dst->SetFrom(dst_from);
    dst->SetTo(dst_to);
    m_TotalRange += TRange(dst_from, dst_to);
    return true;
}


inline
bool CSeq_loc_Conversion::ConvertInterval(const CSeq_interval& src)
{
    return GoodSrcId(src.GetId()) &&
        ConvertInterval(src.GetFrom(), src.GetTo());
}


bool CSeq_loc_Conversion::Convert(const CSeq_loc& src, CRef<CSeq_loc>& dst,
                                  bool always)
{
    dst.Reset();
    switch ( src.Which() ) {
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Null:
    case CSeq_loc::e_Empty:
    case CSeq_loc::e_Feat:
        // Nothing to do, although this should never happen --
        // the seq_loc is intersecting with the conv. loc.
        break;
    case CSeq_loc::e_Whole:
    {
        const CSeq_id& src_id = src.GetWhole();
        // Convert to the allowed master seq interval
        if ( GoodSrcId(src_id) ) {
            CBioseq_Handle bh = m_Scope->GetBioseqHandle(src_id);
            if ( !bh ) {
                THROW1_TRACE(runtime_error,
                             "CSeq_loc_Conversion::Convert: "
                             "cannot determine sequence length");
            }
            CBioseq_Handle::TBioseqCore core = bh.GetBioseqCore();
            if ( !core->GetInst().IsSetLength() ) {
                THROW1_TRACE(runtime_error,
                             "CSeq_loc_Conversion::Convert: "
                             "cannot determine sequence length");
            }
            if ( ConvertInterval(0, core->GetInst().GetLength()) ) {
                dst.Reset(new CSeq_loc);
                dst->SetInt(*dst_int);
            }
        }
        break;
    }
    case CSeq_loc::e_Int:
    {
        if ( ConvertInterval(src.GetInt()) ) {
            dst.Reset(new CSeq_loc);
            dst->SetInt(*dst_int);
        }
        break;
    }
    case CSeq_loc::e_Pnt:
    {
        if ( ConvertPoint(src.GetPnt()) ) {
            dst.Reset(new CSeq_loc);
            dst->SetPnt(*dst_pnt);
        }
        break;
    }
    case CSeq_loc::e_Packed_int:
    {
        const CPacked_seqint::Tdata& src_ints = src.GetPacked_int().Get();
        CPacked_seqint::Tdata* dst_ints = 0;
        ITERATE ( CPacked_seqint::Tdata, i, src_ints ) {
            if ( ConvertInterval(**i) ) {
                if ( !dst_ints ) {
                    dst.Reset(new CSeq_loc);
                    dst_ints = &dst->SetPacked_int().Set();
                }
                dst_ints->push_back(dst_int);
            }
        }
        break;
    }
    case CSeq_loc::e_Packed_pnt:
    {
        const CPacked_seqpnt& src_pack_pnts = src.GetPacked_pnt();
        if ( GoodSrcId(src_pack_pnts.GetId()) ) {
            const CPacked_seqpnt::TPoints& src_pnts =
                src_pack_pnts.GetPoints();
            CPacked_seqpnt::TPoints* dst_pnts = 0;
            ITERATE ( CPacked_seqpnt::TPoints, i, src_pnts ) {
                TSeqPos dst_pos = ConvertPos(*i);
                if ( dst_pos != kInvalidSeqPos ) {
                    if ( !dst_pnts ) {
                        dst.Reset(new CSeq_loc);
                        CPacked_seqpnt& pnts = dst->SetPacked_pnt();
                        pnts.SetId().Assign(m_Dst_id);
                        dst_pnts = &pnts.SetPoints();
                    }
                    dst_pnts->push_back(dst_pos);
                    m_TotalRange += TRange(dst_pos, dst_pos);
                }
            }
        }
        break;
    }
    case CSeq_loc::e_Mix:
    {
        const CSeq_loc_mix::Tdata& src_mix = src.GetMix().Get();
        CSeq_loc_mix::Tdata* dst_mix = 0;
        CRef<CSeq_loc> dst_loc;
        ITERATE ( CSeq_loc_mix::Tdata, i, src_mix ) {
            if ( Convert(**i, dst_loc) ) {
                if ( !dst_mix ) {
                    dst.Reset(new CSeq_loc);
                    dst_mix = &dst->SetMix().Set();
                }
                dst_mix->push_back(dst_loc);
            }
        }
        break;
    }
    case CSeq_loc::e_Equiv:
    {
        const CSeq_loc_equiv::Tdata& src_equiv = src.GetEquiv().Get();
        CSeq_loc_equiv::Tdata* dst_equiv = 0;
        CRef<CSeq_loc> dst_loc;
        ITERATE ( CSeq_loc_equiv::Tdata, i, src_equiv ) {
            if ( Convert(**i, dst_loc) ) {
                if ( !dst_equiv ) {
                    dst.Reset(new CSeq_loc);
                    dst_equiv = &dst->SetEquiv().Set();
                }
                dst_equiv->push_back(dst_loc);
            }
        }
        break;
    }
    case CSeq_loc::e_Bond:
    {
        const CSeq_bond& src_bond = src.GetBond();
        CSeq_bond* dst_bond = 0;
        if ( ConvertPoint(src_bond.GetA()) ) {
            dst.Reset(new CSeq_loc);
            dst_bond = &dst->SetBond();
            dst_bond->SetA(*dst_pnt);
        }
        if ( src_bond.IsSetB() ) {
            if ( ConvertPoint(src_bond.GetB()) ) {
                if ( !dst_bond ) {
                    dst.Reset(new CSeq_loc);
                    dst_bond = &dst->SetBond();
                }
                dst_bond->SetB(*dst_pnt);
            }
        }
        break;
    }
    default:
        THROW1_TRACE(runtime_error,
                     "CSeq_loc_Conversion::Convert: "
                     "Unsupported location type");
    }
    if ( always && !dst ) {
        dst.Reset(new CSeq_loc);
        dst->SetEmpty();
    }
    return dst;
}


void CSeq_loc_Conversion::Convert(CAnnotObject_Ref& ref, int index)
{
    Reset();
    const CAnnotObject_Info& obj = ref.Get();
    switch ( obj.Which() ) {
    case CSeq_annot::C_Data::e_Ftable:
    {
        const CSeq_feat& feat = *obj.GetFeatFast();
        if ( index == 0 ) {
            Convert(feat.GetLocation(), ref.m_MappedLoc, true);
            ref.SetPartial(IsPartial());
        }
        else if ( feat.IsSetProduct() ) {
            Convert(feat.GetProduct(), ref.m_MappedProd, true);
            ref.SetPartial(IsPartial());
        }
        break;
    }
    case CSeq_annot::C_Data::e_Graph:
    {
        const CSeq_graph& graph = *obj.GetGraphFast();
        Convert(graph.GetLoc(), ref.m_MappedLoc, true);
        ref.SetPartial(IsPartial());
        break;
    }
    case CSeq_annot::C_Data::e_Align:
    {
        // TODO: map align
        break;
    }
    }
    ref.m_TotalRange = m_TotalRange;
}

void CAnnotTypes_CI::x_Initialize(const CHandleRangeMap& master_loc)
{
    try {
        if ( !m_LimitObject )
            m_LimitObjectType = eLimit_None;

        x_Search(master_loc, 0);
        if ( m_ResolveMethod != eResolve_None && m_ResolveDepth > 0 ) {
            ITERATE ( CHandleRangeMap::TLocMap, idit, master_loc.GetMap() ) {
                //### Check for eLoadedOnly
                CBioseq_Handle bh = m_Scope->GetBioseqHandle(idit->first);
                if ( !bh ) {
                    if (m_IdResolving == eFailUnresolved) {
                        // resolve by Seq-id only
                        THROW1_TRACE(runtime_error,
                                     "CAnnotTypes_CI::x_Initialize -- "
                                     "Cannot resolve master id");
                    }
                    continue; // Do not crash - just skip unresolvable IDs
                }

                CHandleRange::TRange idrange =
                    idit->second.GetOverlappingRange();
                const CSeqMap& seqMap = bh.GetSeqMap();
                CSeqMap_CI smit(seqMap.FindResolved(idrange.GetFrom(),
                                                    m_Scope,
                                                    m_ResolveDepth-1,
                                                    CSeqMap::fFindRef));
                while ( smit && smit.GetPosition() < idrange.GetToOpen() ) {
                    _ASSERT(smit.GetType() == CSeqMap::eSeqRef);
                    if ( m_ResolveMethod == eResolve_TSE &&
                         !m_Scope->GetBioseqHandleFromTSE(smit.GetRefSeqid(),
                                                          bh) ) {
                        smit.Next(false);
                        continue;
                    }
                    x_SearchMapped(smit, idit->first, idit->second);
                    ++smit;
                }
            }
        }
        switch ( m_SortOrder ) {
        case eSortOrder_Normal:
            if ( m_AnnotChoice == CSeq_annot::C_Data::e_Ftable ) {
                sort(m_AnnotSet.begin(), m_AnnotSet.end(),
                     CFeat_Less());
            }
            else {
                sort(m_AnnotSet.begin(), m_AnnotSet.end(),
                     CAnnotObject_Less());
            }
            break;
        case eSortOrder_Reverse:
            if ( m_AnnotChoice == CSeq_annot::C_Data::e_Ftable ) {
                sort(m_AnnotSet.begin(), m_AnnotSet.end(),
                     CFeat_Reverse_Less());
            }
            else {
                sort(m_AnnotSet.begin(), m_AnnotSet.end(),
                     CAnnotObject_Reverse_Less());
            }
            break;
        }
        Rewind();
    }
    catch (...) {
        x_Clear();
        throw;
    }
}


inline
bool
CAnnotTypes_CI::x_MatchLimitObject(const CAnnotObject_Info& annot_info) const
{
    if ( m_LimitObjectType == eLimit_Entry ) {
        return &annot_info.GetSeq_entry() == m_LimitObject.GetPointerOrNull();
    }
    else if ( m_LimitObjectType == eLimit_Annot ) {
        return &annot_info.GetSeq_annot() == m_LimitObject.GetPointerOrNull();
    }
    return true;
}


inline
bool CAnnotTypes_CI::x_MatchType(const CAnnotObject_Info& annot_info) const
{
    if ( m_AnnotChoice != CSeq_annot::C_Data::e_not_set ) {
        if ( m_AnnotChoice != annot_info.GetAnnotType() ) {
            return false;
        }
        if ( m_AnnotChoice == CSeq_annot::C_Data::e_Ftable ) {
            if ( m_FeatSubtype != CSeqFeatData::eSubtype_any ) {
                if ( m_FeatSubtype != annot_info.GetFeatSubtype() ) {
                    return false;
                }
            }
            else if ( m_FeatChoice != CSeqFeatData::e_not_set ) {
                if ( m_FeatChoice != annot_info.GetFeatType() ) {
                    return false;
                }
            }
        }
    }
    return true;
}


inline
bool CAnnotTypes_CI::x_MatchRange(const CHandleRange& hr,
                                  const CRange<TSeqPos>& range,
                                  const SAnnotObject_Index& index) const
{
    if ( m_OverlapType == eOverlap_Intervals ) {
        if ( index.m_HandleRange ) {
            if ( !hr.IntersectingWith(*index.m_HandleRange) ) {
                return false;
            }
        }
        else {
            if ( !hr.IntersectingWith(range) ) {
                return false;
            }
        }
    }
    if ( m_FeatProduct != index.m_AnnotLocationIndex ) {
        return false;
    }
    return true;
}


void CAnnotTypes_CI::x_Search(const CSeq_id_Handle& id,
                              const CHandleRange& hr,
                              CSeq_loc_Conversion* cvt)
{
    if ( cvt )
        cvt->SetSrcId(id);

    CHandleRange::TRange range = hr.GetOverlappingRange();

    CConstRef<CScope::TAnnotRefSet> annot_ref_set;
    const TTSE_LockSet* entries;
    switch ( m_LimitObjectType ) {
    case eLimit_TSE:
    {
        if ( m_TSE_LockSet.empty() ) {
            const CSeq_entry* tse = 
                static_cast<const CSeq_entry*>(m_LimitObject.GetPointer());
            TTSE_Lock tse_info(m_Scope->GetTSEInfo(tse));
            if ( tse_info ) {
                m_TSE_LockSet.insert(tse_info);
            }
        }
        entries = &m_TSE_LockSet;
        break;
    }
    case eLimit_Entry:
    case eLimit_Annot:
    default:
        annot_ref_set = m_Scope->GetTSESetWithAnnots(id);
        entries = &annot_ref_set->GetData();
        break;
    }

    ITERATE ( TTSE_LockSet, tse_it, *entries ) {
        const CTSE_Info& tse_info = **tse_it;
        //CTSE_Guard guard(tse_info);

        CTSE_Info::TAnnotObjsLock::TReadLockGuard
            guard(tse_info.m_AnnotObjsLock);
        
        pair<size_t, size_t> idxs =
            CTSE_Info::x_GetIndexRange(GetAnnotChoice(), GetFeatChoice());
        for ( size_t index = idxs.first; index <= idxs.second; ++index ) {
            const CTSE_Info::TRangeMap* rmap =
                tse_info.x_GetRangeMap(id, index);
            if ( !rmap ) {
                continue;
            }
            
            m_TSE_LockSet.insert(*tse_it);
            for ( CTSE_Info::TRangeMap::const_iterator aoit =
                      rmap->begin(range); aoit; ++aoit ) {
                const CAnnotObject_Info& annot_info =
                    *aoit->second.m_AnnotObject_Info;
                if ( !x_MatchLimitObject(annot_info) ) {
                    continue;
                }

                if ( !x_MatchType(annot_info) ) {
                    continue;
                }

                if ( !x_MatchRange(hr, aoit->first, aoit->second) ) {
                    continue;
                }
                
                CAnnotObject_Ref annot_ref(annot_info);
                if ( cvt ) {
                    cvt->Convert(annot_ref, m_FeatProduct);
                    annot_ref.m_TotalRange = cvt->GetTotalRange();
                }
                else {
                    annot_ref.m_TotalRange = aoit->first;
                }
                m_AnnotSet.push_back(annot_ref);

#if 0 && defined _DEBUG
                {{ // some debug checks
                    const CAnnotObject_Ref& obj = m_AnnotSet.back();
                    const CSeq_loc* loc;
                    if ( !m_FeatProduct ) {
                        if ( obj.IsMappedLoc() ) {
                            loc = &obj.GetMappedLoc();
                        }
                        else {
                            switch(obj.Get().Which()) {
                            case CSeq_annot::C_Data::e_Ftable:
                                loc = &obj.GetFeatFast().GetLocation();
                                break;
                            case CSeq_annot::C_Data::e_Graph:
                                loc = &obj.Get().GetGraphFast()->GetLoc();
                                break;
                            case CSeq_annot::C_Data::e_Align:
                                // TODO: map align
                                loc = 0;
                                break;
                            }
                        }
                    }
                    else {
                        if ( obj.IsMappedProd() ) {
                            loc = &obj.GetMappedProd();
                        }
                        else {
                            _ASSERT(obj.Get().IsFeat());
                            const CSeq_feat& feat = obj.GetFeatFast();
                            _ASSERT(feat.IsSetProduct());
                            loc = &feat.GetProduct();
                        }
                    }
                    if ( loc ) {
                        CHandleRange::TRange lrange;
                        CHandleRangeMap hrm;
                        CSeq_id_Handle dst_id;
                        if ( cvt )
                            dst_id = CSeq_id_Mapper::GetSeq_id_Mapper().GetHandle(cvt->GetDstId());
                        else
                            dst_id = id;
                        hrm.AddLocation(*loc);
                        ITERATE ( CHandleRangeMap, hrmit, hrm ) {
                            if ( hrmit->first == dst_id ) {
                                lrange = hrmit->second.GetOverlappingRange();
                                break;
                            }
                        }
                        if ( obj.GetTotalRange() != lrange ) {
                            LOG_POST("Mapping feature:");
                            {{
                                CObjectOStreamAsn out(NcbiCout);
                                switch(obj.Get().Which()) {
                                case CSeq_annot::C_Data::e_Ftable:
                                    out << obj.Get().GetFeat();
                                    break;
                                case CSeq_annot::C_Data::e_Graph:
                                    out << obj.Get().GetGraph();
                                    break;
                                case CSeq_annot::C_Data::e_Align:
                                    out << obj.Get().GetAlign();
                                    break;
                                }
                            }}
                            LOG_POST("Mapped loc:");
                            {{
                                CObjectOStreamAsn out(NcbiCout);
                                out << *loc;
                            }}
                            LOG_POST("mappedLoc: "<<lrange.GetFrom()<<"-"<<lrange.GetTo());
                            LOG_POST("  sortLoc: "<<obj.GetTotalRange().GetFrom()<<"-"<<obj.GetTotalRange().GetTo());
                        }
                    }
                }}
#endif // _DEBUG
                // Limit number of annotations to m_MaxSize
                if ( m_MaxSize  &&  m_AnnotSet.size() >= size_t(m_MaxSize) )
                    return;
            }
        }
    }
}


void CAnnotTypes_CI::x_Search(const CHandleRangeMap& loc,
                              CSeq_loc_Conversion* cvt)
{
    switch ( m_LimitObjectType ) {
    case eLimit_TSE:
    case eLimit_Entry:
    {
        // Limit update to one seq-entry
        // Eugene: I had to split expression to avoid segfault on ICC-Linux
        const CObject* obj = &*m_LimitObject;
        const CSeq_entry& entry = dynamic_cast<const CSeq_entry&>(*obj);
        m_Scope->UpdateAnnotIndex(loc, *this, entry);
        break;
    }
    case eLimit_Annot:
        // Do not update datasources: if we have seq-annot,
        // it's already loaded and indexed (?)
    {
        // Limit update to one seq-entry
        // Eugene: I had to split expression to avoid segfault on ICC-Linux
        const CObject* obj = &*m_LimitObject;
        const CSeq_annot& annot = dynamic_cast<const CSeq_annot&>(*obj);
        m_Scope->UpdateAnnotIndex(loc, *this, annot);
        break;
    }
    default:
        // eLimit_None
        m_Scope->UpdateAnnotIndex(loc, *this);
        break;
    }

    ITERATE ( CHandleRangeMap, idit, loc ) {
        if ( idit->second.Empty() ) {
            continue;
        }

        CConstRef<CSynonymsSet> syns = m_Scope->GetSynonyms(idit->first);
        if ( !syns  &&  m_IdResolving == eFailUnresolved ) {
            THROW1_TRACE(runtime_error,
                         "CAnnotTypes_CI::x_Search -- "
                         "Cannot find id synonyms");
        }
        if ( !syns  ||  syns->find(idit->first) == syns->end() ) {
            x_Search(idit->first, idit->second, cvt);
        }
        if ( syns ) {
            ITERATE ( CSynonymsSet, synit, *syns ) {
                x_Search(*synit, idit->second, cvt);
            }
        }
    }
}


void CAnnotTypes_CI::x_SearchMapped(const CSeqMap_CI& seg,
                                    const CSeq_id_Handle& master_id,
                                    const CHandleRange& master_hr)
{
    CHandleRange::TOpenRange master_seg_range(seg.GetPosition(),
                                              seg.GetEndPosition());
    CHandleRange::TOpenRange ref_seg_range(seg.GetRefPosition(),
                                           seg.GetRefEndPosition());
    bool reversed = seg.GetRefMinusStrand();
    TSignedSeqPos shift;
    if ( !reversed ) {
        shift = ref_seg_range.GetFrom() - master_seg_range.GetFrom();
    }
    else {
        shift = ref_seg_range.GetTo() + master_seg_range.GetFrom();
    }
    CSeq_id_Handle ref_id = seg.GetRefSeqid();
    CHandleRangeMap ref_loc;
    {{ // translate master_loc to ref_loc
        CHandleRange& hr = ref_loc.AddRanges(ref_id);
        ITERATE ( CHandleRange, mlit, master_hr ) {
            CHandleRange::TOpenRange range =
                master_seg_range.IntersectionWith(mlit->first);
            if ( !range.Empty() ) {
                ENa_strand strand = mlit->second;
                if ( !reversed ) {
                    range.SetOpen(range.GetFrom() + shift,
                                  range.GetToOpen() + shift);
                }
                else {
                    if ( strand == eNa_strand_minus )
                        strand = eNa_strand_plus;
                    else if ( strand == eNa_strand_plus )
                        strand = eNa_strand_minus;
                    range.Set(shift - range.GetTo(), shift - range.GetFrom());
                }
                hr.AddRange(range, strand);
            }
        }
        if ( hr.Empty() )
            return;
    }}

    CSeq_loc_Conversion cvt(master_id.GetSeqId(),
                            seg, ref_id, m_Scope);
    x_Search(ref_loc, &cvt);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.78  2003/07/29 15:55:16  vasilche
* Catch exceptions when sorting features.
*
* Revision 1.77  2003/07/18 19:32:30  vasilche
* Workaround for GCC 3.0.4 bug.
*
* Revision 1.76  2003/07/17 20:07:55  vasilche
* Reduced memory usage by feature indexes.
* SNP data is loaded separately through PUBSEQ_OS.
* String compression for SNP data.
*
* Revision 1.75  2003/07/08 15:09:22  vasilche
* Annotations iterator erroneously was resolving one level of segments
* deeper than requested.
*
* Revision 1.74  2003/07/01 18:00:13  vasilche
* Fixed unsigned/signed comparison.
*
* Revision 1.73  2003/06/25 20:56:30  grichenk
* Added max number of annotations to annot-selector, updated demo.
*
* Revision 1.72  2003/06/24 14:25:18  vasilche
* Removed obsolete CTSE_Guard class.
* Used separate mutexes for bioseq and annot maps.
*
* Revision 1.71  2003/06/19 18:23:45  vasilche
* Added several CXxx_ScopeInfo classes for CScope related information.
* CBioseq_Handle now uses reference to CBioseq_ScopeInfo.
* Some fine tuning of locking in CScope.
*
* Revision 1.70  2003/06/17 20:34:04  grichenk
* Added flag to ignore sorting
*
* Revision 1.69  2003/06/13 17:22:54  grichenk
* Protected against multi-ID seq-locs
*
* Revision 1.68  2003/06/02 16:06:37  dicuccio
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
* Revision 1.67  2003/05/12 19:18:29  vasilche
* Fixed locking of object manager classes in multi-threaded application.
*
* Revision 1.66  2003/04/29 19:51:13  vasilche
* Fixed interaction of Data Loader garbage collector and TSE locking mechanism.
* Made some typedefs more consistent.
*
* Revision 1.65  2003/04/28 15:00:46  vasilche
* Workaround for ICC bug with dynamic_cast<>.
*
* Revision 1.64  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.63  2003/03/31 21:48:29  grichenk
* Added possibility to select feature subtype through SAnnotSelector.
*
* Revision 1.62  2003/03/27 19:40:11  vasilche
* Implemented sorting in CGraph_CI.
* Added Rewind() method to feature/graph/align iterators.
*
* Revision 1.61  2003/03/26 21:00:19  grichenk
* Added seq-id -> tse with annotation cache to CScope
*
* Revision 1.60  2003/03/26 17:11:19  vasilche
* Added reverse feature traversal.
*
* Revision 1.59  2003/03/21 19:22:51  grichenk
* Redesigned TSE locking, replaced CTSE_Lock with CRef<CTSE_Info>.
*
* Revision 1.58  2003/03/20 20:36:06  vasilche
* Fixed mapping of mix Seq-loc.
*
* Revision 1.57  2003/03/18 14:52:59  grichenk
* Removed obsolete methods, replaced seq-id with seq-id handle
* where possible. Added argument to limit annotations update to
* a single seq-entry.
*
* Revision 1.56  2003/03/14 19:10:41  grichenk
* + SAnnotSelector::EIdResolving; fixed operator=() for several classes
*
* Revision 1.55  2003/03/13 21:49:58  vasilche
* Fixed mapping of Mix location.
*
* Revision 1.54  2003/03/11 20:42:53  grichenk
* Skip unresolvable IDs and synonym
*
* Revision 1.53  2003/03/11 15:51:06  kuznets
* iterate -> ITERATE
*
* Revision 1.52  2003/03/10 16:55:17  vasilche
* Cleaned SAnnotSelector structure.
* Added shortcut when features are limited to one TSE.
*
* Revision 1.51  2003/03/05 20:56:43  vasilche
* SAnnotSelector now holds all parameters of annotation iterators.
*
* Revision 1.50  2003/03/03 20:32:24  vasilche
* Use cached synonyms.
*
* Revision 1.49  2003/02/28 19:27:19  vasilche
* Cleaned Seq_loc conversion class.
*
* Revision 1.48  2003/02/27 20:56:51  vasilche
* Use one method for lookup on main sequence and segments.
*
* Revision 1.47  2003/02/27 16:29:27  vasilche
* Fixed lost features from first segment.
*
* Revision 1.46  2003/02/27 14:35:32  vasilche
* Splitted PopulateTSESet() by logically independent parts.
*
* Revision 1.45  2003/02/26 17:54:14  vasilche
* Added cached total range of mapped location.
*
* Revision 1.44  2003/02/25 20:10:40  grichenk
* Reverted to single total-range index for annotations
*
* Revision 1.43  2003/02/24 21:35:22  vasilche
* Reduce checks in CAnnotObject_Ref comparison.
* Fixed compilation errors on MS Windows.
* Removed obsolete file src/objects/objmgr/annot_object.hpp.
*
* Revision 1.42  2003/02/24 18:57:22  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.41  2003/02/13 14:34:34  grichenk
* Renamed CAnnotObject -> CAnnotObject_Info
* + CSeq_annot_Info and CAnnotObject_Ref
* Moved some members of CAnnotObject to CSeq_annot_Info
* and CAnnotObject_Ref.
* Added feat/align/graph to CAnnotObject_Info map
* to CDataSource.
*
* Revision 1.40  2003/02/12 19:17:31  vasilche
* Fixed GetInt() when CSeq_loc is Whole.
*
* Revision 1.39  2003/02/10 15:53:24  grichenk
* Sort features by mapped location
*
* Revision 1.38  2003/02/06 22:31:02  vasilche
* Use CSeq_feat::Compare().
*
* Revision 1.37  2003/02/05 17:59:16  dicuccio
* Moved formerly private headers into include/objects/objmgr/impl
*
* Revision 1.36  2003/02/04 21:44:11  grichenk
* Convert seq-loc instead of seq-annot to the master coordinates
*
* Revision 1.35  2003/01/29 22:03:46  grichenk
* Use single static CSeq_id_Mapper instead of per-OM model.
*
* Revision 1.34  2003/01/29 17:45:02  vasilche
* Annotaions index is split by annotation/feature type.
*
* Revision 1.33  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.32  2002/12/26 20:55:17  dicuccio
* Moved seq_id_mapper.hpp, tse_info.hpp, and bioseq_info.hpp -> include/ tree
*
* Revision 1.31  2002/12/26 16:39:23  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.30  2002/12/24 15:42:45  grichenk
* CBioseqHandle argument to annotation iterators made const
*
* Revision 1.29  2002/12/19 20:15:28  grichenk
* Fixed code formatting
*
* Revision 1.28  2002/12/06 15:36:00  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.27  2002/12/05 19:28:32  grichenk
* Prohibited postfix operator ++()
*
* Revision 1.26  2002/11/04 21:29:11  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.25  2002/10/08 18:57:30  grichenk
* Added feature sorting to the iterator class.
*
* Revision 1.24  2002/07/08 20:51:01  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.23  2002/05/31 17:53:00  grichenk
* Optimized for better performance (CTSE_Info uses atomic counter,
* delayed annotations indexing, no location convertions in
* CAnnot_Types_CI if no references resolution is required etc.)
*
* Revision 1.22  2002/05/24 14:58:55  grichenk
* Fixed Empty() for unsigned intervals
* SerialAssign<>() -> CSerialObject::Assign()
* Improved performance for eResolve_None case
*
* Revision 1.21  2002/05/09 14:17:22  grichenk
* Added unresolved references checking
*
* Revision 1.20  2002/05/06 03:28:46  vakatov
* OM/OM1 renaming
*
* Revision 1.19  2002/05/03 21:28:08  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.18  2002/05/02 20:43:15  grichenk
* Improved strand processing, throw -> THROW1_TRACE
*
* Revision 1.17  2002/04/30 14:30:44  grichenk
* Added eResolve_TSE flag in CAnnot_Types_CI, made it default
*
* Revision 1.16  2002/04/23 15:18:33  grichenk
* Fixed: missing features on segments and packed-int convertions
*
* Revision 1.15  2002/04/22 20:06:17  grichenk
* Minor changes in private interface
*
* Revision 1.14  2002/04/17 21:11:59  grichenk
* Fixed annotations loading
* Set "partial" flag in features if necessary
* Implemented most seq-loc types in reference resolving methods
* Fixed searching for annotations within a signle TSE
*
* Revision 1.13  2002/04/12 19:32:20  grichenk
* Removed temp. patch for SerialAssign<>()
*
* Revision 1.12  2002/04/11 12:07:29  grichenk
* Redesigned CAnnotTypes_CI to resolve segmented sequences correctly.
*
* Revision 1.11  2002/04/05 21:26:19  grichenk
* Enabled iteration over annotations defined on segments of a
* delta-sequence.
*
* Revision 1.10  2002/03/07 21:25:33  grichenk
* +GetSeq_annot() in annotation iterators
*
* Revision 1.9  2002/03/05 16:08:14  grichenk
* Moved TSE-restriction to new constructors
*
* Revision 1.8  2002/03/04 15:07:48  grichenk
* Added "bioseq" argument to CAnnotTypes_CI constructor to iterate
* annotations from a single TSE.
*
* Revision 1.7  2002/02/21 19:27:05  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.6  2002/02/15 20:35:38  gouriano
* changed implementation of HandleRangeMap
*
* Revision 1.5  2002/02/07 21:27:35  grichenk
* Redesigned CDataSource indexing: seq-id handle -> TSE -> seq/annot
*
* Revision 1.4  2002/01/23 21:59:31  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.3  2002/01/18 15:51:18  gouriano
* *** empty log message ***
*
* Revision 1.2  2002/01/16 16:25:57  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:17  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
