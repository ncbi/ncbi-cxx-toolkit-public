#ifndef OBJECTS_OBJMGR_IMPL___DATA_SOURCE__HPP
#define OBJECTS_OBJMGR_IMPL___DATA_SOURCE__HPP

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
*   Data source for object manager
*
*/

#include <objmgr/impl/tse_info.hpp>

#include <objects/seq/seq_id_mapper.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/data_loader.hpp>

#include <corelib/ncbimtx.hpp>

#include <set>
#include <map>
#include <list>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// objects
class CDelta_seq;
class CDelta_ext;
class CSeq_interval;
class CSeq_data;
class CSeq_entry;
class CSeq_annot;
class CBioseq;
class CBioseq_set;

// infos
class CTSE_Info;
class CSeq_entry_Info;
class CSeq_annot_Info;
class CBioseq_Info;

// others
class CBioseq_Handle;
class CPrefetchToken_Impl;
class CPrefetchThread;

class CTSE_LockingSetLock;

class NCBI_XOBJMGR_EXPORT CTSE_LockingSet
{
public:
    CTSE_LockingSet(void);
    CTSE_LockingSet(const CTSE_LockingSet&);
    ~CTSE_LockingSet(void);
    CTSE_LockingSet& operator=(const CTSE_LockingSet&);

    typedef set<CTSE_Info*> TTSESet;
    typedef TTSESet::iterator iterator;
    typedef TTSESet::const_iterator const_iterator;

    bool insert(CTSE_Info* tse);
    bool erase(CTSE_Info* tse);

    const_iterator begin(void) const
        {
            return m_TSE_set.begin();
        }
    const_iterator end(void) const
        {
            return m_TSE_set.end();
        }

    bool empty(void) const
        {
            return m_TSE_set.empty();
        }

    friend class CTSE_LockingSetLock;

private:
    bool x_Locked(void) const
        {
            return m_LockCount != 0;
        }
    void x_Lock(void);
    void x_Unlock(void);

    DECLARE_CLASS_STATIC_FAST_MUTEX(sm_Mutex);

    int        m_LockCount;
    TTSESet    m_TSE_set;
};


class NCBI_XOBJMGR_EXPORT CTSE_LockingSetLock
{
public:
    CTSE_LockingSetLock(void)
        : m_TSE_set(0)
        {
        }
    CTSE_LockingSetLock(CTSE_LockingSet& tse_set)
        : m_TSE_set(&tse_set)
        {
            x_Lock();
        }
    CTSE_LockingSetLock(const CTSE_LockingSetLock& lock)
        : m_TSE_set(lock.m_TSE_set)
        {
            x_Lock();
        }
    ~CTSE_LockingSetLock(void)
        {
            x_Unlock();
        }
    CTSE_LockingSetLock& operator=(const CTSE_LockingSetLock& lock)
        {
            x_Lock(lock.m_TSE_set);
            return *this;
        }

    void Lock(CTSE_LockingSet& lock)
        {
            x_Lock(&lock);
        }
    void Unlock(void)
        {
            x_Lock(0);
        }

private:
    void x_Lock(void)
        {
            if ( m_TSE_set ) {
                m_TSE_set->x_Lock();
            }
        }
    void x_Unlock(void)
        {
            if ( m_TSE_set ) {
                m_TSE_set->x_Unlock();
            }
        }
    void x_Lock(CTSE_LockingSet* lock)
        {
            if ( m_TSE_set != lock ) {
                x_Unlock();
                m_TSE_set = lock;
                x_Lock();
            }
        }

    CTSE_LockingSet* m_TSE_set;
};


class NCBI_XOBJMGR_EXPORT CDataSource : public CObject
{
public:
    /// 'ctors
    CDataSource(CDataLoader& loader, CObjectManager& objmgr);
    CDataSource(CSeq_entry& entry, CObjectManager& objmgr);
    virtual ~CDataSource(void);

    /// Register new TSE (Top Level Seq-entry)
    typedef set<TTSE_Lock>     TTSE_LockSet;

    CRef<CTSE_Info> AddTSE(CSeq_entry& se,
                           bool dead = false,
                           const CObject* blob_id = 0);
    void AddTSE(CRef<CTSE_Info> tse);


    // Modification methods.
    /// Add new sub-entry to "parent".
    CRef<CSeq_entry_Info> AttachEntry(CBioseq_set_Info& parent,
                                      CSeq_entry& entry,
                                      int index = -1);
    void RemoveEntry(CSeq_entry_Info& entry);

    /// Add annotations to a Seq-entry.
    CRef<CSeq_annot_Info> AttachAnnot(CSeq_entry_Info& parent,
                                      const CSeq_annot& annot);
    CRef<CSeq_annot_Info> AttachAnnot(CBioseq_Base_Info& parent,
                                      const CSeq_annot& annot);
    // Remove/replace seq-annot from the given entry
    void RemoveAnnot(CSeq_annot_Info& annot);
    CRef<CSeq_annot_Info> ReplaceAnnot(CSeq_annot_Info& old_annot,
                                       const CSeq_annot& new_annot);

    /// Get TSE info by seq-id handle. This should also get the list of all
    /// seq-ids for all bioseqs and the list of seq-ids used in annotations.
    TTSE_Lock GetBlobById(const CSeq_id_Handle& idh);

    /// Get Bioseq info by Seq-Id.
    /// Return "NULL" handle if the Bioseq cannot be resolved.
    CConstRef<CBioseq_Info> GetBioseq_Info(const CSeqMatch_Info& info);

    // Remove TSE from the datasource, update indexes
    void DropAllTSEs(void);
    bool DropTSE(CTSE_Info& info);

    // Contains (or can load) any entries?
    bool IsEmpty(void) const;

    CDataLoader* GetDataLoader(void) const;

    CConstRef<CSeq_entry> GetTopEntry(void);
    CConstRef<CTSE_Info> GetTopEntry_Info(void);

    // Internal typedefs
    typedef CTSE_Info::TRange                        TRange;
    typedef CTSE_Info::TRangeMap                     TRangeMap;

    typedef map<CSeq_id_Handle, CTSE_LockingSet>     TTSEMap;

    typedef map<CConstRef<CSeq_entry>, CTSE_Info*>       TTSE_InfoMap;
    typedef map<CConstRef<CSeq_entry>, CSeq_entry_Info*> TSeq_entry_InfoMap;
    typedef map<CConstRef<CSeq_annot>, CSeq_annot_Info*> TSeq_annot_InfoMap;
    typedef map<CConstRef<CBioseq>, CBioseq_Info*>       TBioseq_InfoMap;
    //typedef map<const CObject*, CAnnotObject_Info* > TAnnotObject_InfoMap;
    typedef set< CRef<CTSE_Info> >                   TTSE_Set;
    typedef int                                      TPriority;
    typedef set<CTSE_Info*>                          TDirtyAnnot_TSEs;

    void UpdateAnnotIndex(void);
    void UpdateAnnotIndex(const CSeq_entry_Info& entry_info);
    void UpdateAnnotIndex(const CSeq_annot_Info& annot_info);
    void GetSynonyms(const CSeq_id_Handle& id, set<CSeq_id_Handle>& syns);
    void GetTSESetWithAnnots(const CSeq_id_Handle& idh, TTSE_LockSet& tse_set);

    // Fill the set with bioseq handles for all sequences from a given TSE.
    // Return empty tse lock if the entry was not found or is not a TSE.
    // "filter" may be used to select a particular sequence type.
    // "level" may be used to select bioseqs from given levels only.
    // Used to initialize bioseq iterators.
    typedef vector< CConstRef<CBioseq_Info> > TBioseq_InfoSet;
    typedef int TBioseqLevelFlag;
    void GetBioseqs(const CSeq_entry_Info& entry,
                    TBioseq_InfoSet& bioseqs,
                    CSeq_inst::EMol filter,
                    TBioseqLevelFlag level);

    CSeqMatch_Info BestResolve(CSeq_id_Handle idh);
    CSeqMatch_Info HistoryResolve(CSeq_id_Handle idh,
                                  const TTSE_LockSet& tse_set);

    // Select the best of the two bioseqs if possible (e.g. dead vs live).
    CSeqMatch_Info* ResolveConflict(const CSeq_id_Handle& id,
                                    CSeqMatch_Info& info1,
                                    CSeqMatch_Info& info2);
    bool IsLive(const CTSE_Info& tse);

    //bool IsSynonym(const CSeq_id_Handle& h1, const CSeq_id_Handle& h2) const;

    string GetName(void) const;

    TPriority GetDefaultPriority(void) const;
    void SetDefaultPriority(TPriority priority);

    virtual void DebugDump(CDebugDumpContext ddc, unsigned int depth) const;

    CConstRef<CTSE_Info> FindTSEInfo(const CSeq_entry& entry);
    CConstRef<CSeq_entry_Info> FindSeq_entry_Info(const CSeq_entry& entry);
    CConstRef<CSeq_annot_Info> FindSeq_annot_Info(const CSeq_annot& annot);
    CConstRef<CBioseq_Info> FindBioseq_Info(const CBioseq& bioseq);

    virtual void Prefetch(CPrefetchToken_Impl& token);

private:
    friend class CAnnotTypes_CI; // using mutex etc.
    friend class CBioseq_Handle; // using mutex
    friend class CGBDataLoader;  //
    friend class CTSE_Info;
    friend class CSeq_entry_Info;
    friend class CSeq_annot_Info;
    friend class CBioseq_Info;
    friend class CPrefetchToken_Impl;

    // attach, detach, index & unindex methods
    // TSE
    void x_DropTSE(CTSE_Info& info);

    void x_Map(CConstRef<CSeq_entry> obj, CTSE_Info* info);
    void x_Unmap(CConstRef<CSeq_entry> obj, CTSE_Info* info);
    void x_Map(CConstRef<CSeq_entry> obj, CSeq_entry_Info* info);
    void x_Unmap(CConstRef<CSeq_entry> obj, CSeq_entry_Info* info);
    void x_Map(CConstRef<CSeq_annot> obj, CSeq_annot_Info* info);
    void x_Unmap(CConstRef<CSeq_annot> obj, CSeq_annot_Info* info);
    void x_Map(CConstRef<CBioseq> obj, CBioseq_Info* info);
    void x_Unmap(CConstRef<CBioseq> obj, CBioseq_Info* info);

    //void x_AttachMap(CSeq_entry_Info& bioseq, CSeqMap& seqmap);

    // lookup Xxx_Info objects
    // TSE
    CConstRef<CTSE_Info> x_FindTSE_Info(const CSeq_entry& tse);
    // Seq-entry
    CConstRef<CSeq_entry_Info> x_FindSeq_entry_Info(const CSeq_entry& entry);
    // Bioseq
    CConstRef<CBioseq_Info> x_FindBioseq_Info(const CBioseq& seq);
    // Seq-annot
    CConstRef<CSeq_annot_Info> x_FindSeq_annot_Info(const CSeq_annot& annot);

    // Find the seq-entry with best bioseq for the seq-id handle.
    // The best bioseq is the bioseq from the live TSE or from the
    // only one TSE containing the ID (no matter live or dead).
    // If no matches were found, return 0.
    TTSE_Lock x_FindBestTSE(const CSeq_id_Handle& handle) const;

    void x_SetDirtyAnnotIndex(CTSE_Info& tse);
    void x_ResetDirtyAnnotIndex(CTSE_Info& tse);

    void x_IndexTSE(TTSEMap& tse_map,
                    const CSeq_id_Handle& id, CTSE_Info* tse_info);
    void x_UnindexTSE(TTSEMap& tse_map,
                      const CSeq_id_Handle& id, CTSE_Info* tse_info);
    void x_IndexSeqTSE(const CSeq_id_Handle& idh, CTSE_Info* tse_info);
    void x_UnindexSeqTSE(const CSeq_id_Handle& idh, CTSE_Info* tse_info);
    void x_IndexAnnotTSE(const CSeq_id_Handle& idh, CTSE_Info* tse_info);
    void x_UnindexAnnotTSE(const CSeq_id_Handle& idh, CTSE_Info* tse_info);
    void x_IndexAnnotTSEs(CTSE_Info* tse_info);
    void x_UnindexAnnotTSEs(CTSE_Info* tse_info);

    // Global cleanup -- search for unlocked TSEs and drop them.
    void x_CleanupUnusedEntries(void);

    // Change live/dead status of a TSE
    void x_UpdateTSEStatus(CTSE_Info& tse, bool dead);

    void x_CollectBioseqs(const CSeq_entry_Info& info,
                          TBioseq_InfoSet& bioseqs,
                          CSeq_inst::EMol filter,
                          TBioseqLevelFlag level);

#if 0
    typedef CRWLock TMainLock;
#else
    typedef CMutex TMainLock;
#endif
#if 0
    typedef CRWLock TAnnotLock;
#else
    typedef CFastMutex TAnnotLock;
#endif
    typedef TMainLock::TReadLockGuard   TMainReadLockGuard;
    typedef TMainLock::TWriteLockGuard  TMainWriteLockGuard;
    typedef TAnnotLock::TReadLockGuard  TAnnotReadLockGuard;
    typedef TAnnotLock::TWriteLockGuard TAnnotWriteLockGuard;

    // Used to lock: m_*_InfoMap, m_TSE_seq
    // Is locked before locks in CTSE_Info
    mutable TMainLock     m_DSMainLock;
    // Used to lock: m_TSE_annot, m_TSE_annot_is_dirty
    // Is locked after locks in CTSE_Info
    mutable TAnnotLock    m_DSAnnotLock;

    CRef<CDataLoader>     m_Loader;
    CConstRef<CSeq_entry> m_pTopEntry;
    CObjectManager*       m_ObjMgr;

    TTSE_Set              m_TSE_Set;
    TTSE_InfoMap          m_TSE_InfoMap;       // All known TSEs, (locked once)
    TSeq_entry_InfoMap    m_Seq_entry_InfoMap; // All known Seq-entries
    TSeq_annot_InfoMap    m_Seq_annot_InfoMap; // All known Seq-annots
    TBioseq_InfoMap       m_Bioseq_InfoMap;    // All known Bioseqs

    TTSEMap               m_TSE_seq;           // id -> TSE with bioseq
    TTSEMap               m_TSE_annot;         // id -> TSE with annots
    // TSE with annotations need to be indexed.
    TDirtyAnnot_TSEs      m_DirtyAnnot_TSEs;

    // Default priority for the datasource
    TPriority             m_DefaultPriority;

    // Prefetching thread and lock, used when initializing the thread
    CRef<CPrefetchThread> m_PrefetchThread;
    CFastMutex            m_PrefetchLock;

    // hide copy constructor
    CDataSource(const CDataSource&);
    CDataSource& operator=(const CDataSource&);
};

inline
CDataLoader* CDataSource::GetDataLoader(void) const
{
    return const_cast<CDataLoader*>(m_Loader.GetPointerOrNull());
}

inline
CConstRef<CSeq_entry> CDataSource::GetTopEntry(void)
{
    return m_pTopEntry;
}

inline
bool CDataSource::IsEmpty(void) const
{
    return m_Loader == 0  &&  m_TSE_Set.empty();
}

inline
bool CDataSource::IsLive(const CTSE_Info& tse)
{
    return !tse.IsDead();
}

inline
CDataSource::TPriority CDataSource::GetDefaultPriority(void) const
{
    return m_DefaultPriority;
}

inline
void CDataSource::SetDefaultPriority(TPriority priority)
{
    m_DefaultPriority = priority;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.75  2004/07/12 15:05:31  grichenk
* Moved seq-id mapper from xobjmgr to seq library
*
* Revision 1.74  2004/04/16 13:31:46  grichenk
* Added data pre-fetching functions.
*
* Revision 1.73  2004/03/31 17:08:06  vasilche
* Implemented ConvertSeqToSet and ConvertSetToSeq.
*
* Revision 1.72  2004/03/24 18:30:28  vasilche
* Fixed edit API.
* Every *_Info object has its own shallow copy of original object.
*
* Revision 1.71  2004/03/16 15:47:26  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.70  2004/02/03 19:02:16  vasilche
* Fixed broken 'dirty annot index' state after RemoveEntry().
*
* Revision 1.69  2004/02/02 14:46:42  vasilche
* Several performance fixed - do not iterate whole tse set in CDataSource.
*
* Revision 1.68  2003/12/18 16:38:05  grichenk
* Added CScope::RemoveEntry()
*
* Revision 1.67  2003/10/07 13:43:22  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.66  2003/09/30 16:22:01  vasilche
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
* Revision 1.65  2003/09/03 20:00:00  grichenk
* Added sequence filtering by level (mains/parts/all)
*
* Revision 1.64  2003/08/15 19:19:15  vasilche
* Fixed memory leak in string packing hooks.
* Fixed processing of 'partial' flag of features.
* Allow table packing of non-point SNP.
* Allow table packing of SNP with long alleles.
*
* Revision 1.63  2003/08/14 20:05:18  vasilche
* Simple SNP features are stored as table internally.
* They are recreated when needed using CFeat_CI.
*
* Revision 1.62  2003/08/06 20:51:16  grichenk
* Removed declarations of non-existent methods
*
* Revision 1.61  2003/08/04 17:04:29  grichenk
* Added default data-source priority assignment.
* Added support for iterating all annotations from a
* seq-entry or seq-annot.
*
* Revision 1.60  2003/07/17 20:07:55  vasilche
* Reduced memory usage by feature indexes.
* SNP data is loaded separately through PUBSEQ_OS.
* String compression for SNP data.
*
* Revision 1.59  2003/06/24 14:25:18  vasilche
* Removed obsolete CTSE_Guard class.
* Used separate mutexes for bioseq and annot maps.
*
* Revision 1.58  2003/06/19 18:23:44  vasilche
* Added several CXxx_ScopeInfo classes for CScope related information.
* CBioseq_Handle now uses reference to CBioseq_ScopeInfo.
* Some fine tuning of locking in CScope.
*
* Revision 1.55  2003/05/20 15:44:37  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.54  2003/05/14 18:39:26  grichenk
* Simplified TSE caching and filtering in CScope, removed
* some obsolete members and functions.
*
* Revision 1.53  2003/05/06 18:54:08  grichenk
* Moved TSE filtering from CDataSource to CScope, changed
* some filtering rules (e.g. priority is now more important
* than scope history). Added more caches to CScope.
*
* Revision 1.52  2003/05/05 20:59:48  vasilche
* Use one static mutex for all instances of CTSE_LockingSet.
*
* Revision 1.51  2003/04/29 19:51:12  vasilche
* Fixed interaction of Data Loader garbage collector and TSE locking mechanism.
* Made some typedefs more consistent.
*
* Revision 1.50  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.49  2003/04/14 21:32:16  grichenk
* Avoid passing CScope as an argument to CDataSource methods
*
* Revision 1.48  2003/03/24 21:26:43  grichenk
* Added support for CTSE_CI
*
* Revision 1.47  2003/03/21 19:22:50  grichenk
* Redesigned TSE locking, replaced CTSE_Lock with CRef<CTSE_Info>.
*
* Revision 1.46  2003/03/18 21:48:28  grichenk
* Removed obsolete class CAnnot_CI
*
* Revision 1.45  2003/03/18 14:52:57  grichenk
* Removed obsolete methods, replaced seq-id with seq-id handle
* where possible. Added argument to limit annotations update to
* a single seq-entry.
*
* Revision 1.44  2003/03/11 14:15:50  grichenk
* +Data-source priority
*
* Revision 1.43  2003/03/10 16:55:16  vasilche
* Cleaned SAnnotSelector structure.
* Added shortcut when features are limited to one TSE.
*
* Revision 1.42  2003/03/03 20:31:09  vasilche
* Removed obsolete method PopulateTSESet().
*
* Revision 1.41  2003/02/27 14:35:32  vasilche
* Splitted PopulateTSESet() by logically independent parts.
*
* Revision 1.40  2003/02/25 14:48:07  vasilche
* Added Win32 export modifier to object manager classes.
*
* Revision 1.39  2003/02/24 18:57:21  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.38  2003/02/24 14:51:10  grichenk
* Minor improvements in annot indexing
*
* Revision 1.37  2003/02/13 14:34:32  grichenk
* Renamed CAnnotObject -> CAnnotObject_Info
* + CSeq_annot_Info and CAnnotObject_Ref
* Moved some members of CAnnotObject to CSeq_annot_Info
* and CAnnotObject_Ref.
* Added feat/align/graph to CAnnotObject_Info map
* to CDataSource.
*
* Revision 1.36  2003/02/05 17:57:41  dicuccio
* Moved into include/objects/objmgr/impl to permit data loaders to be defined
* outside of xobjmgr.
*
* Revision 1.35  2003/01/29 22:03:46  grichenk
* Use single static CSeq_id_Mapper instead of per-OM model.
*
* Revision 1.34  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.33  2002/12/26 20:55:17  dicuccio
* Moved seq_id_mapper.hpp, tse_info.hpp, and bioseq_info.hpp -> include/ tree
*
* Revision 1.32  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.31  2002/12/06 15:36:00  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.30  2002/11/08 22:15:51  grichenk
* Added methods for removing/replacing annotations
*
* Revision 1.29  2002/10/18 19:12:40  grichenk
* Removed mutex pools, converted most static mutexes to non-static.
* Protected CSeqMap::x_Resolve() with mutex. Modified code to prevent
* dead-locks.
*
* Revision 1.28  2002/10/02 17:58:23  grichenk
* Added sequence type filter to CBioseq_CI
*
* Revision 1.27  2002/09/30 20:01:19  grichenk
* Added methods to support CBioseq_CI
*
* Revision 1.26  2002/07/08 20:51:01  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.25  2002/06/04 17:18:33  kimelman
* memory cleanup :  new/delete/Cref rearrangements
*
* Revision 1.24  2002/05/31 17:53:00  grichenk
* Optimized for better performance (CTSE_Info uses atomic counter,
* delayed annotations indexing, no location convertions in
* CAnnot_Types_CI if no references resolution is required etc.)
*
* Revision 1.23  2002/05/28 18:00:43  gouriano
* DebugDump added
*
* Revision 1.22  2002/05/14 20:06:26  grichenk
* Improved CTSE_Info locking by CDataSource and CDataLoader
*
* Revision 1.21  2002/05/06 03:28:47  vakatov
* OM/OM1 renaming
*
* Revision 1.20  2002/05/03 21:28:09  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.19  2002/04/17 21:09:14  grichenk
* Fixed annotations loading
* +IsSynonym()
*
* Revision 1.18  2002/04/11 12:08:21  grichenk
* Fixed GetResolvedSeqMap() implementation
*
* Revision 1.17  2002/03/28 14:02:31  grichenk
* Added scope history checks to CDataSource::x_FindBestTSE()
*
* Revision 1.16  2002/03/20 04:50:13  kimelman
* GB loader added
*
* Revision 1.15  2002/03/07 21:25:34  grichenk
* +GetSeq_annot() in annotation iterators
*
* Revision 1.14  2002/03/05 18:44:55  grichenk
* +x_UpdateTSEStatus()
*
* Revision 1.13  2002/03/05 16:09:10  grichenk
* Added x_CleanupUnusedEntries()
*
* Revision 1.12  2002/03/04 15:09:27  grichenk
* Improved MT-safety. Added live/dead flag to CDataSource methods.
*
* Revision 1.11  2002/03/01 19:41:34  gouriano
* *** empty log message ***
*
* Revision 1.10  2002/02/28 20:53:32  grichenk
* Implemented attaching segmented sequence data. Fixed minor bugs.
*
* Revision 1.9  2002/02/21 19:27:05  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.8  2002/02/07 21:27:35  grichenk
* Redesigned CDataSource indexing: seq-id handle -> TSE -> seq/annot
*
* Revision 1.7  2002/02/06 21:46:11  gouriano
* *** empty log message ***
*
* Revision 1.6  2002/02/05 21:46:28  gouriano
* added FindSeqid function, minor tuneup in CSeq_id_mapper
*
* Revision 1.5  2002/01/29 17:45:00  grichenk
* Added seq-id handles locking
*
* Revision 1.4  2002/01/28 19:44:49  gouriano
* changed the interface of BioseqHandle: two functions moved from Scope
*
* Revision 1.3  2002/01/23 21:59:31  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.2  2002/01/18 15:56:24  gouriano
* changed TSeqMaps definition
*
* Revision 1.1  2002/01/16 16:25:55  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:04:01  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#endif  // OBJECTS_OBJMGR_IMPL___DATA_SOURCE__HPP
