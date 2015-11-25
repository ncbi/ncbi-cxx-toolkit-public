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
*   Type reference definition
*/

#include <ncbi_pch.hpp>
#include "reftype.hpp"
#include "unitype.hpp"
#include "statictype.hpp"
#include "typestr.hpp"
#include "value.hpp"
#include "module.hpp"
#include "exceptions.hpp"
#include "blocktype.hpp"
#include "enumtype.hpp"
#include "srcutil.hpp"
#include <serial/serialimpl.hpp>

BEGIN_NCBI_SCOPE

CReferenceDataType::CReferenceDataType(const string& n, bool ref_to_parent /*=false*/)
    : m_UserTypeName(n), m_RefToParent(ref_to_parent)
{
}

void CReferenceDataType::PrintASN(CNcbiOstream& out, int /*indent*/) const
{
    PrintASNTag(out);
    out << CDataTypeModule::ToAsnName(m_UserTypeName);
}

string CReferenceDataType::GetSpecKeyword(void) const
{
    return string("/") + GetUserTypeName();
}

// XML schema generator submitted by
// Marc Dumontier, Blueprint initiative, dumontier@mshri.on.ca
// modified by Andrei Gourianov, gouriano@ncbi
void CReferenceDataType::PrintXMLSchema(CNcbiOstream& out,
    int indent, bool contents_only) const
{
    string tag(XmlTagName());
    string userType(UserTypeXmlTagName());

    if (tag == userType || (GetEnforcedStdXml() && contents_only)) {

        if (IsRefToParent()) {
            const CDataType* par = GetParentType();
            while ( par->GetParentType() ) {
                par = par->GetParentType();
            }
            PrintASNNewLine(out,indent) <<
                "<xs:element name=\"" << userType << "\""
                << " type=\"" << par->GetMemberName() << userType << "_Type\"";
            if (GetDataMember()) {
                if (GetDataMember()->Optional()) {
                    out << " minOccurs=\"0\"";
                }
                if (GetDataMember()->GetDefault()) {
                    out << " default=\"" << GetDataMember()->GetDefault()->GetXmlString() << "\"";
                }
            }
            out << "/>";
            return;
        }

        PrintASNNewLine(out,indent) <<
            "<xs:element ref=\"" << userType << "\"";
        if (GetDataMember()) {
            if (GetDataMember()->Optional()) {
                out << " minOccurs=\"0\"";
            }
            if (GetDataMember()->GetDefault()) {
                out << " default=\"" << GetDataMember()->GetDefault()->GetXmlString() << "\"";
            }
        }
        out << "/>";
    } else {
        if (!contents_only) {
            PrintASNNewLine(out,indent++) << "<xs:element name=\"" << tag << "\"";
            if (GetDataMember() && GetDataMember()->Optional()) {
                out << " minOccurs=\"0\"";
            }
#if _DATATOOL_USE_SCHEMA_STYLE_COMMENTS
            if (!(GetDataMember() && GetDataMember()->GetComments().PrintSchemaComments(out, indent))) {
                out << ">";
            }
#else
            out << ">";
#endif
            PrintASNNewLine(out,indent++) << "<xs:complexType>";
            PrintASNNewLine(out,indent++) << "<xs:sequence>";
        }
        PrintASNNewLine(out,indent) << "<xs:element ref=\"" << userType << "\"/>";
        if (!contents_only) {
            PrintASNNewLine(out,--indent) << "</xs:sequence>";
            PrintASNNewLine(out,--indent) << "</xs:complexType>";
            PrintASNNewLine(out,--indent) << "</xs:element>";
        }
    }
}

void CReferenceDataType::PrintDTDElement(CNcbiOstream& out, bool contents_only) const
{
    string tag(XmlTagName());
    string userType(UserTypeXmlTagName());
    const CUniSequenceDataType* uniType = 
        dynamic_cast<const CUniSequenceDataType*>(GetParentType());

    if (tag == userType || (GetEnforcedStdXml() && uniType)) {
        const CDataType* realType = ResolveOrNull();
        if (realType) {
            realType->PrintDTDElement(out, contents_only);
        }
        return;
    }
    out <<
        "\n<!ELEMENT "<<XmlTagName()<<" ("<<UserTypeXmlTagName()<<")>";
}

void CReferenceDataType::PrintDTDExtra(CNcbiOstream& out) const
{
    string tag(XmlTagName());
    string userType(UserTypeXmlTagName());
    const CUniSequenceDataType* uniType = 
        dynamic_cast<const CUniSequenceDataType*>(GetParentType());

    if (tag == userType || (GetEnforcedStdXml() && uniType)) {
        const CDataType* realType = ResolveOrNull();
        if (realType) {
            realType->PrintDTDExtra(out);
        }
    }
}

void CReferenceDataType::FixTypeTree(void) const
{
    CParent::FixTypeTree();
    CDataType* resolved = ResolveOrNull();
    if ( resolved )
        resolved->AddReference(this);
}

bool CReferenceDataType::CheckType(void) const
{
    try {
        ResolveLocalOrParent(m_UserTypeName);
        return true;
    }
    catch ( CNotFoundException& exc) {
        Warning("Unresolved type: " + m_UserTypeName + ": " + exc.what(), 2);
    }
    return false;
}

bool CReferenceDataType::CheckValue(const CDataValue& value) const
{
    CDataType* resolved = ResolveOrNull();
    if ( !resolved )
        return false;
    return resolved->CheckValue(value);
}

TTypeInfo CReferenceDataType::GetRealTypeInfo(void)
{
    CDataType* dataType = ResolveOrThrow();
    if ( dynamic_cast<CDataMemberContainerType*>(dataType) ||
         dynamic_cast<CEnumDataType*>(dataType) )
        return dataType->GetRealTypeInfo();
    return CParent::GetRealTypeInfo();
}

CTypeInfo* CReferenceDataType::CreateTypeInfo(void)
{
    CClassTypeInfo* info = CClassInfoHelper<AnyType>::CreateClassInfo(m_UserTypeName.c_str());
    info->SetImplicit();
    CMemberInfo* memInfo = info->AddMember("", 0, ResolveOrThrow()->GetTypeInfo());
    const CDataMember *mem = GetDataMember();
    if (!mem && GetParentType()) {
        mem = GetParentType()->GetDataMember();
    }
    if (mem) {
        if (mem->Optional()) {
            memInfo->SetOptional();
        }
        if (mem->NoPrefix()) {
            memInfo->SetNoPrefix();
        }
        if (IsNillable()) {
            memInfo->SetNillable();
        }
    }
    if ( GetParentType() == 0 ) {
        // global
        info->SetModuleName(GetModule()->GetName());
    }
    return info;
}

TObjectPtr CReferenceDataType::CreateDefault(const CDataValue& value) const
{
    return ResolveOrThrow()->CreateDefault(value);
}

string CReferenceDataType::GetDefaultString(const CDataValue& value) const
{
    return ResolveOrThrow()->GetDefaultString(value);
}

AutoPtr<CTypeStrings> CReferenceDataType::GenerateCode(void) const
{
    return CParent::GenerateCode();
}

AutoPtr<CTypeStrings> CReferenceDataType::GetFullCType(void) const
{
    const CDataType* resolved = ResolveOrThrow();
    AutoPtr<CTypeStrings> type = resolved->Skipped() ?
        resolved->GetFullCType() : resolved->GetRefCType();
    type->SetDataType(this);
    return type;  
}

CDataType* CReferenceDataType::ResolveLocalOrParent(const string& typeName) const
{
    if (m_RefToParent) {
        const CDataType* type = GetParentType();
        for (; type != NULL; type = type->GetParentType()) {
            if (type->GetMemberName() == typeName) {
                return const_cast<CDataType*>(type);
            }
        }
        NCBI_THROW(CNotFoundException,eType, "undefined type: "+typeName);
    }
    return ResolveLocal(m_UserTypeName);
}

CDataType* CReferenceDataType::ResolveOrNull(void) const
{
    try {
        return ResolveLocalOrParent(m_UserTypeName);
    }
    catch ( CNotFoundException& /* ignored */) {
    }
    return 0;
}

CDataType* CReferenceDataType::ResolveOrThrow(void) const
{
    try {
        return ResolveLocalOrParent(m_UserTypeName);
    }
    catch ( CNotFoundException& exc) {
        NCBI_RETHROW_SAME(exc, LocationString());
    }
    // ASSERT("Not reached" == 0);
    return static_cast<CDataType*>(NULL);  // Happy compiler fix
}

CDataType* CReferenceDataType::Resolve(void)
{
    CDataType* resolved = ResolveOrNull();
    if ( !resolved )
        return this;
    return resolved;
}

const CDataType* CReferenceDataType::Resolve(void) const
{
    CDataType* resolved = ResolveOrNull();
    if ( !resolved )
        return this;
    return resolved;
}

END_NCBI_SCOPE
