#ifndef HOOKDATA__HPP
#define HOOKDATA__HPP

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
* Revision 1.2  2000/09/29 16:18:12  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.1  2000/09/18 20:00:01  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <serial/weakmap.hpp>
#include <algorithm>
#include <memory>

BEGIN_NCBI_SCOPE

template<class Hook, typename Function>
class CHookData
{
public:
    typedef Hook THook;
    typedef CRef<THook> mapped_type;
    typedef CWeakMapKey<mapped_type> key_type;

    typedef Function TFunction;

private:
    struct SHooks {
        typedef CWeakMap<mapped_type> TMap;

        mapped_type m_GlobalHook;
        TMap m_LocalHooks;
        
        bool empty(void) const
            {
                return !m_GlobalHook && m_LocalHooks.empty();
            }
    };

    SHooks& GetHooksForSet(void)
        {
            SHooks* hooks = m_Hooks.get();
            if ( !hooks ) {
                m_Hooks.reset(hooks = new SHooks);
                swap(m_CurrentFunction, m_SecondaryFunction);
            }
            return *hooks;
        }
    SHooks& GetHooksForReset(void)
        {
            SHooks* hooks = m_Hooks.get();
            _ASSERT(hooks);
            return *hooks;
        }
    const SHooks* GetHooksForGet(void) const
        {
            return m_Hooks.get();
        }
    void ResetHooks(void)
        {
            swap(m_CurrentFunction, m_SecondaryFunction);
            m_Hooks.reset(0);
        }

public:

    CHookData(TFunction typeFunction, TFunction hookFunction)
        : m_CurrentFunction(typeFunction), m_SecondaryFunction(hookFunction)
        {
        }

    TFunction GetCurrentFunction(void) const
        {
            return m_CurrentFunction;
        }

    bool HaveHooks(void) const
        {
            return m_Hooks.get();
        }

    TFunction& GetDefaultFunction(void)
        {
            return HaveHooks()? m_SecondaryFunction: m_CurrentFunction;
        }
    TFunction GetDefaultFunction(void) const
        {
            return HaveHooks()? m_SecondaryFunction: m_CurrentFunction;
        }

    void SetLocalHook(key_type& key, THook* hook)
        {
            _ASSERT(hook);
            GetHooksForSet().m_LocalHooks.insert(key, hook);
        }
    void SetGlobalHook(THook* hook)
        {
            _ASSERT(hook);
            GetHooksForSet().m_GlobalHook.Reset(hook);
        }

    void ResetLocalHook(key_type& key)
        {
            SHooks& hooks = GetHooksForReset();
            hooks.m_LocalHooks.erase(key);
            if ( hooks.empty() )
                ResetHooks();
        }
    void ResetGlobalHook(void)
        {
            SHooks& hooks = GetHooksForReset();
            hooks.m_GlobalHook.Reset(0);
            if ( hooks.m_LocalHooks.empty() ) // m_GlobalHook already empty
                ResetHooks();
        }

    THook* GetGlobalHook(void) const
        {
            const SHooks* hooks = GetHooksForGet();
            if ( hooks )
                return hooks->m_GlobalHook.GetPointer();
            return 0;
        }
    THook* GetHook(key_type& key) const
        {
            const SHooks* hooks = GetHooksForGet();
            if ( hooks ) {
                typedef typename SHooks::TMap::const_iterator TMapCI;
                TMapCI i = hooks->m_LocalHooks.find(key);
                if ( i != hooks->m_LocalHooks.end() )
                    return i->second.GetPointer();
                return hooks->m_GlobalHook.GetPointer();
            }
            return 0;
        }

private:
    TFunction m_CurrentFunction;   // current function
    TFunction m_SecondaryFunction;

    // CHookData can be in two states:
    // 1. m_Hooks.get() == 0
    //     This state means that no hooks were installed
    //     m_CurrentFunction == type specific function
    //     m_SecondaryFunction == function checking hooks
    // 2. m_Hooks.get() != 0
    //     This state means that some hooks were installed
    //     m_CurrentFunction == function checking hooks
    //     m_SecondaryFunction == original type specific function

    auto_ptr<SHooks> m_Hooks;
};

//#include <serial/hookdata.inl>

END_NCBI_SCOPE

#endif  /* HOOKDATA__HPP */
