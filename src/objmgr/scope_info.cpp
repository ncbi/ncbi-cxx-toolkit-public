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

#include <corelib/ncbi_config_value.hpp>
#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

#if 0
# define _TRACE_TSE_LOCK(x) _TRACE(x)
#else
# define _TRACE_TSE_LOCK(x) ((void)0)
#endif


static bool s_GetScopeAutoReleaseEnabled(void)
{
    static int sx_Value = -1;
    int value = sx_Value;
    if ( value < 0 ) {
        value = GetConfigFlag("OBJMGR", "SCOPE_AUTORELEASE", true);
        sx_Value = value;
    }
    return value != 0;
}


static unsigned s_GetScopeAutoReleaseSize(void)
{
    static unsigned sx_Value = kMax_UInt;
    unsigned value = sx_Value;
    if ( value == kMax_UInt ) {
        value = GetConfigInt("OBJMGR", "SCOPE_AUTORELEASE_SIZE", 10);
        if ( value == kMax_UInt ) {
            --value;
        }
        sx_Value = value;
    }
    return value;
}


/////////////////////////////////////////////////////////////////////////////
// CDataSource_ScopeInfo
/////////////////////////////////////////////////////////////////////////////

CDataSource_ScopeInfo::CDataSource_ScopeInfo(CScope_Impl& scope,
                                             CDataSource& ds,
                                             bool shared)
    : m_Scope(&scope),
      m_DataSource(&ds),
      m_CanBeUnloaded(s_GetScopeAutoReleaseEnabled() &&
                      ds.GetDataLoader() &&
                      ds.GetDataLoader()->CanGetBlobById()),
      m_Shared(shared),
      m_NextTSEIndex(0),
      m_TSE_UnlockQueue(s_GetScopeAutoReleaseSize())
{
}


CDataSource_ScopeInfo::~CDataSource_ScopeInfo(void)
{
    Reset();
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


void CDataSource_ScopeInfo::DetachFromOM(CObjectManager& om)
{
    if ( m_DataSource ) {
        Reset();
        m_Scope = 0;
        om.ReleaseDataSource(m_DataSource);
        _ASSERT(!m_DataSource);
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


CDataSource_ScopeInfo::TTSE_Lock
CDataSource_ScopeInfo::GetTSE_Lock(const CTSE_Lock& lock)
{
    _ASSERT(lock);
    TTSE_ScopeInfo info;
    {{
        TTSE_InfoMapMutex::TWriteLockGuard guard(m_TSE_InfoMapMutex);
        STSE_Key key(*lock, m_CanBeUnloaded);
        TTSE_ScopeInfo& slot = m_TSE_InfoMap[key];
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
        UpdateTSELock(*info, lock);
        info->m_TSE_LockCounter.Add(-1);
        _ASSERT(info->m_TSE_Lock == lock);
    }}
    return CTSE_ScopeUserLock(info);
}


void CDataSource_ScopeInfo::x_IndexTSE(CTSE_ScopeInfo& tse)
{
    CTSE_ScopeInfo::TBlobOrder order = tse.GetBlobOrder();
    ITERATE ( CTSE_ScopeInfo::TBioseqsIds, it, tse.GetBioseqsIds() ) {
        m_TSE_BySeqId.insert(TTSE_BySeqId::value_type(*it, Ref(&tse)));
    }
}


void CDataSource_ScopeInfo::x_UnindexTSE(const CTSE_ScopeInfo& tse)
{
    CTSE_ScopeInfo::TBlobOrder order = tse.GetBlobOrder();
    ITERATE ( CTSE_ScopeInfo::TBioseqsIds, it, tse.GetBioseqsIds() ) {
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


void CDataSource_ScopeInfo::UpdateTSELock(CTSE_ScopeInfo& tse,
                                          const CTSE_Lock& lock)
{
    TTSE_LockSetMutex::TWriteLockGuard guard(m_TSE_LockSetMutex);
    _ASSERT(tse.m_TSE_LockCounter.Get() > 0);
    m_TSE_UnlockQueue.Get(&tse);
    _ASSERT(tse.m_TSE_LockCounter.Get() > 0);
    if ( !tse.m_TSE_Lock ) {
        if ( lock ) {
            tse.m_TSE_Lock = lock;
        }
        else {
            tse.m_TSE_Lock = tse.m_UnloadedInfo->LockTSE();
        }
        _ASSERT(tse.m_TSE_Lock);
        _VERIFY(m_TSE_LockSet.AddLock(tse.m_TSE_Lock));
    }
    _ASSERT(tse.m_TSE_LockCounter.Get() > 0);
    _ASSERT(tse.m_TSE_Lock);
}


void CDataSource_ScopeInfo::ReleaseTSELock(CTSE_ScopeInfo& tse)
{
    TTSE_LockSetMutex::TWriteLockGuard guard(m_TSE_LockSetMutex);
    if ( tse.m_TSE_LockCounter.Get() > 0 ) {
        // relocked already
        return;
    }
    if ( !tse.m_TSE_Lock ) {
        // already unlocked
        return;
    }
    m_TSE_UnlockQueue.Put(&tse, CTSE_ScopeInternalLock(&tse));
}


void CDataSource_ScopeInfo::ForgetTSELock(CTSE_ScopeInfo& tse)
{
    TTSE_LockSetMutex::TWriteLockGuard guard(m_TSE_LockSetMutex);
    if ( tse.m_TSE_LockCounter.Get() > 0 ) {
        // relocked already
        return;
    }
    if ( !tse.m_TSE_Lock ) {
        // already unlocked
        return;
    }
    _VERIFY(m_TSE_LockSet.RemoveLock(tse.m_TSE_Lock));
    tse.x_ForgetLocks();
}


void CDataSource_ScopeInfo::RemoveFromHistory(CTSE_ScopeInfo& tse)
{
    TTSE_InfoMapMutex::TWriteLockGuard guard1(m_TSE_InfoMapMutex);
    TTSE_LockSetMutex::TWriteLockGuard guard2(m_TSE_LockSetMutex);
    {{ // remove TSE lock completely
        m_TSE_UnlockQueue.Get(&tse);
    }}
    if ( tse.CanBeUnloaded() ) {
        x_UnindexTSE(tse);
    }
    _VERIFY(m_TSE_InfoMap.erase(tse));
    tse.x_ForgetLocks();
    tse.m_DS_Info = 0;
    _ASSERT(!tse.m_TSE_Lock);
}


void CDataSource_ScopeInfo::Reset(void)
{
    TTSE_InfoMapMutex::TWriteLockGuard guard1(m_TSE_InfoMapMutex);
    TTSE_LockSetMutex::TWriteLockGuard guard2(m_TSE_LockSetMutex);
    m_TSE_UnlockQueue.Clear();
    NON_CONST_ITERATE ( TTSE_InfoMap, it, m_TSE_InfoMap ) {
        it->second->x_ForgetLocks();
    }
    m_TSE_InfoMap.clear();
    m_TSE_BySeqId.clear();
    m_TSE_LockSet.clear();
}


CDataSource_ScopeInfo::TTSE_Lock
CDataSource_ScopeInfo::FindTSE_Lock(const CSeq_entry& tse)
{
    CDataSource::TTSE_Lock lock;
    {{
        TTSE_LockSetMutex::TReadLockGuard guard(GetTSE_LockSetMutex());
        lock = GetDataSource().FindTSE_Lock(tse, GetTSE_LockSet());
    }}
    if ( lock ) {
        return GetTSE_Lock(lock);
    }
    return TTSE_Lock();
}


CDataSource_ScopeInfo::TSeq_entry_Lock
CDataSource_ScopeInfo::FindSeq_entry_Lock(const CSeq_entry& entry)
{
    CDataSource::TSeq_entry_Lock lock;
    {{
        TTSE_LockSetMutex::TReadLockGuard guard(GetTSE_LockSetMutex());
        lock = GetDataSource().FindSeq_entry_Lock(entry, GetTSE_LockSet());
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
        TTSE_LockSetMutex::TReadLockGuard guard(GetTSE_LockSetMutex());
        lock = GetDataSource().FindSeq_annot_Lock(annot, GetTSE_LockSet());
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
        TTSE_LockSetMutex::TReadLockGuard guard(GetTSE_LockSetMutex());
        lock = GetDataSource().FindBioseq_set_Lock(seqset, GetTSE_LockSet());
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
        TTSE_LockSetMutex::TReadLockGuard guard(GetTSE_LockSetMutex());
        lock = GetDataSource().FindBioseq_Lock(bioseq, GetTSE_LockSet());
    }}
    if ( lock.first ) {
        return GetTSE_Lock(lock.second)->GetBioseqLock(null, lock.first);
    }
    return TBioseq_Lock();
}


CDataSource_ScopeInfo::STSE_Key::STSE_Key(const CTSE_Info& tse,
                                          bool can_be_unloaded)
{
    if ( can_be_unloaded ) {
        m_Loader = tse.GetDataSource().GetDataLoader();
        _ASSERT(m_Loader);
        m_BlobId = tse.GetBlobId();
        _ASSERT(m_BlobId);
        _ASSERT(m_BlobId.GetPointer() != &tse);
    }
    else {
        m_Loader = 0;
        m_BlobId = &tse;
    }
}


CDataSource_ScopeInfo::STSE_Key::STSE_Key(const CTSE_ScopeInfo& tse)
{
    if ( tse.CanBeUnloaded() ) {
        m_Loader = tse.GetDSInfo().GetDataSource().GetDataLoader();
        _ASSERT(m_Loader);
    }
    else {
        m_Loader = 0;
    }
    m_BlobId = tse.GetBlobId();
    _ASSERT(m_BlobId);
}


bool CDataSource_ScopeInfo::STSE_Key::operator<(const STSE_Key& tse2) const
{
    _ASSERT(m_Loader == tse2.m_Loader);
    if ( m_Loader ) {
        return m_Loader->LessBlobId(m_BlobId, tse2.m_BlobId);
    }
    else {
        return m_BlobId < tse2.m_BlobId;
    }
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


SSeqMatch_Scope CDataSource_ScopeInfo::x_GetSeqMatch(const CSeq_id_Handle& idh)
{
    SSeqMatch_Scope ret = x_FindBestTSE(idh);
    if ( !ret && idh.HaveMatchingHandles() ) {
        CSeq_id_Handle::TMatches ids;
        idh.GetMatchingHandles(ids);
        ITERATE ( CSeq_id_Handle::TMatches, it, ids ) {
            if ( *it == idh ) // already checked
                continue;
            if ( ret && ret.m_Seq_id.IsBetter(*it) ) // worse hit
                continue;
            ret = x_FindBestTSE(*it);
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
            TTSE_LockSetMutex::TReadLockGuard guard(GetTSE_LockSetMutex());
            CDataSource::TSeqMatches matches2 =
                GetDataSource().GetMatches(idh, GetTSE_LockSet());
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
    match.m_Bioseq = match.m_TSE_Lock->m_TSE_Lock->FindBioseq(idh);
    _ASSERT(match.m_Bioseq);
}


void CDataSource_ScopeInfo::x_SetMatch(SSeqMatch_Scope& match,
                                       const SSeqMatch_DS& ds_match)
{
    match.m_Seq_id = ds_match.m_Seq_id;
    match.m_TSE_Lock = GetTSE_Lock(ds_match.m_TSE_Lock);
    match.m_Bioseq = ds_match.m_Bioseq;
    _ASSERT(match.m_Bioseq);
    _ASSERT(match.m_Bioseq == match.m_TSE_Lock->m_TSE_Lock->FindBioseq(match.m_Seq_id));
}


bool CDataSource_ScopeInfo::TSEIsInQueue(const CTSE_ScopeInfo& tse) const
{
    TTSE_LockSetMutex::TReadLockGuard guard(m_TSE_LockSetMutex);
    return m_TSE_UnlockQueue.Contains(&tse);
}


/////////////////////////////////////////////////////////////////////////////
// CTSE_ScopeLocker
/////////////////////////////////////////////////////////////////////////////

void CTSE_ScopeLocker::Lock(CTSE_ScopeInfo* tse) const
{
    _TRACE_TSE_LOCK("CTSE_ScopeLocker("<<this<<") "<<tse<<" lock");
    CObjectCounterLocker::Lock(tse);
    tse->x_LockTSE();
}


void CTSE_ScopeInternalLocker::Unlock(CTSE_ScopeInfo* tse) const
{
    _TRACE_TSE_LOCK("CTSE_ScopeInternalLocker("<<this<<") "<<tse<<" unlock");
    tse->x_InternalUnlockTSE();
    CObjectCounterLocker::Unlock(tse);
}


void CTSE_ScopeUserLocker::Unlock(CTSE_ScopeInfo* tse) const
{
    _TRACE_TSE_LOCK("CTSE_ScopeUserLocker("<<this<<") "<<tse<<" unlock");
    tse->x_UserUnlockTSE();
    CObjectCounterLocker::Unlock(tse);
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
                               const CTSE_Lock& tse_lock,
                               int load_index,
                               bool can_be_unloaded)
    : m_DS_Info(&ds_info),
      m_LoadIndex(load_index),
      m_UsedByTSE(0)
{
    m_TSE_LockCounter.Set(0);
    if ( can_be_unloaded ) {
        _ASSERT(tse_lock->GetBlobId());
        m_UnloadedInfo.reset(new SUnloadedInfo(tse_lock));
    }
    else {
        // permanent lock
        _TRACE_TSE_LOCK("CTSE_ScopeInfo("<<this<<") perm lock");
        m_TSE_LockCounter.Add(1);
        _ASSERT(!m_TSE_Lock);
        GetDSInfo().UpdateTSELock(*this, tse_lock);
        _ASSERT(m_TSE_Lock == tse_lock);
    }
}


CTSE_ScopeInfo::~CTSE_ScopeInfo(void)
{
    if ( !CanBeUnloaded() ) {
        // remove permanent lock
        _TRACE_TSE_LOCK("CTSE_ScopeInfo("<<this<<") perm unlock: "<<m_TSE_LockCounter.Get());
        _VERIFY(m_TSE_LockCounter.Add(-1) == 0);
    }
    x_ForgetLocks();
    _TRACE_TSE_LOCK("CTSE_ScopeInfo("<<this<<") final: "<<m_TSE_LockCounter.Get());
    _ASSERT(m_TSE_LockCounter.Get() == 0);
    _ASSERT(!m_TSE_Lock);
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
        return TBlobId(&*m_TSE_Lock);
    }
}


const CTSE_ScopeInfo::TBioseqsIds& CTSE_ScopeInfo::GetBioseqsIds(void) const
{
    _ASSERT(CanBeUnloaded());
    return m_UnloadedInfo->m_BioseqsIds;
}


void CTSE_ScopeInfo::x_LockTSE(void)
{
    m_TSE_LockCounter.Add(1);
    if ( !m_TSE_Lock ) {
        GetDSInfo().UpdateTSELock(*this, CTSE_Lock());
    }
    _ASSERT(m_TSE_Lock);
}

/*
void CTSE_ScopeInfo::x_LockTSE(const CTSE_Lock& lock)
{
    m_TSE_LockCounter.Add(1);
    if ( !m_TSE_Lock ) {
        GetDSInfo().UpdateTSELock(*this, lock);
    }
    _ASSERT(m_TSE_Lock);
}
*/

void CTSE_ScopeInfo::x_UserUnlockTSE(void)
{
    if ( m_TSE_LockCounter.Add(-1) == 0 ) {
        _ASSERT(CanBeUnloaded());
        GetDSInfo().ReleaseTSELock(*this);
    }
}


void CTSE_ScopeInfo::x_InternalUnlockTSE(void)
{
    if ( m_TSE_LockCounter.Add(-1) == 0 ) {
        _ASSERT(CanBeUnloaded());
        GetDSInfo().ForgetTSELock(*this);
    }
}

/*
void CTSE_ScopeInfo::x_ReleaseTSE(void)
{
    _ASSERT( !LockedMoreThanOnce() );
    m_TSE_LockCounter.Add(-1);
    if ( CanBeUnloaded() ) {
        x_ForgetLocks();
        _ASSERT(!m_TSE_Lock);
    }
}
*/

bool CTSE_ScopeInfo::x_SameTSE(const CTSE_Info& tse) const
{
    return m_TSE_LockCounter.Get() > 0 && m_TSE_Lock && &*m_TSE_Lock == &tse;
}


bool CTSE_ScopeInfo::AddUsedTSE(const CTSE_ScopeUserLock& used_tse) const
{
    CTSE_ScopeInfo& add_info = const_cast<CTSE_ScopeInfo&>(*used_tse);
    if ( m_TSE_LockCounter.Get() == 0 || // this one is unlocked
         &add_info == this || // the same TSE
         !add_info.CanBeUnloaded() || // permanentrly locked
         &add_info.GetDSInfo() != &GetDSInfo() || // another data source
         add_info.m_UsedByTSE ) { // already used
        return false;
    }
    CDataSource_ScopeInfo::TTSE_LockSetMutex::TWriteLockGuard
        guard(GetDSInfo().GetTSE_LockSetMutex());
    if ( m_TSE_LockCounter.Get() == 0 || // this one is unlocked
         add_info.m_UsedByTSE ) { // already used
        return false;
    }
    // check if used TSE uses this TSE indirectly
    for ( const CTSE_ScopeInfo* p = m_UsedByTSE; p; p = p->m_UsedByTSE ) {
        _ASSERT(&p->GetDSInfo() == &GetDSInfo());
        if ( p == &add_info ) {
            return false;
        }
    }
    add_info.m_UsedByTSE = this;
    _VERIFY(m_UsedTSE_Set.insert(CTSE_ScopeInternalLock(&add_info)).second);
    return true;
}


void CTSE_ScopeInfo::x_ForgetLocks(void)
{
    CMutexGuard guard(m_TSE_LockMutex);
    /*
    NON_CONST_ITERATE ( TBioseqById, it, m_BioseqById ) {
        it->second->m_SynCache.Reset();
        it->second->m_BioseqAnnotRef_Info.Reset();
    }
    */
    ITERATE ( TUsedTSE_LockSet, it, m_UsedTSE_Set ) {
        _ASSERT((*it)->m_UsedByTSE == this);
        (*it)->m_UsedByTSE = 0;
    }
    NON_CONST_ITERATE ( TScopeInfoMap, it, m_ScopeInfoMap ) {
        it->second->x_ResetLock();
        if ( it->second->IsTemporary() ) {
            it->second->x_DetachTSE(this);
        }
    }
    m_ScopeInfoMap.clear();
    m_UsedTSE_Set.clear();
    m_TSE_Lock.Reset();
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
    return m_TSE_LockCounter.Get() > x_GetDSLocksCount();
}


bool CTSE_ScopeInfo::LockedMoreThanOnce(void) const
{
    return m_TSE_LockCounter.Get() > x_GetDSLocksCount() + 1;
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


bool CTSE_ScopeInfo::ContainsMatchingBioseq(const CSeq_id_Handle& id) const
{
    if ( CanBeUnloaded() ) {
        if ( ContainsBioseq(id) ) {
            return true;
        }
        if ( id.HaveMatchingHandles() ) {
            CSeq_id_Handle::TMatches ids;
            id.GetMatchingHandles(ids);
            ITERATE ( CSeq_id_Handle::TMatches, it, ids ) {
                if ( *it != id ) {
                    if ( ContainsBioseq(*it) ) {
                        return true;
                    }
                }
            }
        }
        return false;
    }
    else {
        return m_TSE_Lock->ContainsMatchingBioseq(id);
    }
}


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
    }
    else {
        scope_info = &dynamic_cast<CSeq_entry_ScopeInfo&>(*iter->second);
    }
    if ( !scope_info->m_ObjectInfo ) {
        scope_info->x_SetLock(tse.m_TSE, info);
    }
    return TSeq_entry_Lock(*scope_info);
}


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
    }
    else {
        scope_info = &dynamic_cast<CSeq_annot_ScopeInfo&>(*iter->second);
    }
    if ( !scope_info->m_ObjectInfo ) {
        scope_info->x_SetLock(tse.m_TSE, info);
    }
    return TSeq_annot_Lock(*scope_info);
}


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
    }
    else {
        scope_info = &dynamic_cast<CBioseq_set_ScopeInfo&>(*iter->second);
    }
    if ( !scope_info->m_ObjectInfo ) {
        scope_info->x_SetLock(tse.m_TSE, info);
    }
    return TBioseq_set_Lock(*scope_info);
}


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
            }
            else {
                info.Reset(&dynamic_cast<CBioseq_ScopeInfo&>(*iter->second));
            }
            if ( !info->HasObject() ) {
                info->x_SetLock(tse, *bioseq);
            }
            return TBioseq_Lock(*info);
        }
    }
    _ASSERT(info);
    _ASSERT(!info->IsRemoved());
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
        info->x_SetLock(tse, *bioseq);
    }
    _ASSERT(info->IsValid());
    _ASSERT(info->GetObjectInfo().BelongsToTSE_Info(*m_TSE_Lock));
    _ASSERT(m_ScopeInfoMap.find(TScopeInfoMapKey(&info->GetObjectInfo()))->second == info);
    return TBioseq_Lock(*info);
}


void CTSE_ScopeInfo::RemoveLastInfoLock(CScopeInfo_Base& info)
{
    if ( !info.m_ObjectInfo ) {
        return;
    }
    CRef<CTSE_ScopeInfo> self; // We keep CRef<> to this CTSE_ScopeInfo object
    // to prevent its deletion if x_ResetLock() will cause deletion of scope.
    // We have to save CRef pointer from within guarded code,
    // but it should be released after the guard.

    CMutexGuard guard(m_TSE_LockMutex);
    self = this;
    if ( !info.m_ObjectInfo || info.m_LockCounter.Get() != 0 ) {
        // already unlocked, or the lock was reaquired in another thread
        return;
    }
    _ASSERT(info.m_LockCounter.Get() == 0);
    TScopeInfoMapKey key
        (&static_cast<const CTSE_Info_Object&>(info.GetObjectInfo_Base()));
    _ASSERT(m_ScopeInfoMap.find(key) != m_ScopeInfoMap.end());
    _ASSERT(m_ScopeInfoMap.find(key)->second == &info);
    _VERIFY(m_ScopeInfoMap.erase(key));
    info.x_ResetLock();
    if ( info.IsTemporary() ) {
        info.x_DetachTSE(this);
    }
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
CTSE_ScopeInfo::x_FindBioseqInfo(const TBioseqsIds& ids) const
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


CRef<CBioseq_ScopeInfo>
CTSE_ScopeInfo::x_CreateBioseqInfo(const TBioseqsIds& ids)
{
    // We'll create new CBioseq_ScopeInfo object
    // and remember it in m_BioseqById map.
    CRef<CBioseq_ScopeInfo> info(new CBioseq_ScopeInfo(*this, ids));

    ITERATE ( TBioseqsIds, it, ids ) {
        m_BioseqById.insert(TBioseqById::value_type(*it, info));
    }

    return info;
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


void CTSE_ScopeInfo::ResetEntry(const CSeq_entry_ScopeInfo& info)
{
    CSeq_entry_Info& entry =
        const_cast<CSeq_entry_Info&>(info.GetObjectInfo());
    CMutexGuard guard(m_TSE_LockMutex);
    entry.Reset();
    x_CleanRemovedObjects();
}


void CTSE_ScopeInfo::RemoveEntry(const CSeq_entry_ScopeInfo& info)
{
    CSeq_entry_Info& entry =
        const_cast<CSeq_entry_Info&>(info.GetObjectInfo());
    CMutexGuard guard(m_TSE_LockMutex);
    entry.GetParentBioseq_set_Info().RemoveEntry(Ref(&entry));
    x_CleanRemovedObjects();
}


void CTSE_ScopeInfo::RemoveAnnot(const CSeq_annot_ScopeInfo& info)
{
    CSeq_annot_Info& annot =
        const_cast<CSeq_annot_Info&>(info.GetObjectInfo());
    CMutexGuard guard(m_TSE_LockMutex);
    annot.GetParentBioseq_Base_Info().RemoveAnnot(Ref(&annot));
    x_CleanRemovedObjects();
}


void CTSE_ScopeInfo::x_CleanRemovedObjects(void)
{
    _ASSERT(!m_UnloadedInfo);
    _ASSERT(m_TSE_Lock);
    for ( TScopeInfoMap::iterator it = m_ScopeInfoMap.begin();
          it != m_ScopeInfoMap.end(); ) {
        if ( !it->first->BelongsToTSE_Info(*m_TSE_Lock) ) {
            it->second->x_ResetLock();
            const CBioseq_ScopeInfo* info =
                dynamic_cast<const CBioseq_ScopeInfo*>(&*it->second);
            if ( info ) {
                const CBioseq_ScopeInfo::TIds& ids = info->GetIds();
                ITERATE ( CBioseq_ScopeInfo::TIds, it2, ids ) {
                    x_UnindexBioseq(*it2, info);
                }
            }
            it->second->x_DetachTSE(this);
            m_ScopeInfoMap.erase(it++);
        }
        else {
            ++it;
        }
    }
#ifdef _DEBUG
    ITERATE ( TBioseqById, it, m_BioseqById ) {
        _ASSERT(!it->second->IsRemoved());
        _ASSERT(&it->second->x_GetTSE_ScopeInfo() == this);
        _ASSERT(!it->second->m_ObjectInfo || dynamic_cast<const CTSE_Info_Object&>(*it->second->m_ObjectInfo).BelongsToTSE_Info(*m_TSE_Lock));
    }
#endif
}


/////////////////////////////////////////////////////////////////////////////
// CBioseq_ScopeInfo
/////////////////////////////////////////////////////////////////////////////


CBioseq_ScopeInfo::CBioseq_ScopeInfo(TBlobStateFlags flags)
    : m_BlobState(flags)
{
}


CBioseq_ScopeInfo::CBioseq_ScopeInfo(CTSE_ScopeInfo& tse)
    : m_BlobState(CBioseq_Handle::fState_no_data)
{
    x_AttachTSE(&tse);
}


CBioseq_ScopeInfo::CBioseq_ScopeInfo(CTSE_ScopeInfo& tse,
                                     const TIds& ids)
    : m_Ids(ids),
      m_BlobState(CBioseq_Handle::fState_none)
{
    x_AttachTSE(&tse);
}


CBioseq_ScopeInfo::~CBioseq_ScopeInfo(void)
{
}


const CBioseq_ScopeInfo::TIndexIds* CBioseq_ScopeInfo::GetIndexIds(void) const
{
    const TIds& ids = GetIds();
    return ids.empty()? 0: &ids;
}


CBioseq_ScopeInfo::TBioseq_Lock
CBioseq_ScopeInfo::GetLock(CConstRef<CBioseq_Info> bioseq)
{
    return x_GetTSE_ScopeInfo().GetBioseqLock(Ref(this), bioseq);
}

/*
void CBioseq_ScopeInfo::x_ResetLock(void)
{
    m_SynCache.Reset();
    m_BioseqAnnotRef_Info.Reset();
    CScopeInfo_Base::x_ResetLock();
}
*/

void CBioseq_ScopeInfo::x_DetachTSE(CTSE_ScopeInfo* tse)
{
    m_SynCache.Reset();
    m_BioseqAnnotRef_Info.Reset();
    CScopeInfo_Base::x_DetachTSE(tse);
}


void CBioseq_ScopeInfo::x_ForgetTSE(CTSE_ScopeInfo* tse)
{
    m_SynCache.Reset();
    m_BioseqAnnotRef_Info.Reset();
    CScopeInfo_Base::x_ForgetTSE(tse);
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
    _ASSERT(IsValid());
    const_cast<CBioseq_Info&>(GetObjectInfo()).ResetId();
    ITERATE ( TIds, it, GetIds() ) {
        x_GetTSE_ScopeInfo().x_UnindexBioseq(*it, this);
    }
    m_Ids.clear();
}


bool CBioseq_ScopeInfo::AddId(const CSeq_id_Handle& id)
{
    _ASSERT(IsValid());
    if ( !const_cast<CBioseq_Info&>(GetObjectInfo()).AddId(id) ) {
        return false;
    }
    m_Ids.push_back(id);
    x_GetTSE_ScopeInfo().x_IndexBioseq(id, this);
    return true;
}


bool CBioseq_ScopeInfo::RemoveId(const CSeq_id_Handle& id)
{
    _ASSERT(IsValid());
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

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.19  2005/06/27 18:17:04  vasilche
* Allow getting CBioseq_set_Handle from CBioseq_set.
*
* Revision 1.18  2005/06/24 19:14:13  vasilche
* Disabled excessive _TRACE messages.
*
* Revision 1.17  2005/06/22 14:27:31  vasilche
* Implemented copying of shared Seq-entries at edit request.
* Added invalidation of handles to removed objects.
*
* Revision 1.16  2005/04/20 15:45:36  vasilche
* Fixed removal of TSE from scope's history.
*
* Revision 1.15  2005/03/15 19:14:27  vasilche
* Correctly update and check  bioseq ids in split blobs.
*
* Revision 1.14  2005/03/14 18:17:15  grichenk
* Added CScope::RemoveFromHistory(), CScope::RemoveTopLevelSeqEntry() and
* CScope::RemoveDataLoader(). Added requested seq-id information to
* CTSE_Info.
*
* Revision 1.13  2005/03/14 17:05:56  vasilche
* Thread safe retrieval of configuration variables.
*
* Revision 1.12  2005/01/05 18:45:57  vasilche
* Added GetConfigXxx() functions.
*
* Revision 1.11  2004/12/28 18:39:46  vasilche
* Added lost scope lock in CTSE_Handle.
*
* Revision 1.10  2004/12/22 15:56:26  vasilche
* Used deep copying of CPriorityTree in AddScope().
* Implemented auto-release of unused TSEs in scope.
* Introduced CTSE_Handle.
* Fixed annotation collection.
* Removed too long cvs log.
*
* Revision 1.9  2004/10/25 16:53:32  vasilche
* Added suppord for orphan annotations.
*
* Revision 1.8  2004/08/04 14:53:26  vasilche
* Revamped object manager:
* 1. Changed TSE locking scheme
* 2. TSE cache is maintained by CDataSource.
* 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
* 4. Fixed processing of split data.
*
* Revision 1.7  2004/05/21 21:42:12  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.6  2003/11/17 16:03:13  grichenk
* Throw exception in CBioseq_Handle if the parent scope has been reset
*
* Revision 1.5  2003/09/30 16:22:03  vasilche
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
* Revision 1.4  2003/06/19 19:48:16  vasilche
* Removed unnecessary copy constructor of SSeq_id_ScopeInfo.
*
* Revision 1.3  2003/06/19 19:31:23  vasilche
* Added missing CBioseq_ScopeInfo destructor.
*
* Revision 1.2  2003/06/19 19:08:55  vasilche
* Added explicit constructor/destructor.
*
* Revision 1.1  2003/06/19 18:23:46  vasilche
* Added several CXxx_ScopeInfo classes for CScope related information.
* CBioseq_Handle now uses reference to CBioseq_ScopeInfo.
* Some fine tuning of locking in CScope.
*
*
* ===========================================================================
*/
