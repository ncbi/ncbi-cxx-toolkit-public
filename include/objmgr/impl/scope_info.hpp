#ifndef SCOPE_INFO__HPP
#define SCOPE_INFO__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*     Structures used by CScope
*
*/

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>

#include <objects/seq/seq_id_handle.hpp>
#include <util/mutex_pool.hpp>
#include <objmgr/impl/tse_lock.hpp>
#include <objmgr/impl/tse_scope_lock.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/tse_handle.hpp>
#include <objmgr/blob_id.hpp>

#include <set>
#include <map>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CObjectManager;
class CDataSource;
class CDataLoader;
class CSeqMap;

class CScope;
class CScope_Impl;
class CSynonymsSet;

class CTSE_Info_Object;
class CSeq_entry_Info;
class CSeq_annot_Info;
class CBioseq_Info;
class CBioseq_set_Info;

class CDataSource_ScopeInfo;
class CTSE_ScopeInfo;
class CBioseq_ScopeInfo;

class CTSE_Handle;

class CScopeInfo_Base;
class CSeq_entry_ScopeInfo;
class CSeq_annot_ScopeInfo;
class CBioseq_set_ScopeInfo;
class CBioseq_ScopeInfo;

struct SSeqMatch_Scope;
struct SSeqMatch_DS;
struct SSeq_id_ScopeInfo;

class CSeq_entry;
class CSeq_annot;
class CBioseq;
class CBioseq_set;
class CSeq_feat;

template<typename Key, typename Value>
class CDeleteQueue
{
public:
    typedef Key key_type;
    typedef Value value_type;
    typedef vector<value_type> TUnlockSet;

    explicit CDeleteQueue(size_t max_size = 0)
        : m_MaxSize(max_size)
        {
        }

    bool Contains(const key_type& key) const
        {
            _ASSERT(m_Queue.size() == m_Index.size());
            return m_Index.find(key) != m_Index.end();
        }

    value_type Get(const key_type& key)
        {
            value_type ret;

            _ASSERT(m_Queue.size() == m_Index.size());
            TIndexIter iter = m_Index.find(key);
            if ( iter != m_Index.end() ) {
                ret = iter->second->second;
                m_Queue.erase(iter->second);
                m_Index.erase(iter);
                _ASSERT(m_Queue.size() == m_Index.size());
            }

            return ret;
        }
    void Erase(const key_type& key)
        {
            _ASSERT(m_Queue.size() == m_Index.size());
            TIndexIter iter = m_Index.find(key);
            if ( iter != m_Index.end() ) {
                m_Queue.erase(iter->second);
                m_Index.erase(iter);
                _ASSERT(m_Queue.size() == m_Index.size());
            }
        }
    void Put(const key_type& key, const value_type& value,
             value_type* unlock_ptr)
        {
            _ASSERT(m_Queue.size() == m_Index.size());
            _ASSERT(m_Index.find(key) == m_Index.end());

            TQueueIter queue_iter =
                m_Queue.insert(m_Queue.end(), TQueueValue(key, value));
                               
            _VERIFY(m_Index.insert(TIndexValue(key, queue_iter)).second);

            _ASSERT(m_Queue.size() == m_Index.size());

            if ( m_Index.size() > m_MaxSize ) {
                _VERIFY(m_Index.erase(m_Queue.front().first) == 1);
                if ( unlock_ptr ) {
                    *unlock_ptr = m_Queue.front().second;
                }
                m_Queue.pop_front();
                _ASSERT(m_Queue.size() == m_Index.size());
            }
        }
    void Put(const key_type& key, const value_type& value,
             TUnlockSet* unlock_set = 0)
        {
            _ASSERT(m_Queue.size() == m_Index.size());
            _ASSERT(m_Index.find(key) == m_Index.end());

            TQueueIter queue_iter =
                m_Queue.insert(m_Queue.end(), TQueueValue(key, value));
                               
            _VERIFY(m_Index.insert(TIndexValue(key, queue_iter)).second);

            _ASSERT(m_Queue.size() == m_Index.size());

            while ( m_Index.size() > m_MaxSize ) {
                _VERIFY(m_Index.erase(m_Queue.front().first) == 1);
                if ( unlock_set ) {
                    unlock_set->push_back(m_Queue.front().second);
                }
                m_Queue.pop_front();
                _ASSERT(m_Queue.size() == m_Index.size());
            }
        }

    void Clear(TUnlockSet* unlock_set = 0)
        {
            if ( unlock_set ) {
                ITERATE ( typename TQueue, it, m_Queue ) {
                    unlock_set->push_back(it->second);
                }
            }
            m_Queue.clear();
            m_Index.clear();
        }

private:
    typedef pair<key_type, value_type> TQueueValue;
    typedef list<TQueueValue> TQueue;
    typedef typename TQueue::iterator TQueueIter;

    typedef map<key_type, TQueueIter> TIndex;
    typedef typename TIndex::value_type TIndexValue;
    typedef typename TIndex::iterator TIndexIter;

    size_t m_MaxSize;
    TQueue m_Queue;
    TIndex m_Index;
};


class NCBI_XOBJMGR_EXPORT CDataSource_ScopeInfo : public CObject
{
public:
    typedef CRef<CDataSource>                           TDataSourceLock;
    typedef CRef<CTSE_ScopeInfo>                        TTSE_ScopeInfo;
    typedef CBlobIdKey                                  TBlobId;
    typedef map<TBlobId, TTSE_ScopeInfo>                TTSE_InfoMap;
    typedef CTSE_LockSet                                TTSE_LockSet;
    typedef multimap<CSeq_id_Handle, TTSE_ScopeInfo>    TTSE_BySeqId;
    typedef CDeleteQueue<const CTSE_ScopeInfo*,
                         CTSE_ScopeInternalLock>        TTSE_UnlockQueue;

    CDataSource_ScopeInfo(CScope_Impl& scope, CDataSource& ds);
    ~CDataSource_ScopeInfo(void);

    CScope_Impl& GetScopeImpl(void) const;
    void DetachScope(void);

    void ResetDS(void);
    void ResetHistory(int action_if_locked);//CScope_Impl::EActionIfLocked

    typedef CMutex TTSE_InfoMapMutex;
    const TTSE_InfoMap& GetTSE_InfoMap(void) const;
    TTSE_InfoMapMutex& GetTSE_InfoMapMutex(void) const;

    typedef CMutex TTSE_LockSetMutex;
    const TTSE_LockSet& GetTSE_LockSet(void) const;
    TTSE_LockSetMutex& GetTSE_LockSetMutex(void) const;
    void UpdateTSELock(CTSE_ScopeInfo& tse, CTSE_Lock lock);
    void ReleaseTSELock(CTSE_ScopeInfo& tse); // into queue
    void ForgetTSELock(CTSE_ScopeInfo& tse); // completely
    void RemoveFromHistory(CTSE_ScopeInfo& tse, bool drop_from_ds = false);
    bool TSEIsInQueue(const CTSE_ScopeInfo& tse) const;

    void RemoveTSE_Lock(const CTSE_Lock& lock);
    void AddTSE_Lock(const CTSE_Lock& lock);
    
    void UnlockedTSEsSave(void);
    void UnlockedTSEsRelease(void);

    CDataSource& GetDataSource(void);
    const CDataSource& GetDataSource(void) const;
    CDataLoader* GetDataLoader(void);
    // CanBeEdited() is true for a data source with blobs in 'edited' state
    bool CanBeEdited(void) const;
    // IsConst() is true for a data source with manually added const TSEs
    bool IsConst(void) const;
    void SetConst(void);
    // Make unlocked TSEs will be removed completely in ResetHistory()
    // this mode is for edited entries retrieved from data loader
    void SetCanRemoveOnResetHistory(void);
    bool CanRemoveOnResetHistory(void) const;

    typedef CTSE_ScopeUserLock                          TTSE_Lock;
    typedef pair<CConstRef<CSeq_entry_Info>, TTSE_Lock> TSeq_entry_Lock;
    typedef pair<CConstRef<CSeq_annot_Info>, TTSE_Lock> TSeq_annot_Lock;
    typedef pair<CConstRef<CBioseq_set_Info>, TTSE_Lock> TBioseq_set_Lock;
    typedef CScopeInfo_Ref<CBioseq_ScopeInfo>           TBioseq_Lock;
    typedef pair<TSeq_annot_Lock, int>                  TSeq_feat_Lock;

    TTSE_Lock GetTSE_Lock(const CTSE_Lock& tse);
    TTSE_Lock FindTSE_Lock(const CSeq_entry& tse);
    TSeq_entry_Lock GetSeq_entry_Lock(const CBlobIdKey& blob_id);
    TSeq_entry_Lock FindSeq_entry_Lock(const CSeq_entry& entry);
    TSeq_annot_Lock FindSeq_annot_Lock(const CSeq_annot& annot);
    TBioseq_set_Lock FindBioseq_set_Lock(const CBioseq_set& seqset);
    TBioseq_Lock FindBioseq_Lock(const CBioseq& bioseq);
    TSeq_feat_Lock FindSeq_feat_Lock(const CSeq_id_Handle& loc_id,
                                     TSeqPos loc_pos,
                                     const CSeq_feat& feat);

    SSeqMatch_Scope BestResolve(const CSeq_id_Handle& idh, int get_flag);
    SSeqMatch_Scope Resolve(const CSeq_id_Handle& idh, CTSE_ScopeInfo& tse);

    void AttachTSE(CTSE_ScopeInfo& tse, const CTSE_Lock& lock);

    typedef map<CSeq_id_Handle, SSeqMatch_Scope> TSeqMatchMap;
    void GetBlobs(TSeqMatchMap& match_map);

protected:
    friend class CScope_Impl;

    SSeqMatch_Scope x_GetSeqMatch(const CSeq_id_Handle& idh);
    SSeqMatch_Scope x_FindBestTSE(const CSeq_id_Handle& idh);
    void x_SetMatch(SSeqMatch_Scope& match,
                    CTSE_ScopeInfo& tse,
                    const CSeq_id_Handle& idh) const;
    void x_SetMatch(SSeqMatch_Scope& match,
                    const SSeqMatch_DS& ds_match);

    void x_IndexTSE(CTSE_ScopeInfo& tse);
    void x_UnindexTSE(const CTSE_ScopeInfo& tse);
    TTSE_ScopeInfo x_FindBestTSEInIndex(const CSeq_id_Handle& idh) const;
    static bool x_IsBetter(const CSeq_id_Handle& idh,
                           const CTSE_ScopeInfo& tse1,
                           const CTSE_ScopeInfo& tse2);

private: // members
    CScope_Impl*                m_Scope;
    TDataSourceLock             m_DataSource;
    bool                        m_CanBeUnloaded;
    bool                        m_CanBeEdited;
    bool                        m_CanRemoveOnResetHistory;
    int                         m_NextTSEIndex;
    TTSE_InfoMap                m_TSE_InfoMap;
    mutable TTSE_InfoMapMutex   m_TSE_InfoMapMutex;
    TTSE_BySeqId                m_TSE_BySeqId;
    TTSE_LockSet                m_TSE_LockSet;
    mutable TTSE_LockSetMutex   m_TSE_LockSetMutex;
    TTSE_UnlockQueue            m_TSE_UnlockQueue;
    mutable TTSE_LockSetMutex   m_TSE_UnlockQueueMutex;
    CRef<CDataSource_ScopeInfo> m_EditDS;

private: // to prevent copying
    CDataSource_ScopeInfo(const CDataSource_ScopeInfo&);
    const CDataSource_ScopeInfo& operator=(const CDataSource_ScopeInfo&);
};


class CUnlockedTSEsGuard {
public:
    typedef vector< CConstRef<CTSE_Info> > TUnlockedTSEsLock;
    typedef vector<CTSE_ScopeInternalLock> TUnlockedTSEsInternal;

    CUnlockedTSEsGuard(void);
    ~CUnlockedTSEsGuard(void);

    static void SaveLock(const CTSE_Lock& lock);
    static void SaveInternal(const CTSE_ScopeInternalLock& lock);
    static void SaveInternal(const TUnlockedTSEsInternal& locks);
private:
    TUnlockedTSEsLock m_UnlockedTSEsLock;
    TUnlockedTSEsInternal m_UnlockedTSEsInternal;
};

    
class NCBI_XOBJMGR_EXPORT CTSE_ScopeInfo : public CTSE_ScopeInfo_Base
{
public:
    typedef CBlobIdKey                                    TBlobId;
    typedef multimap<CSeq_id_Handle, CRef<CBioseq_ScopeInfo> >  TBioseqById;
    typedef vector<CSeq_id_Handle>                        TSeqIds;
    typedef pair<int, int>                                TBlobOrder;
    typedef CConstRef<CTSE_ScopeInfo>                     TUsedTSE_LockKey;
    typedef map<TUsedTSE_LockKey, CTSE_ScopeInternalLock> TUsedTSE_LockSet;

    CTSE_ScopeInfo(CDataSource_ScopeInfo& ds_info,
                   const CTSE_Lock& tse_lock,
                   int load_index,
                   bool can_be_unloaded);
    ~CTSE_ScopeInfo(void);

    CScope_Impl& GetScopeImpl(void) const;
    CDataSource_ScopeInfo& GetDSInfo(void) const;

    bool IsAttached(void) const;
    void RemoveFromHistory(int action_if_locked, bool drop_from_ds = false);//CScope_Impl::EActionIfLocked

    bool HasResolvedBioseq(const CSeq_id_Handle& id) const;

    bool CanBeUnloaded(void) const;
    bool CanBeEdited(void) const;

    // True if the TSE is referenced
    bool IsLocked(void) const;
    // True if the TSE is referenced by more than one handle
    bool LockedMoreThanOnce(void) const;

    bool ContainsBioseq(const CSeq_id_Handle& id) const;
    // returns matching Seq-id handle
    CSeq_id_Handle ContainsMatchingBioseq(const CSeq_id_Handle& id) const;

    bool x_SameTSE(const CTSE_Info& tse) const;

    int GetLoadIndex(void) const;
    TBlobId GetBlobId(void) const;
    TBlobOrder GetBlobOrder(void) const;
    const TSeqIds& GetBioseqsIds(void) const;

    bool AddUsedTSE(const CTSE_ScopeUserLock& lock) const;
    void ReleaseUsedTSEs(void);
    
    typedef map<CConstRef<CObject>, CRef<CObject> >  TEditInfoMap;
    void SetEditTSE(const CTSE_Lock& new_tse_lock,
                    CDataSource_ScopeInfo& new_ds,
                    const TEditInfoMap& edit_map);

    // gets locked CScopeInfo_Base object
    typedef CScopeInfo_Ref<CSeq_entry_ScopeInfo> TSeq_entry_Lock;
    typedef CScopeInfo_Ref<CSeq_annot_ScopeInfo> TSeq_annot_Lock;
    typedef CScopeInfo_Ref<CBioseq_set_ScopeInfo> TBioseq_set_Lock;
    typedef CScopeInfo_Ref<CBioseq_ScopeInfo> TBioseq_Lock;
    TSeq_entry_Lock GetScopeLock(const CTSE_Handle& tse,
                                 const CSeq_entry_Info& info);
    TSeq_annot_Lock GetScopeLock(const CTSE_Handle& tse,
                                 const CSeq_annot_Info& info);
    TBioseq_set_Lock GetScopeLock(const CTSE_Handle& tse,
                                  const CBioseq_set_Info& info);

    CRef<CBioseq_ScopeInfo> GetBioseqInfo(const SSeqMatch_Scope& match);
    TBioseq_Lock GetBioseqLock(CRef<CBioseq_ScopeInfo> info,
                               CConstRef<CBioseq_Info> bioseq);

    void ResetEntry(CSeq_entry_ScopeInfo& info);
    void RemoveEntry(CSeq_entry_ScopeInfo& info);
    void RemoveAnnot(CSeq_annot_ScopeInfo& info);
    void AddEntry(CBioseq_set_ScopeInfo& seqset,
                  CSeq_entry_ScopeInfo& info,
                  int index);
    void AddAnnot(CSeq_entry_ScopeInfo& entry,
                  CSeq_annot_ScopeInfo& info);
    void SelectSeq(CSeq_entry_ScopeInfo& entry,
                   CBioseq_ScopeInfo& info);
    void SelectSet(CSeq_entry_ScopeInfo& entry,
                   CBioseq_set_ScopeInfo& info);

    void x_SaveRemoved(CScopeInfo_Base& info);
    void x_CheckAdded(CScopeInfo_Base& parent, CScopeInfo_Base& child);
    void x_RestoreAdded(CScopeInfo_Base& parent, CScopeInfo_Base& child);

    void RemoveLastInfoLock(CScopeInfo_Base& info);

    friend class CTSE_ScopeInfo_Base;

    void ForgetTSE_Lock(void);

    const CTSE_Lock& GetTSE_Lock(void) const;
    void SetTSE_Lock(const CTSE_Lock& lock);
    void ResetTSE_Lock(void);
    void DropTSE_Lock(void);

    SSeqMatch_Scope Resolve(const CSeq_id_Handle& id);

protected:
    void x_SetTSE_Lock(const CTSE_Lock& lock);
    void x_ResetTSE_Lock(void);
    void x_DetachDS(void);

    // Number of internal locks, not related to handles
    int x_GetDSLocksCount(void) const;

    CRef<CBioseq_ScopeInfo> x_FindBioseqInfo(const TSeqIds& ids) const;
    CRef<CBioseq_ScopeInfo> x_CreateBioseqInfo(const TSeqIds& ids);
    void x_IndexBioseq(const CSeq_id_Handle& id,
                       CBioseq_ScopeInfo* info);
    void x_UnindexBioseq(const CSeq_id_Handle& id,
                         const CBioseq_ScopeInfo* info);

private: // members
    friend class CScope_Impl;
    friend class CTSE_Handle;
    friend class CDataSource_ScopeInfo;
    friend class CBioseq_ScopeInfo;

    typedef CConstRef<CTSE_Info_Object>                 TScopeInfoMapKey;
    typedef CRef<CScopeInfo_Base>                       TScopeInfoMapValue;
    typedef map<TScopeInfoMapKey, TScopeInfoMapValue>   TScopeInfoMap;

    CDataSource_ScopeInfo*      m_DS_Info;
    int                         m_LoadIndex;
    struct SUnloadedInfo {
        SUnloadedInfo(const CTSE_Lock& lock);
        CTSE_Lock LockTSE(void);

        CRef<CDataLoader>       m_Loader;
        TBlobId                 m_BlobId;
        TBlobOrder              m_BlobOrder;
        TSeqIds                 m_BioseqsIds;
    };

    AutoPtr<SUnloadedInfo>      m_UnloadedInfo;
    TBioseqById                 m_BioseqById;
    // TSE locking support
    mutable CMutex              m_TSE_LockMutex;
    mutable CTSE_Lock           m_TSE_Lock;
    // Used by TSE support
    mutable const CTSE_ScopeInfo*   m_UsedByTSE;
    mutable TUsedTSE_LockSet        m_UsedTSE_Set;

    mutable CTSE_ScopeUserLock  m_EditLock;

    TScopeInfoMap               m_ScopeInfoMap;

private: // to prevent copying
    CTSE_ScopeInfo(const CTSE_ScopeInfo&);
    CTSE_ScopeInfo& operator=(const CTSE_ScopeInfo&);
};


class NCBI_XOBJMGR_EXPORT CBioseq_ScopeInfo : public CScopeInfo_Base
{
public:
    typedef CRef<CTSE_ScopeInfo>                        TTSE_ScopeInfo;
    typedef set<CSeq_id_Handle>                         TSeq_idSet;
    typedef vector< pair<TTSE_ScopeInfo, CSeq_id_Handle> >             TTSE_MatchSet;
    typedef CObjectFor<TTSE_MatchSet>                   TTSE_MatchSetObject;
    typedef CInitMutex<TTSE_MatchSetObject>             TAnnotRefInfo;
    typedef TIndexIds                                   TIds;
    typedef int                                         TBlobStateFlags;
    typedef CScopeInfo_Ref<CBioseq_ScopeInfo>           TBioseq_Lock;

    explicit CBioseq_ScopeInfo(TBlobStateFlags flag); // no sequence
    explicit CBioseq_ScopeInfo(CTSE_ScopeInfo& tse); // unnamed
    CBioseq_ScopeInfo(CTSE_ScopeInfo& tse, const TIds& ids);
    ~CBioseq_ScopeInfo(void);

    const CBioseq_Info& GetObjectInfo(void) const
        {
            return reinterpret_cast<const CBioseq_Info&>(GetObjectInfo_Base());
        }
    CBioseq_Info& GetNCObjectInfo(void)
        {
            return const_cast<CBioseq_Info&>(GetObjectInfo());
        }

    const TIds& GetIds(void) const
        {
            return m_Ids;
        }
    const TIndexIds* GetIndexIds(void) const;

    bool HasBioseq(void) const;

    string IdString(void) const;

    TBlobStateFlags GetBlobState(void) const
        {
            return m_BlobState;
        }

    TBioseq_Lock GetLock(CConstRef<CBioseq_Info> bioseq);

    // id modification methods are required because we need to update
    // index information in CTSE_ScopeInfo.
    void ResetId(void);
    bool AddId(const CSeq_id_Handle& id);
    bool RemoveId(const CSeq_id_Handle& id);
    
protected: // protected object manager interface
    friend class CScope_Impl;
    friend class CTSE_ScopeInfo;
    friend class CSeq_id_ScopeInfo;

    //void x_ResetLock(void);
    void x_AttachTSE(CTSE_ScopeInfo* tse);
    void x_DetachTSE(CTSE_ScopeInfo* tse);
    void x_ForgetTSE(CTSE_ScopeInfo* tse);

private: // members
    // Real Seq-ids of the bioseq.
    TIds                            m_Ids;
    // Additional blob state flags.
    TBlobStateFlags                 m_BlobState;

    // Cached information.
    // Cache synonyms of bioseq if any.
    // All synonyms share the same CBioseq_ScopeInfo object.
    CInitMutex<CSynonymsSet>        m_SynCache;
    // Cache TSEs with external annotations on this Bioseq.
    CInitMutex<TTSE_MatchSetObject> m_BioseqAnnotRef_Info;

private: // to prevent copying
    CBioseq_ScopeInfo(const CBioseq_ScopeInfo& info);
    const CBioseq_ScopeInfo& operator=(const CBioseq_ScopeInfo& info);
};


struct SSeq_id_ScopeInfo
{
    SSeq_id_ScopeInfo(void);
    ~SSeq_id_ScopeInfo(void);

    typedef CBioseq_ScopeInfo::TTSE_MatchSetObject TTSE_MatchSetObject;

    // Resolved Bioseq information.
    CInitMutex<CBioseq_ScopeInfo>   m_Bioseq_Info;

    // Caches other (not main) TSEs with annotations on this Seq-id.
    CInitMutex<TTSE_MatchSetObject> m_AllAnnotRef_Info;
};


/////////////////////////////////////////////////////////////////////////////
// Inline methods
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// CDataSource_ScopeInfo
/////////////////////////////////////////////////////////////////////////////


inline
CDataSource& CDataSource_ScopeInfo::GetDataSource(void)
{
    return *m_DataSource;
}


inline
const CDataSource& CDataSource_ScopeInfo::GetDataSource(void) const
{
    return *m_DataSource;
}


inline
CDataSource_ScopeInfo::TTSE_InfoMapMutex&
CDataSource_ScopeInfo::GetTSE_InfoMapMutex(void) const
{
    return m_TSE_InfoMapMutex;
}

inline
CDataSource_ScopeInfo::TTSE_LockSetMutex&
CDataSource_ScopeInfo::GetTSE_LockSetMutex(void) const
{
    return m_TSE_LockSetMutex;
}


inline
bool CDataSource_ScopeInfo::CanBeEdited(void) const
{
    return m_CanBeEdited;
}


inline
bool CDataSource_ScopeInfo::CanRemoveOnResetHistory(void) const
{
    return m_CanRemoveOnResetHistory;
}


/////////////////////////////////////////////////////////////////////////////
// CTSE_ScopeInfo
/////////////////////////////////////////////////////////////////////////////


inline
int CTSE_ScopeInfo::GetLoadIndex(void) const
{
    return m_LoadIndex;
}


inline
bool CTSE_ScopeInfo::IsAttached(void) const
{
    return m_DS_Info != 0;
}


inline
bool CTSE_ScopeInfo::CanBeUnloaded(void) const
{
    return m_UnloadedInfo;
}


inline
CDataSource_ScopeInfo& CTSE_ScopeInfo::GetDSInfo(void) const
{
    _ASSERT(m_DS_Info);
    return *m_DS_Info;
}


inline
bool CTSE_ScopeInfo::CanBeEdited(void) const
{
    return GetDSInfo().CanBeEdited();
}


inline
CScope_Impl& CTSE_ScopeInfo::GetScopeImpl(void) const
{
    return GetDSInfo().GetScopeImpl();
}


inline
const CTSE_Lock& CTSE_ScopeInfo::GetTSE_Lock(void) const
{
    return m_TSE_Lock;
}


/////////////////////////////////////////////////////////////////////////////
// CBioseq_ScopeInfo
/////////////////////////////////////////////////////////////////////////////


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SCOPE_INFO__HPP
