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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*    Handle to top level Seq-entry
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/tse_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/impl/scope_info.hpp>
#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/seq_feat_handle.hpp>
#include <objmgr/impl/seq_annot_info.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

#if 0
# define _TRACE_TSE_LOCK(type)                                          \
    if ( !m_TSE.GetPointer() )                                          \
        ;                                                               \
    else                                                                \
        _TRACE("CTSE_Handle("<<this<<") "<<m_TSE.GetPointer()<<" " type)
#else
# define _TRACE_TSE_LOCK(type) ((void)0)
#endif

#define _CHECK() _ASSERT(!*this || &m_TSE->GetScopeImpl() == m_Scope.GetImpl())

CTSE_Handle::CTSE_Handle(TScopeInfo& object)
    : m_Scope(object.GetScopeImpl().GetScope()),
      m_TSE(&object)
{
    _TRACE_TSE_LOCK("lock");
    _CHECK();
}


CTSE_Handle::CTSE_Handle(const CTSE_ScopeUserLock& lock)
    : m_Scope(lock->GetScopeImpl().GetScope()),
      m_TSE(lock)
{
    _TRACE_TSE_LOCK("lock");
    _CHECK();
}


CTSE_Handle::CTSE_Handle(const CTSE_Handle& tse)
    : m_Scope(tse.m_Scope),
      m_TSE(tse.m_TSE)
{
    _TRACE_TSE_LOCK("lock");
    _CHECK();
}


CTSE_Handle::~CTSE_Handle(void)
{
    _CHECK();
    _TRACE_TSE_LOCK("unlock");
}


CTSE_Handle& CTSE_Handle::operator=(const CTSE_Handle& tse)
{
    _CHECK();
    if ( this != &tse ) {
        _TRACE_TSE_LOCK("unlock");
        m_TSE = tse.m_TSE;
        m_Scope = tse.m_Scope;
        _TRACE_TSE_LOCK("lock");
        _CHECK();
    }
    return *this;
}


void CTSE_Handle::Reset(void)
{
    _CHECK();
    _TRACE_TSE_LOCK("unlock");
    m_TSE.Reset();
    m_Scope.Reset();
    _CHECK();
}


const CTSE_Info& CTSE_Handle::x_GetTSE_Info(void) const
{
    return *m_TSE->m_TSE_Lock;
}


CTSE_Handle::TBlobId CTSE_Handle::GetBlobId(void) const
{
    return x_GetTSE_Info().GetBlobId();
}


CDataLoader* CTSE_Handle::GetDataLoader(void) const
{
    return x_GetTSE_Info().GetDataSource().GetDataLoader();
}


bool CTSE_Handle::IsValid(void) const
{
    return m_TSE && m_TSE->IsAttached();
}


bool CTSE_Handle::Blob_IsSuppressed(void) const
{
    return Blob_IsSuppressedTemp()  ||  Blob_IsSuppressedPerm();
}


bool CTSE_Handle::Blob_IsSuppressedTemp(void) const
{
    return (x_GetTSE_Info().GetBlobState() &
            CBioseq_Handle::fState_suppress_temp) != 0;
}


bool CTSE_Handle::Blob_IsSuppressedPerm(void) const
{
    return (x_GetTSE_Info().GetBlobState() &
            CBioseq_Handle::fState_suppress_perm) != 0;
}


bool CTSE_Handle::Blob_IsDead(void) const
{
    return (x_GetTSE_Info().GetBlobState() &
            CBioseq_Handle::fState_dead) != 0;
}


CConstRef<CSeq_entry> CTSE_Handle::GetCompleteTSE(void) const
{
    return x_GetTSE_Info().GetCompleteSeq_entry();
}


CConstRef<CSeq_entry> CTSE_Handle::GetTSECore(void) const
{
    return x_GetTSE_Info().GetSeq_entryCore();
}


CSeq_entry_Handle CTSE_Handle::GetTopLevelEntry(void) const
{
    return CSeq_entry_Handle(x_GetTSE_Info(), *this);
}


CBioseq_Handle CTSE_Handle::GetBioseqHandle(const CSeq_id_Handle& id) const
{
    return x_GetScopeImpl().GetBioseqHandleFromTSE(id, *this);
}


CBioseq_Handle CTSE_Handle::GetBioseqHandle(const CSeq_id& id) const
{
    return GetBioseqHandle(CSeq_id_Handle::GetHandle(id));
}


bool CTSE_Handle::AddUsedTSE(const CTSE_Handle& tse) const
{
    return x_GetScopeInfo().AddUsedTSE(tse.m_TSE);
}


bool CTSE_Handle::CanBeEdited(void) const
{
    return x_GetScopeInfo().CanBeEdited();
}


////////////////////////////////////////////////////////////////////////////
// CHandleInfo_Base
////////////////////////////////////////////////////////////////////////////


/*
*****************************************************************************
CScopeInfo_Base can be in the following states:

S0. detached, unlocked:
   TSE_ScopeInfo == 0,
   LockCounter == 0,
   TSE_Handle == 0,
   ObjectInfo == 0.
    
S1. attached, locked, indexed:
   TSE_ScopeInfo != 0,
   LockCounter > 0,
   TSE_Handle != 0,
   ObjectInfo != 0, indexed.
 When unlocked by handles (LockCounter becomes 0) (A1):
  1. TSE_Handle is reset,
  New state: S4.
 When removed explicitly (A2):
  1. Do actual remove,
  2. Scan for implicit removals,
  3. Assert that this CScopeInfo_Base is removed (detached),
  New state: S3 (implicitly).
 When removed implicitly (found in ObjectInfo index as being removed) (A3):
  1. TSE_Handle is reset,
  2. Removed from index by ObjectInfo,
  3. Detached from TSE,
  New state: S3.
    
S2. attached, unlocked, non-temporary:
   TSE_ScopeInfo != 0,
   LockCounter == 0,
   TSE_Handle == 0,
   ObjectInfo == 0.
 When relocked (A5):
  1. TSE_Handle is set,
  2. ObjectInfo is set, adding to index by ObjectInfo,
  New state: S1.
 When removed implicitly (is it possible to determine?) (A3):
  1. Detached from TSE,
  New state: S0.
 When TSE is unlocked completely, non-temporary (A4):
  Same state: S2.
  
S3. detached, locked:
   TSE_ScopeInfo == 0,
   LockCounter > 0,
   TSE_Handle == 0,
   ObjectInfo != 0, not indexed.
 When unlocked by handles (LockCounter becomes 0) (A1):
  1. ObjectInfo is reset,
  New state: S0.
 When added into another place (A7):
  1. attached to TSE,
  2. TSE_Handle is set,
  3. ObjectInfo is set, adding to index by ObjectInfo,
  New state: S1.
  
S4. attached, unlocked, indexed:
   TSE_ScopeInfo != 0,
   LockCounter == 0,
   TSE_Handle == 0,
   ObjectInfo != 0, indexed.
 When relocked (A5):
  1. TSE_Handle is set,
  New state: S1.
 When removed implicitly (found in ObjectInfo index as being removed) (A3):
  1. Removing from index by ObjectInfo,
  2. Detached from TSE,
  New state: S3.
 When TSE is unlocked completely, temporary (A4):
  1. ObjectInfo is reset, with removing from index by ObjectInfo,
  2. Detached from TSE,
  New state: S0.
 When TSE is unlocked completely, non-temporary (A4):
  1. ObjectInfo is reset, with removing from index by ObjectInfo,
  New state: S2.
  
S5. detached, locked, dummy:
   TSE_ScopeInfo == 0,
   LockCounter > 0,
   TSE_Handle == 0,
   ObjectInfo == 0.
 When unlocked by handles (LockCounter becomes 0) (A1):
  1. -> S0.

*****************************************************************************
Meaning of members:    
A. TSE_ScopeInfo != 0: attached, means it's used by some TSE_ScopeInfo.
     S1,S4,S2.

B. LockCounter > 0: locked, means it's used by some handle.
     S1,S3,S5.

C. TSE_Handle != 0 only when attached and locked.
     S1.

D. Indexed by ObjectInfo only when attached and ObjectInfo != 0.
     S1,S4.

E. Empty, only LockCounter can be set, other members are null.
   Scope info cannot leave this state.
     S0,S5.

*****************************************************************************
Actions:
A1. Unlocking by handles: when LockCounter goes to zero.
  Pre: locked (S1,S3,S5).
  Post: unlocked (S4,S0).

  S1 (attached)    -> S4
  S3,S5 (detached) -> S0

A2. Explicit removal: when one of Remove/Reset methods is called.
  Pre: attached, locked (S1).
  Post: detached, locked (S3).

  Perform implicit removal A3.

A3. Implicit removal: when one of Remove/Reset methods is called.
  Pre: attached (S1,S4,S2).
  Post: detached (S3,S0).

  S1 (locked) -> S3. // TSE_Handle.Reset(), unindex.
  S4 (unlocked, indexed) -> S0.
  S2 (unlocked, not indexed) -> ???

A4. TSE unlocking: when TSE_Lock is removed from unlock queue.
  Pre: unlocked, attached (S2,S4).
  Post: unlocked, unindexed (S2,S0).

  S2 (non-temporary) -> S2.
  S4 (non-temporary) -> S2.
  S4 (temporary) -> S0.
  
A5. Relocking: when ScopeInfo is requested again.
  Pre: attached, unlocked (S2,S4).
  Post: attached, locked (S1).
  
  S4 -> S1.
  S2 -> S1.
  
A6. Removing from history.
  Pre: 

A7. Reattaching in new place.
  Pre: detached, locked (S3).
  Post: attached, locked (S1).
  
*/

CScopeInfo_Base::CScopeInfo_Base(void)
    : m_TSE_ScopeInfo(0)
{
    _ASSERT(x_Check(fForceZero | fForbidInfo));
}


CScopeInfo_Base::CScopeInfo_Base(const CTSE_ScopeUserLock& tse,
                                 const CTSE_Info_Object& info)
    : m_TSE_ScopeInfo(tse.GetNonNullNCPointer()),
      m_TSE_Handle(tse),
      m_ObjectInfo(&reinterpret_cast<const CObject&>(info))
{
    _ASSERT(x_Check(fForceZero | fForceInfo));
}


CScopeInfo_Base::CScopeInfo_Base(const CTSE_Handle& tse,
                                 const CTSE_Info_Object& info)
    : m_TSE_ScopeInfo(&tse.x_GetScopeInfo()),
      m_TSE_Handle(tse),
      m_ObjectInfo(&reinterpret_cast<const CObject&>(info))
{
    _ASSERT(x_Check(fForceZero | fForceInfo));
}


CScopeInfo_Base::~CScopeInfo_Base(void)
{
    _ASSERT(x_Check(fForceZero | fForbidInfo));
}


CScope_Impl& CScopeInfo_Base::x_GetScopeImpl(void) const
{
    return x_GetTSE_ScopeInfo().GetScopeImpl();
}


const CScopeInfo_Base::TIndexIds* CScopeInfo_Base::GetIndexIds(void) const
{
    return 0;
}


bool CScopeInfo_Base::x_Check(TCheckFlags zero_counter_mode) const
{
    return true;
/*
    if ( IsRemoved() ) {
        return !m_TSE_Handle;
    }
    if ( m_LockCounter.Get() <= 0 ) {
        if ( zero_counter_mode & fForbidZero ) {
            return false;
        }
        if ( m_ObjectInfo ) {
            if ( zero_counter_mode & fForbidInfo ) {
                return false;
            }
            return m_TSE_Handle;
        }
        else {
            if ( zero_counter_mode & fForceInfo ) {
                return false;
            }
            return !m_TSE_Handle;
        }
    }
    else {
        if ( zero_counter_mode & fForceZero ) {
            return false;
        }
        return m_TSE_Handle && m_ObjectInfo ||
            !m_TSE_Handle && !m_ObjectInfo;
    }
*/
}


void CScopeInfo_Base::x_SetLock(const CTSE_ScopeUserLock& tse,
                                const CTSE_Info_Object& info)
{
    _ASSERT(x_Check(fAllowZero|fAllowInfo));
    _ASSERT(!IsDetached());
    _ASSERT(tse);
    _ASSERT(&*tse == m_TSE_ScopeInfo);
    _ASSERT(!m_TSE_Handle || &m_TSE_Handle.x_GetScopeInfo() == &*tse);
    _ASSERT(!m_ObjectInfo || &GetObjectInfo_Base() == &info);
    m_TSE_Handle = tse;
    m_ObjectInfo = &reinterpret_cast<const CObject&>(info);
    _ASSERT(x_Check(fAllowZero|fForceInfo));
}


void CScopeInfo_Base::x_ResetLock(void)
{
    //_ASSERT(x_Check(fForceZero|fAllowInfo));
    _ASSERT(!IsDetached());
    m_ObjectInfo.Reset();
    m_TSE_Handle.Reset();
    //_ASSERT(x_Check(fForceZero|fForbidInfo));
}


// Action A1
void CScopeInfo_Base::x_RemoveLastInfoLock(void)
{
    CTSE_ScopeInfo* tse = m_TSE_ScopeInfo;
    if ( tse ) {
        tse->RemoveLastInfoLock(*this);
    }
}


void CScopeInfo_Base::x_AttachTSE(CTSE_ScopeInfo* tse)
{
    _ASSERT(tse);
    _ASSERT(!m_TSE_ScopeInfo);
    _ASSERT(IsDetached());
    _ASSERT(x_Check(fAllowZero|fForbidInfo));
    m_TSE_ScopeInfo = tse;
    _ASSERT(x_Check(fAllowZero|fForbidInfo));
}


void CScopeInfo_Base::x_DetachTSE(CTSE_ScopeInfo* tse)
{
    _ASSERT(tse);
    _ASSERT(!IsDetached());
    _ASSERT(m_TSE_ScopeInfo == tse);
    //_ASSERT(x_Check(fForceZero|fForbidInfo));
    _ASSERT(!m_TSE_Handle);
    m_TSE_ScopeInfo = 0;
    //_ASSERT(x_Check(fForceZero|fForbidInfo));
}


void CScopeInfo_Base::x_ForgetTSE(CTSE_ScopeInfo* tse)
{
    _ASSERT(tse);
    _ASSERT(!IsDetached());
    _ASSERT(m_TSE_ScopeInfo == tse);
    _ASSERT(x_Check(fAllowZero));
    m_ObjectInfo.Reset();
    m_TSE_Handle.Reset();
    m_TSE_ScopeInfo = 0;
    _ASSERT(x_Check(fForceZero|fForbidInfo));
}

/////////////////////////////////////////////////////////////////////////////
// FeatId support
CSeq_feat_Handle
CTSE_Handle::x_MakeHandle(CAnnotObject_Info* info) const
{
    return CSeq_feat_Handle(GetScope(), info);
}


CSeq_feat_Handle
CTSE_Handle::x_MakeHandle(const TAnnotObjectList& infos) const
{
    return infos.empty()? CSeq_feat_Handle(): x_MakeHandle(*infos.begin());
}


CTSE_Handle::TSeq_feat_Handles
CTSE_Handle::x_MakeHandles(const TAnnotObjectList& infos) const
{
    TSeq_feat_Handles handles;
    handles.reserve(infos.size());
    ITERATE ( TAnnotObjectList, it, infos ) {
        handles.push_back(x_MakeHandle(*it));
    }
    return handles;
}


/////////////////////////////////////////////////////////////////////////////
// integer FeatId
CTSE_Handle::TSeq_feat_Handles
CTSE_Handle::GetFeaturesWithId(CSeqFeatData::E_Choice type,
                               TFeatureIdInt id) const
{
    return x_MakeHandles(x_GetTSE_Info().x_GetFeaturesById(type, id, eFeatId_id));
}


CTSE_Handle::TSeq_feat_Handles
CTSE_Handle::GetFeaturesWithId(CSeqFeatData::ESubtype subtype,
                               TFeatureIdInt id) const
{
    return x_MakeHandles(x_GetTSE_Info().x_GetFeaturesById(subtype, id, eFeatId_id));
}


CTSE_Handle::TSeq_feat_Handles
CTSE_Handle::GetFeaturesWithXref(CSeqFeatData::E_Choice type,
                                 TFeatureIdInt id) const
{
    return x_MakeHandles(x_GetTSE_Info().x_GetFeaturesById(type, id, eFeatId_xref));
}


CTSE_Handle::TSeq_feat_Handles
CTSE_Handle::GetFeaturesWithXref(CSeqFeatData::ESubtype subtype,
                                 TFeatureIdInt id) const
{
    return x_MakeHandles(x_GetTSE_Info().x_GetFeaturesById(subtype, id, eFeatId_xref));
}


CSeq_feat_Handle CTSE_Handle::GetFeatureWithId(CSeqFeatData::E_Choice type,
                                               TFeatureIdInt id) const
{
    return x_MakeHandle(x_GetTSE_Info().x_GetFeaturesById(type, id, eFeatId_id));
}


CSeq_feat_Handle CTSE_Handle::GetFeatureWithId(CSeqFeatData::ESubtype subtype,
                                               TFeatureIdInt id) const
{
    return x_MakeHandle(x_GetTSE_Info().x_GetFeaturesById(subtype, id, eFeatId_id));
}


/////////////////////////////////////////////////////////////////////////////
// string FeatId
CTSE_Handle::TSeq_feat_Handles
CTSE_Handle::GetFeaturesWithId(CSeqFeatData::E_Choice type,
                               const TFeatureIdStr& id) const
{
    return x_MakeHandles(x_GetTSE_Info().x_GetFeaturesById(type, id, eFeatId_id));
}


CTSE_Handle::TSeq_feat_Handles
CTSE_Handle::GetFeaturesWithId(CSeqFeatData::ESubtype subtype,
                               const TFeatureIdStr& id) const
{
    return x_MakeHandles(x_GetTSE_Info().x_GetFeaturesById(subtype, id, eFeatId_id));
}


CTSE_Handle::TSeq_feat_Handles
CTSE_Handle::GetFeaturesWithXref(CSeqFeatData::E_Choice type,
                                 const TFeatureIdStr& id) const
{
    return x_MakeHandles(x_GetTSE_Info().x_GetFeaturesById(type, id, eFeatId_xref));
}


CTSE_Handle::TSeq_feat_Handles
CTSE_Handle::GetFeaturesWithXref(CSeqFeatData::ESubtype subtype,
                                 const TFeatureIdStr& id) const
{
    return x_MakeHandles(x_GetTSE_Info().x_GetFeaturesById(subtype, id, eFeatId_xref));
}


CSeq_feat_Handle CTSE_Handle::GetFeatureWithId(CSeqFeatData::E_Choice type,
                                               const TFeatureIdStr& id) const
{
    return x_MakeHandle(x_GetTSE_Info().x_GetFeaturesById(type, id, eFeatId_id));
}


CSeq_feat_Handle CTSE_Handle::GetFeatureWithId(CSeqFeatData::ESubtype subtype,
                                               const TFeatureIdStr& id) const
{
    return x_MakeHandle(x_GetTSE_Info().x_GetFeaturesById(subtype, id, eFeatId_id));
}


/////////////////////////////////////////////////////////////////////////////
// CObject_id FeatId
CTSE_Handle::TSeq_feat_Handles
CTSE_Handle::GetFeaturesWithId(CSeqFeatData::E_Choice type,
                               const TFeatureId& id) const
{
    return x_MakeHandles(x_GetTSE_Info().x_GetFeaturesById(type, id, eFeatId_id));
}


CTSE_Handle::TSeq_feat_Handles
CTSE_Handle::GetFeaturesWithId(CSeqFeatData::ESubtype subtype,
                               const TFeatureId& id) const
{
    return x_MakeHandles(x_GetTSE_Info().x_GetFeaturesById(subtype, id, eFeatId_id));
}


CTSE_Handle::TSeq_feat_Handles
CTSE_Handle::GetFeaturesWithXref(CSeqFeatData::E_Choice type,
                                 const TFeatureId& id) const
{
    return x_MakeHandles(x_GetTSE_Info().x_GetFeaturesById(type, id, eFeatId_xref));
}


CTSE_Handle::TSeq_feat_Handles
CTSE_Handle::GetFeaturesWithXref(CSeqFeatData::ESubtype subtype,
                                 const TFeatureId& id) const
{
    return x_MakeHandles(x_GetTSE_Info().x_GetFeaturesById(subtype, id, eFeatId_xref));
}


CSeq_feat_Handle CTSE_Handle::GetFeatureWithId(CSeqFeatData::E_Choice type,
                                               const TFeatureId& id) const
{
    return x_MakeHandle(x_GetTSE_Info().x_GetFeaturesById(type, id, eFeatId_id));
}


CSeq_feat_Handle CTSE_Handle::GetFeatureWithId(CSeqFeatData::ESubtype subtype,
                                               const TFeatureId& id) const
{
    return x_MakeHandle(x_GetTSE_Info().x_GetFeaturesById(subtype, id, eFeatId_id));
}


/////////////////////////////////////////////////////////////////////////////
// gene id support
CSeq_feat_Handle CTSE_Handle::GetGeneWithLocus(const string& locus,
                                               bool tag) const
{
    return x_MakeHandle(x_GetTSE_Info().x_GetFeaturesByLocus(locus, tag));
}


CTSE_Handle::TSeq_feat_Handles
CTSE_Handle::GetGenesWithLocus(const string& locus,
                               bool tag) const
{
    return x_MakeHandles(x_GetTSE_Info().x_GetFeaturesByLocus(locus, tag));
}


CSeq_feat_Handle CTSE_Handle::GetGeneByRef(const CGene_ref& ref) const
{
    CSeq_feat_Handle feat;
    if ( ref.IsSetLocus_tag() ) {
        feat = GetGeneWithLocus(ref.GetLocus_tag(), true);
    }
    if ( !feat && ref.IsSetLocus() ) {
        feat = GetGeneWithLocus(ref.GetLocus(), false);
    }
    return feat;
}


CTSE_Handle::TSeq_feat_Handles
CTSE_Handle::GetGenesByRef(const CGene_ref& ref) const
{
    TSeq_feat_Handles ret;
    if ( ref.IsSetLocus_tag() ) {
        GetGenesWithLocus(ref.GetLocus_tag(), true).swap(ret);
    }
    if ( ref.IsSetLocus() ) {
        TSeq_feat_Handles hh = GetGenesWithLocus(ref.GetLocus(), false);
        ret.insert(ret.end(), hh.begin(), hh.end());
    }
    return ret;
}


END_SCOPE(objects)
END_NCBI_SCOPE
