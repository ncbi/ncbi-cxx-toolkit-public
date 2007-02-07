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
    CAtomicCounter m_HookCount; // including global hook
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
            return m_CurrentFunction;
        }

    TFunction GetDefaultFunction(void) const
        {
            return m_DefaultFunction;
        }

    void SetDefaultFunction(const TFunction& func)
        {
            m_DefaultFunction = func;
            if ( !HaveHooks() ) {
                m_CurrentFunction = func;
            }
        }

    void SetLocalHook(TLocalHooks& key, THook* hook)
        {
            CParent::SetLocalHook(key, hook);
            m_CurrentFunction = m_HookFunction;
        }
    void SetGlobalHook(THook* hook)
        {
            CParent::SetGlobalHook(hook);
            m_CurrentFunction = m_HookFunction;
        }
    void SetPathHook(CObjectStack* stk, const string& path, THook* hook)
        {
            CParent::SetPathHook(stk, path, hook);
            m_CurrentFunction = m_HookFunction;
        }

    void ResetLocalHook(TLocalHooks& key)
        {
            CParent::ResetLocalHook(key);
            m_CurrentFunction = HaveHooks()? m_HookFunction: m_DefaultFunction;
        }
    void ResetGlobalHook(void)
        {
            CParent::ResetGlobalHook();
            m_CurrentFunction = HaveHooks()? m_HookFunction: m_DefaultFunction;
        }
    void ResetPathHook(CObjectStack* stk, const string& path)
        {
            CParent::ResetPathHook(stk, path);
            m_CurrentFunction = HaveHooks()? m_HookFunction: m_DefaultFunction;
        }

    THook* GetHook(const TLocalHooks& key) const
        {
            return static_cast<THook*>(CParent::GetHook(key));
        }
    THook* GetPathHook(CObjectStack& stk) const
        {
            return static_cast<THook*>(CParent::GetPathHook(stk));
        }

private:
    TFunction m_CurrentFunction;   // current function
    TFunction m_DefaultFunction;
    TFunction m_HookFunction;
};


END_NCBI_SCOPE

#endif  /* HOOKDATA__HPP */


/* @} */


/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2006/10/05 19:23:37  gouriano
* Moved from parent folder
*
* Revision 1.8  2004/01/05 14:24:07  gouriano
* Added possibility to set serialization hooks by stack path
*
* Revision 1.7  2003/07/29 18:47:46  vasilche
* Fixed thread safeness of object stream hooks.
*
* Revision 1.6  2003/04/15 14:15:13  siyan
* Added doxygen support
*
* Revision 1.5  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.4  2000/10/13 16:28:30  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* Revision 1.3  2000/10/03 17:22:31  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
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
