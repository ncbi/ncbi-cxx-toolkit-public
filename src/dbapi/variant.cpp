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
#include <algorithm>
//#include <corelib/ncbistr.hpp>


BEGIN_NCBI_SCOPE

//CVariantException::CVariantException(const string& msg)
//    : m_msg(msg) 
//{
//
//}
//    
//CVariantException::~CVariantException() throw() 
//{
//
//}
//
//const char* CVariantException::what() const throw() 
//{
//    return m_msg.c_str();
//}

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

CVariant CVariant::VarChar(const char *p, size_t len)
{
    return CVariant(p ? (len ? new CDB_VarChar(p, len) : new CDB_VarChar(p))
                    : new CDB_VarChar());
}

CVariant CVariant::Char(size_t size, const char *p)
{
    return CVariant(p ? new CDB_Char(size, p) : new CDB_Char(size));
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
        m_data = new CDB_LongChar(size);
        return;  
    case eDB_VarChar:
        m_data = new CDB_VarChar();
        return;  
    case eDB_Char:
        m_data = new CDB_Char(size);
        return;  
    case eDB_LongBinary:
        m_data = new CDB_LongBinary(size);
        return;  
    case eDB_VarBinary:
        m_data = new CDB_VarBinary();
        return;  
    case eDB_Binary:
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
    NCBI_THROW(CVariantException, eVariant, "CVariant::ctor():  unsupported type");
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
    : m_data(new CDB_VarChar(v)) {}

CVariant::CVariant(const char* s) 
    : m_data(new CDB_VarChar(s)) {}

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
        NCBI_THROW(CVariantException, eVariant, "CVariant::ctor(): unsupported datetime type");
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
    delete m_data;
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
    if( IsNull() )
    {
        switch( GetType() ) {
        case eDB_Char:
        case eDB_VarChar:
        case eDB_LongChar:
        case eDB_Binary:
        case eDB_LongBinary:
        case eDB_VarBinary:
        case eDB_Bit:
        case eDB_DateTime:
        case eDB_SmallDateTime:
            return "";
        case eDB_TinyInt:
        case eDB_SmallInt:      
        case eDB_Int:
        case eDB_BigInt:
        case eDB_Numeric:
            return "0";
        case eDB_Float:
        case eDB_Double:
            return "0.0";
        default:
            break;
        }
    }
    else
    {
        switch( GetType() ) {
        case eDB_Char:
            return ((CDB_Char*)GetData())->Value();
        case eDB_VarChar:
            return ((CDB_VarChar*)GetData())->Value();
        case eDB_LongChar:
            return ((CDB_LongChar*)GetData())->Value();
        case eDB_Binary:
            {
                CDB_Binary *b = (CDB_Binary*)GetData();
                return string((char*)b->Value(), b->Size());
            }
        case eDB_LongBinary:
            {
                CDB_LongBinary *vb = (CDB_LongBinary*)GetData();
                return string((char*)vb->Value(), vb->Size());
            }
        case eDB_VarBinary:
            {
                CDB_VarBinary *vb = (CDB_VarBinary*)GetData();
                return string((char*)vb->Value(), vb->Size());
            }
        case eDB_TinyInt:
            return NStr::IntToString((long)GetByte()); 
        case eDB_SmallInt:      
            return NStr::IntToString(GetInt2()); 
        case eDB_Int:
            return NStr::IntToString(GetInt4()); 
        case eDB_BigInt:
            return NStr::Int8ToString(GetInt8()); 
        case eDB_Float:
            return NStr::DoubleToString(GetFloat()); 
        case eDB_Double:
            return NStr::DoubleToString(GetDouble()); 
        case eDB_Bit:
            return NStr::BoolToString(GetBit()); 
        case eDB_Numeric:
            return ((CDB_Numeric*)GetData())->Value(); 
        case eDB_DateTime:
        case eDB_SmallDateTime:
            return GetCTime().AsString();
        default:
            break;
        }
    }  
    NCBI_THROW(CVariantException, eVariant, "Type not supported");
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
            VerifyType(false);
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
            VerifyType(false);
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
            VerifyType(false);
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
            VerifyType(false);
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
            VerifyType(false);
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
            VerifyType(false);
        }
    }
    return 0.;
}

bool CVariant::GetBit() const
{
    if( !IsNull() )
    {
        VerifyType( GetType() == eDB_Bit );
        return ((CDB_Bit*)GetData())->Value() != 0;
    }
    return false;
}

string CVariant::GetNumeric() const
{
    if( !IsNull() )
    {
        VerifyType( GetType() == eDB_Numeric );
        return ((CDB_Numeric*)GetData())->Value();
    }
    return "";
}

const CTime& CVariant::GetCTime() const
{
    CTime *ptr;
    switch(GetType()) {
    case eDB_DateTime:
        ptr = const_cast<CTime*>(&((CDB_DateTime*)GetData())->Value());
        break;
    case eDB_SmallDateTime:
        ptr = const_cast<CTime*>(&((CDB_SmallDateTime*)GetData())->Value());
        break;
    default:
        VerifyType(false);
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
            VerifyType(false);
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
        VerifyType(false);
    }
    return 0;
}

size_t CVariant::GetBlobSize() const
{
    if( !IsNull() )
    {
        switch(GetType()) {
        case eDB_Image:
        case eDB_Text:
            return ((CDB_Stream*)GetData())->Size();
        default:
            VerifyType(false);
        }
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
        VerifyType(false);
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
        VerifyType(false);
    }
    return false;
}

CVariant& CVariant::operator=(const Int8& v)
{
    VerifyType(GetType() == eDB_BigInt);
    *((CDB_BigInt*)GetData()) = v;
    return *this;
}

CVariant& CVariant::operator=(const Int4& v)
{
    VerifyType(GetType() == eDB_Int);
    *((CDB_Int*)GetData()) = v;
    return *this;
}

CVariant& CVariant::operator=(const Int2& v)
{
    VerifyType(GetType() == eDB_SmallInt);
    *((CDB_SmallInt*)GetData()) = v;
    return *this;
}

CVariant& CVariant::operator=(const Uint1& v)
{
    VerifyType(GetType() == eDB_TinyInt);
    *((CDB_TinyInt*)GetData()) = v;
    return *this;
}

CVariant& CVariant::operator=(const float& v)
{
    VerifyType(GetType() == eDB_Float);
    *((CDB_Float*)GetData()) = v;
    return *this;
}

CVariant& CVariant::operator=(const double& v)
{
    VerifyType(GetType() == eDB_Double);
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
        VerifyType(false);
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
        VerifyType(false);
    }

    return *this;
}

CVariant& CVariant::operator=(const bool& v)
{
    VerifyType(GetType() == eDB_Bit);
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
        VerifyType(false);
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
            NCBI_THROW(CVariantException, eVariant, "Cannot compare different types");
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
            NCBI_THROW(CVariantException, eVariant, "Type not supported");
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
            NCBI_THROW(CVariantException, eVariant, "Cannot compare different types");
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
            NCBI_THROW(CVariantException, eVariant, "Type not supported");
        }
    }
    return less;
}
/*
* $Log$
* Revision 1.36  2005/05/05 20:09:18  kholodov
* Fixed: comparison operators
*
* Revision 1.35  2005/05/03 19:25:45  kholodov
* Modified: return empty string for NULL datetime columns instead of CTime().AsString()
*
* Revision 1.34  2005/05/03 19:13:35  kholodov
* Modified: NULL CVariants return "natural empty values", no exception thrown
* Added: CVariantException(const string&) constructor for backward compatibility.
*
* Revision 1.33  2005/05/02 21:13:31  kholodov
* Modified: made CException base class for CVariantException
* Added: throw exception if attempt to get value from NULL column is made
*
* Revision 1.32  2005/02/02 19:49:54  grichenk
* Fixed more warnings
*
* Revision 1.31  2004/09/29 14:58:07  kholodov
* Added: SetNull() method
*
* Revision 1.30  2004/07/20 17:49:17  kholodov
* Added: IReader/IWriter support for BLOB I/O
*
* Revision 1.29  2004/05/17 21:10:28  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.28  2004/04/08 15:56:58  kholodov
* Multiple bug fixes and optimizations
*
* Revision 1.27  2004/02/10 18:50:44  kholodov
* Modified: made Move() method const
*
* Revision 1.26  2003/12/10 22:34:14  kholodov
* Added: MoveTo() method
*
* Revision 1.25  2003/12/10 21:08:48  kholodov
* Fixed: size of the fixed size columns is now correct for NULL value
*
* Revision 1.24  2003/12/05 15:05:06  kholodov
* Added: float->double implicit conversion
*
* Revision 1.23  2003/11/18 16:59:18  kholodov
* Added: operator=(const char*)
*
* Revision 1.22  2003/08/15 19:48:43  kholodov
* Fixed: const method GetBlobSize()
*
* Revision 1.21  2003/08/12 21:08:39  kholodov
* Added: AsNotNullString() method
*
* Revision 1.20  2003/07/17 18:34:11  kholodov
* Modified: operator=(CVariant& ) uses now CDB_Object::AssignValue() method
*
* Revision 1.19  2003/06/25 22:24:21  kholodov
* Added: GetBlobSize() method
*
* Revision 1.18  2003/05/05 18:32:50  kholodov
* Added: LONGCHAR and LONGBINARY support
*
* Revision 1.17  2003/01/10 14:23:33  kholodov
* Modified: GetString() uses NStr::<...> type conversion
*
* Revision 1.16  2003/01/09 19:59:29  kholodov
* Fixed: operator=(CVariant&) rewritten using copy ctor
*
* Revision 1.15  2002/10/31 22:37:05  kholodov
* Added: DisableBind(), GetColumnNo(), GetTotalColumns() methods
* Fixed: minor errors, diagnostic messages
*
* Revision 1.14  2002/10/21 20:38:08  kholodov
* Added: GetParentConn() method to get the parent connection from IStatement,
* ICallableStatement and ICursor objects.
* Fixed: Minor fixes
*
* Revision 1.13  2002/10/11 16:42:50  kholodov
* Added: return Binary and Varbinary columns as CVariant::GetString()
*
* Revision 1.12  2002/09/16 21:04:02  kholodov
* Modified: CVariant::Assign<> template removed
*
* Revision 1.11  2002/09/16 20:22:44  kholodov
* Fixed: return statement for the void function
*
* Revision 1.10  2002/09/16 19:30:58  kholodov
* Added: Numeric datatype support
* Added: CVariant::operator=() methods for working with bulk insert
* Added: Methods for writing BLOBs during bulk insert
*
* Revision 1.9  2002/09/10 16:54:15  kholodov
* Modified: using CDB_Object::operator=() for CVariant assignments
*
* Revision 1.8  2002/04/15 19:12:11  kholodov
* Added simple type conversions
*
* Revision 1.7  2002/03/13 16:52:10  kholodov
* Added: Full destructor definition in CVariantException with throw()
* to conform with the parent's virtual destructor.
* Modified: Moved CVariantException methods' definitions to variant.cpp file
*
* Revision 1.6  2002/02/15 23:24:42  vakatov
* CVariant::CVariant() -- fixed a bug introduced during the cleanup
*
* Revision 1.5  2002/02/14 00:59:40  vakatov
* Clean-up: warning elimination, fool-proofing, fine-tuning, identation, etc.
*
* Revision 1.4  2002/02/08 15:50:32  kholodov
* Modified: integer types used are Int8, Int4, Int2, Uint1
* Added: factories for CVariants of a particular type
*
* Revision 1.3  2002/02/06 22:50:48  kholodov
* Conditionalized the usage of long long
*
* Revision 1.2  2002/02/06 22:21:00  kholodov
* Added constructor from long long to BigInt type
*
* Revision 1.1  2002/01/30 14:51:22  kholodov
* User DBAPI implementation, first commit
*
*
*/
END_NCBI_SCOPE
