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
class CBioseq_set_Info;
class CBioseq_Info;

// others
class CBioseq_Handle;
class CPrefetchTokenOld_Impl;
class CPrefetchThreadOld;
class CDSAnnotLockReadGuard;
class CDSAnnotLockWriteGuard;
class CScope_Impl;

/*
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
*/

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
    CDataSource(CDataLoader& loader);
    CDataSource(const CObject& shared_object, const CSeq_entry& entry);
    virtual ~CDataSource(void);

    // typedefs
    typedef int                                     TPriority;
    typedef CTSE_Lock                               TTSE_Lock;
    typedef CTSE_LockSet                            TTSE_LockSet;
    typedef set<CSeq_id_Handle>                     TSeq_idSet;
    typedef vector<pair<TTSE_Lock,CSeq_id_Handle> > TTSE_LockMatchSet;
    typedef CRef<CTSE_Info>                         TTSE_Ref;
    typedef CBlobIdKey                              TBlobId;

    typedef CDSAnnotLockReadGuard                   TAnnotLockReadGuard;
    typedef CDSAnnotLockWriteGuard                  TAnnotLockWriteGuard;
    typedef CMutex TMainLock;
    typedef CMutex TAnnotLock;
    typedef CMutex TCacheLock;

    /// Register new TSE (Top Level Seq-entry)
    TTSE_Lock AddTSE(CSeq_entry& se,
                     CTSE_Info::TBlobState = CBioseq_Handle::fState_none);
    TTSE_Lock AddTSE(CSeq_entry& se,
                     bool dead);
    TTSE_Lock AddTSE(CRef<CTSE_Info> tse);
    TTSE_Lock AddStaticTSE(CSeq_entry& se);
    TTSE_Lock AddStaticTSE(CRef<CTSE_Info> tse);

    // Modification methods.
    /// Add new sub-entry to "parent".
    CRef<CSeq_entry_Info> AttachEntry(CBioseq_set_Info& parent,
                                      CSeq_entry& entry,
                                      int index = -1);
    void RemoveEntry(CSeq_entry_Info& entry);

    /// Add annotations to a Seq-entry.
    CRef<CSeq_annot_Info> AttachAnnot(CSeq_entry_Info& parent,
                                      CSeq_annot& annot);
    CRef<CSeq_annot_Info> AttachAnnot(CBioseq_Base_Info& parent,
                                      CSeq_annot& annot);
    // Remove/replace seq-annot from the given entry
    void RemoveAnnot(CSeq_annot_Info& annot);
    CRef<CSeq_annot_Info> ReplaceAnnot(CSeq_annot_Info& old_annot,
                                       CSeq_annot& new_annot);

    /// Get TSE info by seq-id handle. This should also get the list of all
    /// seq-ids for all bioseqs and the list of seq-ids used in annotations.
    //TTSE_Lock GetBlobById(const CSeq_id_Handle& idh);

    // Remove TSE from the datasource, update indexes
    void DropAllTSEs(void);
    bool DropStaticTSE(CTSE_Info& info);
    bool DropTSE(CTSE_Info& info);

    // Contains (or can load) any entries?
    bool IsEmpty(void) const;
    const CTSE_LockSet& GetStaticBlobs(void) const;

    CDataLoader* GetDataLoader(void) const;
    const CConstRef<CObject>& GetSharedObject(void) const;
    TTSE_Lock GetSharedTSE(void) const;
    bool CanBeEdited(void) const;

    void UpdateAnnotIndex(void);
    void UpdateAnnotIndex(const CSeq_entry_Info& entry_info);
    void UpdateAnnotIndex(const CSeq_annot_Info& annot_info);

    void GetTSESetWithOrphanAnnots(const TSeq_idSet& ids,
                                   TTSE_LockMatchSet& tse_set,
                                   const SAnnotSelector* sel);
    void GetTSESetWithBioseqAnnots(const CBioseq_Info& bioseq,
                                   const TTSE_Lock& tse,
                                   TTSE_LockMatchSet& tse_set,
                                   const SAnnotSelector* sel);

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

    SSeqMatch_DS BestResolve(const CSeq_id_Handle& idh);
    typedef vector<SSeqMatch_DS> TSeqMatches;
    TSeqMatches GetMatches(const CSeq_id_Handle& idh,
                           const TTSE_LockSet& locks);

    typedef vector<CSeq_id_Handle> TIds;
    void GetIds(const CSeq_id_Handle& idh, TIds& ids);
    CSeq_id_Handle GetAccVer(const CSeq_id_Handle& idh);
    TGi GetGi(const CSeq_id_Handle& idh);
    string GetLabel(const CSeq_id_Handle& idh);
    int GetTaxId(const CSeq_id_Handle& idh);
    TSeqPos GetSequenceLength(const CSeq_id_Handle& idh);
    CSeq_inst::TMol GetSequenceType(const CSeq_id_Handle& idh);
    int GetSequenceState(const CSeq_id_Handle& idh);

    // bulk interface
    typedef vector<bool> TLoaded;
    typedef vector<TGi> TGis;
    typedef vector<string> TLabels;
    typedef vector<int> TTaxIds;
    typedef vector<TSeqPos> TSequenceLengths;
    typedef vector<CSeq_inst::TMol> TSequenceTypes;
    typedef vector<int> TSequenceStates;
    void GetAccVers(const TIds& ids, TLoaded& loaded, TIds& ret);
    void GetGis(const TIds& ids, TLoaded& loaded, TGis& ret);
    void GetLabels(const TIds& ids, TLoaded& loaded, TLabels& ret);
    void GetTaxIds(const TIds& ids, TLoaded& loaded, TTaxIds& ret);
    void GetSequenceLengths(const TIds& ids, TLoaded& loaded,
                            TSequenceLengths& ret);
    void GetSequenceTypes(const TIds& ids, TLoaded& loaded,
                          TSequenceTypes& ret);
    void GetSequenceStates(const TIds& ids, TLoaded& loaded,
                           TSequenceStates& ret);

    typedef map<CSeq_id_Handle, SSeqMatch_DS>       TSeqMatchMap;
    void GetBlobs(TSeqMatchMap& match_map);

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
    CTSE_LoadLock GetTSE_LoadLockIfLoaded(const TBlobId& blob_id);
    bool IsLoaded(const CTSE_Info& tse) const;
    void SetLoaded(CTSE_LoadLock& lock);

    typedef pair<CConstRef<CSeq_entry_Info>, TTSE_Lock> TSeq_entry_Lock;
    typedef pair<CConstRef<CSeq_annot_Info>, TTSE_Lock> TSeq_annot_Lock;
    typedef pair<TSeq_annot_Lock, int> TSeq_feat_Lock;
    typedef pair<CConstRef<CBioseq_set_Info>, TTSE_Lock> TBioseq_set_Lock;
    typedef pair<CConstRef<CBioseq_Info>, TTSE_Lock> TBioseq_Lock;

    TSeq_entry_Lock GetSeq_entry_Lock(const CBlobIdKey& blob_id);

    TTSE_Lock FindTSE_Lock(const CSeq_entry& entry,
                           const TTSE_LockSet& history) const;
    TSeq_entry_Lock FindSeq_entry_Lock(const CSeq_entry& entry,
                                       const TTSE_LockSet& history) const;
    TSeq_annot_Lock FindSeq_annot_Lock(const CSeq_annot& annot,
                                       const TTSE_LockSet& history) const;
    TBioseq_set_Lock FindBioseq_set_Lock(const CBioseq_set& seqset,
                                       const TTSE_LockSet& history) const;
    TBioseq_Lock FindBioseq_Lock(const CBioseq& bioseq,
                                 const TTSE_LockSet& history) const;
    TSeq_feat_Lock FindSeq_feat_Lock(const CSeq_id_Handle& loc_id,
                                     TSeqPos loc_pos,
                                     const CSeq_feat& feat) const;

    typedef vector<TBlobId> TLoadedBlob_ids;
    enum {
        fLoaded_bioseqs       = 1<<0,
        fLoaded_bioseq_annots = 1<<1,
        fLoaded_orphan_annots = 1<<2,
        fLoaded_annots        = fLoaded_bioseq_annots | fLoaded_orphan_annots,
        fLoaded_all           = fLoaded_bioseqs | fLoaded_annots
    };
    typedef int TLoadedTypes;
    void GetLoadedBlob_ids(const CSeq_id_Handle& idh,
                           TLoadedTypes types,
                           TLoadedBlob_ids& blob_ids) const;

    virtual void Prefetch(CPrefetchTokenOld_Impl& token);

    TMainLock& GetMainLock() const { return m_DSMainLock; }

private:
    // internal typedefs

    // blob lookup map
    typedef map<TBlobId, TTSE_Ref>                  TBlob_Map;
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
    typedef map<const CObject*, const CTSE_Info_Object*> TInfoMap;

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
    friend class CBioseq_set_Info;
    friend class CBioseq_Info;
    friend class CPrefetchTokenOld_Impl;
    friend class CScope_Impl;
    friend class CDSAnnotLockReadGuard;
    friend class CDSAnnotLockWriteGuard;

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

    void x_Map(const CObject* obj, const CTSE_Info_Object* info);
    void x_Unmap(const CObject* obj, const CTSE_Info_Object* info);

    // lookup Xxx_Info objects
    // TSE
    CConstRef<CTSE_Info>
    x_FindTSE_Info(const CSeq_entry& tse) const;
    // Seq-entry
    CConstRef<CSeq_entry_Info>
    x_FindSeq_entry_Info(const CSeq_entry& entry) const;
    // Bioseq
    CConstRef<CBioseq_Info>
    x_FindBioseq_Info(const CBioseq& seq) const;
    // Seq-annot
    CConstRef<CSeq_annot_Info>
    x_FindSeq_annot_Info(const CSeq_annot& annot) const;
    CConstRef<CBioseq_set_Info>
    x_FindBioseq_set_Info(const CBioseq_set& seqset) const;

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
    void x_IndexSeqTSE(const vector<CSeq_id_Handle>& idh, CTSE_Info* tse_info);
    void x_UnindexSeqTSE(const CSeq_id_Handle& ids, CTSE_Info* tse_info);
    void x_IndexAnnotTSE(const CSeq_id_Handle& idh,
                         CTSE_Info* tse_info,
                         bool orphan);
    void x_UnindexAnnotTSE(const CSeq_id_Handle& idh,
                           CTSE_Info* tse_info,
                           bool orphan);
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

    void x_AddTSEAnnots(TTSE_LockMatchSet& ret,
                        const CSeq_id_Handle& id,
                        const CTSE_Lock& tse_lock);
    void x_AddTSEBioseqAnnots(TTSE_LockMatchSet& ret,
                              const CBioseq_Info& bioseq,
                              const CTSE_Lock& tse_lock);
    void x_AddTSEOrphanAnnots(TTSE_LockMatchSet& ret,
                              const TSeq_idSet& ids,
                              const CTSE_Lock& tse_lock);

    // Used to lock: m_*_InfoMap, m_TSE_seq
    // Is locked before locks in CTSE_Info
    mutable TMainLock     m_DSMainLock;
    // Used to lock: m_TSE_annot, m_TSE_annot_is_dirty
    // Is locked after locks in CTSE_Info
    mutable TAnnotLock    m_DSAnnotLock;
    // Used to lock: m_TSE_Cache, CTSE_Info::m_CacheState, m_TSE_Map
    mutable TCacheLock    m_DSCacheLock;

    CRef<CDataLoader>     m_Loader;
    CConstRef<CObject>    m_SharedObject;
    TTSE_LockSet          m_StaticBlobs;        // manually added TSEs

    TInfoMap              m_InfoMap;            // All known TSE objects

    TSeq_id2TSE_Set       m_TSE_seq;            // id -> TSE with bioseq
    TSeq_id2TSE_Set       m_TSE_seq_annot;      // id -> TSE with bioseq annots
    TSeq_id2TSE_Set       m_TSE_orphan_annot;   // id -> TSE with orphan annots
    TTSE_Set              m_DirtyAnnot_TSEs;    // TSE with uninexed annots

    // Default priority for the datasource
    TPriority             m_DefaultPriority;

    TBlob_Map             m_Blob_Map;       // TBlobId -> CTSE_Info
    mutable TBlob_Cache   m_Blob_Cache;     // unlocked blobs
    mutable size_t        m_Blob_Cache_Size;// list<>::size() is slow

    // Prefetching thread and lock, used when initializing the thread
    CRef<CPrefetchThreadOld> m_PrefetchThread;
    CFastMutex            m_PrefetchLock;

    // hide copy constructor
    CDataSource(const CDataSource&);
    CDataSource& operator=(const CDataSource&);
};


class NCBI_XOBJMGR_EXPORT CDSAnnotLockReadGuard
{
public:
    explicit CDSAnnotLockReadGuard(EEmptyGuard);
    explicit CDSAnnotLockReadGuard(CDataSource& ds);

    void Guard(CDataSource& ds);

private:
    CDataSource::TMainLock::TReadLockGuard     m_MainGuard;
    CDataSource::TAnnotLock::TReadLockGuard    m_AnnotGuard;
};


class NCBI_XOBJMGR_EXPORT CDSAnnotLockWriteGuard
{
public:
    explicit CDSAnnotLockWriteGuard(EEmptyGuard);
    explicit CDSAnnotLockWriteGuard(CDataSource& ds);

    void Guard(CDataSource& ds);

private:
    CDataSource::TMainLock::TReadLockGuard      m_MainGuard;
    CDataSource::TAnnotLock::TWriteLockGuard    m_AnnotGuard;
};



inline
CDataLoader* CDataSource::GetDataLoader(void) const
{
    return m_Loader.GetNCPointerOrNull();
}


inline
const CConstRef<CObject>& CDataSource::GetSharedObject(void) const
{
    return m_SharedObject;
}


inline
bool CDataSource::CanBeEdited(void) const
{
    return !m_Loader && !m_SharedObject;
}


inline
bool CDataSource::IsEmpty(void) const
{
    return m_Loader == 0  &&  m_Blob_Map.empty();
}


inline
const CTSE_LockSet& CDataSource::GetStaticBlobs(void) const
{
    return m_StaticBlobs;
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
