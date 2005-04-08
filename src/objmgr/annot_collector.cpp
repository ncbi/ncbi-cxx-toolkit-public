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
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/objmgr_exception.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>

#include <serial/typeinfo.hpp>
#include <serial/objostr.hpp>
#include <serial/objostrasn.hpp>
#include <serial/serial.hpp>
#include <serial/serialutil.hpp>

#include <algorithm>
#include <typeinfo>

BEGIN_NCBI_SCOPE
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


const CAnnotObject_Info& CAnnotObject_Ref::GetAnnotObject_Info(void) const
{
    return GetSeq_annot_Info().GetAnnotObject_Info(GetAnnotObjectIndex());
}


const SSNP_Info& CAnnotObject_Ref::GetSNP_Info(void) const
{
    return GetSeq_annot_SNP_Info().GetSNP_Info(GetAnnotObjectIndex());
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


void CAnnotMapping_Info::UpdateMappedSeq_loc(CRef<CSeq_loc>& loc) const
{
    _ASSERT(MappedSeq_locNeedsUpdate());
    
    CSeq_id& id = const_cast<CSeq_id&>(GetMappedSeq_id());
    if ( !loc || !loc->ReferencedOnlyOnce() ) {
        loc.Reset(new CSeq_loc);
    }
    else {
        loc->InvalidateTotalRangeCache();
    }
    if ( IsMappedPoint() ) {
        CSeq_point& point = loc->SetPnt();
        point.SetId(id);
        point.SetPoint(GetFrom());
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
        CSeq_interval& interval = loc->SetInt();
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


void CAnnotMapping_Info::UpdateMappedSeq_loc(CRef<CSeq_loc>& loc,
                                             CRef<CSeq_point>& pnt_ref,
                                             CRef<CSeq_interval>& int_ref) const
{
    _ASSERT(MappedSeq_locNeedsUpdate());

    CSeq_id& id = const_cast<CSeq_id&>(GetMappedSeq_id());
    if ( !loc || !loc->ReferencedOnlyOnce() ) {
        loc.Reset(new CSeq_loc);
    }
    else {
        loc->Reset();
        loc->InvalidateTotalRangeCache();
    }
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


void CAnnotMapping_Info::SetMappedSeq_feat(CSeq_feat& feat)
{
    _ASSERT( IsMapped() );
    _ASSERT(GetMappedObjectType() != eMappedObjType_Seq_feat);

    // Fill mapped location and product in the mapped feature
    CRef<CSeq_loc> mapped_loc;
    if ( MappedSeq_locNeedsUpdate() ) {
        mapped_loc.Reset(new CSeq_loc);
        UpdateMappedSeq_loc(mapped_loc);
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

    if ( IsProduct() ) {
        // Safe to re-use location
        dst.SetLocation(src_nc.SetLocation());
    }
    else {
        // Safe to re-use product
        if ( src_nc.IsSetProduct() ) {
            dst.SetProduct(src_nc.SetProduct());
        }
        else {
            dst.ResetProduct();
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
// CAnnotObject_Ref
/////////////////////////////////////////////////////////////////////////////


CAnnotObject_Ref::CAnnotObject_Ref(const CAnnotObject_Info& object)
    : m_Object(&object.GetSeq_annot_Info()),
      m_AnnotObject_Index(object.GetSeq_annot_Info()
                          .GetAnnotObjectIndex(object)),
      m_ObjectType(eType_Seq_annot_Info)
{
    if ( object.IsFeat() ) {
        const CSeq_feat& feat = *object.GetFeatFast();
        if ( feat.IsSetPartial() ) {
            m_MappingInfo.SetPartial(feat.GetPartial());
        }
    }
}


CAnnotObject_Ref::CAnnotObject_Ref(const CSeq_annot_SNP_Info& snp_annot,
                                   TSeqPos index)
    : m_Object(&snp_annot),
      m_AnnotObject_Index(index),
      m_ObjectType(eType_Seq_annot_SNP_Info)
{
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


bool CAnnotObject_Ref::IsFeat(void) const
{
    return GetObjectType() == eType_Seq_annot_SNP_Info ||
        (GetObjectType() == eType_Seq_annot_Info &&
         GetAnnotObject_Info().IsFeat());
}


bool CAnnotObject_Ref::IsGraph(void) const
{
    return GetObjectType() == eType_Seq_annot_Info &&
        GetAnnotObject_Info().IsGraph();
}


bool CAnnotObject_Ref::IsAlign(void) const
{
    return GetObjectType() == eType_Seq_annot_Info &&
        GetAnnotObject_Info().IsAlign();
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


inline
void CAnnotObject_Ref::SetSNP_Point(const SSNP_Info& snp,
                                    CSeq_loc_Conversion* cvt)
{
    _ASSERT(GetObjectType() == eType_Seq_annot_SNP_Info);
    TSeqPos src_from = snp.GetFrom(), src_to = snp.GetTo();
    ENa_strand src_strand =
        snp.MinusStrand()? eNa_strand_minus: eNa_strand_plus;
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


/////////////////////////////////////////////////////////////////////////////
// CAnnotObject_Ref comparision
/////////////////////////////////////////////////////////////////////////////

struct CAnnotObjectType_Less
{
    bool operator()(const CAnnotObject_Ref& x,
                    const CAnnotObject_Ref& y) const;

    static const CSeq_loc& GetLocation(const CAnnotObject_Ref& ref,
                                       const CSeq_feat& feat);
};


const CSeq_loc& CAnnotObjectType_Less::GetLocation(const CAnnotObject_Ref& ref,
                                                   const CSeq_feat& feat)
{
    CAnnotMapping_Info& map_info = ref.GetMappingInfo();
    if ( map_info.GetMappedObjectType() ==
        CAnnotMapping_Info::eMappedObjType_Seq_loc &&
        !map_info.IsProduct() ) {
        return map_info.GetMappedSeq_loc();
    }
    else {
        return feat.GetLocation();
    }
}


bool CAnnotObjectType_Less::operator()(const CAnnotObject_Ref& x,
                                       const CAnnotObject_Ref& y) const
{
    // gather x annotation type
    const CAnnotObject_Info* x_info;
    CSeq_annot::C_Data::E_Choice x_annot_type;
    if ( !x.IsSNPFeat() ) {
        const CSeq_annot_Info& x_annot = x.GetSeq_annot_Info();
        x_info = &x_annot.GetAnnotObject_Info(x.GetAnnotObjectIndex());
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
        const CSeq_annot_Info& y_annot = y.GetSeq_annot_Info();
        y_info = &y_annot.GetAnnotObject_Info(y.GetAnnotObjectIndex());
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
        // compare features by type
        if ( !x_info != !y_info ) {
            CSeqFeatData::E_Choice x_feat_type = CSeqFeatData::e_Imp;
            CSeqFeatData::ESubtype x_feat_subtype =
                CSeqFeatData::eSubtype_variation;
            if ( x_info ) {
                x_feat_type = x_info->GetFeatType();
                x_feat_subtype = x_info->GetFeatSubtype();
            }

            CSeqFeatData::E_Choice y_feat_type = CSeqFeatData::e_Imp;
            CSeqFeatData::ESubtype y_feat_subtype =
                CSeqFeatData::eSubtype_variation;
            if ( y_info ) {
                y_feat_type = y_info->GetFeatType();
                y_feat_subtype = y_info->GetFeatSubtype();
            }

            // one is simple SNP feature, another is some complex feature
            if ( x_feat_type != y_feat_type ) {
                int x_order = CSeq_feat::GetTypeSortingOrder(x_feat_type);
                int y_order = CSeq_feat::GetTypeSortingOrder(y_feat_type);
                if ( x_order != y_order ) {
                    return x_order < y_order;
                }
            }

            // non-variations first
            if ( x_feat_subtype != y_feat_subtype ) {
                return x_feat_subtype < y_feat_subtype;
            }

            // both are variations but one is simple and another is not.
            // required order: simple first.
            // if !x_info == true -> x - simple, y - complex -> return true
            // if !x_info == false -> x - complex, y - simple -> return false
            return !x_info;
        }

        if ( x_info ) {
            // both are complex features
            try {
                const CSeq_feat* x_feat = x_info->GetFeatFast();
                const CSeq_feat* y_feat = y_info->GetFeatFast();
                _ASSERT(x_feat && y_feat);
                int diff = x_feat->CompareNonLocation(*y_feat,
                                                      GetLocation(x, *x_feat),
                                                      GetLocation(y, *y_feat));
                if ( diff != 0 ) {
                    return diff < 0;
                }
            }
            catch ( exception& /*ignored*/ ) {
                // do not fail sort when compare function throws an exception
            }
        }
    }
    return x < y;
}


struct CAnnotObject_Less
{
    // Compare CRef-s: both must be features
    bool operator()(const CAnnotObject_Ref& x,
                    const CAnnotObject_Ref& y) const
        {
            TSeqPos x_from = x.GetMappingInfo().GetFrom();
            TSeqPos y_from = y.GetMappingInfo().GetFrom();
            TSeqPos x_to = x.GetMappingInfo().GetToOpen();
            TSeqPos y_to = y.GetMappingInfo().GetToOpen();
            // from > to = circular location.
            // Any circular location is less than non-circular one.
            // If both are circular, compare in normal way.
            if ( (x_from > x_to || y_from > y_to)  &&
                    (x_from > x_to) != (y_from > y_to) ) {
                return x.GetMappingInfo().GetTotalRange().Empty();
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
    // Compare CRef-s: both must be features
    bool operator()(const CAnnotObject_Ref& x,
                    const CAnnotObject_Ref& y) const
        {
            {
                // largest right extreme first
                TSeqPos x_to = x.GetMappingInfo().GetToOpen();
                TSeqPos y_to = y.GetMappingInfo().GetToOpen();
                if ( x_to != y_to ) {
                    return x_to > y_to;
                }
            }
            {
                // longest feature first
                TSeqPos x_from = x.GetMappingInfo().GetFrom();
                TSeqPos y_from = y.GetMappingInfo().GetFrom();
                if ( x_from != y_from ) {
                    return x_from < y_from;
                }
            }
            return type_less(x, y);
        }
    CAnnotObjectType_Less type_less;
};


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
CCreatedFeat_Ref::MakeOriginalFeature(const CSeq_feat_Handle& feat_h)
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
    else {
        ret = feat_h.GetSeq_feat();
    }
    return ret;
}


CConstRef<CSeq_loc>
CCreatedFeat_Ref::MakeMappedLocation(const CAnnotMapping_Info& map_info)
{
    CConstRef<CSeq_loc> ret;
    if ( map_info.MappedSeq_locNeedsUpdate() ) {
        // need to covert Seq_id to Seq_loc
        // clear references to mapped location from mapped feature
        // Can not use m_MappedSeq_feat since it's a const-ref
        CRef<CSeq_feat> mapped_feat;
        m_CreatedSeq_feat.AtomicReleaseTo(mapped_feat);
        if ( mapped_feat ) {
            if ( !mapped_feat->ReferencedOnlyOnce() ) {
                mapped_feat.Reset();
            }
            else {
                // hack with null pointer as ResetLocation doesn't reset CRef<>
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
        map_info.UpdateMappedSeq_loc(mapped_loc,
                                     created_point,
                                     created_interval);
        ret = mapped_loc;
        ResetRefsFrom(0, &mapped_loc, &created_point, &created_interval);
    }
    else if ( map_info.IsMapped() ) {
        ret = &map_info.GetMappedSeq_loc();
    }
    return ret;
}


CConstRef<CSeq_feat>
CCreatedFeat_Ref::MakeMappedFeature(const CSeq_feat_Handle& orig_feat,
                                    const CAnnotMapping_Info& map_info,
                                    CSeq_loc& mapped_location)
{
    CConstRef<CSeq_feat> ret;
    if ( map_info.IsMapped() ) {
        if (map_info.GetMappedObjectType() ==
            CAnnotMapping_Info::eMappedObjType_Seq_feat) {
            ret = &map_info.GetMappedSeq_feat();
            return ret;
        }
        // some Seq-loc object is mapped
        CRef<CSeq_feat> mapped_feat;
        m_CreatedSeq_feat.AtomicReleaseTo(mapped_feat);
        if ( !mapped_feat || !mapped_feat->ReferencedOnlyOnce() ) {
            mapped_feat.Reset(new CSeq_feat);
        }
        CConstRef<CSeq_feat> orig_seq_feat = orig_feat.GetSeq_feat();
        map_info.InitializeMappedSeq_feat(*orig_seq_feat, *mapped_feat);

        if ( map_info.IsMappedLocation() ) {
            mapped_feat->SetLocation(mapped_location);
        }
        else if ( map_info.IsMappedProduct() ) {
            mapped_feat->SetProduct(mapped_location);
        }
        if ( map_info.IsPartial() ) {
            mapped_feat->SetPartial(true);
        }
        else {
            mapped_feat->ResetPartial();
        }

        ret = mapped_feat;
        m_CreatedSeq_feat.AtomicResetFrom(mapped_feat);
    }
    else {
         ret = orig_feat.GetSeq_feat();
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


CAnnot_Collector::CAnnot_Collector(TAnnotType& type,
                                   CScope&     scope)
    : m_Selector(type),
      m_Scope(scope),
      m_MappingCollector(new CAnnotMappingCollector),
      m_CreatedOriginal(new CCreatedFeat_Ref),
      m_CreatedMapped(new CCreatedFeat_Ref)
{
    return;
}

CAnnot_Collector::CAnnot_Collector(const SAnnotSelector& selector,
                                   CScope&               scope)
    : m_Selector(selector),
      m_Scope(scope),
      m_MappingCollector(new CAnnotMappingCollector),
      m_CreatedOriginal(new CCreatedFeat_Ref),
      m_CreatedMapped(new CCreatedFeat_Ref)
{
    return;
}

CAnnot_Collector::~CAnnot_Collector(void)
{
    x_Clear();
}


inline
size_t CAnnot_Collector::x_GetAnnotCount(void) const
{
    return m_AnnotSet.size() +
        (m_MappingCollector.get() ?
        m_MappingCollector->m_AnnotMappingSet.size() : 0);
}


CSeq_annot_Handle CAnnot_Collector::GetAnnot(const CAnnotObject_Ref& ref) const
{
    const CSeq_annot_Info* info;
    if ( ref.GetObjectType() == ref.eType_Seq_annot_SNP_Info ) {
        info = &ref.GetSeq_annot_SNP_Info().GetParentSeq_annot_Info();
    }
    else {
        info = &ref.GetSeq_annot_Info();
    }
    // return GetScope().GetSeq_annotHandle(*info->GetSeq_annotCore());

    TTSE_LockMap::const_iterator tse_it =
        m_TSE_LockMap.find(&info->GetTSE_Info());
    _ASSERT(tse_it != m_TSE_LockMap.end());
    return CSeq_annot_Handle(*info, tse_it->second);
}


void CAnnot_Collector::x_Clear(void)
{
    m_AnnotSet.clear();
    if ( m_MappingCollector.get() ) {
        m_MappingCollector.reset();
    }
    m_TSE_LockMap.clear();
    m_Scope = CHeapScope();
}


bool CAnnot_Collector::CanResolveId(const CSeq_id_Handle& idh,
                                    const CBioseq_Handle& bh)
{
    switch ( m_Selector.m_ResolveMethod ) {
    case SAnnotSelector::eResolve_All:
        return true;
    case SAnnotSelector::eResolve_TSE:
        return m_Scope->GetBioseqHandleFromTSE(idh, bh.GetTSE_Handle());
    default:
        return false;
    }
}


void CAnnot_Collector::x_Initialize(const CBioseq_Handle& bh,
                                    const CRange<TSeqPos>& range,
                                    ENa_strand strand)
{
    if ( !bh ) {
        NCBI_THROW(CAnnotException, eBadLocation,
                   "Bioseq handle is null");
    }
    try {
        if ( !m_Selector.m_LimitObject ) {
            m_Selector.m_LimitObjectType = SAnnotSelector::eLimit_None;
        }
        if ( m_Selector.m_LimitObjectType != SAnnotSelector::eLimit_None ) {
            x_GetTSE_Info();
        }

        const CSeq_id_Handle& master_id = bh.GetSeq_id_Handle();
        CHandleRange master_range;
        master_range.AddRange(range, strand);
        bool found = false;
        {{
            if ( m_Selector.m_LimitObjectType == SAnnotSelector::eLimit_None ) {
                // any data source
                const CTSE_Handle& tse = bh.GetTSE_Handle();
                if ( m_Selector.m_ExcludeExternal ) {
                    if ( tse.x_GetTSE_Info().HasMatchingAnnotIds() ) {
                        CConstRef<CSynonymsSet> syns = m_Scope->GetSynonyms(bh);
                        ITERATE(CSynonymsSet, syn_it, *syns) {
                            found |= x_SearchTSE(tse,
                                                 syns->GetSeq_id_Handle(syn_it),
                                                 master_range, 0);
                        }
                    }
                    else {
                        const CBioseq_Handle::TId& syns = bh.GetId();
                        bool only_gi = tse.x_GetTSE_Info().OnlyGiAnnotIds();
                        ITERATE ( CBioseq_Handle::TId, syn_it, syns ) {
                            if ( !only_gi || syn_it->IsGi() ) {
                                found |= x_SearchTSE(tse, *syn_it,
                                                     master_range, 0);
                            }
                        }
                    }
                }
                else {
                    CScope_Impl::TTSE_LockMatchSet tse_map =
                        m_Scope->GetTSESetWithAnnots(bh);
                    ITERATE (CScope_Impl::TTSE_LockMatchSet, tse_it, tse_map) {
                        tse.AddUsedTSE(tse_it->first);
                        ITERATE(CScope_Impl::TSeq_idSet, id_it, tse_it->second){
                            found |= x_SearchTSE(tse_it->first, *id_it,
                                                 master_range, 0);
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
                    if ( tse_info.HasMatchingAnnotIds() ) {
                        if ( !syns_initialized ) {
                            syns = m_Scope->GetSynonyms(bh);
                            syns_initialized = true;
                        }
                        if ( !syns ) {
                            found |= x_SearchTSE(tse_it->second, master_id,
                                                 master_range, 0);
                        }
                        else {
                            ITERATE(CSynonymsSet, syn_it, *syns) {
                                found |= x_SearchTSE(tse_it->second,
                                                     syns->GetSeq_id_Handle(syn_it),
                                                     master_range, 0);
                            }
                        }
                    }
                    else {
                        const CBioseq_Handle::TId& syns = bh.GetId();
                        bool only_gi = tse_info.OnlyGiAnnotIds();
                        ITERATE ( CBioseq_Handle::TId, syn_it, syns ) {
                            if ( !only_gi || syn_it->IsGi() ) {
                                found |= x_SearchTSE(tse_it->second, *syn_it,
                                                     master_range, 0);
                            }
                        }
                    }
                }
            }
        }}
        bool deeper = !(found && m_Selector.m_AdaptiveDepth) &&
            m_Selector.m_ResolveMethod != SAnnotSelector::eResolve_None  &&
            m_Selector.m_ResolveDepth > 0 &&
            bh.GetSeqMap().HasSegmentOfType(CSeqMap::eSeqRef);
        if ( deeper ) {
            CRef<CSeq_loc> master_loc_empty(new CSeq_loc);
            master_loc_empty->SetEmpty(const_cast<CSeq_id&>(*master_id.GetSeqId()));
            
            SSeqMapSelector sel(CSeqMap::fFindRef,
                                m_Selector.m_ResolveDepth-1);
            if ( m_Selector.m_ResolveMethod == m_Selector.eResolve_TSE ) {
                sel.SetLimitTSE(bh.GetTSE_Handle());
            }
            CSeqMap_CI smit(bh, sel, range.GetFrom());
            while ( smit && smit.GetPosition() < range.GetToOpen() ) {
                _ASSERT(smit.GetType() == CSeqMap::eSeqRef);
                if ( !CanResolveId(smit.GetRefSeqid(), bh) ) {
                    // External bioseq, try to search if limit is set
                    if ( m_Selector.m_UnresolvedFlag !=
                         SAnnotSelector::eSearchUnresolved  ||
                         !m_Selector.m_LimitObject ) {
                        // Do not try to search on external segments
                        smit.Next(false);
                        continue;
                    }
                }
                found = x_SearchMapped(smit,
                                       *master_loc_empty,
                                       master_id,
                                       master_range);
                deeper = !(found && m_Selector.m_AdaptiveDepth);
                try {
                    smit.Next(deeper);
                }
                catch (CSeqMapException) {
                    switch ( m_Selector.m_UnresolvedFlag ) {
                    case SAnnotSelector::eIgnoreUnresolved:
                    case SAnnotSelector::eSearchUnresolved:
                        // Skip unresolved segment
                        smit.Next(false);
                        break;
                    default:
                        throw;
                    }
                }
            }
        }
        NON_CONST_ITERATE(CAnnotMappingCollector::TAnnotMappingSet, amit,
                          m_MappingCollector->m_AnnotMappingSet) {
            CAnnotObject_Ref annot_ref = amit->first;
            amit->second->Convert(annot_ref,
                m_Selector.m_FeatProduct ? CSeq_loc_Conversion::eProduct :
                                           CSeq_loc_Conversion::eLocation);
            m_AnnotSet.push_back(annot_ref);
        }
        m_MappingCollector->m_AnnotMappingSet.clear();
        x_Sort();
        m_MappingCollector.reset();
    }
    catch (...) {
        // clear all members - GCC 3.0.4 does not do it
        x_Clear();
        throw;
    }
}


void CAnnot_Collector::x_Initialize(const CHandleRangeMap& master_loc)
{
    try {
        if ( !m_Selector.m_LimitObject ) {
            m_Selector.m_LimitObjectType = SAnnotSelector::eLimit_None;
        }
        if ( m_Selector.m_LimitObjectType != SAnnotSelector::eLimit_None ) {
            x_GetTSE_Info();
        }

        bool found = x_SearchLoc(master_loc, 0, 0, true);
        bool deeper = !(found && m_Selector.m_AdaptiveDepth) &&
            m_Selector.m_ResolveMethod != SAnnotSelector::eResolve_None  &&
            m_Selector.m_ResolveDepth > 0;
        if ( deeper ) {
            ITERATE ( CHandleRangeMap::TLocMap, idit, master_loc.GetMap() ) {
                CBioseq_Handle bh = m_Scope->GetBioseqHandle(idit->first,
                                                             GetGetFlag());
                if ( !bh ) {
                    if (m_Selector.m_UnresolvedFlag ==
                        SAnnotSelector::eFailUnresolved) {
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
                
                CHandleRange::TRange idrange =
                    idit->second.GetOverlappingRange();
                SSeqMapSelector sel(CSeqMap::fFindRef,
                                    m_Selector.m_ResolveDepth-1);
                if ( m_Selector.m_ResolveMethod == m_Selector.eResolve_TSE ) {
                    sel.SetLimitTSE(bh.GetTSE_Handle());
                }
                CSeqMap_CI smit(bh, sel, idrange.GetFrom());
                while ( smit && smit.GetPosition() < idrange.GetToOpen() ) {
                    _ASSERT(smit.GetType() == CSeqMap::eSeqRef);
                    if ( !CanResolveId(smit.GetRefSeqid(), bh) ) {
                        // External bioseq, try to search if limit is set
                        if ( m_Selector.m_UnresolvedFlag !=
                            SAnnotSelector::eSearchUnresolved  ||
                            !m_Selector.m_LimitObject ) {
                            // Do not try to search on external segments
                            smit.Next(false);
                            continue;
                        }
                    }
                    found = x_SearchMapped(smit,
                                           *master_loc_empty,
                                           idit->first,
                                           idit->second);
                    deeper = !(found && m_Selector.m_AdaptiveDepth);
                    try {
                        smit.Next(deeper);
                    }
                    catch (CSeqMapException) {
                        switch ( m_Selector.m_UnresolvedFlag ) {
                        case SAnnotSelector::eIgnoreUnresolved:
                        case SAnnotSelector::eSearchUnresolved:
                            // Skip unresolved segment
                            smit.Next(false);
                            break;
                        default:
                            throw;
                        }
                    }
                }
            }
        }
        NON_CONST_ITERATE(CAnnotMappingCollector::TAnnotMappingSet, amit,
            m_MappingCollector->m_AnnotMappingSet) {
            CAnnotObject_Ref annot_ref = amit->first;
            amit->second->Convert(annot_ref,
                m_Selector.m_FeatProduct ? CSeq_loc_Conversion::eProduct :
                CSeq_loc_Conversion::eLocation);
            m_AnnotSet.push_back(annot_ref);
        }
        m_MappingCollector->m_AnnotMappingSet.clear();
        x_Sort();
        m_MappingCollector.reset();
    }
    catch (...) {
        // clear all members - GCC 3.0.4 does not do it
        x_Clear();
        throw;
    }
}


void CAnnot_Collector::x_Initialize(void)
{
    try {
        // Limit must be set, resolving is obsolete
        _ASSERT(m_Selector.m_LimitObjectType != SAnnotSelector::eLimit_None);
        _ASSERT(m_Selector.m_LimitObject);
        _ASSERT(m_Selector.m_ResolveMethod == SAnnotSelector::eResolve_None);
        x_GetTSE_Info();

        x_SearchAll();
        x_Sort();
        m_MappingCollector.reset();
    }
    catch (...) {
        // clear all members - GCC 3.0.4 does not do it
        x_Clear();
        throw;
    }
}


void CAnnot_Collector::x_Sort(void)
{
    _ASSERT(m_MappingCollector->m_AnnotMappingSet.empty());
    switch ( m_Selector.m_SortOrder ) {
    case SAnnotSelector::eSortOrder_Normal:
        sort(m_AnnotSet.begin(), m_AnnotSet.end(), CAnnotObject_Less());
        break;
    case SAnnotSelector::eSortOrder_Reverse:
        sort(m_AnnotSet.begin(), m_AnnotSet.end(), CAnnotObject_LessReverse());
        break;
    default:
        // do nothing
        break;
    }
}


inline
bool
CAnnot_Collector::x_MatchLimitObject(const CAnnotObject_Info& object) const
{
    if ( m_Selector.m_LimitObjectType != SAnnotSelector::eLimit_None ) {
        const CObject* limit = &*m_Selector.m_LimitObject;
        switch ( m_Selector.m_LimitObjectType ) {
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
bool CAnnot_Collector::x_NeedSNPs(void) const
{
    if ( m_Selector.GetAnnotType() != CSeq_annot::C_Data::e_not_set ) {
        if ( m_Selector.GetAnnotType() != CSeq_annot::C_Data::e_Ftable ) {
            return false;
        }
        if ( !m_Selector.IncludedFeatSubtype(
            CSeqFeatData::eSubtype_variation) ) {
            return false;
        }
        else if ( !m_Selector.IncludedFeatType(CSeqFeatData::e_Imp) ) {
            return false;
        }
    }
    return true;
}


inline
bool CAnnot_Collector::x_MatchLocIndex(const SAnnotObject_Index& index) const
{
    return index.m_AnnotObject_Info->IsAlign()  ||
        m_Selector.m_FeatProduct == (index.m_AnnotLocationIndex == 1);
}


inline
bool CAnnot_Collector::x_MatchRange(const CHandleRange&       hr,
                                    const CRange<TSeqPos>&    range,
                                    const SAnnotObject_Index& index) const
{
    if ( m_Selector.m_OverlapType == SAnnotSelector::eOverlap_Intervals ) {
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
    else {
        if ( (hr.GetStrandsFlag() & index.m_StrandIndex) == 0 ) {
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
    _ASSERT(m_Selector.m_LimitObjectType != SAnnotSelector::eLimit_None);
    _ASSERT(m_Selector.m_LimitObject);
    
    switch ( m_Selector.m_LimitObjectType ) {
    case SAnnotSelector::eLimit_TSE_Info:
    {
        _ASSERT(m_Selector.m_LimitTSE);
        _ASSERT(CTypeConverter<CTSE_Info>::
                SafeCast(&*m_Selector.m_LimitObject));
        break;
    }
    case SAnnotSelector::eLimit_Seq_entry_Info:
    {
        _ASSERT(m_Selector.m_LimitTSE);
        _ASSERT(CTypeConverter<CSeq_entry_Info>::
                SafeCast(&*m_Selector.m_LimitObject));
        break;
    }
    case SAnnotSelector::eLimit_Seq_annot_Info:
    {
        _ASSERT(m_Selector.m_LimitTSE);
        _ASSERT(CTypeConverter<CSeq_annot_Info>::
                SafeCast(&*m_Selector.m_LimitObject));
        break;
    }
    default:
        NCBI_THROW(CAnnotException, eLimitError,
                   "CAnnot_Collector::x_GetTSE_Info: invalid mode");
    }
    _ASSERT(m_Selector.m_LimitObject);
    _ASSERT(m_Selector.m_LimitTSE);
    m_TSE_LockMap[&m_Selector.m_LimitTSE.x_GetTSE_Info()] =
        m_Selector.m_LimitTSE;
}


static CSeqFeatData::ESubtype s_DefaultAdaptiveTriggers[] = {
    CSeqFeatData::eSubtype_gene,
    CSeqFeatData::eSubtype_cdregion,
    CSeqFeatData::eSubtype_mRNA
};


bool CAnnot_Collector::x_SearchTSE(const CTSE_Handle&    tseh,
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

    if ( m_Selector.m_AdaptiveDepth && tse.ContainsMatchingBioseq(id) ) {
        const SIdAnnotObjs* objs = tse.x_GetUnnamedIdObjects(id);
        if ( objs ) {
            vector<char> indexes;
            if ( m_Selector.m_AdaptiveTriggers.empty() ) {
                const size_t count =
                    sizeof(s_DefaultAdaptiveTriggers)/
                    sizeof(s_DefaultAdaptiveTriggers[0]);
                for ( int i = count - 1; i >= 0; --i ) {
                    CSeqFeatData::ESubtype subtype =
                        s_DefaultAdaptiveTriggers[i];
                    size_t index = CAnnotType_Index::GetSubtypeIndex(subtype);
                    if ( index ) {
                        indexes.resize(max(indexes.size(), index + 1));
                        indexes[index] = 1;
                    }
                }
            }
            else {
                ITERATE ( SAnnotSelector::TAdaptiveTriggers, it,
                    m_Selector.m_AdaptiveTriggers ) {
                    pair<size_t, size_t> idxs =
                        CAnnotType_Index::GetIndexRange(*it);
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
                if ( index >= objs->x_GetRangeMapCount() ) {
                    break;
                }
                if ( !objs->x_RangeMapIsEmpty(index) ) {
                    found = true;
                    break;
                }
            }
        }
    }
    
    if ( !m_Selector.m_IncludeAnnotsNames.empty() ) {
        // only 'included' annots
        ITERATE ( SAnnotSelector::TAnnotsNames, iter,
            m_Selector.m_IncludeAnnotsNames ) {
            _ASSERT(!m_Selector.ExcludedAnnotName(*iter)); // consistency check
            const SIdAnnotObjs* objs = tse.x_GetIdObjects(*iter, id);
            if ( objs ) {
                x_SearchObjects(tseh, objs, guard, *iter, id, hr, cvt);
            }
        }
    }
    else {
        // all annots, skipping 'excluded'
        ITERATE (CTSE_Info::TNamedAnnotObjs, iter, tse.m_NamedAnnotObjs) {
            if ( m_Selector.ExcludedAnnotName(iter->first) ) {
                continue;
            }
            const SIdAnnotObjs* objs = tse.x_GetIdObjects(iter->second, id);
            if ( objs ) {
                x_SearchObjects(tseh, objs, guard, iter->first, id, hr, cvt);
            }
        }
    }
    return found;
}


bool CAnnot_Collector::x_AddObjectMapping(CAnnotObject_Ref&    object_ref,
                                          CSeq_loc_Conversion* cvt,
                                          unsigned int         loc_index)
{
    object_ref.ResetLocation();
    CRef<CSeq_loc_Conversion_Set>& mapping_set =
        m_MappingCollector->m_AnnotMappingSet[object_ref];
    if ( !mapping_set ) {
        mapping_set.Reset(new CSeq_loc_Conversion_Set(m_Scope));
    }
    if ( cvt ) {
        _ASSERT(cvt->IsPartial() || object_ref.IsAlign());
        CRef<CSeq_loc_Conversion> cvt_copy(new CSeq_loc_Conversion(*cvt));
        mapping_set->Add(*cvt_copy, loc_index);
    }
    return x_GetAnnotCount() >= m_Selector.m_MaxSize;
}


inline
bool CAnnot_Collector::x_AddObject(CAnnotObject_Ref& object_ref)
{
    m_AnnotSet.push_back(object_ref);
    return x_GetAnnotCount() >= m_Selector.m_MaxSize;
}


inline
bool CAnnot_Collector::x_AddObject(CAnnotObject_Ref&    object_ref,
                                   CSeq_loc_Conversion* cvt,
                                   unsigned int         loc_index)
{
    // Always map aligns through conv. set
    return ( cvt && (cvt->IsPartial() || object_ref.IsAlign()) )?
        x_AddObjectMapping(object_ref, cvt, loc_index)
        : x_AddObject(object_ref);
}


void CAnnot_Collector::x_SearchObjects(const CTSE_Handle&    tseh,
                                       const SIdAnnotObjs*   objs,
                                       CTSE_Info::TAnnotLockReadGuard& guard,
                                       const CAnnotName&     annot_name,
                                       const CSeq_id_Handle& id,
                                       const CHandleRange&   hr,
                                       CSeq_loc_Conversion*  cvt)
{
    if ( !m_Selector.m_AnnotTypesBitset.any() ) {
        pair<size_t, size_t> range =
            CAnnotType_Index::GetIndexRange(m_Selector, *objs);
        if ( range.first < range.second ) {
            x_SearchRange(tseh, objs, guard, annot_name, id, hr, cvt,
                          range.first, range.second);
        }
    }
    else {
        pair<size_t, size_t> range(0, 0);
        bool last_bit = false;
        bool cur_bit;
        for (size_t idx = 0; idx < objs->x_GetRangeMapCount(); ++idx) {
            cur_bit = m_Selector.m_AnnotTypesBitset.test(idx);
            if (!last_bit  &&  cur_bit) {
                // open range
                range.first = idx;
            }
            else if (last_bit  &&  !cur_bit) {
                // close and search range
                range.second = idx;
                x_SearchRange(tseh, objs, guard, annot_name, id, hr, cvt,
                              range.first, range.second);
            }
            last_bit = cur_bit;
        }
        if (last_bit) {
            // search to the end of annot set
            x_SearchRange(tseh, objs, guard, annot_name, id, hr, cvt,
                          range.first, objs->x_GetRangeMapCount());
        }
    }

    if ( x_NeedSNPs() ) {
        CHandleRange::TRange range = hr.GetOverlappingRange();
        ITERATE ( CTSE_Info::TSNPSet, snp_annot_it, objs->m_SNPSet ) {
            const CSeq_annot_SNP_Info& snp_annot = **snp_annot_it;
            CSeq_annot_SNP_Info::const_iterator snp_it =
                snp_annot.FirstIn(range);
            if ( snp_it != snp_annot.end() ) {
                m_TSE_LockMap[&tseh.x_GetTSE_Info()] = tseh;
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
                    if ( x_AddObject(annot_ref, cvt, 0) ) {
                        return;
                    }
                    if ( m_Selector.m_CollectSeq_annots ) {
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
                                     CSeq_loc_Conversion*  cvt,
                                     size_t                from_idx,
                                     size_t                to_idx)
{
    const CTSE_Info& tse = tseh.x_GetTSE_Info();
    _ASSERT(objs);

    // CHandleRange::TRange range = hr.GetOverlappingRange();

    m_TSE_LockMap[&tseh.x_GetTSE_Info()] = tseh;

    for ( size_t index = from_idx; index < to_idx; ++index ) {
        size_t start_size = m_AnnotSet.size(); // for rollback

        if ( objs->x_RangeMapIsEmpty(index) ) {
            continue;
        }
        const CTSE_Info::TRangeMap& rmap = objs->x_GetRangeMap(index);

        bool need_unique = false;

        ITERATE(CHandleRange, rg_it, hr) {
            CHandleRange::TRange range = rg_it->first;
            
            bool enough = false;

            for ( CTSE_Info::TRangeMap::const_iterator aoit(rmap.begin(range));
                aoit; ++aoit ) {
                const CAnnotObject_Info& annot_info =
                    *aoit->second.m_AnnotObject_Info;

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
                        chunk.Load();

                        // Acquire the lock again:
                        guard.Guard(tse.GetAnnotLock());

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
                        ERR_POST("CAnnot_Collector: "
                                "Seq-annot.locs is not Seq-interval");
                        continue;
                    }
                    const CSeq_interval& ref_int = ref_loc.GetInt();
                    const CSeq_id& ref_id = ref_int.GetId();
                    CSeq_id_Handle ref_idh = CSeq_id_Handle::GetHandle(ref_id);
                    // check ResolveTSE limit
                    if ( m_Selector.m_ResolveMethod == SAnnotSelector::eResolve_TSE ) {
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

                    if (m_Selector.m_NoMapping) {
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
                    continue;
                }

                _ASSERT(m_Selector.MatchType(annot_info));

                if ( !x_MatchLimitObject(annot_info) ) {
                    continue;
                }
                    
                if ( !x_MatchRange(hr, aoit->first, aoit->second) ) {
                    continue;
                }

                bool is_circular = aoit->second.m_HandleRange  &&
                    aoit->second.m_HandleRange->GetData().IsCircular();
                need_unique |= is_circular;
                CAnnotObject_Ref annot_ref(annot_info);
                if (!cvt  &&  annot_info.GetMultiIdFlags()) {
                    // Create self-conversion, add to conversion set
                    if (x_AddObjectMapping(annot_ref,
                        0, aoit->second.m_AnnotLocationIndex)) {
                        enough = true;
                        break;
                    }
                }
                else {
                    if (cvt  &&  !annot_ref.IsAlign() ) {
                        cvt->Convert
                            (annot_ref,
                            m_Selector.m_FeatProduct ?
                            CSeq_loc_Conversion::eProduct :
                            CSeq_loc_Conversion::eLocation);
                    }
                    else {
                        CHandleRange::TRange ref_rg = aoit->first;
                        if (is_circular ) {
                            TSeqPos from = aoit->second.m_HandleRange->
                                GetData().GetLeft();
                            TSeqPos to =aoit->second.m_HandleRange->
                                GetData().GetRight();
                            ref_rg = CHandleRange::TRange(from, to);
                        }
                        annot_ref.GetMappingInfo().SetAnnotObjectRange(ref_rg,
                            m_Selector.m_FeatProduct);
                    }
                    if ( x_AddObject(annot_ref, cvt,
                        aoit->second.m_AnnotLocationIndex) ) {
                        enough = true;
                        break;
                    }
                }
            }
            if ( enough ) {
                break;
            }
        }
        if ( need_unique  ||  hr.end() - hr.begin() > 1 ) {
            TAnnotSet::iterator first_added = m_AnnotSet.begin() + start_size;
            sort(first_added, m_AnnotSet.end());
            m_AnnotSet.erase(unique(first_added, m_AnnotSet.end()),
                             m_AnnotSet.end());
        }
    }
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
        if ( m_Selector.m_LimitObjectType == SAnnotSelector::eLimit_None ) {
            // any data source
            const CTSE_Handle* tse = 0;
            CScope::EGetBioseqFlag flag =
                top_level? CScope::eGetBioseq_All: GetGetFlag();
            CBioseq_Handle bh = m_Scope->GetBioseqHandle(idit->first, flag);
            if ( !bh ) {
                if ( m_Selector.m_UnresolvedFlag ==
                     m_Selector.eFailUnresolved ) {
                    NCBI_THROW(CAnnotException, eFindFailed,
                               "Cannot find id synonyms");
                }
                if ( m_Selector.m_UnresolvedFlag ==
                     m_Selector.eIgnoreUnresolved ) {
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
            if ( m_Selector.m_ExcludeExternal ) {
                if ( !bh ) {
                    // no sequence tse
                    continue;
                }
                _ASSERT(tse);
                if ( tse->x_GetTSE_Info().HasMatchingAnnotIds() ) {
                    CConstRef<CSynonymsSet> syns = m_Scope->GetSynonyms(bh);
                    ITERATE(CSynonymsSet, syn_it, *syns) {
                        found |= x_SearchTSE(*tse,
                                             syns->GetSeq_id_Handle(syn_it),
                                             idit->second, cvt);
                    }
                }
                else {
                    const CBioseq_Handle::TId& syns = bh.GetId();
                    bool only_gi = tse->x_GetTSE_Info().OnlyGiAnnotIds();
                    ITERATE ( CBioseq_Handle::TId, syn_it, syns ) {
                        if ( !only_gi || syn_it->IsGi() ) {
                            found |= x_SearchTSE(*tse, *syn_it,
                                                 idit->second, cvt);
                        }
                    }
                }
            }
            else {
                CScope_Impl::TTSE_LockMatchSet tse_map =
                    m_Scope->GetTSESetWithAnnots(idit->first);
                ITERATE ( CScope_Impl::TTSE_LockMatchSet, tse_it, tse_map ) {
                    if ( tse ) {
                        tse->AddUsedTSE(tse_it->first);
                    }
                    ITERATE( CScope_Impl::TSeq_idSet, id_it, tse_it->second ) {
                        found |= x_SearchTSE(tse_it->first, *id_it,
                                             idit->second, cvt);
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
                if ( tse_info.HasMatchingAnnotIds() ) {
                    if ( !syns_initialized ) {
                        syns = m_Scope->GetSynonyms(idit->first,
                                                    GetGetFlag());
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
                        }
                    }
                }
            }
        }
    }
    return found;
}


void CAnnot_Collector::x_SearchAll(void)
{
    _ASSERT(m_Selector.m_LimitObjectType != SAnnotSelector::eLimit_None);
    _ASSERT(m_Selector.m_LimitObject);
    if ( m_TSE_LockMap.empty() ) {
        // data source name not matched
        return;
    }
    switch ( m_Selector.m_LimitObjectType ) {
    case SAnnotSelector::eLimit_TSE_Info:
        x_SearchAll(*CTypeConverter<CTSE_Info>::
                    SafeCast(&*m_Selector.m_LimitObject));
        break;
    case SAnnotSelector::eLimit_Seq_entry_Info:
        x_SearchAll(*CTypeConverter<CSeq_entry_Info>::
                    SafeCast(&*m_Selector.m_LimitObject));
        break;
    case SAnnotSelector::eLimit_Seq_annot_Info:
        x_SearchAll(*CTypeConverter<CSeq_annot_Info>::
                    SafeCast(&*m_Selector.m_LimitObject));
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
            if ( x_GetAnnotCount() >= m_Selector.m_MaxSize )
                return;
        }
    }}

    if ( entry_info.IsSet() ) {
        CConstRef<CBioseq_set_Info> set(&entry_info.GetSet());
        // Collect annotations from all children
        ITERATE( CBioseq_set_Info::TSeq_set, cit, set->GetSeq_set() ) {
            x_SearchAll(**cit);
            if ( x_GetAnnotCount() >= m_Selector.m_MaxSize )
                return;
        }
    }
}


void CAnnot_Collector::x_SearchAll(const CSeq_annot_Info& annot_info)
{
    // Collect all annotations from the annot
    ITERATE ( CSeq_annot_Info::TAnnotObjectInfos, aoit,
              annot_info.GetAnnotObjectInfos() ) {
        if ( !m_Selector.MatchType(*aoit) ) {
            continue;
        }
        CAnnotObject_Ref annot_ref(*aoit);
        if ( x_AddObject(annot_ref) ) {
            return;
        }
    }

    if ( x_NeedSNPs() && annot_info.x_HasSNP_annot_Info() ) {
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

    if (m_Selector.m_NoMapping) {
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


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.57  2005/04/08 14:31:26  grichenk
* Changed TAnnotTypesSet from vector to bitset
*
* Revision 1.56  2005/04/05 13:42:51  vasilche
* Optimized annotation iterator from CBioseq_Handle.
*
* Revision 1.55  2005/03/31 21:20:16  vasilche
* Optimize GetTSESetWithAnnots to avoid lookup by matching ids.
*
* Revision 1.54  2005/03/31 18:06:19  grichenk
* Fixed filtering of duplicate annotations
*
* Revision 1.53  2005/03/17 17:52:27  grichenk
* Added flag to SAnnotSelector for skipping multiple SNPs from the same
* seq-annot. Optimized CAnnotCollector::GetAnnot().
*
* Revision 1.52  2005/03/15 19:14:27  vasilche
* Correctly update and check  bioseq ids in split blobs.
*
* Revision 1.51  2005/03/07 16:19:05  vasilche
* Added lost ampersand.
*
* Revision 1.50  2005/02/28 17:26:44  vasilche
* Fixed collection of stranded features from minus strand segments.
*
* Revision 1.49  2005/02/24 19:13:34  grichenk
* Redesigned CMappedFeat not to hold the whole annot collector.
*
* Revision 1.48  2005/02/16 18:51:30  vasilche
* Fixed feature iteration by location in fresh new scope.
*
* Revision 1.47  2005/02/01 19:15:15  vasilche
* Skip SeqMap lookup for unresolved Seq-ids.
*
* Revision 1.46  2005/01/21 20:19:09  vasilche
* Fixed non-point non-mapped SNPs.
*
* Revision 1.45  2005/01/18 15:00:10  grichenk
* UpdateAnnotIndex before collecting annots.
*
* Revision 1.44  2005/01/12 17:16:14  vasilche
* Avoid performance warning on MSVC.
*
* Revision 1.43  2005/01/06 16:41:31  grichenk
* Removed deprecated methods
*
* Revision 1.42  2005/01/04 16:50:34  vasilche
* Added SAnnotSelector::SetExcludeExternal().
*
* Revision 1.41  2004/12/22 15:56:19  vasilche
* Added CTSE_Handle.
* Renamed x_Search() methods to avoid name clash.
* Allow used TSE linking.
*
* Revision 1.40  2004/11/09 21:53:09  grichenk
* Removed debug output
*
* Revision 1.39  2004/11/05 19:29:28  grichenk
* Fixed sorting of circular features
*
* Revision 1.38  2004/11/04 19:21:18  grichenk
* Marked non-handle versions of SetLimitXXXX as deprecated
*
* Revision 1.37  2004/10/29 16:29:47  grichenk
* Prepared to remove deprecated methods, added new constructors.
*
* Revision 1.36  2004/10/27 19:29:23  vasilche
* Reset partial flag in CAnnotObject_Ref::ResetLocation().
* Several methods of CAnnotObject_Ref made non-inline.
*
* Revision 1.35  2004/10/26 17:13:38  grichenk
* Filter duplicates of circular features
*
* Revision 1.34  2004/10/26 15:46:59  vasilche
* Fixed processing of partial intervals in feature mapping.
*
* Revision 1.33  2004/10/25 16:53:35  vasilche
* Check if TSE contains Bioseq by matching Seq-id.
*
* Revision 1.32  2004/10/22 17:36:42  grichenk
* Minor fixes in filtering duplicates
*
* Revision 1.31  2004/10/21 18:55:31  grichenk
* Fixed searching by several ranges in TotalRange mode.
* Fixed filtering of duplicates.
*
* Revision 1.30  2004/10/20 15:33:51  vasilche
* Reset PARTIAL field if necessary.
* Fixed source code formatting.
*
* Revision 1.29  2004/10/19 20:51:49  vasilche
* Restored lost PARTIAL flag on mapped features.
*
* Revision 1.28  2004/10/12 17:09:00  grichenk
* Added mapping of code-break.
*
* Revision 1.27  2004/10/08 14:18:34  grichenk
* Moved MakeMappedXXXX methods to CAnnotCollector,
* fixed mapped feature initialization bug.
*
* Revision 1.26  2004/10/07 14:03:32  vasilche
* Use shared among TSEs CTSE_Split_Info.
* Use typedefs and methods for TSE and DataSource locking.
* Load split CSeqdesc on the fly in CSeqdesc_CI.
*
* Revision 1.25  2004/09/30 15:03:41  grichenk
* Fixed segments resolving
*
* Revision 1.24  2004/09/27 14:35:46  grichenk
* +Flag for handling unresolved IDs (search/ignore/fail)
* +Selector method for external annotations search
*
* Revision 1.23  2004/08/31 14:23:47  vasilche
* Use methods instead of data members directly.
*
* Revision 1.22  2004/08/17 14:31:46  grichenk
* operators <, == and != made inline
*
* Revision 1.21  2004/08/17 03:28:20  grichenk
* Added operator !=()
*
* Revision 1.20  2004/08/16 18:00:40  grichenk
* Added detection of circular locations, improved annotation
* indexing by strand.
*
* Revision 1.19  2004/08/05 18:27:21  vasilche
* Fixed assert when LimitTSE is used.
*
* Revision 1.18  2004/08/04 14:53:26  vasilche
* Revamped object manager:
* 1. Changed TSE locking scheme
* 2. TSE cache is maintained by CDataSource.
* 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
* 4. Fixed processing of split data.
*
* Revision 1.17  2004/07/19 18:28:43  grichenk
* Fixed strand
*
* Revision 1.16  2004/07/19 17:41:59  grichenk
* Added strand processing in annot.locs
*
* Revision 1.15  2004/07/19 14:24:00  grichenk
* Simplified and fixed mapping through annot.locs
*
* Revision 1.14  2004/07/15 16:51:30  vasilche
* Fixed segment shift in Seq-annot.locs processing.
*
* Revision 1.13  2004/07/14 16:04:03  grichenk
* Fixed range when searching through annot-locs
*
* Revision 1.12  2004/07/12 15:05:32  grichenk
* Moved seq-id mapper from xobjmgr to seq library
*
* Revision 1.11  2004/06/10 16:21:27  grichenk
* Changed CSeq_id_Mapper singleton type to pointer, GetSeq_id_Mapper
* returns CRef<> which is locked by CObjectManager.
*
* Revision 1.10  2004/06/08 14:24:25  grichenk
* Restricted depth of mapping through annot-locs
*
* Revision 1.9  2004/06/07 17:01:17  grichenk
* Implemented referencing through locs annotations
*
* Revision 1.8  2004/06/03 18:33:48  grichenk
* Modified annot collector to better resolve synonyms
* and matching IDs. Do not add id to scope history when
* collecting annots. Exclude TSEs with bioseqs from data
* source's annot index.
*
* Revision 1.7  2004/05/26 14:29:20  grichenk
* Redesigned CSeq_align_Mapper: preserve non-mapping intervals,
* fixed strands handling, improved performance.
*
* Revision 1.6  2004/05/21 21:42:12  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.5  2004/05/10 18:26:37  grichenk
* Fixed 'not used' warnings
*
* Revision 1.4  2004/05/03 17:01:03  grichenk
* Invalidate total range before reusing seq-loc.
*
* Revision 1.3  2004/04/13 21:14:27  vasilche
* Fixed wrong order of object deletion causing "tse is locked" error.
*
* Revision 1.2  2004/04/13 15:59:35  grichenk
* Added CScope::GetBioseqHandle() with id resolving flag.
*
* Revision 1.1  2004/04/05 15:54:26  grichenk
* Initial revision
*
*
* ===========================================================================
*/
