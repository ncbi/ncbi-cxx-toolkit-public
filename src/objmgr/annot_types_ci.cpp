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
* Author: Aleksey Grichenko
*
* File Description:
*   Object manager iterators
*
*/

#include <objects/objmgr/annot_types_ci.hpp>
#include <objects/objmgr/impl/annot_object.hpp>
#include <serial/typeinfo.hpp>
#include <objects/objmgr/impl/tse_info.hpp>
#include <objects/objmgr/impl/handle_range_map.hpp>
#include <objects/objmgr/impl/synonyms.hpp>
#include <objects/objmgr/scope.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/general/Dbtag.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>
#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CAnnotObject_Ref::TRange CAnnotObject_Ref::x_UpdateTotalRange(void) const
{
    TRange range = m_TotalRange;
    if ( range.GetFrom() == TSeqPos(kDirtyCache) &&
         range.GetToOpen() == TSeqPos(kDirtyCache) ) {
        range = m_TotalRange = GetFeatLoc().GetTotalRange();
    }
    return range;
}


bool CFeat_Less::operator ()(const CAnnotObject_Ref& x,
                             const CAnnotObject_Ref& y) const
{
    CAnnotObject_Ref::TRange x_range = x.GetTotalRange();
    CAnnotObject_Ref::TRange y_range = y.GetTotalRange();
    // smallest left extreme first
    if ( x_range.GetFrom() != y_range.GetFrom() ) {
        return x_range.GetFrom() < y_range.GetFrom();
    }

    // longest feature first
    if ( x_range.GetToOpen() != y_range.GetToOpen() ) {
        return x_range.GetToOpen() > y_range.GetToOpen();
    }

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
    int diff = x_feat->CompareNonLocation(*y_feat, *x_loc, *y_loc);
    return diff? diff < 0: x_info < y_info;
}


bool CFeat_Reverse_Less::operator ()(const CAnnotObject_Ref& x,
                                     const CAnnotObject_Ref& y) const
{
    CAnnotObject_Ref::TRange x_range = x.GetTotalRange();
    CAnnotObject_Ref::TRange y_range = y.GetTotalRange();
    // smallest left extreme first
    if ( x_range.GetToOpen() != y_range.GetToOpen() ) {
        return x_range.GetToOpen() > y_range.GetToOpen();
    }

    // longest feature first
    if ( x_range.GetFrom() != y_range.GetFrom() ) {
        return x_range.GetFrom() < y_range.GetFrom();
    }

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
    int diff = x_feat->CompareNonLocation(*y_feat, *x_loc, *y_loc);
    return diff? diff < 0: x_info < y_info;
}


bool CAnnotObject_Less::operator()(const CAnnotObject_Ref& x,
                                   const CAnnotObject_Ref& y) const
{
    const CAnnotObject_Info* x_info = x.m_Object.GetPointerOrNull();
    const CAnnotObject_Info* y_info = y.m_Object.GetPointerOrNull();
    _ASSERT(x_info && y_info);
    if ( x_info->IsFeat()  &&  y_info->IsFeat() ) {
        // CSeq_feat::operator<() may report x == y while the features
        // are different. In this case compare pointers too.
        int diff = x_info->GetFeatFast()->Compare(*y_info->GetFeatFast(),
                                                  x.GetFeatLoc(),
                                                  y.GetFeatLoc());
        if ( diff )
            return diff < 0;
    }
    // Compare pointers
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
      m_Scope(bioseq.m_Scope)
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
      m_Scope(bioseq.m_Scope)
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
}


CAnnotTypes_CI& CAnnotTypes_CI::operator= (const CAnnotTypes_CI& it)
{
    if (this != &it) {
        static_cast<SAnnotSelector&>(*this) = it;
        m_Scope = it.m_Scope;
        m_AnnotSet = it.m_AnnotSet;
        m_CurAnnot = m_AnnotSet.begin()+(it.m_CurAnnot - it.m_AnnotSet.begin());
        // Copy TSE list, set TSE locks
        m_TSESet = it.m_TSESet;
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

    void SetSrcId(const CSeq_id_Handle& src)
        {
            m_Src_id = src;
        }
    void SetConversion(const CSeqMap_CI& seg);

private:
    CSeq_id_Handle m_Src_id;
    TSeqPos        m_Src_from;
    TSeqPos        m_Src_to;
    TSignedSeqPos  m_Shift;
    bool           m_Reverse;
    const CSeq_id& m_Dst_id;
    bool           m_Partial;
    CScope*        m_Scope;
    // temporaries for conversion results
    CRef<CSeq_interval> dst_int;
    CRef<CSeq_point>    dst_pnt;
};


inline
void CSeq_loc_Conversion::Reset(void)
{
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
    if ( obj.IsFeat() ) {
        const CSeq_feat& feat = obj.GetFeat();
        if ( index == 0 ) {
            Convert(feat.GetLocation(), ref.m_MappedLoc, true);
            ref.SetPartial(IsPartial());
        }
        else if ( feat.IsSetProduct() ) {
            Convert(feat.GetProduct(), ref.m_MappedProd, true);
            ref.SetPartial(IsPartial());
        }
    }
}


void CAnnotTypes_CI::x_Initialize(const CHandleRangeMap& master_loc)
{
    if ( !m_LimitObject )
        m_LimitObjectType = eLimit_None;

    x_Search(master_loc, 0);
    if ( m_ResolveMethod != eResolve_None ) {
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

            CHandleRange::TRange idrange = idit->second.GetOverlappingRange();
            const CSeqMap& seqMap = bh.GetSeqMap();
            CSeqMap_CI smit(seqMap.FindResolved(idrange.GetFrom(),
                                                m_Scope,
                                                size_t(-1),
                                                CSeqMap::fFindRef));
            while ( smit && smit.GetPosition() < idrange.GetToOpen() ) {
                _ASSERT(smit.GetType() == CSeqMap::eSeqRef);
                if ( m_ResolveMethod == eResolve_TSE ) {
                    if ( !m_Scope->GetBioseqHandleFromTSE(smit.GetRefSeqid(),
                                                          bh) ) {
                        smit.Next(false);
                        continue;
                    }
                }
                x_SearchMapped(smit, idit);
                ++smit;
            }
        }
    }
    if ( m_AnnotChoice == CSeq_annot::C_Data::e_Ftable ) {
        if ( m_SortOrder == eSortOrder_Normal ) {
            sort(m_AnnotSet.begin(), m_AnnotSet.end(), CFeat_Less());
        }
        else {
            sort(m_AnnotSet.begin(), m_AnnotSet.end(), CFeat_Reverse_Less());
        }
    }
    else {
        sort(m_AnnotSet.begin(), m_AnnotSet.end(), CAnnotObject_Less());
    }
    m_CurAnnot = m_AnnotSet.begin();
}


void CAnnotTypes_CI::x_Search(const CSeq_id_Handle& id,
                              const CHandleRange& hr,
                              CSeq_loc_Conversion* cvt)
{
    if ( cvt )
        cvt->SetSrcId(id);

    CHandleRange::TRange range = hr.GetOverlappingRange();

    const TTSESet* entries;
    switch ( m_LimitObjectType ) {
    case eLimit_TSE:
    {
        if ( m_TSESet.empty() ) {
            const CSeq_entry* tse = 
                static_cast<const CSeq_entry*>(m_LimitObject.GetPointer());
            TTSE_Lock tse_info(m_Scope->GetTSEInfo(tse));
            if ( tse_info ) {
                m_TSESet.insert(tse_info);
            }
        }
        entries = &m_TSESet;
        break;
    }
    case eLimit_Entry:
    case eLimit_Annot:
    default:
        entries = &m_Scope->GetTSESetWithAnnots(id);
        break;
    }
    ITERATE ( TTSESet, tse_it, *entries ) {
        const CTSE_Info& tse_info = **tse_it;
        CTSE_Guard guard(tse_info);

        const CTSE_Info::TRangeMap* rmap = tse_info.x_GetRangeMap(id, *this);
        if ( !rmap ) {
            continue;
        }

        m_TSESet.insert(*tse_it);

        for ( CTSE_Info::TRangeMap::const_iterator aoit = rmap->begin(range);
              aoit; ++aoit ) {
            const SAnnotObject_Index& annot_index = aoit->second;
            const CAnnotObject_Info& annot_info =
                *annot_index.m_AnnotObject;
            if ( m_LimitObjectType == eLimit_Entry ) {
                if ( &annot_info.GetSeq_entry() !=
                     m_LimitObject.GetPointerOrNull() ) {
                    continue;
                }
            }
            else if ( m_LimitObjectType == eLimit_Annot ) {
                if ( &annot_info.GetSeq_annot() !=
                     m_LimitObject.GetPointerOrNull() ) {
                    continue;
                }
            }

            if ( m_OverlapType == eOverlap_Intervals &&
                 !hr.IntersectingWith(annot_index.m_HandleRange->second) ) {
                continue;
            }
                
            m_AnnotSet.push_back(CAnnotObject_Ref(annot_info));
            if ( cvt ) {
                cvt->Convert(m_AnnotSet.back(), m_FeatProduct);
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
        const CSeq_entry* tse = 
            static_cast<const CSeq_entry*>(m_LimitObject.GetPointer());
        m_Scope->UpdateAnnotIndex(loc, m_AnnotChoice, tse);
        break;
    }
    case eLimit_Annot:
        // Do not update datasources: if we have seq-annot,
        // it's already loaded and indexed (?)
    default:
        // eLimit_None
        m_Scope->UpdateAnnotIndex(loc, m_AnnotChoice);
        break;
    }

    ITERATE ( CHandleRangeMap, idit, loc ) {
        if ( idit->second.Empty() ) {
            continue;
        }

        const CSynonymsSet* syns = m_Scope->x_GetSynonyms(idit->first);
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
                                    CHandleRangeMap::const_iterator master_loc)
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
        ITERATE ( CHandleRange, mlit, master_loc->second ) {
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

    CSeq_loc_Conversion cvt(master_loc->first.GetSeqId(),
                            seg, ref_id, m_Scope);
    x_Search(ref_loc, &cvt);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
