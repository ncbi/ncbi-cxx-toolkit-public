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
*
*/

#include <objmgr/annot_selector.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

////////////////////////////////////////////////////////////////////
//
//  SAnnotSelector
//

SAnnotSelector::SAnnotSelector(TAnnotChoice annot,
                               TFeatChoice feat)
    : SAnnotTypeSelector(annot),
      m_FeatProduct(false),
      m_ResolveDepth(kMax_Int),
      m_OverlapType(eOverlap_Intervals),
      m_ResolveMethod(eResolve_TSE),
      m_SegmentSelect(eSegmentSelect_All),
      m_SortOrder(eSortOrder_Normal),
      m_CombineMethod(eCombine_None),
      m_LimitObjectType(eLimit_None),
      m_IdResolving(eIgnoreUnresolved),
      m_MaxSize(kMax_UInt),
      m_NoMapping(false),
      m_AdaptiveDepth(false)
{
    if ( feat != CSeqFeatData::e_not_set ) {
        SetFeatChoice(feat);
    }
}

SAnnotSelector::SAnnotSelector(TFeatChoice feat)
    : SAnnotTypeSelector(feat),
      m_FeatProduct(false),
      m_ResolveDepth(kMax_Int),
      m_OverlapType(eOverlap_Intervals),
      m_ResolveMethod(eResolve_TSE),
      m_SegmentSelect(eSegmentSelect_All),
      m_SortOrder(eSortOrder_Normal),
      m_CombineMethod(eCombine_None),
      m_LimitObjectType(eLimit_None),
      m_IdResolving(eIgnoreUnresolved),
      m_MaxSize(kMax_UInt),
      m_NoMapping(false),
      m_AdaptiveDepth(false)
{
}


SAnnotSelector::SAnnotSelector(TAnnotChoice annot,
                               TFeatChoice feat,
                               bool feat_product)
    : SAnnotTypeSelector(annot),
      m_FeatProduct(feat_product),
      m_ResolveDepth(kMax_Int),
      m_OverlapType(eOverlap_Intervals),
      m_ResolveMethod(eResolve_TSE),
      m_SegmentSelect(eSegmentSelect_All),
      m_SortOrder(eSortOrder_Normal),
      m_CombineMethod(eCombine_None),
      m_LimitObjectType(eLimit_None),
      m_IdResolving(eIgnoreUnresolved),
      m_MaxSize(kMax_UInt),
      m_NoMapping(false),
      m_AdaptiveDepth(false)
{
    if ( feat != CSeqFeatData::e_not_set ) {
        SetFeatChoice(feat);
    }
}

SAnnotSelector::SAnnotSelector(TFeatChoice feat,
                               bool feat_product)
    : SAnnotTypeSelector(feat),
      m_FeatProduct(feat_product),
      m_ResolveDepth(kMax_Int),
      m_OverlapType(eOverlap_Intervals),
      m_ResolveMethod(eResolve_TSE),
      m_SegmentSelect(eSegmentSelect_All),
      m_SortOrder(eSortOrder_Normal),
      m_CombineMethod(eCombine_None),
      m_LimitObjectType(eLimit_None),
      m_IdResolving(eIgnoreUnresolved),
      m_MaxSize(kMax_UInt),
      m_NoMapping(false),
      m_AdaptiveDepth(false)
{
}


SAnnotSelector& SAnnotSelector::SetLimitNone(void)
{
    m_LimitObjectType = eLimit_None;
    m_LimitObject.Reset();
    return *this;
}


SAnnotSelector& SAnnotSelector::SetLimitTSE(const CSeq_entry* tse)
{
    m_LimitObjectType = tse ? eLimit_TSE : eLimit_None;
    m_LimitObject.Reset(tse);
    return *this;
}


SAnnotSelector& SAnnotSelector::SetLimitSeqEntry(const CSeq_entry* entry)
{
    m_LimitObjectType = entry ? eLimit_Entry : eLimit_None;
    m_LimitObject.Reset(entry);
    return *this;
}


SAnnotSelector& SAnnotSelector::SetLimitSeqAnnot(const CSeq_annot* annot)
{
    m_LimitObjectType = annot ? eLimit_Annot : eLimit_None;
    m_LimitObject.Reset(annot);
    return *this;
}


bool SAnnotSelector::x_Has(const TAnnotsNames& names, const CAnnotName& name)
{
    return find(names.begin(), names.end(), name) != names.end();
}


void SAnnotSelector::x_Add(TAnnotsNames& names, const CAnnotName& name)
{
    if ( !x_Has(names, name) ) {
        names.push_back(name);
    }
}


void SAnnotSelector::x_Del(TAnnotsNames& names, const CAnnotName& name)
{
    NON_CONST_ITERATE( TAnnotsNames, it, names ) {
        if ( *it == name ) {
            names.erase(it);
            return;
        }
    }
}


bool SAnnotSelector::IncludedAnnotName(const CAnnotName& name) const
{
    return x_Has(m_IncludeAnnotsNames, name);
}


bool SAnnotSelector::ExcludedAnnotName(const CAnnotName& name) const
{
    return x_Has(m_ExcludeAnnotsNames, name);
}


SAnnotSelector& SAnnotSelector::ResetAnnotsNames(void)
{
    m_IncludeAnnotsNames.clear();
    m_ExcludeAnnotsNames.clear();
    return *this;
}


SAnnotSelector& SAnnotSelector::AddNamedAnnots(const CAnnotName& name)
{
    x_Add(m_IncludeAnnotsNames, name);
    x_Del(m_ExcludeAnnotsNames, name);
    return *this;
}


SAnnotSelector& SAnnotSelector::AddNamedAnnots(const char* name)
{
    return AddNamedAnnots(CAnnotName(name));
}


SAnnotSelector& SAnnotSelector::AddUnnamedAnnots(void)
{
    return AddNamedAnnots(CAnnotName());
}


SAnnotSelector& SAnnotSelector::ExcludeNamedAnnots(const CAnnotName& name)
{
    x_Add(m_ExcludeAnnotsNames, name);
    x_Del(m_IncludeAnnotsNames, name);
    return *this;
}


SAnnotSelector& SAnnotSelector::ExcludeNamedAnnots(const char* name)
{
    return ExcludeNamedAnnots(CAnnotName(name));
}


SAnnotSelector& SAnnotSelector::ExcludeUnnamedAnnots(void)
{
    return ExcludeNamedAnnots(CAnnotName());
}


SAnnotSelector& SAnnotSelector::SetAllNamedAnnots(void)
{
    ResetAnnotsNames();
    ExcludeUnnamedAnnots();
    return *this;
}


SAnnotSelector& SAnnotSelector::SetDataSource(const string& source)
{
    if ( source.empty() ) {
        AddUnnamedAnnots();
    }
    return AddNamedAnnots(source);
}


SAnnotSelector&
SAnnotSelector::SetAdaptiveTrigger(const SAnnotTypeSelector& sel)
{
    ITERATE ( TAdaptiveTriggers, it, m_AdaptiveTriggers ) {
        if ( *it == sel ) {
            return *this;
        }
    }
    m_AdaptiveTriggers.push_back(sel);
    return *this;
}


SAnnotSelector&
SAnnotSelector::ExcludeTSE(const CSeq_entry& tse)
{
    if ( !ExcludedTSE(tse) ) {
        m_ExcludedTSE.push_back(&tse);
    }
    return *this;
}


SAnnotSelector&
SAnnotSelector::ResetExcludedTSE(void)
{
    m_ExcludedTSE.clear();
    return *this;
}


bool SAnnotSelector::ExcludedTSE(const CSeq_entry& tse) const
{
    return find(m_ExcludedTSE.begin(), m_ExcludedTSE.end(), &tse)
        != m_ExcludedTSE.end();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2004/01/23 16:14:47  grichenk
* Implemented alignment mapping
*
* Revision 1.3  2003/11/13 19:12:52  grichenk
* Added possibility to exclude TSEs from annotations request.
*
* Revision 1.2  2003/10/09 20:20:58  vasilche
* Added possibility to include and exclude Seq-annot names to annot iterator.
* Fixed adaptive search. It looked only on selected set of annot names before.
*
* Revision 1.1  2003/10/07 13:43:23  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.27  2003/09/30 16:22:02  vasilche
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
* Revision 1.26  2003/08/27 21:24:51  vasilche
* Reordered member initializers.
*
* Revision 1.25  2003/08/27 14:29:52  vasilche
* Reduce object allocations in feature iterator.
*
* Revision 1.24  2003/08/26 21:28:47  grichenk
* Added seq-align verification
*
* Revision 1.23  2003/07/18 16:58:23  grichenk
* Fixed alignment coordinates
*
* Revision 1.22  2003/07/17 20:07:55  vasilche
* Reduced memory usage by feature indexes.
* SNP data is loaded separately through PUBSEQ_OS.
* String compression for SNP data.
*
* Revision 1.21  2003/06/02 16:06:37  dicuccio
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
* Revision 1.20  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.19  2003/03/11 15:51:06  kuznets
* iterate -> ITERATE
*
* Revision 1.18  2003/02/24 18:57:22  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.17  2003/02/13 14:34:34  grichenk
* Renamed CAnnotObject -> CAnnotObject_Info
* + CSeq_annot_Info and CAnnotObject_Ref
* Moved some members of CAnnotObject to CSeq_annot_Info
* and CAnnotObject_Ref.
* Added feat/align/graph to CAnnotObject_Info map
* to CDataSource.
*
* Revision 1.16  2003/02/04 18:53:36  dicuccio
* Include file clean-up
*
* Revision 1.15  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.14  2002/09/13 15:20:30  dicuccio
* Fix memory leak (static deleted before termination).
*
* Revision 1.13  2002/09/11 16:08:26  dicuccio
* Fixed memory leak in x_PrepareAlign().
*
* Revision 1.12  2002/09/03 17:45:45  ucko
* Avoid overrunning alignment data when the claimed dimensions are too high.
*
* Revision 1.11  2002/07/25 15:01:51  grichenk
* Replaced non-const GetXXX() with SetXXX()
*
* Revision 1.10  2002/07/08 20:51:00  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.9  2002/07/01 15:31:57  grichenk
* Fixed 'enumeration value e_not_set...' warning
*
* Revision 1.8  2002/05/29 21:21:13  gouriano
* added debug dump
*
* Revision 1.7  2002/05/24 14:57:12  grichenk
* SerialAssign<>() -> CSerialObject::Assign()
*
* Revision 1.6  2002/05/03 21:28:08  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.5  2002/04/05 21:26:19  grichenk
* Enabled iteration over annotations defined on segments of a
* delta-sequence.
*
* Revision 1.4  2002/02/21 19:27:04  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.3  2002/01/23 21:59:31  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.2  2002/01/16 16:25:56  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:16  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
