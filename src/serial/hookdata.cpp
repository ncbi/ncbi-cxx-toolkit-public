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
#include <serial/objhook.hpp>
#include <serial/hookfunc.hpp>
#include <algorithm>

BEGIN_NCBI_SCOPE

template<class Object, typename Function>
CHookData<Object, Function>::CHookData(TFunction typeFunction,
                                       TFunction hookFunction)
    : m_CurrentFunction(typeFunction), m_SecondaryFunction(hookFunction)
{
}

template<class Object, typename Function>
CHookData<Object, Function>::~CHookData(void)
{
    if ( m_Data.get() )
        ResetHooks();
}

template<class Object, typename Function>
CHookData<Object, Function>::TData&
CHookData<Object, Function>::CreateData(void)
{
    _ASSERT(!m_Data.get());
    TData* hooks = new TData;
    m_Data.reset(hooks);
    swap(m_CurrentFunction, m_SecondaryFunction);
    return *hooks;
}

template<class Object, typename Function>
void CHookData<Object, Function>::ResetHooks(void)
{
    _ASSERT(m_Data.get());
    swap(m_CurrentFunction, m_SecondaryFunction);
    m_Data.reset(0);
}

template<class Object, typename Function>
void CHookData<Object, Function>::SetLocalHook(key_type& key, THook* hook)
{
    _ASSERT(hook);
    GetDataForSet().m_LocalHooks.insert(key, hook);
}

template<class Object, typename Function>
void CHookData<Object, Function>::SetGlobalHook(THook* hook)
{
    _ASSERT(hook);
    GetDataForSet().m_GlobalHook.Reset(hook);
}

template<class Object, typename Function>
void CHookData<Object, Function>::ResetLocalHook(key_type& key)
{
    TData& hooks = GetDataForReset();
    hooks.m_LocalHooks.erase(key);
    if ( hooks.empty() )
        ResetHooks();
}

template<class Object, typename Function>
void CHookData<Object, Function>::ResetGlobalHook(void)
{
    TData& hooks = GetDataForReset();
    hooks.m_GlobalHook.Reset(0);
    if ( hooks.m_LocalHooks.empty() ) // m_GlobalHook already empty
        ResetHooks();
}

template<class Object, typename Function>
CHookData<Object, Function>::THook*
CHookData<Object, Function>::GetGlobalHook(void) const
{
    const TData* hooks = GetDataForGet();
    if ( hooks )
        return hooks->m_GlobalHook.GetPointer();
    return 0;
}

template<class Object, typename Function>
CHookData<Object, Function>::THook*
CHookData<Object, Function>::GetHook(key_type& key) const
{
    const TData* hooks = GetDataForGet();
    if ( hooks ) {
        typedef typename TData::TMap::const_iterator TMapCI;
        TMapCI i = hooks->m_LocalHooks.find(key);
        if ( i != hooks->m_LocalHooks.end() )
            return i->second.GetPointer();
        return hooks->m_GlobalHook.GetPointer();
    }
    return 0;
}

template class CHookData<CReadObjectHook, TTypeReadFunction>;
template class CHookData<CWriteObjectHook, TTypeWriteFunction>;
template class CHookData<CCopyObjectHook, TTypeCopyFunction>;

template class CHookData<CReadClassMemberHook, SMemberReadFunctions>;
template class CHookData<CWriteClassMemberHook, TMemberWriteFunction>;
template class CHookData<CCopyClassMemberHook, SMemberCopyFunctions>;

template class CHookData<CReadChoiceVariantHook, TVariantReadFunction>;
template class CHookData<CWriteChoiceVariantHook, TVariantWriteFunction>;
template class CHookData<CCopyChoiceVariantHook, TVariantCopyFunction>;

END_NCBI_SCOPE
