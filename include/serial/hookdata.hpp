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

#include <corelib/ncbistd.hpp>
#include <serial/hookdatakey.hpp>

BEGIN_NCBI_SCOPE

class CObject;
class CHookDataData;

class CHookDataBase
{
public:
    CHookDataBase(void);
    ~CHookDataBase(void);

    bool HaveHooks(void) const
        {
            return m_Data != 0;
        }

protected:
    bool Empty(void) const
        {
            return m_Data == 0;
        }

    typedef CHookDataKeyBase TKey;
    typedef CObject THook;
    typedef CHookDataData TData;

    bool SetLocalHook(TKey& key, THook* hook);
    bool SetGlobalHook(THook* hook);

    bool ResetLocalHook(TKey& key);
    bool ResetGlobalHook(void);

    THook* GetGlobalHook(void) const;
    THook* GetHook(TKey& key) const;

private:
    void ResetData(void);

    TData& GetDataForSet(void);
    TData& GetDataForReset(void);
    const TData* GetDataForGet(void) const
        {
            return m_Data;
        }

    // CHookData can be in two states:
    // 1. m_Hooks.get() == 0
    //     This state means that no hooks were installed
    //     m_CurrentFunction == type specific function
    //     m_SecondaryFunction == function checking hooks
    // 2. m_Hooks.get() != 0
    //     This state means that some hooks were installed
    //     m_CurrentFunction == function checking hooks
    //     m_SecondaryFunction == original type specific function

    TData* m_Data;
};

template<class Hook, typename Function>
class CHookData : public CHookDataBase
{
    typedef CHookDataBase CParent;
public:
    typedef Hook THook;
    typedef Function TFunction;
    typedef CHookDataKey<THook> TKey;

    CHookData(TFunction typeFunction, TFunction hookFunction)
        : m_CurrentFunction(typeFunction), m_SecondaryFunction(hookFunction)
        {
        }

    TFunction GetCurrentFunction(void) const
        {
            return m_CurrentFunction;
        }

    TFunction GetDefaultFunction(void) const
        {
            return HaveHooks()? m_SecondaryFunction: m_CurrentFunction;
        }
    TFunction& GetDefaultFunction(void)
        {
            return HaveHooks()? m_SecondaryFunction: m_CurrentFunction;
        }

private:
    void SwapFunctions(void)
        {
            TFunction temp = m_CurrentFunction;
            m_CurrentFunction = m_SecondaryFunction;
            m_SecondaryFunction = temp;
        }

public:
    void SetLocalHook(TKey& key, THook* hook)
        {
            if ( CParent::SetLocalHook(key, hook) )
                SwapFunctions();
        }
    void SetGlobalHook(THook* hook)
        {
            if ( CParent::SetGlobalHook(hook) )
                SwapFunctions();
        }

    void ResetLocalHook(TKey& key)
        {
            if ( CParent::ResetLocalHook(key) )
                SwapFunctions();
        }
    void ResetGlobalHook(void)
        {
            if ( CParent::ResetGlobalHook() )
                SwapFunctions();
        }

    THook* GetGlobalHook(void) const
        {
            return static_cast<THook*>(CParent::GetGlobalHook());
        }
    THook* GetHook(TKey& key) const
        {
            return static_cast<THook*>(CParent::GetHook(key));
        }

private:
    TFunction m_CurrentFunction;   // current function
    TFunction m_SecondaryFunction;
};

END_NCBI_SCOPE

#endif  /* HOOKDATA__HPP */
