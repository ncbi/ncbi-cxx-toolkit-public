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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
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

#include <corelib/ncbistd.hpp>
#include <serial/hookdata.hpp>
#include <serial/hookdataimpl.hpp>
#include <serial/hookdatakey.hpp>
#include <serial/weakmap.hpp>

BEGIN_NCBI_SCOPE

CHookDataKeyBase::CHookDataKeyBase(void)
    : m_Key(0)
{
}

CHookDataKeyBase::~CHookDataKeyBase(void)
{
    delete m_Key;
}

CHookDataKeyBase::TData& CHookDataKeyBase::Get(void)
{
    TData* key = m_Key;
    if ( !key )
        m_Key = key = new TData;
    return *key;
}

CHookDataBase::TData& CHookDataBase::GetDataForSet(void)
{
    TData* hooks = m_Data;
    if ( !hooks )
        m_Data = hooks = new TData;
    return *hooks;
}

CHookDataBase::TData& CHookDataBase::GetDataForReset(void)
{
    _ASSERT(m_Data);
    return *m_Data;
}

CHookDataBase::CHookDataBase(void)
    : m_Data(0)
{
}

CHookDataBase::~CHookDataBase(void)
{
    delete m_Data;
}

void CHookDataBase::ResetData(void)
{
    _ASSERT(m_Data);
    delete m_Data;
    m_Data = 0;
}

bool CHookDataBase::SetLocalHook(TKey& key, THook* hook)
{
    _ASSERT(hook);
    bool wasEmpty = Empty();
    GetDataForSet().SetLocalHook(key.Get(), hook);
    _ASSERT(!Empty());
    return wasEmpty;
}

bool CHookDataBase::SetGlobalHook(THook* hook)
{
    _ASSERT(hook);
    bool wasEmpty = Empty();
    GetDataForSet().SetGlobalHook(hook);
    _ASSERT(!Empty());
    return wasEmpty;
}

bool CHookDataBase::ResetLocalHook(TKey& key)
{
    _ASSERT(!Empty());
    TData& hooks = GetDataForReset();
    hooks.ResetLocalHook(key.Get());
    if ( hooks.Empty() ) {
        ResetData();
        return true;
    }
    return false;
}

bool CHookDataBase::ResetGlobalHook(void)
{
    _ASSERT(!Empty());
    TData& hooks = GetDataForReset();
    hooks.ResetGlobalHook();
    if ( hooks.Empty() ) {
        ResetData();
        return true;
    }
    return false;
}

CHookDataBase::THook* CHookDataBase::GetHook(TKey& key) const
{
    const TData* hooks = GetDataForGet();
    if ( hooks )
        return hooks->GetHook(key.Get());
    return 0;
}

CHookDataKeyData::CHookDataKeyData(void)
{
}

CHookDataKeyData::~CHookDataKeyData(void)
{
}

CHookDataData::CHookDataData(void)
{
}

CHookDataData::~CHookDataData(void)
{
}

bool CHookDataData::Empty(void) const
{
    return !m_GlobalHook && m_LocalHooks.empty();
}

void CHookDataData::SetGlobalHook(CObject* hook)
{
    m_GlobalHook.Reset(hook);
}

void CHookDataData::ResetGlobalHook(void)
{
    m_GlobalHook.Reset();
}

void CHookDataData::SetLocalHook(CHookDataKeyData& key, CObject* hook)
{
    m_LocalHooks.insert(key.m_Key, hook);
}

void CHookDataData::ResetLocalHook(CHookDataKeyData& key)
{
    m_LocalHooks.erase(key.m_Key);
}

CObject* CHookDataData::GetGlobalHook(void) const
{
    return m_GlobalHook.GetPointer();
}

CObject* CHookDataData::GetHook(CHookDataKeyData& key) const
{
    TMap::const_iterator i = m_LocalHooks.find(key.m_Key);
    if ( i != m_LocalHooks.end() )
        return i->second.GetPointer();
    return m_GlobalHook.GetPointer();
}

END_NCBI_SCOPE
