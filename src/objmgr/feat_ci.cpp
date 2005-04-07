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
    if ( this != &iter ) {
        CAnnotTypes_CI::operator=(iter);
        Update();
    }
    return *this;
}


CFeat_CI::CFeat_CI(const CBioseq_Handle& bioseq)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Ftable, bioseq)
{
    Update();
}


CFeat_CI::CFeat_CI(const CBioseq_Handle& bioseq,
                   const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Ftable,
                     bioseq,
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


CMappedFeat::CMappedFeat(void)
{
    m_MappingInfoPtr = &m_MappingInfoObj;
}


CMappedFeat::CMappedFeat(const CMappedFeat& feat)
{
    *this = feat;
}


CMappedFeat& CMappedFeat::operator=(const CMappedFeat& feat)
{
    if ( this != &feat ) {
        m_OriginalFeat = feat.m_OriginalFeat;
        m_MappingInfoObj = *feat.m_MappingInfoPtr;
        m_MappingInfoPtr = &m_MappingInfoObj;
        m_MappedFeat = feat.m_MappedFeat;
        m_OriginalSeq_feat_Lock.Reset();
    }
    return *this;
}


CMappedFeat::~CMappedFeat(void)
{
}


CSeq_annot_Handle CMappedFeat::GetAnnot(void) const
{
    return m_OriginalFeat.GetAnnot();
}


void CMappedFeat::Reset(void)
{
    m_OriginalFeat.Reset();
    m_MappingInfoObj.Reset();
    m_MappingInfoPtr = &m_MappingInfoObj;
    m_MappedFeat.ResetRefs();
    m_OriginalSeq_feat_Lock.Reset();
}


CMappedFeat& CMappedFeat::Set(CAnnot_Collector& collector,
                              const TIterator& annot)
{
    m_OriginalSeq_feat_Lock.Reset();

    const CAnnotObject_Ref& feat_ref = *annot;
    _ASSERT(feat_ref.IsFeat());

    const CSeq_annot_Info* annot_info;
    if ( feat_ref.IsSNPFeat() ) {
        annot_info =
            &feat_ref.GetSeq_annot_SNP_Info().GetParentSeq_annot_Info();
        m_OriginalFeat.m_AnnotInfoType =
            CSeq_feat_Handle::eType_Seq_annot_SNP_Info;
    }
    else {
        annot_info = &feat_ref.GetSeq_annot_Info();
        m_OriginalFeat.m_AnnotInfoType =
            CSeq_feat_Handle::eType_Seq_annot_Info;
    }
    if ( m_OriginalFeat.m_Annot.m_Info.GetPointerOrNull() != annot_info ) {
        CAnnot_Collector::TTSE_LockMap::const_iterator tse_it =
            collector.m_TSE_LockMap.find(&annot_info->GetTSE_Info());
        _ASSERT(tse_it != collector.m_TSE_LockMap.end());
        m_OriginalFeat.m_Annot.m_Info = annot_info;
        m_OriginalFeat.m_Annot.m_TSE = tse_it->second;
    }

    m_OriginalFeat.m_Index = feat_ref.GetAnnotObjectIndex();
    m_OriginalFeat.m_CreatedFeat = collector.m_CreatedOriginal;

    m_MappingInfoPtr = &feat_ref.GetMappingInfo();
    m_MappedFeat.ResetRefs();
    return *this;
}


const CSeq_feat& CMappedFeat::GetMappedFeature(void) const
{
    CRef<CSeq_loc> mapped_location(&const_cast<CSeq_loc&>(GetLocation()));
    return *m_MappedFeat.MakeMappedFeature(m_OriginalFeat,
                                           *m_MappingInfoPtr,
                                           *mapped_location);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.43  2005/04/07 18:10:33  vasilche
* Fixed compatibility with Sun.
*
* Revision 1.42  2005/04/07 16:30:42  vasilche
* Inlined handles' constructors and destructors.
* Optimized handles' assignment operators.
*
* Revision 1.41  2005/03/07 17:30:01  vasilche
* Added methods to get feature type and subtype
*
* Revision 1.40  2005/02/24 20:38:28  grichenk
* Optimized mapping info in CMappedFeat.
*
* Revision 1.39  2005/02/24 19:13:34  grichenk
* Redesigned CMappedFeat not to hold the whole annot collector.
*
* Revision 1.38  2005/01/06 16:41:31  grichenk
* Removed deprecated methods
*
* Revision 1.37  2004/12/22 15:56:09  vasilche
* Used CSeq_annot_Handle GetAnnot() to get annotation handle.
* Avoid use of deprecated API.
*
* Revision 1.36  2004/10/29 16:29:47  grichenk
* Prepared to remove deprecated methods, added new constructors.
*
* Revision 1.35  2004/10/12 17:07:39  grichenk
* Fixed x_MakeMappedFeature
*
* Revision 1.34  2004/10/08 14:18:34  grichenk
* Moved MakeMappedXXXX methods to CAnnotCollector,
* fixed mapped feature initialization bug.
*
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
