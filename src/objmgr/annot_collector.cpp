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
*   Annotation collector for annot iterators
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/impl/annot_collector.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/seq_feat_handle.hpp>
#include <objmgr/seq_map_ci.hpp>
#include <objmgr/impl/annot_object.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/annot_type_index.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/impl/synonyms.hpp>
#include <objmgr/impl/seq_loc_cvt.hpp>
#include <objmgr/impl/seq_align_mapper.hpp>
#include <objmgr/impl/snp_annot_info.hpp>
#include <objmgr/impl/seq_table_info.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/error_codes.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objects/general/User_object.hpp>

#include <serial/typeinfo.hpp>
#include <serial/objostr.hpp>
#include <serial/objostrasn.hpp>
#include <serial/serial.hpp>
#include <serial/serialutil.hpp>

#include <algorithm>
#include <typeinfo>


#define NCBI_USE_ERRCODE_X   ObjMgr_AnnotCollect

BEGIN_NCBI_SCOPE

NCBI_DEFINE_ERR_SUBCODE_X(1);

BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
// CAnnotMapping_Info
/////////////////////////////////////////////////////////////////////////////


void CAnnotMapping_Info::Reset(void)
{
    m_TotalRange = TRange::GetEmpty();
    m_MappedObject.Reset();
    m_MappedObjectType = eMappedObjType_not_set;
    m_MappedStrand = eNa_strand_unknown;
    m_MappedFlags = 0;
}


void CAnnotMapping_Info::SetMappedSeq_align(CSeq_align* align)
{
    _ASSERT(m_MappedObjectType == eMappedObjType_Seq_loc_Conv_Set);
    m_MappedObject.Reset(align);
    m_MappedObjectType =
        align? eMappedObjType_Seq_align: eMappedObjType_not_set;
}


void CAnnotMapping_Info::SetMappedSeq_align_Cvts(CSeq_loc_Conversion_Set& cvts)
{
    _ASSERT(!IsMapped());
    m_MappedObject.Reset(&cvts);
    m_MappedObjectType = eMappedObjType_Seq_loc_Conv_Set;
}


const CSeq_align&
CAnnotMapping_Info::GetMappedSeq_align(const CSeq_align& orig) const
{
    if (m_MappedObjectType == eMappedObjType_Seq_loc_Conv_Set) {
        // Map the alignment, replace conv-set with the mapped align
        CSeq_loc_Conversion_Set& cvts =
            const_cast<CSeq_loc_Conversion_Set&>(
            *CTypeConverter<CSeq_loc_Conversion_Set>::
            SafeCast(m_MappedObject.GetPointer()));
        CRef<CSeq_align> dst;
        cvts.Convert(orig, &dst);
        const_cast<CAnnotMapping_Info&>(*this).
            SetMappedSeq_align(dst.GetPointerOrNull());
    }
    _ASSERT(m_MappedObjectType == eMappedObjType_Seq_align);
    return *CTypeConverter<CSeq_align>::
        SafeCast(m_MappedObject.GetPointer());
}


void CAnnotMapping_Info::UpdateMappedSeq_loc(CRef<CSeq_loc>& loc,
                                             CRef<CSeq_point>& pnt_ref,
                                             CRef<CSeq_interval>& int_ref,
                                             const CSeq_feat* orig_feat) const
{
    _ASSERT(MappedSeq_locNeedsUpdate());
    if ( !loc || !loc->ReferencedOnlyOnce() ) {
        loc.Reset(new CSeq_loc);
    }
    else {
        loc->Reset();
        loc->InvalidateTotalRangeCache();
    }
    if ( GetMappedObjectType() == eMappedObjType_Seq_id ) {
        CSeq_id& id = const_cast<CSeq_id&>(GetMappedSeq_id());
        if ( IsMappedPoint() ) {
            if ( !pnt_ref || !pnt_ref->ReferencedOnlyOnce() ) {
                pnt_ref.Reset(new CSeq_point);
            }
            CSeq_point& point = *pnt_ref;
            loc->SetPnt(point);
            point.SetId(id);
            point.SetPoint(m_TotalRange.GetFrom());
            if ( GetMappedStrand() != eNa_strand_unknown )
                point.SetStrand(GetMappedStrand());
            else
                point.ResetStrand();
            if ( m_MappedFlags & fMapped_Partial_from ) {
                point.SetFuzz().SetLim(CInt_fuzz::eLim_lt);
            }
            else {
                point.ResetFuzz();
            }
        }
        else {
            if ( !int_ref || !int_ref->ReferencedOnlyOnce() ) {
                int_ref.Reset(new CSeq_interval);
            }
            CSeq_interval& interval = *int_ref;
            loc->SetInt(interval);
            interval.SetId(id);
            interval.SetFrom(m_TotalRange.GetFrom());
            interval.SetTo(m_TotalRange.GetTo());
            if ( GetMappedStrand() != eNa_strand_unknown )
                interval.SetStrand(GetMappedStrand());
            else
                interval.ResetStrand();
            if ( m_MappedFlags & fMapped_Partial_from ) {
                interval.SetFuzz_from().SetLim(CInt_fuzz::eLim_lt);
            }
            else {
                interval.ResetFuzz_from();
            }
            if ( m_MappedFlags & fMapped_Partial_to ) {
                interval.SetFuzz_to().SetLim(CInt_fuzz::eLim_gt);
            }
            else {
                interval.ResetFuzz_to();
            }
        }
    }
    else {
        CSeq_loc_Conversion& cvt = GetMappedSeq_loc_Conv();
        const CSeq_loc& orig_loc = m_MappedFlags & fMapped_Product?
            orig_feat->GetProduct(): orig_feat->GetLocation();
        cvt.MakeDstMix(loc->SetMix(), orig_loc.GetMix());
    }
}


void CAnnotMapping_Info::SetMappedSeq_feat(CSeq_feat& feat)
{
    _ASSERT( IsMapped() );
    _ASSERT(GetMappedObjectType() != eMappedObjType_Seq_feat);

    // Fill mapped location and product in the mapped feature
    CRef<CSeq_loc> mapped_loc;
    if ( MappedSeq_locNeedsUpdate() ) {
        mapped_loc.Reset(new CSeq_loc);
        CRef<CSeq_point> mapped_pnt;
        CRef<CSeq_interval> mapped_int;
        UpdateMappedSeq_loc(mapped_loc, mapped_pnt, mapped_int, &feat);
    }
    else {
        mapped_loc.Reset(&const_cast<CSeq_loc&>(GetMappedSeq_loc()));
    }
    if ( IsMappedLocation() ) {
        feat.SetLocation(*mapped_loc);
    }
    else if ( IsMappedProduct() ) {
        feat.SetProduct(*mapped_loc);
    }
    if ( IsPartial() ) {
        feat.SetPartial(true);
    }
    else {
        feat.ResetPartial();
    }

    m_MappedObject.Reset(&feat);
    m_MappedObjectType = eMappedObjType_Seq_feat;
}


void CAnnotMapping_Info::InitializeMappedSeq_feat(const CSeq_feat& src,
                                                  CSeq_feat& dst) const
{
    CSeq_feat& src_nc = const_cast<CSeq_feat&>(src);
    if ( src_nc.IsSetId() )
        dst.SetId(src_nc.SetId());
    else
        dst.ResetId();

    dst.SetData(src_nc.SetData());

    if ( src_nc.IsSetExcept() )
        dst.SetExcept(src_nc.GetExcept());
    else
        dst.ResetExcept();
    
    if ( src_nc.IsSetComment() )
        dst.SetComment(src_nc.GetComment());
    else
        dst.ResetComment();

    if ( src_nc.IsSetQual() )
        dst.SetQual() = src_nc.GetQual();
    else
        dst.ResetQual();

    if ( src_nc.IsSetTitle() )
        dst.SetTitle(src_nc.GetTitle());
    else
        dst.ResetTitle();

    if ( src_nc.IsSetExt() )
        dst.SetExt(src_nc.SetExt());
    else
        dst.ResetExt();

    if ( src_nc.IsSetCit() )
        dst.SetCit(src_nc.SetCit());
    else
        dst.ResetCit();

    if ( src_nc.IsSetExp_ev() )
        dst.SetExp_ev(src_nc.GetExp_ev());
    else
        dst.ResetExp_ev();

    if ( src_nc.IsSetXref() )
        dst.SetXref() = src_nc.SetXref();
    else
        dst.ResetXref();

    if ( src_nc.IsSetDbxref() )
        dst.SetDbxref() = src_nc.SetDbxref();
    else
        dst.ResetDbxref();

    if ( src_nc.IsSetPseudo() )
        dst.SetPseudo(src_nc.GetPseudo());
    else
        dst.ResetPseudo();

    if ( src_nc.IsSetExcept_text() )
        dst.SetExcept_text(src_nc.GetExcept_text());
    else
        dst.ResetExcept_text();

    if ( src_nc.IsSetIds() )
        dst.SetIds() = src_nc.GetIds();
    else
        dst.ResetIds();

    if ( src_nc.IsSetExts() )
        dst.SetExts() = src_nc.GetExts();
    else
        dst.ResetExts();
    
    dst.SetLocation(src_nc.SetLocation());
    if ( src_nc.IsSetProduct() )
        dst.SetProduct(src_nc.SetProduct());
    else
        dst.ResetProduct();
}


const CSeq_id* CAnnotMapping_Info::GetLocationId(void) const
{
    switch ( GetMappedObjectType() ) {
    case eMappedObjType_Seq_id:
        return &GetMappedSeq_id();
    case eMappedObjType_Seq_loc:
        return GetMappedSeq_loc().GetId();
    case eMappedObjType_Seq_feat:
        return GetMappedSeq_feat().GetLocation().GetId();
    case eMappedObjType_Seq_loc_Conv:
        return &GetMappedSeq_loc_Conv().GetId();
    default:
        break;
    }
    return 0;
}


const CSeq_id* CAnnotMapping_Info::GetProductId(void) const
{
    switch ( GetMappedObjectType() ) {
    case eMappedObjType_Seq_id:
        return &GetMappedSeq_id();
    case eMappedObjType_Seq_loc:
        return GetMappedSeq_loc().GetId();
    case eMappedObjType_Seq_feat:
        return GetMappedSeq_feat().GetProduct().GetId();
    default:
        break;
    }
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
// CAnnotObject_Ref
/////////////////////////////////////////////////////////////////////////////


CAnnotObject_Ref::CAnnotObject_Ref(const CAnnotObject_Info& object,
                                   const CSeq_annot_Handle& annot_handle)
    : m_Seq_annot(annot_handle),
      m_AnnotIndex(object.GetAnnotIndex())
{
    if ( object.IsFeat() ) {
        if ( object.IsRegular() ) {
            const CSeq_feat& feat = *object.GetFeatFast();
            if ( feat.IsSetPartial() ) {
                m_MappingInfo.SetPartial(feat.GetPartial());
            }
        }
        else {
            m_MappingInfo.SetPartial(GetSeq_annot_Info().IsTableFeatPartial(object));
        }
    }
    if ( object.HasSingleKey() ) {
        m_MappingInfo.SetTotalRange(object.GetKey().m_Range);
    }
    else {
        size_t beg = object.GetKeysBegin();
        size_t end = object.GetKeysEnd();
        if ( beg < end ) {
            const SAnnotObject_Key& key =
                GetSeq_annot_Info().GetAnnotObjectKey(beg);
            m_MappingInfo.SetTotalRange(key.m_Range);
        }
    }
}


CAnnotObject_Ref::CAnnotObject_Ref(const CSeq_annot_SNP_Info& snp_annot,
                                   const CSeq_annot_Handle& annot_handle,
                                   const SSNP_Info& snp,
                                   CSeq_loc_Conversion* cvt)
    : m_Seq_annot(annot_handle),
      m_AnnotIndex(snp_annot.GetIndex(snp) | kSNPTableBit)
{
    _ASSERT(IsSNPFeat());
    TSeqPos src_from = snp.GetFrom(), src_to = snp.GetTo();
    ENa_strand src_strand = eNa_strand_unknown;
    if ( snp.MinusStrand() ) {
        src_strand = eNa_strand_minus;
    }
    else if ( snp.PlusStrand() ) {
        src_strand = eNa_strand_plus;
    }
    if ( !cvt ) {
        m_MappingInfo.SetTotalRange(TRange(src_from, src_to));
        m_MappingInfo.SetMappedSeq_id(
            const_cast<CSeq_id&>(GetSeq_annot_SNP_Info().
            GetSeq_id()),
            src_from == src_to);
        m_MappingInfo.SetMappedStrand(src_strand);
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
    cvt->SetMappedLocation(*this, CSeq_loc_Conversion::eLocation);
}


void CAnnotObject_Ref::ResetLocation(void)
{
    m_MappingInfo.Reset();
    if ( !IsSNPFeat() ) {
        const CAnnotObject_Info& object = GetAnnotObject_Info();
        if ( object.IsFeat() ) {
            const CSeq_feat& feat = *object.GetFeatFast();
            if ( feat.IsSetPartial() ) {
                m_MappingInfo.SetPartial(feat.GetPartial());
            }
        }
    }
}


const CSeq_annot_SNP_Info& CAnnotObject_Ref::GetSeq_annot_SNP_Info(void) const
{
    _ASSERT(IsSNPFeat());
    return GetSeq_annot_Info().x_GetSNP_annot_Info();
}


const CAnnotObject_Info& CAnnotObject_Ref::GetAnnotObject_Info(void) const
{
    _ASSERT(HasAnnotObject_Info());
    return GetSeq_annot_Info().GetInfo(GetAnnotIndex());
}


const SSNP_Info& CAnnotObject_Ref::GetSNP_Info(void) const
{
    _ASSERT(IsSNPFeat());
    return GetSeq_annot_SNP_Info().GetInfo(GetAnnotIndex());
}


bool CAnnotObject_Ref::IsFeat(void) const
{
    return IsSNPFeat() || GetAnnotObject_Info().IsFeat();
}


bool CAnnotObject_Ref::IsGraph(void) const
{
    return HasAnnotObject_Info()  &&  GetAnnotObject_Info().IsGraph();
}


bool CAnnotObject_Ref::IsAlign(void) const
{
    return HasAnnotObject_Info()  &&  GetAnnotObject_Info().IsAlign();
}


const CSeq_feat& CAnnotObject_Ref::GetFeat(void) const
{
    return GetAnnotObject_Info().GetFeat();
}


const CSeq_graph& CAnnotObject_Ref::GetGraph(void) const
{
    return GetAnnotObject_Info().GetGraph();
}


const CSeq_align& CAnnotObject_Ref::GetAlign(void) const
{
    return GetAnnotObject_Info().GetAlign();
}


BEGIN_LOCAL_NAMESPACE;

/////////////////////////////////////////////////////////////////////////////
// CAnnotObject_Ref comparision
/////////////////////////////////////////////////////////////////////////////

struct CAnnotObjectType_Less
{
    bool m_ByProduct;
    IFeatComparator* m_FeatComparator;
    CScope* m_Scope;
    bool m_DoWeIgnoreFarLocationsForSorting;
    
    class CNearnessTester : public CSeq_loc::ISubLocFilter {
    public:
        CNearnessTester( const CBioseq_Handle &handle ) 
            : m_BioseqHandle(handle) 
        {

        }

        bool operator()( const CSeq_id *id ) const {
            if( NULL == id ) {
                return false;
            }
            return m_BioseqHandle.IsSynonym(*id);
        }
    private:
        const CBioseq_Handle &m_BioseqHandle;
    };

    CNearnessTester m_TesterForIgnoreFarLocationsForSorting;
    explicit CAnnotObjectType_Less(const SAnnotSelector* sel = 0,
                                   CScope* scope = 0)
        : m_ByProduct(sel && sel->GetFeatProduct()),
          m_FeatComparator(sel? sel->GetFeatComparator(): 0),
          m_Scope(scope),
          m_TesterForIgnoreFarLocationsForSorting( sel->GetIgnoreFarLocationsForSorting() )
        {
            m_DoWeIgnoreFarLocationsForSorting = (!! sel->GetIgnoreFarLocationsForSorting() );
        }

    bool operator()(const CAnnotObject_Ref& x,
                    const CAnnotObject_Ref& y) const;

    // smaller first
    static int GetTypeOrder(CSeqFeatData::E_Choice type,
                            CSeqFeatData::ESubtype subtype)
        {
            if ( subtype == CSeqFeatData::eSubtype_operon ) {
                // operon first
                return -1;
            }
            else {
                return CSeq_feat::GetTypeSortingOrder(type);
            }
        }
};

class CCreateFeat
{
public:
    const CSeq_feat& GetOriginalFeat(const CAnnotObject_Ref& ref,
                                     const CAnnotObject_Info* info);
    const CSeq_feat& GetMappedFeat(const CAnnotObject_Ref& ref,
                                   const CAnnotObject_Info* info);
    int GetCdregionOrder(const CAnnotObject_Ref& ref,
                         const CAnnotObject_Info* info);
    const char* GetImpKey(const CAnnotObject_Ref& ref,
                          const CAnnotObject_Info* info);

    static const CSeq_loc& GetLoc(const CSeq_feat& feat, bool by_product) {
        return by_product? feat.GetProduct(): feat.GetLocation();
    }

    ENa_strand GetStrand(const CAnnotObject_Ref& ref,
                         const CAnnotObject_Info* info,
                         bool by_product);

    const CSeq_loc* GetLoc(const CAnnotObject_Ref& ref,
                           const CAnnotObject_Info* info,
                           bool by_product);

private:
    CRef<CSeq_feat> m_CreatedOriginalFeat;
};


const CSeq_feat& CCreateFeat::GetOriginalFeat(const CAnnotObject_Ref& ref,
                                              const CAnnotObject_Info* info)
{
    if ( ref.IsPlainFeat() ) {
        // real Seq-feat exists
        return *info->GetFeatFast();
    }
    else {
        // table feature
        if ( !m_CreatedOriginalFeat ) {
            CRef<CSeq_point> seq_pnt;
            CRef<CSeq_interval> seq_int;
            if ( !info ) {
                // SNP table feature
                const CSeq_annot_SNP_Info& annot_snp_info =
                    ref.GetSeq_annot_SNP_Info();
                ref.GetSNP_Info().UpdateSeq_feat(m_CreatedOriginalFeat,
                                                 seq_pnt, seq_int,
                                                 annot_snp_info);
            }
            else {
                _ASSERT(ref.IsTableFeat());
                ref.GetSeq_annot_Info().GetTableInfo().
                    UpdateSeq_feat(ref.GetAnnotIndex(),
                                   m_CreatedOriginalFeat, seq_pnt, seq_int);
            }
            _ASSERT(m_CreatedOriginalFeat);
        }
        return *m_CreatedOriginalFeat;
    }
}


const CSeq_feat& CCreateFeat::GetMappedFeat(const CAnnotObject_Ref& ref,
                                            const CAnnotObject_Info* info)
{
    CAnnotMapping_Info& map = ref.GetMappingInfo();
    if ( !map.IsMapped() ) {
        return GetOriginalFeat(ref, info);
    }
    if ( map.GetMappedObjectType() == map.eMappedObjType_Seq_feat ) {
        // mapped Seq-feat is created already
        return map.GetMappedSeq_feat();
    }
    
    CRef<CSeq_feat> mapped_feat(new CSeq_feat);
    map.InitializeMappedSeq_feat(GetOriginalFeat(ref, info), *mapped_feat);
    map.SetMappedSeq_feat(*mapped_feat);
    return map.GetMappedSeq_feat();
}


int CCreateFeat::GetCdregionOrder(const CAnnotObject_Ref& ref,
                                  const CAnnotObject_Info* info)
{
    CCdregion::EFrame frame = 
        GetMappedFeat(ref, info).GetData().GetCdregion().GetFrame();
    if ( frame == CCdregion::eFrame_not_set ) {
        frame = CCdregion::eFrame_one;
    }
    return frame;
}


const char* CCreateFeat::GetImpKey(const CAnnotObject_Ref& ref,
                                   const CAnnotObject_Info* info)
{
    static const char* const variation_key = "variation";
    if ( !info ) {
        return variation_key;
    }
    return GetOriginalFeat(ref, info).GetData().GetImp().GetKey().c_str();
}


ENa_strand CCreateFeat::GetStrand(const CAnnotObject_Ref& ref,
                                  const CAnnotObject_Info* info,
                                  bool by_product)
{
    try {
        CAnnotMapping_Info& map = ref.GetMappingInfo();
        if ( map.IsMappedLocation() ) {
            // location is mapped
            if ( map.GetMappedObjectType() == map.eMappedObjType_Seq_feat ) {
                // mapped Seq-feat is created already
                return GetLoc(map.GetMappedSeq_feat(), by_product).GetStrand();
            }
            else if ( map.GetMappedObjectType() == map.eMappedObjType_Seq_loc ) {
                // mapped Seq-loc is created already
                return map.GetMappedSeq_loc().GetStrand();
            }
            else {
                // whole, interval, point, or mix
                return map.GetMappedStrand();
            }
        }
        else {
            // location is not mapped - use original
            if ( !info ) {
                // table SNP without mapping
                return map.GetMappedStrand();
            }
            else {
                // get location from the Seq-feat
                return GetLoc(GetOriginalFeat(ref, info), by_product).GetStrand();
            }
        }
    }
    catch ( CException& /*ignored*/ ) {
        // assume unknown strand for sorting
        return eNa_strand_unknown;
    }
}


const CSeq_loc* CCreateFeat::GetLoc(const CAnnotObject_Ref& ref,
                                    const CAnnotObject_Info* info,
                                    bool by_product)
{
    if ( !info ) {
        // table SNP -> no mix
        return 0;
    }
    CAnnotMapping_Info& map = ref.GetMappingInfo();
    if ( map.IsMappedLocation() ) {
        // location is mapped
        if ( map.GetMappedObjectType() == map.eMappedObjType_Seq_loc ) {
            // mapped Seq-loc is created already
            const CSeq_loc& loc = map.GetMappedSeq_loc();
            return &loc;
        }
        else if ( map.GetMappedObjectType() == map.eMappedObjType_Seq_id ) {
            // whole, interval, or point
            return 0;
        }
        // get location from the Seq-feat
        const CSeq_loc& loc = GetLoc(GetMappedFeat(ref, info), by_product);
        return &loc;
    }
    else {
        // get location from the Seq-feat
        const CSeq_loc& loc = GetLoc(GetOriginalFeat(ref, info), by_product);
        return &loc;
    }
}


bool CAnnotObjectType_Less::operator()(const CAnnotObject_Ref& x,
                                       const CAnnotObject_Ref& y) const
{
    // gather x annotation type
    const CAnnotObject_Info* x_info;
    CSeq_annot::C_Data::E_Choice x_annot_type;
    if ( !x.IsSNPFeat() ) {
        x_info = &x.GetAnnotObject_Info();
        x_annot_type = x_info->GetAnnotType();
    }
    else {
        x_info = 0;
        x_annot_type = CSeq_annot::C_Data::e_Ftable;
    }

    // gather y annotation type
    const CAnnotObject_Info* y_info;
    CSeq_annot::C_Data::E_Choice y_annot_type;
    if ( !y.IsSNPFeat() ) {
        y_info = &y.GetAnnotObject_Info();
        y_annot_type = y_info->GetAnnotType();
    }
    else {
        y_info = 0;
        y_annot_type = CSeq_annot::C_Data::e_Ftable;
    }
    
    // compare by annotation type (feature, align, graph)
    if ( x_annot_type != y_annot_type ) {
        return x_annot_type < y_annot_type;
    }

    if ( x_annot_type == CSeq_annot::C_Data::e_Ftable ) {
        // compare features

        // get x feature type
        CSeqFeatData::E_Choice x_feat_type;
        CSeqFeatData::ESubtype x_feat_subtype;
        if ( x_info ) {
            x_feat_type = x_info->GetFeatType();
            x_feat_subtype = x_info->GetFeatSubtype();
        }
        else {
            x_feat_type = CSeqFeatData::e_Imp;
            x_feat_subtype = CSeqFeatData::eSubtype_variation;
        }

        // get y feature type
        CSeqFeatData::E_Choice y_feat_type;
        CSeqFeatData::ESubtype y_feat_subtype;
        if ( y_info ) {
            y_feat_type = y_info->GetFeatType();
            y_feat_subtype = y_info->GetFeatSubtype();
        }
        else {
            y_feat_type = CSeqFeatData::e_Imp;
            y_feat_subtype = CSeqFeatData::eSubtype_variation;
        }

        // order by feature type
        if ( x_feat_subtype != y_feat_subtype ) {
            int x_order = GetTypeOrder(x_feat_type, x_feat_subtype);
            int y_order = GetTypeOrder(y_feat_type, y_feat_subtype);
            if ( x_order != y_order ) {
                return x_order < y_order;
            }
        }

        CCreateFeat x_create, y_create;

        // compare strands
        ENa_strand x_strand = x_create.GetStrand(x, x_info, m_ByProduct);
        ENa_strand y_strand = y_create.GetStrand(y, y_info, m_ByProduct);
        bool x_minus = IsReverse(x_strand);
        bool y_minus = IsReverse(y_strand);
        if ( x_minus != y_minus ) {
            // minus strand last
            return y_minus;
        }
        
        // compare complex locations (mix or packed intervals)
        const CSeq_loc* x_loc = x_create.GetLoc(x, x_info, m_ByProduct);
        const CSeq_loc* y_loc = y_create.GetLoc(y, y_info, m_ByProduct);
        
        bool x_complex = x_loc && (x_loc->IsMix() || x_loc->IsPacked_int());
        bool y_complex = y_loc && (y_loc->IsMix() || y_loc->IsPacked_int());
        if ( x_complex != y_complex ) {
            // simple loc before complex on plus strand, after on minus strand
            return x_minus ^ y_complex;
        }

        if ( x_complex ) {
            int diff = 0;
            if( m_DoWeIgnoreFarLocationsForSorting ) {
                diff = x_loc->CompareSubLoc(*y_loc, x_strand, &m_TesterForIgnoreFarLocationsForSorting);
            } else {
                diff = x_loc->CompareSubLoc(*y_loc, x_strand);
            }
            if ( diff != 0 ) {
                return diff < 0;
            }
        }
            
        // compare subtypes
        if ( x_feat_subtype != y_feat_subtype ) {
            return x_feat_subtype < y_feat_subtype;
        }

        _ASSERT(x_feat_type == y_feat_type);
        // type dependent comparison
        if ( x_feat_type == CSeqFeatData::e_Cdregion ) {
            // compare frames of identical CDS ranges
            int x_frame = x_create.GetCdregionOrder(x, x_info);
            int y_frame = y_create.GetCdregionOrder(y, y_info);
            if ( x_frame != y_frame ) {
                return x_frame < y_frame;
            }
        }
        else if ( x_feat_type == CSeqFeatData::e_Imp ) {
            const char* x_key = x_create.GetImpKey(x, x_info);
            const char* y_key = y_create.GetImpKey(y, y_info);

            // compare labels of imp features
            if ( x_key != y_key ) {
                int diff = NStr::CompareNocase(x_key, y_key);
                if ( diff != 0 ) {
                    return diff < 0;
                }
            }
        }

        if ( m_FeatComparator ) {
            const CSeq_feat& x_feat = x_create.GetMappedFeat(x, x_info);
            const CSeq_feat& y_feat = y_create.GetMappedFeat(y, y_info);
            if ( m_FeatComparator->Less(x_feat, y_feat, m_Scope) ) {
                return true;
            }
            if ( m_FeatComparator->Less(y_feat, x_feat, m_Scope) ) {
                return false;
            }
        }
    }

    return x < y;
}


struct CAnnotObject_Less
{
    explicit CAnnotObject_Less(const SAnnotSelector* sel = 0,
                               CScope* scope = 0)
        : type_less(sel, scope)
        {
        }
    // Compare CRef-s: both must be features
    bool operator()(const CAnnotObject_Ref& x,
                    const CAnnotObject_Ref& y) const
        {
            if ( x == y ) { // small speedup
                return false;
            }

            TSeqPos x_from = x.GetMappingInfo().GetFrom();
            TSeqPos y_from = y.GetMappingInfo().GetFrom();
            TSeqPos x_to = x.GetMappingInfo().GetToOpen();
            TSeqPos y_to = y.GetMappingInfo().GetToOpen();
            // (from >= to) means circular location.
            // Any circular location is less than (before) non-circular one.
            // If both are circular, compare them regular way.
            bool x_circular = x_from >= x_to;
            bool y_circular = y_from >= y_to;
            if ( x_circular != y_circular ) {
                return x_circular;
            }
            // smallest left extreme first
            if ( x_from != y_from ) {
                return x_from < y_from;
            }
            // longest feature first
            if ( x_to != y_to ) {
                return x_to > y_to;
            }
            return type_less(x, y);
        }
    CAnnotObjectType_Less type_less;
};


struct CAnnotObject_LessReverse
{
    explicit CAnnotObject_LessReverse(const SAnnotSelector* sel = 0,
                                      CScope* scope = 0)
        : type_less(sel, scope)
        {
        }
    // Compare CRef-s: both must be features
    bool operator()(const CAnnotObject_Ref& x,
                    const CAnnotObject_Ref& y) const
        {
            if ( x == y ) { // small speedup
                return false;
            }

            TSeqPos x_from = x.GetMappingInfo().GetFrom();
            TSeqPos y_from = y.GetMappingInfo().GetFrom();
            TSeqPos x_to = x.GetMappingInfo().GetToOpen();
            TSeqPos y_to = y.GetMappingInfo().GetToOpen();
            // (from >= to) means circular location.
            // Any circular location is less than (before) non-circular one.
            // If both are circular, compare them regular way.
            bool x_circular = x_from >= x_to;
            bool y_circular = y_from >= y_to;
            if ( x_circular != y_circular ) {
                return x_circular;
            }
            // largest right extreme first
            if ( x_to != y_to ) {
                return x_to > y_to;
            }
            // longest feature first
            if ( x_from != y_from ) {
                return x_from < y_from;
            }
            return type_less(x, y);
        }
    CAnnotObjectType_Less type_less;
};


END_LOCAL_NAMESPACE;


/////////////////////////////////////////////////////////////////////////////
// CCreatedFeat_Ref
/////////////////////////////////////////////////////////////////////////////


CCreatedFeat_Ref::CCreatedFeat_Ref(void)
{
}


CCreatedFeat_Ref::~CCreatedFeat_Ref(void)
{
}


void CCreatedFeat_Ref::ResetRefs(void)
{
    m_CreatedSeq_feat.Reset();
    m_CreatedSeq_loc.Reset();
    m_CreatedSeq_point.Reset();
    m_CreatedSeq_interval.Reset();
}


void CCreatedFeat_Ref::ReleaseRefsTo(CRef<CSeq_feat>*     feat,
                                     CRef<CSeq_loc>*      loc,
                                     CRef<CSeq_point>*    point,
                                     CRef<CSeq_interval>* interval)
{
    if (feat) {
        m_CreatedSeq_feat.AtomicReleaseTo(*feat);
    }
    if (loc) {
        m_CreatedSeq_loc.AtomicReleaseTo(*loc);
    }
    if (point) {
        m_CreatedSeq_point.AtomicReleaseTo(*point);
    }
    if (interval) {
        m_CreatedSeq_interval.AtomicReleaseTo(*interval);
    }
}


void CCreatedFeat_Ref::ResetRefsFrom(CRef<CSeq_feat>*     feat,
                                     CRef<CSeq_loc>*      loc,
                                     CRef<CSeq_point>*    point,
                                     CRef<CSeq_interval>* interval)
{
    if (feat) {
        m_CreatedSeq_feat.AtomicResetFrom(*feat);
    }
    if (loc) {
        m_CreatedSeq_loc.AtomicResetFrom(*loc);
    }
    if (point) {
        m_CreatedSeq_point.AtomicResetFrom(*point);
    }
    if (interval) {
        m_CreatedSeq_interval.AtomicResetFrom(*interval);
    }
}


CConstRef<CSeq_feat>
CCreatedFeat_Ref::GetOriginalFeature(const CSeq_feat_Handle& feat_h)
{
    CConstRef<CSeq_feat> ret;
    if ( feat_h.IsTableSNP() ) {
        const CSeq_annot_SNP_Info& snp_annot = feat_h.x_GetSNP_annot_Info();
        const SSNP_Info& snp_info = feat_h.x_GetSNP_Info();
        CRef<CSeq_feat> orig_feat;
        CRef<CSeq_point> created_point;
        CRef<CSeq_interval> created_interval;
        ReleaseRefsTo(&orig_feat, 0, &created_point, &created_interval);
        snp_info.UpdateSeq_feat(orig_feat,
                                created_point,
                                created_interval,
                                snp_annot);
        ret = orig_feat;
        ResetRefsFrom(&orig_feat, 0, &created_point, &created_interval);
    }
    else if ( feat_h.IsTableFeat() ) {
        if ( feat_h.m_CreatedOriginalFeat ) {
            ret = feat_h.m_CreatedOriginalFeat;
        }
        else {
            const CSeq_annot_Info& annot = feat_h.x_GetSeq_annot_Info();
            CRef<CSeq_feat> orig_feat;
            CRef<CSeq_point> created_point;
            CRef<CSeq_interval> created_interval;
            //ReleaseRefsTo(&orig_feat, 0, &created_point, &created_interval);
            annot.UpdateTableFeat(orig_feat,
                                  created_point,
                                  created_interval,
                                  feat_h.x_GetAnnotObject_Info());
            ret = orig_feat;
            //ResetRefsFrom(&orig_feat, 0, &created_point, &created_interval);
            feat_h.m_CreatedOriginalFeat = ret;
        }
    }
    else {
        ret = feat_h.GetPlainSeq_feat();
    }
    return ret;
}


CRef<CSeq_loc>
CCreatedFeat_Ref::GetMappedLocation(const CAnnotMapping_Info& map,
                                    const CSeq_feat& orig_feat)
{
    CRef<CSeq_loc> ret;
    if ( map.MappedSeq_locNeedsUpdate() ) {
        // need to convert Seq_id to Seq_loc
        // clear references to mapped location from mapped feature
        // Can not use m_MappedSeq_feat since it's a const-ref
        CRef<CSeq_feat> mapped_feat;
        m_CreatedSeq_feat.AtomicReleaseTo(mapped_feat);
        if ( mapped_feat ) {
            if ( !mapped_feat->ReferencedOnlyOnce() ) {
                mapped_feat.Reset();
            }
            else {
                // hack with null ptr as ResetLocation doesn't reset CRef<>
                CSeq_loc* loc = 0;
                mapped_feat->SetLocation(*loc);
                mapped_feat->ResetProduct();
            }
        }
        m_CreatedSeq_feat.AtomicResetFrom(mapped_feat);
    
        CRef<CSeq_loc> mapped_loc;
        CRef<CSeq_point> created_point;
        CRef<CSeq_interval> created_interval;
        ReleaseRefsTo(0, &mapped_loc, &created_point, &created_interval);
        map.UpdateMappedSeq_loc(mapped_loc,
                                created_point,
                                created_interval,
                                &orig_feat);
        ret = mapped_loc;
        ResetRefsFrom(0, &mapped_loc, &created_point, &created_interval);
    }
    else if ( map.IsMapped() ) {
        ret = const_cast<CSeq_loc*>(&map.GetMappedSeq_loc());
    }
    return ret;
}


CRef<CSeq_loc>
CCreatedFeat_Ref::GetMappedLocation(const CAnnotMapping_Info& map,
                                    const CMappedFeat& feat)
{
    if ( !map.IsMapped() ) {
        return null;
    }
    else if ( !map.MappedSeq_locNeedsUpdate() ) {
        return Ref(const_cast<CSeq_loc*>(&map.GetMappedSeq_loc()));
    }
    else {
        return GetMappedLocation(map, *feat.GetOriginalSeq_feat());
    }
}


CConstRef<CSeq_feat>
CCreatedFeat_Ref::GetMappedFeature(const CAnnotMapping_Info& map,
                                   const CMappedFeat& feat)
{
    if ( map.GetMappedObjectType() == map.eMappedObjType_Seq_feat) {
        return ConstRef(&map.GetMappedSeq_feat());
    }
    else {
        return GetMappedFeature(map, *feat.GetOriginalSeq_feat());
    }
}


CConstRef<CSeq_feat>
CCreatedFeat_Ref::GetMappedFeature(const CAnnotMapping_Info& map,
                                   const CSeq_feat& orig_feat)
{
    CConstRef<CSeq_feat> ret;
    if ( map.GetMappedObjectType() == map.eMappedObjType_Seq_feat) {
        ret = &map.GetMappedSeq_feat();
    }
    else if ( !map.IsMapped() ) {
        ret = &orig_feat;
    }
    else {
        CRef<CSeq_loc> loc = GetMappedLocation(map, orig_feat);

        // some Seq-loc object is mapped
        CRef<CSeq_feat> mapped_feat;
        m_CreatedSeq_feat.AtomicReleaseTo(mapped_feat);
        if ( !mapped_feat || !mapped_feat->ReferencedOnlyOnce() ) {
            mapped_feat.Reset(new CSeq_feat);
            // copy all fields from original feature
            map.InitializeMappedSeq_feat(orig_feat, *mapped_feat);
        }
        else {
            // copy only unmapped location/product fields from original feature
            CSeq_feat& src_nc = const_cast<CSeq_feat&>(orig_feat);
            if ( !map.IsMappedLocation() ) {
                mapped_feat->SetLocation(src_nc.SetLocation());
            }
            if ( !map.IsMappedProduct() ) {
                if ( orig_feat.IsSetProduct() )
                    mapped_feat->SetProduct(src_nc.SetProduct());
                else
                    mapped_feat->ResetProduct();
            }
        }

        // set mapped location/product field
        if ( map.IsMappedLocation() ) {
            mapped_feat->SetLocation(*loc);
        }
        else if ( map.IsMappedProduct() ) {
            mapped_feat->SetProduct(*loc);
        }
        // set mapped partial field
        if ( map.IsPartial() ) {
            mapped_feat->SetPartial(true);
        }
        else {
            mapped_feat->ResetPartial();
        }

        ret = mapped_feat;
        m_CreatedSeq_feat.AtomicResetFrom(mapped_feat);
    }
    return ret;
}


/////////////////////////////////////////////////////////////////////////////
// CAnnot_Collector, CAnnotMappingCollector
/////////////////////////////////////////////////////////////////////////////


class CAnnotMappingCollector
{
public:
    typedef map<CAnnotObject_Ref,
                CRef<CSeq_loc_Conversion_Set> > TAnnotMappingSet;
    // Set of annotations for complex remapping
    TAnnotMappingSet              m_AnnotMappingSet;
};


CAnnot_Collector::CAnnot_Collector(CScope& scope)
    : m_Selector(0),
      m_Scope(scope)
{
}


CAnnot_Collector::~CAnnot_Collector(void)
{
}


inline
bool CAnnot_Collector::x_NoMoreObjects(void) const
{
    size_t limit = m_Selector->m_MaxSize;
    if ( limit >= kMax_UInt ) {
        return false;
    }
    size_t size = m_AnnotSet.size();
    if ( m_MappingCollector.get() ) {
        size += m_MappingCollector->m_AnnotMappingSet.size();
    }
    return size >= limit;
}


bool CAnnot_Collector::CanResolveId(const CSeq_id_Handle& idh,
                                    const CBioseq_Handle& bh)
{
    switch ( m_Selector->GetResolveMethod() ) {
    case SAnnotSelector::eResolve_All:
        return true;
    case SAnnotSelector::eResolve_TSE:
        return m_Scope->GetBioseqHandleFromTSE(idh, bh.GetTSE_Handle());
    default:
        return false;
    }
}

static CSeqFeatData::ESubtype s_DefaultAdaptiveTriggers[] = {
    CSeqFeatData::eSubtype_gene,
    CSeqFeatData::eSubtype_cdregion,
    CSeqFeatData::eSubtype_mRNA
};

void CAnnot_Collector::x_Initialize0(const SAnnotSelector& selector)
{
    m_Selector = &selector;
    m_TriggerTypes.reset();
    SAnnotSelector::TAdaptiveDepthFlags adaptive_flags = 0;
    if ( !selector.GetExactDepth() ||
         selector.GetResolveDepth() == kMax_Int ) {
        adaptive_flags = selector.GetAdaptiveDepthFlags();
    }
    if ( adaptive_flags & selector.fAdaptive_ByTriggers ) {
        if ( selector.m_AdaptiveTriggers.empty() ) {
            const size_t count =
                sizeof(s_DefaultAdaptiveTriggers)/
                sizeof(s_DefaultAdaptiveTriggers[0]);
            for ( int i = count - 1; i >= 0; --i ) {
                CSeqFeatData::ESubtype subtype = s_DefaultAdaptiveTriggers[i];
                size_t index = CAnnotType_Index::GetSubtypeIndex(subtype);
                if ( index ) {
                    m_TriggerTypes.set(index);
                }
            }
        }
        else {
            ITERATE ( SAnnotSelector::TAdaptiveTriggers, it,
                      selector.m_AdaptiveTriggers ) {
                pair<size_t, size_t> idxs =
                    CAnnotType_Index::GetIndexRange(*it);
                for ( size_t i = idxs.first; i < idxs.second; ++i ) {
                    m_TriggerTypes.set(i);
                }
            }
        }
    }
    m_UnseenAnnotTypes.set();
    m_CollectAnnotTypes = selector.m_AnnotTypesBitset;
    if ( !m_CollectAnnotTypes.any() ) {
        pair<size_t, size_t> range =
            CAnnotType_Index::GetIndexRange(selector);
        for ( size_t index = range.first; index < range.second; ++index ) {
            m_CollectAnnotTypes.set(index);
        }
    }
    if ( selector.m_CollectNames ) {
        m_AnnotNames.reset(new TAnnotNames());
    }
    selector.CheckLimitObjectType();
    if ( selector.m_LimitObjectType != SAnnotSelector::eLimit_None ) {
        x_GetTSE_Info();
    }
}

void CAnnot_Collector::x_Initialize(const SAnnotSelector& selector,
                                    const CBioseq_Handle& bh,
                                    const CRange<TSeqPos>& range,
                                    ENa_strand strand)
{
    if ( !bh ) {
        NCBI_THROW(CAnnotException, eBadLocation,
                   "Bioseq handle is null");
    }
    CScope_Impl::TConfReadLockGuard guard(m_Scope->m_ConfLock);
    x_Initialize0(selector);

    CSeq_id_Handle master_id = bh.GetAccessSeq_id_Handle();
    CHandleRange master_range;
    master_range.AddRange(range, strand);

    int depth = selector.GetResolveDepth();
    bool depth_is_set = depth >= 0 && depth < kMax_Int;
    bool exact_depth = selector.GetExactDepth() && depth_is_set;
    int adaptive_flags = exact_depth? 0: selector.GetAdaptiveDepthFlags();
    int by_policy = adaptive_flags & SAnnotSelector::fAdaptive_ByPolicy;
    adaptive_flags &=
        SAnnotSelector::fAdaptive_ByTriggers |
        SAnnotSelector::fAdaptive_BySubtypes;

    // main sequence
    bool deeper = true;
    if ( adaptive_flags || !exact_depth || depth == 0 ) {
        x_SearchMaster(bh, master_id, master_range);
        deeper = !x_NoMoreObjects();
    }
    if ( deeper ) {
        deeper = depth > 0 &&
            selector.GetResolveMethod() != selector.eResolve_None;
    }
    if ( deeper && by_policy ) {
        deeper =
            bh.GetFeatureFetchPolicy() != bh.eFeatureFetchPolicy_only_near;
    }
    if ( deeper && adaptive_flags ) {
        m_CollectAnnotTypes &= m_UnseenAnnotTypes;
        deeper = m_CollectAnnotTypes.any();
    }
    if ( deeper ) {
        deeper = bh.GetSeqMap().HasSegmentOfType(CSeqMap::eSeqRef);
    }

    if ( deeper ) {
        CRef<CSeq_loc> master_loc_empty(new CSeq_loc);
        master_loc_empty->
            SetEmpty(const_cast<CSeq_id&>(*master_id.GetSeqId()));
        for ( int level = 1; level <= depth && deeper; ++level ) {
            // segments
            if ( adaptive_flags || !exact_depth || depth == level ) {
                deeper = x_SearchSegments(bh, master_id, master_range,
                                          *master_loc_empty, level);
                if ( deeper ) {
                    deeper = !x_NoMoreObjects();
                }
            }
            if ( deeper ) {
                deeper = depth > level;
            }
            if ( deeper && adaptive_flags ) {
                m_CollectAnnotTypes &= m_UnseenAnnotTypes;
                deeper = m_CollectAnnotTypes.any();
            }
        }
    }
        
    x_AddPostMappings();
    x_Sort();
}


void CAnnot_Collector::x_Initialize(const SAnnotSelector& selector,
                                    const CHandleRangeMap& master_loc)
{
    CScope_Impl::TConfReadLockGuard guard(m_Scope->m_ConfLock);
    x_Initialize0(selector);

    int depth = selector.GetResolveDepth();
    bool depth_is_set = depth >= 0 && depth < kMax_Int;
    bool exact_depth = selector.GetExactDepth() && depth_is_set;
    int adaptive_flags = exact_depth? 0: selector.GetAdaptiveDepthFlags();
    adaptive_flags &=
        SAnnotSelector::fAdaptive_ByTriggers |
        SAnnotSelector::fAdaptive_BySubtypes;

    // main sequence
    bool deeper = true;
    if ( adaptive_flags || !exact_depth || depth == 0 ) {
        x_SearchLoc(master_loc, 0, 0, true);
        deeper = !x_NoMoreObjects();
    }
    if ( deeper ) {
        deeper = depth > 0 &&
            selector.GetResolveMethod() != selector.eResolve_None;
    }
    if ( deeper && adaptive_flags ) {
        m_CollectAnnotTypes &= m_UnseenAnnotTypes;
        deeper = m_CollectAnnotTypes.any();
    }

    if ( deeper ) {
        for ( int level = 1; level <= depth && deeper; ++level ) {
            // segments
            if ( adaptive_flags || !exact_depth || depth == level ) {
                deeper = x_SearchSegments(master_loc, level);
                if ( deeper ) {
                    deeper = !x_NoMoreObjects();
                }
            }
            if ( deeper ) {
                deeper = depth > level;
            }
            if ( deeper && adaptive_flags ) {
                m_CollectAnnotTypes &= m_UnseenAnnotTypes;
                deeper = m_CollectAnnotTypes.any();
            }
        }
    }

    x_AddPostMappings();
    x_Sort();
}


void CAnnot_Collector::x_SearchMaster(const CBioseq_Handle& bh,
                                      const CSeq_id_Handle& master_id,
                                      const CHandleRange& master_range)
{
    if ( m_Selector->m_LimitObjectType == SAnnotSelector::eLimit_None ) {
        // any data source
        const CTSE_Handle& tse = bh.GetTSE_Handle();
        if ( m_Selector->m_ExcludeExternal ) {
            const CTSE_Info& tse_info = tse.x_GetTSE_Info();
            tse_info.UpdateAnnotIndex();
            if ( tse_info.HasMatchingAnnotIds() ) {
                CConstRef<CSynonymsSet> syns = m_Scope->GetSynonyms(bh);
                ITERATE(CSynonymsSet, syn_it, *syns) {
                    x_SearchTSE(tse, syns->GetSeq_id_Handle(syn_it),
                                master_range, 0);
                    if ( x_NoMoreObjects() ) {
                        break;
                    }
                }
            }
            else {
                const CBioseq_Handle::TId& syns = bh.GetId();
                bool only_gi = tse_info.OnlyGiAnnotIds();
                ITERATE ( CBioseq_Handle::TId, syn_it, syns ) {
                    if ( !only_gi || syn_it->IsGi() ) {
                        x_SearchTSE(tse, *syn_it,
                                    master_range, 0);
                        if ( x_NoMoreObjects() ) {
                            break;
                        }
                    }
                }
            }
        }
        else {
            CScope_Impl::TTSE_LockMatchSet tse_map;
            if ( m_Selector->IsIncludedAnyNamedAnnotAccession() ) {
                m_Scope->GetTSESetWithAnnots(bh, tse_map, *m_Selector);
            }
            else {
                m_Scope->GetTSESetWithAnnots(bh, tse_map);
            }
            ITERATE (CScope_Impl::TTSE_LockMatchSet, tse_it, tse_map) {
                tse.AddUsedTSE(tse_it->first);
                x_SearchTSE(tse_it->first, tse_it->second,
                            master_range, 0);
                if ( x_NoMoreObjects() ) {
                    break;
                }
            }
        }
    }
    else {
        // Search in the limit objects
        CConstRef<CSynonymsSet> syns;
        bool syns_initialized = false;
        ITERATE ( TTSE_LockMap, tse_it, m_TSE_LockMap ) {
            const CTSE_Info& tse_info = *tse_it->first;
            tse_info.UpdateAnnotIndex();
            if ( tse_info.HasMatchingAnnotIds() ) {
                if ( !syns_initialized ) {
                    syns = m_Scope->GetSynonyms(bh);
                    syns_initialized = true;
                }
                if ( !syns ) {
                    x_SearchTSE(tse_it->second, master_id,
                                master_range, 0);
                }
                else {
                    ITERATE(CSynonymsSet, syn_it, *syns) {
                        x_SearchTSE(tse_it->second,
                                    syns->GetSeq_id_Handle(syn_it),
                                    master_range, 0);
                        if ( x_NoMoreObjects() ) {
                            break;
                        }
                    }
                }
            }
            else {
                const CBioseq_Handle::TId& syns = bh.GetId();
                bool only_gi = tse_info.OnlyGiAnnotIds();
                ITERATE ( CBioseq_Handle::TId, syn_it, syns ) {
                    if ( !only_gi || syn_it->IsGi() ) {
                        x_SearchTSE(tse_it->second, *syn_it,
                                    master_range, 0);
                        if ( x_NoMoreObjects() ) {
                            break;
                        }
                    }
                }
            }
            if ( x_NoMoreObjects() ) {
                break;
            }
        }
    }
}


bool CAnnot_Collector::x_SearchSegments(const CBioseq_Handle& bh,
                                        const CSeq_id_Handle& master_id,
                                        const CHandleRange& master_range,
                                        CSeq_loc& master_loc_empty,
                                        int level)
{
    _ASSERT(m_Selector->m_ResolveMethod != m_Selector->eResolve_None);
    CSeqMap::TFlags flags = CSeqMap::fFindRef | CSeqMap::fFindExactLevel;
    if ( m_Selector->m_UnresolvedFlag != SAnnotSelector::eFailUnresolved ) {
        flags |= CSeqMap::fIgnoreUnresolved;
    }
    SSeqMapSelector sel(flags, level-1);
    if ( m_Selector->m_ResolveMethod == SAnnotSelector::eResolve_TSE ) {
        sel.SetLimitTSE(bh.GetTSE_Handle());
    }
    
    int depth = m_Selector->GetResolveDepth();
    bool depth_is_set = depth >= 0 && depth < kMax_Int;
    bool exact_depth = m_Selector->GetExactDepth() && depth_is_set;
    int adaptive_flags = exact_depth? 0: m_Selector->GetAdaptiveDepthFlags();
    if ( adaptive_flags & SAnnotSelector::fAdaptive_ByPolicy ) {
        sel.SetByFeaturePolicy();
    }

    bool has_more = false;
    const CRange<TSeqPos>& range = master_range.begin()->first;
    for ( CSeqMap_CI smit(bh, sel, range);
          smit && smit.GetPosition() < range.GetToOpen();
          ++smit ) {
        _ASSERT(smit.GetType() == CSeqMap::eSeqRef);
        if ( !CanResolveId(smit.GetRefSeqid(), bh) ) {
            // External bioseq, try to search if limit is set
            if ( m_Selector->m_UnresolvedFlag !=
                 SAnnotSelector::eSearchUnresolved  ||
                 !m_Selector->m_LimitObject ) {
                // Do not try to search on external segments
                continue;
            }
        }

        has_more = true;
        x_SearchMapped(smit, master_loc_empty, master_id, master_range);

        if ( x_NoMoreObjects() ) {
            return has_more;
        }
    }
    return has_more;
}


static inline
CScope::EGetBioseqFlag sx_GetFlag(const SAnnotSelector& selector)
{
    switch (selector.GetResolveMethod()) {
    case SAnnotSelector::eResolve_All:
        return CScope::eGetBioseq_All;
    default:
        // Do not load new TSEs
        return CScope::eGetBioseq_Loaded;
    }
}


bool CAnnot_Collector::x_SearchSegments(const CHandleRangeMap& master_loc,
                                        int level)
{
    bool has_more = false;
    ITERATE ( CHandleRangeMap::TLocMap, idit, master_loc.GetMap() ) {
        CBioseq_Handle bh =
            m_Scope->GetBioseqHandle(idit->first, sx_GetFlag(GetSelector()));
        if ( !bh ) {
            if (m_Selector->m_UnresolvedFlag == SAnnotSelector::eFailUnresolved) {
                // resolve by Seq-id only
                NCBI_THROW(CAnnotException, eFindFailed,
                           "Cannot resolve master id");
            }
            // skip unresolvable IDs
            continue;
        }
        
        if ( !bh.GetSeqMap().HasSegmentOfType(CSeqMap::eSeqRef) ) {
            continue;
        }
        
        CRef<CSeq_loc> master_loc_empty(new CSeq_loc);
        master_loc_empty->SetEmpty(
            const_cast<CSeq_id&>(*idit->first.GetSeqId()));
                
        CSeqMap::TFlags flags = CSeqMap::fFindRef | CSeqMap::fFindExactLevel;
        if ( m_Selector->m_UnresolvedFlag != m_Selector->eFailUnresolved ) {
            flags |= CSeqMap::fIgnoreUnresolved;
        }

        SSeqMapSelector sel(flags, level-1);
        if ( m_Selector->m_ResolveMethod == SAnnotSelector::eResolve_TSE ) {
            sel.SetLimitTSE(bh.GetTSE_Handle());
        }
        
        int depth = m_Selector->GetResolveDepth();
        bool depth_is_set = depth >= 0 && depth < kMax_Int;
        bool exact_depth = m_Selector->GetExactDepth() && depth_is_set;
        int adaptive_flags = exact_depth?0:m_Selector->GetAdaptiveDepthFlags();
        if ( adaptive_flags & SAnnotSelector::fAdaptive_ByPolicy ) {
            sel.SetByFeaturePolicy();
        }

        CHandleRange::TRange range = idit->second.GetOverlappingRange();
        for ( CSeqMap_CI smit(bh, sel, range);
              smit && smit.GetPosition() < range.GetToOpen();
              ++smit ) {
            _ASSERT(smit.GetType() == CSeqMap::eSeqRef);
            if ( !CanResolveId(smit.GetRefSeqid(), bh) ) {
                // External bioseq, try to search if limit is set
                if ( m_Selector->m_UnresolvedFlag !=
                     SAnnotSelector::eSearchUnresolved  ||
                     !m_Selector->m_LimitObject ) {
                    // Do not try to search on external segments
                    continue;
                }
            }

            has_more = true;
            x_SearchMapped(smit, *master_loc_empty, idit->first, idit->second);

            if ( x_NoMoreObjects() ) {
                return has_more;
            }
        }
    }
    return has_more;
}

void CAnnot_Collector::x_AddTSE(const CTSE_Handle& tse)
{
    const CTSE_Info* key = &tse.x_GetTSE_Info();
    _ASSERT(key);
    TTSE_LockMap::iterator iter = m_TSE_LockMap.lower_bound(key);
    if ( iter == m_TSE_LockMap.end() || iter->first != key ) {
        iter = m_TSE_LockMap.insert(iter, TTSE_LockMap::value_type(key, tse));
    }
    _ASSERT(iter != m_TSE_LockMap.end());
    _ASSERT(iter->first == key);
    _ASSERT(iter->second == tse);
}



struct SLessByInfo
{
    bool operator()(const CSeq_annot_Handle& a,
                    const CSeq_annot_Handle& b) const
        {
            return &a.x_GetInfo() < &b.x_GetInfo();
        }
    bool operator()(const CSeq_annot_Handle& a,
                    const CSeq_annot_Info* b) const
        {
            return &a.x_GetInfo() < b;
        }
    bool operator()(const CSeq_annot_Info* a,
                    const CSeq_annot_Handle& b) const
        {
            return a < &b.x_GetInfo();
        }
    bool operator()(const CSeq_annot_Info* a,
                    const CSeq_annot_Info* b) const
        {
            return a < b;
        }
};


void CAnnot_Collector::x_AddObject(CAnnotObject_Ref& ref)
{
    m_AnnotSet.push_back(ref);
}


inline
void CAnnot_Collector::x_AddObject(CAnnotObject_Ref&    object_ref,
                                   CSeq_loc_Conversion* cvt,
                                   unsigned int         loc_index)
{
    // Always map aligns through conv. set
    if ( (cvt && cvt->IsPartial()) || object_ref.IsAlign() ) {
        x_AddObjectMapping(object_ref, cvt, loc_index);
    }
    else {
        x_AddObject(object_ref);
    }
}


void CAnnot_Collector::x_AddPostMappings(void)
{
    if ( !m_MappingCollector.get() ) {
        return;
    }
    NON_CONST_ITERATE(CAnnotMappingCollector::TAnnotMappingSet, amit,
                      m_MappingCollector->m_AnnotMappingSet) {
        CAnnotObject_Ref annot_ref = amit->first;
        if ( !amit->second ) {
            // no actual mapping, just filtering duplicates
            x_AddObject(annot_ref);
            continue;
        }
        amit->second->Convert(annot_ref,
            m_Selector->m_FeatProduct ? CSeq_loc_Conversion::eProduct :
                                        CSeq_loc_Conversion::eLocation);
        if ( annot_ref.IsAlign() ||
             !annot_ref.GetMappingInfo().GetTotalRange().Empty() ) {
            x_AddObject(annot_ref);
        }
    }
    m_MappingCollector->m_AnnotMappingSet.clear();
    m_MappingCollector.reset();
}


void CAnnot_Collector::x_Initialize(const SAnnotSelector& selector)
{
    CScope_Impl::TConfReadLockGuard guard(m_Scope->m_ConfLock);
    x_Initialize0(selector);
    // Limit must be set, resolving is obsolete
    _ASSERT(m_Selector->m_LimitObjectType != SAnnotSelector::eLimit_None);
    _ASSERT(m_Selector->m_LimitObject);
    _ASSERT(m_Selector->m_ResolveMethod == SAnnotSelector::eResolve_None);
    x_SearchAll();
    x_Sort();
}


void CAnnot_Collector::x_Sort(void)
{
    _ASSERT(!m_MappingCollector.get());
    switch ( m_Selector->m_SortOrder ) {
    case SAnnotSelector::eSortOrder_Normal:
        stable_sort(m_AnnotSet.begin(), m_AnnotSet.end(),
             CAnnotObject_Less(m_Selector, m_Scope));
        break;
    case SAnnotSelector::eSortOrder_Reverse:
        stable_sort(m_AnnotSet.begin(), m_AnnotSet.end(),
             CAnnotObject_LessReverse(m_Selector, m_Scope));
        break;
    default:
        // do nothing
        break;
    }
}


bool
CAnnot_Collector::x_MatchLimitObject(const CAnnotObject_Info& object) const
{
    if ( m_Selector->m_LimitObjectType != SAnnotSelector::eLimit_None ) {
        const CObject* limit = &*m_Selector->m_LimitObject;
        switch ( m_Selector->m_LimitObjectType ) {
        case SAnnotSelector::eLimit_TSE_Info:
        {{
            const CTSE_Info* info = &object.GetTSE_Info();
            _ASSERT(info);
            return info == limit;
        }}
        case SAnnotSelector::eLimit_Seq_entry_Info:
        {{
            const CSeq_entry_Info* info = &object.GetSeq_entry_Info();
            _ASSERT(info);
            for ( ;; ) {
                if ( info == limit ) {
                    return true;
                }
                if ( !info->HasParent_Info() ) {
                    return false;
                }
                info = &info->GetParentSeq_entry_Info();
            }
        }}
        case SAnnotSelector::eLimit_Seq_annot_Info:
        {{
            const CSeq_annot_Info* info = &object.GetSeq_annot_Info();
            _ASSERT(info);
            return info == limit;
        }}
        default:
            NCBI_THROW(CAnnotException, eLimitError,
                       "CAnnot_Collector::x_MatchLimitObject: invalid mode");
        }
    }
    return true;
}


inline
bool CAnnot_Collector::x_MatchLocIndex(const SAnnotObject_Index& index) const
{
    return index.m_AnnotObject_Info->IsAlign()  ||
        m_Selector->m_FeatProduct == (index.m_AnnotLocationIndex == 1);
}


inline
bool CAnnot_Collector::x_MatchRange(const CHandleRange&       hr,
                                    const CRange<TSeqPos>&    range,
                                    const SAnnotObject_Index& index) const
{
    if ( m_Selector->m_OverlapType == SAnnotSelector::eOverlap_Intervals ) {
        if ( index.m_HandleRange ) {
            if (m_Selector->m_IgnoreStrand) {
                if ( !hr.IntersectingWith_NoStrand(*index.m_HandleRange) ) {
                    return false;
                }
            }
            else {
                if ( !hr.IntersectingWith(*index.m_HandleRange) ) {
                    return false;
                }
            }
        }
        else {
            ENa_strand strand;
            if (m_Selector->m_IgnoreStrand) {
                strand = eNa_strand_unknown;
            }
            else {
                switch ( index.m_Flags & SAnnotObject_Index::fStrand_both ) {
                case SAnnotObject_Index::fStrand_plus:
                    strand = eNa_strand_plus;
                    break;
                case SAnnotObject_Index::fStrand_minus:
                    strand = eNa_strand_minus;
                    break;
                default:
                    strand = eNa_strand_unknown;
                    break;
                }
            }
            if ( !hr.IntersectingWith(range, strand) ) {
                return false;
            }
        }
    }
    else {
        if ( !m_Selector->m_IgnoreStrand  &&
            (hr.GetStrandsFlag() & index.m_Flags) == 0 ) {
            return false; // different strands
        }
    }
    if ( !x_MatchLocIndex(index) ) {
        return false;
    }
    return true;
}


void CAnnot_Collector::x_GetTSE_Info(void)
{
    // only one TSE is needed
    _ASSERT(m_TSE_LockMap.empty());
    _ASSERT(m_Selector->m_LimitObjectType != SAnnotSelector::eLimit_None);
    _ASSERT(m_Selector->m_LimitObject);
    
    switch ( m_Selector->m_LimitObjectType ) {
    case SAnnotSelector::eLimit_TSE_Info:
    {
        _ASSERT(m_Selector->m_LimitTSE);
        _ASSERT(CTypeConverter<CTSE_Info>::
                SafeCast(&*m_Selector->m_LimitObject));
        break;
    }
    case SAnnotSelector::eLimit_Seq_entry_Info:
    {
        _ASSERT(m_Selector->m_LimitTSE);
        _ASSERT(CTypeConverter<CSeq_entry_Info>::
                SafeCast(&*m_Selector->m_LimitObject));
        break;
    }
    case SAnnotSelector::eLimit_Seq_annot_Info:
    {
        _ASSERT(m_Selector->m_LimitTSE);
        _ASSERT(CTypeConverter<CSeq_annot_Info>::
                SafeCast(&*m_Selector->m_LimitObject));
        break;
    }
    default:
        NCBI_THROW(CAnnotException, eLimitError,
                   "CAnnot_Collector::x_GetTSE_Info: invalid mode");
    }
    _ASSERT(m_Selector->m_LimitObject);
    _ASSERT(m_Selector->m_LimitTSE);
    x_AddTSE(m_Selector->m_LimitTSE);
}


bool CAnnot_Collector::x_SearchTSE(const CTSE_Handle&    tseh,
                                   const CSeq_id_Handle& id,
                                   const CHandleRange&   hr,
                                   CSeq_loc_Conversion*  cvt)
{
    if ( !m_Selector->m_SourceLoc ) {
        return x_SearchTSE2(tseh, id, hr, cvt);
    }
    const CHandleRangeMap& src_hrm = *m_Selector->m_SourceLoc;
    CHandleRangeMap::const_iterator it = src_hrm.find(id);
    if ( it == src_hrm.end() || !hr.IntersectingWithTotalRange(it->second) ) {
        // non-overlapping loc
        return false;
    }
    CHandleRange hr2(hr, it->second.GetOverlappingRange());
    return !hr2.Empty() && x_SearchTSE2(tseh, id, hr2, cvt);
}


bool CAnnot_Collector::x_SearchTSE2(const CTSE_Handle&    tseh,
                                    const CSeq_id_Handle& id,
                                    const CHandleRange&   hr,
                                    CSeq_loc_Conversion*  cvt)
{
    const CTSE_Info& tse = tseh.x_GetTSE_Info();
    bool found = false;

    tse.UpdateAnnotIndex(id);
    CTSE_Info::TAnnotLockReadGuard guard(tse.GetAnnotLock());

    if (cvt) {
        cvt->SetSrcId(id);
    }
    // Skip excluded TSEs
    //if ( ExcludedTSE(tse) ) {
    //continue;
    //}

    SAnnotSelector::TAdaptiveDepthFlags adaptive_flags = 0;
    if ( !m_Selector->GetExactDepth() ||
         m_Selector->GetResolveDepth() == kMax_Int ) {
        adaptive_flags = m_Selector->GetAdaptiveDepthFlags();
    }
    if ( (adaptive_flags & SAnnotSelector::fAdaptive_ByTriggers) &&
         m_TriggerTypes.any() &&
         tse.ContainsMatchingBioseq(id) ) {
        // first check triggers
        const SIdAnnotObjs* objs = tse.x_GetUnnamedIdObjects(id);
        if ( objs ) {
            for ( size_t index = 0, count = objs->x_GetRangeMapCount();
                  index < count; ++index ) {
                if ( objs->x_RangeMapIsEmpty(index) ) {
                    continue;
                }
                if ( m_TriggerTypes.test(index) ) {
                    m_UnseenAnnotTypes.reset();
                    found = true;
                    break;
                }
            }
        }
    }
    if ( (adaptive_flags & SAnnotSelector::fAdaptive_BySubtypes) &&
         m_UnseenAnnotTypes.any() ) {
        ITERATE (CTSE_Info::TNamedAnnotObjs, iter, tse.m_NamedAnnotObjs) {
            const SIdAnnotObjs* objs =
                tse.x_GetIdObjects(iter->second, id);
            if ( objs ) {
                for ( size_t index = 0, count = objs->x_GetRangeMapCount();
                      index < count; ++index ) {
                    if ( !objs->x_RangeMapIsEmpty(index) ) {
                        m_UnseenAnnotTypes.reset(index);
                    }
                }
            }
        }
    }
    
    if ( !m_Selector->m_IncludeAnnotsNames.empty() ) {
        // only 'included' annots
        ITERATE ( SAnnotSelector::TAnnotsNames, iter,
                  m_Selector->m_IncludeAnnotsNames ) {
            if ( m_Selector->ExcludedAnnotName(*iter) ) {
                // it may happen e.g. when another zoom level is selected
                continue;
            }
            const SIdAnnotObjs* objs = tse.x_GetIdObjects(*iter, id);
            if ( objs ) {
                x_SearchObjects(tseh, objs, guard, *iter, id, hr, cvt);
                if ( x_NoMoreObjects() ) {
                    return found;
                }
            }
        }
    }
    else {
        // all annots, skipping 'excluded'
        ITERATE (CTSE_Info::TNamedAnnotObjs, iter, tse.m_NamedAnnotObjs) {
            if ( m_Selector->ExcludedAnnotName(iter->first) ) {
                continue;
            }
            const SIdAnnotObjs* objs = tse.x_GetIdObjects(iter->second, id);
            if ( objs ) {
                x_SearchObjects(tseh, objs, guard, iter->first, id, hr, cvt);
                if ( x_NoMoreObjects() ) {
                    return found;
                }
            }
        }
    }
    return found;
}


void CAnnot_Collector::x_AddObjectMapping(CAnnotObject_Ref&    object_ref,
                                          CSeq_loc_Conversion* cvt,
                                          unsigned int         loc_index)
{
    if ( cvt ) {
        // reset current mapping info, it will be updated by conversion set
        object_ref.ResetLocation();
    }
    if ( !m_MappingCollector.get() ) {
        m_MappingCollector.reset(new CAnnotMappingCollector);
    }
    CRef<CSeq_loc_Conversion_Set>& mapping_set =
        m_MappingCollector->m_AnnotMappingSet[object_ref];
    if ( cvt ) {
        if ( !mapping_set ) {
            mapping_set.Reset(new CSeq_loc_Conversion_Set(m_Scope));
        }
        _ASSERT(cvt->IsPartial() || object_ref.IsAlign());
        CRef<CSeq_loc_Conversion> cvt_copy(new CSeq_loc_Conversion(*cvt));
        mapping_set->Add(*cvt_copy, loc_index);
    }
}


static bool sx_IsEmpty(const SAnnotSelector& sel)
{
    if ( sel.GetAnnotType() != CSeq_annot::C_Data::e_not_set ) {
        return false;
    }
    return true;
}


void CAnnot_Collector::x_SearchObjects(const CTSE_Handle&    tseh,
                                       const SIdAnnotObjs*   objs,
                                       CTSE_Info::TAnnotLockReadGuard& guard,
                                       const CAnnotName&     annot_name,
                                       const CSeq_id_Handle& id,
                                       const CHandleRange&   hr,
                                       CSeq_loc_Conversion*  cvt)
{
    if ( m_Selector->m_CollectNames ) {
        if ( m_AnnotNames->find(annot_name) != m_AnnotNames->end() ) {
            // already found
            return;
        }
        if ( sx_IsEmpty(*m_Selector) ) {
            // no search for individual annotations
            // just remember the name and leave
            m_AnnotNames->insert(annot_name);
            return;
        }
    }

    if ( m_CollectAnnotTypes.any() ) {
        x_SearchRange(tseh, objs, guard, annot_name, id, hr, cvt);
        if ( x_NoMoreObjects() ) {
            return;
        }
    }

    static const size_t kAnnotTypeIndex_SNP =
        CAnnotType_Index::GetSubtypeIndex(CSeqFeatData::eSubtype_variation);

    if ( m_CollectAnnotTypes.test(kAnnotTypeIndex_SNP) ) {
        if ( m_Selector->m_CollectTypes &&
             m_AnnotTypes.test(kAnnotTypeIndex_SNP) ) {
            return;
        }
        CSeq_annot_Handle sah;
        CHandleRange::TRange range = hr.GetOverlappingRange();
        ITERATE ( CTSE_Info::TSNPSet, snp_annot_it, objs->m_SNPSet ) {
            const CSeq_annot_SNP_Info& snp_annot = **snp_annot_it;
            CSeq_annot_SNP_Info::const_iterator snp_it =
                snp_annot.FirstIn(range);
            if ( snp_it != snp_annot.end() ) {
                //x_AddTSE(tseh);
                const CSeq_annot_Info& annot_info =
                    snp_annot.GetParentSeq_annot_Info();
                if ( !sah || &sah.x_GetInfo() != &annot_info ) {
                    sah.x_Set(annot_info, tseh);
                }
                
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

                    if (m_Selector->m_CollectTypes) {
                        m_AnnotTypes.set(kAnnotTypeIndex_SNP);
                        break;
                    }
                    if (m_Selector->m_CollectNames) {
                        m_AnnotNames->insert(annot_name);
                        break;
                    }
                    
                    CAnnotObject_Ref annot_ref(snp_annot, sah, snp, cvt);
                    x_AddObject(annot_ref);
                    if ( x_NoMoreObjects() ) {
                        return;
                    }
                    if ( m_Selector->m_CollectSeq_annots ) {
                        // Ignore multiple SNPs from the same seq-annot
                        break;
                    }
                } while ( ++snp_it != snp_annot.end() );
            }
        }
    }
}


void CAnnot_Collector::x_SearchRange(const CTSE_Handle&    tseh,
                                     const SIdAnnotObjs*   objs,
                                     CTSE_Info::TAnnotLockReadGuard& guard,
                                     const CAnnotName&     annot_name,
                                     const CSeq_id_Handle& id,
                                     const CHandleRange&   hr,
                                     CSeq_loc_Conversion*  cvt)
{
    const CTSE_Info& tse = tseh.x_GetTSE_Info();
    _ASSERT(objs);

    // CHandleRange::TRange range = hr.GetOverlappingRange();

    //x_AddTSE(tseh);
    CSeq_annot_Handle sah;

    size_t from_idx = 0;
    bool enough = false;

    typedef vector<const CTSE_Chunk_Info*> TStubs;
    typedef map<const CTSE_Split_Info*, CTSE_Split_Info::TChunkIds> TStubMap;
    TStubs stubs;
    do {
        if ( !stubs.empty() ) {
            _ASSERT(!enough);

            TStubMap stubmap;
            ITERATE ( TStubs, it, stubs ) {
                const CTSE_Chunk_Info& chunk = **it;
                stubmap[&chunk.GetSplitInfo()].
                    push_back(chunk.GetChunkId());
            }
            stubs.clear();

            // Release lock for tse update:
            guard.Release();
            ITERATE(TStubMap, it, stubmap) {
                if ( m_Selector->m_MaxSize != kMax_UInt ) {
                    it->first->LoadChunk(*it->second.begin());
                    break;
                }
                it->first->LoadChunks(it->second);
            }
            tse.UpdateAnnotIndex(id);

            // Acquire the lock again:
            guard.Guard(tse.GetAnnotLock());

            // Reget range map pointer as it may change:
            objs = tse.x_GetIdObjects(annot_name, id);
            _ASSERT(objs);
        }
        for ( size_t index = from_idx, count = objs->x_GetRangeMapCount();
              index < count; ++index ) {
            if ( m_Selector->m_CollectTypes && m_AnnotTypes.test(index)) {
                continue;
            }
            if ( !m_CollectAnnotTypes.test(index) ) {
                continue;
            }
            
            if ( objs->x_RangeMapIsEmpty(index) ) {
                continue;
            }
            const CTSE_Info::TRangeMap& rmap = objs->x_GetRangeMap(index);

            size_t start_size = m_AnnotSet.size(); // for rollback

            bool need_unique = false;

            ITERATE(CHandleRange, rg_it, hr) {
                CHandleRange::TRange range = rg_it->first;

                for ( CTSE_Info::TRangeMap::const_iterator
                    aoit(rmap.begin(range));
                    aoit; ++aoit ) {
                    const CAnnotObject_Info& annot_info =
                        *aoit->second.m_AnnotObject_Info;

                    // Collect types
                    if (m_Selector->m_CollectTypes) {
                        if (x_MatchLimitObject(annot_info)  &&
                            x_MatchRange(hr, aoit->first, aoit->second) ) {
                            m_AnnotTypes.set(index);
                            break;
                        }
                    }
                    if (m_Selector->m_CollectNames) {
                        if (x_MatchLimitObject(annot_info)  &&
                            x_MatchRange(hr, aoit->first, aoit->second) ) {
                            m_AnnotNames->insert(annot_name);
                            return;
                        }
                    }

                    if ( annot_info.IsChunkStub() ) {
                        const CTSE_Chunk_Info& chunk = annot_info.GetChunk_Info();
                        if ( !chunk.NotLoaded() ) {
                            // Skip chunk stub
                            continue;
                        }
                        if ( stubs.empty() ) {
                            // New annot objects are to be loaded,
                            // so we'll need to restart scan of current range.
                            // Forget already found objects
                            // as they will be found again:
                            m_AnnotSet.resize(start_size);
                            // Update start index for the new search
                            from_idx = index;
                        }
                        stubs.push_back(&chunk);
                    }
                    if ( !stubs.empty() ) {
                        _ASSERT(!enough);
                        continue;
                    }

                    if ( annot_info.IsLocs() ) {
                        const CSeq_loc& ref_loc = annot_info.GetLocs();

                        // Check if the stub has been already processed
                        if ( m_AnnotLocsSet.get() ) {
                            CConstRef<CSeq_loc> ploc(&ref_loc);
                            TAnnotLocsSet::const_iterator found =
                                m_AnnotLocsSet->find(ploc);
                            if (found != m_AnnotLocsSet->end()) {
                                continue;
                            }
                        }
                        else {
                            m_AnnotLocsSet.reset(new TAnnotLocsSet);
                        }
                        m_AnnotLocsSet->insert(ConstRef(&ref_loc));

                        // Search annotations on the referenced location
                        if ( !ref_loc.IsInt() ) {
                            ERR_POST_X(1, "CAnnot_Collector: "
                                       "Seq-annot.locs is not Seq-interval");
                            continue;
                        }
                        const CSeq_interval& ref_int = ref_loc.GetInt();
                        const CSeq_id& ref_id = ref_int.GetId();
                        CSeq_id_Handle ref_idh = CSeq_id_Handle::GetHandle(ref_id);
                        // check ResolveTSE limit
                        if ( m_Selector->m_ResolveMethod == SAnnotSelector::eResolve_TSE ) {
                            if ( !tseh.GetBioseqHandle(ref_idh) ) {
                                continue;
                            }
                        }

                        // calculate ranges
                        TSeqPos ref_from = ref_int.GetFrom();
                        TSeqPos ref_to = ref_int.GetTo();
                        bool ref_minus = ref_int.IsSetStrand()?
                            IsReverse(ref_int.GetStrand()) : false;
                        TSeqPos loc_from = aoit->first.GetFrom();
                        TSeqPos loc_to = aoit->first.GetTo();
                        TSeqPos loc_view_from = max(range.GetFrom(), loc_from);
                        TSeqPos loc_view_to = min(range.GetTo(), loc_to);

                        CHandleRangeMap ref_rmap;
                        CHandleRange::TRange ref_search_range;
                        if ( !ref_minus ) {
                            ref_search_range.Set(ref_from + (loc_view_from - loc_from),
                                                ref_to + (loc_view_to - loc_to));
                        }
                        else {
                            ref_search_range.Set(ref_from - (loc_view_to - loc_to),
                                                ref_to - (loc_view_from - loc_from));
                        }
                        ref_rmap.AddRanges(ref_idh).AddRange(ref_search_range,
                                                            eNa_strand_unknown);

                        if (m_Selector->m_NoMapping) {
                            x_SearchLoc(ref_rmap, 0, &tseh);
                        }
                        else {
                            CRef<CSeq_loc> master_loc_empty(new CSeq_loc);
                            master_loc_empty->SetEmpty(
                                const_cast<CSeq_id&>(*id.GetSeqId()));
                            CRef<CSeq_loc_Conversion> locs_cvt(new CSeq_loc_Conversion(
                                    *master_loc_empty,
                                    id,
                                    aoit->first,
                                    ref_idh,
                                    ref_from,
                                    ref_minus,
                                    m_Scope));
                            if ( cvt ) {
                                locs_cvt->CombineWith(*cvt);
                            }
                            x_SearchLoc(ref_rmap, &*locs_cvt, &tseh);
                        }
                        if ( x_NoMoreObjects() ) {
                            _ASSERT(stubs.empty());
                            enough = true;
                            break;
                        }
                        continue;
                    }

                    _ASSERT(m_Selector->MatchType(annot_info));

                    if ( !x_MatchLimitObject(annot_info) ) {
                        continue;
                    }
                        
                    if ( !x_MatchRange(hr, aoit->first, aoit->second) ) {
                        continue;
                    }

                    bool is_circular = aoit->second.m_HandleRange  &&
                        aoit->second.m_HandleRange->GetData().IsCircular();
                    need_unique |= is_circular;
                    const CSeq_annot_Info& sa_info =
                        annot_info.GetSeq_annot_Info();
                    if ( !sah || &sah.x_GetInfo() != &sa_info ){
                        sah.x_Set(sa_info, tseh);
                    }

                    CAnnotObject_Ref annot_ref(annot_info, sah);
                    if ( !cvt  &&  aoit->second.GetMultiIdFlag() ) {
                        // Create self-conversion, add to conversion set
                        CHandleRange::TRange ref_rg = aoit->first;
                        if (is_circular ) {
                            TSeqPos from = aoit->second.m_HandleRange->
                                GetData().GetLeft();
                            TSeqPos to =aoit->second.m_HandleRange->
                                GetData().GetRight();
                            ref_rg = CHandleRange::TRange(from, to);
                        }
                        annot_ref.GetMappingInfo().SetAnnotObjectRange(ref_rg,
                                m_Selector->m_FeatProduct);
                        x_AddObjectMapping(annot_ref, 0,
                                           aoit->second.m_AnnotLocationIndex);
                    }
                    else {
                        if (cvt  &&  !annot_ref.IsAlign() ) {
                            cvt->Convert(annot_ref,
                                         m_Selector->m_FeatProduct ?
                                         CSeq_loc_Conversion::eProduct :
                                         CSeq_loc_Conversion::eLocation,
                                         id,
                                         aoit->first,
                                         aoit->second);
                        }
                        else {
                            CHandleRange::TRange ref_rg = aoit->first;
                            if ( is_circular ) {
                                TSeqPos from = aoit->second.m_HandleRange->
                                    GetData().GetLeft();
                                TSeqPos to = aoit->second.m_HandleRange->
                                    GetData().GetRight();
                                ref_rg = CHandleRange::TRange(from, to);
                            }
                            annot_ref.GetMappingInfo().SetAnnotObjectRange(ref_rg,
                                m_Selector->m_FeatProduct);
                        }
                        x_AddObject(annot_ref, cvt,
                                    aoit->second.m_AnnotLocationIndex);
                    }
                    if ( x_NoMoreObjects() ) {
                        _ASSERT(stubs.empty());
                        enough = true;
                        break;
                    }
                }
                if ( enough ) {
                    _ASSERT(stubs.empty());
                    break;
                }
                if ( !stubs.empty() ) {
                    _ASSERT(!enough);
                    continue;
                }
            }
            if ( !stubs.empty() ) {
                _ASSERT(!enough);
                continue;
            }
            if ( need_unique  ||  hr.end() - hr.begin() > 1 ) {
                TAnnotSet::iterator first_added = m_AnnotSet.begin() + start_size;
                stable_sort(first_added, m_AnnotSet.end());
                m_AnnotSet.erase(unique(first_added, m_AnnotSet.end()),
                                m_AnnotSet.end());
            }
            if ( enough ) {
                _ASSERT(stubs.empty());
                break;
            }
        }
        if ( enough ) {
            _ASSERT(stubs.empty());
            break;
        }
    } while ( !stubs.empty() );
}


bool CAnnot_Collector::x_SearchLoc(const CHandleRangeMap& loc,
                                   CSeq_loc_Conversion*   cvt,
                                   const CTSE_Handle*     using_tse,
                                   bool top_level)
{
    bool found = false;
    ITERATE ( CHandleRangeMap, idit, loc ) {
        if ( idit->second.Empty() ) {
            continue;
        }
        if ( m_Selector->m_LimitObjectType == SAnnotSelector::eLimit_None ) {
            // any data source
            const CTSE_Handle* tse = 0;
            CScope::EGetBioseqFlag flag =
                top_level? CScope::eGetBioseq_All: sx_GetFlag(GetSelector());
            CBioseq_Handle bh = m_Scope->GetBioseqHandle(idit->first, flag);
            if ( !bh ) {
                if ( m_Selector->m_UnresolvedFlag ==
                    SAnnotSelector::eFailUnresolved ) {
                    NCBI_THROW(CAnnotException, eFindFailed,
                               "Cannot find id synonyms");
                }
                if ( m_Selector->m_UnresolvedFlag ==
                    SAnnotSelector::eIgnoreUnresolved ) {
                    continue; // skip unresolvable IDs
                }
                tse = using_tse;
            }
            else {
                tse = &bh.GetTSE_Handle();
                if ( using_tse ) {
                    using_tse->AddUsedTSE(*tse);
                }
            }
            if ( m_Selector->m_ExcludeExternal ) {
                if ( !bh ) {
                    // no sequence tse
                    continue;
                }
                _ASSERT(tse);
                const CTSE_Info& tse_info = tse->x_GetTSE_Info();
                tse_info.UpdateAnnotIndex();
                if ( tse_info.HasMatchingAnnotIds() ) {
                    CConstRef<CSynonymsSet> syns = m_Scope->GetSynonyms(bh);
                    ITERATE(CSynonymsSet, syn_it, *syns) {
                        found |= x_SearchTSE(*tse,
                                             syns->GetSeq_id_Handle(syn_it),
                                             idit->second, cvt);
                        if ( x_NoMoreObjects() ) {
                            break;
                        }
                    }
                }
                else {
                    const CBioseq_Handle::TId& syns = bh.GetId();
                    bool only_gi = tse_info.OnlyGiAnnotIds();
                    ITERATE ( CBioseq_Handle::TId, syn_it, syns ) {
                        if ( !only_gi || syn_it->IsGi() ) {
                            found |= x_SearchTSE(*tse, *syn_it,
                                                 idit->second, cvt);
                            if ( x_NoMoreObjects() ) {
                                break;
                            }
                        }
                    }
                }
            }
            else {
                CScope_Impl::TTSE_LockMatchSet tse_map;
                if ( m_Selector->IsIncludedAnyNamedAnnotAccession() ) {
                    m_Scope->GetTSESetWithAnnots(idit->first, tse_map,
                                                 *m_Selector);
                }
                else {
                    m_Scope->GetTSESetWithAnnots(idit->first, tse_map);
                }
                ITERATE ( CScope_Impl::TTSE_LockMatchSet, tse_it, tse_map ) {
                    if ( tse ) {
                        tse->AddUsedTSE(tse_it->first);
                    }
                    found |= x_SearchTSE(tse_it->first, tse_it->second,
                                         idit->second, cvt);
                    if ( x_NoMoreObjects() ) {
                        break;
                    }
                }
            }
        }
        else if ( m_Selector->m_UnresolvedFlag == SAnnotSelector::eSearchUnresolved &&
                  m_Selector->m_ResolveMethod == SAnnotSelector::eResolve_TSE &&
                  m_Selector->m_LimitObjectType == SAnnotSelector::eLimit_TSE_Info &&
                  m_Selector->m_LimitObject ) {
            // external annotations only
            ITERATE ( TTSE_LockMap, tse_it, m_TSE_LockMap ) {
                const CTSE_Info& tse_info = *tse_it->first;
                tse_info.UpdateAnnotIndex();
                found |= x_SearchTSE(tse_it->second, idit->first,
                                     idit->second, cvt);
            }
        }
        else {
            // Search in the limit objects
            CConstRef<CSynonymsSet> syns;
            bool syns_initialized = false;
            ITERATE ( TTSE_LockMap, tse_it, m_TSE_LockMap ) {
                const CTSE_Info& tse_info = *tse_it->first;
                tse_info.UpdateAnnotIndex();
                if ( tse_info.HasMatchingAnnotIds() ) {
                    if ( !syns_initialized ) {
                        syns = m_Scope->GetSynonyms(idit->first,
                                                    sx_GetFlag(GetSelector()));
                        syns_initialized = true;
                    }
                    if ( !syns ) {
                        found |= x_SearchTSE(tse_it->second, idit->first,
                                             idit->second, cvt);
                    }
                    else {
                        ITERATE(CSynonymsSet, syn_it, *syns) {
                            found |= x_SearchTSE(tse_it->second,
                                                 syns->GetSeq_id_Handle(syn_it),
                                                 idit->second, cvt);
                            if ( x_NoMoreObjects() ) {
                                break;
                            }
                        }
                    }
                }
                else {
                    const CBioseq_Handle::TId& syns =
                        m_Scope->GetIds(idit->first);
                    bool only_gi = tse_info.OnlyGiAnnotIds();
                    ITERATE ( CBioseq_Handle::TId, syn_it, syns ) {
                        if ( !only_gi || syn_it->IsGi() ) {
                            found |= x_SearchTSE(tse_it->second, *syn_it,
                                                 idit->second, cvt);
                            if ( x_NoMoreObjects() ) {
                                break;
                            }
                        }
                    }
                }
                if ( x_NoMoreObjects() ) {
                    break;
                }
            }
        }
        if ( x_NoMoreObjects() ) {
            break;
        }
    }
    return found;
}


void CAnnot_Collector::x_SearchAll(void)
{
    _ASSERT(m_Selector->m_LimitObjectType != SAnnotSelector::eLimit_None);
    _ASSERT(m_Selector->m_LimitObject);
    if ( m_TSE_LockMap.empty() ) {
        // data source name not matched
        return;
    }
    switch ( m_Selector->m_LimitObjectType ) {
    case SAnnotSelector::eLimit_TSE_Info:
        x_SearchAll(*CTypeConverter<CTSE_Info>::
                    SafeCast(&*m_Selector->m_LimitObject));
        break;
    case SAnnotSelector::eLimit_Seq_entry_Info:
        x_SearchAll(*CTypeConverter<CSeq_entry_Info>::
                    SafeCast(&*m_Selector->m_LimitObject));
        break;
    case SAnnotSelector::eLimit_Seq_annot_Info:
        x_SearchAll(*CTypeConverter<CSeq_annot_Info>::
                    SafeCast(&*m_Selector->m_LimitObject));
        break;
    default:
        NCBI_THROW(CAnnotException, eLimitError,
                   "CAnnot_Collector::x_SearchAll: invalid mode");
    }
}


void CAnnot_Collector::x_SearchAll(const CSeq_entry_Info& entry_info)
{
    {{
        entry_info.UpdateAnnotIndex();
        CConstRef<CBioseq_Base_Info> base = entry_info.m_Contents;
        // Collect all annotations from the entry
        ITERATE( CBioseq_Base_Info::TAnnot, ait, base->GetAnnot() ) {
            x_SearchAll(**ait);
            if ( x_NoMoreObjects() )
                return;
        }
    }}

    if ( entry_info.IsSet() ) {
        CConstRef<CBioseq_set_Info> set(&entry_info.GetSet());
        // Collect annotations from all children
        ITERATE( CBioseq_set_Info::TSeq_set, cit, set->GetSeq_set() ) {
            x_SearchAll(**cit);
            if ( x_NoMoreObjects() )
                return;
        }
    }
}


void CAnnot_Collector::x_SearchAll(const CSeq_annot_Info& annot_info)
{
    if ( !m_Selector->m_IncludeAnnotsNames.empty() ) {
        // only included
        if ( !m_Selector->IncludedAnnotName(annot_info.GetName()) ) {
            return;
        }
    }
    else if ( m_Selector->ExcludedAnnotName(annot_info.GetName()) ) {
        return;
    }

    _ASSERT(m_Selector->m_LimitTSE);
    annot_info.UpdateAnnotIndex();
    CSeq_annot_Handle sah(annot_info, m_Selector->m_LimitTSE);
    // Collect all annotations from the annot
    ITERATE ( CSeq_annot_Info::TAnnotObjectInfos, aoit,
              annot_info.GetAnnotObjectInfos() ) {
        if ( aoit->IsRemoved() ) {
            continue;
        }
        if ( !m_Selector->MatchType(*aoit) ) {
            continue;
        }
        CAnnotObject_Ref annot_ref(*aoit, sah);
        x_AddObject(annot_ref);
        if ( m_Selector->m_CollectSeq_annots || x_NoMoreObjects() ) {
            return;
        }
    }

    static const size_t kAnnotTypeIndex_SNP =
        CAnnotType_Index::GetSubtypeIndex(CSeqFeatData::eSubtype_variation);

    if ( m_CollectAnnotTypes.test(kAnnotTypeIndex_SNP) &&
         annot_info.x_HasSNP_annot_Info() ) {
        const CSeq_annot_SNP_Info& snp_annot =
            annot_info.x_GetSNP_annot_Info();
        TSeqPos index = 0;
        ITERATE ( CSeq_annot_SNP_Info, snp_it, snp_annot ) {
            const SSNP_Info& snp = *snp_it;
            CAnnotObject_Ref annot_ref(snp_annot, sah, snp, 0);
            x_AddObject(annot_ref);
            if ( m_Selector->m_CollectSeq_annots || x_NoMoreObjects() ) {
                return;
            }
            ++index;
        }
    }
}


bool CAnnot_Collector::x_SearchMapped(const CSeqMap_CI&     seg,
                                      CSeq_loc&             master_loc_empty,
                                      const CSeq_id_Handle& master_id,
                                      const CHandleRange&   master_hr)
{
    CHandleRange::TOpenRange master_seg_range(
        seg.GetPosition(),
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
            CHandleRange::TOpenRange range = master_seg_range & mlit->first;
            if ( !range.Empty() ) {
                ENa_strand strand = mlit->second;
                if ( !reversed ) {
                    range.SetOpen(range.GetFrom() + shift,
                                  range.GetToOpen() + shift);
                }
                else {
                    if ( strand != eNa_strand_unknown ) {
                        strand = Reverse(strand);
                    }
                    range.Set(shift - range.GetTo(), shift - range.GetFrom());
                }
                hr.AddRange(range, strand);
            }
        }
        if ( hr.Empty() )
            return false;
    }}

    if (m_Selector->m_NoMapping) {
        return x_SearchLoc(ref_loc, 0, &seg.GetUsingTSE());
    }
    else {
        CRef<CSeq_loc_Conversion> cvt(new CSeq_loc_Conversion(master_loc_empty,
                                                              master_id,
                                                              seg,
                                                              ref_id,
                                                              m_Scope));
        return x_SearchLoc(ref_loc, &*cvt, &seg.GetUsingTSE());
    }
}


const CAnnot_Collector::TAnnotNames&
CAnnot_Collector::x_GetAnnotNames(void) const
{
    if ( !m_AnnotNames.get() ) {
        TAnnotNames* names = new TAnnotNames;
        m_AnnotNames.reset(names);
        ITERATE ( TAnnotSet, it, m_AnnotSet ) {
            names->insert(it->GetSeq_annot_Info().GetName());
        }
    }
    return *m_AnnotNames;
}


END_SCOPE(objects)
END_NCBI_SCOPE
