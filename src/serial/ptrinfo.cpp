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

#include <corelib/ncbistd.hpp>
#include <serial/ptrinfo.hpp>
#include <serial/objostr.hpp>
#include <serial/objistr.hpp>
#include <serial/asntypes.hpp>

BEGIN_NCBI_SCOPE

CTypeInfoMap<CPointerTypeInfo> CPointerTypeInfo::sm_Map;

pair<TConstObjectPtr, TTypeInfo>
CPointerTypeInfo::GetSource(TConstObjectPtr object) const
{
    TConstObjectPtr source = GetObjectPointer(object);
    TTypeInfo dataTypeInfo = GetDataTypeInfo();
    if ( source )
        dataTypeInfo = dataTypeInfo->GetRealTypeInfo(source);
    return make_pair(source, dataTypeInfo);
}
    
TConstObjectPtr CPointerTypeInfo::GetObjectPointer(TConstObjectPtr object) const
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

static const TConstObjectPtr zeroPointer = 0;

TConstObjectPtr CPointerTypeInfo::GetDefault(void) const
{
    return &zeroPointer;
}

bool CPointerTypeInfo::Equals(TConstObjectPtr object1,
                              TConstObjectPtr object2) const
{
    pair<TConstObjectPtr, TTypeInfo> data1 = GetSource(object1);
    pair<TConstObjectPtr, TTypeInfo> data2 = GetSource(object2);
    if ( data1.first == 0 ) {
        return data2.first == 0;
    }
    else {
        if ( data2.first == 0 )
            return false;
        return data1.second == data2.second &&
            data1.second->Equals(data1.first, data2.first);
    }
}

void CPointerTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
    pair<TConstObjectPtr, TTypeInfo> data = GetSource(src);
    if ( data.first == 0 )
        SetObjectPointer(dst, 0);
    else
        SetObjectPointer(dst, data.second->Clone(data.first));
}

void CPointerTypeInfo::CollectExternalObjects(COObjectList& objectList,
                                              TConstObjectPtr object) const
{
    pair<TConstObjectPtr, TTypeInfo> data = GetSource(object);
    if ( data.first )
        data.second->CollectObjects(objectList, data.first);
}

void CPointerTypeInfo::WriteData(CObjectOStream& out,
                                 TConstObjectPtr object) const
{
    pair<TConstObjectPtr, TTypeInfo> data = GetSource(object);
    out.WritePointer(data.first, data.second);
}

void CPointerTypeInfo::ReadData(CObjectIStream& in, TObjectPtr object) const
{
    SetObjectPointer(object, in.ReadPointer(GetDataTypeInfo()));
}

END_NCBI_SCOPE
