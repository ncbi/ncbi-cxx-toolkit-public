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

#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/seq_map.hpp>
#include <objmgr/impl/annot_object.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/impl/synonyms.hpp>
#include <objmgr/impl/seq_loc_cvt.hpp>
#include <objmgr/impl/snp_annot_info.hpp>
#include <objmgr/objmgr_exception.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_bond.hpp>

#include <serial/typeinfo.hpp>
#include <serial/objostr.hpp>
#include <serial/objostrasn.hpp>
#include <serial/serial.hpp>

#include <algorithm>
#include <typeinfo>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
// CAnnotObject_Ref
/////////////////////////////////////////////////////////////////////////////


inline
CAnnotObject_Ref::CAnnotObject_Ref(const CAnnotObject_Info& object)
    : m_Object(&object.GetSeq_annot_Info()),
      m_AnnotObject_Index(object.GetSeq_annot_Info().GetAnnotObjectIndex(object)),
      m_MappedIndex(0),
      m_ObjectType(eType_Seq_annot_Info),
      m_Partial(false),
      m_MappedType(CSeq_loc::e_not_set),
      m_MappedStrand(eNa_strand_unknown)
{
    if ( object.IsFeat() ) {
        const CSeq_feat& feat = *object.GetFeatFast();
        if ( feat.IsSetPartial() ) {
            m_Partial = feat.GetPartial();
        }
    }
}


inline
CAnnotObject_Ref::CAnnotObject_Ref(const CSeq_annot_SNP_Info& snp_annot,
                                   TSeqPos index)
    : m_Object(&snp_annot),
      m_AnnotObject_Index(index),
      m_MappedIndex(0),
      m_ObjectType(eType_Seq_annot_SNP_Info),
      m_Partial(false),
      m_MappedType(CSeq_loc::e_not_set),
      m_MappedStrand(eNa_strand_unknown)
{
}


const CSeq_annot_Info& CAnnotObject_Ref::GetSeq_annot_Info(void) const
{
    _ASSERT(m_ObjectType == eType_Seq_annot_Info);
    return static_cast<const CSeq_annot_Info&>(*m_Object);
}


const CSeq_annot_SNP_Info& CAnnotObject_Ref::GetSeq_annot_SNP_Info(void) const
{
    _ASSERT(m_ObjectType == eType_Seq_annot_SNP_Info);
    return static_cast<const CSeq_annot_SNP_Info&>(*m_Object);
}


const CAnnotObject_Info& CAnnotObject_Ref::GetAnnotObject_Info(void) const
{
    _ASSERT(m_ObjectType == eType_Seq_annot_Info);
    return static_cast<const CSeq_annot_Info&>(*m_Object).GetAnnotObject_Info(m_AnnotObject_Index);
}


const SSNP_Info& CAnnotObject_Ref::GetSNP_Info(void) const
{
    _ASSERT(m_ObjectType == eType_Seq_annot_SNP_Info);
    return static_cast<const CSeq_annot_SNP_Info&>(*m_Object).GetSNP_Info(m_AnnotObject_Index);
}


const CSeq_annot& CAnnotObject_Ref::GetSeq_annot(void) const
{
    if ( m_ObjectType == eType_Seq_annot_SNP_Info ) {
        return GetSeq_annot_SNP_Info().GetSeq_annot();
    }
    else {
        return GetAnnotObject_Info().GetSeq_annot();
    }
}


void CAnnotObject_Ref::SetPartial(bool value)
{
    m_Partial = value;
}


inline
void CAnnotObject_Ref::SetSNP_Point(const SSNP_Info& snp,
                                    CSeq_loc_Conversion* cvt)
{
    _ASSERT(m_ObjectType == eType_Seq_annot_SNP_Info);
    TSeqPos src_from = snp.GetFrom(), src_to = snp.GetTo();
    ENa_strand src_strand =
        snp.MinusStrand()? eNa_strand_minus: eNa_strand_plus;
    if ( !cvt ) {
        m_TotalRange.SetFrom(src_from).SetTo(src_to);
        m_Partial = false;
        m_MappedLocation.Reset();
        m_MappedIndex = 0;
        m_MappedType = CSeq_loc::e_Pnt;
        m_MappedStrand = src_strand;
        return;
    }

    cvt->Reset();
    if ( src_from == src_to ) {
        // point
        _VERIFY(cvt->ConvertPoint(src_from, src_strand));
    }
    else {
        // interval
        _VERIFY(cvt->ConvertInterval(src_from, src_to, src_strand));
    }
    cvt->SetMappedLocation(*this, 0);
}


void CAnnotObject_Ref::UpdateMappedLocation(CRef<CSeq_loc>& loc) const
{
    _ASSERT(MappedNeedsUpdate());

    CRef<CSeq_id> id(const_cast<CSeq_id*>(&m_MappedLocation->GetEmpty()));
    if ( !loc || !loc->ReferencedOnlyOnce() ) {
        loc.Reset(new CSeq_loc);
    }
    if ( m_MappedType == CSeq_loc::e_Pnt ) {
        CSeq_point& point = loc->SetPnt();
        point.SetId(*id);
        point.SetPoint(m_TotalRange.GetFrom());
        if ( m_MappedStrand != eNa_strand_unknown )
            point.SetStrand(ENa_strand(m_MappedStrand));
        else
            point.ResetStrand();
    }
    else {
        CSeq_interval& interval = loc->SetInt();
        interval.SetId(*id);
        interval.SetFrom(m_TotalRange.GetFrom());
        interval.SetTo(m_TotalRange.GetTo());
        if ( m_MappedStrand != eNa_strand_unknown )
            interval.SetStrand(ENa_strand(m_MappedStrand));
        else
            interval.ResetStrand();
    }
}


void CAnnotObject_Ref::UpdateMappedLocation(CRef<CSeq_loc>& loc,
                                            CRef<CSeq_point>& pnt_ref,
                                            CRef<CSeq_interval>& int_ref) const
{
    _ASSERT(MappedNeedsUpdate());

    CRef<CSeq_id> id(const_cast<CSeq_id*>(&m_MappedLocation->GetEmpty()));
    if ( !loc || !loc->ReferencedOnlyOnce() ) {
        loc.Reset(new CSeq_loc);
    }
    else {
        loc->Reset();
    }
    if ( m_MappedType == CSeq_loc::e_Pnt ) {
        if ( !pnt_ref || !pnt_ref->ReferencedOnlyOnce() ) {
            pnt_ref.Reset(new CSeq_point);
        }
        CSeq_point& point = *pnt_ref;
        loc->SetPnt(point);
        point.SetId(*id);
        point.SetPoint(m_TotalRange.GetFrom());
        if ( m_MappedStrand != eNa_strand_unknown )
            point.SetStrand(ENa_strand(m_MappedStrand));
        else
            point.ResetStrand();
    }
    else {
        if ( !int_ref || !int_ref->ReferencedOnlyOnce() ) {
            int_ref.Reset(new CSeq_interval);
        }
        CSeq_interval& interval = *int_ref;
        loc->SetInt(interval);
        interval.SetId(*id);
        interval.SetFrom(m_TotalRange.GetFrom());
        interval.SetTo(m_TotalRange.GetTo());
        if ( m_MappedStrand != eNa_strand_unknown )
            interval.SetStrand(ENa_strand(m_MappedStrand));
        else
            interval.ResetStrand();
    }
}


bool CAnnotObject_Ref::operator<(const CAnnotObject_Ref& ref) const
{
    if ( m_ObjectType != ref.m_ObjectType ) {
        return m_ObjectType < ref.m_ObjectType;
    }
    if ( m_Object != ref.m_Object ) {
        return m_Object < ref.m_Object;
    }
    return m_AnnotObject_Index < ref.m_AnnotObject_Index;
}


/////////////////////////////////////////////////////////////////////////////
// CAnnotObject_Ref comparision
/////////////////////////////////////////////////////////////////////////////

template<class ObjectLess>
struct CAnnotObject_Ref_Less : public ObjectLess
{
    // Compare CRef-s: both must be features
    bool operator()(const CAnnotObject_Ref& x,
                    const CAnnotObject_Ref& y) const
        {
            {
                // smallest left extreme first
                TSeqPos x_from = x.GetFrom();
                TSeqPos y_from = y.GetFrom();
                if ( x_from != y_from ) {
                    return x_from < y_from;
                }
            }
            {
                // longest feature first
                TSeqPos x_to = x.GetToOpen();
                TSeqPos y_to = y.GetToOpen();
                if ( x_to != y_to ) {
                    return x_to > y_to;
                }
            }
            return x_less(x, y);
        }
};


template<class ObjectReverseLess>
struct CAnnotObject_Ref_Reverse_Less : public ObjectReverseLess
{
    // Compare CRef-s: both must be features
    bool operator()(const CAnnotObject_Ref& x,
                    const CAnnotObject_Ref& y) const
        {
            {
                // largest right extreme first
                TSeqPos x_to = x.GetToOpen();
                TSeqPos y_to = y.GetToOpen();
                if ( x_to != y_to ) {
                    return x_to > y_to;
                }
            }
            {
                // longest feature first
                TSeqPos x_from = x.GetFrom();
                TSeqPos y_from = y.GetFrom();
                if ( x_from != y_from ) {
                    return x_from < y_from;
                }
            }
            return x_less(x, y);
        }
};


struct CFeat_Less
{
    bool x_less(const CAnnotObject_Ref& x, const CAnnotObject_Ref& y) const;
};


struct CAnnotObject_Less
{
    bool x_less(const CAnnotObject_Ref& x, const CAnnotObject_Ref& y) const;
};


bool CFeat_Less::x_less(const CAnnotObject_Ref& x,
                        const CAnnotObject_Ref& y) const
{
    if ( !x.IsSNPFeat() && !y.IsSNPFeat() ) {
        const CAnnotObject_Info& x_info = x.GetAnnotObject_Info();
        const CAnnotObject_Info& y_info = y.GetAnnotObject_Info();
        const CSeq_feat* x_feat = x_info.GetFeatFast();
        const CSeq_feat* y_feat = y_info.GetFeatFast();
        _ASSERT(x_feat && y_feat);
        const CSeq_loc* x_loc = x.GetMappedLocation();
        const CSeq_loc* y_loc = y.GetMappedLocation();
        if ( !x_loc ) {
            if ( x.GetMappedIndex() == 0 ) {
                x_loc = &x_feat->GetLocation();
            }
            else {
                _ASSERT(x_feat->IsSetProduct());
                x_loc = &x_feat->GetProduct();
            }
        }
        if ( !y_loc ) {
            if ( y.GetMappedIndex() == 0 ) {
                y_loc = &y_feat->GetLocation();
            }
            else {
                _ASSERT(y_feat->IsSetProduct());
                y_loc = &y_feat->GetProduct();
            }
        }
        try {
            int diff = x_feat->CompareNonLocation(*y_feat, *x_loc, *y_loc);
            if ( diff != 0 ) {
                return diff < 0;
            }
        }
        catch ( exception& /*ignored*/ ) {
            // do not fail sort when compare function throws an exception
        }
    }
    else if ( !y.IsSNPFeat() ) {
        const CAnnotObject_Info& y_info = y.GetAnnotObject_Info();
        const CSeq_feat* y_feat = y_info.GetFeatFast();
        _ASSERT(y_feat);
        const CSeqFeatData& y_data = y_feat->GetData();
        int x_order = CSeq_feat::GetTypeSortingOrder(CSeqFeatData::e_Imp);
        int y_order = CSeq_feat::GetTypeSortingOrder(y_data.Which());
        if ( x_order != y_order ) {
            return x_order < y_order;
        }
        CSeqFeatData::ESubtype y_subtype = y_data.GetSubtype();
        if ( CSeqFeatData::eSubtype_variation != y_subtype ) {
            return CSeqFeatData::eSubtype_variation < y_order;
        }
        // both are SNP but x is simple, and y is not, so x is first
        return true;
    }
    else if ( !x.IsSNPFeat() ) {
        const CAnnotObject_Info& x_info = x.GetAnnotObject_Info();
        const CSeq_feat* x_feat = x_info.GetFeatFast();
        _ASSERT(x_feat);
        const CSeqFeatData& x_data = x_feat->GetData();
        int x_order = CSeq_feat::GetTypeSortingOrder(x_data.Which());
        int y_order = CSeq_feat::GetTypeSortingOrder(CSeqFeatData::e_Imp);
        if ( x_order != y_order ) {
            return x_order < y_order;
        }
        CSeqFeatData::ESubtype x_subtype = x_data.GetSubtype();
        if ( x_subtype != CSeqFeatData::eSubtype_variation ) {
            return x_subtype < CSeqFeatData::eSubtype_variation;
        }
        // both are SNP but y is simple, and x is not, so y is first
        return false;
    }
    const CSeq_annot& x_annot = x.GetSeq_annot();
    const CSeq_annot& y_annot = y.GetSeq_annot();
    if ( &x_annot != &y_annot ) {
        return &x_annot < &y_annot;
    }
    return x.GetAnnotObjectIndex() < y.GetAnnotObjectIndex();
}


bool CAnnotObject_Less::x_less(const CAnnotObject_Ref& x,
                               const CAnnotObject_Ref& y) const
{
    const CSeq_annot& x_annot = x.GetSeq_annot();
    const CSeq_annot& y_annot = y.GetSeq_annot();
    if ( &x_annot != &y_annot ) {
        return &x_annot < &y_annot;
    }
    return x.GetAnnotObjectIndex() < y.GetAnnotObjectIndex();
}


/////////////////////////////////////////////////////////////////////////////
// CAnnotTypes_CI
/////////////////////////////////////////////////////////////////////////////


class CAnnotDataCollector
{
public:
    typedef map<CAnnotObject_Ref, CSeq_loc_Conversion_Set> TAnnotMappingSet;
    // Set of annotations for complex remapping
    TAnnotMappingSet              m_AnnotMappingSet;
    // info of limit object
    CConstRef<CObject>            m_LimitObjectInfo;
};


CAnnotTypes_CI::CAnnotTypes_CI(void)
    : m_DataCollector(0)
{
}


CAnnotTypes_CI::CAnnotTypes_CI(CScope& scope,
                               const CSeq_loc& loc,
                               const SAnnotSelector& params)
    : SAnnotSelector(params),
      m_Scope(scope),
      m_DataCollector(new CAnnotDataCollector)
{
    CHandleRangeMap master_loc;
    master_loc.AddLocation(loc);
    x_Initialize(master_loc);
}


CAnnotTypes_CI::CAnnotTypes_CI(const CBioseq_Handle& bioseq,
                               TSeqPos start, TSeqPos stop,
                               const SAnnotSelector& params)
    : SAnnotSelector(params),
      m_Scope(bioseq.GetScope()),
      m_DataCollector(new CAnnotDataCollector)
{
    x_Initialize(bioseq, start, stop);
}


CAnnotTypes_CI::CAnnotTypes_CI(const CSeq_annot_Handle& annot,
                               const SAnnotSelector& params)
    : SAnnotSelector(params),
      m_Scope(annot.GetScope()),
      m_DataCollector(new CAnnotDataCollector)
{
    SetResolveNone(); // nothing to resolve
    SetLimitSeqAnnot(&annot.GetSeq_annot());
    x_Initialize();
}


CAnnotTypes_CI::CAnnotTypes_CI(CScope& scope,
                               const CSeq_entry& entry,
                               const SAnnotSelector& params)
    : SAnnotSelector(params),
      m_Scope(scope),
      m_DataCollector(new CAnnotDataCollector)
{
    SetResolveNone(); // nothing to resolve
    SetSortOrder(eSortOrder_None);
    SetLimitSeqEntry(&entry);
    x_Initialize();
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
      m_Scope(scope),
      m_DataCollector(new CAnnotDataCollector)
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
      m_Scope(bioseq.GetScope()),
      m_DataCollector(new CAnnotDataCollector)
{
    x_Initialize(bioseq, start, stop);
}


inline
size_t CAnnotTypes_CI::x_GetAnnotCount(void) const
{
    return m_AnnotSet.size() +
        (m_DataCollector.get() ? m_DataCollector->m_AnnotMappingSet.size() : 0);
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
    if ( m_DataCollector.get() ) {
        m_DataCollector.reset();
    }
    m_CurAnnot = m_AnnotSet.begin();
    m_Scope = CHeapScope();
    m_TSE_LockSet.clear();
}


CAnnotTypes_CI& CAnnotTypes_CI::operator= (const CAnnotTypes_CI& it)
{
    _ASSERT( !it.m_DataCollector.get() );
    if ( this != &it ) {
        x_Clear();
        // Copy TSE list, set TSE locks
        m_TSE_LockSet = it.m_TSE_LockSet;
        static_cast<SAnnotSelector&>(*this) = it;
        m_AnnotSet = it.m_AnnotSet;
        size_t index = it.m_CurAnnot - it.m_AnnotSet.begin();
        _ASSERT(index < x_GetAnnotCount());
        m_CurAnnot = m_AnnotSet.begin()+index;
    }
    return *this;
}

void CAnnotTypes_CI::x_Initialize(const CBioseq_Handle& bioseq,
                                  TSeqPos start, TSeqPos stop)
{
    try {
        if ( start == 0 && stop == 0 ) {
            stop = bioseq.GetBioseqLength()-1;
        }
        CHandleRangeMap master_loc;
        master_loc.AddRange(bioseq.GetSeq_id_Handle(),
                            CHandleRange::TRange(start, stop),
                            eNa_strand_unknown);
        x_Initialize(master_loc);
    } catch (...) {
        // clear all members - GCC 3.0.4 does not do it
        x_Clear();
        throw;
    }
}


void CAnnotTypes_CI::x_Initialize(const CHandleRangeMap& master_loc)
{
    try {
        if ( !m_LimitObject ) {
            m_LimitObjectType = eLimit_None;
        }
        if ( m_LimitObjectType != eLimit_None ) {
            x_GetTSE_Info();
        }

        bool found = x_Search(master_loc, 0);
        bool deeper = !(found && m_AdaptiveDepth) &&
            m_ResolveMethod != eResolve_None && m_ResolveDepth > 0;
        if ( deeper ) {
            ITERATE ( CHandleRangeMap::TLocMap, idit, master_loc.GetMap() ) {
                //### Check for eLoadedOnly
                CBioseq_Handle bh = m_Scope->GetBioseqHandle(idit->first);
                if ( !bh ) {
                    if (m_IdResolving == eFailUnresolved) {
                        // resolve by Seq-id only
                        NCBI_THROW(CAnnotException, eFindFailed,
                                   "Cannot resolve master id");
                    }
                    continue; // Do not crash - just skip unresolvable IDs
                }

                CRef<CSeq_loc> master_loc_empty(new CSeq_loc);
                master_loc_empty->SetEmpty(
                    const_cast<CSeq_id&>(*idit->first.GetSeqId()));
                
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
                    found = x_SearchMapped(smit, idit->first,
                                           *master_loc_empty,
                                           idit->second);
                    deeper = !(found && m_AdaptiveDepth);
                    smit.Next(deeper);
                }
            }
        }
        NON_CONST_ITERATE(CAnnotDataCollector::TAnnotMappingSet, amit,
            m_DataCollector->m_AnnotMappingSet) {
            CAnnotObject_Ref annot_ref = amit->first;
            amit->second.Convert(annot_ref, m_FeatProduct);
            m_AnnotSet.push_back(annot_ref);
        }
        m_DataCollector->m_AnnotMappingSet.clear();
        x_Sort();
        Rewind();
        m_DataCollector.reset();
    }
    catch (...) {
        // clear all members - GCC 3.0.4 does not do it
        x_Clear();
        throw;
    }
}


void CAnnotTypes_CI::x_Initialize(void)
{
    try {
        // Limit must be set, resolving is obsolete
        _ASSERT(m_LimitObjectType != eLimit_None);
        _ASSERT(m_LimitObject);
        _ASSERT(m_ResolveMethod == eResolve_None);
        x_GetTSE_Info();

        x_SearchAll();
        x_Sort();
        Rewind();
        m_DataCollector.reset();
    }
    catch (...) {
        // clear all members - GCC 3.0.4 does not do it
        x_Clear();
        throw;
    }
}


void CAnnotTypes_CI::x_Sort(void)
{
    _ASSERT(m_DataCollector->m_AnnotMappingSet.empty());
    switch ( m_SortOrder ) {
    case eSortOrder_Normal:
        if ( GetAnnotChoice() == CSeq_annot::C_Data::e_Ftable ) {
            sort(m_AnnotSet.begin(), m_AnnotSet.end(),
                 CAnnotObject_Ref_Less<CFeat_Less>());
        }
        else {
            sort(m_AnnotSet.begin(), m_AnnotSet.end(),
                 CAnnotObject_Ref_Less<CAnnotObject_Less>());
        }
        break;
    case eSortOrder_Reverse:
        if ( GetAnnotChoice() == CSeq_annot::C_Data::e_Ftable ) {
            sort(m_AnnotSet.begin(), m_AnnotSet.end(),
                 CAnnotObject_Ref_Reverse_Less<CFeat_Less>());
        }
        else {
            sort(m_AnnotSet.begin(), m_AnnotSet.end(),
                 CAnnotObject_Ref_Reverse_Less<CAnnotObject_Less>());
        }
        break;
    default:
        // do nothing
        break;
    }
}


inline
bool
CAnnotTypes_CI::x_MatchLimitObject(const CAnnotObject_Info& annot_info) const
{
    if ( m_LimitObjectType == eLimit_Entry ) {
        const CSeq_entry_Info* entry = &annot_info.GetSeq_entry_Info();
        _ASSERT(m_DataCollector->m_LimitObjectInfo);
        while ( entry ) {
            if ( entry == &*m_DataCollector->m_LimitObjectInfo ) {
                return true;
            }
            entry = entry->GetParentSeq_entry_Info();
        }
        return false;
    }
    else if ( m_LimitObjectType == eLimit_Annot ) {
        return &annot_info.GetSeq_annot_Info() ==
            &*m_DataCollector->m_LimitObjectInfo;
    }
    return true;
}


inline
bool CAnnotTypes_CI::x_MatchType(const CAnnotObject_Info& annot_info) const
{
    if ( GetAnnotChoice() != CSeq_annot::C_Data::e_not_set ) {
#ifdef _DEBUG
        if ( GetAnnotChoice() != annot_info.GetAnnotType() ) {
            LOG_POST("invalid annot-choice: " <<
                     annot_info.GetAnnotType() << " != " << GetAnnotChoice());
            return false;
        }
#endif
        if ( GetAnnotChoice() == CSeq_annot::C_Data::e_Ftable ) {
            if ( GetFeatSubtype() != CSeqFeatData::eSubtype_any ) {
#ifdef _DEBUG
                if ( GetFeatSubtype() != annot_info.GetFeatSubtype() ) {
                    LOG_POST("invalid feat-subtype: " <<
                             annot_info.GetFeatSubtype() << " != " <<
                             GetFeatSubtype());
                    return false;
                }
#endif
            }
            else if ( GetFeatChoice() != CSeqFeatData::e_not_set ) {
#ifdef _DEBUG
                if ( GetFeatChoice() != annot_info.GetFeatType() ) {
                    LOG_POST("invalid feat-choice: " <<
                             annot_info.GetFeatType() << " != " <<
                             GetFeatChoice());
                    return false;
                }
#endif
            }
        }
    }
    return true;
}


inline
bool CAnnotTypes_CI::x_NeedSNPs(void) const
{
    if ( GetAnnotChoice() != CSeq_annot::C_Data::e_not_set ) {
        if ( GetAnnotChoice() != CSeq_annot::C_Data::e_Ftable ) {
            return false;
        }
        if ( GetFeatSubtype() != CSeqFeatData::eSubtype_any ) {
            if ( GetFeatSubtype() != CSeqFeatData::eSubtype_variation ) {
                return false;
            }
        }
        else if ( GetFeatChoice() != CSeqFeatData::e_not_set ) {
            if ( GetFeatChoice() != CSeqFeatData::e_Imp ) {
                return false;
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


void CAnnotTypes_CI::x_GetTSE_Info(void)
{
    // only one TSE is needed
    _ASSERT(m_TSE_LockSet.empty());
    _ASSERT(m_LimitObjectType != eLimit_None);
    _ASSERT(m_LimitObject);
    
    TTSE_Lock tse_info;
    switch ( m_LimitObjectType ) {
    case eLimit_TSE:
    {
        CConstRef<CTSE_Info> info =
            m_Scope->GetTSE_Info(static_cast<const CSeq_entry&>
                                 (*m_LimitObject));
        if ( !info ) {
            NCBI_THROW(CAnnotException, eLimitError,
                       "CAnnotTypes_CI::x_GetTSE_Info: "
                       "unknown top level Seq-entry");
        }
        tse_info = info;
        m_DataCollector->m_LimitObjectInfo.Reset(info.GetPointer());
        break;
    }
    case eLimit_Entry:
    {
        CConstRef<CSeq_entry_Info> info =
            m_Scope->GetSeq_entry_Info(static_cast<const CSeq_entry&>
                                       (*m_LimitObject));
        if ( !info ) {
            NCBI_THROW(CAnnotException, eLimitError,
                       "CAnnotTypes_CI::x_GetTSE_Info: "
                       "unknown Seq-entry");
        }
        tse_info.Reset(&info->GetTSE_Info());
        m_DataCollector->m_LimitObjectInfo.Reset(info.GetPointer());
        break;
    }
    case eLimit_Annot:
    {
        CConstRef<CSeq_annot_Info> info =
            m_Scope->GetSeq_annot_Info(static_cast<const CSeq_annot&>
                                       (*m_LimitObject));
        if ( !info ) {
            NCBI_THROW(CAnnotException, eLimitError,
                       "CAnnotTypes_CI::x_GetTSE_Info: "
                       "unknown Seq-annot");
        }
        tse_info.Reset(&info->GetTSE_Info());
        m_DataCollector->m_LimitObjectInfo.Reset(info.GetPointer());
        break;
    }
    default:
        // no limit object -> do nothing
        break;
    }
    _ASSERT(m_DataCollector->m_LimitObjectInfo);
    _ASSERT(tse_info);
    tse_info->UpdateAnnotIndex();
    //if ( !IsSetAnnotsNames() || x_MatchAnnotName(*tse_info) ) {
        m_TSE_LockSet.insert(tse_info);
    //}
}


bool CAnnotTypes_CI::x_Search(const CSeq_id_Handle& id,
                              const CBioseq_Handle& bh,
                              const CHandleRange& hr,
                              CSeq_loc_Conversion* cvt)
{
    if ( cvt )
        cvt->SetSrcId(id);

    bool found = false;
    if ( m_LimitObjectType == eLimit_None ) {
        // any data source
        CConstRef<CScope_Impl::TAnnotRefSet> tse_set;
        if ( bh ) {
            tse_set = m_Scope->GetTSESetWithAnnots(bh);
        }
        else {
            tse_set = m_Scope->GetTSESetWithAnnots(id);
        }
        if ( tse_set ) {
            found = x_Search(*tse_set, id, hr, cvt) || found;
        }
    }
    else {
        found = x_Search(m_TSE_LockSet, id, hr, cvt) || found;
    }
    return found;
}


static CSeqFeatData::ESubtype s_DefaultAdaptiveTriggers[] = {
    CSeqFeatData::eSubtype_gene,
    CSeqFeatData::eSubtype_cdregion,
    CSeqFeatData::eSubtype_mRNA
};


bool CAnnotTypes_CI::x_Search(const TTSE_LockSet& tse_set,
                              const CSeq_id_Handle& id,
                              const CHandleRange& hr,
                              CSeq_loc_Conversion* cvt)
{
    bool found = false;
    ITERATE ( TTSE_LockSet, tse_it, tse_set ) {
        const CTSE_Info& tse = **tse_it;

        CTSE_Info::TAnnotReadLockGuard guard(tse.m_AnnotObjsLock);

        // Skip excluded TSEs
        if ( ExcludedTSE(tse.GetTSE()) ) {
            continue;
        }

        if ( m_AdaptiveDepth && tse.ContainsSeqid(id) ) {
            const SIdAnnotObjs* objs = tse.x_GetUnnamedIdObjects(id);
            if ( objs ) {
                vector<char> indexes;
                if ( m_AdaptiveTriggers.empty() ) {
                    const size_t count =
                        sizeof(s_DefaultAdaptiveTriggers)/
                        sizeof(s_DefaultAdaptiveTriggers[0]);
                    for ( int i = count - 1; i >= 0; --i ) {
                        CSeqFeatData::ESubtype subtype =
                            s_DefaultAdaptiveTriggers[i];
                        size_t index = CTSE_Info::x_GetSubtypeIndex(subtype);
                        if ( index ) {
                            indexes.resize(max(indexes.size(), index + 1));
                            indexes[index] = 1;
                        }
                    }
                }
                else {
                    ITERATE ( TAdaptiveTriggers, it, m_AdaptiveTriggers ) {
                        pair<size_t, size_t> idxs =
                            CTSE_Info::x_GetIndexRange(*it);
                        indexes.resize(max(indexes.size(), idxs.second));
                        for ( size_t i = idxs.first; i < idxs.second; ++i ) {
                            indexes[i] = 1;
                        }
                    }
                }
                for ( size_t index = 0; index < indexes.size(); ++index ) {
                    if ( !indexes[index] ) {
                        continue;
                    }
                    if ( index >= objs->m_AnnotSet.size() ) {
                        break;
                    }
                    if ( !objs->m_AnnotSet[index].empty() ) {
                        found = true;
                        break;
                    }
                }
            }
        }
        
        if ( !m_IncludeAnnotsNames.empty() ) {
            // only 'included' annots
            ITERATE ( TAnnotsNames, iter, m_IncludeAnnotsNames ) {
                _ASSERT(!ExcludedAnnotName(*iter)); // consistency check
                const SIdAnnotObjs* objs = tse.x_GetIdObjects(*iter, id);
                if ( objs ) {
                    x_Search(tse, objs, guard, *iter, id, hr, cvt);
                }
            }
        }
        else {
            // all annots, skipping 'excluded'
            ITERATE (CTSE_Info::TNamedAnnotObjs, iter, tse.m_NamedAnnotObjs) {
                if ( ExcludedAnnotName(iter->first) ) {
                    continue;
                }
                const SIdAnnotObjs* objs =
                    tse.x_GetIdObjects(iter->second, id);
                if ( objs ) {
                    x_Search(tse, objs, guard, iter->first, id, hr, cvt);
                }
            }
        }
    }
    return found;
}


bool CAnnotTypes_CI::x_AddObjectMapping(CAnnotObject_Ref& object_ref,
                                        CSeq_loc_Conversion* cvt)
{
    _ASSERT( cvt->IsPartial() );
    object_ref.ResetLocation();
    CSeq_loc_Conversion_Set& mapping_set =
        m_DataCollector->m_AnnotMappingSet[object_ref];
    mapping_set.SetScope(m_Scope);
    CRef<CSeq_loc_Conversion> cvt_copy(new CSeq_loc_Conversion(*cvt));
    mapping_set.Add(*cvt_copy);
    return x_GetAnnotCount() >= m_MaxSize;
}


inline
bool CAnnotTypes_CI::x_AddObject(CAnnotObject_Ref& object_ref)
{
    m_AnnotSet.push_back(object_ref);
    return x_GetAnnotCount() >= m_MaxSize;
}


inline
bool CAnnotTypes_CI::x_AddObject(CAnnotObject_Ref& object_ref,
                                 CSeq_loc_Conversion* cvt)
{
    return ( cvt && cvt->IsPartial() )?
        x_AddObjectMapping(object_ref, cvt): x_AddObject(object_ref);
}


void CAnnotTypes_CI::x_Search(const CTSE_Info& tse,
                              const SIdAnnotObjs* objs,
                              CReadLockGuard& guard,
                              const CAnnotName& annot_name,
                              const CSeq_id_Handle& id,
                              const CHandleRange& hr,
                              CSeq_loc_Conversion* cvt)
{
    _ASSERT(objs);

    CHandleRange::TRange range = hr.GetOverlappingRange();

    pair<size_t, size_t> idxs = CTSE_Info::x_GetIndexRange(*this, *objs);
    if ( idxs.first < idxs.second ) {
        m_TSE_LockSet.insert(ConstRef(&tse));
    }

    for ( size_t index = idxs.first; index < idxs.second; ++index ) {
        size_t start_size = m_AnnotSet.size(); // for rollback
        
        const CTSE_Info::TRangeMap& rmap = objs->m_AnnotSet[index];
        if ( rmap.empty() ) {
            continue;
        }

        for ( CTSE_Info::TRangeMap::const_iterator aoit(rmap.begin(range));
              aoit; ++aoit ) {
            const CAnnotObject_Info& annot_info =
                *aoit->second.m_AnnotObject_Info;

            _ASSERT(x_MatchType(annot_info));

            if ( annot_info.IsChunkStub() ) {
                const CTSE_Chunk_Info& chunk = annot_info.GetChunk_Info();
                if ( chunk.NotLoaded() ) {
                    // New annot objects are to be loaded,
                    // so we'll need to restart scan of current range.

                    // Forget already found object
                    // as they will be found again:
                    m_AnnotSet.resize(start_size);

                    CAnnotName name(annot_name);

                    // Release lock for tse update:
                    guard.Release();
                        
                    // Load the stub:
                    const_cast<CTSE_Chunk_Info&>(chunk).Load();

                    // Acquire the lock again:
                    guard.Guard(tse.m_AnnotObjsLock);

                    // Reget range map pointer as it may change:
                    objs = tse.x_GetIdObjects(name, id);
                    _ASSERT(objs);

                    // Restart this index again:
                    --index;
                    break;
                }
                else {
                    // Skip chunk stub
                    continue;
                }
            }

            if ( !x_MatchLimitObject(annot_info) ) {
                continue;
            }
                
            if ( !x_MatchRange(hr, aoit->first, aoit->second) ) {
                continue;
            }
                
            CAnnotObject_Ref annot_ref(annot_info);
            if ( cvt ) {
                cvt->Convert(annot_ref, m_FeatProduct);
            }
            else {
                annot_ref.SetAnnotObjectRange(aoit->first, m_FeatProduct);
            }
            if ( x_AddObject(annot_ref, cvt) ) {
                return;
            }
        }
    }

    if ( x_NeedSNPs() ) {
        ITERATE ( CTSE_Info::TSNPSet, snp_annot_it, objs->m_SNPSet ) {
            const CSeq_annot_SNP_Info& snp_annot = **snp_annot_it;
            CSeq_annot_SNP_Info::const_iterator snp_it =
                snp_annot.FirstIn(range);
            if ( snp_it != snp_annot.end() ) {
                m_TSE_LockSet.insert(ConstRef(&tse));
                TSeqPos index = snp_it - snp_annot.begin() - 1;
                do {
                    ++index;
                    const SSNP_Info& snp = *snp_it;
                    if ( snp.NoMore(range) ) {
                        break;
                    }
                    if ( snp.NotThis(range) ) {
                        continue;
                    }

                    CAnnotObject_Ref annot_ref(snp_annot, index);
                    annot_ref.SetSNP_Point(snp, cvt);
                    if ( x_AddObject(annot_ref, cvt) ) {
                        return;
                    }
                } while ( ++snp_it != snp_annot.end() );
            }
        }
    }
}


bool CAnnotTypes_CI::x_Search(const CHandleRangeMap& loc,
                              CSeq_loc_Conversion* cvt)
{
    bool found = false;
    ITERATE ( CHandleRangeMap, idit, loc ) {
        if ( idit->second.Empty() ) {
            continue;
        }

        bool have_main = false;
        CConstRef<CSynonymsSet> syns = m_Scope->GetSynonyms(idit->first);
        if ( !syns ) {
            if ( m_IdResolving == eFailUnresolved ) {
                NCBI_THROW(CAnnotException, eFindFailed,
                           "Cannot find id synonyms");
            }
        }
        else {
            ITERATE ( CSynonymsSet, synit, *syns ) {
                CSeq_id_Handle idh = CSynonymsSet::GetSeq_id_Handle(synit);
                if ( !have_main ) {
                    have_main = idit->first == idh;
                }
                found = x_Search(idh, CSynonymsSet::GetBioseqHandle(synit),
                                 idit->second, cvt)  ||  found;
            }
        }
        if ( !have_main ) {
            found = x_Search(idit->first, CBioseq_Handle(),
                             idit->second, cvt)  ||  found;
        }
    }
    return found;
}


void CAnnotTypes_CI::x_SearchAll(void)
{
    _ASSERT(m_LimitObjectType != eLimit_None);
    _ASSERT(m_LimitObject);
    _ASSERT(m_DataCollector->m_LimitObjectInfo);
    if ( m_TSE_LockSet.empty() ) {
        // data source name not matched
        return;
    }
    switch ( m_LimitObjectType ) {
    case eLimit_TSE:
        x_SearchAll(dynamic_cast<const CTSE_Info&>(
            *m_DataCollector->m_LimitObjectInfo));
        break;
    case eLimit_Entry:
        x_SearchAll(dynamic_cast<const CSeq_entry_Info&>(
            *m_DataCollector->m_LimitObjectInfo));
        break;
    case eLimit_Annot:
        x_SearchAll(dynamic_cast<const CSeq_annot_Info&>(
            *m_DataCollector->m_LimitObjectInfo));
        break;
    default:
        // no limit object -> do nothing
        break;
    }
}


void CAnnotTypes_CI::x_SearchAll(const CSeq_entry_Info& entry_info)
{
    // Collect all annotations from the entry
    ITERATE( CSeq_entry_Info::TAnnots, ait, entry_info.m_Annots ) {
        x_SearchAll(**ait);
        if ( x_GetAnnotCount() >= m_MaxSize )
            return;
    }
    // Collect annotations from all children
    ITERATE( CSeq_entry_Info::TEntries, cit, entry_info.m_Entries ) {
        x_SearchAll(**cit);
        if ( x_GetAnnotCount() >= m_MaxSize )
            return;
    }
}


void CAnnotTypes_CI::x_SearchAll(const CSeq_annot_Info& annot_info)
{
    // Collect all annotations from the annot
    ITERATE ( SAnnotObjects_Info::TObjectInfos, aoit,
              annot_info.m_ObjectInfos.GetInfos() ) {
        if ( !x_MatchType(*aoit) ) {
            continue;
        }
        CAnnotObject_Ref annot_ref(*aoit);
        if ( x_AddObject(annot_ref) ) {
            return;
        }
    }

    if ( x_NeedSNPs() && annot_info.x_HaveSNP_annot_Info() ) {
        const CSeq_annot_SNP_Info& snp_annot =
            annot_info.x_GetSNP_annot_Info();
        TSeqPos index = 0;
        ITERATE ( CSeq_annot_SNP_Info, snp_it, snp_annot ) {
            const SSNP_Info& snp = *snp_it;
            CAnnotObject_Ref annot_ref(snp_annot, index);
            annot_ref.SetSNP_Point(snp, 0);
            if ( x_AddObject(annot_ref) ) {
                return;
            }
            ++index;
        }
    }
}


bool CAnnotTypes_CI::x_SearchMapped(const CSeqMap_CI& seg,
                                    const CSeq_id_Handle& master_id,
                                    CSeq_loc& master_loc_empty,
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
                    strand = Reverse(strand);
                    range.Set(shift - range.GetTo(), shift - range.GetFrom());
                }
                hr.AddRange(range, strand);
            }
        }
        if ( hr.Empty() )
            return false;
    }}

    if (m_NoMapping) {
        return x_Search(ref_loc, 0);
    }
    else {
        CRef<CSeq_loc_Conversion> cvt(new CSeq_loc_Conversion(master_id,
                                                              master_loc_empty,
                                                              seg,
                                                              ref_id,
                                                              m_Scope));
        return x_Search(ref_loc, &*cvt);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.104  2004/01/22 20:10:40  vasilche
* 1. Splitted ID2 specs to two parts.
* ID2 now specifies only protocol.
* Specification of ID2 split data is moved to seqsplit ASN module.
* For now they are still reside in one resulting library as before - libid2.
* As the result split specific headers are now in objects/seqsplit.
* 2. Moved ID2 and ID1 specific code out of object manager.
* Protocol is processed by corresponding readers.
* ID2 split parsing is processed by ncbi_xreader library - used by all readers.
* 3. Updated OBJMGR_LIBS correspondingly.
*
* Revision 1.103  2003/11/26 17:55:56  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.102  2003/11/13 19:12:53  grichenk
* Added possibility to exclude TSEs from annotations request.
*
* Revision 1.101  2003/11/10 18:11:03  grichenk
* Moved CSeq_loc_Conversion_Set to seq_loc_cvt
*
* Revision 1.100  2003/11/05 00:33:53  ucko
* Un-inline CSeq_loc_Conversion_Set::Add due to use before definition.
*
* Revision 1.99  2003/11/04 21:10:01  grichenk
* Optimized feature mapping through multiple segments.
* Fixed problem with CAnnotTypes_CI not releasing scope
* when exception is thrown from constructor.
*
* Revision 1.98  2003/11/04 16:21:37  grichenk
* Updated CAnnotTypes_CI to map whole features instead of splitting
* them by sequence segments.
*
* Revision 1.97  2003/10/28 14:46:29  vasilche
* Fixed wrong _ASSERT().
*
* Revision 1.96  2003/10/27 20:07:10  vasilche
* Started implementation of full annotations' mapping.
*
* Revision 1.95  2003/10/10 12:47:24  dicuccio
* Fixed off-by-one error in x_Search() - allocate correct size for array
*
* Revision 1.94  2003/10/09 20:20:58  vasilche
* Added possibility to include and exclude Seq-annot names to annot iterator.
* Fixed adaptive search. It looked only on selected set of annot names before.
*
* Revision 1.93  2003/10/07 13:43:23  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.92  2003/09/30 16:22:02  vasilche
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
* Revision 1.91  2003/09/16 14:21:47  grichenk
* Added feature indexing and searching by subtype.
*
* Revision 1.90  2003/09/12 17:43:15  dicuccio
* Replace _ASSERT() with handled check in x_Search() (again...)
*
* Revision 1.89  2003/09/12 16:57:52  dicuccio
* Revert previous change
*
* Revision 1.88  2003/09/12 16:55:31  dicuccio
* Temporarily disable assertion in CAnnotTypes_CI::x_Search()
*
* Revision 1.87  2003/09/12 15:50:10  grichenk
* Updated adaptive-depth triggering
*
* Revision 1.86  2003/09/11 17:45:07  grichenk
* Added adaptive-depth option to annot-iterators.
*
* Revision 1.85  2003/09/05 17:29:40  grichenk
* Structurized Object Manager exceptions
*
* Revision 1.84  2003/09/03 19:59:01  grichenk
* Initialize m_MappedIndex to 0
*
* Revision 1.83  2003/08/27 14:29:52  vasilche
* Reduce object allocations in feature iterator.
*
* Revision 1.82  2003/08/22 14:58:57  grichenk
* Added NoMapping flag (to be used by CAnnot_CI for faster fetching).
* Added GetScope().
*
* Revision 1.81  2003/08/15 19:19:16  vasilche
* Fixed memory leak in string packing hooks.
* Fixed processing of 'partial' flag of features.
* Allow table packing of non-point SNP.
* Allow table packing of SNP with long alleles.
*
* Revision 1.80  2003/08/14 20:05:19  vasilche
* Simple SNP features are stored as table internally.
* They are recreated when needed using CFeat_CI.
*
* Revision 1.79  2003/08/04 17:03:01  grichenk
* Added constructors to iterate all annotations from a
* seq-entry or seq-annot.
*
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
