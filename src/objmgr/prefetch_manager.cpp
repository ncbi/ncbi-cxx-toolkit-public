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


IPrefetchListener::~IPrefetchListener(void)
{
}


CPrefetchToken::CPrefetchToken(void)
{
}


CPrefetchToken::~CPrefetchToken(void)
{
}


CPrefetchToken::CPrefetchToken(const CPrefetchToken& token)
    : m_Handle(token.m_Handle), m_Manager(token.m_Manager)
{
}


CPrefetchToken& CPrefetchToken::operator=(const CPrefetchToken& token)
{
    m_Handle = token.m_Handle;
    m_Manager = token.m_Manager;
    return *this;
}


CPrefetchToken::CPrefetchToken(CPrefetchRequest* impl)
    : m_Handle(impl->m_QueueItem), m_Manager(impl->GetManager())
{
}


CPrefetchToken::CPrefetchToken(CPrefetchManager_Impl::TItemHandle handle)
    : m_Handle(handle), m_Manager(handle->GetRequest().GetManager())
{
}


CPrefetchRequest& CPrefetchToken::x_GetImpl(void) const
{
    return m_Handle.GetNCObject().SetRequest();
}


IPrefetchAction* CPrefetchToken::GetAction(void) const
{
    return x_GetImpl().GetAction();
}


IPrefetchListener* CPrefetchToken::GetListener(void) const
{
    return x_GetImpl().GetListener();
}


void CPrefetchToken::SetListener(IPrefetchListener* listener)
{
    x_GetImpl().SetListener(m_Manager.GetNCObject(), listener);
}


CPrefetchToken::EState CPrefetchToken::GetState(void) const
{
    return x_GetImpl().GetState();
}


bool CPrefetchToken::IsDone(void) const
{
    return x_GetImpl().IsDone();
}


CPrefetchToken::TProgress CPrefetchToken::GetProgress(void) const
{
    return x_GetImpl().GetProgress();
}


CPrefetchToken::TProgress CPrefetchToken::SetProgress(TProgress progress)
{
    return x_GetImpl().SetProgress(m_Manager.GetNCObject(), progress);
}


void CPrefetchToken::Cancel(void) const
{
    x_GetImpl().Cancel(m_Manager.GetNCObject());
}


CPrefetchManager::CPrefetchManager(void)
    : m_Impl(new CPrefetchManager_Impl())
{
}


CPrefetchManager::~CPrefetchManager(void)
{
}


CPrefetchToken CPrefetchManager::AddAction(TPriority priority,
                                           IPrefetchAction* action,
                                           IPrefetchListener* listener)
{
    if ( !action ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CPrefetchManager::AddAction: action is null");
    }
    return m_Impl->AddAction(priority, action, listener);
}


const char* CPrefetchCanceled::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eCanceled: return "eCanceled";
    default:        return CException::GetErrCodeString();
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
