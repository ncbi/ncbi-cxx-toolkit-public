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

#include <ncbi_pch.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/impl/annot_object.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/snp_annot_info.hpp>
#include <objmgr/impl/annot_type_index.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_interval.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CFeat_CI::CFeat_CI(void)
{
}


CFeat_CI::CFeat_CI(const CFeat_CI& iter)
    : CAnnotTypes_CI(iter)
{
    Update();
}


CFeat_CI::~CFeat_CI(void)
{
}


CFeat_CI& CFeat_CI::operator= (const CFeat_CI& iter)
{
    if ( this != &iter ) {
        CAnnotTypes_CI::operator=(iter);
        Update();
    }
    return *this;
}


CFeat_CI::CFeat_CI(const CBioseq_Handle& bioseq)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Ftable,
                     bioseq,
                     CRange<TSeqPos>::GetWhole(),
                     eNa_strand_unknown)
{
    Update();
}


CFeat_CI::CFeat_CI(const CBioseq_Handle& bioseq,
                   const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Ftable,
                     bioseq,
                     CRange<TSeqPos>::GetWhole(),
                     eNa_strand_unknown,
                     &sel)
{
    Update();
}


CFeat_CI::CFeat_CI(const CBioseq_Handle& bioseq,
                   const CRange<TSeqPos>& range,
                   ENa_strand strand)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Ftable,
                     bioseq,
                     range,
                     strand)
{
    Update();
}


CFeat_CI::CFeat_CI(const CBioseq_Handle& bioseq,
                   const CRange<TSeqPos>& range,
                   const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Ftable,
                     bioseq,
                     range,
                     eNa_strand_unknown,
                     &sel)
{
    Update();
}


CFeat_CI::CFeat_CI(const CBioseq_Handle& bioseq,
                   const CRange<TSeqPos>& range,
                   ENa_strand strand,
                   const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Ftable,
                     bioseq,
                     range,
                     strand,
                     &sel)
{
    Update();
}


CFeat_CI::CFeat_CI(CScope& scope,
                   const CSeq_loc& loc)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Ftable,
                     scope,
                     loc)
{
    Update();
}


CFeat_CI::CFeat_CI(CScope& scope,
                   const CSeq_loc& loc,
                   const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Ftable,
                     scope,
                     loc,
                     &sel)
{
    Update();
}


CFeat_CI::CFeat_CI(const CSeq_annot_Handle& annot)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Ftable,
                     annot)
{
    Update();
}


CFeat_CI::CFeat_CI(const CSeq_annot_Handle& annot,
                   const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Ftable,
                     annot,
                     &sel)
{
    Update();
}


CFeat_CI::CFeat_CI(const CSeq_entry_Handle& entry)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Ftable,
                     entry)
{
    Update();
}


CFeat_CI::CFeat_CI(const CSeq_entry_Handle& entry,
                   const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Ftable,
                     entry,
                     &sel)
{
    Update();
}


CFeat_CI::CFeat_CI(const CTSE_Handle& tse,
                   const SAnnotSelector& sel,
                   const TFeatureId& feat_id)
    : CAnnotTypes_CI(tse.GetScope())
{
    x_AddFeaturesWithId(tse, sel, feat_id);
}


CFeat_CI::CFeat_CI(const CTSE_Handle& tse,
                   const SAnnotSelector& sel,
                   const TFeatureIdInt& int_id)
    : CAnnotTypes_CI(tse.GetScope())
{
    CObject_id feat_id;
    feat_id.SetId(int_id);
    x_AddFeaturesWithId(tse, sel, feat_id);
}


CFeat_CI::CFeat_CI(const CTSE_Handle& tse,
                   const SAnnotSelector& sel,
                   const TFeatureIdStr& str_id)
    : CAnnotTypes_CI(tse.GetScope())
{
    CObject_id feat_id;
    feat_id.SetStr(str_id);
    x_AddFeaturesWithId(tse, sel, feat_id);
}


void CFeat_CI::x_AddFeaturesWithId(const CTSE_Handle& tse,
                                   const SAnnotSelector& sel,
                                   const TFeatureId& feat_id)
{
    CSeqFeatData::ESubtype subtype = sel.GetFeatSubtype();
    if ( subtype == CSeqFeatData::eSubtype_any ) {
        if ( sel.GetFeatProduct() ) {
            x_AddFeatures(sel, tse.GetFeaturesWithXref(subtype, feat_id));
        }
        else {
            x_AddFeatures(sel, tse.GetFeaturesWithId(subtype, feat_id));
        }
    }
    else {
        pair<size_t, size_t> range = CAnnotType_Index::GetIndexRange(sel);
        for ( size_t i = range.first; i < range.second; ++i ) {
            subtype = CAnnotType_Index::GetSubtypeForIndex(i);
            if ( !tse.x_GetTSE_Info().x_HasFeaturesWithId(subtype) ) {
                continue;
            }
            if ( sel.GetFeatProduct() ) {
                x_AddFeatures(sel, tse.GetFeaturesWithXref(subtype, feat_id));
            }
            else {
                x_AddFeatures(sel, tse.GetFeaturesWithId(subtype, feat_id));
            }
        }
    }
    Rewind();
}


void CFeat_CI::x_AddFeatures(const SAnnotSelector& sel,
                             const TSeq_feat_Handles& feats)
{
    CAnnot_Collector& collector = GetCollector();
    collector.m_Selector = &sel;
    ITERATE ( TSeq_feat_Handles, it, feats ) {
        const CAnnotObject_Info& info = it->x_GetAnnotObject_Info();
        if ( !collector.x_MatchLimitObject(info) ) {
            CAnnotObject_Ref feat_ref(info, it->GetAnnot());
            collector.x_AddObject(feat_ref);
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
