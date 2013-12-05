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
 * Author:  Vladimir Soussov
 *
 * File Description:  Type conversions
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbitime.hpp>

#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/dbapi_object_convert.hpp>
#include <dbapi/driver/util/numeric_convert.hpp>
#include "memory_store.hpp"

#include <dbapi/driver/types.hpp>
#include <dbapi/error_codes.hpp>

#include <string.h>


#define NCBI_USE_ERRCODE_X   Dbapi_DrvrTypes


BEGIN_NCBI_SCOPE


inline
void s_MakeLittleEndian(TStringUCS2& s)
{
#ifdef WORDS_BIGENDIAN
    NON_CONST_ITERATE (TStringUCS2, it, s) {
        *it = (*it << 8) | (*it >> 8);
    }
#endif
}


/////////////////////////////////////////////////////////////////////////////
template<>
CGenericSqlString<char>::operator const char*(void) const
{
    return c_str();
}

#ifdef HAVE_WSTRING
template<>
CGenericSqlString<wchar_t>::operator const wchar_t*(void) const
{
    return c_str();
}
#endif


/////////////////////////////////////////////////////////////////////////////
CWString::CWString(void) :
    m_AvailableValueType(0),
    m_StringEncoding(eEncoding_Unknown),
    m_Char(NULL)
#ifdef HAVE_WSTRING
    , m_WChar(NULL)
#endif
{
}


CWString::CWString(const CWString& str) :
    m_AvailableValueType(str.m_AvailableValueType),
    m_StringEncoding(str.m_StringEncoding),
    m_Char(NULL),
#ifdef HAVE_WSTRING
    m_WChar(NULL),
#endif
    m_String(str.m_String),
#ifdef HAVE_WSTRING
    m_WString(str.m_WString),
#endif
    m_UTF8String(str.m_UTF8String)
{
    // Assign m_Char even if m_String is empty ...
    m_Char = m_String.c_str();

#ifdef HAVE_WSTRING
    // Assign m_WChar even if m_WString is empty ...
    m_WChar = m_WString.c_str();
#endif
}


CWString::CWString(const char* str,
                   string::size_type size,
                   EEncoding enc) :
    m_AvailableValueType(eChar),
    m_StringEncoding(enc),
    m_Char(NULL)
#ifdef HAVE_WSTRING
    , m_WChar(NULL)
#endif
{
    if (size == string::npos) {
        m_Char = str;
    } else {
        if (str) {
            m_String.assign(str, size);
            m_Char = m_String.c_str();
        }
        m_AvailableValueType |= eString;
    }
}


#ifdef HAVE_WSTRING
CWString::CWString(const wchar_t* str,
                   wstring::size_type size) :
    m_AvailableValueType(eWChar),
    m_StringEncoding(eEncoding_Unknown),
    m_Char(NULL),
    m_WChar(NULL)
{
    if (size == wstring::npos) {
        m_WChar = str;
    } else {
        if (str) {
            m_WString.assign(str, size);
            m_WChar = m_WString.c_str();
        }
        m_AvailableValueType |= eWString;
    }
}
#endif


CWString::CWString(const string& str, EEncoding enc) :
    m_AvailableValueType(eString),
    m_StringEncoding(enc),
    m_Char(NULL),
#ifdef HAVE_WSTRING
    m_WChar(NULL),
#endif
    m_String(str)
{
    m_Char = m_String.c_str();
    m_AvailableValueType |= eChar;
}


#ifdef HAVE_WSTRING
CWString::CWString(const wstring& str) :
    m_AvailableValueType(eWString),
    m_Char(NULL),
    m_WChar(NULL),
    m_WString(str)
{
    m_WChar = m_WString.c_str();
    m_AvailableValueType |= eWChar;
}
#endif


CWString::~CWString(void)
{
}


CWString& CWString::operator=(const CWString& str)
{
    if (&str != this) {
        m_AvailableValueType = str.m_AvailableValueType;
        m_StringEncoding = str.m_StringEncoding;
        m_Char = NULL;
        m_String = str.m_String;
        m_UTF8String = str.m_UTF8String;
        m_Char = m_String.c_str();

#ifdef HAVE_WSTRING
        m_WChar = NULL;
        m_WString = str.m_WString;
        m_WChar = m_WString.c_str();
#endif
    }

    return *this;
}

// Deprecated conversions, defined out of line to reduce warnings
CWString::operator char*(void) const
{
    return const_cast<char*>(AsCString());
}

CWString::operator const char*(void) const
{
    return AsCString();
}

CWString::operator wchar_t*(void) const
{
    return const_cast<wchar_t*>(AsCWString());
}

CWString::operator const wchar_t*(void) const
{
    return AsCWString();
}


void CWString::x_CalculateEncoding(EEncoding str_enc) const
{
    if (str_enc != eEncoding_Unknown) {
        m_StringEncoding = str_enc;
    } else {
        m_StringEncoding = eEncoding_ISO8859_1;
    }
}

void CWString::x_UTF8ToString(EEncoding str_enc) const
{
    if (m_StringEncoding == eEncoding_Unknown) {
        x_CalculateEncoding(str_enc);
    }

    if (m_StringEncoding == eEncoding_UTF8) {
        m_String = m_UTF8String;
    } else {
        m_String = CUtf8::AsSingleByteString(m_UTF8String,m_StringEncoding);
    }

    m_AvailableValueType |= eString;
}


void CWString::x_StringToUTF8(EEncoding str_enc) const
{
    if (m_StringEncoding == eEncoding_Unknown) {
        x_CalculateEncoding(str_enc);
    }

    if (m_AvailableValueType & eString) {
        m_UTF8String = CUtf8::AsUTF8(m_String, m_StringEncoding);
    } else if (m_AvailableValueType & eChar) {
        if (m_Char) {
            m_UTF8String = CUtf8::AsUTF8(m_Char, m_StringEncoding);
        } else {
            m_UTF8String.erase();
        }
    }

    m_AvailableValueType |= eUTF8String;
}


void CWString::x_MakeString(EEncoding str_enc) const
{
    if (m_AvailableValueType & eString) {
        if (!(m_AvailableValueType & eChar)) {
            if (m_String.empty()) {
                m_Char = NULL;
            } else {
                m_Char = m_String.c_str();
            }
            m_AvailableValueType |= eChar;
        }
    } else if (m_AvailableValueType & eChar) {
        if (m_Char) {
            m_String.assign(m_Char);
        } else {
            m_String.erase();
        }
        m_AvailableValueType |= eString;
    } else if (m_AvailableValueType & eUTF8String) {
        x_UTF8ToString(str_enc);
        x_MakeString(str_enc);
#ifdef HAVE_WSTRING
    } else if (m_AvailableValueType & eWString) {
        m_UTF8String = CUtf8::AsUTF8(m_WString);
        m_AvailableValueType |= eUTF8String;
        x_UTF8ToString(str_enc);
        x_MakeString(str_enc);
    } else if (m_AvailableValueType & eWChar) {
        if (m_WChar) {
            m_UTF8String = CUtf8::AsUTF8(m_WChar);
            m_AvailableValueType |= eUTF8String;
            x_UTF8ToString(str_enc);
        } else {
            m_String.erase();
            m_AvailableValueType |= eString;
        }
        x_MakeString(str_enc);
#endif
    }
}

#ifdef HAVE_WSTRING
void CWString::x_MakeWString(EEncoding str_enc) const
{
    if (m_AvailableValueType & eWString) {
        if (!(m_AvailableValueType & eWChar)) {
            if (m_WString.empty()) {
                m_WChar = NULL;
            } else {
                m_WChar = m_WString.c_str();
            }
            m_AvailableValueType |= eWChar;
        }
    } else if (m_AvailableValueType & eWChar) {
        if (m_WChar) {
            m_WString.assign(m_WChar);
        } else {
            m_WString.erase();
        }
        m_AvailableValueType |= eWString;
    } else if (m_AvailableValueType & eUTF8String) {
        m_WString = CUtf8::AsBasicString<wchar_t>(m_UTF8String);
        m_AvailableValueType |= eWString;
        x_MakeWString(str_enc);
    } else if (m_AvailableValueType & eString) {
        x_StringToUTF8(str_enc);
        x_MakeWString(str_enc);
    } else if (m_AvailableValueType & eChar) {
        if (m_Char) {
            x_StringToUTF8(str_enc);
            x_MakeWString(str_enc);
        } else {
            m_WString.erase();
            m_AvailableValueType |= eWString;
        }
    }
}
#endif

void CWString::x_MakeUTF8String(EEncoding str_enc) const
{
    if (m_AvailableValueType & eUTF8String) {
        return;
    } else if (m_AvailableValueType & eString) {
        x_StringToUTF8(str_enc);
    } else if (m_AvailableValueType & eChar) {
        x_StringToUTF8(str_enc);
#ifdef HAVE_WSTRING
    } else if (m_AvailableValueType & eWString) {
        m_UTF8String = CUtf8::AsUTF8(m_WString);
        m_AvailableValueType |= eUTF8String;
    } else if (m_AvailableValueType & eWChar) {
        if (m_WChar) {
            m_UTF8String = CUtf8::AsUTF8(m_WChar);
        } else {
            m_UTF8String.erase();
        }
        m_AvailableValueType |= eUTF8String;
#endif
    }
}

void CWString::x_MakeUCS2LEString(EEncoding str_enc) const
{
    if (m_AvailableValueType & eUCS2LEString) {
        return;
    } else {
        x_MakeUTF8String(str_enc);
        m_UCS2LEString = CUtf8::AsBasicString<TCharUCS2>(m_UTF8String);
        s_MakeLittleEndian(m_UCS2LEString);
        m_AvailableValueType |= eUCS2LEString;
    }
}

size_t CWString::GetSymbolNum(void) const
{
    size_t num = 0;

    if (m_AvailableValueType & eString) {
        num = m_String.size();
#ifdef HAVE_WSTRING
    } else if (m_AvailableValueType & eWString) {
        num = m_WString.size();
#endif
    } else if (m_AvailableValueType & eChar) {
        if (m_Char) {
            num = strlen(m_Char);
        }
#ifdef HAVE_WSTRING
    } else if (m_AvailableValueType & eWChar) {
        if (m_WChar) {
            // ??? Should be a better solution ...
            x_MakeWString();
            num = m_WString.size();
        }
#endif
    } else if (m_AvailableValueType & eUTF8String) {
        num = CUtf8::GetSymbolCount(m_UTF8String);
    }

    return num;
}

void CWString::Clear(void)
{
    m_AvailableValueType = 0;
    m_StringEncoding = eEncoding_Unknown;
    m_Char = NULL;
    m_String.erase();
#ifdef HAVE_WSTRING
    m_WChar = NULL;
    m_WString.erase();
#endif
    m_UTF8String.erase();
}

void CWString::Assign(const char* str,
                      string::size_type size,
                      EEncoding enc)
{
#ifdef HAVE_WSTRING
    m_WChar = NULL;
    m_WString.erase();
#endif
    m_UTF8String.erase();

    m_StringEncoding = enc;
    if (size == string::npos) {
        m_String.erase();
        m_Char = str;
        m_AvailableValueType = eChar;
    } else {
        if (str) {
            m_String.assign(str, size);
            m_Char = m_String.c_str();
        } else {
            m_Char = NULL;
            m_String.erase();
        }
        m_AvailableValueType = eChar | eString;
    }
}

#ifdef HAVE_WSTRING
void CWString::Assign(const wchar_t* str,
                      wstring::size_type size)
{
    m_StringEncoding = eEncoding_Unknown;
    m_Char = NULL;
    m_String.erase();
    m_UTF8String.erase();

    if (size == wstring::npos) {
        m_WString.erase();
        m_WChar = str;
        m_AvailableValueType = eWChar;
    } else {
        if (str) {
            m_WString.assign(str, size);
            m_WChar = m_WString.c_str();
        } else {
            m_WChar = NULL;
            m_WString.erase();
        }
        m_AvailableValueType = eWChar | eWString;
    }
}
#endif

void CWString::Assign(const string& str,
                      EEncoding enc)
{
#ifdef HAVE_WSTRING
    m_WChar = NULL;
    m_WString.erase();
#endif
    m_UTF8String.erase();

    m_StringEncoding = enc;
    m_String = str;
    m_Char = m_String.c_str();
    m_AvailableValueType = eChar | eString;
}

#ifdef HAVE_WSTRING
void CWString::Assign(const wstring& str)
{
    m_StringEncoding = eEncoding_Unknown;
    m_Char = NULL;
    m_String.erase();
    m_UTF8String.erase();

    m_WString = str;
    m_WChar = m_WString.c_str();
    m_AvailableValueType = eWChar | eWString;
}
#endif


////////////////////////////////////////////////////////////////////////////////
static
void CheckStringTruncation(size_t cur_len, size_t max_len)
{
    if (cur_len > max_len) {
        ERR_POST_X(1, Warning << "String of size " << cur_len <<
                      " was truncated to " << max_len << " character(s)");
    }
}

////////////////////////////////////////////////////////////////////////////////
static
void CheckBinaryTruncation(size_t cur_len, size_t max_len)
{
    if (cur_len > max_len) {
        ERR_POST_X(2, Warning << "Binary data of size " << cur_len <<
                      " was truncated to " << max_len << " byte(s)");
    }
}

////////////////////////////////////////////////////////////////////////////////
//  CDB_Object::
//

CDB_Object::CDB_Object(bool is_null) : 
    m_Null(is_null)
{ 
    return; 
}

CDB_Object::~CDB_Object()
{
    return;
}

void CDB_Object::AssignNULL()
{
    SetNULL();
}


CDB_Object* CDB_Object::Create(EDB_Type type, size_t size)
{
    switch ( type ) {
    case eDB_Int             : return new CDB_Int           ();
    case eDB_SmallInt        : return new CDB_SmallInt      ();
    case eDB_TinyInt         : return new CDB_TinyInt       ();
    case eDB_BigInt          : return new CDB_BigInt        ();
    case eDB_VarChar         : return new CDB_VarChar       ();
    case eDB_Char            : return new CDB_Char      (size);
    case eDB_VarBinary       : return new CDB_VarBinary     ();
    case eDB_Binary          : return new CDB_Binary    (size);
    case eDB_Float           : return new CDB_Float         ();
    case eDB_Double          : return new CDB_Double        ();
    case eDB_DateTime        : return new CDB_DateTime      ();
    case eDB_SmallDateTime   : return new CDB_SmallDateTime ();
    case eDB_Text            : return new CDB_Text          ();
    case eDB_Image           : return new CDB_Image         ();
    case eDB_Bit             : return new CDB_Bit           ();
    case eDB_Numeric         : return new CDB_Numeric       ();
    case eDB_LongBinary      : return new CDB_LongBinary(size);
    case eDB_LongChar        : return new CDB_LongChar  (size);
    case eDB_UnsupportedType : break;
    }
    DATABASE_DRIVER_ERROR("unknown type " + NStr::IntToString(type), 2);
}


const char* CDB_Object::GetTypeName(EDB_Type db_type, bool throw_on_unknown)
{
    switch ( db_type ) {
    case eDB_Int             : return "DB_Int";
    case eDB_SmallInt        : return "DB_SmallInt";
    case eDB_TinyInt         : return "DB_TinyInt";
    case eDB_BigInt          : return "DB_BigInt";
    case eDB_VarChar         : return "DB_VarChar";
    case eDB_Char            : return "DB_Char";
    case eDB_VarBinary       : return "DB_VarBinary";
    case eDB_Binary          : return "DB_Binary";
    case eDB_Float           : return "DB_Float";
    case eDB_Double          : return "DB_Double";
    case eDB_DateTime        : return "DB_DateTime";
    case eDB_SmallDateTime   : return "DB_SmallDateTime";
    case eDB_Text            : return "DB_Text";
    case eDB_Image           : return "DB_Image";
    case eDB_Bit             : return "DB_Bit";
    case eDB_Numeric         : return "DB_Numeric";
    case eDB_LongBinary      : return "DB_LongBinary";
    case eDB_LongChar        : return "DB_LongChar";
    case eDB_UnsupportedType : return "DB_UnsupportedType";
    }

    if (throw_on_unknown) {
        DATABASE_DRIVER_ERROR("unknown type " + NStr::IntToString(db_type), 2);
    }

    return "DB_???";
}


NCBI_PARAM_DEF(unsigned int, dbapi, max_logged_param_length, 255);


string CDB_Object::GetLogString(void) const
{
    if (IsNULL()) {
        return "NULL";
    }

    unsigned int max_length = TDbapi_MaxLoggedParamLength::GetDefault();
    string       result;

    switch (GetType()) {
    case eDB_Image:
    case eDB_Text:
    {
        const CDB_Stream& s = static_cast<const CDB_Stream&>(*this);
        AutoArray<char> buff(max_length + 1);
        result.assign(buff.get(), s.Peek(buff.get(), max_length + 1));
        break;
    }
    default:
        try {
            result = (string)ConvertSQL(*this);
        } catch (exception& e) {
            ERR_POST(Warning << "Exception converting from "
                     << GetTypeName(GetType()) << ": " << e.what());
            return "???";
        }
        break;
    }

    if (result.length() > max_length) {
        result.resize(max_length);
        if (max_length > 3) { 
            result[max_length - 1] = result[max_length - 2] =
                result[max_length - 3] = '.';
        }
    }
    // XXX - retrim?
    return NStr::PrintableString
        (result, NStr::fNewLine_Quote | NStr::fNonAscii_Quote);
}


/////////////////////////////////////////////////////////////////////////////
//  CDB_Int::
//

CDB_Int::CDB_Int() : 
    CDB_Object(true),
    m_Val(0)
{ 
    return; 
}

CDB_Int::CDB_Int(const Int4& i) : 
    CDB_Object(false), 
    m_Val(i)
{ 
    return; 
}

CDB_Int::~CDB_Int()
{ 
    return; 
}

EDB_Type CDB_Int::GetType() const
{
    return eDB_Int;
}

CDB_Object* CDB_Int::Clone() const
{
    return IsNULL() ? new CDB_Int : new CDB_Int(m_Val);
}

void CDB_Int::AssignValue(const CDB_Object& v)
{
    switch( v.GetType() ) {
        case eDB_Int     : *this = static_cast<const CDB_Int&>(v); break;
        case eDB_SmallInt: *this = static_cast<const CDB_SmallInt&>(v).Value(); break;
        case eDB_TinyInt : *this = static_cast<const CDB_TinyInt &>(v).Value(); break;
        default:
            DATABASE_DRIVER_ERROR( "wrong type of CDB_Object", 2 );
    }

}


/////////////////////////////////////////////////////////////////////////////
//  CDB_SmallInt::
//

CDB_SmallInt::CDB_SmallInt() : 
    CDB_Object(true),
    m_Val(0)
{ 
    return; 
}

CDB_SmallInt::CDB_SmallInt(const Int2& i) : 
    CDB_Object(false), 
    m_Val(i)
{ 
    return; 
}

CDB_SmallInt::~CDB_SmallInt()
{ 
    return; 
}

EDB_Type CDB_SmallInt::GetType() const
{
    return eDB_SmallInt;
}

CDB_Object* CDB_SmallInt::Clone() const
{
    return IsNULL() ? new CDB_SmallInt : new CDB_SmallInt(m_Val);
}

void CDB_SmallInt::AssignValue(const CDB_Object& v)
{
    switch( v.GetType() ) {
        case eDB_SmallInt: *this= (const CDB_SmallInt&)v; break;
        case eDB_TinyInt : *this= ((const CDB_TinyInt &)v).Value(); break;
        default:
            DATABASE_DRIVER_ERROR(string("wrong type of CDB_Object: ")
                                  + GetTypeName(v.GetType(), false),
                                  2);
    }
    *this= (CDB_SmallInt&)v;
}

/////////////////////////////////////////////////////////////////////////////
//  CDB_TinyInt::
//

CDB_TinyInt::CDB_TinyInt() : 
    CDB_Object(true),
    m_Val(0)
{ 
    return; 
}

CDB_TinyInt::CDB_TinyInt(const Uint1& i) : 
    CDB_Object(false), 
    m_Val(i)
{ 
    return; 
}

CDB_TinyInt::~CDB_TinyInt()
{ 
    return; 
}

EDB_Type CDB_TinyInt::GetType() const
{
    return eDB_TinyInt;
}

CDB_Object* CDB_TinyInt::Clone() const
{
    return IsNULL() ? new CDB_TinyInt : new CDB_TinyInt(m_Val);
}

void CDB_TinyInt::AssignValue(const CDB_Object& v)
{
    CHECK_DRIVER_ERROR(v.GetType() != eDB_TinyInt,
                       string("wrong type of CDB_Object: ")
                       + GetTypeName(v.GetType(), false),
                       2);

    *this= (const CDB_TinyInt&)v;
}


/////////////////////////////////////////////////////////////////////////////
//  CDB_BigInt::
//

CDB_BigInt::CDB_BigInt() : 
    CDB_Object(true),
    m_Val(0)  
{ 
    return; 
}

CDB_BigInt::CDB_BigInt(const Int8& i) : 
    CDB_Object(false), 
    m_Val(i)  
{ 
    return; 
}

CDB_BigInt::~CDB_BigInt()
{ 
    return; 
}

EDB_Type CDB_BigInt::GetType() const
{
    return eDB_BigInt;
}

CDB_Object* CDB_BigInt::Clone() const
{
    return IsNULL() ? new CDB_BigInt : new CDB_BigInt(m_Val);
}

void CDB_BigInt::AssignValue(const CDB_Object& v)
{
    switch( v.GetType() ) {
        case eDB_BigInt  : *this= (const CDB_BigInt&)v; break;
        case eDB_Int     : *this= ((const CDB_Int     &)v).Value(); break;
        case eDB_SmallInt: *this= ((const CDB_SmallInt&)v).Value(); break;
        case eDB_TinyInt : *this= ((const CDB_TinyInt &)v).Value(); break;
        default:
            DATABASE_DRIVER_ERROR(string("wrong type of CDB_Object: ")
                                  + GetTypeName(v.GetType(), false),
                                  2);
    }
}


template <typename TChar>
inline size_t my_strnlen(const TChar* str, size_t maxlen)
{
    size_t len = 0;
    while (len < maxlen  &&  *str != 0) {
        ++len;
        ++str;
    }
    return len;
}


/////////////////////////////////////////////////////////////////////////////
inline
string MakeString(const string& s, string::size_type size)
{
    string value(s, 0, size);

    if (size != string::npos) {
        value.resize(size, ' ');
    }

    return value;
}

inline
string MakeString(const TStringUCS2& s, TStringUCS2::size_type size)
{
    TStringUCS2 value(s, 0, size);

    if (size != TStringUCS2::npos) {
        value.resize(size, TCharUCS2(' '));
    }

    s_MakeLittleEndian(value);
    return string((const char*)value.data(), value.size() * sizeof(TCharUCS2));
}

template <typename TChar>
inline
string MakeString(const TChar* s, typename basic_string<TChar>::size_type size)
{
    typedef basic_string<TChar> TStr;
    if (s == NULL) {
        return MakeString(TStr(), size);
    }

    TStr str;
    if (size == TStr::npos)
        str.assign(s);
    else {
        size_t str_size = my_strnlen(s, size);
        str.assign(s, str_size);
    }
    return MakeString(str, size);
}

/////////////////////////////////////////////////////////////////////////////
//  CDB_String::
//

CDB_String::CDB_String(void) 
    : CDB_Object(true), m_BulkInsertionEnc(eBulkEnc_RawBytes)
{
}


CDB_String::CDB_String(const CDB_String& other)
    : CDB_Object(other), m_WString(other.m_WString),
      m_BulkInsertionEnc(other.m_BulkInsertionEnc)
{
}


CDB_String::CDB_String(const string& s, EEncoding enc)
    : CDB_Object(false), m_WString(s, enc),
      m_BulkInsertionEnc(eBulkEnc_RawBytes)
{
}


CDB_String::CDB_String(const char* s,
                       string::size_type size,
                       EEncoding enc)
    : CDB_Object(s == NULL), m_WString(MakeString(s, size), enc),
      m_BulkInsertionEnc(eBulkEnc_RawBytes)
{
}


CDB_String::CDB_String(const string& s,
                       string::size_type size,
                       EEncoding enc) 
    : CDB_Object(false), m_WString(MakeString(s, size), enc),
      m_BulkInsertionEnc(eBulkEnc_RawBytes)
{
}


CDB_String::CDB_String(const TStringUCS2& s, TStringUCS2::size_type size)
    : CDB_Object(false), m_WString(MakeString(s, size)),
      m_BulkInsertionEnc(eBulkEnc_RawUCS2)
{
}


/*
CDB_String::CDB_String(const TCharUCS2* s, string::size_type size)
    : CDB_Object(s == NULL), m_WString(MakeString(s, size)),
      m_BulkInsertionEnc(eBulkEnc_RawUCS2)
{
}
*/


CDB_String::~CDB_String(void)
{
}


CDB_String& CDB_String::operator= (const CDB_String& other)
{
    if (this != &other) {
        Assign(other);
    }

    return *this;
}


CDB_String& CDB_String::operator= (const string& s)
{
    Assign(s);
    return *this;
}


CDB_String& CDB_String::operator= (const char* s)
{
    Assign(s);
    return *this;
}


CDB_String& CDB_String::operator= (const TStringUCS2& s)
{
    Assign(s);
    return *this;
}


CDB_String::operator const char*(void) const
{
    return m_WString.AsCString();
}


#ifdef HAVE_WSTRING
const wchar_t* CDB_String::AsUnicode(EEncoding enc) const
{
    return IsNULL() ? NULL : m_WString.AsUnicode(enc).c_str();
}
#endif


void CDB_String::Assign(const CDB_String& other)
{
    SetNULL(other.IsNULL());
    m_WString = other.m_WString;
    m_BulkInsertionEnc = other.m_BulkInsertionEnc;
}


void CDB_String::Assign(const char* s,
                        string::size_type size,
                        EEncoding enc)
{
    if ( s ) {
        SetNULL(false);

        if (size == string::npos) {
            m_WString.Assign(string(s), enc);
        } else {
            m_WString.Assign(MakeString(s, size), enc);
        }
    } else {
        SetNULL();
    }
    if (m_BulkInsertionEnc == eBulkEnc_RawUCS2) {
        m_BulkInsertionEnc = eBulkEnc_UCS2FromChar;
    }
}


void CDB_String::Assign(const string& s,
                        string::size_type size,
                        EEncoding enc)
{
    SetNULL(false);
    m_WString.Assign(MakeString(s, size), enc);
    if (m_BulkInsertionEnc == eBulkEnc_RawUCS2) {
        m_BulkInsertionEnc = eBulkEnc_UCS2FromChar;
    }
}


/*
void CDB_String::Assign(const TCharUCS2* s,
                        TStringUCS2::size_type size)
{
    if ( s ) {
        SetNULL(false);
        m_WString.Assign(MakeString(s, size));
    } else {
        SetNULL();
    }
    m_BulkInsertionEnc = eBulkEnc_RawUCS2;
}
*/


void CDB_String::Assign(const TStringUCS2& s,
                        TStringUCS2::size_type size)
{
    SetNULL(false);
    m_WString.Assign(MakeString(s, size));
    m_BulkInsertionEnc = eBulkEnc_RawUCS2;
}


void CDB_String::GetBulkInsertionData(CTempString* ts, bool convert_raw_bytes)
    const
{
    _ASSERT(ts);

    if (IsNULL()) {
        ts->clear();
        return;
    }

    EBulkEnc enc = m_BulkInsertionEnc;
    if (convert_raw_bytes  &&  enc == eBulkEnc_RawBytes) {
        enc = eBulkEnc_UCS2FromChar;
    }

    switch (enc) {
    case eBulkEnc_RawBytes:
    case eBulkEnc_RawUCS2:
        *ts = x_GetWString();
        break;
    case eBulkEnc_UCS2FromChar:
    {
        const TStringUCS2& s2 = m_WString.AsUCS2_LE();
        ts->assign((char*) s2.data(), s2.size() * sizeof(TCharUCS2));
        break;
    }
    default:
        _TROUBLE;
    }
}

/////////////////////////////////////////////////////////////////////////////
template <typename TChar>
static
typename basic_string<TChar>::size_type 
get_string_size_varchar(const TChar* str,
                        typename basic_string<TChar>::size_type len)
{
    typedef basic_string<TChar> TStr;

    if (len == TStr::npos) {
        return len;
    } else if (str == NULL) {
        return 0;
    } else {
        return my_strnlen(str, len ? len : TStr::npos);
    }
}

/////////////////////////////////////////////////////////////////////////////
//  CDB_VarChar::
//

CDB_VarChar::CDB_VarChar(void)
{
}


CDB_VarChar::CDB_VarChar(const string& s,
                         EEncoding enc)
: CDB_String(s, enc)
{
}


CDB_VarChar::CDB_VarChar(const char* s,
                         EEncoding enc)
: CDB_String(s, string::npos, enc)
{
}


CDB_VarChar::CDB_VarChar(const char* s,
                         size_t l,
                         EEncoding enc)
: CDB_String(s, get_string_size_varchar(s, l), enc)
{
}


CDB_VarChar::CDB_VarChar(const TStringUCS2& s, size_t l)
    : CDB_String(s, get_string_size_varchar(s.data(), l))
{
}


/*
CDB_VarChar::CDB_VarChar(const TCharUCS2* s, size_t l)
    : CDB_String(s, get_string_size_varchar(s, l))
{
}
*/


CDB_VarChar::~CDB_VarChar(void)
{
}


CDB_VarChar& CDB_VarChar::SetValue(const string& s,
                                   EEncoding enc)
{
    Assign(s, string::npos, enc);

    return *this;
}


CDB_VarChar& CDB_VarChar::SetValue(const char* s,
                                   EEncoding enc)
{
    Assign(s, string::npos, enc);

    return *this;
}


CDB_VarChar& CDB_VarChar::SetValue(const char* s, size_t l,
                                   EEncoding enc)
{
    Assign(s, l, enc);

    return *this;
}


CDB_VarChar& CDB_VarChar::SetValue(const TStringUCS2& s)
{
    Assign(s, TStringUCS2::npos);

    return *this;
}


/*
CDB_VarChar& CDB_VarChar::SetValue(const TCharUCS2* s, size_t l)
{
    Assign(s, l);

    return *this;
}
*/


EDB_Type CDB_VarChar::GetType() const
{
    return eDB_VarChar;
}


CDB_Object* CDB_VarChar::Clone() const
{
    return IsNULL() ? new CDB_VarChar() : new CDB_VarChar(*this);
}


void CDB_VarChar::AssignValue(const CDB_Object& v)
{
    CHECK_DRIVER_ERROR(v.GetType() != eDB_VarChar,
                       string("wrong type of CDB_Object: ")
                       + GetTypeName(v.GetType(), false),
                       2);

    *this= (const CDB_VarChar&)v;
}


/////////////////////////////////////////////////////////////////////////////
//  CDB_Char::
//


CDB_Char::CDB_Char(size_t s)
: m_Size((s < 1) ? 1 : s)
{
}


CDB_Char::CDB_Char(size_t s,
                   const string& v,
                   EEncoding enc) :
    CDB_String(v, s, enc),
    m_Size(CDB_String::Size())
{
}


CDB_Char::CDB_Char(size_t s,
                   const char* str,
                   EEncoding enc) :
    CDB_String(str, s, enc),
    m_Size(CDB_String::Size())
{
}


CDB_Char::CDB_Char(size_t s, const TStringUCS2& v)
    : CDB_String(v, s), m_Size(CDB_String::Size())
{
}


CDB_Char::CDB_Char(const CDB_Char& v) :
    CDB_String(v),
    m_Size(v.m_Size)
{
}


CDB_Char& CDB_Char::operator= (const CDB_Char& v)
{
    if (this != &v) {
        m_Size = v.m_Size;
        Assign(v);
    }

    return *this;
}


CDB_Char& CDB_Char::operator= (const string& s)
{
    // Encoding of s ???
    CheckStringTruncation(s.size(), m_Size);
    Assign(s, m_Size);

    return *this;
}


CDB_Char& CDB_Char::operator= (const char* s)
{
    if (s) {
        // Encoding of s ???
        size_t len = strlen(s);
        CheckStringTruncation(len, m_Size);
        Assign(s, m_Size);
    }

    return *this;
}


CDB_Char& CDB_Char::operator= (const TStringUCS2& s)
{
    CheckStringTruncation(s.size(), m_Size);
    Assign(s, m_Size);

    return *this;
}


void CDB_Char::SetValue(const char* str, size_t len, EEncoding enc)
{
    CDB_VarChar vc_value(str, len, enc);
    CheckStringTruncation(vc_value.Size(), m_Size);

    Assign(vc_value.AsCString(), m_Size, enc);
}


void CDB_Char::SetValue(const TStringUCS2& v, size_t len)
{
    CDB_VarChar vc_value(v, len);
    CheckStringTruncation(vc_value.Size(), m_Size);

    Assign(vc_value.Data(), m_Size);
}


EDB_Type CDB_Char::GetType() const
{
    return eDB_Char;
}

CDB_Object* CDB_Char::Clone() const
{
    return new CDB_Char(*this);
}

void CDB_Char::AssignValue(const CDB_Object& v)
{
    CHECK_DRIVER_ERROR(v.GetType() != eDB_Char,
                       string("wrong type of CDB_Object: ")
                       + GetTypeName(v.GetType(), false),
                       2);

    const CDB_Char& cv = (const CDB_Char&)v;
    *this = cv;
}

CDB_Char::~CDB_Char()
{
}

/////////////////////////////////////////////////////////////////////////////
static
string::size_type 
get_string_size_longchar(const char* str, string::size_type len)
{
    if (len == string::npos) {
        return len;
    }

    if (str != NULL) {
        if (len == 0) {
            return strlen(str); // Similar to string::npos ...
        } else {
            return max(len, my_strnlen(str, len)); // This line is "min(len, str_len)" in case of varchar ...
        }
    }

    return 0;
}

/////////////////////////////////////////////////////////////////////////////
//  CDB_LongChar::
//


CDB_LongChar::CDB_LongChar(size_t s)
    : m_Size((s < 1) ? 1 : s)
{
}


CDB_LongChar::CDB_LongChar(size_t s,
                           const string& v,
                           EEncoding enc) :
    CDB_String(v, s, enc),
    m_Size(CDB_String::Size())
{
}


CDB_LongChar::CDB_LongChar(size_t len,
                           const char* str,
                           EEncoding enc) :
    CDB_String(str, get_string_size_longchar(str, len), enc),
    m_Size(CDB_String::Size())
{
}


CDB_LongChar::CDB_LongChar(size_t s,
                           const TStringUCS2& v)
    : CDB_String(v, s), m_Size(CDB_String::Size())
{
}


CDB_LongChar::CDB_LongChar(const CDB_LongChar& v) :
    CDB_String(v),
    m_Size(v.m_Size)
{
}


CDB_LongChar& CDB_LongChar::operator= (const CDB_LongChar& v)
{
    if (this != &v) {
        m_Size = v.m_Size;
        Assign(v);
    }

    return *this;
}


CDB_LongChar& CDB_LongChar::operator= (const string& s)
{
    // Encoding of s ???
    CheckStringTruncation(s.size(), m_Size);
    Assign(s, m_Size);

    return *this;
}


CDB_LongChar& CDB_LongChar::operator= (const char* s)
{
    if (s) {
        // Encoding of s ???
        size_t len = strlen(s);
        CheckStringTruncation(len, m_Size);
        Assign(s, m_Size);
    }

    return *this;
}


CDB_LongChar& CDB_LongChar::operator= (const TStringUCS2& s)
{
    CheckStringTruncation(s.size(), m_Size);
    Assign(s, m_Size);

    return *this;
}


void CDB_LongChar::SetValue(const char* str,
                            size_t len,
                            EEncoding enc)
{
    CheckStringTruncation(CDB_VarChar(str, len, enc).Size(), m_Size);

    Assign(str, m_Size, enc);
}


void CDB_LongChar::SetValue(const TStringUCS2& str, size_t len)
{
    CheckStringTruncation(CDB_VarChar(str, len).Size(), m_Size);

    Assign(str, m_Size);
}


EDB_Type CDB_LongChar::GetType() const
{
    return eDB_LongChar;
}


CDB_Object* CDB_LongChar::Clone() const
{
    return new CDB_LongChar(*this);
}


void CDB_LongChar::AssignValue(const CDB_Object& v)
{
    CHECK_DRIVER_ERROR(v.GetType() != eDB_LongChar,
                       string("wrong type of CDB_Object: ")
                       + GetTypeName(v.GetType(), false),
                       2);

    const CDB_LongChar& cv= (const CDB_LongChar&)v;
    *this = cv;
}


CDB_LongChar::~CDB_LongChar()
{
}


/////////////////////////////////////////////////////////////////////////////
//  CDB_VarBinary::
//


CDB_VarBinary::CDB_VarBinary(void) 
{ 
}

CDB_VarBinary::CDB_VarBinary(const void* v, size_t l) 
{ 
    SetValue(v, l); 
}

CDB_VarBinary::~CDB_VarBinary(void)
{ 
}

void CDB_VarBinary::SetValue(const void* v, size_t l)
{
    if (v  &&  l) {
        m_Value.assign(static_cast<const char*>(v), l);
        SetNULL(false);
    } else {
        SetNULL();
    }
}


CDB_VarBinary& CDB_VarBinary::operator= (const CDB_VarBinary& v)
{
    if (this != &v) {
        SetNULL(v.IsNULL());
        m_Value = v.m_Value;
    }

    return *this;
}


EDB_Type CDB_VarBinary::GetType() const
{
    return eDB_VarBinary;
}


CDB_Object* CDB_VarBinary::Clone() const
{
    return IsNULL() ? new CDB_VarBinary : new CDB_VarBinary(m_Value.data(), m_Value.size());
}


void CDB_VarBinary::AssignValue(const CDB_Object& v)
{
    CHECK_DRIVER_ERROR(v.GetType() != eDB_VarBinary,
                       string("wrong type of CDB_Object: ")
                       + GetTypeName(v.GetType(), false),
                       2);

    *this= (const CDB_VarBinary&)v;
}

/////////////////////////////////////////////////////////////////////////////
//  CDB_Binary::
//


CDB_Binary::CDB_Binary(size_t s)
{
    m_Size = (s < 1) ? 1 : s;
}


CDB_Binary::CDB_Binary(size_t s, const void* v, size_t v_size)
{
    m_Size = (s == 0) ? 1 : s;
    SetValue(v, v_size);
}


CDB_Binary::CDB_Binary(const CDB_Binary& v)
{
    SetNULL(v.IsNULL());
    m_Size = v.m_Size;
    m_Value = v.m_Value;
}


void CDB_Binary::SetValue(const void* v, size_t v_size)
{
    if (v && v_size) {
        CheckBinaryTruncation(v_size, m_Size);

        m_Value.assign(static_cast<const char*>(v), min(v_size, m_Size));
        m_Value.resize(m_Size, '\0');
        SetNULL(false);
    } else {
        SetNULL();
    }
}


CDB_Binary& CDB_Binary::operator= (const CDB_Binary& v)
{
    if (this != &v) {
        SetNULL(v.IsNULL());
        m_Size = v.m_Size;
        m_Value = v.m_Value;
    }

    return *this;
}


EDB_Type CDB_Binary::GetType() const
{
    return eDB_Binary;
}


CDB_Object* CDB_Binary::Clone() const
{
    return IsNULL() ? new CDB_Binary(m_Size) : new CDB_Binary(*this);
}


void CDB_Binary::AssignValue(const CDB_Object& v)
{
    CHECK_DRIVER_ERROR(v.GetType() != eDB_Binary,
                       string("wrong type of CDB_Object: ")
                       + GetTypeName(v.GetType(), false),
                       2);

    const CDB_Binary& cv = static_cast<const CDB_Binary&>(v);
    *this = cv;
}


CDB_Binary::~CDB_Binary()
{
}

/////////////////////////////////////////////////////////////////////////////
//  CDB_LongBinary::
//


CDB_LongBinary::CDB_LongBinary(size_t s)
: m_Size((s < 1) ? 1 : s)
, m_DataSize(0)
{
}


CDB_LongBinary::CDB_LongBinary(size_t s, const void* v, size_t v_size)
: m_Size(s)
{
    SetValue(v, v_size);
}


CDB_LongBinary::CDB_LongBinary(const CDB_LongBinary& v)
: m_Size(v.m_Size)
, m_DataSize(v.m_DataSize)
, m_Value(v.m_Value)
{
    SetNULL(v.IsNULL());
}


void CDB_LongBinary::SetValue(const void* v, size_t v_size)
{
    if (v && v_size) {
        m_DataSize = min(v_size, m_Size);
        CheckBinaryTruncation(v_size, m_Size);
        m_Value.assign(static_cast<const char*>(v), m_DataSize);
        m_Value.resize(m_Size, '\0');
        SetNULL(false);
    } else {
        SetNULL();
        m_DataSize = 0;
    }
}


CDB_LongBinary& CDB_LongBinary::operator= (const CDB_LongBinary& v)
{
    if (this != &v) {
        SetNULL(v.IsNULL());
        m_Size = v.m_Size;
        m_DataSize = v.m_DataSize;
        m_Value = v.m_Value;
    }

    return *this;
}


EDB_Type CDB_LongBinary::GetType() const
{
    return eDB_LongBinary;
}


CDB_Object* CDB_LongBinary::Clone() const
{
    return new CDB_LongBinary(*this);
}


void CDB_LongBinary::AssignValue(const CDB_Object& v)
{
    CHECK_DRIVER_ERROR(
        v.GetType() != eDB_LongBinary,
        string("wrong type of CDB_Object: ") + GetTypeName(v.GetType(), false),
        2 );

    const CDB_LongBinary& cv = static_cast<const CDB_LongBinary&>(v);
    *this = cv;
}


CDB_LongBinary::~CDB_LongBinary()
{
}


/////////////////////////////////////////////////////////////////////////////
//  CDB_Float::
//


CDB_Float::CDB_Float() : 
    CDB_Object(true),
    m_Val(0.0)  
{ 
    return; 
}

CDB_Float::CDB_Float(float i) : 
    CDB_Object(false), 
    m_Val(i)  
{ 
    return; 
}

CDB_Float::~CDB_Float(void)
{
}


CDB_Float& CDB_Float::operator= (const float& i)
{
    SetNULL(false);
    m_Val  = i;
    return *this;
}


EDB_Type CDB_Float::GetType() const
{
    return eDB_Float;
}

CDB_Object* CDB_Float::Clone() const
{
    return IsNULL() ? new CDB_Float : new CDB_Float(m_Val);
}

void CDB_Float::AssignValue(const CDB_Object& v)
{
    switch( v.GetType() ) {
        case eDB_Float   : *this = (const CDB_Float&)v; break;
        case eDB_SmallInt: *this = ((const CDB_SmallInt&)v).Value(); break;
        case eDB_TinyInt : *this = ((const CDB_TinyInt &)v).Value(); break;
        default:
            DATABASE_DRIVER_ERROR(string("wrong type of CDB_Object: ")
                                  + GetTypeName(v.GetType(), false),
                                  2);
    }
}

/////////////////////////////////////////////////////////////////////////////
//  CDB_Double::
//


CDB_Double::CDB_Double() : 
    CDB_Object(true),
    m_Val(0.0)  
{ 
    return; 
}

CDB_Double::CDB_Double(double i) : 
    CDB_Object(false), 
    m_Val(i)  
{ 
    return; 
}

CDB_Double::~CDB_Double(void)
{
}

CDB_Double& CDB_Double::operator= (const double& i)
{
    SetNULL(false);
    m_Val  = i;
    return *this;
}

EDB_Type CDB_Double::GetType() const
{
    return eDB_Double;
}

CDB_Object* CDB_Double::Clone() const
{
    return IsNULL() ? new CDB_Double : new CDB_Double(m_Val);
}

void CDB_Double::AssignValue(const CDB_Object& v)
{
    switch( v.GetType() ) {
        case eDB_Double  : *this = (const CDB_Double&)v; break;
        case eDB_Float   : *this = ((const CDB_Float   &)v).Value(); break;
        case eDB_Int     : *this = ((const CDB_Int     &)v).Value(); break;
        case eDB_SmallInt: *this = ((const CDB_SmallInt&)v).Value(); break;
        case eDB_TinyInt : *this = ((const CDB_TinyInt &)v).Value(); break;
        default:
            DATABASE_DRIVER_ERROR(string("wrong type of CDB_Object: ")
                                  + GetTypeName(v.GetType(), false),
                                  2);
    }
}

/////////////////////////////////////////////////////////////////////////////
//  CDB_Stream::
//

CDB_Stream::CDB_Stream()
    : CDB_Object(true)
{
    m_Store = new CMemStore;
}

CDB_Stream& CDB_Stream::Assign(const CDB_Stream& v)
{
    SetNULL(v.IsNULL());
    m_Store->Truncate();
    if ( !IsNULL() ) {
        char buff[1024];
        CMemStore* s = v.m_Store;
        size_t pos = s->Tell();
        for (size_t n = s->Read((void*) buff, sizeof(buff));
             n > 0;
             n = s->Read((void*) buff, sizeof(buff))) {
            Append((void*) buff, n);
        }
        s->Seek((long) pos, C_RA_Storage::eHead);
    }
    return *this;
}

void CDB_Stream::AssignNULL()
{
    CDB_Object::AssignNULL();
    Truncate();
}

size_t CDB_Stream::Read(void* buff, size_t nof_bytes)
{
    return m_Store->Read(buff, nof_bytes);
}

size_t CDB_Stream::Peek(void* buff, size_t nof_bytes) const
{
    return m_Store->Peek(buff, nof_bytes);
}

size_t CDB_Stream::Append(const void* buff, size_t nof_bytes)
{
    /* if (buff && (nof_bytes > 0)) */ SetNULL(false);
    return m_Store->Append(buff, nof_bytes);
}

bool CDB_Stream::MoveTo(size_t byte_number)
{
    return m_Store->Seek((long) byte_number, C_RA_Storage::eHead)
        == (long) byte_number;
}

size_t CDB_Stream::Size() const
{
    return m_Store->GetDataSize();
}

void CDB_Stream::Truncate(size_t nof_bytes)
{
    m_Store->Truncate(nof_bytes);
    SetNULL(m_Store->GetDataSize() <= 0);
}

void CDB_Stream::AssignValue(const CDB_Object& v)
{
    CHECK_DRIVER_ERROR(
        (v.GetType() != eDB_Image) && (v.GetType() != eDB_Text),
        string("wrong type of CDB_Object: ") + GetTypeName(v.GetType(), false),
        2
        );
    Assign(static_cast<const CDB_Stream&>(v));
}

CDB_Stream::~CDB_Stream()
{
    try {
        delete m_Store;
    }
    NCBI_CATCH_ALL_X( 7, NCBI_CURRENT_FUNCTION )
}


/////////////////////////////////////////////////////////////////////////////
//  CDB_Image::
//

CDB_Image::CDB_Image(void)
{
}

CDB_Image::~CDB_Image(void)
{
}

CDB_Image& CDB_Image::operator= (const CDB_Image& image)
{
    return dynamic_cast<CDB_Image&> (Assign(image));
}

EDB_Type CDB_Image::GetType() const
{
    return eDB_Image;
}

CDB_Object* CDB_Image::Clone() const
{
    CHECK_DRIVER_ERROR(
        !IsNULL(),
        "Clone for the non NULL image is not supported",
        1
        );

    return new CDB_Image;
}


/////////////////////////////////////////////////////////////////////////////
//  CDB_Text::
//

CDB_Text::CDB_Text(void)
    : m_Encoding(eBulkEnc_RawBytes)
{
}

CDB_Text::~CDB_Text(void)
{
}

void CDB_Text::SetEncoding(EBulkEnc e)
{
    if (e == eBulkEnc_UCS2FromChar) {
        e = eBulkEnc_RawUCS2;
    }
    if (e != m_Encoding  &&  Size() > 0) {
        // Alternatively, arrange to recode existing text?
        ERR_POST_X(8, "Creating a mixed-encoding CDB_Text object.");
    }
    m_Encoding = e;
}

size_t CDB_Text::Append(const void* buff, size_t nof_bytes)
{
    if (buff == NULL) {
        // return 0;
        buff = kEmptyCStr;
    }
    // Warn if nof_bytes == 0?
    return CDB_Stream::Append
        (buff, nof_bytes ? nof_bytes : strlen((const char*) buff));
}

size_t CDB_Text::Append(const char* buff)
{
    if (!buff) return 0;
    return CDB_Stream::Append(buff, strlen(buff));
}

size_t CDB_Text::Append(const string& s, EEncoding enc)
{
    switch (m_Encoding) {
    case eBulkEnc_RawBytes:
        return CDB_Stream::Append(s.data(), s.size());
    case eBulkEnc_RawUCS2:
    {
        TStringUCS2 s2 = CUtf8::AsBasicString<TCharUCS2>
            (CUtf8::AsUTF8(s, enc));
        s_MakeLittleEndian(s2);
        return CDB_Stream::Append(s2.data(), s2.size() * sizeof(TCharUCS2));
    }
    default:
        _TROUBLE;
        return 0;
    }
}


size_t CDB_Text::Append(const TStringUCS2& s)
{
    SetEncoding(eBulkEnc_RawUCS2);
#ifdef WORDS_BIGENDIAN
    TStringUCS2 s2(s);
    s_MakeLittleEndian(s2);
    return CDB_Stream::Append(s2.data(), s2.size() * sizeof(TCharUCS2));
#else
    return CDB_Stream::Append(s.data(), s.size() * sizeof(TCharUCS2));
#endif
}


CDB_Text& CDB_Text::operator= (const CDB_Text& text)
{
    m_Encoding = text.m_Encoding;
    return dynamic_cast<CDB_Text&> (Assign(text));
}

EDB_Type CDB_Text::GetType() const
{
    return eDB_Text;
}

CDB_Object* CDB_Text::Clone() const
{
    CHECK_DRIVER_ERROR(
        !IsNULL(),
        "Clone for the non-NULL text is not supported",
        1
        );

    return new CDB_Text;
}


/////////////////////////////////////////////////////////////////////////////
//  CDB_SmallDateTime::
//


CDB_SmallDateTime::CDB_SmallDateTime(CTime::EInitMode mode)
: m_NCBITime(mode)
, m_Status( 0x1 )
{
    m_DBTime.days = 0;
    m_DBTime.time = 0;
    SetNULL(mode == CTime::eEmpty);
}


CDB_SmallDateTime::CDB_SmallDateTime(const CTime& t)
: m_NCBITime( t )
, m_Status( 0x1 )
{
    m_DBTime.days = 0;
    m_DBTime.time = 0;
    SetNULL(t.IsEmpty());
}


CDB_SmallDateTime::CDB_SmallDateTime(Uint2 days, Uint2 minutes)
: m_Status( 0x2 )
{
    m_DBTime.days = days;
    m_DBTime.time = minutes;
    SetNULL(false);
}


CDB_SmallDateTime::~CDB_SmallDateTime(void)
{
}

CDB_SmallDateTime& CDB_SmallDateTime::Assign(Uint2 days, Uint2 minutes)
{
    m_DBTime.days = days;
    m_DBTime.time = minutes;
    m_Status      = 0x2;
    SetNULL(false);

    return *this;
}


CDB_SmallDateTime& CDB_SmallDateTime::operator= (const CTime& t)
{
    m_NCBITime = t;
    m_DBTime.days = 0;
    m_DBTime.time = 0;
    m_Status = 0x1;
    SetNULL(t.IsEmpty());

    return *this;
}


const CTime& CDB_SmallDateTime::Value(void) const
{
    if((m_Status & 0x1) == 0) {
        m_NCBITime.SetTimeDBU(m_DBTime);
        m_Status |= 0x1;
    }
    return m_NCBITime;
}


Uint2 CDB_SmallDateTime::GetDays(void) const
{
    if((m_Status & 0x2) == 0) {
        m_DBTime = m_NCBITime.GetTimeDBU();
        m_Status |= 0x2;
    }
    return m_DBTime.days;
}


Uint2 CDB_SmallDateTime::GetMinutes(void) const
{
    if((m_Status & 0x2) == 0) {
        m_DBTime = m_NCBITime.GetTimeDBU();
        m_Status |= 0x2;
    }
    return m_DBTime.time;
}


EDB_Type CDB_SmallDateTime::GetType() const
{
    return eDB_SmallDateTime;
}

CDB_Object* CDB_SmallDateTime::Clone() const
{
    return IsNULL() ? new CDB_SmallDateTime : new CDB_SmallDateTime(Value());
}

void CDB_SmallDateTime::AssignValue(const CDB_Object& v)
{
    CHECK_DRIVER_ERROR(
        v.GetType() != eDB_SmallDateTime,
        string("wrong type of CDB_Object: ") + GetTypeName(v.GetType(), false),
        2 );
    *this= (const CDB_SmallDateTime&)v;
}


/////////////////////////////////////////////////////////////////////////////
//  CDB_DateTime::
//


CDB_DateTime::CDB_DateTime(CTime::EInitMode mode)
: m_NCBITime(mode)
, m_Status(0x1)
{
    m_DBTime.days = 0;
    m_DBTime.time = 0;
    SetNULL(mode == CTime::eEmpty);
}


CDB_DateTime::CDB_DateTime(const CTime& t)
: m_NCBITime( t )
, m_Status( 0x1 )
{
    m_DBTime.days = 0;
    m_DBTime.time = 0;
    SetNULL(t.IsEmpty());
}


CDB_DateTime::CDB_DateTime(Int4 d, Int4 s300)
: m_Status( 0x2 )
{
    m_DBTime.days = d;
    m_DBTime.time = s300;
    SetNULL(false);
}


CDB_DateTime::~CDB_DateTime(void)
{
}


CDB_DateTime& CDB_DateTime::operator= (const CTime& t)
{
    m_NCBITime = t;
    m_DBTime.days = 0;
    m_DBTime.time = 0;
    m_Status = 0x1;
    SetNULL(t.IsEmpty());
    return *this;
}


CDB_DateTime& CDB_DateTime::Assign(Int4 d, Int4 s300)
{
    m_DBTime.days = d;
    m_DBTime.time = s300;
    m_Status = 0x2;
    SetNULL(false);
    return *this;
}


const CTime& CDB_DateTime::Value(void) const
{
    if((m_Status & 0x1) == 0) {
        m_NCBITime.SetTimeDBI(m_DBTime);
        m_Status |= 0x1;
    }
    return m_NCBITime;
}


Int4 CDB_DateTime::GetDays(void) const
{
    if((m_Status & 0x2) == 0) {
        m_DBTime = m_NCBITime.GetTimeDBI();
        m_Status |= 0x2;
    }
    return m_DBTime.days;
}


Int4 CDB_DateTime::Get300Secs(void) const
{
    if((m_Status & 0x2) == 0) {
        m_DBTime = m_NCBITime.GetTimeDBI();
        m_Status |= 0x2;
    }
    return m_DBTime.time;
}


EDB_Type CDB_DateTime::GetType() const
{
    return eDB_DateTime;
}

CDB_Object* CDB_DateTime::Clone() const
{
    return IsNULL() ? new CDB_DateTime : new CDB_DateTime(Value());
}

void CDB_DateTime::AssignValue(const CDB_Object& v)
{
    CHECK_DRIVER_ERROR(
        v.GetType() != eDB_DateTime,
        string("wrong type of CDB_Object: ") + GetTypeName(v.GetType(), false),
        2 );
    *this= (const CDB_DateTime&)v;
}

/////////////////////////////////////////////////////////////////////////////
//  CDB_Bit::
//


CDB_Bit::CDB_Bit() : 
    CDB_Object(true),
    m_Val(0)
{ 
    return; 
}

CDB_Bit::CDB_Bit(int i) : 
    CDB_Object(false)  
{ 
    m_Val = i ? 1 : 0; 
}

CDB_Bit::CDB_Bit(bool i) : 
    CDB_Object(false)  
{ 
    m_Val = i ? 1 : 0; 
}

CDB_Bit::~CDB_Bit(void)
{
}

CDB_Bit& CDB_Bit::operator= (const int& i)
{
    SetNULL(false);
    m_Val = i ? 1 : 0;
    return *this;
}


CDB_Bit& CDB_Bit::operator= (const bool& i)
{
    SetNULL(false);
    m_Val = i ? 1 : 0;
    return *this;
}


EDB_Type CDB_Bit::GetType() const
{
    return eDB_Bit;
}

CDB_Object* CDB_Bit::Clone() const
{
    return IsNULL() ? new CDB_Bit : new CDB_Bit(m_Val ? 1 : 0);
}

void CDB_Bit::AssignValue(const CDB_Object& v)
{
    CHECK_DRIVER_ERROR(
        v.GetType() != eDB_Bit,
        string("wrong type of CDB_Object: ") + GetTypeName(v.GetType(), false),
        2 );
    *this= (const CDB_Bit&)v;
}


/////////////////////////////////////////////////////////////////////////////
//
//  CDB_Numeric::
//


CDB_Numeric::CDB_Numeric() : 
    CDB_Object(true),
    m_Precision(0),
    m_Scale(0)
{
    memset(m_Body, 0, sizeof(m_Body));
}


CDB_Numeric::CDB_Numeric(unsigned int precision, unsigned int scale)
    : CDB_Object(false),
    m_Precision(precision),
    m_Scale(scale)
{
    memset(m_Body, 0, sizeof(m_Body));
}


CDB_Numeric::CDB_Numeric(unsigned int precision,
                         unsigned int scale,
                         const unsigned char* arr) : 
    CDB_Object(false),
    m_Precision(precision),
    m_Scale(scale)
{
    memcpy(m_Body, arr, sizeof(m_Body));
}


CDB_Numeric::CDB_Numeric(unsigned int precision,
                         unsigned int scale,
                         bool is_negative,
                         const unsigned char* arr) : 
    CDB_Object(false),
    m_Precision(precision),
    m_Scale(scale)
{
    m_Body[0]= is_negative? 1 : 0;
    memcpy(m_Body+1, arr, sizeof(m_Body)-1);
}


CDB_Numeric::CDB_Numeric(unsigned int precision, unsigned int scale, const char* val)
    : m_Precision(0), 
    m_Scale(0)
{
    x_MakeFromString(precision, scale, val);
}


CDB_Numeric::CDB_Numeric(unsigned int precision, unsigned int scale, const string& val)
    : m_Precision(0), 
    m_Scale(0)
{
    x_MakeFromString(precision, scale, val);
}


CDB_Numeric::~CDB_Numeric(void)
{
}


CDB_Numeric& CDB_Numeric::Assign(unsigned int precision,
                                 unsigned int scale,
                                 const unsigned char* arr)
{
    m_Precision = precision;
    m_Scale     = scale;
    SetNULL(false);
    memcpy(m_Body, arr, sizeof(m_Body));
    return *this;
}


CDB_Numeric& CDB_Numeric::Assign(unsigned int precision,
                                 unsigned int scale,
                                 bool is_negative,
                                 const unsigned char* arr)
{
    m_Precision = precision;
    m_Scale     = scale;
    SetNULL(false);
    m_Body[0] = is_negative? 1 : 0;
    memcpy(m_Body + 1, arr, sizeof(m_Body) - 1);
    return *this;
}


CDB_Numeric& CDB_Numeric::operator= (const char* val)
{
    x_MakeFromString(m_Precision, m_Scale, val);
    return *this;
}


CDB_Numeric& CDB_Numeric::operator= (const string& val)
{
    x_MakeFromString(m_Precision, m_Scale, val);
    return *this;
}


EDB_Type CDB_Numeric::GetType() const
{
    return eDB_Numeric;
}


CDB_Object* CDB_Numeric::Clone() const
{
    return new CDB_Numeric((unsigned int) m_Precision,
                           (unsigned int) m_Scale, m_Body);
}


static int s_NumericBytesPerPrec[] =
{
    2, 2, 3, 3, 4, 4, 4, 5, 5, 6, 6, 6, 7, 7, 8, 8, 9, 9, 9,
    10, 10, 11, 11, 11, 12, 12, 13, 13, 14, 14, 14, 15, 15,
    16, 16, 16, 17, 17, 18, 18, 19, 19, 19, 20, 20, 21, 21, 21,
    22, 22, 23, 23, 24, 24, 24, 25, 25, 26, 26, 26
};


static const unsigned int kMaxPrecision = 50;


static void s_DoCarry(unsigned char* product)
{
    for (unsigned int j = 0;  j < kMaxPrecision;  j++) {
        if (product[j] > 9) {
            product[j + 1] += product[j] / 10;
            product[j]     %= 10;
        }
    }
}


static void s_MultiplyByte(unsigned char* product, int num,
                           const unsigned char* multiplier)
{
    unsigned char number[3];
    number[0] =  num        % 10;
    number[1] = (num /  10) % 10;
    number[2] = (num / 100) % 10;

    int top;
    for (top = kMaxPrecision - 1;  top >= 0  &&  !multiplier[top];  top--)
        continue;

    int start = 0;
    for (int i = 0;  i <= top;  i++) {
        for (int j =0;  j < 3;  j++) {
            product[j + start] += multiplier[i] * number[j];
        }
        s_DoCarry(product);
        start++;
    }
}


static char* s_ArrayToString(const unsigned char* array, int scale, char* s)
{
    int top;

    for (top = kMaxPrecision - 1;  top >= 0  &&  top > scale  &&  !array[top];
         top--)
        continue;

    if (top == -1) {
        s[0] = '0';
        s[1] = '\0';
        return s;
    }

    int j = 0;
    for (int i = top;  i >= 0;  i--) {
        if (top + 1 - j == scale)
            s[j++] = '.';
        s[j++] = array[i] + '0';
    }
    s[j] = '\0';

    return s;
}


string CDB_Numeric::Value() const
{
    unsigned char multiplier[kMaxPrecision];
    unsigned char temp[kMaxPrecision];
    unsigned char product[kMaxPrecision];
    char result[kMaxPrecision + 1];
    char* s = result;
    int num_bytes = 0;

    memset(multiplier, 0, kMaxPrecision);
    memset(product,    0, kMaxPrecision);
    multiplier[0] = 1;
    if (m_Precision != 0) {
        num_bytes = s_NumericBytesPerPrec[m_Precision-1];
    }

    if (m_Body[0] == 1) {
        *s++ = '-';
    }

    for (int pos = num_bytes - 1;  pos > 0;  pos--) {
        s_MultiplyByte(product, m_Body[pos], multiplier);

        memcpy(temp, multiplier, kMaxPrecision);
        memset(multiplier, 0, kMaxPrecision);
        s_MultiplyByte(multiplier, 256, temp);
    }

    s_ArrayToString(product, m_Scale, s);
    return result;
}



static int s_Div256(const char* value, char* product, int base)
{
    int res = 0;
    char* const initial = product;

    while (*value < base) {
        res = res % 256 * base + (int)*value;
        ++value;
        while (product == initial && *value < base && res < 256) {
            res = res * base + (int)*value;
            ++value;
        }
        *product = (char) (res / 256);
        ++product;
    }
    *product = base;
    return res % 256;
}


void CDB_Numeric::x_MakeFromString(unsigned int precision, unsigned int scale,
                                   const CTempString& s)
{

    if (m_Precision == 0  &&  precision == 0  &&  !s.empty()) {
        precision = (unsigned int) s.size();
        if (scale == 0) {
            SIZE_TYPE dot_pos = s.find('.');
            scale = (dot_pos == NPOS) ? 0 : precision - dot_pos;
            if (scale > 1)
                --scale;
        }
    }

    CHECK_DRIVER_ERROR(
        !precision  ||  precision > kMaxPrecision,
        "illegal precision " + NStr::NumericToString(precision),
        100 );
    CHECK_DRIVER_ERROR(
        scale > precision,
        "scale (" + NStr::NumericToString(scale)
        + ") cannot be more than precision ("
        + NStr::NumericToString(precision) + ')',
        101 );

    bool is_negative= false;
    CTempString::const_iterator val = s.begin();
    if (val != s.end()  &&  *val == '-') {
        is_negative= true;
        ++val;
    }

    while (val != s.end()  &&  *val == '0') {
        ++val;
    }

    char buff1[kMaxPrecision + 1];
    unsigned int n = 0;
    while (val != s.end()  &&  n < precision) {
        if (*val >= '0'  &&  *val <= '9') {
            buff1[n++] = *val - '0';
        } else if (*val == '.') {
            break;
        } else {
            DATABASE_DRIVER_ERROR("string cannot be converted: " + string(s),
                                  102 );
        }
        ++val;
    }

    CHECK_DRIVER_ERROR(
        precision - n < scale,
        "string cannot be converted because of overflow: " + string(s),
        103 );

    unsigned int dec = 0;
    if (val != s.end()  &&  *val == '.') {
        ++val;
        while (val != s.end()  &&  dec < scale) {
            if (*val >= '0'  &&  *val <= '9') {
                buff1[n++] = *val - '0';
            } else {
                DATABASE_DRIVER_ERROR
                    ("string cannot be converted: " + string(s), 102);
            }
            ++dec;
            ++val;
        }
    }

    while (dec++ < scale) {
        buff1[n++] = 0;
    }
    if (n == 0) {
        buff1[n++] = 0;
    }
    buff1[n] = 10;

    char  buff2[kMaxPrecision + 1];
    char* p[2];
    p[0] = buff1;
    p[1] = buff2;

    // Setup everything now
    memset(m_Body, 0, sizeof(m_Body));
    if (is_negative) {
        m_Body[0] = 1/*sign*/;
    }
    unsigned char* num = m_Body + s_NumericBytesPerPrec[precision - 1] - 1;
    for (int i = 0;  *p[i];  i = 1 - i) {
        *num = s_Div256(p[i], p[1-i], 10);
        --num;
    }

    m_Precision = precision;
    m_Scale     = scale;
    SetNULL(false);
}

void CDB_Numeric::AssignValue(const CDB_Object& v)
{
    CHECK_DRIVER_ERROR(
        v.GetType() != eDB_Numeric,
        string("wrong type of CDB_Object: ") + GetTypeName(v.GetType(), false),
        2 );
    *this = (const CDB_Numeric&)v;
}

END_NCBI_SCOPE


