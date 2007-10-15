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
#include <serial/impl/choiceptr.hpp>
#include <serial/impl/typeref.hpp>
#include <serial/impl/classinfo.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>
#include <serial/impl/typemap.hpp>
#include <serial/impl/ptrinfo.hpp>
#include <serial/impl/typeinfoimpl.hpp>
#include <serial/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Serial_TypeInfo

BEGIN_NCBI_SCOPE

CChoicePointerTypeInfo::CChoicePointerTypeInfo(TTypeInfo pointerType)
    : CParent(pointerType->GetSize(),
              "",
              TConstObjectPtr(0), &CVoidTypeFunctions::Create, typeid(bool),
              &GetPtrIndex, &SetPtrIndex, &ResetPtrIndex)
{
    SetPointerType(pointerType);
}

static CTypeInfoMap s_ChoicePointerTypeInfo_map;

TTypeInfo CChoicePointerTypeInfo::GetTypeInfo(TTypeInfo base)
{
    return s_ChoicePointerTypeInfo_map.GetTypeInfo(base, &CreateTypeInfo);
}

CTypeInfo* CChoicePointerTypeInfo::CreateTypeInfo(TTypeInfo base)
{
    return new CChoicePointerTypeInfo(base);
}

void CChoicePointerTypeInfo::SetPointerType(TTypeInfo base)
{
    m_NullPointerIndex = kEmptyChoice;

    if ( base->GetTypeFamily() != eTypeFamilyPointer )
        NCBI_THROW(CSerialException,eInvalidData,
                     "invalid argument: must be CPointerTypeInfo");
    const CPointerTypeInfo* ptrType =
        CTypeConverter<CPointerTypeInfo>::SafeCast(base);
    m_PointerTypeInfo = ptrType;

    if ( ptrType->GetPointedType()->GetTypeFamily() != eTypeFamilyClass )
        NCBI_THROW(CSerialException,eInvalidData,
                     "invalid argument: data must be CClassTypeInfo");
    const CClassTypeInfo* classType =
        CTypeConverter<CClassTypeInfo>::SafeCast(ptrType->GetPointedType());
    /* Do we really need it to be CObject???
    if ( !classType->IsCObject() )
        NCBI_THROW(CSerialException,eInvalidData,
                     "invalid argument:: choice ptr type must be CObject");
    */
    const CClassTypeInfo::TSubClasses* subclasses =
        classType->SubClasses();
    if ( !subclasses )
        return;

    TTypeInfo nullTypeInfo = CNullTypeInfo::GetTypeInfo();

    for ( CClassTypeInfo::TSubClasses::const_iterator i = subclasses->begin();
          i != subclasses->end(); ++i ) {
        TTypeInfo variantType = i->second.Get();
        if ( !variantType ) {
            // null
            variantType = nullTypeInfo;
        }
        AddVariant(i->first, 0, variantType)->SetSubClass();
        TMemberIndex index = GetVariants().LastIndex();
        if ( variantType == nullTypeInfo ) {
            if ( m_NullPointerIndex == kEmptyChoice )
                m_NullPointerIndex = index;
            else {
                ERR_POST_X(1, "double null");
            }
        }
        else {
            const type_info* id = &CTypeConverter<CClassTypeInfo>::SafeCast(variantType)->GetId();
            if ( !m_VariantsByType.insert(TVariantsByType::value_type(id, index)).second ) {
                NCBI_THROW(CSerialException,eInvalidData,
                           "conflict subclasses: "+variantType->GetName());
            }
        }
    }
}

TMemberIndex
CChoicePointerTypeInfo::GetPtrIndex(const CChoiceTypeInfo* choiceType,
                                    TConstObjectPtr choicePtr)
{
    const CChoicePointerTypeInfo* choicePtrType = 
        CTypeConverter<CChoicePointerTypeInfo>::SafeCast(choiceType);

    const CPointerTypeInfo* ptrType = choicePtrType->m_PointerTypeInfo;
    TConstObjectPtr classPtr = ptrType->GetObjectPointer(choicePtr);
    if ( !classPtr )
        return choicePtrType->m_NullPointerIndex;
    const CClassTypeInfo* classType =
        CTypeConverter<CClassTypeInfo>::SafeCast(ptrType->GetPointedType());
    const TVariantsByType& variants = choicePtrType->m_VariantsByType;
    TVariantsByType::const_iterator v =
        variants.find(classType->GetCPlusPlusTypeInfo(classPtr));
    if ( v == variants.end() )
        NCBI_THROW(CSerialException,eInvalidData,
                   "incompatible CChoicePointerTypeInfo type");
    return v->second;
}

void CChoicePointerTypeInfo::ResetPtrIndex(const CChoiceTypeInfo* choiceType,
                                           TObjectPtr choicePtr)
{
    const CChoicePointerTypeInfo* choicePtrType = 
        CTypeConverter<CChoicePointerTypeInfo>::SafeCast(choiceType);

    const CPointerTypeInfo* ptrType = choicePtrType->m_PointerTypeInfo;
    ptrType->SetObjectPointer(choicePtr, 0);
}

void CChoicePointerTypeInfo::SetPtrIndex(const CChoiceTypeInfo* choiceType,
                                         TObjectPtr choicePtr,
                                         TMemberIndex index,
                                         CObjectMemoryPool* memPool)
{
    const CChoicePointerTypeInfo* choicePtrType = 
        CTypeConverter<CChoicePointerTypeInfo>::SafeCast(choiceType);

    const CPointerTypeInfo* ptrType = choicePtrType->m_PointerTypeInfo;
    _ASSERT(!ptrType->GetObjectPointer(choicePtr));
    const CVariantInfo* variantInfo = choicePtrType->GetVariantInfo(index);
    ptrType->SetObjectPointer(choicePtr,
                              variantInfo->GetTypeInfo()->Create(memPool));
}

class CNullFunctions
{
public:
    static TObjectPtr Create(TTypeInfo /*typeInfo*/,
                             CObjectMemoryPool* /*memoryPool*/)
        {
            return 0;
        }
    static void Read(CObjectIStream& in, TTypeInfo ,
                     TObjectPtr objectPtr)
        {
            if ( objectPtr != 0 ) {
                in.ThrowError(in.fInvalidData,
                    "non-null value when reading NULL member");
            }
            in.ReadNull();
        }
    static void Write(CObjectOStream& out, TTypeInfo ,
                      TConstObjectPtr objectPtr)
        {
            if ( objectPtr != 0 ) {
                out.ThrowError(out.fInvalidData,
                    "non-null value when writing NULL member");
            }
            out.WriteNull();
        }
    static void Copy(CObjectStreamCopier& copier, TTypeInfo )
        {
            copier.In().ReadNull();
            copier.Out().WriteNull();
        }
    static void Skip(CObjectIStream& in, TTypeInfo )
        {
            in.SkipNull();
        }
};

CNullTypeInfo::CNullTypeInfo(void)
{
    SetCreateFunction(&CNullFunctions::Create);
    SetReadFunction(&CNullFunctions::Read);
    SetWriteFunction(&CNullFunctions::Write);
    SetCopyFunction(&CNullFunctions::Copy);
    SetSkipFunction(&CNullFunctions::Skip);
}

TTypeInfo CNullTypeInfo::GetTypeInfo(void)
{
    TTypeInfo typeInfo = new CNullTypeInfo();
    return typeInfo;
}

END_NCBI_SCOPE
