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
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.11  2004/08/10 14:45:42  vakatov
* Fixed sign/unsign warn on MSVC
*
* Revision 1.10  2004/05/17 21:03:02  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.9  2004/01/05 14:25:20  gouriano
* Added possibility to set serialization hooks by stack path
*
* Revision 1.8  2003/08/19 18:32:38  vasilche
* Optimized reading and writing strings.
* Avoid string reallocation when checking char values.
* Try to reuse old string data when string reference counting is not working.
*
* Revision 1.7  2003/07/29 20:36:48  vasilche
* Fixed warnings on Windows.
*
* Revision 1.6  2003/07/29 18:59:21  vasilche
* Fixed compilation errors.
*
* Revision 1.5  2003/07/29 18:47:47  vasilche
* Fixed thread safeness of object stream hooks.
*
* Revision 1.4  2002/11/04 21:29:20  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.3  2001/01/05 20:10:50  vasilche
* CByteSource, CIStrBuffer, COStrBuffer, CLightString, CChecksum, CWeakMap
* were moved to util.
*
* Revision 1.2  2000/10/13 16:28:39  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* Revision 1.1  2000/10/03 17:22:42  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <serial/hookdata.hpp>
#include <serial/objstack.hpp>

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
    _ASSERT(m_HookCount.Get() >= 0);
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
    _ASSERT(m_HookCount.Get() >= 0);
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
