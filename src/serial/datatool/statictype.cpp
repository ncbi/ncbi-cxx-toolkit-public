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

#include <serial/datatool/statictype.hpp>
#include <serial/datatool/stdstr.hpp>
#include <serial/datatool/stlstr.hpp>
#include <serial/datatool/value.hpp>
#include <serial/stdtypes.hpp>
#include <serial/stltypes.hpp>
#include <serial/autoptrinfo.hpp>
#include <typeinfo>
#include <vector>

BEGIN_NCBI_SCOPE

void CStaticDataType::PrintASN(CNcbiOstream& out, int ) const
{
    out << GetASNKeyword();
}

void CStaticDataType::PrintDTD(CNcbiOstream& out) const
{
    out <<
        "<!ELEMENT "<<XmlTagName()<<" "<<GetXMLContents()<<">\n";
}

const char* CStaticDataType::GetXMLContents(void) const
{
    return 0;
}

AutoPtr<CTypeStrings> CStaticDataType::GetFullCType(void) const
{
    string type = GetVar("_type");
    if ( type.empty() )
        type = GetDefaultCType();
    return AutoPtr<CTypeStrings>(new CStdTypeStrings(type));
}

string CStaticDataType::GetDefaultCType(void) const
{
    CNcbiOstrstream msg;
    msg << typeid(*this).name() << ": Cannot generate C++ type: ";
    PrintASN(msg, 0);
    string message = CNcbiOstrstreamToString(msg);
    THROW1_TRACE(runtime_error, message);
}

const char* CNullDataType::GetASNKeyword(void) const
{
    return "NULL";
}

const char* CNullDataType::GetXMLContents(void) const
{
    return "%NULL;";
}

bool CNullDataType::CheckValue(const CDataValue& value) const
{
    CheckValueType(value, CNullDataValue, "NULL");
    return true;
}

TObjectPtr CNullDataType::CreateDefault(const CDataValue& ) const
{
    THROW1_TRACE(runtime_error, "NULL cannot have DEFAULT");
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

string CNullDataType::GetDefaultCType(void) const
{
    return "bool";
}

const char* CBoolDataType::GetASNKeyword(void) const
{
    return "BOOLEAN";
}

void CBoolDataType::PrintDTD(CNcbiOstream& out) const
{
    string tag = XmlTagName();
    out <<
        "<!ELEMENT "<<tag<<" %BOOLEAN; >\n"
        "<!ATTLIST "<<tag<<" value ( true | false ) #REQUIRED >\n"
        "\n";
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

string CBoolDataType::GetDefaultCType(void) const
{
    return "bool";
}

const char* CRealDataType::GetASNKeyword(void) const
{
    return "REAL";
}

const char* CRealDataType::GetXMLContents(void) const
{
    return "( %REAL; )";
}

bool CRealDataType::CheckValue(const CDataValue& value) const
{
    const CBlockDataValue* block = dynamic_cast<const CBlockDataValue*>(&value);
    if ( !block ) {
        value.Warning("REAL value expected");
        return false;
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

TObjectPtr CRealDataType::CreateDefault(const CDataValue& ) const
{
    THROW1_TRACE(runtime_error, "REAL default not implemented");
}

TTypeInfo CRealDataType::GetRealTypeInfo(void)
{
    if ( HaveModuleName() )
        return UpdateModuleName(CStdTypeInfo<double>::CreateTypeInfo());
    return CStdTypeInfo<double>::GetTypeInfo();
}

string CRealDataType::GetDefaultCType(void) const
{
    return "double";
}

CStringDataType::CStringDataType(void)
{
}

const char* CStringDataType::GetASNKeyword(void) const
{
    return "VisibleString";
}

const char* CStringDataType::GetXMLContents(void) const
{
    return "( #PCDATA )";
}

bool CStringDataType::CheckValue(const CDataValue& value) const
{
    CheckValueType(value, CStringDataValue, "string");
    return true;
}

TObjectPtr CStringDataType::CreateDefault(const CDataValue& value) const
{
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
    string type = GetVar("_type");
    if ( type.empty() )
        type = GetDefaultCType();
    return AutoPtr<CTypeStrings>(new CStringTypeStrings(type));
}

string CStringDataType::GetDefaultCType(void) const
{
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
    string type = GetVar("_type");
    if ( type.empty() ) {
        type = GetDefaultCType();
    }
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
    return "( %BITS; )";
}

TObjectPtr CBitStringDataType::CreateDefault(const CDataValue& ) const
{
    THROW1_TRACE(runtime_error, "BIT STRING default not implemented");
}

const char* COctetStringDataType::GetASNKeyword(void) const
{
    return "OCTET STRING";
}

const char* COctetStringDataType::GetXMLContents(void) const
{
    return "( %OCTETS; )";
}

bool COctetStringDataType::CheckValue(const CDataValue& value) const
{
    CheckValueType(value, CBitStringDataValue, "OCTET STRING");
    return true;
}

TObjectPtr COctetStringDataType::CreateDefault(const CDataValue& ) const
{
    THROW1_TRACE(runtime_error, "OCTET STRING default not implemented");
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

const char* CIntDataType::GetASNKeyword(void) const
{
    return "INTEGER";
}

const char* CIntDataType::GetXMLContents(void) const
{
    return "( %INTEGER; )";
}

bool CIntDataType::CheckValue(const CDataValue& value) const
{
    CheckValueType(value, CIntDataValue, "INTEGER");
    return true;
}

TObjectPtr CIntDataType::CreateDefault(const CDataValue& value) const
{
    return new int(dynamic_cast<const CIntDataValue&>(value).GetValue());
}

string CIntDataType::GetDefaultString(const CDataValue& value) const
{
    return NStr::IntToString(dynamic_cast<const CIntDataValue&>(value).GetValue());
}

CTypeRef CIntDataType::GetTypeInfo(void)
{
    if ( HaveModuleName() )
        return UpdateModuleName(CStdTypeInfo<AnyType::TInteger>::CreateTypeInfo());
    return &CStdTypeInfo<AnyType::TInteger>::GetTypeInfo;
}

string CIntDataType::GetDefaultCType(void) const
{
    return "int";
}

END_NCBI_SCOPE
