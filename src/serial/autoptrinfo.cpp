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
* Revision 1.19  2004/05/17 21:03:02  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.18  2003/05/16 18:02:18  gouriano
* revised exception error messages
*
* Revision 1.17  2002/10/25 14:49:27  vasilche
* NCBI C Toolkit compatibility code extracted to libxcser library.
* Serial streams flags names were renamed to fXxx.
*
* Names of flags
*
* Revision 1.16  2001/05/17 15:07:04  lavr
* Typos corrected
*
* Revision 1.15  2000/11/07 17:25:40  vasilche
* Fixed encoding of XML:
*     removed unnecessary apostrophes in OCTET STRING
*     removed unnecessary content in NULL
* Added module names to CTypeInfo and CEnumeratedTypeValues
*
* Revision 1.14  2000/10/20 15:51:36  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.13  2000/10/17 18:45:32  vasilche
* Added possibility to turn off object cross reference detection in
* CObjectIStream and CObjectOStream.
*
* Revision 1.12  2000/10/13 16:28:38  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* Revision 1.11  2000/09/18 20:00:20  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.10  2000/09/01 13:16:13  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.9  2000/08/15 19:44:46  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.8  2000/05/24 20:08:46  vasilche
* Implemented XML dump.
*
* Revision 1.7  2000/03/29 15:55:26  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.6  2000/03/10 21:47:50  vasilche
* AutoPointer should write/read data inline.
*
* Revision 1.5  2000/03/07 14:06:21  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
* Added generation of reference counted objects.
*
* Revision 1.4  1999/12/28 18:55:49  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.3  1999/12/17 19:05:01  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.2  1999/09/14 18:54:15  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.1  1999/09/07 20:57:57  vasilche
* Forgot to add some files.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <serial/autoptrinfo.hpp>
#include <serial/typemap.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>
#include <serial/serialutil.hpp>

BEGIN_NCBI_SCOPE

static CTypeInfoMap s_AutoPointerTypeInfo_map;

CAutoPointerTypeInfo::CAutoPointerTypeInfo(TTypeInfo type)
    : CParent(type->GetName(), type)
{
    SetReadFunction(&ReadAutoPtr);
    SetWriteFunction(&WriteAutoPtr);
    SetCopyFunction(&CopyAutoPtr);
    SetSkipFunction(&SkipAutoPtr);
}

const string& CAutoPointerTypeInfo::GetModuleName(void) const
{
    return GetPointedType()->GetModuleName();
}

TTypeInfo CAutoPointerTypeInfo::GetTypeInfo(TTypeInfo base)
{
    return s_AutoPointerTypeInfo_map.GetTypeInfo(base, &CreateTypeInfo);
}

CTypeInfo* CAutoPointerTypeInfo::CreateTypeInfo(TTypeInfo base)
{
    return new CAutoPointerTypeInfo(base);
}

void CAutoPointerTypeInfo::WriteAutoPtr(CObjectOStream& out,
                                        TTypeInfo objectType,
                                        TConstObjectPtr objectPtr)
{
    const CAutoPointerTypeInfo* autoPtrType =
        CTypeConverter<CAutoPointerTypeInfo>::SafeCast(objectType);

    TConstObjectPtr dataPtr = autoPtrType->GetObjectPointer(objectPtr);
    if ( dataPtr == 0 )
        out.ThrowError(out.fIllegalCall, "null auto pointer");

    TTypeInfo dataType = autoPtrType->GetPointedType();
    if ( dataType->GetRealTypeInfo(dataPtr) != dataType )
        out.ThrowError(out.fIllegalCall,"auto pointer have different type");
    out.WriteObject(dataPtr, dataType);
}

void CAutoPointerTypeInfo::ReadAutoPtr(CObjectIStream& in,
                                       TTypeInfo objectType,
                                       TObjectPtr objectPtr)
{
    const CAutoPointerTypeInfo* autoPtrType =
        CTypeConverter<CAutoPointerTypeInfo>::SafeCast(objectType);

    TObjectPtr dataPtr = autoPtrType->GetObjectPointer(objectPtr);
    TTypeInfo dataType = autoPtrType->GetPointedType();
    if ( dataPtr == 0 ) {
        autoPtrType->SetObjectPointer(objectPtr, dataPtr = dataType->Create());
    }
    else if ( dataType->GetRealTypeInfo(dataPtr) != dataType ) {
        in.ThrowError(in.fIllegalCall,"auto pointer have different type");
    }
    in.ReadObject(dataPtr, dataType);
}

void CAutoPointerTypeInfo::CopyAutoPtr(CObjectStreamCopier& copier,
                                       TTypeInfo objectType)
{
    const CAutoPointerTypeInfo* autoPtrType =
        CTypeConverter<CAutoPointerTypeInfo>::SafeCast(objectType);

    autoPtrType->GetPointedType()->CopyData(copier);
}

void CAutoPointerTypeInfo::SkipAutoPtr(CObjectIStream& in,
                                       TTypeInfo objectType)
{
    const CAutoPointerTypeInfo* autoPtrType =
        CTypeConverter<CAutoPointerTypeInfo>::SafeCast(objectType);

    autoPtrType->GetPointedType()->SkipData(in);
}

END_NCBI_SCOPE
