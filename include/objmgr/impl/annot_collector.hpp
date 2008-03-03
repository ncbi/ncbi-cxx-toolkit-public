#ifndef ANNOT_COLLECTOR__HPP
#define ANNOT_COLLECTOR__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman, Eugene Vasilchenko
*
* File Description:
*   Annotation collector for annot iterators
*
*/

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/annot_selector.hpp>
#include <objmgr/tse_handle.hpp>
#include <objmgr/impl/heap_scope.hpp>
#include <objmgr/impl/seq_annot_info.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include <set>
#include <vector>
#include <memory>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)

class CHandleRangeMap;
class CHandleRange;
struct SAnnotObject_Index;
class CSeq_annot_SNP_Info;
struct SSNP_Info;
struct SIdAnnotObjs;
class CSeq_loc_Conversion;
class CSeq_loc_Conversion_Set;
class CSeq_feat_Handle;
class CAnnot_CI;
class CSeqMap_CI;

class NCBI_XOBJMGR_EXPORT CAnnotMapping_Info
{
public:
    enum FMappedFlags {
        fMapped_Partial      = 1<<0,
        fMapped_Product      = 1<<1,
        fMapped_Seq_point    = 1<<2,
        fMapped_Partial_from = 1<<3,
        fMapped_Partial_to   = 1<<4
    };

    enum EMappedObjectType {
        eMappedObjType_not_set,
        eMappedObjType_Seq_loc,
        eMappedObjType_Seq_id,
        eMappedObjType_Seq_feat,
        eMappedObjType_Seq_align,
        eMappedObjType_Seq_loc_Conv_Set
    };

    CAnnotMapping_Info(void);
    CAnnotMapping_Info(Int1 mapped_flags, Int1 mapped_type, Int1 mapped_strand);

    void Reset(void);

    Int1 GetMappedFlags(void) const;
    bool IsMapped(void) const;
    bool IsPartial(void) const;
    bool IsProduct(void) const;
    EMappedObjectType GetMappedObjectType(void) const;

    void SetPartial(bool value);
    void SetProduct(bool product);

    bool IsMappedLocation(void) const;
    bool IsMappedProduct(void) const;
    bool IsMappedPoint(void) const;

    typedef CRange<TSeqPos> TRange;

    TSeqPos GetFrom(void) const;
    TSeqPos GetToOpen(void) const;
    const TRange& GetTotalRange(void) const;
    void SetTotalRange(const TRange& range);

    ENa_strand GetMappedStrand(void) const;
    void SetMappedStrand(ENa_strand strand);

    const CSeq_loc& GetMappedSeq_loc(void) const;
    const CSeq_id& GetMappedSeq_id(void) const;
    const CSeq_feat& GetMappedSeq_feat(void) const;
    const CSeq_align& GetMappedSeq_align(const CSeq_align& orig) const;

    void SetMappedSeq_loc(CSeq_loc& loc);
    void SetMappedSeq_loc(CSeq_loc* loc);
    void SetMappedSeq_id(CSeq_id& id);
    void SetMappedPoint(bool point);
    void SetMappedPartial_from(void);
    void SetMappedPartial_to(void);
    void SetMappedSeq_id(CSeq_id& id, bool point);
    void SetMappedSeq_feat(CSeq_feat& feat);
    void SetMappedSeq_align(CSeq_align* align);
    void SetMappedSeq_align_Cvts(CSeq_loc_Conversion_Set& cvts);

    bool MappedSeq_locNeedsUpdate(void) const;
    void UpdateMappedSeq_loc(CRef<CSeq_loc>& loc) const;
    void UpdateMappedSeq_loc(CRef<CSeq_loc>&      loc,
                             CRef<CSeq_point>&    pnt_ref,
                             CRef<CSeq_interval>& int_ref) const;

    // Copy non-modified members from original feature
    // (all except partial flag and location/product, depending on mode.
    void InitializeMappedSeq_feat(const CSeq_feat& src, CSeq_feat& dst) const;

    void SetAnnotObjectRange(const TRange& range, bool product);
    
    void Swap(CAnnotMapping_Info& info);

private:
    CRef<CObject>           m_MappedObject; // master sequence coordinates
    TRange                  m_TotalRange;
    Int1                    m_MappedFlags; // partial, product
    Int1                    m_MappedObjectType;
    Int1                    m_MappedStrand;
};


class NCBI_XOBJMGR_EXPORT CAnnotObject_Ref
{
public:
    typedef CRange<TSeqPos> TRange;
    typedef Int4           TAnnotIndex;
    enum {
        kSNPTableBit    = 0x80000000,
        kAnnotIndexMask = 0x7fffffff
    };

    CAnnotObject_Ref(void);
    CAnnotObject_Ref(const CAnnotObject_Info& object);
    CAnnotObject_Ref(const CSeq_annot_SNP_Info& snp_annot,
                     const SSNP_Info& snp_info,
                     CSeq_loc_Conversion* cvt);

    bool IsRegular(void) const;
    bool IsSNPFeat(void) const;
    bool IsTableFeat(void) const;

    const CSeq_annot_Info& GetSeq_annot_Info(void) const;
    const CSeq_annot_SNP_Info& GetSeq_annot_SNP_Info(void) const;
    TAnnotIndex GetAnnotIndex(void) const;

    const CAnnotObject_Info& GetAnnotObject_Info(void) const;
    const SSNP_Info& GetSNP_Info(void) const;

    bool IsFeat(void) const;
    bool IsGraph(void) const;
    bool IsAlign(void) const;
    const CSeq_feat& GetFeat(void) const;
    const CSeq_graph& GetGraph(void) const;
    const CSeq_align& GetAlign(void) const;

    CAnnotMapping_Info& GetMappingInfo(void) const;

    void ResetLocation(void);
    bool operator<(const CAnnotObject_Ref& ref) const; // sort by object
    bool operator==(const CAnnotObject_Ref& ref) const; // sort by object
    bool operator!=(const CAnnotObject_Ref& ref) const; // sort by object

    void Swap(CAnnotObject_Ref& ref);

private:
    CConstRef<CSeq_annot_Info> m_Seq_annot;   //  4 or  8
    TAnnotIndex                m_AnnotIndex;  //  4 or  4
    mutable CAnnotMapping_Info m_MappingInfo; // 16 or 20
};


class CCreatedFeat_Ref : public CObject
{
public:
    CCreatedFeat_Ref(void);
    ~CCreatedFeat_Ref(void);

    void ResetRefs(void);
    void ReleaseRefsTo(CRef<CSeq_feat>*     feat,
                       CRef<CSeq_loc>*      loc,
                       CRef<CSeq_point>*    point,
                       CRef<CSeq_interval>* interval);
    void ResetRefsFrom(CRef<CSeq_feat>*     feat,
                       CRef<CSeq_loc>*      loc,
                       CRef<CSeq_point>*    point,
                       CRef<CSeq_interval>* interval);

    CConstRef<CSeq_feat> MakeOriginalFeature(const CSeq_feat_Handle& feat_h);
    CConstRef<CSeq_loc>  MakeMappedLocation(const CAnnotMapping_Info& map_info);
    CConstRef<CSeq_feat> MakeMappedFeature(const CSeq_feat_Handle& orig_feat,
                                           const CAnnotMapping_Info& map_info,
                                           CSeq_loc& mapped_location);
private:
    CRef<CSeq_feat>      m_CreatedSeq_feat;
    CRef<CSeq_loc>       m_CreatedSeq_loc;
    CRef<CSeq_point>     m_CreatedSeq_point;
    CRef<CSeq_interval>  m_CreatedSeq_interval;
};


class CSeq_annot_Handle;


class CAnnotMappingCollector;


class NCBI_XOBJMGR_EXPORT CAnnot_Collector : public CObject
{
public:
    typedef SAnnotSelector::TAnnotType TAnnotType;
    typedef vector<CAnnotObject_Ref>   TAnnotSet;

    CAnnot_Collector(CScope& scope);
    ~CAnnot_Collector(void);

private:
    CAnnot_Collector(const CAnnot_Collector&);
    CAnnot_Collector& operator= (const CAnnot_Collector&);


    const TAnnotSet& GetAnnotSet(void) const;
    CScope& GetScope(void) const;
    void SetAnnotHandle(CSeq_annot_Handle& annot_handle,
                        const CAnnotObject_Ref& ref) const;

    const SAnnotSelector& GetSelector(void);
    CScope::EGetBioseqFlag GetGetFlag(void) const;
    bool CanResolveId(const CSeq_id_Handle& idh, const CBioseq_Handle& bh);

    void x_Clear(void);
    void x_Initialize(const SAnnotSelector& selector,
                      const CBioseq_Handle& bioseq,
                      const CRange<TSeqPos>& range,
                      ENa_strand strand);
    void x_Initialize(const SAnnotSelector& selector,
                      const CHandleRangeMap& master_loc);
    void x_Initialize(const SAnnotSelector& selector);
    void x_GetTSE_Info(void);
    bool x_SearchMapped(const CSeqMap_CI&     seg,
                        CSeq_loc&             master_loc_empty,
                        const CSeq_id_Handle& master_id,
                        const CHandleRange&   master_hr);
    bool x_SearchLoc(const CHandleRangeMap& loc,
                     CSeq_loc_Conversion*   cvt,
                     const CTSE_Handle*     using_tse,
                     bool top_level = false);
    bool x_SearchTSE(const CTSE_Handle&    tse,
                     const CSeq_id_Handle& id,
                     const CHandleRange&   hr,
                     CSeq_loc_Conversion*  cvt);
    void x_SearchObjects(const CTSE_Handle&    tse,
                         const SIdAnnotObjs*   objs,
                         CMutexGuard&          guard,
                         const CAnnotName&     name,
                         const CSeq_id_Handle& id,
                         const CHandleRange&   hr,
                         CSeq_loc_Conversion*  cvt);
    void x_SearchRange(const CTSE_Handle&    tse,
                       const SIdAnnotObjs*   objs,
                       CMutexGuard&          guard,
                       const CAnnotName&     name,
                       const CSeq_id_Handle& id,
                       const CHandleRange&   hr,
                       CSeq_loc_Conversion*  cvt,
                       size_t                from_idx,
                       size_t                to_idx);
    void x_SearchAll(void);
    void x_SearchAll(const CSeq_entry_Info& entry_info);
    void x_SearchAll(const CSeq_annot_Info& annot_info);
    void x_Sort(void);
    
    void x_AddObjectMapping(CAnnotObject_Ref&    object_ref,
                            CSeq_loc_Conversion* cvt,
                            unsigned int         loc_index);
    void x_AddObject(CAnnotObject_Ref& object_ref);
    void x_AddObject(CAnnotObject_Ref&    object_ref,
                     CSeq_loc_Conversion* cvt,
                     unsigned int         loc_index);

    // Release all locked resources TSE etc
    void x_ReleaseAll(void);

    bool x_NeedSNPs(void) const;
    bool x_MatchLimitObject(const CAnnotObject_Info& annot_info) const;
    bool x_MatchRange(const CHandleRange&       hr,
                      const CRange<TSeqPos>&    range,
                      const SAnnotObject_Index& index) const;
    bool x_MatchLocIndex(const SAnnotObject_Index& index) const;

    bool x_NoMoreObjects(void) const;

    void x_AddPostMappings(void);
    void x_AddTSE(const CTSE_Handle& tse);
    void x_AddAnnot(const CAnnotObject_Ref& ref);
    void x_CreateAnnotHandle(CSeq_annot_Handle& annot_handle,
                             const CSeq_annot_Info* info) const;

    // Set of processed annot-locs to avoid duplicates
    typedef set< CConstRef<CSeq_loc> >   TAnnotLocsSet;
    typedef map<const CTSE_Info*, CTSE_Handle> TTSE_LockMap;
    typedef vector<CSeq_annot_Handle> TAnnotLocks;
    typedef SAnnotSelector::TAnnotTypesBitset TAnnotTypesBitset;

    const SAnnotSelector*            m_Selector;
    CHeapScope                       m_Scope;
    // TSE set to keep all the TSEs locked
    TTSE_LockMap                     m_TSE_LockMap;
    CSeq_annot_Handle                m_FirstAnnotLock;
    TAnnotLocks                      m_AnnotLocks;
    auto_ptr<CAnnotMappingCollector> m_MappingCollector;
    // Set of all the annotations found
    TAnnotSet                        m_AnnotSet;

    // Temporary objects to be re-used by iterators
    CRef<CCreatedFeat_Ref>  m_CreatedOriginal;
    CRef<CCreatedFeat_Ref>  m_CreatedMapped;
    auto_ptr<TAnnotLocsSet> m_AnnotLocsSet;
    TAnnotTypesBitset       m_TypesBitset;

    friend class CAnnotTypes_CI;
    friend class CMappedFeat;
    friend class CMappedGraph;
    friend class CAnnot_CI;
};


/////////////////////////////////////////////////////////////////////////////
// CAnnotMapping_Info
/////////////////////////////////////////////////////////////////////////////


inline
CAnnotMapping_Info::CAnnotMapping_Info(void)
    : m_MappedFlags(0),
      m_MappedObjectType(eMappedObjType_not_set),
      m_MappedStrand(eNa_strand_unknown)
{
}


inline
CAnnotMapping_Info::CAnnotMapping_Info(Int1 mapped_flags,
                                       Int1 mapped_type,
                                       Int1 mapped_strand)
    : m_MappedFlags(mapped_flags),
      m_MappedObjectType(mapped_type),
      m_MappedStrand(mapped_strand)
{
}


inline
TSeqPos CAnnotMapping_Info::GetFrom(void) const
{
    return m_TotalRange.GetFrom();
}


inline
TSeqPos CAnnotMapping_Info::GetToOpen(void) const
{
    return m_TotalRange.GetToOpen();
}


inline
const CAnnotMapping_Info::TRange&
CAnnotMapping_Info::GetTotalRange(void) const
{
    return m_TotalRange;
}


inline
void CAnnotMapping_Info::SetTotalRange(const TRange& range)
{
    m_TotalRange = range;
}


inline
bool CAnnotMapping_Info::IsPartial(void) const
{
    return (m_MappedFlags & fMapped_Partial) != 0;
}


inline
bool CAnnotMapping_Info::IsProduct(void) const
{
    return (m_MappedFlags & fMapped_Product) != 0;
}


inline
ENa_strand CAnnotMapping_Info::GetMappedStrand(void) const
{
    return ENa_strand(m_MappedStrand);
}


inline
void CAnnotMapping_Info::SetMappedStrand(ENa_strand strand)
{
    _ASSERT(IsMapped());
    m_MappedStrand = strand;
}


inline
Int1 CAnnotMapping_Info::GetMappedFlags(void) const
{
    return m_MappedFlags;
}


inline
CAnnotMapping_Info::EMappedObjectType
CAnnotMapping_Info::GetMappedObjectType(void) const
{
    return EMappedObjectType(m_MappedObjectType);
}


inline
bool CAnnotMapping_Info::IsMapped(void) const
{
    _ASSERT((GetMappedObjectType() == eMappedObjType_not_set) ==
            !m_MappedObject);
    return GetMappedObjectType() != eMappedObjType_not_set;
}


inline
bool CAnnotMapping_Info::MappedSeq_locNeedsUpdate(void) const
{
    return GetMappedObjectType() == eMappedObjType_Seq_id;
}


inline
bool CAnnotMapping_Info::IsMappedLocation(void) const
{
    return IsMapped() && !IsProduct();
}


inline
bool CAnnotMapping_Info::IsMappedProduct(void) const
{
    return IsMapped() && IsProduct();
}


inline
const CSeq_loc& CAnnotMapping_Info::GetMappedSeq_loc(void) const
{
    if (GetMappedObjectType() == eMappedObjType_Seq_feat) {
        return IsProduct() ? GetMappedSeq_feat().GetProduct()
            : GetMappedSeq_feat().GetLocation();
    }
    _ASSERT(GetMappedObjectType() == eMappedObjType_Seq_loc);
    return static_cast<const CSeq_loc&>(*m_MappedObject);
}


inline
const CSeq_id& CAnnotMapping_Info::GetMappedSeq_id(void) const
{
    _ASSERT(GetMappedObjectType() == eMappedObjType_Seq_id);
    return static_cast<const CSeq_id&>(*m_MappedObject);
}


inline
const CSeq_feat& CAnnotMapping_Info::GetMappedSeq_feat(void) const
{
    _ASSERT(GetMappedObjectType() == eMappedObjType_Seq_feat);
    return static_cast<const CSeq_feat&>(*m_MappedObject);
}


inline
void CAnnotMapping_Info::SetMappedSeq_loc(CSeq_loc* loc)
{
    _ASSERT(!IsMapped());
    m_MappedObject.Reset(loc);
    m_MappedObjectType = loc ?
        eMappedObjType_Seq_loc : eMappedObjType_not_set;
}


inline
void CAnnotMapping_Info::SetMappedSeq_loc(CSeq_loc& loc)
{
    _ASSERT(!IsMapped());
    m_MappedObject.Reset(&loc);
    m_MappedObjectType = eMappedObjType_Seq_loc;
}


inline
void CAnnotMapping_Info::SetMappedSeq_id(CSeq_id& id)
{
    _ASSERT(!IsMapped());
    m_MappedObject.Reset(&id);
    m_MappedObjectType = eMappedObjType_Seq_id;
}


inline
void CAnnotMapping_Info::SetMappedPoint(bool point)
{
    _ASSERT(GetMappedObjectType() == eMappedObjType_Seq_id);
    if ( point ) {
        m_MappedFlags |= fMapped_Seq_point;
    }
    else {
        m_MappedFlags &= ~fMapped_Seq_point;
    }
}


inline
void CAnnotMapping_Info::SetMappedPartial_from(void)
{
    _ASSERT(GetMappedObjectType() == eMappedObjType_Seq_id);
    m_MappedFlags |= fMapped_Partial_from;
}


inline
void CAnnotMapping_Info::SetMappedPartial_to(void)
{
    _ASSERT(GetMappedObjectType() == eMappedObjType_Seq_id);
    m_MappedFlags |= fMapped_Partial_to;
}


inline
bool CAnnotMapping_Info::IsMappedPoint(void) const
{
    _ASSERT(GetMappedObjectType() == eMappedObjType_Seq_id);
    return (m_MappedFlags & fMapped_Seq_point) != 0;
}


inline
void CAnnotMapping_Info::SetMappedSeq_id(CSeq_id& id, bool point)
{
    SetMappedSeq_id(id);
    SetMappedPoint(point);
}


inline
void CAnnotMapping_Info::SetPartial(bool partial)
{
    if ( partial ) {
        m_MappedFlags |= fMapped_Partial;
    }
    else {
        m_MappedFlags &= ~fMapped_Partial;
    }
}


inline
void CAnnotMapping_Info::SetProduct(bool product)
{
    if ( product ) {
        m_MappedFlags |= fMapped_Product;
    }
    else {
        m_MappedFlags &= ~fMapped_Product;
    }
}


inline
void CAnnotMapping_Info::SetAnnotObjectRange(const TRange& range, bool product)
{
    m_TotalRange = range;
    SetProduct(product);
}


inline
void CAnnotMapping_Info::Swap(CAnnotMapping_Info& info)
{
    m_MappedObject.Swap(info.m_MappedObject);
    swap(m_TotalRange, info.m_TotalRange);
    swap(m_MappedFlags, info.m_MappedFlags);
    swap(m_MappedObjectType, info.m_MappedObjectType);
    swap(m_MappedStrand, info.m_MappedStrand);
}


/////////////////////////////////////////////////////////////////////////////
// CAnnotObject_Ref
/////////////////////////////////////////////////////////////////////////////


inline
CAnnotObject_Ref::CAnnotObject_Ref(void)
    : m_AnnotIndex(0)
{
}


inline
CAnnotObject_Ref::TAnnotIndex CAnnotObject_Ref::GetAnnotIndex(void) const
{
    return m_AnnotIndex & kAnnotIndexMask;
}


inline
bool CAnnotObject_Ref::IsRegular(void) const
{
    return (m_AnnotIndex & kSNPTableBit) == 0;
}


inline
bool CAnnotObject_Ref::IsSNPFeat(void) const
{
    return (m_AnnotIndex & kSNPTableBit) != 0;
}


inline
const CSeq_annot_Info& CAnnotObject_Ref::GetSeq_annot_Info(void) const
{
    return *m_Seq_annot;
}


inline
bool CAnnotObject_Ref::operator<(const CAnnotObject_Ref& ref) const
{
    if ( m_Seq_annot != ref.m_Seq_annot ) {
        return m_Seq_annot < ref.m_Seq_annot;
    }
    return m_AnnotIndex < ref.m_AnnotIndex;
}


inline
bool CAnnotObject_Ref::operator==(const CAnnotObject_Ref& ref) const
{
    return (m_Seq_annot == ref.m_Seq_annot &&
            m_AnnotIndex == ref.m_AnnotIndex);
}


inline
bool CAnnotObject_Ref::operator!=(const CAnnotObject_Ref& ref) const
{
    return (m_Seq_annot != ref.m_Seq_annot ||
            m_AnnotIndex != ref.m_AnnotIndex);
}


inline
CAnnotMapping_Info& CAnnotObject_Ref::GetMappingInfo(void) const
{
    return m_MappingInfo;
}


/////////////////////////////////////////////////////////////////////////////
// CAnnot_Collector
/////////////////////////////////////////////////////////////////////////////


inline
const CAnnot_Collector::TAnnotSet&
CAnnot_Collector::GetAnnotSet(void) const
{
    return m_AnnotSet;
}


inline
CScope& CAnnot_Collector::GetScope(void) const
{
    return m_Scope;
}


inline
const SAnnotSelector& CAnnot_Collector::GetSelector(void)
{
    return *m_Selector;
}


inline
CScope::EGetBioseqFlag CAnnot_Collector::GetGetFlag(void) const
{
    switch (m_Selector->m_ResolveMethod) {
    case SAnnotSelector::eResolve_All:
        return CScope::eGetBioseq_All;
    default:
        // Do not load new TSEs
        return CScope::eGetBioseq_Loaded;
    }
}


inline
void CAnnotObject_Ref::Swap(CAnnotObject_Ref& ref)
{
    m_Seq_annot.Swap(ref.m_Seq_annot);
    swap(m_AnnotIndex, ref.m_AnnotIndex);
    m_MappingInfo.Swap(ref.m_MappingInfo);
}

END_SCOPE(objects)
END_NCBI_SCOPE


BEGIN_STD_SCOPE
inline
void swap(NCBI_NS_NCBI::objects::CAnnotMapping_Info& info1,
		  NCBI_NS_NCBI::objects::CAnnotMapping_Info& info2)
{
    info1.Swap(info2);
}


inline
void swap(NCBI_NS_NCBI::objects::CAnnotObject_Ref& ref1,
		  NCBI_NS_NCBI::objects::CAnnotObject_Ref& ref2)
{
    ref1.Swap(ref2);
}

END_STD_SCOPE

#endif  // ANNOT_COLLECTOR__HPP
