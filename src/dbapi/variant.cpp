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
* File Name:  $Id$
*
* Author:  Michael Kholodov
*
* File Description:   CVariant class implementation
*
*
*
*/

#include <ncbi_pch.hpp>
#include <dbapi/variant.hpp>
#include <dbapi/error_codes.hpp>
#include <algorithm>
//#include <corelib/ncbistr.hpp>


#define NCBI_USE_ERRCODE_X   Dbapi_Variant


BEGIN_NCBI_SCOPE

//==================================================================
CVariantException::CVariantException(const string& message)
    : CException(DIAG_COMPILE_INFO, 0, (CException::EErrCode)eVariant,  message)
{
}


const char* CVariantException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eVariant:    return "eVariant";
    default:       return CException::GetErrCodeString();
    }
}


//==================================================================
CVariant CVariant::BigInt(Int8 *p)
{
    return CVariant(p ? new CDB_BigInt(*p) : new CDB_BigInt());
}

CVariant CVariant::Int(Int4 *p)
{
    return CVariant(p ? new CDB_Int(*p) : new CDB_Int());
}

CVariant CVariant::SmallInt(Int2 *p)
{
    return CVariant(p ? new CDB_SmallInt(*p) : new CDB_SmallInt());
}

CVariant CVariant::TinyInt(Uint1 *p)
{
    return CVariant(p ? new CDB_TinyInt(*p) : new CDB_TinyInt());
}

CVariant CVariant::Float(float *p)
{
    return CVariant(p ? new CDB_Float(*p) : new CDB_Float());
}

CVariant CVariant::Double(double *p)
{
    return CVariant(p ? new CDB_Double(*p) : new CDB_Double());
}

CVariant CVariant::Bit(bool *p)
{
    return CVariant(p ? new CDB_Bit(*p) : new CDB_Bit());
}

CVariant CVariant::LongChar(const char *p, size_t len)
{
    return CVariant(p ? new CDB_LongChar(len, p) : new CDB_LongChar(len));
}

CVariant CVariant::LongChar(const TStringUCS2 &s, size_t len)
{
    return CVariant(new CDB_LongChar(len, s));
}

CVariant CVariant::VarChar(const char *p, size_t len)
{
    return CVariant(p ? (len ? new CDB_VarChar(p, len) : new CDB_VarChar(p))
                    : new CDB_VarChar());
}
CVariant CVariant::VarChar(const TStringUCS2 &s, size_t len)
{
    return CVariant(len ? new CDB_VarChar(s, len) : new CDB_VarChar(s));
}

CVariant CVariant::Char(size_t size, const char *p)
{
    return CVariant(p ? new CDB_Char(size, p) : new CDB_Char(size));
}

CVariant CVariant::Char(size_t size, const TStringUCS2 &s)
{
    return CVariant(new CDB_Char(size, s));
}

CVariant CVariant::LongBinary(size_t maxSize, const void *p, size_t len)
{
    return CVariant(p ? new CDB_LongBinary(maxSize, p, len) : new CDB_LongBinary(maxSize));
}

CVariant CVariant::VarBinary(const void *p, size_t len)
{
    return CVariant(p ? new CDB_VarBinary(p, len) : new CDB_VarBinary());
}

CVariant CVariant::Binary(size_t size, const void *p, size_t len)
{
    return CVariant(p ? new CDB_Binary(size, p, len) : new CDB_Binary(size));
}

CVariant CVariant::SmallDateTime(CTime *p)
{
    return CVariant(p ? new CDB_SmallDateTime(*p) : new CDB_SmallDateTime());
}

CVariant CVariant::DateTime(CTime *p)
{
    return CVariant(p ? new CDB_DateTime(*p) : new CDB_DateTime());
}

CVariant CVariant::Numeric(unsigned int precision,
                           unsigned int scale,
                           const char* p)
{
    return CVariant(p ? new CDB_Numeric(precision, scale, p)
                    : new CDB_Numeric());
}


CVariant::CVariant(EDB_Type type, size_t size)
    : m_data(0)
{
    switch ( type ) {
    case eDB_Int:
        m_data = new CDB_Int();
        return;
    case eDB_SmallInt:
        m_data = new CDB_SmallInt();
        return;
    case eDB_TinyInt:
        m_data = new CDB_TinyInt();
        return;
    case eDB_BigInt:
        m_data = new CDB_BigInt();
        return;
    case eDB_LongChar:
        if( size == 0 )
        {
            NCBI_THROW(CVariantException, eVariant, "Illegal argument, the size of LONGCHAR should not be 0");
        }
        m_data = new CDB_LongChar(size);
        return;
    case eDB_VarChar:
        m_data = new CDB_VarChar();
        return;
    case eDB_Char:
        if( size == 0 )
        {
            NCBI_THROW(CVariantException, eVariant, "Illegal argument, the size of CHAR should not be 0");
        }
        m_data = new CDB_Char(size);
        return;
    case eDB_LongBinary:
        if( size == 0 )
        {
            NCBI_THROW(CVariantException, eVariant, "Illegal argument, the size of LONGBINARY should not be 0");
        }
        m_data = new CDB_LongBinary(size);
        return;
    case eDB_VarBinary:
        m_data = new CDB_VarBinary();
        return;
    case eDB_Binary:
        if( size == 0 )
        {
            NCBI_THROW(CVariantException, eVariant, "Illegal argument, the size of BINARY should not be 0");
        }
        m_data = new CDB_Binary(size);
        return;
    case eDB_Float:
        m_data = new CDB_Float();
        return;
    case eDB_Double:
        m_data = new CDB_Double();
        return;
    case eDB_DateTime:
        m_data = new CDB_DateTime();
        return;
    case eDB_SmallDateTime:
        m_data = new CDB_SmallDateTime();
        return;
    case eDB_Text:
        m_data = new CDB_Text();
        return;
    case eDB_Image:
        m_data = new CDB_Image();
        return;
    case eDB_Bit:
        m_data = new CDB_Bit();
        return;
    case eDB_Numeric:
        m_data = new CDB_Numeric();
        return;
    case eDB_UnsupportedType:
        break;
    }
    NCBI_THROW(CVariantException, eVariant,
               string("Unsupported type: ")
               + CDB_Object::GetTypeName(type, false));
}


CVariant::CVariant(CDB_Object* o)
    : m_data(o)
{
    return;
}


CVariant::CVariant(Int8 v)
    : m_data(new CDB_BigInt(v)) {}


CVariant::CVariant(Int4 v)
    : m_data(new CDB_Int(v)) {}

//CVariant::CVariant(int v)
//: m_data(new CDB_Int(v)) {}

CVariant::CVariant(Int2 v)
    : m_data(new CDB_SmallInt(v)) {}

CVariant::CVariant(Uint1 v)
    : m_data(new CDB_TinyInt(v)) {}

CVariant::CVariant(float v)
    : m_data(new CDB_Float(v)) {}

CVariant::CVariant(double v)
    : m_data(new CDB_Double(v)) {}

CVariant::CVariant(bool v)
    : m_data(new CDB_Bit(v)) {}

CVariant::CVariant(const string& v)
    : m_data(new CDB_VarChar(v))
{
}

CVariant::CVariant(const char* s)
    : m_data(new CDB_VarChar(s))
{
}

CVariant::CVariant(const TStringUCS2& v)
    : m_data(new CDB_VarChar(v))
{
}

CVariant::CVariant(const CTime& v, EDateTimeFormat fmt)
    : m_data(0)
{

    switch(fmt) {
    case eShort:
        m_data = new CDB_SmallDateTime(v);
        break;
    case eLong:
        m_data = new CDB_DateTime(v);
        break;
    default:
        NCBI_THROW(CVariantException, eVariant,
                   "CVariant::ctor(): unsupported datetime type "
                   + NStr::IntToString(fmt));
    }

    if (v.IsEmpty()) {
        SetNull();
    }
}


CVariant::CVariant(const CVariant& v)
    : m_data(0)
{
    if( v.GetData() != 0 ) {
        m_data = v.GetData()->Clone();
    }
}

CVariant::~CVariant(void)
{
    try {
        delete m_data;
    }
    NCBI_CATCH_ALL_X( 1, kEmptyStr )
}


CDB_Object* CVariant::GetNonNullData() const {
    if( m_data == 0 )
        NCBI_THROW(CVariantException, eVariant, "CVariant::GetNonNullData(): null data");

    return m_data;
}

void CVariant::SetData(CDB_Object* o) {
    delete m_data;
    m_data = o;
}



string CVariant::GetString(void) const
{
    string s("");

    if( IsNull() )
    {
        switch( GetType() ) {
            case eDB_TinyInt:
            case eDB_SmallInt:
            case eDB_Int:
            case eDB_BigInt:
            case eDB_Numeric:
                s = "0";
                break;
            case eDB_Float:
            case eDB_Double:
                s = "0.0";
                break;
            default:
                break;
        }
    }
    else
    {
        switch( GetType() ) {
            case eDB_Char:
            case eDB_VarChar:
            case eDB_LongChar:
                s = ((CDB_String*)GetData())->AsString();
                break;
            case eDB_Binary:
                {
                    CDB_Binary *b = (CDB_Binary*)GetData();
                    s = string((char*)b->Value(), b->Size());
                    break;
                }
            case eDB_LongBinary:
                {
                    CDB_LongBinary *vb = (CDB_LongBinary*)GetData();
                    s = string((char*)vb->Value(), vb->DataSize());
                    break;
                }
            case eDB_VarBinary:
                {
                    CDB_VarBinary *vb = (CDB_VarBinary*)GetData();
                    s = string((char*)vb->Value(), vb->Size());
                    break;
                }
            case eDB_TinyInt:
                s = NStr::IntToString(GetByte());
                break;
            case eDB_SmallInt:
                s = NStr::IntToString(GetInt2());
                break;
            case eDB_Int:
                s = NStr::IntToString(GetInt4());
                break;
            case eDB_BigInt:
                s = NStr::Int8ToString(GetInt8());
                break;
            case eDB_Float:
                s = NStr::DoubleToString(GetFloat());
                break;
            case eDB_Double:
                s = NStr::DoubleToString(GetDouble());
                break;
            case eDB_Bit:
                s = NStr::BoolToString(GetBit());
                break;
            case eDB_Numeric:
                s = ((CDB_Numeric*)GetData())->Value();
                break;
            case eDB_DateTime:
            case eDB_SmallDateTime:
                s = GetCTime().AsString();
                break;
            case eDB_Text:
            case eDB_Image:
                {
                    CDB_Stream* stream = (CDB_Stream*)GetData();
                    char* buff[4096];
                    size_t read_bytes = 0;
                
                    s.reserve(stream->Size());
                    while ((read_bytes = stream->Read(buff, sizeof(buff))) != 0) {
                        s.append((const char*) buff, read_bytes);

                        if (read_bytes < sizeof(buff)) {
                            break;
                        }
                    }
                }
                break;
            default:
                x_Verify_AssignType(eDB_UnsupportedType, "string");
                break;
        }
    }

    return s;
}


Int8 CVariant::GetInt8() const
{
    if( !IsNull() )
    {
        switch( GetType() ) {
        case eDB_BigInt:
            return ((CDB_BigInt*)GetData())->Value();
        case eDB_Int:
            return ((CDB_Int*)GetData())->Value();
        case eDB_SmallInt:
            return ((CDB_SmallInt*)GetData())->Value();
        case eDB_TinyInt:
            return ((CDB_TinyInt*)GetData())->Value();
        default:
            x_Verify_AssignType(eDB_UnsupportedType, "Int8");
        }
    }
    return 0;
}


Int4 CVariant::GetInt4() const
{
    if( ! IsNull() )
    {
        switch( GetType() ) {
        case eDB_Int:
            return ((CDB_Int*)GetData())->Value();
        case eDB_SmallInt:
            return ((CDB_SmallInt*)GetData())->Value();
        case eDB_TinyInt:
            return ((CDB_TinyInt*)GetData())->Value();
        default:
            x_Verify_AssignType(eDB_UnsupportedType, "Int4");
        }
    }
    return 0;
}

Int2 CVariant::GetInt2() const
{
    if( !IsNull() )
    {
        switch( GetType() ) {
        case eDB_SmallInt:
            return ((CDB_SmallInt*)GetData())->Value();
        case eDB_TinyInt:
            return ((CDB_TinyInt*)GetData())->Value();
        default:
            x_Verify_AssignType(eDB_UnsupportedType, "Int2");
        }
    }
    return 0;
}

Uint1 CVariant::GetByte() const
{
    if( !IsNull() )
    {
        switch( GetType() ) {
        case eDB_TinyInt:
            return ((CDB_TinyInt*)GetData())->Value();
        default:
            x_Verify_AssignType(eDB_UnsupportedType, "Uint1");
        }
    }
    return 0;
}

float CVariant::GetFloat() const
{
    if( !IsNull() )
    {
        switch( GetType() ) {
        case eDB_Float:
            return ((CDB_Float*)GetData())->Value();
        case eDB_SmallInt:
            return ((CDB_SmallInt*)GetData())->Value();
        case eDB_TinyInt:
            return ((CDB_TinyInt*)GetData())->Value();
        default:
            x_Verify_AssignType(eDB_UnsupportedType, "float");
        }
    }
    return 0.;
}

double CVariant::GetDouble() const
{
    if( !IsNull() )
    {
        switch( GetType() ) {
        case eDB_Float:
            return ((CDB_Float*)GetData())->Value();
        case eDB_Double:
            return ((CDB_Double*)GetData())->Value();
        case eDB_Int:
            return ((CDB_Int*)GetData())->Value();
        case eDB_SmallInt:
            return ((CDB_SmallInt*)GetData())->Value();
        case eDB_TinyInt:
            return ((CDB_TinyInt*)GetData())->Value();
        default:
            x_Verify_AssignType(eDB_UnsupportedType, "double");
        }
    }
    return 0.;
}

bool CVariant::GetBit() const
{
    if( !IsNull() )
    {
        x_Verify_AssignType(eDB_Bit, "bool");
        return ((CDB_Bit*)GetData())->Value() != 0;
    }
    return false;
}

string CVariant::GetNumeric() const
{
    if( !IsNull() )
    {
        x_Verify_AssignType(eDB_Numeric, "string");
        return ((CDB_Numeric*)GetData())->Value();
    }
    return "";
}

const CTime& CVariant::GetCTime() const
{
    CTime *ptr = NULL;
    switch(GetType()) {
    case eDB_DateTime:
        ptr = const_cast<CTime*>(&((CDB_DateTime*)GetData())->Value());
        break;
    case eDB_SmallDateTime:
        ptr = const_cast<CTime*>(&((CDB_SmallDateTime*)GetData())->Value());
        break;
    default:
        x_Verify_AssignType(eDB_UnsupportedType, "CTime");
    }
    if( IsNull() )
        ptr->Clear();
    return *ptr;
}

string CVariant::AsNotNullString(const string& v) const
{
    if( IsNull() )
        return v;
    else
        return GetString();
}

bool CVariant::IsNull() const
{
    return GetData() == 0 ? true : GetData()->IsNULL();
}

void CVariant::SetNull()
{
    if( GetData() != 0 )
        GetData()->AssignNULL();
}

size_t CVariant::Read(void* buf, size_t len) const
{
    if( !IsNull() )
    {
        switch(GetType()) {
        case eDB_Image:
        case eDB_Text:
            return ((CDB_Stream*)GetData())->Read(buf, len);
        default:
            x_Inapplicable_Method("Read()");
        }
    }
    return 0;
}

size_t CVariant::Append(const void* buf, size_t len)
{
    switch(GetType()) {
    case eDB_Image:
    case eDB_Text:
        return ((CDB_Stream*)GetData())->Append(buf, len);

    default:
        x_Inapplicable_Method("Append()");
    }
    return 0;
}

size_t CVariant::Append(const string& str)
{
    switch(GetType()) {
    case eDB_Text:
        return ((CDB_Text*)GetData())->Append(str);

    default:
        x_Inapplicable_Method("Append()");
    }
    return 0;
}

size_t CVariant::Append(const TStringUCS2& str)
{
    switch(GetType()) {
    case eDB_Text:
        return ((CDB_Text*)GetData())->Append(str);

    default:
        x_Inapplicable_Method("Append()");
    }
    return 0;
}

size_t CVariant::GetBlobSize() const
{
    switch(GetType()) {
    case eDB_Image:
    case eDB_Text:
        return ((CDB_Stream*)GetData())->Size();
    default:
        x_Inapplicable_Method("GetBlobSize()");
    }
    return 0;
}

void CVariant::Truncate(size_t len)
{
    switch(GetType()) {
    case eDB_Image:
        ((CDB_Image*)GetData())->Truncate(len);
        break;
    case eDB_Text:
        ((CDB_Text*)GetData())->Truncate(len);
        break;
    default:
        x_Inapplicable_Method("Truncate()");
    }
    return;
}

bool CVariant::MoveTo(size_t pos) const
{
    switch(GetType()) {
    case eDB_Image:
        return ((CDB_Image*)GetData())->MoveTo(pos);
    case eDB_Text:
        return ((CDB_Text*)GetData())->MoveTo(pos);
    default:
        x_Inapplicable_Method("MoveTo()");
    }
    return false;
}

CVariant& CVariant::operator=(const Int8& v)
{
    x_Verify_AssignType(eDB_BigInt, "Int8");
    *((CDB_BigInt*)GetData()) = v;
    return *this;
}

CVariant& CVariant::operator=(const Int4& v)
{
    x_Verify_AssignType(eDB_Int, "Int4");
    *((CDB_Int*)GetData()) = v;
    return *this;
}

CVariant& CVariant::operator=(const Int2& v)
{
    x_Verify_AssignType(eDB_SmallInt, "Int2");
    *((CDB_SmallInt*)GetData()) = v;
    return *this;
}

CVariant& CVariant::operator=(const Uint1& v)
{
    x_Verify_AssignType(eDB_TinyInt, "Uint1");
    *((CDB_TinyInt*)GetData()) = v;
    return *this;
}

CVariant& CVariant::operator=(const float& v)
{
    x_Verify_AssignType(eDB_Float, "float");
    *((CDB_Float*)GetData()) = v;
    return *this;
}

CVariant& CVariant::operator=(const double& v)
{
    x_Verify_AssignType(eDB_Double, "double");
    *((CDB_Double*)GetData()) = v;
    return *this;
}

CVariant& CVariant::operator=(const string& v)
{
    switch( GetType()) {
    case eDB_VarChar:
        *((CDB_VarChar*)GetData()) = v;
        break;
    case eDB_LongChar:
        *((CDB_LongChar*)GetData()) = v;
        break;
    case eDB_Char:
        *((CDB_Char*)GetData()) = v;
        break;
    default:
        x_Verify_AssignType(eDB_UnsupportedType, "string");
    }

    return *this;
}

CVariant& CVariant::operator=(const char* v)
{
    switch( GetType()) {
    case eDB_VarChar:
        *((CDB_VarChar*)GetData()) = v;
        break;
    case eDB_LongChar:
        *((CDB_LongChar*)GetData()) = v;
        break;
    case eDB_Char:
        *((CDB_Char*)GetData()) = v;
        break;
    default:
        x_Verify_AssignType(eDB_UnsupportedType, "const char *");
    }

    return *this;
}

CVariant& CVariant::operator=(const TStringUCS2& v)
{
    switch( GetType()) {
    case eDB_VarChar:
    case eDB_LongChar:
    case eDB_Char:
        *((CDB_String*)GetData()) = v;
        break;
    default:
        x_Verify_AssignType(eDB_UnsupportedType, "TStringUCS2");
    }

    return *this;
}

CVariant& CVariant::operator=(const bool& v)
{
    x_Verify_AssignType(eDB_Bit, "bool");
    *((CDB_Bit*)GetData()) = v;
    return *this;
}

CVariant& CVariant::operator=(const CTime& v)
{
    switch(GetType()) {
    case eDB_DateTime:
        *((CDB_DateTime*)GetData()) = v;
        break;
    case eDB_SmallDateTime:
        *((CDB_SmallDateTime*)GetData()) = v;
        break;
    default:
        x_Verify_AssignType(eDB_UnsupportedType, "CTime");
    }
    return *this;
}

CVariant& CVariant::operator=(const CVariant& v)
{
    this->m_data->AssignValue(*(v.m_data));
    return *this;
}

bool operator<(const CVariant& v1, const CVariant& v2)
{
    bool less = false;

    if( v1.IsNull() || v2.IsNull() ) {
        less = v1.IsNull() && !v2.IsNull();
    }
    else {
        if( v1.GetType() != v2.GetType() ) {
            NCBI_THROW(CVariantException, eVariant,
                       string("Cannot compare different types ")
                       + CDB_Object::GetTypeName(v1.GetType(), false) + " and "
                       + CDB_Object::GetTypeName(v2.GetType(), false));
        }

        switch( v1.GetType() ) {
        case eDB_Char:
        case eDB_VarChar:
        case eDB_LongChar:
            less = v1.GetString() < v2.GetString();
            break;
        case eDB_TinyInt:
            less = v1.GetByte() < v2.GetByte();
            break;
        case eDB_SmallInt:
            less = v1.GetInt2() < v2.GetInt2();
            break;
        case eDB_Int:
            less = v1.GetInt4() < v2.GetInt4();
            break;
        case eDB_BigInt:
            less = v1.GetInt8() < v2.GetInt8();
            break;
        case eDB_Float:
            less = v1.GetFloat() < v2.GetFloat();
            break;
        case eDB_Double:
            less = v1.GetDouble() < v2.GetDouble();
            break;
        case eDB_DateTime:
        case eDB_SmallDateTime:
            less = v1.GetCTime() < v2.GetCTime();
            break;
        default:
            NCBI_THROW(CVariantException, eVariant,
                       string("Type not supported: ")
                       + CDB_Object::GetTypeName(v1.GetType(), false));
        }
    }
    return less;
}

bool operator==(const CVariant& v1, const CVariant& v2)
{
    bool less = false;

    if( v1.IsNull() || v2.IsNull() ) {
        less = v1.IsNull() && !v2.IsNull();
    }
    else {
        if( v1.GetType() != v2.GetType() ) {
            NCBI_THROW(CVariantException, eVariant,
                       string("Cannot compare different types ")
                       + CDB_Object::GetTypeName(v1.GetType(), false) + " and "
                       + CDB_Object::GetTypeName(v2.GetType(), false));
        }

        switch( v1.GetType() ) {
        case eDB_Char:
        case eDB_VarChar:
        case eDB_LongChar:
        case eDB_Binary:
        case eDB_VarBinary:
            less = v1.GetString() == v2.GetString();
            break;
        case eDB_Bit:
            less = v1.GetBit() == v2.GetBit();
            break;
        case eDB_TinyInt:
            less = v1.GetByte() == v2.GetByte();
            break;
        case eDB_SmallInt:
            less = v1.GetInt2() == v2.GetInt2();
            break;
        case eDB_Int:
            less = v1.GetInt4() == v2.GetInt4();
            break;
        case eDB_BigInt:
            less = v1.GetInt8() == v2.GetInt8();
            break;
        case eDB_Float:
            less = v1.GetFloat() == v2.GetFloat();
            break;
        case eDB_Double:
            less = v1.GetDouble() == v2.GetDouble();
            break;
        case eDB_DateTime:
        case eDB_SmallDateTime:
            less = v1.GetCTime() == v2.GetCTime();
            break;
        default:
            NCBI_THROW(CVariantException, eVariant,
                       string("Type not supported: ")
                       + CDB_Object::GetTypeName(v1.GetType(), false));
        }
    }
    return less;
}

EBulkEnc CVariant::GetBulkInsertionEnc(void) const
{
    if ( !GetData() ) {
        return eBulkEnc_RawBytes;
    }

    switch (GetType()) {
    case eDB_VarChar:
    case eDB_Char:
    case eDB_LongChar:
        return ((const CDB_String*)GetData())->GetBulkInsertionEnc();
    case eDB_Text:
        return ((const CDB_Text*)GetData())->GetEncoding();
    default:
        return eBulkEnc_RawBytes;
    }
}

void CVariant::SetBulkInsertionEnc(EBulkEnc e)
{
    if ( !GetData() ) {
        return;
    }

    switch (GetType()) {
    case eDB_VarChar:
    case eDB_Char:
    case eDB_LongChar:
        ((CDB_String*)GetData())->SetBulkInsertionEnc(e);
        break;
    case eDB_Text:
        ((CDB_Text*)GetData())->SetEncoding(e);
        break;
    default:
        break;
    }
}


END_NCBI_SCOPE
