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
*   Type descriptions of predefined types
*/

#include <ncbi_pch.hpp>
#include "exceptions.hpp"
#include "statictype.hpp"
#include "stdstr.hpp"
#include "stlstr.hpp"
#include "value.hpp"
#include "blocktype.hpp"
#include "srcutil.hpp"
#include <serial/impl/stdtypes.hpp>
#include <serial/impl/stltypes.hpp>
#include <serial/impl/autoptrinfo.hpp>
#include <typeinfo>
#include <vector>

BEGIN_NCBI_SCOPE

TObjectPtr CStaticDataType::CreateDefault(const CDataValue& ) const
{
    NCBI_THROW(CDatatoolException, eNotImplemented,
                 GetASNKeyword() + string(" default not implemented"));
}

void CStaticDataType::PrintASN(CNcbiOstream& out, int /*indent*/) const
{
    out << GetASNKeyword();
}

// XML schema generator submitted by
// Marc Dumontier, Blueprint initiative, dumontier@mshri.on.ca
// modified by Andrei Gourianov, gouriano@ncbi
void CStaticDataType::PrintXMLSchema(CNcbiOstream& out,
    int indent, bool contents_only) const
{
    string tag( XmlTagName());
    string xsdk("element"), use, form;
    const CDataMember* mem = GetDataMember();
    bool optional = mem ? mem->Optional() : false;

    if (GetParentType() && GetParentType()->GetDataMember()) {
        if (GetParentType()->GetDataMember()->Attlist()) {
            xsdk = "attribute";
            if (optional) {
                use = "optional";
                if (mem->GetDefault()) {
                    use += "\" default=\"" + mem->GetDefault()->GetXmlString();
                }
            } else {
                use = "required";
            }
            if (IsNsQualified() == eNSQualified) {
                form = " form=\"qualified\"";
            }
        }
    }
    PrintASNNewLine(out, indent) << "<xs:" << xsdk << " name=\"" << tag << "\"";
    string type = GetSchemaTypeString();
    if (!type.empty()) {
        out << " type=\"" << type << "\"";
    }
    if (!use.empty()) {
        out << " use=\"" << use << "\"";
    } else {
        if (GetXmlSourceSpec()) {
            if (optional) {
                out << " minOccurs=\"0\"";
            }
            if (mem && mem->GetDefault()) {
                out << " default=\"" << mem->GetDefault()->GetXmlString() << "\"";
            }
        } else {
            const CBoolDataType* bt = dynamic_cast<const CBoolDataType*>(this);
            if (mem && optional) {
                if (bt) {
                    out << " minOccurs=\"0\"";
                } else {
                    if (mem->GetDefault()) {
                        out << " default=\"" << mem->GetDefault()->GetXmlString() << "\"";
                    } else {
                        out << " minOccurs=\"0\"";
                    }
                }
            }
        }
    }
    if (!form.empty()) {
        out << form;
    }
    if (type.empty() && PrintXMLSchemaContents(out,indent+1)) {
        PrintASNNewLine(out, indent) << "</xs:" << xsdk << ">";
    } else {
        out << "/>";
    }
}

bool CStaticDataType::PrintXMLSchemaContents(CNcbiOstream& out, int indent) const
{
    return false;
}

void CStaticDataType::PrintDTDElement(CNcbiOstream& out, bool contents_only) const
{
    string tag(XmlTagName());
    string content(GetXMLContents());
    if (GetParentType() && 
        GetParentType()->GetDataMember() &&
        GetParentType()->GetDataMember()->Attlist()) {
        const CDataMember* mem = GetDataMember();
        out << "\n    " << tag;
        const CBoolDataType* bt = dynamic_cast<const CBoolDataType*>(this);
        if (bt) {
           out << " ( true | false ) ";
        } else {
           out << " CDATA ";
        }
        if (mem->GetDefault()) {
            out << "\"" << mem->GetDefault()->GetXmlString() << "\"";
        } else {
            if (mem->Optional()) {
                out << "#IMPLIED";
            } else {
                out << "#REQUIRED";
            }
        }
    } else {
        string open("("), close(")");
        if (content == "EMPTY") {
            open.erase();
            close.erase();
        }
        if (!contents_only) {
            out << "\n<!ELEMENT " << tag << ' ' << open;
        }
        out << content;
        if (!contents_only) {
            out << close << ">";
        }
    }
}

AutoPtr<CTypeStrings> CStaticDataType::GetFullCType(void) const
{
    string type = GetAndVerifyVar("_type");
    if ( type.empty() )
        type = GetDefaultCType();
    return AutoPtr<CTypeStrings>(new CStdTypeStrings(type,Comments()));
}

const char* CNullDataType::GetASNKeyword(void) const
{
    return "NULL";
}

const char* CNullDataType::GetDEFKeyword(void) const
{
    return "_NULL_";
}

const char* CNullDataType::GetXMLContents(void) const
{
    return "EMPTY";
}

bool CNullDataType::PrintXMLSchemaContents(CNcbiOstream& out, int indent) const
{
    out << ">";
    PrintASNNewLine(out, indent) << "<xs:complexType/>";
    return true;
}

bool CNullDataType::CheckValue(const CDataValue& value) const
{
    CheckValueType(value, CNullDataValue, "NULL");
    return true;
}

TObjectPtr CNullDataType::CreateDefault(const CDataValue& ) const
{
    NCBI_THROW(CDatatoolException, eNotImplemented,
        "NULL cannot have DEFAULT");
}

CTypeRef CNullDataType::GetTypeInfo(void)
{
    if ( HaveModuleName() )
        return UpdateModuleName(CStdTypeInfo<bool>::CreateTypeInfoNullBool());
    return &CStdTypeInfo<bool>::GetTypeInfoNullBool;
}

AutoPtr<CTypeStrings> CNullDataType::GetFullCType(void) const
{
    return AutoPtr<CTypeStrings>(new CNullTypeStrings(Comments()));
}

const char* CNullDataType::GetDefaultCType(void) const
{
    return "bool";
}

const char* CBoolDataType::GetASNKeyword(void) const
{
    return "BOOLEAN";
}

const char* CBoolDataType::GetDEFKeyword(void) const
{
    return "_BOOLEAN_";
}

const char* CBoolDataType::GetXMLContents(void) const
{
//    return "%BOOLEAN;";
    return "EMPTY";
}

string CBoolDataType::GetSchemaTypeString(void) const
{
    if (GetXmlSourceSpec()) {
        return "xs:boolean";
    }
    if (GetParentType() && 
        GetParentType()->GetDataMember() &&
        GetParentType()->GetDataMember()->Attlist()) {
        return "xs:boolean";
    }
    return kEmptyStr;
}

bool CBoolDataType::PrintXMLSchemaContents(CNcbiOstream& out, int indent) const
{
    if (GetParentType() && 
        GetParentType()->GetDataMember() &&
        GetParentType()->GetDataMember()->Attlist()) {
        return false;
    }
    out << ">";
    const CBoolDataValue *val = GetDataMember() ?
        dynamic_cast<const CBoolDataValue*>(GetDataMember()->GetDefault()) : 0;

    PrintASNNewLine(out,indent++) << "<xs:complexType>";
    PrintASNNewLine(out,indent++) << "<xs:attribute name=\"value\" use=";
    if (val) {
        out << "\"optional\" default=";
        if (val->GetValue()) {
            out << "\"true\"";
        } else {
            out << "\"false\"";
        }
    } else {
        out << "\"required\"";
    }
    out << ">";
    PrintASNNewLine(out,indent++) << "<xs:simpleType>";
    PrintASNNewLine(out,indent++) << "<xs:restriction base=\"xs:string\">";
    PrintASNNewLine(out,indent)   << "<xs:enumeration value=\"true\"/>";
    PrintASNNewLine(out,indent)   << "<xs:enumeration value=\"false\"/>";
    PrintASNNewLine(out,--indent) << "</xs:restriction>";
    PrintASNNewLine(out,--indent) << "</xs:simpleType>";
    PrintASNNewLine(out,--indent) << "</xs:attribute>";
    PrintASNNewLine(out,--indent) << "</xs:complexType>";
    return true;
}

void CBoolDataType::PrintDTDExtra(CNcbiOstream& out) const
{
    const char *attr;
    const CBoolDataValue *val = GetDataMember() ?
        dynamic_cast<const CBoolDataValue*>(GetDataMember()->GetDefault()) : 0;

    if(val) {
        attr = val->GetValue() ? "\"true\"" : "\"false\"";
    }
    else {
        attr = "#REQUIRED";
    }

    out <<
      "\n<!ATTLIST "<<XmlTagName()<<" value ( true | false ) " 
	<< attr << " >\n";
}

bool CBoolDataType::CheckValue(const CDataValue& value) const
{
    CheckValueType(value, CBoolDataValue, "BOOLEAN");
    return true;
}

TObjectPtr CBoolDataType::CreateDefault(const CDataValue& value) const
{
    return new bool(dynamic_cast<const CBoolDataValue&>(value).GetValue());
}

string CBoolDataType::GetDefaultString(const CDataValue& value) const
{
    return (dynamic_cast<const CBoolDataValue&>(value).GetValue()?
            "true": "false");
}

CTypeRef CBoolDataType::GetTypeInfo(void)
{
    if ( HaveModuleName() )
        return UpdateModuleName(CStdTypeInfo<bool>::CreateTypeInfo());
    return &CStdTypeInfo<bool>::GetTypeInfo;
}

const char* CBoolDataType::GetDefaultCType(void) const
{
    return "bool";
}

CRealDataType::CRealDataType(void)
{
    ForbidVar("_type", "string");
}

const char* CRealDataType::GetASNKeyword(void) const
{
    return "REAL";
}

const char* CRealDataType::GetDEFKeyword(void) const
{
    return "_REAL_";
}

const char* CRealDataType::GetXMLContents(void) const
{
    return DTDEntitiesEnabled() ? "%REAL;" : "#PCDATA";
}

string CRealDataType::GetSchemaTypeString(void) const
{
    return "xs:double";
}

bool CRealDataType::CheckValue(const CDataValue& value) const
{
    const CBlockDataValue* block = dynamic_cast<const CBlockDataValue*>(&value);
    if ( !block ) {
        return  dynamic_cast<const CDoubleDataValue*>(&value) != 0  ||
                dynamic_cast<const CIntDataValue*>(&value) != 0;
    }
    if ( block->GetValues().size() != 3 ) {
        value.Warning("wrong number of elements in REAL value", 16);
        return false;
    }
    for ( CBlockDataValue::TValues::const_iterator i = block->GetValues().begin();
          i != block->GetValues().end(); ++i ) {
        CheckValueType(**i, CIntDataValue, "INTEGER");
    }
    return true;
}

TObjectPtr CRealDataType::CreateDefault(const CDataValue& value) const
{
    double d=0.;
    const CDoubleDataValue* dbl = dynamic_cast<const CDoubleDataValue*>(&value);
    if (dbl) {
        d = dbl->GetValue();
    } else {
        const CIntDataValue* i = dynamic_cast<const CIntDataValue*>(&value);
        if (i) {
            d = (double)(i->GetValue());
        }
    }
    return new double(d);
}

string CRealDataType::GetDefaultString(const CDataValue& value) const
{
    const CDoubleDataValue* dbl = dynamic_cast<const CDoubleDataValue*>(&value);
    if (dbl) {
        return NStr::DoubleToString(dbl->GetValue(),
            DBL_DIG, NStr::fDoubleGeneral | NStr::fDoublePosix);
    } else {
        const CIntDataValue* i = dynamic_cast<const CIntDataValue*>(&value);
        if (i) {
            return NStr::DoubleToString((double)(i->GetValue()),
                DBL_DIG, NStr::fDoubleGeneral | NStr::fDoublePosix);
        }
    }
    value.Warning("REAL value expected", 17);
    return kEmptyStr;
}

TTypeInfo CRealDataType::GetRealTypeInfo(void)
{
    if ( HaveModuleName() )
        return UpdateModuleName(CStdTypeInfo<double>::CreateTypeInfo());
    return CStdTypeInfo<double>::GetTypeInfo();
}

const char* CRealDataType::GetDefaultCType(void) const
{
    return "double";
}

CStringDataType::CStringDataType(EType type)
    : m_Type(type)
{
    ForbidVar("_type", "short");
    ForbidVar("_type", "int");
    ForbidVar("_type", "long");
    ForbidVar("_type", "unsigned");
    ForbidVar("_type", "unsigned short");
    ForbidVar("_type", "unsigned int");
    ForbidVar("_type", "unsigned long");
}

const char* CStringDataType::GetASNKeyword(void) const
{
    if (m_Type == eStringTypeUTF8) {
        return "UTF8String";
    }
    return "VisibleString";
}

const char* CStringDataType::GetDEFKeyword(void) const
{
    if (m_Type == eStringTypeUTF8) {
        return "_UTF8String_";
    }
    return "_VisibleString_";
}

const char* CStringDataType::GetXMLContents(void) const
{
    return "#PCDATA";
}

string CStringDataType::GetSchemaTypeString(void) const
{
    return "xs:string";
}

bool CStringDataType::CheckValue(const CDataValue& value) const
{
    CheckValueType(value, CStringDataValue, "string");
    return true;
}

TObjectPtr CStringDataType::CreateDefault(const CDataValue& value) const
{
    if (m_Type == eStringTypeUTF8) {
        return new (CStringUTF8*)(new CStringUTF8(dynamic_cast<const CStringDataValue&>(value).GetValue()));
    }
    return new (string*)(new string(dynamic_cast<const CStringDataValue&>(value).GetValue()));
}

string CStringDataType::GetDefaultString(const CDataValue& value) const
{
    string s;
    s += '\"';
    const string& v = dynamic_cast<const CStringDataValue&>(value).GetValue();
    for ( string::const_iterator i = v.begin(); i != v.end(); ++i ) {
        switch ( *i ) {
        case '\r':
            s += "\\r";
            break;
        case '\n':
            s += "\\n";
            break;
        case '\"':
            s += "\\\"";
            break;
        case '\\':
            s += "\\\\";
            break;
        default:
            s += *i;
        }
    }
    return s + '\"';
}

TTypeInfo CStringDataType::GetRealTypeInfo(void)
{
    if ( HaveModuleName() )
        return UpdateModuleName(CStdTypeInfo<string>::CreateTypeInfo());
    if ( m_Type == eStringTypeUTF8 )
        return CStdTypeInfo<CStringUTF8>::GetTypeInfo();
    return CStdTypeInfo<string>::GetTypeInfo();
}

bool CStringDataType::NeedAutoPointer(TTypeInfo /*typeInfo*/) const
{
    return true;
}

AutoPtr<CTypeStrings> CStringDataType::GetFullCType(void) const
{
    string type = GetAndVerifyVar("_type");
    if ( type.empty() )
        type = GetDefaultCType();
    return AutoPtr<CTypeStrings>(new CStringTypeStrings(type,Comments()));
}

const char* CStringDataType::GetDefaultCType(void) const
{
    if (m_Type == eStringTypeUTF8) {
        return "ncbi::CStringUTF8";
    }
    return "NCBI_NS_STD::string";
}

CStringStoreDataType::CStringStoreDataType(void)
{
}

const char* CStringStoreDataType::GetASNKeyword(void) const
{
    return "StringStore";
}

const char* CStringStoreDataType::GetDEFKeyword(void) const
{
    return "_StringStore_";
}

TTypeInfo CStringStoreDataType::GetRealTypeInfo(void)
{
    return CStdTypeInfo<string>::GetTypeInfoStringStore();
}

bool CStringStoreDataType::NeedAutoPointer(TTypeInfo /*typeInfo*/) const
{
    return true;
}

AutoPtr<CTypeStrings> CStringStoreDataType::GetFullCType(void) const
{
    string type = GetAndVerifyVar("_type");
    if ( type.empty() )
        type = GetDefaultCType();
    return AutoPtr<CTypeStrings>(new CStringStoreTypeStrings(type,Comments()));
}

const char* CBitStringDataType::GetASNKeyword(void) const
{
    return "BIT STRING";
}

const char* CBitStringDataType::GetDEFKeyword(void) const
{
    return "_BIT_STRING_";
}

bool CBitStringDataType::CheckValue(const CDataValue& value) const
{
    CheckValueType(value, CBitStringDataValue, "BIT STRING");
    return true;
}

TTypeInfo CBitStringDataType::GetRealTypeInfo(void)
{
    if ( HaveModuleName() )
        return UpdateModuleName(CStdTypeInfo<CBitString>::CreateTypeInfo());
    return CStdTypeInfo<CBitString>::GetTypeInfo();
}

bool CBitStringDataType::NeedAutoPointer(TTypeInfo /*typeInfo*/) const
{
    return true;
}

AutoPtr<CTypeStrings> CBitStringDataType::GetFullCType(void) const
{
    return AutoPtr<CTypeStrings>(new CBitStringTypeStrings( GetDefaultCType(), Comments() ));
}

const char* CBitStringDataType::GetDefaultCType(void) const
{
    return "ncbi::CBitString";
}

const char* CBitStringDataType::GetXMLContents(void) const
{
    return DTDEntitiesEnabled() ? "%BITS;" : "#PCDATA";
}

bool CBitStringDataType::PrintXMLSchemaContents(CNcbiOstream& out, int indent) const
{
    out << ">";
    PrintASNNewLine(out,indent++) << "<xs:simpleType>";
    PrintASNNewLine(out,indent++) << "<xs:restriction base=\"xs:string\">";
    PrintASNNewLine(out,indent)   << "<xs:pattern value=\"([0-1])*\"/>";
    PrintASNNewLine(out,--indent) << "</xs:restriction>";
    PrintASNNewLine(out,--indent) << "</xs:simpleType>";
    return true;
}

const char* COctetStringDataType::GetASNKeyword(void) const
{
    return "OCTET STRING";
}

const char* COctetStringDataType::GetDEFKeyword(void) const
{
    return "_OCTET_STRING_";
}

const char* COctetStringDataType::GetDefaultCType(void) const
{
    if (x_AsBitString()) {
        return CBitStringDataType::GetDefaultCType();
    }
    return "NCBI_NS_STD::vector<char>";
}

const char* COctetStringDataType::GetXMLContents(void) const
{
    return DTDEntitiesEnabled() ? "%OCTETS;" : "#PCDATA";
}

string COctetStringDataType::GetSchemaTypeString(void) const
{
    return "xs:hexBinary";
}

bool COctetStringDataType::CheckValue(const CDataValue& value) const
{
    CheckValueType(value, COctetStringDataType, "OCTET STRING");
    return true;
}

TTypeInfo COctetStringDataType::GetRealTypeInfo(void)
{
    if (x_AsBitString()) {
        return CBitStringDataType::GetRealTypeInfo();
    }
    if ( HaveModuleName() )
        return UpdateModuleName(CStdTypeInfo<vector<char> >::CreateTypeInfo());
    return CStdTypeInfo< vector<char> >::GetTypeInfo();
}

bool COctetStringDataType::NeedAutoPointer(TTypeInfo /*typeInfo*/) const
{
    return true;
}

AutoPtr<CTypeStrings> COctetStringDataType::GetFullCType(void) const
{
    if (x_AsBitString()) {
        return CBitStringDataType::GetFullCType();
    }
    string charType = GetVar("_char");
    if ( charType.empty() )
        charType = "char";
    return AutoPtr<CTypeStrings>(new CVectorTypeStrings(
        charType, GetNamespaceName(), this, Comments()));
}

bool COctetStringDataType::IsCompressed(void) const
{
    return x_AsBitString();
}

bool COctetStringDataType::x_AsBitString(void) const
{
    string type = GetVar("_type");
    return NStr::FindNoCase(type, "CBitString") != NPOS;
}

string CBase64BinaryDataType::GetSchemaTypeString(void) const
{
    return "xs:base64Binary";
}

bool CBase64BinaryDataType::IsCompressed(void) const
{
    return true;
}

bool CBase64BinaryDataType::x_AsBitString(void) const
{
    return false;
}

CIntDataType::CIntDataType(void)
{
    ForbidVar("_type", "string");
}

const char* CIntDataType::GetASNKeyword(void) const
{
    return "INTEGER";
}

const char* CIntDataType::GetDEFKeyword(void) const
{
    return "_INTEGER_";
}

const char* CIntDataType::GetXMLContents(void) const
{
    return DTDEntitiesEnabled() ? "%INTEGER;" : "#PCDATA";
}

string CIntDataType::GetSchemaTypeString(void) const
{
    return "xs:integer";
}

bool CIntDataType::CheckValue(const CDataValue& value) const
{
    CheckValueType(value, CIntDataValue, "INTEGER");
    return true;
}

TObjectPtr CIntDataType::CreateDefault(const CDataValue& value) const
{
    return new Int4((Int4)dynamic_cast<const CIntDataValue&>(value).GetValue());
}

string CIntDataType::GetDefaultString(const CDataValue& value) const
{
    return NStr::Int8ToString(dynamic_cast<const CIntDataValue&>(value).GetValue());
}

CTypeRef CIntDataType::GetTypeInfo(void)
{
    if ( HaveModuleName() )
        return UpdateModuleName(CStdTypeInfo<Int4>::CreateTypeInfo());
    return &CStdTypeInfo<Int4>::GetTypeInfo;
}

const char* CIntDataType::GetDefaultCType(void) const
{
    return "int";
}

const char* CBigIntDataType::GetASNKeyword(void) const
{
    return "BigInt";
}

const char* CBigIntDataType::GetDEFKeyword(void) const
{
    return "_BigInt_";
}

const char* CBigIntDataType::GetXMLContents(void) const
{
    return DTDEntitiesEnabled() ? "%INTEGER;" : "#PCDATA";
}

string CBigIntDataType::GetSchemaTypeString(void) const
{
    return "xs:long";
}

bool CBigIntDataType::CheckValue(const CDataValue& value) const
{
    CheckValueType(value, CIntDataValue, "BigInt");
    return true;
}

TObjectPtr CBigIntDataType::CreateDefault(const CDataValue& value) const
{
    return new Int8(dynamic_cast<const CIntDataValue&>(value).GetValue());
}

string CBigIntDataType::GetDefaultString(const CDataValue& value) const
{
    return NStr::Int8ToString(dynamic_cast<const CIntDataValue&>(value).GetValue());
}

CTypeRef CBigIntDataType::GetTypeInfo(void)
{
    if ( HaveModuleName() )
        return UpdateModuleName(CStdTypeInfo<Int8>::CreateTypeInfo());
    return &CStdTypeInfo<Int8>::GetTypeInfo;
}

const char* CBigIntDataType::GetDefaultCType(void) const
{
    return "Int8";
}


bool CAnyContentDataType::CheckValue(const CDataValue& /* value */) const
{
    return true;
}

void CAnyContentDataType::PrintASN(CNcbiOstream& out, int /* indent */) const
{
    out << GetASNKeyword();
}

void CAnyContentDataType::PrintXMLSchema(CNcbiOstream& out, int indent, bool contents_only) const
{
    const CDataMember* mem = GetDataMember();
    if (mem) {
        PrintASNNewLine(out,indent)   << "<xs:any processContents=\"lax\"";
        const string& ns = GetNamespaceName();
        if (!ns.empty()) {
            out << " namespace=\"" << ns << "\"";
        }
        if (mem->Optional()) {
            out << " minOccurs=\"0\"";
        }
        out << "/>";
    } else {
        if (!contents_only) {
            PrintASNNewLine(out,indent++) <<
                "<xs:element name=\"" << XmlTagName() << "\">";
        }
        PrintASNNewLine(out,indent++) << "<xs:complexType>";
        PrintASNNewLine(out,indent++) << "<xs:sequence>";
        PrintASNNewLine(out,indent)   << "<xs:any processContents=\"lax\"/>";
        PrintASNNewLine(out,--indent) << "</xs:sequence>";
        PrintASNNewLine(out,--indent) << "</xs:complexType>";
        if (!contents_only) {
            PrintASNNewLine(out,--indent) << "</xs:element>";
        }
    }
}

void CAnyContentDataType::PrintDTDElement(CNcbiOstream& out, bool contents_only) const
{
    if (!contents_only) {
        out << "\n<!ELEMENT " << XmlTagName() << " ";
    }
    out << GetXMLContents();
    if (!contents_only) {
        out << ">";
    }
}

TObjectPtr CAnyContentDataType::CreateDefault(const CDataValue& value) const
{
    return new (string*)(new string(dynamic_cast<const CStringDataValue&>(value).GetValue()));
}

AutoPtr<CTypeStrings> CAnyContentDataType::GetFullCType(void) const
{
// TO BE CHANGED !!!
    string type = GetAndVerifyVar("_type");
    if ( type.empty() )
        type = GetDefaultCType();
    return AutoPtr<CTypeStrings>(new CAnyContentTypeStrings(type,Comments()));
}

const char* CAnyContentDataType::GetDefaultCType(void) const
{
    return "ncbi::CAnyContentObject";
}

const char* CAnyContentDataType::GetASNKeyword(void) const
{
// not exactly, but...
// (ASN.1 does not seem to support this type of data)
    return "VisibleString";
}

const char* CAnyContentDataType::GetDEFKeyword(void) const
{
    return "_AnyContent_";
}

const char* CAnyContentDataType::GetXMLContents(void) const
{
    return "ANY";
}

END_NCBI_SCOPE
