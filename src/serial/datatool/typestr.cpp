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
* Revision 1.5  1999/12/01 17:36:28  vasilche
* Fixed CHOICE processing.
*
* Revision 1.4  1999/11/18 17:13:07  vasilche
* Fixed generation of ENUMERATED CHOICE and VisibleString.
* Added generation of initializers to zero for primitive types and pointers.
*
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
    case eStringType:
        return "STD, (" + GetCType() + ')';
    case eEnumType:
        return "ENUM, (" + GetCType() + ',' + GetMacro() + ')';
    case eClassType:
        return "CLASS, (" + GetCType() + ')';
    case ePointerType:
        if ( IsChoice() )
            return "CHOICE, (POINTER, (" + GetMacro() + "))";
        else
            return "POINTER, (" + GetMacro() + ')';
    default:
        if ( IsChoice() )
            return "CHOICE, (" + GetMacro() + ')';
        else
            return GetMacro();
    }
}

string CTypeStrings::GetInitializer(void) const
{
    switch ( GetType() ) {
    case eStdType:
    case ePointerType:
    case ePointerTemplateType:
        return "0";
    case eEnumType:
        return GetCType() + "(0)";
    default:
        return NcbiEmptyString;
    }
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

void CTypeStrings::SetStd(const string& c, bool stringType)
{
    m_Type = stringType? eStringType: eStdType;
    m_CType = c;
    m_Choice = false;
}

void CTypeStrings::SetClass(const string& c)
{
    m_Type = eClassType;
    m_CType = c;
    m_Choice = false;
}

void CTypeStrings::SetEnum(const string& c, const string& e)
{
    m_Type = eEnumType;
    m_CType = c;
    m_Macro = e;
    m_Choice = false;
}

void CTypeStrings::SetTemplate(const string& c, const string& m)
{
    m_Type = eTemplateType;
    m_CType = c;
    m_Macro = m;
    m_Choice = false;
}

void CTypeStrings::SetTemplate(const string& c, const string& m,
                               const CTypeStrings& arg)
{
    string cc = c + "< " + arg.GetCType() + " >";
    string mm = m + ", (" + arg.GetRef() + ')';
    AddIncludes(arg);
    m_Type = eTemplateType;
    m_CType = cc;
    m_Macro = mm;
    m_Choice = false;
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
    m_Choice = false;
}

void CTypeStrings::ToPointer(void)
{
    if ( GetType() != ePointerType ) {
        m_Macro = GetRef();
        m_CType = GetCType() + '*';
        m_CPPIncludes = m_HPPIncludes;
        m_HPPIncludes.clear();
        m_Type = ePointerType;
        m_Choice = false;
    }
}

void CTypeStrings::ToPointer(const string& c, const string& m,
                             bool simplePointer)
{
    if ( GetType() != ePointerType ) {
        THROW1_TRACE(runtime_error, "bad ToPointer call");
    }
    m_Macro = m + ", (" + m_Macro + ')';
    m_CType = c + "< " + m_CType.substr(0, m_CType.size() - 1) + " >";
    m_Type = simplePointer? ePointerTemplateType: eTemplateType;
}

void CTypeStrings::SetChoice(void)
{
    switch ( GetType() ) {
    case ePointerType:
    case eTemplateType:
    case ePointerTemplateType:
        m_Choice = true;
        break;
    default:
        THROW1_TRACE(runtime_error, "bad choice type");
    }
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
            "    ADD_N_M("<< name << ", " << member << ", " << GetRef() <<')';
        break;
    }
    string init = GetInitializer();
    if ( !init.empty() )
        code.AddInitializer(member, init);
}
