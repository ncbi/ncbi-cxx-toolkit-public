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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

#if 1
# define _TRACE_TSE_LOCK(x) _TRACE(x)
#else
# define _TRACE_TSE_LOCK(x) ((void)0)
#endif

#define _CHECK() _ASSERT(!*this || &m_TSE->GetScopeImpl() == m_Scope.GetImpl())

CTSE_Handle::CTSE_Handle(TObject& object)
    : m_Scope(object.GetScopeImpl().GetScope()),
      m_TSE(&object)
{
    if ( m_TSE.GetPointer() )
        _TRACE_TSE_LOCK("CTSE_Handle("<<this<<") "<<m_TSE.GetPointer()<<" lock");
    _CHECK();
}


CTSE_Handle::CTSE_Handle(const CTSE_ScopeUserLock& lock)
    : m_Scope(lock->GetScopeImpl().GetScope()),
      m_TSE(lock)
{
    if ( m_TSE.GetPointer() )
        _TRACE_TSE_LOCK("CTSE_Handle("<<this<<") "<<m_TSE.GetPointer()<<" lock");
    _CHECK();
}


CTSE_Handle::CTSE_Handle(const CTSE_Handle& tse)
    : m_Scope(tse.m_Scope),
      m_TSE(tse.m_TSE)
{
    if ( m_TSE.GetPointer() )
        _TRACE_TSE_LOCK("CTSE_Handle("<<this<<") "<<m_TSE.GetPointer()<<" lock");
    _CHECK();
}


CTSE_Handle::~CTSE_Handle(void)
{
    _CHECK();
    if ( m_TSE.GetPointer() )
        _TRACE_TSE_LOCK("CTSE_Handle("<<this<<") "<<m_TSE.GetPointer()<<" unlock");
}


CTSE_Handle& CTSE_Handle::operator=(const CTSE_Handle& tse)
{
    _CHECK();
    if ( this != &tse ) {
        if ( m_TSE.GetPointer() )
            _TRACE_TSE_LOCK("CTSE_Handle("<<this<<") "<<m_TSE.GetPointer()<<" unlock");
        m_TSE = tse.m_TSE;
        m_Scope = tse.m_Scope;
        if ( m_TSE.GetPointer() )
            _TRACE_TSE_LOCK("CTSE_Handle("<<this<<") "<<m_TSE.GetPointer()<<" lock");
        _CHECK();
    }
    return *this;
}


void CTSE_Handle::Reset(void)
{
    _CHECK();
    if ( m_TSE.GetPointer() )
        _TRACE_TSE_LOCK("CTSE_Handle("<<this<<") "<<m_TSE.GetPointer()<<" unlock");
    m_TSE.Reset();
    m_Scope.Reset();
    _CHECK();
}


const CTSE_Info& CTSE_Handle::x_GetTSE_Info(void) const
{
    return *m_TSE->m_TSE_Lock;
}


CBlobIdKey CTSE_Handle::GetBlobId(void) const
{
    CBlobIdKey ret;
    if ( *this ) {
        const CTSE_Info& tse = x_GetTSE_Info();
        ret = CBlobIdKey(tse.GetDataSource().GetDataLoader(), tse.GetBlobId());
    }
    return ret;
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
    CDataSource& ds = x_GetTSE_Info().GetDataSource();
    return !ds.GetDataLoader() && !ds.GetSharedObject();
}


////////////////////////////////////////////////////////////////////////////
// CHandleInfo_Base
////////////////////////////////////////////////////////////////////////////


CScopeInfo_Base::CScopeInfo_Base(void)
    : m_TSE_ScopeInfo(0)
{
    m_LockCounter.Set(0);
    _ASSERT(x_Check(fForceZero | fForbidInfo));
}


CScopeInfo_Base::CScopeInfo_Base(const CTSE_ScopeUserLock& tse,
                                 const CObject& info)
    : m_TSE_ScopeInfo(tse.GetNonNullNCPointer()),
      m_TSE_Handle(tse),
      m_ObjectInfo(&info)
{
    m_LockCounter.Set(0);
    _ASSERT(x_Check(fForceZero | fForceInfo));
}


CScopeInfo_Base::CScopeInfo_Base(const CTSE_Handle& tse, const CObject& info)
    : m_TSE_ScopeInfo(&tse.x_GetScopeInfo()),
      m_TSE_Handle(tse),
      m_ObjectInfo(&info)
{
    m_LockCounter.Set(0);
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
    if ( IsRemoved() ) {
        return !m_TSE_Handle && !m_ObjectInfo;
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
}


void CScopeInfo_Base::x_SetLock(const CTSE_ScopeUserLock& tse,
                                const CObject& info)
{
    _ASSERT(x_Check(fAllowZero|fAllowInfo));
    _ASSERT(!IsRemoved());
    _ASSERT(tse);
    _ASSERT(&*tse == m_TSE_ScopeInfo);
    _ASSERT(!m_TSE_Handle || &m_TSE_Handle.x_GetScopeInfo() == &*tse);
    _ASSERT(!m_ObjectInfo || m_ObjectInfo == &info);
    m_TSE_Handle = tse;
    m_ObjectInfo = &info;
    _ASSERT(x_Check(fAllowZero|fForceInfo));
}


void CScopeInfo_Base::x_ResetLock(void)
{
    //_ASSERT(x_Check(fForceZero|fAllowInfo));
    _ASSERT(!IsRemoved());
    m_ObjectInfo.Reset();
    m_TSE_Handle.Reset();
    //_ASSERT(x_Check(fForceZero|fForbidInfo));
}


void CScopeInfo_Base::x_RemoveLastInfoLock(void)
{
    if ( m_TSE_ScopeInfo ) {
        m_TSE_ScopeInfo->RemoveLastInfoLock(*this);
    }
    else {
        _ASSERT(!m_TSE_Handle && !m_ObjectInfo);
    }
}


void CScopeInfo_Base::x_AttachTSE(CTSE_ScopeInfo* tse)
{
    _ASSERT(tse);
    _ASSERT(!m_TSE_ScopeInfo);
    _ASSERT(IsRemoved());
    _ASSERT(x_Check(fAllowZero|fForbidInfo));
    m_TSE_ScopeInfo = tse;
    _ASSERT(x_Check(fAllowZero|fForbidInfo));
}


void CScopeInfo_Base::x_DetachTSE(CTSE_ScopeInfo* tse)
{
    _ASSERT(tse);
    _ASSERT(!IsRemoved());
    _ASSERT(m_TSE_ScopeInfo == tse);
    //_ASSERT(x_Check(fForceZero|fForbidInfo));
    _ASSERT(!m_ObjectInfo && !m_TSE_Handle);
    m_TSE_ScopeInfo = 0;
    //_ASSERT(x_Check(fForceZero|fForbidInfo));
}


void CScopeInfo_Base::x_ForgetTSE(CTSE_ScopeInfo* tse)
{
    _ASSERT(tse);
    _ASSERT(!IsRemoved());
    _ASSERT(m_TSE_ScopeInfo == tse);
    _ASSERT(x_Check(fAllowZero));
    m_ObjectInfo.Reset();
    m_TSE_Handle.Reset();
    m_TSE_ScopeInfo = 0;
    _ASSERT(x_Check(fForceZero|fForbidInfo));
}


END_SCOPE(objects)
END_NCBI_SCOPE
