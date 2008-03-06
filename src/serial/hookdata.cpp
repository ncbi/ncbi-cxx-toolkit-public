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
*   Class for storing local hooks information in CTypeInfo
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <serial/impl/hookdata.hpp>
#include <serial/impl/objstack.hpp>

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
// CHookDataBase
/////////////////////////////////////////////////////////////////////////////


CHookDataBase::CHookDataBase(void)
{
    m_HookCount.Set(0);
}


CHookDataBase::~CHookDataBase(void)
{
    _ASSERT(m_HookCount.Get() == 0);
}


void CHookDataBase::SetLocalHook(TLocalHooks& key, THook* hook)
{
    _ASSERT(hook);
    _ASSERT(m_HookCount.Get() >= (TNCBIAtomicValue)(m_GlobalHook? 1: 0));
    key.SetHook(this, hook);
    m_HookCount.Add(1);
    _ASSERT(m_HookCount.Get() > (TNCBIAtomicValue)(m_GlobalHook? 1: 0));
    _ASSERT(!Empty());
}


void CHookDataBase::ResetLocalHook(TLocalHooks& key)
{
    _ASSERT(!Empty());
    _ASSERT(m_HookCount.Get() > (TNCBIAtomicValue)(m_GlobalHook? 1: 0));
    key.ResetHook(this);
    m_HookCount.Add(-1);
    _ASSERT(m_HookCount.Get() >= (TNCBIAtomicValue)(m_GlobalHook? 1: 0));
}


void CHookDataBase::ForgetLocalHook(TLocalHooks& _DEBUG_ARG(key))
{
    _ASSERT(!Empty());
    _ASSERT(m_HookCount.Get() > (TNCBIAtomicValue)(m_GlobalHook? 1: 0));
    _ASSERT(key.GetHook(this) != 0);
    m_HookCount.Add(-1);
    _ASSERT(m_HookCount.Get() >= (TNCBIAtomicValue)(m_GlobalHook? 1: 0));
}


void CHookDataBase::SetGlobalHook(THook* hook)
{
    _ASSERT(hook);
    _ASSERT(!m_GlobalHook);
    m_GlobalHook.Reset(hook);
    m_HookCount.Add(1);
    _ASSERT(m_HookCount.Get() > 0);
    _ASSERT(!Empty());
}


void CHookDataBase::ResetGlobalHook(void)
{
    _ASSERT(!Empty());
    _ASSERT(m_GlobalHook);
    _ASSERT(m_HookCount.Get() > 0);
    m_GlobalHook.Reset();
    m_HookCount.Add(-1);
}

void CHookDataBase::SetPathHook(CObjectStack* stk, const string& path, THook* hook)
{
    if (m_PathHooks.SetHook(stk, path, hook)) {
        m_HookCount.Add(1);
    }
}

void CHookDataBase::ResetPathHook(CObjectStack* stk, const string& path)
{
    if (m_PathHooks.SetHook(stk, path, 0)) {
        m_HookCount.Add(-1);
    }
}


END_NCBI_SCOPE
