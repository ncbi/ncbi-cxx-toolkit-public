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
    : m_Impl(token.m_Impl)
{
}


CPrefetchToken& CPrefetchToken::operator=(const CPrefetchToken& token)
{
    m_Impl = token.m_Impl;
    return *this;
}


CPrefetchToken::CPrefetchToken(CPrefetchToken_Impl* impl)
    : m_Impl(impl)
{
}


IPrefetchAction* CPrefetchToken::GetAction(void) const
{
    return x_GetImpl().GetAction();
}


IPrefetchListener* CPrefetchToken::GetListener(void) const
{
    return x_GetImpl().GetListener();
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
    return x_GetImpl().SetProgress(progress);
}


void CPrefetchToken::Wait(void) const
{
    x_GetImpl().Wait();
}


void CPrefetchToken::Cancel(void) const
{
    x_GetImpl().Cancel();
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
                                           IPrefetchListener* listener,
                                           TFlags flags)
{
    if ( !action ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CPrefetchManager::AddAction: action is null");
    }
    return m_Impl->AddAction(priority, action, listener, flags);
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
