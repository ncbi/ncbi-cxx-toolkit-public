#ifndef OBJMGR_SCOPE__HPP
#define OBJMGR_SCOPE__HPP

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

#include <corelib/ncbiobj.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objmgr/tse_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/bioseq_set_handle.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_annot_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/** @addtogroup ObjectManagerCore
 *
 * @{
 */


// fwd decl
// objects
class CSeq_entry;
class CBioseq_set;
class CBioseq;
class CSeq_annot;
class CSeq_id;
class CSeq_loc;

// objmgr
class CSeq_id_Handle;
class CObjectManager;
class CScope_Impl;
class CSynonymsSet;


/////////////////////////////////////////////////////////////////////////////
///
///  CScope --
///
///  Scope of cache visibility and reference resolution
///

class NCBI_XOBJMGR_EXPORT CScope : public CObject
{
public:
    explicit CScope(CObjectManager& objmgr);
    virtual ~CScope(void);

    // CBioseq_Handle methods:
    /// Get bioseq handle by seq-id
    CBioseq_Handle GetBioseqHandle(const CSeq_id& id);

    /// Get bioseq handle by seq-id handle
    CBioseq_Handle GetBioseqHandle(const CSeq_id_Handle& id);

    /// Get bioseq handle by seq-id location
    CBioseq_Handle GetBioseqHandle(const CSeq_loc& loc);

    enum EGetBioseqFlag {
        eGetBioseq_Resolved, ///< Search only in already resolved ids
        eGetBioseq_Loaded,   ///< Search in all loaded TSEs in the scope
        eGetBioseq_All       ///< Search bioseq, load if not loaded yet
    };

    /// Get bioseq handle without loading new data
    CBioseq_Handle GetBioseqHandle(const CSeq_id& id,
                                   EGetBioseqFlag get_flag);
    /// Get bioseq handle without loading new data
    CBioseq_Handle GetBioseqHandle(const CSeq_id_Handle& id,
                                   EGetBioseqFlag get_flag);

    // Deprecated interface
    CTSE_Handle GetTSE_Handle(const CSeq_entry& tse);
    CBioseq_Handle GetBioseqHandle(const CBioseq& bioseq);
    CSeq_entry_Handle GetSeq_entryHandle(const CSeq_entry& entry);
    CSeq_annot_Handle GetSeq_annotHandle(const CSeq_annot& annot);


    /// Get bioseq handle for sequence withing one TSE
    CBioseq_Handle GetBioseqHandleFromTSE(const CSeq_id& id,
                                          const CTSE_Handle& tse);
    CBioseq_Handle GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                          const CTSE_Handle& tse);

    CBioseq_Handle GetBioseqHandleFromTSE(const CSeq_id& id,
                                          const CBioseq_Handle& bh);

    /// Get bioseq handle for sequence withing one TSE
    CBioseq_Handle GetBioseqHandleFromTSE(const CSeq_id& id,
                                          const CSeq_entry_Handle& seh);

    /// Get bioseq handle for sequence withing one TSE
    CBioseq_Handle GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                          const CBioseq_Handle& bh);

    /// Get bioseq handle for sequence withing one TSE
    CBioseq_Handle GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                          const CSeq_entry_Handle& seh);


    // CScope contents modification methods
    typedef int TPriority;
    enum EPriority {
        kPriority_NotSet = -1
    };

    /// Add default data loaders from object manager
    void AddDefaults(TPriority pri = kPriority_NotSet);

    /// Add data loader by name.
    /// The loader (or its factory) must be known to Object Manager.
    void AddDataLoader(const string& loader_name,
                       TPriority pri = kPriority_NotSet);

    /// Add the scope's datasources as a single group with the given priority
    void AddScope(CScope& scope,
                  TPriority pri = kPriority_NotSet);


    /// Add seq_entry, default priority is higher than for defaults or loaders
    CSeq_entry_Handle AddTopLevelSeqEntry(CSeq_entry& top_entry,
                                          TPriority pri = kPriority_NotSet);

    /// Add bioseq, return bioseq handle. Try to use unresolved seq-id
    /// from the bioseq, fail if all ids are already resolved to
    /// other sequences.
    CBioseq_Handle AddBioseq(CBioseq& bioseq,
                             TPriority pri = kPriority_NotSet);
    /// Add Seq-annot
    CSeq_annot_Handle AddSeq_annot(CSeq_annot& annot,
                                   TPriority pri = kPriority_NotSet);

    /// Get editable Biosec handle by regular one
    CBioseq_EditHandle     GetEditHandle(const CBioseq_Handle&     seq);

    /// Get editable SeqEntry handle by regular one
    CSeq_entry_EditHandle  GetEditHandle(const CSeq_entry_Handle&  entry);

    /// Get editable Seq-annot handle by regular one
    CSeq_annot_EditHandle  GetEditHandle(const CSeq_annot_Handle&  annot);

    /// Get editable Biosec-set handle by regular one
    CBioseq_set_EditHandle GetEditHandle(const CBioseq_set_Handle& seqset);

    /// Clean all unused TSEs from the scope's cache and release the memory.
    /// TSEs referenced by any handles are not removed.
    void ResetHistory(void);
    /// Remove single TSE from the scope's history. If there are other
    /// live handles referencing the TSE, nothing is removed.
    /// @param tse
    ///  TSE to be removed from the cache. If the TSE is removed from the
    ///  histoty, the handle is reset.
    void RemoveFromHistory(CTSE_Handle& tse);
    /// Remove the bioseq's TSE from the scope's history. If there are other
    /// live handles referencing the TSE, nothing is removed.
    /// @param bioseq
    ///  Bioseq, which TSE is to be removed from the cache. The handle
    /// is reset even if the TSE can not be removed from the scope's history.
    void RemoveFromHistory(CBioseq_Handle& bioseq);

    /// Revoke data loader from the scope. Throw exception if the
    /// operation fails (e.g. data source is in use or not found).
    void RemoveDataLoader(const string& loader_name);
    // Revoke TSE previously added using AddTopLevelSeqEntry() or
    // AddBioseq(). Throw exception if the TSE is still in use or
    /// not found in the scope.
    void RemoveTopLevelSeqEntry(CTSE_Handle& entry);

    typedef CBioseq_Handle::TId TIds;
    /// Get "native" bioseq ids without filtering and matching.
    TIds GetIds(const CSeq_id&        id );

    /// Get "native" bioseq ids without filtering and matching.
    TIds GetIds(const CSeq_id_Handle& idh);


    /// Get bioseq synonyms, resolving to the bioseq in this scope.
    CConstRef<CSynonymsSet> GetSynonyms(const CSeq_id&        id);

    /// Get bioseq synonyms, resolving to the bioseq in this scope.
    CConstRef<CSynonymsSet> GetSynonyms(const CSeq_id_Handle& id);

    /// Get bioseq synonyms, resolving to the bioseq in this scope.
    CConstRef<CSynonymsSet> GetSynonyms(const CBioseq_Handle& bh);

    // deprecated interface
    void AttachEntry(CSeq_entry& parent, CSeq_entry& entry);
    void RemoveEntry(CSeq_entry& entry);

    void AttachAnnot(CSeq_entry& parent, CSeq_annot& annot);
    void RemoveAnnot(CSeq_entry& parent, CSeq_annot& annot);
    void ReplaceAnnot(CSeq_entry& entry,
                      CSeq_annot& old_annot, CSeq_annot& new_annot);

    CBioseq_Handle GetBioseqHandleFromTSE(const CSeq_id& id,
                                          const CSeq_entry& tse);
    CBioseq_Handle GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                          const CSeq_entry& tse);

    enum ETSEKind {
        eManualTSEs,
        eAllTSEs
    };
    typedef vector<CSeq_entry_Handle> TTSE_Handles;
    void GetAllTSEs(TTSE_Handles& tses, enum ETSEKind kind = eManualTSEs);

protected:
    CScope_Impl& GetImpl(void);

private:
    // to prevent copying
    CScope(const CScope&);
    CScope& operator=(const CScope&);

    friend class CSeqMap_CI;
    friend class CSeq_annot_CI;
    friend class CAnnot_Collector;
    friend class CBioseq_CI;
    friend class CHeapScope;
    friend class CPrefetchToken_Impl;

    CRef<CScope>      m_HeapScope;
    CRef<CScope_Impl> m_Impl;
};


/////////////////////////////////////////////////////////////////////////////
// CScope inline methods
/////////////////////////////////////////////////////////////////////////////


inline
CScope_Impl& CScope::GetImpl(void)
{
    return *m_Impl;
}

/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.85  2005/03/14 18:17:14  grichenk
* Added CScope::RemoveFromHistory(), CScope::RemoveTopLevelSeqEntry() and
* CScope::RemoveDataLoader(). Added requested seq-id information to
* CTSE_Info.
*
* Revision 1.84  2004/12/22 15:56:04  vasilche
* Introduced CTSE_Handle.
*
* Revision 1.83  2004/11/22 16:04:06  grichenk
* Fixed/added doxygen comments
*
* Revision 1.82  2004/09/28 14:30:02  vasilche
* Implemented CScope::AddSeq_annot().
*
* Revision 1.81  2004/09/27 13:55:23  kononenk
* Added doxygen formating
*
* Revision 1.80  2004/08/31 21:03:48  grichenk
* Added GetIds()
*
* Revision 1.79  2004/07/12 15:05:31  grichenk
* Moved seq-id mapper from xobjmgr to seq library
*
* Revision 1.78  2004/04/16 13:31:46  grichenk
* Added data pre-fetching functions.
*
* Revision 1.77  2004/04/13 16:39:36  grichenk
* Corrected comments
*
* Revision 1.76  2004/04/13 15:59:35  grichenk
* Added CScope::GetBioseqHandle() with id resolving flag.
*
* Revision 1.75  2004/04/12 18:40:24  grichenk
* Added GetAllTSEs()
*
* Revision 1.74  2004/04/05 15:56:13  grichenk
* Redesigned CAnnotTypes_CI: moved all data and data collecting
* functions to CAnnotDataCollector. CAnnotTypes_CI is no more
* inherited from SAnnotSelector.
*
* Revision 1.73  2004/03/24 18:30:28  vasilche
* Fixed edit API.
* Every *_Info object has its own shallow copy of original object.
*
* Revision 1.72  2004/03/16 15:47:26  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.71  2004/02/19 17:23:00  vasilche
* Changed order of deletion of heap scope and scope impl objects.
* Reduce number of calls to x_ResetHistory().
*
* Revision 1.70  2004/02/02 14:46:42  vasilche
* Several performance fixed - do not iterate whole tse set in CDataSource.
*
* Revision 1.69  2004/01/29 20:33:27  vasilche
* Do not resolve any Seq-ids in CScope::GetSynonyms() -
* assume all not resolved Seq-id as synonym.
* Seq-id conflict messages made clearer.
*
* Revision 1.68  2004/01/27 17:11:13  ucko
* Remove redundant forward declaration of CTSE_Info
*
* Revision 1.67  2003/12/29 20:04:52  grichenk
* CHeapScope can be initialized with null pointer.
*
* Revision 1.66  2003/12/18 16:38:05  grichenk
* Added CScope::RemoveEntry()
*
* Revision 1.65  2003/11/28 15:13:24  grichenk
* Added CSeq_entry_Handle
*
* Revision 1.64  2003/11/21 20:33:03  grichenk
* Added GetBioseqHandleFromTSE(CSeq_id, CSeq_entry)
*
* Revision 1.63  2003/11/12 15:49:48  vasilche
* Added loading annotations on non gi Seq-id.
*
* Revision 1.62  2003/10/07 13:43:22  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.61  2003/09/30 16:21:59  vasilche
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
* Revision 1.60  2003/09/05 20:50:24  grichenk
* +AddBioseq(CBioseq&)
*
* Revision 1.59  2003/09/03 19:59:59  grichenk
* Added sequence filtering by level (mains/parts/all)
*
* Revision 1.58  2003/08/12 18:25:39  grichenk
* Fixed default priorities
*
* Revision 1.57  2003/08/06 20:51:14  grichenk
* Removed declarations of non-existent methods
*
* Revision 1.56  2003/08/04 17:02:57  grichenk
* Added constructors to iterate all annotations from a
* seq-entry or seq-annot.
*
* Revision 1.55  2003/07/25 15:25:22  grichenk
* Added CSeq_annot_CI class
*
* Revision 1.54  2003/07/17 20:07:55  vasilche
* Reduced memory usage by feature indexes.
* SNP data is loaded separately through PUBSEQ_OS.
* String compression for SNP data.
*
* Revision 1.53  2003/06/30 18:42:09  vasilche
* CPriority_I made to use less memory allocations/deallocations.
*
* Revision 1.52  2003/06/19 18:34:07  vasilche
* Fixed compilation on Windows.
*
* Revision 1.51  2003/06/19 18:23:44  vasilche
* Added several CXxx_ScopeInfo classes for CScope related information.
* CBioseq_Handle now uses reference to CBioseq_ScopeInfo.
* Some fine tuning of locking in CScope.
*
* Revision 1.49  2003/05/27 19:44:04  grichenk
* Added CSeqVector_CI class
*
* Revision 1.48  2003/05/20 15:44:37  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.47  2003/05/14 18:39:25  grichenk
* Simplified TSE caching and filtering in CScope, removed
* some obsolete members and functions.
*
* Revision 1.46  2003/05/12 19:18:28  vasilche
* Fixed locking of object manager classes in multi-threaded application.
*
* Revision 1.45  2003/05/06 18:54:06  grichenk
* Moved TSE filtering from CDataSource to CScope, changed
* some filtering rules (e.g. priority is now more important
* than scope history). Added more caches to CScope.
*
* Revision 1.44  2003/04/29 19:51:12  vasilche
* Fixed interaction of Data Loader garbage collector and TSE locking mechanism.
* Made some typedefs more consistent.
*
* Revision 1.43  2003/04/24 17:24:57  vasilche
* SAnnotSelector is struct.
* Added include required by MS VC.
*
* Revision 1.42  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.41  2003/04/09 16:04:29  grichenk
* SDataSourceRec replaced with CPriorityNode
* Added CScope::AddScope(scope, priority) to allow scope nesting
*
* Revision 1.40  2003/04/03 14:18:08  vasilche
* Added public GetSynonyms() method.
*
* Revision 1.39  2003/03/26 21:00:07  grichenk
* Added seq-id -> tse with annotation cache to CScope
*
* Revision 1.38  2003/03/24 21:26:42  grichenk
* Added support for CTSE_CI
*
* Revision 1.37  2003/03/21 19:22:48  grichenk
* Redesigned TSE locking, replaced CTSE_Lock with CRef<CTSE_Info>.
*
* Revision 1.36  2003/03/18 14:46:36  grichenk
* Set limit object type to "none" if the object is null.
*
* Revision 1.35  2003/03/12 20:09:30  grichenk
* Redistributed members between CBioseq_Handle, CBioseq_Info and CTSE_Info
*
* Revision 1.34  2003/03/11 14:15:49  grichenk
* +Data-source priority
*
* Revision 1.33  2003/03/10 16:55:16  vasilche
* Cleaned SAnnotSelector structure.
* Added shortcut when features are limited to one TSE.
*
* Revision 1.32  2003/03/03 20:32:47  vasilche
* Removed obsolete method GetSynonyms().
*
* Revision 1.31  2003/02/28 20:02:35  grichenk
* Added synonyms cache and x_GetSynonyms()
*
* Revision 1.30  2003/02/27 14:35:32  vasilche
* Splitted PopulateTSESet() by logically independent parts.
*
* Revision 1.29  2003/02/24 18:57:21  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.28  2003/01/22 20:11:53  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.27  2002/12/26 20:51:36  dicuccio
* Added Win32 export specifier
*
* Revision 1.26  2002/12/26 16:39:21  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.25  2002/11/08 22:15:50  grichenk
* Added methods for removing/replacing annotations
*
* Revision 1.24  2002/11/01 05:34:06  kans
* added GetBioseqHandle taking CBioseq parameter
*
* Revision 1.23  2002/10/31 22:25:09  kans
* added GetBioseqHandle taking CSeq_loc parameter
*
* Revision 1.22  2002/10/18 19:12:39  grichenk
* Removed mutex pools, converted most static mutexes to non-static.
* Protected CSeqMap::x_Resolve() with mutex. Modified code to prevent
* dead-locks.
*
* Revision 1.21  2002/10/02 17:58:21  grichenk
* Added sequence type filter to CBioseq_CI
*
* Revision 1.20  2002/09/30 20:01:17  grichenk
* Added methods to support CBioseq_CI
*
* Revision 1.19  2002/07/08 20:50:56  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.18  2002/06/04 17:18:32  kimelman
* memory cleanup :  new/delete/Cref rearrangements
*
* Revision 1.17  2002/05/28 18:01:11  gouriano
* DebugDump added
*
* Revision 1.16  2002/05/14 20:06:23  grichenk
* Improved CTSE_Info locking by CDataSource and CDataLoader
*
* Revision 1.15  2002/05/06 03:30:36  vakatov
* OM/OM1 renaming
*
* Revision 1.14  2002/05/03 21:28:02  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.13  2002/04/17 21:09:38  grichenk
* Fixed annotations loading
*
* Revision 1.12  2002/04/08 19:14:15  gouriano
* corrected comment to AddDefaults()
*
* Revision 1.11  2002/03/27 18:46:26  gouriano
* three functions made public
*
* Revision 1.10  2002/03/20 21:20:38  grichenk
* +CScope::ResetHistory()
*
* Revision 1.9  2002/02/21 19:27:00  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.8  2002/02/07 21:27:33  grichenk
* Redesigned CDataSource indexing: seq-id handle -> TSE -> seq/annot
*
* Revision 1.7  2002/02/06 21:46:43  gouriano
* *** empty log message ***
*
* Revision 1.6  2002/02/05 21:47:21  gouriano
* added FindSeqid function, minor tuneup in CSeq_id_mapper
*
* Revision 1.5  2002/01/28 19:45:33  gouriano
* changed the interface of BioseqHandle: two functions moved from Scope
*
* Revision 1.4  2002/01/23 21:59:29  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.3  2002/01/18 17:07:12  gouriano
* renamed GetSequence to GetSeqVector
*
* Revision 1.2  2002/01/16 16:26:35  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:04:03  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#endif//OBJMGR_SCOPE__HPP
