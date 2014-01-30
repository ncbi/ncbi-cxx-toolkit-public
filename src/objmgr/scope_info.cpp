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
*           Eugene Vasilchenko
*
* File Description:
*           Structures used by CScope
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/impl/scope_info.hpp>
#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/scope.hpp>

#include <objmgr/impl/synonyms.hpp>
#include <objmgr/impl/data_source.hpp>

#include <objmgr/impl/tse_info.hpp>
#include <objmgr/tse_handle.hpp>
#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>
#include <objmgr/bioseq_set_handle.hpp>

#include <corelib/ncbi_param.hpp>
#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

#if 0
# define _TRACE_TSE_LOCK(x) _TRACE(x)
#else
# define _TRACE_TSE_LOCK(x) ((void)0)
#endif


NCBI_PARAM_DECL(bool, OBJMGR, SCOPE_AUTORELEASE);
NCBI_PARAM_DEF_EX(bool, OBJMGR, SCOPE_AUTORELEASE, true,
                  eParam_NoThread, OBJMGR_SCOPE_AUTORELEASE);

static bool s_GetScopeAutoReleaseEnabled(void)
{
    static const bool sx_Value =
        NCBI_PARAM_TYPE(OBJMGR, SCOPE_AUTORELEASE)::GetDefault();
    return sx_Value;
}


NCBI_PARAM_DECL(unsigned, OBJMGR, SCOPE_AUTORELEASE_SIZE);
NCBI_PARAM_DEF_EX(unsigned, OBJMGR, SCOPE_AUTORELEASE_SIZE, 10,
                  eParam_NoThread, OBJMGR_SCOPE_AUTORELEASE_SIZE);

static unsigned s_GetScopeAutoReleaseSize(void)
{
    static const unsigned sx_Value =
        NCBI_PARAM_TYPE(OBJMGR, SCOPE_AUTORELEASE_SIZE)::GetDefault();
    return sx_Value;
}


NCBI_PARAM_DECL(int, OBJMGR, SCOPE_POSTPONE_DELETE);
NCBI_PARAM_DEF_EX(int, OBJMGR, SCOPE_POSTPONE_DELETE, 1,
                  eParam_NoThread, OBJMGR_SCOPE_POSTPONE_DELETE);

static int s_GetScopePostponeDelete(void)
{
    static const int sx_Value =
        NCBI_PARAM_TYPE(OBJMGR, SCOPE_POSTPONE_DELETE)::GetDefault();
    return sx_Value;
}


DEFINE_STATIC_FAST_MUTEX(sx_UsedTSEMutex);


/////////////////////////////////////////////////////////////////////////////
// CDataSource_ScopeInfo
/////////////////////////////////////////////////////////////////////////////

CDataSource_ScopeInfo::CDataSource_ScopeInfo(CScope_Impl& scope,
                                             CDataSource& ds)
    : m_Scope(&scope),
      m_DataSource(&ds),
      m_CanBeUnloaded(s_GetScopeAutoReleaseEnabled() &&
                      ds.GetDataLoader() &&
                      ds.GetDataLoader()->CanGetBlobById()),
      m_CanBeEdited(ds.CanBeEdited()),
      m_CanRemoveOnResetHistory(false),
      m_NextTSEIndex(0),
      m_TSE_UnlockQueue(s_GetScopeAutoReleaseSize())
{
}


CDataSource_ScopeInfo::~CDataSource_ScopeInfo(void)
{
    _ASSERT(!m_Scope);
    _ASSERT(!m_DataSource);
}


CScope_Impl& CDataSource_ScopeInfo::GetScopeImpl(void) const
{
    if ( !m_Scope ) {
        NCBI_THROW(CCoreException, eNullPtr,
                   "CDataSource_ScopeInfo is not attached to CScope");
    }
    return *m_Scope;
}


CDataLoader* CDataSource_ScopeInfo::GetDataLoader(void)
{
    return GetDataSource().GetDataLoader();
}


bool CDataSource_ScopeInfo::IsConst(void) const
{
    return !CanBeEdited() && GetDataSource().CanBeEdited();
}


void CDataSource_ScopeInfo::SetConst(void)
{
    _ASSERT(CanBeEdited());
    _ASSERT(GetDataSource().CanBeEdited());
    m_CanBeEdited = false;
    _ASSERT(IsConst());
}


void CDataSource_ScopeInfo::SetCanRemoveOnResetHistory(void)
{
    _ASSERT(CanBeEdited());
    _ASSERT(GetDataSource().CanBeEdited());
    m_CanRemoveOnResetHistory = true;
    _ASSERT(CanRemoveOnResetHistory());
}


void CDataSource_ScopeInfo::DetachScope(void)
{
    if ( m_Scope ) {
        _ASSERT(m_DataSource);
        ResetDS();
        GetScopeImpl().m_ObjMgr->ReleaseDataSource(m_DataSource);
        _ASSERT(!m_DataSource);
        m_Scope = 0;
    }
}


const CDataSource_ScopeInfo::TTSE_InfoMap&
CDataSource_ScopeInfo::GetTSE_InfoMap(void) const
{
    return m_TSE_InfoMap;
}


const CDataSource_ScopeInfo::TTSE_LockSet&
CDataSource_ScopeInfo::GetTSE_LockSet(void) const
{
    return m_TSE_LockSet;
}


static DECLARE_TLS_VAR(CUnlockedTSEsGuard*, st_Guard);


CUnlockedTSEsGuard::CUnlockedTSEsGuard(void)
{
    if ( !st_Guard ) {
        st_Guard = this;
    }
}


CUnlockedTSEsGuard::~CUnlockedTSEsGuard(void)
{
    if ( st_Guard == this ) {
        while ( !m_UnlockedTSEsInternal.empty() ) {
            TUnlockedTSEsInternal locks;
            swap(m_UnlockedTSEsInternal, locks);
        }
        st_Guard = 0;
    }
}


void CUnlockedTSEsGuard::SaveLock(const CTSE_Lock& lock)
{
    if ( !s_GetScopePostponeDelete() ) {
        return;
    }
    _ASSERT(st_Guard);
    if ( CUnlockedTSEsGuard* guard = st_Guard ) {
        guard->m_UnlockedTSEsLock.push_back(ConstRef(&*lock));
    }
}


void CUnlockedTSEsGuard::SaveInternal(const CTSE_ScopeInternalLock& lock)
{
    if ( !s_GetScopePostponeDelete() ) {
        return;
    }
    _ASSERT(st_Guard);
    if ( CUnlockedTSEsGuard* guard = st_Guard ) {
        guard->m_UnlockedTSEsInternal.push_back(lock);
    }
}


void CUnlockedTSEsGuard::SaveInternal(const TUnlockedTSEsInternal& locks)
{
    if ( !s_GetScopePostponeDelete() ) {
        return;
    }
    _ASSERT(st_Guard);
    if ( CUnlockedTSEsGuard* guard = st_Guard ) {
        guard->m_UnlockedTSEsInternal.insert
            (guard->m_UnlockedTSEsInternal.end(), locks.begin(), locks.end());
    }
}


void CDataSource_ScopeInfo::RemoveTSE_Lock(const CTSE_Lock& lock)
{
    CUnlockedTSEsGuard::SaveLock(lock);
    TTSE_LockSetMutex::TWriteLockGuard guard(m_TSE_LockSetMutex);
    _VERIFY(m_TSE_LockSet.RemoveLock(lock));
}


void CDataSource_ScopeInfo::AddTSE_Lock(const CTSE_Lock& lock)
{
    TTSE_LockSetMutex::TWriteLockGuard guard(m_TSE_LockSetMutex);
    _VERIFY(m_TSE_LockSet.AddLock(lock));
}


CDataSource_ScopeInfo::TTSE_Lock
CDataSource_ScopeInfo::GetTSE_Lock(const CTSE_Lock& lock)
{
    CTSE_ScopeUserLock ret;
    _ASSERT(lock);
    TTSE_ScopeInfo info;
    {{
        TTSE_InfoMapMutex::TWriteLockGuard guard(m_TSE_InfoMapMutex);
        TTSE_ScopeInfo& slot = m_TSE_InfoMap[lock->GetBlobId()];
        if ( !slot ) {
            slot = info = new CTSE_ScopeInfo(*this, lock,
                                             m_NextTSEIndex++,
                                             m_CanBeUnloaded);
            if ( m_CanBeUnloaded ) {
                // add this TSE into index by SeqId
                x_IndexTSE(*info);
            }
        }
        else {
            info = slot;
        }
        _ASSERT(info->IsAttached() && &info->GetDSInfo() == this);
        info->m_TSE_LockCounter.Add(1);
        {{
            // first remove the TSE from unlock queue
            TTSE_LockSetMutex::TWriteLockGuard guard2(m_TSE_UnlockQueueMutex);
            // TSE must be locked already by caller
            _ASSERT(info->m_TSE_LockCounter.Get() > 0);
            m_TSE_UnlockQueue.Erase(info);
            // TSE must be still locked by caller even after removing it
            // from unlock queue
            _ASSERT(info->m_TSE_LockCounter.Get() > 0);
        }}
        info->SetTSE_Lock(lock);
        ret.Reset(info);
        _VERIFY(info->m_TSE_LockCounter.Add(-1) > 0);
        _ASSERT(info->GetTSE_Lock() == lock);
    }}
    return ret;
}


void CDataSource_ScopeInfo::AttachTSE(CTSE_ScopeInfo& info,
                                      const CTSE_Lock& lock)
{
    _ASSERT(m_CanBeUnloaded == info.CanBeUnloaded());
    _ASSERT(!info.m_DS_Info);
    _ASSERT(!info.m_TSE_Lock);
    _ASSERT(lock && &lock->GetDataSource() == &GetDataSource());
    TTSE_InfoMapMutex::TWriteLockGuard guard(m_TSE_InfoMapMutex);
    _VERIFY(m_TSE_InfoMap.insert(TTSE_InfoMap::value_type
                                 (lock->GetBlobId(),
                                  //STSE_Key(*lock, m_CanBeUnloaded),
                                  Ref(&info))).second);
    if ( m_CanBeUnloaded ) {
        // add this TSE into index by SeqId
        x_IndexTSE(info);
    }
    info.m_DS_Info = this;
    info.SetTSE_Lock(lock);
}


void CDataSource_ScopeInfo::x_IndexTSE(CTSE_ScopeInfo& tse)
{
    ITERATE ( CTSE_ScopeInfo::TSeqIds, it, tse.GetBioseqsIds() ) {
        m_TSE_BySeqId.insert(TTSE_BySeqId::value_type(*it, Ref(&tse)));
    }
}


void CDataSource_ScopeInfo::x_UnindexTSE(const CTSE_ScopeInfo& tse)
{
    ITERATE ( CTSE_ScopeInfo::TSeqIds, it, tse.GetBioseqsIds() ) {
        TTSE_BySeqId::iterator tse_it = m_TSE_BySeqId.lower_bound(*it);
        while ( tse_it != m_TSE_BySeqId.end() && tse_it->first == *it ) {
            if ( tse_it->second == &tse ) {
                m_TSE_BySeqId.erase(tse_it++);
            }
            else {
                ++tse_it;
            }
        }
    }
}


CDataSource_ScopeInfo::TTSE_ScopeInfo
CDataSource_ScopeInfo::x_FindBestTSEInIndex(const CSeq_id_Handle& idh) const
{
    TTSE_ScopeInfo tse;
    for ( TTSE_BySeqId::const_iterator it = m_TSE_BySeqId.lower_bound(idh);
          it != m_TSE_BySeqId.end() && it->first == idh; ++it ) {
        if ( !tse || x_IsBetter(idh, *it->second, *tse) ) {
            tse = it->second;
        }
    }
    return tse;
}


void CDataSource_ScopeInfo::UpdateTSELock(CTSE_ScopeInfo& tse, CTSE_Lock lock)
{
    {{
        // first remove the TSE from unlock queue
        TTSE_LockSetMutex::TWriteLockGuard guard(m_TSE_UnlockQueueMutex);
        // TSE must be locked already by caller
        _ASSERT(tse.m_TSE_LockCounter.Get() > 0);
        m_TSE_UnlockQueue.Erase(&tse);
        // TSE must be still locked by caller even after removing it
        // from unlock queue
        _ASSERT(tse.m_TSE_LockCounter.Get() > 0);
    }}
    if ( !tse.GetTSE_Lock() ) {
        // OK, we need to update the lock
        if ( !lock ) { // obtain lock from CDataSource
            lock = tse.m_UnloadedInfo->LockTSE();
            _ASSERT(lock);
        }
        tse.SetTSE_Lock(lock);
        _ASSERT(tse.GetTSE_Lock() == lock);
    }
    _ASSERT(tse.m_TSE_LockCounter.Get() > 0);
    _ASSERT(tse.GetTSE_Lock());
}


// Called by destructor of CTSE_ScopeUserLock when lock counter goes to 0
void CDataSource_ScopeInfo::ReleaseTSELock(CTSE_ScopeInfo& tse)
{
    CUnlockedTSEsGuard guard;
    {{
        CTSE_ScopeInternalLock unlocked;
        TTSE_LockSetMutex::TWriteLockGuard guard(m_TSE_UnlockQueueMutex);
        if ( tse.m_TSE_LockCounter.Get() > 0 ) {
            // relocked already
            return;
        }
        if ( !tse.GetTSE_Lock() ) {
            // already unlocked
            return;
        }
        m_TSE_UnlockQueue.Put(&tse, CTSE_ScopeInternalLock(&tse), &unlocked);
        if ( unlocked ) {
            CUnlockedTSEsGuard::SaveInternal(unlocked);
        }
    }}
}


// Called by destructor of CTSE_ScopeInternalLock when lock counter goes to 0
// CTSE_ScopeInternalLocks are stored in m_TSE_UnlockQueue 
void CDataSource_ScopeInfo::ForgetTSELock(CTSE_ScopeInfo& tse)
{
    if ( tse.m_TSE_LockCounter.Get() > 0 ) {
        // relocked already
        return;
    }
    if ( !tse.GetTSE_Lock() ) {
        // already unlocked
        return;
    }
    CUnlockedTSEsGuard guard;
    tse.ForgetTSE_Lock();
}


void CDataSource_ScopeInfo::ResetDS(void)
{
    CUnlockedTSEsGuard guard;
    TTSE_InfoMapMutex::TWriteLockGuard guard1(m_TSE_InfoMapMutex);
    {{
        TTSE_UnlockQueue::TUnlockSet unlocked;
        {{
            TTSE_LockSetMutex::TWriteLockGuard guard2(m_TSE_UnlockQueueMutex);
            m_TSE_UnlockQueue.Clear(&unlocked);
        }}
        if ( !unlocked.empty() ) {
            CUnlockedTSEsGuard::SaveInternal(unlocked);
        }
    }}
    NON_CONST_ITERATE ( TTSE_InfoMap, it, m_TSE_InfoMap ) {
        it->second->DropTSE_Lock();
        it->second->x_DetachDS();
    }
    m_TSE_InfoMap.clear();
    m_TSE_BySeqId.clear();
    {{
        TTSE_LockSetMutex::TWriteLockGuard guard2(m_TSE_LockSetMutex);
        m_TSE_LockSet.clear();
    }}
    m_NextTSEIndex = 0;
}


void CDataSource_ScopeInfo::ResetHistory(int action_if_locked)
{
    if ( action_if_locked == CScope::eRemoveIfLocked ) {
        // no checks -> fast reset
        ResetDS();
        return;
    }
    typedef vector< CRef<CTSE_ScopeInfo> > TTSEs;
    TTSEs tses;
    {{
        TTSE_InfoMapMutex::TWriteLockGuard guard1(m_TSE_InfoMapMutex);
        tses.reserve(m_TSE_InfoMap.size());
        ITERATE ( TTSE_InfoMap, it, m_TSE_InfoMap ) {
            tses.push_back(it->second);
        }
    }}
    CUnlockedTSEsGuard guard;
    ITERATE ( TTSEs, it, tses ) {
        it->GetNCObject().RemoveFromHistory(action_if_locked);
    }
}


void CDataSource_ScopeInfo::RemoveFromHistory(CTSE_ScopeInfo& tse,
                                              bool drop_from_ds)
{
    tse.ReleaseUsedTSEs();
    TTSE_InfoMapMutex::TWriteLockGuard guard1(m_TSE_InfoMapMutex);
    if ( tse.CanBeUnloaded() ) {
        x_UnindexTSE(tse);
    }
    _VERIFY(m_TSE_InfoMap.erase(tse.GetBlobId()));
    tse.m_TSE_LockCounter.Add(1); // to prevent storing into m_TSE_UnlockQueue
    // remove TSE lock completely
    {{
        TTSE_LockSetMutex::TWriteLockGuard guard2(m_TSE_UnlockQueueMutex);
        m_TSE_UnlockQueue.Erase(&tse);
    }}
    if ( CanRemoveOnResetHistory() ||
         (drop_from_ds && GetDataSource().CanBeEdited()) ) {
        // remove TSE from static blob set in DataSource
        CConstRef<CTSE_Info> tse_info(&*tse.GetTSE_Lock());
        tse.ResetTSE_Lock();
        GetDataSource().DropStaticTSE(const_cast<CTSE_Info&>(*tse_info));
    }
    else {
        tse.ResetTSE_Lock();
    }
    tse.x_DetachDS();
    tse.m_TSE_LockCounter.Add(-1); // restore lock counter
    _ASSERT(!tse.GetTSE_Lock());
    _ASSERT(!tse.m_DS_Info);
}


CDataSource_ScopeInfo::TTSE_Lock
CDataSource_ScopeInfo::FindTSE_Lock(const CSeq_entry& tse)
{
    CDataSource::TTSE_Lock lock;
    {{
        TTSE_LockSetMutex::TReadLockGuard guard(m_TSE_LockSetMutex);
        lock = GetDataSource().FindTSE_Lock(tse, m_TSE_LockSet);
    }}
    if ( lock ) {
        return GetTSE_Lock(lock);
    }
    return TTSE_Lock();
}


CDataSource_ScopeInfo::TSeq_entry_Lock
CDataSource_ScopeInfo::GetSeq_entry_Lock(const CBlobIdKey& blob_id)
{
    CDataSource::TSeq_entry_Lock lock;
    {{
        TTSE_LockSetMutex::TReadLockGuard guard(m_TSE_LockSetMutex);
        lock = GetDataSource().GetSeq_entry_Lock(blob_id);
    }}
    if ( lock.first ) {
        return TSeq_entry_Lock(lock.first, GetTSE_Lock(lock.second));
    }
    return TSeq_entry_Lock();
}


CDataSource_ScopeInfo::TSeq_entry_Lock
CDataSource_ScopeInfo::FindSeq_entry_Lock(const CSeq_entry& entry)
{
    CDataSource::TSeq_entry_Lock lock;
    {{
        TTSE_LockSetMutex::TReadLockGuard guard(m_TSE_LockSetMutex);
        lock = GetDataSource().FindSeq_entry_Lock(entry, m_TSE_LockSet);
    }}
    if ( lock.first ) {
        return TSeq_entry_Lock(lock.first, GetTSE_Lock(lock.second));
    }
    return TSeq_entry_Lock();
}


CDataSource_ScopeInfo::TSeq_annot_Lock
CDataSource_ScopeInfo::FindSeq_annot_Lock(const CSeq_annot& annot)
{
    CDataSource::TSeq_annot_Lock lock;
    {{
        TTSE_LockSetMutex::TReadLockGuard guard(m_TSE_LockSetMutex);
        lock = GetDataSource().FindSeq_annot_Lock(annot, m_TSE_LockSet);
    }}
    if ( lock.first ) {
        return TSeq_annot_Lock(lock.first, GetTSE_Lock(lock.second));
    }
    return TSeq_annot_Lock();
}


CDataSource_ScopeInfo::TBioseq_set_Lock
CDataSource_ScopeInfo::FindBioseq_set_Lock(const CBioseq_set& seqset)
{
    CDataSource::TBioseq_set_Lock lock;
    {{
        TTSE_LockSetMutex::TReadLockGuard guard(m_TSE_LockSetMutex);
        lock = GetDataSource().FindBioseq_set_Lock(seqset, m_TSE_LockSet);
    }}
    if ( lock.first ) {
        return TBioseq_set_Lock(lock.first, GetTSE_Lock(lock.second));
    }
    return TBioseq_set_Lock();
}


CDataSource_ScopeInfo::TBioseq_Lock
CDataSource_ScopeInfo::FindBioseq_Lock(const CBioseq& bioseq)
{
    CDataSource::TBioseq_Lock lock;
    {{
        TTSE_LockSetMutex::TReadLockGuard guard(m_TSE_LockSetMutex);
        lock = GetDataSource().FindBioseq_Lock(bioseq, m_TSE_LockSet);
    }}
    if ( lock.first ) {
        return GetTSE_Lock(lock.second)->GetBioseqLock(null, lock.first);
    }
    return TBioseq_Lock();
}


CDataSource_ScopeInfo::TSeq_feat_Lock
CDataSource_ScopeInfo::FindSeq_feat_Lock(const CSeq_id_Handle& loc_id,
                                         TSeqPos loc_pos,
                                         const CSeq_feat& feat)
{
    TSeq_feat_Lock ret;
    CDataSource::TSeq_feat_Lock lock;
    {{
        TTSE_LockSetMutex::TReadLockGuard guard(m_TSE_LockSetMutex);
        lock = GetDataSource().FindSeq_feat_Lock(loc_id, loc_pos, feat);
    }}
    if ( lock.first.first ) {
        ret.first.first = lock.first.first;
        ret.first.second = GetTSE_Lock(lock.first.second);
        ret.second = lock.second;
    }
    return ret;
}


SSeqMatch_Scope CDataSource_ScopeInfo::BestResolve(const CSeq_id_Handle& idh,
                                                   int get_flag)
{
    SSeqMatch_Scope ret = x_GetSeqMatch(idh);
    if ( !ret && get_flag == CScope::eGetBioseq_All ) {
        // Try to load the sequence from the data source
        SSeqMatch_DS ds_match = GetDataSource().BestResolve(idh);
        if ( ds_match ) {
            x_SetMatch(ret, ds_match);
        }
    }
#ifdef _DEBUG
    if ( ret ) {
        _ASSERT(ret.m_Seq_id);
        _ASSERT(ret.m_Bioseq);
        _ASSERT(ret.m_TSE_Lock);
        _ASSERT(ret.m_Bioseq == ret.m_TSE_Lock->m_TSE_Lock->FindBioseq(ret.m_Seq_id));
    }
#endif
    return ret;
}


SSeqMatch_Scope CDataSource_ScopeInfo::Resolve(const CSeq_id_Handle& idh,
                                               CTSE_ScopeInfo& tse)
{
    SSeqMatch_Scope ret;
    x_SetMatch(ret, tse, idh);
    return ret;
}


SSeqMatch_Scope CDataSource_ScopeInfo::x_GetSeqMatch(const CSeq_id_Handle& idh)
{
    SSeqMatch_Scope ret = x_FindBestTSE(idh);
    if ( !ret && idh.HaveMatchingHandles() ) {
        CSeq_id_Handle::TMatches ids;
        idh.GetMatchingHandles(ids, eAllowWeakMatch);
        ITERATE ( CSeq_id_Handle::TMatches, it, ids ) {
            if ( *it == idh ) // already checked
                continue;
            if ( ret && ret.m_Seq_id.IsBetter(*it) ) // worse hit
                continue;
            if ( SSeqMatch_Scope match = x_FindBestTSE(*it) ) {
                ret = match;
            }
        }
    }
    return ret;
}


SSeqMatch_Scope CDataSource_ScopeInfo::x_FindBestTSE(const CSeq_id_Handle& idh)
{
    SSeqMatch_Scope ret;
    if ( m_CanBeUnloaded ) {
        // We have full index of static TSEs.
        TTSE_InfoMapMutex::TReadLockGuard guard(GetTSE_InfoMapMutex());
        TTSE_ScopeInfo tse = x_FindBestTSEInIndex(idh);
        if ( tse ) {
            x_SetMatch(ret, *tse, idh);
        }
    }
    else {
        // We have to ask data source about it.
        CDataSource::TSeqMatches matches;
        {{
            TTSE_LockSetMutex::TReadLockGuard guard(m_TSE_LockSetMutex);
            CDataSource::TSeqMatches matches2 =
                GetDataSource().GetMatches(idh, m_TSE_LockSet);
            matches.swap(matches2);
        }}
        ITERATE ( CDataSource::TSeqMatches, it, matches ) {
            SSeqMatch_Scope nxt;
            x_SetMatch(nxt, *it);
            if ( !ret || x_IsBetter(idh, *nxt.m_TSE_Lock, *ret.m_TSE_Lock) ) {
                ret = nxt;
            }
        }
    }
    return ret;
}


bool CDataSource_ScopeInfo::x_IsBetter(const CSeq_id_Handle& idh,
                                       const CTSE_ScopeInfo& tse1,
                                       const CTSE_ScopeInfo& tse2)
{
    // First of all we check if we already resolve bioseq with this id.
    bool resolved1 = tse1.HasResolvedBioseq(idh);
    bool resolved2 = tse2.HasResolvedBioseq(idh);
    if ( resolved1 != resolved2 ) {
        return resolved1;
    }
    // Now check TSEs' orders.
    CTSE_ScopeInfo::TBlobOrder order1 = tse1.GetBlobOrder();
    CTSE_ScopeInfo::TBlobOrder order2 = tse2.GetBlobOrder();
    if ( order1 != order2 ) {
        return order1 < order2;
    }

    // Now we have very similar TSE's so we'll prefer the first one added.
    return tse1.GetLoadIndex() < tse2.GetLoadIndex();
}


void CDataSource_ScopeInfo::x_SetMatch(SSeqMatch_Scope& match,
                                       CTSE_ScopeInfo& tse,
                                       const CSeq_id_Handle& idh) const
{
    match.m_Seq_id = idh;
    match.m_TSE_Lock = CTSE_ScopeUserLock(&tse);
    _ASSERT(match.m_Seq_id);
    _ASSERT(match.m_TSE_Lock);
    match.m_Bioseq = match.m_TSE_Lock->GetTSE_Lock()->FindBioseq(idh);
    _ASSERT(match.m_Bioseq);
    _ASSERT(match.m_Bioseq == match.m_TSE_Lock->m_TSE_Lock->FindBioseq(match.m_Seq_id));
}


void CDataSource_ScopeInfo::x_SetMatch(SSeqMatch_Scope& match,
                                       const SSeqMatch_DS& ds_match)
{
    match.m_Seq_id = ds_match.m_Seq_id;
    match.m_TSE_Lock = GetTSE_Lock(ds_match.m_TSE_Lock);
    match.m_Bioseq = ds_match.m_Bioseq;
    _ASSERT(match.m_Seq_id);
    _ASSERT(match.m_Bioseq);
    _ASSERT(match.m_TSE_Lock);
    _ASSERT(match.m_Bioseq == match.m_TSE_Lock->GetTSE_Lock()->FindBioseq(match.m_Seq_id));
}


void CDataSource_ScopeInfo::GetBlobs(TSeqMatchMap& match_map)
{
    CDataSource::TSeqMatchMap ds_match_map;
    ITERATE(TSeqMatchMap, it, match_map) {
        if ( it->second ) {
            continue;
        }
        ds_match_map.insert(CDataSource::TSeqMatchMap::value_type(
            it->first, SSeqMatch_DS()));
    }
    if ( match_map.empty() ) {
        return;
    }
    GetDataSource().GetBlobs(ds_match_map);
    ITERATE(CDataSource::TSeqMatchMap, ds_match, ds_match_map) {
        if ( !ds_match->second ) {
            continue;
        }
        SSeqMatch_Scope& scope_match = match_map[ds_match->first];
        scope_match = x_GetSeqMatch(ds_match->first);
        x_SetMatch(scope_match, ds_match->second);
    }
}


bool CDataSource_ScopeInfo::TSEIsInQueue(const CTSE_ScopeInfo& tse) const
{
    TTSE_LockSetMutex::TReadLockGuard guard(m_TSE_UnlockQueueMutex);
    return m_TSE_UnlockQueue.Contains(&tse);
}


/////////////////////////////////////////////////////////////////////////////
// CTSE_ScopeInfo
/////////////////////////////////////////////////////////////////////////////


CTSE_ScopeInfo::SUnloadedInfo::SUnloadedInfo(const CTSE_Lock& tse_lock)
    : m_Loader(tse_lock->GetDataSource().GetDataLoader()),
      m_BlobId(tse_lock->GetBlobId()),
      m_BlobOrder(tse_lock->GetBlobOrder())
{
    _ASSERT(m_Loader);
    _ASSERT(m_BlobId);
    // copy all bioseq ids
    tse_lock->GetBioseqsIds(m_BioseqsIds);
}


CTSE_Lock CTSE_ScopeInfo::SUnloadedInfo::LockTSE(void)
{
    _ASSERT(m_Loader);
    _ASSERT(m_BlobId);
    return m_Loader->GetBlobById(m_BlobId);
}


CTSE_ScopeInfo::CTSE_ScopeInfo(CDataSource_ScopeInfo& ds_info,
                               const CTSE_Lock& lock,
                               int load_index,
                               bool can_be_unloaded)
    : m_DS_Info(&ds_info),
      m_LoadIndex(load_index),
      m_UsedByTSE(0)
{
    _ASSERT(lock);
    if ( can_be_unloaded ) {
        _ASSERT(lock->GetBlobId());
        m_UnloadedInfo.reset(new SUnloadedInfo(lock));
    }
    else {
        // permanent lock
        _TRACE_TSE_LOCK("CTSE_ScopeInfo("<<this<<") perm lock");
        m_TSE_LockCounter.Add(1);
        x_SetTSE_Lock(lock);
        _ASSERT(m_TSE_Lock == lock);
    }
}


CTSE_ScopeInfo::~CTSE_ScopeInfo(void)
{
    if ( !CanBeUnloaded() ) {
        // remove permanent lock
        _TRACE_TSE_LOCK("CTSE_ScopeInfo("<<this<<") perm unlock: "<<m_TSE_LockCounter.Get());
        _VERIFY(m_TSE_LockCounter.Add(-1) == 0);
    }
    x_DetachDS();
    _TRACE_TSE_LOCK("CTSE_ScopeInfo("<<this<<") final: "<<m_TSE_LockCounter.Get());
    _ASSERT(m_TSE_LockCounter.Get() == 0);
    _ASSERT(!m_TSE_Lock);
    _ASSERT(!m_UsedByTSE);
    _ASSERT(m_UsedTSE_Set.empty());
}


CTSE_ScopeInfo::TBlobOrder CTSE_ScopeInfo::GetBlobOrder(void) const
{
    if ( CanBeUnloaded() ) {
        _ASSERT(m_UnloadedInfo.get());
        return m_UnloadedInfo->m_BlobOrder;
    }
    else {
        _ASSERT(m_TSE_Lock);
        return m_TSE_Lock->GetBlobOrder();
    }
}


CTSE_ScopeInfo::TBlobId CTSE_ScopeInfo::GetBlobId(void) const
{
    if ( CanBeUnloaded() ) {
        _ASSERT(m_UnloadedInfo.get());
        return m_UnloadedInfo->m_BlobId;
    }
    else {
        _ASSERT(m_TSE_Lock);
        return m_TSE_Lock->GetBlobId();
    }
}


const CTSE_ScopeInfo::TSeqIds& CTSE_ScopeInfo::GetBioseqsIds(void) const
{
    _ASSERT(CanBeUnloaded());
    return m_UnloadedInfo->m_BioseqsIds;
}


void CTSE_ScopeInfo_Base::x_LockTSE(void)
{
    CTSE_ScopeInfo* tse = static_cast<CTSE_ScopeInfo*>(this);
    if ( !tse->m_TSE_Lock ) {
        CDataSource_ScopeInfo* ds = tse->m_DS_Info;
        if ( !ds ) {
            tse->m_TSE_LockCounter.Add(-1);
            NCBI_THROW(CCoreException, eNullPtr,
                       "CTSE_ScopeInfo is not attached to CScope");
        }
        ds->UpdateTSELock(*tse, CTSE_Lock());
    }
    _ASSERT(tse->m_TSE_Lock);
}


void CTSE_ScopeInfo_Base::x_UserUnlockTSE(void)
{
    CTSE_ScopeInfo* tse = static_cast<CTSE_ScopeInfo*>(this);
    _ASSERT(tse->CanBeUnloaded());
    if ( tse->IsAttached() ) {
        tse->GetDSInfo().ReleaseTSELock(*tse);
    }
}


void CTSE_ScopeInfo_Base::x_InternalUnlockTSE(void)
{
    CTSE_ScopeInfo* tse = static_cast<CTSE_ScopeInfo*>(this);
    _ASSERT(tse->CanBeUnloaded());
    if ( tse->IsAttached() ) {
        tse->GetDSInfo().ForgetTSELock(*tse);
    }
}


bool CTSE_ScopeInfo::x_SameTSE(const CTSE_Info& tse) const
{
    return m_TSE_LockCounter.Get() > 0 && m_TSE_Lock && &*m_TSE_Lock == &tse;
}


void CTSE_ScopeInfo::ReleaseUsedTSEs(void)
{
    // release all used TSEs
    TUsedTSE_LockSet used;
    CFastMutexGuard guard(sx_UsedTSEMutex);
    NON_CONST_ITERATE ( TUsedTSE_LockSet, it, m_UsedTSE_Set ) {
        _ASSERT(it->second->m_UsedByTSE == this);
        it->second->m_UsedByTSE = 0;
    }
    m_UsedTSE_Set.swap(used);
}


bool CTSE_ScopeInfo::AddUsedTSE(const CTSE_ScopeUserLock& used_tse) const
{
    CTSE_ScopeInternalLock add_lock(used_tse.GetNCPointer());
    CTSE_ScopeInfo& add_info = const_cast<CTSE_ScopeInfo&>(*used_tse);
    if ( &add_info == this || // the same TSE
         !add_info.CanBeUnloaded() || // added is permanentrly locked
         m_TSE_LockCounter.Get() == 0 ) { // this one is unlocked
        return false;
    }
    CFastMutexGuard guard(sx_UsedTSEMutex);
    if ( add_info.m_UsedByTSE ) { // already used
        return false;
    }
    for ( const CTSE_ScopeInfo* p = m_UsedByTSE; p; p = p->m_UsedByTSE ) {
        if ( p == &add_info ) {
            return false;
        }
    }
    CTSE_ScopeInternalLock& add_slot = m_UsedTSE_Set[ConstRef(&*used_tse)];
    _ASSERT(!add_slot);
    add_info.m_UsedByTSE = this;
    swap(add_slot, add_lock);
    return true;
}


void CTSE_ScopeInfo::x_SetTSE_Lock(const CTSE_Lock& lock)
{
    _ASSERT(lock);
    if ( !m_TSE_Lock ) {
        m_TSE_Lock = lock;
        GetDSInfo().AddTSE_Lock(lock);
    }
    _ASSERT(m_TSE_Lock == lock);
}


void CTSE_ScopeInfo::x_ResetTSE_Lock(void)
{
    if ( m_TSE_Lock ) {
        CTSE_Lock lock;
        lock.Swap(m_TSE_Lock);
        GetDSInfo().RemoveTSE_Lock(lock);
    }
    _ASSERT(!m_TSE_Lock);
}


void CTSE_ScopeInfo::SetTSE_Lock(const CTSE_Lock& lock)
{
    _ASSERT(lock);
    if ( !m_TSE_Lock ) {
        CMutexGuard guard(m_TSE_LockMutex);
        x_SetTSE_Lock(lock);
    }
    _ASSERT(m_TSE_Lock == lock);
}


void CTSE_ScopeInfo::ResetTSE_Lock(void)
{
    if ( m_TSE_Lock ) {
        CMutexGuard guard(m_TSE_LockMutex);
        x_ResetTSE_Lock();
    }
    _ASSERT(!m_TSE_Lock);
}


void CTSE_ScopeInfo::DropTSE_Lock(void)
{
    if ( m_TSE_Lock ) {
        CMutexGuard guard(m_TSE_LockMutex);
        m_TSE_Lock.Reset();
    }
    _ASSERT(!m_TSE_Lock);
}


void CTSE_ScopeInfo::SetEditTSE(const CTSE_Lock& new_tse_lock,
                                CDataSource_ScopeInfo& new_ds,
                                const TEditInfoMap& edit_map)
{
    _ASSERT(!CanBeEdited());
    _ASSERT(new_ds.CanBeEdited());
    _ASSERT(&new_tse_lock->GetDataSource() == &new_ds.GetDataSource());

    CUnlockedTSEsGuard unlocked_guard;
    CMutexGuard guard(m_TSE_LockMutex);
    _ASSERT(m_TSE_Lock);
    _ASSERT(&m_TSE_Lock->GetDataSource() == &GetDSInfo().GetDataSource());
    CTSE_Lock old_tse_lock = m_TSE_Lock;
    
    TScopeInfoMap old_map; // save old scope info map
    old_map.swap(m_ScopeInfoMap);
    TBioseqById old_bioseq_map; // save old bioseq info map
    old_bioseq_map.swap(m_BioseqById);

    GetDSInfo().RemoveFromHistory(*this); // remove tse from old ds
    _ASSERT(!m_TSE_Lock);
    _ASSERT(!m_DS_Info);
    if ( CanBeUnloaded() ) {
        m_UnloadedInfo.reset(); // edit tse cannot be unloaded
        m_TSE_LockCounter.Add(1);
    }

    // convert scope info map
    NON_CONST_ITERATE ( TScopeInfoMap, it, old_map ) {
        CConstRef<CObject> old_obj(it->first);
        _ASSERT(old_obj);
        TEditInfoMap::const_iterator iter = edit_map.find(old_obj);
        TScopeInfoMapKey new_obj;
        if ( iter == edit_map.end() ) {
            _ASSERT(&*old_obj == &*old_tse_lock);
            new_obj.Reset(&*new_tse_lock);
        }
        else {
            new_obj.Reset(&dynamic_cast<const CTSE_Info_Object&>(*iter->second));
        }
        _ASSERT(new_obj);
        _ASSERT(&*new_obj != &*old_obj);
        TScopeInfoMapValue info = it->second;
        _ASSERT(info->m_ObjectInfo == old_obj);
        info->m_ObjectInfo = new_obj;
        _VERIFY(m_ScopeInfoMap.insert
                (TScopeInfoMap::value_type(new_obj, info)).second);
    }
    // restore bioseq info map
    m_BioseqById.swap(old_bioseq_map);

    new_ds.AttachTSE(*this, new_tse_lock);

    _ASSERT(&GetDSInfo() == &new_ds);
    _ASSERT(m_TSE_Lock == new_tse_lock);
}


// Action A4.
void CTSE_ScopeInfo::ForgetTSE_Lock(void)
{
    if ( m_TSE_LockCounter.Get() > 0 ) {
        // relocked already
        return;
    }
    ReleaseUsedTSEs();
    if ( !m_TSE_Lock ) {
        return;
    }
    CMutexGuard guard(m_TSE_LockMutex);
    if ( m_TSE_LockCounter.Get() > 0 ) {
        // relocked already
        return;
    }
    if ( !m_TSE_Lock ) {
        return;
    }
    NON_CONST_ITERATE ( TScopeInfoMap, it, m_ScopeInfoMap ) {
        _ASSERT(!it->second->m_TSE_Handle.m_TSE);
        it->second->m_ObjectInfo.Reset();
        if ( it->second->IsTemporary() ) {
            it->second->x_DetachTSE(this);
        }
    }
    m_ScopeInfoMap.clear();
    x_ResetTSE_Lock();
}


void CTSE_ScopeInfo::x_DetachDS(void)
{
    if ( !m_DS_Info ) {
        return;
    }
    ReleaseUsedTSEs();
    CMutexGuard guard(m_TSE_LockMutex);
    NON_CONST_ITERATE ( TScopeInfoMap, it, m_ScopeInfoMap ) {
        it->second->m_TSE_Handle.Reset();
        it->second->m_ObjectInfo.Reset();
        it->second->x_DetachTSE(this);
    }
    m_ScopeInfoMap.clear();
    m_TSE_Lock.Reset();
    while ( !m_BioseqById.empty() ) {
        CRef<CBioseq_ScopeInfo> bioseq = m_BioseqById.begin()->second;
        bioseq->x_DetachTSE(this);
        _ASSERT(m_BioseqById.empty()||m_BioseqById.begin()->second != bioseq);
    }
    m_DS_Info = 0;
}


int CTSE_ScopeInfo::x_GetDSLocksCount(void) const
{
    int max_locks = CanBeUnloaded() ? 0 : 1;
    if ( GetDSInfo().TSEIsInQueue(*this) ) {
        // Extra-lock from delete queue allowed
        ++max_locks;
    }
    return max_locks;
}


bool CTSE_ScopeInfo::IsLocked(void) const
{
    return int(m_TSE_LockCounter.Get()) > x_GetDSLocksCount();
}


bool CTSE_ScopeInfo::LockedMoreThanOnce(void) const
{
    return int(m_TSE_LockCounter.Get()) > x_GetDSLocksCount() + 1;
}


void CTSE_ScopeInfo::RemoveFromHistory(int action_if_locked, bool drop_from_ds)
{
    if ( IsLocked() ) {
        switch ( action_if_locked ) {
        case CScope::eKeepIfLocked:
            return;
        case CScope::eThrowIfLocked:
            NCBI_THROW(CObjMgrException, eLockedData,
                       "Cannot remove TSE from scope's history "
                       "because it's locked");
        default: // forced removal
            break;
        }
    }
    CUnlockedTSEsGuard guard;
    GetDSInfo().RemoveFromHistory(*this, drop_from_ds);
}


bool CTSE_ScopeInfo::HasResolvedBioseq(const CSeq_id_Handle& id) const
{
    return m_BioseqById.find(id) != m_BioseqById.end();
}


bool CTSE_ScopeInfo::ContainsBioseq(const CSeq_id_Handle& id) const
{
    if ( CanBeUnloaded() ) {
        return binary_search(m_UnloadedInfo->m_BioseqsIds.begin(),
                             m_UnloadedInfo->m_BioseqsIds.end(),
                             id);
    }
    else {
        return m_TSE_Lock->ContainsBioseq(id);
    }
}


CSeq_id_Handle
CTSE_ScopeInfo::ContainsMatchingBioseq(const CSeq_id_Handle& id) const
{
    if ( CanBeUnloaded() ) {
        if ( ContainsBioseq(id) ) {
            return id;
        }
        if ( id.HaveMatchingHandles() ) {
            CSeq_id_Handle::TMatches ids;
            id.GetMatchingHandles(ids, eAllowWeakMatch);
            ITERATE ( CSeq_id_Handle::TMatches, it, ids ) {
                if ( *it != id ) {
                    if ( ContainsBioseq(*it) ) {
                        return *it;
                    }
                }
            }
        }
        return null;
    }
    else {
        return m_TSE_Lock->ContainsMatchingBioseq(id);
    }
}

// Action A5.
CTSE_ScopeInfo::TSeq_entry_Lock
CTSE_ScopeInfo::GetScopeLock(const CTSE_Handle& tse,
                             const CSeq_entry_Info& info)
{
    CMutexGuard guard(m_TSE_LockMutex);
    _ASSERT(x_SameTSE(tse.x_GetTSE_Info()));
    CRef<CSeq_entry_ScopeInfo> scope_info;
    TScopeInfoMapKey key(&info);
    TScopeInfoMap::iterator iter = m_ScopeInfoMap.lower_bound(key);
    if ( iter == m_ScopeInfoMap.end() || iter->first != key ) {
        scope_info = new CSeq_entry_ScopeInfo(tse, info);
        TScopeInfoMapValue value(scope_info);
        m_ScopeInfoMap.insert(iter, TScopeInfoMap::value_type(key, value));
        value->m_ObjectInfo = &info;
    }
    else {
        _ASSERT(iter->second->HasObject());
        _ASSERT(&iter->second->GetObjectInfo_Base() == &info);
        scope_info = &dynamic_cast<CSeq_entry_ScopeInfo&>(*iter->second);
    }
    if ( !scope_info->m_TSE_Handle.m_TSE ) {
        scope_info->m_TSE_Handle = tse.m_TSE;
    }
    _ASSERT(scope_info->IsAttached());
    _ASSERT(scope_info->m_TSE_Handle.m_TSE);
    _ASSERT(scope_info->HasObject());
    return TSeq_entry_Lock(*scope_info);
}

// Action A5.
CTSE_ScopeInfo::TSeq_annot_Lock
CTSE_ScopeInfo::GetScopeLock(const CTSE_Handle& tse,
                             const CSeq_annot_Info& info)
{
    CMutexGuard guard(m_TSE_LockMutex);
    _ASSERT(x_SameTSE(tse.x_GetTSE_Info()));
    CRef<CSeq_annot_ScopeInfo> scope_info;
    TScopeInfoMapKey key(&info);
    TScopeInfoMap::iterator iter = m_ScopeInfoMap.lower_bound(key);
    if ( iter == m_ScopeInfoMap.end() || iter->first != key ) {
        scope_info = new CSeq_annot_ScopeInfo(tse, info);
        TScopeInfoMapValue value(scope_info);
        m_ScopeInfoMap.insert(iter, TScopeInfoMap::value_type(key, value));
        value->m_ObjectInfo = &info;
    }
    else {
        _ASSERT(iter->second->HasObject());
        _ASSERT(&iter->second->GetObjectInfo_Base() == &info);
        scope_info = &dynamic_cast<CSeq_annot_ScopeInfo&>(*iter->second);
    }
    if ( !scope_info->m_TSE_Handle.m_TSE ) {
        scope_info->m_TSE_Handle = tse.m_TSE;
    }
    _ASSERT(scope_info->IsAttached());
    _ASSERT(scope_info->m_TSE_Handle.m_TSE);
    _ASSERT(scope_info->HasObject());
    return TSeq_annot_Lock(*scope_info);
}

// Action A5.
CTSE_ScopeInfo::TBioseq_set_Lock
CTSE_ScopeInfo::GetScopeLock(const CTSE_Handle& tse,
                             const CBioseq_set_Info& info)
{
    CMutexGuard guard(m_TSE_LockMutex);
    _ASSERT(x_SameTSE(tse.x_GetTSE_Info()));
    CRef<CBioseq_set_ScopeInfo> scope_info;
    TScopeInfoMapKey key(&info);
    TScopeInfoMap::iterator iter = m_ScopeInfoMap.lower_bound(key);
    if ( iter == m_ScopeInfoMap.end() || iter->first != key ) {
        scope_info = new CBioseq_set_ScopeInfo(tse, info);
        TScopeInfoMapValue value(scope_info);
        m_ScopeInfoMap.insert(iter, TScopeInfoMap::value_type(key, value));
        value->m_ObjectInfo = &info;
    }
    else {
        _ASSERT(iter->second->HasObject());
        _ASSERT(&iter->second->GetObjectInfo_Base() == &info);
        scope_info = &dynamic_cast<CBioseq_set_ScopeInfo&>(*iter->second);
    }
    if ( !scope_info->m_TSE_Handle.m_TSE ) {
        scope_info->m_TSE_Handle = tse.m_TSE;
    }
    _ASSERT(scope_info->IsAttached());
    _ASSERT(scope_info->m_TSE_Handle.m_TSE);
    _ASSERT(scope_info->HasObject());
    return TBioseq_set_Lock(*scope_info);
}

// Action A5.
CTSE_ScopeInfo::TBioseq_Lock
CTSE_ScopeInfo::GetBioseqLock(CRef<CBioseq_ScopeInfo> info,
                              CConstRef<CBioseq_Info> bioseq)
{
    CMutexGuard guard(m_TSE_LockMutex);
    CTSE_ScopeUserLock tse(this);
    _ASSERT(m_TSE_Lock);
    if ( !info ) {
        // find CBioseq_ScopeInfo
        _ASSERT(bioseq);
        _ASSERT(bioseq->BelongsToTSE_Info(*m_TSE_Lock));
        const CBioseq_Info::TId& ids = bioseq->GetId();
        if ( !ids.empty() ) {
            // named bioseq, look in Seq-id index only
            info = x_FindBioseqInfo(ids);
            if ( !info ) {
                info = x_CreateBioseqInfo(ids);
            }
        }
        else {
            // unnamed bioseq, look in object map, create if necessary
            TScopeInfoMapKey key(bioseq);
            TScopeInfoMap::iterator iter = m_ScopeInfoMap.lower_bound(key);
            if ( iter == m_ScopeInfoMap.end() || iter->first != key ) {
                info = new CBioseq_ScopeInfo(*this);
                TScopeInfoMapValue value(info);
                iter = m_ScopeInfoMap
                    .insert(iter, TScopeInfoMap::value_type(key, value));
                value->m_ObjectInfo = &*bioseq;
            }
            else {
                _ASSERT(iter->second->HasObject());
                _ASSERT(&iter->second->GetObjectInfo_Base() == &*bioseq);
                info.Reset(&dynamic_cast<CBioseq_ScopeInfo&>(*iter->second));
            }
            if ( !info->m_TSE_Handle.m_TSE ) {
                info->m_TSE_Handle = tse;
            }
            _ASSERT(info->IsAttached());
            _ASSERT(info->m_TSE_Handle.m_TSE);
            _ASSERT(info->HasObject());
            return TBioseq_Lock(*info);
        }
    }
    _ASSERT(info);
    _ASSERT(!info->IsDetached());
    // update CBioseq_ScopeInfo object
    if ( !info->HasObject() ) {
        if ( !bioseq ) {
            const CBioseq_ScopeInfo::TIds& ids = info->GetIds();
            if ( !ids.empty() ) {
                const CSeq_id_Handle& id = *ids.begin();
                bioseq = m_TSE_Lock->FindBioseq(id);
                _ASSERT(bioseq);
            }
            else {
                // unnamed bioseq without object - error,
                // this situation must be prevented by code.
                _ASSERT(0 && "CBioseq_ScopeInfo without ids and bioseq");
            }
        }
        _ASSERT(bioseq);
        _ASSERT(bioseq->GetId() == info->GetIds());
        TScopeInfoMapKey key(bioseq);
        TScopeInfoMapValue value(info);
        _VERIFY(m_ScopeInfoMap
                .insert(TScopeInfoMap::value_type(key, value)).second);
        info->m_ObjectInfo = &*bioseq;
        info->x_SetLock(tse, *bioseq);
    }
    if ( !info->m_TSE_Handle.m_TSE ) {
        info->m_TSE_Handle = tse;
    }
    _ASSERT(info->HasObject());
    _ASSERT(info->GetObjectInfo().BelongsToTSE_Info(*m_TSE_Lock));
    _ASSERT(m_ScopeInfoMap.find(TScopeInfoMapKey(&info->GetObjectInfo()))->second == info);
    _ASSERT(info->IsAttached());
    _ASSERT(info->m_TSE_Handle.m_TSE);
    _ASSERT(info->HasObject());
    return TBioseq_Lock(*info);
}


// Action A1
void CTSE_ScopeInfo::RemoveLastInfoLock(CScopeInfo_Base& info)
{
    if ( !info.m_TSE_Handle.m_TSE ) {
        // already unlocked
        return;
    }
    CRef<CTSE_ScopeInfo> self;
    {{
        CMutexGuard guard(m_TSE_LockMutex);
        if ( info.m_LockCounter.Get() > 0 ) {
            // already locked again
            return;
        }
        self = this; // to prevent deletion of this while mutex is locked.
        info.m_TSE_Handle.Reset();
    }}
}


// Find scope bioseq info by match: CConstRef<CBioseq_Info> & CSeq_id_Handle
// The problem is that CTSE_Info and CBioseq_Info may be unloaded and we
// cannot store pointers to them.
// However, we have to find the same CBioseq_ScopeInfo object.
// It is stored in m_BioseqById map under one of Bioseq's ids.
CRef<CBioseq_ScopeInfo>
CTSE_ScopeInfo::GetBioseqInfo(const SSeqMatch_Scope& match)
{
    _ASSERT(&*match.m_TSE_Lock == this);
    _ASSERT(match.m_Seq_id);
    _ASSERT(match.m_Bioseq);
    CRef<CBioseq_ScopeInfo> info;
    const CBioseq_Info::TId& ids = match.m_Bioseq->GetId();
    _ASSERT(find(ids.begin(), ids.end(), match.m_Seq_id) != ids.end());

    CMutexGuard guard(m_TSE_LockMutex);
    
    info = x_FindBioseqInfo(ids);
    if ( !info ) {
        info = x_CreateBioseqInfo(ids);
    }
    return info;
}


CRef<CBioseq_ScopeInfo>
CTSE_ScopeInfo::x_FindBioseqInfo(const TSeqIds& ids) const
{
    if ( !ids.empty() ) {
        const CSeq_id_Handle& id = *ids.begin();
        for ( TBioseqById::const_iterator it(m_BioseqById.lower_bound(id));
              it != m_BioseqById.end() && it->first == id; ++it ) {
            if ( it->second->GetIds() == ids ) {
                return it->second;
            }
        }
    }
    return null;
}


CRef<CBioseq_ScopeInfo> CTSE_ScopeInfo::x_CreateBioseqInfo(const TSeqIds& ids)
{
    return Ref(new CBioseq_ScopeInfo(*this, ids));
}


void CTSE_ScopeInfo::x_IndexBioseq(const CSeq_id_Handle& id,
                                   CBioseq_ScopeInfo* info)
{
    m_BioseqById.insert(TBioseqById::value_type(id, Ref(info)));
}


void CTSE_ScopeInfo::x_UnindexBioseq(const CSeq_id_Handle& id,
                                     const CBioseq_ScopeInfo* info)
{
    for ( TBioseqById::iterator it = m_BioseqById.lower_bound(id);
          it != m_BioseqById.end() && it->first == id; ++it ) {
        if ( it->second == info ) {
            m_BioseqById.erase(it);
            return;
        }
    }
    _ASSERT(0 && "UnindexBioseq: CBioseq_ScopeInfo is not in index");
}

// Action A2.
void CTSE_ScopeInfo::ResetEntry(CSeq_entry_ScopeInfo& info)
{
    CMutexGuard guard(m_TSE_LockMutex);
    _ASSERT(info.IsAttached());
    CScopeInfo_Ref<CScopeInfo_Base> child;
    if ( info.GetObjectInfo().Which() == CSeq_entry::e_Set ) {
        child.Reset(&*GetScopeLock(info.m_TSE_Handle,
                                   info.GetObjectInfo().GetSet()));
    }
    else if ( info.GetObjectInfo().Which() == CSeq_entry::e_Seq ) {
        CConstRef<CBioseq_Info> bioseq(&info.GetObjectInfo().GetSeq());
        child.Reset(&GetBioseqLock(null, bioseq).GetNCObject());
    }
    else {
        // nothing to do
        return;
    }
    info.GetNCObjectInfo().Reset();
    x_SaveRemoved(*child);
    _ASSERT(child->IsDetached());
}

// Action A2.
void CTSE_ScopeInfo::RemoveEntry(CSeq_entry_ScopeInfo& info)
{
    CMutexGuard guard(m_TSE_LockMutex);
    _ASSERT(info.IsAttached());
    CSeq_entry_Info& entry = info.GetNCObjectInfo();
    entry.GetParentBioseq_set_Info().RemoveEntry(Ref(&entry));
    x_SaveRemoved(info);
    _ASSERT(info.IsDetached());
}

// Action A2.
void CTSE_ScopeInfo::RemoveAnnot(CSeq_annot_ScopeInfo& info)
{
    CMutexGuard guard(m_TSE_LockMutex);
    _ASSERT(info.IsAttached());
    CSeq_annot_Info& annot = info.GetNCObjectInfo();
    annot.GetParentBioseq_Base_Info().RemoveAnnot(Ref(&annot));
    x_SaveRemoved(info);
    _ASSERT(info.IsDetached());
}


// Action A7.
void CTSE_ScopeInfo::x_CheckAdded(CScopeInfo_Base& parent,
                                  CScopeInfo_Base& child)
{
    _ASSERT(parent.IsAttached());
    _ASSERT(parent.HasObject());
    _ASSERT(parent.m_LockCounter.Get() > 0);
    _ASSERT(child.IsDetached());
    _ASSERT(child.m_DetachedInfo);
    _ASSERT(child.HasObject());
    _ASSERT(!child.GetObjectInfo_Base().HasParent_Info());
    _ASSERT(child.m_LockCounter.Get() > 0);
    _ASSERT(x_SameTSE(parent.GetTSE_Handle().x_GetTSE_Info()));
}


// Action A7.
void CTSE_ScopeInfo::AddEntry(CBioseq_set_ScopeInfo& parent,
                              CSeq_entry_ScopeInfo& child,
                              int index)
{
    CMutexGuard guard(m_TSE_LockMutex);
    x_CheckAdded(parent, child);
    parent.GetNCObjectInfo().AddEntry(Ref(&child.GetNCObjectInfo()), index, true);
    x_RestoreAdded(parent, child);
    _ASSERT(child.IsAttached());
}


// Action A7.
void CTSE_ScopeInfo::AddAnnot(CSeq_entry_ScopeInfo& parent,
                              CSeq_annot_ScopeInfo& child)
{
    CMutexGuard guard(m_TSE_LockMutex);
    x_CheckAdded(parent, child);
    parent.GetNCObjectInfo().AddAnnot(Ref(&child.GetNCObjectInfo()));
    x_RestoreAdded(parent, child);
    _ASSERT(child.IsAttached());
}


// Action A7.
void CTSE_ScopeInfo::SelectSet(CSeq_entry_ScopeInfo& parent,
                               CBioseq_set_ScopeInfo& child)
{
    CMutexGuard guard(m_TSE_LockMutex);
    x_CheckAdded(parent, child);
    _ASSERT(parent.GetObjectInfo().Which() == CSeq_entry::e_not_set);
    parent.GetNCObjectInfo().SelectSet(child.GetNCObjectInfo());
    x_RestoreAdded(parent, child);
    _ASSERT(child.IsAttached());
}


// Action A7.
void CTSE_ScopeInfo::SelectSeq(CSeq_entry_ScopeInfo& parent,
                               CBioseq_ScopeInfo& child)
{
    CMutexGuard guard(m_TSE_LockMutex);
    x_CheckAdded(parent, child);
    _ASSERT(parent.GetObjectInfo().Which() == CSeq_entry::e_not_set);
    parent.GetNCObjectInfo().SelectSeq(child.GetNCObjectInfo());
    x_RestoreAdded(parent, child);
    _ASSERT(child.IsAttached());
}


// Save and restore scope info objects.

typedef pair<CConstRef<CTSE_Info_Object>,
             CRef<CScopeInfo_Base> > TDetachedInfoElement;
typedef vector<TDetachedInfoElement> TDetachedInfo;

// Action A3.
void CTSE_ScopeInfo::x_SaveRemoved(CScopeInfo_Base& info)
{
    _ASSERT(info.IsAttached()); // info is not yet detached
    _ASSERT(!info.m_DetachedInfo); // and doesn't contain m_DetachedInfo yet
    _ASSERT(info.HasObject()); // it contains pointer to removed object
    _ASSERT(!info.GetObjectInfo_Base().HasParent_Info()); //and is root of tree
    CRef<CObjectFor<TDetachedInfo> > save(new CObjectFor<TDetachedInfo>);
    _ASSERT(!m_UnloadedInfo); // this TSE cannot be unloaded
    _ASSERT(m_TSE_Lock); // and TSE is locked
    _TRACE("x_SaveRemoved("<<&info<<") TSE: "<<this);
    for ( TScopeInfoMap::iterator it = m_ScopeInfoMap.begin();
          it != m_ScopeInfoMap.end(); ) {
        if ( !it->first->BelongsToTSE_Info(*m_TSE_Lock) ) {
            _TRACE(" "<<it->second<<" " << it->first);
            it->second->m_TSE_Handle.Reset();
            it->second->x_DetachTSE(this);
            if ( &*it->second != &info ) {
                _ASSERT(it->first->HasParent_Info());
                save->GetData().push_back(TDetachedInfoElement(it->first,
                                                               it->second));
            }
            m_ScopeInfoMap.erase(it++);
        }
        else {
            ++it;
        }
    }
    _ASSERT(info.IsDetached()); // info is already detached
    _ASSERT(m_TSE_Lock);
    info.m_DetachedInfo.Reset(save); // save m_DetachedInfo
#ifdef _DEBUG
    ITERATE ( TBioseqById, it, m_BioseqById ) {
        _ASSERT(!it->second->IsDetached());
        _ASSERT(&it->second->x_GetTSE_ScopeInfo() == this);
        _ASSERT(!it->second->HasObject() || it->second->GetObjectInfo_Base().BelongsToTSE_Info(*m_TSE_Lock));
    }
#endif
    // post checks
    _ASSERT(info.IsDetached());
    _ASSERT(info.m_DetachedInfo);
    _ASSERT(info.HasObject()); // it contains pointer to removed object
    _ASSERT(!info.GetObjectInfo_Base().HasParent_Info());//and is root of tree
}

// Action A7
void CTSE_ScopeInfo::x_RestoreAdded(CScopeInfo_Base& parent,
                                    CScopeInfo_Base& child)
{
    _ASSERT(parent.IsAttached()); // parent is attached
    _ASSERT(parent.m_TSE_Handle); // and locked
    _ASSERT(parent.m_LockCounter.Get() > 0);
    _ASSERT(child.IsDetached()); // child is detached
    _ASSERT(child.m_DetachedInfo); // and contain m_DetachedInfo
    _ASSERT(child.HasObject()); // it contains pointer to removed object
    _ASSERT(child.GetObjectInfo_Base().HasParent_Info());//and is connected
    _ASSERT(child.m_LockCounter.Get() > 0);

    _TRACE("x_RestoreAdded("<<&child<<") TSE: "<<this);

    CRef<CObjectFor<TDetachedInfo> > infos
        (&dynamic_cast<CObjectFor<TDetachedInfo>&>(*child.m_DetachedInfo));
    child.m_DetachedInfo.Reset();
    infos->GetData().push_back
        (TDetachedInfoElement(ConstRef(&child.GetObjectInfo_Base()),
                              Ref(&child)));
    
    ITERATE ( TDetachedInfo, it, infos->GetData() ) {
        _TRACE(" "<<it->second<<" " << it->first);
        CScopeInfo_Base& info = it->second.GetNCObject();
        if ( info.m_LockCounter.Get() > 0 ) {
            info.x_AttachTSE(this);
            _VERIFY(m_ScopeInfoMap.insert
                    (TScopeInfoMap::value_type(it->first, it->second)).second);
            info.m_TSE_Handle = parent.m_TSE_Handle;
        }
    }
    _ASSERT(child.IsAttached());
    _ASSERT(child.m_TSE_Handle.m_TSE);
    _ASSERT(child.HasObject());
}


SSeqMatch_Scope CTSE_ScopeInfo::Resolve(const CSeq_id_Handle& id)
{
    return GetDSInfo().Resolve(id, *this);
}


/////////////////////////////////////////////////////////////////////////////
// CBioseq_ScopeInfo
/////////////////////////////////////////////////////////////////////////////

// If this define will be uncomented then it must be changed to use ERR_POST_X
//#define BIOSEQ_TRACE(x) ERR_POST(x)
#ifndef BIOSEQ_TRACE
# define BIOSEQ_TRACE(x)
#endif


CBioseq_ScopeInfo::CBioseq_ScopeInfo(TBlobStateFlags flags)
    : m_BlobState(flags)
{
    BIOSEQ_TRACE("CBioseq_ScopeInfo: "<<this);
}


CBioseq_ScopeInfo::CBioseq_ScopeInfo(CTSE_ScopeInfo& tse)
    : m_BlobState(CBioseq_Handle::fState_none)
{
    BIOSEQ_TRACE("CBioseq_ScopeInfo: "<<this);
    x_AttachTSE(&tse);
}


CBioseq_ScopeInfo::CBioseq_ScopeInfo(CTSE_ScopeInfo& tse,
                                     const TIds& ids)
    : m_Ids(ids),
      m_BlobState(CBioseq_Handle::fState_none)
{
    BIOSEQ_TRACE("CBioseq_ScopeInfo: "<<this);
    x_AttachTSE(&tse);
}


CBioseq_ScopeInfo::~CBioseq_ScopeInfo(void)
{
    if ( IsAttached() ) {
        BIOSEQ_TRACE("~CBioseq_ScopeInfo: "<<this<<
                     " TSE "<<&x_GetTSE_ScopeInfo());
    }
    else {
        BIOSEQ_TRACE("~CBioseq_ScopeInfo: "<<this);
    }
    _ASSERT(!IsAttached());
}


const CBioseq_ScopeInfo::TIndexIds* CBioseq_ScopeInfo::GetIndexIds(void) const
{
    const TIds& ids = GetIds();
    return ids.empty()? 0: &ids;
}


bool CBioseq_ScopeInfo::HasBioseq(void) const
{
    return (GetBlobState() & CBioseq_Handle::fState_no_data) == 0;
}


CBioseq_ScopeInfo::TBioseq_Lock
CBioseq_ScopeInfo::GetLock(CConstRef<CBioseq_Info> bioseq)
{
    return x_GetTSE_ScopeInfo().GetBioseqLock(Ref(this), bioseq);
}


void CBioseq_ScopeInfo::x_AttachTSE(CTSE_ScopeInfo* tse)
{
    BIOSEQ_TRACE("CBioseq_ScopeInfo: "<<this<<" x_AttachTSE "<<tse);
    CScopeInfo_Base::x_AttachTSE(tse);
    ITERATE ( TIds, it, GetIds() ) {
        tse->x_IndexBioseq(*it, this);
    }
}

void CBioseq_ScopeInfo::x_DetachTSE(CTSE_ScopeInfo* tse)
{
    BIOSEQ_TRACE("CBioseq_ScopeInfo: "<<this<<" x_DetachTSE "<<tse);
    m_SynCache.Reset();
    m_BioseqAnnotRef_Info.Reset();
    ITERATE ( TIds, it, GetIds() ) {
        tse->x_UnindexBioseq(*it, this);
    }
    CScopeInfo_Base::x_DetachTSE(tse);
    BIOSEQ_TRACE("CBioseq_ScopeInfo: "<<this<<" x_DetachTSE "<<tse<<" DONE");
}


void CBioseq_ScopeInfo::x_ForgetTSE(CTSE_ScopeInfo* tse)
{
    BIOSEQ_TRACE("CBioseq_ScopeInfo: "<<this<<" x_ForgetTSE "<<tse);
    m_SynCache.Reset();
    m_BioseqAnnotRef_Info.Reset();
    CScopeInfo_Base::x_ForgetTSE(tse);
    BIOSEQ_TRACE("CBioseq_ScopeInfo: "<<this<<" x_ForgetTSE "<<tse<<" DONE");
}


string CBioseq_ScopeInfo::IdString(void) const
{
    CNcbiOstrstream os;
    const TIds& ids = GetIds();
    ITERATE ( TIds, it, ids ) {
        if ( it != ids.begin() )
            os << " | ";
        os << it->AsString();
    }
    return CNcbiOstrstreamToString(os);
}


void CBioseq_ScopeInfo::ResetId(void)
{
    _ASSERT(HasObject());
    const_cast<CBioseq_Info&>(GetObjectInfo()).ResetId();
    ITERATE ( TIds, it, GetIds() ) {
        x_GetTSE_ScopeInfo().x_UnindexBioseq(*it, this);
    }
    m_Ids.clear();
}


bool CBioseq_ScopeInfo::AddId(const CSeq_id_Handle& id)
{
    _ASSERT(HasObject());
    CBioseq_Info& info = const_cast<CBioseq_Info&>(GetObjectInfo());
    if ( !info.AddId(id) ) {
        return false;
    }
    m_Ids.push_back(id);
    x_GetTSE_ScopeInfo().x_IndexBioseq(id, this);
    x_GetScopeImpl().x_ClearCacheOnNewData(info.GetTSE_Info(), id);
    return true;
}


bool CBioseq_ScopeInfo::RemoveId(const CSeq_id_Handle& id)
{
    _ASSERT(HasObject());
    if ( !const_cast<CBioseq_Info&>(GetObjectInfo()).RemoveId(id) ) {
        return false;
    }
    TIds::iterator it = find(m_Ids.begin(), m_Ids.end(), id);
    _ASSERT(it != m_Ids.end());
    m_Ids.erase(it);
    x_GetTSE_ScopeInfo().x_UnindexBioseq(id, this);
    return true;
}


/////////////////////////////////////////////////////////////////////////////
// SSeq_id_ScopeInfo
/////////////////////////////////////////////////////////////////////////////

SSeq_id_ScopeInfo::SSeq_id_ScopeInfo(void)
{
}

SSeq_id_ScopeInfo::~SSeq_id_ScopeInfo(void)
{
}

/////////////////////////////////////////////////////////////////////////////
// CSynonymsSet
/////////////////////////////////////////////////////////////////////////////

CSynonymsSet::CSynonymsSet(void)
{
}


CSynonymsSet::~CSynonymsSet(void)
{
}


CSeq_id_Handle CSynonymsSet::GetSeq_id_Handle(const const_iterator& iter)
{
    return (*iter)->first;
}


CBioseq_Handle CSynonymsSet::GetBioseqHandle(const const_iterator& iter)
{
    return CBioseq_Handle((*iter)->first, *(*iter)->second.m_Bioseq_Info);
}


bool CSynonymsSet::ContainsSynonym(const CSeq_id_Handle& id) const
{
   ITERATE ( TIdSet, iter, m_IdSet ) {
        if ( (*iter)->first == id ) {
            return true;
        }
    }
    return false;
}


void CSynonymsSet::AddSynonym(const value_type& syn)
{
    _ASSERT(!ContainsSynonym(syn->first));
    m_IdSet.push_back(syn);
}


END_SCOPE(objects)
END_NCBI_SCOPE
