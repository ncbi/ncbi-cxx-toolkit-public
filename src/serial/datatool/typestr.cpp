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
*   Type info for class generation: includes, used classes, C code etc.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  1999/11/15 19:36:20  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include "typestr.hpp"
#include "code.hpp"

inline
string CTypeStrings::GetRef(void) const
{
    switch ( type ) {
    case eStdType:
        return "STD, (" + cType + ')';
    case eClassType:
        return "CLASS, (" + cType + ')';
    default:
        return macro;
    }
}

void CTypeStrings::SetStd(const string& c)
{
    type = eStdType;
    cType = c;
}

void CTypeStrings::SetClass(const string& c)
{
    type = eClassType;
    cType = c;
}

void CTypeStrings::SetEnum(const string& c, const string& e)
{
    type = eEnumType;
    cType = c;
    macro = "ENUM, (" + c + ", " + e + ")";
}

void CTypeStrings::SetComplex(const string& c, const string& m)
{
    type = eComplexType;
    cType = c;
    macro = m;
}

void CTypeStrings::SetComplex(const string& c, const string& m,
                              const CTypeStrings& arg)
{
    string cc = c + "< " + arg.cType + " >";
    string mm = m + ", (" + arg.GetRef() + ')';
    AddIncludes(arg);
    type = eComplexType;
    cType = cc;
    macro = mm;
}

void CTypeStrings::SetComplex(const string& c, const string& m,
                              const CTypeStrings& arg1,
                              const CTypeStrings& arg2)
{
    string cc = c + "< " + arg1.cType + ", " + arg2.cType + " >";
    string mm = m + ", (" + arg1.GetRef() + ", " + arg2.GetRef() + ')';
    AddIncludes(arg1);
    AddIncludes(arg2);
    type = eComplexType;
    cType = cc;
    macro = mm;
}

template<class C>
inline
void insert(C& dst, const C& src)
{
	for ( typename C::const_iterator i = src.begin(); i != src.end(); ++i )
		dst.insert(*i);
}

void CTypeStrings::AddIncludes(const CTypeStrings& arg)
{
    insert(m_HPPIncludes, arg.m_HPPIncludes);
    insert(m_CPPIncludes, arg.m_CPPIncludes);
    insert(m_ForwardDeclarations, arg.m_ForwardDeclarations);
}

void CTypeStrings::ToSimple(void)
{
    switch (type ) {
    case eStdType:
    case ePointerType:
    case eEnumType:
        return;
    case eClassType:
    case eComplexType:
        macro = "POINTER, (" + GetRef() + ')';
        cType += '*';
        m_CPPIncludes = m_HPPIncludes;
        m_HPPIncludes.clear();
        type = ePointerType;
        break;
    }
}

void CTypeStrings::AddMember(CClassCode& code,
                             const string& member) const
{
    x_AddMember(code, "NCBI_NS_NCBI::NcbiEmptyString", member);
}

void CTypeStrings::AddMember(CClassCode& code,
                             const string& name, const string& member) const
{
    x_AddMember(code, '"' + name + '"', member);
}

void CTypeStrings::x_AddMember(CClassCode& code,
                               const string& name, const string& member) const
{
    code.AddForwardDeclarations(m_ForwardDeclarations);
    code.AddHPPIncludes(m_HPPIncludes);
    code.AddCPPIncludes(m_CPPIncludes);
    code.ClassPrivate() <<
        "    " << cType << ' ' << member << ';' << NcbiEndl;

    if ( type == eStdType || type == eClassType ) {
        code.TypeInfoBody() <<
            "    ADD_N_STD_M(" << name << ", " << member << ')';
    }
    else {
        code.TypeInfoBody() <<
            "    ADD_N_M(" << name << ", " << member << ", " << macro << ')';
    }
}
