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
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.38  2005/02/09 14:35:20  gouriano
* Corrected formatting when writing DTD
*
* Revision 1.37  2005/02/02 19:08:36  gouriano
* Corrected DTD generation
*
* Revision 1.36  2005/02/01 21:47:14  grichenk
* Fixed warnings
*
* Revision 1.35  2004/05/17 21:03:14  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.34  2004/05/12 18:33:01  gouriano
* Added type conversion check (when using _type DEF file directive)
*
* Revision 1.33  2004/04/02 16:55:32  gouriano
* Added CRealDataType::CreateDefault method
*
* Revision 1.32  2004/02/25 19:45:19  gouriano
* Made it possible to define DEFAULT for data members of type REAL
*
* Revision 1.31  2004/01/22 20:44:25  gouriano
* Corrected generation of XML schema for boolean types
*
* Revision 1.30  2003/12/04 20:56:59  gouriano
* corrected DTD generation for Bool type
*
* Revision 1.29  2003/08/13 18:22:31  gouriano
* added conversion of ANY type DTD element to schema
*
* Revision 1.28  2003/08/13 15:45:54  gouriano
* implemented generation of code, which uses AnyContent objects
*
* Revision 1.27  2003/06/16 14:41:05  gouriano
* added possibility to convert DTD to XML schema
*
* Revision 1.26  2003/05/22 20:10:25  gouriano
* added UTF8 strings
*
* Revision 1.25  2003/05/14 14:42:22  gouriano
* added generation of XML schema
*
* Revision 1.24  2003/03/10 18:55:19  gouriano
* use new structured exceptions (based on CException)
*
* Revision 1.23  2003/02/10 17:56:15  gouriano
* make it possible to disable scope prefixes when reading and writing objects generated from ASN specification in XML format, or when converting an ASN spec into DTD.
*
* Revision 1.22  2001/02/15 21:39:14  kholodov
* Modified: pointer to parent CDataMember added to CDataType class.
* Modified: default value for BOOLEAN type in DTD is copied from ASN.1 spec.
*
* Revision 1.21  2000/12/15 15:38:51  vasilche
* Added support of Int8 and long double.
* Added support of BigInt ASN.1 extension - mapped to Int8.
* Enum values now have type Int4 instead of long.
*
* Revision 1.20  2000/11/20 17:26:33  vasilche
* Fixed warnings on 64 bit platforms.
* Updated names of config variables.
*
* Revision 1.19  2000/11/15 20:34:55  vasilche
* Added user comments to ENUMERATED types.
* Added storing of user comments to ASN.1 module definition.
*
* Revision 1.18  2000/11/14 21:41:26  vasilche
* Added preserving of ASN.1 definition comments.
*
* Revision 1.17  2000/11/08 17:02:52  vasilche
* Added generation of modular DTD files.
*
* Revision 1.16  2000/11/07 17:26:26  vasilche
* Added module names to CTypeInfo and CEnumeratedTypeValues
* Added possibility to set include directory for whole module
*
* Revision 1.15  2000/10/13 16:28:45  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* Revision 1.14  2000/10/03 17:22:50  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.13  2000/08/25 15:59:24  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.12  2000/07/03 18:42:57  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
*
* Revision 1.11  2000/05/24 20:09:29  vasilche
* Implemented DTD generation.
*
* Revision 1.10  2000/04/07 19:26:33  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.9  2000/03/10 15:00:46  vasilche
* Fixed OPTIONAL members reading.
*
* Revision 1.8  2000/02/01 21:48:05  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.7  2000/01/10 19:46:46  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.6  1999/12/03 21:42:13  vasilche
* Fixed conflict of enums in choices.
*
* Revision 1.5  1999/12/01 17:36:26  vasilche
* Fixed CHOICE processing.
*
* Revision 1.4  1999/11/18 17:13:06  vasilche
* Fixed generation of ENUMERATED CHOICE and VisibleString.
* Added generation of initializers to zero for primitive types and pointers.
*
* Revision 1.3  1999/11/16 15:41:16  vasilche
* Added plain pointer choice.
* By default we use C pointer instead of auto_ptr.
* Start adding initializers.
*
* Revision 1.2  1999/11/15 19:36:19  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <serial/datatool/exceptions.hpp>
#include <serial/datatool/statictype.hpp>
#include <serial/datatool/stdstr.hpp>
#include <serial/datatool/stlstr.hpp>
#include <serial/datatool/value.hpp>
#include <serial/datatool/blocktype.hpp>
#include <serial/stdtypes.hpp>
#include <serial/stltypes.hpp>
#include <serial/autoptrinfo.hpp>
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

void CStaticDataType::PrintDTDElement(CNcbiOstream& out, bool contents_only) const
{
    string tag(XmlTagName());
    string content(GetXMLContents());
    if (GetParentType() && 
        GetParentType()->GetDataMember() &&
        GetParentType()->GetDataMember()->Attlist()) {
        const CDataMember* mem = GetDataMember();
        out << tag << " CDATA ";
        if (mem->GetDefault()) {
            out << "\"" << mem->GetDefault()->GetXmlString() << "\"";
        } else {
            if (mem->Optional()) {
                out << "#IMPLIED";
            } else {
                out << "#REQUIRED";
            }
        }
        out << '\n';
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

// XML schema generator submitted by
// Marc Dumontier, Blueprint initiative, dumontier@mshri.on.ca
// modified by Andrei Gourianov, gouriano@ncbi
void CStaticDataType::PrintXMLSchemaElement(CNcbiOstream& out) const
{
    string tag( XmlTagName());
    PrintXMLSchemaElementWithTag( out, tag);
}

void CStaticDataType::PrintXMLSchemaElementWithTag(
    CNcbiOstream& out, const string& tag) const
{
    string tagOpen("<xs:element"), tagClose("</xs:element"), use;
    if (GetEnforcedStdXml() &&
        GetParentType() && 
        GetParentType()->GetDataMember() &&
        GetParentType()->GetDataMember()->Attlist()) {
        const CDataMember* mem = GetDataMember();
        tagOpen = "        <xs:attribute";
        tagClose= "        </xs:attribute";
        if (mem->Optional()) {
            use = "optional";
            if (mem->GetDefault()) {
                use += "\" default=\"" + mem->GetDefault()->GetXmlString();
            }
        } else {
            use = "required";
        }
    }
    string type;
    string contents;
    GetXMLSchemaContents(type,contents);

    out << tagOpen << " name=\"" << tag << "\"";
    if (!type.empty()) {
        out << " type=\"" << type << "\"";
    }
    if (!use.empty()) {
        out << " use=\"" << use << "\"";
    }
    if (!contents.empty()) {
        out << ">\n" << contents << tagClose << ">\n";
    } else {
        out << "/>\n";
    }
}

AutoPtr<CTypeStrings> CStaticDataType::GetFullCType(void) const
{
    string type = GetAndVerifyVar("_type");
    if ( type.empty() )
        type = GetDefaultCType();
    return AutoPtr<CTypeStrings>(new CStdTypeStrings(type));
}

const char* CNullDataType::GetASNKeyword(void) const
{
    return "NULL";
}

const char* CNullDataType::GetXMLContents(void) const
{
    return "EMPTY";
}

void CNullDataType::GetXMLSchemaContents(string& type, string& contents) const
{
    type.erase();
    contents = "  <xs:complexType/>\n";
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
    return AutoPtr<CTypeStrings>(new CNullTypeStrings());
}

const char* CNullDataType::GetDefaultCType(void) const
{
    return "bool";
}

const char* CBoolDataType::GetASNKeyword(void) const
{
    return "BOOLEAN";
}

const char* CBoolDataType::GetXMLContents(void) const
{
    return "%BOOLEAN;";
}

void CBoolDataType::GetXMLSchemaContents(string& type, string& contents) const
{
    type.erase();
    const CBoolDataValue *val = GetDataMember() ?
        dynamic_cast<const CBoolDataValue*>(GetDataMember()->GetDefault()) : 0;
    contents =
        "  <xs:complexType>\n"
        "    <xs:attribute name=\"value\" use=";
    if (val) {
        contents += "\"optional\" default=";
        contents += val->GetValue() ? "\"true\"" : "\"false\"";
    } else {
        contents += "\"required\"";
    }
    contents += ">\n"
        "      <xs:simpleType>\n"
        "        <xs:restriction base=\"xs:string\">\n"
        "          <xs:enumeration value=\"true\"/>\n"
        "          <xs:enumeration value=\"false\"/>\n"
        "        </xs:restriction>\n"
        "      </xs:simpleType>\n"
        "    </xs:attribute>\n"
        "  </xs:complexType>\n";
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

const char* CRealDataType::GetXMLContents(void) const
{
    return "%REAL;";
}

void CRealDataType::GetXMLSchemaContents(string& type, string& contents) const
{
    type = "xs:decimal";
    contents.erase();
}

bool CRealDataType::CheckValue(const CDataValue& value) const
{
    const CBlockDataValue* block = dynamic_cast<const CBlockDataValue*>(&value);
    if ( !block ) {
        return dynamic_cast<const CDoubleDataValue*>(&value) != 0;
    }
    if ( block->GetValues().size() != 3 ) {
        value.Warning("wrong number of elements in REAL value");
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
    return new double(dynamic_cast<const CDoubleDataValue&>(value).GetValue());
}

string CRealDataType::GetDefaultString(const CDataValue& value) const
{
    const CDoubleDataValue* dbl = dynamic_cast<const CDoubleDataValue*>(&value);
    if (dbl) {
        return NStr::DoubleToString(dbl->GetValue());
    }
    value.Warning("REAL value expected");
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

const char* CStringDataType::GetXMLContents(void) const
{
    return "#PCDATA";
}

void CStringDataType::GetXMLSchemaContents(string& type, string& contents) const
{
    type = "xs:string";
    contents.erase();
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
    return AutoPtr<CTypeStrings>(new CStringTypeStrings(type));
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
    return AutoPtr<CTypeStrings>(new CStringStoreTypeStrings(type));
}

const char* CBitStringDataType::GetASNKeyword(void) const
{
    return "BIT STRING";
}

bool CBitStringDataType::CheckValue(const CDataValue& value) const
{
    CheckValueType(value, CBitStringDataValue, "BIT STRING");
    return true;
}

const char* CBitStringDataType::GetXMLContents(void) const
{
    return "%BITS;";
}

void CBitStringDataType::GetXMLSchemaContents(string& type, string& contents) const
{
    type = "xs:hexBinary";
    contents.erase();
}

const char* COctetStringDataType::GetASNKeyword(void) const
{
    return "OCTET STRING";
}

const char* COctetStringDataType::GetDefaultCType(void) const
{
    return "NCBI_NS_STD::vector<char>";
}

const char* COctetStringDataType::GetXMLContents(void) const
{
    return "%OCTETS;";
}

void COctetStringDataType::GetXMLSchemaContents(string& type, string& contents) const
{
    type = "xs:hexBinary";
    contents.erase();
}

bool COctetStringDataType::CheckValue(const CDataValue& value) const
{
    CheckValueType(value, COctetStringDataType, "OCTET STRING");
    return true;
}

TTypeInfo COctetStringDataType::GetRealTypeInfo(void)
{
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
    string charType = GetVar("_char");
    if ( charType.empty() )
        charType = "char";
    return AutoPtr<CTypeStrings>(new CVectorTypeStrings(charType));
}

CIntDataType::CIntDataType(void)
{
    ForbidVar("_type", "string");
}

const char* CIntDataType::GetASNKeyword(void) const
{
    return "INTEGER";
}

const char* CIntDataType::GetXMLContents(void) const
{
    return "%INTEGER;";
}

void CIntDataType::GetXMLSchemaContents(string& type, string& contents) const
{
    type = "xs:integer";
    contents.erase();
}

bool CIntDataType::CheckValue(const CDataValue& value) const
{
    CheckValueType(value, CIntDataValue, "INTEGER");
    return true;
}

TObjectPtr CIntDataType::CreateDefault(const CDataValue& value) const
{
    return new Int4(dynamic_cast<const CIntDataValue&>(value).GetValue());
}

string CIntDataType::GetDefaultString(const CDataValue& value) const
{
    return NStr::IntToString(dynamic_cast<const CIntDataValue&>(value).GetValue());
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

const char* CBigIntDataType::GetXMLContents(void) const
{
    return "%INTEGER;";
}

void CBigIntDataType::GetXMLSchemaContents(string& type, string& contents) const
{
    type = "xs:integer";
    contents.erase();
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
    return NStr::IntToString(dynamic_cast<const CIntDataValue&>(value).GetValue());
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

void CAnyContentDataType::PrintXMLSchemaElement(CNcbiOstream& out) const
{
    out << 
        "<xs:element name=\"" << XmlTagName() << "\">\n"
        "  <xs:complexType>\n"
        "    <xs:sequence>\n"
        "      <xs:any processContext=\"lax\"/>\n"
        "    </xs:sequence>\n"
        "  </xs:complexType>\n"
        "</xs:element>\n";
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
    return AutoPtr<CTypeStrings>(new CAnyContentTypeStrings(type));
}

const char* CAnyContentDataType::GetDefaultCType(void) const
{
    return "ncbi::CAnyContentObject";
}

const char* CAnyContentDataType::GetASNKeyword(void) const
{
// not exactly, but...
// (ASN.1 does not seem to suppport this type of data)
    return "VisibleString";
}

const char* CAnyContentDataType::GetXMLContents(void) const
{
    return "ANY";
}

void CAnyContentDataType::GetXMLSchemaContents(string& type, string& contents) const
{
    type.erase();
    contents.erase();
}

END_NCBI_SCOPE
