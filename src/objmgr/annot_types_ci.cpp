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
#include <objmgr/annot_types_ci.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/impl/snp_annot_info.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
// CAnnotTypes_CI
/////////////////////////////////////////////////////////////////////////////


CAnnotTypes_CI::CAnnotTypes_CI(void)
    : m_DataCollector(0)
{
    return;
}


CAnnotTypes_CI::CAnnotTypes_CI(TAnnotType type,
                               const CBioseq_Handle& bioseq,
                               const SAnnotSelector* params)
    : m_DataCollector(new CAnnot_Collector(bioseq.GetScope()))
{
    if ( !params ) {
        SAnnotSelector sel(type);
        m_DataCollector->x_Initialize(sel,
                                      bioseq,
                                      CRange<TSeqPos>::GetWhole(),
                                      eNa_strand_unknown);
    }
    else if ( !params->CheckAnnotType(type) ) {
        SAnnotSelector sel(*params);
        sel.ForceAnnotType(type);
        m_DataCollector->x_Initialize(sel,
                                      bioseq,
                                      CRange<TSeqPos>::GetWhole(),
                                      eNa_strand_unknown);
    }
    else {
        m_DataCollector->x_Initialize(*params,
                                      bioseq,
                                      CRange<TSeqPos>::GetWhole(),
                                      eNa_strand_unknown);
    }
    Rewind();
}


void CAnnotTypes_CI::x_Init(CScope& scope,
                            const CSeq_loc& loc,
                            const SAnnotSelector& params)
{
    if ( loc.IsWhole() ) {
        CBioseq_Handle bh = scope.GetBioseqHandle(loc.GetWhole());
        if ( bh ) {
            m_DataCollector->x_Initialize(params,
                                          bh,
                                          CRange<TSeqPos>::GetWhole(),
                                          eNa_strand_unknown);
            Rewind();
            return;
        }
    }
    else if ( loc.IsInt() ) {
        const CSeq_interval& seq_int = loc.GetInt();
        CBioseq_Handle bh = scope.GetBioseqHandle(seq_int.GetId());
        if ( bh ) {
            CRange<TSeqPos> range(seq_int.GetFrom(), seq_int.GetTo());
            ENa_strand strand =
                seq_int.IsSetStrand()? seq_int.GetStrand(): eNa_strand_unknown;
            m_DataCollector->x_Initialize(params, bh, range, strand);
            Rewind();
            return;
        }
    }
    CHandleRangeMap master_loc;
    master_loc.AddLocation(loc);
    m_DataCollector->x_Initialize(params, master_loc);
    Rewind();
}


CAnnotTypes_CI::CAnnotTypes_CI(TAnnotType type,
                               CScope& scope,
                               const CSeq_loc& loc,
                               const SAnnotSelector* params)
    : m_DataCollector(new CAnnot_Collector(scope))
{
    if ( !params ) {
        x_Init(scope, loc, SAnnotSelector(type));
    }
    else if ( !params->CheckAnnotType(type) ) {
        SAnnotSelector sel(*params);
        sel.ForceAnnotType(type);
        x_Init(scope, loc, sel);
    }
    else {
        x_Init(scope, loc, *params);
    }
}


CAnnotTypes_CI::CAnnotTypes_CI(TAnnotType type,
                               const CSeq_annot_Handle& annot,
                               const SAnnotSelector* params)
    : m_DataCollector(new CAnnot_Collector(annot.GetScope()))
{
    SAnnotSelector sel = params ? *params : SAnnotSelector();
    sel.ForceAnnotType(type)
        .SetResolveNone() // nothing to resolve
        .SetLimitSeqAnnot(annot);
    m_DataCollector->x_Initialize(sel);
    Rewind();
}


CAnnotTypes_CI::CAnnotTypes_CI(TAnnotType type,
                               const CSeq_entry_Handle& entry,
                               const SAnnotSelector* params)
    : m_DataCollector(new CAnnot_Collector(entry.GetScope()))
{
    SAnnotSelector sel = params ? *params : SAnnotSelector();
    sel.ForceAnnotType(type)
        .SetResolveNone() // nothing to resolve
        .SetSortOrder(SAnnotSelector::eSortOrder_None)
        .SetLimitSeqEntry(entry);
    m_DataCollector->x_Initialize(sel);
    Rewind();
}


CSeq_annot_Handle CAnnotTypes_CI::GetAnnot(void) const
{
    return m_DataCollector->GetAnnot(Get());
}


CAnnotTypes_CI::~CAnnotTypes_CI(void)
{
    return;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.127  2005/05/10 17:03:55  grichenk
* Check for null params
*
* Revision 1.126  2005/04/11 17:51:38  grichenk
* Fixed m_CollectSeq_annots initialization.
* Avoid copying SAnnotSelector in CAnnotTypes_CI.
*
* Revision 1.125  2005/04/05 13:43:28  vasilche
* Use optimized variant of x_Initialize().
*
* Revision 1.124  2005/01/06 16:41:31  grichenk
* Removed deprecated methods
*
* Revision 1.123  2004/10/29 16:29:47  grichenk
* Prepared to remove deprecated methods, added new constructors.
*
* Revision 1.122  2004/08/04 14:53:26  vasilche
* Revamped object manager:
* 1. Changed TSE locking scheme
* 2. TSE cache is maintained by CDataSource.
* 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
* 4. Fixed processing of split data.
*
* Revision 1.121  2004/05/21 21:42:12  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.120  2004/04/05 15:56:14  grichenk
* Redesigned CAnnotTypes_CI: moved all data and data collecting
* functions to CAnnotDataCollector. CAnnotTypes_CI is no more
* inherited from SAnnotSelector.
*
* Revision 1.119  2004/04/01 20:18:12  grichenk
* Added initialization of m_MultiId member.
*
* Revision 1.118  2004/03/31 20:43:29  grichenk
* Fixed mapping of seq-locs containing both master sequence
* and its segments.
*
* Revision 1.117  2004/03/30 15:42:33  grichenk
* Moved alignment mapper to separate file, added alignment mapping
* to CSeq_loc_Mapper.
*
* Revision 1.116  2004/03/24 18:30:29  vasilche
* Fixed edit API.
* Every *_Info object has its own shallow copy of original object.
*
* Revision 1.115  2004/03/16 15:47:27  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.114  2004/02/26 14:41:41  grichenk
* Fixed types excluding in SAnnotSelector and multiple types search
* in CAnnotTypes_CI.
*
* Revision 1.113  2004/02/19 19:25:08  vasilche
* Hidded implementation of CAnnotObject_Ref.
* Added necessary access methods.
*
* Revision 1.112  2004/02/09 14:48:37  vasilche
* Got rid of performance warning on MSVC.
*
* Revision 1.111  2004/02/06 18:31:54  vasilche
* Fixed annot sorting class - deal with different annot types (graph align feat).
*
* Revision 1.110  2004/02/05 19:53:40  grichenk
* Fixed type matching in SAnnotSelector. Added IncludeAnnotType().
*
* Revision 1.109  2004/02/04 18:05:38  grichenk
* Added annotation filtering by set of types/subtypes.
* Renamed *Choice to *Type in SAnnotSelector.
*
* Revision 1.108  2004/01/30 15:25:45  grichenk
* Fixed alignments mapping and sorting
*
* Revision 1.107  2004/01/28 20:54:36  vasilche
* Fixed mapping of annotations.
*
* Revision 1.106  2004/01/26 17:50:55  vasilche
* Do assert check only when object is non-null.
*
* Revision 1.105  2004/01/23 16:14:47  grichenk
* Implemented alignment mapping
*
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
