#ifndef OBJMGR_IMPL_SCOPE_IMPL__HPP
#define OBJMGR_IMPL_SCOPE_IMPL__HPP

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
#include <corelib/ncbimtx.hpp>

#include <objmgr/impl/heap_scope.hpp>
#include <objmgr/impl/priority.hpp>

#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_annot_handle.hpp>

#include <objects/seq/seq_id_handle.hpp>
#include <objects/seq/seq_id_mapper.hpp>

#include <objmgr/impl/scope_info.hpp>
#include <objmgr/impl/mutex_pool.hpp>

#include <objects/seq/Seq_inst.hpp> // for enum EMol

#include <set>
#include <map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// fwd decl
// objects
class CSeq_entry;
class CSeq_annot;
class CSeq_data;
class CSeq_id;
class CSeq_loc;
class CBioseq;

// objmgr
class CScope;
class CHeapScope;
class CObjectManager;
class CDataSource;
class CSeq_entry_Info;
class CSeq_annot_Info;
class CBioseq_Info;
class CBioseq_set_Info;
class CSeq_id_Handle;
class CSeqMap;
class CSeqMatch_Info;
class CSynonymsSet;
class CBioseq_Handle;
class CSeq_annot_Handle;
class CSeq_entry_Handle;
class CBioseq_EditHandle;
class CSeq_annot_EditHandle;
class CSeq_entry_EditHandle;
class CHandleRangeMap;
struct CDataSource_ScopeInfo;
struct SAnnotTypeSelector;
struct SAnnotSelector;
class CPriorityTree;
class CPriorityNode;


/////////////////////////////////////////////////////////////////////////////
// CScope_Impl
/////////////////////////////////////////////////////////////////////////////

class NCBI_XOBJMGR_EXPORT CScope_Impl : public CObject
{
public:
    typedef set<TTSE_Lock>                           TTSE_LockSet;

    // History of requests
    typedef map<CSeq_id_Handle, SSeq_id_ScopeInfo>   TSeq_idMap;
    typedef TSeq_idMap::value_type                   TSeq_idMapValue;
    typedef CRef<CBioseq_ScopeInfo>                  TBioseqMapValue;
    typedef const CBioseq_Info*                      TBioseqMapKey;
    typedef map<TBioseqMapKey, TBioseqMapValue>      TBioseqMap;
    typedef SSeq_id_ScopeInfo::TTSE_LockMap          TTSE_LockMap;
    typedef SSeq_id_ScopeInfo::TAnnotRefMap          TAnnotRefMap;
    typedef int                                      TPriority;
    typedef map<CConstRef<CObject>, CRef<CObject> >  TEditInfoMap;

    //////////////////////////////////////////////////////////////////
    // Adding top level objects: DataLoader, Seq-entry, Bioseq, Seq-annot
    enum EPriority {
        kPriority_NotSet = -1
    };

    // Add default data loaders from object manager
    void AddDefaults(TPriority priority = kPriority_NotSet);
    // Add data loader by name.
    // The loader (or its factory) must be known to Object Manager.
    void AddDataLoader(const string& loader_name,
                       TPriority priority = kPriority_NotSet);
    // Add the scope's datasources as a single group with the given priority
    void AddScope(CScope_Impl& scope,
                  TPriority priority = kPriority_NotSet);

    // Add seq_entry, default priority is higher than for defaults or loaders
    CSeq_entry_Handle AddTopLevelSeqEntry(CSeq_entry& top_entry,
                                          TPriority pri = kPriority_NotSet);
    // Add bioseq, return bioseq handle. Try to use unresolved seq-id
    // from the bioseq, fail if all ids are already resolved to
    // other sequences.
    CBioseq_Handle AddBioseq(CBioseq& bioseq,
                             TPriority pri = kPriority_NotSet);

    // Add Seq-annot.
    CSeq_annot_Handle AddAnnot(CSeq_annot& annot,
                               TPriority pri = kPriority_NotSet);

    //////////////////////////////////////////////////////////////////
    // Modification of existing object tree
    CBioseq_EditHandle GetEditHandle(const CBioseq_Handle& seq);
    CSeq_entry_EditHandle GetEditHandle(const CSeq_entry_Handle& entry);
    CSeq_annot_EditHandle GetEditHandle(const CSeq_annot_Handle& annot);
    CBioseq_set_EditHandle GetEditHandle(const CBioseq_set_Handle& seqset);

    // Add new sub-entry to the existing tree if it is in this scope
    CSeq_entry_EditHandle AttachEntry(const CBioseq_set_EditHandle& seqset,
                                      CSeq_entry& entry,
                                      int index = -1);
    CSeq_entry_EditHandle CopyEntry(const CBioseq_set_EditHandle& seqset,
                                    const CSeq_entry_Handle& entry,
                                    int index = -1);
    CSeq_entry_EditHandle TakeEntry(const CBioseq_set_EditHandle& seqset,
                                    const CSeq_entry_EditHandle& entry,
                                    int index = -1);

    // Add annotations to a seq-entry (seq or set)
    CSeq_annot_EditHandle AttachAnnot(const CSeq_entry_EditHandle& entry,
                                      const CSeq_annot& annot);
    CSeq_annot_EditHandle CopyAnnot(const CSeq_entry_EditHandle& entry,
                                    const CSeq_annot_Handle& annot);
    CSeq_annot_EditHandle TakeAnnot(const CSeq_entry_EditHandle& entry,
                                    const CSeq_annot_EditHandle& annot);

    // Remove methods
    void RemoveEntry(const CSeq_entry_EditHandle& entry);
    void RemoveBioseq(const CBioseq_EditHandle& seq);
    void RemoveBioseq_set(const CBioseq_set_EditHandle& seqset);
    void RemoveAnnot(const CSeq_annot_EditHandle& annot);

    // Modify Seq-entry
    void SelectNone(const CSeq_entry_EditHandle& entry);
    CBioseq_EditHandle SelectSeq(const CSeq_entry_EditHandle& entry,
                                 CBioseq& seq);
    CBioseq_EditHandle CopySeq(const CSeq_entry_EditHandle& entry,
                               const CBioseq_Handle& seq);
    CBioseq_EditHandle TakeSeq(const CSeq_entry_EditHandle& entry,
                               const CBioseq_EditHandle& seq);

    CBioseq_set_EditHandle SelectSet(const CSeq_entry_EditHandle& entry,
                                     CBioseq_set& seqset);
    CBioseq_set_EditHandle CopySet(const CSeq_entry_EditHandle& entry,
                                   const CBioseq_set_Handle& seqset);
    CBioseq_set_EditHandle TakeSet(const CSeq_entry_EditHandle& entry,
                                   const CBioseq_set_EditHandle& seqset);

    // Get bioseq handle, limit id resolving
    CBioseq_Handle GetBioseqHandle(const CSeq_id& id, int get_flag);
    CBioseq_Handle GetBioseqHandle(const CSeq_id_Handle& id, int get_flag);

    CBioseq_Handle GetBioseqHandleFromTSE(const CSeq_id& id,
                                          const CBioseq_Handle& bh);
    CBioseq_Handle GetBioseqHandleFromTSE(const CSeq_id& id,
                                          const CSeq_entry_Handle& seh);
    CBioseq_Handle GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                          const CBioseq_Handle& bh);
    CBioseq_Handle GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                          const CSeq_entry_Handle& seh);

    // Get bioseq handle by seqloc
    CBioseq_Handle GetBioseqHandle(const CSeq_loc& loc, int get_flag);

    // Deprecated interface
    CBioseq_Handle GetBioseqHandle(const CBioseq& bioseq);
    CSeq_entry_Handle GetSeq_entryHandle(const CSeq_entry& entry);
    CSeq_annot_Handle GetSeq_annotHandle(const CSeq_annot& annot);
    CSeq_entry_Handle GetSeq_entryHandle(const CTSE_Lock& tse_lock);

    void ResetHistory(void);

    virtual void DebugDump(CDebugDumpContext ddc, unsigned int depth) const;

    // Get "native" bioseq ids without filtering and matching.
    typedef CBioseq_Handle::TId TIds;
    TIds GetIds(const CSeq_id& id);
    TIds GetIds(const CSeq_id_Handle& idh);

    // Get bioseq synonyms, resolving to the bioseq in this scope.
    CConstRef<CSynonymsSet> GetSynonyms(const CSeq_id& id,
                                        int get_flag);
    CConstRef<CSynonymsSet> GetSynonyms(const CSeq_id_Handle& id,
                                        int get_flag);
    CConstRef<CSynonymsSet> GetSynonyms(const CBioseq_Handle& bh);

    typedef vector<CSeq_entry_Handle> TTSE_Handles;
    void GetAllTSEs(TTSE_Handles& tses, int kind);

private:
    // constructor/destructor visible from CScope
    CScope_Impl(CObjectManager& objmgr);
    virtual ~CScope_Impl(void);

    // to prevent copying
    CScope_Impl(const CScope_Impl&);
    CScope_Impl& operator=(const CScope_Impl&);

    // Return the highest priority loader or null
    CDataSource* GetFirstLoaderSource(void);

    void UpdateAnnotIndex(const CSeq_annot& limit_annot);
    void UpdateAnnotIndex(const CSeq_annot_Handle& annot);

    CConstRef<TAnnotRefMap>
    GetTSESetWithAnnots(const CSeq_id_Handle& idh_type);

    void x_AttachToOM(CObjectManager& objmgr);
    void x_DetachFromOM(void);
    void x_ResetHistory(void);

    // clean some cache entries when new data source is added
    void x_ClearCacheOnNewData(void);
    void x_ClearAnnotCache(void);
    void x_ClearCacheOnRemoveData(const CSeq_entry_Info& entry);
    void x_ClearCacheOnRemoveData(const CBioseq_set_Info& seqset);
    void x_ClearCacheOnRemoveData(const CBioseq_Info& seq);

    CRef<CTSE_Info> x_GetEditTSE(const CTSE_Info& src_tse);

    CSeq_entry_EditHandle x_AttachEntry(const CBioseq_set_EditHandle& seqset,
                                        CRef<CSeq_entry_Info> entry,
                                        int index);
    CSeq_annot_EditHandle x_AttachAnnot(const CSeq_entry_EditHandle& entry,
                                        CRef<CSeq_annot_Info> annot);

    CBioseq_EditHandle x_SelectSeq(const CSeq_entry_EditHandle& entry,
                                   CRef<CBioseq_Info> bioseq);
    CBioseq_set_EditHandle x_SelectSet(const CSeq_entry_EditHandle& entry,
                                       CRef<CBioseq_set_Info> seqset);

    // Find the best possible resolution for the Seq-id
    void x_ResolveSeq_id(TSeq_idMapValue& id, int get_flag);
    // Iterate over priorities, find all possible data sources
    CDataSource_ScopeInfo* x_FindBioseqInfo(const CPriorityTree& tree,
                                            const CSeq_id_Handle& idh,
                                            const TSeq_id_HandleSet* hset,
                                            CSeqMatch_Info& match_info,
                                            int get_flag);
    CDataSource_ScopeInfo* x_FindBioseqInfo(const CPriorityNode& node,
                                            const CSeq_id_Handle& idh,
                                            const TSeq_id_HandleSet* hset,
                                            CSeqMatch_Info& match_info,
                                            int get_flag);
    CDataSource_ScopeInfo* x_FindBioseqInfo(CDataSource_ScopeInfo& ds_info,
                                            const CSeq_id_Handle& idh,
                                            const TSeq_id_HandleSet* hset,
                                            CSeqMatch_Info& match_info,
                                            int get_flag);

    CBioseq_Handle x_GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                            const TTSE_Lock& tse);

    CBioseq_Handle GetBioseqHandle(const CBioseq_Info& seq,
                                   const TTSE_Lock& tse_lock);
    CBioseq_Handle x_GetBioseqHandle(const CBioseq_Info& seq,
                                     const TTSE_Lock& tse_lock);

public:
    typedef pair<CConstRef<CSeq_entry_Info>, TTSE_Lock> TSeq_entry_Lock;
    typedef pair<CConstRef<CSeq_annot_Info>, TTSE_Lock> TSeq_annot_Lock;
    typedef pair<CConstRef<CBioseq_Info>, TTSE_Lock> TBioseq_Lock;

    TTSE_Lock x_GetTSE_Lock(const CSeq_entry& tse);
    TSeq_entry_Lock x_GetSeq_entry_Lock(const CSeq_entry& entry);
    TSeq_annot_Lock x_GetSeq_annot_Lock(const CSeq_annot& annot);
    TBioseq_Lock x_GetBioseq_Lock(const CBioseq& bioseq);

private:
    // Get bioseq handles for sequences from the given TSE using the filter
    typedef vector<CBioseq_Handle> TBioseq_HandleSet;
    typedef int TBioseqLevelFlag;
    void x_PopulateBioseq_HandleSet(const CSeq_entry_Handle& tse,
                                    TBioseq_HandleSet& handles,
                                    CSeq_inst::EMol filter,
                                    TBioseqLevelFlag level);

    CConstRef<CSynonymsSet> x_GetSynonyms(CBioseq_ScopeInfo& info);
    void x_AddSynonym(const CSeq_id_Handle& idh,
                      CSynonymsSet& syn_set, CBioseq_ScopeInfo& info);

    // Check if any bioseq synonym has been resolved
    bool x_HaveResolvedSynonym(CSeqMatch_Info& info);
    // Conflict reporting function
    enum EConflict {
        eConflict_History,
        eConflict_Live
    };
    void x_ThrowConflict(EConflict conflict_type,
                         const CSeqMatch_Info& info1,
                         const CSeqMatch_Info& info2) const;

    TSeq_idMapValue& x_GetSeq_id_Info(const CSeq_id_Handle& id);
    TSeq_idMapValue& x_GetSeq_id_Info(const CBioseq_Handle& bh);
    TSeq_idMapValue* x_FindSeq_id_Info(const CSeq_id_Handle& id);

    CRef<CBioseq_ScopeInfo> x_InitBioseq_Info(TSeq_idMapValue& info,
                                              int get_flag);
    bool x_InitBioseq_Info(TSeq_idMapValue& info,
                           CBioseq_ScopeInfo& bioseq_info);
    CRef<CBioseq_ScopeInfo> x_GetBioseq_Info(const CSeq_id_Handle& id,
                                             int get_flag);
    CRef<CBioseq_ScopeInfo> x_FindBioseq_Info(const CSeq_id_Handle& id,
                                              int get_flag);

    void x_AddTSESetWithAnnots(TTSE_LockMap& tse_map,
                               const TTSE_LockMap& add,
                               CDataSource_ScopeInfo& ds_info);
    CDataSource_ScopeInfo*
    x_GetTSESetWithOrphanAnnots(TTSE_LockMap& tse_map,
                                const TSeq_id_HandleSet& ids,
                                CDataSource* bioseq_ds);
    CDataSource_ScopeInfo*
    x_GetTSESetWithBioseqAnnots(TTSE_LockMap& tse_map,
                                CBioseq_ScopeInfo& binfo);

private:
    CScope*         m_HeapScope;

    CObjectManager* m_pObjMgr;
    CPriorityTree   m_setDataSrc; // Data sources ordered by priority

    CInitMutexPool  m_MutexPool;

    typedef CRWLock                  TRWLock;
    typedef TRWLock::TReadLockGuard  TReadLockGuard;
    typedef TRWLock::TWriteLockGuard TWriteLockGuard;

    mutable TRWLock m_Scope_Conf_RWLock;

    TSeq_idMap      m_Seq_idMap;
    mutable TRWLock m_Seq_idMapLock;
    TBioseqMap      m_BioseqMap;
    mutable TRWLock m_BioseqMapLock;
    TEditInfoMap    m_EditInfoMap;

    friend class CScope;
    friend class CHeapScope;
    friend class CObjectManager;
    friend class CSeqVector;
    friend class CDataSource;
    friend class CBioseq_CI;
    friend class CAnnot_Collector;
    friend class CBioseq_Handle;
    friend class CBioseq_set_Handle;
    friend class CSeq_entry_Handle;
    friend class CBioseq_EditHandle;
    friend class CBioseq_set_EditHandle;
    friend class CSeq_entry_EditHandle;
    friend class CTSE_CI;
    friend class CSeq_annot_CI;
    friend class CSeqMap_CI;
    friend class CPrefetchToken_Impl;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//OBJMGR_IMPL_SCOPE_IMPL__HPP
