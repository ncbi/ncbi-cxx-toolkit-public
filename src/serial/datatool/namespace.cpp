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
* Revision 1.8  2005/03/28 20:41:24  gouriano
* Fixed namespace string parsing in CNamespace ctor
*
* Revision 1.7  2004/05/17 21:03:14  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.6  2003/03/11 20:06:47  kuznets
* iterate -> ITERATE
*
* Revision 1.5  2000/08/25 15:59:23  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.4  2000/04/28 16:58:17  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.3  2000/04/18 19:24:37  vasilche
* Added BEGIN_SCOPE and END_SCOPE macros to allow source brawser gather names from namespaces.
*
* Revision 1.2  2000/04/12 15:36:52  vasilche
* Added -on <namespace> argument to datatool.
* Removed unnecessary namespace specifications in generated files.
*
* Revision 1.1  2000/04/07 19:26:30  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/datatool/namespace.hpp>

BEGIN_NCBI_SCOPE

const string CNamespace::KNCBINamespaceName("ncbi");
const string CNamespace::KNCBINamespaceDefine("NCBI_NS_NCBI");
const string CNamespace::KSTDNamespaceName("std");
const string CNamespace::KSTDNamespaceDefine("NCBI_NS_STD");

const CNamespace CNamespace::KEmptyNamespace;
const CNamespace CNamespace::KNCBINamespace(KNCBINamespaceName);
const CNamespace CNamespace::KSTDNamespace(KSTDNamespaceName);

CNamespace::CNamespace(void)
{
}

CNamespace::CNamespace(const string& ns)
{
    SIZE_TYPE pos = 0;
    if ( NStr::StartsWith(ns, "::") )
        pos = 2; // skip leading ::

    SIZE_TYPE end = ns.find("::", pos);
    while ( end != NPOS ) {
        m_Namespaces.push_back(ns.substr(pos, end-pos));
        pos = end + 2;
        end = ns.find("::", pos);
    }
    m_Namespaces.push_back(ns.substr(pos));
    if ( m_Namespaces[0] == KNCBINamespaceDefine )
        m_Namespaces[0] = KNCBINamespaceName;
    else if ( m_Namespaces[0] == KSTDNamespaceDefine )
        m_Namespaces[0] = KSTDNamespaceName;
}

size_t CNamespace::EqualLevels(const CNamespace& ns) const
{
    size_t end = min(GetNamespaceLevel(), ns.GetNamespaceLevel());
    for ( size_t i = 0; i < end; ++i ) {
        if ( GetNamespaces()[i] != ns.GetNamespaces()[i] )
            return i;
    }
    return end;
}

void CNamespace::Set(const CNamespace& ns, CNcbiOstream& out, bool mainHeader)
{
    size_t equal = EqualLevels(ns);
    CloseAllAbove(equal, out);
    for ( size_t i = equal, end = ns.GetNamespaceLevel(); i < end; ++i )
        Open(ns.GetNamespaces()[i], out, mainHeader);
}

string CNamespace::GetNamespaceRef(const CNamespace& ns) const
{
    size_t equal = EqualLevels(ns);
    string s;
    if ( equal == GetNamespaceLevel() ) {
        // internal namespace
    }
    else {
        // reference from root
        equal = 0;
        if ( ns.InNCBI() ) {
            s = KNCBINamespaceDefine;
            equal = 1;
        }
        else if ( ns.InSTD() ) {
            s = KSTDNamespaceDefine;
            equal = 1;
        }
        if ( equal == 1 ) {
            // std or ncbi
            if ( InNCBI() )
                s.erase();
            else
                s += "::";
        }
        else {
            // from root
            s = "::";
        }
    }
    for ( size_t i = equal, end = ns.GetNamespaceLevel(); i < end; ++i ) {
        s += ns.GetNamespaces()[i];
        s += "::";
    }
    return s;
}

void CNamespace::Open(const string& s, CNcbiOstream& out, bool mainHeader)
{
    m_Namespaces.push_back(s);
    if ( IsNCBI() ) {
        out <<
            "BEGIN_NCBI_SCOPE\n"
            "\n";
    }
    else {
        if ( mainHeader ) {
            out <<
                "#ifndef BEGIN_"<<s<<"_SCOPE\n"
                "#  define BEGIN_"<<s<<"_SCOPE BEGIN_SCOPE("<<s<<")\n"
                "#  define END_"<<s<<"_SCOPE END_SCOPE("<<s<<")\n"
                "#endif\n";
        }
        out <<
            "BEGIN_"<<s<<"_SCOPE // namespace "<<*this<<"\n"
            "\n";
    }
}

void CNamespace::Close(CNcbiOstream& out)
{
    _ASSERT(!m_Namespaces.empty());
    if ( IsNCBI() ) {
        out <<
            "END_NCBI_SCOPE\n"
            "\n";
    }
    else {
        out <<
            "END_"<<m_Namespaces.back()<<"_SCOPE // namespace "<<*this<<"\n"
            "\n";
    }
    m_Namespaces.pop_back();
}

void CNamespace::CloseAllAbove(size_t level, CNcbiOstream& out)
{
    for ( size_t size = GetNamespaceLevel(); size > level; --size )
        Close(out);
}

CNcbiOstream& CNamespace::PrintFullName(CNcbiOstream& out) const
{
    ITERATE ( TNamespaces, i, GetNamespaces() )
        out << *i << "::";
    return out;
}

void CNamespace::ToStringTo(string& s) const
{
    ITERATE ( TNamespaces, i, GetNamespaces() ) {
        s += *i;
        s += "::";
    }
}

END_NCBI_SCOPE
