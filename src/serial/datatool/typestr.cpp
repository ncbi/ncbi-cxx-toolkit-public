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
* Revision 1.3  1999/11/16 15:41:17  vasilche
* Added plain pointer choice.
* By default we use C pointer instead of auto_ptr.
* Start adding initializers.
*
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
    switch ( GetType() ) {
    case eStdType:
        return "STD, (" + GetCType() + ')';
    case eEnumType:
        return "ENUM, (" + GetCType() + ',' + GetMacro() + ')';
    case eClassType:
        return "CLASS, (" + GetCType() + ')';
    default:
        return GetMacro();
    }
}

string CTypeStrings::GetInitializer(void) const
{
    switch ( GetType() ) {
    case eStdType:
    case ePointerType:
        return "0";
    case eEnumType:
        return GetCType() + "(0)";
    default:
        return NcbiEmptyString;
    }
}

void CTypeStrings::SetStd(const string& c, bool stringType)
{
    m_Type = stringType? eStringType: eStdType;
    m_CType = c;
}

void CTypeStrings::SetClass(const string& c)
{
    m_Type = eClassType;
    m_CType = c;
}

void CTypeStrings::SetEnum(const string& c, const string& e)
{
    m_Type = eEnumType;
    m_CType = c;
    m_Macro = e;
}

void CTypeStrings::SetTemplate(const string& c, const string& m)
{
    m_Type = eTemplateType;
    m_CType = c;
    m_Macro = m;
}

void CTypeStrings::SetTemplate(const string& c, const string& m,
                               const CTypeStrings& arg, bool simplePointer)
{
    string cc;
    if ( c == "*" )
        cc = arg.GetCType() + '*';
    else
        cc = c + "< " + arg.GetCType() + " >";
    string mm = m + ", (" + arg.GetRef() + ')';
    AddIncludes(arg);
    m_Type = simplePointer? ePointerTemplateType: eTemplateType;
    m_CType = cc;
    m_Macro = mm;
}

void CTypeStrings::SetTemplate(const string& c, const string& m,
                               const CTypeStrings& arg1,
                               const CTypeStrings& arg2)
{
    string cc = c + "< " + arg1.GetCType() + ", " + arg2.GetCType() + " >";
    string mm = m + ", (" + arg1.GetRef() + ", " + arg2.GetRef() + ')';
    AddIncludes(arg1);
    AddIncludes(arg2);
    m_Type = eTemplateType;
    m_CType = cc;
    m_Macro = mm;
}

void CTypeStrings::ToPointer(void)
{
    m_Macro = "POINTER, (" + GetRef() + ')';
    m_CType += '*';
    m_CPPIncludes = m_HPPIncludes;
    m_HPPIncludes.clear();
    m_Type = ePointerType;
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

bool CTypeStrings::CanBeKey(void) const
{
    switch ( GetType() ) {
    case eStdType:
    case eEnumType:
    case eStringType:
        return true;
    default:
        return false;
    }
}

bool CTypeStrings::CanBeInSTL(void) const
{
    switch ( GetType() ) {
    case eStdType:
    case eEnumType:
    case eStringType:
    case ePointerType:
    case ePointerTemplateType:
        return true;
    default:
        return false;
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
        "    " << GetCType() << ' ' << member << ';' << NcbiEndl;

    switch ( GetType() ) {
    case eStdType:
    case eStringType:
    case eClassType:
        code.TypeInfoBody() <<
            "    ADD_N_STD_M(" << name << ", " << member << ')';
        break;
    default:
        code.TypeInfoBody() <<
            "    ADD_N_M(" << name << ", " << member << ", " << GetMacro() << ')';
        break;
    }
}
