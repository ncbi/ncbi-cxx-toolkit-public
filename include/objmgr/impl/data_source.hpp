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

#include <objects/seq/Seq_inst.hpp>
#include <objmgr/data_loader.hpp>

#include <objmgr/impl/mutex_pool.hpp>

#include <corelib/ncbimtx.hpp>

//#define DEBUG_MAPS
#ifdef DEBUG_MAPS
# include <util/debug/map.hpp>
# include <util/debug/set.hpp>
#endif

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
class CDSAnnotLockReadGuard;
class CDSAnnotLockWriteGuard;
class CScope_Impl;

struct SBlobIdComp
{
    typedef CConstRef<CObject>                      TBlobId;

    SBlobIdComp(CDataLoader* dl = 0)
        : m_DataLoader(dl)
        {
        }

    bool operator()(const TBlobId& id1, const TBlobId& id2) const
        {
            if ( m_DataLoader ) {
                return m_DataLoader->LessBlobId(id1, id2);
            }
            else {
                return id1 < id2;
            }
        }
        
    CRef<CDataLoader> m_DataLoader;
};


struct SSeqMatch_DS : public SSeqMatch_TSE
{
    SSeqMatch_DS(void)
        {
        }
    SSeqMatch_DS(const CTSE_Lock& tse_lock, const CSeq_id_Handle& id)
        : SSeqMatch_TSE(tse_lock->GetSeqMatch(id)),
          m_TSE_Lock(tse_lock)
        {
        }

    CTSE_Lock               m_TSE_Lock;
};


class NCBI_XOBJMGR_EXPORT CDataSource : public CObject
{
public:
    /// 'ctors
    CDataSource(void);
    CDataSource(CDataLoader& loader, CObjectManager& objmgr);
    CDataSource(CSeq_entry& entry, CObjectManager& objmgr);
    virtual ~CDataSource(void);

    // typedefs
    typedef int                                     TPriority;
    typedef CTSE_Lock                               TTSE_Lock;
    typedef CTSE_LockSet                            TTSE_LockSet;
    typedef set<CSeq_id_Handle>                     TSeq_idSet;
    typedef map<TTSE_Lock, TSeq_idSet>              TTSE_LockMatchSet;
    typedef CRef<CTSE_Info>                         TTSE_Ref;
    typedef CConstRef<CObject>                      TBlobId;

    /// Register new TSE (Top Level Seq-entry)
    TTSE_Lock AddTSE(CSeq_entry& se,
                     CTSE_Info::TBlobState = CBioseq_Handle::fState_none);
    TTSE_Lock AddTSE(CSeq_entry& se,
                     bool dead);
    TTSE_Lock AddTSE(CRef<CTSE_Info> tse);

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
    //TTSE_Lock GetBlobById(const CSeq_id_Handle& idh);

    // Remove TSE from the datasource, update indexes
    void DropAllTSEs(void);
    bool DropTSE(CTSE_Info& info);

    // Contains (or can load) any entries?
    bool IsEmpty(void) const;

    CDataLoader* GetDataLoader(void) const;

    CConstRef<CSeq_entry> GetTopEntry(void);
    TTSE_Lock GetTopEntry_Info(void);

    void UpdateAnnotIndex(void);
    void UpdateAnnotIndex(const CSeq_entry_Info& entry_info);
    void UpdateAnnotIndex(const CSeq_annot_Info& annot_info);

    TTSE_LockMatchSet GetTSESetWithOrphanAnnots(const TSeq_idSet& ids);
    TTSE_LockMatchSet GetTSESetWithBioseqAnnots(const CBioseq_Info& bioseq,
                                                const TTSE_Lock& tse);

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

    SSeqMatch_DS BestResolve(CSeq_id_Handle idh);
    typedef vector<SSeqMatch_DS> TSeqMatches;
    TSeqMatches GetMatches(CSeq_id_Handle idh, const TTSE_LockSet& locks);

    typedef vector<CSeq_id_Handle> TIds;
    void GetIds(const CSeq_id_Handle& idh, TIds& ids);

    bool IsLive(const CTSE_Info& tse);

    string GetName(void) const;

    TPriority GetDefaultPriority(void) const;
    void SetDefaultPriority(TPriority priority);

    // get locks
    enum FLockFlags {
        fLockNoHistory = 1<<0,
        fLockNoManual  = 1<<1,
        fLockNoThrow   = 1<<2
    };
    typedef int TLockFlags;
    TTSE_Lock x_LockTSE(const CTSE_Info& tse_info,
                        const TTSE_LockSet& locks,
                        TLockFlags = 0);
    CTSE_LoadLock GetTSE_LoadLock(const TBlobId& blob_id);
    bool IsLoaded(const CTSE_Info& tse) const;
    void SetLoaded(CTSE_LoadLock& lock);

    typedef pair<CConstRef<CSeq_entry_Info>, TTSE_Lock> TSeq_entry_Lock;
    typedef pair<CConstRef<CSeq_annot_Info>, TTSE_Lock> TSeq_annot_Lock;
    typedef pair<CConstRef<CBioseq_Info>, TTSE_Lock> TBioseq_Lock;

    TTSE_Lock FindTSE_Lock(const CSeq_entry& entry,
                           const TTSE_LockSet& history) const;
    TSeq_entry_Lock FindSeq_entry_Lock(const CSeq_entry& entry,
                                       const TTSE_LockSet& history) const;
    TSeq_annot_Lock FindSeq_annot_Lock(const CSeq_annot& annot,
                                       const TTSE_LockSet& history) const;
    TBioseq_Lock FindBioseq_Lock(const CBioseq& bioseq,
                                 const TTSE_LockSet& history) const;

    virtual void Prefetch(CPrefetchToken_Impl& token);

private:
    // internal typedefs

    // blob lookup map
    typedef map<TBlobId, TTSE_Ref, SBlobIdComp>     TBlob_Map;
    // unlocked blobs cache
    typedef list<TTSE_Ref>                          TBlob_Cache;

#ifdef DEBUG_MAPS
    typedef debug::set<TTSE_Ref>                    TTSE_Set;
    typedef debug::map<CSeq_id_Handle, TTSE_Set>    TSeq_id2TSE_Set;
#else
    typedef set<TTSE_Ref>                           TTSE_Set;
    typedef map<CSeq_id_Handle, TTSE_Set>           TSeq_id2TSE_Set;
#endif

    // registered objects
    typedef map<CConstRef<CSeq_entry>, CRef<CTSE_Info> >       TTSE_InfoMap;
    typedef map<CConstRef<CSeq_entry>, CRef<CSeq_entry_Info> > TEntry_InfoMap;
    typedef map<CConstRef<CSeq_annot>, CRef<CSeq_annot_Info> > TAnnot_InfoMap;
    typedef map<CConstRef<CBioseq>, CRef<CBioseq_Info> >       TBioseq_InfoMap;

    // friend classes
    friend class CAnnotTypes_CI; // using mutex etc.
    friend class CBioseq_Handle; // using mutex
    friend class CGBDataLoader;  //
    friend class CTSE_Info;
    friend class CTSE_Split_Info;
    friend class CTSE_Lock;
    friend class CTSE_LoadLock;
    friend class CSeq_entry_Info;
    friend class CSeq_annot_Info;
    friend class CBioseq_Info;
    friend class CPrefetchToken_Impl;
    friend class CScope_Impl;

    // 
    void x_SetLock(CTSE_Lock& lock, CConstRef<CTSE_Info> tse) const;
    //void x_SetLoadLock(CTSE_LoadLock& lock, CRef<CTSE_Info> tse) const;
    void x_ReleaseLastLock(CTSE_Lock& lock);
    void x_SetLoadLock(CTSE_LoadLock& loadlock, CTSE_Lock& lock);
    void x_SetLoadLock(CTSE_LoadLock& loadlock,
                       CTSE_Info& tse, CRef<CTSE_Info::CLoadMutex> load_mutex);
    void x_ReleaseLastLoadLock(CTSE_LoadLock& lock);
    void x_ReleaseLastTSELock(CRef<CTSE_Info> info);

    // attach, detach, index & unindex methods
    // TSE
    void x_ForgetTSE(CRef<CTSE_Info> info);
    void x_DropTSE(CRef<CTSE_Info> info);

    void x_Map(CConstRef<CSeq_entry> obj, CTSE_Info* info);
    void x_Unmap(CConstRef<CSeq_entry> obj, CTSE_Info* info);
    void x_Map(CConstRef<CSeq_entry> obj, CSeq_entry_Info* info);
    void x_Unmap(CConstRef<CSeq_entry> obj, CSeq_entry_Info* info);
    void x_Map(CConstRef<CSeq_annot> obj, CSeq_annot_Info* info);
    void x_Unmap(CConstRef<CSeq_annot> obj, CSeq_annot_Info* info);
    void x_Map(CConstRef<CBioseq> obj, CBioseq_Info* info);
    void x_Unmap(CConstRef<CBioseq> obj, CBioseq_Info* info);

    // lookup Xxx_Info objects
    // TSE
    CConstRef<CTSE_Info> x_FindTSE_Info(const CSeq_entry& tse) const;
    // Seq-entry
    CConstRef<CSeq_entry_Info> x_FindSeq_entry_Info(const CSeq_entry& entry) const;
    // Bioseq
    CConstRef<CBioseq_Info> x_FindBioseq_Info(const CBioseq& seq) const;
    // Seq-annot
    CConstRef<CSeq_annot_Info> x_FindSeq_annot_Info(const CSeq_annot& annot) const;

    // Find the seq-entry with best bioseq for the seq-id handle.
    // The best bioseq is the bioseq from the live TSE or from the
    // only one TSE containing the ID (no matter live or dead).
    // If no matches were found, return 0.
    TTSE_Lock x_FindBestTSE(const CSeq_id_Handle& handle,
                            const TTSE_LockSet& locks);
    SSeqMatch_DS x_GetSeqMatch(const CSeq_id_Handle& idh,
                               const TTSE_LockSet& locks);

    void x_SetDirtyAnnotIndex(CTSE_Info& tse);
    void x_ResetDirtyAnnotIndex(CTSE_Info& tse);

    void x_IndexTSE(TSeq_id2TSE_Set& tse_map,
                    const CSeq_id_Handle& id, CTSE_Info* tse_info);
    void x_UnindexTSE(TSeq_id2TSE_Set& tse_map,
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
    void x_CollectBioseqs(const CSeq_entry_Info& info,
                          TBioseq_InfoSet& bioseqs,
                          CSeq_inst::EMol filter,
                          TBioseqLevelFlag level);

    // choice should be only eBioseqCore, eExtAnnot, or eOrphanAnnot
    TTSE_LockSet x_GetRecords(const CSeq_id_Handle& idh,
                              CDataLoader::EChoice choice);

    void x_SelectTSEsWithAnnots(const CSeq_id_Handle& idh,
                                TTSE_LockSet& load_locks,
                                TTSE_LockMatchSet& match_set);

    //typedef CMutex TMainLock;
    //typedef CFastMutex TAnnotLock;
    typedef CRWLock TMainLock;
    typedef CRWLock TAnnotLock;
    typedef CRWLock TCacheLock;

    friend class CDSAnnotLockReadGuard;
    friend class CDSAnnotLockWriteGuard;
    typedef CDSAnnotLockReadGuard TAnnotLockReadGuard;
    typedef CDSAnnotLockWriteGuard TAnnotLockWriteGuard;

    // Used to lock: m_*_InfoMap, m_TSE_seq
    // Is locked before locks in CTSE_Info
    mutable TMainLock     m_DSMainLock;
    // Used to lock: m_TSE_annot, m_TSE_annot_is_dirty
    // Is locked after locks in CTSE_Info
    mutable TAnnotLock    m_DSAnnotLock;
    // Used to lock: m_TSE_Cache, CTSE_Info::m_CacheState, m_TSE_Map
    mutable TCacheLock    m_DSCacheLock;

    CRef<CDataLoader>     m_Loader;
    TTSE_Lock             m_ManualBlob;     // manually added TSEs
    CObjectManager*       m_ObjMgr;

    TTSE_InfoMap          m_TSE_InfoMap;    // All known TSEs
    TEntry_InfoMap        m_Entry_InfoMap;  // All known Seq-entries
    TAnnot_InfoMap        m_Annot_InfoMap;  // All known Seq-annots
    TBioseq_InfoMap       m_Bioseq_InfoMap; // All known Bioseqs

    TSeq_id2TSE_Set       m_TSE_seq;        // id -> TSE with bioseq
    TSeq_id2TSE_Set       m_TSE_annot;      // id -> TSE with external annots
    TTSE_Set              m_DirtyAnnot_TSEs;// TSE with uninexed annots

    // Default priority for the datasource
    TPriority             m_DefaultPriority;

    TBlob_Map             m_Blob_Map;       // TBlobId -> CTSE_Info
    mutable TBlob_Cache   m_Blob_Cache;     // unlocked blobs

    // Prefetching thread and lock, used when initializing the thread
    CRef<CPrefetchThread> m_PrefetchThread;
    CFastMutex            m_PrefetchLock;

    // mutex pool
    CInitMutexPool  m_MutexPool;

    // hide copy constructor
    CDataSource(const CDataSource&);
    CDataSource& operator=(const CDataSource&);
};


class NCBI_XOBJMGR_EXPORT CDSAnnotLockReadGuard
{
public:
    CDSAnnotLockReadGuard(CDataSource& ds);

private:
    CDataSource::TMainLock::TReadLockGuard     m_MainGuard;
    CDataSource::TAnnotLock::TReadLockGuard    m_AnnotGuard;
};


class NCBI_XOBJMGR_EXPORT CDSAnnotLockWriteGuard
{
public:
    CDSAnnotLockWriteGuard(CDataSource& ds);

private:
    CDataSource::TMainLock::TReadLockGuard      m_MainGuard;
    CDataSource::TAnnotLock::TWriteLockGuard    m_AnnotGuard;
};



inline
CDataLoader* CDataSource::GetDataLoader(void) const
{
    return const_cast<CDataLoader*>(m_Loader.GetPointerOrNull());
}

inline
CConstRef<CSeq_entry> CDataSource::GetTopEntry(void)
{
    return m_ManualBlob->GetSeq_entryCore();
}

inline
bool CDataSource::IsEmpty(void) const
{
    return m_Loader == 0  &&  m_Blob_Map.empty();
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

#endif  // OBJECTS_OBJMGR_IMPL___DATA_SOURCE__HPP
