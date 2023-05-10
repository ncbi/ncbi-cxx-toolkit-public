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
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbicntr.hpp>

#include <serial/impl/hookdatakey.hpp>
#include <serial/impl/pathhook.hpp>


/** @addtogroup HookSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CObject;
class CLocalHookSetBase;

class NCBI_XSERIAL_EXPORT CHookDataBase
{
public:
    CHookDataBase(void);
    ~CHookDataBase(void);

    bool HaveHooks(void) const
        {
            //return m_Data != 0;
            return m_HookCount.Get() != 0;
        }

protected:
    bool Empty(void) const
        {
            //return m_Data == 0;
            return m_HookCount.Get() == 0;
        }

    typedef CLocalHookSetBase TLocalHooks;
    typedef CObject THook;

    void SetLocalHook(TLocalHooks& key, THook* hook);
    void SetGlobalHook(THook* hook);
    void SetPathHook(CObjectStack* stk, const string& path, THook* hook);

    void ResetLocalHook(TLocalHooks& key);
    void ResetGlobalHook(void);
    void ResetPathHook(CObjectStack* stk, const string& path);

    void ForgetLocalHook(TLocalHooks& key);

    THook* GetHook(const TLocalHooks& key) const
        {
            const THook* hook = key.GetHook(this);
            if ( !hook ) {
                hook = m_GlobalHook.GetPointer();
            }
            return const_cast<THook*>(hook);
        }
    THook* GetPathHook(CObjectStack& stk) const
        {
            return const_cast<THook*>(m_PathHooks.GetHook(stk));
        }

private:
    friend class CLocalHookSetBase;

    CRef<THook>    m_GlobalHook;
    CPathHook      m_PathHooks;
    CAtomicCounter_WithAutoInit m_HookCount; // including global hook
};


template<class Hook, typename Function>
class CHookData : public CHookDataBase
{
    typedef CHookDataBase CParent;
public:
    typedef Hook THook;
    typedef Function TFunction;
    typedef CLocalHookSet<THook> TLocalHooks;

    CHookData(TFunction typeFunction, TFunction hookFunction)
        : m_CurrentFunction(typeFunction),
          m_DefaultFunction(typeFunction),
          m_HookFunction(hookFunction)
        {
        }

    TFunction GetCurrentFunction(void) const
        {
            return m_CurrentFunction.load(memory_order_relaxed);
        }

    TFunction GetDefaultFunction(void) const
        {
            return m_DefaultFunction;
        }

    void SetDefaultFunction(TFunction func)
        {
            m_DefaultFunction = func;
            if ( !HaveHooks() ) {
                x_SetCurrentFunction(func);
            }
        }

    void SetLocalHook(TLocalHooks& key, THook* hook)
        {
            CParent::SetLocalHook(key, hook);
            x_SetCurrentFunction(m_HookFunction);
        }
    void SetGlobalHook(THook* hook)
        {
            CParent::SetGlobalHook(hook);
            x_SetCurrentFunction(m_HookFunction);
        }
    void SetPathHook(CObjectStack* stk, const string& path, THook* hook)
        {
            CParent::SetPathHook(stk, path, hook);
            x_SetCurrentFunction();
        }

    void ResetLocalHook(TLocalHooks& key)
        {
            CParent::ResetLocalHook(key);
            x_SetCurrentFunction();
        }
    void ResetGlobalHook(void)
        {
            CParent::ResetGlobalHook();
            x_SetCurrentFunction();
        }
    void ResetPathHook(CObjectStack* stk, const string& path)
        {
            CParent::ResetPathHook(stk, path);
            x_SetCurrentFunction();
        }

    THook* GetHook(const TLocalHooks& key) const
        {
            return static_cast<THook*>(CParent::GetHook(key));
        }
    THook* GetPathHook(CObjectStack& stk) const
        {
            return static_cast<THook*>(CParent::GetPathHook(stk));
        }

protected:
    void x_SetCurrentFunction(TFunction func)
        {
            m_CurrentFunction.store(func, memory_order_relaxed);
        }
    void x_SetCurrentFunction()
        {
            x_SetCurrentFunction(HaveHooks()? m_HookFunction: m_DefaultFunction);
        }

private:
    atomic<TFunction> m_CurrentFunction;   // current function
    TFunction m_DefaultFunction;   // function without hook processing
    TFunction m_HookFunction;      // function with hook processing
};


template<class Hook, typename Function>
class CHookPairData : public CHookDataBase
{
    typedef CHookDataBase CParent;
public:
    typedef Hook THook;
    typedef Function TFunction;
    typedef pair<TFunction, TFunction> TFunctions;
    typedef CLocalHookSet<THook> TLocalHooks;

    CHookPairData(TFunctions typeFunctions, TFunctions hookFunctions)
        : m_CurrentFunction1st(typeFunctions.first),
          m_CurrentFunction2nd(typeFunctions.second),
          m_DefaultFunctions(typeFunctions),
          m_HookFunctions(hookFunctions)
        {
        }

    TFunction GetCurrentFunction1st(void) const
        {
            return m_CurrentFunction1st.load(memory_order_relaxed);
        }
    TFunction GetCurrentFunction2nd(void) const
        {
            return m_CurrentFunction2nd.load(memory_order_relaxed);
        }

    TFunction GetDefaultFunction1st(void) const
        {
            return m_DefaultFunctions.first;
        }
    TFunction GetDefaultFunction2nd(void) const
        {
            return m_DefaultFunctions.second;
        }

    void SetDefaultFunctions(TFunctions funcs)
        {
            m_DefaultFunctions = funcs;
            if ( !HaveHooks() ) {
                x_SetCurrentFunctions(funcs);
            }
        }
    void SetDefaultFunction1st(TFunction func)
        {
            m_DefaultFunctions.first = func;
            if ( !HaveHooks() ) {
                x_SetCurrentFunctions(m_DefaultFunctions);
            }
        }
    void SetDefaultFunction2nd(TFunction func)
        {
            m_DefaultFunctions.second = func;
            if ( !HaveHooks() ) {
                x_SetCurrentFunctions(m_DefaultFunctions);
            }
        }

    void SetLocalHook(TLocalHooks& key, THook* hook)
        {
            CParent::SetLocalHook(key, hook);
            x_SetCurrentFunctions(m_HookFunctions);
        }
    void SetGlobalHook(THook* hook)
        {
            CParent::SetGlobalHook(hook);
            x_SetCurrentFunctions(m_HookFunctions);
        }
    void SetPathHook(CObjectStack* stk, const string& path, THook* hook)
        {
            CParent::SetPathHook(stk, path, hook);
            x_SetCurrentFunctions();
        }

    void ResetLocalHook(TLocalHooks& key)
        {
            CParent::ResetLocalHook(key);
            x_SetCurrentFunctions();
        }
    void ResetGlobalHook(void)
        {
            CParent::ResetGlobalHook();
            x_SetCurrentFunctions();
        }
    void ResetPathHook(CObjectStack* stk, const string& path)
        {
            CParent::ResetPathHook(stk, path);
            x_SetCurrentFunctions();
        }

    THook* GetHook(const TLocalHooks& key) const
        {
            return static_cast<THook*>(CParent::GetHook(key));
        }
    THook* GetPathHook(CObjectStack& stk) const
        {
            return static_cast<THook*>(CParent::GetPathHook(stk));
        }

protected:
    void x_SetCurrentFunctions(TFunctions funcs)
        {
            m_CurrentFunction1st.store(funcs.first, memory_order_relaxed);
            m_CurrentFunction2nd.store(funcs.second, memory_order_relaxed);
        }
    void x_SetCurrentFunctions()
        {
            x_SetCurrentFunctions(HaveHooks()? m_HookFunctions: m_DefaultFunctions);
        }

private:
    atomic<TFunction> m_CurrentFunction1st;  // current 1st function
    atomic<TFunction> m_CurrentFunction2nd;  // current 2nd function
    TFunctions m_DefaultFunctions;   // functions without hook processing
    TFunctions m_HookFunctions;      // functions with hook processing
};


END_NCBI_SCOPE

#endif  /* HOOKDATA__HPP */


/* @} */
