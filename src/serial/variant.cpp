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
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>

#include <serial/variant.hpp>
#include <serial/objectinfo.hpp>
#include <serial/objectiter.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>
#include <serial/delaybuf.hpp>
#include <serial/choiceptr.hpp>
#include <serial/ptrinfo.hpp>
#include <serial/serialimpl.hpp>

BEGIN_NCBI_SCOPE

class CVariantInfoFunctions
{
public:

    static
    TConstObjectPtr GetConstInlineVariant(const CVariantInfo* variantInfo,
                                          TConstObjectPtr choicePtr);
    static
    TConstObjectPtr GetConstPointerVariant(const CVariantInfo* variantInfo,
                                           TConstObjectPtr choicePtr);
    static
    TConstObjectPtr GetConstDelayedVariant(const CVariantInfo* variantInfo,
                                           TConstObjectPtr choicePtr);
    static
    TConstObjectPtr GetConstSubclassVariant(const CVariantInfo* variantInfo,
                                            TConstObjectPtr choicePtr);
    static TObjectPtr GetInlineVariant(const CVariantInfo* variantInfo,
                                       TObjectPtr choicePtr);
    static TObjectPtr GetPointerVariant(const CVariantInfo* variantInfo,
                                        TObjectPtr choicePtr);
    static TObjectPtr GetDelayedVariant(const CVariantInfo* variantInfo,
                                        TObjectPtr choicePtr);
    static TObjectPtr GetSubclassVariant(const CVariantInfo* variantInfo,
                                         TObjectPtr choicePtr);

    static void ReadInlineVariant(CObjectIStream& in,
                                  const CVariantInfo* variantInfo,
                                  TObjectPtr choicePtr);
    static void ReadPointerVariant(CObjectIStream& in,
                                   const CVariantInfo* variantInfo,
                                   TObjectPtr choicePtr);
    static void ReadObjectPointerVariant(CObjectIStream& in,
                                         const CVariantInfo* variantInfo,
                                         TObjectPtr choicePtr);
    static void ReadDelayedVariant(CObjectIStream& in,
                                   const CVariantInfo* variantInfo,
                                   TObjectPtr choicePtr);
    static void ReadSubclassVariant(CObjectIStream& in,
                                    const CVariantInfo* variantInfo,
                                    TObjectPtr choicePtr);
    static void ReadHookedVariant(CObjectIStream& in,
                                  const CVariantInfo* variantInfo,
                                  TObjectPtr choicePtr);
    static void WriteInlineVariant(CObjectOStream& out,
                                   const CVariantInfo* variantInfo,
                                   TConstObjectPtr choicePtr);
    static void WritePointerVariant(CObjectOStream& out,
                                    const CVariantInfo* variantInfo,
                                    TConstObjectPtr choicePtr);
    static void WriteObjectPointerVariant(CObjectOStream& out,
                                          const CVariantInfo* variantInfo,
                                          TConstObjectPtr choicePtr);
    static void WriteSubclassVariant(CObjectOStream& out,
                                     const CVariantInfo* variantInfo,
                                     TConstObjectPtr choicePtr);
    static void WriteDelayedVariant(CObjectOStream& out,
                                    const CVariantInfo* variantInfo,
                                    TConstObjectPtr choicePtr);
    static void WriteHookedVariant(CObjectOStream& out,
                                   const CVariantInfo* variantInfo,
                                   TConstObjectPtr choicePtr);
    static void SkipNonObjectVariant(CObjectIStream& in,
                                     const CVariantInfo* variantInfo);
    static void SkipObjectPointerVariant(CObjectIStream& in,
                                         const CVariantInfo* variantInfo);
    static void SkipHookedVariant(CObjectIStream& in,
                                  const CVariantInfo* variantInfo);
    static void CopyNonObjectVariant(CObjectStreamCopier& copier,
                                     const CVariantInfo* variantInfo);
    static void CopyObjectPointerVariant(CObjectStreamCopier& copier,
                                         const CVariantInfo* variantInfo);
    static void CopyHookedVariant(CObjectStreamCopier& copier,
                                  const CVariantInfo* variantInfo);
};

typedef CVariantInfoFunctions TFunc;

CVariantInfo::CVariantInfo(const CChoiceTypeInfo* choiceType,
                           const CMemberId& id, TPointerOffsetType offset,
                           const CTypeRef& type)
    : CParent(id, offset, type), m_ChoiceType(choiceType),
      m_VariantType(eInlineVariant), m_DelayOffset(eNoOffset),
      m_GetConstFunction(&TFunc::GetConstInlineVariant),
      m_GetFunction(&TFunc::GetInlineVariant),
      m_ReadHookData(&TFunc::ReadInlineVariant, &TFunc::ReadHookedVariant),
      m_WriteHookData(&TFunc::WriteInlineVariant, &TFunc::WriteHookedVariant),
      m_SkipHookData(&TFunc::SkipNonObjectVariant, &TFunc::SkipHookedVariant),
      m_CopyHookData(&TFunc::CopyNonObjectVariant, &TFunc::CopyHookedVariant)
{
}

CVariantInfo::CVariantInfo(const CChoiceTypeInfo* choiceType,
                           const CMemberId& id, TPointerOffsetType offset,
                           TTypeInfo type)
    : CParent(id, offset, type), m_ChoiceType(choiceType),
      m_VariantType(eInlineVariant), m_DelayOffset(eNoOffset),
      m_GetConstFunction(&TFunc::GetConstInlineVariant),
      m_GetFunction(&TFunc::GetInlineVariant),
      m_ReadHookData(&TFunc::ReadInlineVariant, &TFunc::ReadHookedVariant),
      m_WriteHookData(&TFunc::WriteInlineVariant, &TFunc::WriteHookedVariant),
      m_SkipHookData(&TFunc::SkipNonObjectVariant, &TFunc::SkipHookedVariant),
      m_CopyHookData(&TFunc::CopyNonObjectVariant, &TFunc::CopyHookedVariant)
{
}

CVariantInfo::CVariantInfo(const CChoiceTypeInfo* choiceType,
                           const char* id, TPointerOffsetType offset,
                           const CTypeRef& type)
    : CParent(id, offset, type), m_ChoiceType(choiceType),
      m_VariantType(eInlineVariant), m_DelayOffset(eNoOffset),
      m_GetConstFunction(&TFunc::GetConstInlineVariant),
      m_GetFunction(&TFunc::GetInlineVariant),
      m_ReadHookData(&TFunc::ReadInlineVariant, &TFunc::ReadHookedVariant),
      m_WriteHookData(&TFunc::WriteInlineVariant, &TFunc::WriteHookedVariant),
      m_SkipHookData(&TFunc::SkipNonObjectVariant, &TFunc::SkipHookedVariant),
      m_CopyHookData(&TFunc::CopyNonObjectVariant, &TFunc::CopyHookedVariant)
{
}

CVariantInfo::CVariantInfo(const CChoiceTypeInfo* choiceType,
                           const char* id, TPointerOffsetType offset,
                           TTypeInfo type)
    : CParent(id, offset, type), m_ChoiceType(choiceType),
      m_VariantType(eInlineVariant), m_DelayOffset(eNoOffset),
      m_GetConstFunction(&TFunc::GetConstInlineVariant),
      m_GetFunction(&TFunc::GetInlineVariant),
      m_ReadHookData(&TFunc::ReadInlineVariant, &TFunc::ReadHookedVariant),
      m_WriteHookData(&TFunc::WriteInlineVariant, &TFunc::WriteHookedVariant),
      m_SkipHookData(&TFunc::SkipNonObjectVariant, &TFunc::SkipHookedVariant),
      m_CopyHookData(&TFunc::CopyNonObjectVariant, &TFunc::CopyHookedVariant)
{
}

CVariantInfo* CVariantInfo::SetPointer(void)
{
    if ( !IsInline() ) {
        NCBI_THROW(CSerialException,eIllegalCall,
                   "SetPointer() is not first call");
    }
    m_VariantType = eNonObjectPointerVariant;
    UpdateFunctions();
    return this;
}

CVariantInfo* CVariantInfo::SetObjectPointer(void)
{
    if ( !IsInline() ) {
        NCBI_THROW(CSerialException,eIllegalCall,
                   "SetObjectPointer() is not first call");
    }
    m_VariantType = eObjectPointerVariant;
    UpdateFunctions();
    return this;
}

CVariantInfo* CVariantInfo::SetSubClass(void)
{
    if ( !IsInline() ) {
        NCBI_THROW(CSerialException,eIllegalCall,
                   "SetSubClass() is not first call");
    }
    if ( CanBeDelayed() ) {
        NCBI_THROW(CSerialException,eIllegalCall,
                  "sub class cannot be delayed");
    }
    m_VariantType = eSubClassVariant;
    UpdateFunctions();
    return this;
}

bool NCBI_XSERIAL_EXPORT EnabledDelayBuffers(void);

CVariantInfo* CVariantInfo::SetDelayBuffer(CDelayBuffer* buffer)
{
    if ( IsSubClass() ) {
        NCBI_THROW(CSerialException,eIllegalCall,
                   "sub class cannot be delayed");
    }
    if ( EnabledDelayBuffers() ) {
        m_DelayOffset = TPointerOffsetType(buffer);
        UpdateFunctions();
    }
    return this;
}

void CVariantInfo::UpdateFunctions(void)
{
    // determine function pointers
    TVariantGetConst getConstFunc;
    TVariantGet getFunc;
    TVariantReadFunction readFunc;
    TVariantWriteFunction writeFunc;
    TVariantSkipFunction skipFunc;
    TVariantCopyFunction copyFunc;

    // read/write/get
    if ( CanBeDelayed() ) {
        _ASSERT(!IsSubClass());
        getConstFunc = &TFunc::GetConstDelayedVariant;
        getFunc = &TFunc::GetDelayedVariant;
        readFunc = &TFunc::ReadDelayedVariant;
        writeFunc = &TFunc::WriteDelayedVariant;
    }
    else if ( IsInline() ) {
        getConstFunc = &TFunc::GetConstInlineVariant;
        getFunc = &TFunc::GetInlineVariant;
        readFunc = &TFunc::ReadInlineVariant;
        writeFunc = &TFunc::WriteInlineVariant;
    }
    else if ( IsObjectPointer() ) {
        getConstFunc = &TFunc::GetConstPointerVariant;
        getFunc = &TFunc::GetPointerVariant;
        readFunc = &TFunc::ReadObjectPointerVariant;
        writeFunc = &TFunc::WriteObjectPointerVariant;
    }
    else if ( IsNonObjectPointer() ) {
        getConstFunc = &TFunc::GetConstPointerVariant;
        getFunc = &TFunc::GetPointerVariant;
        readFunc = &TFunc::ReadPointerVariant;
        writeFunc = &TFunc::WritePointerVariant;
    }
    else { // subclass
        getConstFunc = &TFunc::GetConstSubclassVariant;
        getFunc = &TFunc::GetSubclassVariant;
        readFunc = &TFunc::ReadSubclassVariant;
        writeFunc = &TFunc::WriteSubclassVariant;
    }

    // copy/skip
    if ( IsObject() ) {
        copyFunc = &TFunc::CopyObjectPointerVariant;
        skipFunc = &TFunc::SkipObjectPointerVariant;
    }
    else {
        copyFunc = &TFunc::CopyNonObjectVariant;
        skipFunc = &TFunc::SkipNonObjectVariant;
    }

    // update function pointers
    m_GetConstFunction = getConstFunc;
    m_GetFunction = getFunc;
    m_ReadHookData.SetDefaultFunction(readFunc);
    m_WriteHookData.SetDefaultFunction(writeFunc);
    m_SkipHookData.SetDefaultFunction(skipFunc);
    m_CopyHookData.SetDefaultFunction(copyFunc);
}

void CVariantInfo::UpdateDelayedBuffer(CObjectIStream& in,
                                       TObjectPtr choicePtr) const
{
    _ASSERT(CanBeDelayed());
    _ASSERT(GetDelayBuffer(choicePtr).GetIndex() == GetIndex());

    TObjectPtr variantPtr = GetItemPtr(choicePtr);
    TTypeInfo variantType = GetTypeInfo();
    if ( IsPointer() ) {
        // create object itself
        variantPtr = CTypeConverter<TObjectPtr>::Get(variantPtr) =
            variantType->Create();
        if ( IsObjectPointer() ) {
            _TRACE("Should check for real pointer type (CRef...)");
            CTypeConverter<CObject>::Get(variantPtr).AddReference();
        }
    }

    BEGIN_OBJECT_FRAME_OF2(in, eFrameChoice, GetChoiceType());
    BEGIN_OBJECT_FRAME_OF2(in, eFrameChoiceVariant, GetId());
    variantType->ReadData(in, variantPtr);
    END_OBJECT_FRAME_OF(in);
    END_OBJECT_FRAME_OF(in);
}

void CVariantInfo::SetReadFunction(TVariantReadFunction func)
{
    m_ReadHookData.SetDefaultFunction(func);
}

void CVariantInfo::SetWriteFunction(TVariantWriteFunction func)
{
    m_WriteHookData.SetDefaultFunction(func);
}

void CVariantInfo::SetCopyFunction(TVariantCopyFunction func)
{
    m_CopyHookData.SetDefaultFunction(func);
}

void CVariantInfo::SetSkipFunction(TVariantSkipFunction func)
{
    m_SkipHookData.SetDefaultFunction(func);
}

TObjectPtr CVariantInfo::CreateChoice(void) const
{
    return GetChoiceType()->Create();
}

TConstObjectPtr
CVariantInfoFunctions::GetConstInlineVariant(const CVariantInfo* variantInfo,
                                             TConstObjectPtr choicePtr)
{
    _ASSERT(!variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->IsInline());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    return variantInfo->GetItemPtr(choicePtr);
}

TConstObjectPtr
CVariantInfoFunctions::GetConstPointerVariant(const CVariantInfo* variantInfo,
                                              TConstObjectPtr choicePtr)
{
    _ASSERT(!variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->IsPointer());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    TConstObjectPtr variantPtr = variantInfo->GetItemPtr(choicePtr);
    variantPtr = CTypeConverter<TConstObjectPtr>::Get(variantPtr);
    _ASSERT(variantPtr);
    return variantPtr;
}

TConstObjectPtr
CVariantInfoFunctions::GetConstDelayedVariant(const CVariantInfo* variantInfo,
                                              TConstObjectPtr choicePtr)
{
    _ASSERT(variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    const_cast<CDelayBuffer&>(variantInfo->GetDelayBuffer(choicePtr)).Update();
    TConstObjectPtr variantPtr = variantInfo->GetItemPtr(choicePtr);
    if ( variantInfo->IsPointer() ) {
        variantPtr = CTypeConverter<TConstObjectPtr>::Get(variantPtr);
        _ASSERT(variantPtr);
    }
    return variantPtr;
}

TConstObjectPtr
CVariantInfoFunctions::GetConstSubclassVariant(const CVariantInfo* variantInfo,
                                               TConstObjectPtr choicePtr)
{
    _ASSERT(variantInfo->IsSubClass());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    const CChoiceTypeInfo* choiceType = variantInfo->GetChoiceType();
    const CChoicePointerTypeInfo* choicePtrType =
        CTypeConverter<CChoicePointerTypeInfo>::SafeCast(choiceType);
    TConstObjectPtr variantPtr =
        choicePtrType->GetPointerTypeInfo()->GetObjectPointer(choicePtr);
    _ASSERT(variantPtr);
    return variantPtr;
}

TObjectPtr
CVariantInfoFunctions::GetInlineVariant(const CVariantInfo* variantInfo,
                                        TObjectPtr choicePtr)
{
    _ASSERT(!variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->IsInline());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    return variantInfo->GetItemPtr(choicePtr);
}

TObjectPtr
CVariantInfoFunctions::GetPointerVariant(const CVariantInfo* variantInfo,
                                         TObjectPtr choicePtr)
{
    _ASSERT(!variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->IsPointer());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    TObjectPtr variantPtr = variantInfo->GetItemPtr(choicePtr);
    variantPtr = CTypeConverter<TObjectPtr>::Get(variantPtr);
    _ASSERT(variantPtr);
    return variantPtr;
}

TObjectPtr
CVariantInfoFunctions::GetDelayedVariant(const CVariantInfo* variantInfo,
                                         TObjectPtr choicePtr)
{
    _ASSERT(variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    variantInfo->GetDelayBuffer(choicePtr).Update();
    TObjectPtr variantPtr = variantInfo->GetItemPtr(choicePtr);
    if ( variantInfo->IsPointer() ) {
        variantPtr = CTypeConverter<TObjectPtr>::Get(variantPtr);
        _ASSERT(variantPtr);
    }
    return variantPtr;
}

TObjectPtr
CVariantInfoFunctions::GetSubclassVariant(const CVariantInfo* variantInfo,
                                          TObjectPtr choicePtr)
{
    _ASSERT(variantInfo->IsSubClass());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    const CChoiceTypeInfo* choiceType = variantInfo->GetChoiceType();
    const CChoicePointerTypeInfo* choicePtrType =
        CTypeConverter<CChoicePointerTypeInfo>::SafeCast(choiceType);
    TObjectPtr variantPtr =
        choicePtrType->GetPointerTypeInfo()->GetObjectPointer(choicePtr);
    _ASSERT(variantPtr);
    return variantPtr;
}

void CVariantInfoFunctions::ReadInlineVariant(CObjectIStream& in,
                                              const CVariantInfo* variantInfo,
                                              TObjectPtr choicePtr)
{
    _ASSERT(!variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->IsInline());
    const CChoiceTypeInfo* choiceType = variantInfo->GetChoiceType();
    TMemberIndex index = variantInfo->GetIndex();
    choiceType->SetIndex(choicePtr, index, in.GetMemoryPool());
    in.ReadObject(variantInfo->GetItemPtr(choicePtr),
                  variantInfo->GetTypeInfo());
}

void CVariantInfoFunctions::ReadPointerVariant(CObjectIStream& in,
                                               const CVariantInfo* variantInfo,
                                               TObjectPtr choicePtr)
{
    _ASSERT(!variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->IsNonObjectPointer());
    const CChoiceTypeInfo* choiceType = variantInfo->GetChoiceType();
    TMemberIndex index = variantInfo->GetIndex();
    choiceType->SetIndex(choicePtr, index, in.GetMemoryPool());
    TObjectPtr variantPtr = variantInfo->GetItemPtr(choicePtr);
    variantPtr = CTypeConverter<TObjectPtr>::Get(variantPtr);
    _ASSERT(variantPtr != 0 );
    in.ReadObject(variantPtr, variantInfo->GetTypeInfo());
}

void CVariantInfoFunctions::ReadObjectPointerVariant(CObjectIStream& in,
                                                     const CVariantInfo* variantInfo,
                                                     TObjectPtr choicePtr)
{
    _ASSERT(!variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->IsObjectPointer());
    const CChoiceTypeInfo* choiceType = variantInfo->GetChoiceType();
    TMemberIndex index = variantInfo->GetIndex();
    choiceType->SetIndex(choicePtr, index, in.GetMemoryPool());
    TObjectPtr variantPtr = variantInfo->GetItemPtr(choicePtr);
    variantPtr = CTypeConverter<TObjectPtr>::Get(variantPtr);
    _ASSERT(variantPtr != 0 );
    in.ReadExternalObject(variantPtr, variantInfo->GetTypeInfo());
}

void CVariantInfoFunctions::ReadSubclassVariant(CObjectIStream& in,
                                                const CVariantInfo* variantInfo,
                                                TObjectPtr choicePtr)
{
    _ASSERT(!variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->IsSubClass());
    const CChoiceTypeInfo* choiceType = variantInfo->GetChoiceType();
    TMemberIndex index = variantInfo->GetIndex();
    choiceType->SetIndex(choicePtr, index, in.GetMemoryPool());
    const CChoicePointerTypeInfo* choicePtrType =
        CTypeConverter<CChoicePointerTypeInfo>::SafeCast(choiceType);
    TObjectPtr variantPtr =
        choicePtrType->GetPointerTypeInfo()->GetObjectPointer(choicePtr);
    _ASSERT(variantPtr);
    in.ReadExternalObject(variantPtr, variantInfo->GetTypeInfo());
}

void CVariantInfoFunctions::ReadDelayedVariant(CObjectIStream& in,
                                               const CVariantInfo* variantInfo,
                                               TObjectPtr choicePtr)
{
    _ASSERT(variantInfo->CanBeDelayed());
    const CChoiceTypeInfo* choiceType = variantInfo->GetChoiceType();
    TMemberIndex index = variantInfo->GetIndex();
    TTypeInfo variantType = variantInfo->GetTypeInfo();
    if ( index != choiceType->GetIndex(choicePtr) ) {
        // index is differnet from current -> first, reset choice
        choiceType->ResetIndex(choicePtr);
        CDelayBuffer& buffer = variantInfo->GetDelayBuffer(choicePtr);
        if ( !buffer ) {
            in.StartDelayBuffer();
            if ( variantInfo->IsObjectPointer() )
                in.SkipExternalObject(variantType);
            else
                in.SkipObject(variantType);
            in.EndDelayBuffer(buffer, variantInfo, choicePtr);
            // update index
            choiceType->SetDelayIndex(choicePtr, index);
            return;
        }
        buffer.Update();
        _ASSERT(!variantInfo->GetDelayBuffer(choicePtr));
    }
    // select for reading
    choiceType->SetIndex(choicePtr, index, in.GetMemoryPool());

    TObjectPtr variantPtr = variantInfo->GetItemPtr(choicePtr);
    if ( variantInfo->IsPointer() ) {
        variantPtr = CTypeConverter<TObjectPtr>::Get(variantPtr);
        _ASSERT(variantPtr != 0 );
        if ( variantInfo->IsObjectPointer() ) {
            in.ReadExternalObject(variantPtr, variantType);
            return;
        }
    }
    in.ReadObject(variantPtr, variantType);
}

void CVariantInfoFunctions::WriteInlineVariant(CObjectOStream& out,
                                               const CVariantInfo* variantInfo,
                                               TConstObjectPtr choicePtr)
{
    _ASSERT(!variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->IsInline());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    out.WriteObject(variantInfo->GetItemPtr(choicePtr),
                    variantInfo->GetTypeInfo());
}

void CVariantInfoFunctions::WritePointerVariant(CObjectOStream& out,
                                                const CVariantInfo* variantInfo,
                                                TConstObjectPtr choicePtr)
{
    _ASSERT(!variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->IsNonObjectPointer());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    TConstObjectPtr variantPtr = variantInfo->GetItemPtr(choicePtr);
    variantPtr = CTypeConverter<TConstObjectPtr>::Get(variantPtr);
    _ASSERT(variantPtr != 0 );
    out.WriteObject(variantPtr, variantInfo->GetTypeInfo());
}

void CVariantInfoFunctions::WriteObjectPointerVariant(CObjectOStream& out,
                                                      const CVariantInfo* variantInfo,
                                                      TConstObjectPtr choicePtr)
{
    _ASSERT(!variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->IsObjectPointer());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    TConstObjectPtr variantPtr = variantInfo->GetItemPtr(choicePtr);
    variantPtr = CTypeConverter<TConstObjectPtr>::Get(variantPtr);
    _ASSERT(variantPtr != 0 );
    out.WriteExternalObject(variantPtr, variantInfo->GetTypeInfo());
}

void CVariantInfoFunctions::WriteSubclassVariant(CObjectOStream& out,
                                                 const CVariantInfo* variantInfo,
                                                 TConstObjectPtr choicePtr)
{
    _ASSERT(!variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->IsSubClass());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    const CChoiceTypeInfo* choiceType = variantInfo->GetChoiceType();
    const CChoicePointerTypeInfo* choicePtrType =
        CTypeConverter<CChoicePointerTypeInfo>::SafeCast(choiceType);
    TConstObjectPtr variantPtr =
        choicePtrType->GetPointerTypeInfo()->GetObjectPointer(choicePtr);
    _ASSERT(variantPtr);
    out.WriteExternalObject(variantPtr, variantInfo->GetTypeInfo());
}

void CVariantInfoFunctions::WriteDelayedVariant(CObjectOStream& out,
                                                const CVariantInfo* variantInfo,
                                                TConstObjectPtr choicePtr)
{
    _ASSERT(variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    const CDelayBuffer& buffer = variantInfo->GetDelayBuffer(choicePtr);
    if ( buffer.GetIndex() == variantInfo->GetIndex() ) {
        if ( buffer.HaveFormat(out.GetDataFormat()) ) {
            out.Write(buffer.GetSource());
            return;
        }
        const_cast<CDelayBuffer&>(buffer).Update();
        _ASSERT(!variantInfo->GetDelayBuffer(choicePtr));
    }
    TConstObjectPtr variantPtr = variantInfo->GetItemPtr(choicePtr);
    if ( variantInfo->IsPointer() ) {
        variantPtr = CTypeConverter<TConstObjectPtr>::Get(variantPtr);
        _ASSERT(variantPtr != 0 );
        if ( variantInfo->IsObjectPointer() ) {
            out.WriteExternalObject(variantPtr, variantInfo->GetTypeInfo());
            return;
        }
    }
    out.WriteObject(variantPtr, variantInfo->GetTypeInfo());
}

void CVariantInfoFunctions::CopyNonObjectVariant(CObjectStreamCopier& copier,
                                                 const CVariantInfo* variantInfo)
{
    _ASSERT(variantInfo->IsNotObject());
    copier.CopyObject(variantInfo->GetTypeInfo());
}

void CVariantInfoFunctions::CopyObjectPointerVariant(CObjectStreamCopier& copier,
                                                     const CVariantInfo* variantInfo)
{
    _ASSERT(variantInfo->IsObjectPointer());
    copier.CopyExternalObject(variantInfo->GetTypeInfo());
}

void CVariantInfoFunctions::SkipNonObjectVariant(CObjectIStream& in,
                                                 const CVariantInfo* variantInfo)
{
    _ASSERT(variantInfo->IsNotObject());
    in.SkipObject(variantInfo->GetTypeInfo());
}

void CVariantInfoFunctions::SkipObjectPointerVariant(CObjectIStream& in,
                                                     const CVariantInfo* variantInfo)
{
    _ASSERT(variantInfo->IsObjectPointer());
    in.SkipExternalObject(variantInfo->GetTypeInfo());
}

void CVariantInfoFunctions::ReadHookedVariant(CObjectIStream& stream,
                                              const CVariantInfo* variantInfo,
                                              TObjectPtr choicePtr)
{
    CReadChoiceVariantHook* hook =
        variantInfo->m_ReadHookData.GetHook(stream.m_ChoiceVariantHookKey);
    if (!hook) {
        hook = variantInfo->m_ReadHookData.GetPathHook(stream);
    }
    if ( hook ) {
        CObjectInfo choice(choicePtr, variantInfo->GetChoiceType());
        TMemberIndex index = variantInfo->GetIndex();
        CObjectInfo::CChoiceVariant variant(choice, index);
        _ASSERT(variant.Valid());
        hook->ReadChoiceVariant(stream, variant);
    }
    else
        variantInfo->DefaultReadVariant(stream, choicePtr);
}

void CVariantInfoFunctions::WriteHookedVariant(CObjectOStream& stream,
                                               const CVariantInfo* variantInfo,
                                               TConstObjectPtr choicePtr)
{
    CWriteChoiceVariantHook* hook =
        variantInfo->m_WriteHookData.GetHook(stream.m_ChoiceVariantHookKey);
    if (!hook) {
        hook = variantInfo->m_WriteHookData.GetPathHook(stream);
    }
    if ( hook ) {
        CConstObjectInfo choice(choicePtr, variantInfo->GetChoiceType());
        TMemberIndex index = variantInfo->GetIndex();
        CConstObjectInfo::CChoiceVariant variant(choice, index);
        _ASSERT(variant.Valid());
        hook->WriteChoiceVariant(stream, variant);
    }
    else
        variantInfo->DefaultWriteVariant(stream, choicePtr);
}

void CVariantInfoFunctions::SkipHookedVariant(CObjectIStream& stream,
                                              const CVariantInfo* variantInfo)
{
    CSkipChoiceVariantHook* hook =
        variantInfo->m_SkipHookData.GetHook(stream.m_ChoiceVariantSkipHookKey);
    if (!hook) {
        hook = variantInfo->m_SkipHookData.GetPathHook(stream);
    }
    if ( hook ) {
        CObjectTypeInfo type(variantInfo->GetChoiceType());
        TMemberIndex index = variantInfo->GetIndex();
        CObjectTypeInfo::CChoiceVariant variant(type, index);
        _ASSERT(variant.Valid());
        hook->SkipChoiceVariant(stream, variant);
    }
    else
        variantInfo->DefaultSkipVariant(stream);
}

void CVariantInfoFunctions::CopyHookedVariant(CObjectStreamCopier& stream,
                                              const CVariantInfo* variantInfo)
{
    CCopyChoiceVariantHook* hook =
        variantInfo->m_CopyHookData.GetHook(stream.m_ChoiceVariantHookKey);
    if (!hook) {
        hook = variantInfo->m_CopyHookData.GetPathHook(stream.In());
    }
    if ( hook ) {
        CObjectTypeInfo type(variantInfo->GetChoiceType());
        TMemberIndex index = variantInfo->GetIndex();
        CObjectTypeInfo::CChoiceVariant variant(type, index);
        _ASSERT(variant.Valid());
        hook->CopyChoiceVariant(stream, variant);
    }
    else
        variantInfo->DefaultCopyVariant(stream);
}

void CVariantInfo::SetGlobalReadHook(CReadChoiceVariantHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_ReadHookData.SetGlobalHook(hook);
}

void CVariantInfo::SetLocalReadHook(CObjectIStream& stream,
                                    CReadChoiceVariantHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_ReadHookData.SetLocalHook(stream.m_ChoiceVariantHookKey, hook);
}

void CVariantInfo::ResetGlobalReadHook(void)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_ReadHookData.ResetGlobalHook();
}

void CVariantInfo::ResetLocalReadHook(CObjectIStream& stream)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_ReadHookData.ResetLocalHook(stream.m_ChoiceVariantHookKey);
}

void CVariantInfo::SetPathReadHook(CObjectIStream* in, const string& path,
                                   CReadChoiceVariantHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_ReadHookData.SetPathHook(in,path,hook);
}

void CVariantInfo::SetGlobalWriteHook(CWriteChoiceVariantHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_WriteHookData.SetGlobalHook(hook);
}

void CVariantInfo::SetLocalWriteHook(CObjectOStream& stream,
                                     CWriteChoiceVariantHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_WriteHookData.SetLocalHook(stream.m_ChoiceVariantHookKey, hook);
}

void CVariantInfo::ResetGlobalWriteHook(void)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_WriteHookData.ResetGlobalHook();
}

void CVariantInfo::ResetLocalWriteHook(CObjectOStream& stream)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_WriteHookData.ResetLocalHook(stream.m_ChoiceVariantHookKey);
}

void CVariantInfo::SetPathWriteHook(CObjectOStream* out, const string& path,
                                    CWriteChoiceVariantHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_WriteHookData.SetPathHook(out,path,hook);
}

void CVariantInfo::SetGlobalSkipHook(CSkipChoiceVariantHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_SkipHookData.SetGlobalHook(hook);
}

void CVariantInfo::SetLocalSkipHook(CObjectIStream& stream,
                                    CSkipChoiceVariantHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_SkipHookData.SetLocalHook(stream.m_ChoiceVariantSkipHookKey, hook);
}

void CVariantInfo::ResetGlobalSkipHook(void)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_SkipHookData.ResetGlobalHook();
}

void CVariantInfo::ResetLocalSkipHook(CObjectIStream& stream)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_SkipHookData.ResetLocalHook(stream.m_ChoiceVariantSkipHookKey);
}

void CVariantInfo::SetPathSkipHook(CObjectIStream* in, const string& path,
                                   CSkipChoiceVariantHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_SkipHookData.SetPathHook(in,path,hook);
}

void CVariantInfo::SetGlobalCopyHook(CCopyChoiceVariantHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_CopyHookData.SetGlobalHook(hook);
}

void CVariantInfo::SetLocalCopyHook(CObjectStreamCopier& stream,
                                    CCopyChoiceVariantHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_CopyHookData.SetLocalHook(stream.m_ChoiceVariantHookKey, hook);
}

void CVariantInfo::ResetGlobalCopyHook(void)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_CopyHookData.ResetGlobalHook();
}

void CVariantInfo::ResetLocalCopyHook(CObjectStreamCopier& stream)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_CopyHookData.ResetLocalHook(stream.m_ChoiceVariantHookKey);
}

void CVariantInfo::SetPathCopyHook(CObjectStreamCopier* stream, const string& path,
                                   CCopyChoiceVariantHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_CopyHookData.SetPathHook(stream ? &(stream->In()) : 0,path,hook);
}

END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.17  2005/04/26 14:18:50  vasilche
* Allow allocation of objects in CObjectMemoryPool.
*
* Revision 1.16  2004/09/09 19:18:24  vasilche
* Some object streams require class/choice to be in stack.
*
* Revision 1.15  2004/09/09 15:33:35  vasilche
* Allow to disable delayed parsing.
*
* Revision 1.14  2004/05/17 21:03:04  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.13  2004/01/05 14:25:23  gouriano
* Added possibility to set serialization hooks by stack path
*
* Revision 1.12  2003/07/29 18:47:48  vasilche
* Fixed thread safeness of object stream hooks.
*
* Revision 1.11  2003/03/10 18:54:26  gouriano
* use new structured exceptions (based on CException)
*
* Revision 1.10  2002/09/09 18:14:02  grichenk
* Added CObjectHookGuard class.
* Added methods to be used by hooks for data
* reading and skipping.
*
* Revision 1.9  2001/05/17 15:07:09  lavr
* Typos corrected
*
* Revision 1.8  2000/10/20 15:51:44  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.7  2000/10/17 18:45:36  vasilche
* Added possibility to turn off object cross reference detection in
* CObjectIStream and CObjectOStream.
*
* Revision 1.6  2000/10/13 20:22:57  vasilche
* Fixed warnings on 64 bit compilers.
* Fixed missing typename in templates.
*
* Revision 1.5  2000/10/03 17:22:45  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.4  2000/09/29 16:18:25  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.3  2000/09/26 19:24:58  vasilche
* Added user interface for setting read/write/copy hooks.
*
* Revision 1.2  2000/09/26 17:38:23  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.1  2000/09/18 20:00:26  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* ===========================================================================
*/
