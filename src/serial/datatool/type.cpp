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
*   Base class for type description
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.30  1999/11/15 19:36:19  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <serial/autoptrinfo.hpp>
#include <algorithm>
#include "type.hpp"
#include "value.hpp"
#include "module.hpp"
#include "code.hpp"
#include "typestr.hpp"
#include "exceptions.hpp"
#include "blocktype.hpp"

TTypeInfo CAnyTypeSource::GetTypeInfo(void)
{
    TTypeInfo typeInfo = m_Type->GetTypeInfo();
    if ( typeInfo->GetSize() > sizeof(AnyType) )
        typeInfo = new CAutoPointerTypeInfo(typeInfo);
    return typeInfo;
}

string CDataType::Identifier(const string& typeName, bool capitalize)
{
    string s;
    s.reserve(typeName.size());
    string::const_iterator i = typeName.begin();
    if ( i != typeName.end() ) {
        s += capitalize? toupper(*i): *i;
        while ( ++i != typeName.end() ) {
            char c = *i;
            if ( c == '-' || c == '.' )
                c = '_';
            s += c;
        }
    }
    return s;
}

string CDataType::GetTemplateHeader(const string& tmpl)
{
    if ( tmpl == "multiset" )
        return "<set>";
    if ( tmpl == "multimap" )
        return "<map>";
    if ( tmpl == "auto_ptr" )
        return "<memory>";
    if ( tmpl == "AutoPtr" )
        return "<corelib/autoptr.hpp>";
    return '<' + tmpl + '>';
}

bool CDataType::IsSimpleTemplate(const string& tmpl)
{
    if ( tmpl == "AutoPtr" )
        return true;
    return false;
}

string CDataType::GetTemplateNamespace(const string& tmpl)
{
    if ( tmpl == "AutoPtr" )
        return "NCBI_NS_NCBI";
    return "NCBI_NS_STD";
}

string CDataType::GetTemplateMacro(const string& tmpl)
{
    if ( tmpl == "auto_ptr" )
        return "STL_CHOICE_auto_ptr";
    return "STL_" + tmpl;
}

CDataType::CDataType(void)
    : m_ParentType(0), m_Module(0), m_SourceLine(0),
      m_InSet(false), m_Checked(false)
{
}

CDataType::~CDataType()
{
}

void CDataType::SetSourceLine(int line)
{
    m_SourceLine = line;
}

void CDataType::Warning(const string& mess) const
{
    CNcbiDiag() << LocationString() << ": " << mess;
}

void CDataType::SetInSet(void)
{
    m_InSet = true;
}

void CDataType::SetInChoice(const CChoiceDataType* choice)
{
    m_Choices.insert(choice);
}

void CDataType::SetParent(const CDataType* parent, const string& memberName)
{
    _ASSERT(parent != 0);
    _ASSERT(m_ParentType == 0 && m_Module == 0 && m_MemberName.empty());
    m_ParentType = parent;
    m_Module = parent->GetModule();
    m_MemberName = memberName;
    _ASSERT(m_Module != 0);
    FixTypeTree();
}

void CDataType::SetParent(const CDataTypeModule* module, const string& typeName)
{
    _ASSERT(module != 0);
    _ASSERT(m_ParentType == 0 && m_Module == 0 && m_MemberName.empty());
    m_Module = module;
    m_MemberName = typeName;
    FixTypeTree();
}

void CDataType::FixTypeTree(void) const
{
}

bool CDataType::Check(void)
{
    if ( m_Checked )
        return true;
    m_Checked = true;
    _ASSERT(m_Module != 0);
    return CheckType();
}

bool CDataType::CheckType(void) const
{
    return true;
}

CNcbiOstream& CDataType::NewLine(CNcbiOstream& out, int indent)
{
    out << NcbiEndl;
    for ( int i = 0; i < indent; ++i )
        out << "  ";
    return out;
}

const string& CDataType::GetVar(const string& value) const
{
    const CDataType* parent = GetParentType();
    if ( !parent ) {
        return GetModule()->GetVar(m_MemberName, value);
    }
    else {
        return parent->GetVar(m_MemberName + '.' + value);
    }
}

const string& CDataType::GetSourceFileName(void) const
{
    return GetModule()->GetSourceFileName();
}

string CDataType::LocationString(void) const
{
    return GetSourceFileName() + ':' + NStr::IntToString(GetSourceLine()) +
        ": " + IdName(); 
}

string CDataType::IdName(void) const
{
    const CDataType* parent = GetParentType();
    if ( !parent ) {
        // root type
        return m_MemberName;
    }
    else {
        // member
        return parent->IdName() + '.' + m_MemberName;
    }
}

string CDataType::GetKeyPrefix(void) const
{
    const CDataType* parent = GetParentType();
    if ( !parent ) {
        // root type
        return NcbiEmptyString;
    }
    else {
        // member
        string parentPrefix = parent->GetKeyPrefix();
        if ( parentPrefix.empty() )
            return m_MemberName;
        else
            return parentPrefix + '.' + m_MemberName;
    }
}

bool CDataType::Skipped(void) const
{
    return GetVar("_class") == "-";
}

string CDataType::ClassName(void) const
{
    const string& cls = GetVar("_class");
    if ( !cls.empty() )
        return cls;
    const CDataType* parent = GetParentType();
    if ( parent ) {
        return parent->ClassName() +
            "__" + Identifier(m_MemberName, false);
    }
    return Identifier(m_MemberName);
}

string CDataType::FileName(void) const
{
    const string& file = GetVar("_file");
    if ( !file.empty() )
        return file;
    const CDataType* parent = GetParentType();
    if ( parent ) {
        return parent->FileName() + "_M";
    }
    return Identifier(m_MemberName);
}

string CDataType::Namespace(void) const
{
    return GetVar("_namespace");
}

string CDataType::InheritFromClass(void) const
{
    return GetVar("_parent_class");
}

const CDataType* CDataType::InheritFromType(void) const
{
    const string& parentName = GetVar("_parent");
    if ( parentName.empty() )
        return 0;

    return GetModule()->InternalResolve(parentName);
}

CDataType* CDataType::Resolve(void)
{
    return this;
}

const CDataType* CDataType::Resolve(void) const
{
    return this;
}

TTypeInfo CDataType::GetTypeInfo(void)
{
    if ( !m_CreatedTypeInfo ) {
        m_CreatedTypeInfo = CreateTypeInfo();
        if ( !m_CreatedTypeInfo )
            THROW1_TRACE(runtime_error, "type undefined");
    }
    return m_CreatedTypeInfo.get();
}

void CDataType::GenerateCode(CClassCode& code) const
{
    // by default, generate implicit class with one data member
    string memberName = GetVar("_member");
    if ( memberName.empty() ) {
        memberName = "m_" + Identifier(IdName());
    }
    code.TypeInfoBody() <<
        "    info->SetImplicit();" << NcbiEndl;
    CTypeStrings tType;
    GetCType(tType, code);
    tType.AddMember(code, memberName);
    code.TypeInfoBody() << ';' << NcbiEndl;
    code.ClassPublic() <<
        "    operator "<<tType.cType<<"&(void)"<<NcbiEndl<<
        "    {"<<NcbiEndl<<
        "        return "<<memberName<<';'<<NcbiEndl<<
        "    }"<<NcbiEndl<<
        "    operator const "<<tType.cType<<"&(void) const"<<NcbiEndl<<
        "    {"<<NcbiEndl<<
        "        return "<<memberName<<';'<<NcbiEndl<<
        "    }"<<NcbiEndl<<
        NcbiEndl;
}

bool CDataType::InChoice(void) const
{
    return dynamic_cast<const CChoiceDataType*>(GetParentType()) != 0;
}

void CDataType::GetCType(CTypeStrings& , CClassCode& ) const
{
    THROW1_TRACE(runtime_error, LocationString() + ": C++ type undefined");
}

string CDataType::GetDefaultString(const CDataValue& ) const
{
    Warning("Default is not supported by this type");
    return "...";
}

CTypeInfo* CDataType::CreateTypeInfo(void)
{
    return 0;
}
