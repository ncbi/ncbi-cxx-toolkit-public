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

#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/synonyms.hpp>
#include <objmgr/impl/data_source.hpp>

#include <objmgr/bioseq_handle.hpp>
#include <objmgr/tse_handle.hpp>

#include <corelib/ncbi_config_value.hpp>
#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


static unsigned s_GetScopeAutoReleaseSize(void)
{
    static unsigned sx_Value = kMax_UInt;
    unsigned value = sx_Value;
    if ( value == kMax_UInt ) {
        if ( GetConfigFlag("OBJMGR", "SCOPE_AUTORELEASE", true) ) {
            value = GetConfigInt("OBJMGR", "SCOPE_AUTORELEASE_SIZE", 10);
            if ( value == kMax_UInt ) {
                --value;
            }
        }
        else {
            value = 0;
        }
        sx_Value = value;
    }
    return value;
}


/////////////////////////////////////////////////////////////////////////////
// CDataSource_ScopeInfo
/////////////////////////////////////////////////////////////////////////////

CDataSource_ScopeInfo::CDataSource_ScopeInfo(CScope_Impl& scope,
                                             CDataSource& ds)
    : m_Scope(&scope),
      m_DataSource(&ds),
      m_CanBeUnloaded(s_GetScopeAutoReleaseSize() &&
                      ds.GetDataLoader() &&
                      ds.GetDataLoader()->CanGetBlobById()),
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
    }}
    return CTSE_ScopeUserLock(*info, lock);
}


void CDataSource_ScopeInfo::x_IndexTSE(CTSE_ScopeInfo& tse)
{
    CTSE_ScopeInfo::TBlobOrder order = tse.GetBlobOrder();
    const CTSE_ScopeInfo::TBioseqsIds& ids = tse.GetBioseqsIds();
    ITERATE ( CTSE_ScopeInfo::TBioseqsIds, it, ids ) {
        TTSE_BySeqId::iterator p = m_TSE_BySeqId.lower_bound(*it);
        if ( p != m_TSE_BySeqId.end() && p->first != *it ) {
            // There are old TSEs with this Seq-id.
            if ( order > p->second->GetBlobOrder() ) {
                // New TSE is worse than old ones.
                // Skip this Seq-id.
                continue;
            }
            
            // New TSE is ok to insert.
            if ( order < p->second->GetBlobOrder() ) {
                // New TSE is better than all old TSEs.
                // Remove old ones from index.
                while ( p != m_TSE_BySeqId.end() && p->first == *it ) {
                    TTSE_BySeqId::iterator to_erase = p++;
                    m_TSE_BySeqId.erase(to_erase);
                }
            }
        }
        // insert new
        m_TSE_BySeqId.insert(p, TTSE_BySeqId::value_type(*it, Ref(&tse)));
    }
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
    m_TSE_UnlockQueue.Put(&tse, CTSE_ScopeInternalLock(tse));
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


void CDataSource_ScopeInfo::Reset(void)
{
    {{
        TTSE_LockSetMutex::TWriteLockGuard guard2(m_TSE_LockSetMutex);
        m_TSE_UnlockQueue.Clear();
    }}
    {{
        TTSE_InfoMapMutex::TWriteLockGuard guard1(m_TSE_InfoMapMutex);
        m_TSE_InfoMap.clear();
        m_TSE_BySeqId.clear();
    }}
    {{
        TTSE_LockSetMutex::TWriteLockGuard guard2(m_TSE_LockSetMutex);
        m_TSE_LockSet.Clear();
    }}
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


CDataSource_ScopeInfo::TBioseq_Lock
CDataSource_ScopeInfo::FindBioseq_Lock(const CBioseq& bioseq)
{
    CDataSource::TBioseq_Lock lock;
    {{
        TTSE_LockSetMutex::TReadLockGuard guard(GetTSE_LockSetMutex());
        lock = GetDataSource().FindBioseq_Lock(bioseq, GetTSE_LockSet());
    }}
    if ( lock.first ) {
        return TBioseq_Lock(lock.first, GetTSE_Lock(lock.second));
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
        _ASSERT(ret.m_Bioseq == ret.m_TSE_Lock.GetTSE_Lock()->FindBioseq(ret.m_Seq_id));
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
        TTSE_ScopeInfo tse;
        TTSE_InfoMapMutex::TReadLockGuard guard(GetTSE_InfoMapMutex());
        for ( TTSE_BySeqId::const_iterator it = m_TSE_BySeqId.lower_bound(idh);
              it != m_TSE_BySeqId.end() && it->first == idh; ++it ) {
            if ( !tse || x_IsBetter(idh, *it->second, *tse) ) {
                tse = it->second;
            }
        }
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
    match.m_TSE_Lock = CTSE_ScopeUserLock(tse);
    match.m_Bioseq = match.m_TSE_Lock.GetTSE_Lock()->FindBioseq(idh);
    _ASSERT(match.m_Bioseq);
}


void CDataSource_ScopeInfo::x_SetMatch(SSeqMatch_Scope& match,
                                       const SSeqMatch_DS& ds_match)
{
    match.m_Seq_id = ds_match.m_Seq_id;
    match.m_TSE_Lock = GetTSE_Lock(ds_match.m_TSE_Lock);
    match.m_Bioseq = ds_match.m_Bioseq;
    _ASSERT(match.m_Bioseq);
    _ASSERT(match.m_Bioseq == match.m_TSE_Lock.GetTSE_Lock()->FindBioseq(match.m_Seq_id));
}


/////////////////////////////////////////////////////////////////////////////
// CTSE_ScopeInternalLock
/////////////////////////////////////////////////////////////////////////////

void CTSE_ScopeInternalLock::x_Lock(TObject& object)
{
    //_TRACE("CTSE_ScopeInternalLock("<<this<<") "<<&object<<" lock");
    _ASSERT(!*this);
    object.x_LockTSE();
    m_Object = &object;
}


void CTSE_ScopeInternalLock::x_Relock(const CTSE_ScopeInternalLock& src)
{
    Reset();
    if ( src ) {
        //_TRACE("CTSE_ScopeInternalLock("<<this<<") "<<&*src<<" lock");
        src->x_RelockTSE();
        m_Object = &*src;
    }
}


void CTSE_ScopeInternalLock::x_Unlock(void)
{
    if ( m_Object ) {
        //_TRACE("CTSE_ScopeInternalLock("<<this<<") "<<&*m_Object<<" unlock");
        (*this)->x_InternalUnlockTSE();
        m_Object.Reset();
    }
}


/////////////////////////////////////////////////////////////////////////////
// CTSE_ScopeUserLock
/////////////////////////////////////////////////////////////////////////////


const CTSE_Lock& CTSE_ScopeUserLock::GetTSE_Lock(void) const
{
    return (**this).m_TSE_Lock;
}


void CTSE_ScopeUserLock::x_Lock(TObject& object)
{
    //_TRACE("CTSE_ScopeUserLock("<<this<<") "<<&object<<" lock");
    _ASSERT(!*this);
    object.x_LockTSE();
    m_Object = &object;
}


void CTSE_ScopeUserLock::x_Lock(TObject& object, const CTSE_Lock& lock)
{
    //_TRACE("CTSE_ScopeUserLock("<<this<<") "<<&object<<" lock");
    _ASSERT(!*this);
    object.x_LockTSE(lock);
    m_Object = &object;
}


void CTSE_ScopeUserLock::x_Relock(const CTSE_ScopeUserLock& src)
{
    Reset();
    if ( src ) {
        //_TRACE("CTSE_ScopeUserLock("<<this<<") "<<&*src<<" lock");
        src->x_RelockTSE();
        m_Object = &*src;
    }
}


void CTSE_ScopeUserLock::x_Unlock(void)
{
    if ( m_Object ) {
        //_TRACE("CTSE_ScopeUserLock("<<this<<") "<<&*m_Object<<" unlock");
        (*this)->x_UserUnlockTSE();
        m_Object.Reset();
    }
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
    ITERATE ( CTSE_Info::TBioseqs, it, tse_lock->GetBioseqsMap() ) {
        m_BioseqsIds.push_back(it->first);
    }
    sort(m_BioseqsIds.begin(), m_BioseqsIds.end());
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
        x_LockTSE(tse_lock);
    }
}


CTSE_ScopeInfo::~CTSE_ScopeInfo(void)
{
    if ( !CanBeUnloaded() ) {
        // remove permanent lock
        _ASSERT(m_TSE_Lock);
        _VERIFY(m_TSE_LockCounter.Add(-1) == 0);
        x_ForgetLocks();
    }
    _ASSERT(m_TSE_LockCounter.Get() == 0);
    _ASSERT(!m_TSE_Lock);
#ifdef _DEBUG
    NON_CONST_ITERATE ( TBioseqs, it, m_Bioseqs ) {
        _ASSERT(!it->second->m_Bioseq);
        it->second->m_TSE_ScopeInfo = 0;
    }
#endif
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


void CTSE_ScopeInfo::x_LockTSE(const CTSE_Lock& lock)
{
    m_TSE_LockCounter.Add(1);
    if ( !m_TSE_Lock ) {
        GetDSInfo().UpdateTSELock(*this, lock);
    }
    _ASSERT(m_TSE_Lock);
}


void CTSE_ScopeInfo::x_RelockTSE(void)
{
    _VERIFY(m_TSE_LockCounter.Add(1) > 1);
    _ASSERT(m_TSE_Lock);
}


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


bool CTSE_ScopeInfo::x_SameTSE(const CTSE_Info& tse) const
{
    return m_TSE_LockCounter.Get() > 0 && m_TSE_Lock && &*m_TSE_Lock == &tse;
}


bool CTSE_ScopeInfo::AddUsedTSE(const CTSE_ScopeUserLock& used_tse) const
{
    CTSE_ScopeInfo& add_info = *used_tse;
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
    _VERIFY(m_UsedTSE_Set.insert(CTSE_ScopeInternalLock(add_info)).second);
    return true;
}


void CTSE_ScopeInfo::x_ForgetLocks(void)
{
    NON_CONST_ITERATE ( TBioseqs, it, m_Bioseqs ) {
        it->second->m_Bioseq.Reset();
        it->second->m_BioseqAnnotRef_Info.Reset();
    }
    ITERATE ( TUsedTSE_LockSet, it, m_UsedTSE_Set ) {
        _ASSERT((*it)->m_UsedByTSE == this);
        (*it)->m_UsedByTSE = 0;
    }
    m_UsedTSE_Set.clear();
    m_TSE_Lock.Reset();
}


// Find scope bioseq info by match: CConstRef<CBioseq_Info> & CSeq_id_Handle
// The problem is that CTSE_Info and CBioseq_Info may be unloaded and we
// cannot store pointers to them.
// However, we have to find the same CBioseq_ScopeInfo object.
// It is stored in m_Bioseqs map under one of Bioseq's ids.
CRef<CBioseq_ScopeInfo>
CTSE_ScopeInfo::GetBioseqInfo(const SSeqMatch_Scope& match)
{
    _ASSERT(&*match.m_TSE_Lock == this);

    CDataSource_ScopeInfo::TTSE_InfoMapMutex::TWriteLockGuard
        guard(GetDSInfo().GetTSE_InfoMapMutex());

    // First try to find by existing bioseq.
    TBioseqs::iterator iter = m_Bioseqs.find(match.m_Seq_id);
    if ( iter != m_Bioseqs.end() ) {
        return iter->second;
    }

    // We didn't find CBioseq_ScopeInfo, so it's the first time this bioseq
    // is requested from scope.
    // We'll create new CBioseq_ScopeInfo object
    // and remember it in m_Bioseqs map.
    CRef<CBioseq_ScopeInfo> info(new CBioseq_ScopeInfo(match));

    // Then try to find by another id of bioseq.
    ITERATE ( CBioseq_Info::TId, it, match.m_Bioseq->GetId() ) {
        _VERIFY(m_Bioseqs.insert(TBioseqs::value_type(*it, info)).second);
    }

    return info;
}


bool CTSE_ScopeInfo::HasResolvedBioseq(const CSeq_id_Handle& id) const
{
    return m_Bioseqs.find(id) != m_Bioseqs.end();
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


void CTSE_ScopeInfo::ClearCacheOnRemoveData(const CBioseq_Info& bioseq)
{
    if ( !m_Bioseqs.empty() ) {
        ITERATE ( CBioseq_Info::TId, it, bioseq.GetId() ) {
            m_Bioseqs.erase(*it);
        }
    }
}


CScope_Impl& CTSE_ScopeInfo::GetScopeImpl(void) const
{
    return GetDSInfo().GetScopeImpl();
}


CDataSource_ScopeInfo& CTSE_ScopeInfo::GetDSInfo(void) const
{
    _ASSERT(m_DS_Info);
    return *m_DS_Info;
}


/////////////////////////////////////////////////////////////////////////////
// CBioseq_ScopeInfo
/////////////////////////////////////////////////////////////////////////////


CBioseq_ScopeInfo::CBioseq_ScopeInfo(CScope_Impl& scope)
    : m_ScopeInfo(&scope),
      m_TSE_ScopeInfo(0)
{
}


CBioseq_ScopeInfo::CBioseq_ScopeInfo(const SSeqMatch_Scope& match)
    : m_ScopeInfo(&match.m_TSE_Lock->GetScopeImpl()),
      m_TSE_ScopeInfo(&*match.m_TSE_Lock)
{
    if ( m_TSE_ScopeInfo->CanBeUnloaded() ) {
        m_UnloadedInfo.reset(new SUnloadedInfo);
        m_UnloadedInfo->m_LookupId = match.m_Seq_id;
        m_UnloadedInfo->m_Ids = match.m_Bioseq->GetId();
    }
    else {
        m_Bioseq = match.m_Bioseq;
    }
}


CBioseq_ScopeInfo::~CBioseq_ScopeInfo(void)
{
}


CScope_Impl& CBioseq_ScopeInfo::GetScopeImpl(void) const
{
    if ( !m_ScopeInfo ) {
        NCBI_THROW(CCoreException, eNullPtr,
                   "CBioseq_ScopeInfo is not attached to CScope");
    }
    return *m_ScopeInfo;
}


const CTSE_ScopeInfo& CBioseq_ScopeInfo::GetTSE_ScopeInfo(void) const
{
    if ( !m_TSE_ScopeInfo ) {
        NCBI_THROW(CCoreException, eNullPtr,
                   "CBioseq_ScopeInfo is not attached to CTSE_ScopeInfo");
    }
    return *m_TSE_ScopeInfo;
}


const CSeq_id_Handle& CBioseq_ScopeInfo::GetLookupId(void) const
{
    _ASSERT(m_UnloadedInfo);
    return m_UnloadedInfo->m_LookupId;
}


CConstRef<CBioseq_Info>
CBioseq_ScopeInfo::GetBioseqInfo(const CTSE_Info& tse) const
{
    _ASSERT(GetTSE_ScopeInfo().x_SameTSE(tse));
    if ( !m_Bioseq ) {
        CDataSource_ScopeInfo::TTSE_LockSetMutex::TWriteLockGuard
            guard(GetTSE_ScopeInfo().GetDSInfo().GetTSE_LockSetMutex());

        if ( !m_Bioseq ) {
            m_Bioseq = tse.FindBioseq(GetLookupId());
        }
    }
    _ASSERT(&m_Bioseq->GetTSE_Info() == &tse);
    return m_Bioseq;
}


const CBioseq_ScopeInfo::TIds& CBioseq_ScopeInfo::GetIds(void) const
{
    return m_Bioseq? m_Bioseq->GetId(): m_UnloadedInfo->m_Ids;
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
    CRef<CBioseq_ScopeInfo> binfo = (*iter)->second.m_Bioseq_Info;
    return binfo->GetScopeImpl().GetBioseqHandle((*iter)->first, *binfo);
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
