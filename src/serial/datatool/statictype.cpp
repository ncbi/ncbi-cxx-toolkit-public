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

#include "statictype.hpp"
#include "typestr.hpp"
#include "value.hpp"
#include <serial/stdtypes.hpp>
#include <serial/autoptrinfo.hpp>

void CStaticDataType::PrintASN(CNcbiOstream& out, int ) const
{
    out << GetASNKeyword();
}

void CStaticDataType::GetCType(CTypeStrings& tType, CClassCode& ) const
{
    string type = GetVar("_type");
    if ( type.empty() )
        type = GetDefaultCType();
    tType.SetStd(type);
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

bool CNullDataType::CheckValue(const CDataValue& value) const
{
    CheckValueType(value, CNullDataValue, "NULL");
    return true;
}

TObjectPtr CNullDataType::CreateDefault(const CDataValue& ) const
{
    THROW1_TRACE(runtime_error, "NULL type not implemented");
}

TTypeInfo CNullDataType::GetTypeInfo(void)
{
    THROW1_TRACE(runtime_error, "NULL type not implemented");
}

void CNullDataType::GenerateCode(CClassCode& ) const
{
}

const char* CBoolDataType::GetASNKeyword(void) const
{
    return "BOOLEAN";
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
    string s = "new bool(";
    s += (dynamic_cast<const CBoolDataValue&>(value).GetValue()? "true": "false");
    return s + ')';
}

TTypeInfo CBoolDataType::GetTypeInfo(void)
{
    return CStdTypeInfo<bool>::GetTypeInfo();
}

string CBoolDataType::GetDefaultCType(void) const
{
    return "bool";
}

const char* CRealDataType::GetASNKeyword(void) const
{
    return "REAL";
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

TTypeInfo CRealDataType::GetTypeInfo(void)
{
    return CStdTypeInfo<double>::GetTypeInfo();
}

string CRealDataType::GetDefaultCType(void) const
{
    return "double";
}

CStringDataType::CStringDataType(const string& asnKeyword)
    : m_ASNKeyword(asnKeyword)
{
}

const char* CStringDataType::GetASNKeyword(void) const
{
    return m_ASNKeyword.c_str();
}

bool CStringDataType::CheckValue(const CDataValue& value) const
{
    CheckValueType(value, CStringDataValue, "string");
    return true;
}

TObjectPtr CStringDataType::CreateDefault(const CDataValue& value) const
{
    return new string(dynamic_cast<const CStringDataValue&>(value).GetValue());
}

string CStringDataType::GetDefaultString(const CDataValue& value) const
{
    string s = "new NCBI_NS_STD::string(\"";
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
    return s + "\")";
}

TTypeInfo CStringDataType::GetTypeInfo(void)
{
    return CAutoPointerTypeInfo::GetTypeInfo(
        CStdTypeInfo<string>::GetTypeInfo());
}

string CStringDataType::GetDefaultCType(void) const
{
    return "NCBI_NS_STD::string";
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

TObjectPtr CBitStringDataType::CreateDefault(const CDataValue& ) const
{
    THROW1_TRACE(runtime_error, "BIT STRING default not implemented");
}

const char* COctetStringDataType::GetASNKeyword(void) const
{
    return "OCTET STRING";
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

void COctetStringDataType::GetCType(CTypeStrings& tType, CClassCode& ) const
{
    string charType = GetVar("_char");
    if ( charType.empty() )
        charType = "char";
    tType.SetTemplate("NCBI_NS_STD::vector<" + charType + '>',
                      "STL_CHAR_vector, (" + charType + ')');
    tType.AddHPPInclude("<vector>");
}

const char* CIntDataType::GetASNKeyword(void) const
{
    return "INTEGER";
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
    return "new int(" +
        NStr::IntToString(dynamic_cast<const CIntDataValue&>(value).GetValue()) +
        ')';
}

TTypeInfo CIntDataType::GetTypeInfo(void)
{
    return CStdTypeInfo<TInteger>::GetTypeInfo();
}

string CIntDataType::GetDefaultCType(void) const
{
    return "int";
}

