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
* $Log$
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
*
*/ 

#include <dbapi/variant.hpp>
#include <algorithm>
#include "basetmpl.hpp"
#include <corelib/ncbistre.hpp>


BEGIN_NCBI_SCOPE

const char* CVariantException::what() const throw() 
{
  return m_msg.c_str();
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

CVariant CVariant::VarChar(const char *p, size_t len)
{
    return CVariant(p ? (len ? new CDB_VarChar(p, len) : new CDB_VarChar(p))
                    : new CDB_VarChar());
}

CVariant CVariant::Char(size_t size, const char *p)
{
    return CVariant(p ? new CDB_Char(size, p) : new CDB_Char());
}

CVariant CVariant::VarBinary(const void *p, size_t len)
{
    return CVariant(p ? new CDB_VarBinary(p, len) : new CDB_VarBinary());
}

CVariant CVariant::Binary(size_t size, const void *p, size_t len)
{
    return CVariant(p ? new CDB_Binary(size, p, len) : new CDB_Binary());
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


CVariant::CVariant(EDB_Type type)
  : m_data(0)
{
  switch(type) {
  case eDB_Int:
    m_data = new CDB_Int();
    break;  
  case eDB_SmallInt:
    m_data = new CDB_SmallInt();
    break;  
  case eDB_TinyInt:
    m_data = new CDB_TinyInt();
    break;  
  case eDB_BigInt:
    m_data = new CDB_BigInt();
    break;  
  case eDB_VarChar:
    m_data = new CDB_VarChar();
    break;  
  case eDB_Char:
    m_data = new CDB_Char();
    break;  
  case eDB_VarBinary:
    m_data = new CDB_Binary();
    break;  
  case eDB_Binary:
    m_data = new CDB_Binary();
    break;  
  case eDB_Float:
    m_data = new CDB_Float();
    break;  
  case eDB_Double:
    m_data = new CDB_Double();
    break;  
  case eDB_DateTime:
    m_data = new CDB_DateTime();
    break;
  case eDB_SmallDateTime:
    m_data = new CDB_SmallDateTime();
    break;
  case eDB_Text:
    m_data = new CDB_Text();
    break;
  case eDB_Image:
    m_data = new CDB_Image();
    break;
  case eDB_Bit:
    m_data = new CDB_Bit();
    break;
  case eDB_Numeric:
    m_data = new CDB_Numeric();
    break;
  case eDB_UnsupportedType:
    throw CVariantException("CVariant::ctor(): unsupported type");
    break;
  }
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
  : m_data(new CDB_Bit(v ? 1 : 0)) {}

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
    throw CVariantException("CVariant::ctor(): unsupported datetime type");
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


CDB_Object* CVariant::GetData() const { 
  return m_data; 
}

CDB_Object* CVariant::GetNonNullData() const {
  if( m_data == 0 )
    throw CVariantException("CVariant::GetNonNullData(): null data");
  
  return m_data;
}

void CVariant::SetData(CDB_Object* o) {
  delete m_data;
  m_data = o;
}


EDB_Type CVariant::GetType() const
{
  return GetNonNullData()->GetType();
}

string CVariant::GetString(void) const 
{
  CNcbiOstrstream s;

  if( IsNull() )
    return "null";

  switch( GetType() ) {
  case eDB_Char:
    return ((CDB_Char*)GetData())->Value();
  case eDB_VarChar:
    return ((CDB_VarChar*)GetData())->Value();
  case eDB_TinyInt:
    s << (int)GetByte(); 
    return CNcbiOstrstreamToString(s);
  case eDB_SmallInt:      
    s << GetInt2(); 
    return CNcbiOstrstreamToString(s);
  case eDB_Int:
    s << GetInt4(); 
    return CNcbiOstrstreamToString(s);
  case eDB_Float:
    s << GetFloat(); 
    return CNcbiOstrstreamToString(s);
  case eDB_Double:
    s << GetDouble(); 
    return CNcbiOstrstreamToString(s);
  case eDB_Bit:
    s << GetBit(); 
    return CNcbiOstrstreamToString(s);
  case eDB_DateTime:
  case eDB_SmallDateTime:
    return GetCTime().AsString();
  default:
    throw CVariantException("CVariant::GetString(): type not supported");
  }
  
  return string("");

}

Int8 CVariant::GetInt8() const
{
  crash( GetType() == eDB_BigInt );
  return ((CDB_BigInt*)GetData())->Value();
}


Int4 CVariant::GetInt4() const
{
  crash( GetType() == eDB_Int );
  return ((CDB_Int*)GetData())->Value();
}

Int2 CVariant::GetInt2() const
{
  crash( GetType() == eDB_SmallInt );

  return ((CDB_SmallInt*)GetData())->Value();
}

Uint1 CVariant::GetByte() const
{
  crash( GetType() == eDB_TinyInt );

  return ((CDB_TinyInt*)GetData())->Value();
}

float CVariant::GetFloat() const
{
  
  crash( GetType() == eDB_Float );

  return ((CDB_Float*)GetData())->Value();
}

double CVariant::GetDouble() const
{
  crash( GetType() == eDB_Double );

  return ((CDB_Double*)GetData())->Value();
}

bool CVariant::GetBit() const
{
  crash( GetType() == eDB_Bit );

  return ((CDB_Bit*)GetData())->Value() != 0;
}

const CTime& CVariant::GetCTime() const
{
  switch(GetType()) {
  case eDB_DateTime:
    return ((CDB_DateTime*)GetData())->Value();
  case eDB_SmallDateTime:
    return ((CDB_SmallDateTime*)GetData())->Value();
  default:
    throw CVariantException("CVariant::GetCTime(): invalid type");
  }
}

bool CVariant::IsNull() const
{
  return GetData() == 0 ? true : GetData()->IsNULL();
}

CVariant& CVariant::operator=(const CVariant& v)
{
  CVariant temp(v);
  swap(m_data, temp.m_data);
  return *this;
}

bool CVariant::operator<(const CVariant& v) const 
{
  bool less = false;

  if( IsNull() || v.IsNull() ) {
    less = IsNull() && !v.IsNull();
  }
  else {
    if( GetType() != v.GetType() ) {
      throw CVariantException("CVariant::operator<(): cannot compare different types");
    }

    switch( GetType() ) {
    case eDB_Char:
    case eDB_VarChar:
      less = GetString() < v.GetString();
      break;
    case eDB_TinyInt:
      less = GetByte() < v.GetByte();
      break;
    case eDB_SmallInt:      
      less = GetInt2() < v.GetInt2();
      break;
    case eDB_Int:
      less = GetInt4() < v.GetInt4();
      break;
    case eDB_Float:
      less = GetFloat() < v.GetFloat();
      break;
    case eDB_Double:
      less = GetDouble() < v.GetDouble();
      break;
    case eDB_DateTime:
    case eDB_SmallDateTime:
      less = GetCTime() < v.GetCTime();
      break;
    default:
      throw CVariantException("CVariant::operator<(): type not supported");
    }
  }
  return less;
}

END_NCBI_SCOPE
