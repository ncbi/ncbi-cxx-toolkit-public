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
*   Class for storing local hooks in CObjectIStream
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2004/05/17 21:03:02  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.1  2003/08/19 18:32:38  vasilche
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

#include <serial/hookdatakey.hpp>
#include <serial/hookdata.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
// CLocalHookSetBase
/////////////////////////////////////////////////////////////////////////////


CLocalHookSetBase::CLocalHookSetBase(void)
{
}


CLocalHookSetBase::~CLocalHookSetBase(void)
{
    Clear();
}


inline
CLocalHookSetBase::THooks::iterator
CLocalHookSetBase::x_Find(const THookData* key)
{
    return lower_bound(m_Hooks.begin(), m_Hooks.end(), key, Compare());
}


inline
CLocalHookSetBase::THooks::const_iterator
CLocalHookSetBase::x_Find(const THookData* key) const
{
    return lower_bound(m_Hooks.begin(), m_Hooks.end(), key, Compare());
}


inline
bool CLocalHookSetBase::x_Found(THooks::const_iterator it,
                                const THookData* key) const
{
    return it != m_Hooks.end() && it->first == key;
}


void CLocalHookSetBase::SetHook(THookData* key, THook* hook)
{
    THooks::iterator it = x_Find(key);
    _ASSERT(!x_Found(it, key));
    m_Hooks.insert(it, TValue(key, CRef<CObject>(hook)));
}


void CLocalHookSetBase::ResetHook(THookData* key)
{
    THooks::iterator it = x_Find(key);
    _ASSERT(x_Found(it, key));
    m_Hooks.erase(it);
}


const CObject* CLocalHookSetBase::GetHook(const THookData* key) const
{
    THooks::const_iterator it = x_Find(key);
    return x_Found(it, key)? it->second.GetPointer(): 0;
}


void CLocalHookSetBase::Clear(void)
{
    ITERATE ( THooks, it, m_Hooks ) {
        _ASSERT(it->first);
        it->first->ForgetLocalHook(*this);
    }
    m_Hooks.clear();
}


END_NCBI_SCOPE
