#ifndef PACK_STRING__HPP_INCLUDED
#define PACK_STRING__HPP_INCLUDED

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
*  File Description: Data reader from Pubseq_OS
*
*/

#include <serial/objhook.hpp>
#include <serial/impl/objecttype.hpp>
#include <serial/objistr.hpp>

#include <string>
#include <set>

BEGIN_NCBI_SCOPE

class NCBI_XSERIAL_EXPORT CPackString
{
public:
    CPackString(void);
    CPackString(size_t length_limit, size_t count_limit);
    ~CPackString(void);

    struct SNode {
        SNode(const string& s)
            : m_Length(s.size()),
              m_Chars(s.data()),
              m_CompressedIn(0)
            {
            }
        SNode(const SNode& n)
            : m_Length(n.m_Length),
              m_Chars(n.m_Chars),
              m_CompressedIn(0)
            {
            }
        SNode(const char* str, size_t len)
            : m_Length(len),
              m_Chars(str),
              m_CompressedIn(0)
            {
            }

        int x_Compare(const char* ptr) const
            {
                return memcmp(m_Chars, ptr, m_Length);
            }

        bool operator<(const SNode& n) const
            {
                return m_Length < n.m_Length ||
                    m_Length == n.m_Length && x_Compare(n.m_Chars) < 0;
            }
        bool operator==(const SNode& n) const
            {
                return m_Length == n.m_Length && x_Compare(n.m_Chars) == 0;
            }

        void AssignTo(string& s) const;

        void SetString(const string& s) const;
        void SetString(void) const;

        const string& GetString(void) const
            {
                return m_String;
            }
        size_t GetCount(void) const
            {
                return m_CompressedIn;
            }
        
    private:
        SNode& operator=(const SNode&);

        size_t m_Length;
        const char* m_Chars;
        string m_String;
        mutable size_t m_CompressedIn;
    };

    typedef SNode TKey;
    typedef set<TKey> TStrings;
    typedef TStrings::iterator iterator;
    
    void ReadString(CObjectIStream& in, string& s);

    // return true if src was updated
    static bool Assign(string& s, const string& src);

    size_t GetLengthLimit(void) const;
    size_t GetCountLimit(void) const;
    size_t GetCount(void) const;

    // return true if the string is new in cache
    bool Pack(string& s);
    bool Pack(string& s, const char* data, size_t size);
    
    pair<iterator, bool> Locate(const char* data, size_t size);
    void AddOld(string& s, const iterator& iter);
    bool AddNew(string& s, const char* data, size_t size, iterator iter);
    void Skipped(void);

    static bool s_GetEnvFlag(const char* env, bool def_val);

    static bool TryStringPack(void);

    CNcbiOstream& DumpStatistics(CNcbiOstream& out) const;

private:
    CPackString(const CPackString&);
    CPackString& operator=(const CPackString&);

    static void x_RefCounterError(void);
    // return true if src was updated
    static bool x_Assign(string& s, const string& src);

    size_t m_LengthLimit;
    size_t m_CountLimit;
    size_t m_Skipped;
    size_t m_CompressedIn;
    size_t m_CompressedOut;
    set<SNode> m_Strings;
};


class NCBI_XSERIAL_EXPORT CPackStringClassHook : public CReadClassMemberHook
{
public:
    CPackStringClassHook(void);
    CPackStringClassHook(size_t length_limit, size_t count_limit);
    ~CPackStringClassHook(void);
    
    void ReadClassMember(CObjectIStream& in, const CObjectInfoMI& member);

private:
    CPackString m_PackString;
};


class NCBI_XSERIAL_EXPORT CPackStringChoiceHook : public CReadChoiceVariantHook
{
public:
    CPackStringChoiceHook(void);
    CPackStringChoiceHook(size_t length_limit, size_t count_limit);
    ~CPackStringChoiceHook(void);

    void ReadChoiceVariant(CObjectIStream& in, const CObjectInfoCV& variant);

private:
    CPackString m_PackString;
};


/////////////////////////////////////////////////////////////////////////////
// CPackString
/////////////////////////////////////////////////////////////////////////////

inline
size_t CPackString::GetLengthLimit(void) const
{
    return m_LengthLimit;
}


inline
size_t CPackString::GetCountLimit(void) const
{
    return m_CountLimit;
}


inline
size_t CPackString::GetCount(void) const
{
    return m_CompressedOut;
}


inline
bool CPackString::Assign(string& s, const string& src)
{
    s = src;
    if ( s.data() != src.data() ) {
        return x_Assign(s, src);
    }
    else {
        return false;
    }
}


inline
void CPackString::SNode::AssignTo(string& s) const
{
    ++m_CompressedIn;
    if ( CPackString::Assign(s, m_String) ) {
        const_cast<SNode*>(this)->m_Chars = m_String.data();
    }
}


inline
void CPackString::SNode::SetString(const string& s) const
{
    _ASSERT(m_String.empty());
    _ASSERT(s.size() == m_Length && x_Compare(s.data()) == 0);
    const_cast<SNode*>(this)->m_String = s;
    const_cast<SNode*>(this)->m_Chars = m_String.data();
}


inline
void CPackString::SNode::SetString(void) const
{
    _ASSERT(m_String.empty());
    const_cast<SNode*>(this)->m_String.assign(m_Chars, m_Length);
    const_cast<SNode*>(this)->m_Chars = m_String.data();
}


inline
void CPackString::ReadString(CObjectIStream& in, string& s)
{
    in.ReadPackedString(s, *this);
}


inline
pair<CPackString::iterator, bool>
CPackString::Locate(const char* data, size_t size)
{
    pair<iterator, bool> ret;
    _ASSERT(size <= GetLengthLimit());
    SNode key(data, size);
    ret.first = m_Strings.lower_bound(key);
    ret.second = ret.first != m_Strings.end() && *ret.first == key;
    return ret;
}


inline
void CPackString::AddOld(string& s, const iterator& iter)
{
    ++m_CompressedIn;
    iter->AssignTo(s);
}


inline
void CPackString::Skipped(void)
{
    ++m_Skipped;
}


inline
CNcbiOstream& operator<<(CNcbiOstream& out, const CPackString& pack_string)
{
    return pack_string.DumpStatistics(out);
}


inline
void CPackStringClassHook::ReadClassMember(CObjectIStream& in,
                                           const CObjectInfoMI& member)
{
    m_PackString.ReadString(in, *CType<string>::GetUnchecked(*member));
}


inline
void CPackStringChoiceHook::ReadChoiceVariant(CObjectIStream& in,
                                              const CObjectInfoCV& variant)
{
    m_PackString.ReadString(in, *CType<string>::GetUnchecked(*variant));
}


END_NCBI_SCOPE

#endif // PACK_STRING__HPP_INCLUDED
