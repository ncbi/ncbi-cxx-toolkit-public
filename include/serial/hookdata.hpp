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
#include <memory>
#include <algorithm>
#include <map>

BEGIN_NCBI_SCOPE

template<class Key, class Hook, typename Function>
class CHookData
{
public:
    typedef Key TKey;
    typedef Hook THook;
    typedef Function TFunction;
    typedef map<TKey*, CRef<THook> > THookMap;

    CHookData(TFunction typeFunction, TFunction hookFunction)
        : m_CurrentFunction(typeFunction), m_SecondaryFunction(hookFunction)
        {
        }

    TFunction GetCurrentFunction(void) const
        {
            return m_CurrentFunction;
        }

    TFunction& GetDefaultFunction(void)
        {
            return m_Hooks.get()? m_SecondaryFunction: m_CurrentFunction;
        }
    TFunction GetDefaultFunction(void) const
        {
            return m_Hooks.get()? m_SecondaryFunction: m_CurrentFunction;
        }

    void SetHook(TKey* key, THook* hook)
        {
            _ASSERT(hook);
            THookMap* hookMap = m_Hooks.get();
            if ( !hookMap ) {
                m_Hooks.reset(hookMap = new THookMap);
                swap(m_CurrentFunction, m_SecondaryFunction);
            }
            hookMap->insert(THookMap::value_type(key, hook));
        }

    void ResetHook(TKey* key)
        {
            THookMap* hookMap = m_Hooks.get();
            _ASSERT(hookMap);
            hookMap->erase(key);
            if ( hookMap->empty() ) {
                swap(m_CurrentFunction, m_SecondaryFunction);
                m_Hooks.reset(0);
            }
        }

    THook* GetHook(TKey* key) const
        {
            const THookMap* hookMap = m_Hooks.get();
            if ( hookMap ) {
                THookMap::const_iterator hi;
                hi = hookMap->find(key);
                if ( hi != hookMap->end() )
                    return hi->second.GetPointer();
                hi = hookMap->find(0);
                if ( hi != hookMap->end() )
                    return hi->second.GetPointer();
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

    auto_ptr<THookMap> m_Hooks;
};

//#include <serial/hookdata.inl>

END_NCBI_SCOPE

#endif  /* HOOKDATA__HPP */
