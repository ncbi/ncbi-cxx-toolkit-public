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
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/general/Dbtag.hpp>
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
    CAnnotTypes_CI::operator=(iter);
    Update();
    return *this;
}


CFeat_CI::CFeat_CI(const CBioseq_Handle& bioseq,
                   TSeqPos start, TSeqPos stop,
                   TFeatType feat_type,
                   SAnnotSelector::EOverlapType overlap_type,
                   SAnnotSelector::EResolveMethod resolve,
                   EFeat_Location loc_type)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Ftable,
                     bioseq, start, stop,
                     SAnnotSelector(feat_type)
                     .SetByProduct(loc_type == e_Product)
                     .SetOverlapType(overlap_type)
                     .SetResolveMethod(resolve))
{
    Update();
}


CFeat_CI::CFeat_CI(const CBioseq_Handle& bioseq,
                   TSeqPos start, TSeqPos stop,
                   TFeatType feat_type,
                   SAnnotSelector::EOverlapType overlap_type,
                   SAnnotSelector::EResolveMethod resolve,
                   EFeat_Location loc_type,
                   const CSeq_entry_Handle& limitEntry)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Ftable,
                     bioseq, start, stop,
                     SAnnotSelector(feat_type)
                     .SetByProduct(loc_type == e_Product)
                     .SetOverlapType(overlap_type)
                     .SetResolveMethod(resolve)
                     .SetLimitSeqEntry(limitEntry))
{
    Update();
}


CFeat_CI::CFeat_CI(const CBioseq_Handle& bioseq,
                   TSeqPos start, TSeqPos stop,
                   TFeatType feat_type,
                   SAnnotSelector::EOverlapType overlap_type,
                   SAnnotSelector::EResolveMethod resolve,
                   EFeat_Location loc_type,
                   const CSeq_entry* limitEntry)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Ftable,
                     bioseq, start, stop,
                     SAnnotSelector(feat_type)
                     .SetByProduct(loc_type == e_Product)
                     .SetOverlapType(overlap_type)
                     .SetResolveMethod(resolve)
                     .SetLimitSeqEntry(limitEntry))
{
    Update();
}


CFeat_CI::CFeat_CI(CScope& scope,
                   const CSeq_loc& loc,
                   TFeatType feat_type,
                   SAnnotSelector::EOverlapType overlap_type,
                   SAnnotSelector::EResolveMethod resolve,
                   EFeat_Location loc_type)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Ftable,
                     scope, loc,
                     SAnnotSelector(feat_type)
                     .SetByProduct(loc_type == e_Product)
                     .SetOverlapType(overlap_type)
                     .SetResolveMethod(resolve))
{
    Update();
}


CFeat_CI::CFeat_CI(CScope& scope,
                   const CSeq_loc& loc)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Ftable,
                     scope, loc,
                     SAnnotSelector::eOverlap_Intervals,
                     SAnnotSelector::eResolve_TSE)
{
    Update();
}


CFeat_CI::CFeat_CI(const CBioseq_Handle& bioseq,
                   TSeqPos start, TSeqPos stop)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Ftable,
                     bioseq, start, stop,
                     SAnnotSelector::eOverlap_Intervals,
                     SAnnotSelector::eResolve_TSE)
{
    Update();
}


CFeat_CI::CFeat_CI(CScope& scope,
                   const CSeq_loc& loc,
                   const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Ftable,
                     scope, loc,
                     sel)
{
    Update();
}


CFeat_CI::CFeat_CI(const CBioseq_Handle& bioseq,
                   TSeqPos start, TSeqPos stop,
                   const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Ftable,
                     bioseq, start, stop,
                     sel)
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
                     sel)
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
                     sel)
{
    Update();
}


CFeat_CI::CFeat_CI(CScope& scope, const CSeq_annot& annot)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Ftable,
                     scope.GetSeq_annotHandle(annot))
{
    Update();
}


CFeat_CI::CFeat_CI(CScope& scope, const CSeq_annot& annot,
                   const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Ftable,
                     scope.GetSeq_annotHandle(annot),
                     sel)
{
    Update();
}


CFeat_CI::CFeat_CI(CScope& scope, const CSeq_entry& entry)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Ftable,
                     scope.GetSeq_entryHandle(entry))
{
    Update();
}


CFeat_CI::CFeat_CI(CScope& scope, const CSeq_entry& entry,
                   const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Ftable,
                     scope.GetSeq_entryHandle(entry),
                     sel)
{
    Update();
}


CMappedFeat::CMappedFeat(void)
{
}


CMappedFeat::CMappedFeat(const CMappedFeat& feat)
{
    *this = feat;
}


CMappedFeat& CMappedFeat::operator=(const CMappedFeat& feat)
{
    if ( this != &feat ) {
        m_Collector = feat.m_Collector;
        m_FeatRef = feat.m_FeatRef;
        m_OriginalSeq_feat = feat.m_OriginalSeq_feat;
        m_MappedSeq_feat = feat.m_MappedSeq_feat;
        m_MappedSeq_loc = feat.m_MappedSeq_loc;
    }
    return *this;
}


CMappedFeat::~CMappedFeat(void)
{
}


CSeq_feat_Handle CMappedFeat::GetSeq_feat_Handle(void) const
{
    if ( m_FeatRef->IsSNPFeat() ) {
        return CSeq_feat_Handle(m_Collector->GetScope(),
            m_FeatRef->GetSeq_annot_SNP_Info(),
            m_FeatRef->GetAnnotObjectIndex());
    }
    return CSeq_feat_Handle(m_Collector->GetScope(),
        m_FeatRef->GetSeq_annot_Info(),
        m_FeatRef->GetAnnotObjectIndex());
}


CSeq_annot_Handle CMappedFeat::GetAnnot(void) const
{
    return m_Collector->GetAnnot(*m_FeatRef);
    /*
    return CSeq_annot_Handle(m_Collector->GetScope(),
        m_FeatRef->GetSeq_annot_Info());
    */
}


const CSeq_annot& CMappedFeat::GetSeq_annot(void) const
{
    ERR_POST_ONCE(Warning<<
                  "CMappedFeat::GetSeq_annot() is deprecated, "
                  "use GetAnnot().");
    return *m_FeatRef->GetSeq_annot_Info().GetCompleteSeq_annot();
}


void CMappedFeat::Reset(void)
{
    m_Collector.Reset();
    m_OriginalSeq_feat.Reset();
    m_MappedSeq_feat.Reset();
    m_MappedSeq_loc.Reset();
}


CMappedFeat& CMappedFeat::Set(CAnnot_Collector& collector,
                              const TIterator& annot)
{
    m_Collector.Reset(&collector);
    _ASSERT(annot->IsFeat());
    m_FeatRef = annot;
    m_OriginalSeq_feat.Reset();
    m_MappedSeq_feat.Reset();
    m_MappedSeq_loc.Reset();
    return *this;
}


const CSeq_feat& CMappedFeat::x_MakeOriginalFeature(void) const
{
    if ( m_FeatRef->IsSNPFeat() ) {
        _ASSERT(!m_OriginalSeq_feat);
        const CSeq_annot_SNP_Info& snp_annot =
            m_FeatRef->GetSeq_annot_SNP_Info();
        const SSNP_Info& snp_info =
            snp_annot.GetSNP_Info(m_FeatRef->GetAnnotObjectIndex());
        CRef<CSeq_feat> orig_feat;
        m_Collector->m_CreatedOriginalSeq_feat.AtomicReleaseTo(orig_feat);
        CRef<CSeq_point> created_point;
        m_Collector->m_CreatedOriginalSeq_point.AtomicReleaseTo(created_point);
        CRef<CSeq_interval> created_interval;
        m_Collector->m_CreatedOriginalSeq_interval.
            AtomicReleaseTo(created_interval);
        snp_info.UpdateSeq_feat(orig_feat,
                                created_point,
                                created_interval,
                                snp_annot);
        m_OriginalSeq_feat = orig_feat;
        m_Collector->m_CreatedOriginalSeq_feat.AtomicResetFrom(orig_feat);
        m_Collector->m_CreatedOriginalSeq_point.AtomicResetFrom(created_point);
        m_Collector->m_CreatedOriginalSeq_interval.
            AtomicResetFrom(created_interval);
    }
    else {
        m_OriginalSeq_feat.Reset(&m_FeatRef->GetFeat());
    }
    return *m_OriginalSeq_feat;
}


const CSeq_loc& CMappedFeat::x_MakeMappedLocation(void) const
{
    _ASSERT(!m_MappedSeq_loc);
    if ( m_FeatRef->MappedSeq_locNeedsUpdate() ) {
        _ASSERT(!m_MappedSeq_feat);
        // need to covert Seq_id to Seq_loc
        // clear references to mapped location from mapped feature
        // Can not use m_MappedSeq_feat since it's a const-ref
        CRef<CSeq_feat> mapped_feat;
        m_Collector->m_CreatedMappedSeq_feat.AtomicReleaseTo(mapped_feat);
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
        m_MappedSeq_feat = mapped_feat;
        m_Collector->m_CreatedMappedSeq_feat.AtomicResetFrom(mapped_feat);

        CRef<CSeq_loc> mapped_loc;
        m_Collector->m_CreatedMappedSeq_loc.AtomicReleaseTo(mapped_loc);
        CRef<CSeq_point> created_point;
        m_Collector->m_CreatedMappedSeq_point.AtomicReleaseTo(created_point);
        CRef<CSeq_interval> created_interval;
        m_Collector->m_CreatedMappedSeq_interval.
            AtomicReleaseTo(created_interval);
        m_FeatRef->UpdateMappedSeq_loc(mapped_loc,
                                       created_point,
                                       created_interval);
        m_MappedSeq_loc = mapped_loc;
        m_Collector->m_CreatedMappedSeq_loc.AtomicResetFrom(mapped_loc);
        m_Collector->m_CreatedMappedSeq_point.AtomicResetFrom(created_point);
        m_Collector->m_CreatedMappedSeq_interval.
            AtomicResetFrom(created_interval);
    }
    else if ( m_FeatRef->IsMapped() ) {
        _ASSERT(!m_MappedSeq_feat);
        m_MappedSeq_loc.Reset(&m_FeatRef->GetMappedSeq_loc());
    }
    return *m_MappedSeq_loc;
}


const CSeq_feat& CMappedFeat::x_MakeMappedFeature(void) const
{
    if ( m_FeatRef->IsMapped() ) {
        _ASSERT(!m_MappedSeq_feat);
        // some Seq-loc object is mapped
        CRef<CSeq_feat> mapped_feat;
        m_Collector->m_CreatedMappedSeq_feat.AtomicReleaseTo(mapped_feat);
        if ( !mapped_feat || !mapped_feat->ReferencedOnlyOnce() ) {
            mapped_feat.Reset(new CSeq_feat);
        }
        CSeq_feat& dst = *mapped_feat;
        CSeq_feat& src = const_cast<CSeq_feat&>(GetOriginalFeature());

        if ( src.IsSetId() )
            dst.SetId(src.SetId());
        else
            dst.ResetId();

        dst.SetData(src.SetData());

        if ( GetPartial() )
            dst.SetPartial(true);
        else
            dst.ResetPartial();

        if ( src.IsSetExcept() )
            dst.SetExcept(src.GetExcept());
        else
            dst.ResetExcept();
        
        if ( src.IsSetComment() )
            dst.SetComment(src.GetComment());
        else
            dst.ResetComment();

        GetMappedLocation();

        dst.SetLocation(const_cast<CSeq_loc&>(GetLocation()));

        if ( src.IsSetProduct() ) {
            dst.SetProduct(const_cast<CSeq_loc&>(GetProduct()));
        }
        else {
            dst.ResetProduct();
        }

        if ( src.IsSetQual() )
            dst.SetQual() = src.SetQual();
        else
            dst.ResetQual();

        if ( src.IsSetTitle() )
            dst.SetTitle(src.GetTitle());
        else
            dst.ResetTitle();

        if ( src.IsSetExt() )
            dst.SetExt(src.SetExt());
        else
            dst.ResetExt();

        if ( src.IsSetCit() )
            dst.SetCit(src.SetCit());
        else
            dst.ResetCit();

        if ( src.IsSetExp_ev() )
            dst.SetExp_ev(src.GetExp_ev());
        else
            dst.ResetExp_ev();

        if ( src.IsSetXref() )
            dst.SetXref() = src.SetXref();
        else
            dst.ResetXref();

        if ( src.IsSetDbxref() )
            dst.SetDbxref() = src.SetDbxref();
        else
            dst.ResetDbxref();

        if ( src.IsSetPseudo() )
            dst.SetPseudo(src.GetPseudo());
        else
            dst.ResetPseudo();

        if ( src.IsSetExcept_text() )
            dst.SetExcept_text(src.GetExcept_text());
        else
            dst.ResetExcept_text();
        m_MappedSeq_feat = mapped_feat;
        m_Collector->m_CreatedMappedSeq_feat.AtomicResetFrom(mapped_feat);
    }
    else {
        m_MappedSeq_feat.Reset(&GetOriginalFeature());
    }
    return *m_MappedSeq_feat;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.33  2004/08/04 14:53:26  vasilche
* Revamped object manager:
* 1. Changed TSE locking scheme
* 2. TSE cache is maintained by CDataSource.
* 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
* 4. Fixed processing of split data.
*
* Revision 1.32  2004/05/21 21:42:12  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.31  2004/05/04 18:08:48  grichenk
* Added CSeq_feat_Handle, CSeq_align_Handle and CSeq_graph_Handle
*
* Revision 1.30  2004/04/07 13:20:17  grichenk
* Moved more data from iterators to CAnnot_Collector
*
* Revision 1.29  2004/04/05 15:56:14  grichenk
* Redesigned CAnnotTypes_CI: moved all data and data collecting
* functions to CAnnotDataCollector. CAnnotTypes_CI is no more
* inherited from SAnnotSelector.
*
* Revision 1.28  2004/03/16 15:47:27  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.27  2004/02/11 22:19:24  grichenk
* Fixed annot type initialization in iterators
*
* Revision 1.26  2004/02/05 19:53:40  grichenk
* Fixed type matching in SAnnotSelector. Added IncludeAnnotType().
*
* Revision 1.25  2004/02/04 18:05:39  grichenk
* Added annotation filtering by set of types/subtypes.
* Renamed *Choice to *Type in SAnnotSelector.
*
* Revision 1.24  2004/01/28 20:54:36  vasilche
* Fixed mapping of annotations.
*
* Revision 1.23  2003/08/27 19:44:06  vasilche
* Avoid error on IRIX with null reference cast.
*
* Revision 1.22  2003/08/27 14:48:56  vasilche
* Fixed constness of mapped location.
*
* Revision 1.21  2003/08/27 14:29:52  vasilche
* Reduce object allocations in feature iterator.
*
* Revision 1.20  2003/08/15 19:19:16  vasilche
* Fixed memory leak in string packing hooks.
* Fixed processing of 'partial' flag of features.
* Allow table packing of non-point SNP.
* Allow table packing of SNP with long alleles.
*
* Revision 1.19  2003/08/14 20:05:19  vasilche
* Simple SNP features are stored as table internally.
* They are recreated when needed using CFeat_CI.
*
* Revision 1.18  2003/06/02 16:06:37  dicuccio
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
* Revision 1.17  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.16  2003/02/24 21:35:22  vasilche
* Reduce checks in CAnnotObject_Ref comparison.
* Fixed compilation errors on MS Windows.
* Removed obsolete file src/objects/objmgr/annot_object.hpp.
*
* Revision 1.15  2003/02/24 18:57:22  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.14  2003/02/13 14:34:34  grichenk
* Renamed CAnnotObject -> CAnnotObject_Info
* + CSeq_annot_Info and CAnnotObject_Ref
* Moved some members of CAnnotObject to CSeq_annot_Info
* and CAnnotObject_Ref.
* Added feat/align/graph to CAnnotObject_Info map
* to CDataSource.
*
* Revision 1.13  2003/02/10 15:50:45  grichenk
* + CMappedFeat, CFeat_CI resolves to CMappedFeat
*
* Revision 1.12  2002/12/20 20:54:24  grichenk
* Added optional location/product switch to CFeat_CI
*
* Revision 1.11  2002/12/06 15:36:00  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.10  2002/11/04 21:29:12  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.9  2002/07/08 20:51:01  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.8  2002/05/06 03:28:47  vakatov
* OM/OM1 renaming
*
* Revision 1.7  2002/05/03 21:28:09  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.6  2002/04/05 21:26:19  grichenk
* Enabled iteration over annotations defined on segments of a
* delta-sequence.
*
* Revision 1.5  2002/03/05 16:08:14  grichenk
* Moved TSE-restriction to new constructors
*
* Revision 1.4  2002/03/04 15:07:48  grichenk
* Added "bioseq" argument to CAnnotTypes_CI constructor to iterate
* annotations from a single TSE.
*
* Revision 1.3  2002/02/21 19:27:05  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.2  2002/01/16 16:25:57  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:18  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
