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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class NCBI_XOBJMGR_EXPORT CFeat_Less
{
public:
    // Compare CRef-s: if at least one is NULL, compare as pointers,
    // otherwise call non-inline x_CompareAnnot() method
    bool operator ()(const CAnnotObject_Ref& x,
                     const CAnnotObject_Ref& y) const;
};


bool CFeat_Less::operator ()(const CAnnotObject_Ref& x,
                             const CAnnotObject_Ref& y) const
{
    const CAnnotObject_Info& x_info = x.Get();
    const CAnnotObject_Info& y_info = y.Get();
    const CSeq_feat& x_feat = x_info.GetFeat();
    const CSeq_feat& y_feat = y_info.GetFeat();
    const CSeq_loc& x_loc =
        x.IsMappedLoc()? x.GetMappedLoc(): x_feat.GetLocation();
    const CSeq_loc& y_loc =
        y.IsMappedLoc()? y.GetMappedLoc(): y_feat.GetLocation();
    int diff = x_feat.Compare(y_feat, x_loc, y_loc);
    if ( diff != 0 )
        return diff < 0;
    return &x_info < &y_info;
}


bool CAnnotObject_Less::x_CompareAnnot(const CAnnotObject_Ref& x,
                                       const CAnnotObject_Ref& y) const
{
    const CAnnotObject_Info& x_info = x.Get();
    const CAnnotObject_Info& y_info = y.Get();
    if ( x_info.IsFeat()  &&  y_info.IsFeat() ) {
        // CSeq_feat::operator<() may report x == y while the features
        // are different. In this case compare pointers too.
        int diff = x_info.GetFeat().Compare(y_info.GetFeat(),
                                            x.GetFeatLoc(), y.GetFeatLoc());
        if ( diff != 0 )
            return diff < 0;
    }
    // Compare pointers
    return &x_info < &y_info;
}


CAnnotObject_Ref::CAnnotObject_Ref(const CAnnotObject_Info& object)
    : m_Object(&object),
      m_MappedLoc(0),
      m_MappedProd(0),
      m_Partial(false)
{
}


CAnnotObject_Ref::~CAnnotObject_Ref(void)
{
    return;
}


CAnnotTypes_CI::CAnnotTypes_CI(void)
    : m_ResolveMethod(eResolve_None)
{
    return;
}


CAnnotTypes_CI::CAnnotTypes_CI(CScope& scope,
                               const CSeq_loc& loc,
                               SAnnotSelector selector,
                               CAnnot_CI::EOverlapType overlap_type,
                               EResolveMethod resolve,
                               const CSeq_entry* entry)
    : m_Selector(selector),
      m_Scope(&scope),
      m_SingleEntry(entry),
      m_ResolveMethod(resolve),
      m_OverlapType(overlap_type)
{
    CHandleRangeMap master_loc;
    master_loc.AddLocation(loc);
    x_Initialize2(master_loc);
}


CAnnotTypes_CI::CAnnotTypes_CI(const CBioseq_Handle& bioseq,
                               TSeqPos start, TSeqPos stop,
                               SAnnotSelector selector,
                               CAnnot_CI::EOverlapType overlap_type,
                               EResolveMethod resolve,
                               const CSeq_entry* entry)
    : m_Selector(selector),
      m_Scope(bioseq.m_Scope),
      m_NativeTSE(static_cast<CTSE_Info*>(bioseq.m_TSE)),
      m_SingleEntry(entry),
      m_ResolveMethod(resolve),
      m_OverlapType(overlap_type)
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
    x_Initialize2(master_loc);
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
    m_Selector = it.m_Selector;
    m_Scope = it.m_Scope;
    m_ResolveMethod = it.m_ResolveMethod;
    m_AnnotSet2 = it.m_AnnotSet2;
    m_CurAnnot = m_AnnotSet2.begin()+(it.m_CurAnnot - it.m_AnnotSet2.begin());
    // Copy TSE list, set TSE locks
    m_TSESet = it.m_TSESet;
    return *this;
}


class CSeq_loc_Conversion
{
public:
    CSeq_loc_Conversion(const CSeq_id& master_id,
                        const CSeqMap_CI& seg,
                        const CSeq_id_Handle& src_id,
                        CScope* scope);

    void Convert(CAnnotObject_Ref& obj, int index);

    CRef<CSeq_loc> Convert(const CSeq_loc& src);
    CRef<CSeq_interval> ConvertInterval(TSeqPos src_from, TSeqPos src_to);
    CRef<CSeq_point> ConvertPoint(TSeqPos src_pos);
    TSeqPos ConvertPos(TSeqPos src_pos);

    void Reset(void);

    bool IsMapped(void) const
        {
            return m_Mapped;
        }

    bool IsPartial(void) const
        {
            return m_Partial;
        }

private:
    CSeq_id_Handle m_Src_id;
    TSignedSeqPos  m_Shift;
    bool           m_Reverse;
    const CSeq_id& m_Master_id;
    TSeqPos        m_Master_from;
    TSeqPos        m_Master_to;
    bool           m_Mapped;
    bool           m_Partial;
    CScope*        m_Scope;
};


CSeq_loc_Conversion::CSeq_loc_Conversion(const CSeq_id& master_id,
                                         const CSeqMap_CI& seg,
                                         const CSeq_id_Handle& src_id,
                                         CScope* scope)
    : m_Src_id(src_id), m_Master_id(master_id), m_Scope(scope)
{
    m_Master_from = seg.GetPosition();
    m_Master_to = m_Master_from + seg.GetLength() - 1;
    m_Reverse = seg.GetRefMinusStrand();
    if ( !m_Reverse ) {
        m_Shift = m_Master_from - seg.GetRefPosition();
    }
    else {
        m_Shift = m_Master_to + seg.GetRefPosition();
    }
    Reset();
}


void CSeq_loc_Conversion::Reset(void)
{
    m_Partial = false;
    m_Mapped = m_Shift != 0 || m_Reverse;
}


TSeqPos CSeq_loc_Conversion::ConvertPos(TSeqPos src_pos)
{
    TSeqPos dst_pos;
    if ( !m_Reverse ) {
        dst_pos = m_Shift + src_pos;
    }
    else {
        dst_pos = m_Shift - src_pos;
    }
    if ( dst_pos < m_Master_from || dst_pos > m_Master_to ) {
        m_Mapped = m_Partial = true;
        return kInvalidSeqPos;
    }
    return dst_pos;
}


CRef<CSeq_interval> CSeq_loc_Conversion::ConvertInterval(TSeqPos src_from,
                                                         TSeqPos src_to)
{
    TSeqPos dst_from, dst_to;
    if ( !m_Reverse ) {
        dst_from = m_Shift + src_from;
        dst_to = m_Shift + src_to;
    }
    else {
        dst_from = m_Shift - src_to;
        dst_to = m_Shift - src_from;
    }
    if ( dst_from < m_Master_from ) {
        dst_from = m_Master_from;
        m_Partial = m_Mapped = true;
    }
    if ( dst_to > m_Master_to ) {
        dst_to = m_Master_to;
        m_Partial = m_Mapped = true;
    }
    CRef<CSeq_interval> dst;
    if ( dst_from <= dst_to ) {
        dst.Reset(new CSeq_interval);
        dst->SetId().Assign(m_Master_id);
        dst->SetFrom(dst_from);
        dst->SetTo(dst_to);
    }
    return dst;
}


CRef<CSeq_point> CSeq_loc_Conversion::ConvertPoint(TSeqPos src_pos)
{
    TSeqPos dst_pos = ConvertPos(src_pos);
    CRef<CSeq_point> dst;
    if ( dst_pos != kInvalidSeqPos ) {
        dst.Reset(new CSeq_point);
        dst->SetId().Assign(m_Master_id);
        dst->SetPoint(dst_pos);
    }
    return dst;
}


CRef<CSeq_loc> CSeq_loc_Conversion::Convert(const CSeq_loc& src)
{
    CRef<CSeq_loc> dst;
    switch ( src.Which() ) {
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Null:
    case CSeq_loc::e_Empty:
    case CSeq_loc::e_Feat:
    {
        // Nothing to do, although this should never happen --
        // the seq_loc is intersecting with the conv. loc.
        break;
    }
    case CSeq_loc::e_Whole:
    {
        // Convert to the allowed master seq interval
        const CSeq_id& src_id = src.GetWhole();
        if ( m_Src_id != m_Scope->GetIdHandle(src_id) ) {
            m_Partial = true;
            break;
        }
        CBioseq_Handle bh = m_Scope->GetBioseqHandle(src_id);
        if ( !bh ) {
            THROW1_TRACE(runtime_error,
                         "CAnnotTypes_CI::x_ConvertLocToMaster: "
                         "cannot determine sequence length");
        }
        CBioseq_Handle::TBioseqCore core = bh.GetBioseqCore();
        if ( !core->GetInst().IsSetLength() ) {
            THROW1_TRACE(runtime_error,
                         "CAnnotTypes_CI::x_ConvertLocToMaster: "
                         "cannot determine sequence length");
        }
        CRef<CSeq_interval> dst_int =
            ConvertInterval(0, core->GetInst().GetLength());
        if ( dst_int ) {
            dst.Reset(new CSeq_loc);
            dst->SetInt(*dst_int);
        }
        break;
    }
    case CSeq_loc::e_Int:
    {
        const CSeq_interval& src_int = src.GetInt();
        if ( m_Src_id != m_Scope->GetIdHandle(src_int.GetId()) ) {
            m_Partial = true;
            break;
        }
        CRef<CSeq_interval> dst_int = ConvertInterval(src_int.GetFrom(),
                                                      src_int.GetTo());
        if ( dst_int ) {
            dst.Reset(new CSeq_loc);
            dst->SetInt(*dst_int);
        }
        break;
    }
    case CSeq_loc::e_Pnt:
    {
        const CSeq_point& src_pnt = src.GetPnt();
        if ( m_Src_id != m_Scope->GetIdHandle(src_pnt.GetId()) ) {
            m_Partial = true;
            break;
        }
        CRef<CSeq_point> dst_pnt = ConvertPoint(src_pnt.GetPoint());
        if ( dst_pnt ) {
            dst.Reset(new CSeq_loc);
            dst->SetPnt(*dst_pnt);
        }
        break;
    }
    case CSeq_loc::e_Packed_int:
    {
        const CPacked_seqint::Tdata& src_ints = src.GetPacked_int().Get();
        CPacked_seqint::Tdata* dst_ints = 0;
        iterate ( CPacked_seqint::Tdata, i, src_ints ) {
            const CSeq_interval& src_int = **i;
            if ( m_Src_id != m_Scope->GetIdHandle(src_int.GetId()) ) {
                m_Partial = true;
                continue;
            }
            CRef<CSeq_interval> dst_int = ConvertInterval(src_int.GetFrom(),
                                                          src_int.GetTo());
            if ( dst_int ) {
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
        if ( m_Src_id != m_Scope->GetIdHandle(src_pack_pnts.GetId()) ) {
            m_Partial = true;
            break;
        }
        const CPacked_seqpnt::TPoints& src_pnts = src_pack_pnts.GetPoints();
        CPacked_seqpnt::TPoints* dst_pnts = 0;
        iterate ( CPacked_seqpnt::TPoints, i, src_pnts ) {
            TSeqPos dst_pos = ConvertPos(*i);
            if ( dst_pos != kInvalidSeqPos ) {
                if ( !dst_pnts ) {
                    dst.Reset(new CSeq_loc);
                    CPacked_seqpnt& pnts = dst->SetPacked_pnt();
                    pnts.SetId().Assign(m_Master_id);
                    dst_pnts = &pnts.SetPoints();
                }
                dst_pnts->push_back(dst_pos);
            }
        }
        break;
    }
    case CSeq_loc::e_Mix:
    {
        const CSeq_loc_mix::Tdata& src_mix = src.GetMix().Get();
        CSeq_loc_mix::Tdata* dst_mix = 0;
        iterate ( CSeq_loc_mix::Tdata, i, src_mix ) {
            CRef<CSeq_loc> dst_loc = Convert(**i);
            if ( dst_loc ) {
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
        iterate ( CSeq_loc_equiv::Tdata, i, src_equiv ) {
            CRef<CSeq_loc> dst_loc = Convert(**i);
            if ( dst_loc ) {
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
        const CSeq_point& src_a = src_bond.GetA();
        CRef<CSeq_point> dst_a = ConvertPoint(src_a.GetPoint());
        CSeq_bond* dst_bond = 0;
        if ( dst_a ) {
            dst.Reset(new CSeq_loc);
            dst_bond = &dst->SetBond();
            dst_bond->SetA(*dst_a);
        }
        if ( src_bond.IsSetB() ) {
            const CSeq_point& src_b = src_bond.GetB();
            CRef<CSeq_point> dst_b = ConvertPoint(src_b.GetPoint());
            if ( dst_b ) {
                if ( dst_bond ) {
                    dst.Reset(new CSeq_loc);
                    dst_bond = &dst->SetBond();
                }
                dst_bond->SetB(*dst_b);
            }
        }
        break;
    }
    default:
    {
        THROW1_TRACE(runtime_error,
                     "CAnnotTypes_CI::x_ConvertLocToMaster() -- "
                     "Unsupported location type");
    }
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
            CRef<CSeq_loc> mapped = Convert(feat.GetLocation());
            if ( mapped && IsMapped() ) {
                ref.SetMappedLoc(*mapped);
            }
        }
        else if ( feat.IsSetProduct() ) {
            CRef<CSeq_loc> mapped = Convert(feat.GetProduct());
            if ( mapped && IsMapped() ) {
                ref.SetMappedProd(*mapped);
            }
        }
    }
    ref.SetPartial(IsPartial());
}


void CAnnotTypes_CI::x_Initialize2(const CHandleRangeMap& master_loc)
{
    x_SearchMain2(master_loc);
    if ( m_ResolveMethod != eResolve_None ) {
        iterate ( CHandleRangeMap::TLocMap, idit, master_loc.GetMap() ) {
            CBioseq_Handle bh = m_Scope->GetBioseqHandle(
                m_Scope->x_GetIdMapper().GetSeq_id(idit->first));
            if ( !bh ) {
                // resolve by Seq-id only
                
                continue;
            }
            const CSeq_entry* limit_tse = 0;
            if (m_ResolveMethod == eResolve_TSE) {
                limit_tse = &bh.GetTopLevelSeqEntry();
            }
        
            CHandleRange::TRange idrange = idit->second.GetOverlappingRange();
            const CSeqMap& seqMap = bh.GetSeqMap();
            CSeqMap_CI smit(seqMap.FindResolved(idrange.GetFrom(),
                                                m_Scope,
                                                size_t(-1),
                                                CSeqMap::fFindRef));
            if ( smit && smit.GetType() != CSeqMap::eSeqRef )
                ++smit;
            while ( smit && smit.GetPosition() < idrange.GetToOpen() ) {
                if ( limit_tse ) {
                    CBioseq_Handle bh2 =
                        m_Scope->GetBioseqHandle(smit.GetRefSeqid());
                    // The referenced sequence must be in the same TSE
                    if ( !bh2  || limit_tse != &bh2.GetTopLevelSeqEntry() ) {
                        smit.Next(false);
                        continue;
                    }
                }
                x_SearchLocation2(smit, idit);
                ++smit;
            }
        }
    }
    if ( m_Selector.m_AnnotChoice == CSeq_annot::C_Data::e_Ftable ) {
        sort(m_AnnotSet2.begin(), m_AnnotSet2.end(), CFeat_Less());
    }
    else {
        sort(m_AnnotSet2.begin(), m_AnnotSet2.end(), CAnnotObject_Less());
    }
    m_CurAnnot = m_AnnotSet2.begin();
}


void CAnnotTypes_CI::x_SearchMain2(CHandleRangeMap loc)
{
    // Search all possible TSEs
    TTSESet entries;
    m_Scope->x_PopulateTSESet(loc, m_Selector.m_AnnotChoice, entries);
    iterate(TTSESet, tse_it, entries) {
        if ( m_NativeTSE  &&  *tse_it != m_NativeTSE ) {
            continue;
        }
        m_TSESet.insert(*tse_it);
        const CTSE_Info& tse_info = **tse_it;

        const CTSE_Info::TAnnotMap& annot_map = tse_info.m_AnnotMap_ByTotal;
        
        iterate ( CHandleRangeMap::TLocMap, idit, loc.GetMap() ) {
            if ( idit->second.Empty() ) {
                continue;
            }

            CTSE_Info::TAnnotMap::const_iterator amit =
                annot_map.find(idit->first);
            if (amit == annot_map.end()) {
                continue;
            }

            CTSE_Info::TAnnotSelectorMap::const_iterator sit =
                amit->second.find(m_Selector);
            if ( sit == amit->second.end() ) {
                continue;
            }

            for ( CTSE_Info::TRangeMap::const_iterator aoit =
                      sit->second.begin(idit->second.GetOverlappingRange());
                  aoit; ++aoit ) {
                const SAnnotObject_Index& annot_index = aoit->second;
                const CAnnotObject_Info& annot_info =
                    *annot_index.m_AnnotObject;
                if ( m_SingleEntry  &&
                     (m_SingleEntry != &annot_info.GetSeq_entry())) {
                    continue;
                }

                if ( m_OverlapType == CAnnot_CI::eOverlap_Intervals &&
                     !idit->second.IntersectingWith(annot_index.m_HandleRange->second) ) {
                    continue;
                }
                
                m_AnnotSet2.push_back(CAnnotObject_Ref(annot_info));
            }
        }
    }
}


void CAnnotTypes_CI::x_SearchLocation2(const CSeqMap_CI& seg,
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
        iterate ( CHandleRange, mlit, master_loc->second ) {
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
    TTSESet entries;
    m_Scope->x_PopulateTSESet(ref_loc, m_Selector.m_AnnotChoice, entries);
    iterate(TTSESet, tse_it, entries) {
        if (bool(m_NativeTSE)  &&  *tse_it != m_NativeTSE)
            continue;
        m_TSESet.insert(*tse_it);
        const CTSE_Info& tse_info = **tse_it;
        CTSE_Guard guard(tse_info);
        const CTSE_Info::TAnnotMap& annot_map = tse_info.m_AnnotMap_ByTotal;

        iterate ( CHandleRangeMap, idit, ref_loc ) {
            CSeq_loc_Conversion cvt(master_loc->first.GetSeqId(),
                                    seg, idit->first,
                                    m_Scope);

            CTSE_Info::TAnnotMap::const_iterator ait =
                annot_map.find(idit->first);
            if ( ait == annot_map.end() )
                continue;

            CTSE_Info::TAnnotSelectorMap::const_iterator sit =
                ait->second.find(m_Selector);
            if ( sit == ait->second.end() )
                continue;

            for ( CTSE_Info::TRangeMap::const_iterator aoit =
                      sit->second.begin(idit->second.GetOverlappingRange());
                  aoit; ++aoit ) {
                const SAnnotObject_Index& annot_index = aoit->second;
                const CAnnotObject_Info& annot_info =
                    *annot_index.m_AnnotObject;
                if ( bool(m_SingleEntry)  &&
                     (m_SingleEntry != &annot_info.GetSeq_entry()) )
                    continue;

                if ( m_OverlapType == CAnnot_CI::eOverlap_Intervals &&
                     !idit->second.IntersectingWith(annot_index.m_HandleRange->second) ) {
                    continue;
                }

                m_AnnotSet2.push_back(CAnnotObject_Ref(annot_info));
                cvt.Convert(m_AnnotSet2.back(), m_Selector.m_FeatProduct);
            }
        }
    }
}


void CAnnotTypes_CI::x_SearchLocation2(const CSeqMap_CI& seg,
                                       const CSeq_id& master_id,
                                       CHandleRange::const_iterator master_seg)
{
    // Search all possible TSEs
    TTSESet entries;
    CHandleRangeMap hrm;
    hrm.AddRange(seg.GetRefSeqid(),
                 COpenRange<TSeqPos>(seg.GetRefPosition(),
                                     seg.GetRefEndPosition()),
                 seg.GetRefMinusStrand()? eNa_strand_minus: eNa_strand_plus);
    m_Scope->x_PopulateTSESet(hrm, m_Selector.m_AnnotChoice, entries);
    iterate(TTSESet, tse_it, entries) {
        if (bool(m_NativeTSE)  &&  *tse_it != m_NativeTSE)
            continue;
        m_TSESet.insert(*tse_it);
        const CTSE_Info& tse_info = **tse_it;
        CTSE_Guard guard(tse_info);
        const CTSE_Info::TAnnotMap& annot_map = tse_info.m_AnnotMap_ByTotal;

        iterate ( CHandleRangeMap::TLocMap, idit, hrm.GetMap() ) {
            _ASSERT(idit->second.end() - idit->second.begin() == 1);
            _ASSERT(idit->second.begin()->first ==
                    COpenRange<TSeqPos>(seg.GetRefPosition(),
                                        seg.GetRefEndPosition()));

            CSeq_loc_Conversion cvt(master_id, seg, idit->first, m_Scope);
            CHandleRangeMap idrm;
            idrm.AddRanges(idit->first, idit->second);

            CTSE_Info::TAnnotMap::const_iterator ait =
                annot_map.find(idit->first);
            if ( ait == annot_map.end() )
                continue;

            CTSE_Info::TAnnotSelectorMap::const_iterator sit =
                ait->second.find(m_Selector);
            if ( sit == ait->second.end() )
                continue;

            for ( CTSE_Info::TRangeMap::const_iterator aoit =
                      sit->second.begin(idit->second.GetOverlappingRange());
                  aoit; ++aoit ) {
                const SAnnotObject_Index& annot_index = aoit->second;
                const CAnnotObject_Info& annot_info =
                    *annot_index.m_AnnotObject;
                if ( bool(m_SingleEntry)  &&
                     (m_SingleEntry != &annot_info.GetSeq_entry()) )
                    continue;

                if ( m_OverlapType == CAnnot_CI::eOverlap_Intervals &&
                     !idit->second.IntersectingWith(annot_index.m_HandleRange->second) ) {
                    continue;
                }

                m_AnnotSet2.push_back(CAnnotObject_Ref(annot_info));
                cvt.Convert(m_AnnotSet2.back(), m_Selector.m_FeatProduct);
            }
        }
    }
}


void CAnnotTypes_CI::x_Initialize(CHandleRangeMap& master_loc)
{
    bool has_references = false;
    if (m_ResolveMethod != eResolve_None) {
        iterate(CHandleRangeMap::TLocMap, id_it, master_loc.GetMap()) {
/*
            CBioseq_Handle h = m_Scope->GetBioseqHandle(
                m_Scope->x_GetIdMapper().GetSeq_id(id_it->first));
            const CSeqMap& smap = h.GetSeqMap();
            for (TSeqPos seg = 0; seg < smap.size(); ++seg) {
                has_references = has_references || (smap[seg].GetType() == CSeqMap::eSeqRef);
            }
            if ( !has_references )
                continue;
*/
has_references = true;
            // Iterate intervals for the current id, resolve each interval
            iterate(CHandleRange, rit, id_it->second) {
                // Resolve the master location, check if there are annotations
                // on referenced sequences.
                x_ResolveReferences(id_it->first,             // master id
                    id_it->first,                             // id to resolve
                    rit->first.GetFrom(), rit->first.GetTo(), // ref. interval
                    rit->second, 0);                          // strand, no shift
            }
        }
    }
    if ( !has_references ) {
        m_ResolveMethod = eResolve_None;
        x_SearchLocation(master_loc);
    }
    if (m_ResolveMethod != eResolve_None) {
        TAnnotSet orig_annots;
        swap(orig_annots, m_AnnotSet);
        non_const_iterate(TAnnotSet, it, orig_annots) {
            m_AnnotSet.insert(x_ConvertAnnotToMaster(it->Get()));
        }
    }
    //m_CurAnnot = m_AnnotSet.begin();
}


void CAnnotTypes_CI::x_ResolveReferences(const CSeq_id_Handle& master_idh,
                                         const CSeq_id_Handle& ref_idh,
                                         TSeqPos rmin,
                                         TSeqPos rmax,
                                         ENa_strand strand,
                                         TSignedSeqPos shift)
{
    // Create a new entry in the convertions map
    CRef<SConvertionRec> rec(new SConvertionRec);
    rec->m_Location.reset(new CHandleRangeMap());
    // Create the location on the referenced sequence
    rec->m_Location->AddRange(ref_idh, CHandleRange::TRange(rmin, rmax), strand);
    rec->m_RefShift = shift;
    rec->m_RefMin = rmin; //???
    rec->m_RefMax = rmax; //???
    rec->m_MasterId = master_idh;
    rec->m_Master = master_idh == ref_idh;
    m_ConvMap[ref_idh].push_back(rec);
    // Search for annotations
    x_SearchLocation(*rec->m_Location);
    if (m_ResolveMethod == eResolve_None)
        return;
    // Resolve references for a segmented bioseq, get features for
    // the segments.
    CBioseq_Handle ref_seq = m_Scope->GetBioseqHandle(ref_idh);
    if ( !ref_seq ) {
        return;
    }
    const CSeqMap& ref_map = ref_seq.GetSeqMap();
    for (CSeqMap::const_iterator seg = ref_map.FindSegment(rmin, m_Scope); seg; ++seg) {
        // Check each map segment for intersections with the range
        if (rmin >= seg.GetPosition() + seg.GetLength())
            continue; // Go to the next segment
        if (rmax < seg.GetPosition())
            break; // No more intersecting segments
        if (seg.GetType() == CSeqMap::eSeqRef) {
            // Check for valid TSE
            if (m_ResolveMethod == eResolve_TSE) {
                CBioseq_Handle check_seq = m_Scope->GetBioseqHandle(seg.GetRefSeqid());
                // The referenced sequence must be in the same TSE as the master one
                CBioseq_Handle master_seq = m_Scope->GetBioseqHandle(master_idh);
                if (!check_seq  ||  !master_seq  ||
                    (&master_seq.GetTopLevelSeqEntry() != &check_seq.GetTopLevelSeqEntry())) {
                    continue;
                }
            }
            // Resolve the reference
            // Adjust the interval
            TSeqPos seg_min = seg.GetPosition();
            TSeqPos seg_max = seg_min + seg.GetLength();
            if (rmin > seg_min)
                seg_min = rmin;
            if (rmax < seg_max)
                seg_max = rmax;
            TSignedSeqPos rshift =
                shift + seg.GetPosition() - seg.GetRefPosition();
            // Adjust strand
            ENa_strand adj_strand = eNa_strand_unknown;
            if ( seg.GetRefMinusStrand() ) {
                if (strand == eNa_strand_minus)
                    adj_strand = eNa_strand_plus;
                else
                    adj_strand = eNa_strand_minus;
            }
            x_ResolveReferences(master_idh, seg.GetRefSeqid(),
                seg_min - rshift, seg_max - rshift, adj_strand, rshift);
        }
    }
}


void CAnnotTypes_CI::x_SearchLocation(CHandleRangeMap& loc)
{
    // Search all possible TSEs
    TTSESet entries;
    m_Scope->x_PopulateTSESet(loc, m_Selector.m_AnnotChoice, entries);
    non_const_iterate(TTSESet, tse_it, entries) {
        if (bool(m_NativeTSE)  &&  *tse_it != m_NativeTSE)
            continue;
        m_TSESet.insert(*tse_it);
        CTSE_Info& tse_info = **tse_it;
        CAnnot_CI annot_it(tse_info, loc, m_Selector, m_OverlapType);
        for ( ; annot_it; ++annot_it ) {
            if (bool(m_SingleEntry)  &&
                (m_SingleEntry != &annot_it->GetSeq_entry())) {
                continue;
            }
            m_AnnotSet.insert(CAnnotObject_Ref(*annot_it));
        }
    }
}


void x_Assign(CSeq_feat& dst, const CSeq_feat& fsrc)
{
    CSeq_feat& src = const_cast<CSeq_feat&>(fsrc);
    dst.SetData(src.SetData());
    if (src.IsSetId()) {
        dst.SetId(src.SetId());
    }
    if (src.IsSetPartial()) {
        dst.SetPartial(src.GetPartial());
    }
    if (src.IsSetExcept()) {
        dst.SetExcept(src.GetExcept());
    }
    if (src.IsSetComment()) {
        dst.SetComment(src.GetComment());
    }
    if (src.IsSetProduct()) {
        dst.SetProduct().Assign(src.GetProduct());
    }
    dst.SetLocation().Assign(src.GetLocation());
    if (src.IsSetQual()) {
        dst.SetQual() = src.GetQual();
    }
    if (src.IsSetTitle()) {
        dst.SetTitle(src.GetTitle());
    }
    if (src.IsSetExt()) {
        dst.SetExt(src.SetExt());
    }
    if (src.IsSetCit()) {
        dst.SetCit(src.SetCit());
    }
    if (src.IsSetExp_ev()) {
        dst.SetExp_ev(src.GetExp_ev());
    }
    if (src.IsSetXref()) {
        dst.SetXref() = src.GetXref();
    }
    if (src.IsSetDbxref()) {
        dst.SetDbxref() = src.GetDbxref();
    }
    if (src.IsSetPseudo()) {
        dst.SetPseudo(src.GetPseudo());
    }
    if (src.IsSetExcept_text()) {
        dst.SetExcept_text(src.GetExcept_text());
    }
}


CAnnotObject_Ref
CAnnotTypes_CI::x_ConvertAnnotToMaster(const CAnnotObject_Info& annot_obj) const
{
    CAnnotObject_Ref AnnotCopy(annot_obj);
    CRef<CSeq_loc> lcopy(new CSeq_loc);
    EConverted conv_res;
    switch ( annot_obj.Which() ) {
    case CSeq_annot::C_Data::e_Ftable:
        {
            const CSeq_feat& fsrc = annot_obj.GetFeat();
            // Process feature location
            conv_res = x_ConvertLocToMaster(fsrc.GetLocation(), lcopy);
            if (conv_res != eConverted_None) {
                AnnotCopy.SetMappedLoc(*lcopy);
                AnnotCopy.SetPartial(conv_res == eConverted_Partial);
            }
            if ( fsrc.IsSetProduct() ) {
                lcopy.Reset();
                conv_res = x_ConvertLocToMaster(fsrc.GetProduct(), lcopy);
                if (conv_res != eConverted_None) {
                    AnnotCopy.SetMappedProd(*lcopy);
                    AnnotCopy.SetPartial(conv_res == eConverted_Partial);
                }
            }
            break;
        }
    case CSeq_annot::C_Data::e_Align:
        {
            LOG_POST(Warning <<
            "CAnnotTypes_CI -- seq-align convertions not implemented.");
            break;
        }
    case CSeq_annot::C_Data::e_Graph:
        {
            const CSeq_graph& gsrc = annot_obj.GetGraph();
            // Process graph location
            conv_res = x_ConvertLocToMaster(gsrc.GetLoc(), lcopy);
            if (conv_res != eConverted_None) {
                AnnotCopy.SetMappedLoc(*lcopy);
                AnnotCopy.SetPartial(conv_res == eConverted_Partial);
            }
            break;
        }
    default:
        {
            LOG_POST(Warning <<
            "CAnnotTypes_CI -- annotation type not implemented.");
            break;
        }
    }
    return AnnotCopy;
}


CAnnotTypes_CI::EConverted
CAnnotTypes_CI::x_ConvertLocToMaster(const CSeq_loc& src,
                                     CRef<CSeq_loc>& dest) const
{
    EConverted conv_res = eConverted_None;
    iterate (TConvMap, convmap_it, m_ConvMap) {
        // Check seq_id
        iterate (TIdConvList, conv_it, convmap_it->second) {
            if ( !(*conv_it)->m_Location->IntersectingWithLoc(src) )
                 continue;
            // Do not use master location intervals for convertions
            if ( (*conv_it)->m_Master )
                continue;
            dest.Reset(new CSeq_loc());
            switch ( src.Which() ) {
            case CSeq_loc::e_not_set:
            case CSeq_loc::e_Null:
            case CSeq_loc::e_Empty:
            case CSeq_loc::e_Feat:
                {
                    // Nothing to do, although this should never happen --
                    // the seq_loc is intersecting with the conv. loc.
                    continue;
                }
            case CSeq_loc::e_Whole:
                {
                    // Convert to the allowed master seq interval
                    dest->SetInt().SetId().Assign(m_Scope->x_GetIdMapper().
                                                  GetSeq_id((*conv_it)->m_MasterId));
                    dest->SetInt().SetFrom
                        ((*conv_it)->m_RefMin + (*conv_it)->m_RefShift);
                    dest->SetInt().SetTo
                        ((*conv_it)->m_RefMax + (*conv_it)->m_RefShift);
                    TSeqPos seqLen =
                        m_Scope->GetBioseqHandle(src.GetWhole())
                        .GetSeqMap().GetLength(m_Scope);
                    if (dest->GetInt().GetLength() < seqLen)
                        conv_res = eConverted_Partial;
                    else
                        conv_res = eConverted_Mapped;
                    continue;
                }
            case CSeq_loc::e_Int:
                {
                    // Convert to the allowed master seq interval
                    dest->SetInt().SetId().Assign(m_Scope->x_GetIdMapper().
                                                  GetSeq_id((*conv_it)->m_MasterId));
                    TSeqPos new_from = src.GetInt().GetFrom();
                    TSeqPos new_to = src.GetInt().GetTo();
                    conv_res = eConverted_Mapped;
                    if (new_from < (*conv_it)->m_RefMin) {
                        new_from = (*conv_it)->m_RefMin;
                        conv_res = eConverted_Partial;
                    }
                    if (new_to > (*conv_it)->m_RefMax) {
                        new_to = (*conv_it)->m_RefMax;
                        conv_res = eConverted_Partial;
                    }
                    new_from += (*conv_it)->m_RefShift;
                    new_to += (*conv_it)->m_RefShift;
                    dest->SetInt().SetFrom(new_from);
                    dest->SetInt().SetTo(new_to);
                    continue;
                }
            case CSeq_loc::e_Pnt:
                {
                    dest->SetPnt().SetId().Assign(m_Scope->x_GetIdMapper().
                                                  GetSeq_id((*conv_it)->m_MasterId));
                    // Convert to the allowed master seq interval
                    TSeqPos new_pnt = src.GetPnt().GetPoint();
                    conv_res = eConverted_Mapped;
                    if (new_pnt < (*conv_it)->m_RefMin  ||
                        new_pnt > (*conv_it)->m_RefMax) {
                        //### Can this happen if loc is intersecting with the
                        //### conv_it location?
                        dest->SetEmpty();
                        conv_res = eConverted_Partial;
                        continue;
                    }
                    dest->SetPnt().SetPoint(new_pnt + (*conv_it)->m_RefShift);
                    continue;
                }
            case CSeq_loc::e_Packed_int:
                {
                    conv_res = eConverted_Mapped;
                    dest->Assign(src);
                    non_const_iterate ( CPacked_seqint::Tdata, ii, dest->SetPacked_int().Set() ) {
                        CSeq_loc idloc;
                        idloc.SetWhole().Assign((*ii)->GetId());
                        if (!(*conv_it)->m_Location->IntersectingWithLoc(idloc))
                            continue;
                        // Convert to the allowed master seq interval
                        (*ii)->SetId().Assign(
                            m_Scope->x_GetIdMapper().GetSeq_id((*conv_it)->m_MasterId));
                        TSeqPos new_from = (*ii)->GetFrom();
                        TSeqPos new_to = (*ii)->GetTo();
                        if (new_from < (*conv_it)->m_RefMin) {
                            new_from = (*conv_it)->m_RefMin;
                            conv_res = eConverted_Partial;
                        }
                        if (new_to > (*conv_it)->m_RefMax) {
                            new_to = (*conv_it)->m_RefMax;
                            conv_res = eConverted_Partial;
                        }
                        new_from += (*conv_it)->m_RefShift;
                        new_to += (*conv_it)->m_RefShift;
                        (*ii)->SetFrom(new_from);
                        (*ii)->SetTo(new_to);
                    }
                    continue;
                }
            case CSeq_loc::e_Packed_pnt:
                {
                    dest->SetPacked_pnt().SetId().Assign(
                        m_Scope->x_GetIdMapper().GetSeq_id((*conv_it)->m_MasterId));
                    const CPacked_seqpnt::TPoints& pnt_copy = src.GetPacked_pnt().GetPoints();
                    dest->SetPacked_pnt().ResetPoints();
                    conv_res = eConverted_Mapped;
                    iterate ( CPacked_seqpnt::TPoints, pi, pnt_copy ) {
                        // Convert to the allowed master seq interval
                        TSeqPos new_pnt = *pi;
                        if (new_pnt >= (*conv_it)->m_RefMin  &&
                            new_pnt <= (*conv_it)->m_RefMax) {
                            dest->SetPacked_pnt().SetPoints().push_back
                                (*pi + (*conv_it)->m_RefShift);
                        }
                        else {
                            conv_res = eConverted_Partial;
                        }
                    }
                    continue;
                }
            case CSeq_loc::e_Mix:
                {
                    EConverted tmp_res;
                    CRef<CSeq_loc> tmp_loc;
                    iterate(CSeq_loc_mix::Tdata, li, src.GetMix().Get()) {
                        tmp_res = x_ConvertLocToMaster(**li, tmp_loc);
                        if ( tmp_res != eConverted_None ) {
                            dest->SetMix().Set().push_back(tmp_loc);
                            if (tmp_res > conv_res)
                                conv_res = tmp_res;
                        }
                        tmp_loc.Reset();
                    }
                    continue;
                }
            case CSeq_loc::e_Equiv:
                {
                    EConverted tmp_res;
                    CRef<CSeq_loc> tmp_loc;
                    iterate(CSeq_loc_equiv::Tdata, li, src.GetEquiv().Get()) {
                        tmp_res = x_ConvertLocToMaster(**li, tmp_loc);
                        if ( tmp_res != eConverted_None ) {
                            dest->SetMix().Set().push_back(tmp_loc);
                            if (tmp_res > conv_res)
                                conv_res = tmp_res;
                        }
                        tmp_loc.Reset();
                    }
                    continue;
                }
            case CSeq_loc::e_Bond:
                {
                    bool corrected = false;
                    CSeq_loc idloc;
                    dest->Assign(src);
                    idloc.SetWhole().Assign(dest->GetBond().GetA().GetId());
                    if ((*conv_it)->m_Location->IntersectingWithLoc(idloc)) {
                        // Convert A to the allowed master seq interval
                        dest->SetBond().SetA().SetId().Assign(
                            m_Scope->x_GetIdMapper().GetSeq_id((*conv_it)->m_MasterId));
                        TSeqPos newA = dest->GetBond().GetA().GetPoint();
                        if (newA >= (*conv_it)->m_RefMin  &&
                            newA <= (*conv_it)->m_RefMax) {
                            dest->SetBond().SetA().SetPoint(newA + (*conv_it)->m_RefShift);
                            corrected = true;
                        }
                        else {
                            conv_res = eConverted_Partial;
                        }
                    }
                    if ( dest->GetBond().IsSetB() ) {
                        idloc.SetWhole().Assign(dest->GetBond().GetB().GetId());
                        if ((*conv_it)->m_Location->IntersectingWithLoc(idloc)) {
                            // Convert A to the allowed master seq interval
                            dest->SetBond().SetB().SetId().Assign(
                                m_Scope->x_GetIdMapper().GetSeq_id((*conv_it)->m_MasterId));
                            TSeqPos newB = dest->GetBond().GetB().GetPoint();
                            if (newB >= (*conv_it)->m_RefMin  &&
                                newB <= (*conv_it)->m_RefMax) {
                                dest->SetBond().SetB().SetPoint(newB + (*conv_it)->m_RefShift);
                                corrected |= true;
                            }
                            else {
                                conv_res = eConverted_Partial;
                            }
                        }
                    }
                    if ( !corrected )
                        dest->SetEmpty();
                    continue;
                }
            default:
                {
                    THROW1_TRACE(runtime_error,
                        "CAnnotTypes_CI::x_ConvertLocToMaster() -- "
                        "Unsupported location type");
                }
            }
        }
    }
    return conv_res;
}



END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
