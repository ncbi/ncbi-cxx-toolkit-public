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
*   Type description for enumerated types
*/

#include <ncbi_pch.hpp>
#include "enumtype.hpp"
#include "blocktype.hpp"
#include "value.hpp"
#include "enumstr.hpp"
#include "module.hpp"
#include "srcutil.hpp"
#include <serial/impl/enumerated.hpp>

BEGIN_NCBI_SCOPE

CEnumDataType::CEnumDataType(void)
{
    ForbidVar("_type", "string");
}

const char* CEnumDataType::GetASNKeyword(void) const
{
    return "ENUMERATED";
}

const char* CEnumDataType::GetDEFKeyword(void) const
{
    return "_ENUMERATED_";
}

string CEnumDataType::GetXMLContents(void) const
{
    string content("\n");
    ITERATE ( TValues, i, m_Values ) {
        if (i != m_Values.begin()) {
            content += " |\n";
        }
        content += "        " + i->GetName();
    }
    return content;
}

bool CEnumDataType::IsInteger(void) const
{
    return false;
}

CEnumDataType::TValue& CEnumDataType::AddValue(const string& valueName,
                                               TEnumValueType value)
{
    m_Values.push_back(TValue(valueName, value));
    return m_Values.back();
}

void CEnumDataType::PrintASN(CNcbiOstream& out, int indent) const
{
    out << GetASNKeyword() << " {";
    ++indent;
    ITERATE ( TValues, i, m_Values ) {
        PrintASNNewLine(out, indent);
        TValues::const_iterator next = i;
        bool last = ++next == m_Values.end();

        bool oneLineComment = i->GetComments().OneLine();
        if ( !oneLineComment )
            i->GetComments().PrintASN(out, indent);
        out << CDataTypeModule::ToAsnId(i->GetName()) << " (" << i->GetValue() << ")";
        if ( !last )
            out << ',';
        if ( oneLineComment ) {
            out << ' ';
            i->GetComments().PrintASN(out, indent, CComments::eOneLine);
        }
    }
    --indent;
    PrintASNNewLine(out, indent);
    m_LastComments.PrintASN(out, indent, CComments::eMultiline);
    out << "}";
}

void CEnumDataType::PrintSpecDumpExtra(CNcbiOstream& out, int indent) const
{
    ++indent;
    ITERATE ( TValues, i, m_Values ) {
        PrintASNNewLine(out, indent);
        out << "V," << i->GetSourceLine() << ',';
        out << GetFullName() << ':' << i->GetName() << ',' << i->GetValue();
        i->GetComments().PrintASN(out, indent, CComments::eNoEOL);
    }
}

// XML schema generator submitted by
// Marc Dumontier, Blueprint initiative, dumontier@mshri.on.ca
// modified by Andrei Gourianov, gouriano@ncbi
void CEnumDataType::PrintXMLSchema(CNcbiOstream& out,
    int indent, bool contents_only) const
{
    string tag(XmlTagName());
    string use("required");
    string value("value");
    string form;
    bool inAttlist= false;
    list<string> opentag, closetag;

    if (GetEnforcedStdXml() &&
        GetParentType() && 
        GetParentType()->GetDataMember() &&
        GetParentType()->GetDataMember()->Attlist()) {
        const CDataMember* mem = GetDataMember();
        inAttlist = true;
        value = tag;
        if (mem->Optional()) {
            use = "optional";
            if (mem->GetDefault()) {
                use += "\" default=\"" + GetXmlValueName(mem->GetDefault()->GetXmlString());
            }
        } else {
            use = "required";
        }
        if (IsNsQualified() == eNSQualified) {
            form = " form=\"qualified\"";
        }
    }
    if (!inAttlist) {
        const CDataMember* mem = GetDataMember();
        string tmp = "<xs:element name=\"" + tag + "\"";
        if (mem && mem->Optional()) {
            tmp += " minOccurs=\"0\"";
            if (mem->GetDefault()) {
                use = "optional";
            }
        }
        opentag.push_back(tmp + ">");
        closetag.push_front("</xs:element>");
        opentag.push_back("<xs:complexType>");
        closetag.push_front("</xs:complexType>");
        if(IsInteger()) {
            opentag.push_back("<xs:simpleContent>");
            closetag.push_front("</xs:simpleContent>");
            opentag.push_back("<xs:extension base=\"xs:integer\">");
            closetag.push_front("</xs:extension>");
            use = "optional";
        }
    }
    string tmp = "<xs:attribute name=\"" + value + "\" use=\"" + use + "\"" + form;
    if (!inAttlist) {
        const CDataMember* mem = GetDataMember();
        if (mem && mem->Optional() && mem->GetDefault()) {
            tmp += " default=\"" + GetXmlValueName(mem->GetDefault()->GetXmlString()) + "\"";
        }
    }
    opentag.push_back(tmp + ">");
    closetag.push_front("</xs:attribute>");
    opentag.push_back("<xs:simpleType>");
    closetag.push_front("</xs:simpleType>");
    opentag.push_back("<xs:restriction base=\"xs:string\">");
    closetag.push_front("</xs:restriction>");

    ITERATE ( list<string>, s, opentag ) {
        PrintASNNewLine(out, indent++) << *s;
    }
    bool haveComments = false;
    ITERATE ( TValues, i, m_Values ) {
        if ( !i->GetComments().Empty() ) {
            haveComments = true;
            break;
        }
    }
    if ( haveComments ) {
        out << "\n<!--\n";
        ITERATE ( TValues, i, m_Values ) {
            if ( !i->GetComments().Empty() ) {
                i->GetComments().Print(out, "    "+i->GetName()+"\t- ",
                                       "\n        ", "\n");
            }
        }
        out << "-->";
    }
    ITERATE ( TValues, i, m_Values ) {
        PrintASNNewLine(out, indent) <<
            "<xs:enumeration value=\"" << i->GetName() << "\"";
        if (IsInteger()) {
            out << " ncbi:intvalue=\"" << i->GetValue() << "\"";
        }
        out << "/>";
    }
    ITERATE ( list<string>, s, closetag ) {
        PrintASNNewLine(out, --indent) << *s;
    }
    m_LastComments.PrintDTD(out, CComments::eMultiline);
}

void CEnumDataType::PrintDTDElement(CNcbiOstream& out, bool contents_only) const
{
    string tag(XmlTagName());
    string content(GetXMLContents());
    if (GetParentType() && 
        GetParentType()->GetDataMember() &&
        GetParentType()->GetDataMember()->Attlist()) {
        const CDataMember* mem = GetDataMember();
        out << "\n    " << tag << " (" << content << ") ";
        if (mem->GetDefault()) {
            out << "\"" << GetXmlValueName(mem->GetDefault()->GetXmlString()) << "\"";
        } else {
            if (mem->Optional()) {
                out << "#IMPLIED";
            } else {
                out << "#REQUIRED";
            }
        }
    } else {
        out <<
            "\n<!ELEMENT " << tag << " ";
        if ( IsInteger() ) {
            if (DTDEntitiesEnabled()) {
                out << "(%INTEGER;)>";
            } else {
                out << "(#PCDATA)>";
            }
        } else {
            if (DTDEntitiesEnabled()) {
                out << "%ENUM;>";
            } else {
                out << "EMPTY>";
            }
        }
    }
}

void CEnumDataType::PrintDTDExtra(CNcbiOstream& out) const
{
    bool haveComments = false;
    ITERATE ( TValues, i, m_Values ) {
        if ( !i->GetComments().Empty() ) {
            haveComments = true;
            break;
        }
    }
    if ( haveComments ) {
        out << "\n\n<!--\n";
        ITERATE ( TValues, i, m_Values ) {
            if ( !i->GetComments().Empty() ) {
                i->GetComments().Print(out, "    "+i->GetName()+"\t- ",
                                       "\n        ", "\n");
            }
        }
        out << "-->";
    }
    out <<
        "\n<!ATTLIST "<<XmlTagName()<<" value (\n";
    ITERATE ( TValues, i, m_Values ) {
        if ( i != m_Values.begin() )
            out << " |\n";
        out << "        " << i->GetName();
    }
    out << "\n        ) ";
    if ( IsInteger() )
        out << "#IMPLIED";
    else
        out << "#REQUIRED";
    out << " >\n";
    m_LastComments.PrintDTD(out, CComments::eMultiline);
}

bool CEnumDataType::CheckValue(const CDataValue& value) const
{
    const CIdDataValue* id = dynamic_cast<const CIdDataValue*>(&value);
    if ( id ) {
        ITERATE ( TValues, i, m_Values ) {
            if ( i->GetName() == id->GetValue() )
                return true;
        }
        value.Warning("illegal ENUMERATED value: " + id->GetValue(), 12);
        return false;
    }

    const CIntDataValue* intValue =
        dynamic_cast<const CIntDataValue*>(&value);
    if ( !intValue ) {
        value.Warning("ENUMERATED or INTEGER value expected", 13);
        return false;
    }

    if ( !IsInteger() ) {
        ITERATE ( TValues, i, m_Values ) {
            if ( i->GetValue() == intValue->GetValue() )
                return true;
        }
        value.Warning("illegal INTEGER value: " + intValue->GetValue(), 14);
        return false;
    }

    return true;
}

TObjectPtr CEnumDataType::CreateDefault(const CDataValue& value) const
{
    const CIdDataValue* id = dynamic_cast<const CIdDataValue*>(&value);
    if ( id == 0 ) {
        return new TEnumValueType((TEnumValueType)dynamic_cast<const CIntDataValue&>(value).GetValue());
    }
    ITERATE ( TValues, i, m_Values ) {
        if ( i->GetName() == id->GetValue() )
            return new TEnumValueType(i->GetValue());
    }
    value.Warning("illegal ENUMERATED value: " + id->GetValue(), 15);
    return 0;
}

string CEnumDataType::GetDefaultString(const CDataValue& value) const
{
    const CIdDataValue* id = dynamic_cast<const CIdDataValue*>(&value);
    if ( id ) {
        return GetEnumCInfo().valuePrefix + Identifier(id->GetValue(), false);
    }
    else {
        const CIntDataValue* intValue =
            dynamic_cast<const CIntDataValue*>(&value);
        return NStr::Int8ToString(intValue->GetValue());
    }
}

string CEnumDataType::GetXmlValueName(const string& value) const
{
    return value;
}

CTypeInfo* CEnumDataType::CreateTypeInfo(void)
{
    AutoPtr<CEnumeratedTypeValues>
        info(new CEnumeratedTypeValues(GlobalName(), IsInteger()));
    ITERATE ( TValues, i, m_Values ) {
        info->AddValue(i->GetName(), i->GetValue());
    }
    if ( HaveModuleName() )
        info->SetModuleName(GetModule()->GetName());
    return new CEnumeratedTypeInfo(sizeof(TEnumValueType), info.release());
}

string CEnumDataType::DefaultEnumName(void) const
{
    // generate enum name from ASN type or field name
    if ( !GetParentType() ) {
        // root enum
        return 'E' + Identifier(IdName());
    }
    else {
        // internal enum
        return 'E' + Identifier(GetKeyPrefix());
    }
}

CEnumDataType::SEnumCInfo CEnumDataType::GetEnumCInfo(void) const
{
    string typeName = GetAndVerifyVar("_type");
    string enumName;
    if ( !typeName.empty() && typeName[0] == 'E' ) {
        enumName = typeName;
    }
    else {
        // make C++ type name
        enumName = DefaultEnumName();
        if ( typeName.empty() ) {
            if ( IsInteger() )
                typeName = "int";
            else
                typeName = enumName;
        }
    }
    string prefix = GetVar("_prefix");
    if ( prefix.empty() ) {
        prefix = char(tolower((unsigned char) enumName[0])) + enumName.substr(1) + '_';
    }
    return SEnumCInfo(enumName, typeName, prefix);
}

AutoPtr<CTypeStrings> CEnumDataType::GetRefCType(void) const
{
    SEnumCInfo enumInfo = GetEnumCInfo();
    return AutoPtr<CTypeStrings>(new CEnumRefTypeStrings(enumInfo.enumName,
                                                         enumInfo.cType,
                                                         Namespace(),
                                                         FileName(),
                                                         Comments()));
}

AutoPtr<CTypeStrings> CEnumDataType::GetFullCType(void) const
{
    SEnumCInfo enumInfo = GetEnumCInfo();
    AutoPtr<CEnumTypeStrings> 
        e(new CEnumTypeStrings(GlobalName(), enumInfo.enumName,
                               GetVar("_packedtype"),
                               enumInfo.cType, IsInteger(),
                               m_Values, enumInfo.valuePrefix,
                               GetNamespaceName(), this, Comments()));
    return AutoPtr<CTypeStrings>(e.release());
}

AutoPtr<CTypeStrings> CEnumDataType::GenerateCode(void) const
{
    return GetFullCType();
}

const char* CIntEnumDataType::GetASNKeyword(void) const
{
    return "INTEGER";
}

const char* CIntEnumDataType::GetDEFKeyword(void) const
{
    return "_INTEGER_ENUM_";
}

bool CIntEnumDataType::IsInteger(void) const
{
    return true;
}

string CIntEnumDataType::GetXmlValueName(const string& value) const
{
    TEnumValueType d = (TEnumValueType)NStr::StringToInt(value);
    ITERATE(TValues, v, GetValues()) {
        if (v->GetValue() == d) {
            return v->GetName();
        }
    }
    return value;
}

const char* CBigIntEnumDataType::GetASNKeyword(void) const
{
    return "BigInt";
}

const char* CBigIntEnumDataType::GetDEFKeyword(void) const
{
    return "_BigInt_ENUM_";
}

END_NCBI_SCOPE
