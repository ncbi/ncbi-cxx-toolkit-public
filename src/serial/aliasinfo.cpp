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
* Author: Aleksey Grichenko
*
* File Description:
*   Alias type info
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2003/10/21 13:45:23  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#include <serial/aliasinfo.hpp>
#include <serial/serialbase.hpp>
#include <serial/serialutil.hpp>

BEGIN_NCBI_SCOPE


class CAliasTypeInfoFunctions
{
public:
    static void ReadAliasDefault(CObjectIStream& in,
                                 TTypeInfo objectType,
                                 TObjectPtr objectPtr);
    static void WriteAliasDefault(CObjectOStream& out,
                                  TTypeInfo objectType,
                                  TConstObjectPtr objectPtr);
    static void SkipAliasDefault(CObjectIStream& in,
                                 TTypeInfo objectType);
    static void CopyAliasDefault(CObjectStreamCopier& copier,
                                 TTypeInfo objectType);
};

typedef CAliasTypeInfoFunctions TFunc;


CAliasTypeInfo::CAliasTypeInfo(const string& name, TTypeInfo type)
    : CParent(type->GetTypeFamily(), type->GetSize(), name),
      m_DataTypeRef(type), m_DataOffset(0)
{
    InitAliasTypeInfoFunctions();
}


void CAliasTypeInfo::InitAliasTypeInfoFunctions(void)
{
    SetReadFunction(&TFunc::ReadAliasDefault);
    SetWriteFunction(&TFunc::WriteAliasDefault);
    SetCopyFunction(&TFunc::CopyAliasDefault);
    SetSkipFunction(&TFunc::SkipAliasDefault);
}


TTypeInfo CAliasTypeInfo::GetReferencedType(void) const
{
    return m_DataTypeRef.Get();
}


const string& CAliasTypeInfo::GetModuleName(void) const
{
    return m_DataTypeRef.Get()->GetModuleName();
}


bool CAliasTypeInfo::MayContainType(TTypeInfo type) const
{
    return m_DataTypeRef.Get()->MayContainType(type);
}


bool CAliasTypeInfo::IsDefault(TConstObjectPtr object) const
{
    return m_DataTypeRef.Get()->IsDefault(object);
}


bool CAliasTypeInfo::Equals(TConstObjectPtr object1,
                            TConstObjectPtr object2) const
{
    return m_DataTypeRef.Get()->Equals(object1, object2);
}


void CAliasTypeInfo::SetDefault(TObjectPtr dst) const
{
    m_DataTypeRef.Get()->SetDefault(dst);
}


void CAliasTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
    m_DataTypeRef.Get()->Assign(dst, src);
}


void CAliasTypeInfo::Delete(TObjectPtr object) const
{
    m_DataTypeRef.Get()->Delete(object);
}


void CAliasTypeInfo::DeleteExternalObjects(TObjectPtr object) const
{
    m_DataTypeRef.Get()->DeleteExternalObjects(object);
}


const CObject* CAliasTypeInfo::GetCObjectPtr(TConstObjectPtr objectPtr) const
{
    return m_DataTypeRef.Get()->GetCObjectPtr(objectPtr);
}


TTypeInfo CAliasTypeInfo::GetRealTypeInfo(TConstObjectPtr object) const
{
    return m_DataTypeRef.Get()->GetRealTypeInfo(object);
}


bool CAliasTypeInfo::IsParentClassOf(const CClassTypeInfo* classInfo) const
{
    return m_DataTypeRef.Get()->IsParentClassOf(classInfo);
}


bool CAliasTypeInfo::IsType(TTypeInfo type) const
{
    return m_DataTypeRef.Get()->IsType(type);
}


void CAliasTypeInfo::SetDataOffset(TPointerOffsetType offset)
{
    m_DataOffset = offset;
}


TObjectPtr CAliasTypeInfo::GetDataPtr(TObjectPtr objectPtr) const
{
    return static_cast<char*>(objectPtr) + m_DataOffset;
}


TConstObjectPtr CAliasTypeInfo::GetDataPtr(TConstObjectPtr objectPtr) const
{
    return static_cast<const char*>(objectPtr) + m_DataOffset;
}


void CAliasTypeInfoFunctions::ReadAliasDefault(CObjectIStream& in,
                                               TTypeInfo objectType,
                                               TObjectPtr objectPtr)
{
    const CAliasTypeInfo* aliasType =
        CTypeConverter<CAliasTypeInfo>::SafeCast(objectType);
    objectPtr = aliasType->GetDataPtr(objectPtr);
    aliasType->m_DataTypeRef.Get()->DefaultReadData(in, objectPtr);
}

void CAliasTypeInfoFunctions::WriteAliasDefault(CObjectOStream& out,
                                                TTypeInfo objectType,
                                                TConstObjectPtr objectPtr)
{
    const CAliasTypeInfo* aliasType =
        CTypeConverter<CAliasTypeInfo>::SafeCast(objectType);
    objectPtr = aliasType->GetDataPtr(objectPtr);
    aliasType->m_DataTypeRef.Get()->DefaultWriteData(out, objectPtr);
}

void CAliasTypeInfoFunctions::CopyAliasDefault(CObjectStreamCopier& copier,
                                               TTypeInfo objectType)
{
    const CAliasTypeInfo* aliasType =
        CTypeConverter<CAliasTypeInfo>::SafeCast(objectType);
    aliasType->m_DataTypeRef.Get()->DefaultCopyData(copier);
}

void CAliasTypeInfoFunctions::SkipAliasDefault(CObjectIStream& in,
                                               TTypeInfo objectType)
{
    const CAliasTypeInfo* aliasType =
        CTypeConverter<CAliasTypeInfo>::SafeCast(objectType);
    aliasType->m_DataTypeRef.Get()->DefaultSkipData(in);
}


END_NCBI_SCOPE
