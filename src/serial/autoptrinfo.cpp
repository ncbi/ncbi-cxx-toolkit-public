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
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <serial/impl/autoptrinfo.hpp>
#include <serial/impl/typemap.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>
#include <serial/serialutil.hpp>

BEGIN_NCBI_SCOPE


static CSafeStaticPtr<CTypeInfoMap> s_AutoPointerTypeInfo_map;

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
    return s_AutoPointerTypeInfo_map->GetTypeInfo(base, &CreateTypeInfo);
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
        out.ThrowError(out.fIllegalCall,"auto pointers have different type");
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
        in.ThrowError(in.fIllegalCall,"auto pointers have different type");
    }
    in.ReadObject(dataPtr, dataType);
}

void CAutoPointerTypeInfo::CopyAutoPtr(CObjectStreamCopier& copier,
                                       TTypeInfo objectType)
{
    const CAutoPointerTypeInfo* autoPtrType =
        CTypeConverter<CAutoPointerTypeInfo>::SafeCast(objectType);

    if (!copier.CopyNullPointer()) {
        autoPtrType->GetPointedType()->CopyData(copier);
    }
}

void CAutoPointerTypeInfo::SkipAutoPtr(CObjectIStream& in,
                                       TTypeInfo objectType)
{
    const CAutoPointerTypeInfo* autoPtrType =
        CTypeConverter<CAutoPointerTypeInfo>::SafeCast(objectType);

    autoPtrType->GetPointedType()->SkipData(in);
}

END_NCBI_SCOPE
