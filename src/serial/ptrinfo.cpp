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

BEGIN_NCBI_SCOPE

size_t CPointerTypeInfo::GetSize(void) const
{
    return sizeof(void*);
}

static const TConstObjectPtr zeroPointer = 0;

TConstObjectPtr CPointerTypeInfo::GetDefault(void) const
{
    return &zeroPointer;
}

bool CPointerTypeInfo::Equals(TConstObjectPtr object1,
                              TConstObjectPtr object2) const
{
    TConstObjectPtr data1 = GetObject(object1);
    TConstObjectPtr data2 = GetObject(object2);
    if ( data1 == 0 ) {
        return data2 == 0;
    }
    else {
        if ( data2 == 0 )
            return false;
        TTypeInfo dataTypeInfo = GetDataTypeInfo();
        TTypeInfo typeInfo1 = dataTypeInfo->GetRealTypeInfo(data1);
        TTypeInfo typeInfo2 = dataTypeInfo->GetRealTypeInfo(data2);
        return typeInfo1 == typeInfo2 &&
            typeInfo1->Equals(data1, data2);
    }
}

void CPointerTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
    TConstObjectPtr srcObject = GetObject(src);
    if ( srcObject == 0 ) {
        GetObject(dst) = 0;
        return;
    }
    
    TTypeInfo srcTypeInfo = GetDataTypeInfo()->GetRealTypeInfo(srcObject);
    TObjectPtr dstObject = GetObject(dst) = srcTypeInfo->Create();
    srcTypeInfo->Assign(dstObject, GetObject(src));
}

void CPointerTypeInfo::CollectExternalObjects(COObjectList& objectList,
                                              TConstObjectPtr object) const
{
    TConstObjectPtr externalObject = GetObject(object);
    _TRACE("CPointerTypeInfo::CollectExternalObjects: " << unsigned(externalObject));
    if ( externalObject ) {
        GetDataTypeInfo()->GetRealTypeInfo(externalObject)->
            CollectObjects(objectList, externalObject);
    }
}

void CPointerTypeInfo::WriteData(CObjectOStream& out,
                                 TConstObjectPtr object) const
{
    TConstObjectPtr externalObject = GetObject(object);
    _TRACE("CPointerTypeInfo::WriteData: " << unsigned(externalObject));
    out.WritePointer(externalObject,
                     GetDataTypeInfo()->GetRealTypeInfo(externalObject));
}

void CPointerTypeInfo::ReadData(CObjectIStream& in, TObjectPtr object) const
{
    _TRACE("CPointerTypeInfo::ReadData: " << unsigned(GetObject(object)));
    GetObject(object) = in.ReadPointer(GetDataTypeInfo());
}

END_NCBI_SCOPE
