/* $Id$
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
 * Author:  Sergey Sikorskiy
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include <dbapi/driver/dbapi_object_convert.hpp>
#include <dbapi/driver/exception.hpp>
#include <corelib/ncbi_safe_static.hpp>

#include <common/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE 

namespace value_slice
{

////////////////////////////////////////////////////////////////////////////////
static
void
ReportTypeConvError(EDB_Type from_type, EDB_Type to_type)
{
    string err_str("Cannot convert type " );
    err_str += CDB_Object::GetTypeName(from_type, false);
    err_str += " to type ";
    err_str += CDB_Object::GetTypeName(to_type, false);
    
    DATABASE_DRIVER_ERROR(err_str, 101100);
}

static
void
ReportTypeConvError(EDB_Type from_type, const char* to_type)
{
    string err_str("Cannot convert type " );
    err_str += CDB_Object::GetTypeName(from_type, false);
    err_str += " to type ";
    err_str += to_type;
    
    DATABASE_DRIVER_ERROR(err_str, 101100);
}

inline
void
CheckNULL(const CDB_Object& value)
{
    if (value.IsNULL()) {
        DATABASE_DRIVER_ERROR("Trying to access a NULL value.", 101100);
    }
}

inline
void
CheckType(const CDB_Object& value, EDB_Type type1)
{
    EDB_Type cur_type = value.GetType();

    if (cur_type != type1) {
        ReportTypeConvError(cur_type, type1);
    }
}

inline
void
CheckType(const CDB_Object& value, EDB_Type type1, EDB_Type type2)
{
    EDB_Type cur_type = value.GetType();

    if (!(cur_type == type1 || cur_type == type2)) {
        DATABASE_DRIVER_ERROR(string("Invalid type conversion: have ")
                              + CDB_Object::GetTypeName(cur_type, false)
                              + " but need either "
                              + CDB_Object::GetTypeName(type1, false)
                              + " or " + CDB_Object::GetTypeName(type2, false),
                              101100);
    }
}

////////////////////////////////////////////////////////////////////////////////
CValueConvert<SSafeCP, CDB_Object>::CValueConvert(obj_type& value)
: m_Value(value)
{
}

CValueConvert<SSafeCP, CDB_Object>::operator bool(void) const
{
    CheckNULL(m_Value);
    CheckType(m_Value, eDB_Bit);

    return ConvertSafe(static_cast<const CDB_Bit&>(m_Value).Value());
}

CValueConvert<SSafeCP, CDB_Object>::operator Uint1(void) const
{
    CheckNULL(m_Value);

    const EDB_Type cur_type = m_Value.GetType();

    switch (cur_type) {
        case eDB_TinyInt:
            return Convert(static_cast<const CDB_TinyInt&>(m_Value).Value());
        case eDB_Bit:
            // CDB_Bit is for some reason "Int4" ...
            // return Convert(static_cast<const CDB_Bit&>(m_Value).Value());
            return (static_cast<const CDB_Bit&>(m_Value).Value() == 0 ? 0 : 1);
        default:
            ReportTypeConvError(cur_type, "Uint1");
    }

    return 0;
}

CValueConvert<SSafeCP, CDB_Object>::operator Int2(void) const
{
    CheckNULL(m_Value);

    const EDB_Type cur_type = m_Value.GetType();

    switch (cur_type) {
        case eDB_SmallInt:
            return ConvertSafe(static_cast<const CDB_SmallInt&>(m_Value).Value());
        case eDB_TinyInt:
            return ConvertSafe(static_cast<const CDB_TinyInt&>(m_Value).Value());
        case eDB_Bit:
            // CDB_Bit is for some reason "Int4" ...
            // return Convert(static_cast<const CDB_Bit&>(m_Value).Value());
            return (static_cast<const CDB_Bit&>(m_Value).Value() == 0 ? 0 : 1);
        default:
            ReportTypeConvError(cur_type, "Int2");
    }

    return 0;
}

CValueConvert<SSafeCP, CDB_Object>::operator Int4(void) const
{
    CheckNULL(m_Value);

    const EDB_Type cur_type = m_Value.GetType();

    switch (cur_type) {
        case eDB_Int:
            return ConvertSafe(static_cast<const CDB_Int&>(m_Value).Value());
        case eDB_SmallInt:
            return ConvertSafe(static_cast<const CDB_SmallInt&>(m_Value).Value());
        case eDB_TinyInt:
            return ConvertSafe(static_cast<const CDB_TinyInt&>(m_Value).Value());
        case eDB_Bit:
            return ConvertSafe(static_cast<const CDB_Bit&>(m_Value).Value());
        default:
            ReportTypeConvError(cur_type, "Int4");
    }

    return 0;
}

CValueConvert<SSafeCP, CDB_Object>::operator Int8(void) const
{
    CheckNULL(m_Value);

    const EDB_Type cur_type = m_Value.GetType();

    switch (cur_type) {
        case eDB_BigInt:
            return ConvertSafe(static_cast<const CDB_BigInt&>(m_Value).Value());
        case eDB_Int:
            return ConvertSafe(static_cast<const CDB_Int&>(m_Value).Value());
        case eDB_SmallInt:
            return ConvertSafe(static_cast<const CDB_SmallInt&>(m_Value).Value());
        case eDB_TinyInt:
            return ConvertSafe(static_cast<const CDB_TinyInt&>(m_Value).Value());
        case eDB_Bit:
            return ConvertSafe(static_cast<const CDB_Bit&>(m_Value).Value());
        default:
            ReportTypeConvError(cur_type, "Int8");
    }

    return 0;
}

CValueConvert<SSafeCP, CDB_Object>::operator float(void) const
{
    CheckNULL(m_Value);

    const EDB_Type cur_type = m_Value.GetType();

    switch (cur_type) {
        case eDB_Float:
            return ConvertSafe(static_cast<const CDB_Float&>(m_Value).Value());
        case eDB_BigInt:
            return ConvertSafe(static_cast<const CDB_BigInt&>(m_Value).Value());
        case eDB_Int:
            return ConvertSafe(static_cast<const CDB_Int&>(m_Value).Value());
        case eDB_SmallInt:
            return ConvertSafe(static_cast<const CDB_SmallInt&>(m_Value).Value());
        case eDB_TinyInt:
            return ConvertSafe(static_cast<const CDB_TinyInt&>(m_Value).Value());
        case eDB_Bit:
            return ConvertSafe(static_cast<const CDB_Bit&>(m_Value).Value());
        default:
            ReportTypeConvError(cur_type, "float");
    }

    return 0.0;
}

CValueConvert<SSafeCP, CDB_Object>::operator double(void) const
{
    CheckNULL(m_Value);

    const EDB_Type cur_type = m_Value.GetType();

    switch (cur_type) {
        case eDB_Double:
            return ConvertSafe(static_cast<const CDB_Double&>(m_Value).Value());
        case eDB_Float:
            return ConvertSafe(static_cast<const CDB_Float&>(m_Value).Value());
        case eDB_BigInt:
            return ConvertSafe(static_cast<const CDB_BigInt&>(m_Value).Value());
        case eDB_Int:
            return ConvertSafe(static_cast<const CDB_Int&>(m_Value).Value());
        case eDB_SmallInt:
            return ConvertSafe(static_cast<const CDB_SmallInt&>(m_Value).Value());
        case eDB_TinyInt:
            return ConvertSafe(static_cast<const CDB_TinyInt&>(m_Value).Value());
        case eDB_Bit:
            return ConvertSafe(static_cast<const CDB_Bit&>(m_Value).Value());
        default:
            ReportTypeConvError(cur_type, "double");
    }

    return 0.0;
}

CValueConvert<SSafeCP, CDB_Object>::operator string(void) const
{
    CheckNULL(m_Value);

    string result;

    const EDB_Type cur_type = m_Value.GetType();

    switch (cur_type) {
        case eDB_Int:
            return NCBI_CONVERT_TO(ConvertSafe(static_cast<const CDB_Int&>(m_Value).Value()), std::string);
        case eDB_SmallInt:
            return NCBI_CONVERT_TO(ConvertSafe(static_cast<const CDB_SmallInt&>(m_Value).Value()), std::string);
        case eDB_TinyInt:
            return NCBI_CONVERT_TO(ConvertSafe(static_cast<const CDB_TinyInt&>(m_Value).Value()), std::string);
        case eDB_BigInt:
            return NCBI_CONVERT_TO(ConvertSafe(static_cast<const CDB_BigInt&>(m_Value).Value()), std::string);
        case eDB_Bit:
            return NCBI_CONVERT_TO(ConvertSafe(static_cast<const CDB_Bit&>(m_Value).Value()), std::string);
        case eDB_Float:
            return NCBI_CONVERT_TO(ConvertSafe(static_cast<const CDB_Float&>(m_Value).Value()), std::string);
        case eDB_Double:
            return NCBI_CONVERT_TO(ConvertSafe(static_cast<const CDB_Double&>(m_Value).Value()),  std::string);
        case eDB_Numeric:
            return NCBI_CONVERT_TO(ConvertSafe(static_cast<const CDB_Numeric&>(m_Value).Value()), std::string);
        case eDB_Char:
        case eDB_VarChar:
        case eDB_LongChar:
            {
                const CDB_String& cdb_str = static_cast<const CDB_String&>(m_Value);
                const string& str = cdb_str.AsString();
                return ConvertSafe(str);
            }
        case eDB_Binary:
            return ConvertSafe(string(
                static_cast<const char*>(static_cast<const CDB_Binary&>(m_Value).Value()),
                static_cast<const CDB_Binary&>(m_Value).Size()
            ));
        case eDB_VarBinary:
            return ConvertSafe(string(
                static_cast<const char*>(static_cast<const CDB_VarBinary&>(m_Value).Value()),
                static_cast<const CDB_VarBinary&>(m_Value).Size()
            ));
        case eDB_LongBinary:
            return Convert(string(
                static_cast<const char*>(static_cast<const CDB_LongBinary&>(m_Value).Value()),
                static_cast<const CDB_LongBinary&>(m_Value).DataSize()
            ));
        case eDB_Text:
        case eDB_Image: 
            {
                CDB_Stream& strm = const_cast<CDB_Stream&>(static_cast<const CDB_Stream&>(m_Value));
                result.resize(strm.Size());
                strm.Read(const_cast<void*>(static_cast<const void*>(result.data())),
                        strm.Size()
                        );
            }
            break;
        case eDB_DateTime: 
            return NCBI_CONVERT_TO(ConvertSafe(static_cast<const CDB_DateTime&>(m_Value).Value()), std::string);
        case eDB_SmallDateTime: 
            return NCBI_CONVERT_TO(ConvertSafe(static_cast<const CDB_SmallDateTime&>(m_Value).Value()), std::string);
        default:
            ReportTypeConvError(cur_type, "string");
            break;
    }


    return Convert(result);
}

CValueConvert<SSafeCP, CDB_Object>::operator const CTime&(void) const
{
    CheckNULL(m_Value);
    CheckType(m_Value, eDB_SmallDateTime, eDB_DateTime);

    EDB_Type cur_type = m_Value.GetType();
    
    if (cur_type == eDB_SmallDateTime) {
        return static_cast<const CDB_SmallDateTime&>(m_Value).Value();
    } else if (cur_type == eDB_DateTime) {
        return static_cast<const CDB_DateTime&>(m_Value).Value();
    } else {
        ReportTypeConvError(cur_type, "CTime");
    }

    static CSafeStatic<CTime> value;
    return value.Get();
}

////////////////////////////////////////////////////////////////////////////////
CValueConvert<SSafeSqlCP, CDB_Object>::CValueConvert(obj_type& value)
: m_Value(value)
{
}

CValueConvert<SSafeSqlCP, CDB_Object>::operator bool(void) const
{
    if (m_Value.IsNULL()) {
       return bool();
    }

    CheckType(m_Value, eDB_Bit);

    return ConvertSafe(static_cast<const CDB_Bit&>(m_Value).Value());
}

CValueConvert<SSafeSqlCP, CDB_Object>::operator Uint1(void) const
{
    if (m_Value.IsNULL()) {
       return Uint1();
    }

    const EDB_Type cur_type = m_Value.GetType();

    switch (cur_type) {
        case eDB_TinyInt:
            return Convert(static_cast<const CDB_TinyInt&>(m_Value).Value());
        case eDB_Bit:
            // CDB_Bit is for some reason "Int4" ...
            // return Convert(static_cast<const CDB_Bit&>(m_Value).Value());
            return (static_cast<const CDB_Bit&>(m_Value).Value() == 0 ? 0 : 1);
        default:
            ReportTypeConvError(cur_type, "Uint1");
    }

    return 0;
}

CValueConvert<SSafeSqlCP, CDB_Object>::operator Int2(void) const
{
    if (m_Value.IsNULL()) {
       return Int2();
    }

    const EDB_Type cur_type = m_Value.GetType();

    switch (cur_type) {
        case eDB_SmallInt:
            return ConvertSafe(static_cast<const CDB_SmallInt&>(m_Value).Value());
        case eDB_TinyInt:
            return ConvertSafe(static_cast<const CDB_TinyInt&>(m_Value).Value());
        case eDB_Bit:
            // CDB_Bit is for some reason "Int4" ...
            // return Convert(static_cast<const CDB_Bit&>(m_Value).Value());
            return (static_cast<const CDB_Bit&>(m_Value).Value() == 0 ? 0 : 1);
        default:
            ReportTypeConvError(cur_type, "Int2");
    }

    return 0;
}

CValueConvert<SSafeSqlCP, CDB_Object>::operator Int4(void) const
{
    if (m_Value.IsNULL()) {
       return Int4();
    }

    const EDB_Type cur_type = m_Value.GetType();

    switch (cur_type) {
        case eDB_Int:
            return ConvertSafe(static_cast<const CDB_Int&>(m_Value).Value());
        case eDB_SmallInt:
            return ConvertSafe(static_cast<const CDB_SmallInt&>(m_Value).Value());
        case eDB_TinyInt:
            return ConvertSafe(static_cast<const CDB_TinyInt&>(m_Value).Value());
        case eDB_Bit:
            return ConvertSafe(static_cast<const CDB_Bit&>(m_Value).Value());
        default:
            ReportTypeConvError(cur_type, "Int4");
    }

    return 0;
}

CValueConvert<SSafeSqlCP, CDB_Object>::operator Int8(void) const
{
    if (m_Value.IsNULL()) {
       return Int8();
    }

    const EDB_Type cur_type = m_Value.GetType();

    switch (cur_type) {
        case eDB_BigInt:
            return ConvertSafe(static_cast<const CDB_BigInt&>(m_Value).Value());
        case eDB_Int:
            return ConvertSafe(static_cast<const CDB_Int&>(m_Value).Value());
        case eDB_SmallInt:
            return ConvertSafe(static_cast<const CDB_SmallInt&>(m_Value).Value());
        case eDB_TinyInt:
            return ConvertSafe(static_cast<const CDB_TinyInt&>(m_Value).Value());
        case eDB_Bit:
            return ConvertSafe(static_cast<const CDB_Bit&>(m_Value).Value());
        default:
            ReportTypeConvError(cur_type, "Int8");
    }

    return 0;
}

CValueConvert<SSafeSqlCP, CDB_Object>::operator float(void) const
{
    if (m_Value.IsNULL()) {
       return float();
    }

    const EDB_Type cur_type = m_Value.GetType();

    switch (cur_type) {
        case eDB_Float:
            return ConvertSafe(static_cast<const CDB_Float&>(m_Value).Value());
        case eDB_BigInt:
            return ConvertSafe(static_cast<const CDB_BigInt&>(m_Value).Value());
        case eDB_Int:
            return ConvertSafe(static_cast<const CDB_Int&>(m_Value).Value());
        case eDB_SmallInt:
            return ConvertSafe(static_cast<const CDB_SmallInt&>(m_Value).Value());
        case eDB_TinyInt:
            return ConvertSafe(static_cast<const CDB_TinyInt&>(m_Value).Value());
        case eDB_Bit:
            return ConvertSafe(static_cast<const CDB_Bit&>(m_Value).Value());
        default:
            ReportTypeConvError(cur_type, "float");
    }

    return 0.0;
}

CValueConvert<SSafeSqlCP, CDB_Object>::operator double(void) const
{
    if (m_Value.IsNULL()) {
       return double();
    }

    const EDB_Type cur_type = m_Value.GetType();

    switch (cur_type) {
        case eDB_Double:
            return ConvertSafe(static_cast<const CDB_Double&>(m_Value).Value());
        case eDB_Float:
            return ConvertSafe(static_cast<const CDB_Float&>(m_Value).Value());
        case eDB_BigInt:
            return ConvertSafe(static_cast<const CDB_BigInt&>(m_Value).Value());
        case eDB_Int:
            return ConvertSafe(static_cast<const CDB_Int&>(m_Value).Value());
        case eDB_SmallInt:
            return ConvertSafe(static_cast<const CDB_SmallInt&>(m_Value).Value());
        case eDB_TinyInt:
            return ConvertSafe(static_cast<const CDB_TinyInt&>(m_Value).Value());
        case eDB_Bit:
            return ConvertSafe(static_cast<const CDB_Bit&>(m_Value).Value());
        default:
            ReportTypeConvError(cur_type, "double");
    }

    return 0.0;
}

CValueConvert<SSafeSqlCP, CDB_Object>::operator string(void) const
{
    if (m_Value.IsNULL()) {
       return string();
    }

    string result;

    const EDB_Type cur_type = m_Value.GetType();

    switch (cur_type) {
        case eDB_Int:
            return NCBI_CONVERT_TO(ConvertSafe(static_cast<const CDB_Int&>(m_Value).Value()), std::string);
        case eDB_SmallInt:
            return NCBI_CONVERT_TO(ConvertSafe(static_cast<const CDB_SmallInt&>(m_Value).Value()), std::string);
        case eDB_TinyInt:
            return NCBI_CONVERT_TO(ConvertSafe(static_cast<const CDB_TinyInt&>(m_Value).Value()), std::string);
        case eDB_BigInt:
            return NCBI_CONVERT_TO(ConvertSafe(static_cast<const CDB_BigInt&>(m_Value).Value()), std::string);
        case eDB_Bit:
            return NCBI_CONVERT_TO(ConvertSafe(static_cast<const CDB_Bit&>(m_Value).Value()), std::string);
        case eDB_Float:
            return NCBI_CONVERT_TO(ConvertSafe(static_cast<const CDB_Float&>(m_Value).Value()), std::string);
        case eDB_Double:
            return NCBI_CONVERT_TO(ConvertSafe(static_cast<const CDB_Double&>(m_Value).Value()), std::string);
        case eDB_Numeric:
            return NCBI_CONVERT_TO(ConvertSafe(static_cast<const CDB_Numeric&>(m_Value).Value()), std::string);
        case eDB_Char:
        case eDB_VarChar:
        case eDB_LongChar:
            {
                const CDB_String& cdb_str = static_cast<const CDB_String&>(m_Value);
                const string& str = cdb_str.AsString();
                return ConvertSafe(str);
            }
        case eDB_Binary:
            return ConvertSafe(string(
                static_cast<const char*>(static_cast<const CDB_Binary&>(m_Value).Value()),
                static_cast<const CDB_Binary&>(m_Value).Size()
            ));
        case eDB_VarBinary:
            return ConvertSafe(string(
                static_cast<const char*>(static_cast<const CDB_VarBinary&>(m_Value).Value()),
                static_cast<const CDB_VarBinary&>(m_Value).Size()
            ));
        case eDB_LongBinary:
            return Convert(string(
                static_cast<const char*>(static_cast<const CDB_LongBinary&>(m_Value).Value()),
                static_cast<const CDB_LongBinary&>(m_Value).DataSize()
            ));
        case eDB_Text:
        case eDB_Image: 
            {
                CDB_Stream& strm = const_cast<CDB_Stream&>(static_cast<const CDB_Stream&>(m_Value));
                result.resize(strm.Size());
                strm.Read(const_cast<void*>(static_cast<const void*>(result.data())),
                        strm.Size()
                        );
            }
            break;
        case eDB_DateTime: 
            return NCBI_CONVERT_TO(ConvertSafe(static_cast<const CDB_DateTime&>(m_Value).Value()), std::string);
        case eDB_SmallDateTime: 
            return NCBI_CONVERT_TO(ConvertSafe(static_cast<const CDB_SmallDateTime&>(m_Value).Value()), std::string);
        default:
            ReportTypeConvError(cur_type, "string");
            break;
    }


    return Convert(result);
}

CValueConvert<SSafeSqlCP, CDB_Object>::operator const CTime&(void) const
{
    static CSafeStatic<CTime> value;

    if (m_Value.IsNULL()) {
       return value.Get();
    }

    CheckType(m_Value, eDB_SmallDateTime, eDB_DateTime);

    EDB_Type cur_type = m_Value.GetType();
    
    if (cur_type == eDB_SmallDateTime) {
        return static_cast<const CDB_SmallDateTime&>(m_Value).Value();
    } else if (cur_type == eDB_DateTime) {
        return static_cast<const CDB_DateTime&>(m_Value).Value();
    } else {
        ReportTypeConvError(cur_type, "CTime");
    }

    return value.Get();
}

////////////////////////////////////////////////////////////////////////////////
template <typename TO>
inline
TO Convert_CDB_Object(const CDB_Object& value)
{
    CheckNULL(value);

    const EDB_Type cur_type = value.GetType();

    switch (cur_type) {
        case eDB_BigInt:
            return NCBI_CONVERT_TO(Convert(static_cast<const CDB_BigInt&>(value).Value()), TO);
        case eDB_Int:
            return NCBI_CONVERT_TO(Convert(static_cast<const CDB_Int&>(value).Value()), TO);
        case eDB_SmallInt:
            return NCBI_CONVERT_TO(Convert(static_cast<const CDB_SmallInt&>(value).Value()), TO);
        case eDB_TinyInt:
            return NCBI_CONVERT_TO(Convert(static_cast<const CDB_TinyInt&>(value).Value()), TO);
        case eDB_Bit:
            return NCBI_CONVERT_TO(Convert(static_cast<const CDB_Bit&>(value).Value()), TO);
        case eDB_Float:
            return NCBI_CONVERT_TO(Convert(static_cast<const CDB_Float&>(value).Value()), TO);
        case eDB_Double:
            return NCBI_CONVERT_TO(Convert(static_cast<const CDB_Double&>(value).Value()), TO);
        case eDB_Numeric:
            return NCBI_CONVERT_TO(Convert(static_cast<const CDB_Numeric&>(value).Value()), TO);
        case eDB_Char:
        case eDB_VarChar:
        case eDB_LongChar:
            {
                const CDB_String& cdb_str = static_cast<const CDB_String&>(value);
                const string& str = cdb_str.AsString();
                return Convert(str);
            }
        case eDB_Binary:
            return Convert(string(
                static_cast<const char*>(static_cast<const CDB_Binary&>(value).Value()),
                static_cast<const CDB_Binary&>(value).Size()
            ));
        case eDB_VarBinary:
            return Convert(string(
                static_cast<const char*>(static_cast<const CDB_VarBinary&>(value).Value()),
                static_cast<const CDB_VarBinary&>(value).Size()
            ));
        case eDB_LongBinary:
            return Convert(string(
                static_cast<const char*>(static_cast<const CDB_LongBinary&>(value).Value()),
                static_cast<const CDB_LongBinary&>(value).DataSize()
            ));
        case eDB_Text:
        case eDB_Image: 
            {
                string result;
                CDB_Stream& strm = const_cast<CDB_Stream&>(static_cast<const CDB_Stream&>(value));
                result.resize(strm.Size());
                strm.Read(const_cast<void*>(static_cast<const void*>(result.data())),
                        strm.Size()
                        );
                return Convert(result);
            }
        default:
            ReportTypeConvError(cur_type, "bool");
    }

    return  TO();
}

template <typename TO>
inline
TO Convert_CDB_Object_DT(const CDB_Object& value)
{
    CheckNULL(value);

    const EDB_Type cur_type = value.GetType();

    switch (cur_type) {
        case eDB_DateTime:
            return NCBI_CONVERT_TO(Convert(static_cast<const CDB_DateTime&>(value).Value()), TO);
        case eDB_SmallDateTime:
            return NCBI_CONVERT_TO(Convert(static_cast<const CDB_SmallDateTime&>(value).Value()), TO);
        default:
            ReportTypeConvError(cur_type, "bool");
    }

    return  TO();
}

////////////////////////////////////////////////////////////////////////////////
CValueConvert<SRunTimeCP, CDB_Object>::CValueConvert(obj_type& value)
: m_Value(value)
{
}

CValueConvert<SRunTimeCP, CDB_Object>::operator bool(void) const
{
    EDB_Type cur_type = m_Value.GetType();
    
    if (cur_type == eDB_SmallDateTime || cur_type == eDB_DateTime) {
        return Convert_CDB_Object_DT<bool>(m_Value);
    }

    return Convert_CDB_Object<bool>(m_Value);
}

CValueConvert<SRunTimeCP, CDB_Object>::operator Uint1(void) const
{
    return Convert_CDB_Object<Uint1>(m_Value);
}

CValueConvert<SRunTimeCP, CDB_Object>::operator Int2(void) const
{
    return Convert_CDB_Object<Int2>(m_Value);
}

CValueConvert<SRunTimeCP, CDB_Object>::operator Int4(void) const
{
    return Convert_CDB_Object<Int4>(m_Value);
}

CValueConvert<SRunTimeCP, CDB_Object>::operator Int8(void) const
{
    return Convert_CDB_Object<Int8>(m_Value);
}

CValueConvert<SRunTimeCP, CDB_Object>::operator float(void) const
{
    return Convert_CDB_Object<float>(m_Value);
}

CValueConvert<SRunTimeCP, CDB_Object>::operator double(void) const
{
    return Convert_CDB_Object<double>(m_Value);
}

CValueConvert<SRunTimeCP, CDB_Object>::operator string(void) const
{
    EDB_Type cur_type = m_Value.GetType();
    
    if (cur_type == eDB_SmallDateTime || cur_type == eDB_DateTime) {
        return Convert_CDB_Object_DT<string>(m_Value);
    }

    return Convert_CDB_Object<string>(m_Value);
}

CValueConvert<SRunTimeCP, CDB_Object>::operator const CTime&(void) const
{
    CheckNULL(m_Value);
    CheckType(m_Value, eDB_SmallDateTime, eDB_DateTime);

    EDB_Type cur_type = m_Value.GetType();
    
    if (cur_type == eDB_SmallDateTime) {
        return static_cast<const CDB_SmallDateTime&>(m_Value).Value();
    } else if (cur_type == eDB_DateTime) {
        return static_cast<const CDB_DateTime&>(m_Value).Value();
    } else {
        ReportTypeConvError(cur_type, "CTime");
    }

    static CSafeStatic<CTime> value;
    return value.Get();
}

////////////////////////////////////////////////////////////////////////////////
template <typename TO>
inline
TO Convert_CDB_ObjectSql(const CDB_Object& value)
{
    if (value.IsNULL()) {
       return TO();
    }

    const EDB_Type cur_type = value.GetType();

    switch (cur_type) {
        case eDB_BigInt:
            return NCBI_CONVERT_TO(Convert(static_cast<const CDB_BigInt&>(value).Value()), TO);
        case eDB_Int:
            return NCBI_CONVERT_TO(Convert(static_cast<const CDB_Int&>(value).Value()), TO);
        case eDB_SmallInt:
            return NCBI_CONVERT_TO(Convert(static_cast<const CDB_SmallInt&>(value).Value()), TO);
        case eDB_TinyInt:
            return NCBI_CONVERT_TO(Convert(static_cast<const CDB_TinyInt&>(value).Value()), TO);
        case eDB_Bit:
            return NCBI_CONVERT_TO(Convert(static_cast<const CDB_Bit&>(value).Value()), TO);
        case eDB_Float:
            return NCBI_CONVERT_TO(Convert(static_cast<const CDB_Float&>(value).Value()), TO);
        case eDB_Double:
            return NCBI_CONVERT_TO(Convert(static_cast<const CDB_Double&>(value).Value()), TO);
        case eDB_Numeric:
            return NCBI_CONVERT_TO(Convert(static_cast<const CDB_Numeric&>(value).Value()), TO);
        case eDB_Char:
        case eDB_VarChar:
        case eDB_LongChar:
            {
                const CDB_String& cdb_str = static_cast<const CDB_String&>(value);
                const string& str = cdb_str.AsString();
                return Convert(str);
            }
        case eDB_Binary:
            return Convert(string(
                static_cast<const char*>(static_cast<const CDB_Binary&>(value).Value()),
                static_cast<const CDB_Binary&>(value).Size()
            ));
        case eDB_VarBinary:
            return Convert(string(
                static_cast<const char*>(static_cast<const CDB_VarBinary&>(value).Value()),
                static_cast<const CDB_VarBinary&>(value).Size()
            ));
        case eDB_LongBinary:
            return Convert(string(
                static_cast<const char*>(static_cast<const CDB_LongBinary&>(value).Value()),
                static_cast<const CDB_LongBinary&>(value).DataSize()
            ));
        case eDB_Text:
        case eDB_Image: 
            {
                string result;
                CDB_Stream& strm = const_cast<CDB_Stream&>(static_cast<const CDB_Stream&>(value));
                result.resize(strm.Size());
                strm.Read(const_cast<void*>(static_cast<const void*>(result.data())),
                        strm.Size()
                        );
                return Convert(result);
            }
        default:
            ReportTypeConvError(cur_type, "bool");
    }

    return  TO();
}

template <typename TO>
inline
TO Convert_CDB_ObjectSql_DT(const CDB_Object& value)
{
    if (value.IsNULL()) {
       return TO();
    }

    const EDB_Type cur_type = value.GetType();

    switch (cur_type) {
        case eDB_DateTime:
            return NCBI_CONVERT_TO(Convert(static_cast<const CDB_DateTime&>(value).Value()), TO);
        case eDB_SmallDateTime:
            return NCBI_CONVERT_TO(Convert(static_cast<const CDB_SmallDateTime&>(value).Value()), TO);
        default:
            ReportTypeConvError(cur_type, "bool");
    }

    return  TO();
}

////////////////////////////////////////////////////////////////////////////////
CValueConvert<SRunTimeSqlCP, CDB_Object>::CValueConvert(obj_type& value)
: m_Value(value)
{
}

CValueConvert<SRunTimeSqlCP, CDB_Object>::operator bool(void) const
{
    EDB_Type cur_type = m_Value.GetType();
    
    if (cur_type == eDB_SmallDateTime || cur_type == eDB_DateTime) {
        return Convert_CDB_ObjectSql_DT<bool>(m_Value);
    }

    return Convert_CDB_ObjectSql<bool>(m_Value);
}

CValueConvert<SRunTimeSqlCP, CDB_Object>::operator Uint1(void) const
{
    return Convert_CDB_ObjectSql<Uint1>(m_Value);
}

CValueConvert<SRunTimeSqlCP, CDB_Object>::operator Int2(void) const
{
    return Convert_CDB_ObjectSql<Int2>(m_Value);
}

CValueConvert<SRunTimeSqlCP, CDB_Object>::operator Int4(void) const
{
    return Convert_CDB_ObjectSql<Int4>(m_Value);
}

CValueConvert<SRunTimeSqlCP, CDB_Object>::operator Int8(void) const
{
    return Convert_CDB_ObjectSql<Int8>(m_Value);
}

CValueConvert<SRunTimeSqlCP, CDB_Object>::operator float(void) const
{
    return Convert_CDB_ObjectSql<float>(m_Value);
}

CValueConvert<SRunTimeSqlCP, CDB_Object>::operator double(void) const
{
    return Convert_CDB_ObjectSql<double>(m_Value);
}

CValueConvert<SRunTimeSqlCP, CDB_Object>::operator string(void) const
{
    EDB_Type cur_type = m_Value.GetType();
    
    if (cur_type == eDB_SmallDateTime || cur_type == eDB_DateTime) {
        return Convert_CDB_ObjectSql_DT<string>(m_Value);
    }

    return Convert_CDB_ObjectSql<string>(m_Value);
}

CValueConvert<SRunTimeSqlCP, CDB_Object>::operator const CTime&(void) const
{
    CheckNULL(m_Value);
    CheckType(m_Value, eDB_SmallDateTime, eDB_DateTime);

    EDB_Type cur_type = m_Value.GetType();
    
    if (cur_type == eDB_SmallDateTime) {
        return static_cast<const CDB_SmallDateTime&>(m_Value).Value();
    } else if (cur_type == eDB_DateTime) {
        return static_cast<const CDB_DateTime&>(m_Value).Value();
    } else {
        ReportTypeConvError(cur_type, "CTime");
    }

    static CSafeStatic<CTime> value;
    return value.Get();
}

} // namespace value_slice

END_NCBI_SCOPE 

