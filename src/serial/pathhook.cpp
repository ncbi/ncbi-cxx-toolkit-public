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
 * Author: Andrei Gourianov
 *
 * File Description:
 *   Helper classes to set serialization hooks by stack path
 */

#include <ncbi_pch.hpp>
#include <serial/impl/pathhook.hpp>
#include <serial/impl/objstack.hpp>
#include <serial/impl/item.hpp>
#include <serial/impl/classinfob.hpp>

BEGIN_NCBI_SCOPE


static const char
    s_Separator = '.',
    s_One       = '?',
    s_Many      = '*';
static const char* s_AllStr  = "?";


CPathHook::CPathHook(void)
{
    m_Empty = true;
    m_Regular = m_All = m_Wildcard = false;
}

CPathHook::~CPathHook(void)
{
}

bool CPathHook::SetHook(CObjectStack* stk, const string& path, CObject* hook)
{
    bool state = false;
    iterator it = find(stk);
    for ( ;it != end() && it->first == stk; ++it) {
        if ((it->second).first == path) {
            if ((it->second).second == hook) {
                return state; // this hook already set - do nothing
            }
            erase(it); // erase existing hook
            state = !state;
            break;
        }
    }
    if (hook) { // set the new one
        insert(pair<CObjectStack* const, pair<string, CRef<CObject> > >(
            stk,pair<string, CRef<CObject> >(path,CRef<CObject>(hook))));
        state = !state;
    }
    bool wildcard = path.find(s_One)  != string::npos ||
                    path.find(s_Many) != string::npos;
    bool all = (path == s_AllStr);
    m_Regular = m_Regular || !wildcard;
    m_All = m_All || all;
    m_Wildcard = m_Wildcard || (wildcard && !all);
    m_Empty = empty();
    return state;
}

CObject* CPathHook::GetHook(CObjectStack& stk) const
{
    if ( IsEmpty() ) {
        return 0;
    }
    CObject* hook;
    if (m_All) {
        hook = x_Get(stk,s_AllStr);
        if (hook) {
            return hook;
        }
    }
    const string& path = stk.GetStackPath();
    if (m_Regular) {
        hook = x_Get(stk,path);
        if (hook) {
            return hook;
        }
    }
    if (m_Wildcard) {
        for (CObjectStack* stmp = &stk; ; stmp = 0) {
            const_iterator it;
            for (it = find(stmp); it != end() && it->first == stmp; ++it) {
                if (CPathHook::Match((it->second).first,path)) {
                    return const_cast<CObject*>((it->second).second.GetPointer());
                }
            }
            if (!stmp) {
                break;
            }
        }
    }
    return 0;
}

CObject* CPathHook::x_Get(CObjectStack& stk, const string& path) const
{
    for (CObjectStack* stmp = &stk; ; stmp = 0) {
        const_iterator it;
        for ( it = find(stmp); it != end() && it->first == stmp; ++it) {
            if ((it->second).first == path) {
                return const_cast<CObject*>((it->second).second.GetPointer());
            }
        }
        if (!stmp) {
            break;
        }
    }
    return 0;
}

bool CPathHook::Match(const string& mask, const string& path)
{
// Path is current stack path
// Mask contains one or more wildcards (if we are here, it does)
// so, for example:
//      mask="A.?.?.D" and path="A.B.C.D" match
//      mask="A.*.D"   and path="A.B.C.D" match

    const char *m00 = mask.c_str();
    const char *p00 = path.c_str();
    const char *m1 = m00 + mask.length() - 1;
    const char *p1 = p00 + path.length() - 1;
    const char *m0;
    const char *p0;

    for ( ; m1 >= m00 && p1 >= p00; --m1, --p1) {
        if (*m1 == s_One) {
            for ( --m1; m1 >= m00 && *m1 != s_Separator; --m1)
                ; // skip "s_One" wildcard
            for (; p1 >= p00 && *p1 != s_Separator; --p1)
                ; // skip one level
        } else if (*m1 == s_Many) {
            for ( --m1; m1 >= m00 && *m1 != s_Separator; --m1)
                ; // skip "s_Many" wildcard
            if (m1 < m00) {
                return true;
            }
            for (; p1 >= p00 && *p1 != s_Separator; --p1)
                ; // skip one level ("many" means "1 or more")
            if (p1 < p00) {
                return false;
            }
            for (m0 = m1-1; m0 >= m00 && *m0 != s_Separator; --m0)
                ; // find level name
            if (m0 < m00) {
                m0 = m00;
            }
            for (p0 = p1-1; p0 >= p00; p1 = p0) {
                for (p0 = p1-1; p0 >= p00 && *p0 != s_Separator; --p0)
                    ; // next level name
                if (p0 < p00) {
                    p0 = p00;
                }
                if (!strncmp(p0,m0,m1-m0+1)) {
                    m1 = m0;
                    p1 = p0;
                    break; // level found, keep looking
                }
                if (p0 == p00) {
                    return false; // no match
                }
            }
        } else if (*m1 != *p1) {
            return false;
        }
    }
    return m1 <= m00 && p1 <= p00;
}



CStreamPathHookBase::CStreamPathHookBase(void)
{
    m_Empty = true;
    m_Regular = m_All = m_Member = m_Wildcard = false;
}

CStreamPathHookBase::~CStreamPathHookBase(void)
{
}

bool CStreamPathHookBase::SetHook(const string& path, CObject* hook)
{
    bool state = false;
    iterator it = find(path);
    if (it != end()) {
        if (hook == it->second) {
            return state;
        }
        erase(it);
        state = !state;
    }
    if (hook) {
        insert(pair<string,CRef<CObject> >(path,CRef<CObject>(hook)));
        state = !state;
    }
    bool wildcard = path.find(s_One)  != string::npos ||
                    path.find(s_Many) != string::npos;
    bool all = (path == s_AllStr);
    m_Regular = m_Regular || !wildcard;
    m_All = m_All || all;
    m_Wildcard = m_Wildcard || (wildcard && !all);
    m_Empty = empty();
    return state;
}

CObject* CStreamPathHookBase::GetHook(CObjectStack& stk) const
{
    if ( IsEmpty() ) {
        return 0;
    }
    CObject* hook;
    if (m_All) {
        hook = x_Get(s_AllStr);
        if (hook) {
            return hook;
        }
    }
    const string& path = stk.GetStackPath();
    if (m_Regular) {
        hook = x_Get(path);
        if (hook) {
            return hook;
        }
    }
    if (m_Wildcard) {
        for (const_iterator it = begin(); it != end(); ++it) {
            if (CPathHook::Match(it->first,path)) {
                return const_cast<CObject*>(it->second.GetPointer());
            }
        }
    }
    return 0;
}

CObject* CStreamPathHookBase::x_Get(const string& path) const
{
    const_iterator it = find(path);
    return it != end() ? const_cast<CObject*>(it->second.GetPointer()) : 0;
}

CTypeInfo* CStreamPathHookBase::FindType(const CObjectStack& stk)
{
    CItemInfo* item = FindItem(stk);
    return item ? const_cast<CTypeInfo*>(item->GetTypeInfo()) : 0;
}

CItemInfo* CStreamPathHookBase::FindItem(const CObjectStack& stk)
{
    if (stk.TopFrame().HasMemberId()) {
        for (size_t i = 0; i < stk.GetStackDepth(); ++i) {
            const CObjectStackFrame& frame = stk.FetchFrameFromTop(i);
            if (!frame.HasTypeInfo()) {
                continue;
            }
            const CClassTypeInfoBase* classInfo =
                dynamic_cast<const CClassTypeInfoBase*>(frame.GetTypeInfo());
            if (classInfo) {
                const string& name(stk.TopFrame().GetMemberId().GetName());
                return (classInfo->GetItems().Find(name) != kInvalidMember) ?
                    const_cast<CItemInfo*>(classInfo->GetItemInfo(name)) : 0;
            }
            break;
        }
    }
    return 0;
}

END_NCBI_SCOPE
