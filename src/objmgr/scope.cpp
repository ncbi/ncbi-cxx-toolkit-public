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
* Authors:
*           Andrei Gourianov
*           Aleksey Grichenko
*           Michael Kimelman
*           Denis Vakatov
*           Eugene Vasilchenko
*
* File Description:
*           Scope is top-level object available to a client.
*           Its purpose is to define a scope of visibility and reference
*           resolution and provide access to the bio sequence data
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/bioseq_set_handle.hpp>
#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/impl/synonyms.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
//
// CScope
//
/////////////////////////////////////////////////////////////////////////////


CScope::CScope(CObjectManager& objmgr)
{
    if ( CanBeDeleted() ) {
        // this CScope object is allocated in heap
        m_Impl.Reset(new CScope_Impl(objmgr));
        m_Impl->m_HeapScope = this;
    }
    else {
        // allocate heap CScope object
        m_HeapScope.Reset(new CScope(objmgr));
        _ASSERT(m_HeapScope->CanBeDeleted());
        m_Impl = m_HeapScope->m_Impl;
        _ASSERT(m_Impl);
    }
}


CScope::~CScope(void)
{
    if ( m_Impl && m_Impl->m_HeapScope == this ) {
        m_Impl->m_HeapScope = 0;
    }
}


CBioseq_Handle CScope::GetBioseqHandle(const CSeq_id& id)
{
    return GetBioseqHandle(id, eGetBioseq_All);
}


CBioseq_Handle CScope::GetBioseqHandle(const CSeq_id_Handle& id)
{
    return GetBioseqHandle(id, eGetBioseq_All);
}


CBioseq_Handle CScope::GetBioseqHandle(const CSeq_loc& loc)
{
    return m_Impl->GetBioseqHandle(loc, eGetBioseq_All);
}


CTSE_Handle CScope::GetTSE_Handle(const CSeq_entry& entry)
{
    return GetSeq_entryHandle(entry).GetTSE_Handle();
}


CBioseq_Handle CScope::GetBioseqHandle(const CBioseq& seq)
{
    //ERR_POST_ONCE(Warning<<"CScope::GetBioseqHandle(CBioseq&) is deprecated");
    return m_Impl->GetBioseqHandle(seq);
}


CBioseq_Handle CScope::GetBioseqHandle(const CSeq_id& id,
                                       EGetBioseqFlag get_flag)
{
    return GetBioseqHandle(CSeq_id_Handle::GetHandle(id), get_flag);
}


CBioseq_Handle CScope::GetBioseqHandle(const CSeq_id_Handle& id,
                                       EGetBioseqFlag get_flag)
{
    return m_Impl->GetBioseqHandle(id, get_flag);
}


CSeq_entry_Handle CScope::GetSeq_entryHandle(const CSeq_entry& entry)
{
    //ERR_POST_ONCE(Warning<<"CScope::GetSeq_entryHandle(CSeq_entry&) is deprecated.");
    return m_Impl->GetSeq_entryHandle(entry);
}


CSeq_annot_Handle CScope::GetSeq_annotHandle(const CSeq_annot& annot)
{
    //ERR_POST_ONCE(Warning<<"CScope::GetSeq_annotHandle(CSeq_annot&) is deprecated.");
    return m_Impl->GetSeq_annotHandle(annot);
}


CBioseq_Handle CScope::GetBioseqHandleFromTSE(const CSeq_id& id,
                                              const CTSE_Handle& tse)
{
    return GetBioseqHandleFromTSE(CSeq_id_Handle::GetHandle(id), tse);
}


CBioseq_Handle CScope::GetBioseqHandleFromTSE(const CSeq_id& id,
                                              const CBioseq_Handle& bh)
{
    return GetBioseqHandleFromTSE(id, bh.GetTSE_Handle());
}


CBioseq_Handle CScope::GetBioseqHandleFromTSE(const CSeq_id& id,
                                              const CSeq_entry_Handle& seh)
{
    return GetBioseqHandleFromTSE(id, seh.GetTSE_Handle());
}


CBioseq_Handle CScope::GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                              const CBioseq_Handle& bh)
{
    return GetBioseqHandleFromTSE(id, bh.GetTSE_Handle());
}


CBioseq_Handle CScope::GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                              const CSeq_entry_Handle& seh)
{
    return GetBioseqHandleFromTSE(id, seh.GetTSE_Handle());
}


CBioseq_Handle CScope::GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                              const CTSE_Handle& tse)
{
    return m_Impl->GetBioseqHandleFromTSE(id, tse);
}


void CScope::GetAllTSEs(TTSE_Handles& tses, enum ETSEKind kind)
{
    m_Impl->GetAllTSEs(tses, int(kind));
}


CBioseq_EditHandle CScope::GetEditHandle(const CBioseq_Handle& seq)
{
    return m_Impl->GetEditHandle(seq);
}


CSeq_entry_EditHandle CScope::GetEditHandle(const CSeq_entry_Handle& entry)
{
    return m_Impl->GetEditHandle(entry);
}


CSeq_annot_EditHandle CScope::GetEditHandle(const CSeq_annot_Handle& annot)
{
    return m_Impl->GetEditHandle(annot);
}


CBioseq_set_EditHandle CScope::GetEditHandle(const CBioseq_set_Handle& seqset)
{
    return m_Impl->GetEditHandle(seqset);
}


void CScope::ResetHistory(void)
{
    m_Impl->ResetHistory();
}


void CScope::RemoveFromHistory(CBioseq_Handle& bioseq)
{
    m_Impl->RemoveFromHistory(bioseq);
}


void CScope::RemoveFromHistory(CTSE_Handle& tse)
{
    m_Impl->RemoveFromHistory(tse);
}


void CScope::RemoveDataLoader(const string& loader_name)
{
    m_Impl->RemoveDataLoader(loader_name);
}


void CScope::RemoveTopLevelSeqEntry(CTSE_Handle& entry)
{
    m_Impl->RemoveTopLevelSeqEntry(entry);
}


CScope::TIds CScope::GetIds(const CSeq_id& id)
{
    return GetIds(CSeq_id_Handle::GetHandle(id));
}


CScope::TIds CScope::GetIds(const CSeq_id_Handle& idh)
{
    return m_Impl->GetIds(idh);
}


CConstRef<CSynonymsSet> CScope::GetSynonyms(const CSeq_id& id)
{
    return GetSynonyms(CSeq_id_Handle::GetHandle(id));
}


CConstRef<CSynonymsSet> CScope::GetSynonyms(const CSeq_id_Handle& id)
{
    return m_Impl->GetSynonyms(id, eGetBioseq_All);
}


CConstRef<CSynonymsSet> CScope::GetSynonyms(const CBioseq_Handle& bh)
{
    return m_Impl->GetSynonyms(bh);
}


void CScope::AddDefaults(TPriority priority)
{
    m_Impl->AddDefaults(priority);
}


void CScope::AddDataLoader(const string& loader_name, TPriority priority)
{
    m_Impl->AddDataLoader(loader_name, priority);
}


void CScope::AddScope(CScope& scope, TPriority priority)
{
    m_Impl->AddScope(*scope.m_Impl, priority);
}


CSeq_entry_Handle CScope::AddTopLevelSeqEntry(CSeq_entry& top_entry,
                                              TPriority priority)
{
    return m_Impl->AddTopLevelSeqEntry(top_entry, priority);
}


CBioseq_Handle CScope::AddBioseq(CBioseq& bioseq, TPriority priority)
{
    return m_Impl->AddBioseq(bioseq, priority);
}


CSeq_annot_Handle CScope::AddSeq_annot(CSeq_annot& annot, TPriority priority)
{
    return m_Impl->AddAnnot(annot, priority);
}


CBioseq_Handle CScope::GetBioseqHandleFromTSE(const CSeq_id& id,
                                              const CSeq_entry& tse)
{
    //ERR_POST_ONCE(Warning<<"GetBioseqHandleFromTSE(CSeq_entry) is deprecated: use handles.");
    return GetBioseqHandleFromTSE(id, GetSeq_entryHandle(tse));
}


CBioseq_Handle CScope::GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                              const CSeq_entry& tse)
{
    //ERR_POST_ONCE(Warning<<"GetBioseqHandleFromTSE(CSeq_entry) is deprecated: use handles.");
    return GetBioseqHandleFromTSE(id, GetSeq_entryHandle(tse));
}


void CScope::AttachEntry(CSeq_entry& parent, CSeq_entry& entry)
{
    //ERR_POST_ONCE(Warning<<"CScope::AttachEntry() is deprecated: use class CSeq_entry_EditHandle.");
    m_Impl->GetSeq_entryHandle(parent).GetSet().GetEditHandle().AttachEntry(entry);
}


void CScope::RemoveEntry(CSeq_entry& entry)
{
    //ERR_POST_ONCE(Warning<<"CScope::RemoveEntry() is deprecated: use class CSeq_entry_EditHandle.");
    m_Impl->GetSeq_entryHandle(entry).GetEditHandle().Remove();
}


void CScope::AttachAnnot(CSeq_entry& parent, CSeq_annot& annot)
{
    //ERR_POST_ONCE(Warning<<"CScope::AttachAnnot() is deprecated: use class CSeq_annot_EditHandle.");
    m_Impl->GetSeq_entryHandle(parent).GetEditHandle().AttachAnnot(annot);
}


void CScope::RemoveAnnot(CSeq_entry& parent, CSeq_annot& annot)
{
    //ERR_POST_ONCE(Warning<<"CScope::RemoveAnnot() is deprecated: use class CSeq_annot_EditHandle.");
    CSeq_entry_EditHandle eh = m_Impl->GetSeq_entryHandle(parent).GetEditHandle();
    CSeq_annot_EditHandle ah = m_Impl->GetSeq_annotHandle(annot).GetEditHandle();
    if ( ah.GetParentEntry() != eh ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CScope::RemoveAnnot: parent doesn't contain annot");
    }
    ah.Remove();
}


void CScope::ReplaceAnnot(CSeq_entry& parent,
                          CSeq_annot& old_annot, CSeq_annot& new_annot)
{
    //ERR_POST_ONCE(Warning<<"CScope::RemoveAnnot() is deprecated: use class CSeq_annot_EditHandle.");
    CSeq_entry_EditHandle eh = m_Impl->GetSeq_entryHandle(parent).GetEditHandle();
    CSeq_annot_EditHandle ah = m_Impl->GetSeq_annotHandle(old_annot).GetEditHandle();
    if ( ah.GetParentEntry() != eh ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CScope::ReplaceAnnot: parent doesn't contain old_annot");
    }
    ah.Remove();
    eh.AttachAnnot(new_annot);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.114  2005/03/14 18:17:15  grichenk
* Added CScope::RemoveFromHistory(), CScope::RemoveTopLevelSeqEntry() and
* CScope::RemoveDataLoader(). Added requested seq-id information to
* CTSE_Info.
*
* Revision 1.113  2005/01/12 17:16:14  vasilche
* Avoid performance warning on MSVC.
*
* Revision 1.112  2004/12/22 15:56:04  vasilche
* Introduced CTSE_Handle.
*
* Revision 1.111  2004/11/04 19:21:18  grichenk
* Marked non-handle versions of SetLimitXXXX as deprecated
*
* Revision 1.110  2004/09/28 14:30:02  vasilche
* Implemented CScope::AddSeq_annot().
*
* Revision 1.109  2004/09/27 14:31:05  grichenk
* Added GetSynonyms() with get-flag
*
* Revision 1.108  2004/08/31 21:03:49  grichenk
* Added GetIds()
*
* Revision 1.107  2004/05/21 21:42:12  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.106  2004/04/13 15:59:35  grichenk
* Added CScope::GetBioseqHandle() with id resolving flag.
*
* Revision 1.105  2004/04/12 18:40:24  grichenk
* Added GetAllTSEs()
*
* Revision 1.104  2004/03/24 18:30:30  vasilche
* Fixed edit API.
* Every *_Info object has its own shallow copy of original object.
*
* Revision 1.103  2004/03/16 21:01:32  vasilche
* Added methods to move Bioseq withing Seq-entry
*
* Revision 1.102  2004/03/16 15:47:28  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.101  2004/02/19 17:23:01  vasilche
* Changed order of deletion of heap scope and scope impl objects.
* Reduce number of calls to x_ResetHistory().
*
* Revision 1.100  2004/02/10 21:15:16  grichenk
* Added reverse ID matching.
*
* Revision 1.99  2004/02/09 14:42:46  vasilche
* Temporary fix in GetSynonyms() to get accession without version.
*
* Revision 1.98  2004/02/02 14:46:43  vasilche
* Several performance fixed - do not iterate whole tse set in CDataSource.
*
* Revision 1.97  2004/01/29 20:33:28  vasilche
* Do not resolve any Seq-ids in CScope::GetSynonyms() -
* assume all not resolved Seq-id as synonym.
* Seq-id conflict messages made clearer.
*
* Revision 1.96  2004/01/28 20:50:49  vasilche
* Fixed NULL pointer exception in GetSynonyms() when matching Seq-id w/o version.
*
* Revision 1.95  2004/01/07 20:42:01  grichenk
* Fixed matching of accession to accession.version
*
* Revision 1.94  2003/12/18 16:38:07  grichenk
* Added CScope::RemoveEntry()
*
* Revision 1.93  2003/12/12 16:59:51  grichenk
* Fixed conflicts resolving (ask data source).
*
* Revision 1.92  2003/12/03 20:55:12  grichenk
* Check value returned by x_GetBioseq_Info()
*
* Revision 1.91  2003/11/21 20:33:03  grichenk
* Added GetBioseqHandleFromTSE(CSeq_id, CSeq_entry)
*
* Revision 1.90  2003/11/19 22:18:03  grichenk
* All exceptions are now CException-derived. Catch "exception" rather
* than "runtime_error".
*
* Revision 1.89  2003/11/12 15:49:39  vasilche
* Added loading annotations on non gi Seq-id.
*
* Revision 1.88  2003/10/23 13:47:27  vasilche
* Check CSeq_id_Handle for null in CScope::GetBioseqHandle().
*
* Revision 1.87  2003/10/22 14:08:15  vasilche
* Detach all CBbioseq_Handle objects from scope in CScope::ResetHistory().
*
* Revision 1.86  2003/10/22 13:54:36  vasilche
* All CScope::GetBioseqHandle() methods return 'null' CBioseq_Handle object
* instead of throwing an exception.
*
* Revision 1.85  2003/10/20 18:23:54  vasilche
* Make CScope::GetSynonyms() to skip conflicting ids.
*
* Revision 1.84  2003/10/09 13:58:21  vasilche
* Fixed conflict when the same datasource appears twice with equal priorities.
*
* Revision 1.83  2003/10/07 13:43:23  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.82  2003/09/30 16:22:03  vasilche
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
* Revision 1.81  2003/09/05 20:50:26  grichenk
* +AddBioseq(CBioseq&)
*
* Revision 1.80  2003/09/05 17:29:40  grichenk
* Structurized Object Manager exceptions
*
* Revision 1.79  2003/09/03 20:00:02  grichenk
* Added sequence filtering by level (mains/parts/all)
*
* Revision 1.78  2003/08/04 17:03:01  grichenk
* Added constructors to iterate all annotations from a
* seq-entry or seq-annot.
*
* Revision 1.77  2003/07/25 15:25:25  grichenk
* Added CSeq_annot_CI class
*
* Revision 1.76  2003/07/17 20:07:56  vasilche
* Reduced memory usage by feature indexes.
* SNP data is loaded separately through PUBSEQ_OS.
* String compression for SNP data.
*
* Revision 1.75  2003/07/14 21:13:59  grichenk
* Added seq-loc dump in GetBioseqHandle(CSeqLoc)
*
* Revision 1.74  2003/06/30 18:42:10  vasilche
* CPriority_I made to use less memory allocations/deallocations.
*
* Revision 1.73  2003/06/24 14:25:18  vasilche
* Removed obsolete CTSE_Guard class.
* Used separate mutexes for bioseq and annot maps.
*
* Revision 1.72  2003/06/19 18:23:46  vasilche
* Added several CXxx_ScopeInfo classes for CScope related information.
* CBioseq_Handle now uses reference to CBioseq_ScopeInfo.
* Some fine tuning of locking in CScope.
*
* Revision 1.69  2003/05/27 19:44:06  grichenk
* Added CSeqVector_CI class
*
* Revision 1.68  2003/05/20 15:44:37  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.67  2003/05/14 18:39:28  grichenk
* Simplified TSE caching and filtering in CScope, removed
* some obsolete members and functions.
*
* Revision 1.66  2003/05/13 18:33:01  vasilche
* Fixed CScope::GetTSESetWithAnnots() conflict resolution.
*
* Revision 1.65  2003/05/12 19:18:29  vasilche
* Fixed locking of object manager classes in multi-threaded application.
*
* Revision 1.64  2003/05/09 20:28:03  grichenk
* Changed warnings to info
*
* Revision 1.63  2003/05/06 18:54:09  grichenk
* Moved TSE filtering from CDataSource to CScope, changed
* some filtering rules (e.g. priority is now more important
* than scope history). Added more caches to CScope.
*
* Revision 1.62  2003/04/29 19:51:13  vasilche
* Fixed interaction of Data Loader garbage collector and TSE locking mechanism.
* Made some typedefs more consistent.
*
* Revision 1.61  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.60  2003/04/15 14:21:52  vasilche
* Removed unnecessary assignment.
*
* Revision 1.59  2003/04/14 21:32:18  grichenk
* Avoid passing CScope as an argument to CDataSource methods
*
* Revision 1.58  2003/04/09 16:04:32  grichenk
* SDataSourceRec replaced with CPriorityNode
* Added CScope::AddScope(scope, priority) to allow scope nesting
*
* Revision 1.57  2003/04/03 14:18:09  vasilche
* Added public GetSynonyms() method.
*
* Revision 1.56  2003/03/26 21:00:19  grichenk
* Added seq-id -> tse with annotation cache to CScope
*
* Revision 1.55  2003/03/24 21:26:45  grichenk
* Added support for CTSE_CI
*
* Revision 1.54  2003/03/21 19:22:51  grichenk
* Redesigned TSE locking, replaced CTSE_Lock with CRef<CTSE_Info>.
*
* Revision 1.53  2003/03/19 21:55:50  grichenk
* Avoid re-mapping TSEs in x_AddToHistory() if already indexed
*
* Revision 1.52  2003/03/18 14:52:59  grichenk
* Removed obsolete methods, replaced seq-id with seq-id handle
* where possible. Added argument to limit annotations update to
* a single seq-entry.
*
* Revision 1.51  2003/03/12 20:09:34  grichenk
* Redistributed members between CBioseq_Handle, CBioseq_Info and CTSE_Info
*
* Revision 1.50  2003/03/11 15:51:06  kuznets
* iterate -> ITERATE
*
* Revision 1.49  2003/03/11 14:15:52  grichenk
* +Data-source priority
*
* Revision 1.48  2003/03/10 16:55:17  vasilche
* Cleaned SAnnotSelector structure.
* Added shortcut when features are limited to one TSE.
*
* Revision 1.47  2003/03/05 20:55:29  vasilche
* Added cache cleaning in CScope::ResetHistory().
*
* Revision 1.46  2003/03/03 20:32:47  vasilche
* Removed obsolete method GetSynonyms().
*
* Revision 1.45  2003/02/28 21:54:18  grichenk
* +CSynonymsSet::empty(), removed _ASSERT() in CScope::GetSynonyms()
*
* Revision 1.44  2003/02/28 20:02:37  grichenk
* Added synonyms cache and x_GetSynonyms()
*
* Revision 1.43  2003/02/27 14:35:31  vasilche
* Splitted PopulateTSESet() by logically independent parts.
*
* Revision 1.42  2003/02/24 18:57:22  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.41  2003/02/05 17:59:17  dicuccio
* Moved formerly private headers into include/objects/objmgr/impl
*
* Revision 1.40  2003/01/29 22:03:46  grichenk
* Use single static CSeq_id_Mapper instead of per-OM model.
*
* Revision 1.39  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.38  2002/12/26 20:55:18  dicuccio
* Moved seq_id_mapper.hpp, tse_info.hpp, and bioseq_info.hpp -> include/ tree
*
* Revision 1.37  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.36  2002/11/08 22:15:51  grichenk
* Added methods for removing/replacing annotations
*
* Revision 1.35  2002/11/08 19:43:35  grichenk
* CConstRef<> constructor made explicit
*
* Revision 1.34  2002/11/04 21:29:12  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.33  2002/11/01 05:34:32  kans
* added GetBioseqHandle taking CBioseq parameter
*
* Revision 1.32  2002/10/31 22:25:42  kans
* added GetBioseqHandle taking CSeq_loc parameter
*
* Revision 1.31  2002/10/18 19:12:40  grichenk
* Removed mutex pools, converted most static mutexes to non-static.
* Protected CSeqMap::x_Resolve() with mutex. Modified code to prevent
* dead-locks.
*
* Revision 1.30  2002/10/16 20:44:29  ucko
* *** empty log message ***
*
* Revision 1.29  2002/10/02 17:58:23  grichenk
* Added sequence type filter to CBioseq_CI
*
* Revision 1.28  2002/09/30 20:01:19  grichenk
* Added methods to support CBioseq_CI
*
* Revision 1.27  2002/08/09 14:59:00  ucko
* Restrict template <> to MIPSpro for now, as it also leads to link
* errors with Compaq's compiler.  (Sigh.)
*
* Revision 1.26  2002/08/08 19:51:24  ucko
* Omit EMPTY_TEMPLATE for GCC and KCC, as it evidently leads to link errors(!)
*
* Revision 1.25  2002/08/08 14:28:00  ucko
* Add EMPTY_TEMPLATE to explicit instantiations.
*
* Revision 1.24  2002/08/07 18:21:57  ucko
* Explicitly instantiate CMutexPool_Base<CScope>::sm_Pool
*
* Revision 1.23  2002/07/08 20:51:02  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.22  2002/06/04 17:18:33  kimelman
* memory cleanup :  new/delete/Cref rearrangements
*
* Revision 1.21  2002/05/28 18:00:43  gouriano
* DebugDump added
*
* Revision 1.20  2002/05/14 20:06:26  grichenk
* Improved CTSE_Info locking by CDataSource and CDataLoader
*
* Revision 1.19  2002/05/06 03:28:47  vakatov
* OM/OM1 renaming
*
* Revision 1.18  2002/04/22 20:04:39  grichenk
* Fixed TSE dropping, removed commented code
*
* Revision 1.17  2002/04/17 21:09:40  grichenk
* Fixed annotations loading
*
* Revision 1.16  2002/03/28 14:02:31  grichenk
* Added scope history checks to CDataSource::x_FindBestTSE()
*
* Revision 1.15  2002/03/27 18:45:44  gouriano
* three functions made public
*
* Revision 1.14  2002/03/20 21:20:39  grichenk
* +CScope::ResetHistory()
*
* Revision 1.13  2002/02/28 20:53:32  grichenk
* Implemented attaching segmented sequence data. Fixed minor bugs.
*
* Revision 1.12  2002/02/25 21:05:29  grichenk
* Removed seq-data references caching. Increased MT-safety. Fixed typos.
*
* Revision 1.11  2002/02/21 19:27:06  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.10  2002/02/07 21:27:35  grichenk
* Redesigned CDataSource indexing: seq-id handle -> TSE -> seq/annot
*
* Revision 1.9  2002/02/06 21:46:11  gouriano
* *** empty log message ***
*
* Revision 1.8  2002/02/05 21:46:28  gouriano
* added FindSeqid function, minor tuneup in CSeq_id_mapper
*
* Revision 1.7  2002/01/29 17:45:34  grichenk
* Removed debug output
*
* Revision 1.6  2002/01/28 19:44:49  gouriano
* changed the interface of BioseqHandle: two functions moved from Scope
*
* Revision 1.5  2002/01/23 21:59:31  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.4  2002/01/18 17:06:29  gouriano
* renamed CScope::GetSequence to CScope::GetSeqVector
*
* Revision 1.3  2002/01/18 15:54:14  gouriano
* changed DropTopLevelSeqEntry()
*
* Revision 1.2  2002/01/16 16:25:57  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:22  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
