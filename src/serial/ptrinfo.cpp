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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.16  2000/02/17 20:02:45  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.15  1999/12/28 18:55:52  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.14  1999/12/17 19:05:04  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.13  1999/10/28 15:37:41  vasilche
* Fixed null choice pointers handling.
* Cleaned enumertion interface.
*
* Revision 1.12  1999/10/25 19:07:15  vasilche
* Fixed coredump on non initialized choices.
* Fixed compilation warning.
*
* Revision 1.11  1999/09/14 18:54:20  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.10  1999/08/31 17:50:09  vasilche
* Implemented several macros for specific data types.
* Added implicit members.
* Added multimap and set.
*
* Revision 1.9  1999/08/13 15:53:52  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.8  1999/07/20 18:23:13  vasilche
* Added interface to old ASN.1 routines.
* Added fixed choice of subclasses to use for pointers.
*
* Revision 1.7  1999/07/13 20:18:22  vasilche
* Changed types naming.
*
* Revision 1.6  1999/07/07 19:59:08  vasilche
* Reduced amount of data allocated on heap
* Cleaned ASN.1 structures info
*
* Revision 1.5  1999/06/30 16:05:04  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.4  1999/06/24 14:45:01  vasilche
* Added binary ASN.1 output.
*
* Revision 1.3  1999/06/15 16:19:52  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.2  1999/06/07 20:42:58  vasilche
* Fixed compilation under MS VS
*
* Revision 1.1  1999/06/04 20:51:48  vasilche
* First compilable version of serialization.
*
* ===========================================================================
*/

#include <serial/ptrinfo.hpp>
#include <serial/objostr.hpp>
#include <serial/objistr.hpp>

BEGIN_NCBI_SCOPE

CPointerTypeInfo::CPointerTypeInfo(TTypeInfo type)
    : CParent(type->GetName() + '*'), m_DataType(type)
{
}

CPointerTypeInfo::CPointerTypeInfo(const string& name, TTypeInfo type)
    : CParent(name), m_DataType(type)
{
}

CPointerTypeInfo::CPointerTypeInfo(const char* name, TTypeInfo type)
    : CParent(name), m_DataType(type)
{
}

CPointerTypeInfo::CPointerTypeInfo(const char* templ, TTypeInfo arg,
                                   TTypeInfo type)
    : CParent(string(templ) + "< " + arg->GetName() + " >"), m_DataType(type)
{
}

CPointerTypeInfo::CPointerTypeInfo(const char* templ, const char* arg,
                                   TTypeInfo type)
    : CParent(string(templ) + "< " + arg + " >"), m_DataType(type)
{
}

CPointerTypeInfo::~CPointerTypeInfo(void)
{
}

TTypeInfo CPointerTypeInfo::GetTypeInfo(TTypeInfo base)
{
    return new CPointerTypeInfo(base);
}

TTypeInfo CPointerTypeInfo::GetRealDataTypeInfo(TConstObjectPtr object) const
{
    TTypeInfo dataTypeInfo = GetDataTypeInfo();
    if ( object )
        dataTypeInfo = dataTypeInfo->GetRealTypeInfo(object);
    return dataTypeInfo;
}

TConstObjectPtr
CPointerTypeInfo::GetObjectPointer(TConstObjectPtr object) const
{
    return *static_cast<const TConstObjectPtr*>(object);
}

void CPointerTypeInfo::SetObjectPointer(TObjectPtr object,
                      TObjectPtr pointer) const
{
    *static_cast<TObjectPtr*>(object) = pointer;
}

size_t CPointerTypeInfo::GetSize(void) const
{
    return sizeof(void*);
}

TObjectPtr CPointerTypeInfo::Create(void) const
{
    return new void*(0);
}

bool CPointerTypeInfo::IsDefault(TConstObjectPtr object) const
{
    return GetObjectPointer(object) == 0;
}

bool CPointerTypeInfo::Equals(TConstObjectPtr object1,
                              TConstObjectPtr object2) const
{
    TConstObjectPtr data1 = GetObjectPointer(object1);
    TConstObjectPtr data2 = GetObjectPointer(object2);
    if ( data1 == 0 ) {
        return data2 == 0;
    }
    else {
        if ( data2 == 0 )
            return false;
        TTypeInfo type1 = GetRealDataTypeInfo(data1);
        TTypeInfo type2 = GetRealDataTypeInfo(data2);
        return type1 == type2 && type1->Equals(data1, data2);
    }
}

void CPointerTypeInfo::SetDefault(TObjectPtr dst) const
{
    SetObjectPointer(dst, 0);
}

void CPointerTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
    TConstObjectPtr data = GetObjectPointer(src);
    if ( data == 0 ) {
        SetObjectPointer(dst, 0);
    }
    else {
        TTypeInfo type = GetRealDataTypeInfo(data);
        TObjectPtr object = type->Create();
        type->Assign(object, data);
        SetObjectPointer(dst, object);
    }
}

void CPointerTypeInfo::WriteData(CObjectOStream& out,
                                 TConstObjectPtr object) const
{
    TConstObjectPtr data = GetObjectPointer(object);
    out.WritePointer(data, GetRealDataTypeInfo(data));
}

void CPointerTypeInfo::ReadData(CObjectIStream& in, TObjectPtr object) const
{
    SetObjectPointer(object, in.ReadPointer(GetDataTypeInfo()));
}

END_NCBI_SCOPE
