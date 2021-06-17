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
*   Prefetch implementation
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/prefetch_manager.hpp>
#include <objmgr/impl/prefetch_manager_impl.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbi_safe_static.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


IPrefetchAction::~IPrefetchAction(void)
{
}


IPrefetchActionSource::~IPrefetchActionSource(void)
{
}


IPrefetchListener::~IPrefetchListener(void)
{
}


CPrefetchManager::CPrefetchManager(void)
    : m_Impl(new CPrefetchManager_Impl(3, CThread::fRunDefault))
{
}


CPrefetchManager::CPrefetchManager(unsigned max_threads,
                                   CThread::TRunMode threads_mode)
    : m_Impl(new CPrefetchManager_Impl(max_threads, threads_mode))
{
}


CPrefetchManager::~CPrefetchManager(void)
{
}


CRef<CPrefetchRequest> CPrefetchManager::AddAction(TPriority priority,
                                                   IPrefetchAction* action,
                                                   IPrefetchListener* listener)
{
    if ( !action ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CPrefetchManager::AddAction: action is null");
    }
    return m_Impl->AddAction(priority, action, listener);
}


CRef<CPrefetchRequest> CPrefetchManager::AddAction(IPrefetchAction* action,
                                                   IPrefetchListener* listener)
{
    return AddAction(0, action, listener);
}


void CPrefetchManager::CancelAllTasks(void)
{
    m_Impl->CancelTasks(CThreadPool::fCancelExecutingTasks|
                        CThreadPool::fCancelQueuedTasks);
}


void CPrefetchManager::Shutdown(void)
{
    m_Impl->Abort();
}


const char* CPrefetchFailed::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eFailed: return "eFailed";
    default:        return CException::GetErrCodeString();
    }
}


const char* CPrefetchCanceled::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eCanceled: return "eCanceled";
    default:        return CException::GetErrCodeString();
    }
}


/////////////////////////////////////////////////////////////////////////////
// CPrefetchSequence


CPrefetchSequence::CPrefetchSequence(CPrefetchManager& manager,
                                     IPrefetchActionSource* source,
                                     size_t active_size)
    : m_Manager(&manager),
      m_Source(source)
{
    for ( size_t i = 0; i < active_size; ++i ) {
        EnqueNextAction();
    }
}


CPrefetchSequence::~CPrefetchSequence(void)
{
    CMutexGuard guard(m_Mutex);
    ITERATE ( list< CRef<CPrefetchRequest> >, it, m_ActiveTokens ) {
        it->GetNCPointer()->RequestToCancel();
    }
}


void CPrefetchSequence::EnqueNextAction(void)
{
    if ( !m_Source ) {
        return;
    }
    CIRef<IPrefetchAction> action(m_Source->GetNextAction());
    if ( !action ) {
        m_Source.Reset();
        return;
    }
    m_ActiveTokens.push_back(m_Manager->AddAction(action));
}


CRef<CPrefetchRequest> CPrefetchSequence::GetNextToken(void)
{
    CRef<CPrefetchRequest> ret;
    CMutexGuard guard(m_Mutex);
    if ( !m_ActiveTokens.empty() ) {
        EnqueNextAction();
        ret = m_ActiveTokens.front();
        m_ActiveTokens.pop_front();
    }
    return ret;
}


END_SCOPE(objects)
END_NCBI_SCOPE
