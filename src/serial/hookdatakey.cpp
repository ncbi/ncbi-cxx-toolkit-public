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
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <serial/impl/hookdatakey.hpp>
#include <serial/impl/hookdata.hpp>

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
