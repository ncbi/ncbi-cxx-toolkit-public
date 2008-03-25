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
*   Class code generator
*
*/

#include <ncbi_pch.hpp>
#include "code.hpp"
#include "type.hpp"
#include "srcutil.hpp"

BEGIN_NCBI_SCOPE

string    CClassCode::sm_ExportSpecifier;
bool      CClassCode::sm_DoxygenComments=false;
string    CClassCode::sm_DoxygenGroup;
string    CClassCode::sm_DocRootURL;


CClassContext::~CClassContext(void)
{
}

CClassCode::CClassCode(CClassContext& owner, const string& className)
    : m_Code(owner),
      m_ClassName(className),
      m_VirtualDestructor(false)
{
}

CClassCode::~CClassCode(void)
{
    {
        CNcbiOstrstream hpp;
        GenerateHPP(hpp);
        m_Code.AddHPPCode(hpp);
    }
    {
        CNcbiOstrstream inl;
        GenerateINL(inl);
        m_Code.AddINLCode(inl);
    }
    {
        CNcbiOstrstream cpp;
        GenerateCPP(cpp);
        m_Code.AddCPPCode(cpp);
    }
}

void CClassCode::SetExportSpecifier(const string& str)
{
    sm_ExportSpecifier = str;
}

const string& CClassCode::GetExportSpecifier(void)
{
    return sm_ExportSpecifier;
}

void CClassCode::SetDoxygenComments(bool set)
{
    sm_DoxygenComments = set;
}
bool CClassCode::GetDoxygenComments(void)
{
    return sm_DoxygenComments;
}

void CClassCode::SetDoxygenGroup(const string& str)
{
    sm_DoxygenGroup = str;
}

const string& CClassCode::GetDoxygenGroup(void)
{
    return sm_DoxygenGroup;
}

void CClassCode::SetDocRootURL(const string& str)
{
    sm_DocRootURL = str;
}

const string& CClassCode::GetDocRootURL(void)
{
    return sm_DocRootURL;
}

const CNamespace& CClassCode::GetNamespace(void) const
{
    return m_Code.GetNamespace();
}

void CClassCode::AddHPPCode(const CNcbiOstrstream& code)
{
    WriteTabbed(m_ClassPublic, code);
}

void CClassCode::AddINLCode(const CNcbiOstrstream& code)
{
    Write(m_InlineMethods, code);
}

void CClassCode::AddCPPCode(const CNcbiOstrstream& code)
{
    Write(m_Methods, code);
}

string CClassCode::GetMethodPrefix(void) const
{
    return m_Code.GetMethodPrefix() + GetClassNameDT() + "::";
}

bool CClassCode::InternalClass(void) const
{
    return !m_Code.GetMethodPrefix().empty();
}

CClassCode::TIncludes& CClassCode::HPPIncludes(void)
{
    return m_Code.HPPIncludes();
}

CClassCode::TIncludes& CClassCode::CPPIncludes(void)
{
    return m_Code.CPPIncludes();
}

void CClassCode::SetParentClass(const string& className,
                                const CNamespace& ns)
{
    m_ParentClassName = className;
    m_ParentClassNamespace = ns;
}

void CClassCode::AddForwardDeclaration(const string& s, const CNamespace& ns)
{
    m_Code.AddForwardDeclaration(s, ns);
}

bool CClassCode::HaveInitializers(void) const
{
    return !Empty(m_Initializers);
}

void CClassCode::AddInitializer(const string& member, const string& init)
{
    if ( init.empty() )
        return;
    if ( HaveInitializers() )
        m_Initializers << ", ";
    m_Initializers << member << '(' << init << ')';
}

void CClassCode::AddConstructionCode(const string& code)
{
    if ( code.empty() )
        return;
    m_ConstructionCode.push_back(code);
}

void CClassCode::AddDestructionCode(const string& code)
{
    if ( code.empty() )
        return;
    m_DestructionCode.push_front(code);
}

CNcbiOstream& CClassCode::WriteInitializers(CNcbiOstream& out) const
{
    return Write(out, m_Initializers);
}

CNcbiOstream& CClassCode::WriteConstructionCode(CNcbiOstream& out) const
{
    ITERATE ( list<string>, i, m_ConstructionCode ) {
        WriteTabbed(out, *i);
    }
    return out;
}

CNcbiOstream& CClassCode::WriteDestructionCode(CNcbiOstream& out) const
{
    ITERATE ( list<string>, i, m_DestructionCode ) {
        WriteTabbed(out, *i);
    }
    return out;
}

CNcbiOstream& CClassCode::GenerateHPP(CNcbiOstream& header) const
{
    if (CClassCode::GetDoxygenComments()) {
        header <<
            "///\n"
            "/// " << GetClassNameDT() << " --\n"
            "///\n\n";
    }
    header << "class ";
    if ( !GetExportSpecifier().empty() )
        header << CClassCode::GetExportSpecifier() << " ";
    header << GetClassNameDT();
    string parentNamespaceRef;
    if ( !GetParentClassName().empty() ) {
        parentNamespaceRef =
            GetNamespace().GetNamespaceRef(GetParentClassNamespace());
        header << " : public "<<parentNamespaceRef<<GetParentClassName();
    }
    header <<
        "\n"
        "{\n";
    if ( !GetParentClassName().empty() ) {
        header <<
            "    typedef "<<parentNamespaceRef<<GetParentClassName()<<" Tparent;\n";
    }
    header <<
        "public:\n";
    Write(header, m_ClassPublic);
    if ( !Empty(m_ClassProtected) ) {
        header << 
            "\n"
            "protected:\n";
        Write(header, m_ClassProtected);
    }
    if ( !Empty(m_ClassPrivate) ) {
        header << 
            "\n"
            "private:\n";
        Write(header, m_ClassPrivate);
    }
    header <<
        "};\n";
    return header;
}

CNcbiOstream& CClassCode::GenerateINL(CNcbiOstream& code) const
{
    Write(code, m_InlineMethods);
    return code;
}

CNcbiOstream& CClassCode::GenerateCPP(CNcbiOstream& code) const
{
    Write(code, m_Methods);
    code << "\n";
    return code;
}

CNcbiOstream& CClassCode::GenerateUserHPP(CNcbiOstream& header) const
{
    if ( InternalClass() ) {
        return header;
    }
    header << "class ";
    if ( !GetExportSpecifier().empty() )
        header << CClassCode::GetExportSpecifier() << " ";
    header << GetClassNameDT()<<" : public "<<GetClassNameDT()<<"_Base\n"
        "{\n"
        "public:\n"
        "    "<<GetClassNameDT()<<"();\n"
        "    "<<'~'<<GetClassNameDT()<<"();\n"
        "\n"
        "};\n";
    return header;
}

CNcbiOstream& CClassCode::GenerateUserCPP(CNcbiOstream& code) const
{
    if ( InternalClass() ) {
        return code;
    }
    code <<
        GetClassNameDT()<<"::"<<GetClassNameDT()<<"()\n"
        "{\n"
        "}\n"
        "\n"
         <<GetClassNameDT()<<"::~"<<GetClassNameDT()<<"()\n"
        "{\n"
        "}\n"
        "\n";
    return code;
}

END_NCBI_SCOPE
