#ifndef NAMESPACE__HPP
#define NAMESPACE__HPP

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
* Revision 1.2  2000/04/12 15:36:41  vasilche
* Added -on <namespace> argument to datatool.
* Removed unnecessary namespace specifications in generated files.
*
* Revision 1.1  2000/04/07 19:26:10  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <vector>

BEGIN_NCBI_SCOPE

class CNamespace
{
public:
    typedef vector<string> TNamespaces;

    CNamespace(void);
    CNamespace(const string& s);

    void Set(const CNamespace& ns, CNcbiOstream& out);

    string GetNamespaceRef(const CNamespace& ns) const;

    void Reset(void)
        {
            m_Namespaces.clear();
        }
    void Reset(CNcbiOstream& out)
        {
            CloseAllAbove(0, out);
        }

    CNcbiOstream& PrintFullName(CNcbiOstream& out) const;

    operator string(void) const
        {
            string s;
            ToStringTo(s);
            return s;
        }

    string ToString(void) const
        {
            string s;
            ToStringTo(s);
            return s;
        }

    bool operator==(const CNamespace& ns) const
        {
            size_t myLevel = GetNamespaceLevel();
            return ns.GetNamespaceLevel() == myLevel &&
                EqualLevels(ns) == myLevel;
        }

    static const CNamespace KEmptyNamespace;
    static const CNamespace KNCBINamespace;
    static const CNamespace KSTDNamespace;
    static const string KNCBINamespaceName;
    static const string KSTDNamespaceName;
    static const string KNCBINamespaceDefine;
    static const string KSTDNamespaceDefine;

    bool IsEmpty(void) const
        {
            return m_Namespaces.empty();
        }
    bool InNCBI(void) const
        {
            return m_Namespaces.size() > 0 &&
                m_Namespaces[0] == KNCBINamespaceName;
        }
    bool InSTD(void) const
        {
            return m_Namespaces.size() > 0 &&
                m_Namespaces[0] == KSTDNamespaceName;
        }
    bool IsNCBI(void) const
        {
            return m_Namespaces.size() == 1 &&
                m_Namespaces[0] == KNCBINamespaceName;
        }
    bool IsSTD(void) const
        {
            return m_Namespaces.size() == 1 &&
                m_Namespaces[0] == KSTDNamespaceName;
        }

    operator bool(void) const
        {
            return !IsEmpty();
        }
    operator bool(void)
        {
            return !IsEmpty();
        }
    bool operator!(void) const
        {
            return IsEmpty();
        }

protected:
    const TNamespaces& GetNamespaces(void) const
        {
            return m_Namespaces;
        }
    size_t GetNamespaceLevel(void) const
        {
            return m_Namespaces.size();
        }

    void Open(const string& s, CNcbiOstream& out);
    void Close(CNcbiOstream& out);
    void CloseAllAbove(size_t level, CNcbiOstream& out);

    size_t EqualLevels(const CNamespace& ns) const;

    void ToStringTo(string& s) const;

    TNamespaces m_Namespaces;
};

inline
CNcbiOstream& operator<<(CNcbiOstream& out, const CNamespace& ns)
{
    return ns.PrintFullName(out);
}

//#include <serial/tool/namespace.inl>

END_NCBI_SCOPE

#endif  /* NAMESPACE__HPP */
