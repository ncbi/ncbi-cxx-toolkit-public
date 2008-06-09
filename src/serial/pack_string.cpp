/*  $Id$
* ===========================================================================
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
* ===========================================================================
*
*  Author:  Eugene Vasilchenko
*
*  File Description: Serialization hooks to make strings with equal value
*                    to share representation object.
*
*/

#include <ncbi_pch.hpp>
#include <serial/pack_string.hpp>
#include <serial/objistr.hpp>
#include <serial/objectiter.hpp>

BEGIN_NCBI_SCOPE

static const char* const STRING_PACK_ENV = "NCBI_SERIAL_PACK_STRINGS";
static const char* const ENV_YES = "YES";

static const size_t kDefaultLengthLimit = 32;
static const size_t kDefaultCountLimit = 32;

CPackString::CPackString(void)
    : m_LengthLimit(kDefaultCountLimit), m_CountLimit(kDefaultCountLimit),
      m_Skipped(0), m_CompressedIn(0),
      m_CompressedOut(0)
{
}


CPackString::CPackString(size_t length_limit, size_t count_limit)
    : m_LengthLimit(length_limit), m_CountLimit(count_limit),
      m_Skipped(0), m_CompressedIn(0),
      m_CompressedOut(0)
{
}


CPackString::~CPackString(void)
{
}


CNcbiOstream& CPackString::DumpStatistics(CNcbiOstream& out) const
{
    size_t total = 0;
    typedef multiset< pair<size_t, string> > TStat;
    TStat stat;
    ITERATE ( TStrings, i, m_Strings ) {
        stat.insert(TStat::value_type(i->GetCount(), i->GetString()));
        total += i->GetCount();
    }
    ITERATE ( TStat, i, stat ) {
        out << setw(10) << i->first << " : \"" << i->second << "\"\n";
    }
    out << setw(10) << total << " = " << m_CompressedIn << " -> " << m_CompressedOut << " strings\n";
    out << setw(10) << m_Skipped << " skipped\n";
    return out;
}


bool CPackString::s_GetEnvFlag(const char* env, bool def_val)
{
    const char* val = ::getenv(env);
    if ( !val ) {
        return def_val;
    }
    string s(val);
    return s == "1" || NStr::CompareNocase(s, ENV_YES) == 0;
}


bool CPackString::TryStringPack(void)
{
    static bool use_string_pack = s_GetEnvFlag(STRING_PACK_ENV, true);
    if ( !use_string_pack ) {
        return false;
    }

    string s1("test"), s2;
    s2 = s1;
    if ( s1.data() != s2.data() ) {
        // strings don't use reference counters
        return (use_string_pack = false);
    }

    return true;
}


void CPackString::x_RefCounterError(void)
{
    THROW1_TRACE(runtime_error,
                 "CPackString: bad ref counting");
}


bool CPackString::x_Assign(string& s, const string& src)
{
    if ( TryStringPack() ) {
        const_cast<string&>(src) = s;
        s = src;
        if ( s.data() != src.data() ) {
            x_RefCounterError();
        }
        return true;
    }
    else {
        return false;
    }
}


bool CPackString::Pack(string& s)
{
    if ( s.size() <= GetLengthLimit() ) {
        SNode key(s);
        iterator iter = m_Strings.lower_bound(key);
        bool found = iter != m_Strings.end() && *iter == key;
        if ( found ) {
            AddOld(s, iter);
            return false;
        }
        else if ( GetCount() < GetCountLimit() ) {
            iter = m_Strings.insert(iter, key);
            ++m_CompressedOut;
            iter->SetString(s);
            AddOld(s, iter);
            return true;
        }
    }
    Skipped();
    return false;
}


bool CPackString::Pack(string& s, const char* data, size_t size)
{
    if ( size <= GetLengthLimit() ) {
        SNode key(data, size);
        iterator iter = m_Strings.lower_bound(key);
        bool found = iter != m_Strings.end() && *iter == key;
        if ( found ) {
            AddOld(s, iter);
            return false;
        }
        else if ( GetCount() < GetCountLimit() ) {
            iter = m_Strings.insert(iter, key);
            ++m_CompressedOut;
            iter->SetString();
            AddOld(s, iter);
            return true;
        }
    }
    Skipped();
    s.assign(data, size);
    return false;
}


bool CPackString::AddNew(string& s, const char* data, size_t size,
                         iterator iter)
{
    SNode key(data, size);
    _ASSERT(size <= GetLengthLimit());
    _ASSERT(iter == m_Strings.lower_bound(key));
    _ASSERT(!(iter != m_Strings.end() && *iter == key));
    if ( GetCount() < GetCountLimit() ) {
        iter = m_Strings.insert(iter, key);
        ++m_CompressedOut;
        iter->SetString();
        AddOld(s, iter);
        return true;
    }
    Skipped();
    s.assign(data, size);
    return false;
}


CPackStringClassHook::CPackStringClassHook(void)
{
}


CPackStringClassHook::CPackStringClassHook(size_t length_limit,
                                           size_t count_limit)
    : m_PackString(length_limit, count_limit)
{
}


CPackStringClassHook::~CPackStringClassHook(void)
{
#if 0
    NcbiCout << "CPackStringClassHook statistics:\n" <<
        m_PackString << NcbiEndl;
#endif
}


CPackStringChoiceHook::CPackStringChoiceHook(void)
{
}


CPackStringChoiceHook::CPackStringChoiceHook(size_t length_limit,
                                             size_t count_limit)
    : m_PackString(length_limit, count_limit)
{
}


CPackStringChoiceHook::~CPackStringChoiceHook(void)
{
#if 0
    NcbiCout << "CPackStringChoiceHook statistics:\n" <<
        m_PackString << NcbiEndl;
#endif
}


END_NCBI_SCOPE
