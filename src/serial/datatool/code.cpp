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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.30  2000/04/12 15:36:50  vasilche
* Added -on <namespace> argument to datatool.
* Removed unnecessary namespace specifications in generated files.
*
* Revision 1.29  2000/04/07 19:26:25  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.28  2000/03/07 20:05:00  vasilche
* Added NewInstance method to generated classes.
*
* Revision 1.27  2000/03/07 14:06:31  vasilche
* Added generation of reference counted objects.
*
* Revision 1.26  2000/02/17 20:05:06  vasilche
* Inline methods now will be generated in *_Base.inl files.
* Fixed processing of StringStore.
* Renamed in choices: Selected() -> Which(), E_choice -> E_Choice.
* Enumerated values now will preserve case as in ASN.1 definition.
*
* Revision 1.25  2000/02/01 21:47:56  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.24  2000/01/11 16:59:02  vasilche
* Changed macros generation for compilation on MS VC.
*
* Revision 1.23  1999/12/01 17:36:25  vasilche
* Fixed CHOICE processing.
*
* Revision 1.22  1999/11/18 17:13:06  vasilche
* Fixed generation of ENUMERATED CHOICE and VisibleString.
* Added generation of initializers to zero for primitive types and pointers.
*
* Revision 1.21  1999/11/15 19:36:13  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <serial/tool/code.hpp>
#include <serial/tool/type.hpp>
#include <serial/tool/fileutil.hpp>

BEGIN_NCBI_SCOPE

CClassContext::~CClassContext(void)
{
}

CClassCode::CClassCode(CClassContext& owner, const string& className)
    : m_Code(owner),
      m_ClassName(className),
      m_VirtualDestructor(false)
{
}

CClassCode::CClassCode(CClassContext& owner, const string& className,
                       const CNamespace& ns)
    : m_Code(owner),
      m_ClassName(className), m_Namespace(ns),
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
    return m_Code.GetMethodPrefix() + GetClassName() + "::";
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

CNcbiOstream& CClassCode::WriteDestructionCode(CNcbiOstream& out) const
{
    iterate ( list<string>, i, m_DestructionCode ) {
        WriteTabbed(out, *i);
    }
    return out;
}

CNcbiOstream& CClassCode::GenerateHPP(CNcbiOstream& header) const
{
    header <<
        "class "<<GetClassName();
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
    header << 
        "\n"
        "protected:\n";
    Write(header, m_ClassProtected);
    header << 
        "\n"
        "private:\n";
    Write(header, m_ClassPrivate);
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
    header <<
        "class "<<GetClassName()<<" : public "<<GetClassName()<<"_Base\n"
        "{\n"
        "public:\n"
        "    "<<GetClassName()<<"();\n"
        "    "<<'~'<<GetClassName()<<"();\n"
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
        GetClassName()<<"::"<<GetClassName()<<"()\n"
        "{\n"
        "}\n"
        "\n"
         <<GetClassName()<<"::~"<<GetClassName()<<"()\n"
        "{\n"
        "}\n"
        "\n";
    return code;
}

END_NCBI_SCOPE
