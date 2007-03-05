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
#include <serial/impl/stltypesimpl.hpp>
#include <serial/serialimpl.hpp>
#include <serial/exception.hpp>
#include <serial/impl/classinfo.hpp>
#include <serial/impl/classinfohelper.hpp>
#include <serial/impl/typemap.hpp>
#include <corelib/ncbi_safe_static.hpp>


BEGIN_NCBI_SCOPE

static CSafeStaticPtr<CTypeInfoMap> s_TypeMap_auto_ptr;
static CSafeStaticPtr<CTypeInfoMap> s_TypeMap_CRef;
static CSafeStaticPtr<CTypeInfoMap> s_TypeMap_CConstRef;
static CSafeStaticPtr<CTypeInfoMap> s_TypeMap_AutoPtr;
static CSafeStaticPtr<CTypeInfoMap> s_TypeMap_list;
static CSafeStaticPtr<CTypeInfoMap> s_TypeMapSet_list;
static CSafeStaticPtr<CTypeInfoMap> s_TypeMap_vector;
static CSafeStaticPtr<CTypeInfoMap> s_TypeMapSet_vector;
static CSafeStaticPtr<CTypeInfoMap> s_TypeMap_set;
static CSafeStaticPtr<CTypeInfoMap> s_TypeMap_multiset;

TTypeInfo CStlClassInfoUtil::Get_auto_ptr(TTypeInfo arg, TTypeInfoCreator1 f)
{
    return s_TypeMap_auto_ptr->GetTypeInfo(arg, f);
}

TTypeInfo CStlClassInfoUtil::Get_CRef(TTypeInfo arg, TTypeInfoCreator1 f)
{
    return s_TypeMap_CRef->GetTypeInfo(arg, f);
}

TTypeInfo CStlClassInfoUtil::Get_CConstRef(TTypeInfo arg, TTypeInfoCreator1 f)
{
    return s_TypeMap_CConstRef->GetTypeInfo(arg, f);
}

TTypeInfo CStlClassInfoUtil::Get_AutoPtr(TTypeInfo arg, TTypeInfoCreator1 f)
{
    return s_TypeMap_AutoPtr->GetTypeInfo(arg, f);
}

TTypeInfo CStlClassInfoUtil::Get_list(TTypeInfo arg, TTypeInfoCreator1 f)
{
    return s_TypeMap_list->GetTypeInfo(arg, f);
}

TTypeInfo CStlClassInfoUtil::GetSet_list(TTypeInfo arg, TTypeInfoCreator1 f)
{
    return s_TypeMapSet_list->GetTypeInfo(arg, f);
}

TTypeInfo CStlClassInfoUtil::Get_vector(TTypeInfo arg, TTypeInfoCreator1 f)
{
    return s_TypeMap_vector->GetTypeInfo(arg, f);
}

TTypeInfo CStlClassInfoUtil::GetSet_vector(TTypeInfo arg, TTypeInfoCreator1 f)
{
    return s_TypeMapSet_vector->GetTypeInfo(arg, f);
}

TTypeInfo CStlClassInfoUtil::Get_set(TTypeInfo arg, TTypeInfoCreator1 f)
{
    return s_TypeMap_set->GetTypeInfo(arg, f);
}

TTypeInfo CStlClassInfoUtil::Get_multiset(TTypeInfo arg, TTypeInfoCreator1 f)
{
    return s_TypeMap_multiset->GetTypeInfo(arg, f);
}

TTypeInfo CStlClassInfoUtil::Get_map(TTypeInfo arg1, TTypeInfo arg2,
                                     TTypeInfoCreator2 f)
{
    return f(arg1, arg2);
}

TTypeInfo CStlClassInfoUtil::Get_multimap(TTypeInfo arg1, TTypeInfo arg2,
                                          TTypeInfoCreator2 f)
{
    return f(arg1, arg2);
}

TTypeInfo CStlClassInfoUtil::GetInfo(TTypeInfo& storage,
                                     TTypeInfo arg, TTypeInfoCreator1 f)
{
    if ( !storage ) {
        CMutexGuard guard(GetTypeInfoMutex());
        if ( !storage ) {
            storage = f(arg);
        }
    }
    return storage;
}

TTypeInfo CStlClassInfoUtil::GetInfo(TTypeInfo& storage,
                                     TTypeInfo arg1, TTypeInfo arg2,
                                     TTypeInfoCreator2 f)
{
    if ( !storage ) {
        CMutexGuard guard(GetTypeInfoMutex());
        if ( !storage ) {
            storage = f(arg1, arg2);
        }
    }
    return storage;
}

void CStlClassInfoUtil::CannotGetElementOfSet(void)
{
    NCBI_THROW(CSerialException,eFail, "cannot get pointer to element of set");
}

void CStlClassInfoUtil::ThrowDuplicateElementError(void)
{
    NCBI_THROW(CSerialException,eFail, "duplicate element of unique container");
}

CStlOneArgTemplate::CStlOneArgTemplate(size_t size,
                                       TTypeInfo type, bool randomOrder,
                                       const string& name)
    : CParent(size, name, type, randomOrder)
{
}

CStlOneArgTemplate::CStlOneArgTemplate(size_t size,
                                       TTypeInfo type, bool randomOrder)
    : CParent(size, type, randomOrder)
{
}

CStlOneArgTemplate::CStlOneArgTemplate(size_t size,
                                       const CTypeRef& type, bool randomOrder)
    : CParent(size, type, randomOrder)
{
}

void CStlOneArgTemplate::SetDataId(const CMemberId& id)
{
    m_DataId = id;
}

bool CStlOneArgTemplate::IsDefault(TConstObjectPtr objectPtr) const
{
    return m_IsDefault(objectPtr);
}

void CStlOneArgTemplate::SetDefault(TObjectPtr objectPtr) const
{
    m_SetDefault(objectPtr);
}

void CStlOneArgTemplate::SetMemFunctions(TTypeCreate create,
                                         TIsDefaultFunction isDefault,
                                         TSetDefaultFunction setDefault)
{
    SetCreateFunction(create);
    m_IsDefault = isDefault;
    m_SetDefault = setDefault;
}

CStlTwoArgsTemplate::CStlTwoArgsTemplate(size_t size,
                                         TTypeInfo keyType,
                                         TPointerOffsetType keyOffset,
                                         TTypeInfo valueType,
                                         TPointerOffsetType valueOffset,
                                         bool randomOrder)
    : CParent(size, CTypeRef(&CreateElementTypeInfo, this), randomOrder),
      m_KeyType(keyType), m_KeyOffset(keyOffset),
      m_ValueType(valueType), m_ValueOffset(valueOffset)
{
}

CStlTwoArgsTemplate::CStlTwoArgsTemplate(size_t size,
                                         const CTypeRef& keyType,
                                         TPointerOffsetType keyOffset,
                                         const CTypeRef& valueType,
                                         TPointerOffsetType valueOffset,
                                         bool randomOrder)
    : CParent(size, CTypeRef(&CreateElementTypeInfo, this), randomOrder),
      m_KeyType(keyType), m_KeyOffset(keyOffset),
      m_ValueType(valueType), m_ValueOffset(valueOffset)
{
}

void CStlTwoArgsTemplate::SetKeyId(const CMemberId& id)
{
    m_KeyId = id;
}

void CStlTwoArgsTemplate::SetValueId(const CMemberId& id)
{
    m_ValueId = id;
}

TTypeInfo CStlTwoArgsTemplate::CreateElementTypeInfo(TTypeInfo argType)
{
    const CStlTwoArgsTemplate* mapType = 
        CTypeConverter<CStlTwoArgsTemplate>::SafeCast(argType);
    CClassTypeInfo* classInfo =
        CClassInfoHelper<bool>::CreateAbstractClassInfo("");
    classInfo->SetRandomOrder(false);
    classInfo->AddMember(mapType->GetKeyId(),
                         TConstObjectPtr(mapType->m_KeyOffset),
                         mapType->m_KeyType.Get());
    classInfo->AddMember(mapType->GetValueId(),
                         TConstObjectPtr(mapType->m_ValueOffset),
                         mapType->m_ValueType.Get());
    return classInfo;
}

END_NCBI_SCOPE
